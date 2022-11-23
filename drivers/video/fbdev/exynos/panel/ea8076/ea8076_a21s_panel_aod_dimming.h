/*
 * linux/drivers/video/fbdev/exynos/panel/ea8076/ea8076_a21s_panel_aod_dimming.h
 *
 * Header file for EA8076 Dimming Driver
 *
 * Copyright (c) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8076_A21S_PANEL_AOD_DIMMING_H__
#define __EA8076_A21S_PANEL_AOD_DIMMING_H__
#include "../dimming.h"
#include "../panel_dimming.h"

/*
 * PANEL INFORMATION
 * LDI : EA8076
 * PANEL : a21s
 */
#define EA8076_AOD_NR_LUMINANCE (4)
#define EA8076_AOD_TARGET_LUMINANCE (420)

static unsigned int a21s_aod_brt_tbl[EA8076_AOD_NR_LUMINANCE] = {
	BRT_LT(40), BRT_LT(71), BRT_LT(94), BRT(255),
};

static unsigned int a21s_aod_lum_tbl[EA8076_AOD_NR_LUMINANCE] = {
	2, 10, 30, 60,
};

static struct brightness_table ea8076_a21s_panel_aod_brightness_table = {
	.brt = a21s_aod_brt_tbl,
	.sz_brt = ARRAY_SIZE(a21s_aod_brt_tbl),
	.lum = a21s_aod_lum_tbl,
	.sz_lum = ARRAY_SIZE(a21s_aod_lum_tbl),
	.brt_to_step = a21s_aod_brt_tbl,
	.sz_brt_to_step = ARRAY_SIZE(a21s_aod_brt_tbl),
 };

static struct panel_dimming_info ea8076_a21s_panel_aod_dimming_info = {
	.name = "ea8076_a21s_aod",
	.target_luminance = EA8076_AOD_TARGET_LUMINANCE,
	.nr_luminance = EA8076_AOD_NR_LUMINANCE,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = 0,
	.brt_tbl = &ea8076_a21s_panel_aod_brightness_table,
};
#endif /* __EA8076_A21S_PANEL_AOD_DIMMING_H__ */
