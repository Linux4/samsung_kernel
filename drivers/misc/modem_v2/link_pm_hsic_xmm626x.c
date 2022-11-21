/* /linux/drivers/misc/modem_v2/link_pm_hsic_xmm626x.c
 *
 * Copyright (C) 2012 Google, Inc.
 * Copyright (C) 2012 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/platform_data/modem.h>
#include <linux/usb/ehci_def.h>

#include "modem_prj.h"
#include "modem_utils.h"
#include "modem_link_device_hsic_ncm.h"

#if defined(CONFIG_OF)
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/platform_data/samsung-usbphy.h>
#endif

#define EHCI_REG_DUMP

#define IOCTL_LINK_CONTROL_ENABLE	_IO('o', 0x30)
#define IOCTL_LINK_CONTROL_ACTIVE	_IO('o', 0x31)
#define IOCTL_LINK_GET_HOSTWAKE		_IO('o', 0x32)
#define IOCTL_LINK_CONNECTED		_IO('o', 0x33)

#define HOSTWAKE_TRIGLEVEL 0
#define get_hostwake(p) \
	(gpio_get_value((p)->pdata->gpio_link_hostwake) == HOSTWAKE_TRIGLEVEL)

#define get_hostactive(p) \
	(gpio_get_value((p)->pdata->gpio_link_active))

/* /sys/module/modem_link_pm_xmm626x/parameters/...*/
static int l2_delay = 500;
module_param(l2_delay, int, S_IRUGO);
MODULE_PARM_DESC(l2_delay, "HSIC autosuspend delay");

static int hub_delay = 100;
module_param(hub_delay, int, S_IRUGO);
MODULE_PARM_DESC(hub_delay, "Root-hub autosuspend delay");

enum linkpm_status {
	HSIC_LINKPM_L0,
	HSIC_LINKPM_L2,
	HSIC_LINKPM_L3,
	HSIC_LINKPM_L3_WAKE,
	HSIC_LINKPM_L3_L0,
	HSIC_LINKPM_L3_WAIT,
};

enum linkpm_event {
	LINKPM_EVENT_RUNTIME,
};

struct xmm626x_linkpm_data {
	struct miscdevice miscdev;
	struct list_head link;
	struct platform_device *pdev;
	struct modemlink_pm_data *pdata;
	struct usb_device *udev;
	struct usb_device *hdev;
	struct notifier_block phy_nfb;
	struct notifier_block usb_nfb;
	struct notifier_block pm_nfb;

	unsigned link_connected;
	bool dpm_suspending;
	spinlock_t lock;
	struct workqueue_struct *wq;
	struct delayed_work link_pm_work;
	struct delayed_work link_pm_event;
	unsigned long events;
	int resume_cnt;
	struct wake_lock l2_wake;
	bool resume_req;
	struct wake_lock tx_wake;
};

enum known_device_type {
	MIF_UNKNOWN_DEVICE,
	MIF_MAIN_DEVICE,
	MIF_BOOT_DEVICE,
};

struct  link_usb_id {
	int vid;
	int pid;
	enum known_device_type type;
};

/*TODO: get pid, vid from platform data */
static struct link_usb_id xmm626x_ids[] = {
	{0x1519, 0x0443, MIF_MAIN_DEVICE}, /* XMM6360 */
	{0x8087, 0x0716, MIF_BOOT_DEVICE}, /* XMM6360 */
	{0x1519, 0x0020, MIF_MAIN_DEVICE}, /* XMM6262 */
	{0x058b, 0x0041, MIF_BOOT_DEVICE}, /* XMM6262 */
	{0x8087, 0x07ed, MIF_MAIN_DEVICE}, /* XMM7260 */
	{0x8087, 0x07ef, MIF_BOOT_DEVICE}, /* XMM7260 */
};

struct linkpm_devices {
	struct list_head pmdata;
	spinlock_t lock;
} xmm626x_devices;

#ifdef CONFIG_PM_RUNTIME
/* hooking from generic_suspend and generic_resume */
static int (*_usb_suspend) (struct usb_device *, pm_message_t);
static int (*_usb_resume) (struct usb_device *, pm_message_t);
#endif

#if defined(CONFIG_OF)
#ifdef EHCI_REG_DUMP
struct exynos5430_ehci_regs {
	/* EHCI Capability Register */
	unsigned hc_cap_base;	/* 0x00 */
	unsigned hc_sparams;
	unsigned hc_cparams;
	unsigned reserved0;

	/* EHCI Operational Register */
	unsigned usb_cmd;	/* 0x10 */
	unsigned usb_sts;
	unsigned usb_intr;
	unsigned fr_index;
	unsigned ctrl_ds_segment;
	unsigned periodic_list_base;
	unsigned async_list_addr;
	unsigned reserved1[9];

	/* EHCI Auxiliary Power Well Register */
	unsigned config_flag;	/* 0x50 */
	unsigned port_sc0;
	unsigned port_sc1;
	unsigned reserved2[13];

	/* EHCI Specific Register */
	unsigned insnreg00;	/* 0x90 */
	unsigned insnreg01;
	unsigned insnreg02;
	unsigned insnreg03;
	unsigned insnreg04;
	unsigned insnreg05;
	unsigned insnreg06;
	unsigned insnreg07;
};
static struct exynos5430_ehci_regs __iomem *ehci_reg;

struct exynos5430_phy_regs {
	/* PHY Control Register */
	unsigned host_phy_ctrl0;	/* 0x00 */
	unsigned host_phy_tune0;
	unsigned reserved0[2];

	unsigned hsic_phy_ctrl1;	/* 0x10 */
	unsigned hsic_phy_tune1;
	unsigned reserved1[6];

	unsigned host_ehci_ctrl;	/* 0x30 */
	unsigned host_ohci_ctrl;
};
static struct exynos5430_phy_regs __iomem *phy_reg;

#define pr_reg(s, r) pr_info("reg(%s):\t 0x%08x\n", s, r)
static void print_ehci_regs(struct exynos5430_ehci_regs *base)
{
	pr_info("------- EHCI reg dump -------\n");
	pr_reg("HCCPBASE", base->hc_cap_base);
	pr_reg("HCSPARAMS", base->hc_sparams);
	pr_reg("HCCPARAMS", base->hc_cparams);

	pr_reg("USBCMD", base->usb_cmd);
	pr_reg("USBSTS", base->usb_sts);
	pr_reg("USBINTR", base->usb_intr);
	pr_reg("FRINDEX", base->fr_index);
	pr_reg("CTRLDSSEGMENT", base->ctrl_ds_segment);
	pr_reg("PERIODICLISTBASE", base->periodic_list_base);
	pr_reg("ASYNCLISTADDR", base->async_list_addr);

	pr_reg("CONFIGFLAG", base->config_flag);
	pr_reg("PORTSC_0", base->port_sc0);
	pr_reg("PORTSC_1", base->port_sc1);

	pr_reg("INSNREG00", base->insnreg00);
	pr_reg("INSNREG01", base->insnreg01);
	pr_reg("INSNREG02", base->insnreg02);
	pr_reg("INSNREG03", base->insnreg03);
	pr_reg("INSNREG04", base->insnreg04);
	pr_reg("INSNREG05", base->insnreg05);
	pr_reg("INSNREG06", base->insnreg06);
	pr_reg("INSNREG07", base->insnreg07);
	pr_info("-----------------------------\n");
}

static void print_phy_regs(struct exynos5430_phy_regs *base)
{
	pr_info("------- PHY reg dump -------\n");
	pr_reg("HOSTPHYCTRL0", base->host_phy_ctrl0);
	pr_reg("HOSTPHYTUNE0", base->host_phy_tune0);

	pr_reg("HSICPHYCTRL1", base->hsic_phy_ctrl1);
	pr_reg("HSICPHYTUNE1", base->hsic_phy_tune1);

	pr_reg("HOSTEHCICTRL", base->host_ehci_ctrl);
	pr_reg("HOSTOHCICTRL", base->host_ohci_ctrl);
	pr_info("-----------------------------\n");
}

static void debug_ehci_reg_dump(void)
{
	print_ehci_regs(ehci_reg);
	print_phy_regs(phy_reg);
}

static unsigned int debug_get_frame_nr(void)
{
	return ehci_reg->fr_index;
}

static int ehci_port_reg_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	if (ehci_reg) {
		mif_info("port reg aleady initialized\n");
		return -EBUSY;
	}

	ehci_reg = of_iomap(dev->of_node, 0);
	if (IS_ERR(ehci_reg)) {
		mif_err("fail to get port reg address\n");
		return -EINVAL;
	}
	mif_info("port reg get success (%p)\n", ehci_reg);

	if (phy_reg) {
		mif_info("phy reg aleady initialized\n");
		return -EBUSY;
	}

	phy_reg = of_iomap(dev->of_node, 1);
	if (IS_ERR(phy_reg)) {
		mif_err("fail to get phy reg address\n");
		return -EINVAL;
	}
	mif_info("phy reg get success (%p)\n", phy_reg);

	return 0;
}
#else
static inline void debug_ehci_reg_dump(void) { return; }
static inline int ehci_port_reg_init(struct platform_device *pdev) { return 0; }
static inline unsigned int debug_get_frame_nr(void) { return 0; }
#endif

static int xmm626x_cp_force_crash_exit(void)
{
	struct device_node *np = NULL;
	unsigned gpio_ap_dump_int;

	np = of_find_compatible_node(NULL, NULL, "sec_modem,modem_pdata");
	if (!np) {
		mif_err("DT, failed to get dp\n");
		return -EINVAL;
	}

	gpio_ap_dump_int = of_get_named_gpio(np, "mif,gpio_ap_dump_int", 0);
	if (!gpio_is_valid(gpio_ap_dump_int)) {
		mif_err("ap_dump_int: Invalied gpio pins\n");
		return -EINVAL;
	}

	if (gpio_get_value(gpio_ap_dump_int)) {
		gpio_set_value(gpio_ap_dump_int, 0);
		msleep(20);
	}
	gpio_set_value(gpio_ap_dump_int, 1);
	msleep(20);
	mif_info("set ap_dump_int(%d) to high=%d\n",
		gpio_ap_dump_int, gpio_get_value(gpio_ap_dump_int));
	gpio_set_value(gpio_ap_dump_int, 0);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int xmm626x_linkpm_phy_notify(struct notifier_block *nfb,
						unsigned long event, void *arg)
{
	struct xmm626x_linkpm_data *pmdata =
			container_of(nfb, struct xmm626x_linkpm_data, phy_nfb);
	struct modemlink_pm_data *pdata = pmdata->pdata;

	mif_info("Enter: event(%ld)\n", event);

	if (pmdata->link_connected) {
		switch (event) {
		case STATE_HSIC_LPA_ENTER:
			gpio_set_value(pdata->gpio_link_active, 0);
			mif_info("enter: active state(%d)\n",
				gpio_get_value(pdata->gpio_link_active));
		break;
		case STATE_HSIC_PHY_SHUTDOWN:
			gpio_set_value(pdata->gpio_link_active, 0);
			mif_info("phy_exit: active state(%d)\n",
				gpio_get_value(pdata->gpio_link_active));
		break;
		case STATE_HSIC_CHECK_HOSTWAKE:
			if (get_hostwake(pmdata))
				return NOTIFY_BAD;
		break;
		}
	}

	return NOTIFY_DONE;
}
#endif

static int xmm626x_gpio_export(struct device *dev,
			unsigned gpio, bool dir, const char *name)
{
	int ret = 0;

	if (!gpio_is_valid(gpio)) {
		mif_err("%s: Invalied gpio pins\n", name);
		return -EINVAL;
	}

	ret = gpio_request(gpio, name);
	if (ret)
		mif_err("%s: failed to request gpio (%d)\n", name, ret);

	ret = gpio_export(gpio, dir);
	if (ret)
		mif_err("%s: failed to export gpio (%d)\n", name, ret);

	ret = gpio_export_link(dev, name, gpio);
	if (ret)
		mif_err("%s: failed to export link_gpio (%d)\n", name, ret);

	return 0;
}

static int parse_dt_common_pdata(struct device_node *np,
				struct modemlink_pm_data *pdata)
{
	u32 val;

	mif_dt_read_string(np, "linkpm,name", pdata->name);
	mif_dt_read_u32(np, "linkpm,port", pdata->port);

	return 0;
}

static int parse_dt_gpio_pdata(struct device_node *np,
				struct modemlink_pm_data *pdata)
{
	unsigned gpio_suspend_req;
	struct device *dev = pdata->dev;
	int ret = 0;

	/* GPIO_LINK_ACTIVE */
	pdata->gpio_link_active =
		of_get_named_gpio(np, "linkpm,gpio_link_active", 0);
	ret = xmm626x_gpio_export(dev,
			pdata->gpio_link_active, false, "link_active");
	if (ret)
		return ret;
	gpio_direction_output(pdata->gpio_link_active, 0);

	/* GPIO_LINK_HOSTWAKE */
	pdata->gpio_link_hostwake =
		of_get_named_gpio(np, "linkpm,gpio_link_hostwake", 0);
	ret = xmm626x_gpio_export(dev,
			pdata->gpio_link_hostwake, false, "link_hostwake");
	if (ret)
		return ret;

	/* GPIO_LINK_SLAVEWAKE */
	pdata->gpio_link_slavewake =
		of_get_named_gpio(np, "linkpm,gpio_link_slavewake", 0);
	ret = xmm626x_gpio_export(dev,
			pdata->gpio_link_slavewake, false, "link_slavewake");
	if (ret)
		return ret;
	gpio_direction_output(pdata->gpio_link_slavewake, 0);

	gpio_suspend_req = of_get_named_gpio(np, "linkpm,gpio_suspend_req", 0);
	if (!gpio_is_valid(gpio_suspend_req)) {
		mif_err("suspend_req: Invalied gpio pins\n");
		return -EINVAL;
	}

	ret = gpio_request(gpio_suspend_req, "suspend_req");
	if (ret)
		mif_err("%s: failed to request gpio with (%d)\n",
							"suspend_req", ret);
	gpio_direction_output(gpio_suspend_req, 0);
	/* s3c_gpio_setpull(GPIO_SUSPEND_REQUEST, S3C_GPIO_PULL_NONE); */

	return ret;
}

static struct modemlink_pm_data *link_pm_parse_dt_pdata(struct device *dev)
{
	struct modemlink_pm_data *pdata = NULL;

	pdata = kzalloc(sizeof(struct modemlink_pm_data), GFP_KERNEL);
	if (!pdata) {
		mif_err("modemlink_pm_data: alloc fail\n");
		return ERR_PTR(-ENOMEM);
	}

	pdata->dev = dev;

	if (parse_dt_common_pdata(dev->of_node, pdata)) {
		mif_err("DT error: failed to parse common\n");
		goto error;
	}

	if (parse_dt_gpio_pdata(dev->of_node, pdata)) {
		mif_err("DT error: failed to parse gpio\n");
		goto error;
	}

	mif_info("DT parse complete!\n");
	return pdata;

error:
	if (pdata)
		kfree(pdata);
	return ERR_PTR(-EINVAL);
}


static const struct of_device_id link_pm_match[] = {
	{ .compatible = "sec_modem,linkpm_pdata", },
	{},
};
MODULE_DEVICE_TABLE(of, link_pm_match);
#else
static struct modemlink_pm_data *link_pm_parse_dt_pdata(struct device *dev)
{
	return ERR_PTR(-ENODEV);
}
static inline int xmm626x_cp_force_crash_exit(void) { return -EINVAL; }
#ifdef EHCI_REG_DUMP
static int ehci_port_reg_init(struct platform_device *pdev) { return -EINVAL; }
#endif
#endif

static int xmm626x_linkpm_known_device(struct xmm626x_linkpm_data *pmdata,
				const struct usb_device_descriptor *desc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(xmm626x_ids); i++) {
		if (xmm626x_ids[i].vid == desc->idVendor &&
					xmm626x_ids[i].pid == desc->idProduct) {
			mif_debug("### vid=0x%x, pid=0x%x\n", desc->idVendor,
				desc->idProduct);
			return xmm626x_ids[i].type;
		}
	}
	return MIF_UNKNOWN_DEVICE;
}

#ifdef CONFIG_PM_RUNTIME
static struct xmm626x_linkpm_data *linkdata_from_udev(struct usb_device *udev)
{
	struct xmm626x_linkpm_data *pmdata = NULL;

	spin_lock_bh(&xmm626x_devices.lock);
	list_for_each_entry(pmdata, &xmm626x_devices.pmdata, link) {
		if (pmdata && (udev == pmdata->udev || udev == pmdata->hdev)) {
			spin_unlock_bh(&xmm626x_devices.lock);
			return pmdata;
		}
		mif_debug("udev=%p, %s\n", udev, dev_name(&udev->dev));
	}
	spin_unlock_bh(&xmm626x_devices.lock);
	return NULL;
}

static void usb_cp_crash(struct usb_device *udev, char *msg)
{
	struct xmm626x_linkpm_data *pmdata = linkdata_from_udev(udev);
	struct modemlink_pm_data *pdata = pmdata->pdata;

	mif_err("Err_msg : %s\n", msg);
	pdata->cp_force_crash_exit();
}

int usb_linkpm_request_resume(struct usb_device *udev) {
	struct xmm626x_linkpm_data *pmdata = linkdata_from_udev(udev);
	struct device *dev;

	if (!pmdata || !pmdata->link_connected)
		return -ENODEV;

	/* already resumed, update lastbusy */
	dev = &pmdata->udev->dev;
	if (dev->power.runtime_status == RPM_ACTIVE) {
		pm_runtime_mark_last_busy(dev);
		return 0;
	}

	/* Hold kernel wakeup status until port resumed */
	wake_lock(&pmdata->tx_wake);

	if (pmdata->dpm_suspending) {
		mif_info("will be resume for TX\n");
		return 0;
	}

	mif_info("resume request for TX\n");
	queue_delayed_work(pmdata->wq, &pmdata->link_pm_work, 0);
	return 0;
}

static void set_slavewake(struct modemlink_pm_data *pm_data, int val)
{
	if (!val) {
		gpio_set_value(pm_data->gpio_link_slavewake, 0);
	} else {
		if (gpio_get_value(pm_data->gpio_link_slavewake)) {
			mif_info("warn.. slavewake toggle\n");
			gpio_set_value(pm_data->gpio_link_slavewake, 0);
			msleep(20);
		}
		gpio_set_value(pm_data->gpio_link_slavewake, 1);
	}
	mif_info("slave wake(%d)\n",
		gpio_get_value(pm_data->gpio_link_slavewake));
}

#define WAIT_CONNECT_CHECK_CNT 10
static void xmm626x_gpio_l3tol0_resume(struct xmm626x_linkpm_data *pmdata)
{
	int cnt = WAIT_CONNECT_CHECK_CNT;

	if (get_hostactive(pmdata))
		return;

	if (!get_hostwake(pmdata)) {
		set_slavewake(pmdata->pdata, 1);
		msleep(20);
	}

	gpio_set_value(pmdata->pdata->gpio_link_active, 1);
	printk(KERN_DEBUG "mif: host active(%d)\n", get_hostactive(pmdata));

	while (cnt-- && !get_hostwake(pmdata)) {
		mif_info("wait cp port power, host wake(%d)\n",
			get_hostwake(pmdata));
		msleep(20);
	}
#ifdef EHCI_REG_DUMP
	if (!get_hostwake(pmdata))
		debug_ehci_reg_dump();
#endif
	return;
}

static int xmm626x_gpio_l2tol0_resume(struct xmm626x_linkpm_data *pmdata)
{
	int spin = 20;

	/* CP initiated L2->L0 */
	if (get_hostwake(pmdata)) {
		mif_debug("CP initiated L2->L0\n");
		goto exit;
	}

	/* AP initiated L2->L0 */
	set_slavewake(pmdata->pdata, 1);

	while (spin-- && !get_hostwake(pmdata))
		mdelay(5);

	if (!get_hostwake(pmdata)) {
		set_slavewake(pmdata->pdata, 0);
		return -ETIMEDOUT;
	}
exit:
	return 0;
}

static int xmm626x_linkpm_usb_resume(struct usb_device *udev, pm_message_t msg)
{
	struct xmm626x_linkpm_data *pmdata = linkdata_from_udev(udev);
	struct device *dev = &udev->dev;
	int ret = 0;
	int cnt = 10;

	if (!pmdata)
		goto generic_resume;

	/* root hub resume
	   If Host active was low before root hub resume, do L3->L0 sequence */
	if (udev == pmdata->hdev) {
		xmm626x_gpio_l3tol0_resume(pmdata);
		goto generic_resume;
	}

	/* get wake lock */
	wake_lock(&pmdata->l2_wake);
	mif_debug("get wakelock\n");

retry:
	/* Sometimes IMC modem send remote wakeup with gpio,
	   we should check the runtime status and if already resumed*/
	if (dev->power.runtime_status == RPM_ACTIVE) {
		mif_info("aleady resume, skip gpio resume\n");
		goto generic_resume;
	}

	ret = xmm626x_gpio_l2tol0_resume(pmdata);
	if (ret < 0 ) {
		if(udev->remote_wake) {
			printk(KERN_INFO "CP remote wake up");
			msleep(5);
			udev->remote_wake = 0;
			goto generic_resume;
		}
		if (cnt--) {
			mif_err("xmm626x_gpio_l2tol0_resume fail(%d)\n", ret);
			goto retry;
		} else  {
			mif_err("hostwakeup fail\n");
			/* TODO: need to check the GPIO timming...
			wake_unlock(&pmdata->l2_wake);
			usb_cp_crash(pmdata->udev, "HostWakeup Fail");
			*/
		}
	}

generic_resume:
	return _usb_resume(udev, msg);
}

static int xmm626x_linkpm_usb_suspend(struct usb_device *udev, pm_message_t msg)
{
	struct xmm626x_linkpm_data *pmdata = linkdata_from_udev(udev);

	if (!pmdata)
		goto generic_suspend;

	if (udev == pmdata->hdev && msg.event == PM_EVENT_SUSPEND) {
		pm_runtime_disable(&udev->dev);
		pm_runtime_set_suspended(&udev->dev);
		pm_runtime_enable(&udev->dev);
		mif_info("Set force root-hub rpm suspend\n");
		goto generic_suspend;
	}

	if (pmdata->pdata->gpio_link_suspend_req
		&& !gpio_get_value(pmdata->pdata->gpio_link_suspend_req)) {
		mif_info("CP not ready to enter L2\n");
		return -EBUSY;
	}

	/* release wake lock with L3 guard time 50ms */
	wake_lock_timeout(&pmdata->l2_wake, msecs_to_jiffies(50));
	mif_debug("release wakelock timeout\n");

generic_suspend:
	return _usb_suspend(udev, msg);
}

static int xmm626x_linkpm_usb_notify(struct notifier_block *nfb,
						unsigned long event, void *arg)
{
	struct xmm626x_linkpm_data *pmdata =
			container_of(nfb, struct xmm626x_linkpm_data, usb_nfb);
	struct usb_device *udev = arg;
	struct usb_device_driver *udriver =
					to_usb_device_driver(udev->dev.driver);
	const struct usb_device_descriptor *desc = &udev->descriptor;
	unsigned long flags;

	switch (event) {
	case USB_DEVICE_ADD:
		switch (xmm626x_linkpm_known_device(pmdata, desc)) {
		case MIF_MAIN_DEVICE:
			if (pmdata->udev) {
				mif_err("pmdata was assigned for udev=%p\n",
								pmdata->udev);
				return NOTIFY_DONE;
			}
			/* clear previous event and stop event work*/
			pmdata->events = 0;
			cancel_delayed_work_sync(&pmdata->link_pm_event);

			pmdata->udev = udev;
			pmdata->hdev = udev->bus->root_hub;
			mif_info("pmdata=%p, udev=%p, hdev=%p\n", pmdata, udev,
								pmdata->hdev);
			spin_lock_irqsave(&pmdata->lock, flags);
			if (!_usb_resume && udriver->resume) {
				_usb_resume = udriver->resume;
				udriver->resume = xmm626x_linkpm_usb_resume;
			}
			if (!_usb_suspend && udriver->suspend) {
				_usb_suspend = udriver->suspend;
				udriver->suspend = xmm626x_linkpm_usb_suspend;
			}
			spin_unlock_irqrestore(&pmdata->lock, flags);
			mif_info("hook: (%pf, %pf), (%pf, %pf)\n",
					_usb_resume, udriver->resume,
					_usb_suspend,	udriver->suspend);
			pmdata->link_connected = MIF_MAIN_DEVICE;
			enable_irq(gpio_to_irq(
					pmdata->pdata->gpio_link_hostwake));
			pmdata->resume_cnt = 0;
			pmdata->dpm_suspending = false;

#ifndef CONFIG_LTE_MODEM_XMM7260
			set_bit(LINKPM_EVENT_RUNTIME, &pmdata->events);
			queue_delayed_work(pmdata->wq, &pmdata->link_pm_event,
							msecs_to_jiffies(500));
#endif
#ifdef CONFIG_LTE_MODEM_XMM7260
			wake_lock(&pmdata->l2_wake);
#endif
			break;
		case MIF_BOOT_DEVICE:
			mif_info("boot dev connected\n");
			pmdata->link_connected = MIF_BOOT_DEVICE;
			break;
		default:
			break;
		}
		break;
	case USB_DEVICE_REMOVE:
		switch (xmm626x_linkpm_known_device(pmdata, desc)) {
		case MIF_MAIN_DEVICE:
			disable_irq(gpio_to_irq(
					pmdata->pdata->gpio_link_hostwake));
			pmdata->link_connected = 0;
			cancel_delayed_work_sync(&pmdata->link_pm_work);
			pmdata->resume_req = false;
			wake_unlock(&pmdata->tx_wake);
			wake_unlock(&pmdata->l2_wake);
			/* clear previous event and stop event work*/
			pmdata->events = 0;
			cancel_delayed_work_sync(&pmdata->link_pm_event);
			mif_info("unhook: (%pf, %pf), (%pf, %pf)\n",
					_usb_resume, udriver->resume,
					_usb_suspend,	udriver->suspend);
			spin_lock_irqsave(&pmdata->lock, flags);
			if (_usb_resume) {
				udriver->resume = _usb_resume;
				_usb_resume = NULL;
			}
			if (_usb_suspend) {
				udriver->suspend = _usb_suspend;
				_usb_suspend = NULL;
			}
			spin_unlock_irqrestore(&pmdata->lock, flags);
			/* set roothub runtime autosuspend delay to default */
			pm_runtime_set_autosuspend_delay
					(&udev->bus->root_hub->dev, 2000);
			pmdata->hdev = NULL;
			pmdata->udev = NULL;
#ifdef EHCI_REG_DUMP
			debug_ehci_reg_dump();
#endif
			break;
		case MIF_BOOT_DEVICE:
			pmdata->link_connected = 0;
			mif_info("boot dev disconnected\n");
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

/* For checking to L3 stats(kernel suspend) */
static int xmm626x_linkpm_pm_notify(struct notifier_block *nfb,
						unsigned long event, void *arg)
{
	struct xmm626x_linkpm_data *pmdata =
			container_of(nfb, struct xmm626x_linkpm_data, pm_nfb);

	if (!pmdata || pmdata->link_connected != MIF_MAIN_DEVICE) {
		mif_info("HSIC not connected, skip\n");
		return NOTIFY_DONE;
	}

	switch (event) {
	case PM_SUSPEND_PREPARE:
		pmdata->dpm_suspending = true;
		cancel_delayed_work_sync(&pmdata->link_pm_work);
		pmdata->resume_req = false;
		break;
	case PM_POST_SUSPEND:
		pmdata->dpm_suspending = false;
		/* L3->L0, CP request resume, TX resume req*/
		if (!get_hostactive(pmdata) || get_hostwake(pmdata)
					|| wake_lock_active(&pmdata->tx_wake)) {
			wake_lock(&pmdata->l2_wake);
			mif_debug("get wakelock\n");
			queue_delayed_work(pmdata->wq, &pmdata->link_pm_work, 0);
		}
		break;
	}
	return NOTIFY_DONE;
}
#else
static int xmm626x_linkpm_usb_notify(struct notifier_block *nfb,
						unsigned long event, void *arg)
{
	struct xmm626x_linkpm_data *pmdata =
			container_of(nfb, struct xmm626x_linkpm_data, usb_nfb);
	struct usb_device *udev = arg;
	const struct usb_device_descriptor *desc = &udev->descriptor;

	switch (event) {
	case USB_DEVICE_ADD:
		switch (xmm626x_linkpm_known_device(pmdata, desc)) {
		case MIF_MAIN_DEVICE:
			if (pmdata->udev) {
				mif_err("pmdata was assigned for udev=%p\n",
								pmdata->udev);
				return NOTIFY_DONE;
			}
			/* clear previous event and stop event work*/
			pmdata->events = 0;
			cancel_delayed_work_sync(&pmdata->link_pm_event);
			pmdata->link_connected = MIF_MAIN_DEVICE;
			break;
		case MIF_BOOT_DEVICE:
			mif_info("boot dev connected\n");
			pmdata->link_connected = MIF_BOOT_DEVICE;
			break;
		default:
			break;
		}
		break;
	case USB_DEVICE_REMOVE:
		switch (xmm626x_linkpm_known_device(pmdata, desc)) {
		case MIF_MAIN_DEVICE:
			pmdata->link_connected = 0;
			cancel_delayed_work_sync(&pmdata->link_pm_work);
			break;
		case MIF_BOOT_DEVICE:
			pmdata->link_connected = 0;
			mif_info("boot dev disconnected\n");
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
#endif

static long link_pm_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct xmm626x_linkpm_data *pmdata =
			(struct xmm626x_linkpm_data *)file->private_data;
	int value;

	mif_debug("%x\n", cmd);

	switch (cmd) {
	case IOCTL_LINK_CONTROL_ENABLE:
		if (copy_from_user(&value, (const void __user *)arg,
							sizeof(int)))
			return -EFAULT;
		if (pmdata->pdata->link_ldo_enable)
			pmdata->pdata->link_ldo_enable(!!value);
		if (pmdata->pdata->gpio_link_enable)
			gpio_set_value(pmdata->pdata->gpio_link_enable, value);
		break;
	case IOCTL_LINK_CONTROL_ACTIVE:
		if (copy_from_user(&value, (const void __user *)arg,
							sizeof(int)))
			return -EFAULT;
		gpio_set_value(pmdata->pdata->gpio_link_active, value);
		break;
	case IOCTL_LINK_GET_HOSTWAKE:
		return !gpio_get_value(pmdata->pdata->gpio_link_hostwake);
	case IOCTL_LINK_CONNECTED:
		mif_info("link_connected(%x)\n", pmdata->link_connected);
		return !!pmdata->link_connected;
	default:
		break;
	}
	return 0;
}

static int link_pm_open(struct inode *inode, struct file *file)
{
	struct xmm626x_linkpm_data *pmdata =
			(struct xmm626x_linkpm_data *)file->private_data;
	/*file->private_data = pmdata->misc*/
	file->private_data = (void *)pmdata;
	return 0;
}

static int link_pm_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static const struct file_operations link_pm_fops = {
	.owner = THIS_MODULE,
	.open = link_pm_open,
	.release = link_pm_release,
	.unlocked_ioctl = link_pm_ioctl,
};

#ifdef CONFIG_PM_RUNTIME
/* CP initiated L2->L0 */
static void link_pm_runtime_work(struct work_struct *work)
{
	int ret;
	struct xmm626x_linkpm_data *pmdata = container_of(work,
				struct xmm626x_linkpm_data, link_pm_work.work);
	struct device *dev = &pmdata->udev->dev;

	mif_debug("rpm_status(%d)\n", dev->power.runtime_status);

	switch (dev->power.runtime_status) {
	case RPM_SUSPENDED:
		if (pmdata->resume_req)
			break;
		pmdata->resume_req = true;
		ret = pm_runtime_resume(dev);
		if (ret < 0) {
			mif_err("resume error(%d)\n", ret);
			/* force to go runtime idle before retry resume */
			if (dev->power.timer_expires == 0 &&
						!dev->power.request_pending) {
				mif_debug("run time idle\n");
				pm_runtime_idle(dev);
				pmdata->resume_req = false;
			}
		}
		break;
	case RPM_SUSPENDING:
		/* Checking the usb_runtime_suspend running time.*/
		mif_info("rpm_states=%d", dev->power.runtime_status);
		msleep(20);
		break;
	default:
		break;
	}

	if (dev->power.runtime_status == RPM_ACTIVE) {	/*resume success*/
		pmdata->resume_cnt = 0;
		pmdata->resume_req = false;
		wake_unlock(&pmdata->tx_wake);
	} else if (pmdata->resume_cnt++ > 30) { /*resume fail over 30 times*/
		pmdata->resume_req = false;
		mif_info("rpm_status(%d), retry_cnt(%d)\n",
			dev->power.runtime_status, pmdata->resume_cnt);
		usb_cp_crash(pmdata->udev, "Runtim Resume timeout");
		wake_unlock(&pmdata->l2_wake);
		wake_unlock(&pmdata->tx_wake);
	} else {				/*wait for runtime resume done*/
		mif_info("rpm (%d), delayed work\n", dev->power.runtime_status);
		queue_delayed_work(pmdata->wq, &pmdata->link_pm_work,
							msecs_to_jiffies(100));
	}
}
#endif

/* link pm event delayed work - runtime start, reconnect, etc.*/
static void link_pm_event_work(struct work_struct *work)
{
	struct xmm626x_linkpm_data *pmdata = container_of(work,
				struct xmm626x_linkpm_data, link_pm_event.work);
	struct usb_device *udev = pmdata->udev;

	mif_info("0x%lx\n", pmdata->events);

	if (test_bit(LINKPM_EVENT_RUNTIME, &pmdata->events) &&
		(pmdata->link_connected == MIF_MAIN_DEVICE)) {
		struct device *dev, *hdev, *roothub;
		mif_info("LINKPM_EVENT_RUNTIME\n");
		pmdata->resume_req = false;
		dev = &udev->dev;
		roothub = &udev->bus->root_hub->dev;
		hdev = udev->bus->root_hub->dev.parent;
		pm_runtime_set_autosuspend_delay(dev, l2_delay);
		pm_runtime_set_autosuspend_delay(roothub, hub_delay);
		pm_runtime_allow(dev);
		pm_runtime_allow(hdev);/*ehci*/
		clear_bit(LINKPM_EVENT_RUNTIME, &pmdata->events);
	}
}

/* Host wakeup interrupt handler */
static irqreturn_t xmm626x_linkpm_hostwake(int irq, void *data)
{
	bool host_wake, slave_wake;
	struct xmm626x_linkpm_data *pmdata = data;

	if (!pmdata || !pmdata->link_connected)
		return IRQ_HANDLED;

	host_wake = get_hostwake(pmdata);
	slave_wake = !!gpio_get_value(pmdata->pdata->gpio_link_slavewake);
	printk(KERN_DEBUG "mif: host wakeup(%d), slave(%d), fr(%u)\n",
				host_wake, slave_wake, debug_get_frame_nr());

	if (slave_wake && !host_wake) {
		gpio_set_value(pmdata->pdata->gpio_link_slavewake, 0);
		printk(KERN_DEBUG "mif: slave(%d)\n",
			gpio_get_value(pmdata->pdata->gpio_link_slavewake));
	}

	/* CP request the resume*/
	if (pmdata->dpm_suspending) {
		mif_info("dpm_suspending, will be resume..\n");
		wake_lock(&pmdata->l2_wake);
		return IRQ_HANDLED;
	}
#ifdef CONFIG_PM_RUNTIME
	if (!slave_wake && host_wake)
		queue_delayed_work(pmdata->wq, &pmdata->link_pm_work, 0);
#endif

	return IRQ_HANDLED;
}


static int xmm626x_linkpm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct xmm626x_linkpm_data *pmdata;
	struct modemlink_pm_data *pdata =
			(struct modemlink_pm_data *)pdev->dev.platform_data;
	int irq;

	if (pdev->dev.of_node) {
		pdata = link_pm_parse_dt_pdata(&pdev->dev);
		if (IS_ERR(pdata)) {
			mif_err("link_pm DT parse error!\n");
			return PTR_ERR(pdata);
		}
		pdata->cp_force_crash_exit = xmm626x_cp_force_crash_exit;
		ehci_port_reg_init(pdev);
	} else {
		if (!pdata) {
			mif_err("Non-DT, incorrect pdata!\n");
			return -EINVAL;
		}
	}

	pmdata = (struct xmm626x_linkpm_data *)
			kzalloc(sizeof(struct xmm626x_linkpm_data), GFP_KERNEL);
	if (!pmdata) {
		mif_err("pmdata alloc fail\n");
		return -ENOMEM;
	}

	dev_set_drvdata(&pdev->dev, pmdata);
	pmdata->pdata = pdata;
	pmdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	pmdata->miscdev.name = "link_pm";
	pmdata->miscdev.fops = &link_pm_fops;
	ret = misc_register(&pmdata->miscdev);
	if (ret < 0) {
		mif_err("fail to register pm device(%d)\n", ret);
		goto err_misc_register;
	}

	/* create work queue & init work for runtime pm */
	pmdata->wq = create_singlethread_workqueue("linkpmd");
	if (!pmdata->wq) {
		mif_err("fail to create wq\n");
		goto err_create_wq;
	}
#ifdef CONFIG_PM_RUNTIME
	INIT_DELAYED_WORK(&pmdata->link_pm_work, link_pm_runtime_work);
#endif
	INIT_DELAYED_WORK(&pmdata->link_pm_event, link_pm_event_work);
	spin_lock_init(&pmdata->lock);
	spin_lock_bh(&xmm626x_devices.lock);
	list_add(&pmdata->link, &xmm626x_devices.pmdata);
	spin_unlock_bh(&xmm626x_devices.lock);

	wake_lock_init(&pmdata->l2_wake, WAKE_LOCK_SUSPEND, "l2_hsic");
	wake_lock_init(&pmdata->tx_wake, WAKE_LOCK_SUSPEND, "tx_hsic");

	irq = gpio_to_irq(pdata->gpio_link_hostwake);
	ret = request_irq(irq, xmm626x_linkpm_hostwake,
		IRQF_NO_SUSPEND | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
		"hostwake", (void *)pmdata);
	if (ret) {
		mif_err("fail to request irq(%d)\n", ret);
		goto err_request_irq;
	}
	disable_irq(irq);
	ret = enable_irq_wake(irq);
	if (ret) {
		mif_err("failed to enable_irq_wake:%d\n", ret);
		goto err_set_wake_irq;
	}

	pmdata->usb_nfb.notifier_call = xmm626x_linkpm_usb_notify;
	usb_register_notify(&pmdata->usb_nfb);
#ifdef CONFIG_PM_RUNTIME
	pmdata->pm_nfb.notifier_call = xmm626x_linkpm_pm_notify;
	register_pm_notifier(&pmdata->pm_nfb);
	pmdata->phy_nfb.notifier_call =	xmm626x_linkpm_phy_notify;
	phy_register_notifier(&pmdata->phy_nfb);
#endif
	mif_info("success\n");
	return 0;

err_set_wake_irq:
	free_irq(irq, (void *)pmdata);
err_request_irq:
	destroy_workqueue(pmdata->wq);
err_create_wq:
	misc_deregister(&pmdata->miscdev);
err_misc_register:
	kfree(pmdata);
	return ret;
}

static int xmm626x_linkpm_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct xmm626x_linkpm_data *pmdata = dev_get_drvdata(&pdev->dev);

	if (!pmdata)
		return 0;

	usb_unregister_notify(&pmdata->usb_nfb);
	unregister_pm_notifier(&pmdata->pm_nfb);
	phy_unregister_notifier(&pmdata->phy_nfb);

	spin_lock_bh(&xmm626x_devices.lock);
	list_del(&pmdata->link);
	spin_unlock_bh(&xmm626x_devices.lock);

	kfree(pmdata);
	dev_set_drvdata(&pdev->dev, NULL);
	mif_info("success\n");

	return ret;
}

static struct platform_driver xmm626x_linkpm_driver = {
	.probe = xmm626x_linkpm_probe,
	.remove = xmm626x_linkpm_remove,
	.driver = {
		.name = "linkpm-xmm626x",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(link_pm_match),
#endif
	},
};

static int __init xmm626x_linkpm_init(void)
{
	int ret;

	INIT_LIST_HEAD(&xmm626x_devices.pmdata);
	spin_lock_init(&xmm626x_devices.lock);
	ret = platform_driver_register(&xmm626x_linkpm_driver);
	if (ret)
		mif_err("xmm626x linkpm register fail: %d\n", ret);
	return ret;
}

static void __exit xmm626x_linkpm_exit(void)
{
	platform_driver_unregister(&xmm626x_linkpm_driver);
}

late_initcall(xmm626x_linkpm_init);
module_exit(xmm626x_linkpm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung Modem Interface Runtime PM Driver");
