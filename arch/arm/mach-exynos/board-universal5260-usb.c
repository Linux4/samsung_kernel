/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/platform_data/dwc3-exynos.h>

#include <plat/ehci.h>
#include <plat/devs.h>
#include <plat/usb-phy.h>
#include <plat/gpio-cfg.h>

#include <mach/ohci.h>
#include <mach/usb3-drd.h>
#include <mach/usb-switch.h>

#ifdef CONFIG_MFD_MAX77804K
#include <linux/mfd/max77804k-private.h>
#else
#ifdef CONFIG_MFD_MAX77803
#include <linux/mfd/max77803-private.h>
#endif
#endif

#if defined(CONFIG_USB_HOST_NOTIFY)
#include <linux/host_notify.h>
#endif

static bool exynos5_usb_vbus_init(struct platform_device *pdev)
{
#if defined(CONFIG_MFD_MAX77803)
	printk(KERN_DEBUG"%s vbus value is (%d) \n",__func__,max77803_muic_read_vbus());
	if(max77803_muic_read_vbus())return 1;
	else return 0;
#else
	return 1;
#endif
}

static int exynos5_usb_get_id_state(struct platform_device *pdev)
{
#if defined(CONFIG_MFD_MAX77803)
	int id;
	id = max77803_muic_read_adc();
	printk(KERN_DEBUG "%s id value is (%d) \n", __func__, id);
	if (id>0)
		return 1;
	else
		return id;
#else
	return 1;
#endif
}

static struct exynos4_ohci_platdata universal5260_ohci_pdata __initdata;
static struct s5p_ehci_platdata universal5260_ehci_pdata __initdata;
static struct dwc3_exynos_data universal5260_drd_pdata __initdata = {
	.udc_name		= "exynos-ss-udc",
	.xhci_name		= "exynos-xhci",
	.phy_type		= S5P_USB_PHY_DRD,
	.phy_init		= s5p_usb_phy_init,
	.phy_tune		= s5p_usb_phy_tune,
	.phy_exit		= s5p_usb_phy_exit,
	.phy_crport_ctrl	= exynos5_usb_phy_crport_ctrl,
	.get_bses_vld = exynos5_usb_vbus_init,
	.get_id_state = exynos5_usb_get_id_state,
	.irq_flags		= IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.id_irq			= -1,
	.vbus_irq		= -1,
};

static void __init universal5260_ohci_init(void)
{
	exynos4_ohci_set_platdata(&universal5260_ohci_pdata);
}
#if defined(CONFIG_USB_HOST_NOTIFY)
void otg_accessory_power(int enable)
{
	u8 on = (u8)!!enable;

	/* max77803 otg power control */
	otg_control(enable);

	pr_info("%s: otg accessory power = %d\n", __func__, on);
}

static void otg_accessory_powered_booster(int enable)
{
	u8 on = (u8)!!enable;

	/* max77803 powered otg power control */
	powered_otg_control(enable);
	pr_info("%s: enable = %d\n", __func__, on);
}

static struct host_notifier_platform_data host_notifier_pdata = {
	.ndev.name	= "usb_otg",
	.booster	= otg_accessory_power,
	.powered_booster = otg_accessory_powered_booster,
	.thread_enable	= 0,
};

struct platform_device host_notifier_device = {
	.name = "host_notifier",
	.dev.platform_data = &host_notifier_pdata,
};
#endif

static void __init universal5260_ehci_init(void)
{
	s5p_ehci_set_platdata(&universal5260_ehci_pdata);
}

static void __init universal5260_drd_phy_shutdown(struct platform_device *pdev)
{
	struct clk *clk;

	clk = clk_get_sys("exynos-dwc3.0", "usbdrd30");
	if (IS_ERR_OR_NULL(clk)) {
		pr_err("failed to get DRD phy clock\n");
		return;
	}

	if (clk_enable(clk)) {
		pr_err("failed to enable DRD clock\n");
		return;
	}

	s5p_usb_phy_exit(pdev, S5P_USB_PHY_DRD);

	clk_disable(clk);
}

static void __init __maybe_unused universal5260_drd0_init(void)
{
	/* Initialize DRD0 gpio */

	universal5260_drd_pdata.quirks = 0;
#if !defined(CONFIG_USB_SUSPEND) || !defined(CONFIG_USB_XHCI_EXYNOS)
	universal5260_drd_pdata.quirks |= (FORCE_RUN_PERIPHERAL | SKIP_XHCI);
#endif

	exynos5_usb3_drd0_set_platdata(&universal5260_drd_pdata);
}

static struct platform_device *universal5260_usb_devices[] __initdata = {
	&exynos4_device_ohci,
	&s5p_device_ehci,
	&exynos5_device_usb3_drd0,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&host_notifier_device,
#endif
};

void __init exynos5_universal5260_usb_init(void)
{
	universal5260_ohci_init();
	universal5260_ehci_init();

	universal5260_drd_phy_shutdown(&exynos5_device_usb3_drd0);
	universal5260_drd0_init();

	platform_add_devices(universal5260_usb_devices,
			ARRAY_SIZE(universal5260_usb_devices));
}
