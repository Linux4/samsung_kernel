/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3xa0/s6e3xa0_win2_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3XA0_WIN2_RESOL_H__
#define __S6E3XA0_WIN2_RESOL_H__

#include "../panel.h"
static struct panel_vrr *s6e3xa0_win2_variable_refresh_rate[] = {
	&S6E3XA0_VRR[S6E3XA0_VRR_MODE_NORMAL],
};

static struct panel_resol s6e3xa0_win2_resol[] = {
	{
		.w = 2160,
		.h = 1536,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 1080,
				.slice_h = 32,
			},
		},
		.available_vrr = s6e3xa0_win2_variable_refresh_rate,
		.nr_available_vrr = ARRAY_SIZE(s6e3xa0_win2_variable_refresh_rate),
	},
	{
		.w = 2160,
		.h = 1536,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 1080,
				.slice_h = 32,
			},
		},
		.available_vrr = s6e3xa0_win2_variable_refresh_rate,
		.nr_available_vrr = ARRAY_SIZE(s6e3xa0_win2_variable_refresh_rate),
	},
	{
		.w = 2160,
		.h = 1536,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 1080,
				.slice_h = 32,
			},
		},
		.available_vrr = s6e3xa0_win2_variable_refresh_rate,
		.nr_available_vrr = ARRAY_SIZE(s6e3xa0_win2_variable_refresh_rate),
	},
};
#endif /* __S6E3XA0_WIN2_RESOL_H__ */
