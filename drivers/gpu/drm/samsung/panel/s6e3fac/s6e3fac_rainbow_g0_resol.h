/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fac/s6e3fac_rainbow_g0_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAC_RAINBOW_G0_RESOL_H__
#define __S6E3FAC_RAINBOW_G0_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "s6e3fac.h"
#include "s6e3fac_dimming.h"

struct panel_vrr s6e3fac_rainbow_g0_default_panel_vrr[] = {
	[S6E3FAC_VRR_60NS] = {
		.fps = 60,
		.base_fps = 60,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
	[S6E3FAC_VRR_48NS] = {
		.fps = 48,
		.base_fps = 60,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
	[S6E3FAC_VRR_120HS] = {
		.fps = 120,
		.base_fps = 120,
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
	[S6E3FAC_VRR_60HS_120HS_TE_HW_SKIP_1] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAC_VRR_30HS_120HS_TE_HW_SKIP_3] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 3,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAC_VRR_24HS_120HS_TE_HW_SKIP_4] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 4,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 11,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAC_VRR_96HS] = {
		.fps = 96,
		.base_fps = 120,
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
	[S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1] = {
		.fps = 96,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAC_VRR_60HS] = {
		.fps = 60,
		.base_fps = 120,
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
	[S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1] = {
		.fps = 60,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAC_VRR_48HS] = {
		.fps = 48,
		.base_fps = 120,
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
	[S6E3FAC_VRR_24HS_48HS_TE_HW_SKIP_1] = {
		.fps = 48,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4] = {
		.fps = 48,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 4,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *s6e3fac_rainbow_g0_default_vrrtbl[] = {
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_60NS],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_48NS],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_120HS],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_60HS_120HS_TE_HW_SKIP_1],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_30HS_120HS_TE_HW_SKIP_3],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_24HS_120HS_TE_HW_SKIP_4],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_96HS],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_60HS],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_48HS],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_24HS_48HS_TE_HW_SKIP_1],
	&s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4],
};

static struct panel_resol s6e3fac_rainbow_g0_default_resol[] = {
	[S6E3FAC_RESOL_1080x2340] = {
		.w = 1080,
		.h = 2340,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 120,
			},
		},
		.available_vrr = s6e3fac_rainbow_g0_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3fac_rainbow_g0_default_vrrtbl),
	},
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode s6e3fac_rainbow_g0_display_mode[MAX_S6E3FAC_DISPLAY_MODE] = {
	/* FHD */
	[S6E3FAC_DISPLAY_MODE_1080x2340_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_120HS,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_120HS],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_60HS_120HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_60HS_120HS_TE_HW_SKIP_1,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_60HS_120HS_TE_HW_SKIP_1],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_30HS_120HS_TE_HW_SKIP_3] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_30HS_120HS_TE_HW_SKIP_3,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_30HS_120HS_TE_HW_SKIP_3],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_24HS_120HS_TE_HW_SKIP_4] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_24HS_120HS_TE_HW_SKIP_4,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_24HS_120HS_TE_HW_SKIP_4],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_10HS_120HS_TE_HW_SKIP_11] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_10HS_120HS_TE_HW_SKIP_11,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_96HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_96HS,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_96HS],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_48HS_96HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_48HS_96HS_TE_HW_SKIP_1,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_60HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_60HS,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_60HS],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_30HS_60HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_30HS_60HS_TE_HW_SKIP_1,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_48HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_48HS,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_48HS],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_24HS_48HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_24HS_48HS_TE_HW_SKIP_1,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_24HS_48HS_TE_HW_SKIP_1],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_10HS_48HS_TE_HW_SKIP_4] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_10HS_48HS_TE_HW_SKIP_4,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_60NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_60NS,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_60NS],
	},
	[S6E3FAC_DISPLAY_MODE_1080x2340_48NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2340_48NS,
		.resol = &s6e3fac_rainbow_g0_default_resol[S6E3FAC_RESOL_1080x2340],
		.vrr = &s6e3fac_rainbow_g0_default_panel_vrr[S6E3FAC_VRR_48NS],
	},
};

static struct common_panel_display_mode *s6e3fac_rainbow_g0_display_mode_array[MAX_S6E3FAC_DISPLAY_MODE] = {
	[S6E3FAC_DISPLAY_MODE_1080x2340_120HS] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_120HS],
	[S6E3FAC_DISPLAY_MODE_1080x2340_60HS_120HS_TE_HW_SKIP_1] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_60HS_120HS_TE_HW_SKIP_1],
	[S6E3FAC_DISPLAY_MODE_1080x2340_30HS_120HS_TE_HW_SKIP_3] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_30HS_120HS_TE_HW_SKIP_3],
	[S6E3FAC_DISPLAY_MODE_1080x2340_24HS_120HS_TE_HW_SKIP_4] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_24HS_120HS_TE_HW_SKIP_4],
	[S6E3FAC_DISPLAY_MODE_1080x2340_10HS_120HS_TE_HW_SKIP_11] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_10HS_120HS_TE_HW_SKIP_11],
	[S6E3FAC_DISPLAY_MODE_1080x2340_96HS] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_96HS],
	[S6E3FAC_DISPLAY_MODE_1080x2340_48HS_96HS_TE_HW_SKIP_1] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_48HS_96HS_TE_HW_SKIP_1],
	[S6E3FAC_DISPLAY_MODE_1080x2340_60HS] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_60HS],
	[S6E3FAC_DISPLAY_MODE_1080x2340_30HS_60HS_TE_HW_SKIP_1] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_30HS_60HS_TE_HW_SKIP_1],
	[S6E3FAC_DISPLAY_MODE_1080x2340_48HS] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_48HS],
	[S6E3FAC_DISPLAY_MODE_1080x2340_24HS_48HS_TE_HW_SKIP_1] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_24HS_48HS_TE_HW_SKIP_1],
	[S6E3FAC_DISPLAY_MODE_1080x2340_10HS_48HS_TE_HW_SKIP_4] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_10HS_48HS_TE_HW_SKIP_4],
	[S6E3FAC_DISPLAY_MODE_1080x2340_60NS] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_60NS],
	[S6E3FAC_DISPLAY_MODE_1080x2340_48NS] = &s6e3fac_rainbow_g0_display_mode[S6E3FAC_DISPLAY_MODE_1080x2340_48NS],
};

static struct common_panel_display_modes s6e3fac_rainbow_g0_display_modes = {
	.num_modes = ARRAY_SIZE(s6e3fac_rainbow_g0_display_mode_array),
	.modes = (struct common_panel_display_mode **)&s6e3fac_rainbow_g0_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __S6E3FAC_RAINBOW_G0_RESOL_H__ */
