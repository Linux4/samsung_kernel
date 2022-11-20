/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hac/s6e3hac_canvas2_mafpc_panel.h
 *
 * Header file for mAFPC Driver
 *
 * Copyright (c) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAC_CANVAS2_MAFPC_PANEL_H__
#define __S6E3HAC_CANVAS2_MAFPC_PANEL_H__

#include "s6e3hac_dimming.h"
#include "s6e3hac_canvas2_mafpc_data.h"

char S6E3HAC_CANVAS2_MAFPC_DEFAULT_SCALE_FACTOR[225] = {
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF,
};


static u8 s6e3hac_canvas2_mafpc_crc_value[] = {
	0xA2, 0x39
};

static u8 s6e3hac_canvas2_mafpc_scale_map_tbl[S6E3HAC_CANVAS2_TOTAL_NR_LUMINANCE] = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 10, 11, 12, 13, 13, 14, 14, 15, 16,
    17, 18, 18, 19, 19, 20, 21, 22, 22, 22,
    23, 23, 24, 24, 25, 25, 26, 26, 27, 27,
    27, 28, 28, 28, 29, 29, 30, 30, 30, 30,
    31, 31, 31, 32, 32, 32, 32, 33, 33, 33,
    34, 34, 34, 34, 35, 35, 35, 35, 36, 36,
    36, 36, 36, 37, 37, 37, 37, 38, 38, 38,
    38, 38, 38, 39, 39, 39, 39, 39, 40, 40,
    40, 40, 40, 41, 41, 41, 41, 41, 41, 41,
    42, 42, 42, 42, 42, 42, 43, 43, 43, 43,
    43, 43, 43, 44, 44, 44, 44, 44, 44, 44,
    45, 45, 45, 45, 45, 45, 45, 45, 46, 46,
    46, 46, 46, 46, 47, 47, 47, 47, 47, 47,
    47, 48, 48, 48, 48, 48, 48, 48, 49, 49,
    49, 49, 49, 49, 49, 49, 50, 50, 50, 50,
    51, 51, 51, 51, 52, 52, 52, 52, 53, 53,
    53, 53, 53, 54, 54, 54, 54, 55, 55, 55,
    55, 55, 56, 56, 56, 56, 56, 56, 56, 56,
    56, 56, 57, 57, 57, 57, 57, 57, 57, 57,
    58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
    59, 59, 59, 59, 59, 59, 59, 59, 60, 61,
    61, 62, 62, 63, 63, 64, 64, 65, 65, 66,
    66, 67, 67, 68, 68, 69, 69, 69, 69, 69,
    70, 70, 70, 70, 70, 71, 71, 71, 71, 71,
    72, 72, 72, 72, 72, 73,
    /* hbm 8x31+7 */
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 74, 74, 74,
};


static struct mafpc_info s6e3hac_canvas2_mafpc = {
	.name = "s6e3hac_canvas2_mafpc",
	.mafpc_img = S6E3HAC_MAFPC_DEFAULT_IMG,
	.mafpc_img_len = ARRAY_SIZE(S6E3HAC_MAFPC_DEFAULT_IMG),
	.mafpc_scale_factor = S6E3HAC_CANVAS2_MAFPC_DEFAULT_SCALE_FACTOR,
	.mafpc_scale_factor_len = ARRAY_SIZE(S6E3HAC_CANVAS2_MAFPC_DEFAULT_SCALE_FACTOR),
	.mafpc_crc_value = s6e3hac_canvas2_mafpc_crc_value,
	.mafpc_crc_value_len = ARRAY_SIZE(s6e3hac_canvas2_mafpc_crc_value),
    .mafpc_scale_map_br_tbl = s6e3hac_canvas2_mafpc_scale_map_tbl,
    .mafpc_scale_map_br_tbl_len = S6E3HAC_CANVAS2_TOTAL_NR_LUMINANCE,
};

#endif //__S6E3HAC_CANVAS2_MAFPC_PANEL_H__

