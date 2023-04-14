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
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../maptbl.h"
#include "../mdnie.h"
#include "oled_common.h"

int getidx_mdnie_scenario_mode_maptbl(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = tbl->pdata;
	mdnie = &panel->mdnie;

	return maptbl_index(tbl, mdnie->props.scenario, mdnie->props.mode, 0);
}
EXPORT_SYMBOL(getidx_mdnie_scenario_mode_maptbl);

#ifdef CONFIG_SUPPORT_HMD
int getidx_mdnie_hmd_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	return maptbl_get_indexof_row(tbl, mdnie->props.hmd);
}
EXPORT_SYMBOL(getidx_mdnie_hmd_maptbl);
#endif

int getidx_mdnie_hdr_maptbl(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = tbl->pdata;
	mdnie = &panel->mdnie;

	return maptbl_get_indexof_row(tbl, mdnie->props.hdr);
}
EXPORT_SYMBOL(getidx_mdnie_hdr_maptbl);

int getidx_mdnie_trans_mode_maptbl(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = tbl->pdata;
	mdnie = &panel->mdnie;

	if (mdnie->props.trans_mode == TRANS_OFF)
		panel_dbg("mdnie trans_mode off\n");

	return maptbl_get_indexof_row(tbl, mdnie->props.trans_mode);
}
EXPORT_SYMBOL(getidx_mdnie_trans_mode_maptbl);

int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl)
{
	int mode = NIGHT_MODE_OFF;
	struct panel_device *panel;
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = tbl->pdata;
	mdnie = &panel->mdnie;

	if (mdnie->props.mode != AUTO)
		mode = NIGHT_MODE_ON;

	return maptbl_index(tbl, mode, mdnie->props.night_level, 0);
}
EXPORT_SYMBOL(getidx_mdnie_night_mode_maptbl);

int getidx_color_lens_maptbl(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = tbl->pdata;
	mdnie = &panel->mdnie;

	if (!IS_COLOR_LENS_MODE(mdnie))
		return -EINVAL;

	if ((mdnie->props.color_lens_color < 0) || (mdnie->props.color_lens_color >= COLOR_LENS_COLOR_MAX)) {
		panel_err("out of color lens color range %d\n", mdnie->props.color_lens_color);
		return -EINVAL;
	}

	if ((mdnie->props.color_lens_level < 0) || (mdnie->props.color_lens_level >= COLOR_LENS_LEVEL_MAX)) {
		panel_err("out of color lens level range %d\n", mdnie->props.color_lens_level);
		return -EINVAL;
	}

	return maptbl_index(tbl, mdnie->props.color_lens_color, mdnie->props.color_lens_level, 0);
}
EXPORT_SYMBOL(getidx_color_lens_maptbl);

void copy_scr_white_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;
	u8 r, g, b;
	int wcrd_type[MODE_MAX] = {
		WCRD_TYPE_D65, WCRD_TYPE_D65, WCRD_TYPE_D65,
		WCRD_TYPE_ADAPTIVE, WCRD_TYPE_ADAPTIVE,
	};
	int type;

	if (!tbl || !tbl->pdata || !dst)
		return;

	panel = tbl->pdata;
	mdnie = &panel->mdnie;

	if (mdnie->props.mode >= MODE_MAX) {
		panel_err("invalid mode(%d)\n", mdnie->props.mode);
		return;
	}
	type = wcrd_type[mdnie->props.mode];

	if (mdnie->props.scr_white_mode ==
			SCR_WHITE_MODE_COLOR_COORDINATE) {
		r = mdnie->props.coord_wrgb[type][RED];
		g = mdnie->props.coord_wrgb[type][GREEN];
		b = mdnie->props.coord_wrgb[type][BLUE];
	} else if (mdnie->props.scr_white_mode ==
			SCR_WHITE_MODE_ADJUST_LDU) {
		r = mdnie->props.adjust_ldu_wrgb[type][mdnie->props.ldu][RED];
		g = mdnie->props.adjust_ldu_wrgb[type][mdnie->props.ldu][GREEN];
		b = mdnie->props.adjust_ldu_wrgb[type][mdnie->props.ldu][BLUE];
	} else if (mdnie->props.scr_white_mode ==
			SCR_WHITE_MODE_SENSOR_RGB) {
		r = mdnie->props.ssr_wrgb[RED];
		g = mdnie->props.ssr_wrgb[GREEN];
		b = mdnie->props.ssr_wrgb[BLUE];
	} else {
		r = dst[RED * mdnie->props.scr_white_len];
		g = dst[GREEN * mdnie->props.scr_white_len];
		b = dst[BLUE * mdnie->props.scr_white_len];
		mdnie_set_cur_wrgb(mdnie, r, g, b);
		return;
	}

	mdnie_update_wrgb(mdnie, r, g, b);
	mdnie_cur_wrgb_to_byte_array(mdnie, dst,
			mdnie->props.scr_white_len);
}
EXPORT_SYMBOL(copy_scr_white_maptbl);

void copy_scr_cr_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata || !dst)
		return;

	panel = tbl->pdata;
	mdnie = &panel->mdnie;

	memcpy(dst, mdnie->props.scr, mdnie->props.sz_scr);
}
EXPORT_SYMBOL(copy_scr_cr_maptbl);

#ifdef CONFIG_SUPPORT_AFC
void copy_afc_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata || !dst)
		return;

	panel = tbl->pdata;
	mdnie = &panel->mdnie;
	memcpy(dst, mdnie->props.afc_roi,
			sizeof(mdnie->props.afc_roi));

	panel_dbg("afc_on %d\n", mdnie->props.afc_on);
	print_data(dst, sizeof(mdnie->props.afc_roi));
}
EXPORT_SYMBOL(copy_afc_maptbl);
#endif

MODULE_DESCRIPTION("oled_common_mdnie driver");
MODULE_LICENSE("GPL");
