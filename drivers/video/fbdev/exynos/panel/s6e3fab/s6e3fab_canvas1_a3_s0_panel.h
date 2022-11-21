/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/s6e3fab_canvas1_a3_s0_panel.h
 *
 * Header file for S6E3FAB Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAB_CANVAS1_A3_S0_PANEL_H__
#define __S6E3FAB_CANVAS1_A3_S0_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "s6e3fab.h"
#include "s6e3fab_dimming.h"
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "s6e3fab_canvas1_a3_s0_panel_mdnie.h"
#endif
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
#include "s6e3fab_canvas1_a3_s0_panel_copr.h"
#endif
#include "s6e3fab_canvas1_a3_s0_panel_dimming.h"
#ifdef CONFIG_SUPPORT_AOD_BL
#include "s6e3fab_canvas1_a3_s0_panel_aod_dimming.h"
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "s6e3fab_canvas1_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
#include "s6e3fab_profiler_panel.h"
#include "../display_profiler/display_profiler.h"
#endif

#undef __pn_name__
#define __pn_name__	canvas1_a3_s0

#undef __PN_NAME__
#define __PN_NAME__	CANVAS1_A3_S0

/* ===================================================================================== */
/* ============================= [S6E3FAB READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [S6E3FAB RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [S6E3FAB MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 canvas1_a3_s0_brt_table[S6E3FAB_TOTAL_STEP][2] = {
	/* Normal 5x51+1 */
	{ 0x00, 0x05 }, { 0x00, 0x08 }, { 0x00, 0x0B }, { 0x00, 0x0E }, { 0x00, 0x11 },
	{ 0x00, 0x14 }, { 0x00, 0x17 }, { 0x00, 0x1A }, { 0x00, 0x1D }, { 0x00, 0x20 },
	{ 0x00, 0x23 }, { 0x00, 0x26 }, { 0x00, 0x29 }, { 0x00, 0x2C }, { 0x00, 0x2F },
	{ 0x00, 0x32 }, { 0x00, 0x35 }, { 0x00, 0x38 }, { 0x00, 0x3B }, { 0x00, 0x3E },
	{ 0x00, 0x41 }, { 0x00, 0x44 }, { 0x00, 0x47 }, { 0x00, 0x4A }, { 0x00, 0x4D },
	{ 0x00, 0x50 }, { 0x00, 0x53 }, { 0x00, 0x56 }, { 0x00, 0x59 }, { 0x00, 0x5C },
	{ 0x00, 0x5F }, { 0x00, 0x62 }, { 0x00, 0x65 }, { 0x00, 0x68 }, { 0x00, 0x6B },
	{ 0x00, 0x6E }, { 0x00, 0x71 }, { 0x00, 0x74 }, { 0x00, 0x77 }, { 0x00, 0x7A },
	{ 0x00, 0x7D }, { 0x00, 0x80 }, { 0x00, 0x83 }, { 0x00, 0x86 }, { 0x00, 0x89 },
	{ 0x00, 0x8C }, { 0x00, 0x8F }, { 0x00, 0x92 }, { 0x00, 0x95 }, { 0x00, 0x98 },
	{ 0x00, 0x9B }, { 0x00, 0x9E }, { 0x00, 0xA1 }, { 0x00, 0xA4 }, { 0x00, 0xA7 },
	{ 0x00, 0xAA }, { 0x00, 0xAD }, { 0x00, 0xB0 }, { 0x00, 0xB3 }, { 0x00, 0xB6 },
	{ 0x00, 0xB9 }, { 0x00, 0xBC }, { 0x00, 0xBF }, { 0x00, 0xC2 }, { 0x00, 0xC5 },
	{ 0x00, 0xC9 }, { 0x00, 0xCC }, { 0x00, 0xD0 }, { 0x00, 0xD3 }, { 0x00, 0xD7 },
	{ 0x00, 0xDA }, { 0x00, 0xDE }, { 0x00, 0xE1 }, { 0x00, 0xE5 }, { 0x00, 0xE8 },
	{ 0x00, 0xEC }, { 0x00, 0xEF }, { 0x00, 0xF3 }, { 0x00, 0xF6 }, { 0x00, 0xFA },
	{ 0x00, 0xFD }, { 0x01, 0x01 }, { 0x01, 0x04 }, { 0x01, 0x08 }, { 0x01, 0x0B },
	{ 0x01, 0x0F }, { 0x01, 0x12 }, { 0x01, 0x16 }, { 0x01, 0x19 }, { 0x01, 0x1D },
	{ 0x01, 0x20 }, { 0x01, 0x24 }, { 0x01, 0x27 }, { 0x01, 0x2B }, { 0x01, 0x2E },
	{ 0x01, 0x32 }, { 0x01, 0x35 }, { 0x01, 0x39 }, { 0x01, 0x3C }, { 0x01, 0x40 },
	{ 0x01, 0x43 }, { 0x01, 0x47 }, { 0x01, 0x4A }, { 0x01, 0x4E }, { 0x01, 0x51 },
	{ 0x01, 0x55 }, { 0x01, 0x58 }, { 0x01, 0x5C }, { 0x01, 0x5F }, { 0x01, 0x63 },
	{ 0x01, 0x66 }, { 0x01, 0x6A }, { 0x01, 0x6D }, { 0x01, 0x71 }, { 0x01, 0x74 },
	{ 0x01, 0x78 }, { 0x01, 0x7B }, { 0x01, 0x7F }, { 0x01, 0x82 }, { 0x01, 0x86 },
	{ 0x01, 0x89 }, { 0x01, 0x8D }, { 0x01, 0x90 }, { 0x01, 0x94 }, { 0x01, 0x97 },
	{ 0x01, 0x9B }, { 0x01, 0x9E }, { 0x01, 0xA2 }, { 0x01, 0xA5 }, { 0x01, 0xAA },
	{ 0x01, 0xAF }, { 0x01, 0xB4 }, { 0x01, 0xB9 }, { 0x01, 0xBE }, { 0x01, 0xC3 },
	{ 0x01, 0xC8 }, { 0x01, 0xCD }, { 0x01, 0xD2 }, { 0x01, 0xD7 }, { 0x01, 0xDC },
	{ 0x01, 0xE1 }, { 0x01, 0xE6 }, { 0x01, 0xEB }, { 0x01, 0xF0 }, { 0x01, 0xF5 },
	{ 0x01, 0xFA }, { 0x01, 0xFF }, { 0x02, 0x04 }, { 0x02, 0x09 }, { 0x02, 0x0E },
	{ 0x02, 0x13 }, { 0x02, 0x18 }, { 0x02, 0x1D }, { 0x02, 0x22 }, { 0x02, 0x27 },
	{ 0x02, 0x2C }, { 0x02, 0x31 }, { 0x02, 0x36 }, { 0x02, 0x3B }, { 0x02, 0x40 },
	{ 0x02, 0x45 }, { 0x02, 0x4A }, { 0x02, 0x4F }, { 0x02, 0x54 }, { 0x02, 0x59 },
	{ 0x02, 0x5E }, { 0x02, 0x63 }, { 0x02, 0x68 }, { 0x02, 0x6D }, { 0x02, 0x72 },
	{ 0x02, 0x77 }, { 0x02, 0x7C }, { 0x02, 0x81 }, { 0x02, 0x86 }, { 0x02, 0x8B },
	{ 0x02, 0x90 }, { 0x02, 0x95 }, { 0x02, 0x9A }, { 0x02, 0x9F }, { 0x02, 0xA4 },
	{ 0x02, 0xA9 }, { 0x02, 0xAE }, { 0x02, 0xB3 }, { 0x02, 0xB8 }, { 0x02, 0xBD },
	{ 0x02, 0xC2 }, { 0x02, 0xC7 }, { 0x02, 0xCC }, { 0x02, 0xD1 }, { 0x02, 0xD6 },
	{ 0x02, 0xDA }, { 0x02, 0xDF }, { 0x02, 0xE3 }, { 0x02, 0xE8 }, { 0x02, 0xEC },
	{ 0x02, 0xF1 }, { 0x02, 0xF5 }, { 0x02, 0xFA }, { 0x02, 0xFE }, { 0x03, 0x03 },
	{ 0x03, 0x07 }, { 0x03, 0x0C }, { 0x03, 0x10 }, { 0x03, 0x15 }, { 0x03, 0x19 },
	{ 0x03, 0x1E }, { 0x03, 0x22 }, { 0x03, 0x27 }, { 0x03, 0x2B }, { 0x03, 0x30 },
	{ 0x03, 0x34 }, { 0x03, 0x39 }, { 0x03, 0x3D }, { 0x03, 0x42 }, { 0x03, 0x46 },
	{ 0x03, 0x4B }, { 0x03, 0x4F }, { 0x03, 0x54 }, { 0x03, 0x58 }, { 0x03, 0x5D },
	{ 0x03, 0x61 }, { 0x03, 0x66 }, { 0x03, 0x6A }, { 0x03, 0x6F }, { 0x03, 0x73 },
	{ 0x03, 0x78 }, { 0x03, 0x7C }, { 0x03, 0x81 }, { 0x03, 0x85 }, { 0x03, 0x8A },
	{ 0x03, 0x8E }, { 0x03, 0x93 }, { 0x03, 0x97 }, { 0x03, 0x9C }, { 0x03, 0xA0 },
	{ 0x03, 0xA5 }, { 0x03, 0xA9 }, { 0x03, 0xAE }, { 0x03, 0xB2 }, { 0x03, 0xB7 },
	{ 0x03, 0xBB }, { 0x03, 0xC0 }, { 0x03, 0xC4 }, { 0x03, 0xC9 }, { 0x03, 0xCD },
	{ 0x03, 0xD2 }, { 0x03, 0xD6 }, { 0x03, 0xDB }, { 0x03, 0xDF }, { 0x03, 0xE4 },
	{ 0x03, 0xE8 }, { 0x03, 0xED }, { 0x03, 0xF1 }, { 0x03, 0xF6 }, { 0x03, 0xFA },
	{ 0x03, 0xFF },
	/* HBM 5x17 */
	{ 0x00, 0x0C }, { 0x00, 0x18 }, { 0x00, 0x24 }, { 0x00, 0x30 }, { 0x00, 0x3C },
	{ 0x00, 0x48 }, { 0x00, 0x54 }, { 0x00, 0x60 }, { 0x00, 0x6C }, { 0x00, 0x78 },
	{ 0x00, 0x84 }, { 0x00, 0x90 }, { 0x00, 0x9C }, { 0x00, 0xA8 }, { 0x00, 0xB4 },
	{ 0x00, 0xC0 }, { 0x00, 0xCC }, { 0x00, 0xD8 }, { 0x00, 0xE4 }, { 0x00, 0xF0 },
	{ 0x00, 0xFC }, { 0x01, 0x08 }, { 0x01, 0x14 }, { 0x01, 0x20 }, { 0x01, 0x2C },
	{ 0x01, 0x38 }, { 0x01, 0x44 }, { 0x01, 0x50 }, { 0x01, 0x5C }, { 0x01, 0x68 },
	{ 0x01, 0x74 }, { 0x01, 0x80 }, { 0x01, 0x8C }, { 0x01, 0x98 }, { 0x01, 0xA4 },
	{ 0x01, 0xB0 }, { 0x01, 0xBC }, { 0x01, 0xC8 }, { 0x01, 0xD4 }, { 0x01, 0xE0 },
	{ 0x01, 0xEC }, { 0x01, 0xF8 }, { 0x02, 0x04 }, { 0x02, 0x10 }, { 0x02, 0x1C },
	{ 0x02, 0x28 }, { 0x02, 0x34 }, { 0x02, 0x40 }, { 0x02, 0x4C }, { 0x02, 0x58 },
	{ 0x02, 0x64 }, { 0x02, 0x70 }, { 0x02, 0x7C }, { 0x02, 0x88 }, { 0x02, 0x94 },
	{ 0x02, 0xA0 }, { 0x02, 0xAC }, { 0x02, 0xB8 }, { 0x02, 0xC4 }, { 0x02, 0xD0 },
	{ 0x02, 0xDC }, { 0x02, 0xE8 }, { 0x02, 0xF4 }, { 0x03, 0x00 }, { 0x03, 0x0C },
	{ 0x03, 0x18 }, { 0x03, 0x24 }, { 0x03, 0x30 }, { 0x03, 0x3C }, { 0x03, 0x48 },
	{ 0x03, 0x54 }, { 0x03, 0x60 }, { 0x03, 0x6C }, { 0x03, 0x78 }, { 0x03, 0x85 },
	{ 0x03, 0x91 }, { 0x03, 0x9D }, { 0x03, 0xA9 }, { 0x03, 0xB5 }, { 0x03, 0xC2 },
	{ 0x03, 0xCE }, { 0x03, 0xDA }, { 0x03, 0xE6 }, { 0x03, 0xF2 }, { 0x03, 0xFF },
};

static u8 canvas1_a3_s0_elvss_table[S6E3FAB_TOTAL_STEP][1] = {
	/* Normal  8x32 */
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	/* HBM 8x10+5 */
	{ 0x26 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 },
	{ 0x25 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x23 },
	{ 0x23 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 },
	{ 0x22 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x20 }, { 0x20 },
	{ 0x20 }, { 0x20 }, { 0x20 }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F },
	{ 0x1E }, { 0x1E }, { 0x1E }, { 0x1E }, { 0x1E }, { 0x1D }, { 0x1D }, { 0x1D },
	{ 0x1D }, { 0x1D }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1B },
	{ 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x1A },
	{ 0x1A }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x18 }, { 0x18 },
	{ 0x18 }, { 0x18 }, { 0x18 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
};

static u8 canvas1_a3_s0_hbm_transition_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
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
		{ 0xE8 },
	}
};

static u8 canvas1_a3_s0_acl_opr_table[ACL_OPR_MAX][1] = {
	{ 0x00 }, /* ACL OFF OPR */
	{ 0x01 }, /* ACL ON OPR_3 */
	{ 0x01 }, /* ACL ON OPR_6 */
	{ 0x01 }, /* ACL ON OPR_8 */
	{ 0x02 }, /* ACL ON OPR_12 */
	{ 0x02 }, /* ACL ON OPR_15 */
};

static u8 canvas1_a3_s0_lpm_nit_table[4][2] = {
	/* LPM 2NIT */
	{ 0x0B, 0xF0 },
	/* LPM 10NIT */
	{ 0x0B, 0x02 },
	/* LPM 30NIT */
	{ 0x08, 0x90 },
	/* LPM 60NIT */
	{ 0x04, 0xF0 },
};

static u8 canvas1_a3_s0_lpm_mode_table[2][1] = {
	[ALPM_MODE] = { 0x0B },
	[HLPM_MODE] = { 0x09 },
};

static u8 canvas1_a3_s0_lpm_off_table[2][1] = {
	[ALPM_MODE] = { 0x0B },
	[HLPM_MODE] = { 0x09 },
};


static struct maptbl canvas1_a3_s0_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = DEFINE_2D_MAPTBL(canvas1_a3_s0_brt_table, init_gamma_mode2_brt_table, getidx_gamma_mode2_brt_table, copy_common_maptbl),
	[HBM_ONOFF_MAPTBL] = DEFINE_3D_MAPTBL(canvas1_a3_s0_hbm_transition_table, init_common_table, getidx_hbm_transition_table, copy_common_maptbl),
	[ACL_OPR_MAPTBL] = DEFINE_2D_MAPTBL(canvas1_a3_s0_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),
	[ELVSS_MAPTBL] = DEFINE_2D_MAPTBL(canvas1_a3_s0_elvss_table, init_common_table, getidx_gm2_elvss_table, copy_common_maptbl),
	[LPM_NIT_MAPTBL] = DEFINE_2D_MAPTBL(canvas1_a3_s0_lpm_nit_table, init_lpm_brt_table, getidx_lpm_brt_table, copy_common_maptbl),
	[LPM_MODE_MAPTBL] = DEFINE_2D_MAPTBL(canvas1_a3_s0_lpm_mode_table, init_common_table, getidx_lpm_mode_table, copy_common_maptbl),
	[LPM_OFF_MAPTBL] = DEFINE_2D_MAPTBL(canvas1_a3_s0_lpm_off_table, init_common_table, getidx_lpm_mode_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [S6E3FAB COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 CANVAS1_A3_S0_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 CANVAS1_A3_S0_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 CANVAS1_A3_S0_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 CANVAS1_A3_S0_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 CANVAS1_A3_S0_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 CANVAS1_A3_S0_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 CANVAS1_A3_S0_SLEEP_OUT[] = { 0x11 };
static u8 CANVAS1_A3_S0_SLEEP_IN[] = { 0x10 };
static u8 CANVAS1_A3_S0_DISPLAY_OFF[] = { 0x28 };
static u8 CANVAS1_A3_S0_DISPLAY_ON[] = { 0x29 };

static u8 CANVAS1_A3_S0_DSC[] = { 0x01 };
static u8 CANVAS1_A3_S0_PPS[] = {
	// FHD : 1080x2340 slice 117*20
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x24,
	0x04, 0x38, 0x00, 0x75, 0x02, 0x1C, 0x02, 0x1C,
	0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x0B, 0x64,
	0x00, 0x07, 0x00, 0x0C, 0x00, 0xD4, 0x00, 0xDF,
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
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 CANVAS1_A3_S0_TE_ON[] = { 0x35, 0x00 };

static u8 CANVAS1_A3_S0_TSP_HSYNC[] = {
	0xB9,
	0x00, 0x00, 0x14, 0x18, 0x00, 0x00, 0x00, 0x11,
	0x03, 0x00, 0x00, 0x00, 0x00, 0x02, 0x15, 0x02,
	0x40
};

static u8 CANVAS1_A3_S0_ELVSS[] = {
	0xC6,
	0x16,
};

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static u8 CANVAS1_A3_S0_CMDLOG_ENABLE[] = { 0xF7, 0x80 };
static u8 CANVAS1_A3_S0_CMDLOG_DISABLE[] = { 0xF7, 0x00 };
static u8 CANVAS1_A3_S0_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x8F };
#else
static u8 CANVAS1_A3_S0_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x0F };
#endif

static u8 CANVAS1_A3_S0_LPM_ON[] = { 0x53, 0x23 };

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static u8 CANVAS1_A3_S0_DYNAMIC_HLPM_ENABLE[] = {
	0x85,
	0x03, 0x0B, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x26, 0x02, 0xB2, 0x07, 0xBC, 0x09, 0xEB
};

static u8 CANVAS1_A3_S0_DYNAMIC_HLPM_DISABLE[] = {
	0x85,
	0x00
};
#endif

static DEFINE_STATIC_PACKET(canvas1_a3_s0_level1_key_enable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_level2_key_enable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_level3_key_enable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_level1_key_disable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_level2_key_disable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_level3_key_disable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(canvas1_a3_s0_sleep_out, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_sleep_in, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_display_on, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_display_off, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_DISPLAY_OFF, 0);

static DEFINE_STATIC_PACKET(canvas1_a3_s0_te_on, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_TE_ON, 0);

static DEFINE_STATIC_PACKET(canvas1_a3_s0_tsp_hsync, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_TSP_HSYNC, 0);

static DEFINE_PKTUI(canvas1_a3_s0_dsc, &canvas1_a3_s0_maptbl[DSC_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(canvas1_a3_s0_dsc, DSI_PKT_TYPE_COMP, CANVAS1_A3_S0_DSC, 0);

static DEFINE_PKTUI(canvas1_a3_s0_pps, &canvas1_a3_s0_maptbl[PPS_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(canvas1_a3_s0_pps, DSI_PKT_TYPE_PPS, CANVAS1_A3_S0_PPS, 0);

static DEFINE_STATIC_PACKET(canvas1_a3_s0_lpm_on, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_LPM_ON, 0);

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static DEFINE_STATIC_PACKET(canvas1_a3_s0_dynamic_hlpm_enable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_DYNAMIC_HLPM_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_dynamic_hlpm_disable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_DYNAMIC_HLPM_DISABLE, 0);
#endif

static DEFINE_PKTUI(canvas1_a3_s0_elvss, &canvas1_a3_s0_maptbl[ELVSS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas1_a3_s0_elvss, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_ELVSS, 0x2);

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static DEFINE_STATIC_PACKET(canvas1_a3_s0_cmdlog_enable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_CMDLOG_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_cmdlog_disable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_CMDLOG_DISABLE, 0);
#endif
static DEFINE_STATIC_PACKET(canvas1_a3_s0_gamma_update_enable, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_GAMMA_UPDATE_ENABLE, 0);

static DEFINE_PANEL_MDELAY(canvas1_a3_s0_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(canvas1_a3_s0_wait_sleep_out_120msec, 120);
static DEFINE_PANEL_MDELAY(canvas1_a3_s0_wait_sleep_in, 120);
static DEFINE_PANEL_FRAME_DELAY(canvas1_a3_s0_wait_1_frame, 1);
static DEFINE_PANEL_UDELAY(canvas1_a3_s0_wait_1usec, 1);

static DEFINE_PANEL_KEY(canvas1_a3_s0_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(canvas1_a3_s0_level1_key_enable));
static DEFINE_PANEL_KEY(canvas1_a3_s0_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(canvas1_a3_s0_level2_key_enable));
static DEFINE_PANEL_KEY(canvas1_a3_s0_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(canvas1_a3_s0_level3_key_enable));
static DEFINE_PANEL_KEY(canvas1_a3_s0_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(canvas1_a3_s0_level1_key_disable));
static DEFINE_PANEL_KEY(canvas1_a3_s0_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(canvas1_a3_s0_level2_key_disable));
static DEFINE_PANEL_KEY(canvas1_a3_s0_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(canvas1_a3_s0_level3_key_disable));


/* temporary bl code start */

static u8 CANVAS1_A3_S0_HBM_TRANSITION[] = {
	0x53, 0x20
};

static DEFINE_PKTUI(canvas1_a3_s0_hbm_transition, &canvas1_a3_s0_maptbl[HBM_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas1_a3_s0_hbm_transition, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_HBM_TRANSITION, 0);

static u8 CANVAS1_A3_S0_ACL[] = {
	0x55, 0x02
};

static DEFINE_PKTUI(canvas1_a3_s0_acl_control, &canvas1_a3_s0_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas1_a3_s0_acl_control, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_ACL, 0);

static u8 CANVAS1_A3_S0_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(canvas1_a3_s0_wrdisbv, &canvas1_a3_s0_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas1_a3_s0_wrdisbv, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_WRDISBV, 0);


static u8 CANVAS1_A3_S0_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 CANVAS1_A3_S0_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x23 };

static DEFINE_STATIC_PACKET(canvas1_a3_s0_caset, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_CASET, 0);
static DEFINE_STATIC_PACKET(canvas1_a3_s0_paset, DSI_PKT_TYPE_WR, CANVAS1_A3_S0_PASET, 0);



static struct seqinfo SEQINFO(canvas1_a3_s0_set_bl_param_seq);

static void *canvas1_a3_s0_fab_init_cmdtbl[] = {
	&DLYINFO(canvas1_a3_s0_wait_10msec),
	&KEYINFO(canvas1_a3_s0_level1_key_enable),
	&PKTINFO(canvas1_a3_s0_dsc),
	&PKTINFO(canvas1_a3_s0_pps),
	&KEYINFO(canvas1_a3_s0_level2_key_enable),
	&KEYINFO(canvas1_a3_s0_level3_key_enable),
	&PKTINFO(canvas1_a3_s0_sleep_out),
	&DLYINFO(canvas1_a3_s0_wait_10msec),
	&PKTINFO(canvas1_a3_s0_caset),
	&PKTINFO(canvas1_a3_s0_paset),
	&PKTINFO(canvas1_a3_s0_te_on),
	&PKTINFO(canvas1_a3_s0_tsp_hsync),

	/* set brightness & fps */
	&SEQINFO(canvas1_a3_s0_set_bl_param_seq),
	&KEYINFO(canvas1_a3_s0_level3_key_disable),
	&KEYINFO(canvas1_a3_s0_level2_key_disable),
	&KEYINFO(canvas1_a3_s0_level1_key_disable),
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	&SEQINFO(canvas1_a3_s0_copr_seqtbl[COPR_SET_SEQ]),
#endif
	&DLYINFO(canvas1_a3_s0_wait_sleep_out_120msec),
};

static void *canvas1_a3_s0_res_init_cmdtbl[] = {
	&KEYINFO(canvas1_a3_s0_level1_key_enable),
	&KEYINFO(canvas1_a3_s0_level2_key_enable),
	&KEYINFO(canvas1_a3_s0_level3_key_enable),
	&s6e3fab_restbl[RES_COORDINATE],
	&s6e3fab_restbl[RES_CODE],
	&s6e3fab_restbl[RES_DATE],
	&s6e3fab_restbl[RES_OCTA_ID],
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e3fab_restbl[RES_CHIP_ID],
	&s6e3fab_restbl[RES_SELF_DIAG],
	&s6e3fab_restbl[RES_ERR_FG],
	&s6e3fab_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(canvas1_a3_s0_level3_key_disable),
	&KEYINFO(canvas1_a3_s0_level2_key_disable),
	&KEYINFO(canvas1_a3_s0_level1_key_disable),
};

static void *canvas1_a3_s0_set_bl_param_cmdtbl[] = {
	&PKTINFO(canvas1_a3_s0_hbm_transition),
	&PKTINFO(canvas1_a3_s0_acl_control),
	&PKTINFO(canvas1_a3_s0_elvss),
	&PKTINFO(canvas1_a3_s0_wrdisbv),
};

static DEFINE_SEQINFO(canvas1_a3_s0_set_bl_param_seq, canvas1_a3_s0_set_bl_param_cmdtbl);

static void *canvas1_a3_s0_set_bl_cmdtbl[] = {
	&KEYINFO(canvas1_a3_s0_level1_key_enable),
	&SEQINFO(canvas1_a3_s0_set_bl_param_seq),
	&PKTINFO(canvas1_a3_s0_gamma_update_enable),
	&KEYINFO(canvas1_a3_s0_level1_key_disable),
};

static void *canvas1_a3_s0_display_on_cmdtbl[] = {
	&KEYINFO(canvas1_a3_s0_level1_key_enable),
	&PKTINFO(canvas1_a3_s0_display_on),
	&KEYINFO(canvas1_a3_s0_level1_key_disable),
};

static void *canvas1_a3_s0_display_off_cmdtbl[] = {
	&KEYINFO(canvas1_a3_s0_level1_key_enable),
	&PKTINFO(canvas1_a3_s0_display_off),
	&KEYINFO(canvas1_a3_s0_level1_key_disable),
};

static void *canvas1_a3_s0_exit_cmdtbl[] = {
 	&KEYINFO(canvas1_a3_s0_level1_key_enable),
#ifdef CONFIG_DISPLAY_USE_INFO
	&KEYINFO(canvas1_a3_s0_level2_key_enable),
	&s6e3fab_dmptbl[DUMP_ERR_FG],
	&KEYINFO(canvas1_a3_s0_level2_key_disable),
	&s6e3fab_dmptbl[DUMP_DSI_ERR],
	&s6e3fab_dmptbl[DUMP_SELF_DIAG],
#endif
	&PKTINFO(canvas1_a3_s0_sleep_in),
	&KEYINFO(canvas1_a3_s0_level1_key_disable),
	&DLYINFO(canvas1_a3_s0_wait_sleep_in),
};

static void *canvas1_a3_s0_alpm_enter_cmdtbl[] = {
	&KEYINFO(canvas1_a3_s0_level1_key_enable),
	&PKTINFO(canvas1_a3_s0_lpm_on),
	&PKTINFO(canvas1_a3_s0_gamma_update_enable),
	&DLYINFO(canvas1_a3_s0_wait_1usec),
	&KEYINFO(canvas1_a3_s0_level1_key_disable),
	&DLYINFO(canvas1_a3_s0_wait_1_frame),
};

static void *canvas1_a3_s0_alpm_exit_cmdtbl[] = {
	&KEYINFO(canvas1_a3_s0_level1_key_enable),
	&DLYINFO(canvas1_a3_s0_wait_1usec),
	&KEYINFO(canvas1_a3_s0_level1_key_disable),
};

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static void *canvas1_a3_s0_dynamic_hlpm_on_cmdtbl[] = {
	&KEYINFO(canvas1_a3_s0_level2_key_enable),
	&PKTINFO(canvas1_a3_s0_dynamic_hlpm_enable),
	&KEYINFO(canvas1_a3_s0_level2_key_disable),
};

static void *canvas1_a3_s0_dynamic_hlpm_off_cmdtbl[] = {
	&KEYINFO(canvas1_a3_s0_level2_key_enable),
	&PKTINFO(canvas1_a3_s0_dynamic_hlpm_disable),
	&KEYINFO(canvas1_a3_s0_level2_key_disable),
};
#endif
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void *canvas1_a3_s0_cmdlog_dump_cmdtbl[] = {
	&KEYINFO(canvas1_a3_s0_level2_key_enable),
	&s6e3fab_dmptbl[DUMP_CMDLOG],
	&KEYINFO(canvas1_a3_s0_level2_key_disable),
};
#endif

static void *canvas1_a3_s0_dump_cmdtbl[] = {
	&KEYINFO(canvas1_a3_s0_level1_key_enable),
	&KEYINFO(canvas1_a3_s0_level2_key_enable),
	&KEYINFO(canvas1_a3_s0_level3_key_enable),
	&s6e3fab_dmptbl[DUMP_RDDPM],
	&s6e3fab_dmptbl[DUMP_RDDSM],
	&s6e3fab_dmptbl[DUMP_ERR],
	&s6e3fab_dmptbl[DUMP_ERR_FG],
	&s6e3fab_dmptbl[DUMP_DSI_ERR],
	&s6e3fab_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(canvas1_a3_s0_level3_key_disable),
	&KEYINFO(canvas1_a3_s0_level2_key_disable),
	&KEYINFO(canvas1_a3_s0_level1_key_disable),
};

static void *canvas1_a3_s0_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo canvas1_a3_s0_pre_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", canvas1_a3_s0_fab_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", canvas1_a3_s0_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", canvas1_a3_s0_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", canvas1_a3_s0_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", canvas1_a3_s0_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", canvas1_a3_s0_exit_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", canvas1_a3_s0_alpm_enter_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", canvas1_a3_s0_alpm_exit_cmdtbl),	
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	[PANEL_DYNAMIC_HLPM_ON_SEQ] = SEQINFO_INIT("dynamic-hlpm-on-seq", canvas1_a3_s0_dynamic_hlpm_on_cmdtbl),
	[PANEL_DYNAMIC_HLPM_OFF_SEQ] = SEQINFO_INIT("dynamic-hlpm-off-seq", canvas1_a3_s0_dynamic_hlpm_off_cmdtbl),
#endif
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", canvas1_a3_s0_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[PANEL_CMDLOG_DUMP_SEQ] = SEQINFO_INIT("cmdlog-dump-seq", canvas1_a3_s0_cmdlog_dump_cmdtbl),
#endif
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", canvas1_a3_s0_dummy_cmdtbl),
};

struct common_panel_info s6e3fab_canvas1_a3_s0_pre_panel_info = {
	.ldi_name = "s6e3fab",
	.name = "s6e3fab_canvas1_a3_s0",
	.model = "AMB675TG01",
	.vendor = "SDC",
	.id = 0x810000,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
	},
	.maptbl = canvas1_a3_s0_maptbl,
	.nr_maptbl = ARRAY_SIZE(canvas1_a3_s0_maptbl),
	.seqtbl = canvas1_a3_s0_pre_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(canvas1_a3_s0_pre_seqtbl),
	.rditbl = s6e3fab_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3fab_rditbl),
	.restbl = s6e3fab_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3fab_restbl),
	.dumpinfo = s6e3fab_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3fab_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &s6e3fab_canvas1_a3_s0_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3fab_canvas1_a3_s0_panel_dimming_info,
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3fab_canvas1_a3_s0_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	.copr_data = &s6e3fab_canvas1_a3_s0_copr_data,
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &s6e3fab_canvas1_aod,
#endif
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = &fab_profiler_tune,
#endif
};

static int __init s6e3fab_canvas1_a3_s0_panel_init(void)
{
	register_common_panel(&s6e3fab_canvas1_a3_s0_pre_panel_info);

	return 0;
}
arch_initcall(s6e3fab_canvas1_a3_s0_panel_init)
#endif /* __S6E3FAB_CANVAS1_A3_S0_PANEL_H__ */
