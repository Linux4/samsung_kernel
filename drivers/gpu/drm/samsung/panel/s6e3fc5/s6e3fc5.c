/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fc5/s6e3fc5.c
 *
 * S6E3FC5 Dimming Driver
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
#include "../panel_function.h"
#include "../util.h"
#include "oled_common.h"
#include "s6e3fc5.h"
#include "s6e3fc5_dimming.h"
#ifdef CONFIG_USDM_PANEL_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"
#include "../panel_debug.h"
#include "oled_common.h"
#include "oled_property.h"

#ifdef CONFIG_USDM_PANEL_DIMMING
unsigned int s6e3fc5_gamma_bits[S6E3FC5_NR_TP] = {
	12, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 12, 12,
};

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

#endif /* CONFIG_USDM_PANEL_DIMMING */

#ifdef CONFIG_USDM_PANEL_LPM
#ifdef CONFIG_USDM_PANEL_AOD_BL
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

int s6e3fc5_maptbl_getidx_hbm_transition(struct maptbl *tbl)
{
	int layer, row;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;

	layer = is_hbm_brightness(panel_bl, panel_bl->props.brightness);
	row = panel_bl->props.smooth_transition;

	return maptbl_index(tbl, layer, row, 0);
}

int s6e3fc5_maptbl_getidx_acl_opr(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row, layer;
	struct panel_bl_device *panel_bl;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;

	layer = is_hbm_brightness(panel_bl, panel_bl->props.brightness) ? PANEL_HBM_ON : PANEL_HBM_OFF;

	if (panel_bl_get_acl_pwrsave(&panel->panel_bl) == ACL_PWRSAVE_OFF)
		row = S6E3FC5_ACL_OPR_0;
	else
		row = panel_bl_get_acl_opr(&panel->panel_bl);

	if (row >= MAX_S6E3FC5_ACL_OPR) {
		panel_warn("invalid range %d\n", row);
		row = S6E3FC5_ACL_OPR_1;
	}

	return maptbl_index(tbl, layer, row, 0);
}

int s6e3fc5_maptbl_init_lpm_brt(struct maptbl *tbl)
{
#ifdef CONFIG_USDM_PANEL_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return oled_maptbl_init_default(tbl);
#endif
}

int s6e3fc5_maptbl_getidx_lpm_brt(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_USDM_PANEL_LPM
#ifdef CONFIG_USDM_PANEL_AOD_BL
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

int getidx_s6e3fc5_vrr_fps(int vrr_fps)
{
	int fps_index = S6E3FC5_VRR_FPS_60;

	switch (vrr_fps) {
	case 120:
		fps_index = S6E3FC5_VRR_FPS_120;
		break;
	case 60:
		fps_index = S6E3FC5_VRR_FPS_60;
		break;
	default:
		fps_index = S6E3FC5_VRR_FPS_60;
		panel_err("undefined FPS:%d\n", vrr_fps);
	}

	return fps_index;
}

int find_s6e3fc5_vrr(struct panel_vrr *vrr)
{
	size_t i;

	if (!vrr) {
		panel_err("panel_vrr is null\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(S6E3FC5_VRR_FPS); i++) {
		if (vrr->fps == S6E3FC5_VRR_FPS[i][S6E3FC5_VRR_KEY_REFRESH_RATE] &&
				vrr->mode == S6E3FC5_VRR_FPS[i][S6E3FC5_VRR_KEY_REFRESH_MODE] &&
				vrr->te_sw_skip_count == S6E3FC5_VRR_FPS[i][S6E3FC5_VRR_KEY_TE_SW_SKIP_COUNT] &&
				vrr->te_hw_skip_count == S6E3FC5_VRR_FPS[i][S6E3FC5_VRR_KEY_TE_HW_SKIP_COUNT])
			return (int)i;
	}

	return -EINVAL;
}

int getidx_s6e3fc5_current_vrr_fps(struct panel_device *panel)
{
	int vrr_fps;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	return getidx_s6e3fc5_vrr_fps(vrr_fps);
}

int s6e3fc5_maptbl_getidx_vrr_fps(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0, index;

	index = getidx_s6e3fc5_current_vrr_fps(panel);
	if (index < 0)
		row = 0;
	else
		row = index;

	return maptbl_index(tbl, layer, row, 0);
}

int s6e3fc5_maptbl_getidx_vrr(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;
	int row = 0, layer = 0, index;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL) {
		panel_err("failed to get vrr\n");
		return -EINVAL;
	}

	index = find_s6e3fc5_vrr(vrr);
	if (index < 0) {
		panel_warn("vrr not found\n");
		row = 0;
	} else {
		row = index;
	}

	return maptbl_index(tbl, layer, row, 0);
}

int s6e3fc5_maptbl_getidx_irc_mode(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	return maptbl_index(tbl, 0, !!panel->panel_data.props.irc_mode, 0);
}

#if defined(CONFIG_USDM_FACTORY) && defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
int s6e3fc5_maptbl_getidx_fast_discharge(struct maptbl *tbl)
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

int gamma_ctoi(s32 (*dst)[MAX_COLOR], u8 *src)
{
	unsigned int i, mask, upper_mask, lower_mask;

	if (!src || !dst)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3fc5_gamma_bits); i++) {
		mask = GENMASK(s6e3fc5_gamma_bits[i] - 1, 0);
		upper_mask = (mask >> 8) & 0xFF;
		lower_mask = mask & 0xFF;

		dst[i][RED] = (((src[i * 6 + 0] & upper_mask) << 8) | (src[i * 6 + 1] & lower_mask));
		dst[i][GREEN] = (((src[i * 6 + 2] & upper_mask) << 8) | (src[i * 6 + 3] & lower_mask));
		dst[i][BLUE] = (((src[i * 6 + 4] & upper_mask) << 8) | (src[i * 6 + 5] & lower_mask));
	}

	return 0;
}

int gamma_itoc(u8 *dst, s32(*src)[MAX_COLOR])
{
	unsigned int i, mask;

	if (!src || !dst)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3fc5_gamma_bits); i++) {
		mask = GENMASK(s6e3fc5_gamma_bits[i] - 1, 0);
		dst[i * 6 + 0] = (src[i][RED] >> 8) & (mask >> 8);
		dst[i * 6 + 1] = src[i][RED] & (mask & 0xFF);
		dst[i * 6 + 2] = (src[i][GREEN] >> 8) & (mask >> 8);
		dst[i * 6 + 3] = src[i][GREEN] & (mask & 0xFF);
		dst[i * 6 + 4] = (src[i][BLUE] >> 8) & (mask >> 8);
		dst[i * 6 + 5] = src[i][BLUE] & (mask & 0xFF);
	}

	return 0;
}


int gamma_int_array_sum(s32 (*dst)[MAX_COLOR], s32 (*src)[MAX_COLOR],
		s32 (*offset)[MAX_COLOR])
{
	unsigned int i, c;
	int max_value;

	if (!src || !dst || !offset)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(s6e3fc5_gamma_bits); i++) {
		max_value = GENMASK(s6e3fc5_gamma_bits[i] - 1, 0);
		for_each_color(c)
			dst[i][c] =
			min(max(src[i][c] + offset[i][c], 0), max_value);
	}

	return 0;
}

int gamma_byte_array_sum(u8 *dst, u8 *src,
		s32 (*offset)[MAX_COLOR])
{
	s32 gamma_org[S6E3FC5_NR_TP][MAX_COLOR];
	s32 gamma_new[S6E3FC5_NR_TP][MAX_COLOR];

	if (!dst || !src || !offset)
		return -EINVAL;

	gamma_ctoi(gamma_org, src);
	gamma_int_array_sum(gamma_new, gamma_org, offset);
	gamma_itoc(dst, gamma_new);

	return 0;
}


struct dimming_color_offset *get_dimming_data(struct panel_device *panel, int idx)
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
	if (!panel_dim_info->dimming_data)
		return ERR_PTR(-ENODATA);
	if (panel_dim_info->nr_dimming_data < 1)
		return ERR_PTR(-ENODATA);
	if (idx < 0 || idx >= panel_dim_info->nr_dimming_data)
		return ERR_PTR(-ERANGE);
	dimming_data = panel_dim_info->dimming_data;

	return (dimming_data + idx);
}

int s6e3fc5_maptbl_init_analog_gamma(struct maptbl *tbl)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	struct dimming_color_offset *dim_data;
	u8 gamma_data_120hs[S6E3FC5_ANALOG_GAMMA_LEN] = { 0, };
	u8 gamma_data_60hs[S6E3FC5_ANALOG_GAMMA_LEN] = { 0, };
	int i, ret, len;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("invalid maptbl\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	len = get_panel_resource_size(panel, "analog_gamma_120hs");
	if (len < 0)
		return len;

	if (maptbl_get_sizeof_copy(tbl) != len) {
		panel_err("invalid resource(analog_gamma_120hs) size(%d %d)\n",
				maptbl_get_sizeof_copy(tbl), len);
		return -EINVAL;
	}

	ret = panel_resource_copy(panel,
			gamma_data_120hs, "analog_gamma_120hs");
	if (ret < 0)
		return ret;

	maptbl_for_each_n_dimen_element(tbl, NDARR_2D, i) {
		dim_data = get_dimming_data(panel, i);
		if (IS_ERR_OR_NULL(dim_data) || !dim_data->rgb_color_offset) {
			panel_err("invalid dim_data(index:%d)\n", i);
			return -EINVAL;
		}

		gamma_byte_array_sum(gamma_data_60hs,
				gamma_data_120hs, dim_data->rgb_color_offset);
		memcpy(tbl->arr + maptbl_get_indexof_row(tbl, i), gamma_data_60hs,
				maptbl_get_sizeof_copy(tbl));
#if 0
		usdm_dbg_bytes(tbl->arr + maptbl_get_indexof_row(tbl, i),
				maptbl_get_sizeof_copy(tbl));
#endif
	}


	return 0;
}

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
/*
 * s6e3fc5_decoder_test (A34/A54) - test ddi's decoder function
 *
 * description of state values:
 * [0](14h 1st): 0xF6 (OK) other (NG)
 * [1](14h 2nd): 0x17 (OK) other (NG)
 *
 * [0](15h 1st): 0xB7 (OK) other (NG)
 * [1](15h 2nd): 0xAA (OK) other (NG)
 */
int s6e3fc5_decoder_test(struct panel_device *panel, void *data, u32 len)
{
	int ret = 0;
	u8 read_buf1[S6E3FC5_DECODER_TEST1_LEN] = { -1, -1 };
	u8 read_buf2[S6E3FC5_DECODER_TEST2_LEN] = { -1, -1 };
	u8 read_buf3[S6E3FC5_DECODER_TEST3_LEN] = { -1, -1 };
	u8 read_buf4[S6E3FC5_DECODER_TEST4_LEN] = { -1, -1 };

	if (!panel)
		return -EINVAL;

	ret = panel_do_seqtbl_by_name_nolock(panel, PANEL_DECODER_TEST_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write decoder-test seq\n");
		return ret;
	}

	ret = panel_resource_copy(panel, read_buf1, "decoder_test1"); // 0x14 in normal voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test1 copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel, read_buf2, "decoder_test2"); // 0x15 in normal voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test2 copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel, read_buf3, "decoder_test3"); // 0x14 in low voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test1 copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel, read_buf4, "decoder_test4"); // 0x15 in low voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test2 copy failed\n");
		return -ENODATA;
	}
	panel_info("buf1: 0x%02x,0x%02x, buf2: 0x%02x,0x%02x, buf3: 0x%02x,0x%02x, buf4: 0x%02x,0x%02x\n",
				read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
				read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1]);

	if ((read_buf1[0] == 0xF6) && (read_buf1[1] == 0x17) &&
		(read_buf2[0] == 0xB7) && (read_buf2[1] == 0xAA) &&
		(read_buf3[0] == 0xF6) && (read_buf3[1] == 0x17) &&
		(read_buf4[0] == 0xB7) && (read_buf4[1] == 0xAA)) {
		ret = PANEL_DECODER_TEST_PASS;
		panel_info("Pass [normal]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, [low]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, ret: %d\n",
			read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
			read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1],	ret);
	} else {
		ret = PANEL_DECODER_TEST_FAIL;
		panel_info("Fail [normal]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, [low]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, ret: %d\n",
			read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
			read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1],	ret);
	}

	snprintf((char *)data, len, "%02x %02x %02x %02x %02x %02x %02x %02x",
		read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
		read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1]);

	return ret;
}

/*
 * s6e3fc5_decoder_test_v2 (A35/A55) - test ddi's decoder function
 *
 * description of state values:
 * [0](14h 1st): 0x29 (OK) other (NG)
 * [1](14h 2nd): 0xDC (OK) other (NG)
 *
 * [0](15h 1st): 0x13 (OK) other (NG)
 * [1](15h 2nd): 0x15 (OK) other (NG)
 */
int s6e3fc5_decoder_test_v2(struct panel_device *panel, void *data, u32 len)
{
	int ret = 0;
	u8 read_buf1[S6E3FC5_DECODER_TEST1_LEN] = { -1, -1 };
	u8 read_buf2[S6E3FC5_DECODER_TEST2_LEN] = { -1, -1 };
	u8 read_buf3[S6E3FC5_DECODER_TEST3_LEN] = { -1, -1 };
	u8 read_buf4[S6E3FC5_DECODER_TEST4_LEN] = { -1, -1 };

	if (!panel)
		return -EINVAL;

	ret = panel_do_seqtbl_by_name_nolock(panel, PANEL_DECODER_TEST_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write decoder-test seq\n");
		return ret;
	}

	ret = panel_resource_copy(panel, read_buf1, "decoder_test1"); // 0x14 in normal voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test1 copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel, read_buf2, "decoder_test2"); // 0x15 in normal voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test2 copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel, read_buf3, "decoder_test3"); // 0x14 in low voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test1 copy failed\n");
		return -ENODATA;
	}

	ret = panel_resource_copy(panel, read_buf4, "decoder_test4"); // 0x15 in low voltage
	if (unlikely(ret < 0)) {
		panel_err("decoder_test2 copy failed\n");
		return -ENODATA;
	}
	panel_info("buf1: 0x%02x,0x%02x, buf2: 0x%02x,0x%02x, buf3: 0x%02x,0x%02x, buf4: 0x%02x,0x%02x\n",
				read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
				read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1]);

	if ((read_buf1[0] == 0x29) && (read_buf1[1] == 0xDC) &&
		(read_buf2[0] == 0x13) && (read_buf2[1] == 0x15) &&
		(read_buf3[0] == 0x29) && (read_buf3[1] == 0xDC) &&
		(read_buf4[0] == 0x13) && (read_buf4[1] == 0x15)) {
		ret = PANEL_DECODER_TEST_PASS;
		panel_info("Pass [normal]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, [low]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, ret: %d\n",
			read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
			read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1],	ret);
	} else {
		ret = PANEL_DECODER_TEST_FAIL;
		panel_info("Fail [normal]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, [low]:0x14->0x%02x,0x%02x, 0x15->0x%02x,0x%02x, ret: %d\n",
			read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
			read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1],	ret);
	}

	snprintf((char *)data, len, "%02x %02x %02x %02x %02x %02x %02x %02x",
		read_buf1[0], read_buf1[1], read_buf2[0], read_buf2[1],
		read_buf3[0], read_buf3[1], read_buf4[0], read_buf4[1]);

	return ret;
}
#endif

#ifdef CONFIG_USDM_FACTORY_ECC_TEST
/*
 * s6e3fc5_ecc_test - check state of dram ecc function register
 *
 * description of state values:
 * [0](F8h 3rd): 0x00 (OK) 0x01 (NG)
 * [1](F8h 4th): 0x00 (OK) other (NG)
 * [2](F8h 5th): 0x00 (OK) other (NG)
 */
int s6e3fc5_ecc_test(struct panel_device *panel, void *data, u32 len)
{
	int ret = 0;
	char read_buf[S6E3FC5_ECC_TEST_LEN] = { 0, };

	if (!panel)
		return -EINVAL;

	ret = panel_do_seqtbl_by_name(panel, PANEL_ECC_TEST_SEQ);
	if (unlikely(ret < 0)) {
		panel_err("failed to write ecc-test seq\n");
		return ret;
	}

	ret = panel_resource_copy(panel, read_buf, "ecc_test");
	if (unlikely(ret < 0)) {
		panel_err("ecc_test copy failed\n");
		return -ENODATA;
	}

	ret = PANEL_ECC_TEST_FAIL;
	if (read_buf[0] == 0x00)
		ret = PANEL_ECC_TEST_PASS;

	panel_info("[0]: 0x%02x ret: %d\n", read_buf[0], ret);
	return ret;
}
#endif

int s6e3fc5_maptbl_init_gamma_mode2_brt(struct maptbl *tbl)
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

int s6e3fc5_maptbl_getidx_ffc(struct maptbl *tbl)
{
	int idx;
	u32 dsi_clk;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;

	dsi_clk = panel_data->props.dsi_freq;

	switch (dsi_clk) {
	case 1108000:
		idx = S6E3FC5_HS_CLK_1108;
		break;
	case 1124000:
		idx = S6E3FC5_HS_CLK_1124;
		break;
	case 1125000:
		idx = S6E3FC5_HS_CLK_1125;
		break;
	default:
		panel_err("invalid dsi clock: %d\n", dsi_clk);
		BUG();
	}
	return maptbl_index(tbl, 0, idx, 0);
}

int s6e3fc5_get_cell_id(struct panel_device *panel, void *buf)
{
	u8 date[PANEL_DATE_LEN] = { 0, }, coordinate[4] = { 0, };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_resource_copy(panel, date, "date");
	panel_resource_copy(panel, coordinate, "coordinate");

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		date[0], date[1], date[2], date[3], date[4], date[5], date[6],
		coordinate[0], coordinate[1], coordinate[2], coordinate[3]);
	return 0;
}
int s6e3fc5_get_octa_id(struct panel_device *panel, void *buf)
{
	int i, site, rework, poc;
	u8 cell_id[16], octa_id[PANEL_OCTA_ID_LEN] = { 0, };
	int len = 0;
	bool cell_id_exist = true;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_resource_copy(panel, octa_id, "octa_id");

	site = (octa_id[0] >> 4) & 0x0F;
	rework = octa_id[0] & 0x0F;
	poc = octa_id[1] & 0x0F;

	panel_dbg("site (%d), rework (%d), poc (%d)\n",
			site, rework, poc);

	panel_dbg("<CELL ID>\n");
	for (i = 0; i < 16; i++) {
		cell_id[i] = isalnum(octa_id[i + 4]) ? octa_id[i + 4] : '\0';
		panel_dbg("%x -> %c\n", octa_id[i + 4], cell_id[i]);
		if (cell_id[i] == '\0') {
			cell_id_exist = false;
			break;
		}
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "%d%d%d%02x%02x",
			site, rework, poc, octa_id[2], octa_id[3]);
	if (cell_id_exist) {
		for (i = 0; i < 16; i++)
			len += snprintf(buf + len, PAGE_SIZE - len, "%c", cell_id[i]);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	return 0;

}

int s6e3fc5_get_manufacture_code(struct panel_device *panel, void *buf)
{
	u8 code[5] = { 0, };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_resource_copy(panel, code, "code");

	snprintf(buf, PAGE_SIZE, "%02X%02X%02X%02X%02X\n",
		code[0], code[1], code[2], code[3], code[4]);

	return 0;
}

int s6e3fc5_get_manufacture_date(struct panel_device *panel, void *buf)
{
	u16 year;
	u8 month, day, hour, min, date[PANEL_DATE_LEN] = { 0, };

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_resource_copy(panel, date, "date");

	year = ((date[0] & 0xF0) >> 4) + 2011;
	month = date[0] & 0xF;
	day = date[1] & 0x1F;
	hour = date[2] & 0x1F;
	min = date[3] & 0x3F;

	snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d:%d\n",
			year, month, day, hour, min);

	return 0;
}

struct pnobj_func s6e3fc5_function_table[MAX_S6E3FC5_FUNCTION] = {
	[S6E3FC5_MAPTBL_INIT_GAMMA_MODE2_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_INIT_GAMMA_MODE2_BRT, s6e3fc5_maptbl_init_gamma_mode2_brt),
	[S6E3FC5_MAPTBL_GETIDX_HBM_TRANSITION] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_GETIDX_HBM_TRANSITION, s6e3fc5_maptbl_getidx_hbm_transition),
	[S6E3FC5_MAPTBL_GETIDX_ACL_OPR] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_GETIDX_ACL_OPR, s6e3fc5_maptbl_getidx_acl_opr),
	[S6E3FC5_MAPTBL_INIT_LPM_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_INIT_LPM_BRT, s6e3fc5_maptbl_init_lpm_brt),
	[S6E3FC5_MAPTBL_GETIDX_LPM_BRT] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_GETIDX_LPM_BRT, s6e3fc5_maptbl_getidx_lpm_brt),
#if defined(CONFIG_USDM_FACTORY) && defined(CONFIG_USDM_FACTORY_FAST_DISCHARGE)
	[S6E3FC5_MAPTBL_GETIDX_FAST_DISCHARGE] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_GETIDX_FAST_DISCHARGE, s6e3fc5_maptbl_getidx_fast_discharge),
#endif
	[S6E3FC5_MAPTBL_GETIDX_VRR_FPS] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_GETIDX_VRR_FPS, s6e3fc5_maptbl_getidx_vrr_fps),
	[S6E3FC5_MAPTBL_GETIDX_VRR] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_GETIDX_VRR, s6e3fc5_maptbl_getidx_vrr),
	[S6E3FC5_MAPTBL_GETIDX_FFC] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_GETIDX_FFC, s6e3fc5_maptbl_getidx_ffc),
	[S6E3FC5_MAPTBL_INIT_ANALOG_GAMMA] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_INIT_ANALOG_GAMMA, s6e3fc5_maptbl_init_analog_gamma),
	[S6E3FC5_MAPTBL_GETIDX_IRC_MODE] = __PNOBJ_FUNC_INITIALIZER(S6E3FC5_MAPTBL_GETIDX_IRC_MODE, s6e3fc5_maptbl_getidx_irc_mode),
};

int s6e3fc5_init(struct common_panel_info *cpi)
{
	static bool once;
	int ret;

	if (once)
		return 0;

	ret = panel_function_insert_array(s6e3fc5_function_table,
			ARRAY_SIZE(s6e3fc5_function_table));
	if (ret < 0)
		panel_err("failed to insert s6e3fc5_function_table\n");

	cpi->prop_lists[USDM_DRV_LEVEL_COMMON] = oled_property_array;
	cpi->num_prop_lists[USDM_DRV_LEVEL_COMMON] = oled_property_array_size;

	once = true;

	return 0;
}

MODULE_DESCRIPTION("Samsung Mobile Panel Driver");
MODULE_LICENSE("GPL");
