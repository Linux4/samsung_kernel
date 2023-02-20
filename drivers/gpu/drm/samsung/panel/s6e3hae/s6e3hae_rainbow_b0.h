/*
 * linux/drivers/video/fbdev/exynos/panel/nt36672c_m33x_00/nt36672c_m33_00.h
 *
 * Header file for TFT_COMMON Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAE_RAINBOW_B0_H__
#define __S6E3HAE_RAINBOW_B0_H__
#include "../panel.h"
#include "../panel_drv.h"
#include "../panel_debug.h"


static inline bool is_id_lt_81(struct panel_device *panel)
{
	if (!panel)
		return false;

	return ((panel->panel_data.id[2] & 0xFF) < 0x81) ? true : false;
}

static inline bool is_id_lt_90(struct panel_device *panel)
{
	if (!panel)
		return false;

	return ((panel->panel_data.id[2] & 0xFF) < 0x90) ? true : false;
}

void copy_gamma_mode2_brt_maptbl(struct maptbl *tbl, u8 *dst);

#endif /* __S6E3HAE_RAINBOW_B0_H__ */
