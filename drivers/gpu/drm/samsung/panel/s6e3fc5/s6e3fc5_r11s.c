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
#include "../panel.h"
#include "../panel_debug.h"
#include "s6e3fc5_r11s.h"
#include "s6e3fc5_r11s_panel.h"

int s6e3fc5_r11s_maptbl_getidx_ffc(struct maptbl *tbl)
{
	int idx;
	u32 dsi_clk;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	dsi_clk = panel_data->props.dsi_freq;

	switch (dsi_clk) {
	case 1328000:
		idx = S6E3FC5_R11S_HS_CLK_1328;
		break;
	case 1362000:
		idx = S6E3FC5_R11S_HS_CLK_1362;
		break;
	case 1368000:
		idx = S6E3FC5_R11S_HS_CLK_1368;
		break;
	default:
		panel_err("invalid dsi clock: %d\n", dsi_clk);
		BUG();
	}
	return maptbl_index(tbl, 0, idx, 0);
}

struct pnobj_func s6e3fc5_r11s_function_table[MAX_S6E3FC5_R11S_FUNCTION] = {
	[S6E3FC5_R11S_MAPTBL_GETIDX_FFC] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_R11S_MAPTBL_GETIDX_FFC, s6e3fc5_r11s_maptbl_getidx_ffc),
};

__visible_for_testing int __init s6e3fc5_r11s_panel_init(void)
{
	struct common_panel_info *cpi = &s6e3fc5_r11s_panel_info;

	s6e3fc5_init(cpi);
	panel_function_insert_array(s6e3fc5_r11s_function_table, ARRAY_SIZE(s6e3fc5_r11s_function_table));
	register_common_panel(cpi);

	return 0;
}

__visible_for_testing void __exit s6e3fc5_r11s_panel_exit(void)
{
	deregister_common_panel(&s6e3fc5_r11s_panel_info);
}

module_init(s6e3fc5_r11s_panel_init)
module_exit(s6e3fc5_r11s_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
