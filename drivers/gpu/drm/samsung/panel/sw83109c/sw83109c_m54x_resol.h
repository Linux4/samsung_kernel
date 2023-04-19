/*
 * linux/drivers/video/fbdev/exynos/panel/sw83109c/sw83109c_m54x_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SW83109C_M54X_RESOL_H__
#define __SW83109C_M54X_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "sw83109c.h"
#include "sw83109c_dimming.h"

struct panel_vrr sw83109c_m54x_default_panel_vrr[] = {
	[SW83109C_VRR_120HS] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[SW83109C_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[SW83109C_VRR_30NS] = {
		.fps = 30,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
};

static struct panel_vrr *sw83109c_m54x_default_vrrtbl[] = {
	&sw83109c_m54x_default_panel_vrr[SW83109C_VRR_120HS],
	&sw83109c_m54x_default_panel_vrr[SW83109C_VRR_60HS],
	&sw83109c_m54x_default_panel_vrr[SW83109C_VRR_30NS],
};

static struct panel_resol sw83109c_m54x_default_resol[] = {
	[SW83109C_RESOL_1080x2400] = {
		.w = 1080,
		.h = 2400,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 60,
			},
		},
		.available_vrr = sw83109c_m54x_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(sw83109c_m54x_default_vrrtbl),
	},
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode sw83109c_m54x_display_mode[] = {
	/* FHD */
	[SW83109C_M54X_DISPLAY_MODE_1080x2400_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_120HS,
		.resol = &sw83109c_m54x_default_resol[SW83109C_RESOL_1080x2400],
		.vrr = &sw83109c_m54x_default_panel_vrr[SW83109C_VRR_120HS],
	},
	[SW83109C_M54X_DISPLAY_MODE_1080x2400_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60HS,
		.resol = &sw83109c_m54x_default_resol[SW83109C_RESOL_1080x2400],
		.vrr = &sw83109c_m54x_default_panel_vrr[SW83109C_VRR_60HS],
	},
	[SW83109C_M54X_DISPLAY_MODE_1080x2400_30NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_30NS,
		.resol = &sw83109c_m54x_default_resol[SW83109C_RESOL_1080x2400],
		.vrr = &sw83109c_m54x_default_panel_vrr[SW83109C_VRR_30NS],
	},

};

static struct common_panel_display_mode *sw83109c_m54x_display_mode_array[] = {
	[SW83109C_M54X_DISPLAY_MODE_1080x2400_120HS] = &sw83109c_m54x_display_mode[SW83109C_M54X_DISPLAY_MODE_1080x2400_120HS],
	[SW83109C_M54X_DISPLAY_MODE_1080x2400_60HS] = &sw83109c_m54x_display_mode[SW83109C_M54X_DISPLAY_MODE_1080x2400_60HS],
	[SW83109C_M54X_DISPLAY_MODE_1080x2400_30NS] = &sw83109c_m54x_display_mode[SW83109C_M54X_DISPLAY_MODE_1080x2400_30NS],
};

static struct common_panel_display_modes sw83109c_m54x_display_modes = {
	.num_modes = ARRAY_SIZE(sw83109c_m54x_display_mode),
	.modes = (struct common_panel_display_mode **)&sw83109c_m54x_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __SW83109C_M54X_RESOL_H__ */
