/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hac/s6e3hac.c
 *
 * S6E3HAC Dimming Driver
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
#include "s6e3hac.h"
#include "s6e3hac_panel.h"
#ifdef CONFIG_PANEL_AID_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"

#ifdef PANEL_PR_TAG
#undef PANEL_PR_TAG
#define PANEL_PR_TAG	"ddi"
#endif

static int find_s6e3hac_vrr(struct panel_vrr *vrr)
{
	int i;

	if (!vrr) {
		panel_err("panel_vrr is null\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(S6E3HAC_VRR_FPS); i++)
		if (vrr->fps == S6E3HAC_VRR_FPS[i][S6E3HAC_VRR_KEY_REFRESH_RATE] &&
			vrr->mode == S6E3HAC_VRR_FPS[i][S6E3HAC_VRR_KEY_REFRESH_MODE] &&
			vrr->te_sw_skip_count == S6E3HAC_VRR_FPS[i][S6E3HAC_VRR_KEY_TE_SW_SKIP_COUNT] &&
			vrr->te_hw_skip_count == S6E3HAC_VRR_FPS[i][S6E3HAC_VRR_KEY_TE_HW_SKIP_COUNT])
			return i;


	return -EINVAL;
}

static int getidx_s6e3hac_lfd_frame_idx(int fps, int mode)
{
	int i = 0, start = 0, end = 0;

	if (mode == VRR_HS_MODE) {
		start = S6E3HAC_VRR_LFD_FRAME_IDX_HS_BEGIN;
		end = S6E3HAC_VRR_LFD_FRAME_IDX_HS_END;
	} else if (mode == VRR_NORMAL_MODE) {
		start = S6E3HAC_VRR_LFD_FRAME_IDX_NS_BEGIN;
		end = S6E3HAC_VRR_LFD_FRAME_IDX_NS_END;
	}
	for (i = start; i <= end; i++)
		if (fps >= S6E3HAC_VRR_LFD_FRAME_IDX_VAL[i])
			return i;

	return -EINVAL;
}

static int get_s6e3hac_lfd_min_freq(u32 scalability, u32 vrr_fps_index)
{
	if (scalability > S6E3HAC_VRR_LFD_SCALABILITY_MAX) {
		panel_warn("exceeded scalability (%d)\n", scalability);
		scalability = S6E3HAC_VRR_LFD_SCALABILITY_MAX;
	}
	if (vrr_fps_index >= MAX_S6E3HAC_VRR) {
		panel_err("invalid vrr_fps_index %d\n", vrr_fps_index);
		return -EINVAL;
	}
	return S6E3HAC_VRR_LFD_MIN_FREQ[scalability][vrr_fps_index];
}

static int get_s6e3hac_lpm_lfd_min_freq(u32 scalability)
{
	if (scalability > S6E3HAC_VRR_LFD_SCALABILITY_MAX) {
		panel_warn("exceeded scalability (%d)\n", scalability);
		scalability = S6E3HAC_VRR_LFD_SCALABILITY_MAX;
	}
	return S6E3HAC_LPM_LFD_MIN_FREQ[scalability];
}

static int s6e3hac_get_vrr_lfd_min_div_count(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct panel_vrr *vrr;
	int index = 0, ret;
	int vrr_fps, vrr_mode;
	u32 lfd_min_freq = 0;
	u32 lfd_min_div_count = 0;
	u32 vrr_div_count;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	index = find_s6e3hac_vrr(vrr);
	if (index < 0) {
		panel_err("vrr(%d %d) not found\n",
				vrr_fps, vrr_mode);
		return -EINVAL;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	ret = get_s6e3hac_lfd_min_freq(vrr_lfd_config->scalability, index);
	if (ret < 0) {
		panel_err("failed to get lfd_min_freq(%d)\n", ret);
		return -EINVAL;
	}
	lfd_min_freq = ret;

	if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH) {
		lfd_min_div_count = vrr_div_count;
	} else if (vrr_lfd_config->fix == VRR_LFD_FREQ_LOW ||
			vrr_lfd_config->min == 0 ||
			vrr_div_count == 0) {
		lfd_min_div_count = disp_div_round(vrr_fps, lfd_min_freq);
	} else {
		lfd_min_freq = max(lfd_min_freq,
				min(disp_div_round(vrr_fps, vrr_div_count), vrr_lfd_config->min));
		lfd_min_div_count = (lfd_min_freq == 0) ?
			MIN_S6E3HAC_FPS_DIV_COUNT : disp_div_round(vrr_fps, lfd_min_freq);
	}

	panel_dbg("vrr(%d %d), div(%d), lfd(fix:%d scale:%d min:%d max:%d), div_count(%d)\n",
			vrr_fps, vrr_mode, vrr_div_count,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max, lfd_min_div_count);

	return lfd_min_div_count;
}

static int s6e3hac_get_vrr_lfd_max_div_count(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct panel_vrr *vrr;
	int index = 0, ret;
	int vrr_fps, vrr_mode;
	u32 lfd_min_freq = 0;
	u32 lfd_max_freq = 0;
	u32 lfd_max_div_count = 0;
	u32 vrr_div_count;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	index = find_s6e3hac_vrr(vrr);
	if (index < 0) {
		panel_err("vrr(%d %d) not found\n",
				vrr_fps, vrr_mode);
		return -EINVAL;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	ret = get_s6e3hac_lfd_min_freq(vrr_lfd_config->scalability, index);
	if (ret < 0) {
		panel_err("failed to get lfd_min_freq(%d)\n", ret);
		return -EINVAL;
	}

	lfd_min_freq = ret;
	if (vrr_lfd_config->fix == VRR_LFD_FREQ_LOW) {
		lfd_max_div_count = disp_div_round((u32)vrr_fps, lfd_min_freq);
	} else if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH ||
		vrr_lfd_config->max == 0 ||
		vrr_div_count == 0) {
		lfd_max_div_count = vrr_div_count;
	} else {
		lfd_max_freq = max(lfd_min_freq,
				min(disp_div_round(vrr_fps, vrr_div_count), vrr_lfd_config->max));
		lfd_max_div_count = (lfd_max_freq == 0) ?
			MIN_S6E3HAC_FPS_DIV_COUNT : disp_div_round(vrr_fps, lfd_max_freq);
	}

	panel_dbg("vrr(%d %d), div(%d), lfd(fix:%d scale:%d min:%d max:%d), div_count(%d)\n",
			vrr_fps, vrr_mode, vrr_div_count,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max,
			lfd_max_div_count);

	return lfd_max_div_count;
}

static int s6e3hac_get_vrr_lfd_min_freq(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	int vrr_fps, div_count;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	div_count = s6e3hac_get_vrr_lfd_min_div_count(panel_data);
	if (div_count <= 0)
		return -EINVAL;

	return (int)disp_div_round(vrr_fps, div_count);
}

static int s6e3hac_get_vrr_lfd_max_freq(struct panel_info *panel_data)
{
	struct panel_device *panel = to_panel_device(panel_data);
	int vrr_fps, div_count;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	div_count = s6e3hac_get_vrr_lfd_max_div_count(panel_data);
	if (div_count <= 0)
		return -EINVAL;

	return (int)disp_div_round(vrr_fps, div_count);
}

static int getidx_s6e3hac_vrr_mode(int mode)
{
	return mode == VRR_HS_MODE ?
		S6E3HAC_VRR_MODE_HS : S6E3HAC_VRR_MODE_NS;
}

static int generate_brt_step_table(struct brightness_table *brt_tbl)
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
	for (i = 0; i < brt_tbl->sz_step_cnt; i++)
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
				if (brt_tbl->brt[brt_tbl->sz_brt - 1] < brt_tbl->brt_to_step[i])
					brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				if (i >= brt_tbl->sz_brt_to_step) {
					panel_err("step cnt over %d %d\n", i, brt_tbl->sz_brt_to_step);
					break;
				}
			}
		}
	}
	return ret;
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

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int getidx_common_maptbl(struct maptbl *tbl)
{
	return 0;
}
#endif

static int gamma_ctoi(s32 (*dst)[MAX_COLOR], u8 *src, int nr_tp)
{
	unsigned int i, c, pos = 1;

	for (i = nr_tp - 1; i > 1; i--) {
		for_each_color(c) {
			if (i == nr_tp - 1)
				dst[i][c] = ((src[0] >> ((MAX_COLOR - c - 1) * 2)) & 0x03) << 8 | src[pos++];
			else
				dst[i][c] = src[pos++];
		}
	}

	dst[1][RED] = (src[pos + 0] >> 4) & 0xF;
	dst[1][GREEN] = src[pos + 0] & 0xF;
	dst[1][BLUE] = (src[pos + 1] >> 4) & 0xF;

	dst[0][RED] = src[pos + 1] & 0xF;
	dst[0][GREEN] = (src[pos + 2] >> 4) & 0xF;
	dst[0][BLUE] = src[pos + 2] & 0xF;

	return 0;
}

static int gamma_sum(s32 (*dst)[MAX_COLOR], s32 (*src)[MAX_COLOR],
		s32 (*offset)[MAX_COLOR], int nr_tp)
{
	unsigned int i, c;
	int upper_limit[S6E3HAC_NR_TP] = {
		0xF, 0xF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3FF
	};

	if (nr_tp != S6E3HAC_NR_TP) {
		panel_err("invalid nr_tp(%d)\n", nr_tp);
		return -EINVAL;
	}

	for (i = 0; i < nr_tp; i++) {
		for_each_color(c)
			dst[i][c] =
			min(max(src[i][c] + offset[i][c], 0), upper_limit[i]);
	}

	return 0;
}

static void copy_tpout_center(u8 *output, u32 value, u32 v, u32 color)
{
	int index = (S6E3HAC_NR_TP - v - 1);

	if (v == S6E3HAC_NR_TP - 1) {
		output[0] &= ~(0x3 << ((MAX_COLOR - color - 1) * 2));
		output[0] |= (((value >> 8) & 0x03) << ((MAX_COLOR - color - 1) * 2));
		output[color + 1] = value & 0x00FF;
		if (value > 1023)
			panel_err("error : exceed output range tp:%d c:%d value:%d!!\n",
					index, color, value);
	} else if (v == 1) {
		if (color == RED) {
			output[index * MAX_COLOR + 1] &= ~(0xF << 4);
			output[index * MAX_COLOR + 1] |= (value & 0xF) << 4;
		} else if (color == GREEN) {
			output[index * MAX_COLOR + 1] &= ~0xF;
			output[index * MAX_COLOR + 1] |= (value & 0xF);
		} else if (color == BLUE) {
			output[index * MAX_COLOR + 2] &= ~(0xF << 4);
			output[index * MAX_COLOR + 2] |= (value & 0xF) << 4;
		}
		if (value > 15)
			panel_err("error : exceed output range tp:%d c:%d value:%d!!\n",
					index, color, value);
	} else if (v == 0) {
		if (color == RED) {
			output[(index - 1) * MAX_COLOR + 2] &= ~0xF;
			output[(index - 1) * MAX_COLOR + 2] |= (value & 0xF);
		} else if (color == GREEN) {
			output[(index - 1) * MAX_COLOR + 3] &= ~(0xF << 4);
			output[(index - 1) * MAX_COLOR + 3] |= (value & 0xF) << 4;
		} else if (color == BLUE) {
			output[(index - 1) * MAX_COLOR + 3] &= ~0xF;
			output[(index - 1) * MAX_COLOR + 3] |= (value & 0xF);
		}
		if (value > 15)
			panel_err("error : exceed output range tp:%d c:%d value:%d!!\n",
					index, color, value);
	} else {
		output[index * MAX_COLOR + color + 1] = value & 0x00FF;
		if (value > 255)
			panel_err("error : exceed output range tp:%d c:%d value:%d!!\n",
					index, color, value);
	}
}

static void gamma_itoc(u8 *dst, s32(*src)[MAX_COLOR], int nr_tp)
{
	int v, c;

	if (nr_tp != S6E3HAC_NR_TP) {
		panel_err("invalid nr_tp(%d)\n", nr_tp);
		return;
	}

	for (v = 0; v < nr_tp; v++) {
		for_each_color(c) {
			copy_tpout_center(dst, src[v][c], v, c);
		}
	}
}

static int init_gamma_mtp_table(struct maptbl *tbl, int vrr_mode, int gamma_index)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	struct panel_dimming_info *panel_dim_info;
	u8 gamma_8_org[S6E3HAC_GAMMA_MTP_LEN];
	u8 gamma_8_new[S6E3HAC_GAMMA_MTP_LEN];
	s32 gamma_32_org[S6E3HAC_NR_TP][MAX_COLOR];
	s32 gamma_32_new[S6E3HAC_NR_TP][MAX_COLOR];
	struct maptbl_pos pos;
	struct gm2_dimming_lut *dim_lut;
	u32 nr_dim_lut;
	int vrr_fps_index, ret;
	s32(*rgb_color_offset)[MAX_COLOR];
	char *gamma_mtp_resource_name[MAX_S6E3HAC_VRR_MODE][MAX_S6E3HAC_GAMMA_MTP] = {
		[S6E3HAC_VRR_MODE_NS] = {
			"gamma_mtp_0_ns",
			"gamma_mtp_1_ns",
			"gamma_mtp_2_ns",
			"gamma_mtp_3_ns",
			"gamma_mtp_4_ns",
			"gamma_mtp_5_ns",
			"gamma_mtp_6_ns",
		},
		[S6E3HAC_VRR_MODE_HS] = {
			"gamma_mtp_0_hs",
			"gamma_mtp_1_hs",
			"gamma_mtp_2_hs",
			"gamma_mtp_3_hs",
			"gamma_mtp_4_hs",
			"gamma_mtp_5_hs",
			"gamma_mtp_6_hs",
		},
	};

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				PANEL_BL_SUBDEV_TYPE_DISP, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	if (gamma_index >= MAX_S6E3HAC_GAMMA_MTP) {
		panel_err("invalid param(gamma_index %d)\n", gamma_index);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];
	if (unlikely(!panel_dim_info)) {
		panel_err("panel_bl-%d panel_dim_info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		return -EINVAL;
	}
	memset(&pos, 0, sizeof(struct maptbl_pos));

	if (panel_dim_info->nr_gm2_dim_init_info == 0) {
		panel_err("panel_bl-%d gammd mode 2 init info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		return -EINVAL;
	}

	dim_lut = panel_dim_info->gm2_dim_init_info[0].dim_lut;
	nr_dim_lut = panel_dim_info->gm2_dim_init_info[0].nr_dim_lut;
	if (nr_dim_lut != MAX_S6E3HAC_VRR * MAX_S6E3HAC_GAMMA_MTP) {
		panel_err("panel_bl-%d invalid nr_dim_lut(%d != %d)\n",
				PANEL_BL_SUBDEV_TYPE_DISP,
				nr_dim_lut, MAX_S6E3HAC_VRR * MAX_S6E3HAC_GAMMA_MTP);
		return -EINVAL;
	}

	/* copy resource */
	ret = resource_copy_by_name(panel_data,
			gamma_8_org, gamma_mtp_resource_name[vrr_mode][gamma_index]);
	if (unlikely(ret < 0)) {
		panel_err("%s not found in panel resource (%d %d)\n",
				gamma_mtp_resource_name[vrr_mode][gamma_index],
				vrr_mode, gamma_index);
		return -EINVAL;
	}

	if (tbl->nrow != MAX_S6E3HAC_VRR) {
		panel_err("invalid row size(%d)\n", tbl->nrow);
		return -EINVAL;
	}

	/* decode gamma8 to gamma32 */
	gamma_ctoi(gamma_32_org, gamma_8_org, S6E3HAC_NR_TP);
	for_each_row(tbl, vrr_fps_index) {
		rgb_color_offset = dim_lut[vrr_fps_index * MAX_S6E3HAC_GAMMA_MTP + gamma_index].rgb_color_offset;
		if (rgb_color_offset == NULL ||
				S6E3HAC_VRR_FPS[vrr_fps_index][1] != vrr_mode)
			memcpy((u8 *)gamma_32_new, gamma_32_org, sizeof(gamma_32_org));
		else
			gamma_sum(gamma_32_new, gamma_32_org,
					rgb_color_offset, S6E3HAC_NR_TP);
		gamma_itoc(gamma_8_new, gamma_32_new, S6E3HAC_NR_TP);
		pos.index[NDARR_2D] = vrr_fps_index;
		maptbl_fill(tbl, &pos, gamma_8_new, S6E3HAC_GAMMA_MTP_LEN);
	}
	panel_dbg("%s initialize\n", tbl->name);

	return 0;
}

static int init_gamma_mtp_0_ns_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_NS, S6E3HAC_GAMMA_MTP_0);
}

static int init_gamma_mtp_1_ns_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_NS, S6E3HAC_GAMMA_MTP_1);
}

static int init_gamma_mtp_2_ns_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_NS, S6E3HAC_GAMMA_MTP_2);
}

static int init_gamma_mtp_3_ns_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_NS, S6E3HAC_GAMMA_MTP_3);
}

static int init_gamma_mtp_4_ns_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_NS, S6E3HAC_GAMMA_MTP_4);
}

static int init_gamma_mtp_5_ns_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_NS, S6E3HAC_GAMMA_MTP_5);
}

static int init_gamma_mtp_6_ns_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_NS, S6E3HAC_GAMMA_MTP_6);
}

static int init_gamma_mtp_0_hs_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_HS, S6E3HAC_GAMMA_MTP_0);
}

static int init_gamma_mtp_1_hs_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_HS, S6E3HAC_GAMMA_MTP_1);
}

static int init_gamma_mtp_2_hs_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_HS, S6E3HAC_GAMMA_MTP_2);
}

static int init_gamma_mtp_3_hs_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_HS, S6E3HAC_GAMMA_MTP_3);
}

static int init_gamma_mtp_4_hs_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_HS, S6E3HAC_GAMMA_MTP_4);
}

static int init_gamma_mtp_5_hs_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_HS, S6E3HAC_GAMMA_MTP_5);
}

static int init_gamma_mtp_6_hs_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl, S6E3HAC_VRR_MODE_HS, S6E3HAC_GAMMA_MTP_6);
}

#if defined(CONFIG_SUPPORT_DOZE) && \
	defined(CONFIG_SUPPORT_AOD_BL)
static int init_aod_dimming_table(struct maptbl *tbl)
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

static int init_vbias1_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	int table_size, ret = 0;
	u8 *vbias;

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

	table_size = sizeof_maptbl(tbl);
	vbias = kzalloc(sizeof(u8) * table_size, GFP_KERNEL);

	ret = resource_copy_by_name(panel_data, vbias, "gm2_flash_vbias1");
	if (unlikely(ret)) {
		panel_err("gm2_flash_vbias1 not found in panel resource\n");
		ret = -EINVAL;
		goto exit;
	}
	memcpy(tbl->arr, vbias, sizeof(u8) * table_size);

exit:
	kfree(vbias);
	return ret;
}

static int init_vbias2_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	int table_size, ret = 0;
	u8 *vbias;

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

	table_size = sizeof_maptbl(tbl);
	vbias = kzalloc(sizeof(u8) * table_size, GFP_KERNEL);

	ret = resource_copy_by_name(panel_data, vbias, "gm2_flash_vbias2");
	if (unlikely(ret)) {
		panel_err("gm2_flash_vbias2 not found in panel resource\n");
		ret = -EINVAL;
		goto exit;
	}
	memcpy(tbl->arr, vbias, sizeof(u8) * table_size);

exit:
	kfree(vbias);
	return ret;
}

static int getidx_vbias1_table(struct maptbl *tbl)
{
	int row, layer;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	layer = get_panel_refresh_mode(panel);
	if (layer < 0)
		return -EINVAL;

	row = (UNDER_MINUS_15(panel_data->props.temperature) ? UNDER_MINUS_FIFTEEN :
			(UNDER_0(panel_data->props.temperature) ? UNDER_ZERO : OVER_ZERO));

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_vbias2_table(struct maptbl *tbl)
{
	int row;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	row = get_panel_refresh_mode(panel);
	if (row < 0)
		return -EINVAL;

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_gm2_elvss_table(struct maptbl *tbl)
{
	int row;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	row = get_actual_brightness_index(panel_bl, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_hbm_transition_table(struct maptbl *tbl)
{
	int layer, row;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	layer = is_hbm_brightness(panel_bl, panel_bl->props.brightness);
	row = panel_bl->props.smooth_transition;

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_dia_onoff_table(struct maptbl *tbl)
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

static int getidx_irc_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	return maptbl_index(tbl, 0, panel->panel_data.props.irc_mode ? 1 : 0, 0);
}

#ifdef CONFIG_SUPPORT_XTALK_MODE
static int getidx_vgh_table(struct maptbl *tbl)
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

static int getidx_acl_opr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (panel_bl_get_acl_pwrsave(&panel->panel_bl) == ACL_PWRSAVE_OFF)
		row = ACL_OPR_OFF;
	else
		row = panel_bl_get_acl_opr(&panel->panel_bl);

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_dsc_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props;
	struct panel_mres *mres;
	int row = 1;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	props = &panel->panel_data.props;
	mres = &panel->panel_data.mres;

	if (mres->nr_resol == 0 || mres->resol == NULL) {
		panel_info("nor_resol is null\n");
		return maptbl_index(tbl, 0, row, 0);
	}

	if (mres->resol[props->mres_mode].comp_type
			== PN_COMP_TYPE_DSC)
		row = 1;

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_resolution_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_properties *props = &panel->panel_data.props;
	int row = 0, layer = 0, index;
	int xres = 0, yres = 0;

	if (mres->nr_resol == 0 || mres->resol == NULL)
		return maptbl_index(tbl, layer, row, 0);

	if (props->mres_mode >= mres->nr_resol) {
		xres = mres->resol[0].w;
		yres = mres->resol[0].h;
		panel_err("invalid mres_mode %d, nr_resol %d\n",
				props->mres_mode, mres->nr_resol);
	} else {
		xres = mres->resol[props->mres_mode].w;
		yres = mres->resol[props->mres_mode].h;
		panel_info("mres_mode %d (%dx%d)\n",
				props->mres_mode,
				mres->resol[props->mres_mode].w,
				mres->resol[props->mres_mode].h);
	}

	index = search_table_u32(S6E3HAC_SCALER_1440,
			ARRAY_SIZE(S6E3HAC_SCALER_1440), xres);
	if (index < 0)
		row = 0;

	row = index;

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_vrr_fps_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_vrr *vrr;
	int row = 0, layer = 0, index;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return -EINVAL;

	index = find_s6e3hac_vrr(vrr);
	if (index < 0)
		row = (vrr->mode == VRR_NORMAL_MODE) ?
			S6E3HAC_VRR_60NS : S6E3HAC_VRR_120HS;
	else
		row = index;

	return maptbl_index(tbl, layer, row, 0);
}

static int init_lpm_brt_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return init_common_table(tbl);
#endif
}

static int getidx_lpm_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_SUPPORT_DOZE
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
		row = tbl->nrow - 1;
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

static int getidx_lpm_mode_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_SUPPORT_DOZE
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
	case ALPM_HIGH_BR:
		row = ALPM_MODE;
		break;
	case HLPM_LOW_BR:
	case HLPM_HIGH_BR:
		row = HLPM_MODE;
		break;
	default:
		panel_err("Invalid alpm mode : %d\n", props->alpm_mode);
		break;
	}
	panel_info("alpm_mode %d -> %d\n", props->cur_alpm_mode, props->alpm_mode);
	props->cur_alpm_mode = props->alpm_mode;
#endif

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_lpm_fps_table(struct maptbl *tbl)
{
	int row = LPM_LFD_1HZ, lpm_lfd_min_freq;
	struct panel_device *panel;
	struct panel_properties *props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct vrr_lfd_status *vrr_lfd_status;

	panel = (struct panel_device *)tbl->pdata;
	props = &panel->panel_data.props;

	vrr_lfd_status = &props->vrr_lfd_info.status[VRR_LFD_SCOPE_LPM];
	vrr_lfd_status->lfd_max_freq = 30;
	vrr_lfd_status->lfd_max_freq_div = 1;
	vrr_lfd_status->lfd_min_freq = 1;
	vrr_lfd_status->lfd_min_freq_div = 30;

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_LPM];
	if (vrr_lfd_config->fix == VRR_LFD_FREQ_HIGH) {
		row = LPM_LFD_30HZ;
		vrr_lfd_status->lfd_min_freq = 30;
		vrr_lfd_status->lfd_min_freq_div = 1;
		panel_info("lpm_fps %dhz (row:%d)\n",
				(row == LPM_LFD_1HZ) ? 1 : 30, row);
		return maptbl_index(tbl, 0, row, 0);
	}

	lpm_lfd_min_freq =
		get_s6e3hac_lpm_lfd_min_freq(vrr_lfd_config->scalability);
	if (lpm_lfd_min_freq <= 0 || lpm_lfd_min_freq > 1) {
		row = LPM_LFD_30HZ;
		vrr_lfd_status->lfd_min_freq = 30;
		vrr_lfd_status->lfd_min_freq_div = 1;
		panel_info("lpm_fps %dhz (row:%d)\n",
				(row == LPM_LFD_1HZ) ? 1 : 30, row);
		return maptbl_index(tbl, 0, row, 0);
	}

#ifdef CONFIG_SUPPORT_DOZE
	switch (props->lpm_fps) {
	case LPM_LFD_1HZ:
		row = props->lpm_fps;
		vrr_lfd_status->lfd_min_freq = 1;
		vrr_lfd_status->lfd_min_freq_div = 30;
		break;
	case LPM_LFD_30HZ:
		row = props->lpm_fps;
		vrr_lfd_status->lfd_min_freq = 30;
		vrr_lfd_status->lfd_min_freq_div = 1;
		break;
	default:
		panel_err("invalid lpm_fps %d\n",
				props->lpm_fps);
		break;
	}
		panel_info("lpm_fps %dhz(row:%d)\n",
				(row == LPM_LFD_1HZ) ? 1 : 30, row);
#endif

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_vrr_mode_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0;
	int vrr_mode;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	row = getidx_s6e3hac_vrr_mode(vrr_mode);
	if (row < 0) {
		panel_err("failed to getidx_s6e3hac_vrr_mode(mode:%d)\n", vrr_mode);
		row = 0;
	}
	panel_dbg("vrr_mode:%d(%s)\n", row,
			row == S6E3HAC_VRR_MODE_HS ? "HS" : "NM");

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_lfd_frame_insertion_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_properties *props = &panel->panel_data.props;
	struct vrr_lfd_config *vrr_lfd_config;
	int vrr_mode;
	int lfd_max_freq, lfd_max_index;
	int lfd_min_freq, lfd_min_index;

	vrr_mode = get_panel_refresh_mode(panel);
	if (vrr_mode < 0)
		return -EINVAL;

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	lfd_max_freq = s6e3hac_get_vrr_lfd_max_freq(panel_data);
	if (lfd_max_freq < 0) {
		panel_err("failed to get s6e3hac_get_vrr_lfd_max_freq\n");
		return -EINVAL;
	}

	lfd_min_freq = s6e3hac_get_vrr_lfd_min_freq(panel_data);
	if (lfd_min_freq < 0) {
		panel_err("failed to get s6e3hac_get_vrr_lfd_min_freq\n");
		return -EINVAL;
	}

	lfd_max_index = getidx_s6e3hac_lfd_frame_idx(lfd_max_freq, vrr_mode);
	if (lfd_max_index < 0) {
		panel_err("failed to get lfd_max_index(lfd_max_freq:%d vrr_mode:%d)\n",
				lfd_max_freq, vrr_mode);
		return -EINVAL;
	}

	lfd_min_index = getidx_s6e3hac_lfd_frame_idx(lfd_min_freq, vrr_mode);
	if (lfd_min_index < 0) {
		panel_err("failed to get lfd_min_index(lfd_min_freq:%d vrr_mode:%d)\n",
				lfd_min_freq, vrr_mode);
		return -EINVAL;
	}

	panel_dbg("lfd_max_freq %d%s(%d) lfd_min_freq %d%s(%d)\n",
			lfd_max_freq, vrr_mode == S6E3HAC_VRR_MODE_HS ? "HS" : "NM", lfd_max_index,
			lfd_min_freq, vrr_mode == S6E3HAC_VRR_MODE_HS ? "HS" : "NM", lfd_min_index);

	return maptbl_index(tbl, lfd_max_index, lfd_min_index, 0);
}

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static int s6e3hac_getidx_vddm_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("vddm %d\n", props->gct_vddm);

	return maptbl_index(tbl, 0, props->gct_vddm, 0);
}

static int s6e3hac_getidx_gram_img_pattern_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	panel_info("gram img %d\n", props->gct_pattern);
	props->gct_valid_chksum[0] = S6E3HAC_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[1] = S6E3HAC_GRAM_CHECKSUM_VALID_2;
	props->gct_valid_chksum[2] = S6E3HAC_GRAM_CHECKSUM_VALID_1;
	props->gct_valid_chksum[3] = S6E3HAC_GRAM_CHECKSUM_VALID_2;

	return maptbl_index(tbl, 0, props->gct_pattern, 0);
}
#endif

#ifdef CONFIG_SUPPORT_TDMB_TUNE
static int s6e3hac_getidx_tdmb_tune_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_properties *props = &panel->panel_data.props;

	return maptbl_index(tbl, 0, props->tdmb_on, 0);
}
#endif

#ifdef CONFIG_DYNAMIC_FREQ

static int getidx_dyn_ffc_table(struct maptbl *tbl)
{
	int row = 0, layer = 0;
	struct df_status_info *status;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	status = &panel->df_status;

	layer = status->current_ddi_osc;
	if (layer >= MAX_S6E3HAC_OSC) {
		panel_warn("osc out of range %d %d, set to %d\n", layer, MAX_S6E3HAC_OSC, S6E3HAC_OSC_DEFAULT);
		layer = status->current_ddi_osc = S6E3HAC_OSC_DEFAULT;
		status->request_ddi_osc = S6E3HAC_OSC_DEFAULT;
	}

	row = status->ffc_df;
	if (row >= S6E3HAC_MAX_MIPI_FREQ) {
		panel_warn("ffc out of range %d %d, set to %d\n", row,
			S6E3HAC_MAX_MIPI_FREQ, S6E3HAC_DEFAULT_MIPI_FREQ);
		row = status->ffc_df = S6E3HAC_DEFAULT_MIPI_FREQ;
	}

	panel_info("ffc idx: %d, ddi_osc: %d, row: %d\n",
			status->ffc_df, status->current_ddi_osc, row);

	return maptbl_index(tbl, layer, row, 0);
}

#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst)
{
}
#endif

static void copy_common_maptbl(struct maptbl *tbl, u8 *dst)
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

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * tbl->sz_copy);
	panel_dbg("copy from %s %d %d\n",
			tbl->name, idx, tbl->sz_copy);
	print_data(dst, tbl->sz_copy);
}

static void copy_tset_maptbl(struct maptbl *tbl, u8 *dst)
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
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
static void copy_copr_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct copr_info *copr;
	struct copr_reg_v5 *reg;
	int i;

	if (!tbl || !dst)
		return;

	copr = (struct copr_info *)tbl->pdata;
	if (unlikely(!copr))
		return;

	reg = &copr->props.reg.v5;

	dst[0] = (reg->copr_mask << 5) | (reg->cnt_re << 4) |
		(reg->copr_ilc << 3) | (reg->copr_gamma << 1) | reg->copr_en;
	dst[1] = ((reg->copr_er >> 8) & 0x3) << 4 |
		((reg->copr_eg >> 8) & 0x3) << 2 | ((reg->copr_eb >> 8) & 0x3);
	dst[2] = ((reg->copr_erc >> 8) & 0x3) << 4 |
		((reg->copr_egc >> 8) & 0x3) << 2 | ((reg->copr_ebc >> 8) & 0x3);
	dst[3] = reg->copr_er;
	dst[4] = reg->copr_eg;
	dst[5] = reg->copr_eb;
	dst[6] = reg->copr_erc;
	dst[7] = reg->copr_egc;
	dst[8] = reg->copr_ebc;
	dst[9] = (reg->max_cnt >> 8) & 0xFF;
	dst[10] = reg->max_cnt & 0xFF;
	dst[11] = reg->roi_on;
	for (i = 0; i < 5; i++) {
		dst[12 + i * 8] = (reg->roi[i].roi_xs >> 8) & 0x7;
		dst[13 + i * 8] = reg->roi[i].roi_xs & 0xFF;
		dst[14 + i * 8] = (reg->roi[i].roi_ys >> 8) & 0xF;
		dst[15 + i * 8] = reg->roi[i].roi_ys & 0xFF;
		dst[16 + i * 8] = (reg->roi[i].roi_xe >> 8) & 0x7;
		dst[17 + i * 8] = reg->roi[i].roi_xe & 0xFF;
		dst[18 + i * 8] = (reg->roi[i].roi_ye >> 8) & 0xF;
		dst[19 + i * 8] = reg->roi[i].roi_ye & 0xFF;
	}
	print_data(dst, 52);
}
#endif

static void copy_lfd_min_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct vrr_lfd_status *vrr_lfd_status;
	struct panel_vrr *vrr;
	int vrr_fps, vrr_mode;
	u32 vrr_div_count;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	if (vrr_div_count < MIN_S6E3HAC_FPS_DIV_COUNT ||
		vrr_div_count > MAX_S6E3HAC_FPS_DIV_COUNT) {
		panel_err("out of range vrr(%d %d) vrr_div_count(%d)\n",
				vrr_fps, vrr_mode, vrr_div_count);
		return;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	vrr_div_count = s6e3hac_get_vrr_lfd_min_div_count(panel_data);
	if (vrr_div_count <= 0) {
		panel_err("failed to get vrr(%d %d) div count\n",
				vrr_fps, vrr_mode);
		return;
	}

	/* update lfd_min status */
	vrr_lfd_status = &props->vrr_lfd_info.status[VRR_LFD_SCOPE_NORMAL];
	vrr_lfd_status->lfd_min_freq_div = vrr_div_count;
	vrr_lfd_status->lfd_min_freq =
		disp_div_round(vrr_fps, vrr_div_count);

	panel_dbg("vrr(%d %d) lfd(fix:%d scale:%d min:%d max:%d) --> lfd_min(1/%d)\n",
			vrr_fps, vrr_mode,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max, vrr_div_count);

	/* change modulation count to skip frame count */
	dst[0] = (u8)(vrr_div_count - MIN_VRR_DIV_COUNT);
}

static void copy_lfd_max_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_properties *props = &panel_data->props;
	struct vrr_lfd_config *vrr_lfd_config;
	struct vrr_lfd_status *vrr_lfd_status;
	struct panel_vrr *vrr;
	int vrr_fps, vrr_mode;
	u32 vrr_div_count;

	vrr = get_panel_vrr(panel);
	if (vrr == NULL)
		return;

	vrr_fps = vrr->fps;
	vrr_mode = vrr->mode;
	vrr_div_count = TE_SKIP_TO_DIV(vrr->te_sw_skip_count, vrr->te_hw_skip_count);
	if (vrr_div_count < MIN_S6E3HAC_FPS_DIV_COUNT ||
		vrr_div_count > MAX_S6E3HAC_FPS_DIV_COUNT) {
		panel_err("out of range vrr(%d %d) vrr_div_count(%d)\n",
				vrr_fps, vrr_mode, vrr_div_count);
		return;
	}

	vrr_lfd_config = &props->vrr_lfd_info.cur[VRR_LFD_SCOPE_NORMAL];
	vrr_div_count = s6e3hac_get_vrr_lfd_max_div_count(panel_data);
	if (vrr_div_count <= 0) {
		panel_err("failed to get vrr(%d %d) div count\n",
				vrr_fps, vrr_mode);
		return;
	}

	/* update lfd_max status */
	vrr_lfd_status = &props->vrr_lfd_info.status[VRR_LFD_SCOPE_NORMAL];
	vrr_lfd_status->lfd_max_freq_div = vrr_div_count;
	vrr_lfd_status->lfd_max_freq =
		disp_div_round(vrr_fps, vrr_div_count);

	panel_dbg("vrr(%d %d) lfd(fix:%d scale:%d min:%d max:%d) --> lfd_max(1/%d)\n",
			vrr_fps, vrr_mode,
			vrr_lfd_config->fix, vrr_lfd_config->scalability,
			vrr_lfd_config->min, vrr_lfd_config->max, vrr_div_count);

	/* change modulation count to skip frame count */
	dst[0] = (u8)(vrr_div_count - MIN_VRR_DIV_COUNT);
}

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int init_color_blind_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (S6E3HAC_SCR_CR_OFS + mdnie->props.sz_scr > sizeof_maptbl(tbl)) {
		panel_err("invalid size (maptbl_size %d, sz_scr %d)\n",
				sizeof_maptbl(tbl), mdnie->props.sz_scr);
		return -EINVAL;
	}

	memcpy(&tbl->arr[S6E3HAC_SCR_CR_OFS],
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
		panel_dbg("mdnie trans_mode off\n");
	return tbl->ncol * (mdnie->props.trans_mode);
}

static int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl)
{
	int mode = 0;
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	if (mdnie->props.mode != AUTO)
		mode = 1;

	return maptbl_index(tbl, mode, mdnie->props.night_level, 0);
}

static int init_mdnie_night_mode_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl *night_maptbl;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	night_maptbl = mdnie_find_etc_maptbl(mdnie, MDNIE_ETC_NIGHT_MAPTBL);
	if (!night_maptbl) {
		panel_err("NIGHT_MAPTBL not found\n");
		return -EINVAL;
	}

	if (sizeof_maptbl(tbl) < (S6E3HAC_NIGHT_MODE_OFS +
			sizeof_row(night_maptbl))) {
		panel_err("invalid size (maptbl_size %d, night_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(night_maptbl));
		return -EINVAL;
	}

	maptbl_copy(night_maptbl, &tbl->arr[S6E3HAC_NIGHT_MODE_OFS]);

	return 0;
}

static int init_mdnie_color_lens_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl *color_lens_maptbl;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	color_lens_maptbl = mdnie_find_etc_maptbl(mdnie, MDNIE_ETC_COLOR_LENS_MAPTBL);
	if (!color_lens_maptbl) {
		panel_err("COLOR_LENS_MAPTBL not found\n");
		return -EINVAL;
	}

	if (sizeof_maptbl(tbl) < (S6E3HAC_COLOR_LENS_OFS +
			sizeof_row(color_lens_maptbl))) {
		panel_err("invalid size (maptbl_size %d, color_lens_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(color_lens_maptbl));
		return -EINVAL;
	}

	if (IS_COLOR_LENS_MODE(mdnie))
		maptbl_copy(color_lens_maptbl, &tbl->arr[S6E3HAC_COLOR_LENS_OFS]);

	return 0;
}

static void update_current_scr_white(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata) {
		panel_err("invalid param\n");
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
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (sizeof_row(tbl) != ARRAY_SIZE(mdnie->props.coord_wrgb[0])) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
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
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (tbl->ncol != ARRAY_SIZE(mdnie->props.ssr_wrgb)) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
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
		panel_err("out of mode range %d\n", mdnie->props.mode);
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
		panel_err("out of mode range %d\n", mdnie->props.mode);
		return -EINVAL;
	}
	if ((mdnie->props.ldu < 0) || (mdnie->props.ldu >= MAX_LDU_MODE)) {
		panel_err("out of ldu mode range %d\n", mdnie->props.ldu);
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
		panel_err("out of color lens color range %d\n", mdnie->props.color_lens_color);
		return -EINVAL;
	}
	if ((mdnie->props.color_lens_level < 0) || (mdnie->props.color_lens_level >= COLOR_LENS_LEVEL_MAX)) {
		panel_err("out of color lens level range %d\n", mdnie->props.color_lens_level);
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
		panel_err("invalid index %d\n", idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		mdnie->props.def_wrgb[i] = tbl->arr[idx + i];
		value = mdnie->props.def_wrgb[i] +
			(char)((mdnie->props.mode == AUTO) ?
				mdnie->props.def_wrgb_ofs[i] : 0);
		mdnie->props.cur_wrgb[i] = value;
		dst[i * 2] = value;
		if (mdnie->props.mode == AUTO)
			panel_dbg("cur_wrgb[%d] %d(%02X) def_wrgb[%d] %d(%02X), def_wrgb_ofs[%d] %d\n",
					i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
					i, mdnie->props.def_wrgb[i], mdnie->props.def_wrgb[i],
					i, mdnie->props.def_wrgb_ofs[i]);
		else
			panel_dbg("cur_wrgb[%d] %d(%02X) def_wrgb[%d] %d(%02X), def_wrgb_ofs[%d] none\n",
					i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
					i, mdnie->props.def_wrgb[i], mdnie->props.def_wrgb[i], i);
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
		panel_err("invalid index %d\n", idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		mdnie->props.cur_wrgb[i] = tbl->arr[idx + i];
		dst[i * 2] = tbl->arr[idx + i];
		panel_dbg("cur_wrgb[%d] %d(%02X)\n",
				i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i]);
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
		panel_err("invalid index %d\n", idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		value = tbl->arr[idx + i] +
			(((mdnie->props.mode == AUTO) && (mdnie->props.scenario != EBOOK_MODE)) ?
				mdnie->props.def_wrgb_ofs[i] : 0);
		mdnie->props.cur_wrgb[i] = value;
		dst[i * 2] = value;
		panel_dbg("cur_wrgb[%d] %d(%02X) (orig:0x%02X offset:%d)\n",
				i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
				tbl->arr[idx + i], mdnie->props.def_wrgb_ofs[i]);
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
		panel_warn("out of range %d\n",
				mdnie->props.scr_white_mode);
		return -1;
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

#ifdef CONFIG_SUPPORT_AFC
static void copy_afc_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %p, dst %p)\n",
				tbl, dst);
		return;
	}
	mdnie = (struct mdnie_info *)tbl->pdata;
	memcpy(dst, mdnie->props.afc_roi,
			sizeof(mdnie->props.afc_roi));

	panel_dbg("afc_on %d\n", mdnie->props.afc_on);
	print_data(dst, sizeof(mdnie->props.afc_roi));
}
#endif
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */

static void show_rddpm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddpm[S6E3HAC_RDDPM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddpm;
#endif

	if (!res || ARRAY_SIZE(rddpm) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(rddpm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
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
	u8 rddsm[S6E3HAC_RDDSM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddsm;
#endif

	if (!res || ARRAY_SIZE(rddsm) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(rddsm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddsm resource\n");
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
	u8 err[S6E3HAC_ERR_LEN] = { 0, }, err_15_8, err_7_0;

	if (!res || ARRAY_SIZE(err) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(err, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy err resource\n");
		return;
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

	panel_info("* CRC Error Count : %d\n", err[2]);
	panel_info("* ECC1 Error Count : %d\n", err[3]);
	panel_info("* ECC2 Error Count : %d\n", err[4]);

	panel_info("==================================================\n");
}

static void show_err_fg(struct dumpinfo *info)
{
	int ret;
	u8 err_fg[S6E3HAC_ERR_FG_LEN] = { 0, };
	struct resinfo *res = info->res;

	if (!res || ARRAY_SIZE(err_fg) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(err_fg, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy err_fg resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [EEh:ERR_FG] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			err_fg[0], (err_fg[0] & 0x4C) ? "NG" : "GOOD");

	if (err_fg[0] & 0x04) {
		panel_info("* VLOUT3 Error\n");
		inc_dpui_u32_field(DPUI_KEY_PNVLO3E, 1);
	}

	if (err_fg[0] & 0x08) {
		panel_info("* ELVDD Error\n");
		inc_dpui_u32_field(DPUI_KEY_PNELVDE, 1);
	}

	if (err_fg[0] & 0x40) {
		panel_info("* VLIN1 Error\n");
		inc_dpui_u32_field(DPUI_KEY_PNVLI1E, 1);
	}

	panel_info("==================================================\n");
}

static void show_dsi_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 dsi_err[S6E3HAC_DSI_ERR_LEN] = { 0, };

	if (!res || ARRAY_SIZE(dsi_err) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(dsi_err, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy dsi_err resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [05h:DSIE_CNT] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			dsi_err[0], (dsi_err[0]) ? "NG" : "GOOD");
	if (dsi_err[0])
		panel_info("* DSI Error Count : %d\n", dsi_err[0]);
	panel_info("====================================================\n");

	inc_dpui_u32_field(DPUI_KEY_PNDSIE, dsi_err[0]);
}

static void show_self_diag(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 self_diag[S6E3HAC_SELF_DIAG_LEN] = { 0, };

	ret = resource_copy(self_diag, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy self_diag resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [0Fh:SELF_DIAG] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			self_diag[0], (self_diag[0] & 0x40) ? "GOOD" : "NG");
	if ((self_diag[0] & 0x80) == 0)
		panel_info("* OTP value is changed\n");
	if ((self_diag[0] & 0x40) == 0)
		panel_info("* Panel Boosting Error\n");

	panel_info("=====================================================\n");

	inc_dpui_u32_field(DPUI_KEY_PNSDRE, (self_diag[0] & 0x40) ? 0 : 1);
}

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void show_cmdlog(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 cmdlog[S6E3HAC_CMDLOG_LEN];

	memset(cmdlog, 0, sizeof(cmdlog));
	ret = resource_copy(cmdlog, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy cmdlog resource\n");
		return;
	}

	panel_info("dump:cmdlog\n");
	print_data(cmdlog, ARRAY_SIZE(cmdlog));
}
#endif

#ifdef CONFIG_SUPPORT_MAFPC

#define MAFPC_ENABLE 0x11

static void copy_mafpc_enable_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel = tbl->pdata;
	struct mafpc_device *mafpc = NULL;

	dst[0] = 0;

	if (panel->mafpc_sd == NULL) {
		panel_err("mafpc_sd is null\n");
		goto err_enable;
	}

	v4l2_subdev_call(panel->mafpc_sd, core, ioctl,
		V4L2_IOCTL_MAFPC_GET_INFO, NULL);

	mafpc = (struct mafpc_device *)v4l2_get_subdev_hostdata(panel->mafpc_sd);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc info\n");
		goto err_enable;
	}

	if (mafpc->enable) {
		if ((mafpc->written & MAFPC_UPDATED_FROM_SVC) &&
			(mafpc->written & MAFPC_UPDATED_TO_DEV)) {
			dst[0] = MAFPC_ENABLE;
		} else {
			panel_warn("enabled: %x, written: %x\n",
					mafpc->enable, mafpc->written);
		}

		panel_info("%x %x %x\n", dst[5], dst[6], dst[7]);

		if (mafpc->written)
			memcpy(&dst[MAFPC_CTRL_CMD_OFFSET], mafpc->ctrl_cmd, MAFPC_CTRL_CMD_SIZE);

		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4,
			dst, MAFPC_CTRL_CMD_SIZE + 1, false);
	}

err_enable:
	return;
}


#define S6E3HAC_MAFPC_SCALE_MAX	75


static int get_mafpc_scale_index(struct mafpc_device *mafpc, struct panel_device *panel)
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

static void copy_mafpc_scale_maptbl(struct maptbl *tbl, u8 *dst)
{
	int row = 0;
	int index = 0;

	struct mafpc_device *mafpc = NULL;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		goto err_scale;
	}

	v4l2_subdev_call(panel->mafpc_sd, core, ioctl,
		V4L2_IOCTL_MAFPC_GET_INFO, NULL);
	mafpc  = (struct mafpc_device *)v4l2_get_subdev_hostdata(panel->mafpc_sd);
	if (mafpc == NULL) {
		panel_err("failed to get mafpc info\n");
		goto err_scale;
	}

	if (!mafpc->scale_buf || !mafpc->scale_map_br_tbl)  {
		panel_err("mafpc img buf is null\n");
		goto err_scale;
	}

	index = get_mafpc_scale_index(mafpc, panel);
	if (index < 0) {
		panel_err("mfapc invalid scale index : %d\n", index);
		goto err_scale;
	}

	if (index >= S6E3HAC_MAFPC_SCALE_MAX)
		index = S6E3HAC_MAFPC_SCALE_MAX - 1;

	row = index * 3;
	memcpy(dst, mafpc->scale_buf + row, 3);

	panel_info("idx: %d, %x:%x:%x\n",
			index, dst[0], dst[1], dst[2]);

err_scale:
	return;
}


static void show_mafpc_log(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 mafpc[S6E3HAC_MAFPC_LEN] = {0, };

	if (!res || ARRAY_SIZE(mafpc) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(mafpc, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [87h:MAFPC_EN] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			mafpc[0], (mafpc[0] & 0x01) ? "ON" : "OFF");
	panel_info("====================================================\n");

}
static void show_mafpc_flash_log(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 mafpc_flash[S6E3HAC_MAFPC_FLASH_LEN] = {0, };

	if (!res || ARRAY_SIZE(mafpc_flash) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(mafpc_flash, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return;
	}

	panel_info("======= SHOW PANEL [FEh(0x09):MAFPC_FLASH] INFO =======\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			mafpc_flash[0], (mafpc_flash[0] & 0x02) ? "BYPASS" : "POC");
	panel_info("====================================================\n");
}
#endif

static void show_self_mask_crc(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 crc[S6E3HAC_SELF_MASK_CRC_LEN] = {0, };

	if (!res || ARRAY_SIZE(crc) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(crc, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to self mask crc resource\n");
		return;
	}

	panel_info("======= SHOW PANEL [7Fh:SELF_MASK_CRC] INFO =======\n");
	panel_info("* Reg Value : 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
			crc[0], crc[1], crc[2], crc[3]);
	panel_info("====================================================\n");
}

static int init_gamma_mode2_brt_table(struct maptbl *tbl)
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

static int getidx_gamma_mode2_brt_table(struct maptbl *tbl)
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

static int getidx_gamma_mode2_exit_lpm_maptbl(struct maptbl *tbl)
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

	row = get_brightness_pac_step_by_subdev_id(
			panel_bl, PANEL_BL_SUBDEV_TYPE_DISP, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}

static bool is_panel_state_not_lpm(struct panel_device *panel)
{
	if (panel->state.cur_state != PANEL_STATE_ALPM)
		return true;

	return false;
}

static bool is_brightdot_enabled(struct panel_device *panel)
{
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	if (panel->panel_data.props.brightdot_test_enable != 0)
		return true;
#endif
	return false;
}

static inline bool is_brightdot_disabled(struct panel_device *panel)
{
	return !is_brightdot_enabled(panel);
}

