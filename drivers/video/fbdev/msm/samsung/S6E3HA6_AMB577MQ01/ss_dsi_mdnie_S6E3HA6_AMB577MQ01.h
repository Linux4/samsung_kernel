/*
 * =================================================================
 *
 *
 *	Description:  samsung display common file
 *
 *	Author: jb09.kim
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2012, Samsung Electronics. All rights reserved.

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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/
#ifndef _SS_DSI_MDNIE_S6E3HA6_AMB577MQ01_H_
#define _SS_DSI_MDNIE_S6E3HA6_AMB577MQ01_H_

#include "../ss_dsi_mdnie_lite_common.h"

#define MDNIE_COLOR_BLINDE_CMD_OFFSET 32

#define ADDRESS_SCR_WHITE_RED   0x32
#define ADDRESS_SCR_WHITE_GREEN 0x34
#define ADDRESS_SCR_WHITE_BLUE  0x36

#define MDNIE_STEP1_INDEX 1
#define MDNIE_STEP2_INDEX 2
#define MDNIE_STEP3_INDEX 3

#define MDNIE_TRANS_DIMMING_DATA_INDEX	17

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
	0xfb, 0xfe, 0xff,
	0xf8, 0xfd, 0xff,
	0xf6, 0xfd, 0xff,
	0xf3, 0xfc, 0xff,
	0xf1, 0xfc, 0xff,
};

static char adjust_ldu_data_2[] = {
	0xff, 0xfa, 0xf1,
	0xfb, 0xf8, 0xf0,
	0xf9, 0xf8, 0xf0,
	0xf7, 0xf8, 0xf0,
	0xf4, 0xf7, 0xf0,
	0xf1, 0xf7, 0xf0,
};

static char *adjust_ldu_data[MAX_MODE] = {
	adjust_ldu_data_2,
	adjust_ldu_data_2,
	adjust_ldu_data_2,
	adjust_ldu_data_1,
	adjust_ldu_data_1,
};

static char night_mode_data[] = {
	0x00, 0xff, 0xfa, 0x00, 0xf1, 0x00, 0xff, 0x00, 0x00, 0xfa, 0xf1, 0x00, 0xff, 0x00, 0xfa, 0x00, 0x00, 0xf1, 0xff, 0x00, 0xfa, 0x00, 0xf1, 0x00, /* 6500K */
	0x00, 0xff, 0xf8, 0x00, 0xea, 0x00, 0xff, 0x00, 0x00, 0xf8, 0xea, 0x00, 0xff, 0x00, 0xf8, 0x00, 0x00, 0xea, 0xff, 0x00, 0xf8, 0x00, 0xea, 0x00, /* 6100K */
	0x00, 0xff, 0xf5, 0x00, 0xe2, 0x00, 0xff, 0x00, 0x00, 0xf5, 0xe2, 0x00, 0xff, 0x00, 0xf5, 0x00, 0x00, 0xe2, 0xff, 0x00, 0xf5, 0x00, 0xe2, 0x00, /* 5700K */
	0x00, 0xff, 0xf2, 0x00, 0xda, 0x00, 0xff, 0x00, 0x00, 0xf2, 0xda, 0x00, 0xff, 0x00, 0xf2, 0x00, 0x00, 0xda, 0xff, 0x00, 0xf2, 0x00, 0xda, 0x00, /* 5300K */
	0x00, 0xff, 0xee, 0x00, 0xd0, 0x00, 0xff, 0x00, 0x00, 0xee, 0xd0, 0x00, 0xff, 0x00, 0xee, 0x00, 0x00, 0xd0, 0xff, 0x00, 0xee, 0x00, 0xd0, 0x00, /* 4900K */
	0x00, 0xff, 0xea, 0x00, 0xc5, 0x00, 0xff, 0x00, 0x00, 0xea, 0xc5, 0x00, 0xff, 0x00, 0xea, 0x00, 0x00, 0xc5, 0xff, 0x00, 0xea, 0x00, 0xc5, 0x00, /* 4500K */
	0x00, 0xff, 0xe5, 0x00, 0xb7, 0x00, 0xff, 0x00, 0x00, 0xe5, 0xb7, 0x00, 0xff, 0x00, 0xe5, 0x00, 0x00, 0xb7, 0xff, 0x00, 0xe5, 0x00, 0xb7, 0x00, /* 4100K */
	0x00, 0xff, 0xde, 0x00, 0xa5, 0x00, 0xff, 0x00, 0x00, 0xde, 0xa5, 0x00, 0xff, 0x00, 0xde, 0x00, 0x00, 0xa5, 0xff, 0x00, 0xde, 0x00, 0xa5, 0x00, /* 3700K */
	0x00, 0xff, 0xd8, 0x00, 0x93, 0x00, 0xff, 0x00, 0x00, 0xd8, 0x93, 0x00, 0xff, 0x00, 0xd8, 0x00, 0x00, 0x93, 0xff, 0x00, 0xd8, 0x00, 0x93, 0x00, /* 3300K */
	0x00, 0xff, 0xcf, 0x00, 0x7f, 0x00, 0xff, 0x00, 0x00, 0xcf, 0x7f, 0x00, 0xff, 0x00, 0xcf, 0x00, 0x00, 0x7f, 0xff, 0x00, 0xcf, 0x00, 0x7f, 0x00, /* 2900K */
	0x00, 0xff, 0xc3, 0x00, 0x6a, 0x00, 0xff, 0x00, 0x00, 0xc3, 0x6a, 0x00, 0xff, 0x00, 0xc3, 0x00, 0x00, 0x6a, 0xff, 0x00, 0xc3, 0x00, 0x6a, 0x00, /* 2500K */
};

static char color_lens_data[] = {
	//Blue
	0x00, 0xcc, 0xcc, 0x00, 0xff, 0x33, 0xcc, 0x00, 0x00, 0xcc, 0xff, 0x33, 0xcc, 0x00, 0xcc, 0x00, 0x33, 0xff, 0xcc, 0x00, 0xcc, 0x00, 0xff, 0x33, /* 20% */
	0x00, 0xbf, 0xbf, 0x00, 0xff, 0x40, 0xbf, 0x00, 0x00, 0xbf, 0xff, 0x40, 0xbf, 0x00, 0xbf, 0x00, 0x40, 0xff, 0xbf, 0x00, 0xbf, 0x00, 0xff, 0x40, /* 25% */
	0x00, 0xb2, 0xb2, 0x00, 0xff, 0x4d, 0xb2, 0x00, 0x00, 0xb2, 0xff, 0x4d, 0xb2, 0x00, 0xb2, 0x00, 0x4d, 0xff, 0xb2, 0x00, 0xb2, 0x00, 0xff, 0x4d, /* 30% */
	0x00, 0xa6, 0xa6, 0x00, 0xff, 0x59, 0xa6, 0x00, 0x00, 0xa6, 0xff, 0x59, 0xa6, 0x00, 0xa6, 0x00, 0x59, 0xff, 0xa6, 0x00, 0xa6, 0x00, 0xff, 0x59, /* 35% */
	0x00, 0x99, 0x99, 0x00, 0xff, 0x66, 0x99, 0x00, 0x00, 0x99, 0xff, 0x66, 0x99, 0x00, 0x99, 0x00, 0x66, 0xff, 0x99, 0x00, 0x99, 0x00, 0xff, 0x66, /* 40% */
	0x00, 0x8c, 0x8c, 0x00, 0xff, 0x73, 0x8c, 0x00, 0x00, 0x8c, 0xff, 0x73, 0x8c, 0x00, 0x8c, 0x00, 0x73, 0xff, 0x8c, 0x00, 0x8c, 0x00, 0xff, 0x73, /* 45% */
	0x00, 0x7f, 0x7f, 0x00, 0xff, 0x80, 0x7f, 0x00, 0x00, 0x7f, 0xff, 0x80, 0x7f, 0x00, 0x7f, 0x00, 0x80, 0xff, 0x7f, 0x00, 0x7f, 0x00, 0xff, 0x80, /* 50% */
	0x00, 0x73, 0x73, 0x00, 0xff, 0x8c, 0x73, 0x00, 0x00, 0x73, 0xff, 0x8c, 0x73, 0x00, 0x73, 0x00, 0x8c, 0xff, 0x73, 0x00, 0x73, 0x00, 0xff, 0x8c, /* 55% */
	0x00, 0x66, 0x66, 0x00, 0xff, 0x99, 0x66, 0x00, 0x00, 0x66, 0xff, 0x99, 0x66, 0x00, 0x66, 0x00, 0x99, 0xff, 0x66, 0x00, 0x66, 0x00, 0xff, 0x99, /* 60% */

	//Azure
	0x00, 0xcc, 0xe5, 0x19, 0xff, 0x33, 0xcc, 0x00, 0x19, 0xe5, 0xff, 0x33, 0xcc, 0x00, 0xe5, 0x19, 0x33, 0xff, 0xcc, 0x00, 0xe5, 0x19, 0xff, 0x33, /* 20% */
	0x00, 0xbf, 0xdf, 0x20, 0xff, 0x40, 0xbf, 0x00, 0x20, 0xdf, 0xff, 0x40, 0xbf, 0x00, 0xdf, 0x20, 0x40, 0xff, 0xbf, 0x00, 0xdf, 0x20, 0xff, 0x40, /* 25% */
	0x00, 0xb2, 0xd8, 0x26, 0xff, 0x4d, 0xb2, 0x00, 0x26, 0xd8, 0xff, 0x4d, 0xb2, 0x00, 0xd8, 0x26, 0x4d, 0xff, 0xb2, 0x00, 0xd8, 0x26, 0xff, 0x4d, /* 30% */
	0x00, 0xa6, 0xd2, 0x2c, 0xff, 0x59, 0xa6, 0x00, 0x2c, 0xd2, 0xff, 0x59, 0xa6, 0x00, 0xd2, 0x2c, 0x59, 0xff, 0xa6, 0x00, 0xd2, 0x2c, 0xff, 0x59, /* 35% */
	0x00, 0x99, 0xcc, 0x33, 0xff, 0x66, 0x99, 0x00, 0x33, 0xcc, 0xff, 0x66, 0x99, 0x00, 0xcc, 0x33, 0x66, 0xff, 0x99, 0x00, 0xcc, 0x33, 0xff, 0x66, /* 40% */
	0x00, 0x8c, 0xc5, 0x39, 0xff, 0x73, 0x8c, 0x00, 0x39, 0xc5, 0xff, 0x73, 0x8c, 0x00, 0xc5, 0x39, 0x73, 0xff, 0x8c, 0x00, 0xc5, 0x39, 0xff, 0x73, /* 45% */
	0x00, 0x7f, 0xbf, 0x40, 0xff, 0x80, 0x7f, 0x00, 0x40, 0xbf, 0xff, 0x80, 0x7f, 0x00, 0xbf, 0x40, 0x80, 0xff, 0x7f, 0x00, 0xbf, 0x40, 0xff, 0x80, /* 50% */
	0x00, 0x73, 0xb9, 0x46, 0xff, 0x8c, 0x73, 0x00, 0x46, 0xb9, 0xff, 0x8c, 0x73, 0x00, 0xb9, 0x46, 0x8c, 0xff, 0x73, 0x00, 0xb9, 0x46, 0xff, 0x8c, /* 55% */
	0x00, 0x66, 0xb2, 0x4c, 0xff, 0x99, 0x66, 0x00, 0x4c, 0xb2, 0xff, 0x99, 0x66, 0x00, 0xb2, 0x4c, 0x99, 0xff, 0x66, 0x00, 0xb2, 0x4c, 0xff, 0x99, /* 60% */

	//Cyan
	0x00, 0xcc, 0xff, 0x33, 0xff, 0x33, 0xcc, 0x00, 0x33, 0xff, 0xff, 0x33, 0xcc, 0x00, 0xff, 0x33, 0x33, 0xff, 0xcc, 0x00, 0xff, 0x33, 0xff, 0x33, /* 20% */
	0x00, 0xbf, 0xff, 0x40, 0xff, 0x40, 0xbf, 0x00, 0x40, 0xff, 0xff, 0x40, 0xbf, 0x00, 0xff, 0x40, 0x40, 0xff, 0xbf, 0x00, 0xff, 0x40, 0xff, 0x40, /* 25% */
	0x00, 0xb2, 0xff, 0x4d, 0xff, 0x4d, 0xb2, 0x00, 0x4d, 0xff, 0xff, 0x4d, 0xb2, 0x00, 0xff, 0x4d, 0x4d, 0xff, 0xb2, 0x00, 0xff, 0x4d, 0xff, 0x4d, /* 30% */
	0x00, 0xa6, 0xff, 0x59, 0xff, 0x59, 0xa6, 0x00, 0x59, 0xff, 0xff, 0x59, 0xa6, 0x00, 0xff, 0x59, 0x59, 0xff, 0xa6, 0x00, 0xff, 0x59, 0xff, 0x59, /* 35% */
	0x00, 0x99, 0xff, 0x66, 0xff, 0x66, 0x99, 0x00, 0x66, 0xff, 0xff, 0x66, 0x99, 0x00, 0xff, 0x66, 0x66, 0xff, 0x99, 0x00, 0xff, 0x66, 0xff, 0x66, /* 40% */
	0x00, 0x8c, 0xff, 0x73, 0xff, 0x73, 0x8c, 0x00, 0x73, 0xff, 0xff, 0x73, 0x8c, 0x00, 0xff, 0x73, 0x73, 0xff, 0x8c, 0x00, 0xff, 0x73, 0xff, 0x73, /* 45% */
	0x00, 0x7f, 0xff, 0x80, 0xff, 0x80, 0x7f, 0x00, 0x80, 0xff, 0xff, 0x80, 0x7f, 0x00, 0xff, 0x80, 0x80, 0xff, 0x7f, 0x00, 0xff, 0x80, 0xff, 0x80, /* 50% */
	0x00, 0x73, 0xff, 0x8c, 0xff, 0x8c, 0x73, 0x00, 0x8c, 0xff, 0xff, 0x8c, 0x73, 0x00, 0xff, 0x8c, 0x8c, 0xff, 0x73, 0x00, 0xff, 0x8c, 0xff, 0x8c, /* 55% */
	0x00, 0x66, 0xff, 0x99, 0xff, 0x99, 0x66, 0x00, 0x99, 0xff, 0xff, 0x99, 0x66, 0x00, 0xff, 0x99, 0x99, 0xff, 0x66, 0x00, 0xff, 0x99, 0xff, 0x99, /* 60% */

	//Spring green
	0x00, 0xcc, 0xff, 0x33, 0xe5, 0x19, 0xcc, 0x00, 0x33, 0xff, 0xe5, 0x19, 0xcc, 0x00, 0xff, 0x33, 0x19, 0xe5, 0xcc, 0x00, 0xff, 0x33, 0xe5, 0x19, /* 20% */
	0x00, 0xbf, 0xff, 0x40, 0xdf, 0x20, 0xbf, 0x00, 0x40, 0xff, 0xdf, 0x20, 0xbf, 0x00, 0xff, 0x40, 0x20, 0xdf, 0xbf, 0x00, 0xff, 0x40, 0xdf, 0x20, /* 25% */
	0x00, 0xb2, 0xff, 0x4d, 0xd8, 0x26, 0xb2, 0x00, 0x4d, 0xff, 0xd8, 0x26, 0xb2, 0x00, 0xff, 0x4d, 0x26, 0xd8, 0xb2, 0x00, 0xff, 0x4d, 0xd8, 0x26, /* 30% */
	0x00, 0xa6, 0xff, 0x59, 0xd2, 0x2c, 0xa6, 0x00, 0x59, 0xff, 0xd2, 0x2c, 0xa6, 0x00, 0xff, 0x59, 0x2c, 0xd2, 0xa6, 0x00, 0xff, 0x59, 0xd2, 0x2c, /* 35% */
	0x00, 0x99, 0xff, 0x66, 0xcc, 0x33, 0x99, 0x00, 0x66, 0xff, 0xcc, 0x33, 0x99, 0x00, 0xff, 0x66, 0x33, 0xcc, 0x99, 0x00, 0xff, 0x66, 0xcc, 0x33, /* 40% */
	0x00, 0x8c, 0xff, 0x73, 0xc5, 0x39, 0x8c, 0x00, 0x73, 0xff, 0xc5, 0x39, 0x8c, 0x00, 0xff, 0x73, 0x39, 0xc5, 0x8c, 0x00, 0xff, 0x73, 0xc5, 0x39, /* 45% */
	0x00, 0x7f, 0xff, 0x80, 0xbf, 0x40, 0x7f, 0x00, 0x80, 0xff, 0xbf, 0x40, 0x7f, 0x00, 0xff, 0x80, 0x40, 0xbf, 0x7f, 0x00, 0xff, 0x80, 0xbf, 0x40, /* 50% */
	0x00, 0x73, 0xff, 0x8c, 0xb9, 0x46, 0x73, 0x00, 0x8c, 0xff, 0xb9, 0x46, 0x73, 0x00, 0xff, 0x8c, 0x46, 0xb9, 0x73, 0x00, 0xff, 0x8c, 0xb9, 0x46, /* 55% */
	0x00, 0x66, 0xff, 0x99, 0xb2, 0x4c, 0x66, 0x00, 0x99, 0xff, 0xb2, 0x4c, 0x66, 0x00, 0xff, 0x99, 0x4c, 0xb2, 0x66, 0x00, 0xff, 0x99, 0xb2, 0x4c, /* 60% */

	//Green
	0x00, 0xcc, 0xff, 0x33, 0xcc, 0x00, 0xcc, 0x00, 0x33, 0xff, 0xcc, 0x00, 0xcc, 0x00, 0xff, 0x33, 0x00, 0xcc, 0xcc, 0x00, 0xff, 0x33, 0xcc, 0x00, /* 20% */
	0x00, 0xbf, 0xff, 0x40, 0xbf, 0x00, 0xbf, 0x00, 0x40, 0xff, 0xbf, 0x00, 0xbf, 0x00, 0xff, 0x40, 0x00, 0xbf, 0xbf, 0x00, 0xff, 0x40, 0xbf, 0x00, /* 25% */
	0x00, 0xb2, 0xff, 0x4d, 0xb2, 0x00, 0xb2, 0x00, 0x4d, 0xff, 0xb2, 0x00, 0xb2, 0x00, 0xff, 0x4d, 0x00, 0xb2, 0xb2, 0x00, 0xff, 0x4d, 0xb2, 0x00, /* 30% */
	0x00, 0xa6, 0xff, 0x59, 0xa6, 0x00, 0xa6, 0x00, 0x59, 0xff, 0xa6, 0x00, 0xa6, 0x00, 0xff, 0x59, 0x00, 0xa6, 0xa6, 0x00, 0xff, 0x59, 0xa6, 0x00, /* 35% */
	0x00, 0x99, 0xff, 0x66, 0x99, 0x00, 0x99, 0x00, 0x66, 0xff, 0x99, 0x00, 0x99, 0x00, 0xff, 0x66, 0x00, 0x99, 0x99, 0x00, 0xff, 0x66, 0x99, 0x00, /* 40% */
	0x00, 0x8c, 0xff, 0x73, 0x8c, 0x00, 0x8c, 0x00, 0x73, 0xff, 0x8c, 0x00, 0x8c, 0x00, 0xff, 0x73, 0x00, 0x8c, 0x8c, 0x00, 0xff, 0x73, 0x8c, 0x00, /* 45% */
	0x00, 0x7f, 0xff, 0x80, 0x7f, 0x00, 0x7f, 0x00, 0x80, 0xff, 0x7f, 0x00, 0x7f, 0x00, 0xff, 0x80, 0x00, 0x7f, 0x7f, 0x00, 0xff, 0x80, 0x7f, 0x00, /* 50% */
	0x00, 0x73, 0xff, 0x8c, 0x73, 0x00, 0x73, 0x00, 0x8c, 0xff, 0x73, 0x00, 0x73, 0x00, 0xff, 0x8c, 0x00, 0x73, 0x73, 0x00, 0xff, 0x8c, 0x73, 0x00, /* 55% */
	0x00, 0x66, 0xff, 0x99, 0x66, 0x00, 0x66, 0x00, 0x99, 0xff, 0x66, 0x00, 0x66, 0x00, 0xff, 0x99, 0x00, 0x66, 0x66, 0x00, 0xff, 0x99, 0x66, 0x00, /* 60% */

	//Chartreuse Green
	0x19, 0xe5, 0xff, 0x33, 0xcc, 0x00, 0xe5, 0x19, 0x33, 0xff, 0xcc, 0x00, 0xe5, 0x19, 0xff, 0x33, 0x00, 0xcc, 0xe5, 0x19, 0xff, 0x33, 0xcc, 0x00, /* 20% */
	0x20, 0xdf, 0xff, 0x40, 0xbf, 0x00, 0xdf, 0x20, 0x40, 0xff, 0xbf, 0x00, 0xdf, 0x20, 0xff, 0x40, 0x00, 0xbf, 0xdf, 0x20, 0xff, 0x40, 0xbf, 0x00, /* 25% */
	0x26, 0xd8, 0xff, 0x4d, 0xb2, 0x00, 0xd8, 0x26, 0x4d, 0xff, 0xb2, 0x00, 0xd8, 0x26, 0xff, 0x4d, 0x00, 0xb2, 0xd8, 0x26, 0xff, 0x4d, 0xb2, 0x00, /* 30% */
	0x2c, 0xd2, 0xff, 0x59, 0xa6, 0x00, 0xd2, 0x2c, 0x59, 0xff, 0xa6, 0x00, 0xd2, 0x2c, 0xff, 0x59, 0x00, 0xa6, 0xd2, 0x2c, 0xff, 0x59, 0xa6, 0x00, /* 35% */
	0x33, 0xcc, 0xff, 0x66, 0x99, 0x00, 0xcc, 0x33, 0x66, 0xff, 0x99, 0x00, 0xcc, 0x33, 0xff, 0x66, 0x00, 0x99, 0xcc, 0x33, 0xff, 0x66, 0x99, 0x00, /* 40% */
	0x39, 0xc5, 0xff, 0x73, 0x8c, 0x00, 0xc5, 0x39, 0x73, 0xff, 0x8c, 0x00, 0xc5, 0x39, 0xff, 0x73, 0x00, 0x8c, 0xc5, 0x39, 0xff, 0x73, 0x8c, 0x00, /* 45% */
	0x40, 0xbf, 0xff, 0x80, 0x7f, 0x00, 0xbf, 0x40, 0x80, 0xff, 0x7f, 0x00, 0xbf, 0x40, 0xff, 0x80, 0x00, 0x7f, 0xbf, 0x40, 0xff, 0x80, 0x7f, 0x00, /* 50% */
	0x46, 0xb9, 0xff, 0x8c, 0x73, 0x00, 0xb9, 0x46, 0x8c, 0xff, 0x73, 0x00, 0xb9, 0x46, 0xff, 0x8c, 0x00, 0x73, 0xb9, 0x46, 0xff, 0x8c, 0x73, 0x00, /* 55% */
	0x4c, 0xb2, 0xff, 0x99, 0x66, 0x00, 0xb2, 0x4c, 0x99, 0xff, 0x66, 0x00, 0xb2, 0x4c, 0xff, 0x99, 0x00, 0x66, 0xb2, 0x4c, 0xff, 0x99, 0x66, 0x00, /* 60% */

	//Yellow
	0x33, 0xff, 0xff, 0x33, 0xcc, 0x00, 0xff, 0x33, 0x33, 0xff, 0xcc, 0x00, 0xff, 0x33, 0xff, 0x33, 0x00, 0xcc, 0xff, 0x33, 0xff, 0x33, 0xcc, 0x00, /* 20% */
	0x40, 0xff, 0xff, 0x40, 0xbf, 0x00, 0xff, 0x40, 0x40, 0xff, 0xbf, 0x00, 0xff, 0x40, 0xff, 0x40, 0x00, 0xbf, 0xff, 0x40, 0xff, 0x40, 0xbf, 0x00, /* 25% */
	0x4d, 0xff, 0xff, 0x4d, 0xb2, 0x00, 0xff, 0x4d, 0x4d, 0xff, 0xb2, 0x00, 0xff, 0x4d, 0xff, 0x4d, 0x00, 0xb2, 0xff, 0x4d, 0xff, 0x4d, 0xb2, 0x00, /* 30% */
	0x59, 0xff, 0xff, 0x59, 0xa6, 0x00, 0xff, 0x59, 0x59, 0xff, 0xa6, 0x00, 0xff, 0x59, 0xff, 0x59, 0x00, 0xa6, 0xff, 0x59, 0xff, 0x59, 0xa6, 0x00, /* 35% */
	0x66, 0xff, 0xff, 0x66, 0x99, 0x00, 0xff, 0x66, 0x66, 0xff, 0x99, 0x00, 0xff, 0x66, 0xff, 0x66, 0x00, 0x99, 0xff, 0x66, 0xff, 0x66, 0x99, 0x00, /* 40% */
	0x73, 0xff, 0xff, 0x73, 0x8c, 0x00, 0xff, 0x73, 0x73, 0xff, 0x8c, 0x00, 0xff, 0x73, 0xff, 0x73, 0x00, 0x8c, 0xff, 0x73, 0xff, 0x73, 0x8c, 0x00, /* 45% */
	0x80, 0xff, 0xff, 0x80, 0x7f, 0x00, 0xff, 0x80, 0x80, 0xff, 0x7f, 0x00, 0xff, 0x80, 0xff, 0x80, 0x00, 0x7f, 0xff, 0x80, 0xff, 0x80, 0x7f, 0x00, /* 50% */
	0x8c, 0xff, 0xff, 0x8c, 0x73, 0x00, 0xff, 0x8c, 0x8c, 0xff, 0x73, 0x00, 0xff, 0x8c, 0xff, 0x8c, 0x00, 0x73, 0xff, 0x8c, 0xff, 0x8c, 0x73, 0x00, /* 55% */
	0x99, 0xff, 0xff, 0x99, 0x66, 0x00, 0xff, 0x99, 0x99, 0xff, 0x66, 0x00, 0xff, 0x99, 0xff, 0x99, 0x00, 0x66, 0xff, 0x99, 0xff, 0x99, 0x66, 0x00, /* 60% */

	//Orange
	0x33, 0xff, 0xe5, 0x19, 0xcc, 0x00, 0xff, 0x33, 0x19, 0xe5, 0xcc, 0x00, 0xff, 0x33, 0xe5, 0x19, 0x00, 0xcc, 0xff, 0x33, 0xe5, 0x19, 0xcc, 0x00, /* 20% */
	0x40, 0xff, 0xdf, 0x20, 0xbf, 0x00, 0xff, 0x40, 0x20, 0xdf, 0xbf, 0x00, 0xff, 0x40, 0xdf, 0x20, 0x00, 0xbf, 0xff, 0x40, 0xdf, 0x20, 0xbf, 0x00, /* 25% */
	0x4d, 0xff, 0xd8, 0x26, 0xb2, 0x00, 0xff, 0x4d, 0x26, 0xd8, 0xb2, 0x00, 0xff, 0x4d, 0xd8, 0x26, 0x00, 0xb2, 0xff, 0x4d, 0xd8, 0x26, 0xb2, 0x00, /* 30% */
	0x59, 0xff, 0xd2, 0x2c, 0xa6, 0x00, 0xff, 0x59, 0x2c, 0xd2, 0xa6, 0x00, 0xff, 0x59, 0xd2, 0x2c, 0x00, 0xa6, 0xff, 0x59, 0xd2, 0x2c, 0xa6, 0x00, /* 35% */
	0x66, 0xff, 0xcc, 0x33, 0x99, 0x00, 0xff, 0x66, 0x33, 0xcc, 0x99, 0x00, 0xff, 0x66, 0xcc, 0x33, 0x00, 0x99, 0xff, 0x66, 0xcc, 0x33, 0x99, 0x00, /* 40% */
	0x73, 0xff, 0xc5, 0x39, 0x8c, 0x00, 0xff, 0x73, 0x39, 0xc5, 0x8c, 0x00, 0xff, 0x73, 0xc5, 0x39, 0x00, 0x8c, 0xff, 0x73, 0xc5, 0x39, 0x8c, 0x00, /* 45% */
	0x80, 0xff, 0xbf, 0x40, 0x7f, 0x00, 0xff, 0x80, 0x40, 0xbf, 0x7f, 0x00, 0xff, 0x80, 0xbf, 0x40, 0x00, 0x7f, 0xff, 0x80, 0xbf, 0x40, 0x7f, 0x00, /* 50% */
	0x8c, 0xff, 0xb9, 0x46, 0x73, 0x00, 0xff, 0x8c, 0x46, 0xb9, 0x73, 0x00, 0xff, 0x8c, 0xb9, 0x46, 0x00, 0x73, 0xff, 0x8c, 0xb9, 0x46, 0x73, 0x00, /* 55% */
	0x99, 0xff, 0xb2, 0x4c, 0x66, 0x00, 0xff, 0x99, 0x4c, 0xb2, 0x66, 0x00, 0xff, 0x99, 0xb2, 0x4c, 0x00, 0x66, 0xff, 0x99, 0xb2, 0x4c, 0x66, 0x00, /* 60% */

	//Red
	0x33, 0xff, 0xcc, 0x00, 0xcc, 0x00, 0xff, 0x33, 0x00, 0xcc, 0xcc, 0x00, 0xff, 0x33, 0xcc, 0x00, 0x00, 0xcc, 0xff, 0x33, 0xcc, 0x00, 0xcc, 0x00, /* 20% */
	0x40, 0xff, 0xbf, 0x00, 0xbf, 0x00, 0xff, 0x40, 0x00, 0xbf, 0xbf, 0x00, 0xff, 0x40, 0xbf, 0x00, 0x00, 0xbf, 0xff, 0x40, 0xbf, 0x00, 0xbf, 0x00, /* 25% */
	0x4d, 0xff, 0xb2, 0x00, 0xb2, 0x00, 0xff, 0x4d, 0x00, 0xb2, 0xb2, 0x00, 0xff, 0x4d, 0xb2, 0x00, 0x00, 0xb2, 0xff, 0x4d, 0xb2, 0x00, 0xb2, 0x00, /* 30% */
	0x59, 0xff, 0xa6, 0x00, 0xa6, 0x00, 0xff, 0x59, 0x00, 0xa6, 0xa6, 0x00, 0xff, 0x59, 0xa6, 0x00, 0x00, 0xa6, 0xff, 0x59, 0xa6, 0x00, 0xa6, 0x00, /* 35% */
	0x66, 0xff, 0x99, 0x00, 0x99, 0x00, 0xff, 0x66, 0x00, 0x99, 0x99, 0x00, 0xff, 0x66, 0x99, 0x00, 0x00, 0x99, 0xff, 0x66, 0x99, 0x00, 0x99, 0x00, /* 40% */
	0x73, 0xff, 0x8c, 0x00, 0x8c, 0x00, 0xff, 0x73, 0x00, 0x8c, 0x8c, 0x00, 0xff, 0x73, 0x8c, 0x00, 0x00, 0x8c, 0xff, 0x73, 0x8c, 0x00, 0x8c, 0x00, /* 45% */
	0x80, 0xff, 0x7f, 0x00, 0x7f, 0x00, 0xff, 0x80, 0x00, 0x7f, 0x7f, 0x00, 0xff, 0x80, 0x7f, 0x00, 0x00, 0x7f, 0xff, 0x80, 0x7f, 0x00, 0x7f, 0x00, /* 50% */
	0x8c, 0xff, 0x73, 0x00, 0x73, 0x00, 0xff, 0x8c, 0x00, 0x73, 0x73, 0x00, 0xff, 0x8c, 0x73, 0x00, 0x00, 0x73, 0xff, 0x8c, 0x73, 0x00, 0x73, 0x00, /* 55% */
	0x99, 0xff, 0x66, 0x00, 0x66, 0x00, 0xff, 0x99, 0x00, 0x66, 0x66, 0x00, 0xff, 0x99, 0x66, 0x00, 0x00, 0x66, 0xff, 0x99, 0x66, 0x00, 0x66, 0x00, /* 60% */

	//Rose
	0x33, 0xff, 0xcc, 0x00, 0xe5, 0x19, 0xff, 0x33, 0x00, 0xcc, 0xe5, 0x19, 0xff, 0x33, 0xcc, 0x00, 0x19, 0xe5, 0xff, 0x33, 0xcc, 0x00, 0xe5, 0x19, /* 20% */
	0x40, 0xff, 0xbf, 0x00, 0xdf, 0x20, 0xff, 0x40, 0x00, 0xbf, 0xdf, 0x20, 0xff, 0x40, 0xbf, 0x00, 0x20, 0xdf, 0xff, 0x40, 0xbf, 0x00, 0xdf, 0x20, /* 25% */
	0x4d, 0xff, 0xb2, 0x00, 0xd8, 0x26, 0xff, 0x4d, 0x00, 0xb2, 0xd8, 0x26, 0xff, 0x4d, 0xb2, 0x00, 0x26, 0xd8, 0xff, 0x4d, 0xb2, 0x00, 0xd8, 0x26, /* 30% */
	0x59, 0xff, 0xa6, 0x00, 0xd2, 0x2c, 0xff, 0x59, 0x00, 0xa6, 0xd2, 0x2c, 0xff, 0x59, 0xa6, 0x00, 0x2c, 0xd2, 0xff, 0x59, 0xa6, 0x00, 0xd2, 0x2c, /* 35% */
	0x66, 0xff, 0x99, 0x00, 0xcc, 0x33, 0xff, 0x66, 0x00, 0x99, 0xcc, 0x33, 0xff, 0x66, 0x99, 0x00, 0x33, 0xcc, 0xff, 0x66, 0x99, 0x00, 0xcc, 0x33, /* 40% */
	0x73, 0xff, 0x8c, 0x00, 0xc5, 0x39, 0xff, 0x73, 0x00, 0x8c, 0xc5, 0x39, 0xff, 0x73, 0x8c, 0x00, 0x39, 0xc5, 0xff, 0x73, 0x8c, 0x00, 0xc5, 0x39, /* 45% */
	0x80, 0xff, 0x7f, 0x00, 0xbf, 0x40, 0xff, 0x80, 0x00, 0x7f, 0xbf, 0x40, 0xff, 0x80, 0x7f, 0x00, 0x40, 0xbf, 0xff, 0x80, 0x7f, 0x00, 0xbf, 0x40, /* 50% */
	0x8c, 0xff, 0x73, 0x00, 0xb9, 0x46, 0xff, 0x8c, 0x00, 0x73, 0xb9, 0x46, 0xff, 0x8c, 0x73, 0x00, 0x46, 0xb9, 0xff, 0x8c, 0x73, 0x00, 0xb9, 0x46, /* 55% */
	0x99, 0xff, 0x66, 0x00, 0xb2, 0x4c, 0xff, 0x99, 0x00, 0x66, 0xb2, 0x4c, 0xff, 0x99, 0x66, 0x00, 0x4c, 0xb2, 0xff, 0x99, 0x66, 0x00, 0xb2, 0x4c, /* 60% */

	//Magenta
	0x33, 0xff, 0xcc, 0x00, 0xff, 0x33, 0xff, 0x33, 0x00, 0xcc, 0xff, 0x33, 0xff, 0x33, 0xcc, 0x00, 0x33, 0xff, 0xff, 0x33, 0xcc, 0x00, 0xff, 0x33, /* 20% */
	0x40, 0xff, 0xbf, 0x00, 0xff, 0x40, 0xff, 0x40, 0x00, 0xbf, 0xff, 0x40, 0xff, 0x40, 0xbf, 0x00, 0x40, 0xff, 0xff, 0x40, 0xbf, 0x00, 0xff, 0x40, /* 25% */
	0x4d, 0xff, 0xb2, 0x00, 0xff, 0x4d, 0xff, 0x4d, 0x00, 0xb2, 0xff, 0x4d, 0xff, 0x4d, 0xb2, 0x00, 0x4d, 0xff, 0xff, 0x4d, 0xb2, 0x00, 0xff, 0x4d, /* 30% */
	0x59, 0xff, 0xa6, 0x00, 0xff, 0x59, 0xff, 0x59, 0x00, 0xa6, 0xff, 0x59, 0xff, 0x59, 0xa6, 0x00, 0x59, 0xff, 0xff, 0x59, 0xa6, 0x00, 0xff, 0x59, /* 35% */
	0x66, 0xff, 0x99, 0x00, 0xff, 0x66, 0xff, 0x66, 0x00, 0x99, 0xff, 0x66, 0xff, 0x66, 0x99, 0x00, 0x66, 0xff, 0xff, 0x66, 0x99, 0x00, 0xff, 0x66, /* 40% */
	0x73, 0xff, 0x8c, 0x00, 0xff, 0x73, 0xff, 0x73, 0x00, 0x8c, 0xff, 0x73, 0xff, 0x73, 0x8c, 0x00, 0x73, 0xff, 0xff, 0x73, 0x8c, 0x00, 0xff, 0x73, /* 45% */
	0x80, 0xff, 0x7f, 0x00, 0xff, 0x80, 0xff, 0x80, 0x00, 0x7f, 0xff, 0x80, 0xff, 0x80, 0x7f, 0x00, 0x80, 0xff, 0xff, 0x80, 0x7f, 0x00, 0xff, 0x80, /* 50% */
	0x8c, 0xff, 0x73, 0x00, 0xff, 0x8c, 0xff, 0x8c, 0x00, 0x73, 0xff, 0x8c, 0xff, 0x8c, 0x73, 0x00, 0x8c, 0xff, 0xff, 0x8c, 0x73, 0x00, 0xff, 0x8c, /* 55% */
	0x99, 0xff, 0x66, 0x00, 0xff, 0x99, 0xff, 0x99, 0x00, 0x66, 0xff, 0x99, 0xff, 0x99, 0x66, 0x00, 0x99, 0xff, 0xff, 0x99, 0x66, 0x00, 0xff, 0x99, /* 60% */

	//Violet
	0x19, 0xe5, 0xcc, 0x00, 0xff, 0x33, 0xe5, 0x19, 0x00, 0xcc, 0xff, 0x33, 0xe5, 0x19, 0xcc, 0x00, 0x33, 0xff, 0xe5, 0x19, 0xcc, 0x00, 0xff, 0x33, /* 20% */
	0x20, 0xdf, 0xbf, 0x00, 0xff, 0x40, 0xdf, 0x20, 0x00, 0xbf, 0xff, 0x40, 0xdf, 0x20, 0xbf, 0x00, 0x40, 0xff, 0xdf, 0x20, 0xbf, 0x00, 0xff, 0x40, /* 25% */
	0x26, 0xd8, 0xb2, 0x00, 0xff, 0x4d, 0xd8, 0x26, 0x00, 0xb2, 0xff, 0x4d, 0xd8, 0x26, 0xb2, 0x00, 0x4d, 0xff, 0xd8, 0x26, 0xb2, 0x00, 0xff, 0x4d, /* 30% */
	0x2c, 0xd2, 0xa6, 0x00, 0xff, 0x59, 0xd2, 0x2c, 0x00, 0xa6, 0xff, 0x59, 0xd2, 0x2c, 0xa6, 0x00, 0x59, 0xff, 0xd2, 0x2c, 0xa6, 0x00, 0xff, 0x59, /* 35% */
	0x33, 0xcc, 0x99, 0x00, 0xff, 0x66, 0xcc, 0x33, 0x00, 0x99, 0xff, 0x66, 0xcc, 0x33, 0x99, 0x00, 0x66, 0xff, 0xcc, 0x33, 0x99, 0x00, 0xff, 0x66, /* 40% */
	0x39, 0xc5, 0x8c, 0x00, 0xff, 0x73, 0xc5, 0x39, 0x00, 0x8c, 0xff, 0x73, 0xc5, 0x39, 0x8c, 0x00, 0x73, 0xff, 0xc5, 0x39, 0x8c, 0x00, 0xff, 0x73, /* 45% */
	0x40, 0xbf, 0x7f, 0x00, 0xff, 0x80, 0xbf, 0x40, 0x00, 0x7f, 0xff, 0x80, 0xbf, 0x40, 0x7f, 0x00, 0x80, 0xff, 0xbf, 0x40, 0x7f, 0x00, 0xff, 0x80, /* 50% */
	0x46, 0xb9, 0x73, 0x00, 0xff, 0x8c, 0xb9, 0x46, 0x00, 0x73, 0xff, 0x8c, 0xb9, 0x46, 0x73, 0x00, 0x8c, 0xff, 0xb9, 0x46, 0x73, 0x00, 0xff, 0x8c, /* 55% */
	0x4c, 0xb2, 0x66, 0x00, 0xff, 0x99, 0xb2, 0x4c, 0x00, 0x66, 0xff, 0x99, 0xb2, 0x4c, 0x66, 0x00, 0x99, 0xff, 0xb2, 0x4c, 0x66, 0x00, 0xff, 0x99, /* 60% */
};

static char DSI0_BYPASS_MDNIE_1[] = {
	//start
	0xDF,
	0x00, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_BYPASS_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_BYPASS_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0x00, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_block 0 000 0000
	0x00, //trans_slope
	0x00, //trans_interval
	//end
};

static char DSI0_NEGATIVE_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0x00, //ascr_skin_Wr
	0x00, //ascr_skin_Wg
	0x00, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0x00, //ascr_Wr
	0xff, //ascr_Kr
	0x00, //ascr_Wg
	0xff, //ascr_Kg
	0x00, //ascr_Wb
	0xff, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_NEGATIVE_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_NEGATIVE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_GRAYSCALE_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0x4c, //ascr_skin_Rr
	0x4c, //ascr_skin_Rg
	0x4c, //ascr_skin_Rb
	0xe2, //ascr_skin_Yr
	0xe2, //ascr_skin_Yg
	0xe2, //ascr_skin_Yb
	0x69, //ascr_skin_Mr
	0x69, //ascr_skin_Mg
	0x69, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0xb3, //ascr_Cr
	0x4c, //ascr_Rr
	0xb3, //ascr_Cg
	0x4c, //ascr_Rg
	0xb3, //ascr_Cb
	0x4c, //ascr_Rb
	0x69, //ascr_Mr
	0x96, //ascr_Gr
	0x69, //ascr_Mg
	0x96, //ascr_Gg
	0x69, //ascr_Mb
	0x96, //ascr_Gb
	0xe2, //ascr_Yr
	0x1d, //ascr_Br
	0xe2, //ascr_Yg
	0x1d, //ascr_Bg
	0xe2, //ascr_Yb
	0x1d, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_GRAYSCALE_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_GRAYSCALE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0x4c, //ascr_skin_Rr
	0x4c, //ascr_skin_Rg
	0x4c, //ascr_skin_Rb
	0xe2, //ascr_skin_Yr
	0xe2, //ascr_skin_Yg
	0xe2, //ascr_skin_Yb
	0x69, //ascr_skin_Mr
	0x69, //ascr_skin_Mg
	0x69, //ascr_skin_Mb
	0x00, //ascr_skin_Wr
	0x00, //ascr_skin_Wg
	0x00, //ascr_skin_Wb
	0xb3, //ascr_Cr
	0x4c, //ascr_Rr
	0xb3, //ascr_Cg
	0x4c, //ascr_Rg
	0xb3, //ascr_Cb
	0x4c, //ascr_Rb
	0x69, //ascr_Mr
	0x96, //ascr_Gr
	0x69, //ascr_Mg
	0x96, //ascr_Gg
	0x69, //ascr_Mb
	0x96, //ascr_Gb
	0xe2, //ascr_Yr
	0x1d, //ascr_Br
	0xe2, //ascr_Yg
	0x1d, //ascr_Bg
	0xe2, //ascr_Yb
	0x1d, //ascr_Bb
	0x00, //ascr_Wr
	0xff, //ascr_Kr
	0x00, //ascr_Wg
	0xff, //ascr_Kg
	0x00, //ascr_Wb
	0xff, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_COLOR_BLIND_MDNIE_1[] = {
	//start
	0xDF,
	0x00, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_COLOR_BLIND_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_COLOR_BLIND_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_NIGHT_MODE_MDNIE_1[] = {
	//start
	0xDF,
	0x00, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_NIGHT_MODE_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_NIGHT_MODE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_COLOR_LENS_MDNIE_1[] = {
	//start
	0xDF,
	0x00, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_COLOR_LENS_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_COLOR_LENS_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_HBM_CE_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_HBM_CE_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x10, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x08, //lce_gain 000 0000
	0x30, //lce_color_gain 00 0000
	0x40, //lce_min_ref_offset
	0xb0, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0xbf,
	0x00, //lce_ref_gain 9
	0xd0,
	0x77, //lce_block_size h v 0000 0000
	0x00, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x55, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_large_w
	0x00, //lce_med_w
	0x10, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x01, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_0
	0x21, //curve_1
	0x38, //curve_2
	0x4c, //curve_3
	0x5d, //curve_4
	0x6e, //curve_5
	0x7d, //curve_6
	0x8c, //curve_7
	0x9a, //curve_8
	0xa8, //curve_9
	0xb5, //curve_10
	0xc2, //curve_11
	0xcf, //curve_12
	0xdc, //curve_13
	0xe8, //curve_14
	0xf5, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_HBM_CE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xfc, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_RGB_SENSOR_MDNIE_1[] = {
	//start
	0xDF,
	0x00, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_RGB_SENSOR_MDNIE_2[] ={
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_RGB_SENSOR_MDNIE_3[] ={
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_SCREEN_CURTAIN_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0x00, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0x00, //ascr_skin_Yr
	0x00, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0x00, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0x00, //ascr_skin_Mb
	0x00, //ascr_skin_Wr
	0x00, //ascr_skin_Wg
	0x00, //ascr_skin_Wb
	0x00, //ascr_Cr
	0x00, //ascr_Rr
	0x00, //ascr_Cg
	0x00, //ascr_Rg
	0x00, //ascr_Cb
	0x00, //ascr_Rb
	0x00, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0x00, //ascr_Gg
	0x00, //ascr_Mb
	0x00, //ascr_Gb
	0x00, //ascr_Yr
	0x00, //ascr_Br
	0x00, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0x00, //ascr_Bb
	0x00, //ascr_Wr
	0x00, //ascr_Kr
	0x00, //ascr_Wg
	0x00, //ascr_Kg
	0x00, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_SCREEN_CURTAIN_MDNIE_2[] ={
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_SCREEN_CURTAIN_MDNIE_3[] ={
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf9, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xac, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf9, //ascr_skin_Wg
	0xac, //ascr_skin_Wb
	0x66, //ascr_Cr
	0xff, //ascr_Rr
	0xf9, //ascr_Cg
	0x60, //ascr_Rg
	0xac, //ascr_Cb
	0x13, //ascr_Rb
	0xff, //ascr_Mr
	0x66, //ascr_Gr
	0x60, //ascr_Mg
	0xf9, //ascr_Gg
	0xac, //ascr_Mb
	0x13, //ascr_Gb
	0xff, //ascr_Yr
	0x66, //ascr_Br
	0xf9, //ascr_Yg
	0x60, //ascr_Bg
	0x13, //ascr_Yb
	0xac, //ascr_Bb
	0xff, //ascr_Wr
	0x66, //ascr_Kr
	0xf9, //ascr_Wg
	0x60, //ascr_Kg
	0xac, //ascr_Wb
	0x13, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x00,
	0x00, //glare_luma_ctl_start 0000 0000
	0x05, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x01, //glare_uni_luma_gain 9
	0x00,
	0xf0, //glare_blk_max_th 0000 0000
	0x12, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x12, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x02, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x18, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static unsigned char DSI0_HDR_VIDEO_1_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xd0, //ascr_skin_Rr
	0x30, //ascr_skin_Rg
	0x50, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xd0, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xf9, //ascr_Wg
	0x00, //ascr_Kg
	0xef, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static unsigned char DSI0_HDR_VIDEO_1_MDNIE_2[] = {
	0xDE,
	0x10, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x05, //gamut_r1
	0xf1,
	0xfe, //gamut_r2
	0xc3,
	0xff, //gamut_r3
	0x4c,
	0xff, //gamut_g1
	0xc3,
	0x03, //gamut_g2
	0xf1,
	0x00, //gamut_g3
	0x4c,
	0x00, //gamut_b1
	0x03,
	0xff, //gamut_b2
	0xec,
	0x04, //gamut_b3
	0x11,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0xf0, //glare_blk_max_th 0000 0000
	0x89, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x89, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x0c, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0xba, //glare_y_avg_th 0000 0000
	0x1e, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x0f, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x20,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x60,
	0x00, //curve_0
	0x09, //curve_1
	0x0d, //curve_2
	0x16, //curve_3
	0x23, //curve_4
	0x33, //curve_5
	0x48, //curve_6
	0x62, //curve_7
	0x7b, //curve_8
	0x96, //curve_9
	0xb4, //curve_10
	0xd2, //curve_11
	0xe6, //curve_12
	0xf0, //curve_13
	0xfa, //curve_14
	0xff, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x05, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static unsigned char DSI0_HDR_VIDEO_1_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf3, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static unsigned char DSI0_HDR_VIDEO_2_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xf0, //ascr_skin_Rr
	0x38, //ascr_skin_Rg
	0x48, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xe0, //ascr_skin_Yg
	0x50, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xf4, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xf4, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xd0, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xe0, //ascr_Yg
	0x00, //ascr_Bg
	0x50, //ascr_Yb
	0xe0, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static unsigned char DSI0_HDR_VIDEO_2_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0xf0, //glare_blk_max_th 0000 0000
	0x89, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x89, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x0c, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0xba, //glare_y_avg_th 0000 0000
	0x1e, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x0f, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0xd0,
	0x00, //curve_0
	0x0d, //curve_1
	0x19, //curve_2
	0x24, //curve_3
	0x35, //curve_4
	0x4e, //curve_5
	0x6d, //curve_6
	0x8f, //curve_7
	0xb2, //curve_8
	0xce, //curve_9
	0xe2, //curve_10
	0xf2, //curve_11
	0xff, //curve_12
	0xff, //curve_13
	0xff, //curve_14
	0xff, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static unsigned char DSI0_HDR_VIDEO_2_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static unsigned char DSI0_HDR_VIDEO_3_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xd0, //ascr_skin_Rr
	0x30, //ascr_skin_Rg
	0x50, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xd0, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xf9, //ascr_Wg
	0x00, //ascr_Kg
	0xef, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static unsigned char DSI0_HDR_VIDEO_3_MDNIE_2[] = {
	0xDE,
	0x10, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x05, //gamut_r1
	0xf1,
	0xfe, //gamut_r2
	0xc3,
	0xff, //gamut_r3
	0x4c,
	0xff, //gamut_g1
	0xc3,
	0x03, //gamut_g2
	0xf1,
	0x00, //gamut_g3
	0x4c,
	0x00, //gamut_b1
	0x03,
	0xff, //gamut_b2
	0xec,
	0x04, //gamut_b3
	0x11,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0xf0, //glare_blk_max_th 0000 0000
	0x89, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x89, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x0c, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0xba, //glare_y_avg_th 0000 0000
	0x1e, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x0f, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x20,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x60,
	0x00, //curve_0
	0x09, //curve_1
	0x0d, //curve_2
	0x16, //curve_3
	0x23, //curve_4
	0x33, //curve_5
	0x46, //curve_6
	0x62, //curve_7
	0x81, //curve_8
	0xa8, //curve_9
	0xd8, //curve_10
	0xf8, //curve_11
	0xfa, //curve_12
	0xfc, //curve_13
	0xff, //curve_14
	0xff, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x05, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static unsigned char DSI0_HDR_VIDEO_3_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf3, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static unsigned char DSI0_HDR_VIDEO_4_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xd0, //ascr_skin_Rr
	0x48, //ascr_skin_Rg
	0x50, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xd0, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xa0, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xf7, //ascr_Wg
	0x00, //ascr_Kg
	0xf0, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static unsigned char DSI0_HDR_VIDEO_4_MDNIE_2[] = {
	0xDE,
	0x10, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x05, //gamut_r1
	0xf1,
	0xfe, //gamut_r2
	0xc3,
	0xff, //gamut_r3
	0x4c,
	0xff, //gamut_g1
	0xc3,
	0x03, //gamut_g2
	0xf1,
	0x00, //gamut_g3
	0x4c,
	0x00, //gamut_b1
	0x03,
	0xff, //gamut_b2
	0xec,
	0x04, //gamut_b3
	0x11,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0xf0, //glare_blk_max_th 0000 0000
	0x89, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x89, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x0c, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0xba, //glare_y_avg_th 0000 0000
	0x1e, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x07, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_0
	0x0c, //curve_1
	0x18, //curve_2
	0x24, //curve_3
	0x34, //curve_4
	0x47, //curve_5
	0x5c, //curve_6
	0x72, //curve_7
	0x8e, //curve_8
	0xaa, //curve_9
	0xc6, //curve_10
	0xd7, //curve_11
	0xe6, //curve_12
	0xf0, //curve_13
	0xfa, //curve_14
	0xff, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x05, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static unsigned char DSI0_HDR_VIDEO_4_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf3, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static unsigned char DSI0_HDR_VIDEO_5_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xd0, //ascr_skin_Rr
	0x48, //ascr_skin_Rg
	0x50, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xd0, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xa0, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xf7, //ascr_Wg
	0x00, //ascr_Kg
	0xf0, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static unsigned char DSI0_HDR_VIDEO_5_MDNIE_2[] = {
	0xDE,
	0x10, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x05, //gamut_r1
	0xf1,
	0xfe, //gamut_r2
	0xc3,
	0xff, //gamut_r3
	0x4c,
	0xff, //gamut_g1
	0xc3,
	0x03, //gamut_g2
	0xf1,
	0x00, //gamut_g3
	0x4c,
	0x00, //gamut_b1
	0x03,
	0xff, //gamut_b2
	0xec,
	0x04, //gamut_b3
	0x11,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0xf0, //glare_blk_max_th 0000 0000
	0x89, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x89, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x0c, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0xba, //glare_y_avg_th 0000 0000
	0x1e, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x07, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_0
	0x0c, //curve_1
	0x18, //curve_2
	0x24, //curve_3
	0x34, //curve_4
	0x47, //curve_5
	0x5c, //curve_6
	0x72, //curve_7
	0x8e, //curve_8
	0xaa, //curve_9
	0xc6, //curve_10
	0xd7, //curve_11
	0xe6, //curve_12
	0xf0, //curve_13
	0xfa, //curve_14
	0xff, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x05, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static unsigned char DSI0_HDR_VIDEO_5_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf3, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static unsigned char DSI0_VIDEO_ENHANCER_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xff, //ascr_skin_Rr
	0x64, //ascr_skin_Rg
	0x70, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfd, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static unsigned char DSI0_VIDEO_ENHANCER_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x11, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x08, //lce_gain 000 0000
	0x18, //lce_color_gain 00 0000
	0xff, //lce_min_ref_offset
	0xa0, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x40,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0x05, //lce_dark_th 000
	0x01, //lce_reduct_slope 0000
	0x46, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x04, //lce_large_w
	0x06, //lce_med_w
	0x06, //lce_small_w
	0x05, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x1b, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x00, //cadrx_ui_zerobincnt_th
	0x24, //cadrx_ui_ratio_th
	0xea,
	0x0e, //cadrx_entire_freq
	0x10,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x21,
	0x90,
	0x00, //cadrx_high_peak_th_in_freq
	0x21,
	0x66,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdd,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x20, //cadrx_ui_illum_a1
	0x40, //cadrx_ui_illum_a2
	0x60, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0xd0,
	0x00, //cadrx_ui_illum_offset2
	0xc0,
	0x00, //cadrx_ui_illum_offset3
	0xb0,
	0x00, //cadrx_ui_illum_offset4
	0xa0,
	0x07, //cadrx_ui_illum_slope1
	0x80,
	0x07, //cadrx_ui_illum_slope2
	0x80,
	0x07, //cadrx_ui_illum_slope3
	0x80,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x20, //cadrx_ui_ref_a1
	0x40, //cadrx_ui_ref_a2
	0x60, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x60,
	0x01, //cadrx_ui_ref_offset2
	0x50,
	0x01, //cadrx_ui_ref_offset3
	0x40,
	0x01, //cadrx_ui_ref_offset4
	0x30,
	0x07, //cadrx_ui_ref_slope1
	0x80,
	0x07, //cadrx_ui_ref_slope2
	0x80,
	0x07, //cadrx_ui_ref_slope3
	0x80,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x00, //cadrx_reg_ref_c1_offset
	0x80,
	0x00, //cadrx_reg_ref_c2_offset
	0xac,
	0x00, //cadrx_reg_ref_c3_offset
	0xb6,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x40,
	0x00, //cadrx_reg_ref_c3_slope
	0x48,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x00, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x01, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0x84,
	0x3c, //le_p
	0x28, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x40,
	0xff, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x80,
	0x01, //le_luminance_slope 10
	0x00,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x07, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x0a, //curve_1
	0x17, //curve_2
	0x26, //curve_3
	0x36, //curve_4
	0x49, //curve_5
	0x5c, //curve_6
	0x6f, //curve_7
	0x82, //curve_8
	0x95, //curve_9
	0xaa, //curve_10
	0xbe, //curve_11
	0xcf, //curve_12
	0xdf, //curve_13
	0xee, //curve_14
	0xf9, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static unsigned char DSI0_VIDEO_ENHANCER_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xfc, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static unsigned char DSI0_VIDEO_ENHANCER_THIRD_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xff, //ascr_skin_Rr
	0x64, //ascr_skin_Rg
	0x70, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfd, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static unsigned char DSI0_VIDEO_ENHANCER_THIRD_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x11, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x01, //lce_gain 000 0000
	0x18, //lce_color_gain 00 0000
	0xff, //lce_min_ref_offset
	0xa0, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x40,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0x05, //lce_dark_th 000
	0x01, //lce_reduct_slope 0000
	0x46, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x04, //lce_large_w
	0x06, //lce_med_w
	0x06, //lce_small_w
	0x05, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x1b, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x00, //cadrx_ui_zerobincnt_th
	0x24, //cadrx_ui_ratio_th
	0xea,
	0x0e, //cadrx_entire_freq
	0x10,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x21,
	0x90,
	0x00, //cadrx_high_peak_th_in_freq
	0x21,
	0x66,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdd,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x20, //cadrx_ui_illum_a1
	0x40, //cadrx_ui_illum_a2
	0x60, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0xd0,
	0x00, //cadrx_ui_illum_offset2
	0xc0,
	0x00, //cadrx_ui_illum_offset3
	0xb0,
	0x00, //cadrx_ui_illum_offset4
	0xa0,
	0x07, //cadrx_ui_illum_slope1
	0x80,
	0x07, //cadrx_ui_illum_slope2
	0x80,
	0x07, //cadrx_ui_illum_slope3
	0x80,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x20, //cadrx_ui_ref_a1
	0x40, //cadrx_ui_ref_a2
	0x60, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x60,
	0x01, //cadrx_ui_ref_offset2
	0x50,
	0x01, //cadrx_ui_ref_offset3
	0x40,
	0x01, //cadrx_ui_ref_offset4
	0x30,
	0x07, //cadrx_ui_ref_slope1
	0x80,
	0x07, //cadrx_ui_ref_slope2
	0x80,
	0x07, //cadrx_ui_ref_slope3
	0x80,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x00, //cadrx_reg_ref_c1_offset
	0x80,
	0x00, //cadrx_reg_ref_c2_offset
	0xac,
	0x00, //cadrx_reg_ref_c3_offset
	0xb6,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x40,
	0x00, //cadrx_reg_ref_c3_slope
	0x48,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x00, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x01, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0x84,
	0x3c, //le_p
	0x28, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x40,
	0xff, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x80,
	0x01, //le_luminance_slope 10
	0x00,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x07, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x82, //curve_8
	0x95, //curve_9
	0xaa, //curve_10
	0xbe, //curve_11
	0xcf, //curve_12
	0xdf, //curve_13
	0xee, //curve_14
	0xf9, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static unsigned char DSI0_VIDEO_ENHANCER_THIRD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xfc, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_UI_DYNAMIC_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xe3, //ascr_skin_Rr
	0x21, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xef, //ascr_skin_Mr
	0x30, //ascr_skin_Mg
	0xe9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5d, //ascr_Cr
	0xe3, //ascr_Rr
	0xf2, //ascr_Cg
	0x21, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xef, //ascr_Mr
	0x50, //ascr_Gr
	0x30, //ascr_Mg
	0xe4, //ascr_Gg
	0xe9, //ascr_Mb
	0x00, //ascr_Gb
	0xf0, //ascr_Yr
	0x2f, //ascr_Br
	0xeb, //ascr_Yg
	0x23, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_UI_DYNAMIC_MDNIE_2[] ={
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_UI_DYNAMIC_MDNIE_3[] ={
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_UI_STANDARD_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xf0, //ascr_skin_Rr
	0x49, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf1, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x43, //ascr_skin_Yb
	0xfd, //ascr_skin_Mr
	0x50, //ascr_skin_Mg
	0xea, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xea, //ascr_Cg
	0x49, //ascr_Rg
	0xe7, //ascr_Cb
	0x2e, //ascr_Rb
	0xfd, //ascr_Mr
	0x00, //ascr_Gr
	0x50, //ascr_Mg
	0xdf, //ascr_Gg
	0xea, //ascr_Mb
	0x31, //ascr_Gb
	0xf1, //ascr_Yr
	0x2c, //ascr_Br
	0xed, //ascr_Yg
	0x22, //ascr_Bg
	0x43, //ascr_Yb
	0xe0, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_UI_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x02, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x18,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_UI_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_UI_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xd6, //ascr_skin_Rr
	0x3c, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x57, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x45, //ascr_skin_Mg
	0xe2, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x92, //ascr_Cr
	0xd6, //ascr_Rr
	0xef, //ascr_Cg
	0x3c, //ascr_Rg
	0xe9, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x89, //ascr_Gr
	0x45, //ascr_Mg
	0xe4, //ascr_Gg
	0xe2, //ascr_Mb
	0x4d, //ascr_Gb
	0xf2, //ascr_Yr
	0x2e, //ascr_Br
	0xed, //ascr_Yg
	0x21, //ascr_Bg
	0x57, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_UI_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_UI_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_UI_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_UI_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0xf0, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0xba, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_UI_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xe3, //ascr_skin_Rr
	0x21, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xef, //ascr_skin_Mr
	0x30, //ascr_skin_Mg
	0xe9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5d, //ascr_Cr
	0xe3, //ascr_Rr
	0xf2, //ascr_Cg
	0x21, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xef, //ascr_Mr
	0x50, //ascr_Gr
	0x30, //ascr_Mg
	0xe4, //ascr_Gg
	0xe9, //ascr_Mb
	0x00, //ascr_Gb
	0xf0, //ascr_Yr
	0x2f, //ascr_Br
	0xeb, //ascr_Yg
	0x23, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x04, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_VIDEO_STANDARD_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xf0, //ascr_skin_Rr
	0x49, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf1, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x43, //ascr_skin_Yb
	0xfd, //ascr_skin_Mr
	0x50, //ascr_skin_Mg
	0xea, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xea, //ascr_Cg
	0x49, //ascr_Rg
	0xe7, //ascr_Cb
	0x2e, //ascr_Rb
	0xfd, //ascr_Mr
	0x00, //ascr_Gr
	0x50, //ascr_Mg
	0xdf, //ascr_Gg
	0xea, //ascr_Mb
	0x31, //ascr_Gb
	0xf1, //ascr_Yr
	0x2c, //ascr_Br
	0xed, //ascr_Yg
	0x22, //ascr_Bg
	0x43, //ascr_Yb
	0xe0, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_VIDEO_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x06, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x18,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_VIDEO_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_VIDEO_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xd6, //ascr_skin_Rr
	0x3c, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x57, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x45, //ascr_skin_Mg
	0xe2, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x92, //ascr_Cr
	0xd6, //ascr_Rr
	0xef, //ascr_Cg
	0x3c, //ascr_Rg
	0xe9, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x89, //ascr_Gr
	0x45, //ascr_Mg
	0xe4, //ascr_Gg
	0xe2, //ascr_Mb
	0x4d, //ascr_Gb
	0xf2, //ascr_Yr
	0x2e, //ascr_Br
	0xed, //ascr_Yg
	0x21, //ascr_Bg
	0x57, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_VIDEO_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x04, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_VIDEO_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_VIDEO_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xff, //ascr_skin_Rr
	0x64, //ascr_skin_Rg
	0x70, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfd, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_VIDEO_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0xf0, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0xba, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x0f, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x00, //de_maxplus 11
	0x40,
	0x00, //de_maxminus 11
	0x40,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_0
	0x0a, //curve_1
	0x17, //curve_2
	0x26, //curve_3
	0x36, //curve_4
	0x49, //curve_5
	0x5c, //curve_6
	0x6f, //curve_7
	0x82, //curve_8
	0x95, //curve_9
	0xa8, //curve_10
	0xbb, //curve_11
	0xcb, //curve_12
	0xdb, //curve_13
	0xeb, //curve_14
	0xf8, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_VIDEO_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_1[] ={
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xe3, //ascr_skin_Rr
	0x21, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xef, //ascr_skin_Mr
	0x30, //ascr_skin_Mg
	0xe9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5d, //ascr_Cr
	0xe3, //ascr_Rr
	0xf2, //ascr_Cg
	0x21, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xef, //ascr_Mr
	0x50, //ascr_Gr
	0x30, //ascr_Mg
	0xe4, //ascr_Gg
	0xe9, //ascr_Mb
	0x00, //ascr_Gb
	0xf0, //ascr_Yr
	0x2f, //ascr_Br
	0xeb, //ascr_Yg
	0x23, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_CAMERA_STANDARD_MDNIE_1[] ={
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xf0, //ascr_skin_Rr
	0x49, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf1, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x43, //ascr_skin_Yb
	0xfd, //ascr_skin_Mr
	0x50, //ascr_skin_Mg
	0xea, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xea, //ascr_Cg
	0x49, //ascr_Rg
	0xe7, //ascr_Cb
	0x2e, //ascr_Rb
	0xfd, //ascr_Mr
	0x00, //ascr_Gr
	0x50, //ascr_Mg
	0xdf, //ascr_Gg
	0xea, //ascr_Mb
	0x31, //ascr_Gb
	0xf1, //ascr_Yr
	0x2c, //ascr_Br
	0xed, //ascr_Yg
	0x22, //ascr_Bg
	0x43, //ascr_Yb
	0xe0, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_CAMERA_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x02, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x18,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_CAMERA_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_CAMERA_NATURAL_MDNIE_1[] ={
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xd6, //ascr_skin_Rr
	0x3c, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x57, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x45, //ascr_skin_Mg
	0xe2, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x92, //ascr_Cr
	0xd6, //ascr_Rr
	0xef, //ascr_Cg
	0x3c, //ascr_Rg
	0xe9, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x89, //ascr_Gr
	0x45, //ascr_Mg
	0xe4, //ascr_Gg
	0xe2, //ascr_Mb
	0x4d, //ascr_Gb
	0xf2, //ascr_Yr
	0x2e, //ascr_Br
	0xed, //ascr_Yg
	0x21, //ascr_Bg
	0x57, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_CAMERA_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_CAMERA_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_CAMERA_AUTO_MDNIE_1[] ={
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x52, //ascr_skin_Rg
	0x50, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf8, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xfa, //ascr_Cb
	0x00, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xf8, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_CAMERA_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0xf0, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0xba, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_CAMERA_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_1[] ={
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xe3, //ascr_skin_Rr
	0x21, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xef, //ascr_skin_Mr
	0x30, //ascr_skin_Mg
	0xe9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5d, //ascr_Cr
	0xe3, //ascr_Rr
	0xf2, //ascr_Cg
	0x21, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xef, //ascr_Mr
	0x50, //ascr_Gr
	0x30, //ascr_Mg
	0xe4, //ascr_Gg
	0xe9, //ascr_Mb
	0x00, //ascr_Gb
	0xf0, //ascr_Yr
	0x2f, //ascr_Br
	0xeb, //ascr_Yg
	0x23, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x04, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_GALLERY_STANDARD_MDNIE_1[] ={
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xf0, //ascr_skin_Rr
	0x49, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf1, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x43, //ascr_skin_Yb
	0xfd, //ascr_skin_Mr
	0x50, //ascr_skin_Mg
	0xea, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xea, //ascr_Cg
	0x49, //ascr_Rg
	0xe7, //ascr_Cb
	0x2e, //ascr_Rb
	0xfd, //ascr_Mr
	0x00, //ascr_Gr
	0x50, //ascr_Mg
	0xdf, //ascr_Gg
	0xea, //ascr_Mb
	0x31, //ascr_Gb
	0xf1, //ascr_Yr
	0x2c, //ascr_Br
	0xed, //ascr_Yg
	0x22, //ascr_Bg
	0x43, //ascr_Yb
	0xe0, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_GALLERY_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x06, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x18,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_GALLERY_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_GALLERY_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xd6, //ascr_skin_Rr
	0x3c, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x57, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x45, //ascr_skin_Mg
	0xe2, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x92, //ascr_Cr
	0xd6, //ascr_Rr
	0xef, //ascr_Cg
	0x3c, //ascr_Rg
	0xe9, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x89, //ascr_Gr
	0x45, //ascr_Mg
	0xe4, //ascr_Gg
	0xe2, //ascr_Mb
	0x4d, //ascr_Gb
	0xf2, //ascr_Yr
	0x2e, //ascr_Br
	0xed, //ascr_Yg
	0x21, //ascr_Bg
	0x57, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_GALLERY_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x04, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_GALLERY_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_GALLERY_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x52, //ascr_skin_Rg
	0x50, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf8, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xfa, //ascr_Cb
	0x00, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xf8, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_GALLERY_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0xf0, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0xba, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x0c, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_GALLERY_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xe3, //ascr_skin_Rr
	0x21, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xef, //ascr_skin_Mr
	0x30, //ascr_skin_Mg
	0xe9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5d, //ascr_Cr
	0xe3, //ascr_Rr
	0xf2, //ascr_Cg
	0x21, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xef, //ascr_Mr
	0x50, //ascr_Gr
	0x30, //ascr_Mg
	0xe4, //ascr_Gg
	0xe9, //ascr_Mb
	0x00, //ascr_Gb
	0xf0, //ascr_Yr
	0x2f, //ascr_Br
	0xeb, //ascr_Yg
	0x23, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_BROWSER_STANDARD_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xf0, //ascr_skin_Rr
	0x49, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf1, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x43, //ascr_skin_Yb
	0xfd, //ascr_skin_Mr
	0x50, //ascr_skin_Mg
	0xea, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xea, //ascr_Cg
	0x49, //ascr_Rg
	0xe7, //ascr_Cb
	0x2e, //ascr_Rb
	0xfd, //ascr_Mr
	0x00, //ascr_Gr
	0x50, //ascr_Mg
	0xdf, //ascr_Gg
	0xea, //ascr_Mb
	0x31, //ascr_Gb
	0xf1, //ascr_Yr
	0x2c, //ascr_Br
	0xed, //ascr_Yg
	0x22, //ascr_Bg
	0x43, //ascr_Yb
	0xe0, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_BROWSER_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x02, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x18,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_BROWSER_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_BROWSER_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xd6, //ascr_skin_Rr
	0x3c, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x57, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x45, //ascr_skin_Mg
	0xe2, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x92, //ascr_Cr
	0xd6, //ascr_Rr
	0xef, //ascr_Cg
	0x3c, //ascr_Rg
	0xe9, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x89, //ascr_Gr
	0x45, //ascr_Mg
	0xe4, //ascr_Gg
	0xe2, //ascr_Mb
	0x4d, //ascr_Gb
	0xf2, //ascr_Yr
	0x2e, //ascr_Br
	0xed, //ascr_Yg
	0x21, //ascr_Bg
	0x57, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_BROWSER_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_BROWSER_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_BROWSER_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x52, //ascr_skin_Rg
	0x50, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf8, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_BROWSER_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x00,
	0x00, //glare_luma_ctl_start 0000 0000
	0x05, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x01, //glare_uni_luma_gain 9
	0x00,
	0xf0, //glare_blk_max_th 0000 0000
	0x12, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x12, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x02, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x18, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_BROWSER_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xe3, //ascr_skin_Rr
	0x21, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xef, //ascr_skin_Mr
	0x30, //ascr_skin_Mg
	0xe9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5d, //ascr_Cr
	0xe3, //ascr_Rr
	0xf2, //ascr_Cg
	0x21, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xef, //ascr_Mr
	0x50, //ascr_Gr
	0x30, //ascr_Mg
	0xe4, //ascr_Gg
	0xe9, //ascr_Mb
	0x00, //ascr_Gb
	0xf0, //ascr_Yr
	0x2f, //ascr_Br
	0xeb, //ascr_Yg
	0x23, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_EBOOK_STANDARD_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xf0, //ascr_skin_Rr
	0x49, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf1, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x43, //ascr_skin_Yb
	0xfd, //ascr_skin_Mr
	0x50, //ascr_skin_Mg
	0xea, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xea, //ascr_Cg
	0x49, //ascr_Rg
	0xe7, //ascr_Cb
	0x2e, //ascr_Rb
	0xfd, //ascr_Mr
	0x00, //ascr_Gr
	0x50, //ascr_Mg
	0xdf, //ascr_Gg
	0xea, //ascr_Mb
	0x31, //ascr_Gb
	0xf1, //ascr_Yr
	0x2c, //ascr_Br
	0xed, //ascr_Yg
	0x22, //ascr_Bg
	0x43, //ascr_Yb
	0xe0, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_EBOOK_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x02, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x18,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_EBOOK_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_EBOOK_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xd6, //ascr_skin_Rr
	0x3c, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x57, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x45, //ascr_skin_Mg
	0xe2, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x92, //ascr_Cr
	0xd6, //ascr_Rr
	0xef, //ascr_Cg
	0x3c, //ascr_Rg
	0xe9, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x89, //ascr_Gr
	0x45, //ascr_Mg
	0xe4, //ascr_Gg
	0xe2, //ascr_Mb
	0x4d, //ascr_Gb
	0xf2, //ascr_Yr
	0x2e, //ascr_Br
	0xed, //ascr_Yg
	0x21, //ascr_Bg
	0x57, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_EBOOK_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_EBOOK_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_EBOOK_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf9, //ascr_skin_Wg
	0xe9, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xf8, //ascr_Wg
	0x00, //ascr_Kg
	0xe7, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_EBOOK_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x00,
	0x00, //glare_luma_ctl_start 0000 0000
	0x05, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x01, //glare_uni_luma_gain 9
	0x00,
	0xf0, //glare_blk_max_th 0000 0000
	0x12, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x12, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x02, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x18, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_EBOOK_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_EMAIL_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfb, //ascr_skin_Wg
	0xee, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfb, //ascr_Wg
	0x00, //ascr_Kg
	0xee, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_EMAIL_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_EMAIL_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_TDMB_STANDARD_MDNIE_1[] ={
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xf0, //ascr_skin_Rr
	0x49, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf1, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x43, //ascr_skin_Yb
	0xfd, //ascr_skin_Mr
	0x50, //ascr_skin_Mg
	0xea, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xea, //ascr_Cg
	0x49, //ascr_Rg
	0xe7, //ascr_Cb
	0x2e, //ascr_Rb
	0xfd, //ascr_Mr
	0x00, //ascr_Gr
	0x50, //ascr_Mg
	0xdf, //ascr_Gg
	0xea, //ascr_Mb
	0x31, //ascr_Gb
	0xf1, //ascr_Yr
	0x2c, //ascr_Br
	0xed, //ascr_Yg
	0x22, //ascr_Bg
	0x43, //ascr_Yb
	0xe0, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_TDMB_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x06, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x18,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_TDMB_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_TDMB_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xd6, //ascr_skin_Rr
	0x3c, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x57, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x45, //ascr_skin_Mg
	0xe2, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x92, //ascr_Cr
	0xd6, //ascr_Rr
	0xef, //ascr_Cg
	0x3c, //ascr_Rg
	0xe9, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x89, //ascr_Gr
	0x45, //ascr_Mg
	0xe4, //ascr_Gg
	0xe2, //ascr_Mb
	0x4d, //ascr_Gb
	0xf2, //ascr_Yr
	0x2e, //ascr_Br
	0xed, //ascr_Yg
	0x21, //ascr_Bg
	0x57, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_TDMB_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x04, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_TDMB_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_TDMB_DYNAMIC_MDNIE_1[] ={
	//start
	0xDF,
	0x11, //linear_on ascr_skin_on 0000 0000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x37, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x47, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x25,
	0x3d,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x1c,
	0xd8,
	0xe3, //ascr_skin_Rr
	0x21, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xef, //ascr_skin_Mr
	0x30, //ascr_skin_Mg
	0xe9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5d, //ascr_Cr
	0xe3, //ascr_Rr
	0xf2, //ascr_Cg
	0x21, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xef, //ascr_Mr
	0x50, //ascr_Gr
	0x30, //ascr_Mg
	0xe4, //ascr_Gg
	0xe9, //ascr_Mb
	0x00, //ascr_Gb
	0xf0, //ascr_Yr
	0x2f, //ascr_Br
	0xeb, //ascr_Yg
	0x23, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_TDMB_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0x00, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x10, //glare_blk_max_curve blk_max_sh 0 0000
	0x00, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x30, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x30, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x30, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x00, //glare_max_cnt_th_dmc 0000 0000
	0x04, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_TDMB_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_TDMB_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x52, //ascr_skin_Rg
	0x50, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf8, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xfa, //ascr_Cb
	0x00, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xf8, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_TDMB_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x28,
	0xa0, //glare_luma_ctl_start 0000 0000
	0x01, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x00, //glare_uni_luma_gain 9
	0x01,
	0xf0, //glare_blk_max_th 0000 0000
	0x00, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x00, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x00, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0xba, //glare_y_avg_th 0000 0000
	0x00, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x0c, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x10,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_TDMB_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_HMT_COLOR_TEMP_3000K_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xcf, //ascr_skin_Wg
	0x7f, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xcf, //ascr_Wg
	0x00, //ascr_Kg
	0x7f, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_HMT_COLOR_TEMP_3000K_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x00,
	0x00, //glare_luma_ctl_start 0000 0000
	0x05, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x01, //glare_uni_luma_gain 9
	0x00,
	0xf0, //glare_blk_max_th 0000 0000
	0x12, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x12, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x02, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x18, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x02, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x40,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_HMT_COLOR_TEMP_3000K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_HMT_COLOR_TEMP_4000K_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xe2, //ascr_skin_Wg
	0xb0, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xe2, //ascr_Wg
	0x00, //ascr_Kg
	0xb0, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_HMT_COLOR_TEMP_4000K_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x00,
	0x00, //glare_luma_ctl_start 0000 0000
	0x05, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x01, //glare_uni_luma_gain 9
	0x00,
	0xf0, //glare_blk_max_th 0000 0000
	0x12, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x12, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x02, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x18, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x02, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x40,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_HMT_COLOR_TEMP_4000K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_HMT_COLOR_TEMP_5000K_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xee, //ascr_skin_Wg
	0xd0, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xee, //ascr_Wg
	0x00, //ascr_Kg
	0xd0, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_HMT_COLOR_TEMP_5000K_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x00,
	0x00, //glare_luma_ctl_start 0000 0000
	0x05, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x01, //glare_uni_luma_gain 9
	0x00,
	0xf0, //glare_blk_max_th 0000 0000
	0x12, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x12, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x02, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x18, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x02, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x40,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_HMT_COLOR_TEMP_5000K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x00,
	0x00, //glare_luma_ctl_start 0000 0000
	0x05, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x01, //glare_uni_luma_gain 9
	0x00,
	0xf0, //glare_blk_max_th 0000 0000
	0x12, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x12, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x02, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x18, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x02, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x40,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static char DSI0_HMT_COLOR_TEMP_7500K_MDNIE_1[] = {
	//start
	0xDF,
	0x01, //linear_on ascr_skin_on 0000 0000
	0x6a, //ascr_skin_cb
	0x9a, //ascr_skin_cr
	0x25, //ascr_dist_up
	0x1a, //ascr_dist_down
	0x16, //ascr_dist_right
	0x2a, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x37,
	0x5a,
	0x00, //ascr_div_down
	0x4e,
	0xc5,
	0x00, //ascr_div_right
	0x5d,
	0x17,
	0x00, //ascr_div_left
	0x30,
	0xc3,
	0xff, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xff, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xff, //ascr_Cb
	0x00, //ascr_Rb
	0xff, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xff, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x00, //ascr_Br
	0xff, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	0x0F, //edge_ctrl
	0x00, //edge_radius 12
	0x6F,
	0x05, //edge_aa_factor
	0x00, //edge_r
	0x00, //edge_g
	0x00, //edge_b
	0x00, //edge_x 11
	0x70,
	0x00, //edge_y 12
	0x70,
	0x44, //edge_coef_a edge_coef_b 0000 0000
};

static char DSI0_HMT_COLOR_TEMP_7500K_MDNIE_2[] = {
	0xDE,
	0x00, //gamut_gamma_on gamut_blk_shift 0000 0000
	0x40, //gamut_scale
	0x04, //gamut_r1
	0x00,
	0x00, //gamut_r2
	0x00,
	0x00, //gamut_r3
	0x00,
	0x00, //gamut_g1
	0x00,
	0x04, //gamut_g2
	0x00,
	0x00, //gamut_g3
	0x00,
	0x00, //gamut_b1
	0x00,
	0x00, //gamut_b2
	0x00,
	0x04, //gamut_b3
	0x00,
	0x00, //slce_on cadrx_en 0000 0000
	0x11, //glare_on
	0x00, //lce_gain 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x77, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //cadrx_dit_en shadow_corr_en ref_ctrl_en 0 0 0
	0x00, //cadrx_gain
	0x0d, //cadrx_low_intensity_th
	0xe1, //cadrx_high_intensity_th
	0x07, //cadrx_ui_zerobincnt_th
	0x29, //cadrx_ui_ratio_th
	0x04,
	0x0f, //cadrx_entire_freq
	0xa0,
	0x00,
	0x03, //cadrx_peak_th_in_freq
	0x7a,
	0xa0,
	0x00, //cadrx_high_peak_th_in_freq
	0x25,
	0x1c,
	0x24, //cadrx_dit_color_gain
	0x00, //cadrx_dit_illum_a0
	0x48, //cadrx_dit_illum_a1
	0xea, //cadrx_dit_illum_a2
	0xfa, //cadrx_dit_illum_a3
	0x00, //cadrx_dit_illum_b0
	0x10,
	0x00, //cadrx_dit_illum_b1
	0x10,
	0x00, //cadrx_dit_illum_b2
	0x9c,
	0x01, //cadrx_dit_illum_b3
	0x00,
	0x00, //cadrx_dit_illum_slope1
	0x00,
	0x00, //cadrx_dit_illum_slope2
	0xdc,
	0x03, //cadrx_dit_illum_slope3
	0xff,
	0x40, //cadrx_ui_illum_a1
	0x80, //cadrx_ui_illum_a2
	0xc0, //cadrx_ui_illum_a3
	0x00, //cadrx_ui_illum_offset1
	0x9a,
	0x00, //cadrx_ui_illum_offset2
	0x9a,
	0x00, //cadrx_ui_illum_offset3
	0x9a,
	0x00, //cadrx_ui_illum_offset4
	0x9a,
	0x00, //cadrx_ui_illum_slope1
	0x00,
	0x00, //cadrx_ui_illum_slope2
	0x00,
	0x00, //cadrx_ui_illum_slope3
	0x00,
	0x00, //cadrx_ui_illum_slope4
	0x00,
	0x40, //cadrx_ui_ref_a1
	0x80, //cadrx_ui_ref_a2
	0xc0, //cadrx_ui_ref_a3
	0x01, //cadrx_ui_ref_offset1
	0x0e,
	0x01, //cadrx_ui_ref_offset2
	0x0e,
	0x01, //cadrx_ui_ref_offset3
	0x0e,
	0x01, //cadrx_ui_ref_offset4
	0x0e,
	0x00, //cadrx_ui_ref_slope1
	0x00,
	0x00, //cadrx_ui_ref_slope2
	0x00,
	0x00, //cadrx_ui_ref_slope3
	0x00,
	0x00, //cadrx_ui_ref_slope4
	0x00,
	0x01, //cadrx_reg_ref_c1_offset
	0x0e,
	0x01, //cadrx_reg_ref_c2_offset
	0x3a,
	0x01, //cadrx_reg_ref_c3_offset
	0x44,
	0x00, //cadrx_reg_ref_c1_slope
	0x01,
	0x00, //cadrx_reg_ref_c2_slope
	0x43,
	0x00, //cadrx_reg_ref_c3_slope
	0x4b,
	0x00, //cadrx_sc_reg_a1
	0x18,
	0x00, //cadrx_sc_reg_b1
	0x1c,
	0x01, //cadrx_sc_k1_int
	0xff,
	0x01, //cadrx_sc_k2_int
	0xff,
	0x03, //cadrx_sc_w1_int
	0xff,
	0xe8,
	0x03, //cadrx_sc_w2_int
	0xff,
	0xe8,
	0x00, //flicker_w_en
	0x14, //flicker_w
	0x01, //flicker_trans_en
	0x04, //flicker_trans_slope
	0x00, //bl_en
	0x03, //bl_hbs_th
	0x10, //bl_lbs_th
	0xC8, //bl_illum_gain
	0x01, //bl_reg_gain 9
	0x2C,
	0x00, //bl_le_diff
	0x00, //bl_cadrx_gain
	0x04, //bl_trans_slope
	0x00, //le_en
	0x40, //le_diff
	0x03, //le_lmax 10
	0xB6,
	0x23, //le_p
	0x24, //le_w_tmf
	0x00, //le_w_struct_diff 9
	0x72,
	0x01, //le_w_struct_diff_gain
	0x00, //le_w_luminance 9
	0x0D,
	0x00, //le_luminance_slope 10
	0x04,
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 9
	0x00,
	0x00, //glare_luma_ctl_start 0000 0000
	0x05, //glare_luma_th 0000 0000
	0x00, //glare_luma_step 9
	0x01,
	0x01, //glare_uni_luma_gain 9
	0x00,
	0xf0, //glare_blk_max_th 0000 0000
	0x12, //glare_blk_max_w 0000 0000
	0x18, //glare_blk_max_curve blk_max_sh 0 0000
	0xf0, //glare_con_ext_max_th 0000 0000
	0x12, //glare_con_ext_max_w 0000 0000
	0x38, //glare_con_ext_max_sign con_ext_max_curve con_ext_max_sh 0 0 0000
	0x00, //glare_max_cnt_th 0000
	0x00, //glare_max_cnt_th 0000 0000
	0x02, //glare_max_cnt_w 0000 0000
	0x38, //glare_max_cnt_sign max_cnt_curve max_cnt_sh 0 0 0000
	0x00, //glare_y_avg_th 0000 0000
	0x18, //glare_y_avg_w 0000 0000
	0x38, //glare_y_avg_sign y_avg_curve y_avg_sh 0 0 0000
	0x00, //glare_max_cnt_th_dmc 0000
	0x05, //glare_max_cnt_th_dmc 0000 0000
	0x02, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x02, //de_maxplus 11
	0x00,
	0x02, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x10,
	0x00, //fa_pcl_ppi 14
	0x00,
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x04, //fa_edge_cnt_weight
	0x0f, //fa_avg_y_weight
	0x01, //cs_gain 10
	0x40,
	0x00, //curve_0
	0x10, //curve_1
	0x20, //curve_2
	0x30, //curve_3
	0x40, //curve_4
	0x50, //curve_5
	0x60, //curve_6
	0x70, //curve_7
	0x80, //curve_8
	0x90, //curve_9
	0xa0, //curve_10
	0xb0, //curve_11
	0xc0, //curve_12
	0xd0, //curve_13
	0xe0, //curve_14
	0xf0, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset 10
	0x00,
	0x00, //curve_low_x
	0x00, //blank
	0x00, //curve_low_y
};

static char DSI0_HMT_COLOR_TEMP_7500K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 00 0000
	0x03, //edge 00
	0xf0, //ascr algo aolce gamut 00 00 00 00
	0xff, //partial_en1
	0xff, //partial_en2
	0x00, //roi_en
	0x00, //roi_block
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0xff, //trans_on trans_block 0 000 0000
	0x04, //trans_slope
	0x01, //trans_interval
	//end
};

static struct dsi_cmd_desc DSI0_BYPASS_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_1)}, DSI0_BYPASS_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_2)}, DSI0_BYPASS_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_3)}, DSI0_BYPASS_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_NEGATIVE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_1)}, DSI0_NEGATIVE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_2)}, DSI0_NEGATIVE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_3)}, DSI0_NEGATIVE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GRAYSCALE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_1)}, DSI0_GRAYSCALE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_2)}, DSI0_GRAYSCALE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_3)}, DSI0_GRAYSCALE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GRAYSCALE_NEGATIVE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_1)}, DSI0_GRAYSCALE_NEGATIVE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_2)}, DSI0_GRAYSCALE_NEGATIVE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_3)}, DSI0_GRAYSCALE_NEGATIVE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_COLOR_BLIND_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_1)}, DSI0_COLOR_BLIND_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_2)}, DSI0_COLOR_BLIND_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_3)}, DSI0_COLOR_BLIND_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_NIGHT_MODE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_1)}, DSI0_NIGHT_MODE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_2)}, DSI0_NIGHT_MODE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_3)}, DSI0_NIGHT_MODE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_COLOR_LENS_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_COLOR_LENS_MDNIE_1)}, DSI0_COLOR_LENS_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_COLOR_LENS_MDNIE_2)}, DSI0_COLOR_LENS_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_COLOR_LENS_MDNIE_3)}, DSI0_COLOR_LENS_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HBM_CE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_1)}, DSI0_HBM_CE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_2)}, DSI0_HBM_CE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_3)}, DSI0_HBM_CE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_RGB_SENSOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_RGB_SENSOR_MDNIE_1)}, DSI0_RGB_SENSOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_RGB_SENSOR_MDNIE_2)}, DSI0_RGB_SENSOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_RGB_SENSOR_MDNIE_3)}, DSI0_RGB_SENSOR_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_SCREEN_CURTAIN_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_1)}, DSI0_SCREEN_CURTAIN_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_2)}, DSI0_SCREEN_CURTAIN_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_3)}, DSI0_SCREEN_CURTAIN_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_LIGHT_NOTIFICATION_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_1)}, DSI0_LIGHT_NOTIFICATION_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_2)}, DSI0_LIGHT_NOTIFICATION_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_3)}, DSI0_LIGHT_NOTIFICATION_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_1_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_1)}, DSI0_HDR_VIDEO_1_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_2)}, DSI0_HDR_VIDEO_1_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_3)}, DSI0_HDR_VIDEO_1_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_2_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_1)}, DSI0_HDR_VIDEO_2_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_2)}, DSI0_HDR_VIDEO_2_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_3)}, DSI0_HDR_VIDEO_2_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_3_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_1)}, DSI0_HDR_VIDEO_3_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_2)}, DSI0_HDR_VIDEO_3_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_3)}, DSI0_HDR_VIDEO_3_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_4_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_1)}, DSI0_HDR_VIDEO_4_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_2)}, DSI0_HDR_VIDEO_4_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_3)}, DSI0_HDR_VIDEO_4_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_5_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_1)}, DSI0_HDR_VIDEO_5_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_2)}, DSI0_HDR_VIDEO_5_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_3)}, DSI0_HDR_VIDEO_5_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_ENHANCER_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_MDNIE_1)}, DSI0_VIDEO_ENHANCER_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_MDNIE_2)}, DSI0_VIDEO_ENHANCER_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_MDNIE_3)}, DSI0_VIDEO_ENHANCER_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_ENHANCER_THIRD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_THIRD_MDNIE_1)}, DSI0_VIDEO_ENHANCER_THIRD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_THIRD_MDNIE_2)}, DSI0_VIDEO_ENHANCER_THIRD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_THIRD_MDNIE_3)}, DSI0_VIDEO_ENHANCER_THIRD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

///////////////////////////////////////////////////////////////////////////////////

static struct dsi_cmd_desc DSI0_UI_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_1)}, DSI0_UI_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_2)}, DSI0_UI_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_3)}, DSI0_UI_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_STANDARD_MDNIE_1)}, DSI0_UI_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_STANDARD_MDNIE_2)}, DSI0_UI_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_STANDARD_MDNIE_3)}, DSI0_UI_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_NATURAL_MDNIE_1)}, DSI0_UI_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_NATURAL_MDNIE_2)}, DSI0_UI_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_NATURAL_MDNIE_3)}, DSI0_UI_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_1)}, DSI0_UI_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_2)}, DSI0_UI_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_3)}, DSI0_UI_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_1)}, DSI0_VIDEO_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_2)}, DSI0_VIDEO_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_3)}, DSI0_VIDEO_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_STANDARD_MDNIE_1)}, DSI0_VIDEO_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_STANDARD_MDNIE_2)}, DSI0_VIDEO_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_STANDARD_MDNIE_3)}, DSI0_VIDEO_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_NATURAL_MDNIE_1)}, DSI0_VIDEO_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_NATURAL_MDNIE_2)}, DSI0_VIDEO_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_NATURAL_MDNIE_3)}, DSI0_VIDEO_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_1)}, DSI0_VIDEO_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_2)}, DSI0_VIDEO_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_3)}, DSI0_VIDEO_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_1)}, DSI0_CAMERA_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_2)}, DSI0_CAMERA_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_3)}, DSI0_CAMERA_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_STANDARD_MDNIE_1)}, DSI0_CAMERA_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_STANDARD_MDNIE_2)}, DSI0_CAMERA_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_STANDARD_MDNIE_3)}, DSI0_CAMERA_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_NATURAL_MDNIE_1)}, DSI0_CAMERA_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_NATURAL_MDNIE_2)}, DSI0_CAMERA_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_NATURAL_MDNIE_3)}, DSI0_CAMERA_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_1)}, DSI0_CAMERA_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_2)}, DSI0_CAMERA_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_3)}, DSI0_CAMERA_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_1)}, DSI0_GALLERY_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_2)}, DSI0_GALLERY_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_3)}, DSI0_GALLERY_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_STANDARD_MDNIE_1)}, DSI0_GALLERY_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_STANDARD_MDNIE_2)}, DSI0_GALLERY_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_STANDARD_MDNIE_3)}, DSI0_GALLERY_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_NATURAL_MDNIE_1)}, DSI0_GALLERY_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_NATURAL_MDNIE_2)}, DSI0_GALLERY_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_NATURAL_MDNIE_3)}, DSI0_GALLERY_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_1)}, DSI0_GALLERY_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_2)}, DSI0_GALLERY_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_3)}, DSI0_GALLERY_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_1)}, DSI0_BROWSER_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_2)}, DSI0_BROWSER_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_3)}, DSI0_BROWSER_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_STANDARD_MDNIE_1)}, DSI0_BROWSER_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_STANDARD_MDNIE_2)}, DSI0_BROWSER_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_STANDARD_MDNIE_3)}, DSI0_BROWSER_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_NATURAL_MDNIE_1)}, DSI0_BROWSER_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_NATURAL_MDNIE_2)}, DSI0_BROWSER_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_NATURAL_MDNIE_3)}, DSI0_BROWSER_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_1)}, DSI0_BROWSER_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_2)}, DSI0_BROWSER_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_3)}, DSI0_BROWSER_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_1)}, DSI0_EBOOK_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_2)}, DSI0_EBOOK_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_3)}, DSI0_EBOOK_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_STANDARD_MDNIE_1)}, DSI0_EBOOK_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_STANDARD_MDNIE_2)}, DSI0_EBOOK_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_STANDARD_MDNIE_3)}, DSI0_EBOOK_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_NATURAL_MDNIE_1)}, DSI0_EBOOK_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_NATURAL_MDNIE_2)}, DSI0_EBOOK_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_NATURAL_MDNIE_3)}, DSI0_EBOOK_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_1)}, DSI0_EBOOK_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_2)}, DSI0_EBOOK_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_3)}, DSI0_EBOOK_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EMAIL_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_1)}, DSI0_EMAIL_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_2)}, DSI0_EMAIL_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_3)}, DSI0_EMAIL_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GAME_LOW_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_1)}, DSI0_BYPASS_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_2)}, DSI0_BYPASS_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_3)}, DSI0_BYPASS_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GAME_MID_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_1)}, DSI0_BYPASS_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_2)}, DSI0_BYPASS_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_3)}, DSI0_BYPASS_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GAME_HIGH_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_1)}, DSI0_BYPASS_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_2)}, DSI0_BYPASS_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_3)}, DSI0_BYPASS_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_1)}, DSI0_TDMB_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_2)}, DSI0_TDMB_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_3)}, DSI0_TDMB_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_STANDARD_MDNIE_1)}, DSI0_TDMB_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_STANDARD_MDNIE_2)}, DSI0_TDMB_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_STANDARD_MDNIE_3)}, DSI0_TDMB_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_NATURAL_MDNIE_1)}, DSI0_TDMB_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_NATURAL_MDNIE_2)}, DSI0_TDMB_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_NATURAL_MDNIE_3)}, DSI0_TDMB_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_1)}, DSI0_TDMB_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_2)}, DSI0_TDMB_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_3)}, DSI0_TDMB_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
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
			{DSI0_UI_STANDARD_MDNIE,	NULL},
			{DSI0_UI_NATURAL_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_UI_AUTO_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// VIDEO_APP
		{
			{DSI0_VIDEO_DYNAMIC_MDNIE,	NULL},
			{DSI0_VIDEO_STANDARD_MDNIE,	NULL},
			{DSI0_VIDEO_NATURAL_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_VIDEO_AUTO_MDNIE,	NULL},
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
			{DSI0_CAMERA_DYNAMIC_MDNIE,	NULL},
			{DSI0_CAMERA_STANDARD_MDNIE,	NULL},
			{DSI0_CAMERA_NATURAL_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_CAMERA_AUTO_MDNIE,	NULL},
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
			{DSI0_GALLERY_DYNAMIC_MDNIE,	NULL},
			{DSI0_GALLERY_STANDARD_MDNIE,	NULL},
			{DSI0_GALLERY_NATURAL_MDNIE,	NULL},
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
			{DSI0_BROWSER_DYNAMIC_MDNIE,	NULL},
			{DSI0_BROWSER_STANDARD_MDNIE,	NULL},
			{DSI0_BROWSER_NATURAL_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_BROWSER_AUTO_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// eBOOK_APP
		{
			{DSI0_EBOOK_DYNAMIC_MDNIE,	NULL},
			{DSI0_EBOOK_STANDARD_MDNIE,NULL},
			{DSI0_EBOOK_NATURAL_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// EMAIL_APP
		{
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// GAME_LOW_APP
		{
			{DSI0_GAME_LOW_MDNIE,	NULL},
			{DSI0_GAME_LOW_MDNIE,	NULL},
			{DSI0_GAME_LOW_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_GAME_LOW_MDNIE,	NULL},
			{DSI0_GAME_LOW_MDNIE,	NULL},
		},
		// GAME_MID_APP
		{
			{DSI0_GAME_MID_MDNIE,	NULL},
			{DSI0_GAME_MID_MDNIE,	NULL},
			{DSI0_GAME_MID_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_GAME_MID_MDNIE,	NULL},
			{DSI0_GAME_MID_MDNIE,	NULL},
		},
		// GAME_HIGH_APP
		{
			{DSI0_GAME_HIGH_MDNIE,	NULL},
			{DSI0_GAME_HIGH_MDNIE,	NULL},
			{DSI0_GAME_HIGH_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_GAME_HIGH_MDNIE,	NULL},
			{DSI0_GAME_HIGH_MDNIE,	NULL},
		},
		// VIDEO_ENHANCER_APP
		{
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
		},
		// VIDEO_ENHANCER_THIRD_APP
		{
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
			{NULL,	NULL},
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
		},
		// TDMB_APP
		{
			{DSI0_TDMB_DYNAMIC_MDNIE,       NULL},
			{DSI0_TDMB_STANDARD_MDNIE,      NULL},
			{DSI0_TDMB_NATURAL_MDNIE,       NULL},
			{NULL,         NULL},
			{DSI0_TDMB_AUTO_MDNIE,          NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_3000K_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_3000K_MDNIE_1)}, DSI0_HMT_COLOR_TEMP_3000K_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_3000K_MDNIE_2)}, DSI0_HMT_COLOR_TEMP_3000K_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_3000K_MDNIE_3)}, DSI0_HMT_COLOR_TEMP_3000K_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_4000K_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_4000K_MDNIE_1)}, DSI0_HMT_COLOR_TEMP_4000K_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_4000K_MDNIE_2)}, DSI0_HMT_COLOR_TEMP_4000K_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_4000K_MDNIE_3)}, DSI0_HMT_COLOR_TEMP_4000K_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_5000K_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_5000K_MDNIE_1)}, DSI0_HMT_COLOR_TEMP_5000K_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_5000K_MDNIE_2)}, DSI0_HMT_COLOR_TEMP_5000K_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_5000K_MDNIE_3)}, DSI0_HMT_COLOR_TEMP_5000K_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_6500K_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_1)}, DSI0_HMT_COLOR_TEMP_6500K_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_2)}, DSI0_HMT_COLOR_TEMP_6500K_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_3)}, DSI0_HMT_COLOR_TEMP_6500K_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_7500K_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_7500K_MDNIE_1)}, DSI0_HMT_COLOR_TEMP_7500K_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_7500K_MDNIE_2)}, DSI0_HMT_COLOR_TEMP_7500K_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_7500K_MDNIE_3)}, DSI0_HMT_COLOR_TEMP_7500K_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc *hmt_color_temperature_tune_value_dsi0[HMT_COLOR_TEMP_MAX] = {
	/*
		HMT_COLOR_TEMP_3000K, // 3000K
		HMT_COLOR_TEMP_4000K, // 4000K
		HMT_COLOR_TEMP_5000K, // 5000K
		HMT_COLOR_TEMP_6500K, // 6500K
		HMT_COLOR_TEMP_7500K, // 7500K
	*/
	NULL,
	DSI0_HMT_COLOR_TEMP_3000K_MDNIE,
	DSI0_HMT_COLOR_TEMP_4000K_MDNIE,
	DSI0_HMT_COLOR_TEMP_5000K_MDNIE,
	DSI0_HMT_COLOR_TEMP_6500K_MDNIE,
	DSI0_HMT_COLOR_TEMP_7500K_MDNIE,
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
	DSI0_HDR_VIDEO_4_MDNIE,
	DSI0_HDR_VIDEO_5_MDNIE,
};

#define DSI0_RGB_SENSOR_MDNIE_1_SIZE ARRAY_SIZE(DSI0_RGB_SENSOR_MDNIE_1)
#define DSI0_RGB_SENSOR_MDNIE_2_SIZE ARRAY_SIZE(DSI0_RGB_SENSOR_MDNIE_2)
#define DSI0_RGB_SENSOR_MDNIE_3_SIZE ARRAY_SIZE(DSI0_RGB_SENSOR_MDNIE_3)

#endif
