/*
 * linux/drivers/video/fbdev/exynos/panel/ana6710/ana6710_m35x_panel_aod_dimming.h
 *
 * Header file for ANA6710 Dimming Driver
 *
 * Copyright (c) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA6710_M35X_PANEL_AOD_DIMMING_H__
#define __ANA6710_M35X_PANEL_AOD_DIMMING_H__
#include "../dimming.h"
#include "../panel_dimming.h"
#include "ana6710_dimming.h"

/*
 * PANEL INFORMATION
 * LDI : ANA6710
 * PANEL : PRE
 */
static unsigned int m35x_aod_brt_tbl[ANA6710_M35X_AOD_NR_LUMINANCE] = {
	BRT_LT(12), BRT_LT(29), BRT_LT(50), BRT(255),
};

static unsigned int m35x_aod_lum_tbl[ANA6710_M35X_AOD_NR_LUMINANCE] = {
	2, 10, 30, 60,
};

static struct brightness_table ana6710_m35x_panel_aod_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = m35x_aod_brt_tbl,
	.sz_brt = ARRAY_SIZE(m35x_aod_brt_tbl),
	.sz_ui_brt = ARRAY_SIZE(m35x_aod_brt_tbl),
	.sz_hbm_brt = 0,
	.lum = m35x_aod_lum_tbl,
	.sz_lum = ARRAY_SIZE(m35x_aod_lum_tbl),
	.brt_to_step = m35x_aod_brt_tbl,
	.sz_brt_to_step = ARRAY_SIZE(m35x_aod_brt_tbl),
};

static struct panel_dimming_info ana6710_m35x_panel_aod_dimming_info = {
	.name = "ana6710_m35x_aod",
	.target_luminance = ANA6710_M35X_AOD_TARGET_LUMINANCE,
	.nr_luminance = ANA6710_M35X_AOD_NR_LUMINANCE,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = 0,
	.brt_tbl = &ana6710_m35x_panel_aod_brightness_table,
};
#endif /* __ANA6710_PANEL_AOD_DIMMING_H__ */
