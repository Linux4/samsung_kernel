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
#ifndef _SS_DSI_MDNIE_S6E3HA3_AMS567JA01_H_
#define _SS_DSI_MDNIE_S6E3HA3_AMS567JA01_H_

#include "../ss_dsi_mdnie_lite_common.h"

#define MDNIE_COLOR_BLINDE_CMD_OFFSET 34

#define ADDRESS_SCR_WHITE_RED   0x34
#define ADDRESS_SCR_WHITE_GREEN 0x36
#define ADDRESS_SCR_WHITE_BLUE  0x38

#define MDNIE_STEP1_INDEX 1
#define MDNIE_STEP2_INDEX 2
#define MDNIE_STEP3_INDEX 3

#define MDNIE_TRANS_DIMMING_DATA_INDEX	2

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
	adjust_ldu_data_1,
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
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_BYPASS_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_BYPASS_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_NEGATIVE_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
};

static char DSI0_NEGATIVE_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_NEGATIVE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_GRAYSCALE_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
};

static char DSI0_GRAYSCALE_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_GRAYSCALE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xb3, //ascr_skin_Rr
	0xb3, //ascr_skin_Rg
	0xb3, //ascr_skin_Rb
	0x1d, //ascr_skin_Yr
	0x1d, //ascr_skin_Yg
	0x1d, //ascr_skin_Yb
	0x96, //ascr_skin_Mr
	0x96, //ascr_skin_Mg
	0x96, //ascr_skin_Mb
	0x00, //ascr_skin_Wr
	0x00, //ascr_skin_Wg
	0x00, //ascr_skin_Wb
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_COLOR_BLIND_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_COLOR_BLIND_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_COLOR_BLIND_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_NIGHT_MODE_MDNIE_1[] = {
	0xDF,
	0x20, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_NIGHT_MODE_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_NIGHT_MODE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_HBM_CE_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_HBM_CE_MDNIE_2[] = {
	0xDE,
	0x88, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x04, //lce_large_w
	0x06, //lce_med_w
	0x06, //lce_small_w
	0x00, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x40,
	0x00, //curve_1_b
	0x6b, //curve_1_a
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
};

static char DSI0_HBM_CE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3f, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_RGB_SENSOR_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_RGB_SENSOR_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_RGB_SENSOR_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_SCREEN_CURTAIN_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
};

static char DSI0_SCREEN_CURTAIN_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_SCREEN_CURTAIN_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_HDR_VIDEO_1_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_HDR_VIDEO_1_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x0c, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0xd0,
	0x00, //curve_1_b
	0x1a, //curve_1_a
	0x00, //curve_2_b
	0x1a, //curve_2_a
	0x00, //curve_3_b
	0x1a, //curve_3_a
	0x00, //curve_4_b
	0x1a, //curve_4_a
	0x01, //curve_5_b
	0x18, //curve_5_a
	0x01, //curve_6_b
	0x18, //curve_6_a
	0x01, //curve_7_b
	0x18, //curve_7_a
	0x01, //curve_8_b
	0x18, //curve_8_a
	0x01, //curve_9_b
	0x18, //curve_9_a
	0x03, //curve10_b
	0x16, //curve10_a
	0x03, //curve11_b
	0x16, //curve11_a
	0x04, //curve12_b
	0x16, //curve12_a
	0x0e, //curve13_b
	0xa2, //curve13_a
	0x0e, //curve14_b
	0xa2, //curve14_a
	0x0f, //curve15_b
	0xa2, //curve15_a
	0x0f, //curve16_b
	0xa2, //curve16_a
	0x34, //curve17_b
	0xb4, //curve17_a
	0x3e, //curve18_b
	0xb8, //curve18_a
	0x50, //curve19_b
	0xbf, //curve19_a
	0x1a, //curve20_b
	0xb1, //curve20_a
	0x6a, //curve21_b
	0x18, //curve21_a
	0xff, //curve22_b
	0x00, //curve22_a
	0xff, //curve23_b
	0x00, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_HDR_VIDEO_1_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_HDR_VIDEO_2_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_HDR_VIDEO_2_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x0c, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0xd0,
	0x00, //curve_1_b
	0x1a, //curve_1_a
	0x00, //curve_2_b
	0x1a, //curve_2_a
	0x00, //curve_3_b
	0x1a, //curve_3_a
	0x00, //curve_4_b
	0x1a, //curve_4_a
	0x01, //curve_5_b
	0x18, //curve_5_a
	0x01, //curve_6_b
	0x18, //curve_6_a
	0x01, //curve_7_b
	0x18, //curve_7_a
	0x01, //curve_8_b
	0x18, //curve_8_a
	0x01, //curve_9_b
	0x18, //curve_9_a
	0x03, //curve10_b
	0x16, //curve10_a
	0x03, //curve11_b
	0x16, //curve11_a
	0x04, //curve12_b
	0x16, //curve12_a
	0x0e, //curve13_b
	0xa2, //curve13_a
	0x0e, //curve14_b
	0xa2, //curve14_a
	0x0f, //curve15_b
	0xa2, //curve15_a
	0x0f, //curve16_b
	0xa2, //curve16_a
	0x34, //curve17_b
	0xb4, //curve17_a
	0x3e, //curve18_b
	0xb8, //curve18_a
	0x5d, //curve19_b
	0xc4, //curve19_a
	0x10, //curve20_b
	0xb0, //curve20_a
	0x9a, //curve21_b
	0x10, //curve21_a
	0xff, //curve22_b
	0x00, //curve22_a
	0xff, //curve23_b
	0x00, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_HDR_VIDEO_2_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_HDR_VIDEO_3_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_HDR_VIDEO_3_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_HDR_VIDEO_3_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_ENHANCER_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_VIDEO_ENHANCER_MDNIE_2[] = {
	0xDE,
	0x88, //lce_on gain 0 000 0000
	0x18, //lce_color_gain 00 0000
	0xff, //lce_min_ref_offset
	0xc0, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x40,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0x05, //lce_dark_th 000
	0x01, //lce_reduct_slope 0000
	0x46, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x04, //lce_large_w
	0x06, //lce_med_w
	0x06, //lce_small_w
	0x00, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x14, //curve_1_a
	0x00, //curve_2_b
	0x14, //curve_2_a
	0x00, //curve_3_b
	0x14, //curve_3_a
	0x00, //curve_4_b
	0x14, //curve_4_a
	0x03, //curve_5_b
	0x9a, //curve_5_a
	0x03, //curve_6_b
	0x9a, //curve_6_a
	0x03, //curve_7_b
	0x9a, //curve_7_a
	0x03, //curve_8_b
	0x9a, //curve_8_a
	0x07, //curve_9_b
	0x9e, //curve_9_a
	0x07, //curve10_b
	0x9e, //curve10_a
	0x07, //curve11_b
	0x9e, //curve11_a
	0x07, //curve12_b
	0x9e, //curve12_a
	0x0a, //curve13_b
	0xa0, //curve13_a
	0x0a, //curve14_b
	0xa0, //curve14_a
	0x0a, //curve15_b
	0xa0, //curve15_a
	0x0a, //curve16_b
	0xa0, //curve16_a
	0x16, //curve17_b
	0xa6, //curve17_a
	0x16, //curve18_b
	0xa6, //curve18_a
	0x16, //curve19_b
	0xa6, //curve19_a
	0x16, //curve20_b
	0xa6, //curve20_a
	0x05, //curve21_b
	0x21, //curve21_a
	0x0b, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_VIDEO_ENHANCER_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3f, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_ENHANCER_THIRD_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_VIDEO_ENHANCER_THIRD_MDNIE_2[] = {
	0xDE,
	0x81, //lce_on gain 0 000 0000
	0x18, //lce_color_gain 00 0000
	0xff, //lce_min_ref_offset
	0xc0, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x40,
	0x01, //lce_ref_gain 9
	0x00,
	0x66, //lce_block_size h v 0000 0000
	0x05, //lce_dark_th 000
	0x01, //lce_reduct_slope 0000
	0x46, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x04, //lce_large_w
	0x06, //lce_med_w
	0x06, //lce_small_w
	0x00, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0x17, //curve20_b
	0xa6, //curve20_a
	0x04, //curve21_b
	0x21, //curve21_a
	0x0a, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_VIDEO_ENHANCER_THIRD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3f, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_UI_DYNAMIC_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_UI_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x03, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_1_b
	0x14, //curve_1_a
	0x00, //curve_2_b
	0x14, //curve_2_a
	0x00, //curve_3_b
	0x14, //curve_3_a
	0x00, //curve_4_b
	0x14, //curve_4_a
	0x03, //curve_5_b
	0x9a, //curve_5_a
	0x03, //curve_6_b
	0x9a, //curve_6_a
	0x03, //curve_7_b
	0x9a, //curve_7_a
	0x03, //curve_8_b
	0x9a, //curve_8_a
	0x07, //curve_9_b
	0x9e, //curve_9_a
	0x07, //curve10_b
	0x9e, //curve10_a
	0x07, //curve11_b
	0x9e, //curve11_a
	0x07, //curve12_b
	0x9e, //curve12_a
	0x0a, //curve13_b
	0xa0, //curve13_a
	0x0a, //curve14_b
	0xa0, //curve14_a
	0x0a, //curve15_b
	0xa0, //curve15_a
	0x0a, //curve16_b
	0xa0, //curve16_a
	0x16, //curve17_b
	0xa6, //curve17_a
	0x16, //curve18_b
	0xa6, //curve18_a
	0x16, //curve19_b
	0xa6, //curve19_a
	0x16, //curve20_b
	0xa6, //curve20_a
	0x05, //curve21_b
	0x21, //curve21_a
	0x0b, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_UI_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_UI_STANDARD_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xf1, //ascr_skin_Rr
	0x2f, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x45, //ascr_skin_Yb
	0xfb, //ascr_skin_Mr
	0x48, //ascr_skin_Mg
	0xe7, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf1, //ascr_Rr
	0xef, //ascr_Cg
	0x2f, //ascr_Rg
	0xe9, //ascr_Cb
	0x2e, //ascr_Rb
	0xfb, //ascr_Mr
	0x00, //ascr_Gr
	0x48, //ascr_Mg
	0xdf, //ascr_Gg
	0xe7, //ascr_Mb
	0x00, //ascr_Gb
	0xf2, //ascr_Yr
	0x30, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x45, //ascr_Yb
	0xdf, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_UI_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_UI_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_UI_NATURAL_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xd2, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x26, //ascr_skin_Rb
	0xf3, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x58, //ascr_skin_Yb
	0xdd, //ascr_skin_Mr
	0x41, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x94, //ascr_Cr
	0xd2, //ascr_Rr
	0xf3, //ascr_Cg
	0x26, //ascr_Rg
	0xea, //ascr_Cb
	0x26, //ascr_Rb
	0xdd, //ascr_Mr
	0x8a, //ascr_Gr
	0x41, //ascr_Mg
	0xe7, //ascr_Gg
	0xe1, //ascr_Mb
	0x4e, //ascr_Gb
	0xf3, //ascr_Yr
	0x2a, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x58, //ascr_Yb
	0xdc, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_UI_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_UI_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_UI_MOVIE_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0xd5, //ascr_skin_Rr
	0x2c, //ascr_skin_Rg
	0x2a, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf5, //ascr_skin_Yg
	0x63, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x4a, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf9, //ascr_skin_Wg
	0xf8, //ascr_skin_Wb
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
	0x18, //lce_on gain 0 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x67, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_UI_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_UI_AUTO_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
};

static char DSI0_UI_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x0c, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_UI_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_OUTDOOR_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VIDEO_OUTDOOR_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_VIDEO_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_1_b
	0x14, //curve_1_a
	0x00, //curve_2_b
	0x14, //curve_2_a
	0x00, //curve_3_b
	0x14, //curve_3_a
	0x00, //curve_4_b
	0x14, //curve_4_a
	0x03, //curve_5_b
	0x9a, //curve_5_a
	0x03, //curve_6_b
	0x9a, //curve_6_a
	0x03, //curve_7_b
	0x9a, //curve_7_a
	0x03, //curve_8_b
	0x9a, //curve_8_a
	0x07, //curve_9_b
	0x9e, //curve_9_a
	0x07, //curve10_b
	0x9e, //curve10_a
	0x07, //curve11_b
	0x9e, //curve11_a
	0x07, //curve12_b
	0x9e, //curve12_a
	0x0a, //curve13_b
	0xa0, //curve13_a
	0x0a, //curve14_b
	0xa0, //curve14_a
	0x0a, //curve15_b
	0xa0, //curve15_a
	0x0a, //curve16_b
	0xa0, //curve16_a
	0x16, //curve17_b
	0xa6, //curve17_a
	0x16, //curve18_b
	0xa6, //curve18_a
	0x16, //curve19_b
	0xa6, //curve19_a
	0x16, //curve20_b
	0xa6, //curve20_a
	0x05, //curve21_b
	0x21, //curve21_a
	0x0b, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_STANDARD_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xf1, //ascr_skin_Rr
	0x2f, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x45, //ascr_skin_Yb
	0xfb, //ascr_skin_Mr
	0x48, //ascr_skin_Mg
	0xe7, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf1, //ascr_Rr
	0xef, //ascr_Cg
	0x2f, //ascr_Rg
	0xe9, //ascr_Cb
	0x2e, //ascr_Rb
	0xfb, //ascr_Mr
	0x00, //ascr_Gr
	0x48, //ascr_Mg
	0xdf, //ascr_Gg
	0xe7, //ascr_Mb
	0x00, //ascr_Gb
	0xf2, //ascr_Yr
	0x30, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x45, //ascr_Yb
	0xdf, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_VIDEO_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VIDEO_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_NATURAL_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xd2, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x26, //ascr_skin_Rb
	0xf3, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x58, //ascr_skin_Yb
	0xdd, //ascr_skin_Mr
	0x41, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x94, //ascr_Cr
	0xd2, //ascr_Rr
	0xf3, //ascr_Cg
	0x26, //ascr_Rg
	0xea, //ascr_Cb
	0x26, //ascr_Rb
	0xdd, //ascr_Mr
	0x8a, //ascr_Gr
	0x41, //ascr_Mg
	0xe7, //ascr_Gg
	0xe1, //ascr_Mb
	0x4e, //ascr_Gb
	0xf3, //ascr_Yr
	0x2a, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x58, //ascr_Yb
	0xdc, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_VIDEO_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VIDEO_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_MOVIE_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0xd5, //ascr_skin_Rr
	0x2c, //ascr_skin_Rg
	0x2a, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf5, //ascr_skin_Yg
	0x63, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x4a, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf9, //ascr_skin_Wg
	0xf8, //ascr_skin_Wb
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
	0x18, //lce_on gain 0 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x67, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VIDEO_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_AUTO_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x0c, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_1_b
	0x14, //curve_1_a
	0x00, //curve_2_b
	0x14, //curve_2_a
	0x00, //curve_3_b
	0x14, //curve_3_a
	0x00, //curve_4_b
	0x14, //curve_4_a
	0x03, //curve_5_b
	0x9a, //curve_5_a
	0x03, //curve_6_b
	0x9a, //curve_6_a
	0x03, //curve_7_b
	0x9a, //curve_7_a
	0x03, //curve_8_b
	0x9a, //curve_8_a
	0x07, //curve_9_b
	0x9e, //curve_9_a
	0x07, //curve10_b
	0x9e, //curve10_a
	0x07, //curve11_b
	0x9e, //curve11_a
	0x07, //curve12_b
	0x9e, //curve12_a
	0x0a, //curve13_b
	0xa0, //curve13_a
	0x0a, //curve14_b
	0xa0, //curve14_a
	0x0a, //curve15_b
	0xa0, //curve15_a
	0x0a, //curve16_b
	0xa0, //curve16_a
	0x16, //curve17_b
	0xa6, //curve17_a
	0x16, //curve18_b
	0xa6, //curve18_a
	0x16, //curve19_b
	0xa6, //curve19_a
	0x16, //curve20_b
	0xa6, //curve20_a
	0x05, //curve21_b
	0x21, //curve21_a
	0x0b, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_VIDEO_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_WARM_OUTDOOR_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VIDEO_WARM_OUTDOOR_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_WARM_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VIDEO_WARM_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_COLD_OUTDOOR_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VIDEO_COLD_OUTDOOR_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VIDEO_COLD_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_VIDEO_COLD_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VIDEO_COLD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_CAMERA_OUTDOOR_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_CAMERA_OUTDOOR_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_CAMERA_OUTDOOR_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_CAMERA_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_CAMERA_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_CAMERA_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_CAMERA_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x03, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_1_b
	0x14, //curve_1_a
	0x00, //curve_2_b
	0x14, //curve_2_a
	0x00, //curve_3_b
	0x14, //curve_3_a
	0x00, //curve_4_b
	0x14, //curve_4_a
	0x03, //curve_5_b
	0x9a, //curve_5_a
	0x03, //curve_6_b
	0x9a, //curve_6_a
	0x03, //curve_7_b
	0x9a, //curve_7_a
	0x03, //curve_8_b
	0x9a, //curve_8_a
	0x07, //curve_9_b
	0x9e, //curve_9_a
	0x07, //curve10_b
	0x9e, //curve10_a
	0x07, //curve11_b
	0x9e, //curve11_a
	0x07, //curve12_b
	0x9e, //curve12_a
	0x0a, //curve13_b
	0xa0, //curve13_a
	0x0a, //curve14_b
	0xa0, //curve14_a
	0x0a, //curve15_b
	0xa0, //curve15_a
	0x0a, //curve16_b
	0xa0, //curve16_a
	0x16, //curve17_b
	0xa6, //curve17_a
	0x16, //curve18_b
	0xa6, //curve18_a
	0x16, //curve19_b
	0xa6, //curve19_a
	0x16, //curve20_b
	0xa6, //curve20_a
	0x05, //curve21_b
	0x21, //curve21_a
	0x0b, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_CAMERA_STANDARD_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xf1, //ascr_skin_Rr
	0x2f, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x45, //ascr_skin_Yb
	0xfb, //ascr_skin_Mr
	0x48, //ascr_skin_Mg
	0xe7, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf1, //ascr_Rr
	0xef, //ascr_Cg
	0x2f, //ascr_Rg
	0xe9, //ascr_Cb
	0x2e, //ascr_Rb
	0xfb, //ascr_Mr
	0x00, //ascr_Gr
	0x48, //ascr_Mg
	0xdf, //ascr_Gg
	0xe7, //ascr_Mb
	0x00, //ascr_Gb
	0xf2, //ascr_Yr
	0x30, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x45, //ascr_Yb
	0xdf, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_CAMERA_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_CAMERA_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_CAMERA_NATURAL_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xd2, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x26, //ascr_skin_Rb
	0xf3, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x58, //ascr_skin_Yb
	0xdd, //ascr_skin_Mr
	0x41, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x94, //ascr_Cr
	0xd2, //ascr_Rr
	0xf3, //ascr_Cg
	0x26, //ascr_Rg
	0xea, //ascr_Cb
	0x26, //ascr_Rb
	0xdd, //ascr_Mr
	0x8a, //ascr_Gr
	0x41, //ascr_Mg
	0xe7, //ascr_Gg
	0xe1, //ascr_Mb
	0x4e, //ascr_Gb
	0xf3, //ascr_Yr
	0x2a, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x58, //ascr_Yb
	0xdc, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_CAMERA_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_CAMERA_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_CAMERA_MOVIE_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0xd5, //ascr_skin_Rr
	0x2c, //ascr_skin_Rg
	0x2a, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf5, //ascr_skin_Yg
	0x63, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x4a, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf9, //ascr_skin_Wg
	0xf8, //ascr_skin_Wb
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
	0x18, //lce_on gain 0 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x67, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_CAMERA_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_CAMERA_AUTO_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0x35, //ascr_skin_Rg
	0x4f, //ascr_skin_Rb
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_CAMERA_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_GALLERY_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_1_b
	0x14, //curve_1_a
	0x00, //curve_2_b
	0x14, //curve_2_a
	0x00, //curve_3_b
	0x14, //curve_3_a
	0x00, //curve_4_b
	0x14, //curve_4_a
	0x03, //curve_5_b
	0x9a, //curve_5_a
	0x03, //curve_6_b
	0x9a, //curve_6_a
	0x03, //curve_7_b
	0x9a, //curve_7_a
	0x03, //curve_8_b
	0x9a, //curve_8_a
	0x07, //curve_9_b
	0x9e, //curve_9_a
	0x07, //curve10_b
	0x9e, //curve10_a
	0x07, //curve11_b
	0x9e, //curve11_a
	0x07, //curve12_b
	0x9e, //curve12_a
	0x0a, //curve13_b
	0xa0, //curve13_a
	0x0a, //curve14_b
	0xa0, //curve14_a
	0x0a, //curve15_b
	0xa0, //curve15_a
	0x0a, //curve16_b
	0xa0, //curve16_a
	0x16, //curve17_b
	0xa6, //curve17_a
	0x16, //curve18_b
	0xa6, //curve18_a
	0x16, //curve19_b
	0xa6, //curve19_a
	0x16, //curve20_b
	0xa6, //curve20_a
	0x05, //curve21_b
	0x21, //curve21_a
	0x0b, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_GALLERY_STANDARD_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xf1, //ascr_skin_Rr
	0x2f, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x45, //ascr_skin_Yb
	0xfb, //ascr_skin_Mr
	0x48, //ascr_skin_Mg
	0xe7, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf1, //ascr_Rr
	0xef, //ascr_Cg
	0x2f, //ascr_Rg
	0xe9, //ascr_Cb
	0x2e, //ascr_Rb
	0xfb, //ascr_Mr
	0x00, //ascr_Gr
	0x48, //ascr_Mg
	0xdf, //ascr_Gg
	0xe7, //ascr_Mb
	0x00, //ascr_Gb
	0xf2, //ascr_Yr
	0x30, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x45, //ascr_Yb
	0xdf, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_GALLERY_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_GALLERY_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_GALLERY_NATURAL_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xd2, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x26, //ascr_skin_Rb
	0xf3, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x58, //ascr_skin_Yb
	0xdd, //ascr_skin_Mr
	0x41, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x94, //ascr_Cr
	0xd2, //ascr_Rr
	0xf3, //ascr_Cg
	0x26, //ascr_Rg
	0xea, //ascr_Cb
	0x26, //ascr_Rb
	0xdd, //ascr_Mr
	0x8a, //ascr_Gr
	0x41, //ascr_Mg
	0xe7, //ascr_Gg
	0xe1, //ascr_Mb
	0x4e, //ascr_Gb
	0xf3, //ascr_Yr
	0x2a, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x58, //ascr_Yb
	0xdc, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_GALLERY_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_GALLERY_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_GALLERY_MOVIE_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0xd5, //ascr_skin_Rr
	0x2c, //ascr_skin_Rg
	0x2a, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf5, //ascr_skin_Yg
	0x63, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x4a, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf9, //ascr_skin_Wg
	0xf8, //ascr_skin_Wb
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
	0x18, //lce_on gain 0 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x67, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_GALLERY_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_GALLERY_AUTO_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0x35, //ascr_skin_Rg
	0x4f, //ascr_skin_Rb
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x0c, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x0c, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_GALLERY_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VT_DYNAMIC_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_VT_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_1_b
	0x14, //curve_1_a
	0x00, //curve_2_b
	0x14, //curve_2_a
	0x00, //curve_3_b
	0x14, //curve_3_a
	0x00, //curve_4_b
	0x14, //curve_4_a
	0x03, //curve_5_b
	0x9a, //curve_5_a
	0x03, //curve_6_b
	0x9a, //curve_6_a
	0x03, //curve_7_b
	0x9a, //curve_7_a
	0x03, //curve_8_b
	0x9a, //curve_8_a
	0x07, //curve_9_b
	0x9e, //curve_9_a
	0x07, //curve10_b
	0x9e, //curve10_a
	0x07, //curve11_b
	0x9e, //curve11_a
	0x07, //curve12_b
	0x9e, //curve12_a
	0x0a, //curve13_b
	0xa0, //curve13_a
	0x0a, //curve14_b
	0xa0, //curve14_a
	0x0a, //curve15_b
	0xa0, //curve15_a
	0x0a, //curve16_b
	0xa0, //curve16_a
	0x16, //curve17_b
	0xa6, //curve17_a
	0x16, //curve18_b
	0xa6, //curve18_a
	0x16, //curve19_b
	0xa6, //curve19_a
	0x16, //curve20_b
	0xa6, //curve20_a
	0x05, //curve21_b
	0x21, //curve21_a
	0x0b, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_VT_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VT_STANDARD_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xf1, //ascr_skin_Rr
	0x2f, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x45, //ascr_skin_Yb
	0xfb, //ascr_skin_Mr
	0x48, //ascr_skin_Mg
	0xe7, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf1, //ascr_Rr
	0xef, //ascr_Cg
	0x2f, //ascr_Rg
	0xe9, //ascr_Cb
	0x2e, //ascr_Rb
	0xfb, //ascr_Mr
	0x00, //ascr_Gr
	0x48, //ascr_Mg
	0xdf, //ascr_Gg
	0xe7, //ascr_Mb
	0x00, //ascr_Gb
	0xf2, //ascr_Yr
	0x30, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x45, //ascr_Yb
	0xdf, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_VT_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VT_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VT_NATURAL_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xd2, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x26, //ascr_skin_Rb
	0xf3, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x58, //ascr_skin_Yb
	0xdd, //ascr_skin_Mr
	0x41, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x94, //ascr_Cr
	0xd2, //ascr_Rr
	0xf3, //ascr_Cg
	0x26, //ascr_Rg
	0xea, //ascr_Cb
	0x26, //ascr_Rb
	0xdd, //ascr_Mr
	0x8a, //ascr_Gr
	0x41, //ascr_Mg
	0xe7, //ascr_Gg
	0xe1, //ascr_Mb
	0x4e, //ascr_Gb
	0xf3, //ascr_Yr
	0x2a, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x58, //ascr_Yb
	0xdc, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_VT_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VT_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VT_MOVIE_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0xd5, //ascr_skin_Rr
	0x2c, //ascr_skin_Rg
	0x2a, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf5, //ascr_skin_Yg
	0x63, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x4a, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf9, //ascr_skin_Wg
	0xf8, //ascr_skin_Wb
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
	0x18, //lce_on gain 0 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x67, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VT_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_VT_AUTO_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
};

static char DSI0_VT_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x0c, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x0c, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_VT_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_BROWSER_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x03, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_1_b
	0x14, //curve_1_a
	0x00, //curve_2_b
	0x14, //curve_2_a
	0x00, //curve_3_b
	0x14, //curve_3_a
	0x00, //curve_4_b
	0x14, //curve_4_a
	0x03, //curve_5_b
	0x9a, //curve_5_a
	0x03, //curve_6_b
	0x9a, //curve_6_a
	0x03, //curve_7_b
	0x9a, //curve_7_a
	0x03, //curve_8_b
	0x9a, //curve_8_a
	0x07, //curve_9_b
	0x9e, //curve_9_a
	0x07, //curve10_b
	0x9e, //curve10_a
	0x07, //curve11_b
	0x9e, //curve11_a
	0x07, //curve12_b
	0x9e, //curve12_a
	0x0a, //curve13_b
	0xa0, //curve13_a
	0x0a, //curve14_b
	0xa0, //curve14_a
	0x0a, //curve15_b
	0xa0, //curve15_a
	0x0a, //curve16_b
	0xa0, //curve16_a
	0x16, //curve17_b
	0xa6, //curve17_a
	0x16, //curve18_b
	0xa6, //curve18_a
	0x16, //curve19_b
	0xa6, //curve19_a
	0x16, //curve20_b
	0xa6, //curve20_a
	0x05, //curve21_b
	0x21, //curve21_a
	0x0b, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_BROWSER_STANDARD_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xf1, //ascr_skin_Rr
	0x2f, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x45, //ascr_skin_Yb
	0xfb, //ascr_skin_Mr
	0x48, //ascr_skin_Mg
	0xe7, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf1, //ascr_Rr
	0xef, //ascr_Cg
	0x2f, //ascr_Rg
	0xe9, //ascr_Cb
	0x2e, //ascr_Rb
	0xfb, //ascr_Mr
	0x00, //ascr_Gr
	0x48, //ascr_Mg
	0xdf, //ascr_Gg
	0xe7, //ascr_Mb
	0x00, //ascr_Gb
	0xf2, //ascr_Yr
	0x30, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x45, //ascr_Yb
	0xdf, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_BROWSER_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_BROWSER_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_BROWSER_NATURAL_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xd2, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x26, //ascr_skin_Rb
	0xf3, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x58, //ascr_skin_Yb
	0xdd, //ascr_skin_Mr
	0x41, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x94, //ascr_Cr
	0xd2, //ascr_Rr
	0xf3, //ascr_Cg
	0x26, //ascr_Rg
	0xea, //ascr_Cb
	0x26, //ascr_Rb
	0xdd, //ascr_Mr
	0x8a, //ascr_Gr
	0x41, //ascr_Mg
	0xe7, //ascr_Gg
	0xe1, //ascr_Mb
	0x4e, //ascr_Gb
	0xf3, //ascr_Yr
	0x2a, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x58, //ascr_Yb
	0xdc, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_BROWSER_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_BROWSER_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_BROWSER_MOVIE_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0xd5, //ascr_skin_Rr
	0x2c, //ascr_skin_Rg
	0x2a, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf5, //ascr_skin_Yg
	0x63, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x4a, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf9, //ascr_skin_Wg
	0xf8, //ascr_skin_Wb
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
	0x18, //lce_on gain 0 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x67, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_BROWSER_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_BROWSER_AUTO_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0x48, //ascr_skin_Rg
	0x58, //ascr_skin_Rb
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x0c, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_BROWSER_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_EBOOK_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x03, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_1_b
	0x14, //curve_1_a
	0x00, //curve_2_b
	0x14, //curve_2_a
	0x00, //curve_3_b
	0x14, //curve_3_a
	0x00, //curve_4_b
	0x14, //curve_4_a
	0x03, //curve_5_b
	0x9a, //curve_5_a
	0x03, //curve_6_b
	0x9a, //curve_6_a
	0x03, //curve_7_b
	0x9a, //curve_7_a
	0x03, //curve_8_b
	0x9a, //curve_8_a
	0x07, //curve_9_b
	0x9e, //curve_9_a
	0x07, //curve10_b
	0x9e, //curve10_a
	0x07, //curve11_b
	0x9e, //curve11_a
	0x07, //curve12_b
	0x9e, //curve12_a
	0x0a, //curve13_b
	0xa0, //curve13_a
	0x0a, //curve14_b
	0xa0, //curve14_a
	0x0a, //curve15_b
	0xa0, //curve15_a
	0x0a, //curve16_b
	0xa0, //curve16_a
	0x16, //curve17_b
	0xa6, //curve17_a
	0x16, //curve18_b
	0xa6, //curve18_a
	0x16, //curve19_b
	0xa6, //curve19_a
	0x16, //curve20_b
	0xa6, //curve20_a
	0x05, //curve21_b
	0x21, //curve21_a
	0x0b, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_EBOOK_STANDARD_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xf1, //ascr_skin_Rr
	0x2f, //ascr_skin_Rg
	0x2e, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x45, //ascr_skin_Yb
	0xfb, //ascr_skin_Mr
	0x48, //ascr_skin_Mg
	0xe7, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf1, //ascr_Rr
	0xef, //ascr_Cg
	0x2f, //ascr_Rg
	0xe9, //ascr_Cb
	0x2e, //ascr_Rb
	0xfb, //ascr_Mr
	0x00, //ascr_Gr
	0x48, //ascr_Mg
	0xdf, //ascr_Gg
	0xe7, //ascr_Mb
	0x00, //ascr_Gb
	0xf2, //ascr_Yr
	0x30, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x45, //ascr_Yb
	0xdf, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_EBOOK_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_EBOOK_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_EBOOK_NATURAL_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xd2, //ascr_skin_Rr
	0x26, //ascr_skin_Rg
	0x26, //ascr_skin_Rb
	0xf3, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x58, //ascr_skin_Yb
	0xdd, //ascr_skin_Mr
	0x41, //ascr_skin_Mg
	0xe1, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x94, //ascr_Cr
	0xd2, //ascr_Rr
	0xf3, //ascr_Cg
	0x26, //ascr_Rg
	0xea, //ascr_Cb
	0x26, //ascr_Rb
	0xdd, //ascr_Mr
	0x8a, //ascr_Gr
	0x41, //ascr_Mg
	0xe7, //ascr_Gg
	0xe1, //ascr_Mb
	0x4e, //ascr_Gb
	0xf3, //ascr_Yr
	0x2a, //ascr_Br
	0xed, //ascr_Yg
	0x2f, //ascr_Bg
	0x58, //ascr_Yb
	0xdc, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_EBOOK_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_EBOOK_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_EBOOK_MOVIE_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0xd5, //ascr_skin_Rr
	0x2c, //ascr_skin_Rg
	0x2a, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf5, //ascr_skin_Yg
	0x63, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x4a, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf9, //ascr_skin_Wg
	0xf8, //ascr_skin_Wb
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
	0x18, //lce_on gain 0 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x67, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_EBOOK_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_EBOOK_AUTO_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x0c, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_EBOOK_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_EMAIL_AUTO_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
};

static char DSI0_EMAIL_AUTO_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_EMAIL_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static unsigned char DSI0_GAME_LOW_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xa0, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x00, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00,
	0x08,
	0x00,
	0x08,
	0x00,
	0x08,
	0x00,
	0x08,
	0x00,
	0x08,
	0x00,
	0x08,
	0x02,
	0x8c,
	0x02,
	0x8c,
	0x02,
	0x8c,
	0x00,
	0x0a,
	0x00,
	0x0a,
	0x00,
	0x0a,
	0x00,
	0x0a,
	0x23,
	0xa0,
	0x23,
	0xa0,
	0x23,
	0xa0,
	0x23,
	0xa0,
	0x21,
	0x9f,
	0x20,
	0x9e,
	0x20,
	0x9e,
	0x20,
	0x9e,
	0x20,
	0x9e,
	0x20,
	0x9e,
	0x00,
	0xFF,
};

static unsigned char DSI0_GAME_LOW_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x0f, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static unsigned char DSI0_GAME_MID_MDNIE_1[] = {
	0xDF,
	0x00,
	0x14,
	0x01,
	0x6a,
	0x9a,
	0x25,
	0x1a,
	0x16,
	0x2a,
	0x00,
	0x37,
	0x5a,
	0x00,
	0x4e,
	0xc5,
	0x00,
	0x5d,
	0x17,
	0x00,
	0x30,
	0xc3,
	0xff,
	0x00,
	0x00,
	0xff,
	0xff,
	0x00,
	0xff,
	0x00,
	0xff,
	0xff,
	0xff,
	0xff,
	0x00,
	0xff,
	0xff,
	0x00,
	0xff,
	0x00,
	0xff,
	0x00,
	0x00,
	0xff,
	0xff,
	0x00,
	0xff,
	0x00,
	0xff,
	0x00,
	0x00,
	0xff,
	0xff,
	0x00,
	0xff,
	0x00,
	0xff,
	0x00,
};

static unsigned char DSI0_GAME_MID_MDNIE_2[] = {
	0xDE,
	0x84,
	0x28,
	0x96,
	0x05,
	0x01,
	0x50,
	0x01,
	0xd0,
	0x66,
	0x03,
	0x05,
	0x46,
	0x00,
	0x08,
	0x04,
	0x04,
	0x00,
	0x80,
	0x28,
	0x3e,
	0x7e,
	0xa0,
	0x28,
	0x18,
	0xa0,
	0x07,
	0x65,
	0x06,
	0x03,
	0x00,
	0x40,
	0x00,
	0x00,
	0x10,
	0x00,
	0x00,
	0x30,
	0x00,
	0x00,
	0x00,
	0x30,
	0x00,
	0x00,
	0x30,
	0x00,
	0x00,
	0x01,
	0xff,
	0x01,
	0x00,
	0x00,
	0x07,
	0xff,
	0x07,
	0xff,
	0x14,
	0x00,
	0x0a,
	0x00,
	0x32,
	0x01,
	0xf4,
	0x0b,
	0x8a,
	0x6e,
	0x99,
	0x1b,
	0x17,
	0x1e,
	0x1b,
	0x02,
	0x5f,
	0x03,
	0x33,
	0x02,
	0xc8,
	0x02,
	0x22,
	0x10,
	0x10,
	0x07,
	0x07,
	0x20,
	0x2d,
	0x01,
	0x00,
	0x00,
	0x08,
	0x00,
	0x08,
	0x00,
	0x08,
	0x00,
	0x08,
	0x00,
	0x08,
	0x00,
	0x08,
	0x02,
	0x8c,
	0x02,
	0x8c,
	0x02,
	0x8c,
	0x00,
	0x0a,
	0x00,
	0x0a,
	0x00,
	0x0a,
	0x00,
	0x0a,
	0x23,
	0xa0,
	0x23,
	0xa0,
	0x23,
	0xa0,
	0x23,
	0xa0,
	0x21,
	0x9f,
	0x20,
	0x9e,
	0x20,
	0x9e,
	0x20,
	0x9e,
	0x20,
	0x9e,
	0x20,
	0x9e,
	0x00,
	0xFF,
};

static unsigned char DSI0_GAME_MID_MDNIE_3[] = {
	0xDD,
	0x01,
	0x00,
	0x0f,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
};

static unsigned char DSI0_GAME_HIGH_MDNIE_1[] = {
	0xDF,
	0x00,
	0x14,
	0x01,
	0x6a,
	0x9a,
	0x25,
	0x1a,
	0x16,
	0x2a,
	0x00,
	0x37,
	0x5a,
	0x00,
	0x4e,
	0xc5,
	0x00,
	0x5d,
	0x17,
	0x00,
	0x30,
	0xc3,
	0xff,
	0x00,
	0x00,
	0xff,
	0xff,
	0x00,
	0xff,
	0x00,
	0xff,
	0xff,
	0xff,
	0xff,
	0x00,
	0xff,
	0xff,
	0x00,
	0xff,
	0x00,
	0xff,
	0x00,
	0x00,
	0xff,
	0xff,
	0x00,
	0xff,
	0x00,
	0xff,
	0x00,
	0x00,
	0xff,
	0xff,
	0x00,
	0xff,
	0x00,
	0xff,
	0x00,
};

static unsigned char DSI0_GAME_HIGH_MDNIE_2[] = {
	0xDE,
	0x84,
	0x28,
	0x96,
	0x05,
	0x01,
	0x60,
	0x01,
	0xf0,
	0x66,
	0x03,
	0x05,
	0x46,
	0x00,
	0x08,
	0x04,
	0x04,
	0x00,
	0x80,
	0x28,
	0x3e,
	0x7e,
	0xa0,
	0x28,
	0x18,
	0xa0,
	0x07,
	0x65,
	0x06,
	0x03,
	0x00,
	0x40,
	0x00,
	0x00,
	0x10,
	0x00,
	0x00,
	0x30,
	0x00,
	0x00,
	0x00,
	0x30,
	0x00,
	0x00,
	0x30,
	0x00,
	0x00,
	0x01,
	0xff,
	0x01,
	0x00,
	0x00,
	0x07,
	0xff,
	0x07,
	0xff,
	0x14,
	0x00,
	0x0a,
	0x00,
	0x32,
	0x01,
	0xf4,
	0x0b,
	0x8a,
	0x6e,
	0x99,
	0x1b,
	0x17,
	0x1e,
	0x1b,
	0x02,
	0x5f,
	0x03,
	0x33,
	0x02,
	0xc8,
	0x02,
	0x22,
	0x10,
	0x10,
	0x07,
	0x07,
	0x20,
	0x2d,
	0x01,
	0x00,
	0x00,
	0x0b,
	0x00,
	0x0b,
	0x00,
	0x0b,
	0x00,
	0x0b,
	0x00,
	0x0b,
	0x0c,
	0xa0,
	0x0c,
	0xa0,
	0x0c,
	0xa0,
	0x02,
	0x12,
	0x02,
	0x12,
	0x02,
	0x12,
	0x02,
	0x12,
	0x02,
	0x12,
	0x02,
	0x12,
	0x02,
	0x12,
	0x02,
	0x12,
	0x02,
	0x12,
	0x1e,
	0xa0,
	0x1e,
	0xa0,
	0x1e,
	0xa0,
	0x1e,
	0xa0,
	0x1e,
	0xa0,
	0x1e,
	0xa0,
	0x00,
	0xFF,
};

static unsigned char DSI0_GAME_HIGH_MDNIE_3[] = {
	0xDD,
	0x01,
	0x00,
	0x0f,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
};

static char DSI0_TDMB_STANDARD_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xf0, //ascr_skin_Rr
	0x2f, //ascr_skin_Rg
	0x31, //ascr_skin_Rb
	0xf2, //ascr_skin_Yr
	0xec, //ascr_skin_Yg
	0x47, //ascr_skin_Yb
	0xfc, //ascr_skin_Mr
	0x48, //ascr_skin_Mg
	0xe9, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x00, //ascr_Cr
	0xf0, //ascr_Rr
	0xed, //ascr_Cg
	0x2f, //ascr_Rg
	0xec, //ascr_Cb
	0x31, //ascr_Rb
	0xfc, //ascr_Mr
	0x00, //ascr_Gr
	0x48, //ascr_Mg
	0xdf, //ascr_Gg
	0xe9, //ascr_Mb
	0x32, //ascr_Gb
	0xf2, //ascr_Yr
	0x34, //ascr_Br
	0xec, //ascr_Yg
	0x31, //ascr_Bg
	0x47, //ascr_Yb
	0xe1, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_TDMB_STANDARD_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_TDMB_STANDARD_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_TDMB_NATURAL_MDNIE_1[] = {
	0xDF,
	0x70, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0xd2, //ascr_skin_Rr
	0x29, //ascr_skin_Rg
	0x28, //ascr_skin_Rb
	0xf3, //ascr_skin_Yr
	0xed, //ascr_skin_Yg
	0x58, //ascr_skin_Yb
	0xe0, //ascr_skin_Mr
	0x42, //ascr_skin_Mg
	0xe2, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xfa, //ascr_skin_Wg
	0xf1, //ascr_skin_Wb
	0x96, //ascr_Cr
	0xd2, //ascr_Rr
	0xf2, //ascr_Cg
	0x29, //ascr_Rg
	0xee, //ascr_Cb
	0x28, //ascr_Rb
	0xe0, //ascr_Mr
	0x8a, //ascr_Gr
	0x42, //ascr_Mg
	0xe6, //ascr_Gg
	0xe2, //ascr_Mb
	0x4e, //ascr_Gb
	0xf3, //ascr_Yr
	0x30, //ascr_Br
	0xed, //ascr_Yg
	0x32, //ascr_Bg
	0x58, //ascr_Yb
	0xdc, //ascr_Bb
	0xff, //ascr_Wr
	0x00, //ascr_Kr
	0xfa, //ascr_Wg
	0x00, //ascr_Kg
	0xf1, //ascr_Wb
	0x00, //ascr_Kb
};

static char DSI0_TDMB_NATURAL_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
	0x00, //de_gain 10
	0x00,
	0x01, //de_maxplus 11
	0x00,
	0x01, //de_maxminus 11
	0x00,
	0x14, //fa_edge_th
	0x00, //fa_step_p 13
	0x01,
	0x00, //fa_step_n 13
	0x01,
	0x00, //fa_max_de_gain 13
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_TDMB_NATURAL_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x30, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_TDMB_DYNAMIC_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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

static char DSI0_TDMB_DYNAMIC_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x20,
	0x00, //curve_1_b
	0x14, //curve_1_a
	0x00, //curve_2_b
	0x14, //curve_2_a
	0x00, //curve_3_b
	0x14, //curve_3_a
	0x00, //curve_4_b
	0x14, //curve_4_a
	0x03, //curve_5_b
	0x9a, //curve_5_a
	0x03, //curve_6_b
	0x9a, //curve_6_a
	0x03, //curve_7_b
	0x9a, //curve_7_a
	0x03, //curve_8_b
	0x9a, //curve_8_a
	0x07, //curve_9_b
	0x9e, //curve_9_a
	0x07, //curve10_b
	0x9e, //curve10_a
	0x07, //curve11_b
	0x9e, //curve11_a
	0x07, //curve12_b
	0x9e, //curve12_a
	0x0a, //curve13_b
	0xa0, //curve13_a
	0x0a, //curve14_b
	0xa0, //curve14_a
	0x0a, //curve15_b
	0xa0, //curve15_a
	0x0a, //curve16_b
	0xa0, //curve16_a
	0x16, //curve17_b
	0xa6, //curve17_a
	0x16, //curve18_b
	0xa6, //curve18_a
	0x16, //curve19_b
	0xa6, //curve19_a
	0x16, //curve20_b
	0xa6, //curve20_a
	0x05, //curve21_b
	0x21, //curve21_a
	0x0b, //curve22_b
	0x20, //curve22_a
	0x87, //curve23_b
	0x0f, //curve23_a
	0x00, //curve24_b
	0xff, //curve24_a
};

static char DSI0_TDMB_DYNAMIC_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_TDMB_MOVIE_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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
	0xd5, //ascr_skin_Rr
	0x2c, //ascr_skin_Rg
	0x2a, //ascr_skin_Rb
	0xff, //ascr_skin_Yr
	0xf5, //ascr_skin_Yg
	0x63, //ascr_skin_Yb
	0xfe, //ascr_skin_Mr
	0x4a, //ascr_skin_Mg
	0xff, //ascr_skin_Mb
	0xff, //ascr_skin_Wr
	0xf9, //ascr_skin_Wg
	0xf8, //ascr_skin_Wb
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
	0x18, //lce_on gain 0 000 0000
	0x24, //lce_color_gain 00 0000
	0x96, //lce_min_ref_offset
	0xb3, //lce_illum_gain
	0x01, //lce_ref_offset 9
	0x0e,
	0x01, //lce_ref_gain 9
	0x00,
	0x67, //lce_block_size h v 0000 0000
	0x03, //lce_dark_th 000
	0x07, //lce_reduct_slope 0000
	0x40, //lce_black cc0 cc1 color_th 0 0 0 0000
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_TDMB_MOVIE_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_TDMB_AUTO_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
	0x35, //ascr_skin_Rg
	0x4f, //ascr_skin_Rb
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
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x0c, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x0c, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_TDMB_AUTO_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_HMT_COLOR_TEMP_3000K_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_HMT_COLOR_TEMP_3000K_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_HMT_COLOR_TEMP_3000K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_HMT_COLOR_TEMP_4000K_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_HMT_COLOR_TEMP_4000K_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_HMT_COLOR_TEMP_4000K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_HMT_COLOR_TEMP_5000K_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_HMT_COLOR_TEMP_5000K_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_HMT_COLOR_TEMP_5000K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_1[] = {
	0xDF,
	0x30, //linear_on ascr_skin_on strength 0 0 00000
	0x14, //ascr_trans_on ascr_trans_slope 0 0000
	0x01, //ascr_trans_interval 0000 0000
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
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x0c, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x00, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xc0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x00, //glare_luma_th 00 0000
	0x01, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x02, //nr fa de cs gamma 0 0000
	0xff, //nr_mask_th
	0x00, //de_skin
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
	0x70,
	0x01, //fa_pcl_ppi 14
	0xc0,
	0x67, //fa_skin_cb
	0xa9, //fa_skin_cr
	0x17, //fa_dist_up
	0x29, //fa_dist_down
	0x19, //fa_dist_right
	0x27, //fa_dist_left
	0x02, //fa_div_dist_up
	0xc8,
	0x01, //fa_div_dist_down
	0x90,
	0x02, //fa_div_dist_right
	0x8f,
	0x01, //fa_div_dist_left
	0xa4,
	0x20, //fa_px_min_weight
	0x20, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x28, //fa_os_cnt_10_co
	0x3c, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x40,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x3c, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
};

static char DSI0_HMT_COLOR_TEMP_7500K_MDNIE_1[] = {
	0xDF,
	0x00, //linear_on ascr_skin_on strength 0 0 00000
	0x00, //ascr_trans_on ascr_trans_slope 0 0000
	0x00, //ascr_trans_interval 0000 0000
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

static char DSI0_HMT_COLOR_TEMP_7500K_MDNIE_2[] = {
	0xDE,
	0x00, //lce_on gain 0 000 0000
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
	0x00, //lce_adaptive_slce adaptive_illum adaptive_reflect 0 0 0
	0x08, //lce_large_w
	0x04, //lce_med_w
	0x04, //lce_small_w
	0x01, //glare_on uni_mode luma_ctrl_sel chroma_ctrl_sel 0 0 0 0
	0x80, //glare_blk_avg_th 0000 0000
	0x28, //glare_luma_gain 000 0000
	0x3e, //glare_luma_gain_min 000 0000
	0x7e, //glare_luma_en 0000 0000
	0xa0, //glare_luma_ctl_start 0000 0000
	0x28, //glare_luma_gain_min_th 0000 0000
	0x18, //glare_chroma_inf 0 0000
	0xa0, //glare_chroma_min 0000 0000
	0x07, //glare_chroma_gain 0 0000
	0x65, //glare_chroma_blk_th 0000 0000
	0x06, //glare_luma_th 00 0000
	0x03, //glare_luma_step 000 0000
	0x00, //glare_luma_upper_limit 0000 0000
	0x40, //glare_uni_luma_gain 000 0000
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
	0x01, //de_skin
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
	0x6e, //fa_skin_cr
	0x99, //fa_skin_cb
	0x1b, //fa_dist_up
	0x17, //fa_dist_down
	0x1e, //fa_dist_right
	0x1b, //fa_dist_left
	0x02, //fa_div_dist_up
	0x5f,
	0x03, //fa_div_dist_down
	0x33,
	0x02, //fa_div_dist_right
	0xc8,
	0x02, //fa_div_dist_left
	0x22,
	0x10, //fa_px_min_weight
	0x10, //fa_fr_min_weight
	0x07, //fa_skin_zone_w
	0x07, //fa_skin_zone_h
	0x20, //fa_os_cnt_10_co
	0x2d, //fa_os_cnt_20_co
	0x01, //cs_gain 10
	0x00,
	0x00, //curve_1_b
	0x20, //curve_1_a
	0x00, //curve_2_b
	0x20, //curve_2_a
	0x00, //curve_3_b
	0x20, //curve_3_a
	0x00, //curve_4_b
	0x20, //curve_4_a
	0x00, //curve_5_b
	0x20, //curve_5_a
	0x00, //curve_6_b
	0x20, //curve_6_a
	0x00, //curve_7_b
	0x20, //curve_7_a
	0x00, //curve_8_b
	0x20, //curve_8_a
	0x00, //curve_9_b
	0x20, //curve_9_a
	0x00, //curve10_b
	0x20, //curve10_a
	0x00, //curve11_b
	0x20, //curve11_a
	0x00, //curve12_b
	0x20, //curve12_a
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
	0xff, //curve24_a
};

static char DSI0_HMT_COLOR_TEMP_7500K_MDNIE_3[] = {
	0xDD,
	0x01, //mdnie_en
	0x00, //mask 0000
	0x00, //ascr algo aolce 00 00 00
	0x00, //gam_scr_bypass 00
	0x00, //agm_bypass_mode cc_bypass_mode cs_bypass_mode fa_de_bypass_mode 00 00 00 00
	0x00, //fa_bypass_mode nr_bypass_mode glare_bypass_mode aolce_bypass_mode 00 00 00 00
	0x00, //ap_roi_mode ddi_roi_en 000 0
	0x00, //ap_roi_sx 0000
	0x00, //ap_roi_sx 0000 0000
	0x00, //ap_roi_sy 0000
	0x00, //ap_roi_sy 0000 0000
	0x00, //ap_roi_ex 0000
	0x00, //ap_roi_ex 0000 0000
	0x00, //ap_roi_ey 0000
	0x00, //ap_roi_ey 0000 0000
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
