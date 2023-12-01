/*
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HX83102J_GTS9FE_BOE_RESOL_H__
#define __HX83102J_GTS9FE_BOE_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "tft_common.h"

enum {
	HX83102J_GTS9FE_BOE_DISPLAY_MODE_1440x2304_90HS,
	HX83102J_GTS9FE_BOE_DISPLAY_MODE_1440x2304_60HS,
	HX83102J_GTS9FE_BOE_DISPLAY_MODE_1440x2304_30NS,
	MAX_HX83102J_GTS9FE_BOE_DISPLAY_MODE,
};

enum {
	HX83102J_GTS9FE_BOE_RESOL_1440x2304,
	MAX_HX83102J_GTS9FE_BOE_RESOL,
};

enum {
	HX83102J_GTS9FE_BOE_VRR_90HS,
	HX83102J_GTS9FE_BOE_VRR_60HS,
	HX83102J_GTS9FE_BOE_VRR_30NS,
	MAX_HX83102J_GTS9FE_BOE_VRR,
};

struct panel_vrr hx83102j_gts9fe_boe_default_panel_vrr[] = {
	[HX83102J_GTS9FE_BOE_VRR_90HS] = {
		.fps = 90,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[HX83102J_GTS9FE_BOE_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[HX83102J_GTS9FE_BOE_VRR_30NS] = {
		.fps = 30,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
};

static struct panel_vrr *hx83102j_gts9fe_boe_default_vrrtbl[] = {
	&hx83102j_gts9fe_boe_default_panel_vrr[HX83102J_GTS9FE_BOE_VRR_90HS],
	&hx83102j_gts9fe_boe_default_panel_vrr[HX83102J_GTS9FE_BOE_VRR_60HS],
	&hx83102j_gts9fe_boe_default_panel_vrr[HX83102J_GTS9FE_BOE_VRR_30NS],
};

static struct panel_resol hx83102j_gts9fe_boe_default_resol[] = {
	[HX83102J_GTS9FE_BOE_RESOL_1440x2304] = {
		.w = 1440,
		.h = 2304,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 720,
				.slice_h = 24,
			},
		},
		.available_vrr = hx83102j_gts9fe_boe_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(hx83102j_gts9fe_boe_default_vrrtbl),
	}
};

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode hx83102j_gts9fe_boe_display_mode[] = {
	[HX83102J_GTS9FE_BOE_DISPLAY_MODE_1440x2304_90HS] = {
		.name = PANEL_DISPLAY_MODE_1440x2304_90HS,
		.resol = &hx83102j_gts9fe_boe_default_resol[HX83102J_GTS9FE_BOE_RESOL_1440x2304],
		.vrr = &hx83102j_gts9fe_boe_default_panel_vrr[HX83102J_GTS9FE_BOE_VRR_90HS],
	},
	[HX83102J_GTS9FE_BOE_DISPLAY_MODE_1440x2304_60HS] = {
		.name = PANEL_DISPLAY_MODE_1440x2304_60HS,
		.resol = &hx83102j_gts9fe_boe_default_resol[HX83102J_GTS9FE_BOE_RESOL_1440x2304],
		.vrr = &hx83102j_gts9fe_boe_default_panel_vrr[HX83102J_GTS9FE_BOE_VRR_60HS],
	},
	[HX83102J_GTS9FE_BOE_DISPLAY_MODE_1440x2304_30NS] = {
		.name = PANEL_DISPLAY_MODE_1440x2304_30NS,
		.resol = &hx83102j_gts9fe_boe_default_resol[HX83102J_GTS9FE_BOE_RESOL_1440x2304],
		.vrr = &hx83102j_gts9fe_boe_default_panel_vrr[HX83102J_GTS9FE_BOE_VRR_30NS],
	},
};

static struct common_panel_display_mode *hx83102j_gts9fe_boe_display_mode_array[] = {
	&hx83102j_gts9fe_boe_display_mode[HX83102J_GTS9FE_BOE_DISPLAY_MODE_1440x2304_90HS],
	&hx83102j_gts9fe_boe_display_mode[HX83102J_GTS9FE_BOE_DISPLAY_MODE_1440x2304_60HS],
	&hx83102j_gts9fe_boe_display_mode[HX83102J_GTS9FE_BOE_DISPLAY_MODE_1440x2304_30NS],
};

static struct common_panel_display_modes hx83102j_gts9fe_boe_display_modes = {
	.num_modes = ARRAY_SIZE(hx83102j_gts9fe_boe_display_mode_array),
	.modes = (struct common_panel_display_mode **)&hx83102j_gts9fe_boe_display_mode_array,
};
#endif /* CONFIG_USDM_PANEL_DISPLAY_MODE */
#endif /* __HX83102J_GTS9FE_BOE_RESOL_H__ */
