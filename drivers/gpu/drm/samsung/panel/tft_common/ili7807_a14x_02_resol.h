/*
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ILI7807_A14X_02_RESOL_H__
#define __ILI7807_A14X_02_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"

enum {
	ILI7807_A14X_02_DISPLAY_MODE_1080x2408_90HS,
	ILI7807_A14X_02_DISPLAY_MODE_1080x2408_60HS,
	MAX_ILI7807_A14X_02_DISPLAY_MODE,
};

enum {
	ILI7807_A14X_02_RESOL_1080x2408,
	MAX_ILI7807_A14X_02_RESOL,
};

enum {
	ILI7807_A14X_02_VRR_90HS,
	ILI7807_A14X_02_VRR_60HS,
	MAX_ILI7807_A14X_02_VRR,
};

struct panel_vrr ili7807_a14x_02_default_panel_vrr[] = {
	[ILI7807_A14X_02_VRR_90HS] = {
		.fps = 90,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[ILI7807_A14X_02_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *ili7807_a14x_02_default_vrrtbl[] = {
	&ili7807_a14x_02_default_panel_vrr[ILI7807_A14X_02_VRR_90HS],
	&ili7807_a14x_02_default_panel_vrr[ILI7807_A14X_02_VRR_60HS],
};

static struct panel_resol ili7807_a14x_02_default_resol[] = {
	[ILI7807_A14X_02_RESOL_1080x2408] = {
		.w = 1080,
		.h = 2408,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 8,
			},
		},
		.available_vrr = ili7807_a14x_02_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(ili7807_a14x_02_default_vrrtbl),
	}
};

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode ili7807_a14x_02_display_mode[] = {
	/* FHD */
	[ILI7807_A14X_02_DISPLAY_MODE_1080x2408_90HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2408_90HS,
		.resol = &ili7807_a14x_02_default_resol[ILI7807_A14X_02_RESOL_1080x2408],
		.vrr = &ili7807_a14x_02_default_panel_vrr[ILI7807_A14X_02_VRR_90HS],
	},
	[ILI7807_A14X_02_DISPLAY_MODE_1080x2408_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2408_60HS,
		.resol = &ili7807_a14x_02_default_resol[ILI7807_A14X_02_RESOL_1080x2408],
		.vrr = &ili7807_a14x_02_default_panel_vrr[ILI7807_A14X_02_VRR_60HS],
	},
};

static struct common_panel_display_mode *ili7807_a14x_02_display_mode_array[] = {
	&ili7807_a14x_02_display_mode[ILI7807_A14X_02_DISPLAY_MODE_1080x2408_90HS],
	&ili7807_a14x_02_display_mode[ILI7807_A14X_02_DISPLAY_MODE_1080x2408_60HS],
};

static struct common_panel_display_modes ili7807_a14x_02_display_modes = {
	.num_modes = ARRAY_SIZE(ili7807_a14x_02_display_mode_array),
	.modes = (struct common_panel_display_mode **)&ili7807_a14x_02_display_mode_array,
};
#endif /* CONFIG_USDM_PANEL_DISPLAY_MODE */
#endif /* __ILI7807_A14X_02_RESOL_H__ */
