/*
 * Header file for Panel Driver
 *
 * Copyright (c) 2022 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TD4160_A14X_00_RESOL_H__
#define __TD4160_A14X_00_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"

enum {
	TD4160_A14X_00_DISPLAY_MODE_720x1600_90HS,
	TD4160_A14X_00_DISPLAY_MODE_720x1600_60HS,
	MAX_TD4160_A14X_00_DISPLAY_MODE,
};

enum {
	TD4160_A14X_00_RESOL_720x1600,
	MAX_TD4160_A14X_00_RESOL,
};

enum {
	TD4160_A14X_00_VRR_90HS,
	TD4160_A14X_00_VRR_60HS,
	MAX_TD4160_A14X_00_VRR,
};

struct panel_vrr td4160_a14x_00_default_panel_vrr[] = {
	[TD4160_A14X_00_VRR_90HS] = {
		.fps = 90,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[TD4160_A14X_00_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *td4160_a14x_00_default_vrrtbl[] = {
	&td4160_a14x_00_default_panel_vrr[TD4160_A14X_00_VRR_90HS],
	&td4160_a14x_00_default_panel_vrr[TD4160_A14X_00_VRR_60HS],
};

static struct panel_resol td4160_a14x_00_default_resol[] = {
	[TD4160_A14X_00_RESOL_720x1600] = {
		.w = 720,
		.h = 1600,
		.comp_type = PN_COMP_TYPE_NONE,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 8,
			},
		},
		.available_vrr = td4160_a14x_00_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(td4160_a14x_00_default_vrrtbl),
	}
};

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode td4160_a14x_00_display_mode[] = {
	/* FHD */
	[TD4160_A14X_00_DISPLAY_MODE_720x1600_90HS] = {
		.name = PANEL_DISPLAY_MODE_720x1600_90HS,
		.resol = &td4160_a14x_00_default_resol[TD4160_A14X_00_RESOL_720x1600],
		.vrr = &td4160_a14x_00_default_panel_vrr[TD4160_A14X_00_VRR_90HS],
	},
	[TD4160_A14X_00_DISPLAY_MODE_720x1600_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2408_60HS,
		.resol = &td4160_a14x_00_default_resol[TD4160_A14X_00_RESOL_720x1600],
		.vrr = &td4160_a14x_00_default_panel_vrr[TD4160_A14X_00_VRR_60HS],
	},
};

static struct common_panel_display_mode *td4160_a14x_00_display_mode_array[] = {
	&td4160_a14x_00_display_mode[TD4160_A14X_00_DISPLAY_MODE_720x1600_90HS],
	&td4160_a14x_00_display_mode[TD4160_A14X_00_DISPLAY_MODE_720x1600_60HS],
};

static struct common_panel_display_modes td4160_a14x_00_display_modes = {
	.num_modes = ARRAY_SIZE(td4160_a14x_00_display_mode_array),
	.modes = (struct common_panel_display_mode **)&td4160_a14x_00_display_mode_array,
};
#endif /* CONFIG_USDM_PANEL_DISPLAY_MODE */
#endif /* __TD4160_A14X_00_RESOL_H__ */
