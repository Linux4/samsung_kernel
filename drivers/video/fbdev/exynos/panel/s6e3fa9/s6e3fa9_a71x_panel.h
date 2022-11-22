/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fa9/s6e3fa9_a71x_panel.h
 *
 * Header file for S6E3FA9 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FA9_A71X_PANEL_H__
#define __S6E3FA9_A71X_PANEL_H__
#include "../panel_drv.h"
#include "s6e3fa9.h"

#ifdef CONFIG_EXYNOS_DECON_MDNIE
#include "s6e3fa9_a71x_panel_mdnie.h"
#endif
#ifdef CONFIG_SUPPORT_HMD
#include "s6e3fa9_a71x_panel_hmd_dimming.h"
#endif
#ifdef CONFIG_SUPPORT_AOD_BL
#include "s6e3fa9_a71x_panel_aod_dimming.h"
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "s6e3fa9_a71x_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
#include "s6e3fa9_profiler_panel.h"
#include "../display_profiler/display_profiler.h"
#endif

#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "s6e3fa9_a71x_panel_poc.h"
#endif

#include "s6e3fa9_a71x_panel_dimming.h"
#include "s6e3fa9_a71x_resol.h"

#ifdef CONFIG_DYNAMIC_FREQ
#include "a71x_df_tbl.h"
#endif

#undef __pn_name__
#define __pn_name__	a71x

#undef __PN_NAME__
#define __PN_NAME__	A71X

static struct seqinfo a71x_seqtbl[MAX_PANEL_SEQ];
static struct seqinfo a71x_ub_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [S6E3FA9 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [S6E3FA9 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [S6E3FA9 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a71x_brt_table[S6E3FA9_TOTAL_NR_LUMINANCE + 1][2] = {
	{0x00, 0x03},	{0x00, 0x06},	{0x00, 0x09},	{0x00, 0x0C},	{0x00, 0x0F},	{0x00, 0x12},	{0x00, 0x15},	{0x00, 0x18},	{0x00, 0x1B},	{0x00, 0x1E},
	{0x00, 0x21},	{0x00, 0x24},	{0x00, 0x27},	{0x00, 0x2A},	{0x00, 0x2D},	{0x00, 0x30},	{0x00, 0x34},	{0x00, 0x37},	{0x00, 0x3B},	{0x00, 0x3E},
	{0x00, 0x42},	{0x00, 0x46},	{0x00, 0x49},	{0x00, 0x4D},	{0x00, 0x50},	{0x00, 0x54},	{0x00, 0x58},	{0x00, 0x5B},	{0x00, 0x5F},	{0x00, 0x62},
	{0x00, 0x66},	{0x00, 0x6A},	{0x00, 0x6D},	{0x00, 0x71},	{0x00, 0x74},	{0x00, 0x78},	{0x00, 0x7C},	{0x00, 0x7F},	{0x00, 0x83},	{0x00, 0x86},
	{0x00, 0x8A},	{0x00, 0x8E},	{0x00, 0x91},	{0x00, 0x95},	{0x00, 0x98},	{0x00, 0x9C},	{0x00, 0xA0},	{0x00, 0xA3},	{0x00, 0xA7},	{0x00, 0xAA},
	{0x00, 0xAE},	{0x00, 0xB2},	{0x00, 0xB5},	{0x00, 0xB9},	{0x00, 0xBC},	{0x00, 0xC0},	{0x00, 0xC4},	{0x00, 0xC7},	{0x00, 0xCB},	{0x00, 0xCE},
	{0x00, 0xD2},	{0x00, 0xD6},	{0x00, 0xD9},	{0x00, 0xDD},	{0x00, 0xE0},	{0x00, 0xE4},	{0x00, 0xE8},	{0x00, 0xEB},	{0x00, 0xEF},	{0x00, 0xF2},
	{0x00, 0xF6},	{0x00, 0xFA},	{0x00, 0xFD},	{0x01, 0x01},	{0x01, 0x04},	{0x01, 0x08},	{0x01, 0x0C},	{0x01, 0x0F},	{0x01, 0x13},	{0x01, 0x16},
	{0x01, 0x1A},	{0x01, 0x1E},	{0x01, 0x21},	{0x01, 0x25},	{0x01, 0x29},	{0x01, 0x2C},	{0x01, 0x30},	{0x01, 0x33},	{0x01, 0x37},	{0x01, 0x3B},
	{0x01, 0x3E},	{0x01, 0x42},	{0x01, 0x45},	{0x01, 0x49},	{0x01, 0x4D},	{0x01, 0x50},	{0x01, 0x54},	{0x01, 0x57},	{0x01, 0x5B},	{0x01, 0x5F},
	{0x01, 0x62},	{0x01, 0x66},	{0x01, 0x69},	{0x01, 0x6D},	{0x01, 0x71},	{0x01, 0x74},	{0x01, 0x78},	{0x01, 0x7B},	{0x01, 0x7F},	{0x01, 0x83},
	{0x01, 0x86},	{0x01, 0x8A},	{0x01, 0x8D},	{0x01, 0x91},	{0x01, 0x95},	{0x01, 0x98},	{0x01, 0x9C},	{0x01, 0x9F},	{0x01, 0xA3},	{0x01, 0xA7},
	{0x01, 0xAA},	{0x01, 0xAE},	{0x01, 0xB1},	{0x01, 0xB5},	{0x01, 0xB9},	{0x01, 0xBC},	{0x01, 0xC0},	{0x01, 0xC3},	{0x01, 0xC7},	{0x01, 0xCB},
	{0x01, 0xD0},	{0x01, 0xD4},	{0x01, 0xD9},	{0x01, 0xDD},	{0x01, 0xE2},	{0x01, 0xE6},	{0x01, 0xEB},	{0x01, 0xEF},	{0x01, 0xF4},	{0x01, 0xF8},
	{0x01, 0xFD},	{0x02, 0x01},	{0x02, 0x06},	{0x02, 0x0A},	{0x02, 0x0F},	{0x02, 0x13},	{0x02, 0x17},	{0x02, 0x1C},	{0x02, 0x20},	{0x02, 0x25},
	{0x02, 0x29},	{0x02, 0x2E},	{0x02, 0x32},	{0x02, 0x37},	{0x02, 0x3B},	{0x02, 0x40},	{0x02, 0x44},	{0x02, 0x49},	{0x02, 0x4D},	{0x02, 0x52},
	{0x02, 0x56},	{0x02, 0x5B},	{0x02, 0x5F},	{0x02, 0x64},	{0x02, 0x68},	{0x02, 0x6C},	{0x02, 0x71},	{0x02, 0x75},	{0x02, 0x7A},	{0x02, 0x7E},
	{0x02, 0x83},	{0x02, 0x87},	{0x02, 0x8C},	{0x02, 0x90},	{0x02, 0x95},	{0x02, 0x99},	{0x02, 0x9E},	{0x02, 0xA2},	{0x02, 0xA7},	{0x02, 0xAB},
	{0x02, 0xB0},	{0x02, 0xB4},	{0x02, 0xB8},	{0x02, 0xBD},	{0x02, 0xC1},	{0x02, 0xC6},	{0x02, 0xCA},	{0x02, 0xCF},	{0x02, 0xD3},	{0x02, 0xD8},
	{0x02, 0xDC},	{0x02, 0xE1},	{0x02, 0xE5},	{0x02, 0xEA},	{0x02, 0xEE},	{0x02, 0xF3},	{0x02, 0xF7},	{0x02, 0xFC},	{0x03, 0x00},	{0x03, 0x05},
	{0x03, 0x09},	{0x03, 0x0D},	{0x03, 0x12},	{0x03, 0x16},	{0x03, 0x1B},	{0x03, 0x1F},	{0x03, 0x24},	{0x03, 0x28},	{0x03, 0x2D},	{0x03, 0x31},
	{0x03, 0x36},	{0x03, 0x3A},	{0x03, 0x3F},	{0x03, 0x43},	{0x03, 0x48},	{0x03, 0x4C},	{0x03, 0x51},	{0x03, 0x55},	{0x03, 0x5A},	{0x03, 0x5E},
	{0x03, 0x62},	{0x03, 0x67},	{0x03, 0x6B},	{0x03, 0x70},	{0x03, 0x74},	{0x03, 0x79},	{0x03, 0x7D},	{0x03, 0x82},	{0x03, 0x86},	{0x03, 0x8B},
	{0x03, 0x8F},	{0x03, 0x94},	{0x03, 0x98},	{0x03, 0x9D},	{0x03, 0xA1},	{0x03, 0xA6},	{0x03, 0xAA},	{0x03, 0xAE},	{0x03, 0xB3},	{0x03, 0xB7},
	{0x03, 0xBC},	{0x03, 0xC0},	{0x03, 0xC5},	{0x03, 0xC9},	{0x03, 0xCE},	{0x03, 0xD2},	{0x03, 0xD7},	{0x03, 0xDB},	{0x03, 0xE0},	{0x03, 0xE4},
	{0x03, 0xE9},	{0x03, 0xED},	{0x03, 0xF2},	{0x03, 0xF6},	{0x03, 0xFB},	{0x03, 0xFF},	{0x00, 0x03},	{0x00, 0x05},	{0x00, 0x07},	{0x00, 0x09},
	{0x00, 0x0C},	{0x00, 0x0E},	{0x00, 0x11},	{0x00, 0x13},	{0x00, 0x15},	{0x00, 0x18},	{0x00, 0x1A},	{0x00, 0x1D},	{0x00, 0x1F},	{0x00, 0x21},
	{0x00, 0x24},	{0x00, 0x26},	{0x00, 0x28},	{0x00, 0x2B},	{0x00, 0x2D},	{0x00, 0x30},	{0x00, 0x32},	{0x00, 0x34},	{0x00, 0x37},	{0x00, 0x39},
	{0x00, 0x3B},	{0x00, 0x3E},	{0x00, 0x40},	{0x00, 0x43},	{0x00, 0x45},	{0x00, 0x47},	{0x00, 0x4A},	{0x00, 0x4C},	{0x00, 0x4E},	{0x00, 0x51},
	{0x00, 0x53},	{0x00, 0x56},	{0x00, 0x58},	{0x00, 0x5A},	{0x00, 0x5D},	{0x00, 0x5F},	{0x00, 0x61},	{0x00, 0x64},	{0x00, 0x66},	{0x00, 0x69},
	{0x00, 0x6B},	{0x00, 0x6D},	{0x00, 0x70},	{0x00, 0x72},	{0x00, 0x74},	{0x00, 0x77},	{0x00, 0x79},	{0x00, 0x7C},	{0x00, 0x7E},	{0x00, 0x80},
	{0x00, 0x83},	{0x00, 0x85},	{0x00, 0x87},	{0x00, 0x8A},	{0x00, 0x8C},	{0x00, 0x8F},	{0x00, 0x91},	{0x00, 0x93},	{0x00, 0x96},	{0x00, 0x98},
	{0x00, 0x9A},	{0x00, 0x9D},	{0x00, 0x9F},	{0x00, 0xA2},	{0x00, 0xA4},	{0x00, 0xA6},	{0x00, 0xA9},	{0x00, 0xAB},	{0x00, 0xAD},	{0x00, 0xB0},
	{0x00, 0xB2},	{0x00, 0xB5},	{0x00, 0xB7},	{0x00, 0xB9},	{0x00, 0xBC},	{0x00, 0xBE},	{0x00, 0xC0},	{0x00, 0xC3},	{0x00, 0xC5},	{0x00, 0xC8},
	{0x00, 0xCA},	{0x00, 0xCC},	{0x00, 0xCF},	{0x00, 0xD1},	{0x00, 0xD3},	{0x00, 0xD6},	{0x00, 0xD8},	{0x00, 0xDB},	{0x00, 0xDD},	{0x00, 0xDF},
	{0x00, 0xE2},	{0x00, 0xE4},	{0x00, 0xE7},	{0x00, 0xE9},	{0x00, 0xEB},	{0x00, 0xEE},	{0x00, 0xF0},	{0x00, 0xF2},	{0x00, 0xF5},	{0x00, 0xF7},
	{0x00, 0xFA},	{0x00, 0xFC},	{0x00, 0xFE},	{0x01, 0x01},	{0x01, 0x03},	{0x01, 0x05},	{0x01, 0x08},	{0x01, 0x0A},	{0x01, 0x0D},	{0x01, 0x0F},
	{0x01, 0x11},	{0x01, 0x14},	{0x01, 0x16},	{0x01, 0x18},	{0x01, 0x1B},	{0x01, 0x1D},	{0x01, 0x20},	{0x01, 0x22},	{0x01, 0x24},	{0x01, 0x27},
	{0x01, 0x29},	{0x01, 0x2B},	{0x01, 0x2E},	{0x01, 0x30},	{0x01, 0x33},	{0x01, 0x35},	{0x01, 0x37},	{0x01, 0x3A},	{0x01, 0x3C},	{0x01, 0x3E},
	{0x01, 0x41},	{0x01, 0x43},	{0x01, 0x46},	{0x01, 0x48},	{0x01, 0x4A},	{0x01, 0x4D},	{0x01, 0x4F},	{0x01, 0x51},	{0x01, 0x54},	{0x01, 0x56},
	{0x01, 0x59},	{0x01, 0x5B},	{0x01, 0x5D},	{0x01, 0x60},	{0x01, 0x62},	{0x01, 0x64},	{0x01, 0x67},	{0x01, 0x69},	{0x01, 0x6C},	{0x01, 0x6E},
	{0x01, 0x70},	{0x01, 0x73},	{0x01, 0x75},	{0x01, 0x77},	{0x01, 0x7A},	{0x01, 0x7C},	{0x01, 0x7F},	{0x01, 0x81},	{0x01, 0x83},	{0x01, 0x86},
	{0x01, 0x88},	{0x01, 0x8A},	{0x01, 0x8D},	{0x01, 0x8F},	{0x01, 0x92},	{0x01, 0x94},
};

static u8 s6e3fa9_a71x_elvss_table[S6E3FA9_TOTAL_STEP][1] = {
		/* OVER_ZERO */
		[0 ... 255] = { 0x10 },
		/* HBM */
		[256 ... 256] = { 0x1E },
		[257 ... 266] = { 0x1D },
		[267 ... 278] = { 0x1C },
		[279 ... 290] = { 0x1B },
		[291 ... 303] = { 0x1A },
		[304 ... 315] = { 0x19 },
		[316 ... 327] = { 0x18 },
		[328 ... 339] = { 0x17 },
		[340 ... 351] = { 0x16 },
		[352 ... 364] = { 0x15 },
		[365 ... 376] = { 0x14 },
		[377 ... 388] = { 0x13 },
		[389 ... 400] = { 0x12 },
		[401 ... 413] = { 0x11 },
		[414 ... 424] = { 0x10 },
		[425] = { 0x10 },
};

#ifdef CONFIG_SUPPORT_HMD
static u8 a71x_hmd_brt_table[S6E3FA9_NR_LUMINANCE + 1][2] = {
	{0x01, 0x0C},	{0x01, 0x0F},	{0x01, 0x12},	{0x01, 0x15},	{0x01, 0x18},	{0x01, 0x1B},	{0x01, 0x1E},	{0x01, 0x21},	{0x01, 0x24},	{0x01, 0x27},
	{0x01, 0x2A},	{0x01, 0x2D},	{0x01, 0x30},	{0x01, 0x32},	{0x01, 0x35},	{0x01, 0x38},	{0x01, 0x3B},	{0x01, 0x3E},	{0x01, 0x41},	{0x01, 0x44},
	{0x01, 0x47},	{0x01, 0x4A},	{0x01, 0x4D},	{0x01, 0x50},	{0x01, 0x53},	{0x01, 0x56},	{0x01, 0x59},	{0x01, 0x5C},	{0x01, 0x5F},	{0x01, 0x62},
	{0x01, 0x65},	{0x01, 0x68},	{0x01, 0x6B},	{0x01, 0x6E},	{0x01, 0x71},	{0x01, 0x74},	{0x01, 0x77},	{0x01, 0x7A},	{0x01, 0x7C},	{0x01, 0x7F},
	{0x01, 0x82},	{0x01, 0x85},	{0x01, 0x88},	{0x01, 0x8B},	{0x01, 0x8E},	{0x01, 0x91},	{0x01, 0x94},	{0x01, 0x97},	{0x01, 0x9A},	{0x01, 0x9D},
	{0x01, 0xA0},	{0x01, 0xA3},	{0x01, 0xA6},	{0x01, 0xA9},	{0x01, 0xAC},	{0x01, 0xAF},	{0x01, 0xB2},	{0x01, 0xB5},	{0x01, 0xB8},	{0x01, 0xBB},
	{0x01, 0xBE},	{0x01, 0xC1},	{0x01, 0xC4},	{0x01, 0xC7},	{0x01, 0xC9},	{0x01, 0xCC},	{0x01, 0xCF},	{0x01, 0xD2},	{0x01, 0xD5},	{0x01, 0xD8},
	{0x01, 0xDB},	{0x01, 0xDE},	{0x01, 0xE1},	{0x01, 0xE4},	{0x01, 0xE7},	{0x01, 0xEA},	{0x01, 0xED},	{0x01, 0xF0},	{0x01, 0xF3},	{0x01, 0xF6},
	{0x01, 0xF9},	{0x01, 0xFC},	{0x01, 0xFF},	{0x02, 0x02},	{0x02, 0x05},	{0x02, 0x08},	{0x02, 0x0B},	{0x02, 0x0E},	{0x02, 0x11},	{0x02, 0x13},
	{0x02, 0x16},	{0x02, 0x19},	{0x02, 0x1C},	{0x02, 0x1F},	{0x02, 0x22},	{0x02, 0x25},	{0x02, 0x28},	{0x02, 0x2B},	{0x02, 0x2E},	{0x02, 0x31},
	{0x02, 0x34},	{0x02, 0x37},	{0x02, 0x3A},	{0x02, 0x3D},	{0x02, 0x40},	{0x02, 0x43},	{0x02, 0x46},	{0x02, 0x49},	{0x02, 0x4C},	{0x02, 0x4F},
	{0x02, 0x52},	{0x02, 0x55},	{0x02, 0x58},	{0x02, 0x5B},	{0x02, 0x5E},	{0x02, 0x60},	{0x02, 0x63},	{0x02, 0x66},	{0x02, 0x69},	{0x02, 0x6C},
	{0x02, 0x6F},	{0x02, 0x72},	{0x02, 0x75},	{0x02, 0x78},	{0x02, 0x7B},	{0x02, 0x7E},	{0x02, 0x81},	{0x02, 0x84},	{0x02, 0x87},	{0x02, 0x8A},
	{0x02, 0x8D},	{0x02, 0x90},	{0x02, 0x93},	{0x02, 0x96},	{0x02, 0x99},	{0x02, 0x9C},	{0x02, 0x9F},	{0x02, 0xA2},	{0x02, 0xA5},	{0x02, 0xA8},
	{0x02, 0xAA},	{0x02, 0xAD},	{0x02, 0xB0},	{0x02, 0xB3},	{0x02, 0xB6},	{0x02, 0xB9},	{0x02, 0xBC},	{0x02, 0xBF},	{0x02, 0xC2},	{0x02, 0xC5},
	{0x02, 0xC8},	{0x02, 0xCB},	{0x02, 0xCE},	{0x02, 0xD1},	{0x02, 0xD4},	{0x02, 0xD7},	{0x02, 0xDA},	{0x02, 0xDD},	{0x02, 0xE0},	{0x02, 0xE3},
	{0x02, 0xE6},	{0x02, 0xE9},	{0x02, 0xEC},	{0x02, 0xEF},	{0x02, 0xF2},	{0x02, 0xF5},	{0x02, 0xF7},	{0x02, 0xFA},	{0x02, 0xFD},	{0x03, 0x00},
	{0x03, 0x03},	{0x03, 0x06},	{0x03, 0x09},	{0x03, 0x0C},	{0x03, 0x0F},	{0x03, 0x12},	{0x03, 0x15},	{0x03, 0x18},	{0x03, 0x1B},	{0x03, 0x1E},
	{0x03, 0x21},	{0x03, 0x24},	{0x03, 0x27},	{0x03, 0x2A},	{0x03, 0x2D},	{0x03, 0x30},	{0x03, 0x33},	{0x03, 0x36},	{0x03, 0x39},	{0x03, 0x3C},
	{0x03, 0x3F},	{0x03, 0x41},	{0x03, 0x44},	{0x03, 0x47},	{0x03, 0x4A},	{0x03, 0x4D},	{0x03, 0x50},	{0x03, 0x53},	{0x03, 0x56},	{0x03, 0x59},
	{0x03, 0x5C},	{0x03, 0x5F},	{0x03, 0x62},	{0x03, 0x65},	{0x03, 0x68},	{0x03, 0x6B},	{0x03, 0x6E},	{0x03, 0x71},	{0x03, 0x74},	{0x03, 0x77},
	{0x03, 0x7A},	{0x03, 0x7D},	{0x03, 0x80},	{0x03, 0x83},	{0x03, 0x86},	{0x03, 0x89},	{0x03, 0x8C},	{0x03, 0x8E},	{0x03, 0x91},	{0x03, 0x94},
	{0x03, 0x97},	{0x03, 0x9A},	{0x03, 0x9D},	{0x03, 0xA0},	{0x03, 0xA3},	{0x03, 0xA6},	{0x03, 0xA9},	{0x03, 0xAC},	{0x03, 0xAF},	{0x03, 0xB2},
	{0x03, 0xB5},	{0x03, 0xB8},	{0x03, 0xBB},	{0x03, 0xBE},	{0x03, 0xC1},	{0x03, 0xC4},	{0x03, 0xC7},	{0x03, 0xCA},	{0x03, 0xCD},	{0x03, 0xD0},
	{0x03, 0xD3},	{0x03, 0xD6},	{0x03, 0xD8},	{0x03, 0xDB},	{0x03, 0xDE},	{0x03, 0xE1},	{0x03, 0xE4},	{0x03, 0xE7},	{0x03, 0xEA},	{0x03, 0xED},
	{0x03, 0xF0},	{0x03, 0xF3},	{0x03, 0xF6},	{0x03, 0xF9},	{0x03, 0xFC},	{0x03, 0xFF},
};
#endif

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 A71X_XTALK_ON[] = { 0xF4, 0xC4 };		/* VGH 6.2V */
static u8 A71X_XTALK_OFF[] = { 0xF4, 0xCC };	/* VGH 7.0V */

static u8 A71X_BRIGHTNESS_MIN[] = {
	0x51,
	0x00, 0x03,
};
#endif

#ifdef CONFIG_SUPPORT_FAST_DISCHARGE
static u8 A71X_FD_ON[] = { 0xB5, 0x40 };
static u8 A71X_FD_OFF[] = { 0xB5, 0x80 };
#endif

#ifdef CONFIG_SUPPORT_MCD_TEST
static u8 A71X_MCD_ON_01[] = { 0xF4, 0xCE };
static u8 A71X_MCD_ON_02[] = {
	0xCB,
	0x00, 0x00, 0x42, 0x0F, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00,
	0x00, 0x40, 0x47, 0x00, 0x4D, 0x00, 0x00, 0x00, 0x20, 0x07,
	0x28, 0x38, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0,
	0x0B, 0x46,
};
static u8 A71X_MCD_ON_03[] = { 0xF6, 0x00 };
static u8 A71X_MCD_ON_04[] = { 0xCC, 0x12 };
static u8 A71X_MCD_ON_05[] = { 0xF7, 0x03 };

static u8 A71X_MCD_OFF_01[] = { 0xF4, 0xCC };
static u8 A71X_MCD_OFF_02[] = {
	0xCB,
	0x00, 0x00, 0x42, 0x0B, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00,
	0x00, 0x40, 0x47, 0x00, 0x4D, 0x00, 0x00, 0x00, 0x20, 0x07,
	0x28, 0x38, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0,
	0x00, 0x00,
};
static u8 A71X_MCD_OFF_03[] = { 0xF6, 0x90 };
static u8 A71X_MCD_OFF_04[] = { 0xCC, 0x00 };
static u8 A71X_MCD_OFF_05[] = { 0xF7, 0x03 };
#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static u8 A71X_GRAYSPOT_ON_01[] = { 0xF6, 0x00 };
static u8 A71X_GRAYSPOT_ON_02[] = {
	0xF2,
	0x02, 0x00, 0x58, 0x38, 0xD0,
};
static u8 A71X_GRAYSPOT_ON_03[] = { 0xF4, 0x1E };
static u8 A71X_GRAYSPOT_ON_04[] = {
	0xB5,
	0x19, 0x8D, 0x35, 0x00,
};
static u8 A71X_GRAYSPOT_ON_05[] = { 0xF7, 0x03 };

static u8 A71X_GRAYSPOT_OFF_01[] = { 0xF6, 0x90 };
static u8 A71X_GRAYSPOT_OFF_02[] = {
	0xF2,
	0x02, 0x0E, 0x58, 0x38, 0x50,
};
static u8 A71X_GRAYSPOT_OFF_03[] = { 0xF4, 0x1E };
static u8 A71X_GRAYSPOT_OFF_04[] = {
	0xB5,
	0x19, 0x8D, 0x00, 0x00,		/* 3rd : Start_Swire , 4th : Cal_Of fset */
};
static u8 A71X_GRAYSPOT_OFF_05[] = { 0xF7, 0x03 };
#endif

#ifdef CONFIG_SUPPORT_MST
static u8 A71X_MST_ON_01[] = {
	0xF6,
	0x3B, 0x00, 0x78,
};

static u8 A71X_MST_ON_02[] = {
	0xBF,
	0x33, 0x25,
};

static u8 A71X_MST_OFF_01[] = {
	0xF6,
	0x00, 0x00, 0x90,
};

static u8 A71X_MST_OFF_02[] = {
	0xBF,
	0x00, 0x07,
};
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static u8 A71X_CCD_ENABLE[] = { 0xCC, 0x01 };
static u8 A71X_CCD_DISABLE[] = { 0xCC, 0x00 };
#endif

static u8 a71x_acl_opr_table[ACL_OPR_MAX][1] = {
	[ACL_OPR_OFF] = { 0x00 }, /* ACL OFF OPR */
	[ACL_OPR_03P ... ACL_OPR_08P] = { 0x01 }, /* ACL ON OPR_8 */
	[ACL_OPR_12P ... ACL_OPR_15P] = { 0x02 }, /* ACL ON OPR_15 */
};

static u8 a71x_fps_table[][1] = {
	[S6E3FA9_VRR_FPS_60] = { 0x00 },
};

static u8 a71x_lpm_nit_table[][4][2] = {
	[ALPM_MODE] = {
		{0x89, 0x07}, // ALPM_2NIT
		{0x89, 0x07}, // ALPM_10NIT
		{0x59, 0x0C}, // ALPM_30NIT
		{0x09, 0x18}, // ALPM_60NIT
	},
	[HLPM_MODE] = {
		{0x89, 0x07}, // HLPM_2NIT
		{0x89, 0x07}, // HLPM_10NIT
		{0x59, 0x0C}, // HLPM_30NIT
		{0x09, 0x18}, // HLPM_60NIT
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

#ifdef CONFIG_DYNAMIC_FREQ
static u8 a71x_dynamic_ffc[][9] = {
	/*	A: 1176.5  B: 1195.5  C: 1150.5  D: 1157	*/
	{
		/* 1176.5 */
		0x0D, 0x10, 0x64, 0x1E, 0xAF, 0x05, 0x00, 0x26, 0xB0
	},
	{
		/* 1195.5 */
		0x0D, 0x10, 0x64, 0x1E, 0x32, 0x05, 0x00, 0x26, 0xB0
	},
	{
		/* 1150.5 */
		0x0D, 0x10, 0x64, 0x1F, 0x61, 0x05, 0x00, 0x26, 0xB0
	},
	{
		/* 1157 */
		0x0D, 0x10, 0x64, 0x1F, 0x34, 0x05, 0x00, 0x26, 0xB0
	},
};
#endif

static struct maptbl a71x_maptbl[MAX_MAPTBL] = {
	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(a71x_tset_table, init_common_table, NULL, copy_tset_maptbl),
	[ELVSS_MAPTBL] = DEFINE_2D_MAPTBL(s6e3fa9_a71x_elvss_table, init_elvss_table, getidx_brt_table, copy_common_maptbl),
	[ELVSS_OTP_MAPTBL] = DEFINE_0D_MAPTBL(s6e3fa9_a71x_elvss_otp_table, init_common_table, getidx_brt_table, copy_elvss_otp_maptbl),
	[ELVSS_TEMP_MAPTBL] = DEFINE_0D_MAPTBL(s6e3fa9_a71x_elvss_temp_table, init_common_table, getidx_brt_table, copy_tset_maptbl),
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(a71x_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
	[BRT_MODE_MAPTBL] = DEFINE_3D_MAPTBL(a71x_brt_mode_table, init_common_table, getidx_brt_mode_table, copy_common_maptbl),
	[ACL_OPR_MAPTBL] = DEFINE_2D_MAPTBL(a71x_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),
	[FPS_MAPTBL] = DEFINE_2D_MAPTBL(a71x_fps_table, init_common_table, getidx_fps_table, copy_common_maptbl),
	[LPM_NIT_MAPTBL] = DEFINE_3D_MAPTBL(a71x_lpm_nit_table, init_lpm_table, getidx_lpm_table, copy_common_maptbl),
	[LPM_ON_MAPTBL] = DEFINE_3D_MAPTBL(a71x_lpm_on_table, init_common_table, getidx_lpm_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_HMD
	[HMD_BRT_MAPTBL] = DEFINE_2D_MAPTBL(a71x_hmd_brt_table, init_hmd_brightness_table, getidx_hmd_brt_table, copy_common_maptbl),
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[GRAYSPOT_OTP_MAPTBL] = DEFINE_0D_MAPTBL(a71x_grayspot_table, init_common_table, NULL, copy_grayspot_otp_maptbl),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	[DYN_FFC_MAPTBL] = DEFINE_2D_MAPTBL(a71x_dynamic_ffc, init_common_table, getidx_dyn_ffc_table, copy_common_maptbl),
#endif
};

/* ===================================================================================== */
/* ============================== [S6E3FA9 COMMAND TABLE] ============================== */
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

static u8 A71X_TE_ON[] = { 0x35, 0x00, 0x00 };
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
	0xB9,
	0x00, 0x00, 0x14, 0x18, 0x00, 0x00, 0x00, 0x10,
};

static u8 A71X_TSP_VSYNC_ON_UB[] = {
	0xB9,
	0x01, 0x80, 0xE8, 0x09, 0x00, 0x00, 0x00, 0x11, 0x03,
};

static u8 A71X_FFC_SET_DEFAULT[] = {
	0xC5,
	0x0D, 0x10, 0x64, 0x1E, 0xAF, 0x05, 0x00, 0x26, 0xB0,
};
#ifdef CONFIG_DYNAMIC_FREQ
static u8 A71X_FFC_SET[] = {
	0xC5,
	0x0D, 0x10, 0x64, 0x1E, 0xAF, 0x05, 0x00, 0x26, 0xB0,
};

static u8 A71X_FFC_OFF_SET[] = {
	0xC5,
	0x08,
};
#endif

static u8 A71X_ERR_FG_EN[] = {
	0xE5,
	0x13,
};

static u8 A71X_ERR_FG_SET[] = {
	0xED,
	0x00, 0x4C, 0x40	/* 3rd	0x40 : default high */
};

static u8 A71X_ELVSS_SET[] = {
	0xB5,
	0x19,	/* 1st: TSET */
	0x8D, 0x10,
};

static u8 A71X_BRIGHTNESS[] = {
	0x51,
	0x01, 0xC7,
};

static u8 A71X_BRIGHTNESS_MODE[] = {
	0x53,
	0x28,	/* Normal Smooth */
};

static u8 A71X_EXIT_ALPM_NORMAL[] = {0x53, 0x20 };
static u8 A71X_ACL_CONTROL[] = {0x55, 0x00 };

static u8 A71X_LPM_AOR[] =  { 0xBB, 0x09, 0x18 };	/* HLPM 60nit */
static u8 A71X_LPM_ON[] = {0x53, 0x22};	/* HLPM 60nit */

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

static DEFINE_STATIC_PACKET(a71x_exit_alpm_normal, DSI_PKT_TYPE_WR, A71X_EXIT_ALPM_NORMAL, 0);

static DEFINE_STATIC_PACKET(a71x_te_off, DSI_PKT_TYPE_WR, A71X_TE_OFF, 0);
static DEFINE_STATIC_PACKET(a71x_te_on, DSI_PKT_TYPE_WR, A71X_TE_ON, 0);
static DEFINE_STATIC_PACKET(a71x_page_set_2a, DSI_PKT_TYPE_WR, A71X_PAGE_ADDR_SET_2A, 0);
static DEFINE_STATIC_PACKET(a71x_page_set_2b, DSI_PKT_TYPE_WR, A71X_PAGE_ADDR_SET_2B, 0);
static DEFINE_STATIC_PACKET(a71x_tsp_vsync, DSI_PKT_TYPE_WR, A71X_TSP_VSYNC_ON, 0);
static DEFINE_STATIC_PACKET(a71x_tsp_vsync_ub, DSI_PKT_TYPE_WR, A71X_TSP_VSYNC_ON_UB, 0);
static DEFINE_STATIC_PACKET(a71x_ffc_set_default, DSI_PKT_TYPE_WR, A71X_FFC_SET_DEFAULT, 0);
#ifdef CONFIG_DYNAMIC_FREQ
static DEFINE_PKTUI(a71x_ffc_set, &a71x_maptbl[DYN_FFC_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a71x_ffc_set, DSI_PKT_TYPE_WR, A71X_FFC_SET, 0);
static DEFINE_STATIC_PACKET(a71x_ffc_off_set, DSI_PKT_TYPE_WR, A71X_FFC_OFF_SET, 0);
#endif
static DEFINE_STATIC_PACKET(a71x_err_fg_enable, DSI_PKT_TYPE_WR, A71X_ERR_FG_EN, 0);
static DEFINE_STATIC_PACKET(a71x_err_fg_set, DSI_PKT_TYPE_WR, A71X_ERR_FG_SET, 0);

static DEFINE_PKTUI(a71x_lpm_nit, &a71x_maptbl[LPM_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a71x_lpm_nit, DSI_PKT_TYPE_WR, A71X_LPM_AOR, 2);
static DEFINE_PKTUI(a71x_lpm_on, &a71x_maptbl[LPM_ON_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a71x_lpm_on, DSI_PKT_TYPE_WR, A71X_LPM_ON, 0);


static DECLARE_PKTUI(a71x_elvss_set) = {
	{ .offset = 1, .maptbl = &a71x_maptbl[ELVSS_TEMP_MAPTBL] },
	{ .offset = 3, .maptbl = &a71x_maptbl[ELVSS_MAPTBL] },
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

#ifdef CONFIG_SUPPORT_XTALK_MODE
static DEFINE_STATIC_PACKET(a71x_xtalk_on, DSI_PKT_TYPE_WR, A71X_XTALK_ON, 0);
static DEFINE_STATIC_PACKET(a71x_xtalk_off, DSI_PKT_TYPE_WR, A71X_XTALK_OFF, 0);
static DEFINE_STATIC_PACKET(a71x_brightness_min, DSI_PKT_TYPE_WR, A71X_BRIGHTNESS_MIN, 0);
#endif
#ifdef CONFIG_SUPPORT_FAST_DISCHARGE
static DEFINE_STATIC_PACKET(a71x_fd_on, DSI_PKT_TYPE_WR, A71X_FD_ON, 0x0B);
static DEFINE_STATIC_PACKET(a71x_fd_off, DSI_PKT_TYPE_WR, A71X_FD_OFF, 0x0B);
#endif

#ifdef CONFIG_SUPPORT_MCD_TEST
static DEFINE_STATIC_PACKET(a71x_mcd_on_01, DSI_PKT_TYPE_WR, A71X_MCD_ON_01, 0);
static DEFINE_STATIC_PACKET(a71x_mcd_on_02, DSI_PKT_TYPE_WR, A71X_MCD_ON_02, 0);
static DEFINE_STATIC_PACKET(a71x_mcd_on_03, DSI_PKT_TYPE_WR, A71X_MCD_ON_03, 0x05);
static DEFINE_STATIC_PACKET(a71x_mcd_on_04, DSI_PKT_TYPE_WR, A71X_MCD_ON_04, 4);
static DEFINE_STATIC_PACKET(a71x_mcd_on_05, DSI_PKT_TYPE_WR, A71X_MCD_ON_05, 0);

static DEFINE_STATIC_PACKET(a71x_mcd_off_01, DSI_PKT_TYPE_WR, A71X_MCD_OFF_01, 0);
static DEFINE_STATIC_PACKET(a71x_mcd_off_02, DSI_PKT_TYPE_WR, A71X_MCD_OFF_02, 0);
static DEFINE_STATIC_PACKET(a71x_mcd_off_03, DSI_PKT_TYPE_WR, A71X_MCD_OFF_03, 0x05);
static DEFINE_STATIC_PACKET(a71x_mcd_off_04, DSI_PKT_TYPE_WR, A71X_MCD_OFF_04, 4);
static DEFINE_STATIC_PACKET(a71x_mcd_off_05, DSI_PKT_TYPE_WR, A71X_MCD_OFF_05, 0);
#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static DEFINE_STATIC_PACKET(a71x_grayspot_on_01, DSI_PKT_TYPE_WR, A71X_GRAYSPOT_ON_01, 5);
static DEFINE_STATIC_PACKET(a71x_grayspot_on_02, DSI_PKT_TYPE_WR, A71X_GRAYSPOT_ON_02, 7);
static DEFINE_STATIC_PACKET(a71x_grayspot_on_03, DSI_PKT_TYPE_WR, A71X_GRAYSPOT_ON_03, 0x11);
static DEFINE_STATIC_PACKET(a71x_grayspot_on_04, DSI_PKT_TYPE_WR, A71X_GRAYSPOT_ON_04, 0);
static DEFINE_STATIC_PACKET(a71x_grayspot_on_05, DSI_PKT_TYPE_WR, A71X_GRAYSPOT_ON_05, 0);

static DEFINE_STATIC_PACKET(a71x_grayspot_off_01, DSI_PKT_TYPE_WR, A71X_GRAYSPOT_OFF_01, 5);
static DEFINE_STATIC_PACKET(a71x_grayspot_off_02, DSI_PKT_TYPE_WR, A71X_GRAYSPOT_OFF_02, 7);
static DEFINE_STATIC_PACKET(a71x_grayspot_off_03, DSI_PKT_TYPE_WR, A71X_GRAYSPOT_OFF_03, 0x11);
static DEFINE_PKTUI(a71x_grayspot_off_04, &a71x_maptbl[GRAYSPOT_OTP_MAPTBL], 3);
static DEFINE_VARIABLE_PACKET(a71x_grayspot_off_04, DSI_PKT_TYPE_WR, A71X_GRAYSPOT_OFF_04, 0);
static DEFINE_STATIC_PACKET(a71x_grayspot_off_05, DSI_PKT_TYPE_WR, A71X_GRAYSPOT_OFF_05, 0);
#endif

#ifdef CONFIG_SUPPORT_MST
static DEFINE_STATIC_PACKET(a71x_mst_on_01, DSI_PKT_TYPE_WR, A71X_MST_ON_01, 3);
static DEFINE_STATIC_PACKET(a71x_mst_on_02, DSI_PKT_TYPE_WR, A71X_MST_ON_02, 0);

static DEFINE_STATIC_PACKET(a71x_mst_off_01, DSI_PKT_TYPE_WR, A71X_MST_OFF_01, 3);
static DEFINE_STATIC_PACKET(a71x_mst_off_02, DSI_PKT_TYPE_WR, A71X_MST_OFF_02, 0);
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static DEFINE_STATIC_PACKET(a71x_ccd_test_enable, DSI_PKT_TYPE_WR, A71X_CCD_ENABLE, 2);
static DEFINE_STATIC_PACKET(a71x_ccd_test_disable, DSI_PKT_TYPE_WR, A71X_CCD_DISABLE, 2);
#endif

#ifdef CONFIG_SUPPORT_HMD
/* Command for hmd on */
static u8 A71X_HMD_ON_AID_SET[] = {
	0xB7,
	0x07, 0x9A,
};

static u8 A71X_HMD_ON_LTPS_SET[] = {
	0xCB,
	0x00, 0x00, 0x42, 0x03 , 0x00, 0x0E,
};

static u8 A71X_HMD_ON_LTPS_UPDATE[] = {
	0xF7,
	0x03,
};

static DEFINE_STATIC_PACKET(a71x_hmd_on_aid_set, DSI_PKT_TYPE_WR, A71X_HMD_ON_AID_SET, 0xE0);
static DEFINE_STATIC_PACKET(a71x_hmd_on_ltps_set, DSI_PKT_TYPE_WR, A71X_HMD_ON_LTPS_SET, 0);
static DEFINE_STATIC_PACKET(a71x_hmd_on_ltps_update, DSI_PKT_TYPE_WR, A71X_HMD_ON_LTPS_UPDATE, 0);

/* Command for hmd off */
static u8 A71X_HMD_OFF_AID_SET[] = {
	0xB7,
	0x00, 0x18,
};

static u8 A71X_HMD_OFF_LTPS_SET[] = {
	0xCB,
	0x00, 0x00, 0x42, 0x0B , 0x00, 0x06,
};

static u8 A71X_HMD_OFF_LTPS_UPDATE[] = {
	0xF7,
	0x03,
};

static DEFINE_STATIC_PACKET(a71x_hmd_off_aid_set, DSI_PKT_TYPE_WR, A71X_HMD_OFF_AID_SET, 0xE0);
static DEFINE_STATIC_PACKET(a71x_hmd_off_ltps_set, DSI_PKT_TYPE_WR, A71X_HMD_OFF_LTPS_SET, 0);
static DEFINE_STATIC_PACKET(a71x_hmd_off_ltps_update, DSI_PKT_TYPE_WR, A71X_HMD_OFF_LTPS_UPDATE, 0);

static u8 A71X_HMD_BRIGHTNESS[] = {
	0x51,
	0x03, 0x21,
};

static DEFINE_PKTUI(a71x_hmd_brightness, &a71x_maptbl[HMD_BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a71x_hmd_brightness, DSI_PKT_TYPE_WR, A71X_HMD_BRIGHTNESS, 0);
#endif

#ifdef CONFIG_USE_DDI_BLACK_GRID
static u8 A71X_DDI_BLACK_GRID_ON[] = {
	0xBF,
	0x01, 0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
};
static DEFINE_STATIC_PACKET(a71x_ddi_black_grid_on, DSI_PKT_TYPE_WR, A71X_DDI_BLACK_GRID_ON, 0);

static u8 A71X_DDI_BLACK_GRID_OFF[] = {
	0xBF,
	0x00, 0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
};
static DEFINE_STATIC_PACKET(a71x_ddi_black_grid_off, DSI_PKT_TYPE_WR, A71X_DDI_BLACK_GRID_OFF, 0);

#endif

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
	&KEYINFO(a71x_level3_key_enable),
	&PKTINFO(a71x_exit_alpm_normal),
	&PKTINFO(a71x_elvss_set),
	&PKTINFO(a71x_brightness),
	&PKTINFO(a71x_acl_control),
	&PKTINFO(a71x_tsp_vsync),
	&KEYINFO(a71x_level2_key_disable),
	&KEYINFO(a71x_level3_key_disable),
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_te_on),
	&KEYINFO(a71x_level2_key_disable),
	&DLYINFO(a71x_wait_110msec),
};

static void *a71x_ub_init_cmdtbl[] = {
	&PKTINFO(a71x_sleep_out),
	&DLYINFO(a71x_wait_sleep_out),
#ifdef CONFIG_SEC_FACTORY
	&KEYINFO(a71x_level2_key_enable),
	&KEYINFO(a71x_level3_key_enable),
	&s6e3fa9_restbl[RES_CELL_ID],
	&s6e3fa9_restbl[RES_MANUFACTURE_INFO],
	&KEYINFO(a71x_level3_key_disable),
	&KEYINFO(a71x_level2_key_disable),
#endif
#ifdef CONFIG_SUPPORT_FAST_DISCHARGE
	&SEQINFO(a71x_ub_seqtbl[PANEL_FD_ON_SEQ]),
#endif
	&PKTINFO(a71x_page_set_2a),
	&PKTINFO(a71x_page_set_2b),
	&KEYINFO(a71x_level2_key_enable),
	&KEYINFO(a71x_level3_key_enable),
	&PKTINFO(a71x_ffc_set_default),
	&PKTINFO(a71x_err_fg_enable),
	&PKTINFO(a71x_err_fg_set),
	&PKTINFO(a71x_exit_alpm_normal),
	&PKTINFO(a71x_elvss_set),
	&PKTINFO(a71x_brightness),
	&PKTINFO(a71x_acl_control),
	&PKTINFO(a71x_tsp_vsync_ub),
	&KEYINFO(a71x_level2_key_disable),
	&KEYINFO(a71x_level3_key_disable),
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_te_on),
	&KEYINFO(a71x_level2_key_disable),
	&DLYINFO(a71x_wait_110msec),
};

static void *a71x_res_init_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&KEYINFO(a71x_level3_key_enable),
	&s6e3fa9_restbl[RES_ID],
	&s6e3fa9_restbl[RES_COORDINATE],
	&s6e3fa9_restbl[RES_CODE],
	&s6e3fa9_restbl[RES_ELVSS],
	&s6e3fa9_restbl[RES_DATE],
	&s6e3fa9_restbl[RES_CELL_ID],
	&s6e3fa9_restbl[RES_MANUFACTURE_INFO],
	&s6e3fa9_restbl[RES_OCTA_ID],
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e3fa9_restbl[RES_ERR_FG],
	&s6e3fa9_restbl[RES_DSI_ERR],
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	&s6e3fa9_restbl[RES_GRAYSPOT],
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
	&s6e3fa9_dmptbl[DUMP_ERR_FG],
	&KEYINFO(a71x_level2_key_disable),
	&s6e3fa9_dmptbl[DUMP_DSI_ERR],
#endif
	&PKTINFO(a71x_sleep_in),
	&DLYINFO(a71x_wait_sleep_in),
};

static void *a71x_alpm_enter_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_lpm_nit),
	&PKTINFO(a71x_lpm_on),
	&DLYINFO(a71x_wait_10usec),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_alpm_exit_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_exit_alpm_normal),
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
	&s6e3fa9_dmptbl[DUMP_RDDPM],
	&s6e3fa9_dmptbl[DUMP_RDDSM],
	&s6e3fa9_dmptbl[DUMP_ERR],
	&s6e3fa9_dmptbl[DUMP_ERR_FG],
	&s6e3fa9_dmptbl[DUMP_DSI_ERR],
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

#ifdef CONFIG_DYNAMIC_FREQ
static void *a71x_dynamic_ffc_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&KEYINFO(a71x_level3_key_enable),
	&PKTINFO(a71x_ffc_set),
	&KEYINFO(a71x_level3_key_disable),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_dynamic_ffc_off_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&KEYINFO(a71x_level3_key_enable),
	&PKTINFO(a71x_ffc_off_set),
	&KEYINFO(a71x_level3_key_disable),
	&KEYINFO(a71x_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_XTALK_MODE
static void *a71x_xtalk_on_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_brightness_min),
	&PKTINFO(a71x_xtalk_on),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_xtalk_off_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_brightness),
	&PKTINFO(a71x_xtalk_off),
	&KEYINFO(a71x_level2_key_disable),
};
#endif
#ifdef CONFIG_SUPPORT_FAST_DISCHARGE
static void *a71x_fd_on_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_fd_on),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_fd_off_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_fd_off),
	&KEYINFO(a71x_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_MCD_TEST
static void *a71x_mcd_on_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_mcd_on_01),
	&PKTINFO(a71x_mcd_on_02),
	&PKTINFO(a71x_mcd_on_03),
	&PKTINFO(a71x_mcd_on_04),
	&PKTINFO(a71x_mcd_on_05),
	&DLYINFO(a71x_wait_100msec),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_mcd_off_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_mcd_off_01),
	&PKTINFO(a71x_mcd_off_02),
	&PKTINFO(a71x_mcd_off_03),
	&PKTINFO(a71x_mcd_off_04),
	&PKTINFO(a71x_mcd_off_05),
	&DLYINFO(a71x_wait_100msec),
	&KEYINFO(a71x_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void *a71x_grayspot_on_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_grayspot_on_01),
	&PKTINFO(a71x_grayspot_on_02),
	&PKTINFO(a71x_grayspot_on_03),
	&PKTINFO(a71x_grayspot_on_04),
	&PKTINFO(a71x_grayspot_on_05),
	&DLYINFO(a71x_wait_100msec),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_grayspot_off_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_grayspot_off_01),
	&PKTINFO(a71x_grayspot_off_02),
	&PKTINFO(a71x_grayspot_off_03),
	&PKTINFO(a71x_grayspot_off_04),
	&PKTINFO(a71x_grayspot_off_05),
	&DLYINFO(a71x_wait_100msec),
	&KEYINFO(a71x_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_MST
static void *a71x_mst_on_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_mst_on_01),
	&PKTINFO(a71x_mst_on_02),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_mst_off_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_mst_off_01),
	&PKTINFO(a71x_mst_off_02),
	&KEYINFO(a71x_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static void *a71x_ccd_test_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_ccd_test_enable),
	&DLYINFO(a71x_wait_1msec),
	&s6e3fa9_restbl[RES_CCD_STATE],
	&PKTINFO(a71x_ccd_test_disable),
	&KEYINFO(a71x_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_HMD
static void *a71x_hmd_on_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_hmd_on_aid_set),
	&PKTINFO(a71x_hmd_on_ltps_set),
	&PKTINFO(a71x_hmd_on_ltps_update),
	&PKTINFO(a71x_hmd_brightness),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_hmd_off_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_hmd_off_aid_set),
	&PKTINFO(a71x_hmd_off_ltps_set),
	&PKTINFO(a71x_hmd_off_ltps_update),
	&KEYINFO(a71x_level2_key_disable),
};

static void *a71x_hmd_bl_cmdtbl[] = {
	&KEYINFO(a71x_level2_key_enable),
	&PKTINFO(a71x_hmd_brightness),
	&KEYINFO(a71x_level2_key_disable),
};
#endif

#ifdef CONFIG_USE_DDI_BLACK_GRID
static void *a71x_ddi_black_grid_on_cmdtbl[] = {
	&KEYINFO(a71x_level3_key_enable),
	&PKTINFO(a71x_ddi_black_grid_on),
	&KEYINFO(a71x_level3_key_disable),
};

static void *a71x_ddi_black_grid_off_cmdtbl[] = {
	&KEYINFO(a71x_level3_key_enable),
	&PKTINFO(a71x_ddi_black_grid_off),
	&KEYINFO(a71x_level3_key_disable),
};
#endif

static struct seqinfo a71x_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a71x_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a71x_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a71x_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a71x_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a71x_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a71x_exit_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", a71x_alpm_enter_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", a71x_alpm_exit_cmdtbl),
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", a71x_set_fps_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", a71x_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_INDISPLAY
	[PANEL_INDISPLAY_ENTER_BEFORE_SEQ] = SEQINFO_INIT("indisplay-delay", a71x_indisplay_enter_before_cmdtbl),
	[PANEL_INDISPLAY_ENTER_AFTER_SEQ] = SEQINFO_INIT("indisplay-delay", a71x_indisplay_enter_after_cmdtbl),
#endif
};

static struct seqinfo a71x_ub_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a71x_ub_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a71x_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a71x_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a71x_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a71x_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a71x_exit_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", a71x_alpm_enter_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", a71x_alpm_exit_cmdtbl),
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", a71x_set_fps_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", a71x_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_INDISPLAY
	[PANEL_INDISPLAY_ENTER_BEFORE_SEQ] = SEQINFO_INIT("indisplay-delay", a71x_indisplay_enter_before_cmdtbl),
	[PANEL_INDISPLAY_ENTER_AFTER_SEQ] = SEQINFO_INIT("indisplay-delay", a71x_indisplay_enter_after_cmdtbl),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	[PANEL_DYNAMIC_FFC_SEQ] = SEQINFO_INIT("dynamic-ffc-seq", a71x_dynamic_ffc_cmdtbl),
	[PANEL_DYNAMIC_FFC_OFF_SEQ] = SEQINFO_INIT("dynamic-ffc-off-seq", a71x_dynamic_ffc_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
	[PANEL_XTALK_ON_SEQ] = SEQINFO_INIT("xtalk-on-seq", a71x_xtalk_on_cmdtbl),
	[PANEL_XTALK_OFF_SEQ] = SEQINFO_INIT("xtalk-off-seq", a71x_xtalk_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_FAST_DISCHARGE
	[PANEL_FD_ON_SEQ] = SEQINFO_INIT("fd-on-seq", a71x_fd_on_cmdtbl),
	[PANEL_FD_OFF_SEQ] = SEQINFO_INIT("fd-off-seq", a71x_fd_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MCD_TEST
	[PANEL_MCD_ON_SEQ] = SEQINFO_INIT("mcd-on-seq", a71x_mcd_on_cmdtbl),
	[PANEL_MCD_OFF_SEQ] = SEQINFO_INIT("mcd-off-seq", a71x_mcd_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[PANEL_GRAYSPOT_ON_SEQ] = SEQINFO_INIT("grayspot-on-seq", a71x_grayspot_on_cmdtbl),
	[PANEL_GRAYSPOT_OFF_SEQ] = SEQINFO_INIT("grayspot-off-seq", a71x_grayspot_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MST
	[PANEL_MST_ON_SEQ] = SEQINFO_INIT("mst-on-seq", a71x_mst_on_cmdtbl),
	[PANEL_MST_OFF_SEQ] = SEQINFO_INIT("mst-off-seq", a71x_mst_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[PANEL_CCD_TEST_SEQ] = SEQINFO_INIT("ccd-test-seq", a71x_ccd_test_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_HMD
	[PANEL_HMD_ON_SEQ] = SEQINFO_INIT("hmd-on-seq", a71x_hmd_on_cmdtbl),
	[PANEL_HMD_OFF_SEQ] = SEQINFO_INIT("hmd-off-seq", a71x_hmd_off_cmdtbl),
	[PANEL_HMD_BL_SEQ] = SEQINFO_INIT("hmd-bl-seq", a71x_hmd_bl_cmdtbl),
#endif
#ifdef CONFIG_USE_DDI_BLACK_GRID
	[PANEL_DDI_BLACK_GRID_ON_SEQ] = SEQINFO_INIT("ddi-black-grid-on-seq", a71x_ddi_black_grid_on_cmdtbl),
	[PANEL_DDI_BLACK_GRID_OFF_SEQ] = SEQINFO_INIT("ddi-black-grid-off-seq", a71x_ddi_black_grid_off_cmdtbl),
#endif
};

struct common_panel_info s6e3fa9_a71x_default_panel_info = {
	.ldi_name = "s6e3fa9",
	.name = "s6e3fa9_a71x_default",
	.model = "AMS667UK01",
	.vendor = "SDC",
	.id = 0x800001,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA),
		.err_fg_recovery = false,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3fa9_a71x_resol),
		.resol = s6e3fa9_a71x_resol,
	},
	.maptbl = a71x_maptbl,
	.nr_maptbl = ARRAY_SIZE(a71x_maptbl),
	.seqtbl = a71x_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a71x_seqtbl),
	.rditbl = s6e3fa9_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3fa9_rditbl),
	.restbl = s6e3fa9_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3fa9_restbl),
	.dumpinfo = s6e3fa9_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3fa9_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE
	.mdnie_tune = &s6e3fa9_a71x_mdnie_tune,
#endif
	.panel_dim_info = {
		&s6e3fa9_a71x_panel_dimming_info,
	},
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = &fa9_profiler_tune,
#endif
};

struct common_panel_info s6e3fa9_a71x_ub_panel_info = {
	.ldi_name = "s6e3fa9",
	.name = "s6e3fa9_a71x_ub",
	.model = "AMB667UM01",
	.vendor = "SDC",
	.id = 0x814C00,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3fa9_a71x_resol),
		.resol = s6e3fa9_a71x_resol,
	},
	.maptbl = a71x_maptbl,
	.nr_maptbl = ARRAY_SIZE(a71x_maptbl),
	.seqtbl = a71x_ub_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a71x_ub_seqtbl),
	.rditbl = s6e3fa9_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3fa9_rditbl),
	.restbl = s6e3fa9_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3fa9_restbl),
	.dumpinfo = s6e3fa9_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3fa9_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE
	.mdnie_tune = &s6e3fa9_a71x_mdnie_tune,
#endif
	.panel_dim_info = {
		&s6e3fa9_a71x_panel_dimming_info,
#ifdef CONFIG_SUPPORT_HMD
		&s6e3fa9_a71x_panel_hmd_dimming_info,
#endif
#ifdef CONFIG_SUPPORT_AOD_BL
		&s6e3fa9_a71x_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &s6e3fa9_a71x_aod,
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	.df_freq_tbl = a71x_dynamic_freq_set,
#endif
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = &fa9_profiler_tune,
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
	.poc_data = &s6e3fa9_a71x_poc_data,
#endif
};

static int __init s6e3fa9_a71x_panel_init(void)
{
	register_common_panel(&s6e3fa9_a71x_default_panel_info);
	register_common_panel(&s6e3fa9_a71x_ub_panel_info);

	return 0;
}
arch_initcall(s6e3fa9_a71x_panel_init)
#endif /* __S6E3FA9_A71X_PANEL_H__ */
