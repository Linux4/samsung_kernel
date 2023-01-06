/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3ha9/s6e3ha9_davinci_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FA7_DAVINCI_RESOL_H__
#define __S6E3FA7_DAVINCI_RESOL_H__

#include "../panel.h"

static struct panel_resol s6e3fa7_davinci_resol[] = {
	{
		.w = 1080,
		.h = 2280,
		.comp_type = PN_COMP_TYPE_NONE,
	},
};

#endif /* __S6E3FA7_DAVINCI_RESOL_H__ */

