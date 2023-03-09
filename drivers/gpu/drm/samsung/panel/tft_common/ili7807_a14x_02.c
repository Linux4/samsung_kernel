/*
 * linux/drivers/video/fbdev/exynos/panel/tft_common/tft_common.c
 *
 * TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "ili7807_a14x_02_panel.h"

static int __init ili7807_a14x_02_panel_init(void)
{
	register_common_panel(&ili7807_a14x_02_panel_info);
	return 0;
}

static void __exit ili7807_a14x_02_panel_exit(void)
{
	deregister_common_panel(&ili7807_a14x_02_panel_info);
}

module_init(ili7807_a14x_02_panel_init)
module_exit(ili7807_a14x_02_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
