/*
 * linux/drivers/video/fbdev/exynos/panel/ea8079/ea8079_r8s_panel_aod_dimming.h
 *
 * Header file for EA8079 Dimming Driver
 *
 * Copyright (c) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8079_PANEL_AOD_DIMMING_H__
#define __EA8079_PANEL_AOD_DIMMING_H__
#include "../dimming.h"
#include "../panel_dimming.h"
#include "ea8079_dimming.h"

/*
 * PANEL INFORMATION
 * LDI : EA8079
 * PANEL : PRE
 */
static unsigned int r8s_aod_brt_tbl[EA8079_R8S_AOD_NR_LUMINANCE] = {
	BRT_LT(13), BRT_LT(32), BRT_LT(55), BRT(255),
};

static unsigned int r8s_aod_lum_tbl[EA8079_R8S_AOD_NR_LUMINANCE] = {
	2, 10, 30, 60,
};

static struct brightness_table ea8079_r8s_panel_aod_brightness_table = {
	.brt = r8s_aod_brt_tbl,
	.sz_brt = ARRAY_SIZE(r8s_aod_brt_tbl),
	.lum = r8s_aod_lum_tbl,
	.sz_lum = ARRAY_SIZE(r8s_aod_lum_tbl),
	.brt_to_step = r8s_aod_brt_tbl,
	.sz_brt_to_step = ARRAY_SIZE(r8s_aod_brt_tbl),
};

static struct panel_dimming_info ea8079_r8s_panel_aod_dimming_info = {
	.name = "ea8079_r8s_aod",
	.target_luminance = EA8079_R8S_AOD_TARGET_LUMINANCE,
	.nr_luminance = EA8079_R8S_AOD_NR_LUMINANCE,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = 0,
	.brt_tbl = &ea8079_r8s_panel_aod_brightness_table,
};
#endif /* __EA8079_PANEL_AOD_DIMMING_H__ */
