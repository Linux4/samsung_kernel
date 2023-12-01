// SPDX-License-Identifier: GPL-2.0
/*
 * xhci-plat.c - xHCI host controller driver platform Bus Glue.
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com
 * Author: Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * A lot of code borrowed from the Linux xHCI driver.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/usb/phy.h>
#include <linux/slab.h>
#include <linux/phy/phy.h>
#include <linux/acpi.h>
#include <linux/usb/of.h>
#include <linux/types.h>
#include "xhci-trace.h"

#include "../core/hub.h"
#include "../core/phy.h"
#include "xhci.h"
#include "xhci-plat.h"
#include "xhci-mvebu.h"
#include "xhci-rcar.h"
#include "../dwc3/dwc3-exynos.h"
#include "../dwc3/exynos-otg.h"
#include "xhci-exynos.h"
#include <soc/samsung/exynos-cpupm.h>

static struct hc_driver __read_mostly xhci_exynos_hc_driver;

static int xhci_exynos_setup(struct usb_hcd *hcd);
static int xhci_exynos_start(struct usb_hcd *hcd);

static int is_rewa_enabled;
static struct xhci_hcd_exynos *g_xhci_exynos;

extern struct usb_xhci_pre_alloc xhci_pre_alloc;
static const struct xhci_driver_overrides xhci_exynos_overrides __initconst = {
	.extra_priv_size = sizeof(struct xhci_exynos_priv),
	.reset = xhci_exynos_setup,
	.start = xhci_exynos_start,
};

#if 0
static void *dma_pre_alloc_coherent(struct xhci_hcd *xhci, size_t size, dma_addr_t * dma_handle, gfp_t gfp)
{
#ifdef CONFIG_SND_EXYNOS_USB_AUDIO_GIC
	return (void *)xhci;
#else
	struct usb_xhci_pre_alloc *xhci_alloc = g_xhci_exynos_audio->xhci_alloc;
	u64 align = size % PAGE_SIZE;
	u64 b_offset = xhci_alloc->offset;

	if (align)
		xhci_alloc->offset = xhci_alloc->offset + size + (PAGE_SIZE - align);
	else
		xhci_alloc->offset = xhci_alloc->offset + size;

	*dma_handle = xhci_alloc->dma + b_offset;

	return (void *)xhci_alloc->pre_dma_alloc + b_offset;
#endif
}
#endif

/*
 * return usb scenario info
 */
int xhci_exynos_scenario_info(void)
{
	return usb_user_scenario;
}

void xhci_exynos_portsc_power_off(void __iomem * portsc, u32 on, u32 prt)
{
	struct xhci_hcd_exynos *xhci_exynos = g_xhci_exynos;

	u32 reg;

	spin_lock(&xhci_exynos->xhcioff_lock);

	pr_info("%s, on=%d portsc_control_priority=%d, prt=%d\n",
		__func__, on, xhci_exynos->portsc_control_priority, prt);

	if (xhci_exynos->portsc_control_priority > prt) {
		spin_unlock(&xhci_exynos->xhcioff_lock);
		return;
	}

	xhci_exynos->portsc_control_priority = prt;

	if (on && !xhci_exynos->port_off_done) {
		pr_info("%s, Do not switch-on port\n", __func__);
		spin_unlock(&xhci_exynos->xhcioff_lock);
		return;
	}

	reg = readl(portsc);

	if (on)
		reg |= PORT_POWER;
	else
		reg &= ~PORT_POWER;

	writel(reg, portsc);
	reg = readl(portsc);

	pr_info("power %s portsc, reg = 0x%x addr = %8x\n", on ? "on" : "off", reg, (u64) portsc);

#if 0
	reg = readl(phycon_base_addr + 0x70);
	if (on)
		reg &= ~DIS_RX_DETECT;
	else
		reg |= DIS_RX_DETECT;

	writel(reg, phycon_base_addr + 0x70);
	pr_info("phycon ess_ctrl = 0x%x\n", readl(phycon_base_addr + 0x70));
#endif

	if (on)
		xhci_exynos->port_off_done = 0;
	else
		xhci_exynos->port_off_done = 1;

	spin_unlock(&xhci_exynos->xhcioff_lock);
}

int xhci_exynos_portsc_set(u32 on)
{
	if (g_xhci_exynos->usb3_portsc != NULL && !on) {
		xhci_exynos_portsc_power_off(g_xhci_exynos->usb3_portsc, 0, 2);
		g_xhci_exynos->port_set_delayed = 0;
		return 0;
	}
	if (!on)
		g_xhci_exynos->port_set_delayed = 1;
	pr_info("%s, usb3_portsc is NULL\n", __func__);
	return -EIO;
}
int xhci_exynos_port_power_set(u32 on, u32 prt)
{
	if (!g_xhci_exynos)
		return -EACCES;

	if (g_xhci_exynos->usb3_portsc) {
		xhci_exynos_portsc_power_off(g_xhci_exynos->usb3_portsc, on, prt);
		return 0;
	}

	pr_info("%s, usb3_portsc is NULL\n", __func__);
	return -EIO;
}

EXPORT_SYMBOL_GPL(xhci_exynos_port_power_set);

static int xhci_exynos_check_port(struct usb_device *dev, bool on)
{
	struct usb_device *hdev;
	struct usb_device *udev = dev;
	struct device *ddev = &udev->dev;
	struct xhci_hcd_exynos *xhci_exynos = g_xhci_exynos;
	enum usb_port_state pre_state;
	int usb3_hub_detect = 0;
	int usb2_detect = 0;
	int port;
	int bInterfaceClass = 0;

	if (udev->bus->root_hub == udev) {
		pr_info("this dev is a root hub\n");
		goto skip;
	}

	pre_state = xhci_exynos->port_state;

	/* Find root hub */
	hdev = udev->parent;
	if (!hdev)
		goto skip;

	hdev = dev->bus->root_hub;
	if (!hdev)
		goto skip;
	pr_info("root hub maxchild = %d\n", hdev->maxchild);

	/* check all ports */
	usb_hub_for_each_child(hdev, port, udev) {
		dev_dbg(ddev, "%s, class = %d, speed = %d\n", __func__, udev->descriptor.bDeviceClass, udev->speed);
		dev_dbg(ddev, "udev = 0x%8x, state = %d\n", udev, udev->state);
		if (udev && udev->state == USB_STATE_CONFIGURED) {
			if (!dev->config->interface[0])
				continue;

			bInterfaceClass = udev->config->interface[0]
			    ->cur_altsetting->desc.bInterfaceClass;
			if (on) {
#if defined(CONFIG_USB_HOST_SAMSUNG_FEATURE)
				if (bInterfaceClass == USB_CLASS_AUDIO) {
#else
				if (bInterfaceClass == USB_CLASS_HID || bInterfaceClass == USB_CLASS_AUDIO) {
#endif
					udev->do_remote_wakeup =
					    (udev->config->desc.bmAttributes & USB_CONFIG_ATT_WAKEUP) ? 1 : 0;
					if (udev->do_remote_wakeup == 1) {
						device_init_wakeup(ddev, 1);
						usb_enable_autosuspend(dev);
					}
					dev_dbg(ddev, "%s, remote_wakeup = %d\n", __func__, udev->do_remote_wakeup);
				}
			}
			if (bInterfaceClass == USB_CLASS_HUB) {
				xhci_exynos->port_state = PORT_HUB;
				usb3_hub_detect = 1;
				break;
			} else if (bInterfaceClass == USB_CLASS_BILLBOARD) {
				xhci_exynos->port_state = PORT_DP;
				usb3_hub_detect = 1;
				break;
			}

			if (udev->speed >= USB_SPEED_SUPER) {
				xhci_exynos->port_state = PORT_USB3;
				usb3_hub_detect = 1;
				break;
			} else {
				xhci_exynos->port_state = PORT_USB2;
				usb2_detect = 1;
			}
		} else {
			pr_info("not configured, state = %d\n", udev->state);
		}
	}

	if (!usb3_hub_detect && !usb2_detect)
		xhci_exynos->port_state = PORT_EMPTY;

	pr_info("%s %s state pre=%d now=%d\n", __func__, on ? "on" : "off", pre_state, xhci_exynos->port_state);

	return xhci_exynos->port_state;

 skip:
	return -EINVAL;
}

static void xhci_exynos_set_port(struct usb_device *dev, bool on)
{
	int port;

	port = xhci_exynos_check_port(dev, on);
	if (g_xhci_exynos->dp_use == true)
		port = PORT_DP;

	switch (port) {
	case PORT_EMPTY:
		pr_info("Port check empty\n");
		is_otg_only = 1;
		if (!g_xhci_exynos->usb3_phy_control) {
			usb_power_notify_control(0, 1);
			g_xhci_exynos->usb3_phy_control = true;
		}
		xhci_exynos_port_power_set(1, 1);
		break;
	case PORT_USB2:
		pr_info("Port check usb2\n");
		is_otg_only = 0;
		xhci_exynos_port_power_set(0, 1);
		usb_power_notify_control(0, 0);
		g_xhci_exynos->usb3_phy_control = false;
		//schedule_delayed_work(&g_dwc->usb_qos_lock_delayed_work,
		//              msecs_to_jiffies(USB_BUS_CLOCK_DELAY_MS));
		break;
	case PORT_USB3:
		/* xhci_port_power_set(1, 1); */
		is_otg_only = 0;
		pr_info("Port check usb3\n");
		break;
	case PORT_HUB:
		/*xhci_exynos_port_power_set(1, 1); */
		pr_info("Port check hub\n");
		is_otg_only = 0;
		break;
	case PORT_DP:
		/*xhci_exynos_port_power_set(1, 1); */
		pr_info("Port check DP\n");
		is_otg_only = 0;
		break;
	default:
		break;
	}
}

int xhci_exynos_inform_dp_use(int use, int lane_cnt)
{
	int ret = 0;

	pr_info("[%s] dp use = %d, lane_cnt = %d\n", __func__, use, lane_cnt);

#if IS_ENABLED(CONFIG_SND_EXYNOS_USB_AUDIO)
	if (!otg_connection || !g_xhci_exynos)
#else
	if (!g_xhci_exynos)
#endif
		return 0;

	if (use == 1) {
		g_xhci_exynos->dp_use = true;
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
		if (g_xhci_exynos->usb3_phy_control == false) {
			usb_power_notify_control(1, 1);
			g_xhci_exynos->usb3_phy_control = true;
		}
		if (lane_cnt == 4) {
			exynos_usbdrd_dp_use_notice(lane_cnt);
			ret = xhci_exynos_port_power_set(0, 2);
		}
#endif
		ret = xhci_exynos_portsc_set(0);
		udelay(1);
	} else {
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
		if (g_xhci_exynos->port_state == PORT_USB2) {
			if (g_xhci_exynos->usb3_phy_control == true) {
				usb_power_notify_control(1, 0);
				g_xhci_exynos->usb3_phy_control = false;
			}
		}
#endif
		g_xhci_exynos->dp_use = false;
	}

	return ret;
}

EXPORT_SYMBOL(xhci_exynos_inform_dp_use);

static int xhci_exynos_power_notify(struct notifier_block *self, unsigned long action, void *dev)
{
	switch (action) {
	case USB_DEVICE_ADD:
		xhci_exynos_set_port(dev, 1);
		break;
	case USB_DEVICE_REMOVE:
		xhci_exynos_set_port(dev, 0);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block dev_nb = {
	.notifier_call = xhci_exynos_power_notify,
};

void xhci_exynos_register_notify(void)
{
	is_otg_only = 1;
	g_xhci_exynos->port_state = PORT_EMPTY;
	usb_register_notify(&dev_nb);
}

void xhci_exynos_unregister_notify(void)
{
	usb_unregister_notify(&dev_nb);
}

static void xhci_priv_exynos_start(struct usb_hcd *hcd)
{
	struct xhci_exynos_priv *priv = hcd_to_xhci_exynos_priv(hcd);

	if (priv->plat_start)
		priv->plat_start(hcd);
}

static int xhci_priv_init_quirk(struct usb_hcd *hcd)
{
	struct xhci_exynos_priv *priv = hcd_to_xhci_exynos_priv(hcd);

	if (!priv->init_quirk)
		return 0;

	return priv->init_quirk(hcd);
}

static int xhci_priv_suspend_quirk(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (!priv->suspend_quirk)
		return 0;

	return priv->suspend_quirk(hcd);
}

static int xhci_priv_resume_quirk(struct usb_hcd *hcd)
{
	struct xhci_exynos_priv *priv = hcd_to_xhci_exynos_priv(hcd);

	if (!priv->resume_quirk)
		return 0;

	return priv->resume_quirk(hcd);
}

static void xhci_exynos_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	struct xhci_exynos_priv *priv = xhci_to_exynos_priv(xhci);

	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT | priv->quirks;
}

/* called during probe() after chip reset completes */
static int xhci_exynos_setup(struct usb_hcd *hcd)
{
	int ret;

	ret = xhci_priv_init_quirk(hcd);
	if (ret)
		return ret;

	ret = xhci_gen_setup(hcd, xhci_exynos_quirks);

	return ret;
}

static int xhci_exynos_start(struct usb_hcd *hcd)
{
	int ret;
	xhci_priv_exynos_start(hcd);
	ret = xhci_run(hcd);

	return ret;
}

static ssize_t xhci_exynos_ss_compliance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	u32 reg;
	void __iomem *reg_base;

	reg_base = hcd->regs;
	reg = readl(reg_base + PORTSC_OFFSET);

	return sysfs_emit(buf, "0x%x\n", reg);
}

static ssize_t
xhci_exynos_ss_compliance_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	int value;
	u32 reg;
	void __iomem *reg_base;

	if (kstrtoint(buf, 10, &value))
		return -EINVAL;

	reg_base = hcd->regs;

	if (value == 1) {
		/* PORTSC PLS is set to 10, LWS to 1 */
		reg = readl(reg_base + PORTSC_OFFSET);
		reg &= ~((0xF << 5) | (1 << 16));
		reg |= (10 << 5) | (1 << 16);
		writel(reg, reg_base + PORTSC_OFFSET);
		pr_info("SS host compliance enabled portsc 0x%x\n", reg);
	} else
		pr_info("Only 1 is allowed for input value\n");

	return n;
}

static DEVICE_ATTR_RW(xhci_exynos_ss_compliance);

static struct attribute *xhci_exynos_attrs[] = {
	&dev_attr_xhci_exynos_ss_compliance.attr,
	NULL
};

ATTRIBUTE_GROUPS(xhci_exynos);

#ifdef CONFIG_OF
static const struct xhci_plat_priv xhci_plat_marvell_armada = {
	.init_quirk = xhci_mvebu_mbus_init_quirk,
};

static const struct xhci_plat_priv xhci_plat_marvell_armada3700 = {
	.init_quirk = xhci_mvebu_a3700_init_quirk,
};

static const struct xhci_plat_priv xhci_plat_renesas_rcar_gen2 = {
	SET_XHCI_PLAT_PRIV_FOR_RCAR(XHCI_RCAR_FIRMWARE_NAME_V1)
};

static const struct xhci_plat_priv xhci_plat_renesas_rcar_gen3 = {
	SET_XHCI_PLAT_PRIV_FOR_RCAR(XHCI_RCAR_FIRMWARE_NAME_V3)
};

static const struct of_device_id usb_xhci_of_match[] = {
	{
	 .compatible = "generic-xhci",
	 }, {
	     .compatible = "xhci-platform",
	     },
	{},
};

MODULE_DEVICE_TABLE(of, usb_xhci_of_match);
#endif

static void xhci_pm_runtime_init(struct device *dev)
{
	dev->power.runtime_status = RPM_SUSPENDED;
	dev->power.idle_notification = false;

	dev->power.disable_depth = 1;
	atomic_set(&dev->power.usage_count, 0);

	dev->power.runtime_error = 0;

	atomic_set(&dev->power.child_count, 0);
	pm_suspend_ignore_children(dev, false);
	dev->power.runtime_auto = true;

	dev->power.request_pending = false;
	dev->power.request = RPM_REQ_NONE;
	dev->power.deferred_resume = false;
	dev->power.accounting_timestamp = jiffies;

	dev->power.timer_expires = 0;
	init_waitqueue_head(&dev->power.wait_queue);
}

/*
 * Set bypass = 1 will skip USB suspend.
 */
static void usb_vendor_dev_suspend(struct usb_device *udev,
				   int *bypass)
{
	int user_scenario;
	struct usb_device *hdev;

	if (!udev || !udev->bus || !udev->bus->root_hub) {
		pr_info("%s: bypass udev 0\n", __func__);
		*bypass = 0;
		return;
	}

	hdev = udev->bus->root_hub;

	if (!g_xhci_exynos->hcd) {
		pr_info("%s: hcd 0\n", __func__);
		*bypass = 0;
		return;
	}

	user_scenario = xhci_exynos_scenario_info();
	pr_info("%s: scenario = %d, pm = %d\n", __func__,
		user_scenario, xhci_exynos_pm_state);
	if (user_scenario == AUDIO_MODE_IN_CALL)
		*bypass = 1;
	else
		*bypass = 0;

}

/*
 * Set bypass = 1 will skip USB resume.
 */
static void usb_vendor_dev_resume(struct usb_device *udev,
				  int *bypass)
{
	int user_scenario;
	struct usb_device *hdev;

	if (!udev || !udev->bus || !udev->bus->root_hub) {
		pr_info("%s: bypass udev 0\n", __func__);
		*bypass = 0;
		return;
	}

	hdev = udev->bus->root_hub;

	if (!g_xhci_exynos->hcd) {
		pr_info("%s: hcd 0\n", __func__);
		*bypass = 0;
		return;
	}

	user_scenario = xhci_exynos_scenario_info();
	pr_info("%s: scenario = %d, pm = %d\n", __func__,
		user_scenario, xhci_exynos_pm_state);
	/* bus resume should be called in suspend state in call mode */
	//if (user_scenario == AUDIO_MODE_IN_CALL &&
	//		xhci_exynos_pm_state == BUS_RESUME && main_hcd)
	if (user_scenario == AUDIO_MODE_IN_CALL)
		*bypass = 1;
	else
		*bypass = 0;

}

static int xhci_exynos_probe(struct platform_device *pdev)
{
	struct device *parent = pdev->dev.parent;
	const struct hc_driver *driver;
	struct device *sysdev, *tmpdev;
	struct xhci_hcd *xhci;
	struct xhci_hcd_exynos *xhci_exynos;
	struct xhci_exynos_priv *priv;
	struct resource *res;
	struct usb_hcd *hcd;
	struct usb_device *hdev;
	struct usb_hub *hub;
	struct usb_port *port_dev;
	int ret;
	int irq;

	struct wakeup_source *main_wakelock, *shared_wakelock;
	int value;

	dev_info(&pdev->dev, "XHCI PLAT START\n");

	main_wakelock = wakeup_source_register(&pdev->dev, dev_name(&pdev->dev));
	__pm_stay_awake(main_wakelock);

	/* Initialization shared wakelock for SS HCD */
	shared_wakelock = wakeup_source_register(&pdev->dev, dev_name(&pdev->dev));
	__pm_stay_awake(shared_wakelock);

	is_rewa_enabled = 0;
	usb_user_scenario = 0;

	if (usb_disabled())
		return -ENODEV;

	driver = &xhci_exynos_hc_driver;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	/*
	 * sysdev must point to a device that is known to the system firmware
	 * or PCI hardware. We handle these three cases here:
	 * 1. xhci_plat comes from firmware
	 * 2. xhci_plat is child of a device from firmware (dwc3-plat)
	 * 3. xhci_plat is grandchild of a pci device (dwc3-pci)
	 */
	for (sysdev = &pdev->dev; sysdev; sysdev = sysdev->parent) {
		if (is_of_node(sysdev->fwnode) || is_acpi_device_node(sysdev->fwnode))
			break;
#ifdef CONFIG_PCI
		else if (sysdev->bus == &pci_bus_type)
			break;
#endif
	}

	if (!sysdev)
		sysdev = &pdev->dev;

	/* Try to set 64-bit DMA first */
	if (WARN_ON(!sysdev->dma_mask))
		/* Platform did not initialize dma_mask */
		ret = dma_coerce_mask_and_coherent(sysdev, DMA_BIT_MASK(64));
	else
		ret = dma_set_mask_and_coherent(sysdev, DMA_BIT_MASK(64));

	/* If seting 64-bit DMA mask fails, fall back to 32-bit DMA mask */
	if (ret) {
		ret = dma_set_mask_and_coherent(sysdev, DMA_BIT_MASK(32));
		if (ret)
			return ret;
	}

	xhci_pm_runtime_init(&pdev->dev);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_noresume(&pdev->dev);

	hcd = __usb_create_hcd(driver, sysdev, &pdev->dev, dev_name(&pdev->dev), NULL);
	if (!hcd) {
		ret = -ENOMEM;
		goto disable_runtime;
	}
	hcd->skip_phy_initialization = 1;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hcd->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(hcd->regs)) {
		ret = PTR_ERR(hcd->regs);
		goto put_hcd;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	xhci = hcd_to_xhci(hcd);

	g_xhci_exynos = devm_kzalloc(&pdev->dev, sizeof(struct xhci_hcd_exynos), GFP_KERNEL);

	if (g_xhci_exynos)
		xhci_exynos = g_xhci_exynos;
	else
		goto put_hcd;

	xhci_exynos->dev = &pdev->dev;

	xhci_exynos->hcd = (struct usb_hcd *)platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, xhci_exynos);

	spin_lock_init(&xhci_exynos->xhcioff_lock);

	xhci_exynos->port_off_done = 0;
	xhci_exynos->portsc_control_priority = 0;
	xhci_exynos->usb3_phy_control = true;
	xhci_exynos->dp_use = 0;

	xhci_exynos->usb3_portsc = hcd->regs + PORTSC_OFFSET;
	if (xhci_exynos->port_set_delayed) {
		pr_info("port power set delayed\n");
		xhci_exynos_portsc_power_off(xhci_exynos->usb3_portsc, 0, 2);
		xhci_exynos->port_set_delayed = 0;
	}

	main_hcd_wakelock = main_wakelock;
	shared_hcd_wakelock = shared_wakelock;

	xhci_exynos_register_notify();

	/*
	 * Not all platforms have clks so it is not an error if the
	 * clock do not exist.
	 */
	xhci->reg_clk = devm_clk_get_optional(&pdev->dev, "reg");
	if (IS_ERR(xhci->reg_clk)) {
		ret = PTR_ERR(xhci->reg_clk);
		goto put_hcd;
	}

	ret = clk_prepare_enable(xhci->reg_clk);
	if (ret)
		goto put_hcd;

	xhci->clk = devm_clk_get_optional(&pdev->dev, NULL);
	if (IS_ERR(xhci->clk)) {
		ret = PTR_ERR(xhci->clk);
		goto disable_reg_clk;
	}

	ret = clk_prepare_enable(xhci->clk);
	if (ret)
		goto disable_reg_clk;

	priv = hcd_to_xhci_exynos_priv(hcd);
	priv->xhci_exynos = xhci_exynos;

	device_wakeup_enable(hcd->self.controller);

	xhci->main_hcd = hcd;
	xhci->shared_hcd = __usb_create_hcd(driver, sysdev, &pdev->dev, dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto disable_clk;
	}
	xhci->shared_hcd->skip_phy_initialization = 1;

	xhci_exynos->shared_hcd = xhci->shared_hcd;

	/* imod_interval is the interrupt moderation value in nanoseconds. */
	xhci->imod_interval = 40000;

	/* Iterate over all parent nodes for finding quirks */
	for (tmpdev = &pdev->dev; tmpdev; tmpdev = tmpdev->parent) {

		if (device_property_read_bool(tmpdev, "usb2-lpm-disable"))
			xhci->quirks |= XHCI_HW_LPM_DISABLE;

		if (device_property_read_bool(tmpdev, "usb3-lpm-capable"))
			xhci->quirks |= XHCI_LPM_SUPPORT;

		if (device_property_read_bool(tmpdev, "quirk-broken-port-ped"))
			xhci->quirks |= XHCI_BROKEN_PORT_PED;

		device_property_read_u32(tmpdev, "imod-interval-ns", &xhci->imod_interval);
	}

	hcd->usb_phy = devm_usb_get_phy_by_phandle(sysdev, "usb-phy", 0);
	if (IS_ERR(hcd->usb_phy)) {
		ret = PTR_ERR(hcd->usb_phy);
		if (ret == -EPROBE_DEFER)
			goto put_usb3_hcd;
		hcd->usb_phy = NULL;
	} else {
		ret = usb_phy_init(hcd->usb_phy);
		if (ret)
			goto put_usb3_hcd;
	}

	/* Get USB2.0 PHY for main hcd */
	if (parent) {
		xhci_exynos->phy_usb2 = devm_phy_get(parent, "usb2-phy");
		if (IS_ERR_OR_NULL(xhci_exynos->phy_usb2)) {
			xhci_exynos->phy_usb2 = NULL;
			dev_err(&pdev->dev, "%s: failed to get phy\n", __func__);
		}
	}

	/* Get USB3.0 PHY to tune the PHY */
	if (parent) {
		xhci_exynos->phy_usb3 = devm_phy_get(parent, "usb3-phy");
		if (IS_ERR_OR_NULL(xhci_exynos->phy_usb3)) {
			xhci_exynos->phy_usb3 = NULL;
			dev_err(&pdev->dev, "%s: failed to get phy\n", __func__);
		}
	}

	ret = of_property_read_u32(parent->of_node, "xhci_l2_support", &value);
	if (ret == 0 && value == 1)
		xhci->quirks |= XHCI_L2_SUPPORT;
	else {
		dev_err(&pdev->dev, "can't get xhci l2 support, error = %d\n", ret);
	}

	hcd->tpl_support = of_usb_host_tpl_support(sysdev->of_node);
	xhci->shared_hcd->tpl_support = hcd->tpl_support;
	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret)
		goto disable_usb_phy;

	if (HCC_MAX_PSA(xhci->hcc_params) >= 4)
		xhci->shared_hcd->can_do_streams = 1;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret)
		goto dealloc_usb2_hcd;

	/* Set port_dev quirks for reduce port initialize time */
	hdev = xhci->main_hcd->self.root_hub;
	hub = usb_get_intfdata(hdev->actconfig->interface[0]);
	port_dev = hub->ports[0];
	port_dev->quirks |= USB_PORT_QUIRK_FAST_ENUM;

	/* Set port_dev quirks for reduce port initialize time */
	hdev = xhci->shared_hcd->self.root_hub;
	hub = usb_get_intfdata(hdev->actconfig->interface[0]);
	if (hub) {
		port_dev = hub->ports[0];
		port_dev->quirks |= USB_PORT_QUIRK_FAST_ENUM;
	}

	xhci_exynos_pm_state = BUS_RESUME;

	device_enable_async_suspend(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);

	device_set_wakeup_enable(&xhci->main_hcd->self.root_hub->dev, 1);

#ifdef CONFIG_EXYNOS_USBDRD_PHY30
	device_set_wakeup_enable(&xhci->shared_hcd->self.root_hub->dev, 1);
#else
	device_set_wakeup_enable(&xhci->shared_hcd->self.root_hub->dev, 0);
#endif

	/*
	 * Prevent runtime pm from being on as default, users should enable
	 * runtime pm using power/control in sysfs.
	 */
	pm_runtime_forbid(&pdev->dev);

	return 0;

 dealloc_usb2_hcd:
	usb_remove_hcd(hcd);

 disable_usb_phy:
	usb_phy_shutdown(hcd->usb_phy);

 put_usb3_hcd:
	usb_put_hcd(xhci->shared_hcd);

 disable_clk:
	clk_disable_unprepare(xhci->clk);

 disable_reg_clk:
	clk_disable_unprepare(xhci->reg_clk);

 put_hcd:
	xhci_exynos_unregister_notify();
	usb_put_hcd(hcd);

 disable_runtime:
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	if (main_wakelock) {
		wakeup_source_unregister(main_wakelock);
		pr_err("%s: unregister main_wakelock", __func__);
	}
	if (shared_wakelock) {
		wakeup_source_unregister(shared_wakelock);
		pr_err("%s: unregister shared_wakelock", __func__);
	}

	return ret;
}

static int xhci_exynos_remove(struct platform_device *dev)
{
	struct xhci_hcd_exynos *xhci_exynos = platform_get_drvdata(dev);
	struct usb_hcd	*hcd = xhci_exynos->hcd;
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct clk *clk = xhci->clk;
	struct clk *reg_clk = xhci->reg_clk;
	struct usb_hcd *shared_hcd = xhci->shared_hcd;
	struct usb_device *rhdev = hcd->self.root_hub;
	struct usb_device *srhdev = shared_hcd->self.root_hub;
	struct usb_device *udev;
	int port, need_wait, timeout;

	dev_info(&dev->dev, "XHCI PLAT REMOVE\n");

#if defined(CONFIG_USB_HOST_SAMSUNG_FEATURE)
	pr_info("%s\n", __func__);
	/* In order to prevent kernel panic */
	if (!pm_runtime_suspended(&xhci->shared_hcd->self.root_hub->dev)) {
		pr_info("%s, shared_hcd pm_runtime_forbid\n", __func__);
		pm_runtime_forbid(&xhci->shared_hcd->self.root_hub->dev);
	}
	if (!pm_runtime_suspended(&xhci->main_hcd->self.root_hub->dev)) {
		pr_info("%s, main_hcd pm_runtime_forbid\n", __func__);
		pm_runtime_forbid(&xhci->main_hcd->self.root_hub->dev);
	}
#endif

	pm_runtime_get_sync(&dev->dev);
	xhci->xhc_state |= XHCI_STATE_REMOVING;

	if (xhci_exynos->port_state == PORT_USB2) {
		usb_power_notify_control(0, 1);
	}
	xhci_exynos_port_power_set(1, 3);

	xhci_exynos->usb3_portsc = NULL;
	xhci_exynos->port_set_delayed = 0;

	__pm_relax(main_hcd_wakelock);

	__pm_relax(shared_hcd_wakelock);

	xhci_exynos_unregister_notify();

	if (!rhdev || !srhdev)
		goto remove_hcd;

	/* check all ports */
	for (timeout = 0; timeout < XHCI_HUB_EVENT_TIMEOUT; timeout++) {
		need_wait = false;
		usb_hub_for_each_child(rhdev, port, udev) {
			if (udev && udev->devnum != -1)
				need_wait = true;
		}
		if (need_wait == false) {
			usb_hub_for_each_child(srhdev, port, udev) {
				if (udev && udev->devnum != -1)
					need_wait = true;
			}
		}
		if (need_wait == true) {
			usleep_range(20000, 22000);
			timeout += 20;
			xhci_info(xhci, "Waiting USB hub disconnect\n");
		} else {
			xhci_info(xhci, "device disconnect all done\n");
			break;
		}
	}

 remove_hcd:
#ifdef CONFIG_USB_DEBUG_DETAILED_LOG
	dev_info(&dev->dev, "remove hcd (shared)\n");
#endif
	usb_remove_hcd(shared_hcd);
	xhci->shared_hcd = NULL;
	usb_phy_shutdown(hcd->usb_phy);
#ifdef CONFIG_USB_DEBUG_DETAILED_LOG
	dev_info(&dev->dev, "remove hcd (main)\n");
#endif
	usb_remove_hcd(hcd);

	wakeup_source_unregister(main_hcd_wakelock);
	wakeup_source_unregister(shared_hcd_wakelock);

	devm_iounmap(&dev->dev, hcd->regs);
	usb_put_hcd(shared_hcd);

	clk_disable_unprepare(clk);
	clk_disable_unprepare(reg_clk);
	usb_put_hcd(hcd);

	pm_runtime_disable(&dev->dev);
	pm_runtime_put_noidle(&dev->dev);
	pm_runtime_set_suspended(&dev->dev);

	return 0;
}

extern u32 otg_is_connect(void);
static int __maybe_unused xhci_exynos_suspend(struct device *dev)
{
	struct xhci_hcd_exynos *xhci_exynos = dev_get_drvdata(dev);
	struct usb_hcd *hcd = xhci_exynos->hcd;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int ret = 0;
	int bypass = 0;
	struct usb_device *udev = hcd->self.root_hub;

	pr_info("[%s]\n", __func__);

	usb_vendor_dev_suspend(udev, &bypass);
	if (bypass) {
		usb_dr_role_control(0);
		return 0;
	}

	if (otg_is_connect() == 1) {	/* If it is OTG_CONNECT_ONLY */
		ret = xhci_priv_suspend_quirk(hcd);
		if (ret)
			return ret;
		/*
		 * xhci_suspend() needs `do_wakeup` to know whether host is allowed
		 * to do wakeup during suspend.
		 */
		pr_info("[%s]: xhci_suspend!\n", __func__);
		ret = xhci_suspend(xhci, device_may_wakeup(dev));
	} else {		/* Enable HS ReWA */
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb2, 1, 0);
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
		/* Enable SS ReWA */
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb3, 1, 0);
#endif
		is_rewa_enabled = 1;
	}
	usb_dr_role_control(0);

	return ret;
}

static int __maybe_unused xhci_exynos_resume(struct device *dev)
{
	struct xhci_hcd_exynos *xhci_exynos = dev_get_drvdata(dev);
	struct usb_hcd *hcd = xhci_exynos->hcd;
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	int ret;
	int bypass = 0;
	struct usb_device *udev = hcd->self.root_hub;

	pr_info("[%s]\n", __func__);

	usb_vendor_dev_resume(udev, &bypass);
	if (bypass) {
		ret = 0;
		usb_dr_role_control(1);
		return 0;
	}

	ret = xhci_priv_resume_quirk(hcd);
	if (ret)
		return ret;

	if (otg_is_connect() == 1) {	/* If it is OTG_CONNECT_ONLY */
		pr_info("[%s]: xhci_resume!\n", __func__);
		ret = xhci_resume(xhci, 0);

		if (ret)
			return ret;
	}

	if (is_rewa_enabled == 1) {
#ifdef CONFIG_EXYNOS_USBDRD_PHY30
		/* Disable SS ReWA */
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb3, 1, 1);
#endif
		/* Disablee HS ReWA */
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb2, 1, 1);
		exynos_usbdrd_phy_vendor_set(xhci_exynos->phy_usb2, 0, 0);
		is_rewa_enabled = 0;
	}
	usb_dr_role_control(1);

	return ret;
}

static const struct dev_pm_ops xhci_plat_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xhci_exynos_suspend, xhci_exynos_resume)
};

static const struct acpi_device_id usb_xhci_acpi_match[] = {
	/* XHCI-compliant USB Controller */
	{"PNP0D10",},
	{}
};

MODULE_DEVICE_TABLE(acpi, usb_xhci_acpi_match);

static struct platform_driver usb_xhci_driver = {
	.probe = xhci_exynos_probe,
	.remove = xhci_exynos_remove,
	/* Host is supposed to removed in usb_reboot_noti.
	   Thats's why we don't need xhci_exynos_shutdown. */
	//.shutdown = xhci_exynos_shutdown,
	.driver = {
		   .name = "xhci-hcd-exynos",
		   .pm = &xhci_plat_pm_ops,
		   .of_match_table = of_match_ptr(usb_xhci_of_match),
		   .acpi_match_table = ACPI_PTR(usb_xhci_acpi_match),
		   .dev_groups = xhci_exynos_groups,
		   },
};

MODULE_ALIAS("platform:xhci-hcd-exynos");

static int __init xhci_exynos_init(void)
{
	xhci_init_driver(&xhci_exynos_hc_driver, &xhci_exynos_overrides);
	return platform_driver_register(&usb_xhci_driver);
}

module_init(xhci_exynos_init);

static void __exit xhci_exynos_exit(void)
{
	platform_driver_unregister(&usb_xhci_driver);
}

module_exit(xhci_exynos_exit);

MODULE_DESCRIPTION("xHCI Exynos Platform Host Controller Driver");
MODULE_LICENSE("GPL");
