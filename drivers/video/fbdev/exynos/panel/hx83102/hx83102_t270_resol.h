/*
 * linux/drivers/video/fbdev/exynos/panel/hx83102/hx83102_daimler2_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HX83102_T270_RESOL_H__
#define __HX83102_T270_RESOL_H__

#include "../panel.h"

static struct panel_resol hx83102_t270_resol[] = {
	{
		.w = 720,
		.h = 1600,
		.comp_type = PN_COMP_TYPE_NONE,
	},
};
#endif /* __HX83102_T270_RESOL_H__ */
