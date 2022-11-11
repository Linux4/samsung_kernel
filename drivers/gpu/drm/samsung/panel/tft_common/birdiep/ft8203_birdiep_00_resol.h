/*
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __FT8203_BIRDIEP_00_RESOL_H__
#define __FT8203_BIRDIEP_00_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../../panel.h"
#include "../tft_common.h"

enum {
	FT8203_BIRDIEP_00_DISPLAY_MODE_1600x2560_60HS,
	MAX_FT8203_BIRDIEP_00_DISPLAY_MODE,
};

enum {
	FT8203_BIRDIEP_00_RESOL_1600x2560,
	MAX_FT8203_BIRDIEP_00_RESOL,
};

enum {
	FT8203_BIRDIEP_00_VRR_60HS,
	MAX_FT8203_BIRDIEP_00_VRR,
};

struct panel_vrr ft8203_birdiep_00_default_panel_vrr[] = {
	[FT8203_BIRDIEP_00_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *ft8203_birdiep_00_default_vrrtbl[] = {
	&ft8203_birdiep_00_default_panel_vrr[FT8203_BIRDIEP_00_VRR_60HS],
};

static struct panel_resol ft8203_birdiep_00_default_resol[] = {
	[FT8203_BIRDIEP_00_RESOL_1600x2560] = {
		.w = 1600,
		.h = 2560,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 8,
			},
		},
		.available_vrr = ft8203_birdiep_00_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(ft8203_birdiep_00_default_vrrtbl),
	}
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode ft8203_birdiep_00_display_mode[] = {
	[FT8203_BIRDIEP_00_DISPLAY_MODE_1600x2560_60HS] = {
		.name = PANEL_DISPLAY_MODE_1600x2560_60HS,
		.resol = &ft8203_birdiep_00_default_resol[FT8203_BIRDIEP_00_RESOL_1600x2560],
		.vrr = &ft8203_birdiep_00_default_panel_vrr[FT8203_BIRDIEP_00_VRR_60HS],
	},
};

static struct common_panel_display_mode *ft8203_birdiep_00_display_mode_array[] = {
	&ft8203_birdiep_00_display_mode[FT8203_BIRDIEP_00_DISPLAY_MODE_1600x2560_60HS],
};

static struct common_panel_display_modes ft8203_birdiep_00_display_modes = {
	.num_modes = ARRAY_SIZE(ft8203_birdiep_00_display_mode_array),
	.modes = (struct common_panel_display_mode **)&ft8203_birdiep_00_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __FT8203_BIRDIEP_00_RESOL_H__ */
