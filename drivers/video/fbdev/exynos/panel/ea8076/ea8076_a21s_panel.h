/*
 * linux/drivers/video/fbdev/exynos/panel/ea8076/ea8076_a21s_panel.h
 *
 * Header file for EA8076 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8076_A21S_PANEL_H__
#define __EA8076_A21S_PANEL_H__
#include "../panel_drv.h"
#include "ea8076.h"

#ifdef CONFIG_EXYNOS_DECON_MDNIE
#include "ea8076_a21s_panel_mdnie.h"
#endif
#ifdef CONFIG_SUPPORT_AOD_BL
#include "ea8076_a21s_panel_aod_dimming.h"
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "ea8076_a21s_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#include "ea8076_a21s_panel_dimming.h"
#include "ea8076_a21s_resol.h"

#undef __pn_name__
#define __pn_name__	a21s

#undef __PN_NAME__
#define __PN_NAME__	A21S

static struct seqinfo a21s_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [EA8076 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [EA8076 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [EA8076 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a21s_brt_table[EA8076_TOTAL_NR_LUMINANCE + 1][2] = {
	{0x00, 0x03}, {0x00, 0x06}, {0x00, 0x09}, {0x00, 0x0C}, {0x00, 0x0F}, {0x00, 0x12}, {0x00, 0x15}, {0x00, 0x18}, {0x00, 0x1B}, {0x00, 0x1E},
	{0x00, 0x21}, {0x00, 0x24}, {0x00, 0x27}, {0x00, 0x2A}, {0x00, 0x2D}, {0x00, 0x30}, {0x00, 0x34}, {0x00, 0x37}, {0x00, 0x3B}, {0x00, 0x3E},
	{0x00, 0x42}, {0x00, 0x45}, {0x00, 0x49}, {0x00, 0x4D}, {0x00, 0x50}, {0x00, 0x54}, {0x00, 0x57}, {0x00, 0x5B}, {0x00, 0x5F}, {0x00, 0x62},
	{0x00, 0x66}, {0x00, 0x69}, {0x00, 0x6D}, {0x00, 0x70}, {0x00, 0x74}, {0x00, 0x78}, {0x00, 0x7B}, {0x00, 0x7F}, {0x00, 0x82}, {0x00, 0x86},
	{0x00, 0x8A}, {0x00, 0x8D}, {0x00, 0x91}, {0x00, 0x94}, {0x00, 0x98}, {0x00, 0x9C}, {0x00, 0x9F}, {0x00, 0xA3}, {0x00, 0xA6}, {0x00, 0xAA},
	{0x00, 0xAD}, {0x00, 0xB1}, {0x00, 0xB5}, {0x00, 0xB8}, {0x00, 0xBC}, {0x00, 0xBF}, {0x00, 0xC3}, {0x00, 0xC7}, {0x00, 0xCA}, {0x00, 0xCE},
	{0x00, 0xD1}, {0x00, 0xD5}, {0x00, 0xD8}, {0x00, 0xDC}, {0x00, 0xE0}, {0x00, 0xE3}, {0x00, 0xE7}, {0x00, 0xEA}, {0x00, 0xEE}, {0x00, 0xF2},
	{0x00, 0xF5}, {0x00, 0xF9}, {0x00, 0xFC}, {0x01, 0x00}, {0x01, 0x03}, {0x01, 0x07}, {0x01, 0x0B}, {0x01, 0x0E}, {0x01, 0x12}, {0x01, 0x15},
	{0x01, 0x19}, {0x01, 0x1D}, {0x01, 0x20}, {0x01, 0x24}, {0x01, 0x27}, {0x01, 0x2B}, {0x01, 0x2E}, {0x01, 0x32}, {0x01, 0x36}, {0x01, 0x39},
	{0x01, 0x3D}, {0x01, 0x40}, {0x01, 0x44}, {0x01, 0x48}, {0x01, 0x4B}, {0x01, 0x4F}, {0x01, 0x52}, {0x01, 0x56}, {0x01, 0x59}, {0x01, 0x5D},
	{0x01, 0x61}, {0x01, 0x64}, {0x01, 0x68}, {0x01, 0x6B}, {0x01, 0x6F}, {0x01, 0x73}, {0x01, 0x76}, {0x01, 0x7A}, {0x01, 0x7D}, {0x01, 0x81},
	{0x01, 0x84}, {0x01, 0x88}, {0x01, 0x8C}, {0x01, 0x8F}, {0x01, 0x93}, {0x01, 0x96}, {0x01, 0x9A}, {0x01, 0x9E}, {0x01, 0xA1}, {0x01, 0xA5},
	{0x01, 0xA8}, {0x01, 0xAC}, {0x01, 0xAF}, {0x01, 0xB3}, {0x01, 0xB7}, {0x01, 0xBA}, {0x01, 0xBE}, {0x01, 0xC1}, {0x01, 0xC5}, {0x01, 0xC9},
	{0x01, 0xCE}, {0x01, 0xD2}, {0x01, 0xD7}, {0x01, 0xDB}, {0x01, 0xE0}, {0x01, 0xE4}, {0x01, 0xE9}, {0x01, 0xED}, {0x01, 0xF2}, {0x01, 0xF6},
	{0x01, 0xFB}, {0x01, 0xFF}, {0x02, 0x04}, {0x02, 0x08}, {0x02, 0x0D}, {0x02, 0x11}, {0x02, 0x16}, {0x02, 0x1A}, {0x02, 0x1F}, {0x02, 0x23},
	{0x02, 0x28}, {0x02, 0x2C}, {0x02, 0x31}, {0x02, 0x35}, {0x02, 0x3A}, {0x02, 0x3E}, {0x02, 0x43}, {0x02, 0x47}, {0x02, 0x4C}, {0x02, 0x50},
	{0x02, 0x55}, {0x02, 0x59}, {0x02, 0x5E}, {0x02, 0x62}, {0x02, 0x67}, {0x02, 0x6B}, {0x02, 0x70}, {0x02, 0x74}, {0x02, 0x79}, {0x02, 0x7D},
	{0x02, 0x81}, {0x02, 0x86}, {0x02, 0x8A}, {0x02, 0x8F}, {0x02, 0x93}, {0x02, 0x98}, {0x02, 0x9C}, {0x02, 0xA1}, {0x02, 0xA5}, {0x02, 0xAA},
	{0x02, 0xAE}, {0x02, 0xB3}, {0x02, 0xB7}, {0x02, 0xBC}, {0x02, 0xC0}, {0x02, 0xC5}, {0x02, 0xC9}, {0x02, 0xCE}, {0x02, 0xD2}, {0x02, 0xD7},
	{0x02, 0xDB}, {0x02, 0xE0}, {0x02, 0xE4}, {0x02, 0xE9}, {0x02, 0xED}, {0x02, 0xF2}, {0x02, 0xF6}, {0x02, 0xFB}, {0x02, 0xFF}, {0x03, 0x04},
	{0x03, 0x08}, {0x03, 0x0D}, {0x03, 0x11}, {0x03, 0x16}, {0x03, 0x1A}, {0x03, 0x1F}, {0x03, 0x23}, {0x03, 0x28}, {0x03, 0x2C}, {0x03, 0x31},
	{0x03, 0x35}, {0x03, 0x3A}, {0x03, 0x3E}, {0x03, 0x42}, {0x03, 0x47}, {0x03, 0x4B}, {0x03, 0x50}, {0x03, 0x54}, {0x03, 0x59}, {0x03, 0x5D},
	{0x03, 0x62}, {0x03, 0x66}, {0x03, 0x6B}, {0x03, 0x6F}, {0x03, 0x74}, {0x03, 0x78}, {0x03, 0x7D}, {0x03, 0x81}, {0x03, 0x86}, {0x03, 0x8A},
	{0x03, 0x8F}, {0x03, 0x93}, {0x03, 0x98}, {0x03, 0x9C}, {0x03, 0xA1}, {0x03, 0xA5}, {0x03, 0xAA}, {0x03, 0xAE}, {0x03, 0xB3}, {0x03, 0xB7},
	{0x03, 0xBC}, {0x03, 0xC0}, {0x03, 0xC5}, {0x03, 0xC9}, {0x03, 0xCE}, {0x03, 0xD2}, {0x03, 0xD7}, {0x03, 0xDB}, {0x03, 0xE0}, {0x03, 0xE4},
	{0x03, 0xE9}, {0x03, 0xED}, {0x03, 0xF2}, {0x03, 0xF6}, {0x03, 0xFA}, {0x03, 0xFF}, {0x00, 0x03}, {0x00, 0x07}, {0x00, 0x0B}, {0x00, 0x0F},
	{0x00, 0x12}, {0x00, 0x16}, {0x00, 0x1A}, {0x00, 0x1D}, {0x00, 0x21}, {0x00, 0x25}, {0x00, 0x28}, {0x00, 0x2C}, {0x00, 0x30}, {0x00, 0x33},
	{0x00, 0x37}, {0x00, 0x3B}, {0x00, 0x3E}, {0x00, 0x42}, {0x00, 0x46}, {0x00, 0x49}, {0x00, 0x4D}, {0x00, 0x51}, {0x00, 0x54}, {0x00, 0x58},
	{0x00, 0x5C}, {0x00, 0x5F}, {0x00, 0x63}, {0x00, 0x67}, {0x00, 0x6B}, {0x00, 0x6E}, {0x00, 0x72}, {0x00, 0x76}, {0x00, 0x79}, {0x00, 0x7D},
	{0x00, 0x81}, {0x00, 0x84}, {0x00, 0x88}, {0x00, 0x8C}, {0x00, 0x8F}, {0x00, 0x93}, {0x00, 0x97}, {0x00, 0x9A}, {0x00, 0x9E}, {0x00, 0xA2},
	{0x00, 0xA5}, {0x00, 0xA9}, {0x00, 0xAD}, {0x00, 0xB0}, {0x00, 0xB4}, {0x00, 0xB8}, {0x00, 0xBB}, {0x00, 0xBF}, {0x00, 0xC3}, {0x00, 0xC6},
	{0x00, 0xCA}, {0x00, 0xCE}, {0x00, 0xD1}, {0x00, 0xD5}, {0x00, 0xD9}, {0x00, 0xDC}, {0x00, 0xE0}, {0x00, 0xE4}, {0x00, 0xE7}, {0x00, 0xEB},
	{0x00, 0xEF}, {0x00, 0xF2}, {0x00, 0xF6}, {0x00, 0xFA}, {0x00, 0xFD}, {0x01, 0x01}, {0x01, 0x05}, {0x01, 0x08}, {0x01, 0x0C}, {0x01, 0x10},
	{0x01, 0x13}, {0x01, 0x17}, {0x01, 0x1B}, {0x01, 0x1E}, {0x01, 0x22}, {0x01, 0x26}, {0x01, 0x29}, {0x01, 0x2D}, {0x01, 0x31}, {0x01, 0x35},
	{0x01, 0x38}, {0x01, 0x3C}, {0x01, 0x40}, {0x01, 0x43}, {0x01, 0x47}, {0x01, 0x4B}, {0x01, 0x4E}, {0x01, 0x52}, {0x01, 0x56}, {0x01, 0x59},
	{0x01, 0x5D}, {0x01, 0x61}, {0x01, 0x64}, {0x01, 0x68}, {0x01, 0x6C}, {0x01, 0x6F}, {0x01, 0x73}, {0x01, 0x77}, {0x01, 0x7A}, {0x01, 0x7E},
	{0x01, 0x82}, {0x01, 0x85}, {0x01, 0x89}, {0x01, 0x8D}, {0x01, 0x90}, {0x01, 0x94},
};

static u8 ea8076_a21s_elvss_table[EA8076_TOTAL_STEP][1] = {
	/* OVER_ZERO */
	[0 ... 255] = { 0x96 },
	/* HBM */
	[256 ... 256] = { 0x9F },
	[257 ... 268] = { 0x9E },
	[269 ... 282] = { 0x9D },
	[283 ... 295] = { 0x9C },
	[296 ... 309] = { 0x9B },
	[310 ... 323] = { 0x99 },
	[324 ... 336] = { 0x98 },
	[337 ... 350] = { 0x97 },
	[351 ... 365] = { 0x96 },
};

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 A21S_XTALK_ON[] = { 0xD9, 0x60 };		/* VGH 6.2V */
static u8 A21S_XTALK_OFF[] = { 0xD9, 0xC0 };	/* VGH 7.0V */

static u8 A21S_BRIGHTNESS_MIN[] = {
	0x51,
	0x00, 0x03,
};
#endif

#ifdef CONFIG_SUPPORT_FAST_DISCHARGE
static u8 A21S_ASWIRE_OFF[] = {
	0xD5,
	0x82, 0xFF, 0x5C, 0x44, 0xBF, 0x89, 0x00, 0x00, 0x03, 0x01,	/* 10th para 0x01 FD enable */
};
static u8 A21S_ASWIRE_OFF_FD_OFF[] = {
	0xD5,
	0x82, 0xFF, 0x5C, 0x44, 0xBF, 0x89, 0x00, 0x00, 0x03, 0x02,	/* 10th para 0x02 FD Off */
};
#else
static u8 A21S_ASWIRE_OFF[] = {
	0xD5,
	0x82, 0xFF, 0x5C, 0x44, 0xBF, 0x89, 0x00, 0x00, 0x00
};
#endif

static u8 a21s_acl_opr_table[ACL_OPR_MAX][1] = {
	[ACL_OPR_OFF] = { 0x00 }, /* ACL OFF OPR */
	[ACL_OPR_03P ... ACL_OPR_08P] = { 0x01 }, /* ACL ON OPR_8 */
	[ACL_OPR_12P ... ACL_OPR_15P] = { 0x03 }, /* ACL ON OPR_15 */
};

static u8 a21s_fps_table[][1] = {
	[EA8076_VRR_FPS_60] = { 0x00 },
};

static u8 a21s_lpm_nit_table[][4][2] = {
	[ALPM_MODE] = {
		{0x9D, 0xC9}, // ALPM_2NIT
		{0x89, 0xC9}, // ALPM_10NIT
		{0x55, 0xA9}, // ALPM_30NIT
		{0x01, 0x89}, // ALPM_60NIT
	},
	[HLPM_MODE] = {
		{0x9D, 0xC9}, // HLPM_2NIT
		{0x89, 0xC9}, // HLPM_10NIT
		{0x55, 0xA9}, // HLPM_30NIT
		{0x01, 0x89}, // HLPM_60NIT
	}
};

static u8 a21s_lpm_on_table[][4][1] = {
	[ALPM_MODE] = {
		{0x23}, // ALPM_2NIT
		{0x22}, // ALPM_10NIT
		{0x22}, // ALPM_30NIT
		{0x22}, // ALPM_60NIT
	},
	[HLPM_MODE] = {
		{0x23}, // HLPM_2NIT
		{0x22}, // HLPM_10NIT
		{0x22}, // HLPM_30NIT
		{0x22}, // HLPM_60NIT
	}
};

static u8 a21s_brt_mode_table[SMOOTH_TRANS_MAX][2][1] = {
	[SMOOTH_TRANS_OFF] = {
		{ 0x20 }, /* Normal direct */
		{ 0xE0 }, /* HBM direct */
	},
	[SMOOTH_TRANS_ON] = {
		{ 0x28 }, /* Normal smooth */
		{ 0xE8 }, /* HBM smooth */
	},
};

static struct maptbl a21s_maptbl[MAX_MAPTBL] = {
	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(a21s_tset_table, init_common_table, NULL, copy_tset_maptbl),
	[ELVSS_MAPTBL] = DEFINE_2D_MAPTBL(ea8076_a21s_elvss_table, init_elvss_table, getidx_brt_table, copy_common_maptbl),
	[ELVSS_OTP_MAPTBL] = DEFINE_0D_MAPTBL(ea8076_a21s_elvss_otp_table, init_common_table, getidx_brt_table, copy_elvss_otp_maptbl),
	[ELVSS_TEMP_MAPTBL] = DEFINE_0D_MAPTBL(ea8076_a21s_elvss_temp_table, init_common_table, getidx_brt_table, copy_tset_maptbl),
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(a21s_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
	[BRT_MODE_MAPTBL] = DEFINE_3D_MAPTBL(a21s_brt_mode_table, init_common_table, getidx_brt_mode_table, copy_common_maptbl),
	[ACL_OPR_MAPTBL] = DEFINE_2D_MAPTBL(a21s_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),
	[FPS_MAPTBL] = DEFINE_2D_MAPTBL(a21s_fps_table, init_common_table, getidx_fps_table, copy_common_maptbl),
	[LPM_NIT_MAPTBL] = DEFINE_3D_MAPTBL(a21s_lpm_nit_table, init_lpm_table, getidx_lpm_table, copy_common_maptbl),
	[LPM_ON_MAPTBL] = DEFINE_3D_MAPTBL(a21s_lpm_on_table, init_common_table, getidx_lpm_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [EA8076 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A21S_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 A21S_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 A21S_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 A21S_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 A21S_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 A21S_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 A21S_SLEEP_OUT[] = { 0x11 };
static u8 A21S_SLEEP_IN[] = { 0x10 };
static u8 A21S_DISPLAY_OFF[] = { 0x28 };
static u8 A21S_DISPLAY_ON[] = { 0x29 };

static u8 A21S_TE_ON[] = { 0x35, 0x00, 0x00 };
static u8 A21S_TE_OFF[] = { 0x34 };

static u8 A21S_PAGE_ADDR_SET_2A[] = {
	0x2A,
	0x00, 0x00, 0x04, 0x37,
};

static u8 A21S_DDI_ERR_DET_IGNORE[] = {
	0xDD,
	0x4A, 0x00
};

static u8 A21S_PAGE_ADDR_SET_2B[] = {
	0x2B,
	0x00, 0x00,
	0x09, 0x23
};

static u8 A21S_TSP_VSYNC_ON[] = {
	0xE0,
	0x01,
};

static u8 A21S_FFC_SET[] = {
	0xE9,
	0x11, 0x55, 0x98, 0x96, 0x80,
	0xB2, 0x41, 0xC3, 0x00, 0x1A,
	0xB8  /* MIPI Speed 1.2Gbps */
};

static u8 A21S_ERR_FG_SET[] = {
	0xE1,
	0x00, 0x00, 0x02,
	0x10, 0x10, 0x10,	/* DET_POL_VGH, ESD_FG_VGH, DET_EN_VGH */
	0x00, 0x00, 0x20, 0x00,
	0x00, 0x01, 0x19
};

static u8 A21S_PCD_SET_DET_LOW[] = {
	0xE3,
	0x57, 0x12	/* 1st 0x57: default high, 2nd 0x12: Disable SW RESET */
};

static u8 A21S_SAP_SET[] = { 0xD4, 0x40 };

static u8 A21S_ELVSS_SET[] = {
	0xB7,
	0x01, 0x53, 0x28, 0x4D, 0x00,
	0x96,	/* 6th: ELVSS return */
	0x04,	/* 7th para : Smooth transition 4-frame */
	0x00,	/* 8th: Normal ELVSS OTP */
	0x00,	/* 9th: HBM ELVSS OTP */
	0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x42,
	0x43, 0x43, 0x43, 0x43, 0x43, 0x83, 0xC3, 0x83,
	0xC3, 0x83, 0xC3, 0x23, 0x03, 0x03, 0x03, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00,	/* 46th: TSET */
};

static u8 A21S_BRIGHTNESS[] = {
	0x51,
	0x01, 0xC7,
};

static u8 A21S_BRIGHTNESS_MODE[] = {
	0x53,
	0x28,	/* Normal Smooth */
};

static u8 A21S_ACL_SET_01[] = {
	0xB9,
	0x55, 0x27, 0x65,
};

static u8 A21S_ACL_SET_02[] = {
	0xB9,
	0x02, 0x61, 0x24, 0x49, 0x41, 0xFF, 0x00,
};

static u8 A21S_EXIT_ALPM_NORMAL[] = { 0x53, 0x20 };
static u8 A21S_ACL_CONTROL[] = { 0x55, 0x00 };

static u8 A21S_LPM_MODE[] = { 0xC7, 0x00 };	/* HLPM */
static u8 A21S_LPM_AOR[] =  { 0xB9, 0x01, 0x89 };	/* HLPM 60nit */
static u8 A21S_LPM_ON[] = { 0x53, 0x22 };	/* HLPM 60nit */

static DEFINE_STATIC_PACKET(a21s_level1_key_enable, DSI_PKT_TYPE_WR, A21S_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(a21s_level2_key_enable, DSI_PKT_TYPE_WR, A21S_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(a21s_level3_key_enable, DSI_PKT_TYPE_WR, A21S_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(a21s_level1_key_disable, DSI_PKT_TYPE_WR, A21S_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(a21s_level2_key_disable, DSI_PKT_TYPE_WR, A21S_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(a21s_level3_key_disable, DSI_PKT_TYPE_WR, A21S_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(a21s_sleep_out, DSI_PKT_TYPE_WR, A21S_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a21s_sleep_in, DSI_PKT_TYPE_WR, A21S_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a21s_display_on, DSI_PKT_TYPE_WR, A21S_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a21s_display_off, DSI_PKT_TYPE_WR, A21S_DISPLAY_OFF, 0);

static DEFINE_STATIC_PACKET(a21s_exit_alpm_normal, DSI_PKT_TYPE_WR, A21S_EXIT_ALPM_NORMAL, 0);

static DEFINE_STATIC_PACKET(a21s_te_off, DSI_PKT_TYPE_WR, A21S_TE_OFF, 0);
static DEFINE_STATIC_PACKET(a21s_te_on, DSI_PKT_TYPE_WR, A21S_TE_ON, 0);
static DEFINE_STATIC_PACKET(a21s_ddi_err_det_ignore, DSI_PKT_TYPE_WR, A21S_DDI_ERR_DET_IGNORE, 0);

static DEFINE_STATIC_PACKET(a21s_page_set_2a, DSI_PKT_TYPE_WR, A21S_PAGE_ADDR_SET_2A, 0);
static DEFINE_STATIC_PACKET(a21s_page_set_2b, DSI_PKT_TYPE_WR, A21S_PAGE_ADDR_SET_2B, 0);
static DEFINE_STATIC_PACKET(a21s_tsp_vsync, DSI_PKT_TYPE_WR, A21S_TSP_VSYNC_ON, 0);
static DEFINE_STATIC_PACKET(a21s_ffc_set, DSI_PKT_TYPE_WR, A21S_FFC_SET, 0);
static DEFINE_STATIC_PACKET(a21s_err_fg_set, DSI_PKT_TYPE_WR, A21S_ERR_FG_SET, 0);
static DEFINE_STATIC_PACKET(a21s_pcd_det_set, DSI_PKT_TYPE_WR, A21S_PCD_SET_DET_LOW, 0);
static DEFINE_STATIC_PACKET(a21s_sap_set, DSI_PKT_TYPE_WR, A21S_SAP_SET, 0x03);
static DEFINE_STATIC_PACKET(a21s_acl_set_01, DSI_PKT_TYPE_WR, A21S_ACL_SET_01, 0xCC);
static DEFINE_STATIC_PACKET(a21s_acl_set_02, DSI_PKT_TYPE_WR, A21S_ACL_SET_02, 0xD7);

static DEFINE_STATIC_PACKET(a21s_lpm_mode, DSI_PKT_TYPE_WR, A21S_LPM_MODE, 0xA3);
static DEFINE_PKTUI(a21s_lpm_nit, &a21s_maptbl[LPM_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a21s_lpm_nit, DSI_PKT_TYPE_WR, A21S_LPM_AOR, 0x68);
static DEFINE_PKTUI(a21s_lpm_on, &a21s_maptbl[LPM_ON_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a21s_lpm_on, DSI_PKT_TYPE_WR, A21S_LPM_ON, 0);


static DECLARE_PKTUI(a21s_elvss_set) = {
	{ .offset = 6, .maptbl = &a21s_maptbl[ELVSS_MAPTBL] },
	{ .offset = 8, .maptbl = &a21s_maptbl[ELVSS_OTP_MAPTBL] },
	{ .offset = 46, .maptbl = &a21s_maptbl[ELVSS_TEMP_MAPTBL] },
};

static DEFINE_VARIABLE_PACKET(a21s_elvss_set, DSI_PKT_TYPE_WR, A21S_ELVSS_SET, 0);
static DEFINE_PKTUI(a21s_brightness, &a21s_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a21s_brightness, DSI_PKT_TYPE_WR, A21S_BRIGHTNESS, 0);
static DEFINE_PKTUI(a21s_brightness_mode, &a21s_maptbl[BRT_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a21s_brightness_mode, DSI_PKT_TYPE_WR, A21S_BRIGHTNESS_MODE, 0);

static DECLARE_PKTUI(a21s_acl_control) = {
	{ .offset = 1, .maptbl = &a21s_maptbl[ACL_OPR_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(a21s_acl_control, DSI_PKT_TYPE_WR, A21S_ACL_CONTROL, 0);

#ifdef CONFIG_SUPPORT_XTALK_MODE
static DEFINE_STATIC_PACKET(a21s_xtalk_on, DSI_PKT_TYPE_WR, A21S_XTALK_ON, 0x1C);
static DEFINE_STATIC_PACKET(a21s_xtalk_off, DSI_PKT_TYPE_WR, A21S_XTALK_OFF, 0x1C);
static DEFINE_STATIC_PACKET(a21s_brightness_min, DSI_PKT_TYPE_WR, A21S_BRIGHTNESS_MIN, 0);
#endif
#ifdef CONFIG_SUPPORT_FAST_DISCHARGE
static DEFINE_STATIC_PACKET(a21s_fd_on, DSI_PKT_TYPE_WR, A21S_ASWIRE_OFF, 0);
static DEFINE_STATIC_PACKET(a21s_fd_off, DSI_PKT_TYPE_WR, A21S_ASWIRE_OFF_FD_OFF, 0);
#else
static DEFINE_STATIC_PACKET(a21s_aswire_off, DSI_PKT_TYPE_WR, A21S_ASWIRE_OFF, 0);
#endif

static DEFINE_PANEL_MDELAY(a21s_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(a21s_wait_2msec, 2);
static DEFINE_PANEL_MDELAY(a21s_wait_5msec, 5);
static DEFINE_PANEL_MDELAY(a21s_wait_110msec, 110);
static DEFINE_PANEL_MDELAY(a21s_wait_sleep_out, 10);
static DEFINE_PANEL_MDELAY(a21s_wait_afc_off, 20);
static DEFINE_PANEL_MDELAY(a21s_wait_sleep_in, 120);
static DEFINE_PANEL_UDELAY(a21s_wait_1_frame_in_60hz, 16700);
static DEFINE_PANEL_UDELAY(a21s_wait_1_frame_in_30hz, 33400);
static DEFINE_PANEL_UDELAY(a21s_wait_10usec, 10);
static DEFINE_PANEL_MDELAY(a21s_wait_100msec, 100);
static DEFINE_PANEL_WAIT_VSYNC(a21s_wait_te_start, 1);
static DEFINE_PANEL_WAIT_VSYNC(a21s_wait_for_2_frames, 2);

static DEFINE_PANEL_KEY(a21s_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(a21s_level1_key_enable));
static DEFINE_PANEL_KEY(a21s_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(a21s_level2_key_enable));
static DEFINE_PANEL_KEY(a21s_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(a21s_level3_key_enable));
static DEFINE_PANEL_KEY(a21s_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(a21s_level1_key_disable));
static DEFINE_PANEL_KEY(a21s_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(a21s_level2_key_disable));
static DEFINE_PANEL_KEY(a21s_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(a21s_level3_key_disable));

static void *a21s_init_cmdtbl[] = {
	&PKTINFO(a21s_sleep_out),
	&DLYINFO(a21s_wait_sleep_out),
	&KEYINFO(a21s_level2_key_enable),
	&KEYINFO(a21s_level3_key_enable),

	&PKTINFO(a21s_ddi_err_det_ignore),

	&DLYINFO(a21s_wait_sleep_out),
	&DLYINFO(a21s_wait_sleep_out),

	&PKTINFO(a21s_page_set_2a),
	&PKTINFO(a21s_page_set_2b),
	&PKTINFO(a21s_ffc_set),
//	&PKTINFO(a21s_err_fg_set),
	&PKTINFO(a21s_tsp_vsync),
#ifdef CONFIG_SUPPORT_FAST_DISCHARGE
	&SEQINFO(a21s_seqtbl[PANEL_FD_ON_SEQ]),
#endif
	&PKTINFO(a21s_pcd_det_set),
//	&PKTINFO(a21s_sap_set),
//	&PKTINFO(a21s_acl_set_01),
//	&PKTINFO(a21s_acl_set_02),
	&PKTINFO(a21s_exit_alpm_normal),
//	&PKTINFO(a21s_elvss_set),
	&PKTINFO(a21s_brightness),
	&PKTINFO(a21s_acl_control),
	&KEYINFO(a21s_level2_key_disable),
	&KEYINFO(a21s_level3_key_disable),
	&KEYINFO(a21s_level2_key_enable),
	&PKTINFO(a21s_te_on),
	&KEYINFO(a21s_level2_key_disable),
	&DLYINFO(a21s_wait_110msec),
	&PKTINFO(a21s_display_on),
};

static void *a21s_res_init_cmdtbl[] = {
	&KEYINFO(a21s_level2_key_enable),
	&KEYINFO(a21s_level3_key_enable),
	&ea8076_restbl[RES_ID],
	&ea8076_restbl[RES_COORDINATE],
	&ea8076_restbl[RES_CODE],
	&ea8076_restbl[RES_ELVSS],
	&ea8076_restbl[RES_DATE],
	&ea8076_restbl[RES_CELL_ID],
	&ea8076_restbl[RES_MANUFACTURE_INFO],
#ifdef CONFIG_DISPLAY_USE_INFO
	&ea8076_restbl[RES_ERR_FG],
	&ea8076_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(a21s_level3_key_disable),
	&KEYINFO(a21s_level2_key_disable),
};

static void *a21s_set_bl_cmdtbl[] = {
	&KEYINFO(a21s_level2_key_enable),
	&PKTINFO(a21s_brightness_mode),
//	&PKTINFO(a21s_elvss_set),
	&PKTINFO(a21s_brightness), //51h
	&PKTINFO(a21s_acl_control),
	&KEYINFO(a21s_level2_key_disable),
};

static void *a21s_display_on_cmdtbl[] = {
	&PKTINFO(a21s_display_on),
};

static void *a21s_display_off_cmdtbl[] = {
	&PKTINFO(a21s_display_off),
};

static void *a21s_exit_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_AFC
	&SEQINFO(a21s_mdnie_seqtbl[MDNIE_AFC_OFF_SEQ]),
	&DLYINFO(a21s_wait_afc_off),
#endif
#ifdef CONFIG_DISPLAY_USE_INFO
	&KEYINFO(a21s_level2_key_enable),
	&ea8076_dmptbl[DUMP_ERR_FG],
	&KEYINFO(a21s_level2_key_disable),
	&ea8076_dmptbl[DUMP_DSI_ERR],
#endif
	&PKTINFO(a21s_sleep_in),
	&DLYINFO(a21s_wait_sleep_in),
};

static void *a21s_alpm_enter_cmdtbl[] = {
	&KEYINFO(a21s_level2_key_enable),
	&PKTINFO(a21s_lpm_mode),
	&PKTINFO(a21s_lpm_nit),
	&PKTINFO(a21s_lpm_on),
	&DLYINFO(a21s_wait_10usec),
	&KEYINFO(a21s_level2_key_disable),
};

static void *a21s_alpm_exit_cmdtbl[] = {
	&KEYINFO(a21s_level2_key_enable),
	&PKTINFO(a21s_aswire_off),
	&PKTINFO(a21s_exit_alpm_normal),
	&DLYINFO(a21s_wait_10usec),
	&KEYINFO(a21s_level2_key_disable),
};

static void *a21s_set_fps_cmdtbl[] = {
	&SEQINFO(a21s_seqtbl[PANEL_SET_BL_SEQ]),
};

static void *a21s_dump_cmdtbl[] = {
	&KEYINFO(a21s_level1_key_enable),
	&KEYINFO(a21s_level2_key_enable),
	&KEYINFO(a21s_level3_key_enable),
	&ea8076_dmptbl[DUMP_RDDPM],
	&ea8076_dmptbl[DUMP_RDDSM],
	&ea8076_dmptbl[DUMP_ERR],
	&ea8076_dmptbl[DUMP_ERR_FG],
	&ea8076_dmptbl[DUMP_DSI_ERR],
	&KEYINFO(a21s_level3_key_disable),
	&KEYINFO(a21s_level2_key_disable),
	&KEYINFO(a21s_level1_key_disable),
};

static void *a21s_indisplay_enter_before_cmdtbl[] = {
	&VSYNCINFO(a21s_wait_te_start),
	&DLYINFO(a21s_wait_2msec),
};

static void *a21s_indisplay_enter_after_cmdtbl[] = {
	&VSYNCINFO(a21s_wait_for_2_frames),
};

#ifdef CONFIG_SUPPORT_XTALK_MODE
static void *a21s_xtalk_on_cmdtbl[] = {
	&KEYINFO(a21s_level2_key_enable),
	&PKTINFO(a21s_brightness_min),
	&PKTINFO(a21s_xtalk_on),
	&KEYINFO(a21s_level2_key_disable),
};

static void *a21s_xtalk_off_cmdtbl[] = {
	&KEYINFO(a21s_level2_key_enable),
	&PKTINFO(a21s_brightness),
	&PKTINFO(a21s_xtalk_off),
	&KEYINFO(a21s_level2_key_disable),
};
#endif
#ifdef CONFIG_SUPPORT_FAST_DISCHARGE
static void *a21s_fd_on_cmdtbl[] = {
	&KEYINFO(a21s_level2_key_enable),
	&PKTINFO(a21s_fd_on),
	&KEYINFO(a21s_level2_key_disable),
};

static void *a21s_fd_off_cmdtbl[] = {
	&KEYINFO(a21s_level2_key_enable),
	&PKTINFO(a21s_fd_off),
	&KEYINFO(a21s_level2_key_disable),
};
#endif

static struct seqinfo a21s_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a21s_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a21s_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a21s_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a21s_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a21s_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a21s_exit_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", a21s_alpm_enter_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", a21s_alpm_exit_cmdtbl),
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", a21s_set_fps_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", a21s_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_INDISPLAY
	[PANEL_INDISPLAY_ENTER_BEFORE_SEQ] = SEQINFO_INIT("indisplay-delay", a21s_indisplay_enter_before_cmdtbl),
	[PANEL_INDISPLAY_ENTER_AFTER_SEQ] = SEQINFO_INIT("indisplay-delay", a21s_indisplay_enter_after_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
	[PANEL_XTALK_ON_SEQ] = SEQINFO_INIT("xtalk-on-seq", a21s_xtalk_on_cmdtbl),
	[PANEL_XTALK_OFF_SEQ] = SEQINFO_INIT("xtalk-off-seq", a21s_xtalk_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_FAST_DISCHARGE
	[PANEL_FD_ON_SEQ] = SEQINFO_INIT("fd-on-seq", a21s_fd_on_cmdtbl),
	[PANEL_FD_OFF_SEQ] = SEQINFO_INIT("fd-off-seq", a21s_fd_off_cmdtbl),
#endif
};

struct common_panel_info ea8076_a21s_default_panel_info = {
	.ldi_name = "ea8076",
	.name = "ea8076_a21s_default",
	.model = "AMS646UJ09",
	.vendor = "SDC",
	.id = 0x814C00,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA),
		.err_fg_recovery = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(ea8076_a21s_resol),
		.resol = ea8076_a21s_resol,
	},
	.maptbl = a21s_maptbl,
	.nr_maptbl = ARRAY_SIZE(a21s_maptbl),
	.seqtbl = a21s_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a21s_seqtbl),
	.rditbl = ea8076_rditbl,
	.nr_rditbl = ARRAY_SIZE(ea8076_rditbl),
	.restbl = ea8076_restbl,
	.nr_restbl = ARRAY_SIZE(ea8076_restbl),
	.dumpinfo = ea8076_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(ea8076_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE
	.mdnie_tune = &ea8076_a21s_mdnie_tune,
#endif
	.panel_dim_info = {
		&ea8076_a21s_panel_dimming_info,
		NULL,
#ifdef CONFIG_SUPPORT_AOD_BL
		&ea8076_a21s_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &ea8076_a21s_aod,
#endif
};

static int __init ea8076_a21s_panel_init(void)
{
	register_common_panel(&ea8076_a21s_default_panel_info);

	return 0;
}
arch_initcall(ea8076_a21s_panel_init)
#endif /* __EA8076_A21S_PANEL_H__ */
