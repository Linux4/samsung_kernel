/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		 http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>

#include <plat/devs.h>
#include <plat/udc-hs.h>
#include <plat/ehci.h>
#include <mach/ohci.h>

static struct exynos4_ohci_platdata universal3472_ohci_pdata __initdata;
static struct s5p_ehci_platdata universal3472_ehci_pdata __initdata;
static struct s3c_hsotg_plat universal3472_hsotg_pdata __initdata;

static void __init universal3472_ohci_init(void)
{
	exynos4_ohci_set_platdata(&universal3472_ohci_pdata);
}

static void __init universal3472_ehci_init(void)
{
	s5p_ehci_set_platdata(&universal3472_ehci_pdata);
}

static void __init universal3472_hsotg_init(void)
{
	s3c_hsotg_set_platdata(&universal3472_hsotg_pdata);
}

static struct platform_device *universal3472_usb_devices[] __initdata = {
	&exynos4_device_ohci,
	&s5p_device_ehci,
	&s3c_device_usb_hsotg,
};

void __init exynos3_universal3472_usb_init(void)
{
	universal3472_ohci_init();
	universal3472_ehci_init();
	universal3472_hsotg_init();

	platform_add_devices(universal3472_usb_devices,
			ARRAY_SIZE(universal3472_usb_devices));
}
