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
#include "td4160_a14x_00_panel.h"

static int __init td4160_a14x_00_panel_init(void)
{
	register_common_panel(&td4160_a14x_00_panel_info);
	return 0;
}

static void __exit td4160_a14x_00_panel_exit(void)
{
	deregister_common_panel(&td4160_a14x_00_panel_info);
}

module_init(td4160_a14x_00_panel_init)
module_exit(td4160_a14x_00_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
