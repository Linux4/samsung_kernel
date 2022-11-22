/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fa9/s6e3fa9_a71x_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FA9_A71X_RESOL_H__
#define __S6E3FA9_A71X_RESOL_H__

#include "../panel.h"

static struct panel_resol s6e3fa9_a71x_resol[] = {
	{
		.w = 1080,
		.h = 2400,
		.comp_type = PN_COMP_TYPE_NONE,
	},
};
#endif /* __S6E3FA9_A71X_RESOL_H__ */
