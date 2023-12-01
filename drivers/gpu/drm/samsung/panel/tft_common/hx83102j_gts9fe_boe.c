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
#include "hx83102j_gts9fe_boe_panel.h"

static int __init hx83102j_gts9fe_boe_panel_init(void)
{
	register_common_panel(&hx83102j_gts9fe_boe_panel_info);
	return 0;
}

static void __exit hx83102j_gts9fe_boe_panel_exit(void)
{
	deregister_common_panel(&hx83102j_gts9fe_boe_panel_info);
}

module_init(hx83102j_gts9fe_boe_panel_init)
module_exit(hx83102j_gts9fe_boe_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
