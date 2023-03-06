/*
 * linux/drivers/video/fbdev/exynos/panel/ea8079/ea8079_r8s_panel.h
 *
 * Header file for EA8079 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8079_R8S_PANEL_H__
#define __EA8079_R8S_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "ea8079.h"
#include "ea8079_dimming.h"
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "ea8079_r8s_panel_mdnie.h"
#endif
#include "ea8079_r8s_panel_dimming.h"
#ifdef CONFIG_SUPPORT_AOD_BL
#include "ea8079_r8s_panel_aod_dimming.h"
#endif

#ifdef CONFIG_DYNAMIC_FREQ
#include "ea8079_r8s_df_tbl.h"
#endif
#include "ea8079_r8s_resol.h"

#undef __pn_name__
#define __pn_name__	r8s

#undef __PN_NAME__
#define __PN_NAME__

/* ===================================================================================== */
/* ============================= [EA8079 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [EA8079 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [EA8079 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 r8s_brt_table[EA8079_TOTAL_STEP][2] = {
	{0x00, 0x03}, {0x00, 0x03}, {0x00, 0x06}, {0x00, 0x06}, {0x00, 0x09}, {0x00, 0x09}, {0x00, 0x0C}, {0x00, 0x0C}, {0x00, 0x0F}, {0x00, 0x0F},
	{0x00, 0x12}, {0x00, 0x15}, {0x00, 0x18}, {0x00, 0x18}, {0x00, 0x1B}, {0x00, 0x1D}, {0x00, 0x1F}, {0x00, 0x22}, {0x00, 0x24}, {0x00, 0x27},
	{0x00, 0x29}, {0x00, 0x2C}, {0x00, 0x2E}, {0x00, 0x31}, {0x00, 0x33}, {0x00, 0x36}, {0x00, 0x39}, {0x00, 0x3C}, {0x00, 0x3E}, {0x00, 0x41},
	{0x00, 0x43}, {0x00, 0x46}, {0x00, 0x49}, {0x00, 0x4C}, {0x00, 0x4F}, {0x00, 0x52}, {0x00, 0x55}, {0x00, 0x58}, {0x00, 0x5B}, {0x00, 0x5E},
	{0x00, 0x61}, {0x00, 0x64}, {0x00, 0x67}, {0x00, 0x6A}, {0x00, 0x6D}, {0x00, 0x70}, {0x00, 0x73}, {0x00, 0x77}, {0x00, 0x7A}, {0x00, 0x7D},
	{0x00, 0x80}, {0x00, 0x84}, {0x00, 0x87}, {0x00, 0x8A}, {0x00, 0x8D}, {0x00, 0x91}, {0x00, 0x94}, {0x00, 0x98}, {0x00, 0x9B}, {0x00, 0x9E},
	{0x00, 0xA2}, {0x00, 0xA5}, {0x00, 0xA9}, {0x00, 0xAC}, {0x00, 0xB0}, {0x00, 0xB3}, {0x00, 0xB7}, {0x00, 0xBA}, {0x00, 0xBE}, {0x00, 0xC1},
	{0x00, 0xC5}, {0x00, 0xC9}, {0x00, 0xCC}, {0x00, 0xD0}, {0x00, 0xD4}, {0x00, 0xD7}, {0x00, 0xDB}, {0x00, 0xDF}, {0x00, 0xE2}, {0x00, 0xE6},
	{0x00, 0xEA}, {0x00, 0xEE}, {0x00, 0xF1}, {0x00, 0xF5}, {0x00, 0xF9}, {0x00, 0xFD}, {0x01, 0x00}, {0x01, 0x04}, {0x01, 0x08}, {0x01, 0x0C},
	{0x01, 0x10}, {0x01, 0x14}, {0x01, 0x18}, {0x01, 0x1C}, {0x01, 0x20}, {0x01, 0x24}, {0x01, 0x27}, {0x01, 0x2B}, {0x01, 0x2F}, {0x01, 0x33},
	{0x01, 0x37}, {0x01, 0x3B}, {0x01, 0x3F}, {0x01, 0x43}, {0x01, 0x48}, {0x01, 0x4C}, {0x01, 0x50}, {0x01, 0x54}, {0x01, 0x58}, {0x01, 0x5C},
	{0x01, 0x60}, {0x01, 0x64}, {0x01, 0x68}, {0x01, 0x6D}, {0x01, 0x71}, {0x01, 0x75}, {0x01, 0x79}, {0x01, 0x7D}, {0x01, 0x81}, {0x01, 0x86},
	{0x01, 0x8A}, {0x01, 0x8E}, {0x01, 0x92}, {0x01, 0x97}, {0x01, 0x9B}, {0x01, 0x9F}, {0x01, 0xA3}, {0x01, 0xA8}, {0x01, 0xAC}, {0x01, 0xB0},
	{0x01, 0xB5}, {0x01, 0xB9}, {0x01, 0xBD}, {0x01, 0xC4}, {0x01, 0xC8}, {0x01, 0xCC}, {0x01, 0xD1}, {0x01, 0xD5}, {0x01, 0xDA}, {0x01, 0xDE},
	{0x01, 0xE2}, {0x01, 0xE7}, {0x01, 0xEB}, {0x01, 0xF0}, {0x01, 0xF4}, {0x01, 0xF8}, {0x01, 0xFD}, {0x02, 0x01}, {0x02, 0x06}, {0x02, 0x0A},
	{0x02, 0x0F}, {0x02, 0x13}, {0x02, 0x18}, {0x02, 0x1C}, {0x02, 0x21}, {0x02, 0x25}, {0x02, 0x2A}, {0x02, 0x2E}, {0x02, 0x33}, {0x02, 0x38},
	{0x02, 0x3C}, {0x02, 0x41}, {0x02, 0x45}, {0x02, 0x4A}, {0x02, 0x4E}, {0x02, 0x53}, {0x02, 0x58}, {0x02, 0x5C}, {0x02, 0x61}, {0x02, 0x66},
	{0x02, 0x6A}, {0x02, 0x6F}, {0x02, 0x74}, {0x02, 0x78}, {0x02, 0x7D}, {0x02, 0x82}, {0x02, 0x86}, {0x02, 0x8B}, {0x02, 0x90}, {0x02, 0x95},
	{0x02, 0x99}, {0x02, 0x9E}, {0x02, 0xA3}, {0x02, 0xA8}, {0x02, 0xAC}, {0x02, 0xB1}, {0x02, 0xB6}, {0x02, 0xBB}, {0x02, 0xBF}, {0x02, 0xC4},
	{0x02, 0xC9}, {0x02, 0xCE}, {0x02, 0xD3}, {0x02, 0xD8}, {0x02, 0xDC}, {0x02, 0xE1}, {0x02, 0xE6}, {0x02, 0xEB}, {0x02, 0xF0}, {0x02, 0xF5},
	{0x02, 0xFA}, {0x02, 0xFF}, {0x03, 0x03}, {0x03, 0x08}, {0x03, 0x0D}, {0x03, 0x12}, {0x03, 0x17}, {0x03, 0x1C}, {0x03, 0x21}, {0x03, 0x26},
	{0x03, 0x2B}, {0x03, 0x30}, {0x03, 0x35}, {0x03, 0x3A}, {0x03, 0x3F}, {0x03, 0x44}, {0x03, 0x49}, {0x03, 0x4B}, {0x03, 0x51}, {0x03, 0x55},
	{0x03, 0x59}, {0x03, 0x5E}, {0x03, 0x62}, {0x03, 0x66}, {0x03, 0x6B}, {0x03, 0x70}, {0x03, 0x75}, {0x03, 0x7A}, {0x03, 0x7F}, {0x03, 0x85},
	{0x03, 0x8B}, {0x03, 0x8F}, {0x03, 0x94}, {0x03, 0x99}, {0x03, 0x9E}, {0x03, 0xA2}, {0x03, 0xA6}, {0x03, 0xAB}, {0x03, 0xAF}, {0x03, 0xB4},
	{0x03, 0xB9}, {0x03, 0xBD}, {0x03, 0xC2}, {0x03, 0xC7}, {0x03, 0xCC}, {0x03, 0xD1}, {0x03, 0xD6}, {0x03, 0xDB}, {0x03, 0xE1}, {0x03, 0xE6},
	{0x03, 0xEC}, {0x03, 0xEF}, {0x03, 0xF3}, {0x03, 0xF7}, {0x03, 0xFB}, {0x03, 0xFF}, {0x00, 0x00}, {0x00, 0x03}, {0x00, 0x05}, {0x00, 0x08},
	{0x00, 0x0B}, {0x00, 0x0D}, {0x00, 0x10}, {0x00, 0x13}, {0x00, 0x15}, {0x00, 0x18}, {0x00, 0x1B}, {0x00, 0x1D}, {0x00, 0x20}, {0x00, 0x23},
	{0x00, 0x25}, {0x00, 0x28}, {0x00, 0x2B}, {0x00, 0x2D}, {0x00, 0x30}, {0x00, 0x33}, {0x00, 0x35}, {0x00, 0x38}, {0x00, 0x3B}, {0x00, 0x3D},
	{0x00, 0x40}, {0x00, 0x43}, {0x00, 0x45}, {0x00, 0x48}, {0x00, 0x4B}, {0x00, 0x4D}, {0x00, 0x50}, {0x00, 0x53}, {0x00, 0x55}, {0x00, 0x58},
	{0x00, 0x5B}, {0x00, 0x5D}, {0x00, 0x60}, {0x00, 0x63}, {0x00, 0x65}, {0x00, 0x68}, {0x00, 0x6B}, {0x00, 0x6D}, {0x00, 0x70}, {0x00, 0x73},
	{0x00, 0x75}, {0x00, 0x78}, {0x00, 0x7B}, {0x00, 0x7D}, {0x00, 0x80}, {0x00, 0x83}, {0x00, 0x85}, {0x00, 0x88}, {0x00, 0x8B}, {0x00, 0x8D},
	{0x00, 0x90}, {0x00, 0x93}, {0x00, 0x95}, {0x00, 0x98}, {0x00, 0x9B}, {0x00, 0x9D}, {0x00, 0xA0}, {0x00, 0xA3}, {0x00, 0xA5}, {0x00, 0xA8},
	{0x00, 0xAB}, {0x00, 0xAD}, {0x00, 0xB0}, {0x00, 0xB3}, {0x00, 0xB5}, {0x00, 0xB8}, {0x00, 0xBB}, {0x00, 0xBD}, {0x00, 0xC0}, {0x00, 0xC3},
	{0x00, 0xC5}, {0x00, 0xC8}, {0x00, 0xCB}, {0x00, 0xCD}, {0x00, 0xD0}, {0x00, 0xD3}, {0x00, 0xD5}, {0x00, 0xD8}, {0x00, 0xDB}, {0x00, 0xDD},
	{0x00, 0xE0}, {0x00, 0xE3}, {0x00, 0xE5}, {0x00, 0xE8}, {0x00, 0xEB}, {0x00, 0xED}, {0x00, 0xF0}, {0x00, 0xF3}, {0x00, 0xF5}, {0x00, 0xF8},
	{0x00, 0xFB}, {0x00, 0xFD}, {0x01, 0x00}, {0x01, 0x03}, {0x01, 0x05}, {0x01, 0x08}, {0x01, 0x0B}, {0x01, 0x0D}, {0x01, 0x10}, {0x01, 0x13},
	{0x01, 0x15}, {0x01, 0x18}, {0x01, 0x1B}, {0x01, 0x1D}, {0x01, 0x20}, {0x01, 0x23}, {0x01, 0x25}, {0x01, 0x28}, {0x01, 0x2A}, {0x01, 0x2D},
	{0x01, 0x30}, {0x01, 0x32}, {0x01, 0x35}, {0x01, 0x38}, {0x01, 0x3A}, {0x01, 0x3D}, {0x01, 0x40}, {0x01, 0x42}, {0x01, 0x45}, {0x01, 0x48},
	{0x01, 0x4A}, {0x01, 0x4D}, {0x01, 0x50}, {0x01, 0x52}, {0x01, 0x55}, {0x01, 0x58}, {0x01, 0x5A}, {0x01, 0x5D}, {0x01, 0x60}, {0x01, 0x62},
	{0x01, 0x65}, {0x01, 0x68}, {0x01, 0x6A}, {0x01, 0x6D}, {0x01, 0x70}, {0x01, 0x72}, {0x01, 0x75}, {0x01, 0x78}, {0x01, 0x7A}, {0x01, 0x7D},
	{0x01, 0x80}, {0x01, 0x82}, {0x01, 0x85}, {0x01, 0x88}, {0x01, 0x8A}, {0x01, 0x8D}, {0x01, 0x90}, {0x01, 0x92}, {0x01, 0x95}, {0x01, 0x98},
	{0x01, 0x9A}, {0x01, 0x9D}, {0x01, 0xA0}, {0x01, 0xA2}, {0x01, 0xA5}, {0x01, 0xA8}, {0x01, 0xAA}, {0x01, 0xAD}, {0x01, 0xB0}, {0x01, 0xB2},
	{0x01, 0xB5}, {0x01, 0xB8}, {0x01, 0xBA}, {0x01, 0xBD}, {0x01, 0xC0}, {0x01, 0xC2}, {0x01, 0xC5}, {0x01, 0xC8}, {0x01, 0xCA}, {0x01, 0xCD},
	{0x01, 0xD0}, {0x01, 0xD2}, {0x01, 0xD5}, {0x01, 0xD8}, {0x01, 0xDA}, {0x01, 0xDD}, {0x01, 0xE0}, {0x01, 0xE2}, {0x01, 0xE5}, {0x01, 0xE8},
	{0x01, 0xEA}, {0x01, 0xED}, {0x01, 0xF0}, {0x01, 0xF2}, {0x01, 0xF5}, {0x01, 0xF8}, {0x01, 0xFA}, {0x01, 0xFD}, {0x02, 0x00}, {0x02, 0x02},
	{0x02, 0x05}, {0x02, 0x08}, {0x02, 0x0A}, {0x02, 0x0D}, {0x02, 0x10}, {0x02, 0x12}, {0x02, 0x15}, {0x02, 0x18}, {0x02, 0x1A}, {0x02, 0x1D},
	{0x02, 0x20}, {0x02, 0x22}, {0x02, 0x25}, {0x02, 0x28}, {0x02, 0x2A}, {0x02, 0x2D}, {0x02, 0x30}, {0x02, 0x32}, {0x02, 0x35}, {0x02, 0x38},
	{0x02, 0x3A}, {0x02, 0x3D}, {0x02, 0x40}, {0x02, 0x42}, {0x02, 0x45}, {0x02, 0x48}, {0x02, 0x4A}, {0x02, 0x4D}, {0x02, 0x50}, {0x02, 0x52},
	{0x02, 0x55}, {0x02, 0x58}, {0x02, 0x5A}, {0x02, 0x5D}, {0x02, 0x60}, {0x02, 0x62}, {0x02, 0x65},
};

static u8 r8s_elvss_table[EA8079_TOTAL_STEP][1] = {
	/* OVER_ZERO */
	[0 ... 255] = { 0x16 },
	/* HBM */
	[256 ... 270] = { 0x26 },
	[271 ... 283] = { 0x25 },
	[284 ... 297] = { 0x24 },
	[298 ... 310] = { 0x23 },
	[311 ... 324] = { 0x22 },
	[325 ... 337] = { 0x21 },
	[338 ... 351] = { 0x20 },
	[352 ... 364] = { 0x1F },
	[365 ... 378] = { 0x1E },
	[379 ... 391] = { 0x1D },
	[392 ... 405] = { 0x1C },
	[406 ... 418] = { 0x1B },
	[419 ... 432] = { 0x1A },
	[433 ... 445] = { 0x19 },
	[446 ... 459] = { 0x18 },
	[460 ... 472] = { 0x17 },
	[473 ... 486] = { 0x16 },
};

static u8 r8s_hbm_transition_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
	/* HBM off */
	{
		/* Normal */
		{ 0x20 },
		/* Smooth */
		{ 0x28 },
	},
	/* HBM on */
	{
		/* Normal */
		{ 0xE0 },
		/* Smooth */
		{ 0xE0 }, /* R8 no smooth in HBM */
	}
};

static u8 r8s_acl_frame_avg_table[][1] = {
	{ 0x09 }, /* 16 Frame Avg */
	{ 0x0B }, /* 32 Frame Avg */
};

static u8 r8s_acl_start_point_table[][2] = {
	{ 0x41, 0xFF }, /* 50 Percent */
	{ 0x42, 0x65 }, /* 60 Percent */
};

static u8 r8s_acl_dim_speed_table[][1] = {
	{ 0x00 }, /* 0x00 : ACL Dimming Off */
	{ 0x20 }, /* 0x20 : ACL Dimming 32 Frames */
};

static u8 r8s_acl_opr_table[ACL_OPR_MAX][1] = {
	{ 0x00 }, /* ACL OFF OPR */
	{ 0x01 }, /* ACL ON OPR_3 */
	{ 0x01 }, /* ACL ON OPR_6 */
	{ 0x01 }, /* ACL ON OPR_8 */
	{ 0x02 }, /* ACL ON OPR_12 */
	{ 0x02 }, /* ACL ON OPR_15 */
};

static u8 r8s_lpm_mode_table[2][1] = {
	[ALPM_MODE] = { 0x00 },
	[HLPM_MODE] = { 0x00 },
};

static u8 r8s_lpm_nit_table[4][2] = {
	/* LPM 2NIT: */
	{ 0x09, 0x24 },
	/* LPM 10NIT */
	{ 0x08, 0x00 },
	/* LPM 30NIT */
	{ 0x04, 0xF0 },
	/* LPM 60NIT */
	{ 0x00, 0x14 },
};

static u8 r8s_lpm_on_table[2][1] = {
	[ALPM_MODE] = { 0x23 },
	[HLPM_MODE] = { 0x23 },
};

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 r8s_vgh_table[][1] = {
	{ 0xC0 },	/* off 7.0 V */
	{ 0x60 },	/* on 6.2 V */
};
#endif

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static u8 r8s_fast_discharge_table[][1] = {
	{ 0x02 },	//fd off
	{ 0x01 },	//fd on
};
#endif

/*
 * 120Hz do not need below setting values
 * 120Hz values == 60Hz values.
 * Guide by Display RnD team.
 */

static u8 r8s_fps_table[][1] = {
	[EA8079_VRR_FPS_120] = { 0x10 },
	[EA8079_VRR_FPS_96] = { 0x00 },
	[EA8079_VRR_FPS_60] = { 0x00 },
};

static u8 r8s_fps_porch_table[][2] = {
	[EA8079_VRR_FPS_120] = { 0xA9, 0x70 },
	[EA8079_VRR_FPS_96] = { 0xA2, 0x5C },
	[EA8079_VRR_FPS_60] = { 0xA9, 0x70 },
};

static u8 r8s_fps_freq_table[][24] = {
	[EA8079_VRR_FPS_120] = {
		0x12, 0xE0, 0x09, 0x2E, 0x09, 0x03, 0x08, 0x96,
		0x08, 0x2C, 0x07, 0x52, 0x06, 0x42, 0x05, 0x2A,
		0x03, 0xDA, 0x02, 0x82, 0x01, 0x2C, 0x00, 0x14
	},
	[EA8079_VRR_FPS_96] = {
		0x0B, 0xCC, 0x09, 0x2A, 0x09, 0x02, 0x08, 0x8e,
		0x08, 0x16, 0x07, 0x54, 0x06, 0x50, 0x05, 0x40,
		0x03, 0xF2, 0x02, 0xA2, 0x00, 0xFC, 0x00, 0x44
	},
	[EA8079_VRR_FPS_60] = {
		0x12, 0xE0, 0x09, 0x2E, 0x09, 0x03, 0x08, 0x96,
		0x08, 0x2C, 0x07, 0x52, 0x06, 0x42, 0x05, 0x2A,
		0x03, 0xDA, 0x02, 0x82, 0x01, 0x2C, 0x00, 0x14
	},
};

static u8 r8s_fps_porch_setting_table[][1] = {
	[EA8079_VRR_FPS_120] = { 0x20 },
	[EA8079_VRR_FPS_96] = { 0x40 },
	[EA8079_VRR_FPS_60] = { 0x20 },
};

#if defined(__PANEL_NOT_USED_VARIABLE__)
static u8 r8s_gamma_mtp_table_0[MAX_EA8079_VRR][MAX_EA8079_GAMMA_BR_INDEX][EA8079_GAMMA_MTP_0_LEN] = {
	{{0x00}},
};

static u8 r8s_gamma_mtp_table_1[MAX_EA8079_VRR][MAX_EA8079_GAMMA_BR_INDEX][EA8079_GAMMA_MTP_1_LEN] = {
	{{0x00}},
};

static u8 r8s_gamma_mtp_table_2[MAX_EA8079_VRR][MAX_EA8079_GAMMA_BR_INDEX][EA8079_GAMMA_MTP_2_LEN] = {
	{{0x00}},
};

static u8 r8s_gamma_mtp_table_3[MAX_EA8079_VRR][MAX_EA8079_GAMMA_BR_INDEX][EA8079_GAMMA_MTP_3_LEN] = {
	{{0x00}},
};
#endif

#ifdef CONFIG_DYNAMIC_FREQ
static u8 r8s_dyn_ffc_table_1[][3] = {
	/* A: 1110  B: 1114  C: 1124 */
	{
		/* 1110 */
		0xA4, 0xF5, 0xBB
	},
	{
		/* 1114 */
		0xA4, 0x5E, 0x19
	},
	{
		/* 1124 */
		0xA2, 0xE7, 0xBD
	},
};

static u8 r8s_dyn_ffc_table_2[][14] = {
	/*	A: 1110  B: 1114  C: 1124	*/
	{
		/* 1110 */
		0x11, 0x75, 0xA4, 0xF5, 0xBB, 0x9E, 0x1F, 0x46,
		0x9E, 0x1F, 0x46, 0x00, 0x32, 0x32
	},
	{
		/* 1114 */
		0x11, 0x75, 0xA4, 0x5E, 0x19, 0x9E, 0x1F, 0x46,
		0x9E, 0x1F, 0x46, 0x00, 0x32, 0x32
	},
	{
		/* 1124 */
		0x11, 0x75, 0xA2, 0xE7, 0xBD, 0x9E, 0x1F, 0x46,
		0x9E, 0x1F, 0x46, 0x00, 0x32, 0x32
	},
};
#endif

static struct maptbl r8s_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = DEFINE_2D_MAPTBL(r8s_brt_table, init_gamma_mode2_brt_table, getidx_gamma_mode2_brt_table, copy_common_maptbl),
	[HBM_ONOFF_MAPTBL] = DEFINE_3D_MAPTBL(r8s_hbm_transition_table, init_common_table, getidx_hbm_transition_table, copy_common_maptbl),
	[ACL_FRAME_AVG_MAPTBL] = DEFINE_2D_MAPTBL(r8s_acl_frame_avg_table, init_common_table, getidx_acl_onoff_table, copy_common_maptbl),
	[ACL_START_POINT_MAPTBL] = DEFINE_2D_MAPTBL(r8s_acl_start_point_table, init_common_table, getidx_hbm_onoff_table, copy_common_maptbl),
	[ACL_DIM_SPEED_MAPTBL] = DEFINE_2D_MAPTBL(r8s_acl_dim_speed_table, init_common_table, getidx_acl_dim_onoff_table, copy_common_maptbl),
	[ACL_OPR_MAPTBL] = DEFINE_2D_MAPTBL(r8s_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),
	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(r8s_tset_table, init_common_table, NULL, copy_tset_maptbl),
	[ELVSS_MAPTBL] = DEFINE_2D_MAPTBL(r8s_elvss_table, init_common_table, getidx_gm2_elvss_table, copy_common_maptbl),
	[LPM_MODE_MAPTBL] = DEFINE_2D_MAPTBL(r8s_lpm_mode_table, init_common_table, getidx_lpm_mode_table, copy_common_maptbl),
	[LPM_NIT_MAPTBL] = DEFINE_2D_MAPTBL(r8s_lpm_nit_table, init_lpm_brt_table, getidx_lpm_brt_table, copy_common_maptbl),
	[LPM_ON_MAPTBL] = DEFINE_2D_MAPTBL(r8s_lpm_on_table, init_common_table, getidx_lpm_mode_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_XTALK_MODE
	[VGH_MAPTBL] = DEFINE_2D_MAPTBL(r8s_vgh_table, init_common_table, getidx_vgh_table, copy_common_maptbl),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	[DYN_FFC_1_MAPTBL] = DEFINE_2D_MAPTBL(r8s_dyn_ffc_table_1, init_common_table, getidx_dyn_ffc_table, copy_common_maptbl),
	[DYN_FFC_2_MAPTBL] = DEFINE_2D_MAPTBL(r8s_dyn_ffc_table_2, init_common_table, getidx_dyn_ffc_table, copy_common_maptbl),
#endif
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	[FAST_DISCHARGE_MAPTBL] = DEFINE_2D_MAPTBL(r8s_fast_discharge_table,
			init_common_table, getidx_fast_discharge_table, copy_common_maptbl),
#endif
	[FPS_MAPTBL] = DEFINE_2D_MAPTBL(r8s_fps_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
#if defined(__PANEL_NOT_USED_VARIABLE__)
	[GAMMA_MTP_0_HS_MAPTBL] = DEFINE_3D_MAPTBL(r8s_gamma_mtp_table_0,
			init_gamma_mtp_all_table, getidx_vrr_gamma_table, copy_common_maptbl),
	[GAMMA_MTP_1_HS_MAPTBL] = DEFINE_3D_MAPTBL(r8s_gamma_mtp_table_1,
			init_common_table, getidx_vrr_gamma_table, copy_common_maptbl),
	[GAMMA_MTP_2_HS_MAPTBL] = DEFINE_3D_MAPTBL(r8s_gamma_mtp_table_2,
			init_common_table, getidx_vrr_gamma_table, copy_common_maptbl),
	[GAMMA_MTP_3_HS_MAPTBL] = DEFINE_3D_MAPTBL(r8s_gamma_mtp_table_3,
			init_common_table, getidx_vrr_gamma_table, copy_common_maptbl),
#endif
	[FPS_PORCH_MAPTBL] = DEFINE_2D_MAPTBL(r8s_fps_porch_table,
			init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[FPS_FREQ_MAPTBL] = DEFINE_2D_MAPTBL(r8s_fps_freq_table,
			init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[FPS_PORCH_SETTING_MAPTBL] = DEFINE_2D_MAPTBL(r8s_fps_porch_setting_table,
			init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [EA8079 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 R8S_KEY1_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 R8S_KEY2_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 R8S_KEY1_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 R8S_KEY2_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 R8S_SLEEP_OUT[] = { 0x11 };
static u8 R8S_SLEEP_IN[] = { 0x10 };
static u8 R8S_DISPLAY_OFF[] = { 0x28 };
static u8 R8S_DISPLAY_ON[] = { 0x29 };

static u8 R8S_TE_ON[] = { 0x35, 0x00 };
static u8 R8S_TE_OFFSET[] = { 0xDF, 0x00, 0x00, 0x00, 0x01, 0x95, 0x80, 0x0C };

static u8 R8S_LPM_ON[] = { 0x53, 0x22 };
static u8 R8S_LPM_NIT[] = { 0xBB, 0x09, 0x24 };

static DEFINE_STATIC_PACKET(r8s_level1_key_enable, DSI_PKT_TYPE_WR, R8S_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(r8s_level2_key_enable, DSI_PKT_TYPE_WR, R8S_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(r8s_level1_key_disable, DSI_PKT_TYPE_WR, R8S_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(r8s_level2_key_disable, DSI_PKT_TYPE_WR, R8S_KEY2_DISABLE, 0);

static DEFINE_STATIC_PACKET(r8s_sleep_out, DSI_PKT_TYPE_WR, R8S_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(r8s_sleep_in, DSI_PKT_TYPE_WR, R8S_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(r8s_display_on, DSI_PKT_TYPE_WR, R8S_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(r8s_display_off, DSI_PKT_TYPE_WR, R8S_DISPLAY_OFF, 0);

static DEFINE_STATIC_PACKET(r8s_te_offset, DSI_PKT_TYPE_WR, R8S_TE_OFFSET, 0);
static DEFINE_STATIC_PACKET(r8s_te_on, DSI_PKT_TYPE_WR, R8S_TE_ON, 0);

static u8 R8S_ELVSS_SET[] = {
	0xB7,
	0x01, 0x5C, 0xCB, 0x0D, 0xC0, 0x16
};

static DECLARE_PKTUI(r8s_elvss_set) = {
	{ .offset = 6, .maptbl = &r8s_maptbl[ELVSS_MAPTBL] },
};

static DEFINE_VARIABLE_PACKET(r8s_elvss_set, DSI_PKT_TYPE_WR, R8S_ELVSS_SET, 0x00);


static u8 R8S_TSET_SET[] = {
	0xB7,
	0x00,
};
static DEFINE_PKTUI(r8s_tset_set, &r8s_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_tset_set, DSI_PKT_TYPE_WR, R8S_TSET_SET, 0x2D);


static u8 R8S_LPM_MODE[] = { 0xE3, 0x00 };
static DEFINE_PKTUI(r8s_lpm_enter_mode, &r8s_maptbl[LPM_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_lpm_enter_mode, DSI_PKT_TYPE_WR, R8S_LPM_MODE, 0);

static u8 R8S_SMOOTH_DIMMING_SETTING[] = { 0xB7, 0x08 };
static DEFINE_STATIC_PACKET(r8s_smooth_dimming_setting, DSI_PKT_TYPE_WR, R8S_SMOOTH_DIMMING_SETTING, 0x06);

static u8 R8S_AOD_SETTING[] = { 0xEE, 0x00 };
static DEFINE_STATIC_PACKET(r8s_aod_setting, DSI_PKT_TYPE_WR, R8S_AOD_SETTING, 0x09);

static u8 R8S_LPM_FREQ[] = { 0x60, 0x10 };
static DEFINE_STATIC_PACKET(r8s_lpm_freq, DSI_PKT_TYPE_WR, R8S_LPM_FREQ, 0x00);

static u8 R8S_LPM_PORCH_0_ON[] = { 0xD3, 0x3B, 0x0F, 0x3B, 0x0F };
static DEFINE_STATIC_PACKET(r8s_lpm_porch_0_on, DSI_PKT_TYPE_WR, R8S_LPM_PORCH_0_ON, 0x5F);

static u8 R8S_LPM_PORCH_1_ON[] = { 0xD3, 0x32, 0x00, 0x00, 0x03, 0x32 };
static DEFINE_STATIC_PACKET(r8s_lpm_porch_1_on, DSI_PKT_TYPE_WR, R8S_LPM_PORCH_1_ON, 0x78);

static u8 R8S_LPM_PORCH_2_ON[] = { 0xD9,  0x48, 0x44, 0x45, 0x4A };
static DEFINE_STATIC_PACKET(r8s_lpm_porch_2_on, DSI_PKT_TYPE_WR, R8S_LPM_PORCH_2_ON, 0x04);

static u8 R8S_LPM_PORCH_3_ON[] = { 0xD1, 0x1B, 0x10 };
static DEFINE_STATIC_PACKET(r8s_lpm_porch_3_on, DSI_PKT_TYPE_WR, R8S_LPM_PORCH_3_ON, 0x16);

static u8 R8S_LPM_PORCH_0_OFF[] = { 0xD3, 0x42, 0x0F, 0x42, 0x0F };
static DEFINE_STATIC_PACKET(r8s_lpm_porch_0_off, DSI_PKT_TYPE_WR, R8S_LPM_PORCH_0_OFF, 0x5F);

static u8 R8S_LPM_PORCH_1_OFF[] = { 0xD3, 0x41, 0x00, 0x00, 0x03, 0x41 };
static DEFINE_STATIC_PACKET(r8s_lpm_porch_1_off, DSI_PKT_TYPE_WR, R8S_LPM_PORCH_1_OFF, 0x78);

static u8 R8S_LPM_PORCH_2_OFF[] = { 0xD9, 0x48, 0x44, 0x45, 0x4A };
static DEFINE_STATIC_PACKET(r8s_lpm_porch_2_off, DSI_PKT_TYPE_WR, R8S_LPM_PORCH_2_OFF, 0x04);

static u8 R8S_LPM_PORCH_3_OFF[] = { 0xD1, 0x1A, 0x1E };
static DEFINE_STATIC_PACKET(r8s_lpm_porch_3_off, DSI_PKT_TYPE_WR, R8S_LPM_PORCH_3_OFF, 0x16);


static DEFINE_PKTUI(r8s_lpm_on, &r8s_maptbl[LPM_ON_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_lpm_on, DSI_PKT_TYPE_WR, R8S_LPM_ON, 0);
static DEFINE_PKTUI(r8s_lpm_nit, &r8s_maptbl[LPM_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_lpm_nit, DSI_PKT_TYPE_WR, R8S_LPM_NIT, 0x0B);

static DEFINE_COND(r8s_cond_is_panel_state_not_lpm, is_panel_state_not_lpm);
static u8 R8S_SAP_SETTING[] = { 0xD4, 0x10, 0x10 };
static DEFINE_STATIC_PACKET(r8s_sap_setting, DSI_PKT_TYPE_WR, R8S_SAP_SETTING, 0x03);

#ifdef CONFIG_SUPPORT_MASK_LAYER
static DEFINE_PANEL_MDELAY(r8s_wait_1msec, 1);
#endif
static DEFINE_PANEL_MDELAY(r8s_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(r8s_wait_17msec, 17);

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static DEFINE_PANEL_MDELAY(r8s_wait_33msec, 33);
#endif

static DEFINE_PANEL_MDELAY(r8s_wait_sleep_out_110msec, 110);
static DEFINE_PANEL_MDELAY(r8s_wait_sleep_in, 120);
static DEFINE_PANEL_UDELAY(r8s_wait_1usec, 1);

static DEFINE_PANEL_KEY(r8s_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(r8s_level1_key_enable));
static DEFINE_PANEL_KEY(r8s_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(r8s_level2_key_enable));
static DEFINE_PANEL_KEY(r8s_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(r8s_level1_key_disable));
static DEFINE_PANEL_KEY(r8s_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(r8s_level2_key_disable));

#ifdef CONFIG_SUPPORT_MASK_LAYER
static DEFINE_PANEL_VSYNC_DELAY(r8s_wait_1_vsync, 1);

static DEFINE_COND(r8s_cond_is_120hz, ea8079_is_120hz);
#endif

static u8 R8S_HBM_TRANSITION[] = {
	0x53, 0x20
};

static DEFINE_PKTUI(r8s_hbm_transition, &r8s_maptbl[HBM_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_hbm_transition, DSI_PKT_TYPE_WR, R8S_HBM_TRANSITION, 0);

static u8 R8S_ACL_FRAME_AVG[] = {
	0xBB, 0x09
};

static DEFINE_PKTUI(r8s_acl_frame_avg, &r8s_maptbl[ACL_FRAME_AVG_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_acl_frame_avg, DSI_PKT_TYPE_WR, R8S_ACL_FRAME_AVG, 0xCA);

static u8 R8S_ACL_START_POINT[] = {
	0xBB, 0x41, 0xFF
};

static DEFINE_PKTUI(r8s_acl_start_point, &r8s_maptbl[ACL_START_POINT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_acl_start_point, DSI_PKT_TYPE_WR, R8S_ACL_START_POINT, 0xCF);

static u8 R8S_ACL_DIMMING_SPEED[] = {
	0xBB, 0x20,
};

static DEFINE_PKTUI(r8s_acl_dimming_speed, &r8s_maptbl[ACL_DIM_SPEED_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_acl_dimming_speed, DSI_PKT_TYPE_WR, R8S_ACL_DIMMING_SPEED, 0xD9);

static u8 R8S_ACL[] = {
	0x55, 0x02
};

static DEFINE_PKTUI(r8s_acl_control, &r8s_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_acl_control, DSI_PKT_TYPE_WR, R8S_ACL, 0);

static u8 R8S_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(r8s_wrdisbv, &r8s_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_wrdisbv, DSI_PKT_TYPE_WR, R8S_WRDISBV, 0);


static u8 R8S_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 R8S_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x5F };

static DEFINE_STATIC_PACKET(r8s_caset, DSI_PKT_TYPE_WR, R8S_CASET, 0);
static DEFINE_STATIC_PACKET(r8s_paset, DSI_PKT_TYPE_WR, R8S_PASET, 0);

#ifdef CONFIG_DYNAMIC_FREQ
static u8 R8S_FFC_SET_1[] = {
	0xE4,
	0xA4, 0xF5, 0xBB
};
static DEFINE_PKTUI(r8s_ffc_set_1, &r8s_maptbl[DYN_FFC_1_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_ffc_set_1, DSI_PKT_TYPE_WR, R8S_FFC_SET_1, 0x01);

static u8 R8S_FFC_SET_2[] = {
	0xE9,
	0x11, 0x75, 0xA4, 0xF5, 0xBB, 0x9E, 0x1F, 0x46,
	0x9E, 0x1F, 0x46, 0x00, 0x32, 0x32
};
static DEFINE_PKTUI(r8s_ffc_set_2, &r8s_maptbl[DYN_FFC_2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_ffc_set_2, DSI_PKT_TYPE_WR, R8S_FFC_SET_2, 0x00);

static u8 R8S_FFC_OFF[] = { 0xE9, 0x10 };
static DEFINE_STATIC_PACKET(r8s_ffc_off, DSI_PKT_TYPE_WR, R8S_FFC_OFF, 0);

#endif

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static u8 R8S_FAST_DISCHARGE[] = {
	0xD5,
	0x01,
};
static DEFINE_PKTUI(r8s_fast_discharge, &r8s_maptbl[FAST_DISCHARGE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_fast_discharge, DSI_PKT_TYPE_WR, R8S_FAST_DISCHARGE, 0x0A);
#endif

static u8 R8S_PCD_SET_DET_LOW[] = {
	0xE8,
	0x57, 0x1A	/* 1st 0x57: default high, 2nd 0x1A : DDI self reset and go to sleep in */
};
static DEFINE_STATIC_PACKET(r8s_pcd_det_set, DSI_PKT_TYPE_WR, R8S_PCD_SET_DET_LOW, 0);

static u8 R8S_ERR_FG_OFF[] = { 0xEC, 0x11 };
static DEFINE_STATIC_PACKET(r8s_err_fg_off, DSI_PKT_TYPE_WR, R8S_ERR_FG_OFF, 0x0D);

static u8 R8S_VSYNC_SET[] = {
	0xE6,
	0x01
};
static DEFINE_STATIC_PACKET(r8s_vsync_set, DSI_PKT_TYPE_WR, R8S_VSYNC_SET, 1);

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 R8S_XTALK_MODE[] = { 0xD9, 0x60 };
static DEFINE_PKTUI(r8s_xtalk_mode, &r8s_maptbl[VGH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_xtalk_mode, DSI_PKT_TYPE_WR, R8S_XTALK_MODE, 0x1C);
#endif

static u8 R8S_FPS[] = { 0x60, 0x00, };
static DEFINE_PKTUI(r8s_fps, &r8s_maptbl[FPS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_fps, DSI_PKT_TYPE_WR, R8S_FPS, 0);

static u8 R8S_FPS_PORCH[] = { 0xD8, 0xA9, 0x70, };
static DEFINE_PKTUI(r8s_fps_porch, &r8s_maptbl[FPS_PORCH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_fps_porch, DSI_PKT_TYPE_WR, R8S_FPS_PORCH, 0x0B);

static u8 R8S_FPS_FREQ[] = {
	0xBA,
	0x12, 0xE0, 0x09, 0x2E, 0x09, 0x03, 0x08, 0x96,
	0x08, 0x2C, 0x07, 0x52, 0x06, 0x42, 0x05, 0x2A,
	0x03, 0xDA, 0x02, 0x82, 0x01, 0x2C, 0x00, 0x14,
};

static DEFINE_PKTUI(r8s_fps_freq, &r8s_maptbl[FPS_FREQ_MAPTBL], 1);

static DEFINE_VARIABLE_PACKET(r8s_fps_freq, DSI_PKT_TYPE_WR, R8S_FPS_FREQ, 0xD4);

static u8 R8S_FPS_PORCH_SETTING[] = {
	0xBB,
	0x7A, 0x01, 0x25, 0x00, 0x14, 0x20, 0x30
};
static DEFINE_PKTUI(r8s_fps_porch_setting, &r8s_maptbl[FPS_PORCH_SETTING_MAPTBL], 6);
static DEFINE_VARIABLE_PACKET(r8s_fps_porch_setting, DSI_PKT_TYPE_WR, R8S_FPS_PORCH_SETTING, 0);


static u8 R8S_PANEL_UPDATE[] = {
	0xF7,
	0x03
};
static DEFINE_STATIC_PACKET(r8s_panel_update, DSI_PKT_TYPE_WR, R8S_PANEL_UPDATE, 0);


static u8 R8S_DSC[] = { 0x01 };
static DEFINE_STATIC_PACKET(r8s_dsc, DSI_PKT_TYPE_COMP, R8S_DSC, 0);

static u8 R8S_PPS[] = {
	// 1080x2400 Slice Info : 540x40
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
	0x04, 0x38, 0x00, 0x28, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0xDD,
	0x00, 0x07, 0x00, 0x0C, 0x02, 0x77, 0x02, 0x8B,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(r8s_pps, DSI_PKT_TYPE_PPS, R8S_PPS, 0);

#if defined(__PANEL_NOT_USED_VARIABLE__)
static u8 R8S_GAMMA_MTP_0_HS[EA8079_GAMMA_MTP_0_LEN + 1] = { EA8079_GAMMA_MTP_0_REG, 0x00, };
static DEFINE_PKTUI(r8s_gamma_mtp_0, &r8s_maptbl[GAMMA_MTP_0_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_gamma_mtp_0, DSI_PKT_TYPE_WR, R8S_GAMMA_MTP_0_HS, EA8079_GAMMA_MTP_0_OFS);

static u8 R8S_GAMMA_MTP_1_HS[EA8079_GAMMA_MTP_1_LEN + 1] = { EA8079_GAMMA_MTP_1_REG, 0x00, };
static DEFINE_PKTUI(r8s_gamma_mtp_1, &r8s_maptbl[GAMMA_MTP_1_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_gamma_mtp_1, DSI_PKT_TYPE_WR, R8S_GAMMA_MTP_1_HS, EA8079_GAMMA_MTP_1_OFS);

static u8 R8S_GAMMA_MTP_2_HS[EA8079_GAMMA_MTP_2_LEN + 1] = { EA8079_GAMMA_MTP_2_REG, 0x00, };
static DEFINE_PKTUI(r8s_gamma_mtp_2, &r8s_maptbl[GAMMA_MTP_2_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_gamma_mtp_2, DSI_PKT_TYPE_WR, R8S_GAMMA_MTP_2_HS, EA8079_GAMMA_MTP_2_OFS);

static u8 R8S_GAMMA_MTP_3_HS[EA8079_GAMMA_MTP_3_LEN + 1] = { EA8079_GAMMA_MTP_3_REG, 0x00, };
static DEFINE_PKTUI(r8s_gamma_mtp_3, &r8s_maptbl[GAMMA_MTP_3_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(r8s_gamma_mtp_3, DSI_PKT_TYPE_WR, R8S_GAMMA_MTP_3_HS, EA8079_GAMMA_MTP_3_OFS);
#endif

static struct seqinfo SEQINFO(r8s_set_bl_param_seq);
#if defined(__PANEL_NOT_USED_VARIABLE__)
static struct seqinfo SEQINFO(r8s_set_gamma_para_seq);
#endif
static struct seqinfo SEQINFO(r8s_set_fps_param_seq);

static void *r8s_init_cmdtbl[] = {
	&DLYINFO(r8s_wait_10msec),
	&PKTINFO(r8s_sleep_out),
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&DLYINFO(r8s_wait_10msec),
	&PKTINFO(r8s_dsc),
	&PKTINFO(r8s_pps),
	&PKTINFO(r8s_caset),
	&PKTINFO(r8s_paset),
#ifdef CONFIG_DYNAMIC_FREQ
	&PKTINFO(r8s_ffc_set_1),
	&PKTINFO(r8s_ffc_set_2),
#endif
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	&PKTINFO(r8s_fast_discharge),
#endif
	&PKTINFO(r8s_vsync_set),
	&PKTINFO(r8s_pcd_det_set),
	&PKTINFO(r8s_smooth_dimming_setting),
	&PKTINFO(r8s_sap_setting),
	&PKTINFO(r8s_panel_update),
	&PKTINFO(r8s_aod_setting),
	&SEQINFO(r8s_set_fps_param_seq),
	&SEQINFO(r8s_set_bl_param_seq),
	&PKTINFO(r8s_te_offset),
	&PKTINFO(r8s_te_on),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
	&DLYINFO(r8s_wait_sleep_out_110msec),
};

static void *r8s_res_init_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&ea8079_restbl[RES_ID],
	&ea8079_restbl[RES_COORDINATE],
	&ea8079_restbl[RES_CODE],
	&ea8079_restbl[RES_DATE],
	&ea8079_restbl[RES_ELVSS],
	&ea8079_restbl[RES_OCTA_ID],
	&ea8079_restbl[RES_GAMMA_MTP_120],
	&ea8079_restbl[RES_GAMMA_MTP_60],
#ifdef CONFIG_DISPLAY_USE_INFO
	&ea8079_restbl[RES_CHIP_ID],
	&ea8079_restbl[RES_SELF_DIAG],
	&ea8079_restbl[RES_ERR_FG],
	&ea8079_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
};

#if defined(__PANEL_NOT_USED_VARIABLE__)
static void *r8s_set_gamma_param_cmdtbl[] = {
	&PKTINFO(r8s_gamma_mtp_0),
	&PKTINFO(r8s_gamma_mtp_1),
	&PKTINFO(r8s_gamma_mtp_2),
	&PKTINFO(r8s_gamma_mtp_3),
};
#endif

static void *r8s_set_bl_param_cmdtbl[] = {
#if defined(__PANEL_NOT_USED_VARIABLE__)
	&SEQINFO(r8s_set_gamma_para_seq), /* gamma offset */
#endif
	&PKTINFO(r8s_hbm_transition),
	&PKTINFO(r8s_elvss_set),
	&PKTINFO(r8s_tset_set),
	&PKTINFO(r8s_acl_frame_avg),
	&PKTINFO(r8s_acl_start_point),
	&PKTINFO(r8s_acl_dimming_speed),
	&PKTINFO(r8s_acl_control),
	&PKTINFO(r8s_wrdisbv),
#if defined(__PANEL_NOT_USED_VARIABLE__)
#ifdef CONFIG_SUPPORT_XTALK_MODE
	&PKTINFO(r8s_xtalk_mode),
#endif
#endif
};

static void *r8s_set_fps_param_cmdtbl[] = {
	&PKTINFO(r8s_fps),
	&PKTINFO(r8s_fps_porch),
	&PKTINFO(r8s_fps_freq),
	&PKTINFO(r8s_fps_porch_setting),
	&PKTINFO(r8s_panel_update),
};

static DEFINE_SEQINFO(r8s_set_bl_param_seq, r8s_set_bl_param_cmdtbl);
#if defined(__PANEL_NOT_USED_VARIABLE__)
static DEFINE_SEQINFO(r8s_set_gamma_para_seq, r8s_set_gamma_param_cmdtbl);
#endif
static DEFINE_SEQINFO(r8s_set_fps_param_seq, r8s_set_fps_param_cmdtbl);

static void *r8s_set_bl_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&SEQINFO(r8s_set_bl_param_seq),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
};

static void *r8s_set_fps_cmdtbl[] = {
	&CONDINFO_S(r8s_cond_is_panel_state_not_lpm),
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&SEQINFO(r8s_set_fps_param_seq),
	&SEQINFO(r8s_set_bl_param_seq),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
	&CONDINFO_E(r8s_cond_is_panel_state_not_lpm),
};

static void *r8s_display_on_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&PKTINFO(r8s_display_on),
	&KEYINFO(r8s_level1_key_disable),
};

static void *r8s_display_off_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&PKTINFO(r8s_display_off),
	&KEYINFO(r8s_level1_key_disable),
};

static void *r8s_exit_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
#ifdef CONFIG_DISPLAY_USE_INFO
	&ea8079_dmptbl[DUMP_DSI_ERR],
	&ea8079_dmptbl[DUMP_SELF_DIAG],
#endif
	&PKTINFO(r8s_err_fg_off),
	&PKTINFO(r8s_sleep_in),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
	&DLYINFO(r8s_wait_sleep_in),
};

static void *r8s_alpm_enter_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&PKTINFO(r8s_lpm_enter_mode),
	&PKTINFO(r8s_lpm_freq),
	&PKTINFO(r8s_lpm_nit),
	&PKTINFO(r8s_lpm_porch_0_on),
	&PKTINFO(r8s_lpm_porch_1_on),
	&PKTINFO(r8s_lpm_porch_2_on),
	&PKTINFO(r8s_lpm_porch_3_on),
	&PKTINFO(r8s_panel_update),
	&PKTINFO(r8s_lpm_on),
	&DLYINFO(r8s_wait_1usec),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
	&DLYINFO(r8s_wait_17msec),
};

static void *r8s_alpm_exit_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&PKTINFO(r8s_lpm_porch_0_off),
	&PKTINFO(r8s_lpm_porch_1_off),
	&PKTINFO(r8s_lpm_porch_2_off),
	&PKTINFO(r8s_lpm_porch_3_off),
	&PKTINFO(r8s_panel_update),
	&SEQINFO(r8s_set_bl_param_seq),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
};

static void *r8s_dump_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&ea8079_dmptbl[DUMP_RDDPM],
	&ea8079_dmptbl[DUMP_RDDSM],
	&ea8079_dmptbl[DUMP_DSI_ERR],
	&ea8079_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
};

#ifdef CONFIG_DYNAMIC_FREQ
static void *r8s_dynamic_ffc_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&PKTINFO(r8s_ffc_set_1),
	&PKTINFO(r8s_ffc_set_2),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
};

static void *r8s_dynamic_ffc_off_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&PKTINFO(r8s_ffc_off),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
};
#endif

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static void *r8s_fast_discharge_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&PKTINFO(r8s_fast_discharge),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
	&DLYINFO(r8s_wait_33msec),
};
#endif

static void *r8s_check_condition_cmdtbl[] = {
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&ea8079_dmptbl[DUMP_RDDPM],
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
};

#ifdef CONFIG_SUPPORT_MASK_LAYER
static void *r8s_mask_layer_workaround_cmdtbl[] = {
	&DLYINFO(r8s_wait_1_vsync),
};

static void *r8s_mask_layer_before_cmdtbl[] = {
	&DLYINFO(r8s_wait_1_vsync),
	&CONDINFO_S(r8s_cond_is_120hz),
	&DLYINFO(r8s_wait_1msec),
	&CONDINFO_E(r8s_cond_is_120hz),
};

static void *r8s_mask_layer_after_cmdtbl[] = {
	&CONDINFO_S(r8s_cond_is_120hz),
	&DLYINFO(r8s_wait_1_vsync),
	&CONDINFO_E(r8s_cond_is_120hz),
};
#endif

static void *r8s_dummy_cmdtbl[] = {
	NULL,
};

/* ============================= [EA8079 R8 DV INFO START] ============================= */

/* FOR DV PANEL */
static void *r8s_set_fps_dv_param_cmdtbl[] = {
	&PKTINFO(r8s_fps),
};

static DEFINE_SEQINFO(r8s_set_fps_dv_param_seq, r8s_set_fps_dv_param_cmdtbl);

static void *r8s_set_fps_dv_cmdtbl[] = {
	&CONDINFO_S(r8s_cond_is_panel_state_not_lpm),
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&SEQINFO(r8s_set_fps_dv_param_seq),
	&SEQINFO(r8s_set_bl_param_seq),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
	&CONDINFO_E(r8s_cond_is_panel_state_not_lpm),
};

/* FOR DV PANEL */
static void *r8s_init_dv_cmdtbl[] = {
	&DLYINFO(r8s_wait_10msec),
	&PKTINFO(r8s_sleep_out),
	&KEYINFO(r8s_level1_key_enable),
	&KEYINFO(r8s_level2_key_enable),
	&DLYINFO(r8s_wait_10msec),
	&PKTINFO(r8s_dsc),
	&PKTINFO(r8s_pps),
	&PKTINFO(r8s_caset),
	&PKTINFO(r8s_paset),
#ifdef CONFIG_DYNAMIC_FREQ
	&PKTINFO(r8s_ffc_set_1),
	&PKTINFO(r8s_ffc_set_2),
#endif
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	&PKTINFO(r8s_fast_discharge),
#endif
	&PKTINFO(r8s_vsync_set),
	&PKTINFO(r8s_pcd_det_set),
	&PKTINFO(r8s_smooth_dimming_setting),
	&PKTINFO(r8s_aod_setting),
	&SEQINFO(r8s_set_fps_dv_param_seq),
	&SEQINFO(r8s_set_bl_param_seq),
	&PKTINFO(r8s_te_offset),
	&PKTINFO(r8s_te_on),
	&KEYINFO(r8s_level2_key_disable),
	&KEYINFO(r8s_level1_key_disable),
	&DLYINFO(r8s_wait_sleep_out_110msec),
};

/* FOR DV PANEL */
static struct seqinfo r8s_dv_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", r8s_init_dv_cmdtbl),	/* DV PANEL */
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", r8s_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", r8s_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", r8s_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", r8s_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", r8s_exit_cmdtbl),
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("exit-seq", r8s_set_fps_dv_cmdtbl), /* DV PANEL */
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", r8s_alpm_enter_cmdtbl),
#ifdef CONFIG_DYNAMIC_FREQ
	[PANEL_DYNAMIC_FFC_SEQ] = SEQINFO_INIT("dynamic-ffc-seq", r8s_dynamic_ffc_cmdtbl),
	[PANEL_DYNAMIC_FFC_OFF_SEQ] = SEQINFO_INIT("dynamic-ffc-off-seq", r8s_dynamic_ffc_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MASK_LAYER
	[PANEL_MASK_LAYER_STOP_DIMMING_SEQ] = SEQINFO_INIT("mask-layer-before-seq", r8s_mask_layer_workaround_cmdtbl),
	[PANEL_MASK_LAYER_BEFORE_SEQ] = SEQINFO_INIT("mask-layer-before-seq", r8s_mask_layer_before_cmdtbl),
	[PANEL_MASK_LAYER_AFTER_SEQ] = SEQINFO_INIT("mask-layer-after-seq", r8s_mask_layer_after_cmdtbl),
#endif
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	[PANEL_FD_SEQ] = SEQINFO_INIT("fast-discharge-seq", r8s_fast_discharge_cmdtbl),
#endif
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", r8s_dump_cmdtbl),
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", r8s_check_condition_cmdtbl),
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", r8s_dummy_cmdtbl),
};

/* FOR DV PANEL */
struct common_panel_info ea8079_r8s_dv_panel_info = {
	.ldi_name = "ea8079",
	.name = "ea8079_r8s_dv",
	.model = "AMS646YB01",
	.vendor = "SDC",
	.id = 0x800040,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA),
		.err_fg_recovery = false,
		.support_vrr = true,
	},
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &ea8079_r8s_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(ea8079_r8s_default_resol),
		.resol = ea8079_r8s_default_resol,
	},
	.vrrtbl = ea8079_r8s_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(ea8079_r8s_default_vrrtbl),
	.maptbl = r8s_maptbl,
	.nr_maptbl = ARRAY_SIZE(r8s_maptbl),
	.seqtbl = r8s_dv_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(r8s_dv_seqtbl),
	.rditbl = ea8079_rditbl,
	.nr_rditbl = ARRAY_SIZE(ea8079_rditbl),
	.restbl = ea8079_restbl,
	.nr_restbl = ARRAY_SIZE(ea8079_restbl),
	.dumpinfo = ea8079_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(ea8079_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &ea8079_r8s_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &ea8079_r8s_panel_dimming_info,
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &ea8079_r8s_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_DYNAMIC_FREQ
	.df_freq_tbl = r8s_dynamic_freq_set,
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = NULL,
#endif
};
/* ============================= [EA8079 R8 DV INFO END] ============================= */


static struct seqinfo r8s_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", r8s_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", r8s_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", r8s_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", r8s_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", r8s_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", r8s_exit_cmdtbl),
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("exit-seq", r8s_set_fps_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", r8s_alpm_enter_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", r8s_alpm_exit_cmdtbl),
#ifdef CONFIG_DYNAMIC_FREQ
	[PANEL_DYNAMIC_FFC_SEQ] = SEQINFO_INIT("dynamic-ffc-seq", r8s_dynamic_ffc_cmdtbl),
	[PANEL_DYNAMIC_FFC_OFF_SEQ] = SEQINFO_INIT("dynamic-ffc-off-seq", r8s_dynamic_ffc_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MASK_LAYER
	[PANEL_MASK_LAYER_STOP_DIMMING_SEQ] = SEQINFO_INIT("mask-layer-before-seq", r8s_mask_layer_workaround_cmdtbl),
	[PANEL_MASK_LAYER_BEFORE_SEQ] = SEQINFO_INIT("mask-layer-before-seq", r8s_mask_layer_before_cmdtbl),
	[PANEL_MASK_LAYER_AFTER_SEQ] = SEQINFO_INIT("mask-layer-after-seq", r8s_mask_layer_after_cmdtbl),
#endif
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
	[PANEL_FD_SEQ] = SEQINFO_INIT("fast-discharge-seq", r8s_fast_discharge_cmdtbl),
#endif
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", r8s_dump_cmdtbl),
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", r8s_check_condition_cmdtbl),
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", r8s_dummy_cmdtbl),
};

struct common_panel_info ea8079_r8s_panel_info = {
	.ldi_name = "ea8079",
	.name = "ea8079_r8s_default",
	.model = "AMS646YB01",
	.vendor = "SDC",
	.id = 0x800042,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA),
		.err_fg_recovery = false,
		.support_vrr = true,
	},
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &ea8079_r8s_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(ea8079_r8s_default_resol),
		.resol = ea8079_r8s_default_resol,
	},
	.vrrtbl = ea8079_r8s_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(ea8079_r8s_default_vrrtbl),
	.maptbl = r8s_maptbl,
	.nr_maptbl = ARRAY_SIZE(r8s_maptbl),
	.seqtbl = r8s_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(r8s_seqtbl),
	.rditbl = ea8079_rditbl,
	.nr_rditbl = ARRAY_SIZE(ea8079_rditbl),
	.restbl = ea8079_restbl,
	.nr_restbl = ARRAY_SIZE(ea8079_restbl),
	.dumpinfo = ea8079_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(ea8079_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &ea8079_r8s_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &ea8079_r8s_panel_dimming_info,
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &ea8079_r8s_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_DYNAMIC_FREQ
	.df_freq_tbl = r8s_dynamic_freq_set,
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = NULL,
#endif
};

static int __init ea8079_r8s_panel_init(void)
{
	register_common_panel(&ea8079_r8s_dv_panel_info);
	register_common_panel(&ea8079_r8s_panel_info);

	return 0;
}
arch_initcall(ea8079_r8s_panel_init)
#endif /* __EA8079_R8S_PANEL_H__ */
