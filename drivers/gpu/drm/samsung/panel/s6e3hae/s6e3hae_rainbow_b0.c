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
#include "s6e3hae_rainbow_b0.h"
#include "s6e3hae_rainbow_b0_panel.h"

void copy_gamma_mode2_brt_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	int idx;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %p, dst %p)\n",
				tbl, dst);
		return;
	}

	if (!tbl->arr) {
		panel_err("source buffer is null\n");
		return;
	}

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		panel_err("failed to getidx %d\n", idx);
		return;
	}

	if (is_id_lt_81(panel)) {
		if (idx >= maptbl_get_countof_col(tbl) * S6E3HAE_B0_NR_STEP) {
			panel_warn("this panel not support hbm mode %d, set to 255\n", idx);
			idx = maptbl_get_countof_col(tbl) * (S6E3HAE_B0_NR_STEP - 1);
		} else if (idx <= maptbl_get_countof_col(tbl) * 30) {
			panel_warn("this panel not support lower brightness mode %d, set to 30\n", idx);
			idx = maptbl_get_countof_col(tbl) * (30);
		}
	}

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * maptbl_get_sizeof_copy(tbl));

	panel_dbg("copy from %s %d %d\n",
			tbl->name, idx, maptbl_get_sizeof_copy(tbl));

	print_data(dst, maptbl_get_sizeof_copy(tbl));
}

__visible_for_testing int __init s6e3hae_b0_panel_init(void)
{
	register_common_panel(&s6e3hae_rainbow_b0_panel_info);
	return 0;
}

__visible_for_testing void __exit s6e3hae_b0_panel_exit(void)
{
	deregister_common_panel(&s6e3hae_rainbow_b0_panel_info);
}

module_init(s6e3hae_b0_panel_init)
module_exit(s6e3hae_b0_panel_exit)

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
