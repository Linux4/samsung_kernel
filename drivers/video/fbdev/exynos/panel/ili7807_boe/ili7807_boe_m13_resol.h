/*
 * linux/drivers/video/fbdev/exynos/panel/ili7807_boe/ili7807_boe_m13_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ILI7807_M13_RESOL_H__
#define __ILI7807_M13_RESOL_H__

#include "../panel.h"

static struct panel_resol ili7807_boe_m13_resol[] = {
	{
		.w = 1080,
		.h = 2408,
		.comp_type = PN_COMP_TYPE_NONE,
	},
};
#endif /* __ILI7807_M13_RESOL_H__ */
