/*
 * =================================================================
 *
 *
 *	Description:  samsung display common file
 *
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2020, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
*/
#ifndef _SAMSUNG_DSI_MDNIE_S6E8FC1_AMS660XR01_
#define _SAMSUNG_DSI_MDNIE_S6E8FC1_AMS660XR01_

#include "ss_dsi_mdnie_lite_common.h"

#define MDNIE_COLOR_BLINDE_CMD_OFFSET 2

#define ADDRESS_SCR_WHITE_RED   0x14
#define ADDRESS_SCR_WHITE_GREEN 0x15
#define ADDRESS_SCR_WHITE_BLUE  0x16

#define MDNIE_STEP1_INDEX 2
#define MDNIE_STEP2_INDEX 2

static char level_1_key_on[] = {
	0xF0,
	0x5A, 0x5A
};

static char level_1_key_off[] = {
	0xF0,
	0xA5, 0xA5
};

static char adjust_ldu_data_1[] = {
	0xff, 0xff, 0xff,
	0xfd, 0xfd, 0xff,
	0xfb, 0xfb, 0xff,
	0xf9, 0xf8, 0xff,
	0xf6, 0xf6, 0xff,
	0xf4, 0xf4, 0xff,
};

static char adjust_ldu_data_2[] = {
	0xff, 0xfc, 0xf6,
	0xfd, 0xfa, 0xf6,
	0xfb, 0xf7, 0xf6,
	0xf9, 0xf5, 0xf6,
	0xf4, 0xf0, 0xf6,
	0xf2, 0xee, 0xf6,
};

static char *adjust_ldu_data[MAX_MODE] = {
	adjust_ldu_data_2,
	adjust_ldu_data_2,
	adjust_ldu_data_2,
	adjust_ldu_data_1,
	adjust_ldu_data_1,
};

static char night_mode_data[] = {
	0xff, 0x00, 0x00, 0x00, 0xf9, 0x00, 0x00, 0x00, 0xee, 0x00, 0xf9, 0xee, 0xff, 0x00, 0xee, 0xff, 0xf9, 0x00, 0xff, 0xf9, 0xee, 0xff, 0x00, 0x00, /* 6500K */
	0xff, 0x00, 0x00, 0x00, 0xf7, 0x00, 0x00, 0x00, 0xe6, 0x00, 0xf7, 0xe6, 0xff, 0x00, 0xe6, 0xff, 0xf7, 0x00, 0xff, 0xf7, 0xe6, 0xff, 0x00, 0x00, /* 6300K */
	0xff, 0x00, 0x00, 0x00, 0xf3, 0x00, 0x00, 0x00, 0xde, 0x00, 0xf3, 0xde, 0xff, 0x00, 0xde, 0xff, 0xf3, 0x00, 0xff, 0xf3, 0xde, 0xff, 0x00, 0x00, /* 6100K */
	0xff, 0x00, 0x00, 0x00, 0xef, 0x00, 0x00, 0x00, 0xd6, 0x00, 0xef, 0xd6, 0xff, 0x00, 0xd6, 0xff, 0xef, 0x00, 0xff, 0xef, 0xd6, 0xff, 0x00, 0x00, /* 5900K */
	0xff, 0x00, 0x00, 0x00, 0xec, 0x00, 0x00, 0x00, 0xcd, 0x00, 0xec, 0xcd, 0xff, 0x00, 0xcd, 0xff, 0xec, 0x00, 0xff, 0xec, 0xcd, 0xff, 0x00, 0x00, /* 5700K */
	0xff, 0x00, 0x00, 0x00, 0xe8, 0x00, 0x00, 0x00, 0xc5, 0x00, 0xe8, 0xc5, 0xff, 0x00, 0xc5, 0xff, 0xe8, 0x00, 0xff, 0xe8, 0xc5, 0xff, 0x00, 0x00, /* 5500K */
	0xff, 0x00, 0x00, 0x00, 0xdd, 0x00, 0x00, 0x00, 0xaa, 0x00, 0xdd, 0xaa, 0xff, 0x00, 0xaa, 0xff, 0xdd, 0x00, 0xff, 0xdd, 0xaa, 0xff, 0x00, 0x00, /* 4900K */
	0xff, 0x00, 0x00, 0x00, 0xcd, 0x00, 0x00, 0x00, 0x87, 0x00, 0xcd, 0x87, 0xff, 0x00, 0x87, 0xff, 0xcd, 0x00, 0xff, 0xcd, 0x87, 0xff, 0x00, 0x00, /* 4300K */
	0xff, 0x00, 0x00, 0x00, 0xb9, 0x00, 0x00, 0x00, 0x62, 0x00, 0xb9, 0x62, 0xff, 0x00, 0x62, 0xff, 0xb9, 0x00, 0xff, 0xb9, 0x62, 0xff, 0x00, 0x00, /* 3700K */
	0xff, 0x00, 0x00, 0x00, 0xa4, 0x00, 0x00, 0x00, 0x42, 0x00, 0xa4, 0x42, 0xff, 0x00, 0x42, 0xff, 0xa4, 0x00, 0xff, 0xa4, 0x42, 0xff, 0x00, 0x00, /* 3100K */
	0xff, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x78, 0x1a, 0xff, 0x00, 0x1a, 0xff, 0x78, 0x00, 0xff, 0x78, 0x1a, 0xff, 0x00, 0x00, /* 2300K */
	0xc6, 0x00, 0x00, 0x10, 0xf9, 0x00, 0x06, 0x06, 0xcb, 0x19, 0xf0, 0xd9, 0xe2, 0x00, 0xcd, 0xea, 0xda, 0x02, 0xff, 0xf9, 0xee, 0xff, 0x00, 0x00, /* 6500K */
	0xc6, 0x00, 0x00, 0x10, 0xf7, 0x00, 0x06, 0x06, 0xc5, 0x19, 0xee, 0xd1, 0xe2, 0x00, 0xc6, 0xea, 0xd8, 0x02, 0xff, 0xf7, 0xe6, 0xff, 0x00, 0x00, /* 6300K */
	0xc6, 0x00, 0x00, 0x10, 0xf3, 0x00, 0x06, 0x06, 0xbe, 0x19, 0xea, 0xca, 0xe2, 0x00, 0xc0, 0xea, 0xd5, 0x02, 0xff, 0xf3, 0xde, 0xff, 0x00, 0x00, /* 6100K */
	0xc6, 0x00, 0x00, 0x10, 0xef, 0x00, 0x06, 0x06, 0xb7, 0x19, 0xe7, 0xc3, 0xe2, 0x00, 0xb9, 0xea, 0xd1, 0x02, 0xff, 0xef, 0xd6, 0xff, 0x00, 0x00, /* 5900K */
	0xc6, 0x00, 0x00, 0x10, 0xec, 0x00, 0x06, 0x06, 0xaf, 0x19, 0xe4, 0xbb, 0xe2, 0x00, 0xb1, 0xea, 0xce, 0x02, 0xff, 0xec, 0xcd, 0xff, 0x00, 0x00, /* 5700K */
	0xc6, 0x00, 0x00, 0x10, 0xe8, 0x00, 0x06, 0x05, 0xa8, 0x19, 0xe0, 0xb3, 0xe2, 0x00, 0xaa, 0xea, 0xcb, 0x02, 0xff, 0xe8, 0xc5, 0xff, 0x00, 0x00, /* 5500K */
	0xc6, 0x00, 0x00, 0x10, 0xdd, 0x00, 0x06, 0x05, 0x91, 0x19, 0xd5, 0x9b, 0xe2, 0x00, 0x93, 0xea, 0xc1, 0x01, 0xff, 0xdd, 0xaa, 0xff, 0x00, 0x00, /* 4900K */
	0xc6, 0x00, 0x00, 0x10, 0xcd, 0x00, 0x06, 0x05, 0x73, 0x19, 0xc6, 0x7b, 0xe2, 0x00, 0x74, 0xea, 0xb3, 0x01, 0xff, 0xcd, 0x87, 0xff, 0x00, 0x00, /* 4300K */
	0xc6, 0x00, 0x00, 0x10, 0xb9, 0x00, 0x06, 0x04, 0x54, 0x19, 0xb2, 0x59, 0xe2, 0x00, 0x55, 0xea, 0xa2, 0x01, 0xff, 0xb9, 0x62, 0xff, 0x00, 0x00, /* 3700K */
	0xc6, 0x00, 0x00, 0x10, 0xa4, 0x00, 0x06, 0x04, 0x38, 0x19, 0x9e, 0x3c, 0xe2, 0x00, 0x39, 0xea, 0x8f, 0x01, 0xff, 0xa4, 0x42, 0xff, 0x00, 0x00, /* 3100K */
	0xc6, 0x00, 0x00, 0x10, 0x78, 0x00, 0x06, 0x03, 0x16, 0x19, 0x74, 0x18, 0xe2, 0x00, 0x16, 0xea, 0x69, 0x00, 0xff, 0x78, 0x1a, 0xff, 0x00, 0x00, /* 2300K */
};

static char DSI0_BYPASS_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x00, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_BYPASS_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xff, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x00, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x00, // crc_lut_mode1_br
	0x00, // crc_lut_mode1_bg
	0xff, // crc_lut_mode1_bb
	0x00, // crc_lut_mode1_cr
	0xf0, // crc_lut_mode1_cg
	0xff, // crc_lut_mode1_cb
	0xff, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xff, // crc_lut_mode1_mb
	0xff, // crc_lut_mode1_yr
	0xff, // crc_lut_mode1_yg
	0x00, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xff, // crc_lut_mode1_wg
	0xff, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_COLOR_BLIND_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_COLOR_BLIND_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xff, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x00, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x00, // crc_lut_mode1_br
	0x00, // crc_lut_mode1_bg
	0xff, // crc_lut_mode1_bb
	0x00, // crc_lut_mode1_cr
	0xff, // crc_lut_mode1_cg
	0xff, // crc_lut_mode1_cb
	0xff, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xff, // crc_lut_mode1_mb
	0xff, // crc_lut_mode1_yr
	0xff, // crc_lut_mode1_yg
	0x00, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xff, // crc_lut_mode1_wg
	0xff, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_NIGHT_MODE_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_NIGHT_MODE_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xff, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x00, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x00, // crc_lut_mode1_br
	0x00, // crc_lut_mode1_bg
	0xff, // crc_lut_mode1_bb
	0x00, // crc_lut_mode1_cr
	0xff, // crc_lut_mode1_cg
	0xff, // crc_lut_mode1_cb
	0xff, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xff, // crc_lut_mode1_mb
	0xff, // crc_lut_mode1_yr
	0xff, // crc_lut_mode1_yg
	0x00, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xff, // crc_lut_mode1_wg
	0xff, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xff, // crc_lut_mode1_rr
	0x60, // crc_lut_mode1_rg
	0x13, // crc_lut_mode1_rb
	0x66, // crc_lut_mode1_gr
	0xf9, // crc_lut_mode1_gg
	0x13, // crc_lut_mode1_gb
	0x66, // crc_lut_mode1_br
	0x60, // crc_lut_mode1_bg
	0xac, // crc_lut_mode1_bb
	0x66, // crc_lut_mode1_cr
	0xf9, // crc_lut_mode1_cg
	0xac, // crc_lut_mode1_cb
	0xff, // crc_lut_mode1_mr
	0x60, // crc_lut_mode1_mg
	0xac, // crc_lut_mode1_mb
	0xff, // crc_lut_mode1_yr
	0xf9, // crc_lut_mode1_yg
	0x13, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xf9, // crc_lut_mode1_wg
	0xac, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_SCREEN_CURTAIN_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_SCREEN_CURTAIN_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0x00, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x00, // crc_lut_mode1_gr
	0x00, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x00, // crc_lut_mode1_br
	0x00, // crc_lut_mode1_bg
	0x00, // crc_lut_mode1_bb
	0x00, // crc_lut_mode1_cr
	0x00, // crc_lut_mode1_cg
	0x00, // crc_lut_mode1_cb
	0x00, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0x00, // crc_lut_mode1_mb
	0x00, // crc_lut_mode1_yr
	0x00, // crc_lut_mode1_yg
	0x00, // crc_lut_mode1_yb
	0x00, // crc_lut_mode1_wr
	0x00, // crc_lut_mode1_wg
	0x00, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_UI_DYNAMIC_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_UI_DYNAMIC_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xc6, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x10, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x06, // crc_lut_mode1_br
	0x06, // crc_lut_mode1_bg
	0xda, // crc_lut_mode1_bb
	0x19, // crc_lut_mode1_cr
	0xf6, // crc_lut_mode1_cg
	0xe8, // crc_lut_mode1_cb
	0xe2, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xdc, // crc_lut_mode1_mb
	0xea, // crc_lut_mode1_yr
	0xdf, // crc_lut_mode1_yg
	0x02, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xfa, // crc_lut_mode1_wg
	0xf0, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x12, // crc_lut_mode2_gr
	0xdf, // crc_lut_mode2_gg
	0x02, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_UI_NATURAL_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_UI_NATURAL_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xae, // crc_lut_mode1_rr
	0x04, // crc_lut_mode1_rg
	0x04, // crc_lut_mode1_rb
	0x3e, // crc_lut_mode1_gr
	0xd7, // crc_lut_mode1_gg
	0x13, // crc_lut_mode1_gb
	0x06, // crc_lut_mode1_br
	0x07, // crc_lut_mode1_bg
	0xc2, // crc_lut_mode1_bb
	0x4c, // crc_lut_mode1_cr
	0xeb, // crc_lut_mode1_cg
	0xe4, // crc_lut_mode1_cb
	0xbc, // crc_lut_mode1_mr
	0x0a, // crc_lut_mode1_mg
	0xd2, // crc_lut_mode1_mb
	0xec, // crc_lut_mode1_yr
	0xe4, // crc_lut_mode1_yg
	0x18, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xfa, // crc_lut_mode1_wg
	0xf0, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_UI_AUTO_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_UI_AUTO_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xff, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x00, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x00, // crc_lut_mode1_br
	0x00, // crc_lut_mode1_bg
	0xff, // crc_lut_mode1_bb
	0x00, // crc_lut_mode1_cr
	0xff, // crc_lut_mode1_cg
	0xff, // crc_lut_mode1_cb
	0xff, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xff, // crc_lut_mode1_mb
	0xff, // crc_lut_mode1_yr
	0xff, // crc_lut_mode1_yg
	0x00, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xff, // crc_lut_mode1_wg
	0xff, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_GALLERY_AUTO_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_GALLERY_AUTO_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xff, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x14, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x00, // crc_lut_mode1_br
	0x00, // crc_lut_mode1_bg
	0xff, // crc_lut_mode1_bb
	0x00, // crc_lut_mode1_cr
	0xf0, // crc_lut_mode1_cg
	0xff, // crc_lut_mode1_cb
	0xff, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xff, // crc_lut_mode1_mb
	0xff, // crc_lut_mode1_yr
	0xff, // crc_lut_mode1_yg
	0x00, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xff, // crc_lut_mode1_wg
	0xff, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_EBOOK_AUTO_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_EBOOK_AUTO_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xff, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x14, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x00, // crc_lut_mode1_br
	0x00, // crc_lut_mode1_bg
	0xff, // crc_lut_mode1_bb
	0x00, // crc_lut_mode1_cr
	0xf0, // crc_lut_mode1_cg
	0xff, // crc_lut_mode1_cb
	0xff, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xff, // crc_lut_mode1_mb
	0xff, // crc_lut_mode1_yr
	0xff, // crc_lut_mode1_yg
	0x00, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xef, // crc_lut_mode1_wg
	0xd6, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_HBM_CE_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_HBM_CE_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xff, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x00, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x00, // crc_lut_mode1_br
	0x00, // crc_lut_mode1_bg
	0xff, // crc_lut_mode1_bb
	0x00, // crc_lut_mode1_cr
	0xff, // crc_lut_mode1_cg
	0xff, // crc_lut_mode1_cb
	0xff, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xff, // crc_lut_mode1_mb
	0xff, // crc_lut_mode1_yr
	0xff, // crc_lut_mode1_yg
	0x00, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xff, // crc_lut_mode1_wg
	0xff, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_HBM_CE_D65_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_HBM_CE_D65_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xff, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x00, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x00, // crc_lut_mode1_br
	0x00, // crc_lut_mode1_bg
	0xff, // crc_lut_mode1_bb
	0x00, // crc_lut_mode1_cr
	0xff, // crc_lut_mode1_cg
	0xff, // crc_lut_mode1_cb
	0xff, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xff, // crc_lut_mode1_mb
	0xff, // crc_lut_mode1_yr
	0xff, // crc_lut_mode1_yg
	0x00, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xfa, // crc_lut_mode1_wg
	0xf0, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_RGB_SENSOR_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_RGB_SENSOR_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xff, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x00, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x00, // crc_lut_mode1_br
	0x00, // crc_lut_mode1_bg
	0xff, // crc_lut_mode1_bb
	0x00, // crc_lut_mode1_cr
	0xf0, // crc_lut_mode1_cg
	0xff, // crc_lut_mode1_cb
	0xff, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xff, // crc_lut_mode1_mb
	0xff, // crc_lut_mode1_yr
	0xff, // crc_lut_mode1_yg
	0x00, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xff, // crc_lut_mode1_wg
	0xff, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_HDR_VIDEO_1_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_HDR_VIDEO_1_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xc6, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x10, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x06, // crc_lut_mode1_br
	0x06, // crc_lut_mode1_bg
	0xda, // crc_lut_mode1_bb
	0x19, // crc_lut_mode1_cr
	0xf6, // crc_lut_mode1_cg
	0xe8, // crc_lut_mode1_cb
	0xe2, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xdc, // crc_lut_mode1_mb
	0xea, // crc_lut_mode1_yr
	0xdf, // crc_lut_mode1_yg
	0x02, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xfa, // crc_lut_mode1_wg
	0xf0, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_HDR_VIDEO_2_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_HDR_VIDEO_2_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xc6, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x10, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x06, // crc_lut_mode1_br
	0x06, // crc_lut_mode1_bg
	0xda, // crc_lut_mode1_bb
	0x19, // crc_lut_mode1_cr
	0xf6, // crc_lut_mode1_cg
	0xe8, // crc_lut_mode1_cb
	0xe2, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xdc, // crc_lut_mode1_mb
	0xea, // crc_lut_mode1_yr
	0xdf, // crc_lut_mode1_yg
	0x02, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xfa, // crc_lut_mode1_wg
	0xf0, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static char DSI0_HDR_VIDEO_3_MDNIE_1[] = {
	0x80, // CRC, BLF
	0x90, // CRC on/off, BLF on/off, CRC on/off 0 0 00 00 00
};

static char DSI0_HDR_VIDEO_3_MDNIE_2[] = {
	0xB1, // CRC, BLF
	0x00, // crc_bypass
	0xc6, // crc_lut_mode1_rr
	0x00, // crc_lut_mode1_rg
	0x00, // crc_lut_mode1_rb
	0x10, // crc_lut_mode1_gr
	0xff, // crc_lut_mode1_gg
	0x00, // crc_lut_mode1_gb
	0x06, // crc_lut_mode1_br
	0x06, // crc_lut_mode1_bg
	0xda, // crc_lut_mode1_bb
	0x19, // crc_lut_mode1_cr
	0xf6, // crc_lut_mode1_cg
	0xe8, // crc_lut_mode1_cb
	0xe2, // crc_lut_mode1_mr
	0x00, // crc_lut_mode1_mg
	0xdc, // crc_lut_mode1_mb
	0xea, // crc_lut_mode1_yr
	0xdf, // crc_lut_mode1_yg
	0x02, // crc_lut_mode1_yb
	0xff, // crc_lut_mode1_wr
	0xfa, // crc_lut_mode1_wg
	0xf0, // crc_lut_mode1_wb
	0xff, // crc_lut_mode2_rr
	0x00, // crc_lut_mode2_rg
	0x00, // crc_lut_mode2_rb
	0x00, // crc_lut_mode2_gr
	0xff, // crc_lut_mode2_gg
	0x00, // crc_lut_mode2_gb
	0x00, // crc_lut_mode2_br
	0x00, // crc_lut_mode2_bg
	0xff, // crc_lut_mode2_bb
	0x00, // crc_lut_mode2_cr
	0xff, // crc_lut_mode2_cg
	0xff, // crc_lut_mode2_cb
	0xff, // crc_lut_mode2_mr
	0x00, // crc_lut_mode2_mg
	0xff, // crc_lut_mode2_mb
	0xff, // crc_lut_mode2_yr
	0xff, // crc_lut_mode2_yg
	0x00, // crc_lut_mode2_yb
	0xff, // crc_lut_mode2_wr
	0xff, // crc_lut_mode2_wg
	0xff, // crc_lut_mode2_wb
	0xff, // crc_lut_mode3_rr
	0x00, // crc_lut_mode3_rg
	0x00, // crc_lut_mode3_rb
	0x00, // crc_lut_mode3_gr
	0xff, // crc_lut_mode3_gg
	0x00, // crc_lut_mode3_gb
	0x00, // crc_lut_mode3_br
	0x00, // crc_lut_mode3_bg
	0xff, // crc_lut_mode3_bb
	0x00, // crc_lut_mode3_cr
	0xff, // crc_lut_mode3_cg
	0xff, // crc_lut_mode3_cb
	0xff, // crc_lut_mode3_mr
	0x00, // crc_lut_mode3_mg
	0xff, // crc_lut_mode3_mb
	0xff, // crc_lut_mode3_yr
	0xff, // crc_lut_mode3_yg
	0x00, // crc_lut_mode3_yb
	0xff, // crc_lut_mode3_wr
	0xff, // crc_lut_mode3_wg
	0xff, // crc_lut_mode3_wb
	0xff, // crc_lut_mode4_rr
	0x00, // crc_lut_mode4_rg
	0x00, // crc_lut_mode4_rb
	0x00, // crc_lut_mode4_gr
	0xff, // crc_lut_mode4_gg
	0x00, // crc_lut_mode4_gb
	0x00, // crc_lut_mode4_br
	0x00, // crc_lut_mode4_bg
	0xff, // crc_lut_mode4_bb
	0x00, // crc_lut_mode4_cr
	0xff, // crc_lut_mode4_cg
	0xff, // crc_lut_mode4_cb
	0xff, // crc_lut_mode4_mr
	0x00, // crc_lut_mode4_mg
	0xff, // crc_lut_mode4_mb
	0xff, // crc_lut_mode4_yr
	0xff, // crc_lut_mode4_yg
	0x00, // crc_lut_mode4_yb
	0xff, // crc_lut_mode4_wr
	0xff, // crc_lut_mode4_wg
	0xff, // crc_lut_mode4_wb
	0x80, // csc_user_mode_on
};

static struct dsi_cmd_desc DSI0_BYPASS_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_1), DSI0_BYPASS_MDNIE_1, 0, NULL}, false, 0, DSI0_BYPASS_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_2), DSI0_BYPASS_MDNIE_2, 0, NULL}, false, 0, DSI0_BYPASS_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_COLOR_BLIND_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_1), DSI0_COLOR_BLIND_MDNIE_1, 0, NULL}, false, 0, DSI0_COLOR_BLIND_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_2), DSI0_COLOR_BLIND_MDNIE_2, 0, NULL}, false, 0, DSI0_COLOR_BLIND_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_NIGHT_MODE_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_1), DSI0_NIGHT_MODE_MDNIE_1, 0, NULL}, false, 0, DSI0_NIGHT_MODE_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_2), DSI0_NIGHT_MODE_MDNIE_2, 0, NULL}, false, 0, DSI0_NIGHT_MODE_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HBM_CE_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_1), DSI0_HBM_CE_MDNIE_1, 0, NULL}, false, 0, DSI0_HBM_CE_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_2), DSI0_HBM_CE_MDNIE_2, 0, NULL}, false, 0, DSI0_HBM_CE_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HBM_CE_D65_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_1), DSI0_HBM_CE_D65_MDNIE_1, 0, NULL}, false, 0, DSI0_HBM_CE_D65_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_2), DSI0_HBM_CE_D65_MDNIE_2, 0, NULL}, false, 0, DSI0_HBM_CE_D65_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_RGB_SENSOR_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_RGB_SENSOR_MDNIE_1), DSI0_RGB_SENSOR_MDNIE_1, 0, NULL}, false, 0, DSI0_RGB_SENSOR_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_RGB_SENSOR_MDNIE_2), DSI0_RGB_SENSOR_MDNIE_2, 0, NULL}, false, 0, DSI0_RGB_SENSOR_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_SCREEN_CURTAIN_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_1), DSI0_SCREEN_CURTAIN_MDNIE_1, 0, NULL}, false, 0, DSI0_SCREEN_CURTAIN_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_2), DSI0_SCREEN_CURTAIN_MDNIE_2, 0, NULL}, false, 0, DSI0_SCREEN_CURTAIN_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_LIGHT_NOTIFICATION_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_1), DSI0_LIGHT_NOTIFICATION_MDNIE_1, 0, NULL}, false, 0, DSI0_LIGHT_NOTIFICATION_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_2), DSI0_LIGHT_NOTIFICATION_MDNIE_2, 0, NULL}, false, 0, DSI0_LIGHT_NOTIFICATION_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_1_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_1), DSI0_HDR_VIDEO_1_MDNIE_1, 0, NULL}, false, 0, DSI0_HDR_VIDEO_1_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_2), DSI0_HDR_VIDEO_1_MDNIE_2, 0, NULL}, false, 0, DSI0_HDR_VIDEO_1_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_2_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_1), DSI0_HDR_VIDEO_2_MDNIE_1, 0, NULL}, false, 0, DSI0_HDR_VIDEO_2_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_2), DSI0_HDR_VIDEO_2_MDNIE_2, 0, NULL}, false, 0, DSI0_HDR_VIDEO_2_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_3_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_1), DSI0_HDR_VIDEO_3_MDNIE_1, 0, NULL}, false, 0, DSI0_HDR_VIDEO_3_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_2), DSI0_HDR_VIDEO_3_MDNIE_2, 0, NULL}, false, 0, DSI0_HDR_VIDEO_3_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

///////////////////////////////////////////////////////////////////////////////////

static struct dsi_cmd_desc DSI0_UI_DYNAMIC_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_1), DSI0_UI_DYNAMIC_MDNIE_1, 0, NULL}, false, 0, DSI0_UI_DYNAMIC_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_2), DSI0_UI_DYNAMIC_MDNIE_2, 0, NULL}, false, 0, DSI0_UI_DYNAMIC_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_NATURAL_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_NATURAL_MDNIE_1), DSI0_UI_NATURAL_MDNIE_1, 0, NULL}, false, 0, DSI0_UI_NATURAL_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_NATURAL_MDNIE_2), DSI0_UI_NATURAL_MDNIE_2, 0, NULL}, false, 0, DSI0_UI_NATURAL_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_1), DSI0_UI_AUTO_MDNIE_1, 0, NULL}, false, 0, DSI0_UI_AUTO_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_2), DSI0_UI_AUTO_MDNIE_2, 0, NULL}, false, 0, DSI0_UI_AUTO_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_1), DSI0_GALLERY_AUTO_MDNIE_1, 0, NULL}, false, 0, DSI0_GALLERY_AUTO_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_2), DSI0_GALLERY_AUTO_MDNIE_2, 0, NULL}, false, 0, DSI0_GALLERY_AUTO_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_on), level_1_key_on, 0, NULL}, false, 0, level_1_key_on},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_1), DSI0_EBOOK_AUTO_MDNIE_1, 0, NULL}, false, 0, DSI0_EBOOK_AUTO_MDNIE_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_2), DSI0_EBOOK_AUTO_MDNIE_2, 0, NULL}, false, 0, DSI0_EBOOK_AUTO_MDNIE_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(level_1_key_off), level_1_key_off, 0, NULL}, true, 0, level_1_key_off},
};

static struct dsi_cmd_desc *mdnie_tune_value_dsi0[MAX_APP_MODE][MAX_MODE][MAX_OUTDOOR_MODE] = {
	/*
		DYNAMIC_MODE
		STANDARD_MODE
		NATURAL_MODE
		MOVIE_MODE
		AUTO_MODE
		READING_MODE
	*/
	// UI_APP
	{
		{DSI0_UI_DYNAMIC_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_UI_NATURAL_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_UI_AUTO_MDNIE,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// VIDEO_APP
	{
		{DSI0_UI_DYNAMIC_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_UI_NATURAL_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_GALLERY_AUTO_MDNIE,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// VIDEO_WARM_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
	},
	// VIDEO_COLD_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
	},
	// CAMERA_APP
	{
		{DSI0_UI_DYNAMIC_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_UI_NATURAL_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_GALLERY_AUTO_MDNIE,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// NAVI_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
	},
	// GALLERY_APP
	{
		{DSI0_UI_DYNAMIC_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_UI_NATURAL_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_GALLERY_AUTO_MDNIE,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// VT_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
	},
	// BROWSER_APP
	{
		{DSI0_UI_DYNAMIC_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_UI_NATURAL_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_GALLERY_AUTO_MDNIE,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// eBOOK_APP
	{
		{DSI0_UI_DYNAMIC_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_UI_NATURAL_MDNIE,	NULL},
		{NULL,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// EMAIL_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
	},
	// GAME_LOW_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
	},
	// GAME_MID_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
	},
	// GAME_HIGH_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
	},
	// VIDEO_ENHANCER_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
	},
	// VIDEO_ENHANCER_THIRD_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
	},
	// TDMB_APP
	{
		{DSI0_UI_DYNAMIC_MDNIE,       NULL},
		{NULL,      NULL},
		{DSI0_UI_NATURAL_MDNIE,       NULL},
		{NULL,         NULL},
		{DSI0_GALLERY_AUTO_MDNIE,          NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
};

static struct dsi_cmd_desc *light_notification_tune_value_dsi0[LIGHT_NOTIFICATION_MAX] = {
	NULL,
	DSI0_LIGHT_NOTIFICATION_MDNIE,
};

static struct dsi_cmd_desc *hdr_tune_value_dsi0[HDR_MAX] = {
	NULL,
	DSI0_HDR_VIDEO_1_MDNIE,
	DSI0_HDR_VIDEO_2_MDNIE,
	DSI0_HDR_VIDEO_3_MDNIE,
	NULL,
	NULL,
};

#define DSI0_RGB_SENSOR_MDNIE_1_SIZE ARRAY_SIZE(DSI0_RGB_SENSOR_MDNIE_1)
#define DSI0_RGB_SENSOR_MDNIE_2_SIZE ARRAY_SIZE(DSI0_RGB_SENSOR_MDNIE_2)

#endif
