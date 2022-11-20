/*
 * linux/drivers/video/fbdev/exynos/panel/ea8076/ea8076.c
 *
 * ea8076 Dimming Driver
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
#include "ea8076.h"
#include "ea8076_panel.h"

#include "../dimming.h"
#include "../panel_dimming.h"

#include "../panel_drv.h"

int init_common_table(struct maptbl *tbl)
{
	if (tbl == NULL) {
		panel_err("PANEL:ERR:%s:maptbl is null\n", __func__);
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("PANEL:ERR:%s:pdata is null\n", __func__);
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_EXYNOS_DECON_MDNIE
static int getidx_common_maptbl(struct maptbl *tbl)
{
	return 0;
}
#endif

#if 0
static int getidx_mps_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;
	int row;

	if (panel == NULL) {
		panel_err("PANEL:ERR:%s:panel is null\n", __func__);
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;
	row = (get_actual_brightness(panel_bl,
			panel_bl->props.brightness) <= 39) ? 0 : 1;

	return maptbl_index(tbl, 0, row, 0);
}
#endif

#if 0
void show_brt_step(struct brightness_table *brt_tbl)
{
	int i;
	char buf[1024];
	int len = 0;
	if (unlikely(!brt_tbl || !brt_tbl->brt_to_step)) {
		pr_err("%s, invalid parameter\n", __func__);
		return ;
	}
	for(i = 1; i <= brt_tbl->sz_brt_to_step; i++) {
		len += snprintf(buf + len, sizeof(buf) - len, "%d ", brt_tbl->brt_to_step[i - 1]);
		if((i != 0) && (i % 10 == 0)) {
			pr_info("%s %d %s\n", __func__, brt_tbl->sz_brt_to_step, buf);
			len = 0;
		}
	}
	pr_info("%s %d %s\n", __func__, brt_tbl->sz_brt_to_step, buf);
}
#endif

#ifdef CONFIG_SUPPORT_DOZE
#ifdef CONFIG_SUPPORT_AOD_BL
static int init_aod_dimming_table(struct maptbl *tbl)
{
	int id = PANEL_BL_SUBDEV_TYPE_AOD;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("%s panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				__func__, id, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_bl = &panel->panel_bl;

	if (unlikely(!panel->panel_data.panel_dim_info[id])) {
		pr_err("%s panel_bl-%d panel_dim_info is null\n", __func__, id);
		return -EINVAL;
	}

	memcpy(&panel_bl->subdev[id].brt_tbl,
			panel->panel_data.panel_dim_info[id]->brt_tbl,
			sizeof(struct brightness_table));

	return 0;
}
#endif
#endif

static int generate_brt_step_table(struct brightness_table *brt_tbl)
{
	int ret = 0;
	int i = 0, j = 0, k = 0;

	if (unlikely(!brt_tbl || !brt_tbl->brt /* || !brt_tbl->lum */)) {
		pr_err("%s, invalid parameter\n", __func__);
		return -EINVAL;
	}
	if (unlikely(!brt_tbl->step_cnt)) {
		if(likely(brt_tbl->brt_to_step)) {
			pr_info("%s we use static step table\n", __func__);
			return ret;
		} else {
			pr_err("%s, invalid parameter, all table is NULL\n", __func__);
			return -EINVAL;
		}
	}

	brt_tbl->sz_brt_to_step = 0;
	for(i = 0; i < brt_tbl->sz_step_cnt; i++)
		brt_tbl->sz_brt_to_step += brt_tbl->step_cnt[i];

	brt_tbl->brt_to_step = (u32 *)kmalloc(brt_tbl->sz_brt_to_step * sizeof(u32), GFP_KERNEL);

	if (unlikely(!brt_tbl->brt_to_step)) {
		pr_err("%s, alloc fail\n", __func__);
		return -EINVAL;
	}
	brt_tbl->brt_to_step[0] = brt_tbl->brt[0];
	i = 1;
	while (i < brt_tbl->sz_brt_to_step) {
		for (k = 1; k < brt_tbl->sz_brt; k++) {
			for (j = 1; j <= brt_tbl->step_cnt[k]; j++, i++) {
				brt_tbl->brt_to_step[i] = interpolation(brt_tbl->brt[k - 1] * disp_pow(10, 2),
					brt_tbl->brt[k] * disp_pow(10, 2), j, brt_tbl->step_cnt[k]);
				brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				brt_tbl->brt_to_step[i] = disp_div64(brt_tbl->brt_to_step[i], disp_pow(10, 2));
				if(brt_tbl->brt[brt_tbl->sz_brt - 1 /*sz_ui_lum - 1 */] < brt_tbl->brt_to_step[i]) {
					brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				}
				if(i >= brt_tbl->sz_brt_to_step) {
					pr_err("%s step cnt over %d %d\n", __func__, i, brt_tbl->sz_brt_to_step);
					break;
				}
			}
		}
	}

	return ret;
}

static int init_brightness_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;

	if (tbl == NULL) {
		panel_err("PANEL:ERR:%s:maptbl is null\n", __func__);
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("PANEL:ERR:%s:pdata is null\n", __func__);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (panel_dim_info == NULL)
		panel_err("PANEL:ERR:%s:panel_dim_info is null\n", __func__);

	if (panel_dim_info->brt_tbl == NULL)
		panel_err("PANEL:ERR:%s:panel_dim_info->brt_tbl is null\n", __func__);

	generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[PANEL_BL_SUBDEV_TYPE_DISP].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	return 0;
}

static int init_elvss_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_DIM_FLASH
	struct panel_device *panel = tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	if (panel_data->props.cur_dim_type == DIM_TYPE_DIM_FLASH)
		return init_maptbl_from_flash(tbl, DIM_FLASH_ELVSS);
	else
		return init_maptbl_from_table(tbl, DIM_FLASH_ELVSS);

	return 0;
#else
	return init_common_table(tbl);
#endif
}

#if 0
static int init_elvss_temp_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	int i, layer, box, ret;
	u8 elvss_temp_0_otp_value = 0;
	u8 elvss_temp_1_otp_value = 0;

	if (tbl == NULL) {
		panel_err("PANEL:ERR:%s:maptbl is null\n", __func__);
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("PANEL:ERR:%s:pdata is null\n", __func__);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	ret = resource_copy_by_name(panel_data, &elvss_temp_0_otp_value, "elvss_temp_0");
	if (unlikely(ret)) {
		pr_err("%s, elvss_temp not found in panel resource\n", __func__);
		return -EINVAL;
	}

	ret = resource_copy_by_name(panel_data, &elvss_temp_1_otp_value, "elvss_temp_1");
	if (unlikely(ret)) {
		pr_err("%s, elvss_temp not found in panel resource\n", __func__);
		return -EINVAL;
	}

	for_each_box(tbl, box) {
		for_each_layer(tbl, layer) {
			for_each_row(tbl, i) {
				tbl->arr[layer * sizeof_layer(tbl) + i] =
					(i < EA8076_NR_LUMINANCE) ?
					elvss_temp_0_otp_value : elvss_temp_1_otp_value;
			}
		}
	}

	return 0;
}

static int getidx_elvss_temp_table(struct maptbl *tbl)
{
	int row, layer, box = 0;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("PANEL:ERR:%s:panel is null\n", __func__);
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	layer = (UNDER_MINUS_15(panel_data->props.temperature) ? UNDER_MINUS_FIFTEEN :
			(UNDER_0(panel_data->props.temperature) ? UNDER_ZERO : OVER_ZERO));
//	row = get_actual_brightness_index(panel_bl, panel_bl->props.brightness);
	row = panel_bl->props.brightness; // platform brightness

	return maptbl_4d_index(tbl, box, layer, row, 0);
}
#endif

static int getidx_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("PANEL:ERR:%s:panel is null\n", __func__);
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	panel_data = &panel->panel_data;

	row = get_brightness_pac_step(panel_bl, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}

#if 0
static int getidx_acl_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("PANEL:ERR:%s:panel is null\n", __func__);
		return -EINVAL;
	}

	return maptbl_index(tbl, 0, panel_bl_get_acl_pwrsave(&panel->panel_bl), 0);
}

#endif
static int getidx_brt_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;

	if (panel == NULL) {
		panel_err("PANEL:ERR:%s:panel is null\n", __func__);
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;

	return maptbl_index(tbl, panel_bl->props.smooth_transition,
			is_hbm_brightness(panel_bl, panel_bl->props.brightness), 0);
}

static int getidx_acl_opr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("PANEL:ERR:%s:panel is null\n", __func__);
		return -EINVAL;
	}

	return maptbl_index(tbl, 0, panel_bl_get_acl_pwrsave(&panel->panel_bl) ?
			panel_bl_get_acl_opr(&panel->panel_bl) : 0, 0);
}

static int getidx_fps_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;
	int row = 0, layer = 0, index;

	index = search_table_u32(EA8076_VRR_FPS,
			ARRAY_SIZE(EA8076_VRR_FPS), props->vrr.mode);

	if (index < 0)
		row = 0;
	else
		row = index;

	return maptbl_index(tbl, layer, row, 0);
}

static int init_lpm_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return init_common_table(tbl);
#endif
}

static int getidx_lpm_table(struct maptbl *tbl)
{
	int layer = 0, row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_SUPPORT_DOZE
#ifdef CONFIG_SUPPORT_AOD_BL
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
	case ALPM_HIGH_BR:
		layer = ALPM_MODE;
		break;
	case HLPM_LOW_BR:
	case HLPM_HIGH_BR:
		layer = HLPM_MODE;
		break;
	default:
		panel_err("PANEL:ERR:%s:Invalid alpm mode : %d\n",
				__func__, props->alpm_mode);
		break;
	}

	panel_bl = &panel->panel_bl;
	row = get_subdev_actual_brightness_index(panel_bl,
			PANEL_BL_SUBDEV_TYPE_AOD,
			panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness);

	props->lpm_brightness =
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness;

	pr_info("%s alpm_mode %d, brightness %d, layer %d row %d\n",
			__func__, props->alpm_mode,
			panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness,
			layer, row);

#else
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
		layer = ALPM_MODE;
		row = 0;
		break;
	case HLPM_LOW_BR:
		layer = HLPM_MODE;
		row = 0;
		break;
	case ALPM_HIGH_BR:
		layer = ALPM_MODE;
		row = tbl->nrow - 1;
		break;
	case HLPM_HIGH_BR:
		layer = HLPM_MODE;
		row = tbl->nrow - 1;
		break;
	default:
		panel_err("PANEL:ERR:%s:Invalid alpm mode : %d\n",
				__func__, props->alpm_mode);
		break;
	}

	pr_debug("%s alpm_mode %d, layer %d row %d\n",
			__func__, props->alpm_mode, layer, row);
#endif
#endif
	props->cur_alpm_mode = props->alpm_mode;

	return maptbl_index(tbl, layer, row, 0);
}

static void copy_common_maptbl(struct maptbl *tbl, u8 *dst)
{
	int idx;

	if (!tbl || !dst) {
		pr_err("%s, invalid parameter (tbl %p, dst %p\n",
				__func__, tbl, dst);
		return;
	}

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		pr_err("%s, failed to getidx %d\n", __func__, idx);
		return;
	}

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * tbl->sz_copy);
#ifdef DEBUG_PANEL
	panel_dbg("%s copy from %s %d %d\n",
			__func__, tbl->name, idx, tbl->sz_copy);
	print_data(dst, tbl->sz_copy);
#endif
}

static void copy_elvss_otp_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;

	if (!tbl || !dst) {
		pr_err("%s, invalid parameter (tbl %p, dst %p\n",
				__func__, tbl, dst);
		return;
	}

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel)) {
		pr_err("ERR:%s:panel is null\n", __func__);
		return;
	}

	panel_data = &panel->panel_data;

	resource_copy_by_name(panel_data, dst, "elvss");
}

static void copy_tset_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;

	if (!tbl || !dst) {
		pr_err("%s, invalid parameter (tbl %p, dst %p\n",
				__func__, tbl, dst);
		return;
	}

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel)) {
		pr_err("ERR:%s:panel is null\n", __func__);
		return;
	}

	panel_data = &panel->panel_data;

	*dst = (panel_data->props.temperature < 0) ?
		BIT(7) | abs(panel_data->props.temperature) :
		panel_data->props.temperature;
}

#ifdef CONFIG_EXYNOS_DECON_MDNIE
static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst)
{
	return;
}

static int init_color_blind_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (tbl == NULL) {
		panel_err("PANEL:ERR:%s:maptbl is null\n", __func__);
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("PANEL:ERR:%s:pdata is null\n", __func__);
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (EA8076_SCR_CR_OFS + mdnie->props.sz_scr > sizeof_maptbl(tbl)) {
		panel_err("%s invalid size (maptbl_size %d, sz_scr %d)\n",
			__func__, sizeof_maptbl(tbl), mdnie->props.sz_scr);
		return -EINVAL;
	}

	memcpy(&tbl->arr[EA8076_SCR_CR_OFS],
			mdnie->props.scr, mdnie->props.sz_scr);

	return 0;
}

static int getidx_mdnie_scenario_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	return tbl->ncol * (mdnie->props.mode);
}

static int getidx_mdnie_hdr_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	return tbl->ncol * (mdnie->props.hdr);
}

static int getidx_mdnie_trans_mode_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	if (mdnie->props.trans_mode == TRANS_OFF)
		panel_dbg("%s mdnie trans_mode off\n", __func__);
	return tbl->ncol * (mdnie->props.trans_mode);
}

static int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	return tbl->ncol * (mdnie->props.night_level);
}

static int init_mdnie_night_mode_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl *night_maptbl;

	if (tbl == NULL) {
		panel_err("PANEL:ERR:%s:maptbl is null\n", __func__);
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("PANEL:ERR:%s:pdata is null\n", __func__);
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	night_maptbl = mdnie_find_etc_maptbl(mdnie, MDNIE_ETC_NIGHT_MAPTBL);
	if (!night_maptbl) {
		panel_err("%s, NIGHT_MAPTBL not found\n", __func__);
		return -EINVAL;
	}

	if (sizeof_maptbl(tbl) < (EA8076_NIGHT_MODE_OFS +
			sizeof_row(night_maptbl))) {
		panel_err("%s invalid size (maptbl_size %d, night_maptbl_size %d)\n",
			__func__, sizeof_maptbl(tbl), sizeof_row(night_maptbl));
		return -EINVAL;
	}

	maptbl_copy(night_maptbl, &tbl->arr[EA8076_NIGHT_MODE_OFS]);

	return 0;
}

static int init_mdnie_color_lens_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl *color_lens_maptbl;

	if (tbl == NULL) {
		panel_err("PANEL:ERR:%s:maptbl is null\n", __func__);
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("PANEL:ERR:%s:pdata is null\n", __func__);
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	color_lens_maptbl = mdnie_find_etc_maptbl(mdnie, MDNIE_ETC_COLOR_LENS_MAPTBL);
	if (!color_lens_maptbl) {
		panel_err("%s, COLOR_LENS_MAPTBL not found\n", __func__);
		return -EINVAL;
	}

	if (sizeof_maptbl(tbl) < (EA8076_COLOR_LENS_OFS +
			sizeof_row(color_lens_maptbl))) {
		panel_err("%s invalid size (maptbl_size %d, color_lens_maptbl_size %d)\n",
			__func__, sizeof_maptbl(tbl), sizeof_row(color_lens_maptbl));
		return -EINVAL;
	}

	if (IS_COLOR_LENS_MODE(mdnie))
		maptbl_copy(color_lens_maptbl, &tbl->arr[EA8076_COLOR_LENS_OFS]);

	return 0;
}

static void update_current_scr_white(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata) {
		pr_err("%s, invalid param\n", __func__);
		return;
	}

	mdnie = (struct mdnie_info *)tbl->pdata;
	mdnie->props.cur_wrgb[0] = *dst;
	mdnie->props.cur_wrgb[1] = *(dst + 2);
	mdnie->props.cur_wrgb[2] = *(dst + 4);
}

static int init_color_coordinate_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	int type, color;

	if (tbl == NULL) {
		panel_err("PANEL:ERR:%s:maptbl is null\n", __func__);
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("PANEL:ERR:%s:pdata is null\n", __func__);
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (sizeof_row(tbl) != ARRAY_SIZE(mdnie->props.coord_wrgb[0])) {
		panel_err("%s invalid maptbl size %d\n", __func__, tbl->ncol);
		return -EINVAL;
	}

	for_each_row(tbl, type) {
		for_each_col(tbl, color) {
			tbl->arr[sizeof_row(tbl) * type + color] =
				mdnie->props.coord_wrgb[type][color];
		}
	}

	return 0;
}

static int init_sensor_rgb_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	int i;

	if (tbl == NULL) {
		panel_err("PANEL:ERR:%s:maptbl is null\n", __func__);
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("PANEL:ERR:%s:pdata is null\n", __func__);
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (tbl->ncol != ARRAY_SIZE(mdnie->props.ssr_wrgb)) {
		panel_err("%s invalid maptbl size %d\n", __func__, tbl->ncol);
		return -EINVAL;
	}

	for (i = 0; i < tbl->ncol; i++)
		tbl->arr[i] = mdnie->props.ssr_wrgb[i];

	return 0;
}

static int getidx_color_coordinate_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;
	static int wcrd_type[MODE_MAX] = {
		WCRD_TYPE_D65, WCRD_TYPE_D65, WCRD_TYPE_D65,
		WCRD_TYPE_ADAPTIVE, WCRD_TYPE_ADAPTIVE,
	};
	if ((mdnie->props.mode < 0) || (mdnie->props.mode >= MODE_MAX)) {
		pr_err("%s, out of mode range %d\n", __func__, mdnie->props.mode);
		return -EINVAL;
	}
	return maptbl_index(tbl, 0, wcrd_type[mdnie->props.mode], 0);
}

static int getidx_adjust_ldu_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;
	static int wcrd_type[MODE_MAX] = {
		WCRD_TYPE_D65, WCRD_TYPE_D65, WCRD_TYPE_D65,
		WCRD_TYPE_ADAPTIVE, WCRD_TYPE_ADAPTIVE,
	};

	if (!IS_LDU_MODE(mdnie))
		return -EINVAL;

	if ((mdnie->props.mode < 0) || (mdnie->props.mode >= MODE_MAX)) {
		pr_err("%s, out of mode range %d\n", __func__, mdnie->props.mode);
		return -EINVAL;
	}
	if ((mdnie->props.ldu < 0) || (mdnie->props.ldu >= MAX_LDU_MODE)) {
		pr_err("%s, out of ldu mode range %d\n", __func__, mdnie->props.ldu);
		return -EINVAL;
	}
	return maptbl_index(tbl, wcrd_type[mdnie->props.mode], mdnie->props.ldu, 0);
}

static int getidx_color_lens_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	if (!IS_COLOR_LENS_MODE(mdnie))
		return -EINVAL;

	if ((mdnie->props.color_lens_color < 0) || (mdnie->props.color_lens_color >= COLOR_LENS_COLOR_MAX)) {
		pr_err("%s, out of color lens color range %d\n", __func__, mdnie->props.color_lens_color);
		return -EINVAL;
	}
	if ((mdnie->props.color_lens_level < 0) || (mdnie->props.color_lens_level >= COLOR_LENS_LEVEL_MAX)) {
		pr_err("%s, out of color lens level range %d\n", __func__, mdnie->props.color_lens_level);
		return -EINVAL;
	}
	return maptbl_index(tbl, mdnie->props.color_lens_color, mdnie->props.color_lens_level, 0);
}

static void copy_color_coordinate_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;
	int i, idx;
	u8 value;

	if (unlikely(!tbl || !dst))
		return;

	mdnie = (struct mdnie_info *)tbl->pdata;
	idx = maptbl_getidx(tbl);
	if (idx < 0 || (idx + MAX_COLOR > sizeof_maptbl(tbl))) {
		panel_err("%s invalid index %d\n", __func__, idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("%s invalid maptbl size %d\n", __func__, tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		mdnie->props.def_wrgb[i] = tbl->arr[idx + i];
		value = mdnie->props.def_wrgb[i] +
			(char)((mdnie->props.mode == AUTO) ?
				mdnie->props.def_wrgb_ofs[i] : 0);
		mdnie->props.cur_wrgb[i] = value;
		dst[i * 2] = value;

#ifdef DEBUG_PANEL
		if (mdnie->props.mode == AUTO) {
			panel_info("%s cur_wrgb[%d] %d(%02X) def_wrgb[%d] %d(%02X), def_wrgb_ofs[%d] %d\n",
					__func__, i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
					i, mdnie->props.def_wrgb[i], mdnie->props.def_wrgb[i],
					i, mdnie->props.def_wrgb_ofs[i]);
		} else {
			panel_info("%s cur_wrgb[%d] %d(%02X) def_wrgb[%d] %d(%02X), def_wrgb_ofs[%d] none\n",
					__func__, i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
					i, mdnie->props.def_wrgb[i], mdnie->props.def_wrgb[i], i);
		}
#endif
	}
}

static void copy_scr_white_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;
	int i, idx;

	if (unlikely(!tbl || !dst))
		return;

	mdnie = (struct mdnie_info *)tbl->pdata;
	idx = maptbl_getidx(tbl);
	if (idx < 0 || (idx + MAX_COLOR > sizeof_maptbl(tbl))) {
		panel_err("%s invalid index %d\n", __func__, idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("%s invalid maptbl size %d\n", __func__, tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		mdnie->props.cur_wrgb[i] = tbl->arr[idx + i];
		dst[i * 2] = tbl->arr[idx + i];
#ifdef DEBUG_PANEL
		panel_info("%s cur_wrgb[%d] %d(%02X)\n",
				__func__, i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i]);
#endif
	}
}

static void copy_adjust_ldu_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;
	int i, idx;
	u8 value;

	if (unlikely(!tbl || !dst))
		return;

	mdnie = (struct mdnie_info *)tbl->pdata;
	idx = maptbl_getidx(tbl);
	if (idx < 0 || (idx + MAX_COLOR > sizeof_maptbl(tbl))) {
		panel_err("%s invalid index %d\n", __func__, idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("%s invalid maptbl size %d\n", __func__, tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		value = tbl->arr[idx + i] +
			(((mdnie->props.mode == AUTO) && (mdnie->props.scenario != EBOOK_MODE)) ?
				mdnie->props.def_wrgb_ofs[i] : 0);
		mdnie->props.cur_wrgb[i] = value;
		dst[i * 2] = value;

#ifdef DEBUG_PANEL
		panel_info("%s cur_wrgb[%d] %d(%02X) (orig:0x%02X offset:%d)\n",
				__func__, i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
				tbl->arr[idx + i], mdnie->props.def_wrgb_ofs[i]);
#endif
	}
}

static int getidx_trans_maptbl(struct pkt_update_info *pktui)
{
	struct panel_device *panel = pktui->pdata;
	struct mdnie_info *mdnie = &panel->mdnie;

	return (mdnie->props.trans_mode == TRANS_ON) ?
		MDNIE_ETC_NONE_MAPTBL : MDNIE_ETC_TRANS_MAPTBL;
}

static int getidx_mdnie_maptbl(struct pkt_update_info *pktui, int offset)
{
	struct panel_device *panel = pktui->pdata;
	struct mdnie_info *mdnie = &panel->mdnie;
	int row = mdnie_get_maptbl_index(mdnie);
	int index;

	if (row < 0) {
		pr_err("%s, invalid row %d\n", __func__, row);
		return -EINVAL;
	}

	index = row * mdnie->nr_reg + offset;
	if (index >= mdnie->nr_maptbl) {
		panel_err("%s exceeded index %d row %d offset %d\n",
				__func__, index, row, offset);
		return -EINVAL;
	}
	return index;
}

static int getidx_mdnie_0_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 0);
}

static int getidx_mdnie_1_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 1);
}

static int getidx_mdnie_2_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 2);
}

static int getidx_mdnie_scr_white_maptbl(struct pkt_update_info *pktui)
{
	struct panel_device *panel = pktui->pdata;
	struct mdnie_info *mdnie = &panel->mdnie;
	int index;

	if (mdnie->props.scr_white_mode < 0 ||
			mdnie->props.scr_white_mode >= MAX_SCR_WHITE_MODE) {
		pr_warn("%s, out of range %d\n",
				__func__, mdnie->props.scr_white_mode);
		return -1;
	}

	if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_COLOR_COORDINATE) {
		pr_debug("%s, coordinate maptbl\n", __func__);
		index = MDNIE_COLOR_COORDINATE_MAPTBL;
	} else if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_ADJUST_LDU) {
		pr_debug("%s, adjust ldu maptbl\n", __func__);
		index = MDNIE_ADJUST_LDU_MAPTBL;
	} else if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_SENSOR_RGB) {
		pr_debug("%s, sensor rgb maptbl\n", __func__);
		index = MDNIE_SENSOR_RGB_MAPTBL;
	} else {
		pr_debug("%s, empty maptbl\n", __func__);
		index = MDNIE_SCR_WHITE_NONE_MAPTBL;
	}

	return index;
}

#ifdef CONFIG_SUPPORT_AFC
static void copy_afc_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;

	if (!tbl || !dst) {
		pr_err("%s, invalid parameter (tbl %p, dst %p\n",
				__func__, tbl, dst);
		return;
	}
	mdnie = (struct mdnie_info *)tbl->pdata;
	memcpy(dst, mdnie->props.afc_roi,
			sizeof(mdnie->props.afc_roi));

#ifdef DEBUG_PANEL
	pr_info("%s afc_on %d\n", __func__, mdnie->props.afc_on);
	print_data(dst, sizeof(mdnie->props.afc_roi));
#endif
}
#endif
#endif /* CONFIG_EXYNOS_DECON_MDNIE */

static void show_rddpm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddpm[EA8076_RDDPM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddpm;
#endif

	if (!res || ARRAY_SIZE(rddpm) != res->dlen) {
		pr_err("%s invalid resource\n", __func__);
		return;
	}

	ret = resource_copy(rddpm, info->res);
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to copy rddpm resource\n", __func__);
		return;
	}

	panel_info("========== SHOW PANEL [0Ah:RDDPM] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddpm[0], ((rddpm[0] & 0x9C) == 0x9C) ? "GOOD" : "NG");
	panel_info("* Bootster Mode : %s\n", rddpm[0] & 0x80 ? "ON (GD)" : "OFF (NG)");
	panel_info("* Idle Mode     : %s\n", rddpm[0] & 0x40 ? "ON (NG)" : "OFF (GD)");
	panel_info("* Partial Mode  : %s\n", rddpm[0] & 0x20 ? "ON" : "OFF");
	panel_info("* Sleep Mode    : %s\n", rddpm[0] & 0x10 ? "OUT (GD)" : "IN (NG)");
	panel_info("* Normal Mode   : %s\n", rddpm[0] & 0x08 ? "OK (GD)" : "SLEEP (NG)");
	panel_info("* Display ON    : %s\n", rddpm[0] & 0x04 ? "ON (GD)" : "OFF (NG)");
	panel_info("=================================================\n");
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	g_rddpm = (unsigned int)rddpm[0];
#endif
}

static void show_rddsm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddsm[EA8076_RDDSM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddsm;
#endif

	if (!res || ARRAY_SIZE(rddsm) != res->dlen) {
		pr_err("%s invalid resource\n", __func__);
		return;
	}

	ret = resource_copy(rddsm, info->res);
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to copy rddsm resource\n", __func__);
		return;
	}

	panel_info("========== SHOW PANEL [0Eh:RDDSM] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddsm[0], (rddsm[0] == 0x80) ? "GOOD" : "NG");
	panel_info("* TE Mode : %s\n", rddsm[0] & 0x80 ? "ON(GD)" : "OFF(NG)");
	panel_info("=================================================\n");
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	g_rddsm = (unsigned int)rddsm[0];
#endif
}

static void show_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 err[EA8076_ERR_LEN] = { 0, }, err_15_8, err_7_0;

	if (!res || ARRAY_SIZE(err) != res->dlen) {
		pr_err("%s invalid resource\n", __func__);
		return;
	}

	ret = resource_copy(err, info->res);
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to copy err resource\n", __func__);
		return;
	}

	err_15_8 = err[0];
	err_7_0 = err[1];

	panel_info("========== SHOW PANEL [EAh:DSIERR] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x%02x, Result : %s\n", err_15_8, err_7_0,
			(err[0] || err[1] || err[2] || err[3] || err[4]) ? "NG" : "GOOD");

	if (err_15_8 & 0x80)
		panel_info("* DSI Protocol Violation\n");

	if (err_15_8 & 0x40)
		panel_info("* Data P Lane Contention Detetion\n");

	if (err_15_8 & 0x20)
		panel_info("* Invalid Transmission Length\n");

	if (err_15_8 & 0x10)
		panel_info("* DSI VC ID Invalid\n");

	if (err_15_8 & 0x08)
		panel_info("* DSI Data Type Not Recognized\n");

	if (err_15_8 & 0x04)
		panel_info("* Checksum Error\n");

	if (err_15_8 & 0x02)
		panel_info("* ECC Error, multi-bit (detected, not corrected)\n");

	if (err_15_8 & 0x01)
		panel_info("* ECC Error, single-bit (detected and corrected)\n");

	if (err_7_0 & 0x80)
		panel_info("* Data Lane Contention Detection\n");

	if (err_7_0 & 0x40)
		panel_info("* False Control Error\n");

	if (err_7_0 & 0x20)
		panel_info("* HS RX Timeout\n");

	if (err_7_0 & 0x10)
		panel_info("* Low-Power Transmit Sync Error\n");

	if (err_7_0 & 0x08)
		panel_info("* Escape Mode Entry Command Error");

	if (err_7_0 & 0x04)
		panel_info("* EoT Sync Error\n");

	if (err_7_0 & 0x02)
		panel_info("* SoT Sync Error\n");

	if (err_7_0 & 0x01)
		panel_info("* SoT Error\n");

	panel_info("* CRC Error Count : %d\n", err[2]);
	panel_info("* ECC1 Error Count : %d\n", err[3]);
	panel_info("* ECC2 Error Count : %d\n", err[4]);

	panel_info("==================================================\n");
}

static void show_err_fg(struct dumpinfo *info)
{
	int ret;
	u8 err_fg[EA8076_ERR_FG_LEN] = { 0, };
	struct resinfo *res = info->res;

	if (!res || ARRAY_SIZE(err_fg) != res->dlen) {
		pr_err("%s invalid resource\n", __func__);
		return;
	}

	ret = resource_copy(err_fg, res);
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to copy err_fg resource\n", __func__);
		return;
	}

	panel_info("========== SHOW PANEL [EEh:ERR_FG] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			err_fg[0], (err_fg[0] & 0x4C) ? "NG" : "GOOD");

	if (err_fg[0] & 0x04) {
		panel_info("* VLOUT3 Error\n");
#ifdef CONFIG_DISPLAY_USE_INFO
		inc_dpui_u32_field(DPUI_KEY_PNVLO3E, 1);
#endif
	}

	if (err_fg[0] & 0x08) {
		panel_info("* ELVDD Error\n");
#ifdef CONFIG_DISPLAY_USE_INFO
		inc_dpui_u32_field(DPUI_KEY_PNELVDE, 1);
#endif
	}

	if (err_fg[0] & 0x40) {
		panel_info("* VLIN1 Error\n");
#ifdef CONFIG_DISPLAY_USE_INFO
		inc_dpui_u32_field(DPUI_KEY_PNVLI1E, 1);
#endif
	}

	panel_info("==================================================\n");
}

static void show_dsi_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 dsi_err[EA8076_DSI_ERR_LEN] = { 0, };

	if (!res || ARRAY_SIZE(dsi_err) != res->dlen) {
		pr_err("%s invalid resource\n", __func__);
		return;
	}

	ret = resource_copy(dsi_err, res);
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to copy dsi_err resource\n", __func__);
		return;
	}

	panel_info("========== SHOW PANEL [05h:DSIE_CNT] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			dsi_err[0], (dsi_err[0]) ? "NG" : "GOOD");
	if (dsi_err[0])
		panel_info("* DSI Error Count : %d\n", dsi_err[0]);
	panel_info("====================================================\n");

#ifdef CONFIG_DISPLAY_USE_INFO
	inc_dpui_u32_field(DPUI_KEY_PNDSIE, dsi_err[0]);
#endif
}
