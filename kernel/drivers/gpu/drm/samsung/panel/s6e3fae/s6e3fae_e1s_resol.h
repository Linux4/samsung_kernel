/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fae/s6e3fae_e1s_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAE_E1S_RESOL_H__
#define __S6E3FAE_E1S_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "s6e3fae.h"
#include "s6e3fae_dimming.h"

struct panel_vrr s6e3fae_e1s_default_panel_vrr[] = {
	[S6E3FAE_VRR_120HS] = {
		.fps = 120,
		.base_fps = 240,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAE_VRR_80HS] = {
		.fps = 80,
		.base_fps = 240,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAE_VRR_60HS] = {
		.fps = 60,
		.base_fps = 240,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAE_VRR_48HS] = {
		.fps = 48,
		.base_fps = 240,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAE_VRR_30HS] = {
		.fps = 30,
		.base_fps = 240,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAE_VRR_24HS] = {
		.fps = 24,
		.base_fps = 240,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAE_VRR_10HS] = {
		.fps = 10,
		.base_fps = 240,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *s6e3fae_e1s_default_vrrtbl[] = {
	&s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_120HS],
	&s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_80HS],
	&s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_60HS],
	&s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_48HS],
	&s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_30HS],
	&s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_24HS],
	&s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_10HS],
};

static struct panel_resol s6e3fae_e1s_default_resol[] = {
	[S6E3FAE_RESOL_1080x2340] = {
		.w = 1080,
		.h = 2340,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 117,
			},
		},
		.available_vrr = s6e3fae_e1s_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3fae_e1s_default_vrrtbl),
	},
};

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode s6e3fae_e1s_display_mode[MAX_S6E3FAE_DISPLAY_MODE] = {
	[S6E3FAE_DISPLAY_MODE_1080x2340_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_120HS,
		.resol = &s6e3fae_e1s_default_resol[S6E3FAE_RESOL_1080x2340],
		.vrr = &s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_120HS],
	},
	[S6E3FAE_DISPLAY_MODE_1080x2340_80HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_80HS,
		.resol = &s6e3fae_e1s_default_resol[S6E3FAE_RESOL_1080x2340],
		.vrr = &s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_80HS],
	},
	[S6E3FAE_DISPLAY_MODE_1080x2340_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_60HS,
		.resol = &s6e3fae_e1s_default_resol[S6E3FAE_RESOL_1080x2340],
		.vrr = &s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_60HS],
	},
	[S6E3FAE_DISPLAY_MODE_1080x2340_48HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_48HS,
		.resol = &s6e3fae_e1s_default_resol[S6E3FAE_RESOL_1080x2340],
		.vrr = &s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_48HS],
	},
	[S6E3FAE_DISPLAY_MODE_1080x2340_30HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_30HS,
		.resol = &s6e3fae_e1s_default_resol[S6E3FAE_RESOL_1080x2340],
		.vrr = &s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_30HS],
	},
	[S6E3FAE_DISPLAY_MODE_1080x2340_24HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_24HS,
		.resol = &s6e3fae_e1s_default_resol[S6E3FAE_RESOL_1080x2340],
		.vrr = &s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_24HS],
	},
	[S6E3FAE_DISPLAY_MODE_1080x2340_10HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_10HS,
		.resol = &s6e3fae_e1s_default_resol[S6E3FAE_RESOL_1080x2340],
		.vrr = &s6e3fae_e1s_default_panel_vrr[S6E3FAE_VRR_10HS],
	},
};

static struct common_panel_display_mode *s6e3fae_e1s_display_mode_array[MAX_S6E3FAE_DISPLAY_MODE] = {
	[S6E3FAE_DISPLAY_MODE_1080x2340_120HS] = &s6e3fae_e1s_display_mode[S6E3FAE_DISPLAY_MODE_1080x2340_120HS],
	[S6E3FAE_DISPLAY_MODE_1080x2340_80HS] = &s6e3fae_e1s_display_mode[S6E3FAE_DISPLAY_MODE_1080x2340_80HS],
	[S6E3FAE_DISPLAY_MODE_1080x2340_60HS] = &s6e3fae_e1s_display_mode[S6E3FAE_DISPLAY_MODE_1080x2340_60HS],
	[S6E3FAE_DISPLAY_MODE_1080x2340_48HS] = &s6e3fae_e1s_display_mode[S6E3FAE_DISPLAY_MODE_1080x2340_48HS],
	[S6E3FAE_DISPLAY_MODE_1080x2340_30HS] = &s6e3fae_e1s_display_mode[S6E3FAE_DISPLAY_MODE_1080x2340_30HS],
	[S6E3FAE_DISPLAY_MODE_1080x2340_24HS] = &s6e3fae_e1s_display_mode[S6E3FAE_DISPLAY_MODE_1080x2340_24HS],
	[S6E3FAE_DISPLAY_MODE_1080x2340_10HS] = &s6e3fae_e1s_display_mode[S6E3FAE_DISPLAY_MODE_1080x2340_10HS],
};

static struct common_panel_display_modes s6e3fae_e1s_display_modes = {
	.num_modes = ARRAY_SIZE(s6e3fae_e1s_display_mode_array),
	.modes = (struct common_panel_display_mode **)&s6e3fae_e1s_display_mode_array,
};
#endif /* CONFIG_USDM_PANEL_DISPLAY_MODE */
#endif /* __S6E3FAE_E1S_RESOL_H__ */
