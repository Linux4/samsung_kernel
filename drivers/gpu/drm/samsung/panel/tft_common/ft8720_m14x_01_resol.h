/*
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __FT8720_M14X_01_RESOL_H__
#define __FT8720_M14X_01_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"

enum {
	FT8720_M14X_01_DISPLAY_MODE_1080x2408_90HS,
	FT8720_M14X_01_DISPLAY_MODE_1080x2408_60HS,
	MAX_FT8720_M14X_01_DISPLAY_MODE,
};

enum {
	FT8720_M14X_01_RESOL_1080x2408,
	MAX_FT8720_M14X_01_RESOL,
};

enum {
	FT8720_M14X_01_VRR_90HS,
	FT8720_M14X_01_VRR_60HS,
	MAX_FT8720_M14X_01_VRR,
};

struct panel_vrr ft8720_m14x_01_default_panel_vrr[] = {
	[FT8720_M14X_01_VRR_90HS] = {
		.fps = 90,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[FT8720_M14X_01_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *ft8720_m14x_01_default_vrrtbl[] = {
	&ft8720_m14x_01_default_panel_vrr[FT8720_M14X_01_VRR_90HS],
	&ft8720_m14x_01_default_panel_vrr[FT8720_M14X_01_VRR_60HS],
};

static struct panel_resol ft8720_m14x_01_default_resol[] = {
	[FT8720_M14X_01_RESOL_1080x2408] = {
		.w = 1080,
		.h = 2408,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 8,
			},
		},
		.available_vrr = ft8720_m14x_01_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(ft8720_m14x_01_default_vrrtbl),
	}
};

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode ft8720_m14x_01_display_mode[] = {
	/* FHD */
	[FT8720_M14X_01_DISPLAY_MODE_1080x2408_90HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2408_90HS,
		.resol = &ft8720_m14x_01_default_resol[FT8720_M14X_01_RESOL_1080x2408],
		.vrr = &ft8720_m14x_01_default_panel_vrr[FT8720_M14X_01_VRR_90HS],
	},
	[FT8720_M14X_01_DISPLAY_MODE_1080x2408_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2408_60HS,
		.resol = &ft8720_m14x_01_default_resol[FT8720_M14X_01_RESOL_1080x2408],
		.vrr = &ft8720_m14x_01_default_panel_vrr[FT8720_M14X_01_VRR_60HS],
	},
};

static struct common_panel_display_mode *ft8720_m14x_01_display_mode_array[] = {
	&ft8720_m14x_01_display_mode[FT8720_M14X_01_DISPLAY_MODE_1080x2408_90HS],
	&ft8720_m14x_01_display_mode[FT8720_M14X_01_DISPLAY_MODE_1080x2408_60HS],
};

static struct common_panel_display_modes ft8720_m14x_01_display_modes = {
	.num_modes = ARRAY_SIZE(ft8720_m14x_01_display_mode_array),
	.modes = (struct common_panel_display_mode **)&ft8720_m14x_01_display_mode_array,
};
#endif /* CONFIG_USDM_PANEL_DISPLAY_MODE */
#endif /* __FT8720_M14X_01_RESOL_H__ */
