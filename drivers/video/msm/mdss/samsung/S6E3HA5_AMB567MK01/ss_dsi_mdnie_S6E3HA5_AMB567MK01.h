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
#ifndef _SS_DSI_MDNIE_S6E3HA5_AMB567MK01_H_
#define _SS_DSI_MDNIE_S6E3HA5_AMB567MK01_H_

#include "../ss_dsi_mdnie_lite_common.h"

#define MDNIE_COLOR_BLINDE_CMD_OFFSET 32

#define ADDRESS_SCR_WHITE_RED   0x32
#define ADDRESS_SCR_WHITE_GREEN 0x34
#define ADDRESS_SCR_WHITE_BLUE  0x36

#define MDNIE_STEP1_INDEX 1
#define MDNIE_STEP2_INDEX 2
#define MDNIE_STEP3_INDEX 3

#define MDNIE_TRANS_DIMMING_DATA_INDEX	13

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
	0xf6, 0xfa, 0xff,
	0xf4, 0xf8, 0xff,
	0xe9, 0xf2, 0xff,
	0xe2, 0xef, 0xff,
	0xd4, 0xe8, 0xff,
};

static char adjust_ldu_data_2[] = {
	0xff, 0xfa, 0xf1,
	0xff, 0xfd, 0xf8,
	0xff, 0xfd, 0xfa,
	0xfa, 0xfd, 0xff,
	0xf5, 0xfb, 0xff,
	0xe5, 0xf3, 0xff,
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
	0x00, 0xff, 0xf8, 0x00, 0xeb, 0x00, 0xff, 0x00, 0x00, 0xf8, 0xeb, 0x00, 0xff, 0x00, 0xf8, 0x00, 0x00, 0xeb, 0xff, 0x00, 0xf8, 0x00, 0xeb, 0x00, /* 6100K */
	0x00, 0xff, 0xf5, 0x00, 0xe2, 0x00, 0xff, 0x00, 0x00, 0xf5, 0xe2, 0x00, 0xff, 0x00, 0xf5, 0x00, 0x00, 0xe2, 0xff, 0x00, 0xf5, 0x00, 0xe2, 0x00, /* 5700K */
	0x00, 0xff, 0xf2, 0x00, 0xda, 0x00, 0xff, 0x00, 0x00, 0xf2, 0xda, 0x00, 0xff, 0x00, 0xf2, 0x00, 0x00, 0xda, 0xff, 0x00, 0xf2, 0x00, 0xda, 0x00, /* 5300K */
	0x00, 0xff, 0xee, 0x00, 0xd0, 0x00, 0xff, 0x00, 0x00, 0xee, 0xd0, 0x00, 0xff, 0x00, 0xee, 0x00, 0x00, 0xd0, 0xff, 0x00, 0xee, 0x00, 0xd0, 0x00, /* 4900K */
	0x00, 0xff, 0xe9, 0x00, 0xc4, 0x00, 0xff, 0x00, 0x00, 0xe9, 0xc4, 0x00, 0xff, 0x00, 0xe9, 0x00, 0x00, 0xc4, 0xff, 0x00, 0xe9, 0x00, 0xc4, 0x00, /* 4500K */
	0x00, 0xff, 0xe3, 0x00, 0xb4, 0x00, 0xff, 0x00, 0x00, 0xe3, 0xb4, 0x00, 0xff, 0x00, 0xe3, 0x00, 0x00, 0xb4, 0xff, 0x00, 0xe3, 0x00, 0xb4, 0x00, /* 4100K */
	0x00, 0xff, 0xdd, 0x00, 0xa4, 0x00, 0xff, 0x00, 0x00, 0xdd, 0xa4, 0x00, 0xff, 0x00, 0xdd, 0x00, 0x00, 0xa4, 0xff, 0x00, 0xdd, 0x00, 0xa4, 0x00, /* 3700K */
	0x00, 0xff, 0xd6, 0x00, 0x91, 0x00, 0xff, 0x00, 0x00, 0xd6, 0x91, 0x00, 0xff, 0x00, 0xd6, 0x00, 0x00, 0x91, 0xff, 0x00, 0xd6, 0x00, 0x91, 0x00, /* 3300K */
	0x00, 0xff, 0xcb, 0x00, 0x7b, 0x00, 0xff, 0x00, 0x00, 0xcb, 0x7b, 0x00, 0xff, 0x00, 0xcb, 0x00, 0x00, 0x7b, 0xff, 0x00, 0xcb, 0x00, 0x7b, 0x00, /* 2900K */
	0x00, 0xff, 0xbe, 0x00, 0x68, 0x00, 0xff, 0x00, 0x00, 0xbe, 0x68, 0x00, 0xff, 0x00, 0xbe, 0x00, 0x00, 0x68, 0xff, 0x00, 0xbe, 0x00, 0x68, 0x00, /* 2500K */
};

static char DSI0_BYPASS_MDNIE_1[] = {
	//start
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_BYPASS_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_BYPASS_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x08, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_NEGATIVE_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_NEGATIVE_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_NEGATIVE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_GRAYSCALE_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_GRAYSCALE_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_GRAYSCALE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_COLOR_BLIND_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_COLOR_BLIND_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_COLOR_BLIND_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_NIGHT_MODE_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_NIGHT_MODE_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_NIGHT_MODE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_HBM_CE_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
	0x28, //ascr_skin_Rg
	0x33, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf0, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xd8, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xd9, //ascr_skin_Mb
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
};

static char DSI0_HBM_CE_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_HBM_CE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3f, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_RGB_SENSOR_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_RGB_SENSOR_MDNIE_2[] ={
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_RGB_SENSOR_MDNIE_3[] ={
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_SCREEN_CURTAIN_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_SCREEN_CURTAIN_MDNIE_2[] ={
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_SCREEN_CURTAIN_MDNIE_3[] ={
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static unsigned char DSI0_HDR_VIDEO_1_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
	0x00, //ascr_skin_Rg
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
	0xf0, //ascr_Yg
	0x00, //ascr_Bg
	0x50, //ascr_Yb
	0xe0, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static unsigned char DSI0_HDR_VIDEO_1_MDNIE_2[] = {
	0xDE,
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
	0x01, //cs_gain 10
	0xd0,
	0x00, //curve_0
	0x0d, //curve_1
	0x19, //curve_2
	0x24, //curve_3
	0x35, //curve_4
	0x4e, //curve_5
	0x6d, //curve_6
	0x8d, //curve_7
	0xac, //curve_8
	0xc8, //curve_9
	0xde, //curve_10
	0xf0, //curve_11
	0xff, //curve_12
	0xff, //curve_13
	0xff, //curve_14
	0xff, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static unsigned char DSI0_HDR_VIDEO_1_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static unsigned char DSI0_HDR_VIDEO_2_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
	0x50, //ascr_skin_Rg
	0x50, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf0, //ascr_skin_Yg
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
	0xf8, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
};

static unsigned char DSI0_HDR_VIDEO_2_MDNIE_2[] = {
	0xDE,
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
	0x01, //cs_gain 10
	0xe0,
	0x00, //curve_0
	0x0d, //curve_1
	0x19, //curve_2
	0x24, //curve_3
	0x35, //curve_4
	0x4e, //curve_5
	0x6d, //curve_6
	0x8f, //curve_7
	0xb2, //curve_8
	0xd5, //curve_9
	0xf3, //curve_10
	0xff, //curve_11
	0xff, //curve_12
	0xff, //curve_13
	0xff, //curve_14
	0xff, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static unsigned char DSI0_HDR_VIDEO_2_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static unsigned char DSI0_HDR_VIDEO_3_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
	0x20, //ascr_skin_Rg
	0x60, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf0, //ascr_skin_Yg
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
	0xb0, //ascr_Yg
	0x20, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
};

static unsigned char DSI0_HDR_VIDEO_3_MDNIE_2[] = {
	0xDE,
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
	0x01, //cs_gain 10
	0xe0,
	0x00, //curve_0
	0x0c, //curve_1
	0x17, //curve_2
	0x21, //curve_3
	0x31, //curve_4
	0x49, //curve_5
	0x66, //curve_6
	0x86, //curve_7
	0xa7, //curve_8
	0xc9, //curve_9
	0xe6, //curve_10
	0xf4, //curve_11
	0xf9, //curve_12
	0xfc, //curve_13
	0xff, //curve_14
	0xff, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static unsigned char DSI0_HDR_VIDEO_3_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static unsigned char DSI0_VIDEO_ENHANCER_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
	0x48, //ascr_skin_Rg
	0x58, //ascr_skin_Rb
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
};

static unsigned char DSI0_VIDEO_ENHANCER_MDNIE_2[] = {
	0xDE,
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
	0x10, //cadrx_gain
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
	0xa8, //curve_10
	0xbb, //curve_11
	0xcb, //curve_12
	0xdb, //curve_13
	0xeb, //curve_14
	0xf8, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static unsigned char DSI0_VIDEO_ENHANCER_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3f, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static unsigned char DSI0_VIDEO_ENHANCER_THIRD_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
	0x48, //ascr_skin_Rg
	0x58, //ascr_skin_Rb
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
};

static unsigned char DSI0_VIDEO_ENHANCER_THIRD_MDNIE_2[] = {
	0xDE,
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
	0x10, //cadrx_gain
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
	0xa8, //curve_10
	0xbb, //curve_11
	0xcb, //curve_12
	0xdb, //curve_13
	0xeb, //curve_14
	0xf8, //curve_15
	0x01, //curve_16
	0x00,
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static unsigned char DSI0_VIDEO_ENHANCER_THIRD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3f, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_UI_DYNAMIC_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xdd, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xee, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xee, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xe6, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5e, //ascr_Cr
	0xdd, //ascr_Rr
	0xf6, //ascr_Cg
	0x00, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xee, //ascr_Mr
	0x4e, //ascr_Gr
	0x00, //ascr_Mg
	0xe7, //ascr_Gg
	0xe6, //ascr_Mb
	0x00, //ascr_Gb
	0xee, //ascr_Yr
	0x34, //ascr_Br
	0xeb, //ascr_Yg
	0x26, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_UI_DYNAMIC_MDNIE_2[] ={
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_UI_DYNAMIC_MDNIE_3[] ={
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_UI_STANDARD_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xef, //ascr_skin_Rr
	0x2e, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xef, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x3a, //ascr_skin_Mg
	0xe8, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xef, //ascr_Rr
	0xee, //ascr_Cg
	0x2e, //ascr_Rg
	0xe6, //ascr_Cb
	0x2e, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x3a, //ascr_Mg
	0xe1, //ascr_Gg
	0xe8, //ascr_Mb
	0x2d, //ascr_Gb
	0xef, //ascr_Yr
	0x31, //ascr_Br
	0xeb, //ascr_Yg
	0x24, //ascr_Bg
	0x3f, //ascr_Yb
	0xde, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_UI_STANDARD_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_UI_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_UI_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xd5, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xec, //ascr_skin_Yg
	0x52, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x34, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x91, //ascr_Cr
	0xd5, //ascr_Rr
	0xf2, //ascr_Cg
	0x26, //ascr_Rg
	0xe8, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x87, //ascr_Gr
	0x34, //ascr_Mg
	0xe6, //ascr_Gg
	0xe1, //ascr_Mb
	0x4a, //ascr_Gb
	0xf0, //ascr_Yr
	0x33, //ascr_Br
	0xec, //ascr_Yg
	0x22, //ascr_Bg
	0x52, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_UI_NATURAL_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_UI_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_UI_MOVIE_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_UI_MOVIE_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_UI_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_UI_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
	0x28, //ascr_skin_Rg
	0x33, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf0, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xd8, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xd9, //ascr_skin_Mb
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
};

static char DSI0_UI_AUTO_MDNIE_2[] = {
	0xDE,
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
	0x07, //nr fa de cs gamma 0 0000
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_UI_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_VIDEO_OUTDOOR_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_VIDEO_OUTDOOR_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VIDEO_OUTDOOR_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xdd, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xee, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xee, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xe6, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5e, //ascr_Cr
	0xdd, //ascr_Rr
	0xf6, //ascr_Cg
	0x00, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xee, //ascr_Mr
	0x4e, //ascr_Gr
	0x00, //ascr_Mg
	0xe7, //ascr_Gg
	0xe6, //ascr_Mb
	0x00, //ascr_Gb
	0xee, //ascr_Yr
	0x34, //ascr_Br
	0xeb, //ascr_Yg
	0x26, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_VIDEO_STANDARD_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xef, //ascr_skin_Rr
	0x2e, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xef, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x3a, //ascr_skin_Mg
	0xe8, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xef, //ascr_Rr
	0xee, //ascr_Cg
	0x2e, //ascr_Rg
	0xe6, //ascr_Cb
	0x2e, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x3a, //ascr_Mg
	0xe1, //ascr_Gg
	0xe8, //ascr_Mb
	0x2d, //ascr_Gb
	0xef, //ascr_Yr
	0x31, //ascr_Br
	0xeb, //ascr_Yg
	0x24, //ascr_Bg
	0x3f, //ascr_Yb
	0xde, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_VIDEO_STANDARD_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VIDEO_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_VIDEO_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xd5, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xec, //ascr_skin_Yg
	0x52, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x34, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x91, //ascr_Cr
	0xd5, //ascr_Rr
	0xf2, //ascr_Cg
	0x26, //ascr_Rg
	0xe8, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x87, //ascr_Gr
	0x34, //ascr_Mg
	0xe6, //ascr_Gg
	0xe1, //ascr_Mb
	0x4a, //ascr_Gb
	0xf0, //ascr_Yr
	0x33, //ascr_Br
	0xec, //ascr_Yg
	0x22, //ascr_Bg
	0x52, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_VIDEO_NATURAL_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VIDEO_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_VIDEO_MOVIE_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_VIDEO_MOVIE_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VIDEO_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_VIDEO_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
	0x60, //ascr_skin_Rg
	0x70, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf4, //ascr_skin_Wg
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
};

static char DSI0_VIDEO_AUTO_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VIDEO_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_VIDEO_WARM_OUTDOOR_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_VIDEO_WARM_OUTDOOR_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VIDEO_WARM_OUTDOOR_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_VIDEO_WARM_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_VIDEO_WARM_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VIDEO_WARM_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_VIDEO_COLD_OUTDOOR_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_VIDEO_COLD_OUTDOOR_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VIDEO_COLD_OUTDOOR_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_VIDEO_COLD_MDNIE_1[] ={
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_VIDEO_COLD_MDNIE_2[] ={
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VIDEO_COLD_MDNIE_3[] ={
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_CAMERA_OUTDOOR_MDNIE_1[] ={
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_CAMERA_OUTDOOR_MDNIE_2[] ={
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_CAMERA_OUTDOOR_MDNIE_3[] ={
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_CAMERA_MDNIE_1[] ={
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_CAMERA_MDNIE_2[] ={
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_CAMERA_MDNIE_3[] ={
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_1[] ={
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xdd, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xee, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xee, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xe6, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5e, //ascr_Cr
	0xdd, //ascr_Rr
	0xf6, //ascr_Cg
	0x00, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xee, //ascr_Mr
	0x4e, //ascr_Gr
	0x00, //ascr_Mg
	0xe7, //ascr_Gg
	0xe6, //ascr_Mb
	0x00, //ascr_Gb
	0xee, //ascr_Yr
	0x34, //ascr_Br
	0xeb, //ascr_Yg
	0x26, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_CAMERA_STANDARD_MDNIE_1[] ={
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xef, //ascr_skin_Rr
	0x2e, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xef, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x3a, //ascr_skin_Mg
	0xe8, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xef, //ascr_Rr
	0xee, //ascr_Cg
	0x2e, //ascr_Rg
	0xe6, //ascr_Cb
	0x2e, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x3a, //ascr_Mg
	0xe1, //ascr_Gg
	0xe8, //ascr_Mb
	0x2d, //ascr_Gb
	0xef, //ascr_Yr
	0x31, //ascr_Br
	0xeb, //ascr_Yg
	0x24, //ascr_Bg
	0x3f, //ascr_Yb
	0xde, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_CAMERA_STANDARD_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_CAMERA_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_CAMERA_NATURAL_MDNIE_1[] ={
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xd5, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xec, //ascr_skin_Yg
	0x52, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x34, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x91, //ascr_Cr
	0xd5, //ascr_Rr
	0xf2, //ascr_Cg
	0x26, //ascr_Rg
	0xe8, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x87, //ascr_Gr
	0x34, //ascr_Mg
	0xe6, //ascr_Gg
	0xe1, //ascr_Mb
	0x4a, //ascr_Gb
	0xf0, //ascr_Yr
	0x33, //ascr_Br
	0xec, //ascr_Yg
	0x22, //ascr_Bg
	0x52, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_CAMERA_NATURAL_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_CAMERA_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_CAMERA_MOVIE_MDNIE_1[] ={
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_CAMERA_MOVIE_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_CAMERA_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_CAMERA_AUTO_MDNIE_1[] ={
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
	0x28, //ascr_skin_Rg
	0x33, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf0, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xd8, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xd9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xe0, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xf6, //ascr_Cb
	0x00, //ascr_Rb
	0xd8, //ascr_Mr
	0x3b, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xd9, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x14, //ascr_Br
	0xf9, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_CAMERA_AUTO_MDNIE_2[] = {
	0xDE,
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
	0x07, //nr fa de cs gamma 0 0000
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_CAMERA_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_1[] ={
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xdd, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xee, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xee, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xe6, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5e, //ascr_Cr
	0xdd, //ascr_Rr
	0xf6, //ascr_Cg
	0x00, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xee, //ascr_Mr
	0x4e, //ascr_Gr
	0x00, //ascr_Mg
	0xe7, //ascr_Gg
	0xe6, //ascr_Mb
	0x00, //ascr_Gb
	0xee, //ascr_Yr
	0x34, //ascr_Br
	0xeb, //ascr_Yg
	0x26, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_GALLERY_STANDARD_MDNIE_1[] ={
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xef, //ascr_skin_Rr
	0x2e, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xef, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x3a, //ascr_skin_Mg
	0xe8, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xef, //ascr_Rr
	0xee, //ascr_Cg
	0x2e, //ascr_Rg
	0xe6, //ascr_Cb
	0x2e, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x3a, //ascr_Mg
	0xe1, //ascr_Gg
	0xe8, //ascr_Mb
	0x2d, //ascr_Gb
	0xef, //ascr_Yr
	0x31, //ascr_Br
	0xeb, //ascr_Yg
	0x24, //ascr_Bg
	0x3f, //ascr_Yb
	0xde, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_GALLERY_STANDARD_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_GALLERY_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_GALLERY_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xd5, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xec, //ascr_skin_Yg
	0x52, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x34, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x91, //ascr_Cr
	0xd5, //ascr_Rr
	0xf2, //ascr_Cg
	0x26, //ascr_Rg
	0xe8, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x87, //ascr_Gr
	0x34, //ascr_Mg
	0xe6, //ascr_Gg
	0xe1, //ascr_Mb
	0x4a, //ascr_Gb
	0xf0, //ascr_Yr
	0x33, //ascr_Br
	0xec, //ascr_Yg
	0x22, //ascr_Bg
	0x52, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_GALLERY_NATURAL_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_GALLERY_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_GALLERY_MOVIE_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_GALLERY_MOVIE_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_GALLERY_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_GALLERY_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
	0x28, //ascr_skin_Rg
	0x33, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf0, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xd8, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xd9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xe0, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xf6, //ascr_Cb
	0x00, //ascr_Rb
	0xd8, //ascr_Mr
	0x3b, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xd9, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x14, //ascr_Br
	0xf9, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_GALLERY_AUTO_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_GALLERY_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_VT_DYNAMIC_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xdd, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xee, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xee, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xe6, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5e, //ascr_Cr
	0xdd, //ascr_Rr
	0xf6, //ascr_Cg
	0x00, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xee, //ascr_Mr
	0x4e, //ascr_Gr
	0x00, //ascr_Mg
	0xe7, //ascr_Gg
	0xe6, //ascr_Mb
	0x00, //ascr_Gb
	0xee, //ascr_Yr
	0x34, //ascr_Br
	0xeb, //ascr_Yg
	0x26, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_VT_DYNAMIC_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VT_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_VT_STANDARD_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xef, //ascr_skin_Rr
	0x2e, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xef, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x3a, //ascr_skin_Mg
	0xe8, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xef, //ascr_Rr
	0xee, //ascr_Cg
	0x2e, //ascr_Rg
	0xe6, //ascr_Cb
	0x2e, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x3a, //ascr_Mg
	0xe1, //ascr_Gg
	0xe8, //ascr_Mb
	0x2d, //ascr_Gb
	0xef, //ascr_Yr
	0x31, //ascr_Br
	0xeb, //ascr_Yg
	0x24, //ascr_Bg
	0x3f, //ascr_Yb
	0xde, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_VT_STANDARD_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VT_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_VT_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xd5, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xec, //ascr_skin_Yg
	0x52, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x34, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x91, //ascr_Cr
	0xd5, //ascr_Rr
	0xf2, //ascr_Cg
	0x26, //ascr_Rg
	0xe8, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x87, //ascr_Gr
	0x34, //ascr_Mg
	0xe6, //ascr_Gg
	0xe1, //ascr_Mb
	0x4a, //ascr_Gb
	0xf0, //ascr_Yr
	0x33, //ascr_Br
	0xec, //ascr_Yg
	0x22, //ascr_Bg
	0x52, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_VT_NATURAL_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VT_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_VT_MOVIE_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_VT_MOVIE_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VT_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_VT_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_VT_AUTO_MDNIE_2[] = {
	0xDE,
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
	0x07, //nr fa de cs gamma 0 0000
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_VT_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xdd, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xee, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xee, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xe6, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5e, //ascr_Cr
	0xdd, //ascr_Rr
	0xf6, //ascr_Cg
	0x00, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xee, //ascr_Mr
	0x4e, //ascr_Gr
	0x00, //ascr_Mg
	0xe7, //ascr_Gg
	0xe6, //ascr_Mb
	0x00, //ascr_Gb
	0xee, //ascr_Yr
	0x34, //ascr_Br
	0xeb, //ascr_Yg
	0x26, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_BROWSER_STANDARD_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xef, //ascr_skin_Rr
	0x2e, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xef, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x3a, //ascr_skin_Mg
	0xe8, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xef, //ascr_Rr
	0xee, //ascr_Cg
	0x2e, //ascr_Rg
	0xe6, //ascr_Cb
	0x2e, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x3a, //ascr_Mg
	0xe1, //ascr_Gg
	0xe8, //ascr_Mb
	0x2d, //ascr_Gb
	0xef, //ascr_Yr
	0x31, //ascr_Br
	0xeb, //ascr_Yg
	0x24, //ascr_Bg
	0x3f, //ascr_Yb
	0xde, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_BROWSER_STANDARD_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_BROWSER_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_BROWSER_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xd5, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xec, //ascr_skin_Yg
	0x52, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x34, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x91, //ascr_Cr
	0xd5, //ascr_Rr
	0xf2, //ascr_Cg
	0x26, //ascr_Rg
	0xe8, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x87, //ascr_Gr
	0x34, //ascr_Mg
	0xe6, //ascr_Gg
	0xe1, //ascr_Mb
	0x4a, //ascr_Gb
	0xf0, //ascr_Yr
	0x33, //ascr_Br
	0xec, //ascr_Yg
	0x22, //ascr_Bg
	0x52, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_BROWSER_NATURAL_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_BROWSER_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_BROWSER_MOVIE_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_BROWSER_MOVIE_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_BROWSER_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_BROWSER_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
	0x34, //ascr_skin_Rg
	0x44, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xff, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xff, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf8, //ascr_skin_Wg
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
};

static char DSI0_BROWSER_AUTO_MDNIE_2[] = {
	0xDE,
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
	0x07, //nr fa de cs gamma 0 0000
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_BROWSER_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xdd, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xee, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xee, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xe6, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5e, //ascr_Cr
	0xdd, //ascr_Rr
	0xf6, //ascr_Cg
	0x00, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xee, //ascr_Mr
	0x4e, //ascr_Gr
	0x00, //ascr_Mg
	0xe7, //ascr_Gg
	0xe6, //ascr_Mb
	0x00, //ascr_Gb
	0xee, //ascr_Yr
	0x34, //ascr_Br
	0xeb, //ascr_Yg
	0x26, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_EBOOK_STANDARD_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xef, //ascr_skin_Rr
	0x2e, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xef, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x3a, //ascr_skin_Mg
	0xe8, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xef, //ascr_Rr
	0xee, //ascr_Cg
	0x2e, //ascr_Rg
	0xe6, //ascr_Cb
	0x2e, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x3a, //ascr_Mg
	0xe1, //ascr_Gg
	0xe8, //ascr_Mb
	0x2d, //ascr_Gb
	0xef, //ascr_Yr
	0x31, //ascr_Br
	0xeb, //ascr_Yg
	0x24, //ascr_Bg
	0x3f, //ascr_Yb
	0xde, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_EBOOK_STANDARD_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_EBOOK_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_EBOOK_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xd5, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xec, //ascr_skin_Yg
	0x52, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x34, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x91, //ascr_Cr
	0xd5, //ascr_Rr
	0xf2, //ascr_Cg
	0x26, //ascr_Rg
	0xe8, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x87, //ascr_Gr
	0x34, //ascr_Mg
	0xe6, //ascr_Gg
	0xe1, //ascr_Mb
	0x4a, //ascr_Gb
	0xf0, //ascr_Yr
	0x33, //ascr_Br
	0xec, //ascr_Yg
	0x22, //ascr_Bg
	0x52, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_EBOOK_NATURAL_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_EBOOK_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_EBOOK_MOVIE_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_EBOOK_MOVIE_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_EBOOK_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_EBOOK_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
	0xf9, //ascr_Wg
	0x00, //ascr_Kg
	0xe9, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_EBOOK_AUTO_MDNIE_2[] = {
	0xDE,
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
	0x07, //nr fa de cs gamma 0 0000
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_EBOOK_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_EMAIL_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
	0xfb, //ascr_Wg
	0x00, //ascr_Kg
	0xee, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_EMAIL_AUTO_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_EMAIL_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static unsigned char DSI0_GAME_LOW_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
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
};

static unsigned char DSI0_GAME_LOW_MDNIE_2[] = {
	0xDE,
	0x10, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x20, //lce_gain 000 0000
	0x18, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0x6e, //lce_illum_gain
	0x00, //lce_ref_offset 9
	0x60,
	0x00, //lce_ref_gain 9
	0x80,
	0x66, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x05, //lce_reduct_slope 0000
	0x46, //lce_black cc0 cc1 color_th 0 0 0 0000
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
	0x01, //nr fa de cs gamma 0 0000
	0x00, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x00, //de_maxplus 11
	0x00,
	0x00, //de_maxminus 11
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
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x04, //curve_1
	0x0a, //curve_2
	0x0f, //curve_3
	0x1d, //curve_4
	0x2b, //curve_5
	0x3a, //curve_6
	0x49, //curve_7
	0x58, //curve_8
	0x67, //curve_9
	0x76, //curve_10
	0x85, //curve_11
	0x94, //curve_12
	0xa3, //curve_13
	0xb2, //curve_14
	0xc1, //curve_15
	0x00, //curve_16
	0xd0,
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static unsigned char DSI0_GAME_LOW_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x0f, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
};

static unsigned char DSI0_GAME_MID_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
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
};

static unsigned char DSI0_GAME_MID_MDNIE_2[] = {
	0xDE,
	0x10, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x04, //lce_gain 000 0000
	0x28, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0x05, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x50,
	0x01, //lce_ref_gain 9
	0xd0,
	0x66, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x05, //lce_reduct_slope 0000
	0x46, //lce_black cc0 cc1 color_th 0 0 0 0000
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
	0x01, //nr fa de cs gamma 0 0000
	0x00, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x00, //de_maxplus 11
	0x00,
	0x00, //de_maxminus 11
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
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x04, //curve_1
	0x0a, //curve_2
	0x0f, //curve_3
	0x1d, //curve_4
	0x2b, //curve_5
	0x3a, //curve_6
	0x49, //curve_7
	0x58, //curve_8
	0x67, //curve_9
	0x76, //curve_10
	0x85, //curve_11
	0x94, //curve_12
	0xa3, //curve_13
	0xb2, //curve_14
	0xc1, //curve_15
	0x00, //curve_16
	0xd0,
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static unsigned char DSI0_GAME_MID_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x0f, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
};

static unsigned char DSI0_GAME_HIGH_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
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
};

static unsigned char DSI0_GAME_HIGH_MDNIE_2[] = {
	0xDE,
	0x10, //slce_on cadrx_en 0000 0000
	0x00, //glare_on
	0x04, //lce_gain 000 0000
	0x28, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0x05, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x60,
	0x01, //lce_ref_gain 9
	0xf0,
	0x66, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x05, //lce_reduct_slope 0000
	0x46, //lce_black cc0 cc1 color_th 0 0 0 0000
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
	0x01, //nr fa de cs gamma 0 0000
	0x00, //nr_mask_th
	0x00, //de_gain 10
	0x00,
	0x00, //de_maxplus 11
	0x00,
	0x00, //de_maxminus 11
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
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_0
	0x06, //curve_1
	0x14, //curve_2
	0x1d, //curve_3
	0x26, //curve_4
	0x32, //curve_5
	0x42, //curve_6
	0x52, //curve_7
	0x62, //curve_8
	0x72, //curve_9
	0x82, //curve_10
	0x92, //curve_11
	0xa2, //curve_12
	0xb2, //curve_13
	0xc2, //curve_14
	0xd2, //curve_15
	0x00, //curve_16
	0xe2,
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static unsigned char DSI0_GAME_HIGH_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x0f, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
};

static char DSI0_TDMB_STANDARD_MDNIE_1[] ={
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xef, //ascr_skin_Rr
	0x2e, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xef, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x3a, //ascr_skin_Mg
	0xe8, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xef, //ascr_Rr
	0xee, //ascr_Cg
	0x2e, //ascr_Rg
	0xe6, //ascr_Cb
	0x2e, //ascr_Rb
	0xfe, //ascr_Mr
	0x00, //ascr_Gr
	0x3a, //ascr_Mg
	0xe1, //ascr_Gg
	0xe8, //ascr_Mb
	0x2d, //ascr_Gb
	0xef, //ascr_Yr
	0x31, //ascr_Br
	0xeb, //ascr_Yg
	0x24, //ascr_Bg
	0x3f, //ascr_Yb
	0xde, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_TDMB_STANDARD_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_TDMB_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_TDMB_NATURAL_MDNIE_1[] = {
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xd5, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf0, //ascr_skin_Yr
	0xec, //ascr_skin_Yg
	0x52, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x34, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x91, //ascr_Cr
	0xd5, //ascr_Rr
	0xf2, //ascr_Cg
	0x26, //ascr_Rg
	0xe8, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x87, //ascr_Gr
	0x34, //ascr_Mg
	0xe6, //ascr_Gg
	0xe1, //ascr_Mb
	0x4a, //ascr_Gb
	0xf0, //ascr_Yr
	0x33, //ascr_Br
	0xec, //ascr_Yg
	0x22, //ascr_Bg
	0x52, //ascr_Yb
	0xdb, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_TDMB_NATURAL_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_TDMB_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_TDMB_DYNAMIC_MDNIE_1[] ={
	//start
	0xDF,
	0x60, //linear_on ascr_skin_on strength 0 0 00000
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
	0xdd, //ascr_skin_Rr
	0x00, //ascr_skin_Rg
	0x00, //ascr_skin_Rb
	0xee, //ascr_skin_Yr
	0xeb, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xee, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xe6, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x5e, //ascr_Cr
	0xdd, //ascr_Rr
	0xf6, //ascr_Cg
	0x00, //ascr_Rg
	0xeb, //ascr_Cb
	0x00, //ascr_Rb
	0xee, //ascr_Mr
	0x4e, //ascr_Gr
	0x00, //ascr_Mg
	0xe7, //ascr_Gg
	0xe6, //ascr_Mb
	0x00, //ascr_Gb
	0xee, //ascr_Yr
	0x34, //ascr_Br
	0xeb, //ascr_Yg
	0x26, //ascr_Bg
	0x00, //ascr_Yb
	0xe2, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_TDMB_DYNAMIC_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_TDMB_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_TDMB_MOVIE_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_TDMB_MOVIE_MDNIE_2[] = {
	0xDE,
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
	0x07, //de_maxplus 11
	0xff,
	0x07, //de_maxminus 11
	0xff,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x0a,
	0x00, //fa_step_n 13
	0x32,
	0x01, //fa_max_de_gain 13
	0xf4,
	0x0b, //fa_pcl_ppi 14
	0x8a,
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_TDMB_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x38, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_TDMB_AUTO_MDNIE_1[] = {
	//start
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
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
	0x28, //ascr_skin_Rg
	0x33, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf0, //ascr_skin_Yg
	0x00, //ascr_skin_Yb
	0xd8, //ascr_skin_Mr
	0x00, //ascr_skin_Mg
	0xd9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xff, //ascr_skin_Wg
	0xff, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xe0, //ascr_Rr
	0xff, //ascr_Cg
	0x00, //ascr_Rg
	0xf6, //ascr_Cb
	0x00, //ascr_Rb
	0xd8, //ascr_Mr
	0x3b, //ascr_Gr
	0x00, //ascr_Mg
	0xff, //ascr_Gg
	0xd9, //ascr_Mb
	0x00, //ascr_Gb
	0xff, //ascr_Yr
	0x14, //ascr_Br
	0xf9, //ascr_Yg
	0x00, //ascr_Bg
	0x00, //ascr_Yb
	0xff, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xff, //ascr_Wg
	0x00, //ascr_Kg
	0xff, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_TDMB_AUTO_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_TDMB_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_HMT_COLOR_TEMP_3000K_MDNIE_1[] = {
	//start
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_HMT_COLOR_TEMP_3000K_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_HMT_COLOR_TEMP_3000K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x08, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_HMT_COLOR_TEMP_4000K_MDNIE_1[] = {
	//start
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_HMT_COLOR_TEMP_4000K_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_HMT_COLOR_TEMP_4000K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x08, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_HMT_COLOR_TEMP_5000K_MDNIE_1[] = {
	//start
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_HMT_COLOR_TEMP_5000K_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_HMT_COLOR_TEMP_5000K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x08, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_1[] = {
	//start
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
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
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x14, //trans_on trans_slope 0 0000
	0x01, //trans_interval
	//end
};

static char DSI0_HMT_COLOR_TEMP_7500K_MDNIE_1[] = {
	//start
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
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
};

static char DSI0_HMT_COLOR_TEMP_7500K_MDNIE_2[] = {
	0xDE,
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
	0x00, //curve_offset
	0x00,
	0x00, //curve_low_x
	0x00, //curve_low_y
};

static char DSI0_HMT_COLOR_TEMP_7500K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x08, //ascr algo aolce 00 00 00
	0x00, //ascr_roi algo_roi aolce_roi roi_en
	0x00, //roi_sx
	0x00, //roi_sx
	0x00, //roi_sy
	0x00, //roi_sy
	0x00, //roi_ex
	0x00, //roi_ex
	0x00, //roi_ey
	0x00, //roi_ey
	0x00, //trans_on trans_slope 0 0000
	0x00, //trans_interval
	//end
};

static struct dsi_cmd_desc DSI0_BYPASS_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_1)}, DSI0_BYPASS_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_2)}, DSI0_BYPASS_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_3)}, DSI0_BYPASS_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_NEGATIVE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_1)}, DSI0_NEGATIVE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_2)}, DSI0_NEGATIVE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_3)}, DSI0_NEGATIVE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GRAYSCALE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_1)}, DSI0_GRAYSCALE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_2)}, DSI0_GRAYSCALE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_3)}, DSI0_GRAYSCALE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GRAYSCALE_NEGATIVE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_1)}, DSI0_GRAYSCALE_NEGATIVE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_2)}, DSI0_GRAYSCALE_NEGATIVE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_3)}, DSI0_GRAYSCALE_NEGATIVE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_COLOR_BLIND_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_1)}, DSI0_COLOR_BLIND_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_2)}, DSI0_COLOR_BLIND_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_3)}, DSI0_COLOR_BLIND_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_NIGHT_MODE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_1)}, DSI0_NIGHT_MODE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_2)}, DSI0_NIGHT_MODE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_3)}, DSI0_NIGHT_MODE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HBM_CE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_1)}, DSI0_HBM_CE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_2)}, DSI0_HBM_CE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_3)}, DSI0_HBM_CE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_RGB_SENSOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_RGB_SENSOR_MDNIE_1)}, DSI0_RGB_SENSOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_RGB_SENSOR_MDNIE_2)}, DSI0_RGB_SENSOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_RGB_SENSOR_MDNIE_3)}, DSI0_RGB_SENSOR_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_SCREEN_CURTAIN_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_1)}, DSI0_SCREEN_CURTAIN_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_2)}, DSI0_SCREEN_CURTAIN_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_3)}, DSI0_SCREEN_CURTAIN_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_1_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_1)}, DSI0_HDR_VIDEO_1_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_2)}, DSI0_HDR_VIDEO_1_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_3)}, DSI0_HDR_VIDEO_1_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_2_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_1)}, DSI0_HDR_VIDEO_2_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_2)}, DSI0_HDR_VIDEO_2_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_3)}, DSI0_HDR_VIDEO_2_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_3_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_1)}, DSI0_HDR_VIDEO_3_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_2)}, DSI0_HDR_VIDEO_3_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_3)}, DSI0_HDR_VIDEO_3_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_ENHANCER_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_MDNIE_1)}, DSI0_VIDEO_ENHANCER_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_MDNIE_2)}, DSI0_VIDEO_ENHANCER_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_MDNIE_3)}, DSI0_VIDEO_ENHANCER_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_ENHANCER_THIRD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_THIRD_MDNIE_1)}, DSI0_VIDEO_ENHANCER_THIRD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_THIRD_MDNIE_2)}, DSI0_VIDEO_ENHANCER_THIRD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_ENHANCER_THIRD_MDNIE_3)}, DSI0_VIDEO_ENHANCER_THIRD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

///////////////////////////////////////////////////////////////////////////////////

static struct dsi_cmd_desc DSI0_UI_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_1)}, DSI0_UI_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_2)}, DSI0_UI_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_3)}, DSI0_UI_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_STANDARD_MDNIE_1)}, DSI0_UI_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_STANDARD_MDNIE_2)}, DSI0_UI_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_STANDARD_MDNIE_3)}, DSI0_UI_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_NATURAL_MDNIE_1)}, DSI0_UI_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_NATURAL_MDNIE_2)}, DSI0_UI_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_NATURAL_MDNIE_3)}, DSI0_UI_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_MOVIE_MDNIE_1)}, DSI0_UI_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_MOVIE_MDNIE_2)}, DSI0_UI_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_MOVIE_MDNIE_3)}, DSI0_UI_MOVIE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_UI_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_1)}, DSI0_UI_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_2)}, DSI0_UI_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_3)}, DSI0_UI_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_OUTDOOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_OUTDOOR_MDNIE_1)}, DSI0_VIDEO_OUTDOOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_OUTDOOR_MDNIE_2)}, DSI0_VIDEO_OUTDOOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_OUTDOOR_MDNIE_3)}, DSI0_VIDEO_OUTDOOR_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_1)}, DSI0_VIDEO_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_2)}, DSI0_VIDEO_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_3)}, DSI0_VIDEO_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_STANDARD_MDNIE_1)}, DSI0_VIDEO_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_STANDARD_MDNIE_2)}, DSI0_VIDEO_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_STANDARD_MDNIE_3)}, DSI0_VIDEO_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_NATURAL_MDNIE_1)}, DSI0_VIDEO_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_NATURAL_MDNIE_2)}, DSI0_VIDEO_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_NATURAL_MDNIE_3)}, DSI0_VIDEO_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_MOVIE_MDNIE_1)}, DSI0_VIDEO_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_MOVIE_MDNIE_2)}, DSI0_VIDEO_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_MOVIE_MDNIE_3)}, DSI0_VIDEO_MOVIE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_1)}, DSI0_VIDEO_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_2)}, DSI0_VIDEO_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_3)}, DSI0_VIDEO_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_WARM_OUTDOOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_WARM_OUTDOOR_MDNIE_1)}, DSI0_VIDEO_WARM_OUTDOOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_WARM_OUTDOOR_MDNIE_2)}, DSI0_VIDEO_WARM_OUTDOOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_WARM_OUTDOOR_MDNIE_3)}, DSI0_VIDEO_WARM_OUTDOOR_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_WARM_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_WARM_MDNIE_1)}, DSI0_VIDEO_WARM_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_WARM_MDNIE_2)}, DSI0_VIDEO_WARM_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_WARM_MDNIE_3)}, DSI0_VIDEO_WARM_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_COLD_OUTDOOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_COLD_OUTDOOR_MDNIE_1)}, DSI0_VIDEO_COLD_OUTDOOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_COLD_OUTDOOR_MDNIE_2)}, DSI0_VIDEO_COLD_OUTDOOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_COLD_OUTDOOR_MDNIE_3)}, DSI0_VIDEO_COLD_OUTDOOR_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VIDEO_COLD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_COLD_MDNIE_1)}, DSI0_VIDEO_COLD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_COLD_MDNIE_2)}, DSI0_VIDEO_COLD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VIDEO_COLD_MDNIE_3)}, DSI0_VIDEO_COLD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_OUTDOOR_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_OUTDOOR_MDNIE_1)}, DSI0_CAMERA_OUTDOOR_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_OUTDOOR_MDNIE_2)}, DSI0_CAMERA_OUTDOOR_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_OUTDOOR_MDNIE_3)}, DSI0_CAMERA_OUTDOOR_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_MDNIE_1)}, DSI0_CAMERA_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_MDNIE_2)}, DSI0_CAMERA_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_MDNIE_3)}, DSI0_CAMERA_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_1)}, DSI0_CAMERA_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_2)}, DSI0_CAMERA_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_3)}, DSI0_CAMERA_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_STANDARD_MDNIE_1)}, DSI0_CAMERA_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_STANDARD_MDNIE_2)}, DSI0_CAMERA_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_STANDARD_MDNIE_3)}, DSI0_CAMERA_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_NATURAL_MDNIE_1)}, DSI0_CAMERA_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_NATURAL_MDNIE_2)}, DSI0_CAMERA_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_NATURAL_MDNIE_3)}, DSI0_CAMERA_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_MOVIE_MDNIE_1)}, DSI0_CAMERA_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_MOVIE_MDNIE_2)}, DSI0_CAMERA_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_MOVIE_MDNIE_3)}, DSI0_CAMERA_MOVIE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_CAMERA_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_1)}, DSI0_CAMERA_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_2)}, DSI0_CAMERA_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_3)}, DSI0_CAMERA_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_1)}, DSI0_GALLERY_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_2)}, DSI0_GALLERY_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_3)}, DSI0_GALLERY_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_STANDARD_MDNIE_1)}, DSI0_GALLERY_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_STANDARD_MDNIE_2)}, DSI0_GALLERY_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_STANDARD_MDNIE_3)}, DSI0_GALLERY_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_NATURAL_MDNIE_1)}, DSI0_GALLERY_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_NATURAL_MDNIE_2)}, DSI0_GALLERY_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_NATURAL_MDNIE_3)}, DSI0_GALLERY_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_MOVIE_MDNIE_1)}, DSI0_GALLERY_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_MOVIE_MDNIE_2)}, DSI0_GALLERY_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_MOVIE_MDNIE_3)}, DSI0_GALLERY_MOVIE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GALLERY_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_1)}, DSI0_GALLERY_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_2)}, DSI0_GALLERY_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_3)}, DSI0_GALLERY_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VT_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_DYNAMIC_MDNIE_1)}, DSI0_VT_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_DYNAMIC_MDNIE_2)}, DSI0_VT_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_DYNAMIC_MDNIE_3)}, DSI0_VT_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VT_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_STANDARD_MDNIE_1)}, DSI0_VT_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_STANDARD_MDNIE_2)}, DSI0_VT_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_STANDARD_MDNIE_3)}, DSI0_VT_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VT_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_NATURAL_MDNIE_1)}, DSI0_VT_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_NATURAL_MDNIE_2)}, DSI0_VT_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_NATURAL_MDNIE_3)}, DSI0_VT_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VT_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_MOVIE_MDNIE_1)}, DSI0_VT_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_MOVIE_MDNIE_2)}, DSI0_VT_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_MOVIE_MDNIE_3)}, DSI0_VT_MOVIE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_VT_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_AUTO_MDNIE_1)}, DSI0_VT_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_AUTO_MDNIE_2)}, DSI0_VT_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_VT_AUTO_MDNIE_3)}, DSI0_VT_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_1)}, DSI0_BROWSER_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_2)}, DSI0_BROWSER_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_3)}, DSI0_BROWSER_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_STANDARD_MDNIE_1)}, DSI0_BROWSER_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_STANDARD_MDNIE_2)}, DSI0_BROWSER_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_STANDARD_MDNIE_3)}, DSI0_BROWSER_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_NATURAL_MDNIE_1)}, DSI0_BROWSER_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_NATURAL_MDNIE_2)}, DSI0_BROWSER_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_NATURAL_MDNIE_3)}, DSI0_BROWSER_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_MOVIE_MDNIE_1)}, DSI0_BROWSER_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_MOVIE_MDNIE_2)}, DSI0_BROWSER_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_MOVIE_MDNIE_3)}, DSI0_BROWSER_MOVIE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_BROWSER_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_1)}, DSI0_BROWSER_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_2)}, DSI0_BROWSER_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_3)}, DSI0_BROWSER_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_1)}, DSI0_EBOOK_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_2)}, DSI0_EBOOK_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_3)}, DSI0_EBOOK_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_STANDARD_MDNIE_1)}, DSI0_EBOOK_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_STANDARD_MDNIE_2)}, DSI0_EBOOK_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_STANDARD_MDNIE_3)}, DSI0_EBOOK_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_NATURAL_MDNIE_1)}, DSI0_EBOOK_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_NATURAL_MDNIE_2)}, DSI0_EBOOK_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_NATURAL_MDNIE_3)}, DSI0_EBOOK_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_MOVIE_MDNIE_1)}, DSI0_EBOOK_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_MOVIE_MDNIE_2)}, DSI0_EBOOK_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_MOVIE_MDNIE_3)}, DSI0_EBOOK_MOVIE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EBOOK_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_1)}, DSI0_EBOOK_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_2)}, DSI0_EBOOK_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_3)}, DSI0_EBOOK_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_EMAIL_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_1)}, DSI0_EMAIL_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_2)}, DSI0_EMAIL_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_3)}, DSI0_EMAIL_AUTO_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GAME_LOW_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GAME_LOW_MDNIE_1)}, DSI0_GAME_LOW_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GAME_LOW_MDNIE_2)}, DSI0_GAME_LOW_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GAME_LOW_MDNIE_3)}, DSI0_GAME_LOW_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GAME_MID_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GAME_MID_MDNIE_1)}, DSI0_GAME_MID_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GAME_MID_MDNIE_2)}, DSI0_GAME_MID_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GAME_MID_MDNIE_3)}, DSI0_GAME_MID_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_GAME_HIGH_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GAME_HIGH_MDNIE_1)}, DSI0_GAME_HIGH_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GAME_HIGH_MDNIE_2)}, DSI0_GAME_HIGH_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_GAME_HIGH_MDNIE_3)}, DSI0_GAME_HIGH_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_DYNAMIC_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_1)}, DSI0_TDMB_DYNAMIC_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_2)}, DSI0_TDMB_DYNAMIC_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_3)}, DSI0_TDMB_DYNAMIC_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_STANDARD_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_STANDARD_MDNIE_1)}, DSI0_TDMB_STANDARD_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_STANDARD_MDNIE_2)}, DSI0_TDMB_STANDARD_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_STANDARD_MDNIE_3)}, DSI0_TDMB_STANDARD_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_NATURAL_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_NATURAL_MDNIE_1)}, DSI0_TDMB_NATURAL_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_NATURAL_MDNIE_2)}, DSI0_TDMB_NATURAL_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_NATURAL_MDNIE_3)}, DSI0_TDMB_NATURAL_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_MOVIE_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_MOVIE_MDNIE_1)}, DSI0_TDMB_MOVIE_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_MOVIE_MDNIE_2)}, DSI0_TDMB_MOVIE_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_MOVIE_MDNIE_3)}, DSI0_TDMB_MOVIE_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_TDMB_AUTO_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_1)}, DSI0_TDMB_AUTO_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_2)}, DSI0_TDMB_AUTO_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_3)}, DSI0_TDMB_AUTO_MDNIE_3},
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
			{DSI0_UI_MOVIE_MDNIE,	NULL},
			{DSI0_UI_AUTO_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// VIDEO_APP
		{
			{DSI0_VIDEO_DYNAMIC_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
			{DSI0_VIDEO_STANDARD_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
			{DSI0_VIDEO_NATURAL_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
			{DSI0_VIDEO_MOVIE_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
			{DSI0_VIDEO_AUTO_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
			{DSI0_EBOOK_AUTO_MDNIE,	DSI0_VIDEO_OUTDOOR_MDNIE},
		},
		// VIDEO_WARM_APP
		{
			{DSI0_VIDEO_WARM_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
			{DSI0_VIDEO_WARM_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
			{DSI0_VIDEO_WARM_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
			{DSI0_VIDEO_WARM_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
			{DSI0_VIDEO_WARM_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
			{DSI0_EBOOK_AUTO_MDNIE,	DSI0_VIDEO_WARM_OUTDOOR_MDNIE},
		},
		// VIDEO_COLD_APP
		{
			{DSI0_VIDEO_COLD_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
			{DSI0_VIDEO_COLD_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
			{DSI0_VIDEO_COLD_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
			{DSI0_VIDEO_COLD_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
			{DSI0_VIDEO_COLD_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
			{DSI0_EBOOK_AUTO_MDNIE,	DSI0_VIDEO_COLD_OUTDOOR_MDNIE},
		},
		// CAMERA_APP
		{
			{DSI0_CAMERA_DYNAMIC_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
			{DSI0_CAMERA_STANDARD_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
			{DSI0_CAMERA_NATURAL_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
			{DSI0_CAMERA_MOVIE_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
			{DSI0_CAMERA_AUTO_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
			{DSI0_EBOOK_AUTO_MDNIE,	DSI0_CAMERA_OUTDOOR_MDNIE},
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
			{DSI0_GALLERY_MOVIE_MDNIE,	NULL},
			{DSI0_GALLERY_AUTO_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// VT_APP
		{
			{DSI0_VT_DYNAMIC_MDNIE,	NULL},
			{DSI0_VT_STANDARD_MDNIE,	NULL},
			{DSI0_VT_NATURAL_MDNIE,	NULL},
			{DSI0_VT_MOVIE_MDNIE,	NULL},
			{DSI0_VT_AUTO_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// BROWSER_APP
		{
			{DSI0_BROWSER_DYNAMIC_MDNIE,	NULL},
			{DSI0_BROWSER_STANDARD_MDNIE,	NULL},
			{DSI0_BROWSER_NATURAL_MDNIE,	NULL},
			{DSI0_BROWSER_MOVIE_MDNIE,	NULL},
			{DSI0_BROWSER_AUTO_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// eBOOK_APP
		{
			{DSI0_EBOOK_DYNAMIC_MDNIE,	NULL},
			{DSI0_EBOOK_STANDARD_MDNIE,NULL},
			{DSI0_EBOOK_NATURAL_MDNIE,	NULL},
			{DSI0_EBOOK_MOVIE_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// EMAIL_APP
		{
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EMAIL_AUTO_MDNIE,	NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
		// GAME_LOW_APP
		{
			{DSI0_GAME_LOW_MDNIE,	NULL},
			{DSI0_GAME_LOW_MDNIE,	NULL},
			{DSI0_GAME_LOW_MDNIE,	NULL},
			{DSI0_GAME_LOW_MDNIE,	NULL},
			{DSI0_GAME_LOW_MDNIE,	NULL},
			{DSI0_GAME_LOW_MDNIE,	NULL},
		},
		// GAME_MID_APP
		{
			{DSI0_GAME_MID_MDNIE,	NULL},
			{DSI0_GAME_MID_MDNIE,	NULL},
			{DSI0_GAME_MID_MDNIE,	NULL},
			{DSI0_GAME_MID_MDNIE,	NULL},
			{DSI0_GAME_MID_MDNIE,	NULL},
			{DSI0_GAME_MID_MDNIE,	NULL},
		},
		// GAME_HIGH_APP
		{
			{DSI0_GAME_HIGH_MDNIE,	NULL},
			{DSI0_GAME_HIGH_MDNIE,	NULL},
			{DSI0_GAME_HIGH_MDNIE,	NULL},
			{DSI0_GAME_HIGH_MDNIE,	NULL},
			{DSI0_GAME_HIGH_MDNIE,	NULL},
			{DSI0_GAME_HIGH_MDNIE,	NULL},
		},
		// VIDEO_ENHANCER_APP
		{
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_MDNIE,	NULL},
		},
		// VIDEO_ENHANCER_THIRD_APP
		{
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
			{DSI0_VIDEO_ENHANCER_THIRD_MDNIE,	NULL},
		},
		// TDMB_APP
		{
			{DSI0_TDMB_DYNAMIC_MDNIE,       NULL},
			{DSI0_TDMB_STANDARD_MDNIE,      NULL},
			{DSI0_TDMB_NATURAL_MDNIE,       NULL},
			{DSI0_TDMB_MOVIE_MDNIE,         NULL},
			{DSI0_TDMB_AUTO_MDNIE,          NULL},
			{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_3000K_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_3000K_MDNIE_1)}, DSI0_HMT_COLOR_TEMP_3000K_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_3000K_MDNIE_2)}, DSI0_HMT_COLOR_TEMP_3000K_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_3000K_MDNIE_3)}, DSI0_HMT_COLOR_TEMP_3000K_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_4000K_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_4000K_MDNIE_1)}, DSI0_HMT_COLOR_TEMP_4000K_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_4000K_MDNIE_2)}, DSI0_HMT_COLOR_TEMP_4000K_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_4000K_MDNIE_3)}, DSI0_HMT_COLOR_TEMP_4000K_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_5000K_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_5000K_MDNIE_1)}, DSI0_HMT_COLOR_TEMP_5000K_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_5000K_MDNIE_2)}, DSI0_HMT_COLOR_TEMP_5000K_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_5000K_MDNIE_3)}, DSI0_HMT_COLOR_TEMP_5000K_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_6500K_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_1)}, DSI0_HMT_COLOR_TEMP_6500K_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_2)}, DSI0_HMT_COLOR_TEMP_6500K_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_3)}, DSI0_HMT_COLOR_TEMP_6500K_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_7500K_MDNIE[] = {
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(level_1_key_on)}, level_1_key_on},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_7500K_MDNIE_1)}, DSI0_HMT_COLOR_TEMP_7500K_MDNIE_1},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_7500K_MDNIE_2)}, DSI0_HMT_COLOR_TEMP_7500K_MDNIE_2},
	{{DTYPE_DCS_LWRITE, 0, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_7500K_MDNIE_3)}, DSI0_HMT_COLOR_TEMP_7500K_MDNIE_3},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(level_1_key_off)}, level_1_key_off},
};

static struct dsi_cmd_desc *hmt_color_temperature_tune_value_dsi0[HMT_COLOR_TEMP_MAX] = {
	/*
		HMT_COLOR_TEMP_3000K, // 3000K
		HMT_COLOR_TEMP_4000K, // 4000K
		HMT_COLOR_TEMP_5000K, // 5000K
		HMT_COLOR_TEMP_6500K, // 6500K + gamma 2.2
		HMT_COLOR_TEMP_7500K, // 7500K + gamma 2.2
	*/
	NULL,
	DSI0_HMT_COLOR_TEMP_3000K_MDNIE,
	DSI0_HMT_COLOR_TEMP_4000K_MDNIE,
	DSI0_HMT_COLOR_TEMP_5000K_MDNIE,
	DSI0_HMT_COLOR_TEMP_6500K_MDNIE,
	DSI0_HMT_COLOR_TEMP_7500K_MDNIE,
};

static struct dsi_cmd_desc *hdr_tune_value_dsi0[HDR_MAX] = {
	NULL,
	DSI0_HDR_VIDEO_1_MDNIE,
	DSI0_HDR_VIDEO_2_MDNIE,
	DSI0_HDR_VIDEO_3_MDNIE,
};

#define DSI0_RGB_SENSOR_MDNIE_1_SIZE ARRAY_SIZE(DSI0_RGB_SENSOR_MDNIE_1)
#define DSI0_RGB_SENSOR_MDNIE_2_SIZE ARRAY_SIZE(DSI0_RGB_SENSOR_MDNIE_2)
#define DSI0_RGB_SENSOR_MDNIE_3_SIZE ARRAY_SIZE(DSI0_RGB_SENSOR_MDNIE_3)

#endif
