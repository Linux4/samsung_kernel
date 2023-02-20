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
#include "s6e3fac_rainbow_g0.h"
#include "s6e3fac_rainbow_g0_panel.h"

int getidx_hbm_vrr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;
	struct panel_vrr *vrr;
	int row = 0, layer = 0, index;

	if (panel == NULL) {
		pr_info("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;
	layer = is_hbm_brightness(panel_bl, panel_bl->props.brightness) ? PANEL_HBM_ON : PANEL_HBM_OFF;
	vrr = get_panel_vrr(panel);
	if (vrr == NULL) {
		pr_info("failed to get vrr\n");
		return -EINVAL;
	}

	index = find_s6e3fac_vrr(vrr);
	if (index < 0) {
		pr_info("vrr not found\n");
		row = 0;
	} else {
		row = index;
	}

	return maptbl_index(tbl, layer, row, 0);
}

__visible_for_testing int __init s6e3fac_g0_panel_init(void)
{
	register_common_panel(&s6e3fac_rainbow_g0_panel_info);
	return 0;
}

__visible_for_testing void __exit s6e3fac_g0_panel_exit(void)
{
	deregister_common_panel(&s6e3fac_rainbow_g0_panel_info);
}

module_init(s6e3fac_g0_panel_init)
module_exit(s6e3fac_g0_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
