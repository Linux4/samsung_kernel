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
*/
#ifndef _SAMSUNG_DSI_MDNIE_SW83106_WM6676Z21_
#define _SAMSUNG_DSI_MDNIE_SW83106_WM6676Z21_

#include "ss_dsi_mdnie_lite_common.h"

#define MDNIE_COLOR_BLINDE_CMD_OFFSET_CUSTOM_1 1
#define MDNIE_COLOR_BLINDE_CMD_OFFSET_CUSTOM_2 2

#define MDNIE_STEP1_INDEX 1
#define MDNIE_STEP2_INDEX 2
#define MDNIE_STEP3_INDEX 3
#define MDNIE_STEP4_INDEX 4
#define MDNIE_STEP5_INDEX 5
#define MDNIE_STEP6_INDEX 6
#define MDNIE_STEP7_INDEX 7
#define MDNIE_STEP8_INDEX 8
#define MDNIE_STEP9_INDEX 9
#define MDNIE_STEP10_INDEX 10
#define MDNIE_STEP11_INDEX 11
#define MDNIE_STEP12_INDEX 12
#define MDNIE_STEP13_INDEX 13
#define MDNIE_STEP14_INDEX 14
#define MDNIE_STEP15_INDEX 15

static char night_mode_data[] = { /*offet 1 lenght 27*/
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xF3, 0x7E, 0xFD, 0x7B, 0xFA, 0x78, 0xF7, 0x75, 0x00, 0xDB, 0x7B, 0xF7, 0x72, 0xEE, 0x69, 0xE5, 0x60, /* 6500K */
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xEF, 0x7E, 0xFC, 0x7A, 0xF8, 0x76, 0xF4, 0x72, 0x00, 0xCF, 0x7A, 0xF4, 0x6E, 0xE8, 0x62, 0xDC, 0x56, /* 6300K */
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xEB, 0x7D, 0xFB, 0x78, 0xF6, 0x73, 0xF1, 0x6E, 0x00, 0xBF, 0x78, 0xF0, 0x68, 0xE0, 0x58, 0xD0, 0x48, /* 6100K */
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xE3, 0x7C, 0xF9, 0x75, 0xF2, 0x6E, 0xEB, 0x67, 0x00, 0xAF, 0x76, 0xEC, 0x62, 0xD8, 0x4E, 0xC4, 0x3A, /* 5900K */
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xDF, 0x7C, 0xF8, 0x74, 0xF0, 0x6C, 0xE8, 0x64, 0x00, 0x9F, 0x74, 0xE8, 0x5C, 0xD0, 0x44, 0xB8, 0x2C, /* 5700K */
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xD7, 0x7B, 0xF6, 0x71, 0xEC, 0x67, 0xE2, 0x5D, 0x00, 0x8B, 0x71, 0xE3, 0x54, 0xC6, 0x37, 0xA9, 0x1A, /* 5500K */
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xC3, 0x78, 0xF1, 0x69, 0xE2, 0x5A, 0xD3, 0x4B, 0x00, 0x4F, 0x6A, 0xD4, 0x3E, 0xA8, 0x12, 0x7C, 0xE6, /* 4900K */
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xA7, 0x75, 0xEA, 0x5F, 0xD4, 0x49, 0xBE, 0x33, 0x00, 0xFA, 0x5F, 0xBE, 0x1E, 0x7D, 0xDC, 0x3C, 0x9B, /* 4300K */
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x83, 0x70, 0xE1, 0x51, 0xC2, 0x32, 0xA3, 0x13, 0x00, 0x92, 0x52, 0xA4, 0xF7, 0x49, 0x9B, 0xEE, 0x40, /* 3700K */
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x4F, 0x6A, 0xD4, 0x3E, 0xA8, 0x12, 0x7C, 0xE6, 0x00, 0x1A, 0x43, 0x86, 0xCA, 0x0D, 0x50, 0x94, 0xD7, /* 3100K */
	0x00, 0xFF, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xFA, 0x5F, 0xBE, 0x1E, 0x7D, 0xDC, 0x3C, 0x9B, 0x00, 0x75, 0x2E, 0x5D, 0x8C, 0xBB, 0xE9, 0x18, 0x47, /* 2300K */
};


static char night_mode_data_extra[] = { /*offeset 2 len  9 */
    0x03, 0x05, 0xAF, 0x03, 0x01, 0x6B, 0x03, 0x01, 0x6B,  /* 6500K */
    0x03, 0x05, 0xAF, 0x03, 0x01, 0x6B, 0x03, 0x01, 0x6B,  /* 6300K */
    0x03, 0x05, 0xAF, 0x03, 0x01, 0x6B, 0x03, 0x01, 0x6B,  /* 6100K */
    0x03, 0x05, 0xAF, 0x03, 0x01, 0x6B, 0x03, 0x01, 0x6B,  /* 5900K */
    0x03, 0x05, 0xAF, 0x03, 0x01, 0x6B, 0x03, 0x01, 0x6B,  /* 5700K */
    0x03, 0x05, 0xAF, 0x03, 0x01, 0x6B, 0x03, 0x01, 0x6B,  /* 5500K */
    0x03, 0x05, 0xAF, 0x03, 0x01, 0x6B, 0x03, 0x01, 0x6A,  /* 4900K */
    0x03, 0x05, 0xAF, 0x03, 0x01, 0x6B, 0x02, 0x01, 0x5A,  /* 4300K */
    0x03, 0x05, 0xAF, 0x03, 0x01, 0x6B, 0x02, 0x00, 0x56,  /* 3700K */
    0x03, 0x05, 0xAF, 0x03, 0x01, 0x6A, 0x02, 0x00, 0x55,  /* 3100K */
    0x03, 0x05, 0xAF, 0x02, 0x01, 0x5A, 0x01, 0x00, 0x05,  /* 2300K */
};

static char DSI0_BYPASS_MDNIE_1[] = {
	0x53,
	0x00, //block on_off
};

static char DSI0_BYPASS_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_BYPASS_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_BYPASS_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_BYPASS_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_BYPASS_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_BYPASS_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_BYPASS_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_BYPASS_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_BYPASS_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_BYPASS_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_BYPASS_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_BYPASS_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_BYPASS_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_BYPASS_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_NEGATIVE_MDNIE_1[] = {
	0x53,
	0x80, //block on_off
};

static char DSI0_NEGATIVE_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_NEGATIVE_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_NEGATIVE_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_NEGATIVE_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_NEGATIVE_MDNIE_6[] = {
	0xB4,
	0x10, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_NEGATIVE_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_NEGATIVE_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_NEGATIVE_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_NEGATIVE_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_NEGATIVE_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_NEGATIVE_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_NEGATIVE_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_NEGATIVE_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_NEGATIVE_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_GRAYSCALE_MDNIE_1[] = {
	0x53,
	0x00, //block on_off
};

static char DSI0_GRAYSCALE_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_GRAYSCALE_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_GRAYSCALE_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_GRAYSCALE_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_GRAYSCALE_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_GRAYSCALE_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_GRAYSCALE_MDNIE_8[] = {
	0xB6,
	0x81, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_GRAYSCALE_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_GRAYSCALE_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_GRAYSCALE_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_GRAYSCALE_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_GRAYSCALE_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_GRAYSCALE_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_GRAYSCALE_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_1[] = {
	0x53,
	0x00, //block on_off
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_8[] = {
	0xB6,
	0x81, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_GRAYSCALE_NEGATIVE_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_COLOR_BLIND_MDNIE_1[] = {
	0x53,
	0x00, //block on_off
};

static char DSI0_COLOR_BLIND_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_COLOR_BLIND_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_COLOR_BLIND_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_COLOR_BLIND_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_COLOR_BLIND_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_COLOR_BLIND_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_COLOR_BLIND_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_COLOR_BLIND_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_COLOR_BLIND_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_COLOR_BLIND_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_COLOR_BLIND_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_COLOR_BLIND_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_COLOR_BLIND_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_COLOR_BLIND_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};


static char DSI0_NIGHT_MODE_MDNIE_1[] = {
	0x53,
	0x84, //block on_off
};

static char DSI0_NIGHT_MODE_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_NIGHT_MODE_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_NIGHT_MODE_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_NIGHT_MODE_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_NIGHT_MODE_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_NIGHT_MODE_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_NIGHT_MODE_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_NIGHT_MODE_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_NIGHT_MODE_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_NIGHT_MODE_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_NIGHT_MODE_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_NIGHT_MODE_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_NIGHT_MODE_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_NIGHT_MODE_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_HBM_CE_MDNIE_1[] = {
	0x53,
	0x84, //block on_off
};

static char DSI0_HBM_CE_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_HBM_CE_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_HBM_CE_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_HBM_CE_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_HBM_CE_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_HBM_CE_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0xe2, //gr0
	0x76, //gr1
	0xf6, //gr2
	0x6a, //gr3
	0xd7, //gr4
	0x3e, //gr5
	0xa1, //gr6
	0x00, //br0_G
	0xff, //br1
	0xe2, //gr0
	0x76, //gr1
	0xf6, //gr2
	0x6a, //gr3
	0xd7, //gr4
	0x3e, //gr5
	0xa1, //gr6
	0x00, //br0_B
	0xff, //br1
	0xe2, //gr0
	0x76, //gr1
	0xf6, //gr2
	0x6a, //gr3
	0xd7, //gr4
	0x3e, //gr5
	0xa1, //gr6
};

static char DSI0_HBM_CE_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_HBM_CE_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_HBM_CE_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_HBM_CE_MDNIE_11[] = {
	0xB9,
	0x46, //cm_lgain_1
	0x46, //cm_lgain_2
	0x46, //cm_lgain_3
	0x46, //cm_lgain_4
	0x46, //cm_lgain_5
	0x46, //cm_lgain_6
	0x46, //cm_lgain_7
	0x46, //cm_lgain_8
	0x46, //cm_lgain_9
	0x46, //cm_lgain_10
	0x46, //cm_lgain_11
	0x46, //cm_lgain_12
};

static char DSI0_HBM_CE_MDNIE_12[] = {
	0xBA,
	0x8a, //cm_hgain_1
	0x8a, //cm_hgain_2
	0x8a, //cm_hgain_3
	0x8a, //cm_hgain_4
	0x8a, //cm_hgain_5
	0x8a, //cm_hgain_6
	0x8a, //cm_hgain_7
	0x8a, //cm_hgain_8
	0x8a, //cm_hgain_9
	0x8a, //cm_hgain_10
	0x8a, //cm_hgain_11
	0x8a, //cm_hgain_12
};

static char DSI0_HBM_CE_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x6a, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_HBM_CE_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_HBM_CE_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_HBM_CE_D65_MDNIE_1[] = {
	0x53,
	0x84, //block on_off
};

static char DSI0_HBM_CE_D65_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_HBM_CE_D65_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_HBM_CE_D65_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_HBM_CE_D65_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_HBM_CE_D65_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_HBM_CE_D65_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0xe2, //gr0
	0x76, //gr1
	0xf6, //gr2
	0x6a, //gr3
	0xd7, //gr4
	0x3e, //gr5
	0xa1, //gr6
	0x00, //br0_G
	0xff, //br1
	0xe2, //gr0
	0x76, //gr1
	0xf6, //gr2
	0x6a, //gr3
	0xd7, //gr4
	0x3e, //gr5
	0xa1, //gr6
	0x00, //br0_B
	0xff, //br1
	0xe2, //gr0
	0x76, //gr1
	0xf6, //gr2
	0x6a, //gr3
	0xd7, //gr4
	0x3e, //gr5
	0xa1, //gr6
};

static char DSI0_HBM_CE_D65_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_HBM_CE_D65_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_HBM_CE_D65_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_HBM_CE_D65_MDNIE_11[] = {
	0xB9,
	0x46, //cm_lgain_1
	0x46, //cm_lgain_2
	0x46, //cm_lgain_3
	0x46, //cm_lgain_4
	0x46, //cm_lgain_5
	0x46, //cm_lgain_6
	0x46, //cm_lgain_7
	0x46, //cm_lgain_8
	0x46, //cm_lgain_9
	0x46, //cm_lgain_10
	0x46, //cm_lgain_11
	0x46, //cm_lgain_12
};

static char DSI0_HBM_CE_D65_MDNIE_12[] = {
	0xBA,
	0x8a, //cm_hgain_1
	0x8a, //cm_hgain_2
	0x8a, //cm_hgain_3
	0x8a, //cm_hgain_4
	0x8a, //cm_hgain_5
	0x8a, //cm_hgain_6
	0x8a, //cm_hgain_7
	0x8a, //cm_hgain_8
	0x8a, //cm_hgain_9
	0x8a, //cm_hgain_10
	0x8a, //cm_hgain_11
	0x8a, //cm_hgain_12
};

static char DSI0_HBM_CE_D65_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x6a, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_HBM_CE_D65_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_HBM_CE_D65_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_SCREEN_CURTAIN_MDNIE_1[] = {
	0x53,
	0x80, //block on_off
};

static char DSI0_SCREEN_CURTAIN_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_SCREEN_CURTAIN_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_SCREEN_CURTAIN_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_SCREEN_CURTAIN_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_SCREEN_CURTAIN_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x00, //br0_br1_R
	0x00, //gr0_gr1_gr2
	0x00, //gr3_gr4_gr5
	0x00, //br0_br1_G
	0x00, //gr0_gr1_gr2
	0x00, //gr3_gr4_gr5
	0x00, //br0_br1_B
	0x00, //gr0_gr1_gr2
	0x00, //gr3_gr4_gr5
};

static char DSI0_SCREEN_CURTAIN_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0x00, //br1
	0x00, //gr0
	0x00, //gr1
	0x00, //gr2
	0x00, //gr3
	0x00, //gr4
	0x00, //gr5
	0x00, //gr6
	0x00, //br0_G
	0x00, //br1
	0x00, //gr0
	0x00, //gr1
	0x00, //gr2
	0x00, //gr3
	0x00, //gr4
	0x00, //gr5
	0x00, //gr6
	0x00, //br0_B
	0x00, //br1
	0x00, //gr0
	0x00, //gr1
	0x00, //gr2
	0x00, //gr3
	0x00, //gr4
	0x00, //gr5
	0x00, //gr6
};

static char DSI0_SCREEN_CURTAIN_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_SCREEN_CURTAIN_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_SCREEN_CURTAIN_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_SCREEN_CURTAIN_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_SCREEN_CURTAIN_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_SCREEN_CURTAIN_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_SCREEN_CURTAIN_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_SCREEN_CURTAIN_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_1[] = {
	0x53,
	0x80, //block on_off
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x02, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x5A, //gr3_gr4_gr5
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xE7, //br1
	0x7D, //gr0
	0xFA, //gr1
	0x77, //gr2
	0xF4, //gr3
	0x71, //gr4
	0xEE, //gr5
	0x6B, //gr6
	0x00, //br0_B
	0xB2, //br1
	0x56, //gr0
	0xAC, //gr1
	0x03, //gr2
	0x59, //gr3
	0xAF, //gr4
	0x06, //gr5
	0x5C, //gr6
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_LIGHT_NOTIFICATION_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_HDR_VIDEO_1_MDNIE_1[] = {
	0x53,
	0x04, //block on_off
};

static char DSI0_HDR_VIDEO_1_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_HDR_VIDEO_1_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_HDR_VIDEO_1_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_HDR_VIDEO_1_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_HDR_VIDEO_1_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_HDR_VIDEO_1_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_HDR_VIDEO_1_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_HDR_VIDEO_1_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_HDR_VIDEO_1_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_HDR_VIDEO_1_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_HDR_VIDEO_1_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_HDR_VIDEO_1_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_HDR_VIDEO_1_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_HDR_VIDEO_1_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_HDR_VIDEO_2_MDNIE_1[] = {
	0x53,
	0x04, //block on_off
};

static char DSI0_HDR_VIDEO_2_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_HDR_VIDEO_2_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_HDR_VIDEO_2_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_HDR_VIDEO_2_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_HDR_VIDEO_2_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_HDR_VIDEO_2_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_HDR_VIDEO_2_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_HDR_VIDEO_2_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_HDR_VIDEO_2_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_HDR_VIDEO_2_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_HDR_VIDEO_2_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_HDR_VIDEO_2_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_HDR_VIDEO_2_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_HDR_VIDEO_2_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_HDR_VIDEO_3_MDNIE_1[] = {
	0x53,
	0x04, //block on_off
};

static char DSI0_HDR_VIDEO_3_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_HDR_VIDEO_3_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_HDR_VIDEO_3_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_HDR_VIDEO_3_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_HDR_VIDEO_3_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_HDR_VIDEO_3_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_HDR_VIDEO_3_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_HDR_VIDEO_3_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_HDR_VIDEO_3_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_HDR_VIDEO_3_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_HDR_VIDEO_3_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_HDR_VIDEO_3_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_HDR_VIDEO_3_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_HDR_VIDEO_3_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_HDR_VIDEO_4_MDNIE_1[] = {
	0x53,
	0x04, //block on_off
};

static char DSI0_HDR_VIDEO_4_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_HDR_VIDEO_4_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_HDR_VIDEO_4_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_HDR_VIDEO_4_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_HDR_VIDEO_4_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_HDR_VIDEO_4_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_HDR_VIDEO_4_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_HDR_VIDEO_4_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_HDR_VIDEO_4_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_HDR_VIDEO_4_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_HDR_VIDEO_4_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_HDR_VIDEO_4_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_HDR_VIDEO_4_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_HDR_VIDEO_4_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_HDR_VIDEO_5_MDNIE_1[] = {
	0x53,
	0x04, //block on_off
};

static char DSI0_HDR_VIDEO_5_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_HDR_VIDEO_5_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_HDR_VIDEO_5_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_HDR_VIDEO_5_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_HDR_VIDEO_5_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_HDR_VIDEO_5_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_HDR_VIDEO_5_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_HDR_VIDEO_5_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_HDR_VIDEO_5_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_HDR_VIDEO_5_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_HDR_VIDEO_5_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_HDR_VIDEO_5_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_HDR_VIDEO_5_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_HDR_VIDEO_5_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_UI_DYNAMIC_MDNIE_1[] = {
	0x53,
	0x04, //block on_off
};

static char DSI0_UI_DYNAMIC_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_UI_DYNAMIC_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_UI_DYNAMIC_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_UI_DYNAMIC_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_UI_DYNAMIC_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_UI_DYNAMIC_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_UI_DYNAMIC_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_UI_DYNAMIC_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_UI_DYNAMIC_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_UI_DYNAMIC_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_UI_DYNAMIC_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_UI_DYNAMIC_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_UI_DYNAMIC_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_UI_DYNAMIC_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_UI_AUTO_MDNIE_1[] = {
	0x53,
	0x80, //block on_off
};

static char DSI0_UI_AUTO_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_UI_AUTO_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_UI_AUTO_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_UI_AUTO_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_UI_AUTO_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_UI_AUTO_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_UI_AUTO_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_UI_AUTO_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_UI_AUTO_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_UI_AUTO_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_UI_AUTO_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_UI_AUTO_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_UI_AUTO_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_UI_AUTO_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_1[] = {
	0x53,
	0x44, //block on_off
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_4[] = {
	0xB2,
	0x8a, //sharpen
	0x10,
	0x30,
	0x33,
	0x33,
	0x33,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_VIDEO_DYNAMIC_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_VIDEO_AUTO_MDNIE_1[] = {
	0x53,
	0xc4, //block on_off
};

static char DSI0_VIDEO_AUTO_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_VIDEO_AUTO_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_VIDEO_AUTO_MDNIE_4[] = {
	0xB2,
	0x8a, //sharpen
	0x10,
	0x30,
	0x33,
	0x33,
	0x33,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_VIDEO_AUTO_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_VIDEO_AUTO_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_VIDEO_AUTO_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_VIDEO_AUTO_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_VIDEO_AUTO_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_VIDEO_AUTO_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x60, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_VIDEO_AUTO_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x24, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x38, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_VIDEO_AUTO_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0xa0, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x78, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_VIDEO_AUTO_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0xa0, //cm_angle_7
	0x90, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x60, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_VIDEO_AUTO_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_VIDEO_AUTO_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_1[] = {
	0x53,
	0x04, //block on_off
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_CAMERA_DYNAMIC_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_CAMERA_AUTO_MDNIE_1[] = {
	0x53,
	0x84, //block on_off
};

static char DSI0_CAMERA_AUTO_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_CAMERA_AUTO_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_CAMERA_AUTO_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_CAMERA_AUTO_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_CAMERA_AUTO_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_CAMERA_AUTO_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_CAMERA_AUTO_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_CAMERA_AUTO_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_CAMERA_AUTO_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x60, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_CAMERA_AUTO_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x24, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x38, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_CAMERA_AUTO_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0xa0, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x78, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_CAMERA_AUTO_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0xa0, //cm_angle_7
	0x90, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x60, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_CAMERA_AUTO_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_CAMERA_AUTO_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_1[] = {
	0x53,
	0x44, //block on_off
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_4[] = {
	0xB2,
	0x8a, //sharpen
	0x10,
	0x30,
	0x33,
	0x33,
	0x33,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_GALLERY_DYNAMIC_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_GALLERY_AUTO_MDNIE_1[] = {
	0x53,
	0xc4, //block on_off
};

static char DSI0_GALLERY_AUTO_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_GALLERY_AUTO_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_GALLERY_AUTO_MDNIE_4[] = {
	0xB2,
	0x8a, //sharpen
	0x10,
	0x30,
	0x33,
	0x33,
	0x33,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_GALLERY_AUTO_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_GALLERY_AUTO_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_GALLERY_AUTO_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_GALLERY_AUTO_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_GALLERY_AUTO_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_GALLERY_AUTO_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x60, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_GALLERY_AUTO_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x24, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x38, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_GALLERY_AUTO_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0xa0, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x78, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_GALLERY_AUTO_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0xa0, //cm_angle_7
	0x90, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x60, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_GALLERY_AUTO_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_GALLERY_AUTO_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_1[] = {
	0x53,
	0x04, //block on_off
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_BROWSER_DYNAMIC_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_BROWSER_AUTO_MDNIE_1[] = {
	0x53,
	0x84, //block on_off
};

static char DSI0_BROWSER_AUTO_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_BROWSER_AUTO_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_BROWSER_AUTO_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_BROWSER_AUTO_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_BROWSER_AUTO_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_BROWSER_AUTO_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_BROWSER_AUTO_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_BROWSER_AUTO_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_BROWSER_AUTO_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x60, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_BROWSER_AUTO_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x24, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x38, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_BROWSER_AUTO_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0xa0, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x78, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_BROWSER_AUTO_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0xa0, //cm_angle_7
	0x90, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x60, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_BROWSER_AUTO_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_BROWSER_AUTO_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_1[] = {
	0x53,
	0x04, //block on_off
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_EBOOK_DYNAMIC_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_EBOOK_AUTO_MDNIE_1[] = {
	0x53,
	0x80, //block on_off
};

static char DSI0_EBOOK_AUTO_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_EBOOK_AUTO_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_EBOOK_AUTO_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_EBOOK_AUTO_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_EBOOK_AUTO_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6b, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6b, //gr3_gr4_gr5
};

static char DSI0_EBOOK_AUTO_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xEB, //br1
	0x7D, //gr0
	0xFB, //gr1
	0x78, //gr2
	0xF6, //gr3
	0x73, //gr4
	0xF1, //gr5
	0x6E, //gr6
	0x00, //br0_B
	0xBF, //br1
	0x78, //gr0
	0xF0, //gr1
	0x68, //gr2
	0xE0, //gr3
	0x58, //gr4
	0xD0, //gr5
	0x48, //gr6
};

static char DSI0_EBOOK_AUTO_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_EBOOK_AUTO_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_EBOOK_AUTO_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_EBOOK_AUTO_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_EBOOK_AUTO_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_EBOOK_AUTO_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_EBOOK_AUTO_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_EBOOK_AUTO_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_EMAIL_AUTO_MDNIE_1[] = {
	0x53,
	0x80, //block on_off
};

static char DSI0_EMAIL_AUTO_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_EMAIL_AUTO_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_EMAIL_AUTO_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_EMAIL_AUTO_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_EMAIL_AUTO_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_EMAIL_AUTO_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_EMAIL_AUTO_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_EMAIL_AUTO_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_EMAIL_AUTO_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_EMAIL_AUTO_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_EMAIL_AUTO_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_EMAIL_AUTO_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_EMAIL_AUTO_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_EMAIL_AUTO_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_TDMB_DYNAMIC_MDNIE_1[] = {
	0x53,
	0x44, //block on_off
};

static char DSI0_TDMB_DYNAMIC_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_TDMB_DYNAMIC_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_TDMB_DYNAMIC_MDNIE_4[] = {
	0xB2,
	0x8a, //sharpen
	0x10,
	0x30,
	0x33,
	0x33,
	0x33,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_TDMB_DYNAMIC_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_TDMB_DYNAMIC_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xAF, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x01, //gr0_gr1_gr2
	0x6B, //gr3_gr4_gr5
};

static char DSI0_TDMB_DYNAMIC_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xFF, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xF3, //br1
	0x7E, //gr0
	0xFD, //gr1
	0x7B, //gr2
	0xFA, //gr3
	0x78, //gr4
	0xF7, //gr5
	0x75, //gr6
	0x00, //br0_B
	0xDB, //br1
	0x7B, //gr0
	0xF7, //gr1
	0x72, //gr2
	0xEE, //gr3
	0x69, //gr4
	0xE5, //gr5
	0x60, //gr6
};

static char DSI0_TDMB_DYNAMIC_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_TDMB_DYNAMIC_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_TDMB_DYNAMIC_MDNIE_10[] = {
	0xB8,
	0x75, //cm_scp_1
	0xa0, //cm_scp_2
	0xD0, //cm_scp_3
	0x90, //cm_scp_4
	0xc0, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x88, //cm_scp_8
	0xC0, //cm_scp_9
	0x80, //cm_scp_10
	0xb0, //cm_scp_11
	0xAA, //cm_scp_12
};

static char DSI0_TDMB_DYNAMIC_MDNIE_11[] = {
	0xB9,
	0x2e, //cm_lgain_1
	0x40, //cm_lgain_2
	0x50, //cm_lgain_3
	0x40, //cm_lgain_4
	0x4d, //cm_lgain_5
	0x3a, //cm_lgain_6
	0x3a, //cm_lgain_7
	0x3f, //cm_lgain_8
	0x5a, //cm_lgain_9
	0x30, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_TDMB_DYNAMIC_MDNIE_12[] = {
	0xBA,
	0x6e, //cm_hgain_1
	0x60, //cm_hgain_2
	0x9f, //cm_hgain_3
	0x70, //cm_hgain_4
	0xaD, //cm_hgain_5
	0x90, //cm_hgain_6
	0x85, //cm_hgain_7
	0x78, //cm_hgain_8
	0x70, //cm_hgain_9
	0x80, //cm_hgain_10
	0x45, //cm_hgain_11
	0xb0, //cm_hgain_12
};

static char DSI0_TDMB_DYNAMIC_MDNIE_13[] = {
	0xBB,
	0x6c, //cm_angle_1
	0x6e, //cm_angle_2
	0x84, //cm_angle_3
	0x8f, //cm_angle_4
	0x6a, //cm_angle_5
	0x74, //cm_angle_6
	0x7e, //cm_angle_7
	0x99, //cm_angle_8
	0x90, //cm_angle_9
	0x8a, //cm_angle_10
	0x84, //cm_angle_11
	0x8b, //cm_angle_12
};

static char DSI0_TDMB_DYNAMIC_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_TDMB_DYNAMIC_MDNIE_15[] = {
	0xED,
	0x01, //WB_en
	0x00, //R
	0xfe, //G
	0xEF, //B
};

static char DSI0_TDMB_AUTO_MDNIE_1[] = {
	0x53,
	0xc4, //block on_off
};

static char DSI0_TDMB_AUTO_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_TDMB_AUTO_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_TDMB_AUTO_MDNIE_4[] = {
	0xB2,
	0x8a, //sharpen
	0x10,
	0x30,
	0x33,
	0x33,
	0x33,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_TDMB_AUTO_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_TDMB_AUTO_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_TDMB_AUTO_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_TDMB_AUTO_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_TDMB_AUTO_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_TDMB_AUTO_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x60, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_TDMB_AUTO_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x24, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x38, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_TDMB_AUTO_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0xa0, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x78, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_TDMB_AUTO_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0xa0, //cm_angle_7
	0x90, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x60, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_TDMB_AUTO_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_TDMB_AUTO_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_1[] = {
	0x53,
	0x80, //block on_off
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_2[] = {
	0x55,
	0x08, //hdr_on_off acl
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_3[] = {
	0xB0,
	0xA4, //level 4 Protection
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_4[] = {
	0xB2,
	0x89, //sharpen
	0x1f,
	0x1f,
	0x01,
	0x24,
	0x68,
	0x22, //contrast
	0x20,
	0x04,
	0x00,
	0x00,
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_5[] = {
	0xB3,
	0x00, //hdr_gain_l
	0x80, //hdr_gain_m
	0xff, //hdr_gain_h
	0x88, //rgain ggain
	0x08, //bgain
	0x8a, //dim
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_6[] = {
	0xB4,
	0x00, //inv_dim
	0x03, //br0_br1_R
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_G
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
	0x03, //br0_br1_B
	0x05, //gr0_gr1_gr2
	0xaf, //gr3_gr4_gr5
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_7[] = {
	0xB5,
	0x00, //br0_R
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_G
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
	0x00, //br0_B
	0xff, //br1
	0x80, //gr0
	0x00, //gr1
	0x80, //gr2
	0x00, //gr3
	0x80, //gr4
	0x00, //gr5
	0x80, //gr6
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_8[] = {
	0xB6,
	0x80, //BnW on_off
	0x12, //Cb 00 Cr 00
	0x60, //Cb
	0x00, //Cr
	0x00, //CM_Gray
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_9[] = {
	0xB7,
	0x80, //cm_ygain_1
	0x80, //cm_ygain_2
	0x80, //cm_ygain_3
	0x80, //cm_ygain_4
	0x80, //cm_ygain_5
	0x80, //cm_ygain_6
	0x80, //cm_ygain_7
	0x80, //cm_ygain_8
	0x80, //cm_ygain_9
	0x80, //cm_ygain_10
	0x80, //cm_ygain_11
	0x80, //cm_ygain_12
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_10[] = {
	0xB8,
	0x80, //cm_scp_1
	0x80, //cm_scp_2
	0x80, //cm_scp_3
	0x80, //cm_scp_4
	0x80, //cm_scp_5
	0x80, //cm_scp_6
	0x80, //cm_scp_7
	0x80, //cm_scp_8
	0x80, //cm_scp_9
	0x80, //cm_scp_10
	0x80, //cm_scp_11
	0x80, //cm_scp_12
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_11[] = {
	0xB9,
	0x40, //cm_lgain_1
	0x40, //cm_lgain_2
	0x40, //cm_lgain_3
	0x40, //cm_lgain_4
	0x40, //cm_lgain_5
	0x40, //cm_lgain_6
	0x40, //cm_lgain_7
	0x40, //cm_lgain_8
	0x40, //cm_lgain_9
	0x40, //cm_lgain_10
	0x40, //cm_lgain_11
	0x40, //cm_lgain_12
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_12[] = {
	0xBA,
	0x80, //cm_hgain_1
	0x80, //cm_hgain_2
	0x80, //cm_hgain_3
	0x80, //cm_hgain_4
	0x80, //cm_hgain_5
	0x80, //cm_hgain_6
	0x80, //cm_hgain_7
	0x80, //cm_hgain_8
	0x80, //cm_hgain_9
	0x80, //cm_hgain_10
	0x80, //cm_hgain_11
	0x80, //cm_hgain_12
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_13[] = {
	0xBB,
	0x80, //cm_angle_1
	0x80, //cm_angle_2
	0x80, //cm_angle_3
	0x80, //cm_angle_4
	0x80, //cm_angle_5
	0x80, //cm_angle_6
	0x80, //cm_angle_7
	0x80, //cm_angle_8
	0x80, //cm_angle_9
	0x80, //cm_angle_10
	0x80, //cm_angle_11
	0x80, //cm_angle_12
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_14[] = {
	0xB0,
	0xA3, //level 3 Protection
};

static char DSI0_HMT_COLOR_TEMP_6500K_MDNIE_15[] = {
	0xED,
	0x00, //WB_en
	0x00, //R
	0x00, //G
	0x00, //B
};

static struct dsi_cmd_desc DSI0_BYPASS_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_1), DSI0_BYPASS_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_2), DSI0_BYPASS_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_3), DSI0_BYPASS_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_4), DSI0_BYPASS_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_5), DSI0_BYPASS_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_6), DSI0_BYPASS_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_7), DSI0_BYPASS_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_8), DSI0_BYPASS_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_9), DSI0_BYPASS_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_10), DSI0_BYPASS_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_11), DSI0_BYPASS_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_12), DSI0_BYPASS_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_13), DSI0_BYPASS_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_14), DSI0_BYPASS_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BYPASS_MDNIE_15), DSI0_BYPASS_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_NEGATIVE_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_1), DSI0_NEGATIVE_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_2), DSI0_NEGATIVE_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_3), DSI0_NEGATIVE_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_4), DSI0_NEGATIVE_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_5), DSI0_NEGATIVE_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_6), DSI0_NEGATIVE_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_7), DSI0_NEGATIVE_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_8), DSI0_NEGATIVE_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_9), DSI0_NEGATIVE_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_10), DSI0_NEGATIVE_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_11), DSI0_NEGATIVE_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_12), DSI0_NEGATIVE_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_13), DSI0_NEGATIVE_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_14), DSI0_NEGATIVE_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NEGATIVE_MDNIE_15), DSI0_NEGATIVE_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_GRAYSCALE_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_1), DSI0_GRAYSCALE_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_2), DSI0_GRAYSCALE_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_3), DSI0_GRAYSCALE_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_4), DSI0_GRAYSCALE_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_5), DSI0_GRAYSCALE_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_6), DSI0_GRAYSCALE_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_7), DSI0_GRAYSCALE_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_8), DSI0_GRAYSCALE_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_9), DSI0_GRAYSCALE_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_10), DSI0_GRAYSCALE_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_11), DSI0_GRAYSCALE_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_12), DSI0_GRAYSCALE_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_13), DSI0_GRAYSCALE_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_14), DSI0_GRAYSCALE_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_MDNIE_15), DSI0_GRAYSCALE_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_GRAYSCALE_NEGATIVE_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_1), DSI0_GRAYSCALE_NEGATIVE_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_2), DSI0_GRAYSCALE_NEGATIVE_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_3), DSI0_GRAYSCALE_NEGATIVE_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_4), DSI0_GRAYSCALE_NEGATIVE_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_5), DSI0_GRAYSCALE_NEGATIVE_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_6), DSI0_GRAYSCALE_NEGATIVE_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_7), DSI0_GRAYSCALE_NEGATIVE_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_8), DSI0_GRAYSCALE_NEGATIVE_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_9), DSI0_GRAYSCALE_NEGATIVE_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_10), DSI0_GRAYSCALE_NEGATIVE_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_11), DSI0_GRAYSCALE_NEGATIVE_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_12), DSI0_GRAYSCALE_NEGATIVE_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_13), DSI0_GRAYSCALE_NEGATIVE_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_14), DSI0_GRAYSCALE_NEGATIVE_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GRAYSCALE_NEGATIVE_MDNIE_15), DSI0_GRAYSCALE_NEGATIVE_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_COLOR_BLIND_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_1), DSI0_COLOR_BLIND_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_2), DSI0_COLOR_BLIND_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_3), DSI0_COLOR_BLIND_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_4), DSI0_COLOR_BLIND_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_5), DSI0_COLOR_BLIND_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_6), DSI0_COLOR_BLIND_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_7), DSI0_COLOR_BLIND_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_8), DSI0_COLOR_BLIND_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_9), DSI0_COLOR_BLIND_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_10), DSI0_COLOR_BLIND_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_11), DSI0_COLOR_BLIND_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_12), DSI0_COLOR_BLIND_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_13), DSI0_COLOR_BLIND_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_14), DSI0_COLOR_BLIND_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_COLOR_BLIND_MDNIE_15), DSI0_COLOR_BLIND_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_NIGHT_MODE_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_1), DSI0_NIGHT_MODE_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_2), DSI0_NIGHT_MODE_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_3), DSI0_NIGHT_MODE_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_4), DSI0_NIGHT_MODE_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_5), DSI0_NIGHT_MODE_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_6), DSI0_NIGHT_MODE_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_7), DSI0_NIGHT_MODE_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_8), DSI0_NIGHT_MODE_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_9), DSI0_NIGHT_MODE_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_10), DSI0_NIGHT_MODE_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_11), DSI0_NIGHT_MODE_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_12), DSI0_NIGHT_MODE_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_13), DSI0_NIGHT_MODE_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_14), DSI0_NIGHT_MODE_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_NIGHT_MODE_MDNIE_15), DSI0_NIGHT_MODE_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_HBM_CE_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_1), DSI0_HBM_CE_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_2), DSI0_HBM_CE_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_3), DSI0_HBM_CE_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_4), DSI0_HBM_CE_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_5), DSI0_HBM_CE_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_6), DSI0_HBM_CE_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_7), DSI0_HBM_CE_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_8), DSI0_HBM_CE_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_9), DSI0_HBM_CE_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_10), DSI0_HBM_CE_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_11), DSI0_HBM_CE_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_12), DSI0_HBM_CE_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_13), DSI0_HBM_CE_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_14), DSI0_HBM_CE_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_MDNIE_15), DSI0_HBM_CE_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_HBM_CE_D65_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_1), DSI0_HBM_CE_D65_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_2), DSI0_HBM_CE_D65_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_3), DSI0_HBM_CE_D65_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_4), DSI0_HBM_CE_D65_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_5), DSI0_HBM_CE_D65_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_6), DSI0_HBM_CE_D65_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_7), DSI0_HBM_CE_D65_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_8), DSI0_HBM_CE_D65_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_9), DSI0_HBM_CE_D65_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_10), DSI0_HBM_CE_D65_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_11), DSI0_HBM_CE_D65_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_12), DSI0_HBM_CE_D65_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_13), DSI0_HBM_CE_D65_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_14), DSI0_HBM_CE_D65_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HBM_CE_D65_MDNIE_15), DSI0_HBM_CE_D65_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_SCREEN_CURTAIN_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_1), DSI0_SCREEN_CURTAIN_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_2), DSI0_SCREEN_CURTAIN_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_3), DSI0_SCREEN_CURTAIN_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_4), DSI0_SCREEN_CURTAIN_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_5), DSI0_SCREEN_CURTAIN_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_6), DSI0_SCREEN_CURTAIN_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_7), DSI0_SCREEN_CURTAIN_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_8), DSI0_SCREEN_CURTAIN_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_9), DSI0_SCREEN_CURTAIN_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_10), DSI0_SCREEN_CURTAIN_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_11), DSI0_SCREEN_CURTAIN_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_12), DSI0_SCREEN_CURTAIN_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_13), DSI0_SCREEN_CURTAIN_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_14), DSI0_SCREEN_CURTAIN_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_SCREEN_CURTAIN_MDNIE_15), DSI0_SCREEN_CURTAIN_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_LIGHT_NOTIFICATION_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_1), DSI0_LIGHT_NOTIFICATION_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_2), DSI0_LIGHT_NOTIFICATION_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_3), DSI0_LIGHT_NOTIFICATION_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_4), DSI0_LIGHT_NOTIFICATION_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_5), DSI0_LIGHT_NOTIFICATION_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_6), DSI0_LIGHT_NOTIFICATION_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_7), DSI0_LIGHT_NOTIFICATION_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_8), DSI0_LIGHT_NOTIFICATION_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_9), DSI0_LIGHT_NOTIFICATION_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_10), DSI0_LIGHT_NOTIFICATION_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_11), DSI0_LIGHT_NOTIFICATION_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_12), DSI0_LIGHT_NOTIFICATION_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_13), DSI0_LIGHT_NOTIFICATION_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_14), DSI0_LIGHT_NOTIFICATION_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_LIGHT_NOTIFICATION_MDNIE_15), DSI0_LIGHT_NOTIFICATION_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_1_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_1), DSI0_HDR_VIDEO_1_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_2), DSI0_HDR_VIDEO_1_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_3), DSI0_HDR_VIDEO_1_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_4), DSI0_HDR_VIDEO_1_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_5), DSI0_HDR_VIDEO_1_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_6), DSI0_HDR_VIDEO_1_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_7), DSI0_HDR_VIDEO_1_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_8), DSI0_HDR_VIDEO_1_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_9), DSI0_HDR_VIDEO_1_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_10), DSI0_HDR_VIDEO_1_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_11), DSI0_HDR_VIDEO_1_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_12), DSI0_HDR_VIDEO_1_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_13), DSI0_HDR_VIDEO_1_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_14), DSI0_HDR_VIDEO_1_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_1_MDNIE_15), DSI0_HDR_VIDEO_1_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_2_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_1), DSI0_HDR_VIDEO_2_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_2), DSI0_HDR_VIDEO_2_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_3), DSI0_HDR_VIDEO_2_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_4), DSI0_HDR_VIDEO_2_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_5), DSI0_HDR_VIDEO_2_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_6), DSI0_HDR_VIDEO_2_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_7), DSI0_HDR_VIDEO_2_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_8), DSI0_HDR_VIDEO_2_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_9), DSI0_HDR_VIDEO_2_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_10), DSI0_HDR_VIDEO_2_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_11), DSI0_HDR_VIDEO_2_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_12), DSI0_HDR_VIDEO_2_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_13), DSI0_HDR_VIDEO_2_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_14), DSI0_HDR_VIDEO_2_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_2_MDNIE_15), DSI0_HDR_VIDEO_2_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_3_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_1), DSI0_HDR_VIDEO_3_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_2), DSI0_HDR_VIDEO_3_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_3), DSI0_HDR_VIDEO_3_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_4), DSI0_HDR_VIDEO_3_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_5), DSI0_HDR_VIDEO_3_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_6), DSI0_HDR_VIDEO_3_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_7), DSI0_HDR_VIDEO_3_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_8), DSI0_HDR_VIDEO_3_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_9), DSI0_HDR_VIDEO_3_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_10), DSI0_HDR_VIDEO_3_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_11), DSI0_HDR_VIDEO_3_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_12), DSI0_HDR_VIDEO_3_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_13), DSI0_HDR_VIDEO_3_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_14), DSI0_HDR_VIDEO_3_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_3_MDNIE_15), DSI0_HDR_VIDEO_3_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_4_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_1), DSI0_HDR_VIDEO_4_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_2), DSI0_HDR_VIDEO_4_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_3), DSI0_HDR_VIDEO_4_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_4), DSI0_HDR_VIDEO_4_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_5), DSI0_HDR_VIDEO_4_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_6), DSI0_HDR_VIDEO_4_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_7), DSI0_HDR_VIDEO_4_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_8), DSI0_HDR_VIDEO_4_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_9), DSI0_HDR_VIDEO_4_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_10), DSI0_HDR_VIDEO_4_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_11), DSI0_HDR_VIDEO_4_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_12), DSI0_HDR_VIDEO_4_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_13), DSI0_HDR_VIDEO_4_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_14), DSI0_HDR_VIDEO_4_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_4_MDNIE_15), DSI0_HDR_VIDEO_4_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_HDR_VIDEO_5_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_1), DSI0_HDR_VIDEO_5_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_2), DSI0_HDR_VIDEO_5_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_3), DSI0_HDR_VIDEO_5_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_4), DSI0_HDR_VIDEO_5_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_5), DSI0_HDR_VIDEO_5_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_6), DSI0_HDR_VIDEO_5_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_7), DSI0_HDR_VIDEO_5_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_8), DSI0_HDR_VIDEO_5_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_9), DSI0_HDR_VIDEO_5_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_10), DSI0_HDR_VIDEO_5_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_11), DSI0_HDR_VIDEO_5_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_12), DSI0_HDR_VIDEO_5_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_13), DSI0_HDR_VIDEO_5_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_14), DSI0_HDR_VIDEO_5_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HDR_VIDEO_5_MDNIE_15), DSI0_HDR_VIDEO_5_MDNIE_15, 0, NULL}, false, 0},
};

///////////////////////////////////////////////////////////////////////////////////

static struct dsi_cmd_desc DSI0_UI_DYNAMIC_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_1), DSI0_UI_DYNAMIC_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_2), DSI0_UI_DYNAMIC_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_3), DSI0_UI_DYNAMIC_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_4), DSI0_UI_DYNAMIC_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_5), DSI0_UI_DYNAMIC_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_6), DSI0_UI_DYNAMIC_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_7), DSI0_UI_DYNAMIC_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_8), DSI0_UI_DYNAMIC_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_9), DSI0_UI_DYNAMIC_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_10), DSI0_UI_DYNAMIC_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_11), DSI0_UI_DYNAMIC_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_12), DSI0_UI_DYNAMIC_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_13), DSI0_UI_DYNAMIC_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_14), DSI0_UI_DYNAMIC_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_DYNAMIC_MDNIE_15), DSI0_UI_DYNAMIC_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_UI_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_1), DSI0_UI_AUTO_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_2), DSI0_UI_AUTO_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_3), DSI0_UI_AUTO_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_4), DSI0_UI_AUTO_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_5), DSI0_UI_AUTO_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_6), DSI0_UI_AUTO_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_7), DSI0_UI_AUTO_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_8), DSI0_UI_AUTO_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_9), DSI0_UI_AUTO_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_10), DSI0_UI_AUTO_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_11), DSI0_UI_AUTO_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_12), DSI0_UI_AUTO_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_13), DSI0_UI_AUTO_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_14), DSI0_UI_AUTO_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_UI_AUTO_MDNIE_15), DSI0_UI_AUTO_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_VIDEO_DYNAMIC_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_1), DSI0_VIDEO_DYNAMIC_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_2), DSI0_VIDEO_DYNAMIC_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_3), DSI0_VIDEO_DYNAMIC_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_4), DSI0_VIDEO_DYNAMIC_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_5), DSI0_VIDEO_DYNAMIC_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_6), DSI0_VIDEO_DYNAMIC_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_7), DSI0_VIDEO_DYNAMIC_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_8), DSI0_VIDEO_DYNAMIC_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_9), DSI0_VIDEO_DYNAMIC_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_10), DSI0_VIDEO_DYNAMIC_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_11), DSI0_VIDEO_DYNAMIC_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_12), DSI0_VIDEO_DYNAMIC_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_13), DSI0_VIDEO_DYNAMIC_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_14), DSI0_VIDEO_DYNAMIC_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_DYNAMIC_MDNIE_15), DSI0_VIDEO_DYNAMIC_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_VIDEO_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_1), DSI0_VIDEO_AUTO_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_2), DSI0_VIDEO_AUTO_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_3), DSI0_VIDEO_AUTO_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_4), DSI0_VIDEO_AUTO_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_5), DSI0_VIDEO_AUTO_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_6), DSI0_VIDEO_AUTO_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_7), DSI0_VIDEO_AUTO_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_8), DSI0_VIDEO_AUTO_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_9), DSI0_VIDEO_AUTO_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_10), DSI0_VIDEO_AUTO_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_11), DSI0_VIDEO_AUTO_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_12), DSI0_VIDEO_AUTO_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_13), DSI0_VIDEO_AUTO_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_14), DSI0_VIDEO_AUTO_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_VIDEO_AUTO_MDNIE_15), DSI0_VIDEO_AUTO_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_CAMERA_DYNAMIC_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_1), DSI0_CAMERA_DYNAMIC_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_2), DSI0_CAMERA_DYNAMIC_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_3), DSI0_CAMERA_DYNAMIC_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_4), DSI0_CAMERA_DYNAMIC_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_5), DSI0_CAMERA_DYNAMIC_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_6), DSI0_CAMERA_DYNAMIC_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_7), DSI0_CAMERA_DYNAMIC_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_8), DSI0_CAMERA_DYNAMIC_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_9), DSI0_CAMERA_DYNAMIC_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_10), DSI0_CAMERA_DYNAMIC_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_11), DSI0_CAMERA_DYNAMIC_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_12), DSI0_CAMERA_DYNAMIC_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_13), DSI0_CAMERA_DYNAMIC_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_14), DSI0_CAMERA_DYNAMIC_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_DYNAMIC_MDNIE_15), DSI0_CAMERA_DYNAMIC_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_CAMERA_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_1), DSI0_CAMERA_AUTO_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_2), DSI0_CAMERA_AUTO_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_3), DSI0_CAMERA_AUTO_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_4), DSI0_CAMERA_AUTO_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_5), DSI0_CAMERA_AUTO_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_6), DSI0_CAMERA_AUTO_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_7), DSI0_CAMERA_AUTO_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_8), DSI0_CAMERA_AUTO_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_9), DSI0_CAMERA_AUTO_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_10), DSI0_CAMERA_AUTO_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_11), DSI0_CAMERA_AUTO_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_12), DSI0_CAMERA_AUTO_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_13), DSI0_CAMERA_AUTO_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_14), DSI0_CAMERA_AUTO_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_CAMERA_AUTO_MDNIE_15), DSI0_CAMERA_AUTO_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_GALLERY_DYNAMIC_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_1), DSI0_GALLERY_DYNAMIC_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_2), DSI0_GALLERY_DYNAMIC_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_3), DSI0_GALLERY_DYNAMIC_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_4), DSI0_GALLERY_DYNAMIC_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_5), DSI0_GALLERY_DYNAMIC_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_6), DSI0_GALLERY_DYNAMIC_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_7), DSI0_GALLERY_DYNAMIC_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_8), DSI0_GALLERY_DYNAMIC_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_9), DSI0_GALLERY_DYNAMIC_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_10), DSI0_GALLERY_DYNAMIC_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_11), DSI0_GALLERY_DYNAMIC_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_12), DSI0_GALLERY_DYNAMIC_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_13), DSI0_GALLERY_DYNAMIC_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_14), DSI0_GALLERY_DYNAMIC_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_DYNAMIC_MDNIE_15), DSI0_GALLERY_DYNAMIC_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_GALLERY_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_1), DSI0_GALLERY_AUTO_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_2), DSI0_GALLERY_AUTO_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_3), DSI0_GALLERY_AUTO_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_4), DSI0_GALLERY_AUTO_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_5), DSI0_GALLERY_AUTO_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_6), DSI0_GALLERY_AUTO_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_7), DSI0_GALLERY_AUTO_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_8), DSI0_GALLERY_AUTO_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_9), DSI0_GALLERY_AUTO_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_10), DSI0_GALLERY_AUTO_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_11), DSI0_GALLERY_AUTO_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_12), DSI0_GALLERY_AUTO_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_13), DSI0_GALLERY_AUTO_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_14), DSI0_GALLERY_AUTO_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_GALLERY_AUTO_MDNIE_15), DSI0_GALLERY_AUTO_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_BROWSER_DYNAMIC_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_1), DSI0_BROWSER_DYNAMIC_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_2), DSI0_BROWSER_DYNAMIC_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_3), DSI0_BROWSER_DYNAMIC_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_4), DSI0_BROWSER_DYNAMIC_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_5), DSI0_BROWSER_DYNAMIC_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_6), DSI0_BROWSER_DYNAMIC_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_7), DSI0_BROWSER_DYNAMIC_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_8), DSI0_BROWSER_DYNAMIC_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_9), DSI0_BROWSER_DYNAMIC_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_10), DSI0_BROWSER_DYNAMIC_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_11), DSI0_BROWSER_DYNAMIC_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_12), DSI0_BROWSER_DYNAMIC_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_13), DSI0_BROWSER_DYNAMIC_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_14), DSI0_BROWSER_DYNAMIC_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_DYNAMIC_MDNIE_15), DSI0_BROWSER_DYNAMIC_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_BROWSER_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_1), DSI0_BROWSER_AUTO_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_2), DSI0_BROWSER_AUTO_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_3), DSI0_BROWSER_AUTO_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_4), DSI0_BROWSER_AUTO_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_5), DSI0_BROWSER_AUTO_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_6), DSI0_BROWSER_AUTO_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_7), DSI0_BROWSER_AUTO_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_8), DSI0_BROWSER_AUTO_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_9), DSI0_BROWSER_AUTO_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_10), DSI0_BROWSER_AUTO_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_11), DSI0_BROWSER_AUTO_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_12), DSI0_BROWSER_AUTO_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_13), DSI0_BROWSER_AUTO_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_14), DSI0_BROWSER_AUTO_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_BROWSER_AUTO_MDNIE_15), DSI0_BROWSER_AUTO_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_EBOOK_DYNAMIC_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_1), DSI0_EBOOK_DYNAMIC_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_2), DSI0_EBOOK_DYNAMIC_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_3), DSI0_EBOOK_DYNAMIC_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_4), DSI0_EBOOK_DYNAMIC_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_5), DSI0_EBOOK_DYNAMIC_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_6), DSI0_EBOOK_DYNAMIC_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_7), DSI0_EBOOK_DYNAMIC_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_8), DSI0_EBOOK_DYNAMIC_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_9), DSI0_EBOOK_DYNAMIC_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_10), DSI0_EBOOK_DYNAMIC_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_11), DSI0_EBOOK_DYNAMIC_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_12), DSI0_EBOOK_DYNAMIC_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_13), DSI0_EBOOK_DYNAMIC_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_14), DSI0_EBOOK_DYNAMIC_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_DYNAMIC_MDNIE_15), DSI0_EBOOK_DYNAMIC_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_EBOOK_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_1), DSI0_EBOOK_AUTO_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_2), DSI0_EBOOK_AUTO_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_3), DSI0_EBOOK_AUTO_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_4), DSI0_EBOOK_AUTO_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_5), DSI0_EBOOK_AUTO_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_6), DSI0_EBOOK_AUTO_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_7), DSI0_EBOOK_AUTO_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_8), DSI0_EBOOK_AUTO_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_9), DSI0_EBOOK_AUTO_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_10), DSI0_EBOOK_AUTO_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_11), DSI0_EBOOK_AUTO_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_12), DSI0_EBOOK_AUTO_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_13), DSI0_EBOOK_AUTO_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_14), DSI0_EBOOK_AUTO_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EBOOK_AUTO_MDNIE_15), DSI0_EBOOK_AUTO_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_EMAIL_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_1), DSI0_EMAIL_AUTO_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_2), DSI0_EMAIL_AUTO_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_3), DSI0_EMAIL_AUTO_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_4), DSI0_EMAIL_AUTO_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_5), DSI0_EMAIL_AUTO_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_6), DSI0_EMAIL_AUTO_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_7), DSI0_EMAIL_AUTO_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_8), DSI0_EMAIL_AUTO_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_9), DSI0_EMAIL_AUTO_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_10), DSI0_EMAIL_AUTO_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_11), DSI0_EMAIL_AUTO_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_12), DSI0_EMAIL_AUTO_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_13), DSI0_EMAIL_AUTO_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_14), DSI0_EMAIL_AUTO_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_EMAIL_AUTO_MDNIE_15), DSI0_EMAIL_AUTO_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_TDMB_DYNAMIC_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_1), DSI0_TDMB_DYNAMIC_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_2), DSI0_TDMB_DYNAMIC_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_3), DSI0_TDMB_DYNAMIC_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_4), DSI0_TDMB_DYNAMIC_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_5), DSI0_TDMB_DYNAMIC_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_6), DSI0_TDMB_DYNAMIC_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_7), DSI0_TDMB_DYNAMIC_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_8), DSI0_TDMB_DYNAMIC_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_9), DSI0_TDMB_DYNAMIC_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_10), DSI0_TDMB_DYNAMIC_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_11), DSI0_TDMB_DYNAMIC_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_12), DSI0_TDMB_DYNAMIC_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_13), DSI0_TDMB_DYNAMIC_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_14), DSI0_TDMB_DYNAMIC_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_DYNAMIC_MDNIE_15), DSI0_TDMB_DYNAMIC_MDNIE_15, 0, NULL}, false, 0},
};

static struct dsi_cmd_desc DSI0_TDMB_AUTO_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_1), DSI0_TDMB_AUTO_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_2), DSI0_TDMB_AUTO_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_3), DSI0_TDMB_AUTO_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_4), DSI0_TDMB_AUTO_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_5), DSI0_TDMB_AUTO_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_6), DSI0_TDMB_AUTO_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_7), DSI0_TDMB_AUTO_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_8), DSI0_TDMB_AUTO_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_9), DSI0_TDMB_AUTO_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_10), DSI0_TDMB_AUTO_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_11), DSI0_TDMB_AUTO_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_12), DSI0_TDMB_AUTO_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_13), DSI0_TDMB_AUTO_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_14), DSI0_TDMB_AUTO_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_TDMB_AUTO_MDNIE_15), DSI0_TDMB_AUTO_MDNIE_15, 0, NULL}, false, 0},
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
		{NULL,	NULL},
		{NULL,	NULL},
		{DSI0_UI_AUTO_MDNIE,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// VIDEO_APP
	{
		{DSI0_VIDEO_DYNAMIC_MDNIE,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
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
		{NULL,	NULL},
		{NULL,	NULL},
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
		{NULL,	NULL},
		{NULL,	NULL},
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
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{DSI0_BROWSER_AUTO_MDNIE,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// eBOOK_APP
	{
		{DSI0_EBOOK_DYNAMIC_MDNIE,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// EMAIL_APP
	{
		{DSI0_EMAIL_AUTO_MDNIE,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{DSI0_EMAIL_AUTO_MDNIE,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// GAME_LOW_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// GAME_MID_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// GAME_HIGH_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// VIDEO_ENHANCER_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// VIDEO_ENHANCER_THIRD_APP
	{
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{NULL,	NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
	// TDMB_APP
	{
		{DSI0_TDMB_DYNAMIC_MDNIE,       NULL},
		{NULL,      NULL},
		{NULL,       NULL},
		{NULL,         NULL},
		{DSI0_TDMB_AUTO_MDNIE,          NULL},
		{DSI0_EBOOK_AUTO_MDNIE,	NULL},
	},
};

static struct dsi_cmd_desc DSI0_HMT_COLOR_TEMP_6500K_MDNIE[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_1), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_1, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_2), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_2, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_3), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_3, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_4), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_4, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_5), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_5, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_6), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_6, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_7), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_7, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_8), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_8, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_9), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_9, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_10), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_10, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_11), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_11, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_12), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_12, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_13), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_13, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_14), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_14, 0, NULL}, false, 0},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(DSI0_HMT_COLOR_TEMP_6500K_MDNIE_15), DSI0_HMT_COLOR_TEMP_6500K_MDNIE_15, 0, NULL}, false, 0},
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
	DSI0_HMT_COLOR_TEMP_6500K_MDNIE,
	DSI0_HMT_COLOR_TEMP_6500K_MDNIE,
	DSI0_HMT_COLOR_TEMP_6500K_MDNIE,
	DSI0_HMT_COLOR_TEMP_6500K_MDNIE,
	DSI0_HMT_COLOR_TEMP_6500K_MDNIE,
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

#endif
