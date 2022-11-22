/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
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
#ifndef SS_DSI_PANEL_S6E3HF4_AMB526JS01_H
#define SS_DSI_PANEL_S6E3HF4_AMB526JS01_H

#include "../ss_dsi_panel_common.h"

#define HBM_INTERPOLATION_STEP 8 /*443, 465, 488, 510, 533, 555, 578, 600*/

/*otherline panel support*/
struct OtherLine_ParseList {
	char title[30];
	int dtsi_data;
};

#define Luminance_Max 74
#define Hmt_Luminance 37
#define Gray_Scale_Max 256
#define Rgb_Compensation 27
#define Irc_Max 20

struct OtherLine_ParseDataList {
	unsigned int base_luminance[Luminance_Max][2];
	char base_luminance_aid[Luminance_Max][2];
	int gradation_offset[Luminance_Max][9];
	int rgb_offset[Luminance_Max][Rgb_Compensation];
	char elvss_offset[3][5];	/*low temp1(-15 < T <= 0) / 2(T<= -15)*/
	char elvss_data[41];	/*variable size: it should be over than latest's elvss_tx_cmd size*/
	char vint[10];	/*vint tx cmd size*/
	char aor_interpolation[Gray_Scale_Max][2];
	unsigned int hmt_base_luminance[Hmt_Luminance][2];
	char hmt_base_luminance_aid[Hmt_Luminance][2];
	int hmt_gradation_offset[Hmt_Luminance][9];
	int hmt_rgb_offset[Hmt_Luminance][Rgb_Compensation];
	char irc_normal[Gray_Scale_Max][Irc_Max];
	char irc_hbm[HBM_INTERPOLATION_STEP][Irc_Max];
	char hbm_elvss[HBM_INTERPOLATION_STEP];
};

extern struct OtherLine_ParseDataList a2_line_data;

/*0xDA register's D7, D6 bit
    a3 01
    a2 00
*/

#define S6E3HF4_AMB526JS01_A3_LINE 0x01 /*BASIC_FB_PANLE_TYPE*/
#define S6E3HF4_AMB526JS01_A2_LINE 0x00 /*NEW_FB_PANLE_TYPE*/
#define LATEST_PANLE_REV 'M'

struct smartdim_conf *smart_get_conf_S6E3HF4_AMB526JS01(void);
struct smartdim_conf *smart_get_conf_S6E3HF4_AMB526JS01_hmt(void);
int  mdss_get_hero2_fab_type(void);
void set_otherline_dimming_data(void);
#endif
