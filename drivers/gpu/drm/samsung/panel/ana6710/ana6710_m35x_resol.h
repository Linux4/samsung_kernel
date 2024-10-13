/*
 * linux/drivers/video/fbdev/exynos/panel/ana6710/ana6710_m35x_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA6710_M35X_RESOL_H__
#define __ANA6710_M35X_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "ana6710.h"
#include "ana6710_dimming.h"

enum {
	ANA6710_M35X_DISPLAY_MODE_1080x2340_120HS,
	ANA6710_M35X_DISPLAY_MODE_1080x2340_60HS_120HS_TE_HW_SKIP_1,
	ANA6710_M35X_DISPLAY_MODE_1080x2340_60HS,
	ANA6710_M35X_DISPLAY_MODE_1080x2340_30NS,
	MAX_ANA6710_M35X_DISPLAY_MODE,
};

struct panel_vrr ana6710_m35x_default_panel_vrr[] = {
	[ANA6710_VRR_120HS] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[ANA6710_VRR_60HS_120HS_TE_HW_SKIP_1] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[ANA6710_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[ANA6710_VRR_30NS] = {
		.fps = 30,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
};

static struct panel_vrr *ana6710_m35x_default_vrrtbl[] = {
	&ana6710_m35x_default_panel_vrr[ANA6710_VRR_120HS],
	&ana6710_m35x_default_panel_vrr[ANA6710_VRR_60HS_120HS_TE_HW_SKIP_1],
	&ana6710_m35x_default_panel_vrr[ANA6710_VRR_60HS],
	&ana6710_m35x_default_panel_vrr[ANA6710_VRR_30NS],
};

static struct panel_resol ana6710_m35x_default_resol[] = {
	[ANA6710_RESOL_1080x2340] = {
		.w = 1080,
		.h = 2340,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 40,
			},
		},
		.available_vrr = ana6710_m35x_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(ana6710_m35x_default_vrrtbl),
	},
};

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode ana6710_m35x_display_mode[] = {
	/* FHD */
	[ANA6710_M35X_DISPLAY_MODE_1080x2340_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_120HS,
		.resol = &ana6710_m35x_default_resol[ANA6710_RESOL_1080x2340],
		.vrr = &ana6710_m35x_default_panel_vrr[ANA6710_VRR_120HS],
	},
	[ANA6710_M35X_DISPLAY_MODE_1080x2340_60HS_120HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_60HS_120HS_TE_SW_SKIP_1,
		.resol = &ana6710_m35x_default_resol[ANA6710_RESOL_1080x2340],
		.vrr = &ana6710_m35x_default_panel_vrr[ANA6710_VRR_60HS_120HS_TE_HW_SKIP_1],
	},
	[ANA6710_M35X_DISPLAY_MODE_1080x2340_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_60HS,
		.resol = &ana6710_m35x_default_resol[ANA6710_RESOL_1080x2340],
		.vrr = &ana6710_m35x_default_panel_vrr[ANA6710_VRR_60HS],
	},
	[ANA6710_M35X_DISPLAY_MODE_1080x2340_30NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_30NS,
		.resol = &ana6710_m35x_default_resol[ANA6710_RESOL_1080x2340],
		.vrr = &ana6710_m35x_default_panel_vrr[ANA6710_VRR_30NS],
	},
};

static struct common_panel_display_mode *ana6710_m35x_display_mode_array[] = {
	[ANA6710_M35X_DISPLAY_MODE_1080x2340_120HS] = &ana6710_m35x_display_mode[ANA6710_M35X_DISPLAY_MODE_1080x2340_120HS],
	[ANA6710_M35X_DISPLAY_MODE_1080x2340_60HS_120HS_TE_HW_SKIP_1] = &ana6710_m35x_display_mode[ANA6710_M35X_DISPLAY_MODE_1080x2340_60HS_120HS_TE_HW_SKIP_1],
	[ANA6710_M35X_DISPLAY_MODE_1080x2340_60HS] = &ana6710_m35x_display_mode[ANA6710_M35X_DISPLAY_MODE_1080x2340_60HS],
	[ANA6710_M35X_DISPLAY_MODE_1080x2340_30NS] = &ana6710_m35x_display_mode[ANA6710_M35X_DISPLAY_MODE_1080x2340_30NS],
};

static struct common_panel_display_modes ana6710_m35x_display_modes = {
	.num_modes = ARRAY_SIZE(ana6710_m35x_display_mode),
	.modes = (struct common_panel_display_mode **)&ana6710_m35x_display_mode_array,
};
#endif /* CONFIG_USDM_PANEL_DISPLAY_MODE */
#endif /* __ANA6710_M35X_RESOL_H__ */
