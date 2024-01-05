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

static struct s3c_hsotg_plat espresso3250_hsotg_pdata __initdata;

static void __init espresso3250_hsotg_init(void)
{
	s3c_hsotg_set_platdata(&espresso3250_hsotg_pdata);
}

static struct platform_device *espresso3250_usb_devices[] __initdata = {
	&s3c_device_usb_hsotg,
};

void __init exynos3_espresso3250_usb_init(void)
{
	espresso3250_hsotg_init();

	platform_add_devices(espresso3250_usb_devices,
			ARRAY_SIZE(espresso3250_usb_devices));
}
