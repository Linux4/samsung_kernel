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
#ifndef _SS_DSI_MDNIE_S6E3HA0_AMS510CW01_H_
#define _SS_DSI_MDNIE_S6E3HA0_AMS510CW01_H_

#include "../ss_dsi_mdnie_lite_common.h"

#define MDNIE_COLOR_BLINDE_CMD_OFFSET 104

#define ADDRESS_SCR_WHITE_RED   0x7A
#define ADDRESS_SCR_WHITE_GREEN 0x7C
#define ADDRESS_SCR_WHITE_BLUE  0x7E

#define MDNIE_STEP1_INDEX 1
#define MDNIE_STEP2_INDEX 2

static char level_1_key_on[] = {
	0xF0,
	0x5A, 0x5A
};

static char level_1_key_off[] = {
	0xF0,
	0xA5, 0xA5
};

static char DSI0_BYPASS_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0x00, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_BYPASS_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x00, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x00,
	0x07, //sharpen_maxplus 11
	0xff,
	0x07, //sharpen_maxminus 11
	0xff,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x0c, //ascr_dist_up
	0x0c, //ascr_dist_down
	0x0c, //ascr_dist_right
	0x0c, //ascr_dist_left
	0x00, //ascr_div_up 20
	0xaa,
	0xab,
	0x00, //ascr_div_down
	0xaa,
	0xab,
	0x00, //ascr_div_right
	0xaa,
	0xab,
	0x00, //ascr_div_left
	0xaa,
	0xab,
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
	//end
};

static char DSI0_NEGATIVE_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0x0c, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};
static char DSI0_NEGATIVE_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x00, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x00,
	0x07, //sharpen_maxplus 11
	0xff,
	0x07, //sharpen_maxminus 11
	0xff,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x0c, //ascr_dist_up
	0x0c, //ascr_dist_down
	0x0c, //ascr_dist_right
	0x0c, //ascr_dist_left
	0x00, //ascr_div_up 20
	0xaa,
	0xab,
	0x00, //ascr_div_down
	0xaa,
	0xab,
	0x00, //ascr_div_right
	0xaa,
	0xab,
	0x00, //ascr_div_left
	0xaa,
	0xab,
	0x00, //ascr_skin_Rr
	0xff, //ascr_skin_Rg
	0xff, //ascr_skin_Rb
	0x00, //ascr_skin_Yr
	0x00, //ascr_skin_Yg
	0xff, //ascr_skin_Yb
	0x00, //ascr_skin_Mr
	0xff, //ascr_skin_Mg
	0x00, //ascr_skin_Mb
	0x00, //ascr_skin_Wr
	0x00, //ascr_skin_Wg
	0x00, //ascr_skin_Wb
	0xff, //ascr_Cr
	0x00, //ascr_Rr
	0x00, //ascr_Cg
	0xff, //ascr_Rg
	0x00, //ascr_Cb
	0xff, //ascr_Rb
	0x00, //ascr_Mr
	0xff, //ascr_Gr
	0xff, //ascr_Mg
	0x00, //ascr_Gg
	0x00, //ascr_Mb
	0xff, //ascr_Gb
	0x00, //ascr_Yr
	0xff, //ascr_Br
	0x00, //ascr_Yg
	0xff, //ascr_Bg
	0xff, //ascr_Yb
	0x00, //ascr_Bb
	0x00, //ascr_Wr
	0xff, //ascr_Kr
	0x00, //ascr_Wg
	0xff, //ascr_Kg
	0x00, //ascr_Wb
	0xff, //ascr_Kb
	//end
};

static char DSI0_COLOR_BLIND_MDNIE_1[] ={
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1  lce_roi 0 1 00
	0x0c, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};
static char DSI0_COLOR_BLIND_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x00, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x00,
	0x07, //sharpen_maxplus 11
	0xff,
	0x07, //sharpen_maxminus 11
	0xff,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x0c, //ascr_dist_up
	0x0c, //ascr_dist_down
	0x0c, //ascr_dist_right
	0x0c, //ascr_dist_left
	0x00, //ascr_div_up 20
	0xaa,
	0xab,
	0x00, //ascr_div_down
	0xaa,
	0xab,
	0x00, //ascr_div_right
	0xaa,
	0xab,
	0x00, //ascr_div_left
	0xaa,
	0xab,
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
	0x00, //ascr_Cr   <=== blind start
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
	0xff, //ascr_Bb   <=== blind end
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_HBM_CE_MDNIE_1[] = {
	//start
	0xEB,
	0x03, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x0c, // lce 1	lce_roi 0 1 00
	0xc0, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_HBM_CE_MDNIE_2[] = {
	0xEC,
	0x06, //lce_gain 00 0000
	0x30, //lce_color_gain 00 0000
	0x00, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0x79, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0xbf,
	0x00, //lce_ref_gain 9
	0xb0,
	0x77, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x7f, //lce_bin_size_ratio
	0x00, //lce_dark_th 000
	0x40, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x7b, //curve_1_a
	0x03, //curve_2_b
	0x48, //curve_2_a
	0x08, //curve_3_b
	0x32, //curve_3_a
	0x08, //curve_4_b
	0x32, //curve_4_a
	0x08, //curve_5_b
	0x32, //curve_5_a
	0x08, //curve_6_b
	0x32, //curve_6_a
	0x08, //curve_7_b
	0x32, //curve_7_a
	0x10, //curve_8_b
	0x28, //curve_8_a
	0x10, //curve_9_b
	0x28, //curve_9_a
	0x10, //curve10_b
	0x28, //curve10_a
	0x10, //curve11_b
	0x28, //curve11_a
	0x10, //curve12_b
	0x28, //curve12_a
	0x19, //curve13_b
	0x22, //curve13_a
	0x19, //curve14_b
	0x22, //curve14_a
	0x19, //curve15_b
	0x22, //curve15_a
	0x19, //curve16_b
	0x22, //curve16_a
	0x19, //curve17_b
	0x22, //curve17_a
	0x19, //curve18_b
	0x22, //curve18_a
	0x23, //curve19_b
	0x1e, //curve19_a
	0x2e, //curve20_b
	0x1b, //curve20_a
	0x33, //curve21_b
	0x1a, //curve21_a
	0x40, //curve22_b
	0x18, //curve22_a
	0x48, //curve23_b
	0x17, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x90,
	0x2f, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x4e, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x5f, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x1a,
	0x74,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x15,
	0x8f,
	0xff, //ascr_skin_Rr
	0x20, //ascr_skin_Rg
	0x20, //ascr_skin_Rb
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
	//end
};
static char DSI0_HBM_CE_TEXT_MDNIE_1[] = {
	//start
	0xEB,
	0x03, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x0c, // lce 1	lce_roi 0 1 00
	0xc0, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_HBM_CE_TEXT_MDNIE_2[] = {
	0xEC,
	0x06, //lce_gain 00 0000
	0x30, //lce_color_gain 00 0000
	0x00, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0x79, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0xbf,
	0x00, //lce_ref_gain 9
	0xb0,
	0x77, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x7f, //lce_bin_size_ratio
	0x00, //lce_dark_th 000
	0x40, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x7b, //curve_1_a
	0x03, //curve_2_b
	0x48, //curve_2_a
	0x08, //curve_3_b
	0x32, //curve_3_a
	0x08, //curve_4_b
	0x32, //curve_4_a
	0x08, //curve_5_b
	0x32, //curve_5_a
	0x08, //curve_6_b
	0x32, //curve_6_a
	0x08, //curve_7_b
	0x32, //curve_7_a
	0x10, //curve_8_b
	0x28, //curve_8_a
	0x10, //curve_9_b
	0x28, //curve_9_a
	0x10, //curve10_b
	0x28, //curve10_a
	0x10, //curve11_b
	0x28, //curve11_a
	0x10, //curve12_b
	0x28, //curve12_a
	0x19, //curve13_b
	0x22, //curve13_a
	0x19, //curve14_b
	0x22, //curve14_a
	0x19, //curve15_b
	0x22, //curve15_a
	0x19, //curve16_b
	0x22, //curve16_a
	0x19, //curve17_b
	0x22, //curve17_a
	0x19, //curve18_b
	0x22, //curve18_a
	0x23, //curve19_b
	0x1e, //curve19_a
	0x2e, //curve20_b
	0x1b, //curve20_a
	0x33, //curve21_b
	0x1a, //curve21_a
	0x40, //curve22_b
	0x18, //curve22_a
	0x48, //curve23_b
	0x17, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x90,
	0x2f, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x4e, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x5f, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x1a,
	0x74,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x15,
	0x8f,
	0xff, //ascr_skin_Rr
	0x20, //ascr_skin_Rg
	0x20, //ascr_skin_Rb
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
	//end
};

static char DSI0_RGB_SENSOR_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //mdnie_en
	0x00, //data_width mask 00 0000
	0x00, //ascr_roi 1 ascr 00 1 0
	0x33, //algo_roi 1 algo lce_roi 1 lce 00 1 0 00 1 0
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_RGB_SENSOR_MDNIE_2[] ={
	0x00,
	0x00, //lce_gain 00 0000
	0x00, //lce_color_gain 00 0000
	0x00, //lce_scene_change_on scene_trans 0 0000
	0x00, //lce_min_diff
	0x00, //lce_illum_gain
	0x00, //lce_ref_offset 9
	0x00,
	0x00, //lce_ref_gain 9
	0x00,
	0x00, //lce_block_size h v 0000 0000
	0x00, //lce_bright_th
	0x00, //lce_bin_size_ratio
	0x00, //lce_dark_th 000
	0x00, //lce_min_ref_offset
	0x00, //nr sharp cs gamma 0000
	0x00, //nr_mask_th
	0x00, //sharpen_weight 10
	0x00,
	0x00, //sharpen_maxplus 11
	0x00,
	0x00, //sharpen_maxminus 11
	0x00,
	0x00, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x00, //curve_1_a
	0x00, //curve_2_b
	0x00, //curve_2_a
	0x00, //curve_3_b
	0x00, //curve_3_a
	0x00, //curve_4_b
	0x00, //curve_4_a
	0x00, //curve_5_b
	0x00, //curve_5_a
	0x00, //curve_6_b
	0x00, //curve_6_a
	0x00, //curve_7_b
	0x00, //curve_7_a
	0x00, //curve_8_b
	0x00, //curve_8_a
	0x00, //curve_9_b
	0x00, //curve_9_a
	0x00, //curve10_b
	0x00, //curve10_a
	0x00, //curve11_b
	0x00, //curve11_a
	0x00, //curve12_b
	0x00, //curve12_a
	0x00, //curve13_b
	0x00, //curve13_a
	0x00, //curve14_b
	0x00, //curve14_a
	0x00, //curve15_b
	0x00, //curve15_a
	0x00, //curve16_b
	0x00, //curve16_a
	0x00, //curve17_b
	0x00, //curve17_a
	0x00, //curve18_b
	0x00, //curve18_a
	0x00, //curve19_b
	0x00, //curve19_a
	0x00, //curve20_b
	0x00, //curve20_a
	0x00, //curve21_b
	0x00, //curve21_a
	0x00, //curve22_b
	0x00, //curve22_a
	0x00, //curve23_b
	0x00, //curve23_a
	0x00, //curve24_b
	0x00, //curve24_a
	0x00, //ascr_skin_on strength 0 00000
	0x00, //ascr_skin_cb
	0x00, //ascr_skin_cr
	0x00, //ascr_dist_up
	0x00, //ascr_dist_down
	0x00, //ascr_dist_right
	0x00, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x00,
	0x00,
	0x00, //ascr_div_down
	0x00,
	0x00,
	0x00, //ascr_div_right
	0x00,
	0x00,
	0x00, //ascr_div_left
	0x00,
	0x00,
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
	//end
};

static char DSI0_UI_DYNAMIC_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_UI_DYNAMIC_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0c, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x90,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x0f, //curve_1_a
	0x00, //curve_2_b
	0x0f, //curve_2_a
	0x00, //curve_3_b
	0x0f, //curve_3_a
	0x00, //curve_4_b
	0x0f, //curve_4_a
	0x09, //curve_5_b
	0xa2, //curve_5_a
	0x09, //curve_6_b
	0xa2, //curve_6_a
	0x09, //curve_7_b
	0xa2, //curve_7_a
	0x09, //curve_8_b
	0xa2, //curve_8_a
	0x09, //curve_9_b
	0xa2, //curve_9_a
	0x09, //curve10_b
	0xa2, //curve10_a
	0x0a, //curve11_b
	0xa2, //curve11_a
	0x0a, //curve12_b
	0xa2, //curve12_a
	0x0a, //curve13_b
	0xa2, //curve13_a
	0x0a, //curve14_b
	0xa2, //curve14_a
	0x0a, //curve15_b
	0xa2, //curve15_a
	0x0a, //curve16_b
	0xa2, //curve16_a
	0x0a, //curve17_b
	0xa2, //curve17_a
	0x0a, //curve18_b
	0xa2, //curve18_a
	0x0f, //curve19_b
	0xa4, //curve19_a
	0x0f, //curve20_b
	0xa4, //curve20_a
	0x0f, //curve21_b
	0xa4, //curve21_a
	0x23, //curve22_b
	0x1c, //curve22_a
	0x48, //curve23_b
	0x17, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x20,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_UI_STANDARD_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_UI_STANDARD_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_UI_NATURAL_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};
static char DSI0_UI_NATURAL_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0xF8, //ascr_Rr
	0xFA, //ascr_Cg
	0x00, //ascr_Rg
	0xF0, //ascr_Cb
	0x0A, //ascr_Rb
	0xFF, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xF0, //ascr_Gg
	0xE1, //ascr_Mb
	0x00, //ascr_Gb
	0xFF, //ascr_Yr
	0x2F, //ascr_Br
	0xFA, //ascr_Yg
	0x0F, //ascr_Bg
	0x37, //ascr_Yb
	0xF8, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_UI_MOVIE_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};
static char DSI0_UI_MOVIE_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0x87, //ascr_Cr
	0xE7, //ascr_Rr
	0xFF, //ascr_Cg
	0x1E, //ascr_Rg
	0xF2, //ascr_Cb
	0x1E, //ascr_Rb
	0xF5, //ascr_Mr
	0x73, //ascr_Gr
	0x2A, //ascr_Mg
	0xF7, //ascr_Gg
	0xF0, //ascr_Mb
	0x37, //ascr_Gb
	0xFA, //ascr_Yr
	0x2E, //ascr_Br
	0xF5, //ascr_Yg
	0x14, //ascr_Bg
	0x3C, //ascr_Yb
	0xED, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_UI_AUTO_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_UI_AUTO_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_VIDEO_OUTDOOR_MDNIE_1[] ={
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x04, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VIDEO_OUTDOOR_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0e, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x2c, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x4e, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x5f, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x1a,
	0x74,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x15,
	0x8f,
	0xff, //ascr_skin_Rr
	0x20, //ascr_skin_Rg
	0x20, //ascr_skin_Rb
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
	0xfa, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xfb, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0e, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0xd0,
	0x00, //sharpen_maxplus 11
	0x40,
	0x00, //sharpen_maxminus 11
	0x40,
	0x00, //curve_1_b
	0x0f, //curve_1_a
	0x00, //curve_2_b
	0x0f, //curve_2_a
	0x00, //curve_3_b
	0x0f, //curve_3_a
	0x00, //curve_4_b
	0x0f, //curve_4_a
	0x09, //curve_5_b
	0xa2, //curve_5_a
	0x09, //curve_6_b
	0xa2, //curve_6_a
	0x09, //curve_7_b
	0xa2, //curve_7_a
	0x09, //curve_8_b
	0xa2, //curve_8_a
	0x09, //curve_9_b
	0xa2, //curve_9_a
	0x09, //curve10_b
	0xa2, //curve10_a
	0x0a, //curve11_b
	0xa2, //curve11_a
	0x0a, //curve12_b
	0xa2, //curve12_a
	0x0a, //curve13_b
	0xa2, //curve13_a
	0x0a, //curve14_b
	0xa2, //curve14_a
	0x0a, //curve15_b
	0xa2, //curve15_a
	0x0a, //curve16_b
	0xa2, //curve16_a
	0x0a, //curve17_b
	0xa2, //curve17_a
	0x0a, //curve18_b
	0xa2, //curve18_a
	0x0f, //curve19_b
	0xa4, //curve19_a
	0x0f, //curve20_b
	0xa4, //curve20_a
	0x0f, //curve21_b
	0xa4, //curve21_a
	0x23, //curve22_b
	0x1c, //curve22_a
	0x48, //curve23_b
	0x17, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x20,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_VIDEO_STANDARD_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VIDEO_STANDARD_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0xc0,
	0x00, //sharpen_maxplus 11
	0x40,
	0x00, //sharpen_maxminus 11
	0x40,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_VIDEO_NATURAL_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};
static char DSI0_VIDEO_NATURAL_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0x40,
	0x00, //sharpen_maxminus 11
	0x40,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0xF8, //ascr_Rr
	0xFA, //ascr_Cg
	0x00, //ascr_Rg
	0xF0, //ascr_Cb
	0x0A, //ascr_Rb
	0xFF, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xF0, //ascr_Gg
	0xE1, //ascr_Mb
	0x00, //ascr_Gb
	0xFF, //ascr_Yr
	0x2F, //ascr_Br
	0xFA, //ascr_Yg
	0x0F, //ascr_Bg
	0x37, //ascr_Yb
	0xF8, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_VIDEO_MOVIE_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};
static char DSI0_VIDEO_MOVIE_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0x40,
	0x00, //sharpen_maxminus 11
	0x40,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0x87, //ascr_Cr
	0xE7, //ascr_Rr
	0xFF, //ascr_Cg
	0x1E, //ascr_Rg
	0xF2, //ascr_Cb
	0x1E, //ascr_Rb
	0xF5, //ascr_Mr
	0x73, //ascr_Gr
	0x2A, //ascr_Mg
	0xF7, //ascr_Gg
	0xF0, //ascr_Mb
	0x37, //ascr_Gb
	0xFA, //ascr_Yr
	0x2E, //ascr_Br
	0xF5, //ascr_Yg
	0x14, //ascr_Bg
	0x3C, //ascr_Yb
	0xED, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_VIDEO_AUTO_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VIDEO_AUTO_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0xc0,
	0x00, //sharpen_maxplus 11
	0x40,
	0x00, //sharpen_maxminus 11
	0x40,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x30, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
	0xff, //ascr_skin_Rr
	0x50, //ascr_skin_Rg
	0x60, //ascr_skin_Rb
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
	//end
};

static char DSI0_VIDEO_WARM_OUTDOOR_MDNIE_1[] ={
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x04, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VIDEO_WARM_OUTDOOR_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0e, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x2c, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x4e, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x5f, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x1a,
	0x74,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x15,
	0x8f,
	0xff, //ascr_skin_Rr
	0x20, //ascr_skin_Rg
	0x20, //ascr_skin_Rb
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
	0xfa, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xfb, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_VIDEO_WARM_MDNIE_1[] ={
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x04, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VIDEO_WARM_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0e, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x2c, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x4e, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x5f, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x1a,
	0x74,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x15,
	0x8f,
	0xff, //ascr_skin_Rr
	0x20, //ascr_skin_Rg
	0x20, //ascr_skin_Rb
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
	0xfa, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xfb, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_VIDEO_COLD_OUTDOOR_MDNIE_1[] ={
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x04, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VIDEO_COLD_OUTDOOR_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0e, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x2c, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x4e, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x5f, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x1a,
	0x74,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x15,
	0x8f,
	0xff, //ascr_skin_Rr
	0x20, //ascr_skin_Rg
	0x20, //ascr_skin_Rb
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
	0xfa, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xfb, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_VIDEO_COLD_MDNIE_1[] ={
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x04, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VIDEO_COLD_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0e, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x2c, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x4e, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x5f, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x1a,
	0x74,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x15,
	0x8f,
	0xff, //ascr_skin_Rr
	0x20, //ascr_skin_Rg
	0x20, //ascr_skin_Rb
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
	0xfa, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xfb, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_CAMERA_OUTDOOR_MDNIE_1[] ={
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x04, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_CAMERA_OUTDOOR_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0e, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x2c, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x4e, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x5f, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x1a,
	0x74,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x15,
	0x8f,
	0xff, //ascr_skin_Rr
	0x20, //ascr_skin_Rg
	0x20, //ascr_skin_Rb
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
	0xfa, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xfb, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_CAMERA_MDNIE_1[] ={
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_CAMERA_MDNIE_2[] ={
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_CAMERA_AUTO_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_CAMERA_AUTO_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x30, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
	0xff, //ascr_skin_Rr
	0x50, //ascr_skin_Rg
	0x60, //ascr_skin_Rb
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
	//end
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0e, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0xd0,
	0x01, //sharpen_maxplus 11
	0x00,
	0x01, //sharpen_maxminus 11
	0x00,
	0x00, //curve_1_b
	0x0f, //curve_1_a
	0x00, //curve_2_b
	0x0f, //curve_2_a
	0x00, //curve_3_b
	0x0f, //curve_3_a
	0x00, //curve_4_b
	0x0f, //curve_4_a
	0x09, //curve_5_b
	0xa2, //curve_5_a
	0x09, //curve_6_b
	0xa2, //curve_6_a
	0x09, //curve_7_b
	0xa2, //curve_7_a
	0x09, //curve_8_b
	0xa2, //curve_8_a
	0x09, //curve_9_b
	0xa2, //curve_9_a
	0x09, //curve10_b
	0xa2, //curve10_a
	0x0a, //curve11_b
	0xa2, //curve11_a
	0x0a, //curve12_b
	0xa2, //curve12_a
	0x0a, //curve13_b
	0xa2, //curve13_a
	0x0a, //curve14_b
	0xa2, //curve14_a
	0x0a, //curve15_b
	0xa2, //curve15_a
	0x0a, //curve16_b
	0xa2, //curve16_a
	0x0a, //curve17_b
	0xa2, //curve17_a
	0x0a, //curve18_b
	0xa2, //curve18_a
	0x0f, //curve19_b
	0xa4, //curve19_a
	0x0f, //curve20_b
	0xa4, //curve20_a
	0x0f, //curve21_b
	0xa4, //curve21_a
	0x23, //curve22_b
	0x1c, //curve22_a
	0x48, //curve23_b
	0x17, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x20,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_GALLERY_STANDARD_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_GALLERY_STANDARD_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0xc0,
	0x01, //sharpen_maxplus 11
	0x00,
	0x01, //sharpen_maxminus 11
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_GALLERY_NATURAL_MDNIE_1[] ={
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_GALLERY_NATURAL_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0xF8, //ascr_Rr
	0xFA, //ascr_Cg
	0x00, //ascr_Rg
	0xF0, //ascr_Cb
	0x0A, //ascr_Rb
	0xFF, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xF0, //ascr_Gg
	0xE1, //ascr_Mb
	0x00, //ascr_Gb
	0xFF, //ascr_Yr
	0x2F, //ascr_Br
	0xFA, //ascr_Yg
	0x0F, //ascr_Bg
	0x37, //ascr_Yb
	0xF8, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_GALLERY_MOVIE_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_GALLERY_MOVIE_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0x87, //ascr_Cr
	0xE7, //ascr_Rr
	0xFF, //ascr_Cg
	0x1E, //ascr_Rg
	0xF2, //ascr_Cb
	0x1E, //ascr_Rb
	0xF5, //ascr_Mr
	0x73, //ascr_Gr
	0x2A, //ascr_Mg
	0xF7, //ascr_Gg
	0xF0, //ascr_Mb
	0x37, //ascr_Gb
	0xFA, //ascr_Yr
	0x2E, //ascr_Br
	0xF5, //ascr_Yg
	0x14, //ascr_Bg
	0x3C, //ascr_Yb
	0xED, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_GALLERY_AUTO_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_GALLERY_AUTO_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0xc0,
	0x01, //sharpen_maxplus 11
	0x00,
	0x01, //sharpen_maxminus 11
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x30, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
	0xff, //ascr_skin_Rr
	0x50, //ascr_skin_Rg
	0x60, //ascr_skin_Rb
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
	//end
};

static char DSI0_VT_DYNAMIC_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VT_DYNAMIC_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0e, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0xd0,
	0x01, //sharpen_maxplus 11
	0x00,
	0x01, //sharpen_maxminus 11
	0x00,
	0x00, //curve_1_b
	0x0f, //curve_1_a
	0x00, //curve_2_b
	0x0f, //curve_2_a
	0x00, //curve_3_b
	0x0f, //curve_3_a
	0x00, //curve_4_b
	0x0f, //curve_4_a
	0x09, //curve_5_b
	0xa2, //curve_5_a
	0x09, //curve_6_b
	0xa2, //curve_6_a
	0x09, //curve_7_b
	0xa2, //curve_7_a
	0x09, //curve_8_b
	0xa2, //curve_8_a
	0x09, //curve_9_b
	0xa2, //curve_9_a
	0x09, //curve10_b
	0xa2, //curve10_a
	0x0a, //curve11_b
	0xa2, //curve11_a
	0x0a, //curve12_b
	0xa2, //curve12_a
	0x0a, //curve13_b
	0xa2, //curve13_a
	0x0a, //curve14_b
	0xa2, //curve14_a
	0x0a, //curve15_b
	0xa2, //curve15_a
	0x0a, //curve16_b
	0xa2, //curve16_a
	0x0a, //curve17_b
	0xa2, //curve17_a
	0x0a, //curve18_b
	0xa2, //curve18_a
	0x0f, //curve19_b
	0xa4, //curve19_a
	0x0f, //curve20_b
	0xa4, //curve20_a
	0x0f, //curve21_b
	0xa4, //curve21_a
	0x23, //curve22_b
	0x1c, //curve22_a
	0x48, //curve23_b
	0x17, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x20,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_VT_STANDARD_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VT_STANDARD_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0xc0,
	0x01, //sharpen_maxplus 11
	0x00,
	0x01, //sharpen_maxminus 11
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_VT_NATURAL_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VT_NATURAL_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0xF8, //ascr_Rr
	0xFA, //ascr_Cg
	0x00, //ascr_Rg
	0xF0, //ascr_Cb
	0x0A, //ascr_Rb
	0xFF, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xF0, //ascr_Gg
	0xE1, //ascr_Mb
	0x00, //ascr_Gb
	0xFF, //ascr_Yr
	0x2F, //ascr_Br
	0xFA, //ascr_Yg
	0x0F, //ascr_Bg
	0x37, //ascr_Yb
	0xF8, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_VT_MOVIE_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VT_MOVIE_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0x87, //ascr_Cr
	0xE7, //ascr_Rr
	0xFF, //ascr_Cg
	0x1E, //ascr_Rg
	0xF2, //ascr_Cb
	0x1E, //ascr_Rb
	0xF5, //ascr_Mr
	0x73, //ascr_Gr
	0x2A, //ascr_Mg
	0xF7, //ascr_Gg
	0xF0, //ascr_Mb
	0x37, //ascr_Gb
	0xFA, //ascr_Yr
	0x2E, //ascr_Br
	0xF5, //ascr_Yg
	0x14, //ascr_Bg
	0x3C, //ascr_Yb
	0xED, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_VT_AUTO_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_VT_AUTO_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0a, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0xc0,
	0x01, //sharpen_maxplus 11
	0x00,
	0x01, //sharpen_maxminus 11
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0c, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x90,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x0f, //curve_1_a
	0x00, //curve_2_b
	0x0f, //curve_2_a
	0x00, //curve_3_b
	0x0f, //curve_3_a
	0x00, //curve_4_b
	0x0f, //curve_4_a
	0x09, //curve_5_b
	0xa2, //curve_5_a
	0x09, //curve_6_b
	0xa2, //curve_6_a
	0x09, //curve_7_b
	0xa2, //curve_7_a
	0x09, //curve_8_b
	0xa2, //curve_8_a
	0x09, //curve_9_b
	0xa2, //curve_9_a
	0x09, //curve10_b
	0xa2, //curve10_a
	0x0a, //curve11_b
	0xa2, //curve11_a
	0x0a, //curve12_b
	0xa2, //curve12_a
	0x0a, //curve13_b
	0xa2, //curve13_a
	0x0a, //curve14_b
	0xa2, //curve14_a
	0x0a, //curve15_b
	0xa2, //curve15_a
	0x0a, //curve16_b
	0xa2, //curve16_a
	0x0a, //curve17_b
	0xa2, //curve17_a
	0x0a, //curve18_b
	0xa2, //curve18_a
	0x0f, //curve19_b
	0xa4, //curve19_a
	0x0f, //curve20_b
	0xa4, //curve20_a
	0x0f, //curve21_b
	0xa4, //curve21_a
	0x23, //curve22_b
	0x1c, //curve22_a
	0x48, //curve23_b
	0x17, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x20,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_BROWSER_STANDARD_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_BROWSER_STANDARD_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_BROWSER_NATURAL_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_BROWSER_NATURAL_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0xF8, //ascr_Rr
	0xFA, //ascr_Cg
	0x00, //ascr_Rg
	0xF0, //ascr_Cb
	0x0A, //ascr_Rb
	0xFF, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xF0, //ascr_Gg
	0xE1, //ascr_Mb
	0x00, //ascr_Gb
	0xFF, //ascr_Yr
	0x2F, //ascr_Br
	0xFA, //ascr_Yg
	0x0F, //ascr_Bg
	0x37, //ascr_Yb
	0xF8, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_BROWSER_MOVIE_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_BROWSER_MOVIE_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0x87, //ascr_Cr
	0xE7, //ascr_Rr
	0xFF, //ascr_Cg
	0x1E, //ascr_Rg
	0xF2, //ascr_Cb
	0x1E, //ascr_Rb
	0xF5, //ascr_Mr
	0x73, //ascr_Gr
	0x2A, //ascr_Mg
	0xF7, //ascr_Gg
	0xF0, //ascr_Mb
	0x37, //ascr_Gb
	0xFA, //ascr_Yr
	0x2E, //ascr_Br
	0xF5, //ascr_Yg
	0x14, //ascr_Bg
	0x3C, //ascr_Yb
	0xED, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_BROWSER_AUTO_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_BROWSER_AUTO_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x30, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
	0xff, //ascr_skin_Rr
	0x50, //ascr_skin_Rg
	0x60, //ascr_skin_Rb
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
	//end;
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x0c, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x90,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x0f, //curve_1_a
	0x00, //curve_2_b
	0x0f, //curve_2_a
	0x00, //curve_3_b
	0x0f, //curve_3_a
	0x00, //curve_4_b
	0x0f, //curve_4_a
	0x09, //curve_5_b
	0xa2, //curve_5_a
	0x09, //curve_6_b
	0xa2, //curve_6_a
	0x09, //curve_7_b
	0xa2, //curve_7_a
	0x09, //curve_8_b
	0xa2, //curve_8_a
	0x09, //curve_9_b
	0xa2, //curve_9_a
	0x09, //curve10_b
	0xa2, //curve10_a
	0x0a, //curve11_b
	0xa2, //curve11_a
	0x0a, //curve12_b
	0xa2, //curve12_a
	0x0a, //curve13_b
	0xa2, //curve13_a
	0x0a, //curve14_b
	0xa2, //curve14_a
	0x0a, //curve15_b
	0xa2, //curve15_a
	0x0a, //curve16_b
	0xa2, //curve16_a
	0x0a, //curve17_b
	0xa2, //curve17_a
	0x0a, //curve18_b
	0xa2, //curve18_a
	0x0f, //curve19_b
	0xa4, //curve19_a
	0x0f, //curve20_b
	0xa4, //curve20_a
	0x0f, //curve21_b
	0xa4, //curve21_a
	0x23, //curve22_b
	0x1c, //curve22_a
	0x48, //curve23_b
	0x17, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x20,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_EBOOK_STANDARD_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_EBOOK_STANDARD_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x80,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	//end
};

static char DSI0_EBOOK_NATURAL_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_EBOOK_NATURAL_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0xF8, //ascr_Rr
	0xFA, //ascr_Cg
	0x00, //ascr_Rg
	0xF0, //ascr_Cb
	0x0A, //ascr_Rb
	0xFF, //ascr_Mr
	0x00, //ascr_Gr
	0x00, //ascr_Mg
	0xF0, //ascr_Gg
	0xE1, //ascr_Mb
	0x00, //ascr_Gb
	0xFF, //ascr_Yr
	0x2F, //ascr_Br
	0xFA, //ascr_Yg
	0x0F, //ascr_Bg
	0x37, //ascr_Yb
	0xF8, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_EBOOK_MOVIE_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0xcc, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_EBOOK_MOVIE_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x08, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0x87, //ascr_Cr
	0xE7, //ascr_Rr
	0xFF, //ascr_Cg
	0x1E, //ascr_Rg
	0xF2, //ascr_Cb
	0x1E, //ascr_Rb
	0xF5, //ascr_Mr
	0x73, //ascr_Gr
	0x2A, //ascr_Mg
	0xF7, //ascr_Gg
	0xF0, //ascr_Mb
	0x37, //ascr_Gb
	0xFA, //ascr_Yr
	0x2E, //ascr_Br
	0xF5, //ascr_Yg
	0x14, //ascr_Bg
	0x3C, //ascr_Yb
	0xED, //ascr_Bb
	0xFF, //ascr_Wr
	0x00, //ascr_Kr
	0xF6, //ascr_Wg
	0x00, //ascr_Kg
	0xEE, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_EBOOK_AUTO_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0x0c, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_EBOOK_AUTO_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x00, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0xf6, //ascr_Wg
	0x00, //ascr_Kg
	0xe6, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_EMAIL_AUTO_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0x0c, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_EMAIL_AUTO_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x00, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x40,
	0x00, //sharpen_maxplus 11
	0xa0,
	0x00, //sharpen_maxminus 11
	0xa0,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x17, //ascr_dist_up
	0x29, //ascr_dist_down
	0x19, //ascr_dist_right
	0x27, //ascr_dist_left
	0x00, //ascr_div_up 20
	0x59,
	0x0b,
	0x00, //ascr_div_down
	0x31,
	0xf4,
	0x00, //ascr_div_right
	0x51,
	0xec,
	0x00, //ascr_div_left
	0x34,
	0x83,
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
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xef, //ascr_Wb
	0x00, //ascr_Kb
	//end
};

static char DSI0_CURTAIN_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0x0c, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_CURTAIN_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x00, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x00,
	0x07, //sharpen_maxplus 11
	0xff,
	0x07, //sharpen_maxminus 11
	0xff,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x0c, //ascr_dist_up
	0x0c, //ascr_dist_down
	0x0c, //ascr_dist_right
	0x0c, //ascr_dist_left
	0x00, //ascr_div_up 20
	0xaa,
	0xab,
	0x00, //ascr_div_down
	0xaa,
	0xab,
	0x00, //ascr_div_right
	0xaa,
	0xab,
	0x00, //ascr_div_left
	0xaa,
	0xab,
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
	//end
};

static char DSI0_GRAYSCALE_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0x0c, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_GRAYSCALE_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x00, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x00,
	0x07, //sharpen_maxplus 11
	0xff,
	0x07, //sharpen_maxminus 11
	0xff,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x0c, //ascr_dist_up
	0x0c, //ascr_dist_down
	0x0c, //ascr_dist_right
	0x0c, //ascr_dist_left
	0x00, //ascr_div_up 20
	0xaa,
	0xab,
	0x00, //ascr_div_down
	0xaa,
	0xab,
	0x00, //ascr_div_right
	0xaa,
	0xab,
	0x00, //ascr_div_left
	0xaa,
	0xab,
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
	//end
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_1[] = {
	//start
	0xEB,
	0x01, //lce_en mdnie_en 0 0
	0x00, //data_width mask 00 0000
	0x00, // lce 1	lce_roi 0 1 00
	0x0c, //algo 1 algo_roi ascr 1 ascr_roi 0 1 00 0 1 00
	0x00, //roi_ctrl 00
	0x00, //roi0_x_start 12
	0x00,
	0x00, //roi0_x_end
	0x00,
	0x00, //roi0_y_start
	0x00,
	0x00, //roi0_y_end
	0x00,
	0x00, //roi1_x_strat
	0x00,
	0x00, //roi1_x_end
	0x00,
	0x00, //roi1_y_start
	0x00,
	0x00, //roi1_y_end
	0x00,
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_2[] = {
	0xEC,
	0x18, //lce_gain 00 0000
	0x24, //lce_color_gain 00 0000
	0x10, //lce_scene_change_on scene_trans 0 0000
	0x14, //lce_min_diff
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0xfa, //lce_bright_th
	0x2d, //lce_bin_size_ratio
	0x03, //lce_dark_th 000
	0x96, //lce_min_ref_offset
	0x00, //gamma cs sharp nr 0000
	0xff, //nr_mask_th
	0x00, //sharpen_weight 10
	0x00,
	0x07, //sharpen_maxplus 11
	0xff,
	0x07, //sharpen_maxminus 11
	0xff,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x02, //curve_5_b
	0x1b, //curve_5_a
	0x02, //curve_6_b
	0x1b, //curve_6_a
	0x02, //curve_7_b
	0x1b, //curve_7_a
	0x02, //curve_8_b
	0x1b, //curve_8_a
	0x09, //curve_9_b
	0xa6, //curve_9_a
	0x09, //curve10_b
	0xa6, //curve10_a
	0x09, //curve11_b
	0xa6, //curve11_a
	0x09, //curve12_b
	0xa6, //curve12_a
	0x00, //curve13_b
	0x20, //curve13_a
	0x00, //curve14_b
	0x20, //curve14_a
	0x00, //curve15_b
	0x20, //curve15_a
	0x00, //curve16_b
	0x20, //curve16_a
	0x00, //curve17_b
	0x20, //curve17_a
	0x00, //curve18_b
	0x20, //curve18_a
	0x00, //curve19_b
	0x20, //curve19_a
	0x00, //curve20_b
	0x20, //curve20_a
	0x00, //curve21_b
	0x20, //curve21_a
	0x00, //curve22_b
	0x20, //curve22_a
	0x00, //curve23_b
	0x20, //curve23_a
	0x00, //curve24_b
	0xFF, //curve24_a
	0x01, //cs_gain 10
	0x00,
	0x20, //ascr_skin_on strength 0 00000
	0x67, //ascr_skin_cb
	0xa9, //ascr_skin_cr
	0x0c, //ascr_dist_up
	0x0c, //ascr_dist_down
	0x0c, //ascr_dist_right
	0x0c, //ascr_dist_left
	0x00, //ascr_div_up 20
	0xaa,
	0xab,
	0x00, //ascr_div_down
	0xaa,
	0xab,
	0x00, //ascr_div_right
	0xaa,
	0xab,
	0x00, //ascr_div_left
	0xaa,
	0xab,
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
	0x4c, //ascr_Cr
	0xb3, //ascr_Rr
	0x4c, //ascr_Cg
	0xb3, //ascr_Rg
	0x4c, //ascr_Cb
	0xb3, //ascr_Rb
	0x96, //ascr_Mr
	0x69, //ascr_Gr
	0x96, //ascr_Mg
	0x69, //ascr_Gg
	0x96, //ascr_Mb
	0x69, //ascr_Gb
	0x1d, //ascr_Yr
	0xe2, //ascr_Br
	0x1d, //ascr_Yg
	0xe2, //ascr_Bg
	0x1d, //ascr_Yb
	0xe2, //ascr_Bb
	0x00, //ascr_Wr
	0xff, //ascr_Kr
	0x00, //ascr_Wg
	0xff, //ascr_Kg
	0x00, //ascr_Wb
	0xff, //ascr_Kb
	//end
};

static char DSI0_TDMB_STANDARD_MDNIE_1[] = {
	0xEB,
	0x01, //mdnie_en
	0x00, //data_width mask 00 000
	0x33, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x0f, //sharpen cc gamma 00 0 0
};

static char DSI0_TDMB_STANDARD_MDNIE_2[] = {
	0xEC,
	0x00, //roi ctrl
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x30, //scr Rg Bg
	0xff, //scr Cb Yr
	0x30, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x30, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	0x00, //curve 1 b
	0x1c, //curve 1 a
	0x00, //curve 2 b
	0x1c, //curve 2 a
	0x00, //curve 3 b
	0x1c, //curve 3 a
	0x00, //curve 4 b
	0x1c, //curve 4 a
	0x00, //curve 5 b
	0x1c, //curve 5 a
	0x00, //curve 6 b
	0x1c, //curve 6 a
	0x00, //curve 7 b
	0x1c, //curve 7 a
	0x00, //curve 8 b
	0x1c, //curve 8 a
	0x00, //curve 9 b
	0x1c, //curve 9 a
	0x00, //curve10 b
	0x1c, //curve10 a
	0x00, //curve11 b
	0x1c, //curve11 a
	0x00, //curve12 b
	0x1c, //curve12 a
	0x00, //curve13 b
	0x1c, //curve13 a
	0x0d, //curve14 b
	0xa4, //curve14 a
	0x0d, //curve15 b
	0xa4, //curve15 a
	0x0d, //curve16 b
	0xa4, //curve16 a
	0x0d, //curve17 b
	0xa4, //curve17 a
	0x0d, //curve18 b
	0xa4, //curve18 a
	0x0d, //curve19 b
	0xa4, //curve19 a
	0x0d, //curve20 b
	0xa4, //curve20 a
	0x0d, //curve21 b
	0xa4, //curve21 a
	0x25, //curve22 b
	0x1c, //curve22 a
	0x4a, //curve23 b
	0x17, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	0x04, //cc r1 0.3
	0xd7,
	0x1f, //cc r2
	0x4c,
	0x1f, //cc r3
	0xdd,
	0x1f, //cc g1
	0xa4,
	0x04, //cc g2
	0x7f,
	0x1f, //cc g3
	0xdd,
	0x1f, //cc b1
	0xa4,
	0x1f, //cc b2
	0x4c,
	0x05, //cc b3
	0x10,
};

static char DSI0_TDMB_NATURAL_MDNIE_1[] = {
	0xEB,
	0x01, //mdnie_en
	0x00, //data_width mask 00 000
	0x33, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x0f, //sharpen cc gamma 00 0 0
};

static char DSI0_TDMB_NATURAL_MDNIE_2[] = {
	0xEC,
	0x00, //roi ctrl
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	0x00, //scr Cr Yb
	0xf5, //scr Rr Bb
	0xfc, //scr Cg Yg
	0x14, //scr Rg Bg
	0xe2, //scr Cb Yr
	0x24, //scr Rb Br
	0xff, //scr Mr Mb
	0x04, //scr Gr Gb
	0x2d, //scr Mg Mg
	0xdb, //scr Gg Gg
	0xea, //scr Mb Mr
	0x23, //scr Gb Gr
	0xf6, //scr Yr Cb
	0x38, //scr Br Rb
	0xfb, //scr Yg Cg
	0x1c, //scr Bg Rg
	0x4e, //scr Yb Cr
	0xe5, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xfb, //scr Wg Wg
	0x00, //scr Kg Kg
	0xf3, //scr Wb Wr
	0x00, //scr Kb Kr
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x02, //curve 5 b
	0x1b, //curve 5 a
	0x02, //curve 6 b
	0x1b, //curve 6 a
	0x02, //curve 7 b
	0x1b, //curve 7 a
	0x02, //curve 8 b
	0x1b, //curve 8 a
	0x09, //curve 9 b
	0xa6, //curve 9 a
	0x09, //curve10 b
	0xa6, //curve10 a
	0x09, //curve11 b
	0xa6, //curve11 a
	0x09, //curve12 b
	0xa6, //curve12 a
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	0x04, //cc r1 0.2
	0x90,
	0x1f, //cc r2
	0x88,
	0x1f, //cc r3
	0xe8,
	0x1f, //cc g1
	0xc3,
	0x04, //cc g2
	0x55,
	0x1f, //cc g3
	0xe8,
	0x1f, //cc b1
	0xc3,
	0x1f, //cc b2
	0x88,
	0x04, //cc b3
	0xb5,
};

static char DSI0_TDMB_DYNAMIC_MDNIE_1[] = {
	0xEB,
	0x01, //mdnie_en
	0x00, //data_width mask 00 000
	0x33, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x0f, //sharpen cc gamma 00 0 0
};

static char DSI0_TDMB_DYNAMIC_MDNIE_2[] = {
	0xEC,
	0x00, //roi ctrl
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x30, //scr Rg Bg
	0xff, //scr Cb Yr
	0x30, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x30, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	0x00, //curve 1 b
	0x1c, //curve 1 a
	0x00, //curve 2 b
	0x1c, //curve 2 a
	0x00, //curve 3 b
	0x1c, //curve 3 a
	0x00, //curve 4 b
	0x1c, //curve 4 a
	0x00, //curve 5 b
	0x1c, //curve 5 a
	0x00, //curve 6 b
	0x1c, //curve 6 a
	0x00, //curve 7 b
	0x1c, //curve 7 a
	0x00, //curve 8 b
	0x1c, //curve 8 a
	0x00, //curve 9 b
	0x1c, //curve 9 a
	0x00, //curve10 b
	0x1c, //curve10 a
	0x00, //curve11 b
	0x1c, //curve11 a
	0x00, //curve12 b
	0x1c, //curve12 a
	0x00, //curve13 b
	0x1c, //curve13 a
	0x0d, //curve14 b
	0xa4, //curve14 a
	0x0d, //curve15 b
	0xa4, //curve15 a
	0x0d, //curve16 b
	0xa4, //curve16 a
	0x0d, //curve17 b
	0xa4, //curve17 a
	0x0d, //curve18 b
	0xa4, //curve18 a
	0x0d, //curve19 b
	0xa4, //curve19 a
	0x0d, //curve20 b
	0xa4, //curve20 a
	0x0d, //curve21 b
	0xa4, //curve21 a
	0x25, //curve22 b
	0x1c, //curve22 a
	0x4a, //curve23 b
	0x17, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	0x04, //cc r1 0.32
	0xe5,
	0x1f, //cc r2
	0x40,
	0x1f, //cc r3
	0xdb,
	0x1f, //cc g1
	0x9e,
	0x04, //cc g2
	0x87,
	0x1f, //cc g3
	0xdb,
	0x1f, //cc b1
	0x9e,
	0x1f, //cc b2
	0x40,
	0x05, //cc b3
	0x22,
};

static char DSI0_TDMB_MOVIE_MDNIE_1[] = {
	0xEB,
	0x01, //mdnie_en
	0x00, //data_width mask 00 000
	0x33, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x03, //sharpen cc gamma 00 0 0
};

static char DSI0_TDMB_MOVIE_MDNIE_2[] = {
	0xEC,
	0x00, //roi ctrl
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	0x90, //scr Cr Yb
	0xd8, //scr Rr Bb
	0xfc, //scr Cg Yg
	0x22, //scr Rg Bg
	0xea, //scr Cb Yr
	0x2a, //scr Rb Br
	0xe3, //scr Mr Mb
	0x68, //scr Gr Gb
	0x34, //scr Mg Mg
	0xeb, //scr Gg Gg
	0xe6, //scr Mb Mr
	0x44, //scr Gb Gr
	0xec, //scr Yr Cb
	0x2e, //scr Br Rb
	0xf6, //scr Yg Cg
	0x26, //scr Bg Rg
	0x60, //scr Yb Cr
	0xe2, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xfb, //scr Wg Wg
	0x00, //scr Kg Kg
	0xf3, //scr Wb Wr
	0x00, //scr Kb Kr
	0x00, //curve 1 b
	0x20, //curve 1 a
	0x00, //curve 2 b
	0x20, //curve 2 a
	0x00, //curve 3 b
	0x20, //curve 3 a
	0x00, //curve 4 b
	0x20, //curve 4 a
	0x02, //curve 5 b
	0x1b, //curve 5 a
	0x02, //curve 6 b
	0x1b, //curve 6 a
	0x02, //curve 7 b
	0x1b, //curve 7 a
	0x02, //curve 8 b
	0x1b, //curve 8 a
	0x09, //curve 9 b
	0xa6, //curve 9 a
	0x09, //curve10 b
	0xa6, //curve10 a
	0x09, //curve11 b
	0xa6, //curve11 a
	0x09, //curve12 b
	0xa6, //curve12 a
	0x00, //curve13 b
	0x20, //curve13 a
	0x00, //curve14 b
	0x20, //curve14 a
	0x00, //curve15 b
	0x20, //curve15 a
	0x00, //curve16 b
	0x20, //curve16 a
	0x00, //curve17 b
	0x20, //curve17 a
	0x00, //curve18 b
	0x20, //curve18 a
	0x00, //curve19 b
	0x20, //curve19 a
	0x00, //curve20 b
	0x20, //curve20 a
	0x00, //curve21 b
	0x20, //curve21 a
	0x00, //curve22 b
	0x20, //curve22 a
	0x00, //curve23 b
	0x20, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	0x04, //cc r1 0.2
	0x90,
	0x1f, //cc r2
	0x88,
	0x1f, //cc r3
	0xe8,
	0x1f, //cc g1
	0xc3,
	0x04, //cc g2
	0x55,
	0x1f, //cc g3
	0xe8,
	0x1f, //cc b1
	0xc3,
	0x1f, //cc b2
	0x88,
	0x04, //cc b3
	0xb5,
};

char DSI0_TDMB_AUTO_MDNIE_1[] = {
	0xEB,
	0x01, //mdnie_en
	0x00, //data_width mask 00 000
	0x33, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
	0x0f, //sharpen cc gamma 00 0 0
};

char DSI0_TDMB_AUTO_MDNIE_2[] = {
	0xEC,
	0x00, //roi ctrl
	0x00, //roi0 x start
	0x00,
	0x00, //roi0 x end
	0x00,
	0x00, //roi0 y start
	0x00,
	0x00, //roi0 y end
	0x00,
	0x00, //roi1 x strat
	0x00,
	0x00, //roi1 x end
	0x00,
	0x00, //roi1 y start
	0x00,
	0x00, //roi1 y end
	0x00,
	0x00, //scr Cr Yb
	0xff, //scr Rr Bb
	0xff, //scr Cg Yg
	0x30, //scr Rg Bg
	0xff, //scr Cb Yr
	0x30, //scr Rb Br
	0xff, //scr Mr Mb
	0x00, //scr Gr Gb
	0x00, //scr Mg Mg
	0xff, //scr Gg Gg
	0xff, //scr Mb Mr
	0x00, //scr Gb Gr
	0xff, //scr Yr Cb
	0x00, //scr Br Rb
	0xff, //scr Yg Cg
	0x00, //scr Bg Rg
	0x30, //scr Yb Cr
	0xff, //scr Bb Rr
	0xff, //scr Wr Wb
	0x00, //scr Kr Kb
	0xff, //scr Wg Wg
	0x00, //scr Kg Kg
	0xff, //scr Wb Wr
	0x00, //scr Kb Kr
	0x00, //curve 1 b
	0x1c, //curve 1 a
	0x00, //curve 2 b
	0x1c, //curve 2 a
	0x00, //curve 3 b
	0x1c, //curve 3 a
	0x00, //curve 4 b
	0x1c, //curve 4 a
	0x00, //curve 5 b
	0x1c, //curve 5 a
	0x00, //curve 6 b
	0x1c, //curve 6 a
	0x00, //curve 7 b
	0x1c, //curve 7 a
	0x00, //curve 8 b
	0x1c, //curve 8 a
	0x00, //curve 9 b
	0x1c, //curve 9 a
	0x00, //curve10 b
	0x1c, //curve10 a
	0x00, //curve11 b
	0x1c, //curve11 a
	0x00, //curve12 b
	0x1c, //curve12 a
	0x00, //curve13 b
	0x1c, //curve13 a
	0x0d, //curve14 b
	0xa4, //curve14 a
	0x0d, //curve15 b
	0xa4, //curve15 a
	0x0d, //curve16 b
	0xa4, //curve16 a
	0x0d, //curve17 b
	0xa4, //curve17 a
	0x0d, //curve18 b
	0xa4, //curve18 a
	0x0d, //curve19 b
	0xa4, //curve19 a
	0x0d, //curve20 b
	0xa4, //curve20 a
	0x0d, //curve21 b
	0xa4, //curve21 a
	0x25, //curve22 b
	0x1c, //curve22 a
	0x4a, //curve23 b
	0x17, //curve23 a
	0x00, //curve24 b
	0xFF, //curve24 a
	0x04, //cc r1 0.3
	0xd7,
	0x1f, //cc r2
	0x4c,
	0x1f, //cc r3
	0xdd,
	0x1f, //cc g1
	0xa4,
	0x04, //cc g2
	0x7f,
	0x1f, //cc g3
	0xdd,
	0x1f, //cc b1
	0xa4,
	0x1f, //cc b2
	0x4c,
	0x05, //cc b3
	0x10,
};

static struct dsi_cmd_desc DSI0_BYPASS_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_1)}, DSI0_BYPASS_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_2)}, DSI0_BYPASS_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_NEGATIVE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_1)}, DSI0_NEGATIVE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_2)}, DSI0_NEGATIVE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_COLOR_BLIND_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_1)}, DSI0_COLOR_BLIND_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_2)}, DSI0_COLOR_BLIND_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HBM_CE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_1)}, DSI0_HBM_CE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_2)}, DSI0_HBM_CE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HBM_CE_TEXT_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HBM_CE_TEXT_MDNIE_1)}, DSI0_HBM_CE_TEXT_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HBM_CE_TEXT_MDNIE_2)}, DSI0_HBM_CE_TEXT_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_RGB_SENSOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_RGB_SENSOR_MDNIE_1)}, DSI0_RGB_SENSOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_RGB_SENSOR_MDNIE_2)}, DSI0_RGB_SENSOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CURTAIN[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CURTAIN_1)}, DSI0_CURTAIN_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CURTAIN_2)}, DSI0_CURTAIN_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GRAYSCALE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_1)}, DSI0_GRAYSCALE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_2)}, DSI0_GRAYSCALE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GRAYSCALE_NEGATIVE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_1)}, DSI0_GRAYSCALE_NEGATIVE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_2)}, DSI0_GRAYSCALE_NEGATIVE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

///////////////////////////////////////////////////////////////////////////////////

static struct dsi_cmd_desc DSI0_UI_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_1)}, DSI0_UI_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_2)}, DSI0_UI_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_STANDARD_MDNIE_1)}, DSI0_UI_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_STANDARD_MDNIE_2)}, DSI0_UI_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_NATURAL_MDNIE_1)}, DSI0_UI_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_NATURAL_MDNIE_2)}, DSI0_UI_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_MOVIE_MDNIE_1)}, DSI0_UI_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_MOVIE_MDNIE_2)}, DSI0_UI_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_1)}, DSI0_UI_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_2)}, DSI0_UI_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_OUTDOOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_OUTDOOR_MDNIE_1)}, DSI0_VIDEO_OUTDOOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_OUTDOOR_MDNIE_2)}, DSI0_VIDEO_OUTDOOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_1)}, DSI0_VIDEO_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_2)}, DSI0_VIDEO_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_STANDARD_MDNIE_1)}, DSI0_VIDEO_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_STANDARD_MDNIE_2)}, DSI0_VIDEO_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_NATURAL_MDNIE_1)}, DSI0_VIDEO_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_NATURAL_MDNIE_2)}, DSI0_VIDEO_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_MOVIE_MDNIE_1)}, DSI0_VIDEO_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_MOVIE_MDNIE_2)}, DSI0_VIDEO_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_1)}, DSI0_VIDEO_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_2)}, DSI0_VIDEO_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_WARM_OUTDOOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_WARM_OUTDOOR_MDNIE_1)}, DSI0_VIDEO_WARM_OUTDOOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_WARM_OUTDOOR_MDNIE_2)}, DSI0_VIDEO_WARM_OUTDOOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_WARM_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_WARM_MDNIE_1)}, DSI0_VIDEO_WARM_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_WARM_MDNIE_2)}, DSI0_VIDEO_WARM_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_COLD_OUTDOOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_COLD_OUTDOOR_MDNIE_1)}, DSI0_VIDEO_COLD_OUTDOOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_COLD_OUTDOOR_MDNIE_2)}, DSI0_VIDEO_COLD_OUTDOOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_COLD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_COLD_MDNIE_1)}, DSI0_VIDEO_COLD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_COLD_MDNIE_2)}, DSI0_VIDEO_COLD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_OUTDOOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_OUTDOOR_MDNIE_1)}, DSI0_CAMERA_OUTDOOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_OUTDOOR_MDNIE_2)}, DSI0_CAMERA_OUTDOOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_MDNIE_1)}, DSI0_CAMERA_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_MDNIE_2)}, DSI0_CAMERA_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_1)}, DSI0_CAMERA_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_2)}, DSI0_CAMERA_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_1)}, DSI0_GALLERY_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_2)}, DSI0_GALLERY_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_STANDARD_MDNIE_1)}, DSI0_GALLERY_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_STANDARD_MDNIE_2)}, DSI0_GALLERY_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_NATURAL_MDNIE_1)}, DSI0_GALLERY_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_NATURAL_MDNIE_2)}, DSI0_GALLERY_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_MOVIE_MDNIE_1)}, DSI0_GALLERY_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_MOVIE_MDNIE_2)}, DSI0_GALLERY_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_1)}, DSI0_GALLERY_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_2)}, DSI0_GALLERY_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VT_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_DYNAMIC_MDNIE_1)}, DSI0_VT_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_DYNAMIC_MDNIE_2)}, DSI0_VT_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VT_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_STANDARD_MDNIE_1)}, DSI0_VT_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_STANDARD_MDNIE_2)}, DSI0_VT_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VT_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_NATURAL_MDNIE_1)}, DSI0_VT_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_NATURAL_MDNIE_2)}, DSI0_VT_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VT_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_MOVIE_MDNIE_1)}, DSI0_VT_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_MOVIE_MDNIE_2)}, DSI0_VT_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VT_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_AUTO_MDNIE_1)}, DSI0_VT_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_AUTO_MDNIE_2)}, DSI0_VT_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_1)}, DSI0_BROWSER_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_2)}, DSI0_BROWSER_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_STANDARD_MDNIE_1)}, DSI0_BROWSER_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_STANDARD_MDNIE_2)}, DSI0_BROWSER_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_NATURAL_MDNIE_1)}, DSI0_BROWSER_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_NATURAL_MDNIE_2)}, DSI0_BROWSER_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_MOVIE_MDNIE_1)}, DSI0_BROWSER_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_MOVIE_MDNIE_2)}, DSI0_BROWSER_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_1)}, DSI0_BROWSER_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_2)}, DSI0_BROWSER_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_1)}, DSI0_EBOOK_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_2)}, DSI0_EBOOK_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_STANDARD_MDNIE_1)}, DSI0_EBOOK_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_STANDARD_MDNIE_2)}, DSI0_EBOOK_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_NATURAL_MDNIE_1)}, DSI0_EBOOK_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_NATURAL_MDNIE_2)}, DSI0_EBOOK_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_MOVIE_MDNIE_1)}, DSI0_EBOOK_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_MOVIE_MDNIE_2)}, DSI0_EBOOK_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_1)}, DSI0_EBOOK_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_2)}, DSI0_EBOOK_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EMAIL_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_1)}, DSI0_EMAIL_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_2)}, DSI0_EMAIL_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_1)}, DSI0_TDMB_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_2)}, DSI0_TDMB_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_STANDARD_MDNIE_1)}, DSI0_TDMB_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_STANDARD_MDNIE_2)}, DSI0_TDMB_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_NATURAL_MDNIE_1)}, DSI0_TDMB_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_NATURAL_MDNIE_2)}, DSI0_TDMB_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_MOVIE_MDNIE_1)}, DSI0_TDMB_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_MOVIE_MDNIE_2)}, DSI0_TDMB_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_1)}, DSI0_TDMB_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_2)}, DSI0_TDMB_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc *mdnie_tune_value_dsi0[MAX_APP_MODE][MAX_MODE][MAX_OUTDOOR_MODE] = {
 		/*
			DYNAMIC_MODE
			STANDARD_MODE
			NATURAL_MODE
			MOVIE_MODE
			AUTO_MODE
		*/
		// UI_APP
		{
			{DSI0_UI_DYNAMIC_MDNIE,	NULL},
			{DSI0_UI_STANDARD_MDNIE,	NULL},
			{DSI0_UI_NATURAL_MDNIE,	NULL},
			{DSI0_UI_MOVIE_MDNIE,	NULL},
			{DSI0_UI_AUTO_MDNIE,	NULL},
		},
		// VIDEO_APP
		{
			{DSI0_VIDEO_DYNAMIC_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
			{DSI0_VIDEO_STANDARD_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
			{DSI0_VIDEO_NATURAL_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
			{DSI0_VIDEO_MOVIE_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
			{DSI0_VIDEO_AUTO_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
		},
		// VIDEO_WARM_APP
		{
			{DSI0_VIDEO_WARM_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
			{DSI0_VIDEO_WARM_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
			{DSI0_VIDEO_WARM_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
			{DSI0_VIDEO_WARM_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
			{DSI0_VIDEO_WARM_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
		},
		// VIDEO_COLD_APP
		{
			{DSI0_VIDEO_COLD_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
			{DSI0_VIDEO_COLD_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
			{DSI0_VIDEO_COLD_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
			{DSI0_VIDEO_COLD_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
			{DSI0_VIDEO_COLD_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
		},
		// CAMERA_APP
		{
			{DSI0_CAMERA_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
			{DSI0_CAMERA_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
			{DSI0_CAMERA_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
			{DSI0_CAMERA_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
			{DSI0_CAMERA_AUTO_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
		},
		// NAVI_APP
		{
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
			{DSI0_GALLERY_MOVIE_MDNIE,	NULL},
			{DSI0_GALLERY_AUTO_MDNIE,	NULL},
		},
		// VT_APP
		{
			{DSI0_VT_DYNAMIC_MDNIE,	NULL},
			{DSI0_VT_STANDARD_MDNIE,	NULL},
			{DSI0_VT_NATURAL_MDNIE,	NULL},
			{DSI0_VT_MOVIE_MDNIE,	NULL},
			{DSI0_VT_AUTO_MDNIE,	NULL},
		},
		// BROWSER_APP
		{
			{DSI0_BROWSER_DYNAMIC_MDNIE,	NULL},
			{DSI0_BROWSER_STANDARD_MDNIE,	NULL},
			{DSI0_BROWSER_NATURAL_MDNIE,	NULL},
			{DSI0_BROWSER_MOVIE_MDNIE,	NULL},
			{DSI0_BROWSER_AUTO_MDNIE,	NULL},
		},
		// eBOOK_APP
		{
			{DSI0_EBOOK_DYNAMIC_MDNIE,	NULL},
			{DSI0_EBOOK_STANDARD_MDNIE,NULL},
			{DSI0_EBOOK_NATURAL_MDNIE,	NULL},
			{DSI0_EBOOK_MOVIE_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// EMAIL_APP
		{
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
		},
		// TDMB_APP
		{
			{DSI0_TDMB_DYNAMIC_MDNIE,	NULL},
			{DSI0_TDMB_STANDARD_MDNIE,	NULL},
			{DSI0_TDMB_NATURAL_MDNIE,	NULL},
			{DSI0_TDMB_MOVIE_MDNIE, NULL},
			{DSI0_TDMB_AUTO_MDNIE,	NULL},
		},
};

#define DSI0_RGB_SENSOR_MDNIE_1_SIZE ARRAY_SIZE(DSI0_RGB_SENSOR_MDNIE_1)
#define DSI0_RGB_SENSOR_MDNIE_2_SIZE ARRAY_SIZE(DSI0_RGB_SENSOR_MDNIE_2)
#endif /*_DSI_TCON_MDNIE_LITE_DATA_FHD_S6E3FA2_H_*/
