/*
 * linux/drivers/video/fbdev/exynos/panel/ana6705/ana6705_a71x_aod_panel.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA6705_A71X_AOD_PANEL_H__
#define __ANA6705_A71X_AOD_PANEL_H__

#include "ana6705_aod.h"
#include "ana6705_aod_panel.h"

/* A71X */
static struct seqinfo ana6705_a71x_aod_seqtbl[MAX_AOD_SEQ] = {
	[ENTER_AOD_SEQ] = SEQINFO_INIT("enter_aod", ana6705_aod_enter_aod_cmdtbl),
	[EXIT_AOD_SEQ] = SEQINFO_INIT("exit_aod", ana6705_aod_exit_aod_cmdtbl),
};

static struct aod_tune ana6705_a71x_aod = {
	.name = "ana6705_a71x_aod",
	.nr_seqtbl = ARRAY_SIZE(ana6705_a71x_aod_seqtbl),
	.seqtbl = ana6705_a71x_aod_seqtbl,
	.nr_maptbl = ARRAY_SIZE(ana6705_aod_maptbl),
	.maptbl = ana6705_aod_maptbl,
	.self_mask_en = false,
};
#endif //__ANA6705_A71X_AOD_PANEL_H__
