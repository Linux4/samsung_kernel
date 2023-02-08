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

#define XYREF5260_ID0_GPIO	EXYNOS5260_GPX1(1)
#define XYREF5260_VBUS0_GPIO	EXYNOS5260_GPX1(0)
#define XYREF5260_BOOST5V_GPIO	EXYNOS5260_GPA2(4)

static int xyref5260_vbus_ctrl(struct platform_device *pdev, int on)
{
	unsigned gpio = XYREF5260_BOOST5V_GPIO;
	int ret = -EINVAL;

	ret = gpio_request(gpio, "UDRD3_VBUSCTRL");
	if (ret < 0) {
		pr_err("failed to request UDRD3_0_VBUSCTRL\n");
		return ret;
	}

	gpio_set_value(gpio, !!on);
	gpio_free(gpio);

	return ret;
}

static int xyref5260_get_id_state(struct platform_device *pdev)
{
	return gpio_get_value(XYREF5260_ID0_GPIO);
}

static bool xyref5260_get_bsession_valid(struct platform_device *pdev)
{
	return !!gpio_get_value(XYREF5260_VBUS0_GPIO);
}

static struct exynos4_ohci_platdata xyref5260_ohci_pdata __initdata;
static struct s5p_ehci_platdata xyref5260_ehci_pdata __initdata;
static struct dwc3_exynos_data xyref5260_drd_pdata __initdata = {
	.udc_name		= "exynos-ss-udc",
	.xhci_name		= "exynos-xhci",
	.phy_type		= S5P_USB_PHY_DRD,
	.phy_init		= s5p_usb_phy_init,
	.phy_exit		= s5p_usb_phy_exit,
	.phy_crport_ctrl	= exynos5_usb_phy_crport_ctrl,
	.vbus_ctrl		= xyref5260_vbus_ctrl,
	.get_id_state		= xyref5260_get_id_state,
	.get_bses_vld		= xyref5260_get_bsession_valid,
	.irq_flags		= IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
};

static void __init xyref5260_ohci_init(void)
{
	exynos4_ohci_set_platdata(&xyref5260_ohci_pdata);
}

static void __init xyref5260_ehci_init(void)
{
	s5p_ehci_set_platdata(&xyref5260_ehci_pdata);
}

static void __init xyref5260_drd_phy_shutdown(struct platform_device *pdev)
{
	int phy_num = pdev->id;
	struct clk *clk;

	switch (phy_num) {
	case 0:
		clk = clk_get_sys("exynos-dwc3.0", "usbdrd30");
		break;
	default:
		clk = NULL;
		break;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("failed to get DRD%d phy clock\n", phy_num);
		return;
	}

	if (clk_enable(clk)) {
		pr_err("failed to enable DRD%d clock\n", phy_num);
		return;
	}

	s5p_usb_phy_exit(pdev, S5P_USB_PHY_DRD);

	clk_disable(clk);
}

static void __init __maybe_unused xyref5260_drd0_init(void)
{
	/* Initialize DRD0 gpio */
	if (gpio_request_one(XYREF5260_ID0_GPIO, GPIOF_IN, "UDRD3_0_ID")) {
		pr_err("failed to request UDRD3_0_ID\n");
		xyref5260_drd_pdata.id_irq = -1;
	} else {
		s3c_gpio_cfgpin(XYREF5260_ID0_GPIO, S3C_GPIO_SFN(0xF));
		s3c_gpio_setpull(XYREF5260_ID0_GPIO, S3C_GPIO_PULL_UP);
		gpio_free(XYREF5260_ID0_GPIO);

		xyref5260_drd_pdata.id_irq = gpio_to_irq(XYREF5260_ID0_GPIO);
	}

	if (gpio_request_one(XYREF5260_VBUS0_GPIO, GPIOF_IN, "UDRD3_0_VBUS")) {
		pr_err("failed to request UDRD3_0_VBUS\n");
		xyref5260_drd_pdata.vbus_irq = -1;
	} else {
		s3c_gpio_cfgpin(XYREF5260_VBUS0_GPIO, S3C_GPIO_SFN(0xF));
		s3c_gpio_setpull(XYREF5260_VBUS0_GPIO, S3C_GPIO_PULL_NONE);
		gpio_free(XYREF5260_VBUS0_GPIO);

		xyref5260_drd_pdata.vbus_irq = gpio_to_irq(XYREF5260_VBUS0_GPIO);
	}

	if (gpio_request_one(XYREF5260_BOOST5V_GPIO, GPIOF_DIR_OUT, "UDRD3_0_BOOST5V")) {
		pr_err("failed to request UDRD3_0_BOOST5V\n");
	} else {
		s3c_gpio_setpull(XYREF5260_BOOST5V_GPIO, S3C_GPIO_PULL_DOWN);
		gpio_free(XYREF5260_BOOST5V_GPIO);
	}

	xyref5260_drd_pdata.quirks = 0;
#if !defined(CONFIG_USB_SUSPEND) || !defined(CONFIG_USB_XHCI_EXYNOS)
	xyref5260_drd_pdata.quirks |= (FORCE_RUN_PERIPHERAL | SKIP_XHCI);
#endif

	exynos5_usb3_drd0_set_platdata(&xyref5260_drd_pdata);
}

#ifdef CONFIG_USB_EXYNOS_SWITCH
static struct s5p_usbswitch_platdata xyref5260_usbswitch_pdata __initdata;

static void __init xyref5260_usbswitch_init(void)
{
	struct s5p_usbswitch_platdata *pdata = &xyref5260_usbswitch_pdata;
	int err;

	pdata->gpio_host_detect = EXYNOS5260_GPX0(6);
	err = gpio_request_one(pdata->gpio_host_detect, GPIOF_IN,
			"HOST_DETECT");
	if (err) {
		pr_err("failed to request host gpio\n");
		return;
	}

	s3c_gpio_cfgpin(pdata->gpio_host_detect, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pdata->gpio_host_detect, S3C_GPIO_PULL_NONE);
	gpio_free(pdata->gpio_host_detect);

	pdata->gpio_host_vbus = EXYNOS5260_GPB0(0);
	err = gpio_request_one(pdata->gpio_host_vbus, GPIOF_OUT_INIT_LOW,
			"HOST_VBUS");
	if (err) {
		pr_err("failed to request host vbus gpio\n");
		return;
	}
	s3c_gpio_setpull(pdata->gpio_host_vbus, S3C_GPIO_PULL_DOWN);
	gpio_free(pdata->gpio_host_vbus);

	s5p_usbswitch_set_platdata(pdata);
}
#endif

static struct platform_device *xyref5260_usb_devices[] __initdata = {
	&exynos4_device_ohci,
	&s5p_device_ehci,
	&exynos5_device_usb3_drd0,
};

void __init exynos5_xyref5260_usb_init(void)
{
	xyref5260_ohci_init();
	xyref5260_ehci_init();

	/*
	 * Shutdown DRD PHYs to reduce power consumption.
	 * Later, DRD driver will turn on only the PHY it needs.
	 */
	xyref5260_drd_phy_shutdown(&exynos5_device_usb3_drd0);
	xyref5260_drd0_init();
#ifdef CONFIG_USB_EXYNOS_SWITCH
	xyref5260_usbswitch_init();
#endif
	platform_add_devices(xyref5260_usb_devices,
			ARRAY_SIZE(xyref5260_usb_devices));
}
