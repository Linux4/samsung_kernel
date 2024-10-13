/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * file for Samsung EXYNOS DSC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <drm/drm_drv.h>
#include <drm/drm_mipi_dsi.h>

#include "exynos_drm_dsc.h"

/* CONSTANT */
#define TBL_IDX_MAX 	6
static const u16 exynos_drm_dsc_rc_model_size = 8192;
static const u8 exynos_drm_dsc_rc_edge_factor = 6;
static const u8 exynos_drm_dsc_rc_tgt_offset_hi = 3;
static const u8 exynos_drm_dsc_rc_tgt_offset_lo = 3;
static const u16 exynos_drm_dsc_initial_xmit_delay[] = {
	512, 512, 512, 341, 341, 341
};
static const u8 exynos_drm_dsc_first_line_bpg_offset[3][TBL_IDX_MAX] = {
	{ 12, 12, 12, 15, 15, 15 }, /* To Do for 1.0 */
	{ 12, 12, 12, 15, 15, 15 }, /* DSC 1.1 */
	{ 15, 15, 15, 15, 15, 15 }, /* DSC 1.2 */
};
static const u16 exynos_drm_dsc_initial_offset[] = {
	6144, 6144, 6144, 2048, 2048, 2048
};
static const u8 exynos_drm_dsc_min_qp[] = {
	3, 7, 11, 3, 7, 11
};
static const u8 exynos_drm_dsc_max_qp[] = {
	12, 16, 20, 12, 16, 20
};
static const u8 exynos_drm_rc_quant_incr_limit0[] = {
	11, 15, 19, 11, 15, 19
};
static const u8 exynos_drm_rc_quant_incr_limit1[] = {
	11, 15, 19, 11, 15, 19
};
static const u16 exynos_drm_dsc_rc_buf_thresh[DSC_NUM_BUF_RANGES - 1] = {
	896, 1792, 2688, 3584, 4480, 5376, 6272, 6720, 7168, 7616,
	7744, 7872, 8000, 8064
};

static const u8 exynos_drm_dsc_rc_range_param_minqp[][DSC_NUM_BUF_RANGES] = {
	/* DSC 1.0 : should fix */
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 9, 12},
	{0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16},
	{0, 4, 9, 9, 11, 11, 11, 11, 11, 11, 13, 13, 13, 15, 21},
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 7, 13},
	{0, 2, 3, 4, 6, 7, 7, 7, 7, 7, 9, 9, 9, 11, 17},
	{0, 4, 7, 8, 10, 11, 11, 11, 11, 11, 13, 13, 13, 15, 21},
	/* DSC 1.1 */
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 9, 12},
	{0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16},
	{0, 4, 9, 9, 11, 11, 11, 11, 11, 11, 13, 13, 13, 15, 21},
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 7, 13},
	{0, 2, 3, 4, 6, 7, 7, 7, 7, 7, 9, 9, 9, 11, 17},
	{0, 4, 7, 8, 10, 11, 11, 11, 11, 11, 13, 13, 13, 15, 21},
	/* DSC 1.2 */
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 9, 12},
	{0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9, 9, 9, 13, 16},
	{0, 4, 9, 9, 11, 11, 11, 11, 11, 11, 13, 13, 13, 17, 20},
	{0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 5, 7, 10},
	{0, 2, 3, 4, 6, 7, 7, 7, 7, 7, 9, 9, 9, 11, 14},
	{0, 4, 7, 8, 10, 11, 11, 11, 11, 11, 13, 13, 13, 15, 18},
};

static const u8 exynos_drm_dsc_rc_range_param_maxqp[][DSC_NUM_BUF_RANGES] = {
	/* DSC 1.0 : should fix */
	{4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 11, 12, 13, 13, 15},
	{4, 8, 9, 10, 11, 11, 11, 12, 13, 14, 15, 16, 17, 17, 19},
	{12, 12, 13, 14, 15, 15, 15, 16, 17, 18, 19, 20, 21, 21, 23},
	{2, 4, 5, 6, 7, 7, 7, 8, 9, 10, 11, 12, 13, 13, 15},
	{2, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 17, 19},
	{6, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 21, 23},
	/* DSC 1.1 */
	{4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 11, 12, 13, 13, 15},
	{4, 8, 9, 10, 11, 11, 11, 12, 13, 14, 15, 16, 17, 17, 19},
	{12, 12, 13, 14, 15, 15, 15, 16, 17, 18, 19, 20, 21, 21, 23},
	{2, 4, 5, 6, 7, 7, 7, 8, 9, 10, 11, 12, 13, 13, 15},
	{2, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 17, 19},
	{6, 9, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 21, 23},
	/* DSC 1.2 */
	{4, 4, 5, 6, 7, 7, 7, 8, 9, 10, 10, 11, 11, 12, 13},
	{8, 8, 9, 10, 11, 11, 11, 12, 13, 14, 14, 15, 15, 16, 17},
	{12, 12, 13, 14, 15, 15, 15, 16, 17, 18, 18, 19, 19, 20, 21},
	{2, 4, 5, 6, 7, 7, 7, 8, 8, 9, 9, 9, 9, 10, 11},
	{2, 5, 7, 8, 9, 10, 11, 12, 12, 13, 13, 13, 13, 14, 15},
	{6, 9, 11, 12, 13, 14, 15, 16, 16, 17, 17, 17, 17, 18, 19},
};
static const s8 exynos_drm_dsc_rc_range_param_offset[DSC_NUM_BUF_RANGES] = {
	2, 0, 0, -2, -4, -6, -8, -8, -8, -10, -10, -12, -12, -12, -12,
};

static u16 __exynos_dsc_ceil(u32 val, u32 den)
{
	if (den == 0)
		return 0;

	if (val % den)
		return (u16)((val / den) + 1);
	else
		return (u16)(val / den);
}

u8 exynos_drm_dsc_get_version(struct drm_dsc_config *cfg)
{
	return ((cfg->dsc_version_major << 4) | cfg->dsc_version_minor);
}

u8 exynos_drm_dsc_get_bpc(struct drm_dsc_config* cfg)
{
	return cfg->bits_per_component;
}

u8 exynos_drm_dsc_get_bpp(struct drm_dsc_config* cfg)
{
	/* 4 fractional bits */
	return EXYNOS_DSC_FRACTIONAL_BIT(cfg->bits_per_pixel, 4);
}

u8 exynos_drm_dsc_get_initial_scale_value(struct drm_dsc_config* cfg)
{
	/* 3 fractional bits */
	return EXYNOS_DSC_FRACTIONAL_BIT(cfg->initial_scale_value, 3);
}

u8 exynos_drm_dsc_get_nfl_bpg_offset(struct drm_dsc_config* cfg)
{
	/* 11 fractional bits */
	return EXYNOS_DSC_FRACTIONAL_BIT(cfg->nfl_bpg_offset, 11);
}

u8 exynos_drm_dsc_get_slice_bpg_offset(struct drm_dsc_config* cfg)
{
	/* 11 fractional bits */
	return EXYNOS_DSC_FRACTIONAL_BIT(cfg->slice_bpg_offset, 11);
}

u8 exynos_drm_dsc_get_rc_edge_factor(struct drm_dsc_config* cfg)
{
	/* 1 fractional bits */
	return EXYNOS_DSC_FRACTIONAL_BIT(cfg->rc_edge_factor, 1);
}

static void exynos_drm_dsc_compute_rc_buf_thresh(struct drm_dsc_config* cfg)
{
	int i;

	for (i = 0; i < DSC_NUM_BUF_RANGES - 1; ++i) {
		cfg->rc_buf_thresh[i] = (exynos_drm_dsc_rc_buf_thresh[i] >> 6);
	}

	return;
}

u8 exynos_drm_dsc_get_comp_config(struct drm_dsc_config* cfg)
{
	u8 data;

	data = (cfg->bits_per_pixel & DSC_PPS_BPP_HIGH_MASK) >> DSC_PPS_MSB_SHIFT;
	data |= (cfg->block_pred_enable << DSC_PPS_BLOCK_PRED_EN_SHIFT);
	data |= (cfg->convert_rgb << DSC_PPS_CONVERT_RGB_SHIFT);
	data |= (cfg->simple_422 << DSC_PPS_SIMPLE422_SHIFT);
	data |= (cfg->vbr_enable << DSC_PPS_VBR_EN_SHIFT);

	return (data & DSC_PPS_LSB_MASK);
}

u16 exynos_drm_dsc_get_rc_range_params(struct drm_dsc_config* cfg, int idx)
{
	u16 data;

	data = cfg->rc_range_params[idx].range_min_qp << DSC_PPS_RC_RANGE_MINQP_SHIFT;
	data |= cfg->rc_range_params[idx].range_max_qp << DSC_PPS_RC_RANGE_MAXQP_SHIFT;
	data |= (cfg->rc_range_params[idx].range_bpg_offset & DSC_RANGE_BPG_OFFSET_MASK);

	return data;
}

static void exynos_drm_dsc_print_pps_table(const u8 *pps)
{
	int i;
	pr_debug("==================== PPS TABLE ==================== \n");
	for (i = 0; i < EXYNOS_DSC_PPS_NUM; ++i) {
		pr_debug("[%02d] 0x%02x ",i , pps[i]);
		if ((i + 1) % 8 == 0)
			pr_debug("\n");
	}
}

void exynos_drm_dsc_print_pps_info(struct drm_dsc_config *cfg, const u8 *table)
{
	exynos_drm_dsc_print_pps_table(table);
	pr_info("==================== PPS INFORMATION ==================== \n");
	pr_info(" * dsc version : major 0x%1x, minor 0x%1x\n", cfg->dsc_version_major, cfg->dsc_version_minor);
	pr_info(" * bpc = %d, bpp = %d\n", exynos_drm_dsc_get_bpc(cfg), exynos_drm_dsc_get_bpp(cfg));
	pr_info(" * block_pred(%d) covert_rgb(%d) simple_422(%d) vbr_enable(%d)\n", cfg->block_pred_enable,
			cfg->convert_rgb, cfg->simple_422, cfg->vbr_enable);
	pr_info(" * native_422(%d) native_420(%d)\n", cfg->native_422, cfg->native_420);
	pr_info(" * picture W x H = %d x %d\n", cfg->pic_width, cfg->pic_height);
	pr_info(" * slice W x H = %d x %d\n", cfg->slice_width, cfg->slice_height);
	pr_info(" * initial xmit delay : %d , initial dec delay = %d\n", cfg->initial_xmit_delay, cfg->initial_dec_delay);
	pr_info("========================================================= \n");
}
EXPORT_SYMBOL(exynos_drm_dsc_print_pps_info);

int convert_pps_to_dsc_config(struct drm_dsc_config *cfg, const u8 *table)
{
	int i, idx;
	u16 data;

	if (!cfg || !table) {
		pr_err("input nullptr");
		return -EINVAL;
	} else
		memset(cfg, 0, sizeof(struct drm_dsc_config));

	/* PPS 0 */
	cfg->dsc_version_major = EXYNOS_DSC_NIBBLE_H(table[0]);
	cfg->dsc_version_minor = EXYNOS_DSC_NIBBLE_L(table[0]);
	/* PPS 1 */
	//cfg->pps_identifier = table[1];
	/* PPS 2 */
	//cfg->pps2_reserved = table[2];
	/* PPS 3 */
	cfg->bits_per_component = EXYNOS_DSC_NIBBLE_H(table[3]);
	if (cfg->bits_per_component == 0) {
		if (cfg->dsc_version_minor == 0x2)
			cfg->bits_per_component = 16;
		else
			pr_info("bpc converting error! bpc(%d) ver_minor(0x%01x)\n",
				cfg->bits_per_component, cfg->dsc_version_minor);
	}
	cfg->line_buf_depth = EXYNOS_DSC_NIBBLE_L(table[3]);
	if (cfg->dsc_version_minor == 0x2) {
		if (cfg->line_buf_depth == 0)
			cfg->line_buf_depth = 16;
	} else {
		if (cfg->line_buf_depth == 0xE || cfg->line_buf_depth == 0xF)
			pr_info("line_buf converting error! lbd(%d) ver_minor(0x%01x)\n",
				cfg->line_buf_depth, cfg->dsc_version_minor);
	}
	/* PPS 4 */
	cfg->block_pred_enable = EXYNOS_DSC_BITMASK_VAL(table[4], DSC_PPS_BLOCK_PRED_EN_SHIFT);
	cfg->convert_rgb = EXYNOS_DSC_BITMASK_VAL(table[4], DSC_PPS_CONVERT_RGB_SHIFT);
	cfg->simple_422 = EXYNOS_DSC_BITMASK_VAL(table[4], DSC_PPS_SIMPLE422_SHIFT);
	cfg->vbr_enable = EXYNOS_DSC_BITMASK_VAL(table[4], DSC_PPS_VBR_EN_SHIFT);
	/* PPS 4&5 & four fractional bits */
	cfg->bits_per_pixel = (((table[4] & 0x3) << DSC_PPS_MSB_SHIFT) | (table[5] & DSC_PPS_LSB_MASK));
	/* PPS 6 & 7 */
	cfg->pic_height = EXYNOS_DSC_BIG_ENDIAN(table[6], table[7]);
	/* PPS 8 & 9 */
	cfg->pic_width = EXYNOS_DSC_BIG_ENDIAN(table[8], table[9]);
	/* PPS 10 & 11 */
	cfg->slice_height = EXYNOS_DSC_BIG_ENDIAN(table[10], table[11]);
	/* PPS 12 & 13 */
	cfg->slice_width = EXYNOS_DSC_BIG_ENDIAN(table[12], table[13]);
	/* PPS 14 & 15 */
	cfg->slice_chunk_size = EXYNOS_DSC_BIG_ENDIAN(table[14], table[15]);
	/* PPS 16 & 17 */
	cfg->initial_xmit_delay = (((table[16] & 0x3) << DSC_PPS_MSB_SHIFT) | (table[17] & DSC_PPS_LSB_MASK));
	/* PPS 18 & 19 */
	cfg->initial_dec_delay = EXYNOS_DSC_BIG_ENDIAN(table[18], table[19]);
	/* PPS 20 */
	//cfg->pps20_reserved = (table[20] << 2) | ((table[21] >> 6) & 0x3);
	/* PPS 21 & three fractional bits */
	cfg->initial_scale_value = table[21] & 0x3f;
	/* PPS 22 & 23 */
	cfg->scale_increment_interval = EXYNOS_DSC_BIG_ENDIAN(table[22], table[23]);
	/* PPS 24 & 25 */
	cfg->scale_decrement_interval = (((table[24] & 0xf) << DSC_PPS_MSB_SHIFT) | (table[25] & DSC_PPS_LSB_MASK));
	/* PPS 26 */
	//cfg->pps26_reserved = table[26] << 3 | ((table[27] >> 5) & 0x7);
	/* PPS 27 */
	cfg->first_line_bpg_offset = table[27] & 0x1f;
	/* PPS 28 & 29, 11 fractional bits */
	cfg->nfl_bpg_offset = EXYNOS_DSC_BIG_ENDIAN(table[28], table[29]);
	/* PPS 30 & 31, 11 fractional bits */
	cfg->slice_bpg_offset = EXYNOS_DSC_BIG_ENDIAN(table[30], table[31]);
	/* PPS 32 & 33 */
	cfg->initial_offset = EXYNOS_DSC_BIG_ENDIAN(table[32], table[33]);
	/* PPS 34 & 35 */
	cfg->final_offset = EXYNOS_DSC_BIG_ENDIAN(table[34], table[35]);
	/* PPS 36 */
	cfg->flatness_min_qp = table[36] & 0x1f;
	/* PPS 37 */
	cfg->flatness_max_qp = table[37] & 0x1f;
	/* PPS 38 & 39 */
	cfg->rc_model_size = EXYNOS_DSC_BIG_ENDIAN(table[38], table[39]);
	/* PPS 40, 1 fractional bit */
	cfg->rc_edge_factor = table[40] & 0xf;
	/* PPS 41 */
	cfg->rc_quant_incr_limit0 = table[41] & 0x1f;
	/* PPS 42 */
	cfg->rc_quant_incr_limit1 = table[42] & 0x1f;
	/* PPS 43 */
	cfg->rc_tgt_offset_high = EXYNOS_DSC_NIBBLE_H(table[43]);
	cfg->rc_tgt_offset_low = EXYNOS_DSC_NIBBLE_L(table[43]);
	/* PPS 44 ~ 57, six 0s are appended to the lsb of each threshold value */
	for (i = 0; i < DSC_NUM_BUF_RANGES - 1; ++i)
		cfg->rc_buf_thresh[i] = (table[44 + i] << 6);
	/* PPS 58 ~ 87 */
	for (i = 0; i < DSC_NUM_BUF_RANGES; ++i) {
		idx = 58 + (i << 1);
		data = EXYNOS_DSC_BIG_ENDIAN(table[idx], table[idx + 1]);
		cfg->rc_range_params[i].range_min_qp = (data >> DSC_PPS_RC_RANGE_MINQP_SHIFT) & 0x1f;
		cfg->rc_range_params[i].range_max_qp = (data >> DSC_PPS_RC_RANGE_MAXQP_SHIFT) & 0x1f;
		cfg->rc_range_params[i].range_bpg_offset = data & 0x3f;
	}
	/* PPS 88 */
	cfg->native_420 = EXYNOS_DSC_BITMASK_VAL(table[88], DSC_PPS_NATIVE_420_SHIFT);
	cfg->native_422 = EXYNOS_DSC_BITMASK_VAL(table[88], 0);

#if defined(DEBUG_PPS)
	exynos_drm_dsc_print_pps_info(table);
#endif

	return 0;
}
EXPORT_SYMBOL(convert_pps_to_dsc_config);

int exynos_drm_dsc_fill_constant(struct drm_dsc_config *cfg, u8 bpc, u8 bpp)
{
	u8 tbl_idx = TBL_IDX_MAX;
	u8 tbl_shift = cfg->dsc_version_minor * 6;
	int i;

	if (bpp == 8) {
		if (bpc == 8)
			tbl_idx = 0;
		else if (bpc == 10)
			tbl_idx = 1;
		else
			tbl_idx = 2;
	} else if (bpp == 12) {
		if (bpc == 8)
			tbl_idx = 3;
		else if (bpc == 10)
			tbl_idx = 4;
		else
			tbl_idx = 5;
	}

	if (tbl_idx == TBL_IDX_MAX)
		return -EINVAL;

	/* PPS 16 & 17 */
	if (!cfg->initial_xmit_delay)
		cfg->initial_xmit_delay = exynos_drm_dsc_initial_xmit_delay[tbl_idx];

	cfg->first_line_bpg_offset
		= exynos_drm_dsc_first_line_bpg_offset[cfg->dsc_version_minor][tbl_idx];
	/* PPS 32 & 33 */
	cfg->initial_offset = exynos_drm_dsc_initial_offset[tbl_idx];
	/* PPS 36 */
	cfg->flatness_min_qp = exynos_drm_dsc_min_qp[tbl_idx];
	/* PPS 37 */
	cfg->flatness_max_qp = exynos_drm_dsc_max_qp[tbl_idx];
	/* PPS 38 & 39 */
	cfg->rc_model_size = exynos_drm_dsc_rc_model_size;
	/* PPS 40 */
	cfg->rc_edge_factor = (exynos_drm_dsc_rc_edge_factor << 1);
	/* PPS 41 */
	cfg->rc_quant_incr_limit0 = exynos_drm_rc_quant_incr_limit0[tbl_idx];
	/* PPS 42 */
	cfg->rc_quant_incr_limit1 = exynos_drm_rc_quant_incr_limit1[tbl_idx];
	/* PPS 43 */
	cfg->rc_tgt_offset_high = exynos_drm_dsc_rc_tgt_offset_hi;
	cfg->rc_tgt_offset_low = exynos_drm_dsc_rc_tgt_offset_lo;
	/* PPS 44 ~ 57 */
	exynos_drm_dsc_compute_rc_buf_thresh(cfg);

	tbl_shift += tbl_idx;
	/* PPS 58 ~ 87 */
	for (i = 0; i < DSC_NUM_BUF_RANGES; ++i) {
		cfg->rc_range_params[i].range_min_qp
			= exynos_drm_dsc_rc_range_param_minqp[tbl_shift][i];
		cfg->rc_range_params[i].range_max_qp
			= exynos_drm_dsc_rc_range_param_maxqp[tbl_shift][i];
		cfg->rc_range_params[i].range_bpg_offset
			= (u8)(exynos_drm_dsc_rc_range_param_offset[i] &
				DSC_RANGE_BPG_OFFSET_MASK);
	}

	return 0;
}
EXPORT_SYMBOL(exynos_drm_dsc_fill_constant);

void exynos_drm_dsc_fill_variant(struct drm_dsc_config* cfg, u8 bpc, u8 bpp)
{
	u16 pixels_per_group = 3, num_extra_mux_bits = NUM_EXTRA_MUX_BITS;
	u16 groups_per_line, groups_per_total, hdrdelay, min_rate_buffer_size;
	u16 slice_bits, final_scale;
	u32 tmp;

	/* PPS 14 & 15 */
	cfg->slice_chunk_size =
		__exynos_dsc_ceil((u32)bpp * (u32)cfg->slice_width, 8);
	slice_bits = (cfg->slice_chunk_size << 3) * cfg->slice_height;

	while ((slice_bits - num_extra_mux_bits) % 48)
		num_extra_mux_bits--;

	groups_per_line = __exynos_dsc_ceil((u32)cfg->slice_width, (u32)pixels_per_group);
	groups_per_total = groups_per_line * cfg->slice_height;

/*	min_rate_buffer_size = cfg->rc_model_size - cfg->initial_offset
 *		+ (cfg->initial_xmit_delay * bpp)
 *		+ groups_per_line * cfg->first_line_bpg_offset;
 */
	/* PPS 18 & 19 */
	tmp = (u32)cfg->initial_xmit_delay * (u32)bpp;
	tmp += ((u32)groups_per_line * (u32)cfg->first_line_bpg_offset);
	min_rate_buffer_size = cfg->rc_model_size - cfg->initial_offset + (u16)tmp;
	hdrdelay = __exynos_dsc_ceil((u32)min_rate_buffer_size, (u32)bpp);
	cfg->initial_dec_delay = hdrdelay - cfg->initial_xmit_delay;

	/* PPS21 & three fractional bits */
	cfg->initial_scale_value = (cfg->rc_model_size << 3) /
		(cfg->rc_model_size - cfg->initial_offset);

	/* PPS 34 & 35 */
	tmp = (u32)cfg->initial_xmit_delay * (8 << 4);
	tmp = (tmp + 8) >> 4;
	cfg->final_offset = cfg->rc_model_size - (u16)tmp + num_extra_mux_bits;
	final_scale = (cfg->rc_model_size << 3) / (cfg->rc_model_size - cfg->final_offset);

	/* PPS 28 & PPS 29, 11 fractional bits */
	cfg->nfl_bpg_offset =
		__exynos_dsc_ceil((u32)cfg->first_line_bpg_offset << 11, (u32)cfg->slice_height - 1);

	/* PPS 30 & PPS31, 11 fractional bits */
	tmp = cfg->rc_model_size - cfg->initial_offset + num_extra_mux_bits;
	cfg->slice_bpg_offset = __exynos_dsc_ceil(tmp << 11, (u32)groups_per_total);

	/* PPS 22 & 23 */
	tmp = (u32)(final_scale - 9) * (u32)(cfg->nfl_bpg_offset + cfg->slice_bpg_offset);
	tmp = ((u32)cfg->final_offset << 11) / tmp;
	cfg->scale_increment_interval = (u16)tmp;

	/* PPS 24 & 25 */
	cfg->scale_decrement_interval =
		groups_per_line / (cfg->initial_scale_value - 8);
}
EXPORT_SYMBOL(exynos_drm_dsc_fill_variant);

int exynos_drm_dsc_compute_config(struct drm_dsc_config *cfg)
{
	u8 bpc, bpp;

	/* PPS 3 */
	if (!cfg->bits_per_component)
		return -EINVAL;
	bpc = cfg->bits_per_component;
	cfg->line_buf_depth = cfg->bits_per_component + 1;

	/* PPS 4 & 5 */
	cfg->simple_422 = 0;
	cfg->vbr_enable = 0;
	cfg->bits_per_pixel = bpc << 4;
	bpp = cfg->bits_per_pixel >> 4;

	/* PPS 6 & 7 & 8 & 9 */
	if (!cfg->pic_height || !cfg->pic_width)
		return -EINVAL;

	/* PPS 10 & 11 */
	if (!cfg->slice_height)
		return -EINVAL;

	/* PPS 12 & 13 */
	if (!cfg->slice_count)
		return -EINVAL;

	cfg->slice_width = cfg->pic_width / cfg->slice_count;

	exynos_drm_dsc_fill_constant(cfg, bpc, bpp);

	exynos_drm_dsc_fill_variant(cfg, bpc, bpp);

	return 0;
}
EXPORT_SYMBOL(exynos_drm_dsc_compute_config);
