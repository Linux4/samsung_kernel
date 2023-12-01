/*
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NT36523N_GTS9FE_CSOT_RESOL_H__
#define __NT36523N_GTS9FE_CSOT_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "tft_common.h"

enum {
	NT36523N_GTS9FE_CSOT_DISPLAY_MODE_1440x2304_90HS,
	NT36523N_GTS9FE_CSOT_DISPLAY_MODE_1440x2304_60HS,
	NT36523N_GTS9FE_CSOT_DISPLAY_MODE_1440x2304_30NS,
	MAX_NT36523N_GTS9FE_CSOT_DISPLAY_MODE,
};

enum {
	NT36523N_GTS9FE_CSOT_RESOL_1440x2304,
	MAX_NT36523N_GTS9FE_CSOT_RESOL,
};

enum {
	NT36523N_GTS9FE_CSOT_VRR_90HS,
	NT36523N_GTS9FE_CSOT_VRR_60HS,
	NT36523N_GTS9FE_CSOT_VRR_30NS,
	MAX_NT36523N_GTS9FE_CSOT_VRR,
};

struct panel_vrr nt36523n_gts9fe_csot_default_panel_vrr[] = {
	[NT36523N_GTS9FE_CSOT_VRR_90HS] = {
		.fps = 90,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[NT36523N_GTS9FE_CSOT_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[NT36523N_GTS9FE_CSOT_VRR_30NS] = {
		.fps = 30,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
};

static struct panel_vrr *nt36523n_gts9fe_csot_default_vrrtbl[] = {
	&nt36523n_gts9fe_csot_default_panel_vrr[NT36523N_GTS9FE_CSOT_VRR_90HS],
	&nt36523n_gts9fe_csot_default_panel_vrr[NT36523N_GTS9FE_CSOT_VRR_60HS],
	&nt36523n_gts9fe_csot_default_panel_vrr[NT36523N_GTS9FE_CSOT_VRR_30NS],
};

static struct panel_resol nt36523n_gts9fe_csot_default_resol[] = {
	[NT36523N_GTS9FE_CSOT_RESOL_1440x2304] = {
		.w = 1440,
		.h = 2304,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 720,
				.slice_h = 24,
			},
		},
		.available_vrr = nt36523n_gts9fe_csot_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(nt36523n_gts9fe_csot_default_vrrtbl),
	}
};

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode nt36523n_gts9fe_csot_display_mode[] = {
	[NT36523N_GTS9FE_CSOT_DISPLAY_MODE_1440x2304_90HS] = {
		.name = PANEL_DISPLAY_MODE_1440x2304_90HS,
		.resol = &nt36523n_gts9fe_csot_default_resol[NT36523N_GTS9FE_CSOT_RESOL_1440x2304],
		.vrr = &nt36523n_gts9fe_csot_default_panel_vrr[NT36523N_GTS9FE_CSOT_VRR_90HS],
	},
	[NT36523N_GTS9FE_CSOT_DISPLAY_MODE_1440x2304_60HS] = {
		.name = PANEL_DISPLAY_MODE_1440x2304_60HS,
		.resol = &nt36523n_gts9fe_csot_default_resol[NT36523N_GTS9FE_CSOT_RESOL_1440x2304],
		.vrr = &nt36523n_gts9fe_csot_default_panel_vrr[NT36523N_GTS9FE_CSOT_VRR_60HS],
	},
	[NT36523N_GTS9FE_CSOT_DISPLAY_MODE_1440x2304_30NS] = {
		.name = PANEL_DISPLAY_MODE_1440x2304_30NS,
		.resol = &nt36523n_gts9fe_csot_default_resol[NT36523N_GTS9FE_CSOT_RESOL_1440x2304],
		.vrr = &nt36523n_gts9fe_csot_default_panel_vrr[NT36523N_GTS9FE_CSOT_VRR_30NS],
	},
};

static struct common_panel_display_mode *nt36523n_gts9fe_csot_display_mode_array[] = {
	&nt36523n_gts9fe_csot_display_mode[NT36523N_GTS9FE_CSOT_DISPLAY_MODE_1440x2304_90HS],
	&nt36523n_gts9fe_csot_display_mode[NT36523N_GTS9FE_CSOT_DISPLAY_MODE_1440x2304_60HS],
	&nt36523n_gts9fe_csot_display_mode[NT36523N_GTS9FE_CSOT_DISPLAY_MODE_1440x2304_30NS],
};

static struct common_panel_display_modes nt36523n_gts9fe_csot_display_modes = {
	.num_modes = ARRAY_SIZE(nt36523n_gts9fe_csot_display_mode_array),
	.modes = (struct common_panel_display_mode **)&nt36523n_gts9fe_csot_display_mode_array,
};
#endif /* CONFIG_USDM_PANEL_DISPLAY_MODE */
#endif /* __NT36523N_GTS9FE_CSOT_RESOL_H__ */
