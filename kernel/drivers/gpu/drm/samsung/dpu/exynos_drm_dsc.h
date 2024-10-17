/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for exynos_drm_dsc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_DSC_H__
#define __EXYNOS_DRM_DSC_H__

#include <linux/printk.h>
#include <linux/bits.h>
#include <linux/types.h>
#include <drm/drm_crtc.h>
#include <drm/display/drm_dsc_helper.h>

#define DSC_NUM_PPS 128
#define EXYNOS_DSC_PPS_NUM 88
#define NUM_EXTRA_MUX_BITS 246
#define EXYNOS_DSC_NIBBLE_H(v) ((v >> 4) & 0xf) /* Most */
#define EXYNOS_DSC_NIBBLE_L(v) (v & 0xf)
#define EXYNOS_DSC_BIT_MASK(v, s) (v & BIT(s))
#define EXYNOS_DSC_BITMASK_VAL(v, s) (EXYNOS_DSC_BIT_MASK(v, s) >> s)
#define EXYNOS_DSC_BIG_ENDIAN(v1, v2) ((v1 << DSC_PPS_MSB_SHIFT) | v2)
#define EXYNOS_DSC_FRACTIONAL_BIT(v, f) (v >> f)
#define EXYNOS_DSC_TO_MSB(v) ((v >> DSC_PPS_MSB_SHIFT) & DSC_PPS_LSB_MASK)
#define EXYNOS_DSC_TO_LSB(v) (v & DSC_PPS_LSB_MASK)

/*
 * const value defined in spec.
 */
#define DEFAULT_DSC_VERSION	0x11
#define DEFAULT_DSC_BPC		8
#define DEFAULT_BLOCK_PRED_EN	1
#define DEFAULT_CONVERT_RGB	1

u8 exynos_drm_dsc_get_version(struct drm_dsc_config *cfg);
u8 exynos_drm_dsc_get_bpc(struct drm_dsc_config* cfg);
u8 exynos_drm_dsc_get_bpp(struct drm_dsc_config* cfg);
u8 exynos_drm_dsc_get_initial_scale_value(struct drm_dsc_config* cfg);
u8 exynos_drm_dsc_get_nfl_bpg_offset(struct drm_dsc_config* cfg);
u8 exynos_drm_dsc_get_slice_bpg_offset(struct drm_dsc_config* cfg);
u8 exynos_drm_dsc_get_comp_config(struct drm_dsc_config* cfg);
u16 exynos_drm_dsc_get_rc_range_params(struct drm_dsc_config* cfg, int idx);
int convert_pps_to_dsc_config(struct drm_dsc_config *cfg, const u8 *table);
void exynos_drm_dsc_print_pps_info(struct drm_dsc_config *cfg, const u8* table);
int exynos_drm_dsc_fill_constant(struct drm_dsc_config* cfg, u8 bpc, u8 bpp);
void exynos_drm_dsc_fill_variant(struct drm_dsc_config* cfg, u8 bpc, u8 bpp);
int exynos_drm_dsc_compute_config(struct drm_dsc_config* cfg);

/* compute helper func */

#endif /* __EXYNOS_DRM_DSC_H__ */
