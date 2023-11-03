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

#ifndef __S6E3FC3_R11S_H__
#define __S6E3FC3_R11S_H__
#include "../panel.h"
#include "../panel_drv.h"
#include "s6e3fc3_dimming.h"

#if 0
enum s6e3fc3_r11s_function {
	MAX_S6E3FC3_R11S_FUNCTION,
};

extern struct pnobj_func s6e3fc3_r11s_function_table[MAX_S6E3FC3_R11S_FUNCTION];

#undef PANEL_FUNC
#define PANEL_FUNC(_index) (s6e3fc3_r11s_function_table[_index])
#else
#undef PANEL_FUNC
#endif

enum {
	S6E3FC3_R11S_HS_CLK_1328 = 0,
	S6E3FC3_R11S_HS_CLK_1362,
	S6E3FC3_R11S_HS_CLK_1368,
	MAX_S6E3FC3_R11S_HS_CLK
};

#define S6E3FC3_R11S_BR_INDEX_PROPERTY ("s6e3fc3_r11s_br_index")
#define S6E3FC3_R11S_LPM_BR_INDEX_PROPERTY ("s6e3fc3_r11s_lpm_br_index")
#define S6E3FC3_R11S_HS_CLK_PROPERTY ("s6e3fc3_r11s_hs_clk")

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
int s6e3fc3_r11s_decoder_test(struct panel_device *panel, void *data, u32 len);
#endif
int s6e3fc3_r11s_getidx_ffc_table(struct maptbl *tbl);

#define S6E3FC3_R11S_NR_LUMINANCE (256)
#define S6E3FC3_R11S_TARGET_LUMINANCE (500)
#define S6E3FC3_R11S_NR_HBM_LUMINANCE (255)
#define S6E3FC3_R11S_TARGET_HBM_LUMINANCE (1000)

#ifdef CONFIG_USDM_PANEL_AOD_BL
#define S6E3FC3_R11S_AOD_NR_LUMINANCE (4)
#define S6E3FC3_R11S_AOD_TARGET_LUMINANCE (60)
#endif

#define S6E3FC3_R11S_TOTAL_NR_LUMINANCE (S6E3FC3_R11S_NR_LUMINANCE + S6E3FC3_R11S_NR_HBM_LUMINANCE)

#endif /* __S6E3FC3_R11S_H__ */
