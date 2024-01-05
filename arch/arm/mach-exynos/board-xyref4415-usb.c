/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		 http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/platform_device.h>

#include <plat/gpio-cfg.h>
#include <plat/ehci.h>
#include <plat/devs.h>
#include <plat/udc-hs.h>
#include <mach/ohci.h>
#include <mach/usb-switch.h>

static struct exynos4_ohci_platdata xyref4415_ohci_pdata __initdata;
static struct s5p_ehci_platdata xyref4415_ehci_pdata __initdata;
static struct s3c_hsotg_plat xyref4415_hsotg_pdata __initdata;

static void __init xyref4415_ohci_init(void)
{
	exynos4_ohci_set_platdata(&xyref4415_ohci_pdata);
}

static void __init xyref4415_ehci_init(void)
{
	s5p_ehci_set_platdata(&xyref4415_ehci_pdata);
}

static void __init xyref4415_hsotg_init(void)
{
	s3c_hsotg_set_platdata(&xyref4415_hsotg_pdata);
}

#ifdef CONFIG_USB_EXYNOS_SWITCH
static struct s5p_usbswitch_platdata xyref4415_usbswitch_pdata __initdata;

static void __init xyref4415_usbswitch_init(void)
{
	struct s5p_usbswitch_platdata *pdata = &xyref4415_usbswitch_pdata;
	int err;

	pdata->gpio_device_detect = EXYNOS4_GPX1(0);
	err = gpio_request_one(pdata->gpio_device_detect, GPIOF_IN,
			"DEVICE_DETECT");
	if (err) {
		pr_err("failed to request host gpio\n");
		return;
	}

	s3c_gpio_cfgpin(pdata->gpio_device_detect, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pdata->gpio_device_detect, S3C_GPIO_PULL_NONE);
	gpio_free(pdata->gpio_device_detect);

	pdata->gpio_host_detect = EXYNOS4_GPX1(1);
	err = gpio_request_one(pdata->gpio_host_detect, GPIOF_IN,
			"HOST_DETECT");
	if (err) {
		pr_err("failed to request host gpio\n");
		return;
	}

	s3c_gpio_cfgpin(pdata->gpio_host_detect, S3C_GPIO_SFN(0xF));
#ifdef CONFIG_MACH_XYREF4415
	/* This internal PULL_UP setting should be applied to xyref board only.
	   Because we cannot expect what external resistance is attached to other board.
	 */
	s3c_gpio_setpull(pdata->gpio_host_detect, S3C_GPIO_PULL_UP);
#else
	s3c_gpio_setpull(pdata->gpio_host_detect, S3C_GPIO_PULL_NONE);
#endif
	gpio_free(pdata->gpio_host_detect);

	pdata->gpio_host_vbus = EXYNOS4_GPY6(3);
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

static struct platform_device *xyref4415_usb_devices[] __initdata = {
	&s5p_device_ehci,
	&exynos4_device_ohci,
	&s3c_device_usb_hsotg,
};

void __init exynos4_xyref4415_usb_init(void)
{
	xyref4415_ohci_init();
	xyref4415_ehci_init();
	xyref4415_hsotg_init();

#ifdef CONFIG_USB_EXYNOS_SWITCH
	xyref4415_usbswitch_init();
#endif
	platform_add_devices(xyref4415_usb_devices,
			ARRAY_SIZE(xyref4415_usb_devices));
}
