/*
 * linux/drivers/video/fbdev/exynos/panel/ana6705/ana6705_a71x_panel.h
 *
 * Header file for ANA6705 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ANA6705_A71X_PANEL_H__
#define __ANA6705_A71X_PANEL_H__
#include "../panel_drv.h"
#include "ana6705.h"

#ifdef CONFIG_EXYNOS_DECON_MDNIE
#include "ana6705_a71x_panel_mdnie.h"
#endif

#include "ana6705_a71x_panel_dimming.h"
#include "ana6705_a71x_resol.h"

#undef __pn_name__
#define __pn_name__	a71x

#undef __PN_NAME__
#define __PN_NAME__	A71X

static struct seqinfo a71x_seqtbl[MAX_PANEL_SEQ];

/* ===================================================================================== */
/* ============================= [ana6705 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [ana6705 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [ana6705 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a71x_brt_table[ANA6705_TOTAL_NR_LUMINANCE + 1][2] = {
	{0x00, 0x03},	{0x00, 0x06},	{0x00, 0x09},	{0x00, 0x0C},	{0x00, 0x0F},	{0x00, 0x12},	{0x00, 0x15},	{0x00, 0x18},	{0x00, 0x1B},	{0x00, 0x1E},
	{0x00, 0x21},	{0x00, 0x24},	{0x00, 0x27},	{0x00, 0x2A},	{0x00, 0x2D},	{0x00, 0x30},	{0x00, 0x35},	{0x00, 0x38},	{0x00, 0x3C},	{0x00, 0x3F},
	{0x00, 0x43},	{0x00, 0x46},	{0x00, 0x4A},	{0x00, 0x4D},	{0x00, 0x51},	{0x00, 0x54},	{0x00, 0x58},	{0x00, 0x5B},	{0x00, 0x5F},	{0x00, 0x62},
	{0x00, 0x66},	{0x00, 0x69},	{0x00, 0x6D},	{0x00, 0x70},	{0x00, 0x74},	{0x00, 0x78},	{0x00, 0x7B},	{0x00, 0x7F},	{0x00, 0x82},	{0x00, 0x86},
	{0x00, 0x89},	{0x00, 0x8D},	{0x00, 0x90},	{0x00, 0x94},	{0x00, 0x97},	{0x00, 0x9B},	{0x00, 0x9E},	{0x00, 0xA2},	{0x00, 0xA5},	{0x00, 0xA9},
	{0x00, 0xAC},	{0x00, 0xB0},	{0x00, 0xB3},	{0x00, 0xB7},	{0x00, 0xBA},	{0x00, 0xBE},	{0x00, 0xC1},	{0x00, 0xC5},	{0x00, 0xC8},	{0x00, 0xCC},
	{0x00, 0xCF},	{0x00, 0xD3},	{0x00, 0xD6},	{0x00, 0xDA},	{0x00, 0xDD},	{0x00, 0xE1},	{0x00, 0xE4},	{0x00, 0xE8},	{0x00, 0xEB},	{0x00, 0xEF},
	{0x00, 0xF2},	{0x00, 0xF6},	{0x00, 0xF9},	{0x00, 0xFD},	{0x01, 0x01},	{0x01, 0x04},	{0x01, 0x08},	{0x01, 0x0B},	{0x01, 0x0F},	{0x01, 0x12},
	{0x01, 0x16},	{0x01, 0x19},	{0x01, 0x1D},	{0x01, 0x20},	{0x01, 0x24},	{0x01, 0x27},	{0x01, 0x2B},	{0x01, 0x2E},	{0x01, 0x32},	{0x01, 0x35},
	{0x01, 0x39},	{0x01, 0x3C},	{0x01, 0x40},	{0x01, 0x43},	{0x01, 0x47},	{0x01, 0x4A},	{0x01, 0x4E},	{0x01, 0x51},	{0x01, 0x55},	{0x01, 0x58},
	{0x01, 0x5C},	{0x01, 0x5F},	{0x01, 0x63},	{0x01, 0x66},	{0x01, 0x6A},	{0x01, 0x6D},	{0x01, 0x71},	{0x01, 0x74},	{0x01, 0x78},	{0x01, 0x7B},
	{0x01, 0x7F},	{0x01, 0x83},	{0x01, 0x86},	{0x01, 0x8A},	{0x01, 0x8D},	{0x01, 0x91},	{0x01, 0x94},	{0x01, 0x98},	{0x01, 0x9B},	{0x01, 0x9F},
	{0x01, 0xA2},	{0x01, 0xA6},	{0x01, 0xA9},	{0x01, 0xAD},	{0x01, 0xB0},	{0x01, 0xB4},	{0x01, 0xB7},	{0x01, 0xBB},	{0x01, 0xBD},	{0x01, 0xC3},
	{0x01, 0xC8},	{0x01, 0xCC},	{0x01, 0xD1},	{0x01, 0xD6},	{0x01, 0xDA},	{0x01, 0xDF},	{0x01, 0xE3},	{0x01, 0xE8},	{0x01, 0xEC},	{0x01, 0xF1},
	{0x01, 0xF4},	{0x01, 0xFA},	{0x01, 0xFE},	{0x02, 0x03},	{0x02, 0x08},	{0x02, 0x0C},	{0x02, 0x11},	{0x02, 0x15},	{0x02, 0x1A},	{0x02, 0x1E},
	{0x02, 0x23},	{0x02, 0x27},	{0x02, 0x2C},	{0x02, 0x31},	{0x02, 0x35},	{0x02, 0x3A},	{0x02, 0x3E},	{0x02, 0x43},	{0x02, 0x47},	{0x02, 0x4C},
	{0x02, 0x50},	{0x02, 0x55},	{0x02, 0x59},	{0x02, 0x5E},	{0x02, 0x63},	{0x02, 0x67},	{0x02, 0x6C},	{0x02, 0x70},	{0x02, 0x75},	{0x02, 0x79},
	{0x02, 0x7E},	{0x02, 0x82},	{0x02, 0x87},	{0x02, 0x8C},	{0x02, 0x90},	{0x02, 0x95},	{0x02, 0x99},	{0x02, 0x9E},	{0x02, 0xA2},	{0x02, 0xA7},
	{0x02, 0xAB},	{0x02, 0xB0},	{0x02, 0xB5},	{0x02, 0xB9},	{0x02, 0xBE},	{0x02, 0xC2},	{0x02, 0xC7},	{0x02, 0xCB},	{0x02, 0xD0},	{0x02, 0xD4},
	{0x02, 0xD9},	{0x02, 0xDD},	{0x02, 0xE2},	{0x02, 0xE7},	{0x02, 0xEB},	{0x02, 0xF0},	{0x02, 0xF4},	{0x02, 0xF9},	{0x02, 0xFD},	{0x03, 0x02},
	{0x03, 0x06},	{0x03, 0x0B},	{0x03, 0x10},	{0x03, 0x14},	{0x03, 0x19},	{0x03, 0x1D},	{0x03, 0x22},	{0x03, 0x26},	{0x03, 0x2B},	{0x03, 0x2F},
	{0x03, 0x34},	{0x03, 0x39},	{0x03, 0x3D},	{0x03, 0x42},	{0x03, 0x46},	{0x03, 0x4B},	{0x03, 0x4F},	{0x03, 0x54},	{0x03, 0x58},	{0x03, 0x5D},
	{0x03, 0x61},	{0x03, 0x66},	{0x03, 0x6B},	{0x03, 0x6F},	{0x03, 0x74},	{0x03, 0x78},	{0x03, 0x7D},	{0x03, 0x81},	{0x03, 0x86},	{0x03, 0x8A},
	{0x03, 0x8F},	{0x03, 0x94},	{0x03, 0x98},	{0x03, 0x9D},	{0x03, 0xA1},	{0x03, 0xA6},	{0x03, 0xAA},	{0x03, 0xAF},	{0x03, 0xB3},	{0x03, 0xB8},
	{0x03, 0xBC},	{0x03, 0xC1},	{0x03, 0xC6},	{0x03, 0xCA},	{0x03, 0xCF},	{0x03, 0xD3},	{0x03, 0xD8},	{0x03, 0xDC},	{0x03, 0xE1},	{0x03, 0xE5},
	{0x03, 0xEA},	{0x03, 0xEF},	{0x03, 0xF3},	{0x03, 0xF8},	{0x03, 0xFC},	{0x03, 0xFF},	{0x00, 0x05},	{0x00, 0x09},	{0x00, 0x0C},	{0x00, 0x10},
	{0x00, 0x14},	{0x00, 0x17},	{0x00, 0x1B},	{0x00, 0x1F},	{0x00, 0x22},	{0x00, 0x26},	{0x00, 0x2A},	{0x00, 0x2D},	{0x00, 0x31},	{0x00, 0x35},
	{0x00, 0x37},	{0x00, 0x3C},	{0x00, 0x40},	{0x00, 0x43},	{0x00, 0x47},	{0x00, 0x4B},	{0x00, 0x4E},	{0x00, 0x52},	{0x00, 0x56},	{0x00, 0x59},
	{0x00, 0x5D},	{0x00, 0x61},	{0x00, 0x64},	{0x00, 0x68},	{0x00, 0x6C},	{0x00, 0x6F},	{0x00, 0x73},	{0x00, 0x77},	{0x00, 0x7B},	{0x00, 0x7E},
	{0x00, 0x82},	{0x00, 0x86},	{0x00, 0x89},	{0x00, 0x8D},	{0x00, 0x91},	{0x00, 0x94},	{0x00, 0x98},	{0x00, 0x9C},	{0x00, 0x9F},	{0x00, 0xA3},
	{0x00, 0xA7},	{0x00, 0xAA},	{0x00, 0xAE},	{0x00, 0xB2},	{0x00, 0xB5},	{0x00, 0xB9},	{0x00, 0xBD},	{0x00, 0xC0},	{0x00, 0xC4},	{0x00, 0xC8},
	{0x00, 0xCB},	{0x00, 0xCF},	{0x00, 0xD3},	{0x00, 0xD6},	{0x00, 0xDA},	{0x00, 0xDE},	{0x00, 0xE1},	{0x00, 0xE5},	{0x00, 0xE9},	{0x00, 0xEC},
	{0x00, 0xF0},	{0x00, 0xF4},	{0x00, 0xF7},	{0x00, 0xFB},	{0x00, 0xFF},	{0x01, 0x02},	{0x01, 0x06},	{0x01, 0x0A},	{0x01, 0x0D},	{0x01, 0x11},
	{0x01, 0x15},	{0x01, 0x18},	{0x01, 0x1C},	{0x01, 0x20},	{0x01, 0x23},	{0x01, 0x27},	{0x01, 0x2B},	{0x01, 0x2E},	{0x01, 0x32},	{0x01, 0x36},
	{0x01, 0x39},	{0x01, 0x3D},	{0x01, 0x41},	{0x01, 0x45},	{0x01, 0x48},	{0x01, 0x4C},	{0x01, 0x50},	{0x01, 0x53},	{0x01, 0x57},	{0x01, 0x5B},
	{0x01, 0x5E},	{0x01, 0x62},	{0x01, 0x66},	{0x01, 0x69},	{0x01, 0x6D},	{0x01, 0x71},	{0x01, 0x74},	{0x01, 0x78},	{0x01, 0x7C},	{0x01, 0x7F},
	{0x01, 0x83},	{0x01, 0x87},	{0x01, 0x8A},	{0x01, 0x8E},	{0x01, 0x92},	{0x01, 0x94},
};

static u8 ana6705_a71x_elvss_table[S6E3HA9_TOTAL_STEP][1] = {
		/* OVER_ZERO */
		[0 ... 255] = { 0x16 },
		/* HBM */
		[256 ... 268] = { 0x16 },
		[269 ... 281] = { 0x1F },
		[282 ... 295] = { 0x1C }, /* { 0x1D }, */
		[296 ... 308] = { 0x1C },
		[309 ... 322] = { 0x1B },
		[323 ... 336] = { 0x19 },
		[337 ... 349] = { 0x18 },
		[350 ... 364] = { 0x17 },
		[365] = { 0x16 },
};

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 a71x_vgh_table[][1] = {
	{ 0xEB }, /* VGH 7.0V */
	{ 0x6B }, /* VGH 6.2V */
};
#endif

static u8 a71x_acl_opr_table[ACL_OPR_MAX][1] = {
	[ACL_OPR_OFF] = { 0x00 }, /* ACL OFF OPR */
	[ACL_OPR_03P ... ACL_OPR_08P] = { 0x01 }, /* ACL ON OPR_8 */
	[ACL_OPR_12P ... ACL_OPR_15P] = { 0x02 }, /* ACL ON OPR_15 */
};

static u8 a71x_fps_table[][1] = {
	[ANA6705_VRR_FPS_60] = { 0x00 },
};

static u8 a71x_lpm_mode_table[][4][1] = {
	[ALPM_MODE] = {
		{0x88}, // ALPM_2NIT
		{0x88}, // ALPM_10NIT
		{0x88}, // ALPM_30NIT
		{0x88}, // ALPM_60NIT
	},
	[HLPM_MODE] = {
		{0x08}, // HLPM_2NIT
		{0x08}, // HLPM_10NIT
		{0x08}, // HLPM_30NIT
		{0x08}, // HLPM_60NIT
	}
};

static u8 a71x_lpm_nit_table[][4][2] = {
	[ALPM_MODE] = {
		{0x08, 0xCA}, // ALPM_2NIT
		{0x08, 0xCA}, // ALPM_10NIT
		{0x05, 0x6C}, // ALPM_30NIT
		{0x00, 0x18}, // ALPM_60NIT
	},
	[HLPM_MODE] = {
		{0x08, 0xCA}, // HLPM_2NIT
		{0x08, 0xCA}, // HLPM_10NIT
		{0x05, 0x6C}, // HLPM_30NIT
		{0x00, 0x18}, // HLPM_60NIT
	}
};

static u8 a71x_lpm_on_table[][4][1] = {
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

static u8 a71x_brt_mode_table[SMOOTH_TRANS_MAX][2][1] = {
	[SMOOTH_TRANS_OFF] = {
		{ 0x20 }, /* Normal direct */
		{ 0xE0 }, /* HBM direct */
	},
	[SMOOTH_TRANS_ON] = {
		{ 0x28 }, /* Normal smooth */
		{ 0xE8 }, /* HBM smooth */
	},
};

static struct maptbl a71x_maptbl[MAX_MAPTBL] = {
	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(a71x_tset_table, init_common_table, NULL, copy_tset_maptbl),
	[ELVSS_MAPTBL] = DEFINE_2D_MAPTBL(ana6705_a71x_elvss_table, init_elvss_table, getidx_brt_table, copy_common_maptbl),
	[ELVSS_OTP_MAPTBL] = DEFINE_0D_MAPTBL(ana6705_a71x_elvss_otp_table, init_common_table, getidx_brt_table, copy_elvss_otp_maptbl),
	[ELVSS_TEMP_MAPTBL] = DEFINE_0D_MAPTBL(ana6705_a71x_elvss_temp_table, init_common_table, getidx_brt_table, copy_tset_maptbl),
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(a71x_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
	[BRT_MODE_MAPTBL] = DEFINE_3D_MAPTBL(a71x_brt_mode_table, init_common_table, getidx_brt_mode_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_XTALK_MODE
	[VGH_MAPTBL] = DEFINE_2D_MAPTBL(a71x_vgh_table, init_common_table, getidx_vgh_table, copy_common_maptbl),
#endif
	[ACL_OPR_MAPTBL] = DEFINE_2D_MAPTBL(a71x_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),
	[FPS_MAPTBL] = DEFINE_2D_MAPTBL(a71x_fps_table, init_common_table, getidx_fps_table, copy_common_maptbl),
	[LPM_NIT_MAPTBL] = DEFINE_3D_MAPTBL(a71x_lpm_nit_table, init_lpm_table, getidx_lpm_table, copy_common_maptbl),
	[LPM_MODE_MAPTBL] = DEFINE_3D_MAPTBL(a71x_lpm_mode_table, init_common_table, getidx_lpm_table, copy_common_maptbl),
	[LPM_ON_MAPTBL] = DEFINE_3D_MAPTBL(a71x_lpm_on_table, init_common_table, getidx_lpm_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [ANA6705 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A71X_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 A71X_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 A71X_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 A71X_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 A71X_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 A71X_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 A71X_SLEEP_OUT[] = { 0x11 };
static u8 A71X_SLEEP_IN[] = { 0x10 };
static u8 A71X_DISPLAY_OFF[] = { 0x28 };
static u8 A71X_DISPLAY_ON[] = { 0x29 };

static u8 A71X_TE_ON[] = { 0x35, 0x00 };
static u8 A71X_TE_OFF[] = { 0x34 };

static u8 A71X_PAGE_ADDR_SET_2A[] = {
	0x2A,
	0x00, 0x00, 0x04, 0x37,
};

static u8 A71X_PAGE_ADDR_SET_2B[] = {
	0x2B,
	0x00, 0x00, 0x09, 0x5F,
};

static u8 A71X_TSP_VSYNC_ON[] = {
	0xD7,
	0x00, 0x00, 0x0D, 0x0A, 0x0C, 0x01,
};

static u8 A71X_ELVSS_SET[] = {
	0xB7,
	0x01, 0x1B, 0x28, 0x0D, 0xC0,
	0x00,	/* 6th: ELVSS return */
	0x04,	/* 7th para : Smooth transition 4-frame */
	0x00,	/* 8th: Normal ELVSS OTP */
	0x00,	/* 9th: HBM ELVSS OTP */
	0x1D,
	0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x42, 0x43, 0x43, 0x43,
	0x43, 0x43, 0x83, 0xC3, 0x83, 0xC3, 0x83, 0xC3, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,	/* 46th: TSET */
};

static u8 A71X_BRIGHTNESS[] = {
	0x51,
	0x01, 0xBD,
};

static u8 A71X_BRIGHTNESS_MODE[] = {
	0x53,
	0x28,	/* Normal Smooth */
};

static u8 A71X_EXIT_ALPM_NORMAL[] = {0x53, 0x20 };
static u8 A71X_ACL_CONTROL[] = {0x55, 0x00 };

static u8 A71X_LPM_AOR[] =  { 0xBB, 0x00, 0x18 };	/* HLPM 60nit */
static u8 A71X_LPM_ON[] = {0x53, 0x22};	/* HLPM 60nit */
static u8 A71X_LPM_MODE[] = {0xD1, 0x08};	/* HLPM Mode */
static u8 A71X_EXIT_ALPM[] = {0x53, 0x28 };

static DEFINE_STATIC_PACKET(a71x_level1_key_enable, DSI_PKT_TYPE_WR, A71X_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(a71x_level2_key_enable, DSI_PKT_TYPE_WR, A71X_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(a71x_level3_key_enable, DSI_PKT_TYPE_WR, A71X_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(a71x_level1_key_disable, DSI_PKT_TYPE_WR, A71X_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(a71x_level2_key_disable, DSI_PKT_TYPE_WR, A71X_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(a71x_level3_key_disable, DSI_PKT_TYPE_WR, A71X_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(a71x_sleep_out, DSI_PKT_TYPE_WR, A71X_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a71x_sleep_in, DSI_PKT_TYPE_WR, A71X_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a71x_display_on, DSI_PKT_TYPE_WR, A71X_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a71x_display_off, DSI_PKT_TYPE_WR, A71X_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(a71x_exit_alpm, DSI_PKT_TYPE_WR, A71X_EXIT_ALPM, 0);

static DEFINE_STATIC_PACKET(a71x_exit_alpm_normal, DSI_PKT_TYPE_WR, A71X_EXIT_ALPM_NORMAL, 0);

static DEFINE_STATIC_PACKET(a71x_te_off, DSI_PKT_TYPE_WR, A71X_TE_OFF, 0);
static DEFINE_STATIC_PACKET(a71x_te_on, DSI_PKT_TYPE_WR, A71X_TE_ON, 0);
static DEFINE_STATIC_PACKET(a71x_page_set_2a, DSI_PKT_TYPE_WR, A71X_PAGE_ADDR_SET_2A, 0);
static DEFINE_STATIC_PACKET(a71x_page_set_2b, DSI_PKT_TYPE_WR, A71X_PAGE_ADDR_SET_2B, 0);
static DEFINE_STATIC_PACKET(a71x_tsp_vsync, DSI_PKT_TYPE_WR, A71X_TSP_VSYNC_ON, 0);


static DEFINE_PKTUI(a71x_lpm_mode, &a71x_maptbl[LPM_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a71x_lpm_mode, DSI_PKT_TYPE_WR, A71X_LPM_MODE, 0x32);
static DEFINE_PKTUI(a71x_lpm_nit, &a71x_maptbl[LPM_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a71x_lpm_nit, DSI_PKT_TYPE_WR, A71X_LPM_AOR, 0x21);
static DEFINE_PKTUI(a71x_lpm_on, &a71x_maptbl[LPM_ON_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a71x_lpm_on, DSI_PKT_TYPE_WR, A71X_LPM_ON, 0);


static DECLARE_PKTUI(a71x_elvss_set) = {
	{ .offset = 6, .maptbl = &a71x_maptbl[ELVSS_MAPTBL] },
	{ .offset = 8, .maptbl = &a71x_maptbl[ELVSS_OTP_MAPTBL] },
	{ .offset = 46, .maptbl = &a71x_maptbl[ELVSS_TEMP_MAPTBL] },
};

static DEFINE_VARIABLE_PACKET(a71x_elvss_set, DSI_PKT_TYPE_WR, A71X_ELVSS_SET, 0);
static DEFINE_PKTUI(a71x_brightness, &a71x_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a71x_brightness, DSI_PKT_TYPE_WR, A71X_BRIGHTNESS, 0);
static DEFINE_PKTUI(a71x_brightness_mode, &a71x_maptbl[BRT_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a71x_brightness_mode, DSI_PKT_TYPE_WR, A71X_BRIGHTNESS_MODE, 0);

static DECLARE_PKTUI(a71x_acl_control) = {
	{ .offset = 1, .maptbl = &a71x_maptbl[ACL_OPR_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(a71x_acl_control, DSI_PKT_TYPE_WR, A71X_ACL_CONTROL, 0);

static DEFINE_PANEL_MDELAY(a71x_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(a71x_wait_2msec, 2);
static DEFINE_PANEL_MDELAY(a71x_wait_5msec, 5);
static DEFINE_PANEL_MDELAY(a71x_wait_110msec, 110);
static DEFINE_PANEL_MDELAY(a71x_wait_sleep_out, 10);
static DEFINE_PANEL_MDELAY(a71x_wait_afc_off, 20);
static DEFINE_PANEL_MDELAY(a71x_wait_sleep_in, 120);
static DEFINE_PANEL_UDELAY(a71x_wait_1_frame_in_60hz, 16700);
static DEFINE_PANEL_UDELAY(a71x_wait_1_frame_in_30hz, 33400);
static DEFINE_PANEL_UDELAY(a71x_wait_10usec, 10);
static DEFINE_PANEL_MDELAY(a71x_wait_100msec, 100);
static DEFINE_PANEL_WAIT_VSYNC(a71x_wait_te_start, 1);
static DEFINE_PANEL_WAIT_VSYNC(a71x_wait_for_2_frames, 2);

static DEFINE_PANEL_KEY(a71x_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(a71x_level1_key_enable));
static DEFINE_PANEL_KEY(a71x_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(a71x_level2_key_enable));
static DEFINE_PANEL_KEY(a71x_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(a71x_level3_key_enable));
static DEFINE_PANEL_KEY(a71x_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(a71x_level1_key_disable));
static DEFINE_PANEL_KEY(a71x_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(a71x_level2_key_disable));
static DEFINE_PANEL_KEY(a71x_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(a71x_level3_key_disable));

static void *a71x_init_cmdtbl[] = {
	&PKTINFO(a71x_sleep_out),
	&DLYINFO(a71x_wait_sleep_out),
	&PKTINFO(a71x_page_set_2a),
	&PKTINFO(a71x_page_set_2b),
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_tsp_vsync),
	&KEYINFO(a71x_level2_key_disable),
	&PKTINFO(a71x_exit_alpm_normal),
	&PKTINFO(a71x_elvss_set),
	&PKTINFO(a71x_brightness),
	&PKTINFO(a71x_acl_control),
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_te_on),
	&KEYINFO(a71x_level2_key_disable),
	&DLYINFO(a71x_wait_110msec),
};

static void *a71x_res_init_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&KEYINFO(a71x_level3_key_enable),
	&ana6705_restbl[RES_ID],
	&ana6705_restbl[RES_COORDINATE],
	&ana6705_restbl[RES_ELVSS],
	&ana6705_restbl[RES_DATE],
	&ana6705_restbl[RES_CELL_ID],
#ifdef CONFIG_DISPLAY_USE_INFO
	&ana6705_restbl[RES_ERR_FG],
	&ana6705_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(a71x_level3_key_disable),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_set_bl_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_brightness_mode),
	&PKTINFO(a71x_elvss_set),
	&PKTINFO(a71x_brightness), //51h
	&PKTINFO(a71x_acl_control),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_display_on_cmdtbl[] = {
	&PKTINFO(a71x_display_on),
};

static void *a71x_display_off_cmdtbl[] = {
	&PKTINFO(a71x_display_off),
};

static void *a71x_exit_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_AFC
	&SEQINFO(a71x_mdnie_seqtbl[MDNIE_AFC_OFF_SEQ]),
	&DLYINFO(a71x_wait_afc_off),
#endif
#ifdef CONFIG_DISPLAY_USE_INFO
	&KEYINFO(a71x_level2_key_enable),
	&ana6705_dmptbl[DUMP_ERR_FG],
	&KEYINFO(a71x_level2_key_disable),
	&ana6705_dmptbl[DUMP_DSI_ERR],
#endif
	&PKTINFO(a71x_sleep_in),
	&DLYINFO(a71x_wait_sleep_in),
};

static void *a71x_alpm_enter_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_lpm_mode),
	&PKTINFO(a71x_lpm_nit),
	&PKTINFO(a71x_lpm_on),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_alpm_exit_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_exit_alpm),
	&DLYINFO(a71x_wait_10usec),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_set_fps_cmdtbl[] = {
	&SEQINFO(a71x_seqtbl[PANEL_SET_BL_SEQ]),
};

static void *a71x_dump_cmdtbl[] = {
	&KEYINFO(a71x_level1_key_enable),
	&KEYINFO(a71x_level2_key_enable),
	&KEYINFO(a71x_level3_key_enable),
	&ana6705_dmptbl[DUMP_RDDPM],
	&ana6705_dmptbl[DUMP_RDDSM],
	&ana6705_dmptbl[DUMP_ERR],
	&ana6705_dmptbl[DUMP_ERR_FG],
	&ana6705_dmptbl[DUMP_DSI_ERR],
	&KEYINFO(a71x_level3_key_disable),
	&KEYINFO(a71x_level2_key_disable),
	&KEYINFO(a71x_level1_key_disable),
};

static void *a71x_indisplay_enter_before_cmdtbl[] = {
	&VSYNCINFO(a71x_wait_te_start),
	&DLYINFO(a71x_wait_2msec),
};

static void *a71x_indisplay_enter_after_cmdtbl[] = {
	&VSYNCINFO(a71x_wait_for_2_frames),
};

static struct seqinfo a71x_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a71x_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a71x_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a71x_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a71x_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a71x_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a71x_exit_cmdtbl),
/*
 *	A71x + ANA6705 have H/W logic fault, ELON 1 and 2 are exchanged.
 *	With ELON 1<->2 FPCB, It is working well, But for all devices, We disable it.
 *
 *	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", a71x_alpm_enter_cmdtbl),
 */
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", a71x_alpm_exit_cmdtbl),
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", a71x_set_fps_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", a71x_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_INDISPLAY
	[PANEL_INDISPLAY_ENTER_BEFORE_SEQ] = SEQINFO_INIT("indisplay-delay", a71x_indisplay_enter_before_cmdtbl),
	[PANEL_INDISPLAY_ENTER_AFTER_SEQ] = SEQINFO_INIT("indisplay-delay", a71x_indisplay_enter_after_cmdtbl),
#endif
};


struct common_panel_info ana6705_a71x_default_panel_info = {
	.ldi_name = "ana6705",
	.name = "ana6705_a71x_default",
	.model = "AMS670TD01",
	.vendor = "SDC",
	.id = 0x800001,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA),
		.err_fg_recovery = false,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(ana6705_a71x_resol),
		.resol = ana6705_a71x_resol,
	},
	.maptbl = a71x_maptbl,
	.nr_maptbl = ARRAY_SIZE(a71x_maptbl),
	.seqtbl = a71x_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a71x_seqtbl),
	.rditbl = ana6705_rditbl,
	.nr_rditbl = ARRAY_SIZE(ana6705_rditbl),
	.restbl = ana6705_restbl,
	.nr_restbl = ARRAY_SIZE(ana6705_restbl),
	.dumpinfo = ana6705_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(ana6705_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE
	.mdnie_tune = &ana6705_a71x_mdnie_tune,
#endif
	.panel_dim_info = {
		&ana6705_a71x_panel_dimming_info,
	},
};

static int __init ana6705_a71x_panel_init(void)
{
	register_common_panel(&ana6705_a71x_default_panel_info);

	return 0;
}
arch_initcall(ana6705_a71x_panel_init)
#endif /* __ANA6705_A71X_PANEL_H__ */
