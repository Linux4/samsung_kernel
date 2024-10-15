/*
 * linux/drivers/video/fbdev/exynos/panel/ana6710/ana6710_r12s_panel_aod_dimming.h
 *
 * Header file for ANA6710 Dimming Driver
 *
 * Copyright (c) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA6710_R12S_PANEL_AOD_DIMMING_H__
#define __ANA6710_R12S_PANEL_AOD_DIMMING_H__
#include "../dimming.h"
#include "../panel_dimming.h"
#include "ana6710_dimming.h"

/*
 * PANEL INFORMATION
 * LDI : ANA6710
 * PANEL : PRE
 */
static unsigned int r12s_aod_brt_tbl[ANA6710_R12S_AOD_NR_LUMINANCE] = {
	BRT_LT(11), BRT_LT(28), BRT_LT(49), BRT(255),
};

static unsigned int r12s_aod_lum_tbl[ANA6710_R12S_AOD_NR_LUMINANCE] = {
	2, 10, 30, 60,
};

static struct brightness_table ana6710_r12s_panel_aod_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = r12s_aod_brt_tbl,
	.sz_brt = ARRAY_SIZE(r12s_aod_brt_tbl),
	.sz_ui_brt = ARRAY_SIZE(r12s_aod_brt_tbl),
	.sz_hbm_brt = 0,
	.lum = r12s_aod_lum_tbl,
	.sz_lum = ARRAY_SIZE(r12s_aod_lum_tbl),
	.brt_to_step = r12s_aod_brt_tbl,
	.sz_brt_to_step = ARRAY_SIZE(r12s_aod_brt_tbl),
};

static struct panel_dimming_info ana6710_r12s_panel_aod_dimming_info = {
	.name = "ana6710_r12s_aod",
	.target_luminance = ANA6710_R12S_AOD_TARGET_LUMINANCE,
	.nr_luminance = ANA6710_R12S_AOD_NR_LUMINANCE,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = 0,
	.brt_tbl = &ana6710_r12s_panel_aod_brightness_table,
};
#endif /* __ANA6710_PANEL_AOD_DIMMING_H__ */
