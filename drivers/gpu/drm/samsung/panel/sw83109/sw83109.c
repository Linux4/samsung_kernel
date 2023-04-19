/*
 * linux/drivers/video/fbdev/exynos/panel/sw83109/sw83109.c
 *
 * SW83109 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "../panel_kunit.h"
#include "../panel.h"
#include "sw83109.h"
#ifdef CONFIG_PANEL_AID_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"
#include "../panel_debug.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#if IS_ENABLED(CONFIG_DISPLAY_USE_INFO)
#include "../dpui.h"
#endif

#include "../abd.h"

#ifdef CONFIG_PANEL_AID_DIMMING
int generate_brt_step_table(struct brightness_table *brt_tbl)
{
	int ret = 0;
	int i = 0, j = 0, k = 0;

	if (unlikely(!brt_tbl || !brt_tbl->brt)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}
	if (unlikely(!brt_tbl->step_cnt)) {
		if (likely(brt_tbl->brt_to_step)) {
			panel_info("we use static step table\n");
			return ret;
		} else {
			panel_err("invalid parameter, all table is NULL\n");
			return -EINVAL;
		}
	}

	brt_tbl->sz_brt_to_step = 0;
	for(i = 0; i < brt_tbl->sz_step_cnt; i++)
		brt_tbl->sz_brt_to_step += brt_tbl->step_cnt[i];

	brt_tbl->brt_to_step =
		(u32 *)kmalloc(brt_tbl->sz_brt_to_step * sizeof(u32), GFP_KERNEL);

	if (unlikely(!brt_tbl->brt_to_step)) {
		panel_err("alloc fail\n");
		return -EINVAL;
	}
	brt_tbl->brt_to_step[0] = brt_tbl->brt[0];
	i = 1;
	while (i < brt_tbl->sz_brt_to_step) {
		for (k = 1; k < brt_tbl->sz_brt; k++) {
			for (j = 1; j <= brt_tbl->step_cnt[k]; j++, i++) {
				brt_tbl->brt_to_step[i] = disp_interpolation64(brt_tbl->brt[k - 1] * disp_pow(10, 2),
					brt_tbl->brt[k] * disp_pow(10, 2), j, brt_tbl->step_cnt[k]);
				brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				brt_tbl->brt_to_step[i] = disp_div64(brt_tbl->brt_to_step[i], disp_pow(10, 2));
				if (brt_tbl->brt[brt_tbl->sz_brt - 1] < brt_tbl->brt_to_step[i]) {

					brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				}
				if (i >= brt_tbl->sz_brt_to_step) {
					panel_err("step cnt over %d %d\n", i, brt_tbl->sz_brt_to_step);
					break;
				}
			}
		}
	}
	return ret;
}
#endif /* CONFIG_PANEL_AID_DIMMING */

#ifdef CONFIG_MCD_PANEL_LPM
#ifdef CONFIG_SUPPORT_AOD_BL
int init_aod_dimming_table(struct maptbl *tbl)
{
	int id = PANEL_BL_SUBDEV_TYPE_AOD;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %pK, pdata %pK)\n",
				id, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_bl = &panel->panel_bl;

	if (unlikely(!panel->panel_data.panel_dim_info[id])) {
		panel_err("panel_bl-%d panel_dim_info is null\n", id);
		return -EINVAL;
	}

	memcpy(&panel_bl->subdev[id].brt_tbl,
			panel->panel_data.panel_dim_info[id]->brt_tbl,
			sizeof(struct brightness_table));

	return 0;
}
#endif
#endif

void copy_tset_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	*dst = (panel_data->props.temperature < 0) ?
		BIT(7) | abs(panel_data->props.temperature) :
		panel_data->props.temperature;
}

int init_lpm_brt_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return init_common_table(tbl);
#endif
}

int getidx_lpm_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_MCD_PANEL_LPM
#ifdef CONFIG_SUPPORT_AOD_BL
	panel_bl = &panel->panel_bl;
	row = get_subdev_actual_brightness_index(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD,
			panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness);

	props->lpm_brightness = panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness;
	panel_info("alpm_mode %d, brightness %d, row %d\n", props->cur_alpm_mode,
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness, row);

#else
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
	case HLPM_LOW_BR:
		row = 0;
		break;
	case ALPM_HIGH_BR:
	case HLPM_HIGH_BR:
		row = maptbl_get_countof_row(tbl) - 1;
		break;
	default:
		panel_err("Invalid alpm mode : %d\n", props->alpm_mode);
		break;
	}

	panel_info("alpm_mode %d, row %d\n", props->alpm_mode, row);
#endif
#endif

	return maptbl_index(tbl, 0, row, 0);
}

int getidx_sw83109_vrr_fps(int vrr_fps)
{
	int fps_index = SW83109_VRR_FPS_60;

	switch (vrr_fps) {
	case 120:
		fps_index = SW83109_VRR_FPS_120;
		break;
	case 90:
		fps_index = SW83109_VRR_FPS_90;
		break;
	case 60:
		fps_index = SW83109_VRR_FPS_60;
		break;
	default:
		fps_index = SW83109_VRR_FPS_60;
		panel_err("undefined FPS:%d\n", vrr_fps);
	}

	return fps_index;
}

int getidx_sw83109_current_vrr_fps(struct panel_device *panel)
{
	int vrr_fps;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	return getidx_sw83109_vrr_fps(vrr_fps);
}

int getidx_vrr_fps_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0, index;

	index = getidx_sw83109_current_vrr_fps(panel);
	if (index < 0)
		row = 0;
	else
		row = index;

	return maptbl_index(tbl, layer, row, 0);
}

void copy_fmem_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	u8 val[SW83109_TRIM_LEN] = { 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12 };
	int ret = 0;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	ret = resource_copy_by_name(panel, val, "trim");
	if (unlikely(ret < 0)) {
		panel_err("%s not found in panel resource\n", "trim");
		return;
	}

	val[8] = val[8] + 0x04;
	memcpy(dst, val, SW83109_TRIM_LEN);
}

int init_gamma_mode2_brt_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;
	//todo:remove
	panel_info("++\n");
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (panel_dim_info == NULL) {
		panel_err("panel_dim_info is null\n");
		return -EINVAL;
	}

	if (panel_dim_info->brt_tbl == NULL) {
		panel_err("panel_dim_info->brt_tbl is null\n");
		return -EINVAL;
	}

	generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[PANEL_BL_SUBDEV_TYPE_DISP].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	return 0;
}

int getidx_gamma_mode2_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;

	row = get_brightness_pac_step_by_subdev_id(panel_bl, 0, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}


int sw83109_getidx_ffc_table(struct maptbl *tbl)
{
	int idx;
	u32 dsi_clk;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	dsi_clk = panel_data->props.dsi_freq;
	panel_info("dsi_clk: %dkz\n", dsi_clk);

	switch (dsi_clk) {
	case 1443000:
		idx = SW83109_HS_CLK_1443;
		break;
	case 1462000:
		idx = SW83109_HS_CLK_1462;
		break;
	case 1471000:
		idx = SW83109_HS_CLK_1471;
		break;
	default:
		panel_err("invalid dsi clock: %d\n", dsi_clk);
		idx = SW83109_HS_CLK_1443;
		break;
	}
	return maptbl_index(tbl, 0, idx, 0);
}

int sw83109_get_cell_id(struct panel_device *panel, void *buf)
{
	u8 cell_id[11] = { 0, };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	resource_copy_by_name(panel, cell_id, "date");

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		cell_id[0], cell_id[1], cell_id[2], cell_id[3], cell_id[4],
		cell_id[5], cell_id[6], cell_id[7], cell_id[8], cell_id[9], cell_id[10]);
	return 0;
}

int sw83109_get_manufacture_code(struct panel_device *panel, void *buf)
{
	u8 code[10] = { 0, };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	resource_copy_by_name(panel, code, "code");

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		code[0], code[1], code[2], code[3], code[4],
		code[5], code[6], code[7], code[8], code[9]);
	return 0;
}

int sw83109_get_manufacture_date(struct panel_device *panel, void *buf)
{
	u8 date[11] = { 0, };
	u16 year;
	u8 month, day, hour, min, sec;
	u16 ms;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	resource_copy_by_name(panel, date, "date");

	year = ((date[0] & 0xF0) >> 4) + 2011;
	month = date[0] & 0xF;
	day = date[1] & 0x1F;
	hour = date[2] & 0x1F;
	min = date[3] & 0x3F;
	sec = date[4];
	ms = (date[5] << 8) | date[6];

	snprintf(buf, PAGE_SIZE, "%04d, %02d, %02d, %02d:%02d:%02d.%04d\n", year, month, day, hour, min, sec, ms);

	return 0;
}

int sw83109_getidx_acl_opr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;
	int row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;
	row = panel_bl_get_acl_opr(&panel->panel_bl);

	if (row == 0)
		row = SW83109_ACL_OPR_0;
	else
		row = SW83109_ACL_OPR_2;
	if (is_hbm_brightness(panel_bl, panel_bl->props.brightness))
		row = SW83109_ACL_OPR_1;

	if (row >= MAX_SW83109_ACL_OPR) {
		panel_warn("invalid range %d\n", row);
		row = SW83109_ACL_OPR_2;
	}

	return maptbl_index(tbl, 0, row, 0);
}

int sw83109_getidx_acl_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	row = panel_bl_get_acl_pwrsave(&panel->panel_bl);

	return maptbl_index(tbl, 0, row, 0);
}

bool is_panel_state_acl(struct panel_device *panel)
{
	if (panel_bl_get_acl_pwrsave(&panel->panel_bl) == ACL_PWRSAVE_ON)
		return true;
	return false;
}

bool is_panel_state_not_lpm(struct panel_device *panel)
{
	if (panel->state.cur_state != PANEL_STATE_ALPM)
		return true;

	return false;
}

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
