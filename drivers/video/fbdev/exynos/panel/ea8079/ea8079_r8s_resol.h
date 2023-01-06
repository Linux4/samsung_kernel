/*
 * linux/drivers/video/fbdev/exynos/panel/ea8079/ea8079_r8s_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8079_R8S_RESOL_H__
#define __EA8079_R8S_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "ea8079.h"
#include "ea8079_dimming.h"

struct panel_vrr ea8079_r8s_default_panel_vrr[] = {
	[EA8079_VRR_120HS] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[EA8079_VRR_96HS] = {
		.fps = 96,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[EA8079_VRR_60HS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *ea8079_r8s_default_vrrtbl[] = {
	&ea8079_r8s_default_panel_vrr[EA8079_VRR_120HS],
	&ea8079_r8s_default_panel_vrr[EA8079_VRR_96HS],
	&ea8079_r8s_default_panel_vrr[EA8079_VRR_60HS],
};

static struct panel_resol ea8079_r8s_default_resol[] = {
	[EA8079_RESOL_1080x2400] = {
		.w = 1080,
		.h = 2400,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 40,
			},
		},
		.available_vrr = ea8079_r8s_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(ea8079_r8s_default_vrrtbl),
	},
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode ea8079_r8s_display_mode[] = {
	/* FHD */
	[EA8079_DISPLAY_MODE_1080x2400_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_120HS,
		.resol = &ea8079_r8s_default_resol[EA8079_RESOL_1080x2400],
		.vrr = &ea8079_r8s_default_panel_vrr[EA8079_VRR_120HS],
	},
	[EA8079_DISPLAY_MODE_1080x2400_96HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_96HS,
		.resol = &ea8079_r8s_default_resol[EA8079_RESOL_1080x2400],
		.vrr = &ea8079_r8s_default_panel_vrr[EA8079_VRR_96HS],
	},
	[EA8079_DISPLAY_MODE_1080x2400_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2400_60HS,
		.resol = &ea8079_r8s_default_resol[EA8079_RESOL_1080x2400],
		.vrr = &ea8079_r8s_default_panel_vrr[EA8079_VRR_60HS],
	},
};

static struct common_panel_display_mode *ea8079_r8s_display_mode_array[] = {
	[EA8079_DISPLAY_MODE_1080x2400_120HS] = &ea8079_r8s_display_mode[EA8079_DISPLAY_MODE_1080x2400_120HS],
	[EA8079_DISPLAY_MODE_1080x2400_96HS] = &ea8079_r8s_display_mode[EA8079_DISPLAY_MODE_1080x2400_96HS],
	[EA8079_DISPLAY_MODE_1080x2400_60HS] = &ea8079_r8s_display_mode[EA8079_DISPLAY_MODE_1080x2400_60HS],
};

static struct common_panel_display_modes ea8079_r8s_display_modes = {
	.num_modes = ARRAY_SIZE(ea8079_r8s_display_mode),
	.modes = (struct common_panel_display_mode **)&ea8079_r8s_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __EA8079_R8S_RESOL_H__ */
