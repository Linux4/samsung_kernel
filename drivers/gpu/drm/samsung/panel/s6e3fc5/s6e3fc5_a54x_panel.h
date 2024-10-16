/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fc5/s6e3fc5_a54x_panel.h
 *
 * Header file for S6E3FC5 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FC5_A54X_PANEL_H__
#define __S6E3FC5_A54X_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "oled_function.h"
#include "s6e3fc5.h"
#include "s6e3fc5_a54x.h"
#include "s6e3fc5_dimming.h"
#ifdef CONFIG_USDM_MDNIE
#include "s6e3fc5_a54x_panel_mdnie.h"
#endif
#include "s6e3fc5_a54x_panel_dimming.h"
#ifdef CONFIG_USDM_PANEL_AOD_BL
#include "s6e3fc5_a54x_panel_aod_dimming.h"
#endif
#include "s6e3fc5_a54x_resol.h"

#ifdef CONFIG_USDM_PANEL_RCD
#include "s6e3fc5_a54x_rcd.h"
#endif

#undef __pn_name__
#define __pn_name__	a54x

#undef __PN_NAME__
#define __PN_NAME__

/* ===================================================================================== */
/* ============================= [S6E3FC5 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [S6E3FC5 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [S6E3FC5 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a54x_brt_table[S6E3FC5_TOTAL_STEP][2] = {
	{ 0x00, 0x02 }, { 0x00, 0x02 }, { 0x00, 0x04 }, { 0x00, 0x05 }, { 0x00, 0x06 },
	{ 0x00, 0x08 }, { 0x00, 0x09 }, { 0x00, 0x0B }, { 0x00, 0x0D }, { 0x00, 0x0F },
	{ 0x00, 0x11 }, { 0x00, 0x13 }, { 0x00, 0x15 }, { 0x00, 0x17 }, { 0x00, 0x19 },
	{ 0x00, 0x1B }, { 0x00, 0x1E }, { 0x00, 0x20 }, { 0x00, 0x22 }, { 0x00, 0x25 },
	{ 0x00, 0x27 }, { 0x00, 0x2A }, { 0x00, 0x2C }, { 0x00, 0x2F }, { 0x00, 0x31 },
	{ 0x00, 0x34 }, { 0x00, 0x36 }, { 0x00, 0x39 }, { 0x00, 0x3C }, { 0x00, 0x3E },
	{ 0x00, 0x41 }, { 0x00, 0x44 }, { 0x00, 0x47 }, { 0x00, 0x4A }, { 0x00, 0x4C },
	{ 0x00, 0x4F }, { 0x00, 0x52 }, { 0x00, 0x55 }, { 0x00, 0x58 }, { 0x00, 0x5B },
	{ 0x00, 0x5E }, { 0x00, 0x61 }, { 0x00, 0x64 }, { 0x00, 0x67 }, { 0x00, 0x6A },
	{ 0x00, 0x6D }, { 0x00, 0x70 }, { 0x00, 0x74 }, { 0x00, 0x77 }, { 0x00, 0x7A },
	{ 0x00, 0x7D }, { 0x00, 0x80 }, { 0x00, 0x84 }, { 0x00, 0x87 }, { 0x00, 0x8A },
	{ 0x00, 0x8D }, { 0x00, 0x91 }, { 0x00, 0x94 }, { 0x00, 0x97 }, { 0x00, 0x9B },
	{ 0x00, 0x9E }, { 0x00, 0xA1 }, { 0x00, 0xA5 }, { 0x00, 0xA8 }, { 0x00, 0xAC },
	{ 0x00, 0xAF }, { 0x00, 0xB3 }, { 0x00, 0xB6 }, { 0x00, 0xBA }, { 0x00, 0xBD },
	{ 0x00, 0xC1 }, { 0x00, 0xC4 }, { 0x00, 0xC8 }, { 0x00, 0xCC }, { 0x00, 0xCF },
	{ 0x00, 0xD3 }, { 0x00, 0xD6 }, { 0x00, 0xDA }, { 0x00, 0xDE }, { 0x00, 0xE1 },
	{ 0x00, 0xE5 }, { 0x00, 0xE9 }, { 0x00, 0xEC }, { 0x00, 0xF0 }, { 0x00, 0xF4 },
	{ 0x00, 0xF8 }, { 0x00, 0xFB }, { 0x00, 0xFF }, { 0x01, 0x03 }, { 0x01, 0x07 },
	{ 0x01, 0x0B }, { 0x01, 0x0E }, { 0x01, 0x12 }, { 0x01, 0x16 }, { 0x01, 0x1A },
	{ 0x01, 0x1E }, { 0x01, 0x22 }, { 0x01, 0x26 }, { 0x01, 0x2A }, { 0x01, 0x2E },
	{ 0x01, 0x32 }, { 0x01, 0x36 }, { 0x01, 0x39 }, { 0x01, 0x3D }, { 0x01, 0x41 },
	{ 0x01, 0x45 }, { 0x01, 0x49 }, { 0x01, 0x4D }, { 0x01, 0x52 }, { 0x01, 0x56 },
	{ 0x01, 0x5A }, { 0x01, 0x5E }, { 0x01, 0x62 }, { 0x01, 0x66 }, { 0x01, 0x6A },
	{ 0x01, 0x6E }, { 0x01, 0x72 }, { 0x01, 0x76 }, { 0x01, 0x7B }, { 0x01, 0x7F },
	{ 0x01, 0x83 }, { 0x01, 0x87 }, { 0x01, 0x8B }, { 0x01, 0x90 }, { 0x01, 0x94 },
	{ 0x01, 0x98 }, { 0x01, 0x9C }, { 0x01, 0xA0 }, { 0x01, 0xA5 }, { 0x01, 0xA9 },
	{ 0x01, 0xAD }, { 0x01, 0xB1 }, { 0x01, 0xB6 }, { 0x01, 0xBA }, { 0x01, 0xBE },
	{ 0x01, 0xC2 }, { 0x01, 0xC7 }, { 0x01, 0xCB }, { 0x01, 0xCF }, { 0x01, 0xD4 },
	{ 0x01, 0xD8 }, { 0x01, 0xDC }, { 0x01, 0xE1 }, { 0x01, 0xE5 }, { 0x01, 0xE9 },
	{ 0x01, 0xEE }, { 0x01, 0xF2 }, { 0x01, 0xF6 }, { 0x01, 0xFB }, { 0x01, 0xFF },
	{ 0x02, 0x04 }, { 0x02, 0x08 }, { 0x02, 0x0D }, { 0x02, 0x11 }, { 0x02, 0x15 },
	{ 0x02, 0x1A }, { 0x02, 0x1E }, { 0x02, 0x23 }, { 0x02, 0x27 }, { 0x02, 0x2C },
	{ 0x02, 0x30 }, { 0x02, 0x35 }, { 0x02, 0x39 }, { 0x02, 0x3E }, { 0x02, 0x43 },
	{ 0x02, 0x47 }, { 0x02, 0x4C }, { 0x02, 0x50 }, { 0x02, 0x55 }, { 0x02, 0x59 },
	{ 0x02, 0x5E }, { 0x02, 0x63 }, { 0x02, 0x67 }, { 0x02, 0x6C }, { 0x02, 0x70 },
	{ 0x02, 0x75 }, { 0x02, 0x7A }, { 0x02, 0x7E }, { 0x02, 0x83 }, { 0x02, 0x88 },
	{ 0x02, 0x8C }, { 0x02, 0x91 }, { 0x02, 0x96 }, { 0x02, 0x9A }, { 0x02, 0x9F },
	{ 0x02, 0xA4 }, { 0x02, 0xA8 }, { 0x02, 0xAD }, { 0x02, 0xB2 }, { 0x02, 0xB7 },
	{ 0x02, 0xBB }, { 0x02, 0xC0 }, { 0x02, 0xC5 }, { 0x02, 0xCA }, { 0x02, 0xCE },
	{ 0x02, 0xD3 }, { 0x02, 0xD8 }, { 0x02, 0xDD }, { 0x02, 0xE2 }, { 0x02, 0xE6 },
	{ 0x02, 0xEB }, { 0x02, 0xF0 }, { 0x02, 0xF5 }, { 0x02, 0xFA }, { 0x02, 0xFF },
	{ 0x03, 0x04 }, { 0x03, 0x08 }, { 0x03, 0x0D }, { 0x03, 0x12 }, { 0x03, 0x17 },
	{ 0x03, 0x1C }, { 0x03, 0x21 }, { 0x03, 0x26 }, { 0x03, 0x2B }, { 0x03, 0x30 },
	{ 0x03, 0x34 }, { 0x03, 0x39 }, { 0x03, 0x3E }, { 0x03, 0x43 }, { 0x03, 0x48 },
	{ 0x03, 0x4D }, { 0x03, 0x52 }, { 0x03, 0x57 }, { 0x03, 0x5C }, { 0x03, 0x61 },
	{ 0x03, 0x66 }, { 0x03, 0x6B }, { 0x03, 0x70 }, { 0x03, 0x75 }, { 0x03, 0x7A },
	{ 0x03, 0x7F }, { 0x03, 0x84 }, { 0x03, 0x89 }, { 0x03, 0x8E }, { 0x03, 0x93 },
	{ 0x03, 0x98 }, { 0x03, 0x9E }, { 0x03, 0xA3 }, { 0x03, 0xA8 }, { 0x03, 0xAD },
	{ 0x03, 0xB2 }, { 0x03, 0xB7 }, { 0x03, 0xBC }, { 0x03, 0xC1 }, { 0x03, 0xC6 },
	{ 0x03, 0xCB }, { 0x03, 0xD1 }, { 0x03, 0xD6 }, { 0x03, 0xDB }, { 0x03, 0xE0 },
	{ 0x03, 0xE5 }, { 0x03, 0xEA }, { 0x03, 0xEF }, { 0x03, 0xF5 }, { 0x03, 0xFA },
	{ 0x03, 0xFF },
	{ 0x00, 0x04 }, { 0x00, 0x08 }, { 0x00, 0x0C }, { 0x00, 0x10 }, { 0x00, 0x14 },
	{ 0x00, 0x18 }, { 0x00, 0x1C }, { 0x00, 0x20 }, { 0x00, 0x24 }, { 0x00, 0x28 },
	{ 0x00, 0x2C }, { 0x00, 0x30 }, { 0x00, 0x34 }, { 0x00, 0x38 }, { 0x00, 0x3C },
	{ 0x00, 0x40 }, { 0x00, 0x44 }, { 0x00, 0x48 }, { 0x00, 0x4C }, { 0x00, 0x50 },
	{ 0x00, 0x54 }, { 0x00, 0x58 }, { 0x00, 0x5C }, { 0x00, 0x60 }, { 0x00, 0x64 },
	{ 0x00, 0x68 }, { 0x00, 0x6C }, { 0x00, 0x70 }, { 0x00, 0x74 }, { 0x00, 0x78 },
	{ 0x00, 0x7C }, { 0x00, 0x80 }, { 0x00, 0x84 }, { 0x00, 0x88 }, { 0x00, 0x8C },
	{ 0x00, 0x90 }, { 0x00, 0x94 }, { 0x00, 0x98 }, { 0x00, 0x9C }, { 0x00, 0xA0 },
	{ 0x00, 0xA4 }, { 0x00, 0xA8 }, { 0x00, 0xAD }, { 0x00, 0xB1 }, { 0x00, 0xB5 },
	{ 0x00, 0xB9 }, { 0x00, 0xBD }, { 0x00, 0xC1 }, { 0x00, 0xC5 }, { 0x00, 0xC9 },
	{ 0x00, 0xCD }, { 0x00, 0xD1 }, { 0x00, 0xD5 }, { 0x00, 0xD9 }, { 0x00, 0xDD },
	{ 0x00, 0xE1 }, { 0x00, 0xE5 }, { 0x00, 0xE9 }, { 0x00, 0xED }, { 0x00, 0xF1 },
	{ 0x00, 0xF5 }, { 0x00, 0xF9 }, { 0x00, 0xFD }, { 0x01, 0x01 }, { 0x01, 0x05 },
	{ 0x01, 0x09 }, { 0x01, 0x0D }, { 0x01, 0x11 }, { 0x01, 0x15 }, { 0x01, 0x19 },
	{ 0x01, 0x1D }, { 0x01, 0x21 }, { 0x01, 0x25 }, { 0x01, 0x29 }, { 0x01, 0x2D },
	{ 0x01, 0x31 }, { 0x01, 0x35 }, { 0x01, 0x39 }, { 0x01, 0x3D }, { 0x01, 0x41 },
	{ 0x01, 0x45 }, { 0x01, 0x49 }, { 0x01, 0x4D }, { 0x01, 0x51 }, { 0x01, 0x55 },
	{ 0x01, 0x59 }, { 0x01, 0x5D }, { 0x01, 0x61 }, { 0x01, 0x65 }, { 0x01, 0x69 },
	{ 0x01, 0x6D }, { 0x01, 0x71 }, { 0x01, 0x75 }, { 0x01, 0x79 }, { 0x01, 0x7D },
	{ 0x01, 0x81 }, { 0x01, 0x85 }, { 0x01, 0x89 }, { 0x01, 0x8D }, { 0x01, 0x91 },
	{ 0x01, 0x95 }, { 0x01, 0x99 }, { 0x01, 0x9D }, { 0x01, 0xA1 }, { 0x01, 0xA5 },
	{ 0x01, 0xA9 }, { 0x01, 0xAD }, { 0x01, 0xB1 }, { 0x01, 0xB5 }, { 0x01, 0xB9 },
	{ 0x01, 0xBD }, { 0x01, 0xC1 }, { 0x01, 0xC5 }, { 0x01, 0xC9 }, { 0x01, 0xCD },
	{ 0x01, 0xD1 }, { 0x01, 0xD5 }, { 0x01, 0xD9 }, { 0x01, 0xDD }, { 0x01, 0xE1 },
	{ 0x01, 0xE5 }, { 0x01, 0xE9 }, { 0x01, 0xED }, { 0x01, 0xF1 }, { 0x01, 0xF5 },
	{ 0x01, 0xF9 }, { 0x01, 0xFD }, { 0x02, 0x02 }, { 0x02, 0x06 }, { 0x02, 0x0A },
	{ 0x02, 0x0E }, { 0x02, 0x12 }, { 0x02, 0x16 }, { 0x02, 0x1A }, { 0x02, 0x1E },
	{ 0x02, 0x22 }, { 0x02, 0x26 }, { 0x02, 0x2A }, { 0x02, 0x2E }, { 0x02, 0x32 },
	{ 0x02, 0x36 }, { 0x02, 0x3A }, { 0x02, 0x3E }, { 0x02, 0x42 }, { 0x02, 0x46 },
	{ 0x02, 0x4A }, { 0x02, 0x4E }, { 0x02, 0x52 }, { 0x02, 0x56 }, { 0x02, 0x5A },
	{ 0x02, 0x5E }, { 0x02, 0x62 }, { 0x02, 0x66 }, { 0x02, 0x6A }, { 0x02, 0x6E },
	{ 0x02, 0x72 }, { 0x02, 0x76 }, { 0x02, 0x7A }, { 0x02, 0x7E }, { 0x02, 0x82 },
	{ 0x02, 0x86 }, { 0x02, 0x8A }, { 0x02, 0x8E }, { 0x02, 0x92 }, { 0x02, 0x96 },
	{ 0x02, 0x9A }, { 0x02, 0x9E }, { 0x02, 0xA2 }, { 0x02, 0xA6 }, { 0x02, 0xAA },
	{ 0x02, 0xAE }, { 0x02, 0xB2 }, { 0x02, 0xB6 }, { 0x02, 0xBA }, { 0x02, 0xBE },
	{ 0x02, 0xC2 }, { 0x02, 0xC6 }, { 0x02, 0xCA }, { 0x02, 0xCE }, { 0x02, 0xD2 },
	{ 0x02, 0xD6 }, { 0x02, 0xDA }, { 0x02, 0xDE }, { 0x02, 0xE2 }, { 0x02, 0xE6 },
	{ 0x02, 0xEA }, { 0x02, 0xEE }, { 0x02, 0xF2 }, { 0x02, 0xF6 }, { 0x02, 0xFA },
	{ 0x02, 0xFE }, { 0x03, 0x02 }, { 0x03, 0x06 }, { 0x03, 0x0A }, { 0x03, 0x0E },
	{ 0x03, 0x12 }, { 0x03, 0x16 }, { 0x03, 0x1A }, { 0x03, 0x1E }, { 0x03, 0x22 },
	{ 0x03, 0x26 }, { 0x03, 0x2A }, { 0x03, 0x2E }, { 0x03, 0x32 }, { 0x03, 0x36 },
	{ 0x03, 0x3A }, { 0x03, 0x3E }, { 0x03, 0x42 }, { 0x03, 0x46 }, { 0x03, 0x4A },
	{ 0x03, 0x4E }, { 0x03, 0x52 }, { 0x03, 0x57 }, { 0x03, 0x5B }, { 0x03, 0x5F },
	{ 0x03, 0x63 }, { 0x03, 0x67 }, { 0x03, 0x6B }, { 0x03, 0x6F }, { 0x03, 0x73 },
	{ 0x03, 0x77 }, { 0x03, 0x7B }, { 0x03, 0x7F }, { 0x03, 0x83 }, { 0x03, 0x87 },
	{ 0x03, 0x8B }, { 0x03, 0x8F }, { 0x03, 0x93 }, { 0x03, 0x97 }, { 0x03, 0x9B },
	{ 0x03, 0x9F }, { 0x03, 0xA3 }, { 0x03, 0xA7 }, { 0x03, 0xAB }, { 0x03, 0xAF },
	{ 0x03, 0xB3 }, { 0x03, 0xB7 }, { 0x03, 0xBB }, { 0x03, 0xBF }, { 0x03, 0xC3 },
	{ 0x03, 0xC7 }, { 0x03, 0xCB }, { 0x03, 0xCF }, { 0x03, 0xD3 }, { 0x03, 0xD7 },
	{ 0x03, 0xDB }, { 0x03, 0xDF }, { 0x03, 0xE3 }, { 0x03, 0xE7 }, { 0x03, 0xEB },
	{ 0x03, 0xEF }, { 0x03, 0xF3 }, { 0x03, 0xF7 }, { 0x03, 0xFB }, { 0x03, 0xFF },
};

static u8 a54x_hbm_transition_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
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
		{ 0xE0 }, /* No smooth dimming in HBM */
	}
};

static u8 a54x_acl_opr_table[MAX_PANEL_HBM][MAX_S6E3FC5_ACL_OPR][1] = {
	[PANEL_HBM_OFF] = {
		[S6E3FC5_ACL_OPR_0] = { 0x00 }, /* ACL OFF, OPR 0% */
		[S6E3FC5_ACL_OPR_1] = { 0x01 }, /* ACL ON, OPR 8% */
		[S6E3FC5_ACL_OPR_2] = { 0x03 }, /* ACL ON, OPR 15% */
		[S6E3FC5_ACL_OPR_3] = { 0x03 }, /* ACL ON, OPR 15% */
	},
	[PANEL_HBM_ON] = {
		[S6E3FC5_ACL_OPR_0] = { 0x00 }, /* ACL ON, OPR 0% */
		[S6E3FC5_ACL_OPR_1] = { 0x01 }, /* ACL ON, OPR 8% */
		[S6E3FC5_ACL_OPR_2] = { 0x01 }, /* ACL ON, OPR 8% */
		[S6E3FC5_ACL_OPR_3] = { 0x01 }, /* ACL ON, OPR 8% */
	},
};

static u8 a54x_lpm_nit_table[4][1] = {
	/* LPM 2NIT: */
	{ 0x27 },
	/* LPM 10NIT */
	{ 0x26 },
	/* LPM 30NIT */
	{ 0x25  },
	/* LPM 60NIT */
	{ 0x24 },
};

static u8 a54x_lpm_on_table[2][1] = {
	[ALPM_MODE] = { 0x23 },
	[HLPM_MODE] = { 0x23 },
};

static u8 a54x_ffc_table[MAX_S6E3FC5_HS_CLK][2] = {
	[S6E3FC5_HS_CLK_1108] = { 0x68, 0xB9 }, // FFC for HS: 1108
	[S6E3FC5_HS_CLK_1124] = { 0x67, 0x3B }, // FFC for HS: 1124
	[S6E3FC5_HS_CLK_1125] = { 0x67, 0x24 }, // FFC for HS: 1125
};

static u8 a54x_fps_table[MAX_S6E3FC5_VRR][1] = {
	[S6E3FC5_VRR_120HS] = { 0x00 },
	[S6E3FC5_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x00 },
	[S6E3FC5_VRR_60HS] = { 0x08 },
	[S6E3FC5_VRR_30NS] = { 0x00 },
};

static u8 a54x_te_frame_sel_table[MAX_S6E3FC5_VRR][12] = {
	[S6E3FC5_VRR_120HS] = {
		0x04, 0x00, 0x00, 0x00, 0x06, 0xF9, 0x00, 0x0F, 0x09, 0x0F, 0x00, 0x0F },
	[S6E3FC5_VRR_60HS_120HS_TE_HW_SKIP_1] = {
		0x51, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0xF9, 0x00, 0x0F },
	[S6E3FC5_VRR_60HS] = { 0x04, 0x00, 0x00, 0x00, 0x06, 0xF9, 0x00, 0x0F, 0x09, 0x0F, 0x00, 0x0F },
	[S6E3FC5_VRR_30NS] = { 0x04, 0x00, 0x00, 0x00, 0x06, 0xF9, 0x00, 0x0F, 0x09, 0x0F, 0x00, 0x0F },
};

static u8 a54x_irc_mode_table[][7] = {
	{ 0x27, 0xE3, 0xFE, 0x8B, 0x00, 0x80, 0x01 },		/* moderato */
	{ 0x27, 0xA3, 0xFE, 0x8B, 0x8A, 0x80, 0x01 },		/* flat gamma */
};

static u8 a54x_analog_gamma_table[S6E3FC5_TOTAL_STEP][S6E3FC5_ANALOG_GAMMA_LEN] = { 0, };

static struct maptbl a54x_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = DEFINE_2D_MAPTBL(a54x_brt_table, &DDI_FUNC(S6E3FC5_MAPTBL_INIT_GAMMA_MODE2_BRT), &OLED_FUNC(OLED_MAPTBL_GETIDX_GM2_BRT), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[HBM_ONOFF_MAPTBL] = DEFINE_3D_MAPTBL(a54x_hbm_transition_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC5_MAPTBL_GETIDX_HBM_TRANSITION), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[ACL_OPR_MAPTBL] = DEFINE_3D_MAPTBL(a54x_acl_opr_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC5_MAPTBL_GETIDX_ACL_OPR), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(a54x_tset_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), NULL, &OLED_FUNC(OLED_MAPTBL_COPY_TSET)),
	[LPM_NIT_MAPTBL] = DEFINE_2D_MAPTBL(a54x_lpm_nit_table, &DDI_FUNC(S6E3FC5_MAPTBL_INIT_LPM_BRT), &DDI_FUNC(S6E3FC5_MAPTBL_GETIDX_LPM_BRT), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[ANALOG_GAMMA_MAPTBL] = DEFINE_2D_MAPTBL(a54x_analog_gamma_table, &DDI_FUNC(S6E3FC5_MAPTBL_INIT_ANALOG_GAMMA), &OLED_FUNC(OLED_MAPTBL_GETIDX_GM2_BRT), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[TE_FRAME_SEL_MAPTBL] = DEFINE_2D_MAPTBL(a54x_te_frame_sel_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC5_MAPTBL_GETIDX_VRR), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[FPS_MAPTBL] = DEFINE_2D_MAPTBL(a54x_fps_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC5_MAPTBL_GETIDX_VRR), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[SET_FFC_MAPTBL] = DEFINE_2D_MAPTBL(a54x_ffc_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC5_MAPTBL_GETIDX_FFC), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
	[IRC_MODE_MAPTBL] = DEFINE_2D_MAPTBL(a54x_irc_mode_table, &OLED_FUNC(OLED_MAPTBL_INIT_DEFAULT), &DDI_FUNC(S6E3FC5_MAPTBL_GETIDX_IRC_MODE), &OLED_FUNC(OLED_MAPTBL_COPY_DEFAULT)),
};

/* ===================================================================================== */
/* ============================== [S6E3FC5 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A54X_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 A54X_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 A54X_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };

static u8 A54X_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 A54X_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 A54X_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 A54X_SLEEP_OUT[] = { 0x11 };
static u8 A54X_SLEEP_IN[] = { 0x10 };
static u8 A54X_DISPLAY_OFF[] = { 0x28 };
static u8 A54X_DISPLAY_ON[] = { 0x29 };
static u8 A54X_TE_ON[] = { 0x35, 0x00, 0x00 };

static DEFINE_STATIC_PACKET(a54x_level1_key_enable, DSI_PKT_TYPE_WR, A54X_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(a54x_level2_key_enable, DSI_PKT_TYPE_WR, A54X_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(a54x_level3_key_enable, DSI_PKT_TYPE_WR, A54X_KEY3_ENABLE, 0);

static DEFINE_STATIC_PACKET(a54x_level1_key_disable, DSI_PKT_TYPE_WR, A54X_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(a54x_level2_key_disable, DSI_PKT_TYPE_WR, A54X_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(a54x_level3_key_disable, DSI_PKT_TYPE_WR, A54X_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(a54x_sleep_out, DSI_PKT_TYPE_WR, A54X_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a54x_sleep_in, DSI_PKT_TYPE_WR, A54X_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a54x_display_on, DSI_PKT_TYPE_WR, A54X_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a54x_display_off, DSI_PKT_TYPE_WR, A54X_DISPLAY_OFF, 0);

static DEFINE_STATIC_PACKET(a54x_te_on, DSI_PKT_TYPE_WR, A54X_TE_ON, 0);

static u8 A54X_CMD_FIFO_SETTING[] = { 0xF8, 0x02 };
static DEFINE_STATIC_PACKET(a54x_cmd_fifo_setting, DSI_PKT_TYPE_WR, A54X_CMD_FIFO_SETTING, 0x1C);

static u8 A54X_TSET_SET[] = {
	0xB5,
	0x00,
};
static DEFINE_PKTUI(a54x_tset_set, &a54x_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a54x_tset_set, DSI_PKT_TYPE_WR, A54X_TSET_SET, 0x01);

static u8 A54X_AOD_AOR_MAX[] = { 0x66, 0x01, 0x16, 0xA8 };
static DEFINE_STATIC_PACKET(a54x_aod_aor_max, DSI_PKT_TYPE_WR, A54X_AOD_AOR_MAX, 0x52);

static u8 A54X_AOD_FREQ[] = { 0x60, 0x00, 0x00 };
static DEFINE_STATIC_PACKET(a54x_aod_freq, DSI_PKT_TYPE_WR, A54X_AOD_FREQ, 0);

static u8 A54X_AOD_VAINT[] = { 0xF4, 0x28 };
static DEFINE_STATIC_PACKET(a54x_aod_vaint, DSI_PKT_TYPE_WR, A54X_AOD_VAINT, 0x7A);

static u8 A54X_AOD_SETTING[] = { 0x68, 0x02 };
static DEFINE_STATIC_PACKET(a54x_aod_setting, DSI_PKT_TYPE_WR, A54X_AOD_SETTING, 0);

static u8 A54X_AOD_AOR[] = { 0x66, 0x00, 0x00, 0x18 };
static DEFINE_STATIC_PACKET(a54x_aod_aor, DSI_PKT_TYPE_WR, A54X_AOD_AOR, 0x52);

static u8 A54X_LPM_PORCH_ON[] = { 0xF2, 0x02, 0x56 };
static DEFINE_STATIC_PACKET(a54x_lpm_porch_on, DSI_PKT_TYPE_WR, A54X_LPM_PORCH_ON, 0x1F);

static u8 A54X_SWIRE_NO_PULSE[] = { 0xB5, 0x00 };
static DEFINE_STATIC_PACKET(a54x_swire_no_pulse, DSI_PKT_TYPE_WR, A54X_SWIRE_NO_PULSE, 0x0A);

static u8 A54X_SWIRE_NO_PULSE_LPM_OFF[] = { 0xB5, 0x00, 0x00, 0x00 };
static DEFINE_STATIC_PACKET(a54x_swire_no_pulse_lpm_off, DSI_PKT_TYPE_WR, A54X_SWIRE_NO_PULSE_LPM_OFF, 0x08);

static u8 A54X_LPM_OFF_SYNC_CTRL[] = {
	0x66, 0x10
};
static DEFINE_STATIC_PACKET(a54x_lpm_off_sync_ctrl, DSI_PKT_TYPE_WR, A54X_LPM_OFF_SYNC_CTRL, 0x0F);

static u8 A54X_LPM_PORCH_OFF[] = { 0xF2, 0x02, 0x7E };
static DEFINE_STATIC_PACKET(a54x_lpm_porch_off, DSI_PKT_TYPE_WR, A54X_LPM_PORCH_OFF, 0x1F);

static u8 A54X_NORMAL_MODE[] = { 0x53, 0x20 };
static DEFINE_STATIC_PACKET(a54x_normal_mode, DSI_PKT_TYPE_WR, A54X_NORMAL_MODE, 0);

static u8 A54X_NORMAL_SETTING[] = { 0x68, 0x02, 0x01 };
static DEFINE_STATIC_PACKET(a54x_normal_setting, DSI_PKT_TYPE_WR, A54X_NORMAL_SETTING, 0);

static u8 A54X_BLACK_FRAME_OFF[] = { 0xBB, 0x05, 0x0C, 0x0C };
static DEFINE_STATIC_PACKET(a54x_black_frame_off, DSI_PKT_TYPE_WR, A54X_BLACK_FRAME_OFF, 0x03);

static u8 A54X_BLACK_FRAME_ON[] = { 0xBB, 0x05, 0x00, 0x0C };
static DEFINE_STATIC_PACKET(a54x_black_frame_on, DSI_PKT_TYPE_WR, A54X_BLACK_FRAME_ON, 0x03);

static u8 A54X_LPM_NIT[] = { 0x53, 0x27 };
static DEFINE_PKTUI(a54x_lpm_nit, &a54x_maptbl[LPM_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a54x_lpm_nit, DSI_PKT_TYPE_WR, A54X_LPM_NIT, 0x00);

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static u8 A54X_DECODER_TEST_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static DEFINE_STATIC_PACKET(a54x_decoder_test_caset, DSI_PKT_TYPE_WR, A54X_DECODER_TEST_CASET, 0x00);

static u8 A54X_DECODER_TEST_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x23 };
static DEFINE_STATIC_PACKET(a54x_decoder_test_paset, DSI_PKT_TYPE_WR, A54X_DECODER_TEST_PASET, 0x00);

static u8 A54X_DECODER_TEST_2C[] = { 0x2C, 0x00 };
static DEFINE_STATIC_PACKET(a54x_decoder_test_2c, DSI_PKT_TYPE_WR, A54X_DECODER_TEST_2C, 0x00);

static u8 A54X_DECODER_CRC_PATTERN_ENABLE[] = { 0xBE, 0x07 };
static DEFINE_STATIC_PACKET(a54x_decoder_crc_pattern_enable, DSI_PKT_TYPE_WR, A54X_DECODER_CRC_PATTERN_ENABLE, 0x00);

static u8 A54X_DECODER_READ_SET_1[] = { 0xD8, 0x11 };
static DEFINE_STATIC_PACKET(a54x_decoder_read_set_1, DSI_PKT_TYPE_WR, A54X_DECODER_READ_SET_1, 0x27);

static u8 A54X_DECODER_READ_SET_2[] = { 0xD8, 0x20 };
static DEFINE_STATIC_PACKET(a54x_decoder_read_set_2, DSI_PKT_TYPE_WR, A54X_DECODER_READ_SET_2, 0x27);

static u8 A54X_DECODER_VDDM_LOW_SET_1[] = { 0xD7, 0x04 };
static DEFINE_STATIC_PACKET(a54x_decoder_vddm_low_set_1, DSI_PKT_TYPE_WR, A54X_DECODER_VDDM_LOW_SET_1, 0x02);

static u8 A54X_DECODER_VDDM_LOW_SET_2[] = { 0xF4, 0x07 };
static DEFINE_STATIC_PACKET(a54x_decoder_vddm_low_set_2, DSI_PKT_TYPE_WR, A54X_DECODER_VDDM_LOW_SET_2, 0x0F);

static u8 A54X_DECODER_FUSING_UPDATE_1[] = { 0xFE, 0xB0 };
static DEFINE_STATIC_PACKET(a54x_decoder_fusing_update_1, DSI_PKT_TYPE_WR, A54X_DECODER_FUSING_UPDATE_1, 0x00);

static u8 A54X_DECODER_FUSING_UPDATE_2[] = { 0xFE, 0x30 };
static DEFINE_STATIC_PACKET(a54x_decoder_fusing_update_2, DSI_PKT_TYPE_WR, A54X_DECODER_FUSING_UPDATE_2, 0x0F);

static u8 A54X_DECODER_CRC_PATERN_DISABLE[] = { 0xBE, 0x00 };
static DEFINE_STATIC_PACKET(a54x_decoder_crc_pattern_disable, DSI_PKT_TYPE_WR, A54X_DECODER_CRC_PATERN_DISABLE, 0x00);

static u8 A54X_DECODER_VDDM_RETURN_SET_1[] = { 0xD7, 0x00 };
static DEFINE_STATIC_PACKET(a54x_decoder_vddm_return_set_1, DSI_PKT_TYPE_WR, A54X_DECODER_VDDM_RETURN_SET_1, 0x02);

static u8 A54X_DECODER_VDDM_RETURN_SET_2[] = { 0xF4, 0x00 };
static DEFINE_STATIC_PACKET(a54x_decoder_vddm_return_set_2, DSI_PKT_TYPE_WR, A54X_DECODER_VDDM_RETURN_SET_2, 0x0F);
#endif

static DEFINE_PANEL_VSYNC_DELAY(a54x_wait_1_vsync, 1);
static DEFINE_PANEL_FRAME_DELAY(a54x_wait_2_frame, 2);
static DEFINE_RULE_BASED_COND(a54x_cond_is_brightness_ge_260,
		PANEL_BL_PROPERTY_BRIGHTNESS, GE, 260);
/* 120hs, 60phs */
static DEFINE_RULE_BASED_COND(a54x_cond_is_120hz,
		PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 120);
/* 60hs */
static DEFINE_RULE_BASED_COND(a54x_cond_is_60hz,
		PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 60);
static DEFINE_FUNC_BASED_COND(a54x_cond_is_panel_refresh_rate_changed,
		&OLED_FUNC(OLED_COND_IS_PANEL_REFRESH_RATE_CHANGED));
static DEFINE_RULE_BASED_COND(a54x_cond_is_panel_state_not_lpm,
		PANEL_PROPERTY_PANEL_STATE, NE, PANEL_STATE_ALPM);
static DEFINE_RULE_BASED_COND(a54x_cond_is_panel_state_lpm,
		PANEL_PROPERTY_PANEL_STATE, EQ, PANEL_STATE_ALPM);

#ifdef CONFIG_USDM_PANEL_MASK_LAYER
static DEFINE_PANEL_MDELAY(a54x_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(a54x_wait_9msec, 9);
static DEFINE_PANEL_MDELAY(a54x_wait_7msec, 7);
static DEFINE_PANEL_MDELAY(a54x_wait_3msec, 3);
#endif

static DEFINE_PANEL_MDELAY(a54x_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(a54x_wait_ccd_test, 20);
static DEFINE_PANEL_MDELAY(a54x_wait_crc_test_50msec, 50);
static DEFINE_PANEL_MDELAY(a54x_wait_30msec, 30);
static DEFINE_PANEL_MDELAY(a54x_wait_17msec, 17);
static DEFINE_PANEL_MDELAY(a54x_wait_dsc_test, 20);

static DEFINE_PANEL_MDELAY(a54x_wait_34msec, 34);

static DEFINE_PANEL_MDELAY(a54x_wait_sleep_out_90msec, 90);
static DEFINE_PANEL_MDELAY(a54x_wait_display_off, 20);

static DEFINE_PANEL_MDELAY(a54x_wait_sleep_in, 120);
static DEFINE_PANEL_UDELAY(a54x_wait_1usec, 1);

static DEFINE_PANEL_KEY(a54x_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(a54x_level1_key_enable));
static DEFINE_PANEL_KEY(a54x_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(a54x_level2_key_enable));
static DEFINE_PANEL_KEY(a54x_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(a54x_level3_key_enable));

static DEFINE_PANEL_KEY(a54x_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(a54x_level1_key_disable));
static DEFINE_PANEL_KEY(a54x_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(a54x_level2_key_disable));
static DEFINE_PANEL_KEY(a54x_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(a54x_level3_key_disable));

static u8 A54X_HBM_TRANSITION[] = {
	0x53, 0x20
};
static DEFINE_PKTUI(a54x_hbm_transition, &a54x_maptbl[HBM_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a54x_hbm_transition, DSI_PKT_TYPE_WR, A54X_HBM_TRANSITION, 0);

static u8 A54X_ACL_SETTING[] = {
	0x66,
	0x50, 0x50,		/* 32 Frame Avg. */
	0x15, 0x55, 0x55, 0x55, 0x08, 0xF1,
	0xC6, 0x48, 0x30, 0x00, 0x51, 0x66, 0x98, 0x00,
	0x20,	/* 32 Frame Dimming */
	0x10,
	0xB0,	/* Start Step 50% */
};
static DEFINE_STATIC_PACKET(a54x_acl_setting, DSI_PKT_TYPE_WR, A54X_ACL_SETTING, 0x3F);

static u8 A54X_ACL_SET[] = {
	0x66,
	0x01, 0x6D,
};
static DEFINE_STATIC_PACKET(a54x_acl_set, DSI_PKT_TYPE_WR, A54X_ACL_SET, 0);

static u8 A54X_ACL_CONTROL[] = {
	0x55, 0x01
};
static DEFINE_PKTUI(a54x_acl_control, &a54x_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a54x_acl_control, DSI_PKT_TYPE_WR, A54X_ACL_CONTROL, 0);

static u8 A54X_ACL_DIM_OFF[] = {
	0x55,
	0x00
};
static DEFINE_STATIC_PACKET(a54x_acl_dim_off, DSI_PKT_TYPE_WR, A54X_ACL_DIM_OFF, 0);

static u8 A54X_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(a54x_wrdisbv, &a54x_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a54x_wrdisbv, DSI_PKT_TYPE_WR, A54X_WRDISBV, 0);

static u8 A54X_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 A54X_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x23 };
static DEFINE_STATIC_PACKET(a54x_caset, DSI_PKT_TYPE_WR, A54X_CASET, 0);
static DEFINE_STATIC_PACKET(a54x_paset, DSI_PKT_TYPE_WR, A54X_PASET, 0);

static u8 A54X_PCD_SET_DET_LOW[] = {
	0xCC,
	0x02, 0x5C, 0x51	/* 1st 0x5C: default high, 2nd 0x51 : Enable SW RESET */
};
static DEFINE_STATIC_PACKET(a54x_pcd_setting, DSI_PKT_TYPE_WR, A54X_PCD_SET_DET_LOW, 0);

static u8 A54X_TE_SHIFT[] = {
	0xB9,
	//0x04, 0x00, 0x00, 0x00, 0x09, 0x0f, 0x00, 0x0F // 130us
	0x04, 0x00, 0x00, 0x00, 0x06, 0xf9, 0x00, 0x0F // 2msec
};
static DEFINE_STATIC_PACKET(a54x_te_shift, DSI_PKT_TYPE_WR, A54X_TE_SHIFT, 0);

static u8 A54X_TE_SHIFT_HLPM[] = {
	0xB9,
	0x04, 0x00, 0x00, 0x00, 0x09, 0x1D, 0x00, 0x0F // 300us
};
static DEFINE_STATIC_PACKET(a54x_te_shift_hlpm, DSI_PKT_TYPE_WR, A54X_TE_SHIFT_HLPM, 0);

static u8 A54X_ERR_FG_ENABLE[] = { 0xE5, 0x15 };
static DEFINE_STATIC_PACKET(a54x_err_fg_enable, DSI_PKT_TYPE_WR, A54X_ERR_FG_ENABLE, 0);

static u8 A54X_ERR_FG_DISABLE[] = { 0xE5, 0x05 };
static DEFINE_STATIC_PACKET(a54x_err_fg_disable, DSI_PKT_TYPE_WR, A54X_ERR_FG_DISABLE, 0);

static u8 A54X_ERR_FG_SETTING[] = {
	/* Defalut LOW */
	0xED, 0x04,
};
static DEFINE_STATIC_PACKET(a54x_err_fg_setting, DSI_PKT_TYPE_WR, A54X_ERR_FG_SETTING, 0);

static u8 A54X_ELVSS_TIMING_SETTING[] = {
	0xB5, 0x48,
};
static DEFINE_STATIC_PACKET(a54x_elvss_timing_setting, DSI_PKT_TYPE_WR, A54X_ELVSS_TIMING_SETTING, 0);

static u8 A54X_TSP_VSYNC_ENABLE1[] = {
	0xF2, 0xE8,
};
static u8 A54X_TSP_VSYNC_ENABLE2[] = {
	0xB9,
	0xB8, 0x6E
};
static u8 A54X_TSP_VSYNC_ENABLE3[] = {
	0xB9,
	0x00, 0x00, 0x04, 0x3D
};
static DEFINE_STATIC_PACKET(a54x_tsp_vsync_enable1, DSI_PKT_TYPE_WR, A54X_TSP_VSYNC_ENABLE1, 0x05);
static DEFINE_STATIC_PACKET(a54x_tsp_vsync_enable2, DSI_PKT_TYPE_WR, A54X_TSP_VSYNC_ENABLE2, 0x64);
static DEFINE_STATIC_PACKET(a54x_tsp_vsync_enable3, DSI_PKT_TYPE_WR, A54X_TSP_VSYNC_ENABLE3, 0x6A);

static u8 A54x_IRC_MDOE[] = {
	0x6A,
	0x27, 0xE3, 0xFE, 0x8B, 0x00, 0x80, 0x01
};
static DEFINE_PKTUI(a54x_irc_mode, &a54x_maptbl[IRC_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a54x_irc_mode, DSI_PKT_TYPE_WR, A54x_IRC_MDOE, 0);

static u8 A54X_FPS[] = { 0x60, 0x08, 0x00 };
static DEFINE_PKTUI(a54x_fps, &a54x_maptbl[FPS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a54x_fps, DSI_PKT_TYPE_WR, A54X_FPS, 0);

static u8 A54X_GLUT_SET_SEL_TUNE[] = { 0x68, 0x05 };
static DEFINE_STATIC_PACKET(a54x_glut_set_sel_tune, DSI_PKT_TYPE_WR, A54X_GLUT_SET_SEL_TUNE, 0x06);

static u8 A54X_GLUT_SET_SEL_ORIG[] = { 0x68, 0x01 };
static DEFINE_STATIC_PACKET(a54x_glut_set_sel_orig, DSI_PKT_TYPE_WR, A54X_GLUT_SET_SEL_ORIG, 0x06);

static u8 A54X_ANALOG_GAMMA_TUNE_ENABLE[] = { 0x67, 0x01 };
static DEFINE_STATIC_PACKET(a54x_analog_gamma_tune_enable, DSI_PKT_TYPE_WR, A54X_ANALOG_GAMMA_TUNE_ENABLE, 0xA5);

static u8 A54X_ANALOG_GAMMA_TUNE_DISABLE[] = { 0x67, 0x00 };
static DEFINE_STATIC_PACKET(a54x_analog_gamma_tune_disable, DSI_PKT_TYPE_WR, A54X_ANALOG_GAMMA_TUNE_DISABLE, 0xA5);

static u8 A54X_ANALOG_GAMMA[] = {
	0x67,
	/* analog gamma value set */
	0x00, 0x00, 0x00, 0x00,	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04,
	0x00, 0x04, 0x00, 0x0C, 0x00, 0x0C, 0x00, 0x0C,
	0x00, 0x1C, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x2C,
	0x00, 0x2C, 0x00, 0x2C, 0x00, 0x5C, 0x00, 0x5C,
	0x00, 0x5C, 0x00, 0x8C, 0x00, 0x8C, 0x00, 0x8C,
	0x00, 0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x01, 0x5D,
	0x01, 0x5D, 0x01, 0x5D, 0x02, 0x5E, 0x02, 0x5E,
	0x02, 0x5E, 0x03, 0x2E, 0x03, 0x2E, 0x03, 0x2E,
	0x0B, 0x2E, 0x09, 0xE6,	0x0C, 0x40,
};
static DEFINE_PKTUI(a54x_analog_gamma, &a54x_maptbl[ANALOG_GAMMA_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a54x_analog_gamma, DSI_PKT_TYPE_WR, A54X_ANALOG_GAMMA, 0xA6);

static u8 A54X_PANEL_UPDATE[] = {
	0xF7,
	0x0F
};
static DEFINE_STATIC_PACKET(a54x_panel_update, DSI_PKT_TYPE_WR, A54X_PANEL_UPDATE, 0);

static u8 A54X_FOD_ENTER[] = {
	0xB5, 0x14
};
static DEFINE_STATIC_PACKET(a54x_fod_enter, DSI_PKT_TYPE_WR, A54X_FOD_ENTER, 0);

static u8 A54X_FOD_EXIT[] = {
	0xB5, 0x14
};
static DEFINE_STATIC_PACKET(a54x_fod_exit, DSI_PKT_TYPE_WR, A54X_FOD_EXIT, 0);

static u8 A54X_TIG_ENABLE[] = {
	0xBF, 0x01, 0x00
};
static DEFINE_STATIC_PACKET(a54x_tig_enable, DSI_PKT_TYPE_WR, A54X_TIG_ENABLE, 0);

static u8 A54X_TIG_DISABLE[] = {
	0xBF, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(a54x_tig_disable, DSI_PKT_TYPE_WR, A54X_TIG_DISABLE, 0);

static u8 A54X_DSC[] = { 0x01 };
static DEFINE_STATIC_PACKET(a54x_dsc, DSI_PKT_TYPE_WR_COMP, A54X_DSC, 0);

static u8 A54X_PPS[] = {
	//1080x2340 Slice Info : 540x30
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x24,
	0x04, 0x38, 0x00, 0x1E, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x02, 0xE3,
	0x00, 0x07, 0x00, 0x0C, 0x03, 0x50, 0x03, 0x64,
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
static DEFINE_STATIC_PACKET(a54x_pps, DSI_PKT_TYPE_WR_PPS, A54X_PPS, 0);

static u8 A54X_FFC_SETTING[] = {
	0xC5,
	0x11, 0x10, 0x50, 0x05, 0x68, 0xB9
};
static DEFINE_PKTUI(a54x_ffc_setting, &a54x_maptbl[SET_FFC_MAPTBL], 5);
static DEFINE_VARIABLE_PACKET(a54x_ffc_setting, DSI_PKT_TYPE_WR, A54X_FFC_SETTING, 0x36);

static DEFINE_PANEL_TIMER_MDELAY(a54x_init_complete_delay, 30);
static DEFINE_PANEL_TIMER_BEGIN(a54x_init_complete_delay,
		TIMER_DLYINFO(&a54x_init_complete_delay));

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static u8 A54X_CCD_ENABLE[] = {
	0xCC,
	0x01, 0x5C, 0x51
};

static DEFINE_STATIC_PACKET(a54x_ccd_enable, DSI_PKT_TYPE_WR, A54X_CCD_ENABLE, 0);

static u8 A54X_CCD_DISABLE[] = {
	0xCC,
	0x02, 0x5C, 0x51
};

static DEFINE_STATIC_PACKET(a54x_ccd_disable, DSI_PKT_TYPE_WR, A54X_CCD_DISABLE, 0);

#endif

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static u8 A54X_CRC_PATTERN_TEST_ON[] = {
	0xBE,
	0x05
};
static DEFINE_STATIC_PACKET(a54x_crc_pattern_test_on, DSI_PKT_TYPE_WR, A54X_CRC_PATTERN_TEST_ON, 0);

static u8 A54X_CRC_PATTERN_TEST_OFF[] = {
	0xBE,
	0x00
};
static DEFINE_STATIC_PACKET(a54x_crc_pattern_test_off, DSI_PKT_TYPE_WR, A54X_CRC_PATTERN_TEST_OFF, 0);

static u8 A54X_CRC_PATTERN_ENABLE[] = {
	0xBE,
	0x07
};
static DEFINE_STATIC_PACKET(a54x_crc_pattern_enable, DSI_PKT_TYPE_WR, A54X_CRC_PATTERN_ENABLE, 0);

static u8 A54X_CRC_READ_SETTING1[] = {
	0xD8,
	0x11
};
static DEFINE_STATIC_PACKET(a54x_crc_read_setting1, DSI_PKT_TYPE_WR, A54X_CRC_READ_SETTING1, 0x27);

static u8 A54X_CRC_READ_SETTING2[] = {
	0xD8,
	0x20
};
static DEFINE_STATIC_PACKET(a54x_crc_read_setting2, DSI_PKT_TYPE_WR, A54X_CRC_READ_SETTING2, 0x27);

#ifdef CONFIG_USDM_FACTORY
static u8 A54X_VDDM_LOW_SETTING[] = {
	0xD7,
	0x04
};
static DEFINE_STATIC_PACKET(a54x_vddm_low_setting, DSI_PKT_TYPE_WR, A54X_VDDM_LOW_SETTING, 0x02);
#else
static u8 A54X_VDDM_LOW_SETTING[] = {
	0xD7,
	0x00
};
static DEFINE_STATIC_PACKET(a54x_vddm_low_setting, DSI_PKT_TYPE_WR, A54X_VDDM_LOW_SETTING, 0x02);
#endif
static u8 A54X_VDDM_RETURN_SETTING[] = {
	0xD7,
	0x00
};
static DEFINE_STATIC_PACKET(a54x_vddm_return_setting, DSI_PKT_TYPE_WR, A54X_VDDM_RETURN_SETTING, 0x02);
#endif


#ifdef CONFIG_USDM_FACTORY_ECC_TEST
static u8 A54X_ECC_ENABLE[] = {
	0xF8,
	0x01
};

static u8 A54X_ECC_DISABLE[] = {
	0xF8,
	0x00
};

static DEFINE_STATIC_PACKET(a54x_ecc_enable, DSI_PKT_TYPE_WR, A54X_ECC_ENABLE, 0x02);
static DEFINE_STATIC_PACKET(a54x_ecc_disable, DSI_PKT_TYPE_WR, A54X_ECC_DISABLE, 0x02);
#endif

static u8 A54X_TE_FRAME_SEL[] = {
	0xB9,
	0x04, 0x00, 0x00, 0x00, 0x06, 0xf9, 0x00, 0x0F, 0x09, 0x0F, 0x00, 0x0F
};
static DEFINE_PKTUI(a54x_te_frame_sel, &a54x_maptbl[TE_FRAME_SEL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a54x_te_frame_sel, DSI_PKT_TYPE_WR, A54X_TE_FRAME_SEL, 0);

static struct seqinfo SEQINFO(a54x_set_bl_param_seq);
static struct seqinfo SEQINFO(a54x_set_fps_param_seq);
#if defined(CONFIG_USDM_FACTORY)
static struct seqinfo SEQINFO(a54x_res_init_seq);
#endif

static void *a54x_common_setting_cmdtbl[] = {
	&KEYINFO(a54x_level1_key_enable),
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),

	&PKTINFO(a54x_cmd_fifo_setting),
	/* 4.1.1 */
	&PKTINFO(a54x_te_on),
	&PKTINFO(a54x_te_shift),
	/* 4.1.2 */
	&PKTINFO(a54x_caset),
	&PKTINFO(a54x_paset),
	/* 4.1.3 */
	&PKTINFO(a54x_ffc_setting),
	/* 4.1.4 */
	&PKTINFO(a54x_err_fg_enable),
	&PKTINFO(a54x_err_fg_setting),
	/* 4.1.5 */
	&PKTINFO(a54x_pcd_setting),
	/* 4.1.6 */
	&PKTINFO(a54x_acl_setting),
	/* 4.1.7 */
	&PKTINFO(a54x_dsc),
	&PKTINFO(a54x_pps),
	/* 4.1.8 */
	&PKTINFO(a54x_swire_no_pulse),
	&PKTINFO(a54x_panel_update),
	/* 4.1.9 */
	&PKTINFO(a54x_tsp_vsync_enable1),
	&PKTINFO(a54x_tsp_vsync_enable2),
	&PKTINFO(a54x_tsp_vsync_enable3),
	/* 4.1.11 */
	&PKTINFO(a54x_elvss_timing_setting),
	&PKTINFO(a54x_panel_update),

	&PKTINFO(a54x_hbm_transition), /* 53h should be not included in bl_seq */
	&SEQINFO(a54x_set_bl_param_seq), /* includes FPS setting also */
	&SEQINFO(a54x_set_fps_param_seq),
	&PKTINFO(a54x_panel_update),

	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level1_key_disable),
};

static DEFINE_SEQINFO(a54x_common_setting_seq, a54x_common_setting_cmdtbl);

static void *a54x_init_cmdtbl[] = {
	&DLYINFO(a54x_wait_10msec),
	&PKTINFO(a54x_sleep_out),
	&DLYINFO(a54x_wait_sleep_out_90msec),
	&SEQINFO(a54x_common_setting_seq),
	&TIMER_DLYINFO_BEGIN(a54x_init_complete_delay),
#if defined(CONFIG_USDM_FACTORY)
	&SEQINFO(a54x_res_init_seq),
#endif
	&TIMER_DLYINFO(a54x_init_complete_delay),
};

static void *a54x_res_init_cmdtbl[] = {
	&KEYINFO(a54x_level1_key_enable),
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),

	&s6e3fc5_restbl[RES_ID],
	&s6e3fc5_restbl[RES_COORDINATE],
	&s6e3fc5_restbl[RES_CODE],
	&s6e3fc5_restbl[RES_DATE],
	&s6e3fc5_restbl[RES_OCTA_ID],
	&s6e3fc5_restbl[RES_ANALOG_GAMMA_120HS],
#ifdef CONFIG_USDM_PANEL_DPUI
	&s6e3fc5_restbl[RES_SELF_DIAG],
	&s6e3fc5_restbl[RES_ERR_FG],
	&s6e3fc5_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level1_key_disable),
};
#if defined(CONFIG_USDM_FACTORY)
static DEFINE_SEQINFO(a54x_res_init_seq, a54x_res_init_cmdtbl);
#endif

static void *a54x_set_bl_param_cmdtbl[] = {
	&PKTINFO(a54x_tset_set),
	&PKTINFO(a54x_acl_set),
	&PKTINFO(a54x_acl_control),
	&PKTINFO(a54x_wrdisbv),
	&PKTINFO(a54x_irc_mode),
};

static DEFINE_SEQINFO(a54x_set_bl_param_seq, a54x_set_bl_param_cmdtbl);

static void *a54x_set_fps_param_cmdtbl[] = {
	&PKTINFO(a54x_fps),
	&PKTINFO(a54x_te_frame_sel),
	&PKTINFO(a54x_panel_update),
	/* display_mode changed between (120hs, 60phs) and (60hs) */
	&CONDINFO_IF(a54x_cond_is_60hz),
		&CONDINFO_IF(a54x_cond_is_brightness_ge_260),
			&PKTINFO(a54x_analog_gamma_tune_disable),
			&PKTINFO(a54x_analog_gamma),
			&PKTINFO(a54x_glut_set_sel_orig),
		&CONDINFO_EL(a54x_cond_is_brightness_ge_260),
			&PKTINFO(a54x_analog_gamma_tune_enable),
			&PKTINFO(a54x_analog_gamma),
			&PKTINFO(a54x_glut_set_sel_tune),
		&CONDINFO_FI(a54x_cond_is_brightness_ge_260),
	&CONDINFO_EL(a54x_cond_is_60hz),
		&PKTINFO(a54x_analog_gamma_tune_disable),
	&CONDINFO_FI(a54x_cond_is_60hz),
};

static DEFINE_SEQINFO(a54x_set_fps_param_seq, a54x_set_fps_param_cmdtbl);

static void *a54x_set_bl_cmdtbl[] = {
	&KEYINFO(a54x_level1_key_enable),
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),
	&SEQINFO(a54x_set_fps_param_seq),
	&PKTINFO(a54x_hbm_transition),
	&SEQINFO(a54x_set_bl_param_seq),
	&PKTINFO(a54x_panel_update),
	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level1_key_disable),
};

static void *a54x_display_mode_cmdtbl[] = {
	&CONDINFO_IF(a54x_cond_is_panel_state_not_lpm),
		&KEYINFO(a54x_level1_key_enable),
		&KEYINFO(a54x_level2_key_enable),
		&KEYINFO(a54x_level3_key_enable),
		&SEQINFO(a54x_set_fps_param_seq),
		&PKTINFO(a54x_hbm_transition),
		&SEQINFO(a54x_set_bl_param_seq),
		&PKTINFO(a54x_panel_update),
		&KEYINFO(a54x_level3_key_disable),
		&KEYINFO(a54x_level2_key_disable),
		&KEYINFO(a54x_level1_key_disable),
	&CONDINFO_FI(a54x_cond_is_panel_state_not_lpm),
};

static void *a54x_display_on_cmdtbl[] = {
	&CONDINFO_IF(a54x_cond_is_panel_state_lpm),
		&KEYINFO(a54x_level2_key_enable),
		&PKTINFO(a54x_tig_enable),
		&KEYINFO(a54x_level2_key_disable),
		&DLYINFO(a54x_wait_17msec),
	&CONDINFO_FI(a54x_cond_is_panel_state_lpm),
	&KEYINFO(a54x_level1_key_enable),
	&PKTINFO(a54x_display_on),
	&KEYINFO(a54x_level1_key_disable),
	&CONDINFO_IF(a54x_cond_is_panel_state_lpm),
		&DLYINFO(a54x_wait_17msec),
		&KEYINFO(a54x_level2_key_enable),
		&PKTINFO(a54x_tig_disable),
		&KEYINFO(a54x_level2_key_disable),
	&CONDINFO_FI(a54x_cond_is_panel_state_lpm),
};

static void *a54x_display_off_cmdtbl[] = {
	&KEYINFO(a54x_level1_key_enable),
	&KEYINFO(a54x_level2_key_enable),
	&PKTINFO(a54x_display_off),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level1_key_disable),
	&DLYINFO(a54x_wait_display_off),
};

static void *a54x_exit_cmdtbl[] = {
	&KEYINFO(a54x_level1_key_enable),
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),
#ifdef CONFIG_USDM_PANEL_DPUI
	&s6e3fc5_dmptbl[DUMP_RDDPM_SLEEP_IN],
	&s6e3fc5_dmptbl[DUMP_RDDSM],
	&s6e3fc5_dmptbl[DUMP_ERR],
	&s6e3fc5_dmptbl[DUMP_DSI_ERR],
	&s6e3fc5_dmptbl[DUMP_SELF_DIAG],
#endif
	&PKTINFO(a54x_sleep_in),
	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level1_key_disable),
	&DLYINFO(a54x_wait_sleep_in),
};

static void *a54x_alpm_init_cmdtbl[] = {
	&KEYINFO(a54x_level2_key_enable),
	&PKTINFO(a54x_aod_aor_max),
	&PKTINFO(a54x_aod_freq),
	&PKTINFO(a54x_panel_update),
	&KEYINFO(a54x_level3_key_enable),
	&PKTINFO(a54x_te_shift_hlpm),
	&PKTINFO(a54x_black_frame_on),
	&PKTINFO(a54x_panel_update),
	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
	&DLYINFO(a54x_wait_17msec),
};

static void *a54x_alpm_set_bl_cmdtbl[] = {
	&KEYINFO(a54x_level2_key_enable),

	&PKTINFO(a54x_aod_vaint),
	&PKTINFO(a54x_aod_setting),
	&PKTINFO(a54x_aod_aor),
	&PKTINFO(a54x_lpm_nit),
	&PKTINFO(a54x_lpm_porch_on),
	&PKTINFO(a54x_panel_update),
	&DLYINFO(a54x_wait_1usec),

	&KEYINFO(a54x_level2_key_disable),
};

static void *a54x_alpm_exit_cmdtbl[] = {
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),
	&PKTINFO(a54x_wrdisbv),
	&DLYINFO(a54x_wait_34msec),
	&PKTINFO(a54x_te_shift),
	&PKTINFO(a54x_black_frame_off),
	&PKTINFO(a54x_panel_update),
	&PKTINFO(a54x_swire_no_pulse_lpm_off),
	&PKTINFO(a54x_lpm_porch_off),
	&PKTINFO(a54x_normal_setting),
	&PKTINFO(a54x_lpm_off_sync_ctrl),
	&PKTINFO(a54x_normal_mode),
	&PKTINFO(a54x_panel_update),
	&DLYINFO(a54x_wait_34msec),
	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
};

static void *a54x_alpm_exit_after_cmdtbl[] = {
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),
	&SEQINFO(a54x_set_fps_param_seq),
	&PKTINFO(a54x_hbm_transition),
	&SEQINFO(a54x_set_bl_param_seq),
	&PKTINFO(a54x_panel_update),
	&DLYINFO(a54x_wait_1usec),
	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
};

static void *a54x_dump_cmdtbl[] = {
	&KEYINFO(a54x_level1_key_enable),
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),
	&s6e3fc5_dmptbl[DUMP_RDDPM],
	&s6e3fc5_dmptbl[DUMP_RDDSM],
	&s6e3fc5_dmptbl[DUMP_DSI_ERR],
	&s6e3fc5_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level1_key_disable),
};

static void *a54x_ffc_cmdtbl[] = {
	&KEYINFO(a54x_level1_key_enable),
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),
	&PKTINFO(a54x_ffc_setting),
	&PKTINFO(a54x_panel_update),
	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level1_key_disable),
};

static void *a54x_check_condition_cmdtbl[] = {
	&KEYINFO(a54x_level1_key_enable),
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),
	&s6e3fc5_dmptbl[DUMP_RDDPM],
	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level1_key_disable),
};

#ifdef CONFIG_USDM_PANEL_MASK_LAYER
static void *a54x_mask_layer_workaround_cmdtbl[] = {
	&DLYINFO(a54x_wait_1_vsync),
	&PKTINFO(a54x_wrdisbv),
	&PKTINFO(a54x_hbm_transition),
	&DLYINFO(a54x_wait_2_frame),
	&CONDINFO_IF(a54x_cond_is_120hz),
		&DLYINFO(a54x_wait_3msec),
	&CONDINFO_FI(a54x_cond_is_120hz),
};

static void *a54x_mask_layer_enter_br_cmdtbl[] = {
	&KEYINFO(a54x_level1_key_enable),
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),

	&PKTINFO(a54x_acl_dim_off),
	&PKTINFO(a54x_hbm_transition),
	&PKTINFO(a54x_wrdisbv),

	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level1_key_disable),

	&CONDINFO_IF(a54x_cond_is_120hz),
		&DLYINFO(a54x_wait_9msec),
	&CONDINFO_FI(a54x_cond_is_120hz),
	&CONDINFO_IF(a54x_cond_is_60hz),
		&DLYINFO(a54x_wait_1_vsync),
	&CONDINFO_FI(a54x_cond_is_60hz),
};

static void *a54x_mask_layer_exit_br_cmdtbl[] = {
	&DLYINFO(a54x_wait_1_vsync),
	&KEYINFO(a54x_level1_key_enable),
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),

	&PKTINFO(a54x_acl_control),
	&CONDINFO_IF(a54x_cond_is_120hz),
		&DLYINFO(a54x_wait_3msec),
	&CONDINFO_FI(a54x_cond_is_120hz),

	&PKTINFO(a54x_hbm_transition),
	&PKTINFO(a54x_wrdisbv),

	&KEYINFO(a54x_level3_key_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level1_key_disable),

	&CONDINFO_IF(a54x_cond_is_120hz),
		&DLYINFO(a54x_wait_9msec),
	&CONDINFO_FI(a54x_cond_is_120hz),
	&CONDINFO_IF(a54x_cond_is_60hz),
		&DLYINFO(a54x_wait_1_vsync),
	&CONDINFO_FI(a54x_cond_is_60hz),
};
#endif

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static void *a54x_decoder_test_cmdtbl[] = {
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),
	&PKTINFO(a54x_decoder_test_caset),
	&PKTINFO(a54x_decoder_test_paset),
	&PKTINFO(a54x_decoder_test_2c),
	&PKTINFO(a54x_crc_pattern_test_on),
	&DLYINFO(a54x_wait_crc_test_50msec),
	&PKTINFO(a54x_crc_pattern_test_off),
	&DLYINFO(a54x_wait_crc_test_50msec),
	&PKTINFO(a54x_crc_pattern_enable),
	&PKTINFO(a54x_crc_read_setting1),
	&DLYINFO(a54x_wait_crc_test_50msec),
	&s6e3fc5_restbl[RES_DECODER_TEST1],
	&PKTINFO(a54x_crc_read_setting2),
	&DLYINFO(a54x_wait_crc_test_50msec),
	&s6e3fc5_restbl[RES_DECODER_TEST2],
	&DLYINFO(a54x_wait_crc_test_50msec),
	&PKTINFO(a54x_vddm_low_setting),
	&PKTINFO(a54x_crc_pattern_test_on),
	&DLYINFO(a54x_wait_crc_test_50msec),
	&PKTINFO(a54x_crc_pattern_test_off),
	&DLYINFO(a54x_wait_crc_test_50msec),
	&PKTINFO(a54x_crc_pattern_enable),
	&PKTINFO(a54x_crc_read_setting1),
	&DLYINFO(a54x_wait_crc_test_50msec),
	&s6e3fc5_restbl[RES_DECODER_TEST3],
	&PKTINFO(a54x_crc_read_setting2),
	&DLYINFO(a54x_wait_crc_test_50msec),
	&s6e3fc5_restbl[RES_DECODER_TEST4],
	&PKTINFO(a54x_crc_pattern_test_off),
	&PKTINFO(a54x_vddm_return_setting),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level3_key_disable),
};
#endif

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static void *a54x_ccd_test_cmdtbl[] = {
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),
	&PKTINFO(a54x_ccd_enable),
	&DLYINFO(a54x_wait_ccd_test),
	&s6e3fc5_restbl[RES_CCD_STATE],
	&PKTINFO(a54x_ccd_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level3_key_disable),
};
#endif

#ifdef CONFIG_USDM_FACTORY_ECC_TEST
static void *a54x_ecc_test_cmdtbl[] = {
	&KEYINFO(a54x_level2_key_enable),
	&KEYINFO(a54x_level3_key_enable),
	&PKTINFO(a54x_ecc_enable),
	&s6e3fc5_restbl[RES_ECC_TEST],
	&PKTINFO(a54x_ecc_disable),
	&KEYINFO(a54x_level2_key_disable),
	&KEYINFO(a54x_level3_key_disable),
};
#endif

static void *a54x_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo a54x_seqtbl[] = {
	SEQINFO_INIT(PANEL_INIT_SEQ, a54x_init_cmdtbl),
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, a54x_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_SET_BL_SEQ, a54x_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, a54x_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, a54x_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, a54x_exit_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, a54x_display_mode_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_INIT_SEQ, a54x_alpm_init_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_SET_BL_SEQ, a54x_alpm_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_EXIT_SEQ, a54x_alpm_exit_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_EXIT_AFTER_SEQ, a54x_alpm_exit_after_cmdtbl),
#ifdef CONFIG_USDM_PANEL_MASK_LAYER
	SEQINFO_INIT(PANEL_MASK_LAYER_STOP_DIMMING_SEQ, a54x_mask_layer_workaround_cmdtbl),
	SEQINFO_INIT(PANEL_MASK_LAYER_ENTER_BR_SEQ, a54x_mask_layer_enter_br_cmdtbl), //temp br
	SEQINFO_INIT(PANEL_MASK_LAYER_EXIT_BR_SEQ, a54x_mask_layer_exit_br_cmdtbl),
#endif
	SEQINFO_INIT(PANEL_FFC_SEQ, a54x_ffc_cmdtbl),
	SEQINFO_INIT(PANEL_DUMP_SEQ, a54x_dump_cmdtbl),
	SEQINFO_INIT(PANEL_CHECK_CONDITION_SEQ, a54x_check_condition_cmdtbl),
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	SEQINFO_INIT(PANEL_DECODER_TEST_SEQ, a54x_decoder_test_cmdtbl),
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	SEQINFO_INIT(PANEL_CCD_TEST_SEQ, a54x_ccd_test_cmdtbl),
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
	SEQINFO_INIT(PANEL_ECC_TEST_SEQ, a54x_ecc_test_cmdtbl),
#endif
	SEQINFO_INIT(PANEL_DUMMY_SEQ, a54x_dummy_cmdtbl),
};

struct common_panel_info s6e3fc5_a54x_panel_info = {
	.ldi_name = "s6e3fc5",
	.name = "s6e3fc5_a54x",
	.model = "AMS642DF01",
	.vendor = "SDC",
	.id = 0x800000,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA | DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_2BYTE_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
		.support_vrr = true,
		.dft_osc_freq = 181300,
		/* Todo the hs_clk must be synchronized with the value of LK,
		 * It must be changed to be read and set in the LK.
		 */
		.dft_dsi_freq = 1108000,
	},

	.ddi_ops = {
		.get_cell_id = s6e3fc5_get_cell_id,
		.get_octa_id = s6e3fc5_get_octa_id,
		.get_manufacture_code = s6e3fc5_get_manufacture_code,
		.get_manufacture_date = s6e3fc5_get_manufacture_date,
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
		.ecc_test = s6e3fc5_ecc_test,
#endif
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
		.decoder_test = s6e3fc5_decoder_test,
#endif
	},
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e3fc5_a54x_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3fc5_a54x_default_resol),
		.resol = s6e3fc5_a54x_default_resol,
	},
	.vrrtbl = s6e3fc5_a54x_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e3fc5_a54x_default_vrrtbl),
	.maptbl = a54x_maptbl,
	.nr_maptbl = ARRAY_SIZE(a54x_maptbl),
	.seqtbl = a54x_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a54x_seqtbl),
	.rditbl = s6e3fc5_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3fc5_rditbl),
	.restbl = s6e3fc5_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3fc5_restbl),
	.dumpinfo = s6e3fc5_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3fc5_dmptbl),

#ifdef CONFIG_USDM_MDNIE
	.mdnie_tune = &s6e3fc5_a54x_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3fc5_a54x_panel_dimming_info,
#ifdef CONFIG_USDM_PANEL_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3fc5_a54x_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_USDM_PANEL_RCD
	.rcd_data = &s6e3fc5_a54x_rcd,
#endif
};

#endif /* __S6E3FC5_A54X_PANEL_H__ */
