// SPDX-License-Identifier: GPL-2.0
/*
 * mtu3_dr.c - dual role switch and host glue layer
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 */

#include <linux/usb/role.h>
#include <linux/of_platform.h>
#include <linux/iopoll.h>

#include "mtu3.h"
#include "mtu3_dr.h"
#include "mtu3_debug.h"

#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_SS_MON_GADGET)
#include <../ss_function/f_ss_mon_gadget.h>
#endif

#define USB2_PORT 2
#define USB3_PORT 3

static inline struct ssusb_mtk *otg_sx_to_ssusb(struct otg_switch_mtk *otg_sx)
{
	return container_of(otg_sx, struct ssusb_mtk, otg_switch);
}

static void toggle_opstate(struct ssusb_mtk *ssusb)
{
	mtu3_setbits(ssusb->mac_base, U3D_DEVICE_CONTROL, DC_SESSION);
	mtu3_setbits(ssusb->mac_base, U3D_POWER_MANAGEMENT, SOFT_CONN);
}

/* only port0 supports dual-role mode */
static int ssusb_port0_switch(struct ssusb_mtk *ssusb,
	int version, bool tohost)
{
	void __iomem *ibase = ssusb->ippc_base;
	u32 value;

	dev_dbg(ssusb->dev, "%s (switch u%d port0 to %s)\n", __func__,
		version, tohost ? "host" : "device");

	if (version == USB2_PORT) {
		/* 1. power off and disable u2 port0 */
		value = mtu3_readl(ibase, SSUSB_U2_CTRL(0));
		value |= SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U2_CTRL(0), value);

		/* 2. power on, enable u2 port0 and select its mode */
		value = mtu3_readl(ibase, SSUSB_U2_CTRL(0));
		value &= ~(SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS);
		value = tohost ? (value | SSUSB_U2_PORT_HOST_SEL) :
			(value & (~SSUSB_U2_PORT_HOST_SEL));
		mtu3_writel(ibase, SSUSB_U2_CTRL(0), value);
	} else {
		/* 1. power off and disable u3 port0 */
		value = mtu3_readl(ibase, SSUSB_U3_CTRL(0));
		value |= SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS;
		mtu3_writel(ibase, SSUSB_U3_CTRL(0), value);

		/* 2. power on, enable u3 port0 and select its mode */
		value = mtu3_readl(ibase, SSUSB_U3_CTRL(0));
		value &= ~(SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS);
		value = tohost ? (value | SSUSB_U3_PORT_HOST_SEL) :
			(value & (~SSUSB_U3_PORT_HOST_SEL));
		mtu3_writel(ibase, SSUSB_U3_CTRL(0), value);
	}

	return 0;
}

static void ssusb_ip_sleep(struct ssusb_mtk *ssusb)
{
	void __iomem *ibase = ssusb->ippc_base;
	u32 value;
	int ret;

	/* power down and disable all u3 ports */
	value = mtu3_readl(ibase, SSUSB_U3_CTRL(0));
	value |= SSUSB_U3_PORT_PDN | SSUSB_U3_PORT_DIS;
	mtu3_writel(ibase, SSUSB_U3_CTRL(0), value);
	mtu3_clrbits(ibase, SSUSB_U3_CTRL(0), SSUSB_U3_PORT_DUAL_MODE);

	/* power down and disable all u2 ports */
	value = mtu3_readl(ibase, SSUSB_U2_CTRL(0));
	value |= SSUSB_U2_PORT_PDN | SSUSB_U2_PORT_DIS;
	mtu3_writel(ibase, SSUSB_U2_CTRL(0), value);
	mtu3_clrbits(ibase, SSUSB_U2_CTRL(0), SSUSB_U2_PORT_OTG_SEL);

	/* power down device ip */
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL2, SSUSB_IP_DEV_PDN);
	/* power down host ip */
	mtu3_setbits(ibase, U3D_SSUSB_IP_PW_CTRL1, SSUSB_IP_HOST_PDN);

	/* wait for ip to sleep */
	ret = readl_poll_timeout(ibase + U3D_SSUSB_IP_PW_STS1, value,
			  (value & SSUSB_IP_SLEEP_STS), 100, 100000);
	if (ret)
		dev_info(ssusb->dev, "ip sleep failed!!!\n");
}

static void switch_port_to_on(struct ssusb_mtk *ssusb, enum phy_mode mode)
{

	dev_info(ssusb->dev, "port on (%d)\n", mode);

	pm_runtime_get(ssusb->dev);

	ssusb_clks_enable(ssusb);
	ssusb_vsvoter_set(ssusb);
	ssusb_phy_power_on(ssusb);
	ssusb_phy_set_mode(ssusb, mode);
	ssusb_ip_sw_reset(ssusb);
}

static void switch_port_to_off(struct ssusb_mtk *ssusb)
{
	dev_info(ssusb->dev, "port off\n");

	synchronize_irq(ssusb->u3d->irq);
	ssusb_ip_sleep(ssusb);
	ssusb_phy_set_mode(ssusb, PHY_MODE_INVALID);
	ssusb_phy_power_off(ssusb);
	ssusb_vsvoter_clr(ssusb);
	ssusb_clks_disable(ssusb);

	pm_runtime_put(ssusb->dev);
}

static void switch_port_to_host(struct ssusb_mtk *ssusb)
{
	u32 check_clk = 0;

	dev_dbg(ssusb->dev, "%s\n", __func__);

	ssusb_port0_switch(ssusb, USB2_PORT, true);

	if (ssusb->otg_switch.is_u3_drd) {
		ssusb_port0_switch(ssusb, USB3_PORT, true);
		check_clk = SSUSB_U3_MAC_RST_B_STS;
	}

	ssusb_check_clocks(ssusb, check_clk);

	/* after all clocks are stable */
	toggle_opstate(ssusb);
}

static void switch_port_to_device(struct ssusb_mtk *ssusb)
{
	u32 check_clk = 0;

	dev_dbg(ssusb->dev, "%s\n", __func__);

	ssusb_port0_switch(ssusb, USB2_PORT, false);

	if (ssusb->otg_switch.is_u3_drd) {
		ssusb_port0_switch(ssusb, USB3_PORT, false);
		check_clk = SSUSB_U3_MAC_RST_B_STS;
	}

	ssusb_check_clocks(ssusb, check_clk);
}

static void ssusb_host_register(struct ssusb_mtk *ssusb, bool on)
{
	int ret;

	dev_info(ssusb->dev, "%s %d\n", __func__, on);

	if (!ssusb->xhci_pdrv)
		return;

	if (on) {
		ret = platform_driver_register(ssusb->xhci_pdrv);
		if (ret)
			dev_info(ssusb->dev, "register host driver fail\n");
	} else {
		platform_driver_unregister(ssusb->xhci_pdrv);
	}
#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	usbpd_set_host_on(ssusb->man, on);
#endif
}

int ssusb_set_vbus(struct otg_switch_mtk *otg_sx, int is_on)
{
	struct ssusb_mtk *ssusb = otg_sx_to_ssusb(otg_sx);
	struct regulator *vbus = otg_sx->vbus;
	int ret;

	/* vbus is optional */
	if (!vbus)
		return 0;

	dev_dbg(ssusb->dev, "%s: turn %s\n", __func__, is_on ? "on" : "off");

	if (is_on) {
		ret = regulator_enable(vbus);
		if (ret) {
			dev_err(ssusb->dev, "vbus regulator enable failed\n");
			return ret;
		}
	} else {
		regulator_disable(vbus);
	}

	return 0;
}

static void ssusb_mode_sw_work_v2(struct work_struct *work)
{
	struct dr_work_data_mtk *work_data =
		container_of(work, struct dr_work_data_mtk, dr_work);

	struct otg_switch_mtk *otg_sx = work_data->otg_sx;
	struct ssusb_mtk *ssusb = otg_sx_to_ssusb(otg_sx);
	struct mtu3 *mtu = ssusb->u3d;
	enum usb_role desired_role;
	enum usb_role current_role;
	unsigned long flags;

	desired_role = work_data->desired_role;
	current_role = otg_sx->current_role;

	dev_info(ssusb->dev, "%s: %s to %s\n", __func__,
		 usb_role_string(current_role), usb_role_string(desired_role));

	if (current_role == desired_role)
		goto same_role;

	mtu3_dbg_trace(ssusb->dev, "set role : %s", usb_role_string(desired_role));

	pm_runtime_get_sync(ssusb->dev);

	/* switch port to off first */
	switch (current_role) {
	case USB_ROLE_HOST:
		ssusb->is_host = false;
		ssusb_set_vbus(otg_sx, 0);
		/* wait for host device remove done, e.g. usb audio */
		mdelay(50);
		/* unregister host driver */
		ssusb_host_register(ssusb, false);
		ssusb_set_power_state(ssusb, MTU3_STATE_POWER_OFF);
		ssusb_host_disable(ssusb);
		switch_port_to_off(ssusb);
		/* wait for hw to complete host off */
		mdelay(50);
		break;
	case USB_ROLE_DEVICE:
		spin_lock_irqsave(&mtu->lock, flags);
#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_SS_MON_GADGET)
		vbus_session_notify(&mtu->g, true, EAGAIN);
#endif
		mtu3_stop(mtu);
		/* report disconnect */
		if (mtu->g.speed != USB_SPEED_UNKNOWN) {
			mtu3_gadget_disconnect(mtu);
			if (ssusb->force_vbus)
				pm_runtime_put(ssusb->dev);
		}
		spin_unlock_irqrestore(&mtu->lock, flags);
		ssusb_set_power_state(ssusb, MTU3_STATE_POWER_OFF);
		mtu3_device_disable(mtu);
		switch_port_to_off(ssusb);
		pm_relax(ssusb->dev);
		break;
	case USB_ROLE_NONE:
		break;
	default:
		dev_info(ssusb->dev, "invalid role\n");
	}

	/* switch port to on again */
	switch (desired_role) {
	case USB_ROLE_HOST:
		switch_port_to_on(ssusb, PHY_MODE_USB_HOST);
		ssusb_host_enable(ssusb);
		ssusb_set_power_state(ssusb, MTU3_STATE_POWER_ON);
		ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_HOST);
		/* register host driver */
		ssusb_host_register(ssusb, true);
		ssusb_set_noise_still_tr(ssusb);
		ssusb_set_vbus(otg_sx, 1);
		mtu3_check_params(mtu);
		mtu3_set_speed(mtu, mtu->speed);
		ssusb->is_host = true;
		break;
	case USB_ROLE_DEVICE:
		/* avoid suspend when works as device */
		pm_stay_awake(ssusb->dev);
		switch_port_to_on(ssusb, PHY_MODE_USB_DEVICE);
		mtu3_device_enable(mtu);
		ssusb_set_power_state(ssusb, MTU3_STATE_POWER_ON);
		ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_DEVICE);
#if IS_ENABLED(CONFIG_USB_CONFIGFS_F_SS_MON_GADGET)
		vbus_session_notify(&mtu->g, true, EAGAIN);
#endif
		mtu3_start(mtu);
		break;
	case USB_ROLE_NONE:
		break;
	default:
		dev_info(ssusb->dev, "invalid role\n");
	}

	otg_sx->current_role = desired_role;

	pm_runtime_put(ssusb->dev);

same_role:
	kfree(work_data);
}

static void ssusb_mode_sw_work(struct work_struct *work)
{
	struct dr_work_data_mtk *work_data =
		container_of(work, struct dr_work_data_mtk, dr_work);
	struct otg_switch_mtk *otg_sx = work_data->otg_sx;

	struct ssusb_mtk *ssusb = otg_sx_to_ssusb(otg_sx);
	struct mtu3 *mtu = ssusb->u3d;
	enum usb_role desired_role;
	enum usb_role current_role;

	desired_role = work_data->desired_role;

	current_role = ssusb->is_host ? USB_ROLE_HOST : USB_ROLE_DEVICE;

	if (desired_role == USB_ROLE_NONE) {
		/* the default mode is host as probe does */
		desired_role = USB_ROLE_HOST;
		if (otg_sx->default_role == USB_ROLE_DEVICE)
			desired_role = USB_ROLE_DEVICE;
	}

	if (current_role == desired_role)
		goto same_role;

	dev_dbg(ssusb->dev, "set role : %s\n", usb_role_string(desired_role));
	mtu3_dbg_trace(ssusb->dev, "set role : %s", usb_role_string(desired_role));
	pm_runtime_get_sync(ssusb->dev);

	switch (desired_role) {
	case USB_ROLE_HOST:
		ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_HOST);
		mtu3_stop(mtu);
		switch_port_to_host(ssusb);
		ssusb_set_vbus(otg_sx, 1);
		ssusb->is_host = true;
		break;
	case USB_ROLE_DEVICE:
		ssusb_set_force_mode(ssusb, MTU3_DR_FORCE_DEVICE);
		ssusb->is_host = false;
		ssusb_set_vbus(otg_sx, 0);
		switch_port_to_device(ssusb);
		mtu3_start(mtu);
		break;
	case USB_ROLE_NONE:
	default:
		dev_err(ssusb->dev, "invalid role\n");
	}
	pm_runtime_put(ssusb->dev);

same_role:
	kfree(work_data);
}

void ssusb_set_mode(struct otg_switch_mtk *otg_sx, enum usb_role role)
{
	struct ssusb_mtk *ssusb = otg_sx_to_ssusb(otg_sx);
	struct dr_work_data_mtk *work_data;

	dev_info(ssusb->dev, "%s %s\n", __func__, usb_role_string(role));

	otg_sx->latest_role = role;

	if (otg_sx->op_mode != MTU3_DR_OPERATION_DUAL) {
		dev_info(ssusb->dev, "op_mode %d, skip set role\n", otg_sx->op_mode);
		return;
	}

	if (ssusb->dr_mode != USB_DR_MODE_OTG)
		return;

	work_data = kzalloc(sizeof(struct dr_work_data_mtk), GFP_ATOMIC);
	if (!work_data) {
		dev_err(ssusb->dev, "allocate work data fail.\n");
		return;
	}
	work_data->otg_sx = otg_sx;

	if (ssusb->clk_mgr)
		INIT_WORK(&work_data->dr_work, ssusb_mode_sw_work_v2);
	else
		INIT_WORK(&work_data->dr_work, ssusb_mode_sw_work);

	work_data->desired_role = role;
	queue_work(otg_sx->wq, &work_data->dr_work);
}

static int ssusb_id_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct otg_switch_mtk *otg_sx =
		container_of(nb, struct otg_switch_mtk, id_nb);

	ssusb_set_mode(otg_sx, event ? USB_ROLE_HOST : USB_ROLE_DEVICE);

	return NOTIFY_DONE;
}

static int ssusb_extcon_register(struct otg_switch_mtk *otg_sx)
{
	struct ssusb_mtk *ssusb = otg_sx_to_ssusb(otg_sx);
	struct extcon_dev *edev = otg_sx->edev;
	int ret;

	/* extcon is optional */
	if (!edev)
		return 0;

	otg_sx->id_nb.notifier_call = ssusb_id_notifier;
	ret = devm_extcon_register_notifier(ssusb->dev, edev, EXTCON_USB_HOST,
					&otg_sx->id_nb);
	if (ret < 0) {
		dev_err(ssusb->dev, "failed to register notifier for USB-HOST\n");
		return ret;
	}

	ret = extcon_get_state(edev, EXTCON_USB_HOST);
	dev_dbg(ssusb->dev, "EXTCON_USB_HOST: %d\n", ret);

	/* default as host, switch to device mode if needed */
	if (!ret)
		ssusb_set_mode(otg_sx, USB_ROLE_DEVICE);

	return 0;
}

/*
 * We provide an interface via debugfs to switch between host and device modes
 * depending on user input.
 * This is useful in special cases, such as uses TYPE-A receptacle but also
 * wants to support dual-role mode.
 */
void ssusb_mode_switch(struct ssusb_mtk *ssusb, int to_host)
{
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	ssusb_set_mode(otg_sx, to_host ? USB_ROLE_HOST : USB_ROLE_DEVICE);
}

void ssusb_set_force_mode(struct ssusb_mtk *ssusb,
			  enum mtu3_dr_force_mode mode)
{
	u32 value;

	value = mtu3_readl(ssusb->ippc_base, SSUSB_U2_CTRL(0));
	switch (mode) {
	case MTU3_DR_FORCE_DEVICE:
		value |= SSUSB_U2_PORT_FORCE_IDDIG | SSUSB_U2_PORT_RG_IDDIG;
		break;
	case MTU3_DR_FORCE_HOST:
		value |= SSUSB_U2_PORT_FORCE_IDDIG;
		value &= ~SSUSB_U2_PORT_RG_IDDIG;
		break;
	case MTU3_DR_FORCE_NONE:
		value &= ~(SSUSB_U2_PORT_FORCE_IDDIG | SSUSB_U2_PORT_RG_IDDIG);
		break;
	default:
		return;
	}
	mtu3_writel(ssusb->ippc_base, SSUSB_U2_CTRL(0), value);
}

static int ssusb_role_sw_set(struct usb_role_switch *sw, enum usb_role role)
{
	struct ssusb_mtk *ssusb = usb_role_switch_get_drvdata(sw);
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	ssusb_set_mode(otg_sx, role);

	return 0;
}

static enum usb_role ssusb_role_sw_get(struct usb_role_switch *sw)
{
	struct ssusb_mtk *ssusb = usb_role_switch_get_drvdata(sw);

	return ssusb->is_host ? USB_ROLE_HOST : USB_ROLE_DEVICE;
}

static int ssusb_role_sw_register(struct otg_switch_mtk *otg_sx)
{
	struct usb_role_switch_desc role_sx_desc = { 0 };
	struct ssusb_mtk *ssusb = otg_sx_to_ssusb(otg_sx);
	struct device *dev = ssusb->dev;
	enum usb_dr_mode mode;

	if (!otg_sx->role_sw_used)
		return 0;

	mode = usb_get_role_switch_default_mode(dev);
	if (mode == USB_DR_MODE_PERIPHERAL)
		otg_sx->default_role = USB_ROLE_DEVICE;
	else
		otg_sx->default_role = USB_ROLE_HOST;

	if (ssusb->clk_mgr) {
		/* workaround, prevent usage count issue */
		pm_runtime_get(ssusb->dev);
		otg_sx->default_role = USB_ROLE_NONE;
	}

	role_sx_desc.set = ssusb_role_sw_set;
	role_sx_desc.get = ssusb_role_sw_get;
	role_sx_desc.fwnode = dev_fwnode(dev);
	role_sx_desc.driver_data = ssusb;
	role_sx_desc.allow_userspace_control = true;
	otg_sx->role_sw = usb_role_switch_register(dev, &role_sx_desc);
	if (IS_ERR(otg_sx->role_sw))
		return PTR_ERR(otg_sx->role_sw);

	ssusb_set_mode(otg_sx, otg_sx->default_role);

	return 0;
}

static ssize_t mode_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;
	enum usb_role role = otg_sx->latest_role;
	int mode;

	if (kstrtoint(buf, 10, &mode))
		return -EINVAL;

	dev_info(dev, "store mode %d op_mode %d\n", mode, otg_sx->op_mode);

	if (otg_sx->op_mode != mode) {
		/* set switch role */
		switch (mode) {
		case MTU3_DR_OPERATION_OFF:
			otg_sx->latest_role = USB_ROLE_NONE;
			break;
		case MTU3_DR_OPERATION_DUAL:
			/* switch usb role to latest role */
			break;
		case MTU3_DR_OPERATION_HOST:
			otg_sx->latest_role = USB_ROLE_HOST;
			break;
		case MTU3_DR_OPERATION_DEVICE:
			otg_sx->latest_role = USB_ROLE_DEVICE;
			break;
		default:
			return -EINVAL;
		}
		/* switch operation mode to normal temporarily */
		otg_sx->op_mode = MTU3_DR_OPERATION_DUAL;
		/* switch usb role */
		ssusb_role_sw_set(otg_sx->role_sw, otg_sx->latest_role);
		/* update operation mode */
		otg_sx->op_mode = mode;
		/* restore role */
		otg_sx->latest_role = role;
	}

	return count;
}

static ssize_t mode_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	return sprintf(buf, "%d\n", otg_sx->op_mode);
}
static DEVICE_ATTR_RW(mode);

static ssize_t role_mode_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	return sprintf(buf, "%d\n", otg_sx->current_role);
}
static DEVICE_ATTR_RO(role_mode);

static ssize_t max_speed_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct mtu3 *mtu = ssusb->u3d;
	int speed;

	if (!strncmp(buf, "super-speed-plus", 16))
		speed = USB_SPEED_SUPER_PLUS;
	else if (!strncmp(buf, "super-speed", 11))
		speed = USB_SPEED_SUPER;
	else if (!strncmp(buf, "high-speed", 10))
		speed = USB_SPEED_HIGH;
	else if (!strncmp(buf, "full-speed", 10))
		speed = USB_SPEED_FULL;
	else
		return -EFAULT;

	dev_info(dev, "store speed %s\n", buf);

	mtu->max_speed = speed;
	mtu->g.max_speed = speed;

	return count;
}

static ssize_t max_speed_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct mtu3 *mtu = ssusb->u3d;

	return sprintf(buf, "%s\n", usb_speed_string(mtu->max_speed));
}
static DEVICE_ATTR_RW(max_speed);

static ssize_t saving_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct mtu3 *mtu = ssusb->u3d;
	int mode;

	if (kstrtoint(buf, 10, &mode))
		return -EINVAL;

	if (mode < MTU3_EP_SLOT_DEFAULT || mode > MTU3_EP_SLOT_MAX)
		return -EINVAL;

	mtu->ep_slot_mode = mode;

	dev_info(dev, "slot mode %d\n", mtu->ep_slot_mode);

	return count;
}

static ssize_t saving_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct mtu3 *mtu = ssusb->u3d;

	return sprintf(buf, "%d\n", mtu->ep_slot_mode);
}
static DEVICE_ATTR_RW(saving);

static ssize_t u3_lpm_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct mtu3 *mtu = ssusb->u3d;
	bool enable;

	if (kstrtobool(buf, &enable))
		return -EINVAL;

	if (of_property_read_bool(dev->of_node, "usb3-lpm-disable"))
		return -EINVAL;

	mtu->u3_lpm = enable ? 1 : 0;

	return count;
}

static ssize_t u3_lpm_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	struct mtu3 *mtu = ssusb->u3d;

	return sprintf(buf, "%d\n", mtu->u3_lpm);
}
static DEVICE_ATTR_RW(u3_lpm);

static ssize_t ux_exit_lfps_gen1_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	int value;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	ssusb->ux_exit_lfps = value;

	dev_info(dev, "ssize_t ux_exit_lfps_gen1 0x%x\n", ssusb->ux_exit_lfps);

	return count;
}

static ssize_t ux_exit_lfps_gen1_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n", ssusb->ux_exit_lfps);
}
static DEVICE_ATTR_RW(ux_exit_lfps_gen1);

static ssize_t ux_exit_lfps_gen2_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);
	int value;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	ssusb->ux_exit_lfps_gen2 = value;

	dev_info(dev, "ssize_t ux_exit_lfps_gen2 0x%x\n", ssusb->ux_exit_lfps_gen2);

	return count;
}

static ssize_t ux_exit_lfps_gen2_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dev);

	return sprintf(buf, "0x%x\n", ssusb->ux_exit_lfps_gen2);
}
static DEVICE_ATTR_RW(ux_exit_lfps_gen2);

static struct attribute *ssusb_dr_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_role_mode.attr,
	&dev_attr_max_speed.attr,
	&dev_attr_saving.attr,
	&dev_attr_u3_lpm.attr,
	&dev_attr_ux_exit_lfps_gen1.attr,
	&dev_attr_ux_exit_lfps_gen2.attr,
	NULL
};

static const struct attribute_group ssusb_dr_group = {
	.attrs = ssusb_dr_attrs,
};

int ssusb_otg_switch_init(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;
	int ret = 0;

	otg_sx->wq = create_singlethread_workqueue("otg_sx");
	if (!otg_sx->wq)
		return -ENOMEM;

	/* initial operation mode */
	otg_sx->op_mode = MTU3_DR_OPERATION_DUAL;
	otg_sx->current_role = ssusb->is_host ?
		USB_ROLE_HOST : USB_ROLE_DEVICE;

	ret = sysfs_create_group(&ssusb->dev->kobj, &ssusb_dr_group);
	if (ret)
		dev_info(ssusb->dev, "error creating sysfs attributes\n");

	if (otg_sx->manual_drd_enabled)
		ssusb_dr_debugfs_init(ssusb);
	else if (otg_sx->role_sw_used)
		ret = ssusb_role_sw_register(otg_sx);
	else
		ret = ssusb_extcon_register(otg_sx);

#if IS_ENABLED(CONFIG_IF_CB_MANAGER)
	ssusb->usb_d.data = (void *)ssusb;
	ssusb->man = register_usb(&ssusb->usb_d);
#endif
	return ret;
}

void ssusb_otg_switch_exit(struct ssusb_mtk *ssusb)
{
	struct otg_switch_mtk *otg_sx = &ssusb->otg_switch;

	flush_workqueue(otg_sx->wq);
	destroy_workqueue(otg_sx->wq);

	usb_role_switch_unregister(otg_sx->role_sw);
	sysfs_remove_group(&ssusb->dev->kobj, &ssusb_dr_group);
}
