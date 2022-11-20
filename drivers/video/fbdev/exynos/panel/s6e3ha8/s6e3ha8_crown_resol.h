/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3ha8/s6e3ha8_crown_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HA8_CROWN_RESOL_H__
#define __S6E3HA8_CROWN_RESOL_H__

#include "../panel.h"

static struct panel_resol s6e3ha8_crown_resol[] = {
	{
		.w = 1440,
		.h = 3040,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 720,
				.slice_h = 40,
			},
		},
	},
	{
		.w = 1080,
		.h = 2280,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 540,
				.slice_h = 30,
			},
		},
	},
	{
		.w = 720,
		.h = 1520,
		.comp_type = PN_COMP_TYPE_DSC,
		.comp_param = {
			.dsc = {
				.slice_w = 360,
				.slice_h = 74,
			},
		},
	},
};
#endif /* __S6E3HA8_CROWN_RESOL_H__ */
