/*
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HX83122A_GTS9FEP_CSOT_RESOL_H__
#define __HX83122A_GTS9FEP_CSOT_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "tft_common.h"

enum {
	HX83122A_GTS9FEP_CSOT_DISPLAY_MODE_1600x2560_90HS,
	HX83122A_GTS9FEP_CSOT_DISPLAY_MODE_1600x2560_60HS,
	HX83122A_GTS9FEP_CSOT_DISPLAY_MODE_1600x2560_30NS,
	MAX_HX83122A_GTS9FEP_CSOT_DISPLAY_MODE,
};

enum {
	HX83122A_GTS9FEP_CSOT_RESOL_1600x2560,
	MAX_HX83122A_GTS9FEP_CSOT_RESOL,
};

enum {
	HX83122A_GTS9FEP_CSOT_VRR_90HS,
	HX83122A_GTS9FEP_CSOT_VRR_60HS,
	HX83122A_GTS9FEP_CSOT_VRR_30NS,
	MAX_HX83122A_GTS9FEP_CSOT_VRR,
};

struct panel_vrr hx83122a_gts9fep_csot_default_panel_vrr[] = {
	[HX83122A_GTS9FEP_CSOT_VRR_90HS] = {
		.fps = 90,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[HX83122A_GTS9FEP_CSOT_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[HX83122A_GTS9FEP_CSOT_VRR_30NS] = {
		.fps = 30,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
};

static struct panel_vrr *hx83122a_gts9fep_csot_default_vrrtbl[] = {
	&hx83122a_gts9fep_csot_default_panel_vrr[HX83122A_GTS9FEP_CSOT_VRR_90HS],
	&hx83122a_gts9fep_csot_default_panel_vrr[HX83122A_GTS9FEP_CSOT_VRR_60HS],
	&hx83122a_gts9fep_csot_default_panel_vrr[HX83122A_GTS9FEP_CSOT_VRR_30NS],
};

static struct panel_resol hx83122a_gts9fep_csot_default_resol[] = {
	[HX83122A_GTS9FEP_CSOT_RESOL_1600x2560] = {
		.w = 1600,
		.h = 2560,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 800,
				.slice_h = 20,
			},
		},
		.available_vrr = hx83122a_gts9fep_csot_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(hx83122a_gts9fep_csot_default_vrrtbl),
	}
};

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode hx83122a_gts9fep_csot_display_mode[] = {
	[HX83122A_GTS9FEP_CSOT_DISPLAY_MODE_1600x2560_90HS] = {
		.name = PANEL_DISPLAY_MODE_1600x2560_90HS,
		.resol = &hx83122a_gts9fep_csot_default_resol[HX83122A_GTS9FEP_CSOT_RESOL_1600x2560],
		.vrr = &hx83122a_gts9fep_csot_default_panel_vrr[HX83122A_GTS9FEP_CSOT_VRR_90HS],
	},
	[HX83122A_GTS9FEP_CSOT_DISPLAY_MODE_1600x2560_60HS] = {
		.name = PANEL_DISPLAY_MODE_1600x2560_60HS,
		.resol = &hx83122a_gts9fep_csot_default_resol[HX83122A_GTS9FEP_CSOT_RESOL_1600x2560],
		.vrr = &hx83122a_gts9fep_csot_default_panel_vrr[HX83122A_GTS9FEP_CSOT_VRR_60HS],
	},
	[HX83122A_GTS9FEP_CSOT_DISPLAY_MODE_1600x2560_30NS] = {
		.name = PANEL_DISPLAY_MODE_1600x2560_30NS,
		.resol = &hx83122a_gts9fep_csot_default_resol[HX83122A_GTS9FEP_CSOT_RESOL_1600x2560],
		.vrr = &hx83122a_gts9fep_csot_default_panel_vrr[HX83122A_GTS9FEP_CSOT_VRR_30NS],
	},
};

static struct common_panel_display_mode *hx83122a_gts9fep_csot_display_mode_array[] = {
	&hx83122a_gts9fep_csot_display_mode[HX83122A_GTS9FEP_CSOT_DISPLAY_MODE_1600x2560_90HS],
	&hx83122a_gts9fep_csot_display_mode[HX83122A_GTS9FEP_CSOT_DISPLAY_MODE_1600x2560_60HS],
	&hx83122a_gts9fep_csot_display_mode[HX83122A_GTS9FEP_CSOT_DISPLAY_MODE_1600x2560_30NS],
};

static struct common_panel_display_modes hx83122a_gts9fep_csot_display_modes = {
	.num_modes = ARRAY_SIZE(hx83122a_gts9fep_csot_display_mode_array),
	.modes = (struct common_panel_display_mode **)&hx83122a_gts9fep_csot_display_mode_array,
};
#endif /* CONFIG_USDM_PANEL_DISPLAY_MODE */
#endif /* __HX83122A_GTS9FEP_CSOT_RESOL_H__ */
