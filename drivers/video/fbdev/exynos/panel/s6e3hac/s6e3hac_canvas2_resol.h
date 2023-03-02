/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hac/s6e3hac_canvas_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAC_CANVAS_RESOL_H__
#define __S6E3HAC_CANVAS_RESOL_H__

#include <dt-bindings/display/panel-display.h>
#include "../panel.h"
#include "s6e3hac.h"
#include "s6e3hac_dimming.h"

struct panel_vrr s6e3hac_canvas_default_panel_vrr[] = {
	[S6E3HAC_VRR_60NS] = {
		.fps = 60,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
	[S6E3HAC_VRR_48NS] = {
		.fps = 48,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
	[S6E3HAC_VRR_120HS] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3HAC_VRR_96HS] = {
		.fps = 96,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = {
		.fps = 120,
		.te_sw_skip_count = 1,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = {
		.fps = 96,
		.te_sw_skip_count = 1,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = {
		.fps = 120,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = {
		.fps = 96,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *s6e3hac_canvas_default_wqhd_vrrtbl[] = {
	&s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60NS],
	&s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48NS],
};

static struct panel_vrr *s6e3hac_canvas_default_vrrtbl[] = {
	&s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60NS],
	&s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48NS],
	&s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_120HS],
	&s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_96HS],
	&s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1],
	&s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1],
	&s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1],
	&s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1],
};

static struct panel_resol s6e3hac_canvas_default_resol[] = {
	[S6E3HAC_RESOL_1440x3088] = {
		.w = 1440,
		.h = 3088,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 720,
				.slice_h = 193,
			},
		},
		.available_vrr = s6e3hac_canvas_default_wqhd_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hac_canvas_default_wqhd_vrrtbl),
	},
	[S6E3HAC_RESOL_1080x2316] = {
		.w = 1080,
		.h = 2316,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 193,
			},
		},
		.available_vrr = s6e3hac_canvas_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hac_canvas_default_vrrtbl),
	},
	[S6E3HAC_RESOL_720x1544] = {
		.w = 720,
		.h = 1544,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 360,
				.slice_h = 193,
			},
		},
		.available_vrr = s6e3hac_canvas_default_vrrtbl,
		.nr_available_vrr = ARRAY_SIZE(s6e3hac_canvas_default_vrrtbl),
	},
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static struct common_panel_display_mode s6e3hac_canvas2_display_mode[] = {
	/* WQHD */
	[S6E3HAC_DISPLAY_MODE_1440x3088_60NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3088_60NS,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_1440x3088],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60NS],
	},
	[S6E3HAC_DISPLAY_MODE_1440x3088_48NS] = {
		.name = PANEL_DISPLAY_MODE_1440x3088_48NS,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_1440x3088],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48NS],
	},

	/* FHD */
	[S6E3HAC_DISPLAY_MODE_1080x2316_120HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2316_120HS,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_1080x2316],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_120HS],
	},
	[S6E3HAC_DISPLAY_MODE_1080x2316_96HS] = {
		.name = PANEL_DISPLAY_MODE_1080x2316_96HS,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_1080x2316],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_96HS],
	},
	[S6E3HAC_DISPLAY_MODE_1080x2316_60HS_120HS_TE_SW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2316_60HS_120HS_TE_SW_SKIP_1,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_1080x2316],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1],
	},
	[S6E3HAC_DISPLAY_MODE_1080x2316_48HS_96HS_TE_SW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2316_48HS_96HS_TE_SW_SKIP_1,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_1080x2316],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1],
	},
	[S6E3HAC_DISPLAY_MODE_1080x2316_60HS_120HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2316_60HS_120HS_TE_HW_SKIP_1,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_1080x2316],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1],
	},
	[S6E3HAC_DISPLAY_MODE_1080x2316_48HS_96HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_1080x2316_48HS_96HS_TE_HW_SKIP_1,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_1080x2316],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1],
	},
	[S6E3HAC_DISPLAY_MODE_1080x2316_60NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2316_60NS,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_1080x2316],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60NS],
	},
	[S6E3HAC_DISPLAY_MODE_1080x2316_48NS] = {
		.name = PANEL_DISPLAY_MODE_1080x2316_48NS,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_1080x2316],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48NS],
	},

	/* HD */
	[S6E3HAC_DISPLAY_MODE_720x1544_120HS] = {
		.name = PANEL_DISPLAY_MODE_720x1544_120HS,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_720x1544],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_120HS],
	},
	[S6E3HAC_DISPLAY_MODE_720x1544_96HS] = {
		.name = PANEL_DISPLAY_MODE_720x1544_96HS,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_720x1544],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_96HS],
	},
	[S6E3HAC_DISPLAY_MODE_720x1544_60HS_120HS_TE_SW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_720x1544_60HS_120HS_TE_SW_SKIP_1,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_720x1544],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1],
	},
	[S6E3HAC_DISPLAY_MODE_720x1544_48HS_96HS_TE_SW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_720x1544_48HS_96HS_TE_SW_SKIP_1,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_720x1544],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1],
	},
	[S6E3HAC_DISPLAY_MODE_720x1544_60HS_120HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_720x1544_60HS_120HS_TE_HW_SKIP_1,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_720x1544],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1],
	},
	[S6E3HAC_DISPLAY_MODE_720x1544_48HS_96HS_TE_HW_SKIP_1] = {
		.name = PANEL_DISPLAY_MODE_720x1544_48HS_96HS_TE_HW_SKIP_1,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_720x1544],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1],
	},
	[S6E3HAC_DISPLAY_MODE_720x1544_60NS] = {
		.name = PANEL_DISPLAY_MODE_720x1544_60NS,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_720x1544],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_60NS],
	},
	[S6E3HAC_DISPLAY_MODE_720x1544_48NS] = {
		.name = PANEL_DISPLAY_MODE_720x1544_48NS,
		.resol = &s6e3hac_canvas_default_resol[S6E3HAC_RESOL_720x1544],
		.vrr = &s6e3hac_canvas_default_panel_vrr[S6E3HAC_VRR_48NS],
	},
};

static struct common_panel_display_mode *s6e3hac_canvas2_display_mode_array[] = {
	[S6E3HAC_DISPLAY_MODE_1440x3088_60NS] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_1440x3088_60NS],
	[S6E3HAC_DISPLAY_MODE_1440x3088_48NS] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_1440x3088_48NS],
	[S6E3HAC_DISPLAY_MODE_1080x2316_120HS] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_1080x2316_120HS],
	[S6E3HAC_DISPLAY_MODE_1080x2316_96HS] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_1080x2316_96HS],
	[S6E3HAC_DISPLAY_MODE_1080x2316_60HS_120HS_TE_SW_SKIP_1] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_1080x2316_60HS_120HS_TE_SW_SKIP_1],
	[S6E3HAC_DISPLAY_MODE_1080x2316_48HS_96HS_TE_SW_SKIP_1] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_1080x2316_48HS_96HS_TE_SW_SKIP_1],
	[S6E3HAC_DISPLAY_MODE_1080x2316_60HS_120HS_TE_HW_SKIP_1] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_1080x2316_60HS_120HS_TE_HW_SKIP_1],
	[S6E3HAC_DISPLAY_MODE_1080x2316_48HS_96HS_TE_HW_SKIP_1] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_1080x2316_48HS_96HS_TE_HW_SKIP_1],
	[S6E3HAC_DISPLAY_MODE_1080x2316_60NS] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_1080x2316_60NS],
	[S6E3HAC_DISPLAY_MODE_1080x2316_48NS] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_1080x2316_48NS],
	[S6E3HAC_DISPLAY_MODE_720x1544_120HS] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_720x1544_120HS],
	[S6E3HAC_DISPLAY_MODE_720x1544_96HS] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_720x1544_96HS],
	[S6E3HAC_DISPLAY_MODE_720x1544_60HS_120HS_TE_SW_SKIP_1] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_720x1544_60HS_120HS_TE_SW_SKIP_1],
	[S6E3HAC_DISPLAY_MODE_720x1544_48HS_96HS_TE_SW_SKIP_1] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_720x1544_48HS_96HS_TE_SW_SKIP_1],
	[S6E3HAC_DISPLAY_MODE_720x1544_60HS_120HS_TE_HW_SKIP_1] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_720x1544_60HS_120HS_TE_HW_SKIP_1],
	[S6E3HAC_DISPLAY_MODE_720x1544_48HS_96HS_TE_HW_SKIP_1] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_720x1544_48HS_96HS_TE_HW_SKIP_1],
	[S6E3HAC_DISPLAY_MODE_720x1544_60NS] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_720x1544_60NS],
	[S6E3HAC_DISPLAY_MODE_720x1544_48NS] = &s6e3hac_canvas2_display_mode[S6E3HAC_DISPLAY_MODE_720x1544_48NS],
};

static struct common_panel_display_modes s6e3hac_canvas2_display_modes = {
	.num_modes = ARRAY_SIZE(s6e3hac_canvas2_display_mode),
	.modes = (struct common_panel_display_mode **)&s6e3hac_canvas2_display_mode_array,
};
#endif /* CONFIG_PANEL_DISPLAY_MODE */
#endif /* __S6E3HAC_CANVAS_RESOL_H__ */
