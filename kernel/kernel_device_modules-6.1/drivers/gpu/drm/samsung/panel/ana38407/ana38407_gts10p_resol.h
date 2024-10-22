/*
 * linux/drivers/video/fbdev/exynos/panel/ana38407/ana38407_gts10p_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA38407_GTS10P_RESOL_H__
#define __ANA38407_GTS10P_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "ana38407.h"
#include "ana38407_dimming.h"

enum {
	ANA38407_GTS10P_DISPLAY_MODE_2800x1752_120HS,
	ANA38407_GTS10P_DISPLAY_MODE_2800x1752_60HS_120HS_TE_HW_SKIP_1,
	ANA38407_GTS10P_DISPLAY_MODE_2800x1752_60HS,
	ANA38407_GTS10P_DISPLAY_MODE_2800x1752_30HS_60HS_TE_HW_SKIP_1,
	ANA38407_GTS10P_DISPLAY_MODE_2800x1752_30HS_120HS_TE_HW_SKIP_3,
	MAX_ANA38407_GTS10P_DISPLAY_MODE,
};

struct panel_vrr ana38407_gts10p_default_panel_vrr[] = {
	[ANA38407_VRR_120HS] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[ANA38407_VRR_60HS_120HS_TE_HW_SKIP_1] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[ANA38407_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[ANA38407_VRR_30HS_60HS_TE_HW_SKIP_1] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[ANA38407_VRR_30HS_120HS_TE_HW_SKIP_3] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 3,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *ana38407_gts10p_default_vrrtbl[] = {
	&ana38407_gts10p_default_panel_vrr[ANA38407_VRR_120HS],
	&ana38407_gts10p_default_panel_vrr[ANA38407_VRR_60HS_120HS_TE_HW_SKIP_1],
	&ana38407_gts10p_default_panel_vrr[ANA38407_VRR_60HS],
	&ana38407_gts10p_default_panel_vrr[ANA38407_VRR_30HS_60HS_TE_HW_SKIP_1],
	&ana38407_gts10p_default_panel_vrr[ANA38407_VRR_30HS_120HS_TE_HW_SKIP_3],
};

static struct panel_resol ana38407_gts10p_default_resol[] = {
	[ANA38407_RESOL_2800x1752] = {
		.w = 2800,
		.h = 1752,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 1400,
				.slice_h = 73,
			},
		},
		.available_vrr = ana38407_gts10p_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(ana38407_gts10p_default_vrrtbl),
	},
};

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode ana38407_gts10p_display_mode[] = {
	[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_120HS] = {
		.name = PANEL_DISPLAY_MODE_2800x1752_120HS,
		.resol = &ana38407_gts10p_default_resol[ANA38407_RESOL_2800x1752],
		.vrr = &ana38407_gts10p_default_panel_vrr[ANA38407_VRR_120HS],
	},
	[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_60HS_120HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_2800x1752_60HS_120HS_TE_HW_SKIP_1,
		.resol = &ana38407_gts10p_default_resol[ANA38407_RESOL_2800x1752],
		.vrr = &ana38407_gts10p_default_panel_vrr[ANA38407_VRR_60HS_120HS_TE_HW_SKIP_1],
	},
	[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_60HS] = {
		.name = PANEL_DISPLAY_MODE_2800x1752_60HS,
		.resol = &ana38407_gts10p_default_resol[ANA38407_RESOL_2800x1752],
		.vrr = &ana38407_gts10p_default_panel_vrr[ANA38407_VRR_60HS],
	},
	[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_30HS_60HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_2800x1752_30HS_60HS_TE_HW_SKIP_1,
		.resol = &ana38407_gts10p_default_resol[ANA38407_RESOL_2800x1752],
		.vrr = &ana38407_gts10p_default_panel_vrr[ANA38407_VRR_30HS_60HS_TE_HW_SKIP_1],
	},
	[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_30HS_120HS_TE_HW_SKIP_3] = {
		.name = PANEL_DISPLAY_MODE_2800x1752_30HS_120HS_TE_HW_SKIP_3,
		.resol = &ana38407_gts10p_default_resol[ANA38407_RESOL_2800x1752],
		.vrr = &ana38407_gts10p_default_panel_vrr[ANA38407_VRR_30HS_120HS_TE_HW_SKIP_3],
	},
};

static struct common_panel_display_mode *ana38407_gts10p_display_mode_array[] = {
	[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_120HS] = &ana38407_gts10p_display_mode[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_120HS],
	[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_60HS_120HS_TE_HW_SKIP_1] = &ana38407_gts10p_display_mode[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_60HS_120HS_TE_HW_SKIP_1],
	[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_60HS] = &ana38407_gts10p_display_mode[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_60HS],
	[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_30HS_60HS_TE_HW_SKIP_1] = &ana38407_gts10p_display_mode[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_30HS_60HS_TE_HW_SKIP_1],
	[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_30HS_120HS_TE_HW_SKIP_3] = &ana38407_gts10p_display_mode[ANA38407_GTS10P_DISPLAY_MODE_2800x1752_30HS_120HS_TE_HW_SKIP_3],
};

static struct common_panel_display_modes ana38407_gts10p_display_modes = {
	.num_modes = ARRAY_SIZE(ana38407_gts10p_display_mode),
	.modes = (struct common_panel_display_mode **)&ana38407_gts10p_display_mode_array,
};
#endif /* CONFIG_USDM_PANEL_DISPLAY_MODE */
#endif /* __ANA38407_GTS10P_RESOL_H__ */
