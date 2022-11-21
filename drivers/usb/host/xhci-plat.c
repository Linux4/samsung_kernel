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

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
#include <linux/usb/exynos_usb_audio.h>
#endif

#include "xhci.h"
#include "xhci-plat.h"
#include "xhci-mvebu.h"
#include "xhci-rcar.h"

static struct hc_driver __read_mostly xhci_plat_hc_driver;

static int xhci_plat_setup(struct usb_hcd *hcd);
static int xhci_plat_start(struct usb_hcd *hcd);
void __iomem		*usb3_portsc;
static u32 pp_set_delayed;
static u32 portsc_control_priority;
static spinlock_t xhcioff_lock;
static int is_rewa_enabled;
#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
static int port_off_done;
#endif
#define PORTSC_OFFSET	0x430
#define DIS_RX_DETECT	(1 << 9)

#if defined(CONFIG_USB_DWC3_EXYNOS)
#define BU31RHBDBG_OFFSET	0xd800
#define BU31RHBDBG_TOUTCTL	(0x1 << 3)

int xhci_soc_config_after_reset(struct xhci_hcd *xhci)
{
	int ret = 0;
	static void __iomem *bu31rhbdbg_reg = NULL;
	u32 reg;

	if (bu31rhbdbg_reg == NULL)
		bu31rhbdbg_reg = ioremap(xhci->main_hcd->rsrc_start +
						BU31RHBDBG_OFFSET, SZ_4);

	reg = readl(bu31rhbdbg_reg);
	reg |= BU31RHBDBG_TOUTCTL;
	writel(reg, bu31rhbdbg_reg);

	return ret;
}
#endif

static const struct xhci_driver_overrides xhci_plat_overrides __initconst = {
	.extra_priv_size = sizeof(struct xhci_plat_priv),
	.reset = xhci_plat_setup,
	.start = xhci_plat_start,
};

static void xhci_priv_plat_start(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (priv->plat_start)
		priv->plat_start(hcd);
}

static int xhci_priv_init_quirk(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (!priv->init_quirk)
		return 0;

	return priv->init_quirk(hcd);
}

static int xhci_priv_resume_quirk(struct usb_hcd *hcd)
{
	struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

	if (!priv->resume_quirk)
		return 0;

	return priv->resume_quirk(hcd);
}

static void xhci_plat_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	/*
	 * As of now platform drivers don't provide MSI support so we ensure
	 * here that the generic code does not try to make a pci_dev from our
	 * dev struct in order to setup MSI
	 */
	xhci->quirks |= XHCI_PLAT;
}

/* called during probe() after chip reset completes */
static int xhci_plat_setup(struct usb_hcd *hcd)
{
	int ret;

	ret = xhci_priv_init_quirk(hcd);
	if (ret)
		return ret;

	ret = xhci_gen_setup(hcd, xhci_plat_quirks);

	/*
	 * DWC3 WORKAROUND: xhci reset clears PHY CR port settings,
	 * so USB3.0 PHY should be tuned again.
	 */
	if (hcd->phy)
		phy_tune(hcd->phy, OTG_STATE_A_HOST);

	return ret;
}

static int xhci_plat_start(struct usb_hcd *hcd)
{
	xhci_priv_plat_start(hcd);
	return xhci_run(hcd);
}

static ssize_t
xhci_plat_show_ss_compliance(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	u32			reg;
	void __iomem *reg_base;

	reg_base = hcd->regs;
	reg = readl(reg_base + PORTSC_OFFSET);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", reg);
}

static ssize_t
xhci_platg_store_ss_compliance(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	int		value;
	u32			reg;
	void __iomem *reg_base;

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;

	reg_base = hcd->regs;

	if (value == 1) {
		/* PORTSC PLS is set to 10, LWS to 1 */
		reg = readl(reg_base + PORTSC_OFFSET);
		reg &= ~((0xF << 5) | (1 << 16));
		reg |= (10 << 5) | (1 << 16);
		writel(reg, reg_base + PORTSC_OFFSET);
		pr_info("Super speed host compliance enabled portsc 0x%x\n", reg);
	} else
		pr_info("Only 1 is allowed for input value\n");

	return n;
}

static DEVICE_ATTR(ss_compliance, S_IWUSR | S_IRUSR | S_IRGRP,
	xhci_plat_show_ss_compliance, xhci_platg_store_ss_compliance);

static struct attribute *exynos_xhci_attributes[] = {
	&dev_attr_ss_compliance.attr,
	NULL
};

static const struct attribute_group xhci_plat_attr_group = {
	.attrs = exynos_xhci_attributes,
};
#ifdef CONFIG_OF
static const struct xhci_plat_priv xhci_plat_marvell_armada = {
	.init_quirk = xhci_mvebu_mbus_init_quirk,
};

static const struct xhci_plat_priv xhci_plat_renesas_rcar_gen2 = {
	.firmware_name = XHCI_RCAR_FIRMWARE_NAME_V1,
	.init_quirk = xhci_rcar_init_quirk,
	.plat_start = xhci_rcar_start,
	.resume_quirk = xhci_rcar_resume_quirk,
};

static const struct xhci_plat_priv xhci_plat_renesas_rcar_gen3 = {
	.firmware_name = XHCI_RCAR_FIRMWARE_NAME_V3,
	.init_quirk = xhci_rcar_init_quirk,
	.plat_start = xhci_rcar_start,
	.resume_quirk = xhci_rcar_resume_quirk,
};

static const struct of_device_id usb_xhci_of_match[] = {
	{
		.compatible = "generic-xhci",
	}, {
		.compatible = "xhci-platform",
	}, {
		.compatible = "marvell,armada-375-xhci",
		.data = &xhci_plat_marvell_armada,
	}, {
		.compatible = "marvell,armada-380-xhci",
		.data = &xhci_plat_marvell_armada,
	}, {
		.compatible = "renesas,xhci-r8a7790",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,xhci-r8a7791",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,xhci-r8a7793",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,xhci-r8a7795",
		.data = &xhci_plat_renesas_rcar_gen3,
	}, {
		.compatible = "renesas,xhci-r8a7796",
		.data = &xhci_plat_renesas_rcar_gen3,
	}, {
		.compatible = "renesas,rcar-gen2-xhci",
		.data = &xhci_plat_renesas_rcar_gen2,
	}, {
		.compatible = "renesas,rcar-gen3-xhci",
		.data = &xhci_plat_renesas_rcar_gen3,
	},
	{},
};
MODULE_DEVICE_TABLE(of, usb_xhci_of_match);
#endif

void xhci_portsc_power_off(void __iomem *portsc, u32 on, u32 prt)
{
	u32 reg;

#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
	spin_lock(&xhcioff_lock);

	pr_info("%s, on=%d portsc_control_priority=%d, prt=%d\n",
			__func__, on,  portsc_control_priority, prt);

	if (portsc_control_priority > prt) {
		spin_unlock(&xhcioff_lock);
		return;
	}

	portsc_control_priority = prt;

	if (on && !port_off_done) {
		pr_info("%s, Do not switch-on port\n", __func__);
		spin_unlock(&xhcioff_lock);
		return;
	}
#endif

	reg = readl(portsc);

	if (on)
		reg |= PORT_POWER;
	else
		reg &= ~PORT_POWER;

	writel(reg, portsc);
	reg = readl(portsc);

	pr_info("power %s portsc, reg = 0x%x addr = %p\n",
		on ? "on" : "off", reg, portsc);

#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
	reg = readl(phycon_base_addr+0x70);
	if (on)
		reg &= ~DIS_RX_DETECT;
	else
		reg |= DIS_RX_DETECT;

	writel(reg, phycon_base_addr+0x70);

	if (on)
		port_off_done = 0;
	else
		port_off_done = 1;

	pr_info("phycon ess_ctrl = 0x%x\n", readl(phycon_base_addr+0x70));

	spin_unlock(&xhcioff_lock);
#endif
}

int xhci_portsc_set(u32 on)
{
	if (usb3_portsc != NULL && !on) {
		xhci_portsc_power_off(usb3_portsc, 0, 2);
		pp_set_delayed = 0;
		return 0;
	}

	if (!on)
		pp_set_delayed = 1;

	pr_info("%s, usb3_portsc is NULL\n", __func__);
	return -EIO;
}
EXPORT_SYMBOL(xhci_portsc_set);

#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
int xhci_port_power_set(u32 on, u32 prt)
{
	if (usb3_portsc != NULL) {
		xhci_portsc_power_off(usb3_portsc, on, prt);
		return 0;
	}

	pr_info("%s, usb3_portsc is NULL\n", __func__);
	return -EIO;
}
EXPORT_SYMBOL(xhci_port_power_set);
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

static int xhci_plat_probe(struct platform_device *pdev)
{
	struct device		*parent = pdev->dev.parent;
	const struct xhci_plat_priv *priv_match;
	const struct hc_driver	*driver;
	struct device		*sysdev, *tmpdev;
	struct xhci_hcd		*xhci;
	struct resource         *res;
	struct usb_hcd		*hcd;
	struct clk              *clk;
	struct clk              *reg_clk;
	int			ret;
	int			irq;

	struct wake_lock	*main_wakelock, *shared_wakelock;
	int			value;

	dev_info(&pdev->dev, "XHCI PLAT START\n");

	/* Initialization main wakelock for HS HCD */
	main_wakelock = kzalloc(sizeof(struct wake_lock), GFP_KERNEL);
	wake_lock_init(main_wakelock, WAKE_LOCK_SUSPEND, dev_name(&pdev->dev));
	wake_lock(main_wakelock);

	/* Initialization shared wakelock for SS HCD */
	shared_wakelock = kzalloc(sizeof(struct wake_lock), GFP_KERNEL);
	wake_lock_init(shared_wakelock, WAKE_LOCK_SUSPEND, dev_name(&pdev->dev));
	wake_lock(shared_wakelock);

#if defined(CONFIG_USB_PORT_POWER_OPTIMIZATION)
	port_off_done = 0;
#endif
	portsc_control_priority = 0;
	is_rewa_enabled = 0;

	if (usb_disabled())
		return -ENODEV;

	driver = &xhci_plat_hc_driver;

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
		if (is_of_node(sysdev->fwnode) ||
			is_acpi_device_node(sysdev->fwnode))
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
		ret = dma_coerce_mask_and_coherent(sysdev,
						   DMA_BIT_MASK(64));
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

	hcd = __usb_create_hcd(driver, sysdev, &pdev->dev,
			       dev_name(&pdev->dev), NULL);
	if (!hcd) {
		ret = -ENOMEM;
		goto disable_runtime;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hcd->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(hcd->regs)) {
		ret = PTR_ERR(hcd->regs);
		goto put_hcd;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	usb3_portsc = hcd->regs + PORTSC_OFFSET;
	pr_info("get usb3_portsc addr = %p pp_set = %d\n",
		usb3_portsc, pp_set_delayed);

	if (pp_set_delayed) {
		xhci_portsc_power_off(usb3_portsc, 0, 2);
		pp_set_delayed = 0;
	}

	/* Get USB2.0 PHY for main hcd */
	if (parent) {
		hcd->phy = devm_phy_get(parent, "usb2-phy");
		if (IS_ERR_OR_NULL(hcd->phy)) {
			hcd->phy = NULL;
			dev_err(&pdev->dev,
				"%s: failed to get phy\n", __func__);
		}
	}

	/*
	 * Not all platforms have clks so it is not an error if the
	 * clock do not exist.
	 */
	reg_clk = devm_clk_get(&pdev->dev, "reg");
	if (!IS_ERR(reg_clk)) {
		ret = clk_prepare_enable(reg_clk);
		if (ret)
			goto put_hcd;
	} else if (PTR_ERR(reg_clk) == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto put_hcd;
	}

	clk = devm_clk_get(&pdev->dev, NULL);
	if (!IS_ERR(clk)) {
		ret = clk_prepare_enable(clk);
		if (ret)
			goto disable_reg_clk;
	} else if (PTR_ERR(clk) == -EPROBE_DEFER) {
		ret = -EPROBE_DEFER;
		goto disable_reg_clk;
	}

	xhci = hcd_to_xhci(hcd);
	priv_match = of_device_get_match_data(&pdev->dev);
	if (priv_match) {
		struct xhci_plat_priv *priv = hcd_to_xhci_priv(hcd);

		/* Just copy data for now */
		if (priv_match)
			*priv = *priv_match;
	}

	device_wakeup_enable(hcd->self.controller);

	xhci->clk = clk;
	xhci->reg_clk = reg_clk;
	xhci->main_wakelock = main_wakelock;
	xhci->shared_wakelock = shared_wakelock;
	xhci->main_hcd = hcd;
	xhci->shared_hcd = __usb_create_hcd(driver, sysdev, &pdev->dev,
			dev_name(&pdev->dev), hcd);
	if (!xhci->shared_hcd) {
		ret = -ENOMEM;
		goto disable_clk;
	}

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

		device_property_read_u32(tmpdev, "imod-interval-ns",
					 &xhci->imod_interval);
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

	/* Get USB3.0 PHY to tune the PHY */
	if (parent) {
		xhci->shared_hcd->phy = devm_phy_get(parent, "usb3-phy");
		if (IS_ERR_OR_NULL(xhci->shared_hcd->phy)) {
			xhci->shared_hcd->phy = NULL;
			dev_err(&pdev->dev,
				"%s: failed to get phy\n", __func__);
		}
	}

	ret = of_property_read_u32(parent->of_node, "xhci_l2_support", &value);
	if (ret == 0 && value == 1)
		xhci->quirks |= XHCI_L2_SUPPORT;
	else {
		dev_err(&pdev->dev,
			"can't get xhci l2 support, error = %d\n", ret);
	}

	ret = of_property_read_u32(parent->of_node,
				"xhci_use_uram_for_audio", &value);
	if (ret == 0 && value == 1) {
		/*
		 * Check URAM address. At least the following address should
		 * be defined.(Otherwise, URAM feature will be disabled.)
		 */
		if (EXYNOS_URAM_DCBAA_ADDR == 0x0 ||
				EXYNOS_URAM_ABOX_ERST_SEG_ADDR == 0x0 ||
				EXYNOS_URAM_ABOX_EVT_RING_ADDR == 0x0 ||
				EXYNOS_URAM_DEVICE_CTX_ADDR == 0x0 ||
				EXYNOS_URAM_ISOC_OUT_RING_ADDR == 0x0) {
			dev_info(&pdev->dev,
				"Some URAM addresses are not defiend!\n");
			goto skip_uram;
		}

		dev_info(&pdev->dev, "Support URAM for USB audio.\n");
		xhci->quirks |= XHCI_USE_URAM_FOR_EXYNOS_AUDIO;
		/* Initialization Default Value */
		xhci->exynos_uram_ctx_alloc = false;
		xhci->exynos_uram_isoc_out_alloc = false;
		xhci->exynos_uram_isoc_in_alloc = false;
		xhci->usb_audio_ctx_addr = NULL;
		xhci->usb_audio_isoc_out_addr = NULL;
		xhci->usb_audio_isoc_in_addr = NULL;
	} else {
		dev_err(&pdev->dev, "URAM is not used.\n");
	}
skip_uram:

	xhci->xhci_alloc = &xhci_pre_alloc;

	xhci->main_hcd->skip_phy_initialization = 1;
	xhci->shared_hcd->skip_phy_initialization = 1;

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret)
		goto disable_usb_phy;

	if (HCC_MAX_PSA(xhci->hcc_params) >= 4)
		xhci->shared_hcd->can_do_streams = 1;

	ret = usb_add_hcd(xhci->shared_hcd, irq, IRQF_SHARED);
	if (ret)
		goto dealloc_usb2_hcd;

#ifdef CONFIG_SND_EXYNOS_USB_AUDIO
	ret = of_property_read_u32(parent->of_node, "usb_audio_offloading", &value);
	if (ret == 0 && value == 1) {
		ret = exynos_usb_audio_init(parent, pdev);
		if (ret) {
			dev_err(&pdev->dev, "USB Audio INIT fail\n");
			return ret;
		} else {
			dev_info(&pdev->dev, "USB Audio offloading is supported\n");
		}
	} else {
		dev_err(&pdev->dev, "can't get audio support, error = %d\n", ret);
		return ret;
	}

	xhci->out_dma = xhci_data.out_data_dma;
	xhci->out_addr = xhci_data.out_data_addr;
	xhci->in_dma = xhci_data.in_data_dma;
	xhci->in_addr = xhci_data.in_data_addr;
#endif

	ret = sysfs_create_group(&pdev->dev.kobj, &xhci_plat_attr_group);
	if (ret)
		dev_err(&pdev->dev, "failed to create xhci-plat attributes\n");

	device_enable_async_suspend(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);

	device_set_wakeup_enable(&xhci->main_hcd->self.root_hub->dev, 1);
	device_set_wakeup_enable(&xhci->shared_hcd->self.root_hub->dev, 1);

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
	clk_disable_unprepare(clk);

disable_reg_clk:
	clk_disable_unprepare(reg_clk);

put_hcd:
	usb_put_hcd(hcd);

disable_runtime:
	pm_runtime_put_noidle(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static int xhci_plat_remove(struct platform_device *dev)
{
	struct device	*parent = dev->dev.parent;
	struct usb_hcd	*hcd = platform_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	struct clk *clk = xhci->clk;
	struct clk *reg_clk = xhci->reg_clk;
	int timeout = 0;
	
	dev_info(&dev->dev, "XHCI PLAT REMOVE\n");

	usb3_portsc = NULL;
	pp_set_delayed = 0;

	/*
	 * Sometimes deadlock occurred in this function.
	 * So, below waiting for completion of hub_event was added.
	 */
	while (xhci->shared_hcd->is_in_hub_event || hcd->is_in_hub_event) {
		msleep(10);
		timeout += 10;
		if (timeout >= XHCI_HUB_EVENT_TIMEOUT) {
			xhci_err(xhci,
				"ERROR: hub_event completion timeout\n");
			break;
		}
	}
	xhci_dbg(xhci, "%s: waited %dmsec", __func__, timeout);
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

	xhci->xhc_state |= XHCI_STATE_REMOVING;
	xhci->xhci_alloc->offset = 0;

	dev_info(&dev->dev, "WAKE UNLOCK\n");
	wake_unlock(xhci->main_wakelock);
	wake_lock_destroy(xhci->main_wakelock);

	wake_unlock(xhci->shared_wakelock);
	wake_lock_destroy(xhci->shared_wakelock);

	pr_info("%s %d xhci->main_hcd = %pS\n", __func__, __LINE__, xhci->main_hcd);
	usb_remove_hcd(xhci->shared_hcd);
	usb_phy_shutdown(hcd->usb_phy);

	/*
	 * In usb_remove_hcd, phy_exit is called if phy is not NULL.
	 * However, in the case that PHY was turn on or off as runtime PM,
	 * PHY sould not exit at this time. So, to prevent the PHY exit,
	 * PHY pointer have to be NULL.
	 */
	if (parent && hcd->phy)
		hcd->phy = NULL;

	usb_remove_hcd(hcd);
	usb_put_hcd(xhci->shared_hcd);

	clk_disable_unprepare(clk);
	clk_disable_unprepare(reg_clk);
	usb_put_hcd(hcd);

	pm_runtime_set_suspended(&dev->dev);
	pm_runtime_disable(&dev->dev);

	return 0;
}

extern u32 otg_is_connect(void);
extern int xhci_halt(struct xhci_hcd *xhci);
extern void xhci_hc_died(struct xhci_hcd *xhci);

static int __maybe_unused xhci_plat_suspend(struct device *dev)
{
	struct usb_hcd	*hcd = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	int ret;

	pr_info("[%s] \n",__func__);

	if (xhci->xhc_state & XHCI_STATE_DYING) {
		pr_info("xhci is already died...\n");
		return 0;
	}

	/*
	 * xhci_suspend() needs `do_wakeup` to know whether host is allowed
	 * to do wakeup during suspend. Since xhci_plat_suspend is currently
	 * only designed for system suspend, device_may_wakeup() is enough
	 * to dertermine whether host is allowed to do wakeup. Need to
	 * reconsider this when xhci_plat_suspend enlarges its scope, e.g.,
	 * also applies to runtime suspend.
	 */

	ret = xhci_suspend(xhci, device_may_wakeup(dev));
	if (ret) {
		if (ret == -ETIMEDOUT) {
			pr_info("xhci stop timeout.(Try to xhci halt)\n");
			xhci_halt(xhci);
			xhci_hc_died(xhci);
		}
		return ret;
	}


	if (otg_is_connect() != 1) { /* If it is not OTG_CONNECT_ONLY */
		/* Enable HS ReWA */
		phy_vendor_set(xhci->main_hcd->phy, 1, 0);
		/* Enable SS ReWA */
		phy_vendor_set(xhci->shared_hcd->phy, 1, 0);
		is_rewa_enabled = 1;
	}

	return  ret;
}

static int __maybe_unused xhci_plat_resume(struct device *dev)
{
	struct usb_hcd	*hcd = dev_get_drvdata(dev);
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	int ret;

	pr_info("[%s] \n",__func__);

	if (xhci->xhc_state & XHCI_STATE_DYING) {
		pr_info("xhci is already died...\n");
		return 0;
	}

	ret = xhci_priv_resume_quirk(hcd);
	if (ret)
		return ret;

	if (is_rewa_enabled == 1) {
		/* Disable SS ReWA */
		phy_vendor_set(xhci->shared_hcd->phy, 1, 1);
		/* Disablee HS ReWA */
		phy_vendor_set(xhci->main_hcd->phy, 1, 1);
		phy_vendor_set(xhci->main_hcd->phy, 0, 0);
		is_rewa_enabled = 0;
	}

	return xhci_resume(xhci, 0);
}

static int __maybe_unused xhci_plat_runtime_suspend(struct device *dev)
{
	/*
	 *struct usb_hcd  *hcd = dev_get_drvdata(dev);
	 *struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	 *
	 *return xhci_suspend(xhci, true);
	 */

	pr_info("[%s] \n",__func__);
	return 0;
}

static int __maybe_unused xhci_plat_runtime_resume(struct device *dev)
{
	/*
	 *struct usb_hcd  *hcd = dev_get_drvdata(dev);
	 *struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	 *
	 *return xhci_resume(xhci, 0);
	 */

	pr_info("[%s] \n",__func__);
	return 0;
}

static const struct dev_pm_ops xhci_plat_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(xhci_plat_suspend, xhci_plat_resume)

	SET_RUNTIME_PM_OPS(xhci_plat_runtime_suspend,
			   xhci_plat_runtime_resume,
			   NULL)
};

static const struct acpi_device_id usb_xhci_acpi_match[] = {
	/* XHCI-compliant USB Controller */
	{ "PNP0D10", },
	{ }
};
MODULE_DEVICE_TABLE(acpi, usb_xhci_acpi_match);

static struct platform_driver usb_xhci_driver = {
	.probe	= xhci_plat_probe,
	.remove	= xhci_plat_remove,
	.driver	= {
		.name = "xhci-hcd",
		.pm = &xhci_plat_pm_ops,
		.of_match_table = of_match_ptr(usb_xhci_of_match),
		.acpi_match_table = ACPI_PTR(usb_xhci_acpi_match),
	},
};
MODULE_ALIAS("platform:xhci-hcd");

static int __init xhci_plat_init(void)
{
	xhci_init_driver(&xhci_plat_hc_driver, &xhci_plat_overrides);
	spin_lock_init(&xhcioff_lock);
	return platform_driver_register(&usb_xhci_driver);
}
module_init(xhci_plat_init);

static void __exit xhci_plat_exit(void)
{
	platform_driver_unregister(&usb_xhci_driver);
}
module_exit(xhci_plat_exit);

MODULE_DESCRIPTION("xHCI Platform Host Controller Driver");
MODULE_LICENSE("GPL");
