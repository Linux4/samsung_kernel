/*
 * linux/drivers/video/fbdev/exynos/panel/ea8076/ea8076_a21s_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8076_A21S_AOD_PANEL_H__
#define __EA8076_A21S_AOD_PANEL_H__

#include "ea8076_aod.h"
#include "ea8076_aod_panel.h"

/* A21S */
static struct seqinfo ea8076_a21s_aod_seqtbl[MAX_AOD_SEQ] = {
	[ENTER_AOD_SEQ] = SEQINFO_INIT("enter_aod", ea8076_aod_enter_aod_cmdtbl),
	[EXIT_AOD_SEQ] = SEQINFO_INIT("exit_aod", ea8076_aod_exit_aod_cmdtbl),
};

static struct aod_tune ea8076_a21s_aod = {
	.name = "ea8076_a21s_aod",
	.nr_seqtbl = ARRAY_SIZE(ea8076_a21s_aod_seqtbl),
	.seqtbl = ea8076_a21s_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(ea8076_aod_maptbl),
	.maptbl = ea8076_aod_maptbl,
	.self_mask_en = true,
};
#endif //__EA8076_A21S_AOD_PANEL_H__
