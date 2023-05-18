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

#ifndef __S6E3FC3_A24_H__
#define __S6E3FC3_A24_H__
#include "../panel.h"
#include "../panel_drv.h"

enum {
	S6E3FC3_A24_HS_CLK_806 = 0,
	S6E3FC3_A24_HS_CLK_822,
	S6E3FC3_A24_HS_CLK_824,
	MAX_S6E3FC3_A24_HS_CLK
};

#ifdef CONFIG_SUPPORT_PANEL_DECODER_TEST
int s6e3fc3_a24_decoder_test(struct panel_device *panel, void *data, u32 len);
#endif
int s6e3fc3_a24_getidx_ffc_table(struct maptbl *tbl);
__visible_for_testing int getidx_irc_mode_table(struct maptbl *tbl);

#endif /* __S6E3FC3_A24_H__ */
