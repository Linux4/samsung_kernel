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

u8 exynos_drm_dsc_get_bpc(struct exynos_drm_pps* pps)
{
	return pps->bits_per_component;
}

u8 exynos_drm_dsc_get_bpp(struct exynos_drm_pps* pps)
{
	/* 4 fractional bits */
	return EXYNOS_DSC_FRACTIONAL_BIT(pps->bits_per_pixel, 4);
}

u8 exynos_drm_dsc_get_initial_scale_value(struct exynos_drm_pps* pps)
{
	/* 3 fractional bits */
	return EXYNOS_DSC_FRACTIONAL_BIT(pps->initial_scale_value, 3);
}

u8 exynos_drm_dsc_get_nfl_bpg_offset(struct exynos_drm_pps* pps)
{
	/* 11 fractional bits */
	return EXYNOS_DSC_FRACTIONAL_BIT(pps->nfl_bpg_offset, 11);
}

u8 exynos_drm_dsc_get_slice_bpg_offset(struct exynos_drm_pps* pps)
{
	/* 11 fractional bits */
	return EXYNOS_DSC_FRACTIONAL_BIT(pps->slice_bpg_offset, 11);
}

u8 exynos_drm_dsc_get_rc_edge_factor(struct exynos_drm_pps* pps)
{
	/* 1 fractional bits */
	return EXYNOS_DSC_FRACTIONAL_BIT(pps->rc_edge_factor, 1);
}

u8 exynos_drm_dsc_get_comp_config(struct exynos_drm_pps* pps)
{
	u8 data;

	data = (pps->bits_per_pixel & DSC_PPS_BPP_HIGH_MASK) >> DSC_PPS_MSB_SHIFT;
	data |= (pps->block_pred_enable << DSC_PPS_BLOCK_PRED_EN_SHIFT);
	data |= (pps->convert_rgb << DSC_PPS_CONVERT_RGB_SHIFT);
	data |= (pps->simple_422 << DSC_PPS_SIMPLE422_SHIFT);
	data |= (pps->vbr_enable << DSC_PPS_VBR_EN_SHIFT);

	return (data & DSC_PPS_LSB_MASK);
}

u16 exynos_drm_dsc_get_rc_range_params(struct exynos_drm_pps* pps, int idx)
{
	u16 data;

	data = pps->rc_range_parameters[idx].range_min_qp << DSC_PPS_RC_RANGE_MINQP_SHIFT;
	data |= pps->rc_range_parameters[idx].range_max_qp << DSC_PPS_RC_RANGE_MAXQP_SHIFT;
	data |= (pps->rc_range_parameters[idx].range_bpg_offset & DSC_RANGE_BPG_OFFSET_MASK);

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

void exynos_drm_dsc_print_pps_info(struct exynos_drm_pps *pps, const u8 *table)
{
	exynos_drm_dsc_print_pps_table(table);
	pr_info("==================== PPS INFORMATION ==================== \n");
	pr_info(" * dsc version : major 0x%1x, minor 0x%1x\n", pps->dsc_version_major, pps->dsc_version_minor);
	pr_info(" * bpc = %d, bpp = %d\n", exynos_drm_dsc_get_bpc(pps), exynos_drm_dsc_get_bpp(pps));
	pr_info(" * block_pred(%d) covert_rgb(%d) simple_422(%d) vbr_enable(%d)\n", pps->block_pred_enable,
			pps->convert_rgb, pps->simple_422, pps->vbr_enable);
	pr_info(" * native_422(%d) native_420(%d)\n", pps->native_422, pps->native_420);
	pr_info(" * picture W x H = %d x %d\n", pps->pic_width, pps->pic_height);
	pr_info(" * slice W x H = %d x %d\n", pps->slice_width, pps->slice_height);
	pr_info(" * initial xmit delay : %d , initial dec delay = %d\n", pps->initial_xmit_delay, pps->initial_dec_delay);
	pr_info("========================================================= \n");
}
EXPORT_SYMBOL(exynos_drm_dsc_print_pps_info);

int convert_pps_to_exynos_pps(struct exynos_drm_pps *pps, const u8 *table)
{
	int i, idx;
	u16 data;

	if (!pps || !table) {
		pr_err("input nullptr");
		return -EINVAL;
	} else
		memset(pps, 0, sizeof(struct exynos_drm_pps));

	/* PPS 0 */
	pps->dsc_version_major = EXYNOS_DSC_NIBBLE_H(table[0]);
	pps->dsc_version_minor = EXYNOS_DSC_NIBBLE_L(table[0]);
	/* PPS 1 */
	pps->pps_identifier = table[1];
	/* PPS 2 */
	//pps->pps2_reserved = table[2];
	/* PPS 3 */
	pps->bits_per_component = EXYNOS_DSC_NIBBLE_H(table[3]);
	if (pps->bits_per_component == 0) {
		if (pps->dsc_version_minor == 0x2)
			pps->bits_per_component = 16;
		else
			pr_info("bpc converting error! bpc(%d) ver_minor(0x%01x)\n",
				pps->bits_per_component, pps->dsc_version_minor);
	}
	pps->linebuf_depth = EXYNOS_DSC_NIBBLE_L(table[3]);
	if (pps->dsc_version_minor == 0x2) {
		if (pps->linebuf_depth == 0)
			pps->linebuf_depth = 16;
	} else {
		if (pps->linebuf_depth == 0xE || pps->linebuf_depth == 0xF)
			pr_info("linebuf converting error! lbd(%d) ver_minor(0x%01x)\n",
				pps->linebuf_depth, pps->dsc_version_minor);
	}
	/* PPS 4 */
	pps->block_pred_enable = EXYNOS_DSC_BITMASK_VAL(table[4], DSC_PPS_BLOCK_PRED_EN_SHIFT);
	pps->convert_rgb = EXYNOS_DSC_BITMASK_VAL(table[4], DSC_PPS_CONVERT_RGB_SHIFT);
	pps->simple_422 = EXYNOS_DSC_BITMASK_VAL(table[4], DSC_PPS_SIMPLE422_SHIFT);
	pps->vbr_enable = EXYNOS_DSC_BITMASK_VAL(table[4], DSC_PPS_VBR_EN_SHIFT);
	/* PPS 4&5 & four fractional bits */
	pps->bits_per_pixel = (((table[4] & 0x3) << DSC_PPS_MSB_SHIFT) | (table[5] & DSC_PPS_LSB_MASK));
	/* PPS 6 & 7 */
	pps->pic_height = EXYNOS_DSC_BIG_ENDIAN(table[6], table[7]);
	/* PPS 8 & 9 */
	pps->pic_width = EXYNOS_DSC_BIG_ENDIAN(table[8], table[9]);
	/* PPS 10 & 11 */
	pps->slice_height = EXYNOS_DSC_BIG_ENDIAN(table[10], table[11]);
	/* PPS 12 & 13 */
	pps->slice_width = EXYNOS_DSC_BIG_ENDIAN(table[12], table[13]);
	/* PPS 14 & 15 */
	pps->chunk_size = EXYNOS_DSC_BIG_ENDIAN(table[14], table[15]);
	/* PPS 16 & 17 */
	pps->initial_xmit_delay = (((table[16] & 0x3) << DSC_PPS_MSB_SHIFT) | (table[17] & DSC_PPS_LSB_MASK));
	/* PPS 18 & 19 */
	pps->initial_dec_delay = EXYNOS_DSC_BIG_ENDIAN(table[18], table[19]);
	/* PPS 20 */
	//pps->pps20_reserved = (table[20] << 2) | ((table[21] >> 6) & 0x3);
	/* PPS 21 & three fractional bits */
	pps->initial_scale_value = table[21] & 0x3f;
	/* PPS 22 & 23 */
	pps->scale_increment_interval = EXYNOS_DSC_BIG_ENDIAN(table[22], table[23]);
	/* PPS 24 & 25 */
	pps->scale_decrement_interval = (((table[24] & 0xf) << DSC_PPS_MSB_SHIFT) | (table[25] & DSC_PPS_LSB_MASK));
	/* PPS 26 */
	//pps->pps26_reserved = table[26] << 3 | ((table[27] >> 5) & 0x7);
	/* PPS 27 */
	pps->first_line_bpg_offset = table[27] & 0x1f;
	/* PPS 28 & 29, 11 fractional bits */
	pps->nfl_bpg_offset = EXYNOS_DSC_BIG_ENDIAN(table[28], table[29]);
	/* PPS 30 & 31, 11 fractional bits */
	pps->slice_bpg_offset = EXYNOS_DSC_BIG_ENDIAN(table[30], table[31]);
	/* PPS 32 & 33 */
	pps->initial_offset = EXYNOS_DSC_BIG_ENDIAN(table[32], table[33]);
	/* PPS 34 & 35 */
	pps->final_offset = EXYNOS_DSC_BIG_ENDIAN(table[34], table[35]);
	/* PPS 36 */
	pps->flatness_min_qp = table[36] & 0x1f;
	/* PPS 37 */
	pps->flatness_max_qp = table[37] & 0x1f;
	/* PPS 38 & 39 */
	pps->rc_model_size = EXYNOS_DSC_BIG_ENDIAN(table[38], table[39]);
	/* PPS 40, 1 fractional bit */
	pps->rc_edge_factor = table[40] & 0xf;
	/* PPS 41 */
	pps->rc_quant_incr_limit0 = table[41] & 0x1f;
	/* PPS 42 */
	pps->rc_quant_incr_limit1 = table[42] & 0x1f;
	/* PPS 43 */
	pps->rc_tgt_offset_hi = EXYNOS_DSC_NIBBLE_H(table[43]);
	pps->rc_tgt_offset_lo = EXYNOS_DSC_NIBBLE_L(table[43]);
	/* PPS 44 ~ 57, six 0s are appended to the lsb of each threshold value */
	for (i = 0; i < DSC_NUM_BUF_RANGES - 1; ++i)
		pps->rc_buf_thresh[i] = (table[44 + i] & DSC_PPS_LSB_MASK) << 6;
	/* PPS 58 ~ 87 */
	for (i = 0; i < DSC_NUM_BUF_RANGES; ++i) {
		idx = 58 + (i << 1);
		data = EXYNOS_DSC_BIG_ENDIAN(table[idx], table[idx + 1]);
		pps->rc_range_parameters[i].range_min_qp = (data >> DSC_PPS_RC_RANGE_MINQP_SHIFT) & 0x1f;
		pps->rc_range_parameters[i].range_max_qp = (data >> DSC_PPS_RC_RANGE_MAXQP_SHIFT) & 0x1f;
		pps->rc_range_parameters[i].range_bpg_offset = data & 0x3f;
	}
	/* PPS 88 */
	pps->native_420 = EXYNOS_DSC_BITMASK_VAL(table[88], DSC_PPS_NATIVE_420_SHIFT);
	pps->native_422 = EXYNOS_DSC_BITMASK_VAL(table[88], 0);

#if defined(DEBUG_PPS)
	exynos_drm_dsc_print_pps_info(table);
#endif

	return 0;
}
EXPORT_SYMBOL(convert_pps_to_exynos_pps);
