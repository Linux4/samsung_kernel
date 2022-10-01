/*
 * linux/drivers/video/fbdev/exynos/panel/nt36675/nt36675_m12_resol.h
 *
 * Header file for Panel Driver
 *
 * Copyright (c) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NT36675_M12_RESOL_H__
#define __NT36675_M12_RESOL_H__

#include "../panel.h"

static struct panel_resol nt36675_m12_resol[] = {
	{
		.w = 720,
		.h = 1600,
		.comp_type = PN_COMP_TYPE_NONE,
	},
};
#endif /* __NT36675_M12_RESOL_H__ */
