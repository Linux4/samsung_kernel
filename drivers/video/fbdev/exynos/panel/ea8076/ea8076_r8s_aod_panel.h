/*
 * linux/drivers/video/fbdev/exynos/panel/ea8076/ea8076_r8s_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8076_R8S_AOD_PANEL_H__
#define __EA8076_R8S_AOD_PANEL_H__

static struct aod_tune ea8076_r8s_aod = {
	.name = "ea8076_r8s_aod",
	.nr_seqtbl = 0,
	.seqtbl = NULL,
	.nr_maptbl = 0,
	.maptbl = NULL,
	.self_mask_en = false,
};
#endif //__EA8076_R8S_AOD_PANEL_H__
