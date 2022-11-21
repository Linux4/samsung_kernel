/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fac/s6e3fac.c
 *
 * S6E3FAC Dimming Driver
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
#include <linux/bits.h>
#include "../panel.h"
#include "s6e3fac.h"
#include "s6e3fac_panel.h"
#include "s6e3fac_dimming.h"
#ifdef CONFIG_PANEL_AID_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "../dpui.h"
#include "../util.h"

#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#ifdef PANEL_PR_TAG
#undef PANEL_PR_TAG
#define PANEL_PR_TAG	"ddi"
#endif

unsigned int s6e3fac_gamma_bits[S6E3FAC_NR_TP] = {
	12, 12, 11, 11, 11, 11, 11, 11,
	11, 11, 11, 11, 12,
};

unsigned int s6e3fac_glut_bits[S6E3FAC_NR_GLUT_POINT] = {
	10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10,
};

static const char *s6e3fac_get_vrr_name(int index)
{
	const char *s6e3fac_vrr_name[MAX_S6E3FAC_VRR] = {
		[S6E3FAC_VRR_60NS] = "60NS",
		[S6E3FAC_VRR_48NS] = "48NS",
		[S6E3FAC_VRR_120HS] = "120HS",
		[S6E3FAC_VRR_60HS_120HS_TE_HW_SKIP_1] = "60HS_120HS_TE_HW_SKIP_1",
		[S6E3FAC_VRR_30HS_120HS_TE_HW_SKIP_3] = "30HS_120HS_TE_HW_SKIP_3",
		[S6E3FAC_VRR_24HS_120HS_TE_HW_SKIP_4] = "24HS_120HS_TE_HW_SKIP_4",
		[S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11] = "10HS_120HS_TE_HW_SKIP_11",
		[S6E3FAC_VRR_96HS] = "96HS",
		[S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1] = "48HS_96HS_TE_HW_SKIP_1",
		[S6E3FAC_VRR_60HS] = "60HS",
		[S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1] = "30HS_60HS_TE_HW_SKIP_1",
		[S6E3FAC_VRR_48HS] = "48HS",
		[S6E3FAC_VRR_24HS_48HS_TE_HW_SKIP_1] = "24HS_48HS_TE_HW_SKIP_1",
		[S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4] = "10HS_48HS_TE_HW_SKIP_4",
	};

	if (index >= MAX_S6E3FAC_VRR)
		return NULL;

	return s6e3fac_vrr_name[index];
}

#ifdef CONFIG_PANEL_AID_DIMMING
__visible_for_testing int generate_brt_step_table(struct brightness_table *brt_tbl)
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
				brt_tbl->brt_to_step[i] = interpolation(brt_tbl->brt[k - 1] * disp_pow(10, 2),
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

__visible_for_testing int find_s6e3fac_vrr(struct panel_vrr *vrr)
{
	size_t i;

	if (!vrr) {
		panel_err("panel_vrr is null\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(S6E3FAC_VRR_FPS); i++) {
		if (vrr->fps == S6E3FAC_VRR_FPS[i][S6E3FAC_VRR_KEY_REFRESH_RATE] &&
				vrr->mode == S6E3FAC_VRR_FPS[i][S6E3FAC_VRR_KEY_REFRESH_MODE] &&
				vrr->te_sw_skip_count == S6E3FAC_VRR_FPS[i][S6E3FAC_VRR_KEY_TE_SW_SKIP_COUNT] &&
				vrr->te_hw_skip_count == S6E3FAC_VRR_FPS[i][S6E3FAC_VRR_KEY_TE_HW_SKIP_COUNT])
			return (int)i;
	}

	return -EINVAL;
}

__visible_for_testing int getidx_s6e3fac_vrr_mode(int mode)
{
	return mode == VRR_HS_MODE ?
		S6E3FAC_VRR_MODE_HS : S6E3FAC_VRR_MODE_NS;
}

int init_common_table(struct maptbl *tbl)
{
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	return 0;
}

__visible_for_testing int getidx_common_maptbl(struct maptbl *tbl)
{
	return 0;
}

#ifdef CONFIG_MCD_PANEL_LPM
#ifdef CONFIG_SUPPORT_AOD_BL
__visible_for_testing int init_aod_dimming_table(struct maptbl *tbl)
{
	int id = PANEL_BL_SUBDEV_TYPE_AOD;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
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

__visible_for_testing int getidx_dimming_frame_table(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct panel_properties *props;
	int layer = S6E3FAC_SMOOTH_DIM_USE, row = 0;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	panel = (struct panel_device *)tbl->pdata;
	props = &panel->panel_data.props;

	/* smooth dim not use when vrr has changed */
	if (props->vrr_idx != props->vrr_origin_idx) {
		panel_dbg("smooth dim off: vrr\n");
		layer = S6E3FAC_SMOOTH_DIM_NOT_USE;
	} else if (panel->state.disp_on == PANEL_DISPLAY_OFF) {
		panel_dbg("smooth dim off: disp_off\n");
		layer = S6E3FAC_SMOOTH_DIM_NOT_USE;
	} else {
		panel_dbg("smooth dim on\n");
		layer = S6E3FAC_SMOOTH_DIM_USE;
	}

	row = props->vrr_idx;

	return maptbl_index(tbl, layer, row, 0);
}

__visible_for_testing int getidx_dia_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_data = &panel->panel_data;

	return maptbl_index(tbl, 0, panel_data->props.dia_mode, 0);
}

__visible_for_testing int getidx_acl_opr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;
	u32 layer, row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;

	layer = is_hbm_brightness(panel_bl, panel_bl->props.brightness) ? PANEL_HBM_ON : PANEL_HBM_OFF;
	row = panel_bl_get_acl_opr(panel_bl);
	if (row >= maptbl_get_countof_row(tbl)) {
		panel_err("row(%d) is out of maptbl range(%d)\n",
				row, maptbl_get_countof_row(tbl));
		row = 0;
	}
	return maptbl_index(tbl, layer, row, 0);
}

__visible_for_testing int getidx_vrr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;
	int row = 0, layer = 0, index;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL) {
		panel_err("failed to get vrr\n");
		return -EINVAL;
	}

	index = find_s6e3fac_vrr(vrr);
	if (index < 0) {
		panel_warn("vrr not found\n");
		row = 0;
	} else {
		row = index;
	}

	return maptbl_index(tbl, layer, row, 0);
}

__visible_for_testing int getidx_vrr_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0;
	int vrr_mode;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	row = getidx_s6e3fac_vrr_mode(vrr_mode);
	if (row < 0) {
		panel_err("failed to getidx_s6e3fac_vrr_mode(mode:%d)\n", vrr_mode);
		row = 0;
	}
	panel_dbg("vrr_mode:%d(%s)\n", row,
			row == S6E3FAC_VRR_MODE_HS ? "HS" : "NM");

	return maptbl_index(tbl, layer, row, 0);
}

__visible_for_testing int init_lpm_brt_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return init_common_table(tbl);
#endif
}

__visible_for_testing int getidx_lpm_brt_table(struct maptbl *tbl)
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

__visible_for_testing int getidx_irc_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	return maptbl_index(tbl, 0, !!panel->panel_data.props.irc_mode, 0);
}

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
__visible_for_testing int s6e3fac_getidx_vddm_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("vddm %d\n", props->gct_vddm);

	return maptbl_index(tbl, 0, props->gct_vddm, 0);
}

__visible_for_testing int s6e3fac_getidx_gram_img_pattern_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("gram img %d\n", props->gct_pattern);
	props->gct_valid_chksum[0] = S6E3FAC_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[1] = S6E3FAC_GRAM_CHECKSUM_VALID_2;
	props->gct_valid_chksum[2] = S6E3FAC_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[3] = S6E3FAC_GRAM_CHECKSUM_VALID_2;

	return maptbl_index(tbl, 0, props->gct_pattern, 0);
}
#endif

#if defined(CONFIG_SUPPORT_FAST_DISCHARGE)
__visible_for_testing int getidx_fast_discharge_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data;
	int row = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;

	row = ((panel_data->props.enable_fd) ? 1 : 0);
	panel_info("fast_discharge %d\n", row);

	return maptbl_index(tbl, 0, row, 0);
}

#endif

#ifdef CONFIG_SUPPORT_XTALK_MODE
__visible_for_testing int getidx_vgh_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data;
	int row = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;

	row = ((panel_data->props.xtalk_mode) ? 1 : 0);
	panel_info("xtalk_mode %d\n", row);

	return maptbl_index(tbl, 0, row, 0);
}
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
__visible_for_testing void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst)
{
	return;
}
#endif

__visible_for_testing void copy_common_maptbl(struct maptbl *tbl, u8 *dst)
{
	int idx;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %p, dst %p)\n",
				tbl, dst);
		return;
	}

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		panel_err("failed to getidx %d\n", idx);
		return;
	}

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * maptbl_get_sizeof_copy(tbl));
	panel_dbg("copy from %s %d %d\n",
			tbl->name, idx, maptbl_get_sizeof_copy(tbl));
	print_data(dst, maptbl_get_sizeof_copy(tbl));
}

__visible_for_testing void copy_tset_maptbl(struct maptbl *tbl, u8 *dst)
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

__visible_for_testing void copy_elvss_cal_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	int ret;
	u8 elvss_cal_offset_otp_value = 0;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	ret = resource_copy_by_name(panel_data,
			&elvss_cal_offset_otp_value, "elvss_cal_offset");
	if (unlikely(ret)) {
		panel_err("elvss_cal_offset not found in panel resource\n");
		return;
	}

	*dst = elvss_cal_offset_otp_value;

}

#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
__visible_for_testing void copy_copr_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct copr_info *copr;

	if (!tbl || !dst)
		return;

	copr = (struct copr_info *)tbl->pdata;
	if (unlikely(!copr))
		return;

	if (get_copr_reg_packed_size(copr->props.version) != S6E3FAC_COPR_CTRL_LEN) {
		panel_err("copr ctrl register size mismatch(%d, %d)\n",
				get_copr_reg_packed_size(copr->props.version), S6E3FAC_COPR_CTRL_LEN);
		return;
	}

	copr_reg_to_byte_array(&copr->props.reg, copr->props.version, dst);
	print_data(dst, get_copr_reg_packed_size(copr->props.version));
}
#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
__visible_for_testing int init_color_blind_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	if (tbl->arr == NULL) {
		panel_err("arr is null\n");
		return -EINVAL;
	}

	if (S6E3FAC_SCR_CR_OFS + mdnie->props.sz_scr > maptbl_get_sizeof_maptbl(tbl)) {
		panel_err("invalid size (maptbl_size %d, sz_scr %d)\n",
				maptbl_get_sizeof_maptbl(tbl), mdnie->props.sz_scr);
		return -EINVAL;
	}

	/* TODO: boundary check in mdnie driver */
	if (mdnie->props.sz_scr > ARRAY_SIZE(mdnie->props.scr)) {
		panel_err("invalid sz_scr %d\n", mdnie->props.sz_scr);
		return -EINVAL;
	}

	memcpy(&tbl->arr[S6E3FAC_SCR_CR_OFS],
			mdnie->props.scr, mdnie->props.sz_scr);

	return 0;
}

__visible_for_testing int getidx_mdnie_scenario_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	return maptbl_get_indexof_row(tbl, mdnie->props.mode);
}

#ifdef CONFIG_SUPPORT_HMD
__visible_for_testing int getidx_mdnie_hmd_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	return maptbl_get_indexof_row(tbl, mdnie->props.hmd);
}
#endif

__visible_for_testing int getidx_mdnie_hdr_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	return maptbl_get_indexof_row(tbl, mdnie->props.hdr);
}

__visible_for_testing int getidx_mdnie_trans_mode_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	if (mdnie->props.trans_mode == TRANS_OFF)
		panel_dbg("mdnie trans_mode off\n");

	return maptbl_get_indexof_row(tbl, mdnie->props.trans_mode);
}

__visible_for_testing int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	int mode = NIGHT_MODE_OFF;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	if (mdnie->props.mode != AUTO)
		mode = NIGHT_MODE_ON;

	return maptbl_index(tbl, mode, mdnie->props.night_level, 0);
}

__visible_for_testing int init_mdnie_night_mode_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl *night_maptbl;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	night_maptbl = mdnie_find_etc_maptbl(mdnie, MDNIE_ETC_NIGHT_MAPTBL);
	if (!night_maptbl) {
		panel_err("NIGHT_MAPTBL not found\n");
		return -EINVAL;
	}

	if (maptbl_get_sizeof_maptbl(tbl) < (S6E3FAC_NIGHT_MODE_OFS +
			maptbl_get_sizeof_row(night_maptbl))) {
		panel_err("invalid size (maptbl_size %d, night_maptbl_size %d)\n",
				maptbl_get_sizeof_maptbl(tbl), maptbl_get_sizeof_row(night_maptbl));
		return -EINVAL;
	}

	maptbl_copy(night_maptbl, &tbl->arr[S6E3FAC_NIGHT_MODE_OFS]);

	return 0;
}

__visible_for_testing int init_mdnie_color_lens_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl *color_lens_maptbl;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	color_lens_maptbl = mdnie_find_etc_maptbl(mdnie, MDNIE_ETC_COLOR_LENS_MAPTBL);
	if (!color_lens_maptbl) {
		panel_err("COLOR_LENS_MAPTBL not found\n");
		return -EINVAL;
	}

	if (maptbl_get_sizeof_maptbl(tbl) < (S6E3FAC_COLOR_LENS_OFS +
			maptbl_get_sizeof_row(color_lens_maptbl))) {
		panel_err("invalid size (maptbl_size %d, color_lens_maptbl_size %d)\n",
				maptbl_get_sizeof_maptbl(tbl), maptbl_get_sizeof_row(color_lens_maptbl));
		return -EINVAL;
	}

	if (IS_COLOR_LENS_MODE(mdnie))
		maptbl_copy(color_lens_maptbl, &tbl->arr[S6E3FAC_COLOR_LENS_OFS]);

	return 0;
}

__visible_for_testing void update_current_scr_white(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata) {
		panel_err("invalid param\n");
		return;
	}

	if (!dst) {
		panel_err("dst is null\n");
		return;
	}

	mdnie = (struct mdnie_info *)tbl->pdata;
	mdnie->props.cur_wrgb[0] = *dst;
	mdnie->props.cur_wrgb[1] = *(dst + 2);
	mdnie->props.cur_wrgb[2] = *(dst + 4);
}

__visible_for_testing int init_color_coordinate_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl_pos pos;
	int type, color, index;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	if (maptbl_get_countof_row(tbl) != ARRAY_SIZE(mdnie->props.coord_wrgb)) {
		panel_err("invalid maptbl row size %d\n", maptbl_get_countof_row(tbl));
		return -EINVAL;
	}

	if (maptbl_get_countof_col(tbl) != ARRAY_SIZE(mdnie->props.coord_wrgb[0])) {
		panel_err("invalid maptbl col size %d\n", maptbl_get_countof_col(tbl));
		return -EINVAL;
	}

	maptbl_for_each(tbl, index) {
		if (maptbl_index_to_pos(tbl, index, &pos) < 0)
			return -EINVAL;

		type = pos.index[NDARR_2D];
		color = pos.index[NDARR_1D];
		tbl->arr[index] =
			mdnie->props.coord_wrgb[type][color];
	}

	return 0;
}

__visible_for_testing int init_sensor_rgb_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl_pos pos;
	int index, indexof_column;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	if (maptbl_get_countof_row(tbl) != 1) {
		panel_err("invalid maptbl row size %d\n", maptbl_get_countof_row(tbl));
		return -EINVAL;
	}

	if (maptbl_get_countof_col(tbl) != ARRAY_SIZE(mdnie->props.ssr_wrgb)) {
		panel_err("invalid maptbl col size %d\n", maptbl_get_countof_col(tbl));
		return -EINVAL;
	}

	maptbl_for_each(tbl, index) {
		if (maptbl_index_to_pos(tbl, index, &pos) < 0)
			return -EINVAL;

		indexof_column = pos.index[NDARR_1D];
		tbl->arr[index] =
			mdnie->props.ssr_wrgb[indexof_column];
	}

	return 0;
}

__visible_for_testing int getidx_color_coordinate_maptbl(struct maptbl *tbl)
{

	struct mdnie_info *mdnie;
	static int wcrd_type[MODE_MAX] = {
		WCRD_TYPE_D65, WCRD_TYPE_D65, WCRD_TYPE_D65,
		WCRD_TYPE_ADAPTIVE, WCRD_TYPE_ADAPTIVE,
	};

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	if ((mdnie->props.mode < 0) || (mdnie->props.mode >= MODE_MAX)) {
		panel_err("out of mode range %d\n", mdnie->props.mode);
		return -EINVAL;
	}
	return maptbl_index(tbl, 0, wcrd_type[mdnie->props.mode], 0);
}

__visible_for_testing int getidx_adjust_ldu_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	static int wcrd_type[MODE_MAX] = {
		WCRD_TYPE_D65, WCRD_TYPE_D65, WCRD_TYPE_D65,
		WCRD_TYPE_ADAPTIVE, WCRD_TYPE_ADAPTIVE,
	};

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

	if ((mdnie->props.mode < 0) || (mdnie->props.mode >= MODE_MAX)) {
		panel_err("out of mode range %d\n", mdnie->props.mode);
		return -EINVAL;
	}

	if ((mdnie->props.ldu < LDU_MODE_OFF) || (mdnie->props.ldu >= MAX_LDU_MODE)) {
		panel_err("out of ldu mode range %d\n", mdnie->props.ldu);
		return -EINVAL;
	}

	return maptbl_index(tbl, wcrd_type[mdnie->props.mode], mdnie->props.ldu, 0);
}

__visible_for_testing int getidx_color_lens_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata)
		return -EINVAL;

	mdnie = tbl->pdata;

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

__visible_for_testing void copy_scr_white_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;
	int idx;

	if (!tbl || !tbl->pdata || !dst)
		return;

	mdnie = tbl->pdata;
	if (maptbl_get_countof_col(tbl) != MAX_COLOR) {
		panel_err("invalid maptbl size %d\n", maptbl_get_countof_col(tbl));
		return;
	}

	idx = maptbl_getidx(tbl);
	if (idx < 0 || (idx + MAX_COLOR > maptbl_get_sizeof_maptbl(tbl))) {
		panel_err("invalid index %d\n", idx);
		return;
	}

	mdnie_update_wrgb(mdnie, tbl->arr[idx + RED],
			tbl->arr[idx + GREEN], tbl->arr[idx + BLUE]);
	mdnie_cur_wrgb_to_byte_array(mdnie, dst, 2);
}

__visible_for_testing int getidx_trans_maptbl(struct pkt_update_info *pktui)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;

	if (!pktui || !pktui->pdata)
		return -EINVAL;

	panel = pktui->pdata;
	mdnie = &panel->mdnie;

	if (mdnie->props.trans_mode >= MAX_TRANS_MODE)
		return -EINVAL;

	return (mdnie->props.trans_mode == TRANS_ON) ?
		MDNIE_ETC_NONE_MAPTBL : MDNIE_ETC_TRANS_MAPTBL;
}

__visible_for_testing int getidx_mdnie_maptbl(struct pkt_update_info *pktui, int offset)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;
	int row, index;

	if (!pktui || !pktui->pdata)
		return -EINVAL;

	panel = pktui->pdata;
	mdnie = &panel->mdnie;

	if (offset < 0 || offset >= mdnie->nr_reg) {
		panel_err("invalid offset %d\n", offset);
		return -EINVAL;
	}

	row = mdnie_get_maptbl_index(mdnie);
	if (row < 0) {
		panel_err("invalid row %d\n", row);
		return -EINVAL;
	}

	index = row * mdnie->nr_reg + offset;
	if (index >= mdnie->nr_maptbl) {
		panel_err("exceeded index %d row %d offset %d\n",
				index, row, offset);
		return -EINVAL;
	}
	return index;
}

__visible_for_testing int getidx_mdnie_0_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 0);
}

__visible_for_testing int getidx_mdnie_1_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 1);
}

__visible_for_testing int getidx_mdnie_2_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 2);
}

__visible_for_testing int getidx_mdnie_scr_white_maptbl(struct pkt_update_info *pktui)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;
	int index;

	if (!pktui || !pktui->pdata)
		return -EINVAL;

	panel = pktui->pdata;
	mdnie = &panel->mdnie;

	if (mdnie->props.scr_white_mode < 0 ||
			mdnie->props.scr_white_mode >= MAX_SCR_WHITE_MODE) {
		panel_warn("out of range %d\n",
				mdnie->props.scr_white_mode);
		return -EINVAL;
	}

	if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_COLOR_COORDINATE) {
		panel_dbg("coordinate maptbl\n");
		index = MDNIE_COLOR_COORDINATE_MAPTBL;
	} else if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_ADJUST_LDU) {
		panel_dbg("adjust ldu maptbl\n");
		index = MDNIE_ADJUST_LDU_MAPTBL;
	} else if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_SENSOR_RGB) {
		panel_dbg("sensor rgb maptbl\n");
		index = MDNIE_SENSOR_RGB_MAPTBL;
	} else {
		panel_dbg("empty maptbl\n");
		index = MDNIE_SCR_WHITE_NONE_MAPTBL;
	}

	return index;
}
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */

__visible_for_testing int show_rddpm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddpm[S6E3FAC_RDDPM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddpm;
#endif

	if (!res || ARRAY_SIZE(rddpm) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(rddpm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [0Ah:RDDPM] INFO (After DISPLAY_ON) ==========\n");
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

	return 0;
}

__visible_for_testing int show_rddpm_before_sleep_in(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddpm[S6E3FAC_RDDPM_LEN] = { 0, };

	if (!res || ARRAY_SIZE(rddpm) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(rddpm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [0Ah:RDDPM] INFO (Before SLEEP_IN) ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddpm[0], ((rddpm[0] & 0x9C) == 0x98) ? "GOOD" : "NG");
	panel_info("* Bootster Mode : %s\n", rddpm[0] & 0x80 ? "ON (GD)" : "OFF (NG)");
	panel_info("* Idle Mode     : %s\n", rddpm[0] & 0x40 ? "ON (NG)" : "OFF (GD)");
	panel_info("* Partial Mode  : %s\n", rddpm[0] & 0x20 ? "ON" : "OFF");
	panel_info("* Sleep Mode    : %s\n", rddpm[0] & 0x10 ? "OUT (GD)" : "IN (NG)");
	panel_info("* Normal Mode   : %s\n", rddpm[0] & 0x08 ? "OK (GD)" : "SLEEP (NG)");
	panel_info("* Display ON    : %s\n", rddpm[0] & 0x04 ? "ON (NG)" : "OFF (GD)");
	panel_info("=================================================\n");

	return 0;
}

__visible_for_testing int show_rddsm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddsm[S6E3FAC_RDDSM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddsm;
#endif

	if (!res || ARRAY_SIZE(rddsm) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(rddsm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddsm resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [0Eh:RDDSM] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddsm[0], (rddsm[0] == 0x80) ? "GOOD" : "NG");
	panel_info("* TE Mode : %s\n", rddsm[0] & 0x80 ? "ON(GD)" : "OFF(NG)");
	panel_info("=================================================\n");
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	g_rddsm = (unsigned int)rddsm[0];
#endif

	return 0;
}

__visible_for_testing int show_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 err[S6E3FAC_ERR_LEN] = { 0, }, err_15_8, err_7_0;

	if (!res || ARRAY_SIZE(err) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(err, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy err resource\n");
		return -EINVAL;
	}

	err_15_8 = err[0];
	err_7_0 = err[1];

	panel_info("========== SHOW PANEL [E9h:DSIERR] INFO ==========\n");
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

	if (err[2] != 0)
		panel_info("* CRC Error Count : %d\n", err[2]);

	if (err[3] != 0)
		panel_info("* ECC1 Error Count : %d\n", err[3]);

	if (err[4] != 0)
		panel_info("* ECC2 Error Count : %d\n", err[4]);

	panel_info("==================================================\n");

	return 0;
}

__visible_for_testing int show_err_fg(struct dumpinfo *info)
{
	int ret;
	u8 err_fg[S6E3FAC_ERR_FG_LEN] = { 0, };
	struct resinfo *res = info->res;

	if (!res || ARRAY_SIZE(err_fg) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(err_fg, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy err_fg resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [EEh:ERR_FG] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			err_fg[0], (err_fg[0] & 0x4C) ? "NG" : "GOOD");

	if (err_fg[0] & 0x04) {
		panel_info("* VLOUT3 Error\n");
//Todo
#ifdef CONFIG_DISPLAY_USE_INFO
		inc_dpui_u32_field(DPUI_KEY_PNVLO3E, 1);
#endif
	}

	if (err_fg[0] & 0x08) {
		panel_info("* ELVDD Error\n");
//Todo
#ifdef CONFIG_DISPLAY_USE_INFO
		inc_dpui_u32_field(DPUI_KEY_PNELVDE, 1);
#endif
	}

	if (err_fg[0] & 0x40) {
		panel_info("* VLIN1 Error\n");
//Todo
#ifdef CONFIG_DISPLAY_USE_INFO
		inc_dpui_u32_field(DPUI_KEY_PNVLI1E, 1);
#endif
	}

	panel_info("==================================================\n");

	return 0;
}

__visible_for_testing int show_dsi_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 dsi_err[S6E3FAC_DSI_ERR_LEN] = { 0, };

	if (!res || ARRAY_SIZE(dsi_err) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(dsi_err, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy dsi_err resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [05h:DSIE_CNT] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			dsi_err[0], (dsi_err[0]) ? "NG" : "GOOD");
	if (dsi_err[0])
		panel_info("* DSI Error Count : %d\n", dsi_err[0]);
	panel_info("====================================================\n");
//Todo
#ifdef CONFIG_DISPLAY_USE_INFO
	inc_dpui_u32_field(DPUI_KEY_PNDSIE, dsi_err[0]);
#endif
#if IS_ENABLED(CONFIG_SEC_ABC)
	if (dsi_err[0] > 0)
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		sec_abc_send_event("MODULE=display@INFO=act_section_panel_main_dsi_error");
#else
		sec_abc_send_event("MODULE=display@WARN=act_section_panel_main_dsi_error");
#endif
#endif

	return 0;
}

__visible_for_testing int show_self_diag(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 self_diag[S6E3FAC_SELF_DIAG_LEN] = { 0, };

	ret = resource_copy(self_diag, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy self_diag resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [0Fh:SELF_DIAG] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			self_diag[0], (self_diag[0] & 0x40) ? "GOOD" : "NG");
	if ((self_diag[0] & 0x80) == 0)
		panel_info("* OTP value is changed\n");
	if ((self_diag[0] & 0x40) == 0)
		panel_info("* Panel Boosting Error\n");

	panel_info("=====================================================\n");
//Todo
#ifdef CONFIG_DISPLAY_USE_INFO
	inc_dpui_u32_field(DPUI_KEY_PNSDRE, (self_diag[0] & 0x40) ? 0 : 1);
#endif

	return 0;
}

__visible_for_testing int show_ecc_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res;
	u8 read[S6E3FAC_ECC_TEST_LEN] = { 0, };

	if (!info)
		return -EINVAL;

	res = info->res;
	if (!res || ARRAY_SIZE(read) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(read, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy ecc_err resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [F8h[0x02]:ECC] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x%02x%02x, Result : %s\n",
		read[0], read[1], read[2],
		((read[0] & S6E3FAC_ECC_ENABLE_MASK) == S6E3FAC_ECC_ENABLE_VALUE) &&
		(read[1] == 0x00) && (read[2] == 0x00) ? "GOOD" : "NG");
	panel_info("* ECC state: %s\n",
		((read[0] & S6E3FAC_ECC_ENABLE_MASK) == S6E3FAC_ECC_ENABLE_VALUE) ? "Enabled" : "Disabled");

	if (read[1] != 0x00)
		panel_info("* Total Error Count: %d\n", read[1]);
	if (read[2] != 0x00)
		panel_info("* Unrecoverable Error Count: %d\n", read[2]);

	panel_info("=====================================================\n");

	return 0;
}

__visible_for_testing int show_ssr_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res;
	u8 read[S6E3FAC_SSR_TEST_LEN] = { 0, };

	if (!info)
		return -EINVAL;

	res = info->res;
	if (!res || ARRAY_SIZE(read) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(read, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy ssr_state resource\n");
		return -EINVAL;
	}


	panel_info("========== SHOW PANEL [FBh[0x09,0x23]:SSR] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x%02x, Result : %s\n",
			read[0], read[1],
			(((read[0] & S6E3FAC_SSR_ENABLE_MASK) == S6E3FAC_SSR_ENABLE_VALUE) &&
			((read[1] & S6E3FAC_SSR_STATE_MASK) == S6E3FAC_SSR_STATE_VALUE) ?
			"GOOD" : "NG"));

	panel_info("* SSR state: %s\n", (read[0] == 0x22 ? "Enabled" : "Disabled"));

	if (read[1] & 0x01)
		panel_info("* Left source recovered\n");
	if (read[1] & 0x04)
		panel_info("* Right source recovered\n");

	panel_info("=====================================================\n");

	return 0;
}

__visible_for_testing int show_flash_loaded(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res;
	u8 read[S6E3FAC_FLASH_LOADED_LEN] = { 0, };

	if (!info)
		return -EINVAL;

	res = info->res;

	if (!res || ARRAY_SIZE(read) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(read, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy flash_loaded state resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [91h[0x40]:FLASH_LOADED] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			read[0], (read[0] == S6E3FAC_FLASH_LOADED_VALUE) ? "GOOD" : "NG");

	panel_info("=====================================================\n");

	return 0;
}

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
__visible_for_testing int show_cmdlog(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 cmdlog[S6E3FAC_CMDLOG_LEN];

	memset(cmdlog, 0, sizeof(cmdlog));
	ret = resource_copy(cmdlog, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy cmdlog resource\n");
		return -EINVAL;
	}

	panel_info("dump:cmdlog\n");
	print_data(cmdlog, ARRAY_SIZE(cmdlog));

	return 0;
}
#endif
#ifdef CONFIG_SUPPORT_MAFPC
__visible_for_testing void copy_mafpc_enable_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct mafpc_device *mafpc;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;

	mafpc = get_mafpc_device(panel);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return;
	}

	panel_info("MCD:ABC:enabled: %x, written: %x\n", mafpc->enable, mafpc->written);

	if (!mafpc->enable) {
		dst[0] = 0;
		goto err_enable;
	}

	if ((mafpc->written & MAFPC_UPDATED_FROM_SVC) &&
		(mafpc->written & MAFPC_UPDATED_TO_DEV)) {

		dst[0] = S6E3FAC_MAFPC_ENABLE;

		if (mafpc->written & MAFPC_UPDATED_FROM_SVC)
			memcpy(&dst[S6E3FAC_MAFPC_CTRL_CMD_OFFSET], mafpc->ctrl_cmd, mafpc->ctrl_cmd_len);
	}

err_enable:
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			dst, S6E3FAC_MAFPC_CTRL_CMD_OFFSET + mafpc->ctrl_cmd_len, false);
	return;
}

__visible_for_testing void copy_mafpc_ctrl_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct mafpc_device *mafpc;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;

	mafpc = get_mafpc_device(panel);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return;
	}

	if (mafpc->enable) {
		if (mafpc->written)
			memcpy(dst, mafpc->ctrl_cmd, mafpc->ctrl_cmd_len);

		print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4, dst, mafpc->ctrl_cmd_len, false);
	}

	return;
}


#define S6E3FAC_MAFPC_SCALE_MAX	75


__visible_for_testing int get_mafpc_scale_index(struct mafpc_device *mafpc, struct panel_device *panel)
{
	int ret = 0;
	int br_index, index = 0;
	struct panel_bl_device *panel_bl;

	panel_bl = &panel->panel_bl;
	if (!panel_bl) {
		panel_err("panel_bl is null\n");
		goto err_get_scale;
	}

	if (!mafpc->scale_buf || !mafpc->scale_map_br_tbl)  {
		panel_err("mafpc img buf is null\n");
		goto err_get_scale;
	}

	br_index = panel_bl->props.brightness;
	if (br_index >= mafpc->scale_map_br_tbl_len)
		br_index = mafpc->scale_map_br_tbl_len - 1;

	index = mafpc->scale_map_br_tbl[br_index];
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", br_index);
		goto err_get_scale;
	}
	return index;

err_get_scale:
	return ret;
}

__visible_for_testing void copy_mafpc_scale_maptbl(struct maptbl *tbl, u8 *dst)
{
	int row = 0;
	int index = 0;

	struct mafpc_device *mafpc;
	struct panel_device *panel;

	if (!tbl || !tbl->pdata)
		return;

	panel = tbl->pdata;

	mafpc = get_mafpc_device(panel);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc device\n");
		return;
	}

	if (!mafpc->scale_buf || !mafpc->scale_map_br_tbl)  {
		panel_err("mafpc img buf is null\n");
		if (!mafpc->scale_buf)
			panel_err("MCD:ABC: scale_buf is null\n");
		if (!mafpc->scale_map_br_tbl)
			panel_err("MCD:ABC: scale_map_br_tbl is null\n");
		return;
	}

	index = get_mafpc_scale_index(mafpc, panel);
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", index);
		return;
	}

	if (index >= S6E3FAC_MAFPC_SCALE_MAX)
		index = S6E3FAC_MAFPC_SCALE_MAX - 1;

	row = index * 3;
	memcpy(dst, mafpc->scale_buf + row, 3);

	panel_info("idx: %d, %x:%x:%x\n",
			index, dst[0], dst[1], dst[2]);

	return;
}


__visible_for_testing int show_mafpc_log(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 mafpc[S6E3FAC_MAFPC_LEN] = {0, };

	if (!res || ARRAY_SIZE(mafpc) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(mafpc, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return -EINVAL;
	}

	panel_info("========== SHOW PANEL [87h:MAFPC_EN] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			mafpc[0], (mafpc[0] & 0x01) ? "ON" : "OFF");
	panel_info("====================================================\n");

	return 0;
}
__visible_for_testing int show_mafpc_flash_log(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 mafpc_flash[S6E3FAC_MAFPC_FLASH_LEN] = {0, };

	if (!res || ARRAY_SIZE(mafpc_flash) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(mafpc_flash, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return -EINVAL;
	}

	panel_info("======= SHOW PANEL [FEh(0x09):MAFPC_FLASH] INFO =======\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			mafpc_flash[0], (mafpc_flash[0] & 0x02) ? "BYPASS" : "POC");
	panel_info("====================================================\n");

	return 0;
}

__visible_for_testing int show_abc_crc_log(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 abc_crc[S6E3FAC_MAFPC_CRC_LEN] = {0, };

	if (!res || ARRAY_SIZE(abc_crc) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(abc_crc, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return -EINVAL;
	}

	panel_info("======= SHOW PANEL [FEh(0x09):MAFPC_CRC] INFO =======\n");
	panel_info("* Reg Value : 0x%02x, 0x%02x Result : %s\n", abc_crc[0], abc_crc[1]);
	panel_info("====================================================\n");

	return 0;
}
#endif

__visible_for_testing int show_self_mask_crc(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 crc[S6E3FAC_SELF_MASK_CRC_LEN] = {0, };

	if (!res || ARRAY_SIZE(crc) != res->dlen) {
		panel_err("invalid resource\n");
		return -EINVAL;
	}

	ret = resource_copy(crc, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to self mask crc resource\n");
		return -EINVAL;
	}

	panel_info("======= SHOW PANEL [7Fh:SELF_MASK_CRC] INFO =======\n");
	panel_info("* Reg Value : 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
			crc[0], crc[1], crc[2], crc[3]);
	panel_info("====================================================\n");

	return 0;
}

__visible_for_testing int init_gamma_mode2_brt_table(struct maptbl *tbl)
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

__visible_for_testing int getidx_gamma_mode2_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;
	panel_data = &panel->panel_data;

	row = get_brightness_pac_step_by_subdev_id(panel_bl, PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}

__visible_for_testing int getidx_vrr_brt_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data;
	struct panel_vrr *vrr;
	int row = 0, layer = 0;
	struct panel_bl_device *panel_bl;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;
	panel_data = &panel->panel_data;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL) {
		panel_err("failed to get vrr\n");
		return -EINVAL;
	}

	layer = find_s6e3fac_vrr(vrr);
	if (layer < 0) {
		panel_warn("vrr not found\n");
		return -EINVAL;
	}

	row = get_brightness_pac_step_by_subdev_id(panel_bl,
			PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness);

	return maptbl_index(tbl, layer, row, 0);
}

#ifdef CONFIG_SUPPORT_HMD
__visible_for_testing int init_gamma_mode2_hmd_brt_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;

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

	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_HMD];

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
	memcpy(&panel->panel_bl.subdev[PANEL_BL_SUBDEV_TYPE_HMD].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	return 0;
}

__visible_for_testing int getidx_gamma_mode2_hmd_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;
	panel_data = &panel->panel_data;

	row = get_brightness_pac_step(panel_bl, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}
#endif

__visible_for_testing int gamma_ctoi(s32 (*dst)[MAX_COLOR], u8 *src)
{
	unsigned int i, mask, upper_mask, lower_mask;

	if (!src || !dst)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3fac_gamma_bits); i++) {
		mask = GENMASK(s6e3fac_gamma_bits[i] - 1, 0);
		upper_mask = (mask >> 8) & 0xFF;
		lower_mask = mask & 0xFF;

		dst[i][RED] = (((src[i * 5 + 0] & upper_mask) << 8) | (src[i * 5 + 2] & lower_mask));
		dst[i][GREEN] = ((((src[i * 5 + 1] >> 4) & upper_mask) << 8) | (src[i * 5 + 3] & lower_mask));
		dst[i][BLUE] = (((src[i * 5 + 1] & upper_mask) << 8) | (src[i * 5 + 4] & lower_mask));
	}

	return 0;
}

__visible_for_testing int gamma_itoc(u8 *dst, s32(*src)[MAX_COLOR])
{
	unsigned int i, mask;

	if (!src || !dst)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3fac_gamma_bits); i++) {
		mask = GENMASK(s6e3fac_gamma_bits[i] - 1, 0);
		dst[i * 5 + 0] = (src[i][RED] >> 8) & (mask >> 8);
		dst[i * 5 + 1] = ((src[i][GREEN] >> 8) & (mask >> 8)) << 4 |
						 ((src[i][BLUE] >> 8) & (mask >> 8));
		dst[i * 5 + 2] = src[i][RED] & (mask & 0xFF);
		dst[i * 5 + 3] = src[i][GREEN] & (mask & 0xFF);
		dst[i * 5 + 4] = src[i][BLUE] & (mask & 0xFF);
	}

	return 0;
}

__visible_for_testing int gamma_int_array_sum(s32 (*dst)[MAX_COLOR], s32 (*src)[MAX_COLOR],
		s32 (*offset)[MAX_COLOR])
{
	unsigned int i, c;
	int max_value;

	if (!src || !dst || !offset)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3fac_gamma_bits); i++) {
		max_value = GENMASK(s6e3fac_gamma_bits[i] - 1, 0);
		for_each_color(c)
			dst[i][c] =
			min(max(src[i][c] + offset[i][c], 0), max_value);
	}

	return 0;
}

__visible_for_testing int gamma_byte_array_sum(u8 *dst, u8 *src,
		s32 (*offset)[MAX_COLOR])
{
	s32 gamma_org[S6E3FAC_NR_TP][MAX_COLOR];
	s32 gamma_new[S6E3FAC_NR_TP][MAX_COLOR];

	if (!dst || !src || !offset)
		return -EINVAL;

	gamma_ctoi(gamma_org, src);
	gamma_int_array_sum(gamma_new, gamma_org, offset);
	gamma_itoc(dst, gamma_new);

	return 0;
}

__visible_for_testing struct dimming_color_offset *get_dimming_data(struct panel_device *panel, int idx)
{
	struct panel_info *panel_data;
	struct panel_dimming_info *panel_dim_info;
	struct dimming_color_offset *dimming_data;

	if (!panel)
		return ERR_PTR(-EINVAL);

	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (!panel_dim_info)
		return ERR_PTR(-EINVAL);

	if (!panel_dim_info->dimming_data || panel_dim_info->nr_dimming_data < 1)
		return ERR_PTR(-ENODATA);

	if (idx < 0 || idx >= panel_dim_info->nr_dimming_data)
		return ERR_PTR(-ERANGE);

	dimming_data = panel_dim_info->dimming_data;
	return (dimming_data + idx);
}

/*
 * glut_ctoi - trasnform GLUT byte array to int array
 */
int s6e3fac_glut_ctoi(s32 (*dst)[S6E3FAC_NR_GLUT_POINT], u8 *src)
{
	unsigned int i, j = 0, c, mask, upper_mask, lower_mask;

	if (!src || !dst)
		return -EINVAL;

	for_each_color(c) {
		for (i = 0; i < ARRAY_SIZE(s6e3fac_glut_bits); i += 2, j += 3) {
			/* set GLUT_DEC[c][i + 0] */
			mask = GENMASK(s6e3fac_glut_bits[i + 0] - 1, 0);
			upper_mask = (mask >> 8) & 0xFF;
			lower_mask = mask & 0xFF;
			dst[c][i + 0] = hextos32((((src[j + 0] >> 4) & upper_mask) << 8) |
					(src[j + 1] & lower_mask), s6e3fac_glut_bits[i + 0]);

			/* set GLUT_DEC[c][i + 0] */
			mask = GENMASK(s6e3fac_glut_bits[i + 1] - 1, 0);
			upper_mask = (mask >> 8) & 0xFF;
			lower_mask = mask & 0xFF;
			dst[c][i + 1] = hextos32((((src[j + 0]) & upper_mask) << 8) |
					(src[j + 2] & lower_mask), s6e3fac_glut_bits[i + 1]);
		}
	}

	return 0;
}

/*
 * glut_itoc - trasnform GLUT int array to byte array
 */
int s6e3fac_glut_itoc(u8 *dst, s32(*src)[S6E3FAC_NR_GLUT_POINT])
{
	unsigned int i, j = 0, c, mask1, mask2, hex1, hex2;

	if (!src || !dst)
		return -EINVAL;

	for_each_color(c) {
		for (i = 0; i < ARRAY_SIZE(s6e3fac_glut_bits); i += 2, j += 3) {
			mask1 = GENMASK(s6e3fac_glut_bits[i + 0] - 1, 0);
			mask2 = GENMASK(s6e3fac_glut_bits[i + 1] - 1, 0);
			hex1 = s32tohex(src[c][i + 0], s6e3fac_glut_bits[i + 0]);
			hex2 = s32tohex(src[c][i + 1], s6e3fac_glut_bits[i + 1]);

			/* set GLUT_HEX */
			dst[j + 0] = ((hex1 >> 8) & (mask1 >> 8)) << 4 |
							 ((hex2 >> 8) & (mask2 >> 8));
			dst[j + 1] = hex1;
			dst[j + 2] = hex2;
		}
	}

	return 0;
}

__visible_for_testing int s6e3fac_do_gamma_flash_checksum(struct panel_device *panel, void *data, u32 len)
{
	int ret = 0;
	struct dim_flash_result *result;
	struct panel_info *panel_data;
	int state, index;
	int res_size;
	char read_buf[16] = { 0, };

	result = (struct dim_flash_result *) data;

	if (!panel)
		return -EINVAL;

	if (!result)
		return -ENODATA;

	if (atomic_cmpxchg(&result->running, 0, 1) != 0) {
		panel_info("already running\n");
		return -EBUSY;
	}

	panel_data = &panel->panel_data;

	result->state = state = GAMMA_FLASH_PROGRESS;
	result->exist = 0;

	memset(result->result, 0, ARRAY_SIZE(result->result));
	do {
		ret = panel_resource_update_by_name(panel, "flash_loaded");
		if (unlikely(ret < 0)) {
			panel_err("resource init failed\n");
			state = GAMMA_FLASH_ERROR_READ_FAIL;
			break;
		}

		res_size = get_resource_size_by_name(panel_data, "flash_loaded");
		if (unlikely(res_size < 0)) {
			panel_err("cannot found flash_loaded in panel resource\n");
			state = GAMMA_FLASH_ERROR_NOT_EXIST;
			break;
		}

		result->exist = 1;
		if (res_size > sizeof(read_buf)) {
			panel_err("flash_loaded resource size is too long %d\n", res_size);
			state = GAMMA_FLASH_ERROR_READ_FAIL;
			break;
		}

		ret = resource_copy_by_name(panel_data, read_buf, "flash_loaded");
		if (unlikely(ret < 0)) {
			panel_err("flash_loaded copy failed\n");
			state = GAMMA_FLASH_ERROR_READ_FAIL;
			break;
		}

		for (index = 0; index < res_size; index++) {
			if (read_buf[index] != 0x00) {
				state = GAMMA_FLASH_ERROR_CHECKSUM_MISMATCH;
				break;
			}
		}
	} while (0);

	if (state == GAMMA_FLASH_PROGRESS)
		state = GAMMA_FLASH_SUCCESS;

	snprintf(result->result, ARRAY_SIZE(result->result), "1\n%d %02X%02X%02X%02X",
		state, read_buf[0], read_buf[1], read_buf[2], read_buf[3]);

	result->state = state;

	atomic_xchg(&result->running, 0);

	return ret;
}

#ifdef CONFIG_SUPPORT_SSR_TEST
/*
 * s6e3fac_ssr_test - check state of ssr function register
 *
 * description of state values:
 * [0](F6h 9th): 0x22 (OK) 0x02 (NG)
 * [1](FBh 36th): 0xX0 (OK) other (NG)
 */
__visible_for_testing int s6e3fac_ssr_test(struct panel_device *panel, void *data, u32 len)
{
	struct panel_info *panel_data;
	int ret = 0;
	char read_buf[S6E3FAC_SSR_TEST_LEN] = { -1, -1 };

	if (!panel)
		return -EINVAL;

	panel_data = &panel->panel_data;

	ret = panel_do_seqtbl_by_index(panel, PANEL_SSR_TEST_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write ssr-test seq\n");
		return ret;
	}

	ret = resource_copy_by_name(panel_data, read_buf, "ssr_test");
	if (unlikely(ret < 0)) {
		panel_err("ssr_test copy failed\n");
		return -ENODATA;
	}

	ret = PANEL_SSR_TEST_FAIL;
	if (read_buf[0] == 0x22 && (read_buf[1] & 0x0F) == 0x00)
		ret = PANEL_SSR_TEST_PASS;

	panel_info("[0]: 0x%02x [1]: 0x%02x ret: %d\n", read_buf[0], read_buf[1], ret);
	return ret;
}
#endif

#ifdef CONFIG_SUPPORT_ECC_TEST
/*
 * s6e3fac_ecc_test - check state of dram ecc function register
 *
 * description of state values:
 * [0](F8h 3rd): 0x00 (OK) 0x01 (NG)
 * [1](F8h 4th): 0x00 (OK) other (NG)
 * [2](F8h 5th): 0x00 (OK) other (NG)
 */
__visible_for_testing int s6e3fac_ecc_test(struct panel_device *panel, void *data, u32 len)
{
	struct panel_info *panel_data;
	int ret = 0;
	char read_buf[S6E3FAC_ECC_TEST_LEN] = { -1, -1, -1 };

	if (!panel)
		return -EINVAL;

	panel_data = &panel->panel_data;

	ret = panel_do_seqtbl_by_index(panel, PANEL_ECC_TEST_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write ecc-test seq\n");
		return ret;
	}

	ret = resource_copy_by_name(panel_data, read_buf, "ecc_test");
	if (unlikely(ret < 0)) {
		panel_err("ecc_test copy failed\n");
		return -ENODATA;
	}

	ret = PANEL_ECC_TEST_FAIL;
	if ((read_buf[0] | read_buf[1] | read_buf[2]) == 0x00)
		ret = PANEL_ECC_TEST_PASS;

	panel_info("[0]: 0x%02x [1]: 0x%02x [2]: 0x%02x ret: %d\n", read_buf[0], read_buf[1], read_buf[2], ret);
	return ret;
}
#endif

#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
/*
 * s6e3fac_decoder_test - test ddi's decoder function
 *
 * description of state values:
 * [0](14h 1st): 0x4D (OK) other (NG)
 * [1](14h 2nd): 0x16 (OK) other (NG)
 */
__visible_for_testing int s6e3fac_decoder_test(struct panel_device *panel, void *data, u32 len)
{
	struct panel_info *panel_data;
	int ret = 0;
	u8 read_buf[S6E3FAC_DECODER_TEST_LEN] = { -1, -1 };

	if (!panel)
		return -EINVAL;

	panel_data = &panel->panel_data;

	ret = panel_do_seqtbl_by_index_nolock(panel, PANEL_DECODER_TEST_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write decoder-test seq\n");
		return ret;
	}

	ret = resource_copy_by_name(panel_data, read_buf, "decoder_test");
	if (unlikely(ret < 0)) {
		panel_err("decoder_test copy failed\n");
		return -ENODATA;
	}

	if ((read_buf[0] == 0x4D) && (read_buf[1] == 0x16)) {
		ret = PANEL_DECODER_TEST_PASS;
		panel_info("Pass [0]: 0x%02x [1]: 0x%02x ret: %d\n", read_buf[0], read_buf[1], ret);
	} else {
		ret = PANEL_DECODER_TEST_FAIL;
		panel_info("Fail [0]: 0x%02x [1]: 0x%02x ret: %d\n", read_buf[0], read_buf[1], ret);
	}

	snprintf((char *)data, len, "%02x %02x", read_buf[0], read_buf[1]);

	return ret;
}
#endif
__visible_for_testing bool is_panel_state_not_lpm(struct panel_device *panel)
{
	if (panel->state.cur_state != PANEL_STATE_ALPM)
		return true;

	return false;
}

__visible_for_testing bool is_vsync_for_mode_change(struct panel_device *panel)
{
	struct panel_properties *props;

	if (!panel)
		return -EINVAL;
	props = &panel->panel_data.props;

	if (props->vrr_idx == props->vrr_origin_idx)
		return false;

	if ((props->vrr_origin_idx == S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1 ||
		props->vrr_origin_idx == S6E3FAC_VRR_24HS_48HS_TE_HW_SKIP_1 ||
		props->vrr_origin_idx == S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4)) {
		panel_info("skip 1-frame(%s->%s)\n",
				s6e3fac_get_vrr_name(props->vrr_origin_idx),
				s6e3fac_get_vrr_name(props->vrr_idx));
		return true;
	}
	return false;
}

__visible_for_testing int get_s6e3fac_vrr_type(struct panel_device *panel)
{
	int refresh_rate, refresh_mode;

	if (!panel)
		return -EINVAL;

	refresh_rate = get_panel_refresh_rate(panel);
	refresh_mode = get_panel_refresh_mode(panel);

	/* default S6E3FAC_VRR_TYPE_OSC_HIGH_SYNC_HIGH (120HS) */
	if (refresh_rate < 0 || refresh_mode < 0) {
		panel_err("invalid vrr(refresh rate:%d, mode:%d)\n",
				refresh_rate, refresh_mode);
		return S6E3FAC_VRR_TYPE_OSC_HIGH_SYNC_HIGH;
	}

	if (refresh_mode == VRR_NORMAL_MODE)
		return S6E3FAC_VRR_TYPE_OSC_LOW_SYNC_LOW;

	return (refresh_rate > 60) ?
		S6E3FAC_VRR_TYPE_OSC_HIGH_SYNC_HIGH :
		S6E3FAC_VRR_TYPE_OSC_HIGH_SYNC_LOW;
}

__visible_for_testing s64 get_frame_time(struct panel_device *panel)
{
	if (!panel)
		return 0;

	return 1000000UL / panel->panel_data.props.vrr_origin_fps;
}

__visible_for_testing int get_s6e3fac_wait_1_frame_after_changing_refresh_rate(
	struct panel_device *panel, ktime_t s_time, ktime_t target_time)
{
	ktime_t now = ktime_get();
	s64 remained_vdelay_us;

	if (!panel)
		return -EINVAL;

	remained_vdelay_us = ktime_to_us(ktime_sub(target_time, now));
	if (remained_vdelay_us <= 0) {
		panel_dbg("skip delay (%lld.%06llds >= %lld.%06llds)\n",
				ktime_to_us(now) / 1000000UL,
				ktime_to_us(now) % 1000000UL,
				ktime_to_us(target_time) / 1000000UL,
				ktime_to_us(target_time) % 1000000UL);
		return 0;
	}

	panel_info("remained vdelay %lld.%03lldms\n",
			remained_vdelay_us / 1000UL, remained_vdelay_us % 1000UL);

	return (int)remained_vdelay_us;
}

__visible_for_testing int mark_s6e3fac_wait_1_frame_after_changing_refresh_rate(
	struct panel_device *panel, ktime_t s_time, ktime_t *mark)
{
	ktime_t target, now = ktime_get();
	s64 vdelay_us;

	if (!panel || !mark)
		return -EINVAL;

	vdelay_us = get_frame_time(panel);
	target = ktime_add_us(now, vdelay_us);
	*mark = target;

	panel_info("initial vdelay %lld.%03lldms (fps:%d)\n",
			vdelay_us / 1000UL, vdelay_us % 1000UL,
			panel->panel_data.props.vrr_origin_fps);

	return 0;
}

__visible_for_testing bool is_120hs_based_fps(struct panel_device *panel)
{
	return (get_s6e3fac_vrr_type(panel) == S6E3FAC_VRR_TYPE_OSC_HIGH_SYNC_HIGH);
}

__visible_for_testing bool is_60ns_based_fps(struct panel_device *panel)
{
	return (get_s6e3fac_vrr_type(panel) == S6E3FAC_VRR_TYPE_OSC_LOW_SYNC_LOW);
}

__visible_for_testing bool is_60hs_based_fps(struct panel_device *panel)
{
	return (get_s6e3fac_vrr_type(panel) == S6E3FAC_VRR_TYPE_OSC_HIGH_SYNC_LOW);
}

__visible_for_testing bool is_96hs_60hs_48hs_based_fps(struct panel_device *panel)
{
	int refresh_rate = get_panel_refresh_rate(panel);
	int refresh_mode = get_panel_refresh_mode(panel);

	if (refresh_mode == VRR_HS_MODE &&
			(refresh_rate == 48 || refresh_rate == 60 ||
			 refresh_rate == 96))
		return true;

	return false;
}

static inline bool is_analog_gamma_supported(int vrr_idx)
{
	if (vrr_idx >= S6E3FAC_VRR_60HS && vrr_idx < MAX_S6E3FAC_VRR)
		return true;
	return false;
}

__visible_for_testing bool is_hs_mode(struct panel_device *panel)
{
	return !(get_s6e3fac_vrr_type(panel) == S6E3FAC_VRR_TYPE_OSC_LOW_SYNC_LOW);
}

__visible_for_testing int init_analog_gamma_offset(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	struct dimming_color_offset *dim_data;
	u8 gamma_data[S6E3FAC_ANALOG_GAMMA_WRITE_LEN];
	int i, ret, len, ofs = 0;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("invalid maptbl\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	for (i = 0; i < S6E3FAC_ANALOG_GAMMA_WRITE_SET_COUNT; i++) {
		len = get_resource_size_by_name(panel_data, S6E3FAC_ANALOG_GAMMA_OFFSET_INIT_SRC[i]);
		if (len < 0)
			return len;

		if (maptbl_get_sizeof_maptbl(tbl) < ofs + len) {
			//out of range
			return -EINVAL;
		}
		ret = resource_copy_by_name(panel_data, gamma_data + ofs, S6E3FAC_ANALOG_GAMMA_OFFSET_INIT_SRC[i]);
		if (ret < 0)
			return ret;

		panel_dbg("copy resource %s[%d]\n", S6E3FAC_ANALOG_GAMMA_OFFSET_INIT_SRC[i], i);

		dim_data = get_dimming_data(panel, i);
		if (!IS_ERR_OR_NULL(dim_data) && dim_data->rgb_color_offset) {
			gamma_byte_array_sum(gamma_data + ofs, gamma_data + ofs, dim_data->rgb_color_offset);
			panel_info("update gamma %s with offset[%d]\n", S6E3FAC_ANALOG_GAMMA_OFFSET_INIT_SRC[i], i);
		}
		ofs += len;
	}

	memcpy(tbl->arr, gamma_data, maptbl_get_sizeof_maptbl(tbl));

	return 0;
}

__visible_for_testing bool is_first_set_bl(struct panel_device *panel)
{
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	int cnt;

	cnt = panel_bl_get_brightness_set_count(panel_bl);
	if (cnt == 0) {
		panel_info("true %d\n", cnt);
		return true;
	}
	return false;
}

__visible_for_testing bool is_wait_vsync_needed(struct panel_device *panel)
{
	struct panel_properties *props = &panel->panel_data.props;

	if (props->vrr_origin_mode != props->vrr_mode) {
		panel_info("true(vrr mode changed)\n");
		return true;
	}

	if (!!(props->vrr_origin_fps % 48) != !!(props->vrr_fps % 48)) {
		panel_info("true(adaptive vrr changed)\n");
		return true;
	}

	return false;
}

__visible_for_testing u32 s6e3fac_get_ddi_rev(struct panel_device *panel)
{
	struct panel_info *panel_data;
	u32 ddi_rev;

	if (!panel)
		return -EINVAL;

	panel_data = &panel->panel_data;
	ddi_rev = (panel_data->id[2] & 0x30) >> 4;

	if (ddi_rev >= MAX_DDI_REV) {
		panel_err("invalid ddi_rev range %u", ddi_rev);
		return 0;
	}
	return ddi_rev;
}

__visible_for_testing bool is_osc_94500khz(struct panel_device *panel)
{
	struct panel_info *panel_data = &panel->panel_data;

	return (panel_data->props.osc_freq == 94500);
}

__visible_for_testing bool is_osc_96500khz(struct panel_device *panel)
{
	struct panel_info *panel_data = &panel->panel_data;

	return (panel_data->props.osc_freq == 96500);
}

__visible_for_testing bool is_ddi_evt0(struct panel_device *panel)
{
	return (s6e3fac_get_ddi_rev(panel) == DDI_REV_EVT0 ? true : false);
}

__visible_for_testing void s6e3fac_set_osc1(struct maptbl *tbl, u8 *dst)
{
	u32 ddi_rev = 0;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	ddi_rev = s6e3fac_get_ddi_rev(panel);
	panel_info("id[3]: %d -> ddi_rev: %u, osc_clk: %dkhz\n",
		panel_data->id[2], ddi_rev, panel_data->props.osc_freq);

	switch (ddi_rev) {
	case DDI_REV_EVT0:
		dst[0] = 0x01;
		break;
	case DDI_REV_EVT0_OSC:
		dst[0] = 0x03;
		break;
	case DDI_REV_EVT1:
		dst[0] = 0x03;
		break;
	default:
		panel_err("invalid ddi_rev: %u\n", ddi_rev);
		return;
	}
}

__visible_for_testing void s6e3fac_set_osc2(struct maptbl *tbl, u8 *dst)
{
	u32 ddi_rev = 0;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	ddi_rev = s6e3fac_get_ddi_rev(panel);
	panel_info("id[3]: %d -> ddi_rev: %u, osc_clk: %dkz\n",
		panel_data->id[2], ddi_rev, panel_data->props.osc_freq);

	switch (ddi_rev) {
	case DDI_REV_EVT0:
		dst[0] = 0x00;
		break;
	case DDI_REV_EVT0_OSC:
		dst[0] = 0x02;
		break;
	case DDI_REV_EVT1:
		dst[0] = 0x02;
		break;
	default:
		panel_err("invalid ddi_rev: %u\n", ddi_rev);
		return;
	}
}

__visible_for_testing int s6e3fac_getidx_ddi_rev_table(struct maptbl *tbl)
{
	int idx;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	idx = s6e3fac_get_ddi_rev(panel);

	return maptbl_index(tbl, 0, idx, 0);
}

__visible_for_testing int s6e3fac_getidx_ffc_table(struct maptbl *tbl)
{
	int idx;
	u32 dsi_clk;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	dsi_clk = panel_data->props.dsi_freq;

	switch (dsi_clk) {
	case 1362000:
		idx = HS_CLK_1362;
		break;
	case 1328000:
		idx = HS_CLK_1328;
		break;
	case 1368000:
		idx = HS_CLK_1368;
		break;
	default:
		panel_err("invalid dsi clock: %d\n", dsi_clk);
		BUG();
	}
	return maptbl_index(tbl, 0, idx, 0);
}

__visible_for_testing int s6e3fac_ddi_init(struct panel_device *panel, void *data, u32 len)
{
	return 0;
}

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
