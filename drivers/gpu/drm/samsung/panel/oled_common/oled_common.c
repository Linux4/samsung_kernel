/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include "../panel_kunit.h"
#include "../panel.h"
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../maptbl.h"

int init_common_table(struct maptbl *tbl)
{
	if (!tbl) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (!tbl->pdata) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(init_common_table);

int getidx_common_maptbl(struct maptbl *tbl)
{
	return 0;
}
EXPORT_SYMBOL(getidx_common_maptbl);

void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst)
{
}
EXPORT_SYMBOL(copy_dummy_maptbl);

void copy_common_maptbl(struct maptbl *tbl, u8 *dst)
{
	int idx;

	if (!tbl) {
		panel_err("tbl is null\n");
		return;
	}

	if (!dst) {
		panel_err("dst is null\n");
		return;
	}

	if (!tbl->arr) {
		panel_err("maptbl(%s) source buffer is null\n",
				maptbl_get_name(tbl));
		return;
	}

	if (maptbl_get_sizeof_copy(tbl) == 0) {
		panel_err("maptbl(%s) sizeof_copy is zero\n",
				maptbl_get_name(tbl));
		return;
	}

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		panel_err("maptbl(%s) failed to get index(%d)\n",
				maptbl_get_name(tbl), idx);
		return;
	}

	memcpy(dst, &(tbl->arr)[idx],
			sizeof(u8) * maptbl_get_sizeof_copy(tbl));
	panel_dbg("maptbl(%s) copy to dst(index:%d size:%d)\n",
			maptbl_get_name(tbl), idx, maptbl_get_sizeof_copy(tbl));
	print_data(dst, maptbl_get_sizeof_copy(tbl));
}
EXPORT_SYMBOL(copy_common_maptbl);

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
void copy_copr_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct copr_info *copr;

	if (!tbl || !dst)
		return;

	panel = tbl->pdata;
	if (unlikely(!panel))
		return;

	copr = &panel->copr;
	copr_reg_to_byte_array(&copr->props.reg,
			copr->props.version, dst);
}
EXPORT_SYMBOL(copy_copr_maptbl);
#endif

MODULE_DESCRIPTION("oled_common driver");
MODULE_LICENSE("GPL");
