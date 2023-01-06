/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3hac/s6e3hac_canvas2_a3_s0_panel.h
 *
 * Header file for S6E3HAC Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3HAC_CANVAS2_A3_S0_PANEL_H__
#define __S6E3HAC_CANVAS2_A3_S0_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "s6e3hac.h"
#include "s6e3hac_dimming.h"
#ifdef CONFIG_SUPPORT_POC_SPI
#include "../panel_spi.h"
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
#include "s6e3hac_canvas2_a3_s0_panel_mdnie.h"
#endif
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
#include "s6e3hac_canvas2_a3_s0_panel_copr.h"
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
#include "s6e3hac_canvas2_panel_poc.h"
#endif
#include "s6e3hac_canvas2_a3_s0_panel_dimming.h"
#ifdef CONFIG_SUPPORT_AOD_BL
#include "s6e3hac_canvas2_a3_s0_panel_aod_dimming.h"
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
#include "s6e3hac_canvas2_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
#include "s6e3hac_profiler_panel.h"
#include "../display_profiler/display_profiler.h"
#endif

#if defined(__PANEL_NOT_USED_VARIABLE__)
#include "s6e3hac_canvas2_a3_s0_panel_irc.h"
#endif
#include "s6e3hac_canvas2_resol.h"
#ifdef CONFIG_SUPPORT_POC_SPI
#include "../spi/w25q80_panel_spi.h"
#include "../spi/mx25r4035_panel_spi.h"
#endif

#ifdef CONFIG_DYNAMIC_FREQ
#include "s6e3hac_c2_df_tbl.h"
#endif

#ifdef CONFIG_SUPPORT_MAFPC
#include "s6e3hac_canvas2_mafpc_panel.h"
#endif

#undef __pn_name__
#define __pn_name__	canvas2_a3_s0

#undef __PN_NAME__
#define __PN_NAME__	CANVAS2_A3_S0

/* ===================================================================================== */
/* ============================= [S6E3HAC READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [S6E3HAC RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [S6E3HAC MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 canvas2_a3_s0_brt_table[S6E3HAC_TOTAL_STEP][2] = {
	/* Normal 5x51+1 */
	{ 0x00, 0x05 }, { 0x00, 0x05 }, { 0x00, 0x09 }, { 0x00, 0x09 }, { 0x00, 0x0A },
	{ 0x00, 0x0A }, { 0x00, 0x0E }, { 0x00, 0x0E }, { 0x00, 0x13 }, { 0x00, 0x13 },
	{ 0x00, 0x14 }, { 0x00, 0x14 }, { 0x00, 0x17 }, { 0x00, 0x17 }, { 0x00, 0x1A },
	{ 0x00, 0x1B }, { 0x00, 0x1D }, { 0x00, 0x1F }, { 0x00, 0x21 }, { 0x00, 0x24 },
	{ 0x00, 0x26 }, { 0x00, 0x28 }, { 0x00, 0x2A }, { 0x00, 0x2C }, { 0x00, 0x2E },
	{ 0x00, 0x31 }, { 0x00, 0x33 }, { 0x00, 0x35 }, { 0x00, 0x38 }, { 0x00, 0x3A },
	{ 0x00, 0x3C }, { 0x00, 0x3F }, { 0x00, 0x41 }, { 0x00, 0x44 }, { 0x00, 0x46 },
	{ 0x00, 0x49 }, { 0x00, 0x4B }, { 0x00, 0x4E }, { 0x00, 0x50 }, { 0x00, 0x53 },
	{ 0x00, 0x55 }, { 0x00, 0x58 }, { 0x00, 0x5B }, { 0x00, 0x5D }, { 0x00, 0x60 },
	{ 0x00, 0x63 }, { 0x00, 0x65 }, { 0x00, 0x68 }, { 0x00, 0x6B }, { 0x00, 0x6E },
	{ 0x00, 0x70 }, { 0x00, 0x73 }, { 0x00, 0x76 }, { 0x00, 0x79 }, { 0x00, 0x7C },
	{ 0x00, 0x7F }, { 0x00, 0x82 }, { 0x00, 0x84 }, { 0x00, 0x87 }, { 0x00, 0x8A },
	{ 0x00, 0x8D }, { 0x00, 0x90 }, { 0x00, 0x93 }, { 0x00, 0x96 }, { 0x00, 0x99 },
	{ 0x00, 0x9C }, { 0x00, 0x9F }, { 0x00, 0xA2 }, { 0x00, 0xA5 }, { 0x00, 0xA8 },
	{ 0x00, 0xAB }, { 0x00, 0xAE }, { 0x00, 0xB2 }, { 0x00, 0xB5 }, { 0x00, 0xB8 },
	{ 0x00, 0xBB }, { 0x00, 0xBE }, { 0x00, 0xC1 }, { 0x00, 0xC5 }, { 0x00, 0xC8 },
	{ 0x00, 0xCB }, { 0x00, 0xCE }, { 0x00, 0xD1 }, { 0x00, 0xD5 }, { 0x00, 0xD8 },
	{ 0x00, 0xDB }, { 0x00, 0xDE }, { 0x00, 0xE2 }, { 0x00, 0xE5 }, { 0x00, 0xE8 },
	{ 0x00, 0xEC }, { 0x00, 0xEF }, { 0x00, 0xF2 }, { 0x00, 0xF6 }, { 0x00, 0xF9 },
	{ 0x00, 0xFC }, { 0x01, 0x00 }, { 0x01, 0x03 }, { 0x01, 0x07 }, { 0x01, 0x0A },
	{ 0x01, 0x0E }, { 0x01, 0x11 }, { 0x01, 0x14 }, { 0x01, 0x18 }, { 0x01, 0x1B },
	{ 0x01, 0x1F }, { 0x01, 0x22 }, { 0x01, 0x26 }, { 0x01, 0x29 }, { 0x01, 0x2D },
	{ 0x01, 0x30 }, { 0x01, 0x34 }, { 0x01, 0x38 }, { 0x01, 0x3B }, { 0x01, 0x3F },
	{ 0x01, 0x42 }, { 0x01, 0x46 }, { 0x01, 0x49 }, { 0x01, 0x4D }, { 0x01, 0x51 },
	{ 0x01, 0x54 }, { 0x01, 0x58 }, { 0x01, 0x5C }, { 0x01, 0x5F }, { 0x01, 0x63 },
	{ 0x01, 0x67 }, { 0x01, 0x6A }, { 0x01, 0x6E }, { 0x01, 0x72 }, { 0x01, 0x75 },
	{ 0x01, 0x79 }, { 0x01, 0x7D }, { 0x01, 0x81 }, { 0x01, 0x84 }, { 0x01, 0x88 },
	{ 0x01, 0x8C }, { 0x01, 0x90 }, { 0x01, 0x93 }, { 0x01, 0x97 }, { 0x01, 0x9B },
	{ 0x01, 0x9F }, { 0x01, 0xA3 }, { 0x01, 0xA6 }, { 0x01, 0xAA }, { 0x01, 0xAE },
	{ 0x01, 0xB2 }, { 0x01, 0xB6 }, { 0x01, 0xBA }, { 0x01, 0xBD }, { 0x01, 0xC1 },
	{ 0x01, 0xC5 }, { 0x01, 0xC9 }, { 0x01, 0xCD }, { 0x01, 0xD1 }, { 0x01, 0xD5 },
	{ 0x01, 0xD9 }, { 0x01, 0xDD }, { 0x01, 0xE1 }, { 0x01, 0xE4 }, { 0x01, 0xE8 },
	{ 0x01, 0xEC }, { 0x01, 0xEC }, { 0x01, 0xF0 }, { 0x01, 0xF3 }, { 0x01, 0xF7 },
	{ 0x01, 0xFB }, { 0x01, 0xFF }, { 0x02, 0x03 }, { 0x02, 0x07 }, { 0x02, 0x0A },
	{ 0x02, 0x0E }, { 0x02, 0x12 }, { 0x02, 0x16 }, { 0x02, 0x1A }, { 0x02, 0x1E },
	{ 0x02, 0x22 }, { 0x02, 0x26 }, { 0x02, 0x29 }, { 0x02, 0x2D }, { 0x02, 0x31 },
	{ 0x02, 0x35 }, { 0x02, 0x39 }, { 0x02, 0x3D }, { 0x02, 0x41 }, { 0x02, 0x45 },
	{ 0x02, 0x49 }, { 0x02, 0x4D }, { 0x02, 0x51 }, { 0x02, 0x55 }, { 0x02, 0x59 },
	{ 0x02, 0x5D }, { 0x02, 0x61 }, { 0x02, 0x65 }, { 0x02, 0x69 }, { 0x02, 0x6D },
	{ 0x02, 0x71 }, { 0x02, 0x75 }, { 0x02, 0x79 }, { 0x02, 0x7D }, { 0x02, 0x81 },
	{ 0x02, 0x85 }, { 0x02, 0x89 }, { 0x02, 0x8D }, { 0x02, 0x91 }, { 0x02, 0x95 },
	{ 0x02, 0x9A }, { 0x02, 0x9E }, { 0x02, 0xA2 }, { 0x02, 0xA6 }, { 0x02, 0xAA },
	{ 0x02, 0xAE }, { 0x02, 0xB2 }, { 0x02, 0xB6 }, { 0x02, 0xBA }, { 0x02, 0xBF },
	{ 0x02, 0xC3 }, { 0x02, 0xC7 }, { 0x02, 0xC9 }, { 0x02, 0xD1 }, { 0x02, 0xD9 },
	{ 0x02, 0xE1 }, { 0x02, 0xEA }, { 0x02, 0xF2 }, { 0x02, 0xFA }, { 0x03, 0x02 },
	{ 0x03, 0x0A }, { 0x03, 0x12 }, { 0x03, 0x1B }, { 0x03, 0x23 }, { 0x03, 0x2B },
	{ 0x03, 0x33 }, { 0x03, 0x3B }, { 0x03, 0x43 }, { 0x03, 0x4B }, { 0x03, 0x54 },
	{ 0x03, 0x5C }, { 0x03, 0x64 }, { 0x03, 0x6C }, { 0x03, 0x74 }, { 0x03, 0x7C },
	{ 0x03, 0x85 }, { 0x03, 0x8D }, { 0x03, 0x95 }, { 0x03, 0x9D }, { 0x03, 0xA5 },
	{ 0x03, 0xAD }, { 0x03, 0xB6 }, { 0x03, 0xBE }, { 0x03, 0xC6 }, { 0x03, 0xCE },
	{ 0x03, 0xD6 }, { 0x03, 0xDE }, { 0x03, 0xE6 }, { 0x03, 0xEF }, { 0x03, 0xF7 },
	{ 0x03, 0xFF },
	/* HBM 5x51 */
	{ 0x00, 0x00 }, { 0x00, 0x04 }, { 0x00, 0x08 }, { 0x00, 0x0C }, { 0x00, 0x10 },
	{ 0x00, 0x14 }, { 0x00, 0x18 }, { 0x00, 0x1C }, { 0x00, 0x20 }, { 0x00, 0x24 },
	{ 0x00, 0x28 }, { 0x00, 0x2C }, { 0x00, 0x30 }, { 0x00, 0x34 }, { 0x00, 0x38 },
	{ 0x00, 0x3C }, { 0x00, 0x40 }, { 0x00, 0x44 }, { 0x00, 0x48 }, { 0x00, 0x4D },
	{ 0x00, 0x51 }, { 0x00, 0x55 }, { 0x00, 0x59 }, { 0x00, 0x5D }, { 0x00, 0x61 },
	{ 0x00, 0x65 }, { 0x00, 0x69 }, { 0x00, 0x6D }, { 0x00, 0x71 }, { 0x00, 0x75 },
	{ 0x00, 0x79 }, { 0x00, 0x7D }, { 0x00, 0x81 }, { 0x00, 0x85 }, { 0x00, 0x89 },
	{ 0x00, 0x8D }, { 0x00, 0x91 }, { 0x00, 0x95 }, { 0x00, 0x99 }, { 0x00, 0x9D },
	{ 0x00, 0xA1 }, { 0x00, 0xA5 }, { 0x00, 0xA9 }, { 0x00, 0xAD }, { 0x00, 0xB1 },
	{ 0x00, 0xB5 }, { 0x00, 0xB9 }, { 0x00, 0xBD }, { 0x00, 0xC1 }, { 0x00, 0xC5 },
	{ 0x00, 0xC9 }, { 0x00, 0xCD }, { 0x00, 0xD1 }, { 0x00, 0xD5 }, { 0x00, 0xD9 },
	{ 0x00, 0xDE }, { 0x00, 0xE2 }, { 0x00, 0xE6 }, { 0x00, 0xEA }, { 0x00, 0xEE },
	{ 0x00, 0xF2 }, { 0x00, 0xF6 }, { 0x00, 0xFA }, { 0x00, 0xFE }, { 0x01, 0x02 },
	{ 0x01, 0x06 }, { 0x01, 0x0A }, { 0x01, 0x0E }, { 0x01, 0x12 }, { 0x01, 0x16 },
	{ 0x01, 0x1A }, { 0x01, 0x1E }, { 0x01, 0x22 }, { 0x01, 0x26 }, { 0x01, 0x2A },
	{ 0x01, 0x2E }, { 0x01, 0x32 }, { 0x01, 0x36 }, { 0x01, 0x3A }, { 0x01, 0x3E },
	{ 0x01, 0x42 }, { 0x01, 0x46 }, { 0x01, 0x4A }, { 0x01, 0x4E }, { 0x01, 0x52 },
	{ 0x01, 0x56 }, { 0x01, 0x5A }, { 0x01, 0x5E }, { 0x01, 0x62 }, { 0x01, 0x66 },
	{ 0x01, 0x6A }, { 0x01, 0x6F }, { 0x01, 0x73 }, { 0x01, 0x77 }, { 0x01, 0x7B },
	{ 0x01, 0x7F }, { 0x01, 0x83 }, { 0x01, 0x87 }, { 0x01, 0x8B }, { 0x01, 0x8F },
	{ 0x01, 0x93 }, { 0x01, 0x97 }, { 0x01, 0x9B }, { 0x01, 0x9F }, { 0x01, 0xA3 },
	{ 0x01, 0xA7 }, { 0x01, 0xAB }, { 0x01, 0xAF }, { 0x01, 0xB3 }, { 0x01, 0xB7 },
	{ 0x01, 0xBB }, { 0x01, 0xBF }, { 0x01, 0xC3 }, { 0x01, 0xC7 }, { 0x01, 0xCB },
	{ 0x01, 0xCF }, { 0x01, 0xD3 }, { 0x01, 0xD7 }, { 0x01, 0xDB }, { 0x01, 0xDF },
	{ 0x01, 0xE3 }, { 0x01, 0xE7 }, { 0x01, 0xEB }, { 0x01, 0xEF }, { 0x01, 0xF3 },
	{ 0x01, 0xF7 }, { 0x01, 0xFB }, { 0x01, 0xFF }, { 0x02, 0x04 }, { 0x02, 0x08 },
	{ 0x02, 0x0C }, { 0x02, 0x10 }, { 0x02, 0x14 }, { 0x02, 0x18 }, { 0x02, 0x1C },
	{ 0x02, 0x20 }, { 0x02, 0x24 }, { 0x02, 0x28 }, { 0x02, 0x2C }, { 0x02, 0x30 },
	{ 0x02, 0x34 }, { 0x02, 0x38 }, { 0x02, 0x3C }, { 0x02, 0x40 }, { 0x02, 0x44 },
	{ 0x02, 0x48 }, { 0x02, 0x4C }, { 0x02, 0x50 }, { 0x02, 0x54 }, { 0x02, 0x58 },
	{ 0x02, 0x5C }, { 0x02, 0x60 }, { 0x02, 0x64 }, { 0x02, 0x68 }, { 0x02, 0x6C },
	{ 0x02, 0x70 }, { 0x02, 0x74 }, { 0x02, 0x78 }, { 0x02, 0x7C }, { 0x02, 0x80 },
	{ 0x02, 0x84 }, { 0x02, 0x88 }, { 0x02, 0x8C }, { 0x02, 0x90 }, { 0x02, 0x95 },
	{ 0x02, 0x99 }, { 0x02, 0x9D }, { 0x02, 0xA1 }, { 0x02, 0xA5 }, { 0x02, 0xA9 },
	{ 0x02, 0xAD }, { 0x02, 0xB1 }, { 0x02, 0xB5 }, { 0x02, 0xB9 }, { 0x02, 0xBD },
	{ 0x02, 0xC1 }, { 0x02, 0xC5 }, { 0x02, 0xC9 }, { 0x02, 0xCD }, { 0x02, 0xD1 },
	{ 0x02, 0xD5 }, { 0x02, 0xD9 }, { 0x02, 0xDD }, { 0x02, 0xE1 }, { 0x02, 0xE5 },
	{ 0x02, 0xE9 }, { 0x02, 0xED }, { 0x02, 0xF1 }, { 0x02, 0xF5 }, { 0x02, 0xF9 },
	{ 0x02, 0xFD }, { 0x03, 0x01 }, { 0x03, 0x05 }, { 0x03, 0x09 }, { 0x03, 0x0D },
	{ 0x03, 0x11 }, { 0x03, 0x15 }, { 0x03, 0x19 }, { 0x03, 0x1D }, { 0x03, 0x21 },
	{ 0x03, 0x26 }, { 0x03, 0x2A }, { 0x03, 0x2E }, { 0x03, 0x32 }, { 0x03, 0x36 },
	{ 0x03, 0x3A }, { 0x03, 0x3E }, { 0x03, 0x42 }, { 0x03, 0x46 }, { 0x03, 0x4A },
	{ 0x03, 0x4E }, { 0x03, 0x52 }, { 0x03, 0x56 }, { 0x03, 0x5A }, { 0x03, 0x5E },
	{ 0x03, 0x62 }, { 0x03, 0x66 }, { 0x03, 0x6A }, { 0x03, 0x6E }, { 0x03, 0x72 },
	{ 0x03, 0x76 }, { 0x03, 0x7A }, { 0x03, 0x7E }, { 0x03, 0x82 }, { 0x03, 0x86 },
	{ 0x03, 0x8A }, { 0x03, 0x8E }, { 0x03, 0x92 }, { 0x03, 0x96 }, { 0x03, 0x9A },
	{ 0x03, 0x9E }, { 0x03, 0xA2 }, { 0x03, 0xA6 }, { 0x03, 0xAA }, { 0x03, 0xAE },
	{ 0x03, 0xB2 }, { 0x03, 0xB7 }, { 0x03, 0xBB }, { 0x03, 0xBF }, { 0x03, 0xC3 },
	{ 0x03, 0xC7 }, { 0x03, 0xCB }, { 0x03, 0xCF }, { 0x03, 0xD3 }, { 0x03, 0xD7 },
	{ 0x03, 0xDB }, { 0x03, 0xDF }, { 0x03, 0xE3 }, { 0x03, 0xE7 }, { 0x03, 0xEB },
	{ 0x03, 0xEF }, { 0x03, 0xF3 }, { 0x03, 0xF7 }, { 0x03, 0xFB }, { 0x03, 0xFF },
};

static u8 canvas2_a3_s0_elvss_table[S6E3HAC_TOTAL_STEP][1] = {
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
	/* HBM 8x31+7 */
	{ 0x2D }, { 0x2D }, { 0x2D }, { 0x2D }, { 0x2D }, { 0x2D }, { 0x2D }, { 0x2D },
	{ 0x2D }, { 0x2D }, { 0x2D }, { 0x2C }, { 0x2C }, { 0x2C }, { 0x2C }, { 0x2C },
	{ 0x2C }, { 0x2C }, { 0x2C }, { 0x2C }, { 0x2C }, { 0x2B }, { 0x2B }, { 0x2B },
	{ 0x2B }, { 0x2B }, { 0x2B }, { 0x2B }, { 0x2B }, { 0x2B }, { 0x2B }, { 0x2B },
	{ 0x2A }, { 0x2A }, { 0x2A }, { 0x2A }, { 0x2A }, { 0x2A }, { 0x2A }, { 0x2A },
	{ 0x2A }, { 0x2A }, { 0x2A }, { 0x29 }, { 0x29 }, { 0x29 }, { 0x29 }, { 0x29 },
	{ 0x29 }, { 0x29 }, { 0x29 }, { 0x29 }, { 0x29 }, { 0x28 }, { 0x28 }, { 0x28 },
	{ 0x28 }, { 0x28 }, { 0x28 }, { 0x28 }, { 0x28 }, { 0x28 }, { 0x28 }, { 0x28 },
	{ 0x27 }, { 0x27 }, { 0x27 }, { 0x27 }, { 0x27 }, { 0x27 }, { 0x27 }, { 0x27 },
	{ 0x27 }, { 0x27 }, { 0x27 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x26 },
	{ 0x26 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x26 }, { 0x25 }, { 0x25 }, { 0x25 },
	{ 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 }, { 0x25 },
	{ 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 }, { 0x24 },
	{ 0x24 }, { 0x24 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x23 },
	{ 0x23 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x23 }, { 0x22 }, { 0x22 }, { 0x22 },
	{ 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 }, { 0x22 },
	{ 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 }, { 0x21 },
	{ 0x21 }, { 0x21 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 },
	{ 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x20 }, { 0x1F }, { 0x1F }, { 0x1F },
	{ 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1F }, { 0x1E },
	{ 0x1E }, { 0x1E }, { 0x1E }, { 0x1E }, { 0x1E }, { 0x1E }, { 0x1E }, { 0x1E },
	{ 0x1E }, { 0x1E }, { 0x1D }, { 0x1D }, { 0x1D }, { 0x1D }, { 0x1D }, { 0x1D },
	{ 0x1D }, { 0x1D }, { 0x1D }, { 0x1D }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C },
	{ 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1C }, { 0x1B },
	{ 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1B }, { 0x1B },
	{ 0x1B }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x1A },
	{ 0x1A }, { 0x1A }, { 0x1A }, { 0x1A }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 },
	{ 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x19 }, { 0x18 },
	{ 0x18 }, { 0x18 }, { 0x18 }, { 0x18 }, { 0x18 }, { 0x18 }, { 0x18 }, { 0x18 },
	{ 0x18 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x17 },
	{ 0x17 }, { 0x17 }, { 0x17 }, { 0x17 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
	{ 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 }, { 0x16 },
};

static u8 canvas2_a3_s0_hbm_transition_table[MAX_PANEL_HBM][SMOOTH_TRANS_MAX][1] = {
	/* HBM off */
	{
		/* Normal */
		{ 0x21 },
		/* Smooth */
		{ 0x29 },
	},
	/* HBM on */
	{
		/* Normal */
		{ 0xE1 },
		/* Smooth */
		{ 0xE1 },
	}
};

static u8 canvas2_a3_s0_acl_opr_table[ACL_OPR_MAX][1] = {
	{ 0x00 }, /* ACL OFF OPR */
	{ 0x01 }, /* ACL ON OPR_3 */
	{ 0x01 }, /* ACL ON OPR_6 */
	{ 0x01 }, /* ACL ON OPR_8 */
	{ 0x02 }, /* ACL ON OPR_12 */
	{ 0x02 }, /* ACL ON OPR_15 */
};


static u8 canvas2_a3_s0_vbias1_table[MAX_S6E3HAC_VRR_MODE][3][14];
static u8 canvas2_a3_s0_vbias2_table[MAX_S6E3HAC_VRR_MODE][1];
#if 0
static u8 canvas2_a3_s0_vbias1_table[MAX_S6E3HAC_VRR_MODE][3][21] = {
	[S6E3HAC_VRR_MODE_NS] = {
		/* over zero */
		{
			0x3C, 0x3C, 0x3C, 0x3C, 0x36, 0x2C, 0x2C, 0x38,
			0x38, 0x38, 0x38, 0x38, 0x44, 0x88, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		/* under zero */
		{
			0x3C, 0x3C, 0x3C, 0x3C, 0x36, 0x2C, 0x2C, 0x50,
			0x50, 0x50, 0x50, 0x50, 0x5C, 0xA0, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		/* under minus fifteen */
		{
			0x3C, 0x3C, 0x3C, 0x3C, 0x36, 0x2C, 0x2C, 0x64,
			0x64, 0x64, 0x64, 0x64, 0x70, 0xB4, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
	},
	[S6E3HAC_VRR_MODE_HS] = {
		/* over zero */
		{
			0x3B, 0x2D, 0x2B, 0x23, 0x25, 0x27, 0x27, 0x38,
			0x38, 0x38, 0x38, 0x38, 0x44, 0x88, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		/* under zero */
		{
			0x3B, 0x2D, 0x2B, 0x23, 0x25, 0x27, 0x27, 0x50,
			0x50, 0x50, 0x50, 0x50, 0x5C, 0xA0, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
		/* under minus fifteen */
		{
			0x3B, 0x2D, 0x2B, 0x23, 0x25, 0x27, 0x27, 0x64,
			0x64, 0x64, 0x64, 0x64, 0x70, 0xB4, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		},
	},
};
#endif

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 canvas2_a3_s0_vgh_table[][1] = {
	{ 0x31 },
	{ 0x28 },
};
#endif
static u8 canvas2_a3_s0_dsc_table[][1] = {
	{ 0x00 },
	{ 0x01 },
};

static u8 canvas2_a3_s0_pps_table[][MAX_S6E3HAC_SCALER][128] = {
	{
		{
			// PPS For DSU MODE 1 : 1440x3088 (Original) Slice Info : 720x193
			0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x10,
			0x05, 0xA0, 0x00, 0xC1, 0x02, 0xD0, 0x02, 0xD0,
			0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x15, 0x8B,
			0x00, 0x0A, 0x00, 0x0C, 0x00, 0x80, 0x00, 0x66,
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
		},
		{
			// PPS For DSU MODE 2 : 720x2316 (Original) Slice Info : 540x193
			0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x0C,
			0x04, 0x38, 0x00, 0xC1, 0x02, 0x1C, 0x02, 0x1C,
			0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x12, 0xD7,
			0x00, 0x07, 0x00, 0x0C, 0x00, 0x80, 0x00, 0x87,
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
		},
		{
			// PPS For DSU MODE 3 : 720x1544 Slice Info : 360x193
			0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x06, 0x08,
			0x02, 0xD0, 0x00, 0xC1, 0x01, 0x68, 0x01, 0x68,
			0x02, 0x00, 0x01, 0xB4, 0x00, 0x20, 0x0E, 0xF8,
			0x00, 0x05, 0x00, 0x0C, 0x00, 0x80, 0x00, 0xCB,
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
		},
	}
};

static u8 canvas2_a3_s0_scaler_table[][MAX_S6E3HAC_SCALER][1] = {
	{
		[S6E3HAC_SCALER_OFF] = { 0x01 },
		[S6E3HAC_SCALER_x1_78] = { 0x02 },
		[S6E3HAC_SCALER_x4] = { 0x00 },

	}
};

static u8 canvas2_a3_s0_caset_table[][MAX_S6E3HAC_SCALER][4] = {
	{
		[S6E3HAC_SCALER_OFF] = { 0x00, 0x00, 0x05, 0x9F },
		[S6E3HAC_SCALER_x1_78] = { 0x00, 0x00, 0x04, 0x37 },
		[S6E3HAC_SCALER_x4] = { 0x00, 0x00, 0x02, 0xCF },
	}
};

static u8 canvas2_a3_s0_paset_table[][MAX_S6E3HAC_SCALER][4] = {
	{
		[S6E3HAC_SCALER_OFF] = { 0x00, 0x00, 0x0C, 0x0F },
		[S6E3HAC_SCALER_x1_78] = { 0x00, 0x00, 0x09, 0x0B },
		[S6E3HAC_SCALER_x4] = { 0x00, 0x00, 0x06, 0x07 },
	}
};

static u8 canvas2_a3_s0_lfd_mode_table[MAX_VRR_MODE][1] = {
	[VRR_NORMAL_MODE] = { 0x00 },
	[VRR_HS_MODE] = { 0x01 },
};

static u8 canvas2_a3_s0_lfd_frame_insertion_table[MAX_S6E3HAC_VRR_LFD_FRAME_IDX][MAX_S6E3HAC_VRR_LFD_FRAME_IDX][3] = {
	[S6E3HAC_VRR_LFD_FRAME_IDX_120_HS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_120_HS ... S6E3HAC_VRR_LFD_FRAME_IDX_60_HS] = {0x10, 0x00, 0x01},
		[S6E3HAC_VRR_LFD_FRAME_IDX_48_HS ... S6E3HAC_VRR_LFD_FRAME_IDX_11_HS] = {0x14, 0x00, 0x01},
		[S6E3HAC_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_96_HS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_96_HS ... S6E3HAC_VRR_LFD_FRAME_IDX_60_HS] = {0x10, 0x00, 0x01},
		[S6E3HAC_VRR_LFD_FRAME_IDX_48_HS ... S6E3HAC_VRR_LFD_FRAME_IDX_11_HS] = {0x14, 0x00, 0x01},
		[S6E3HAC_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_60_HS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_60_HS] = {0x10, 0x00, 0x01},
		[S6E3HAC_VRR_LFD_FRAME_IDX_48_HS ... S6E3HAC_VRR_LFD_FRAME_IDX_11_HS] = {0x00, 0x00, 0x02},
		[S6E3HAC_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_48_HS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_48_HS ... S6E3HAC_VRR_LFD_FRAME_IDX_11_HS] = {0x00, 0x00, 0x02},
		[S6E3HAC_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_32_HS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_32_HS ... S6E3HAC_VRR_LFD_FRAME_IDX_11_HS] = {0x00, 0x00, 0x02},
		[S6E3HAC_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_30_HS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_30_HS ... S6E3HAC_VRR_LFD_FRAME_IDX_11_HS] = {0x00, 0x00, 0x02},
		[S6E3HAC_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_24_HS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_24_HS ... S6E3HAC_VRR_LFD_FRAME_IDX_11_HS] = {0x00, 0x00, 0x02},
		[S6E3HAC_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_11_HS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_11_HS] = {0x00, 0x00, 0x02},
		[S6E3HAC_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_1_HS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_1_HS] = {0xEF, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_60_NS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_60_NS ... S6E3HAC_VRR_LFD_FRAME_IDX_30_NS] = {0x00, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_48_NS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_48_NS ... S6E3HAC_VRR_LFD_FRAME_IDX_30_NS] = {0x00, 0x00, 0x01},
	},
	[S6E3HAC_VRR_LFD_FRAME_IDX_30_NS] = {
		[S6E3HAC_VRR_LFD_FRAME_IDX_30_NS] = {0x00, 0x00, 0x01},
	},
};

/* only for use rev40~rev51 panel */
static u8 canvas2_a3_s0_rev40_osc_table[][17] = {
	[S6E3HAC_VRR_MODE_NS] = {
		0xC3, 0xB4, 0x04, 0x14, 0x01, 0x1C, 0x00, 0x14,
		0x10, 0x00, 0x14, 0xBC, 0x1C, 0x00, 0x34, 0x0F,
		0xE0
	},
	[S6E3HAC_VRR_MODE_HS] = {
		0xC1, 0xB4, 0x04, 0x00, 0x01, 0x1C, 0x00, 0x14,
		0x10, 0x00, 0x14, 0xBC, 0x1C, 0x00, 0x14, 0x10,
		0x00
	},
};
/* rev52~ latest panel */
static u8 canvas2_a3_s0_osc_table[][17] = {
	[S6E3HAC_VRR_MODE_NS] = {
		0xC3, 0xB4, 0x04, 0x14, 0x01, 0x20, 0x00, 0x10,
		0x10, 0x00, 0x14, 0xBC, 0x20, 0x00, 0x30, 0x0F,
		0xE0
	},
	[S6E3HAC_VRR_MODE_HS] = {
		0xC1, 0xB4, 0x04, 0x00, 0x01, 0x20, 0x00, 0x10,
		0x10, 0x00, 0x14, 0xBC, 0x20, 0x00, 0x10, 0x10,
		0x00
	},
};

static u8 canvas2_a3_s0_osc_sel_table[][1] = {
	[S6E3HAC_VRR_MODE_NS] = { 0x00 },
	[S6E3HAC_VRR_MODE_HS] = { 0x21 },
};

static u8 canvas2_a3_s0_fps_table[MAX_S6E3HAC_VRR][2] = {
	/* NS */
	[S6E3HAC_VRR_60NS] = { 0x00, 0x00 },
	[S6E3HAC_VRR_48NS] = { 0x00, 0x00 },
	/* HS */
	[S6E3HAC_VRR_120HS] = { 0x08, 0x00 },
	[S6E3HAC_VRR_96HS] = { 0x08, 0x00 },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { 0x08, 0x00 },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x08, 0x00 },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { 0x08, 0x00 },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x08, 0x00 },
};

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static u8 canvas2_a3_s0_vddm_table[][1] = {
	{0x00}, // VDDM ORIGINAL
	{0x04}, // VDDM LV
	{0x08}, // VDDM HV
};
static u8 canvas2_a3_s0_gram_img_pattern_table[][1] = {
	{0x00}, // GCT_PATTERN_NONE
	{0x07}, // GCT_PATTERN_1
	{0x05}, // GCT_PATTERN_2
};
/* switch pattern_1 and pattern_2 */
static u8 canvas2_a3_s0_gram_inv_img_pattern_table[][1] = {
	{0x00}, // GCT_PATTERN_NONE
	{0x05}, // GCT_PATTERN_2
	{0x07}, // GCT_PATTERN_1
};
#endif

#ifdef CONFIG_DYNAMIC_FREQ
static u8 canvas2_a3_s0_dyn_ffc_table[MAX_S6E3HAC_OSC][S6E3HAC_MAX_MIPI_FREQ][4] = {
	/* frequencies are sync with DT ddi dynamic_freq property */
	{
		/* FFC for Default OSC (96.5) */
		{0x33, 0xBA, 0x33, 0xBA}, /* 1194 */
		{0x35, 0xAE, 0x35, 0xAE}, /* 1150.5 */
		{0x35, 0x61, 0x35, 0x61}, /* 1157 */
		{0x34, 0xC9, 0x34, 0xC9}, /* 1170 */
	},
};
#endif

static u8 canvas2_a3_s0_lpm_nit_table[4][2] = {
	/* LPM 2NIT */
	{ 0x0B, 0xCA },
	/* LPM 10NIT */
	{ 0x0A, 0x40 },
	/* LPM 30NIT */
	{ 0x06, 0x20 },
	/* LPM 60NIT */
	{ 0x00, 0xA0 },
};

/* support for 40 panel */
static u8 canvas2_a3_s0_rev40_lpm_nit_table[4][2] = {
	/* LPM 2NIT */
	{ 0x0B, 0xF0 },
	/* LPM 10NIT */
	{ 0x0B, 0x02 },
	/* LPM 30NIT */
	{ 0x08, 0x90 },
	/* LPM 60NIT */
	{ 0x04, 0xF0 },
};
/* support for 40 panel end */

static u8 canvas2_a3_s0_lpm_mode_table[2][1] = {
	[ALPM_MODE] = { 0x0B },
	[HLPM_MODE] = { 0x09 },
};

static u8 canvas2_a3_s0_lpm_off_table[2][1] = {
	[ALPM_MODE] = { 0x03 },
	[HLPM_MODE] = { 0x01 },
};

static u8 canvas2_a3_s0_lpm_fps_table[2][7] = {
	[LPM_LFD_1HZ] = { 0x00, 0x1D, 0x00, 0x0B, 0x00, 0x00, 0x08 },
	[LPM_LFD_30HZ] = { 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x08 },
};

static u8 canvas2_a3_s0_dia_onoff_table[][1] = {
	{ 0x00 }, /* dia off */
	{ 0x09 }, /* dia on */
};

static u8 canvas2_a3_s0_irc_mode_table[][1] = {
	{ 0x65 }, /* irc moderato */
	{ 0x25 }, /* irc flat */
};

/* for rev40~51 (less than or equal rev51) */
#define CANVAS2_VFP_60NS_LE_REV51_1ST	(0x00)
#define CANVAS2_VFP_60NS_LE_REV51_2ND	(0x34)
#define CANVAS2_VFP_48NS_LE_REV51_1ST	(0x03)
#define CANVAS2_VFP_48NS_LE_REV51_2ND	(0x14)

#define CANVAS2_VTT_60NS_LE_REV51_1ST	(0x0C)
#define CANVAS2_VTT_60NS_LE_REV51_2ND	(0x30)
#define CANVAS2_VTT_48NS_LE_REV51_1ST	(0x0F)
#define CANVAS2_VTT_48NS_LE_REV51_2ND	(0x40)

#define CANVAS2_VFP_OPT_60NS_LE_REV51_1ST	(0x0B)
#define CANVAS2_VFP_OPT_60NS_LE_REV51_2ND	(0xF2)
#define CANVAS2_VFP_OPT_48NS_LE_REV51_1ST	(0x0E)
#define CANVAS2_VFP_OPT_48NS_LE_REV51_2ND	(0xE0)

#define CANVAS2_AOR_60NS_LE_REV51_1ST	(0x01)
#define CANVAS2_AOR_60NS_LE_REV51_2ND	(0xF0)
#define CANVAS2_AOR_48NS_LE_REV51_1ST	(0x02)
#define CANVAS2_AOR_48NS_LE_REV51_2ND	(0x70)

#define CANVAS2_VFP_120HS_LE_REV51_1ST	(0x00)
#define CANVAS2_VFP_120HS_LE_REV51_2ND	(0x14)
#define CANVAS2_VFP_96HS_LE_REV51_1ST	(0x03)
#define CANVAS2_VFP_96HS_LE_REV51_2ND	(0x14)

#define CANVAS2_VTT_120HS_LE_REV51_1ST	(0x0C)
#define CANVAS2_VTT_120HS_LE_REV51_2ND	(0x30)
#define CANVAS2_VTT_96HS_LE_REV51_1ST	(0x0F)
#define CANVAS2_VTT_96HS_LE_REV51_2ND	(0x40)

#define CANVAS2_VFP_OPT_120HS_LE_REV51_1ST	(0x0B)
#define CANVAS2_VFP_OPT_120HS_LE_REV51_2ND	(0xF2)
#define CANVAS2_VFP_OPT_96HS_LE_REV51_1ST	(0x0E)
#define CANVAS2_VFP_OPT_96HS_LE_REV51_2ND	(0xE0)

#define CANVAS2_AOR_120HS_LE_REV51_1ST	(0x01)
#define CANVAS2_AOR_120HS_LE_REV51_2ND	(0xF0)
#define CANVAS2_AOR_96HS_LE_REV51_1ST	(0x02)
#define CANVAS2_AOR_96HS_LE_REV51_2ND	(0x70)

static u8 canvas2_a3_s0_vfp_ns_le_rev51_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VFP_60NS_LE_REV51_1ST, CANVAS2_VFP_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VFP_48NS_LE_REV51_1ST, CANVAS2_VFP_48NS_LE_REV51_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VFP_120HS_LE_REV51_1ST, CANVAS2_VFP_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VFP_96HS_LE_REV51_1ST, CANVAS2_VFP_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VFP_120HS_LE_REV51_1ST, CANVAS2_VFP_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VFP_120HS_LE_REV51_1ST, CANVAS2_VFP_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VFP_96HS_LE_REV51_1ST, CANVAS2_VFP_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VFP_96HS_LE_REV51_1ST, CANVAS2_VFP_96HS_LE_REV51_2ND },
};

static u8 canvas2_a3_s0_vfp_hs_le_rev51_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VFP_120HS_LE_REV51_1ST, CANVAS2_VFP_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VFP_96HS_LE_REV51_1ST, CANVAS2_VFP_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VFP_120HS_LE_REV51_1ST, CANVAS2_VFP_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VFP_96HS_LE_REV51_1ST, CANVAS2_VFP_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VFP_120HS_LE_REV51_1ST, CANVAS2_VFP_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VFP_120HS_LE_REV51_1ST, CANVAS2_VFP_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VFP_96HS_LE_REV51_1ST, CANVAS2_VFP_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VFP_96HS_LE_REV51_1ST, CANVAS2_VFP_96HS_LE_REV51_2ND },
};

static u8 canvas2_a3_s0_vtotal_ns_le_rev51_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VTT_60NS_LE_REV51_1ST, CANVAS2_VTT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VTT_48NS_LE_REV51_1ST, CANVAS2_VTT_48NS_LE_REV51_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VTT_60NS_LE_REV51_1ST, CANVAS2_VTT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VTT_60NS_LE_REV51_1ST, CANVAS2_VTT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VTT_60NS_LE_REV51_1ST, CANVAS2_VTT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VTT_60NS_LE_REV51_1ST, CANVAS2_VTT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VTT_60NS_LE_REV51_1ST, CANVAS2_VTT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VTT_60NS_LE_REV51_1ST, CANVAS2_VTT_60NS_LE_REV51_2ND },
};

static u8 canvas2_a3_s0_vtotal_hs_le_rev51_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VTT_120HS_LE_REV51_1ST, CANVAS2_VTT_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VTT_120HS_LE_REV51_1ST, CANVAS2_VTT_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VTT_120HS_LE_REV51_1ST, CANVAS2_VTT_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VTT_96HS_LE_REV51_1ST, CANVAS2_VTT_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VTT_120HS_LE_REV51_1ST, CANVAS2_VTT_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VTT_120HS_LE_REV51_1ST, CANVAS2_VTT_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VTT_96HS_LE_REV51_1ST, CANVAS2_VTT_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VTT_96HS_LE_REV51_1ST, CANVAS2_VTT_96HS_LE_REV51_2ND },
};

static u8 canvas2_a3_s0_vfp_opt_ns_le_rev51_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VFP_OPT_60NS_LE_REV51_1ST, CANVAS2_VFP_OPT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VFP_OPT_48NS_LE_REV51_1ST, CANVAS2_VFP_OPT_48NS_LE_REV51_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VFP_OPT_60NS_LE_REV51_1ST, CANVAS2_VFP_OPT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VFP_OPT_60NS_LE_REV51_1ST, CANVAS2_VFP_OPT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VFP_OPT_60NS_LE_REV51_1ST, CANVAS2_VFP_OPT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VFP_OPT_60NS_LE_REV51_1ST, CANVAS2_VFP_OPT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VFP_OPT_60NS_LE_REV51_1ST, CANVAS2_VFP_OPT_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VFP_OPT_60NS_LE_REV51_1ST, CANVAS2_VFP_OPT_60NS_LE_REV51_2ND },
};

static u8 canvas2_a3_s0_vfp_opt_hs_le_rev51_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VFP_OPT_120HS_LE_REV51_1ST, CANVAS2_VFP_OPT_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VFP_OPT_120HS_LE_REV51_1ST, CANVAS2_VFP_OPT_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VFP_OPT_120HS_LE_REV51_1ST, CANVAS2_VFP_OPT_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VFP_OPT_96HS_LE_REV51_1ST, CANVAS2_VFP_OPT_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VFP_OPT_120HS_LE_REV51_1ST, CANVAS2_VFP_OPT_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VFP_OPT_120HS_LE_REV51_1ST, CANVAS2_VFP_OPT_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VFP_OPT_96HS_LE_REV51_1ST, CANVAS2_VFP_OPT_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VFP_OPT_96HS_LE_REV51_1ST, CANVAS2_VFP_OPT_96HS_LE_REV51_2ND },
};

static u8 canvas2_a3_s0_aor_ns_le_rev51_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_AOR_60NS_LE_REV51_1ST, CANVAS2_AOR_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_AOR_48NS_LE_REV51_1ST, CANVAS2_AOR_48NS_LE_REV51_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_AOR_60NS_LE_REV51_1ST, CANVAS2_AOR_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_AOR_60NS_LE_REV51_1ST, CANVAS2_AOR_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_AOR_60NS_LE_REV51_1ST, CANVAS2_AOR_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_AOR_60NS_LE_REV51_1ST, CANVAS2_AOR_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_AOR_60NS_LE_REV51_1ST, CANVAS2_AOR_60NS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_AOR_60NS_LE_REV51_1ST, CANVAS2_AOR_60NS_LE_REV51_2ND },
};

static u8 canvas2_a3_s0_aor_hs_le_rev51_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_AOR_120HS_LE_REV51_1ST, CANVAS2_AOR_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_AOR_120HS_LE_REV51_1ST, CANVAS2_AOR_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_AOR_120HS_LE_REV51_1ST, CANVAS2_AOR_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_AOR_96HS_LE_REV51_1ST, CANVAS2_AOR_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_AOR_120HS_LE_REV51_1ST, CANVAS2_AOR_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_AOR_120HS_LE_REV51_1ST, CANVAS2_AOR_120HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_AOR_96HS_LE_REV51_1ST, CANVAS2_AOR_96HS_LE_REV51_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_AOR_96HS_LE_REV51_1ST, CANVAS2_AOR_96HS_LE_REV51_2ND },
};

/* for rev52~ */
#define CANVAS2_VFP_60NS_1ST	(0x00)
#define CANVAS2_VFP_60NS_2ND	(0x30)
#define CANVAS2_VFP_48NS_1ST	(0x03)
#define CANVAS2_VFP_48NS_2ND	(0x20)

#define CANVAS2_VTT_60NS_1ST	(0x0C)
#define CANVAS2_VTT_60NS_2ND	(0x60)
#define CANVAS2_VTT_48NS_1ST	(0x0F)
#define CANVAS2_VTT_48NS_2ND	(0x50)

#define CANVAS2_VFP_OPT_60NS_1ST	(0x0C)
#define CANVAS2_VFP_OPT_60NS_2ND	(0x22)
#define CANVAS2_VFP_OPT_48NS_1ST	(0x0F)
#define CANVAS2_VFP_OPT_48NS_2ND	(0x04)

#define CANVAS2_AOR_60NS_1ST	(0x02)
#define CANVAS2_AOR_60NS_2ND	(0x00)
#define CANVAS2_AOR_48NS_1ST	(0x02)
#define CANVAS2_AOR_48NS_2ND	(0x50)

#define CANVAS2_VFP_120HS_1ST	(0x00)
#define CANVAS2_VFP_120HS_2ND	(0x10)
#define CANVAS2_VFP_96HS_1ST	(0x03)
#define CANVAS2_VFP_96HS_2ND	(0x20)

#define CANVAS2_VTT_120HS_1ST	(0x0C)
#define CANVAS2_VTT_120HS_2ND	(0x40)
#define CANVAS2_VTT_96HS_1ST	(0x0F)
#define CANVAS2_VTT_96HS_2ND	(0x50)

#define CANVAS2_VFP_OPT_120HS_1ST	(0x0C)
#define CANVAS2_VFP_OPT_120HS_2ND	(0x02)
#define CANVAS2_VFP_OPT_96HS_1ST	(0x0F)
#define CANVAS2_VFP_OPT_96HS_2ND	(0x04)

#define CANVAS2_AOR_120HS_1ST	(0x01)
#define CANVAS2_AOR_120HS_2ND	(0xF8)
#define CANVAS2_AOR_96HS_1ST	(0x02)
#define CANVAS2_AOR_96HS_2ND	(0x50)

static u8 canvas2_a3_s0_vfp_ns_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VFP_60NS_1ST, CANVAS2_VFP_60NS_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VFP_48NS_1ST, CANVAS2_VFP_48NS_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VFP_120HS_1ST, CANVAS2_VFP_120HS_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VFP_96HS_1ST, CANVAS2_VFP_96HS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VFP_120HS_1ST, CANVAS2_VFP_120HS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VFP_120HS_1ST, CANVAS2_VFP_120HS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VFP_96HS_1ST, CANVAS2_VFP_96HS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VFP_96HS_1ST, CANVAS2_VFP_96HS_2ND },
};

static u8 canvas2_a3_s0_vfp_hs_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VFP_120HS_1ST, CANVAS2_VFP_120HS_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VFP_120HS_1ST, CANVAS2_VFP_120HS_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VFP_120HS_1ST, CANVAS2_VFP_120HS_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VFP_96HS_1ST, CANVAS2_VFP_96HS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VFP_120HS_1ST, CANVAS2_VFP_120HS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VFP_120HS_1ST, CANVAS2_VFP_120HS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VFP_96HS_1ST, CANVAS2_VFP_96HS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VFP_96HS_1ST, CANVAS2_VFP_96HS_2ND },
};

static u8 canvas2_a3_s0_vtotal_ns_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VTT_60NS_1ST, CANVAS2_VTT_60NS_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VTT_48NS_1ST, CANVAS2_VTT_48NS_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VTT_60NS_1ST, CANVAS2_VTT_60NS_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VTT_60NS_1ST, CANVAS2_VTT_60NS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VTT_60NS_1ST, CANVAS2_VTT_60NS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VTT_60NS_1ST, CANVAS2_VTT_60NS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VTT_60NS_1ST, CANVAS2_VTT_60NS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VTT_60NS_1ST, CANVAS2_VTT_60NS_2ND },
};

static u8 canvas2_a3_s0_vtotal_hs_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VTT_120HS_1ST, CANVAS2_VTT_120HS_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VTT_120HS_1ST, CANVAS2_VTT_120HS_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VTT_120HS_1ST, CANVAS2_VTT_120HS_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VTT_96HS_1ST, CANVAS2_VTT_96HS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VTT_120HS_1ST, CANVAS2_VTT_120HS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VTT_120HS_1ST, CANVAS2_VTT_120HS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VTT_96HS_1ST, CANVAS2_VTT_96HS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VTT_96HS_1ST, CANVAS2_VTT_96HS_2ND },
};

static u8 canvas2_a3_s0_vfp_opt_ns_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VFP_OPT_60NS_1ST, CANVAS2_VFP_OPT_60NS_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VFP_OPT_48NS_1ST, CANVAS2_VFP_OPT_48NS_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VFP_OPT_60NS_1ST, CANVAS2_VFP_OPT_60NS_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VFP_OPT_60NS_1ST, CANVAS2_VFP_OPT_60NS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VFP_OPT_60NS_1ST, CANVAS2_VFP_OPT_60NS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VFP_OPT_60NS_1ST, CANVAS2_VFP_OPT_60NS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VFP_OPT_60NS_1ST, CANVAS2_VFP_OPT_60NS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VFP_OPT_60NS_1ST, CANVAS2_VFP_OPT_60NS_2ND },
};

static u8 canvas2_a3_s0_vfp_opt_hs_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_VFP_OPT_120HS_1ST, CANVAS2_VFP_OPT_120HS_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_VFP_OPT_120HS_1ST, CANVAS2_VFP_OPT_120HS_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_VFP_OPT_120HS_1ST, CANVAS2_VFP_OPT_120HS_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_VFP_OPT_96HS_1ST, CANVAS2_VFP_OPT_96HS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_VFP_OPT_120HS_1ST, CANVAS2_VFP_OPT_120HS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_VFP_OPT_120HS_1ST, CANVAS2_VFP_OPT_120HS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_VFP_OPT_96HS_1ST, CANVAS2_VFP_OPT_96HS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_VFP_OPT_96HS_1ST, CANVAS2_VFP_OPT_96HS_2ND },
};

static u8 canvas2_a3_s0_aor_ns_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_AOR_60NS_1ST, CANVAS2_AOR_60NS_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_AOR_48NS_1ST, CANVAS2_AOR_48NS_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_AOR_60NS_1ST, CANVAS2_AOR_60NS_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_AOR_60NS_1ST, CANVAS2_AOR_60NS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_AOR_60NS_1ST, CANVAS2_AOR_60NS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_AOR_60NS_1ST, CANVAS2_AOR_60NS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_AOR_60NS_1ST, CANVAS2_AOR_60NS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_AOR_60NS_1ST, CANVAS2_AOR_60NS_2ND },
};

static u8 canvas2_a3_s0_aor_hs_table[MAX_S6E3HAC_VRR][2] = {
	[S6E3HAC_VRR_60NS] = { CANVAS2_AOR_120HS_1ST, CANVAS2_AOR_120HS_2ND },
	[S6E3HAC_VRR_48NS] = { CANVAS2_AOR_120HS_1ST, CANVAS2_AOR_120HS_2ND },
	[S6E3HAC_VRR_120HS] = { CANVAS2_AOR_120HS_1ST, CANVAS2_AOR_120HS_2ND },
	[S6E3HAC_VRR_96HS] = { CANVAS2_AOR_96HS_1ST, CANVAS2_AOR_96HS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { CANVAS2_AOR_120HS_1ST, CANVAS2_AOR_120HS_2ND },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { CANVAS2_AOR_120HS_1ST, CANVAS2_AOR_120HS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { CANVAS2_AOR_96HS_1ST, CANVAS2_AOR_96HS_2ND },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { CANVAS2_AOR_96HS_1ST, CANVAS2_AOR_96HS_2ND },
};

static u8 canvas2_a3_s0_gamma_mtp_0_ns_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_1_ns_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_2_ns_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_3_ns_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_4_ns_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_5_ns_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_6_ns_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];

static u8 canvas2_a3_s0_gamma_mtp_0_hs_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_1_hs_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_2_hs_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_3_hs_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_4_hs_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_5_hs_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_gamma_mtp_6_hs_table[MAX_S6E3HAC_VRR][S6E3HAC_GAMMA_MTP_LEN];
static u8 canvas2_a3_s0_te_frame_sel_table[MAX_S6E3HAC_VRR][1] = {
	[S6E3HAC_VRR_60NS] = { 0x09 },
	[S6E3HAC_VRR_48NS] = { 0x09 },
	[S6E3HAC_VRR_120HS] = { 0x09 },
	[S6E3HAC_VRR_96HS] = { 0x09 },
	[S6E3HAC_VRR_60HS_120HS_TE_SW_SKIP_1] = { 0x09 },
	[S6E3HAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x19 },
	[S6E3HAC_VRR_48HS_96HS_TE_SW_SKIP_1] = { 0x09 },
	[S6E3HAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x19 },
};

static struct maptbl canvas2_a3_s0_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_brt_table, init_gamma_mode2_brt_table, getidx_gamma_mode2_brt_table, copy_common_maptbl),
	[GAMMA_MODE2_EXIT_LPM_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_brt_table, init_common_table, getidx_gamma_mode2_exit_lpm_maptbl, copy_common_maptbl),
	[TSET_MAPTBL] = DEFINE_0D_MAPTBL(canvas2_a3_s0_tset_table, init_common_table, NULL, copy_tset_maptbl),
	[ELVSS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_elvss_table, init_common_table, getidx_gm2_elvss_table, copy_common_maptbl),
	[VBIAS1_MAPTBL] = DEFINE_3D_MAPTBL(canvas2_a3_s0_vbias1_table, init_vbias1_table, getidx_vbias1_table, copy_common_maptbl),
	[VBIAS2_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vbias2_table, init_vbias2_table, getidx_vbias2_table, copy_common_maptbl),
	[HBM_ONOFF_MAPTBL] = DEFINE_3D_MAPTBL(canvas2_a3_s0_hbm_transition_table, init_common_table, getidx_hbm_transition_table, copy_common_maptbl),
	[ACL_OPR_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_acl_opr_table, init_common_table, getidx_acl_opr_table, copy_common_maptbl),
	[LPM_NIT_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_lpm_nit_table, init_lpm_brt_table, getidx_lpm_brt_table, copy_common_maptbl),
	[LPM_NIT_REV40_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_rev40_lpm_nit_table, init_lpm_brt_table, getidx_lpm_brt_table, copy_common_maptbl),
	[LPM_MODE_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_lpm_mode_table, init_common_table, getidx_lpm_mode_table, copy_common_maptbl),
	[LPM_FPS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_lpm_fps_table, init_common_table, getidx_lpm_fps_table, copy_common_maptbl),
	[LPM_OFF_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_lpm_off_table, init_common_table, getidx_lpm_mode_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_XTALK_MODE
	[VGH_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vgh_table, init_common_table, getidx_vgh_table, copy_common_maptbl),
#endif
	[DSC_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_dsc_table, init_common_table, getidx_dsc_table, copy_common_maptbl),
	[PPS_MAPTBL] = DEFINE_3D_MAPTBL(canvas2_a3_s0_pps_table, init_common_table, getidx_resolution_table, copy_common_maptbl),
	[SCALER_MAPTBL] = DEFINE_3D_MAPTBL(canvas2_a3_s0_scaler_table, init_common_table, getidx_resolution_table, copy_common_maptbl),
	[CASET_MAPTBL] = DEFINE_3D_MAPTBL(canvas2_a3_s0_caset_table, init_common_table, getidx_resolution_table, copy_common_maptbl),
	[PASET_MAPTBL] = DEFINE_3D_MAPTBL(canvas2_a3_s0_paset_table, init_common_table, getidx_resolution_table, copy_common_maptbl),
	/* for rev40~51 (less than or equal rev51) */
	[VFP_NS_LE_REV51_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vfp_ns_le_rev51_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VFP_HS_LE_REV51_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vfp_hs_le_rev51_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VTOTAL_NS_LE_REV51_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vtotal_ns_le_rev51_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VTOTAL_HS_LE_REV51_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vtotal_hs_le_rev51_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VFP_OPT_NS_LE_REV51_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vfp_opt_ns_le_rev51_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VFP_OPT_HS_LE_REV51_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vfp_opt_hs_le_rev51_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[AOR_NS_LE_REV51_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_aor_ns_le_rev51_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[AOR_HS_LE_REV51_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_aor_hs_le_rev51_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	/* for rev52~ */
	[VFP_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vfp_ns_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VFP_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vfp_hs_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VTOTAL_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vtotal_ns_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VTOTAL_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vtotal_hs_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VFP_OPT_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vfp_opt_ns_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[VFP_OPT_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vfp_opt_hs_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[AOR_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_aor_ns_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[AOR_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_aor_hs_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),

	[FPS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_fps_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
	[OSC_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_osc_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[OSC_REV40_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_rev40_osc_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[OSC_SEL_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_osc_sel_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[LFD_MIN_MAPTBL] = DEFINE_0D_MAPTBL(canvas2_a3_s0_lfd_min_table, init_common_table, NULL, copy_lfd_min_maptbl),
	[LFD_MAX_MAPTBL] = DEFINE_0D_MAPTBL(canvas2_a3_s0_lfd_max_table, init_common_table, NULL, copy_lfd_max_maptbl),
	[LFD_FRAME_INSERTION_MAPTBL] = DEFINE_3D_MAPTBL(canvas2_a3_s0_lfd_frame_insertion_table,
		init_common_table, getidx_lfd_frame_insertion_table, copy_common_maptbl),
	[LFD_MCA_DITHER_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_lfd_mode_table, init_common_table, getidx_vrr_mode_table, copy_common_maptbl),
	[DIA_ONOFF_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_dia_onoff_table, init_common_table, getidx_dia_onoff_table, copy_common_maptbl),
	[IRC_MODE_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_irc_mode_table, init_common_table, getidx_irc_mode_table, copy_common_maptbl),
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	[VDDM_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_vddm_table, init_common_table, s6e3hac_getidx_vddm_table, copy_common_maptbl),
	[GRAM_IMG_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gram_img_pattern_table, init_common_table, s6e3hac_getidx_gram_img_pattern_table, copy_common_maptbl),
	[GRAM_INV_IMG_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gram_inv_img_pattern_table, init_common_table, s6e3hac_getidx_gram_img_pattern_table, copy_common_maptbl),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	[DYN_FFC_MAPTBL] = DEFINE_3D_MAPTBL(canvas2_a3_s0_dyn_ffc_table, init_common_table, getidx_dyn_ffc_table, copy_common_maptbl),
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	[MAFPC_ENA_MAPTBL] = DEFINE_0D_MAPTBL(canvas2_a3_s0_mafpc_enable, init_common_table, NULL, copy_mafpc_enable_maptbl),
	[MAFPC_SCALE_MAPTBL] = DEFINE_0D_MAPTBL(canvas2_a3_s0_mafpc_scale, init_common_table, NULL, copy_mafpc_scale_maptbl),
#endif
	[GAMMA_MTP_0_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_0_ns_table, init_gamma_mtp_0_ns_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_1_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_1_ns_table, init_gamma_mtp_1_ns_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_2_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_2_ns_table, init_gamma_mtp_2_ns_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_3_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_3_ns_table, init_gamma_mtp_3_ns_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_4_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_4_ns_table, init_gamma_mtp_4_ns_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_5_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_5_ns_table, init_gamma_mtp_5_ns_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_6_NS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_6_ns_table, init_gamma_mtp_6_ns_table, getidx_vrr_fps_table, copy_common_maptbl),

	[GAMMA_MTP_0_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_0_hs_table, init_gamma_mtp_0_hs_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_1_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_1_hs_table, init_gamma_mtp_1_hs_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_2_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_2_hs_table, init_gamma_mtp_2_hs_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_3_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_3_hs_table, init_gamma_mtp_3_hs_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_4_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_4_hs_table, init_gamma_mtp_4_hs_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_5_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_5_hs_table, init_gamma_mtp_5_hs_table, getidx_vrr_fps_table, copy_common_maptbl),
	[GAMMA_MTP_6_HS_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_gamma_mtp_6_hs_table, init_gamma_mtp_6_hs_table, getidx_vrr_fps_table, copy_common_maptbl),
	[TE_FRAME_SEL_MAPTBL] = DEFINE_2D_MAPTBL(canvas2_a3_s0_te_frame_sel_table, init_common_table, getidx_vrr_fps_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [S6E3HAC COMMAND TABLE] ============================== */
/* ===================================================================================== */

static u8 CANVAS2_A3_S0_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 CANVAS2_A3_S0_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 CANVAS2_A3_S0_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 CANVAS2_A3_S0_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 CANVAS2_A3_S0_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 CANVAS2_A3_S0_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 CANVAS2_A3_S0_SLEEP_OUT[] = { 0x11 };
static u8 CANVAS2_A3_S0_SLEEP_IN[] = { 0x10 };
static u8 CANVAS2_A3_S0_DISPLAY_OFF[] = { 0x28 };
static u8 CANVAS2_A3_S0_DISPLAY_ON[] = { 0x29 };
static u8 CANVAS2_A3_S0_DUMMY_2C[] = { 0x2C, 0x00, 0x00, 0x00 };

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static u8 CANVAS2_A3_S0_SW_RESET[] = { 0x01 };
static u8 CANVAS2_A3_S0_GCT_DSC[] = { 0x9D, 0x01 };
static u8 CANVAS2_A3_S0_GCT_PPS[] = { 0x9E,
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x10,
	0x05, 0xA0, 0x00, 0xC1, 0x02, 0xD0, 0x02, 0xD0,
	0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x15, 0x8B,
	0x00, 0x0A, 0x00, 0x0C, 0x00, 0x80, 0x00, 0x66,
	0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
	0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
	0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
	0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
	0x00
};
#endif
static u8 CANVAS2_A3_S0_DSC[] = { 0x01 };
static u8 CANVAS2_A3_S0_PPS[] = {
	// WQHD : 1440x3200
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x80,
	0x05, 0xA0, 0x00, 0x28, 0x02, 0xD0, 0x02, 0xD0,
	0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x04, 0x6C,
	0x00, 0x0A, 0x00, 0x0C, 0x02, 0x77, 0x01, 0xE9,
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

static u8 CANVAS2_A3_S0_TE_ON[] = { 0x35, 0x00 };
#ifdef CONFIG_SEC_FACTORY
static u8 CANVAS2_A3_S0_ERR_FG_1[] = { 0xF4, 0x18 };
static u8 CANVAS2_A3_S0_ERR_FG_2[] = { 0xED, 0x40, 0x20 };
#else
static u8 CANVAS2_A3_S0_ERR_FG[] = { 0xED, 0x04, 0x4C };
#endif
#ifdef CONFIG_DYNAMIC_FREQ
static u8 CANVAS2_A3_S0_FFC[] = {
	0xC5,
	0x11, 0x10, 0x50, 0x34, 0xC9, 0x34, 0xC9 /* default 1170 */
};
#endif
static u8 CANVAS2_A3_S0_FFC_DEFAULT[] = {
	0xC5,
	0x11, 0x10, 0x50, 0x34, 0xC9, 0x34, 0xC9 /* default 1170 */
};

static u8 CANVAS2_A3_S0_TSP_HSYNC[] = {
	0xB9,
	0x09,	/* to be updated by TE_FRAME_SEL_MAPTBL */
	0x0C, 0x20, 0x0C, 0x3F, 0x00, 0x00, 0x00,
	0x00, 0x11, 0x11, 0x03, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x15, 0x02, 0x40, 0x02,
	0x15, 0x00, 0x40
};

static u8 CANVAS2_A3_S0_SET_TE_FRAME_SEL[] = {
	0xB9,
	0x09,	/* to be updated by TE_FRAME_SEL_MAPTBL */
	0x0C, 0x20, 0x0C, 0x3F
};

static u8 CANVAS2_A3_S0_CLR_TE_FRAME_SEL[] = {
	0xB9,
	0x08,	/* clear TE_FRAME_SEL & TE_SEL */
};

static u8 CANVAS2_A3_S0_ETC_SET_SLPOUT[] = {
	0xCB, 0x25
};

static u8 CANVAS2_A3_S0_ETC_SET_SLPIN[] = {
	0xCB, 0x65
};

static u8 CANVAS2_A3_S0_VBIAS[] = {
	0xF4,
	0x3C, 0x3C, 0x3C, 0x3C, 0x36, 0x2C, 0x2C, 0x38,
	0x38, 0x38, 0x38, 0x38, 0x44, 0x88, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 CANVAS2_A3_S0_XTALK_VGH[] = {
	0xF4, 0x31
};
#endif

static u8 CANVAS2_A3_S0_LFD_SETTING[] = {
	0xBD,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01
};

static u8 CANVAS2_A3_S0_LPM_LFD[] = {
	0xBD,
	0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x08
};

static u8 CANVAS2_A3_S0_LFD_ON[] = {
	0xBD,
	0x41
};

static u8 CANVAS2_A3_S0_LPM_ON_LTPS_1[] = {
	0xCB,
	0xC4
};

static u8 CANVAS2_A3_S0_LPM_ON_LTPS_2[] = {
	0xCB,
	0xC4
};

static u8 CANVAS2_A3_S0_LPM_OFF_LTPS_1[] = {
	0xCB,
	0xC3
};

static u8 CANVAS2_A3_S0_LPM_OFF_LTPS_2[] = {
	0xCB,
	0xC3
};

static u8 CANVAS2_A3_S0_LPM_OFF_ASWIRE_OFF[] = {
	0xB5,
	0x00
};

/* use only with 5x panel */
static u8 CANVAS2_A3_S0_LPM_ON_LTPS_3[] = {
	0xCB,
	0x00, 0x1F, 0x00, 0x02, 0x00, 0x0A
};

static u8 CANVAS2_A3_S0_LPM_ON_LTPS_4[] = {
	0xCB,
	0x00, 0x00, 0x02
};

static u8 CANVAS2_A3_S0_LPM_ON_LTPS_5[] = {
	0xB7,
	0x1B, 0x00, 0x02, 0x00, 0x06, 0x02
};

static u8 CANVAS2_A3_S0_LPM_OFF_LTPS_3[] = {
	0xCB,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 CANVAS2_A3_S0_LPM_OFF_LTPS_4[] = {
	0xCB,
	0x00, 0x00, 0x00
};

static u8 CANVAS2_A3_S0_LPM_OFF_LTPS_5[] = {
	0xB7,
	0x1B, 0x00, 0x02, 0x00, 0x16, 0x02
};

static u8 CANVAS2_A3_S0_LPM_WACOM_RESTORE_1[] = {
	0xF2,
	0x1C, 0x00, 0x04, 0x10, 0x10
};

static u8 CANVAS2_A3_S0_LPM_WACOM_RESTORE_2[] = {
	0xF2,
	0x1C, 0x00, 0x04, 0x10, 0x10
};

static u8 CANVAS2_A3_S0_LPM_WACOM_RESTORE_3[] = {
	0xCB,
	0x6E, 0x00, 0x00, 0x00, 0x40, 0x0C, 0x6E
};

static u8 CANVAS2_A3_S0_LPM_WACOM_RESTORE_4[] = {
	0xCB,
	0x31, 0x00, 0x00, 0x00, 0xD8, 0x38, 0x31, 0x00,
	0x00, 0x00, 0xF8, 0x14, 0x31, 0x00, 0x00, 0x00,
	0xD8, 0x14, 0x31
};

/* use only with 5x panel end */

static u8 CANVAS2_A3_S0_LPM_FLICKER_SET_1[] = {
	0xF4, 0xD8
};

static u8 CANVAS2_A3_S0_LPM_FLICKER_SET_2[] = {
	0xB5, 0x12
};

static u8 CANVAS2_A3_S0_IRC_MODE[] = {
	0x92, 0x65
};

static u8 CANVAS2_A3_S0_IRC_TEMPORARY[] = {
	0x92, 0x05
};

static u8 CANVAS2_A3_S0_DIA_ONOFF[] = {
	0x91,
	0x09	/* default on */
};

static u8 CANVAS2_A3_S0_SMOOTH_DIMMING[] = {
	0xB1, 0x08
};

static u8 CANVAS2_A3_S0_DITHER[] = {
	0x6A,
	0x18, 0x86, 0xB1, 0x18, 0x86, 0xB1, 0x18, 0x86,
	0xB1, 0x00, 0x00, 0x00, 0x00, 0x0F
};

static u8 CANVAS2_A3_S0_TSET_ELVSS[] = {
	0xB5,
	0x19, 0xCD, 0x16,
};

static u8 CANVAS2_A3_S0_LFD_MODE_DISPLAY_OFF[] = {
	0xB1, 0x00
};

static u8 CANVAS2_A3_S0_VAINT_SET_DISPLAY_OFF[] = {
	0xF4, 0xA8,
};

static u8 CANVAS2_A3_S0_GAMMA_MTP_0_NS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_0_NS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_1_NS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_1_NS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_2_NS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_2_NS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_3_NS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_3_NS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_4_NS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_4_NS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_5_NS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_5_NS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_6_NS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_6_NS_REG, 0x00, };

static u8 CANVAS2_A3_S0_GAMMA_MTP_0_HS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_0_HS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_1_HS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_1_HS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_2_HS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_2_HS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_3_HS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_3_HS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_4_HS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_4_HS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_5_HS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_5_HS_REG, 0x00, };
static u8 CANVAS2_A3_S0_GAMMA_MTP_6_HS[S6E3HAC_GAMMA_CMD_CNT] = { S6E3HAC_GAMMA_MTP_6_HS_REG, 0x00, };

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static u8 CANVAS2_A3_S0_CMDLOG_ENABLE[] = { 0xF7, 0x80 };
static u8 CANVAS2_A3_S0_CMDLOG_DISABLE[] = { 0xF7, 0x00 };
static u8 CANVAS2_A3_S0_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x8F };
#else
static u8 CANVAS2_A3_S0_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x0F };
#endif
static u8 CANVAS2_A3_S0_SCALER[] = { 0xBA, 0x01 };
static u8 CANVAS2_A3_S0_CASET[] = { 0x2A, 0x00, 0x00, 0x05, 0x9F };
static u8 CANVAS2_A3_S0_PASET[] = { 0x2B, 0x00, 0x00, 0x0C, 0x0F };

static u8 CANVAS2_A3_S0_LPM_NIT[] = { 0xBB, 0x0B, 0xCA };
static u8 CANVAS2_A3_S0_LPM_MODE[] = { 0xBB, 0x00 };
static u8 CANVAS2_A3_S0_LPM_ON[] = { 0x53, 0x23 };
static u8 CANVAS2_A3_S0_LPM_VAINT_ON[] = { 0xB1, 0x00 };
static u8 CANVAS2_A3_S0_LPM_VAINT_OFF[] = { 0xB1, 0x0A };
static u8 CANVAS2_A3_S0_LPM_VBIAS[] = {
	0xF4,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x28,
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};
static u8 CANVAS2_A3_S0_LPM_VBIAS_FLICKER_SET[] = {
	0xF4,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xA8
};

static u8 CANVAS2_A3_S0_LPM_VBIAS_FLICKER_EXIT[] = {
	0xF4,
	0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8, 0xA8,
};

static u8 CANVAS2_A3_S0_REV40_LPM_VBIAS[] = {
	0xF4,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x28,
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 CANVAS2_A3_S0_MCD_ON_00[] = {
	0xCB,
	0xD7, 0x00, 0x00, 0x00, 0x40, 0x18, 0xD7
};
static u8 CANVAS2_A3_S0_MCD_ON_01[] = {
	0xB7,
	0x6F, 0x5F, 0x00, 0x00, 0x00, 0xD8, 0x6F, 0x5F,
	0x00, 0x00, 0x00, 0xF8, 0x27, 0x5F, 0x00, 0x00,
	0x00, 0xD8, 0x27, 0x5F, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x83
};
static u8 CANVAS2_A3_S0_MCD_ON_02[] = { 0xB7, 0x8A, 0x87 };
static u8 CANVAS2_A3_S0_MCD_ON_03[] = { 0xF2, 0x20, 0x10 };
static u8 CANVAS2_A3_S0_MCD_ON_04[] = { 0xCC, 0x73 };
static u8 CANVAS2_A3_S0_MCD_ON_05[] = { 0xF6, 0x00 };

static u8 CANVAS2_A3_S0_MCD_OFF_00[] = {
	0xCB,
	0x5A, 0x00, 0x00, 0x00, 0x40, 0x18, 0x5A
};
static u8 CANVAS2_A3_S0_MCD_OFF_01[] = {
	0xB7,
	0x61, 0x20, 0x00, 0x00, 0x00, 0xD8, 0x61, 0x20,
	0x00, 0x00, 0x00, 0xF8, 0x27, 0x11, 0x00, 0x00,
	0x00, 0xD8, 0x27, 0x11, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static u8 CANVAS2_A3_S0_MCD_OFF_02[] = { 0xB7, 0x82, 0x85 };
static u8 CANVAS2_A3_S0_MCD_OFF_03[] = { 0xF2, 0x10, 0x00 };
static u8 CANVAS2_A3_S0_MCD_OFF_04[] = { 0xCC, 0x70 };
static u8 CANVAS2_A3_S0_MCD_OFF_05[] = { 0xF6, 0x69 };

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static u8 CANVAS2_A3_S0_DYNAMIC_HLPM_ENABLE[] = {
	0x85,
	0x03, 0x0B, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x26, 0x02, 0xB2, 0x07, 0xBC, 0x09, 0xEB
};

static u8 CANVAS2_A3_S0_DYNAMIC_HLPM_DISABLE[] = {
	0x85,
	0x00
};
#endif

#if 0
/* MCD 3.0 */
static u8 CANVAS2_A3_S0_MCD_RS_ON_01[] = { 0xF3, 0x1F };
static u8 CANVAS2_A3_S0_MCD_RS_ON_02[] = { 0xFD, 0x30 };
static u8 CANVAS2_A3_S0_MCD_RS_ON_03[] = { 0xCC, 0x00 };
static u8 CANVAS2_A3_S0_MCD_RS_ON_04[] = { 0xF3, 0x5F };
static u8 CANVAS2_A3_S0_MCD_RS_ON_05[] = { 0xF3, 0x5E };
static u8 CANVAS2_A3_S0_MCD_RS_ON_06[] = { 0xF3, 0x5A };
static u8 CANVAS2_A3_S0_MCD_RS_ON_07[] = { 0xF3, 0x52 };
static u8 CANVAS2_A3_S0_MCD_RS_ON_08[] = { 0xCC, 0x88 };
static u8 CANVAS2_A3_S0_MCD_RS_ON_09[] = { 0xF4, 0x97 };

static u8 CANVAS2_A3_S0_MCD_RS_OFF_01[] = { 0xF4, 0x17 };
static u8 CANVAS2_A3_S0_MCD_RS_OFF_02[] = { 0xCC, 0x89 };
static u8 CANVAS2_A3_S0_MCD_RS_OFF_03[] = { 0xF3, 0x5A };
static u8 CANVAS2_A3_S0_MCD_RS_OFF_04[] = { 0xF3, 0x5E };
static u8 CANVAS2_A3_S0_MCD_RS_OFF_05[] = { 0xF3, 0x5F };
static u8 CANVAS2_A3_S0_MCD_RS_OFF_06[] = { 0xCC, 0x89 };
static u8 CANVAS2_A3_S0_MCD_RS_OFF_07[] = { 0xF3, 0x1F };

static u8 CANVAS2_A3_S0_MCD_RS_1_RIGHT_01[] = { 0xCC, 0x00 };
static u8 CANVAS2_A3_S0_MCD_RS_2_RIGHT_01[] = { 0xCC, 0x00 };
static u8 CANVAS2_A3_S0_MCD_RS_RIGHT_02[] = { 0xCC, 0x10 };
static u8 CANVAS2_A3_S0_MCD_RS_1_RIGHT_03[] = { 0xCC, 0x80 };
static u8 CANVAS2_A3_S0_MCD_RS_2_RIGHT_03[] = { 0xCC, 0x40 };
static u8 CANVAS2_A3_S0_MCD_RS_RIGHT_04[] = { 0xCC, 0x00 };
static u8 CANVAS2_A3_S0_MCD_RS_RIGHT_05[] = { 0xCC, 0x00 };

static u8 CANVAS2_A3_S0_MCD_RS_1_LEFT_01[] = { 0xCC, 0x00 };
static u8 CANVAS2_A3_S0_MCD_RS_2_LEFT_01[] = { 0xCC, 0x00 };
static u8 CANVAS2_A3_S0_MCD_RS_LEFT_02[] = { 0xCC, 0x01 };
static u8 CANVAS2_A3_S0_MCD_RS_1_LEFT_03[] = { 0xCC, 0x08 };
static u8 CANVAS2_A3_S0_MCD_RS_2_LEFT_03[] = { 0xCC, 0x04 };
static u8 CANVAS2_A3_S0_MCD_RS_LEFT_04[] = { 0xCC, 0x00 };
static u8 CANVAS2_A3_S0_MCD_RS_LEFT_05[] = { 0xCC, 0x00 };
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static u8 CANVAS2_A3_S0_VDDM_ORIG[] = { 0xD7, 0x00 };
static u8 CANVAS2_A3_S0_VDDM_VOLT[] = { 0xD7, 0x00 };
static u8 CANVAS2_A3_S0_VDDM_INIT[] = { 0xFE, 0x14 };
static u8 CANVAS2_A3_S0_HOP_CHKSUM_ON[] = { 0xFE, 0x7A };
static u8 CANVAS2_A3_S0_GRAM_CHKSUM_START[] = { 0x2C };
static u8 CANVAS2_A3_S0_GRAM_IMG_PATTERN_ON[] = { 0xBE, 0x00 };
static u8 CANVAS2_A3_S0_GRAM_INV_IMG_PATTERN_ON[] = { 0xBE, 0x00 };
static u8 CANVAS2_A3_S0_GRAM_IMG_PATTERN_OFF[] = { 0xBE, 0x00 };
static u8 CANVAS2_A3_S0_GRAM_DBV_MAX[] = { 0x51, 0x03, 0xFF };
static u8 CANVAS2_A3_S0_GRAM_BCTRL_ON[] = { 0x53, 0x21 };
#endif

#ifdef CONFIG_SUPPORT_MST
static u8 CANVAS2_A3_S0_MST_ON_01[] = {
	0xBF,
	0x33, 0x25, 0xFF, 0x00, 0x00, 0x10
};
static u8 CANVAS2_A3_S0_MST_ON_02[] = { 0xF6, 0x50 };

static u8 CANVAS2_A3_S0_MST_OFF_01[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10
};
static u8 CANVAS2_A3_S0_MST_OFF_02[] = { 0xF6, 0x69 };
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static u8 CANVAS2_A3_S0_CCD_ENABLE[] = { 0xCC, 0x01, 0x40, 0x51 };
static u8 CANVAS2_A3_S0_CCD_DISABLE[] = { 0xCC, 0x00, 0x00, 0x00 };
#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static u8 CANVAS2_A3_S0_GRAYSPOT_ON_00[] = {
	0xB5, 0xB5
};
static u8 CANVAS2_A3_S0_GRAYSPOT_ON_01[] = {
	0xF2, 0xF0
};
static u8 CANVAS2_A3_S0_GRAYSPOT_ON_02[] = {
	0xF4, 0x23
};
static u8 CANVAS2_A3_S0_GRAYSPOT_ON_03[] = {
	0xF6, 0x00
};
static u8 CANVAS2_A3_S0_GRAYSPOT_ON_04[] = {
	0xF2, 0x00
};
static u8 CANVAS2_A3_S0_GRAYSPOT_ON_05[] = {
	0xF4,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 CANVAS2_A3_S0_GRAYSPOT_OFF_00[] = {
	0xB5, 0x00
};
static u8 CANVAS2_A3_S0_GRAYSPOT_OFF_01[] = {
	0xF2, 0x70
};
static u8 CANVAS2_A3_S0_GRAYSPOT_OFF_02[] = {
	0xF4, 0x19
};
static u8 CANVAS2_A3_S0_GRAYSPOT_OFF_03[] = {
	0xF6, 0x69
};
static u8 CANVAS2_A3_S0_GRAYSPOT_OFF_04[] = {
	0xF2, 0xB4
};
#endif

#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
static u8 CANVAS2_A3_S0_BRIGHTDOT_ON_MAX_BRIGHTNESS[] = {
	0x51, 0x03, 0xFF
};
static u8 CANVAS2_A3_S0_BRIGHTDOT_ON_HS_AOR[] = {
	0xB1, 0x0C, 0x40
};
#endif

static DEFINE_STATIC_PACKET(canvas2_a3_s0_level1_key_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_level2_key_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_level3_key_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_level1_key_disable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_level2_key_disable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_level3_key_disable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_sleep_out, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_sleep_in, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_display_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_display_off, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_dummy_2c, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_DUMMY_2C, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_te_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_TE_ON, 0);
#ifdef CONFIG_SEC_FACTORY
static DEFINE_STATIC_PACKET(canvas2_a3_s0_err_fg_1, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_ERR_FG_1, 0x23);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_err_fg_2, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_ERR_FG_2, 0x01);
#else
static DEFINE_STATIC_PACKET(canvas2_a3_s0_err_fg, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_ERR_FG, 0);
#endif
static DEFINE_STATIC_PACKET(canvas2_a3_s0_dither, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_DITHER, 0x06);

static DECLARE_PKTUI(canvas2_a3_s0_tset_elvss) = {
	{ .offset = 1, .maptbl = &canvas2_a3_s0_maptbl[TSET_MAPTBL] },
	{ .offset = 3, .maptbl = &canvas2_a3_s0_maptbl[ELVSS_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_tset_elvss, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_TSET_ELVSS, 0);

static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_0_ns, &canvas2_a3_s0_maptbl[GAMMA_MTP_0_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_0_ns, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_0_NS, S6E3HAC_GAMMA_MTP_0_NS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_1_ns, &canvas2_a3_s0_maptbl[GAMMA_MTP_1_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_1_ns, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_1_NS, S6E3HAC_GAMMA_MTP_1_NS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_2_ns, &canvas2_a3_s0_maptbl[GAMMA_MTP_2_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_2_ns, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_2_NS, S6E3HAC_GAMMA_MTP_2_NS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_3_ns, &canvas2_a3_s0_maptbl[GAMMA_MTP_3_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_3_ns, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_3_NS, S6E3HAC_GAMMA_MTP_3_NS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_4_ns, &canvas2_a3_s0_maptbl[GAMMA_MTP_4_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_4_ns, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_4_NS, S6E3HAC_GAMMA_MTP_4_NS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_5_ns, &canvas2_a3_s0_maptbl[GAMMA_MTP_5_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_5_ns, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_5_NS, S6E3HAC_GAMMA_MTP_5_NS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_6_ns, &canvas2_a3_s0_maptbl[GAMMA_MTP_6_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_6_ns, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_6_NS, S6E3HAC_GAMMA_MTP_6_NS_OFS);

static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_0_hs, &canvas2_a3_s0_maptbl[GAMMA_MTP_0_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_0_hs, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_0_HS, S6E3HAC_GAMMA_MTP_0_HS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_1_hs, &canvas2_a3_s0_maptbl[GAMMA_MTP_1_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_1_hs, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_1_HS, S6E3HAC_GAMMA_MTP_1_HS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_2_hs, &canvas2_a3_s0_maptbl[GAMMA_MTP_2_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_2_hs, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_2_HS, S6E3HAC_GAMMA_MTP_2_HS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_3_hs, &canvas2_a3_s0_maptbl[GAMMA_MTP_3_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_3_hs, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_3_HS, S6E3HAC_GAMMA_MTP_3_HS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_4_hs, &canvas2_a3_s0_maptbl[GAMMA_MTP_4_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_4_hs, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_4_HS, S6E3HAC_GAMMA_MTP_4_HS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_5_hs, &canvas2_a3_s0_maptbl[GAMMA_MTP_5_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_5_hs, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_5_HS, S6E3HAC_GAMMA_MTP_5_HS_OFS);
static DEFINE_PKTUI(canvas2_a3_s0_gamma_mtp_6_hs, &canvas2_a3_s0_maptbl[GAMMA_MTP_6_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gamma_mtp_6_hs, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_MTP_6_HS, S6E3HAC_GAMMA_MTP_6_HS_OFS);

#ifdef CONFIG_DYNAMIC_FREQ
static DEFINE_PKTUI(canvas2_a3_s0_ffc, &canvas2_a3_s0_maptbl[DYN_FFC_MAPTBL], 4);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_ffc, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FFC, 0);
#endif
static DEFINE_STATIC_PACKET(canvas2_a3_s0_ffc_default, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FFC_DEFAULT, 0);

static DEFINE_PKTUI(canvas2_a3_s0_tsp_hsync, &canvas2_a3_s0_maptbl[TE_FRAME_SEL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_tsp_hsync, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_TSP_HSYNC, 0);
static DEFINE_PKTUI(canvas2_a3_s0_set_te_frame_sel, &canvas2_a3_s0_maptbl[TE_FRAME_SEL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_set_te_frame_sel, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_SET_TE_FRAME_SEL, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_clr_te_frame_sel, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_CLR_TE_FRAME_SEL, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_etc_set_slpout, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_ETC_SET_SLPOUT, 0x0A);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_etc_set_slpin, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_ETC_SET_SLPIN, 0x0A);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_irc_temporary, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_IRC_TEMPORARY, 0x10);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_smooth_dimming, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_SMOOTH_DIMMING, 0x0B);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lfd_mode_display_off, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LFD_MODE_DISPLAY_OFF, 0x6A);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_vaint_set_display_off, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VAINT_SET_DISPLAY_OFF, 0x30);

static DECLARE_PKTUI(canvas2_a3_s0_lfd_setting) = {
	{ .offset = 2, .maptbl = &canvas2_a3_s0_maptbl[LFD_MIN_MAPTBL] },
	{ .offset = 3, .maptbl = &canvas2_a3_s0_maptbl[LFD_FRAME_INSERTION_MAPTBL] },
	{ .offset = 9, .maptbl = &canvas2_a3_s0_maptbl[LFD_MCA_DITHER_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_lfd_setting, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LFD_SETTING, 0x06);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_lfd_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LFD_ON, 0x13);

static DECLARE_PKTUI(canvas2_a3_s0_vbias) = {
	{ .offset = 1, .maptbl = &canvas2_a3_s0_maptbl[VBIAS1_MAPTBL] },
	{ .offset = 22, .maptbl = &canvas2_a3_s0_maptbl[VBIAS2_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vbias, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VBIAS, 0x29);

static DECLARE_PKTUI(canvas2_a3_s0_vbias_rev40_rev50) = {
	{ .offset = 1, .maptbl = &canvas2_a3_s0_maptbl[VBIAS1_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vbias_rev40_rev50, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VBIAS, 0x29);

#ifdef CONFIG_SUPPORT_XTALK_MODE
static DEFINE_PKTUI(canvas2_a3_s0_xtalk_vgh, &canvas2_a3_s0_maptbl[VGH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_xtalk_vgh, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_XTALK_VGH, 0);
#endif

static DEFINE_PKTUI(canvas2_a3_s0_irc_mode, &canvas2_a3_s0_maptbl[IRC_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_irc_mode, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_IRC_MODE, 0x0B);

static DEFINE_PKTUI(canvas2_a3_s0_dia_onoff, &canvas2_a3_s0_maptbl[DIA_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_dia_onoff, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_DIA_ONOFF, 0x68);

/* for rev40~51 (less than or equal rev51) */
static u8 CANVAS2_A3_S0_AOR_NS_LE_REV51[S6E3HAC_AOR_NS_LEN + 1] = { S6E3HAC_AOR_NS_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_aor_ns_le_rev51, &canvas2_a3_s0_maptbl[AOR_NS_LE_REV51_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_aor_ns_le_rev51, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_AOR_NS_LE_REV51, S6E3HAC_AOR_NS_OFS);

static u8 CANVAS2_A3_S0_AOR_HS_LE_REV51[S6E3HAC_AOR_HS_LEN + 1] = { S6E3HAC_AOR_HS_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_aor_hs_le_rev51, &canvas2_a3_s0_maptbl[AOR_HS_LE_REV51_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_aor_hs_le_rev51, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_AOR_HS_LE_REV51, S6E3HAC_AOR_HS_OFS);

static u8 CANVAS2_A3_S0_VFP_OPT_NS_LE_REV51[S6E3HAC_VFP_OPT_NS_LEN + 1] = { S6E3HAC_VFP_OPT_NS_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vfp_opt_ns_le_rev51, &canvas2_a3_s0_maptbl[VFP_OPT_NS_LE_REV51_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vfp_opt_ns_le_rev51, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VFP_OPT_NS_LE_REV51, S6E3HAC_VFP_OPT_NS_OFS);

static u8 CANVAS2_A3_S0_VFP_OPT_HS_LE_REV51[S6E3HAC_VFP_OPT_HS_LEN + 1] = { S6E3HAC_VFP_OPT_HS_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vfp_opt_hs_le_rev51, &canvas2_a3_s0_maptbl[VFP_OPT_HS_LE_REV51_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vfp_opt_hs_le_rev51, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VFP_OPT_HS_LE_REV51, S6E3HAC_VFP_OPT_HS_OFS);

static u8 CANVAS2_A3_S0_VTOTAL_NS_0_LE_REV51[S6E3HAC_VTOTAL_NS_0_LEN + 1] = { S6E3HAC_VTOTAL_NS_0_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vtotal_ns_0_le_rev51, &canvas2_a3_s0_maptbl[VTOTAL_NS_LE_REV51_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vtotal_ns_0_le_rev51, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VTOTAL_NS_0_LE_REV51, S6E3HAC_VTOTAL_NS_0_OFS);

static u8 CANVAS2_A3_S0_VTOTAL_NS_1_LE_REV51[S6E3HAC_VTOTAL_NS_1_LEN + 1] = { S6E3HAC_VTOTAL_NS_1_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vtotal_ns_1_le_rev51, &canvas2_a3_s0_maptbl[VTOTAL_NS_LE_REV51_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vtotal_ns_1_le_rev51, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VTOTAL_NS_1_LE_REV51, S6E3HAC_VTOTAL_NS_1_OFS);

static u8 CANVAS2_A3_S0_VTOTAL_HS_0_LE_REV51[S6E3HAC_VTOTAL_HS_0_LEN + 1] = { S6E3HAC_VTOTAL_HS_0_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vtotal_hs_0_le_rev51, &canvas2_a3_s0_maptbl[VTOTAL_HS_LE_REV51_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vtotal_hs_0_le_rev51, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VTOTAL_HS_0_LE_REV51, S6E3HAC_VTOTAL_HS_0_OFS);

static u8 CANVAS2_A3_S0_VTOTAL_HS_1_LE_REV51[S6E3HAC_VTOTAL_HS_1_LEN + 1] = { S6E3HAC_VTOTAL_HS_1_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vtotal_hs_1_le_rev51, &canvas2_a3_s0_maptbl[VTOTAL_HS_LE_REV51_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vtotal_hs_1_le_rev51, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VTOTAL_HS_1_LE_REV51, S6E3HAC_VTOTAL_HS_1_OFS);

/* for rev52~ */
static u8 CANVAS2_A3_S0_AOR_NS[S6E3HAC_AOR_NS_LEN + 1] = { S6E3HAC_AOR_NS_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_aor_ns, &canvas2_a3_s0_maptbl[AOR_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_aor_ns, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_AOR_NS, S6E3HAC_AOR_NS_OFS);

static u8 CANVAS2_A3_S0_AOR_HS[S6E3HAC_AOR_HS_LEN + 1] = { S6E3HAC_AOR_HS_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_aor_hs, &canvas2_a3_s0_maptbl[AOR_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_aor_hs, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_AOR_HS, S6E3HAC_AOR_HS_OFS);

static u8 CANVAS2_A3_S0_VFP_OPT_NS[S6E3HAC_VFP_OPT_NS_LEN + 1] = { S6E3HAC_VFP_OPT_NS_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vfp_opt_ns, &canvas2_a3_s0_maptbl[VFP_OPT_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vfp_opt_ns, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VFP_OPT_NS, S6E3HAC_VFP_OPT_NS_OFS);

static u8 CANVAS2_A3_S0_VFP_OPT_HS[S6E3HAC_VFP_OPT_HS_LEN + 1] = { S6E3HAC_VFP_OPT_HS_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vfp_opt_hs, &canvas2_a3_s0_maptbl[VFP_OPT_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vfp_opt_hs, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VFP_OPT_HS, S6E3HAC_VFP_OPT_HS_OFS);

static u8 CANVAS2_A3_S0_VTOTAL_NS_0[S6E3HAC_VTOTAL_NS_0_LEN + 1] = { S6E3HAC_VTOTAL_NS_0_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vtotal_ns_0, &canvas2_a3_s0_maptbl[VTOTAL_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vtotal_ns_0, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VTOTAL_NS_0, S6E3HAC_VTOTAL_NS_0_OFS);

static u8 CANVAS2_A3_S0_VTOTAL_NS_1[S6E3HAC_VTOTAL_NS_1_LEN + 1] = { S6E3HAC_VTOTAL_NS_1_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vtotal_ns_1, &canvas2_a3_s0_maptbl[VTOTAL_NS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vtotal_ns_1, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VTOTAL_NS_1, S6E3HAC_VTOTAL_NS_1_OFS);

static u8 CANVAS2_A3_S0_VTOTAL_HS_0[S6E3HAC_VTOTAL_HS_0_LEN + 1] = { S6E3HAC_VTOTAL_HS_0_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vtotal_hs_0, &canvas2_a3_s0_maptbl[VTOTAL_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vtotal_hs_0, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VTOTAL_HS_0, S6E3HAC_VTOTAL_HS_0_OFS);

static u8 CANVAS2_A3_S0_VTOTAL_HS_1[S6E3HAC_VTOTAL_HS_1_LEN + 1] = { S6E3HAC_VTOTAL_HS_1_REG, };
static DEFINE_PKTUI(canvas2_a3_s0_vtotal_hs_1, &canvas2_a3_s0_maptbl[VTOTAL_HS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vtotal_hs_1, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VTOTAL_HS_1, S6E3HAC_VTOTAL_HS_1_OFS);

static u8 CANVAS2_A3_S0_FPS[] = { 0x60, 0x00, 0x00 };
static DECLARE_PKTUI(canvas2_a3_s0_fps) = {
	{ .offset = 1, .maptbl = &canvas2_a3_s0_maptbl[FPS_MAPTBL] },
	{ .offset = 2, .maptbl = &canvas2_a3_s0_maptbl[LFD_MAX_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_fps, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FPS, 0);

static DEFINE_PKTUI(canvas2_a3_s0_dsc, &canvas2_a3_s0_maptbl[DSC_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_dsc, DSI_PKT_TYPE_COMP, CANVAS2_A3_S0_DSC, 0);

static DEFINE_PKTUI(canvas2_a3_s0_pps, &canvas2_a3_s0_maptbl[PPS_MAPTBL], 0);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_pps, DSI_PKT_TYPE_PPS, CANVAS2_A3_S0_PPS, 0);

static DEFINE_PKTUI(canvas2_a3_s0_scaler, &canvas2_a3_s0_maptbl[SCALER_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_scaler, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_SCALER, 0);
static DEFINE_PKTUI(canvas2_a3_s0_caset, &canvas2_a3_s0_maptbl[CASET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_caset, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_CASET, 0);
static DEFINE_PKTUI(canvas2_a3_s0_paset, &canvas2_a3_s0_maptbl[PASET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_paset, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_PASET, 0);

/* LPM MODE SETTING */
static DEFINE_PKTUI(canvas2_a3_s0_lpm_exit_mode, &canvas2_a3_s0_maptbl[LPM_OFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_lpm_exit_mode, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_MODE, 0);
static DEFINE_PKTUI(canvas2_a3_s0_lpm_enter_mode, &canvas2_a3_s0_maptbl[LPM_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_lpm_enter_mode, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_MODE, 0);
static DEFINE_PKTUI(canvas2_a3_s0_lpm_nit, &canvas2_a3_s0_maptbl[LPM_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_lpm_nit, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_NIT, 0x05);
/* use only with 40 panel */
static DEFINE_PKTUI(canvas2_a3_s0_rev40_lpm_nit, &canvas2_a3_s0_maptbl[LPM_NIT_REV40_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_rev40_lpm_nit, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_NIT, 0x05);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_rev40_lpm_vbias, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_REV40_LPM_VBIAS, 0x29);
/* use only with 40 panel end */
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_vbias, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_VBIAS, 0x29);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_vbias_flicker_set, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_VBIAS_FLICKER_SET, 0x29);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_vbias_flicker_exit, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_VBIAS_FLICKER_EXIT, 0x30);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_vaint_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_VAINT_ON, 0x6A);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_vaint_off, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_VAINT_OFF, 0x6A);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_ON, 0);

static DEFINE_PKTUI(canvas2_a3_s0_lpm_lfd, &canvas2_a3_s0_maptbl[LPM_FPS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_lpm_lfd, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_LFD, 0x04);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_on_ltps_1, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_ON_LTPS_1, 0x14);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_on_ltps_2, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_ON_LTPS_2, 0x26);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_off_ltps_1, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_OFF_LTPS_1, 0x14);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_off_ltps_2, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_OFF_LTPS_2, 0x26);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_off_aswire_off, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_OFF_ASWIRE_OFF, 0x53);

/* use only with 5x panel */
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_on_ltps_3, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_ON_LTPS_3, 0x3E);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_on_ltps_4, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_ON_LTPS_4, 0x52);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_on_ltps_5, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_ON_LTPS_5, 0x35);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_off_ltps_3, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_OFF_LTPS_3, 0x3E);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_off_ltps_4, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_OFF_LTPS_4, 0x52);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_off_ltps_5, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_OFF_LTPS_5, 0x35);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_wacom_restore_1, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_WACOM_RESTORE_1, 0x05);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_wacom_restore_2, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_WACOM_RESTORE_2, 0x0C);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_wacom_restore_3, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_WACOM_RESTORE_3, 0x60);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_wacom_restore_4, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_WACOM_RESTORE_4, 0x8B);
/* use only with 5x panel end */

static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_flicker_set_1, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_FLICKER_SET_1, 0x09);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_flicker_set_2, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_FLICKER_SET_2, 0x4C);

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static DEFINE_STATIC_PACKET(canvas2_a3_s0_sw_reset, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_SW_RESET, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_gct_dsc, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GCT_DSC, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_gct_pps, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GCT_PPS, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_vddm_orig, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VDDM_ORIG, 0x02);
static DEFINE_PKTUI(canvas2_a3_s0_vddm_volt, &canvas2_a3_s0_maptbl[VDDM_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_vddm_volt, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VDDM_VOLT, 0x02);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_vddm_init, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_VDDM_INIT, 0x0A);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_hop_chksum_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_HOP_CHKSUM_ON, 0x2F);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_gram_chksum_start, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAM_CHKSUM_START, 0);
static DEFINE_PKTUI(canvas2_a3_s0_gram_img_pattern_on, &canvas2_a3_s0_maptbl[GRAM_IMG_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gram_img_pattern_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAM_IMG_PATTERN_ON, 0);
static DEFINE_PKTUI(canvas2_a3_s0_gram_inv_img_pattern_on, &canvas2_a3_s0_maptbl[GRAM_INV_IMG_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_gram_inv_img_pattern_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAM_INV_IMG_PATTERN_ON, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_gram_img_pattern_off, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAM_IMG_PATTERN_OFF, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_gram_dbv_max, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAM_DBV_MAX, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_gram_bctrl_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAM_BCTRL_ON, 0);

#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static DEFINE_STATIC_PACKET(canvas2_a3_s0_ccd_test_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_CCD_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_ccd_test_disable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_CCD_DISABLE, 0);
#endif

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static DEFINE_STATIC_PACKET(canvas2_a3_s0_dynamic_hlpm_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_DYNAMIC_HLPM_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_dynamic_hlpm_disable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_DYNAMIC_HLPM_DISABLE, 0);
#endif

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static DEFINE_STATIC_PACKET(canvas2_a3_s0_cmdlog_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_CMDLOG_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_cmdlog_disable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_CMDLOG_DISABLE, 0);
#endif
static DEFINE_STATIC_PACKET(canvas2_a3_s0_gamma_update_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GAMMA_UPDATE_ENABLE, 0);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_on_00, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_ON_00, 0xCA);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_on_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_ON_01, 0x09);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_on_02, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_ON_02, 0xAA);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_on_03, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_ON_03, 0x08);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_on_04, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_ON_04, 0x03);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_on_05, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_ON_05, 0x17);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_off_00, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_OFF_00, 0xCA);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_off_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_OFF_01, 0x09);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_off_02, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_OFF_02, 0xAA);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_off_03, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_OFF_03, 0x08);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_off_04, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_OFF_04, 0x03);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_off_05, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_OFF_05, 0x17);


#if 0
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_on_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_ON_01, 0x05);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_on_02, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_ON_02, 0x24);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_on_03, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_ON_03, 0x03);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_on_04, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_ON_04, 0x05);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_on_05, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_ON_05, 0x05);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_on_06, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_ON_06, 0x05);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_on_07, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_ON_07, 0x05);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_on_08, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_ON_08, 0x02);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_on_09, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_ON_09, 0x03);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_off_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_OFF_01, 0x03);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_off_02, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_OFF_02, 0x02);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_off_03, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_OFF_03, 0x05);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_off_04, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_OFF_04, 0x05);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_off_05, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_OFF_05, 0x05);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_off_06, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_OFF_06, 0x02);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_off_07, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_OFF_07, 0x05);

static DEFINE_PKTUI(canvas2_a3_s0_mcd_rs_1_right_01, &canvas2_a3_s0_maptbl[MCD_RESISTANCE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_mcd_rs_1_right_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_1_RIGHT_01, 0x07);

static DEFINE_PKTUI(canvas2_a3_s0_mcd_rs_2_right_01, &canvas2_a3_s0_maptbl[MCD_RESISTANCE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_mcd_rs_2_right_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_2_RIGHT_01, 0x08);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_right_02, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_RIGHT_02, 0x03);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_1_right_03, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_1_RIGHT_03, 0x04);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_2_right_03, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_2_RIGHT_03, 0x04);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_right_04, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_RIGHT_04, 0x04);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_right_05, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_RIGHT_05, 0x03);

static DEFINE_PKTUI(canvas2_a3_s0_mcd_rs_1_left_01, &canvas2_a3_s0_maptbl[MCD_RESISTANCE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_mcd_rs_1_left_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_1_LEFT_01, 0x0B);

static DEFINE_PKTUI(canvas2_a3_s0_mcd_rs_2_left_01, &canvas2_a3_s0_maptbl[MCD_RESISTANCE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_mcd_rs_2_left_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_2_LEFT_01, 0x0C);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_left_02, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_LEFT_02, 0x03);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_1_left_03, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_1_LEFT_03, 0x04);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_2_left_03, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_2_LEFT_03, 0x04);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_left_04, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_LEFT_04, 0x04);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mcd_rs_left_05, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MCD_RS_LEFT_05, 0x03);
#endif
#ifdef CONFIG_SUPPORT_MST
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mst_on_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MST_ON_01, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mst_on_02, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MST_ON_02, 0x17);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_mst_off_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MST_OFF_01, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mst_off_02, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MST_OFF_02, 0x17);
#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_on_00, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_ON_00, 0x5A);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_on_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_ON_01, 0x48);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_on_02, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_ON_02, 0x1C);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_on_03, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_ON_03, 0x17);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_on_04, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_ON_04, 0x01);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_on_05, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_ON_05, 0x30);

static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_off_00, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_OFF_00, 0x5A);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_off_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_OFF_01, 0x48);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_off_02, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_OFF_02, 0x1C);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_off_03, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_OFF_03, 0x17);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_grayspot_off_04, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_GRAYSPOT_OFF_04, 0x01);
#endif

#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
static DEFINE_STATIC_PACKET(canvas2_a3_s0_brightdot_on_max_brightness, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_BRIGHTDOT_ON_MAX_BRIGHTNESS, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_brightdot_on_hs_aor, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_BRIGHTDOT_ON_HS_AOR, 0x59);
static DEFINE_COND(canvas2_a3_s0_cond_is_brightdot_enabled, is_brightdot_enabled);
static DEFINE_COND(canvas2_a3_s0_cond_is_brightdot_disabled, is_brightdot_disabled);
#endif

static DEFINE_PANEL_UDELAY(canvas2_a3_s0_wait_1usec, 1);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_20msec, 20);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_50msec, 50);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_124msec, 124);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_sleep_out_10msec, 10);
#ifdef CONFIG_SUPPORT_AFC
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_afc_off, 20);
#endif
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_sleep_in, 120);
#if 0
static DEFINE_PANEL_UDELAY(canvas2_a3_s0_wait_1_frame_in_60hz, 16700);
static DEFINE_PANEL_UDELAY(canvas2_a3_s0_wait_1_frame_in_30hz, 33400);
#endif
static DEFINE_PANEL_VSYNC_DELAY(canvas2_a3_s0_wait_1_vsync, 1);
static DEFINE_PANEL_FRAME_DELAY(canvas2_a3_s0_wait_1_frame, 1);

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_120msec, 120);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_vddm_update, 50);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_20msec_gram_img_update, 20);
static DEFINE_PANEL_MDELAY(canvas2_a3_s0_wait_gram_img_update, 150);
#endif

static DEFINE_PANEL_KEY(canvas2_a3_s0_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(canvas2_a3_s0_level1_key_enable));
static DEFINE_PANEL_KEY(canvas2_a3_s0_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(canvas2_a3_s0_level2_key_enable));
static DEFINE_PANEL_KEY(canvas2_a3_s0_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(canvas2_a3_s0_level3_key_enable));
static DEFINE_PANEL_KEY(canvas2_a3_s0_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(canvas2_a3_s0_level1_key_disable));
static DEFINE_PANEL_KEY(canvas2_a3_s0_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(canvas2_a3_s0_level2_key_disable));
static DEFINE_PANEL_KEY(canvas2_a3_s0_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(canvas2_a3_s0_level3_key_disable));


/* temporary init code: for evt0 panel */

static u8 CANVAS2_A3_S0_E7_PRE[] = {
	0xE7, 0x28
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_e7_pre, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_E7_PRE, 0);

static u8 CANVAS2_A3_S0_F2_OFS_5D[] = {
	0xF2,
	0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x52, 0x42, 0x2A, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x39
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_f2_ofs_5d, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_F2_OFS_5D, 0x5D);

static u8 CANVAS2_A3_S0_F4_OFS_0D[] = {
	0xF4, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_f4_ofs_0d, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_F4_OFS_0D, 0x0D);

static u8 CANVAS2_A3_S0_F2_OFS_23[] = {
	0xF2,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_f2_ofs_23, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_F2_OFS_23, 0x23);

static u8 CANVAS2_A3_S0_B5[] = {
	0xB5, 0x19
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_b5, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_B5, 0);

static u8 CANVAS2_A3_S0_B9[] = {
	0xB9,
	0x00, 0x00, 0x14, 0x00, 0x18, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x40, 0x02, 0x40, 0x02,
	0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_b9, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_B9, 0);

static u8 CANVAS2_A3_S0_CE[] = {
	0xCE,
	0x07, 0x00, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_ce, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_CE, 0);

static u8 CANVAS2_A3_S0_D0[] = {
	0xD0,
	0x00, 0x40, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_d0, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_D0, 0);

static u8 CANVAS2_A3_S0_E5[] = {
	0xE5, 0x13
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_e5, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_E5, 0);

static u8 CANVAS2_A3_S0_E5_OFS_05[] = {
	0xE5,
	0xF8, 0xFC, 0x03, 0x00, 0x01, 0x18
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_e5_ofs_05, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_E5_OFS_05, 0x05);

static u8 CANVAS2_A3_S0_E8[] = {
	0xE8,
	0x00, 0x00, 0x0F, 0xFF, 0xFF, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_e8, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_E8, 0);

static u8 CANVAS2_A3_S0_FD_OFS_01[] = {
	0xFD,
	0x0A, 0x00, 0x90, 0x96, 0xFA, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_fd_ofs_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FD_OFS_01, 0x01);

static u8 CANVAS2_A3_S0_FD_OFS_0A[] = {
	0xFD, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_fd_ofs_0a, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FD_OFS_0A, 0x0A);

static u8 CANVAS2_A3_S0_FD_OFS_10[] = {
	0xFD, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_fd_ofs_10, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FD_OFS_10, 0x10);

static u8 CANVAS2_A3_S0_FD_OFS_31[] = {
	0xFD,
	0x00, 0x00, 0x11, 0x45, 0x11, 0x05, 0x03, 0x02
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_fd_ofs_31, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FD_OFS_31, 0x31);

static u8 CANVAS2_A3_S0_FE[] = {
	0xFE,
	0x30, 0x08, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x10, 0x00, 0x50, 0x00, 0x20
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_fe, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FE, 0);

static u8 CANVAS2_A3_S0_FE_OFS_31[] = {
	0xFE,
	0x00, 0x03, 0x00, 0x10, 0x10, 0x10, 0x10, 0xC8,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
	0xFF, 0xFF, 0x02
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_fe_ofs_31, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FE_OFS_31, 0x31);

static u8 CANVAS2_A3_S0_FE_OFS_46[] = {
	0xFE,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x41, 0x68, 0x00,
	0x78, 0x01, 0x68, 0x01, 0x40, 0x02, 0x8A, 0x02,
	0x3A, 0x00, 0x78, 0x01, 0x40, 0x52, 0x3A, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_fe_ofs_46, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FE_OFS_46, 0x46);

static u8 CANVAS2_A3_S0_FF_OFS_01[] = {
	0xFF,
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_ff_ofs_01, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FF_OFS_01, 0x01);

static u8 CANVAS2_A3_S0_FF_OFS_08[] = {
	0xFF,
	0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_ff_ofs_08, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_FF_OFS_08, 0x08);

static u8 CANVAS2_A3_S0_90_OFS_11[] = {
	0x90,
	0x00, 0x80, 0x2B, 0x05, 0x08, 0x0E, 0x07, 0x0B,
	0x05, 0x0D, 0x0A, 0x15, 0x13, 0x20, 0x1E, 0x1A,
	0x19, 0x2C, 0x27, 0x24, 0x21, 0x3B, 0x33, 0x30,
	0x2C, 0x28, 0x26, 0x23, 0x22, 0x1F, 0x1F, 0x1D,
	0x1F
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_90_ofs_11, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_90_OFS_11, 0x11);

static u8 CANVAS2_A3_S0_91_OFS_50[] = {
	0x91,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_91_ofs_50, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_91_OFS_50, 0x50);

static u8 CANVAS2_A3_S0_93_OFS_0D[] = {
	0x93,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_93_ofs_0d, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_93_OFS_0D, 0x0D);


static u8 CANVAS2_A3_S0_E7[] = {
	0xE7, 0x28, 0x1A, 0x22, 0x08, 0x33
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_e7, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_E7, 0);

static u8 CANVAS2_A3_S0_85[] = {
	0x85, 0x00, 0x80
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_85, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_85, 0);

static u8 CANVAS2_A3_S0_87[] = {
	0x87, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_87, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_87, 0);

static u8 CANVAS2_A3_S0_DD[] = {
	0xDD, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_dd, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_DD, 0);

static u8 CANVAS2_A3_S0_E1[] = {
	0xE1, 0x20
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_e1, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_E1, 0);

static u8 CANVAS2_A3_S0_E3[] = {
	0xE3, 0x01
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_e3, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_E3, 0);

static u8 CANVAS2_A3_S0_WACOM_LTPS_1[] = {
	0xCB,
	0x6C, 0x00, 0x00, 0x00, 0x40, 0x0C, 0x6C
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_wacom_ltps_1, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_WACOM_LTPS_1, 0x60);

static u8 CANVAS2_A3_S0_WACOM_LTPS_2[] = {
	0xCB,
	0x30, 0x00, 0x00, 0x00, 0xD8, 0x38, 0x30, 0x00,
	0x00, 0x00, 0xF8, 0x14, 0x30, 0x00, 0x00, 0x00,
	0xD8, 0x14, 0x30
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_wacom_ltps_2, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_WACOM_LTPS_2, 0x8B);

/* for wacom freq setting: 1202 / 5996 end */


static u8 CANVAS2_A3_S0_F1_KEY_ENABLE[] = {
	0xF1, 0x5A, 0x5A,
};
static u8 CANVAS2_A3_S0_F1_KEY_DISABLE[] = {
	0xF1, 0xA5, 0xA5,
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_f1_key_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_F1_KEY_ENABLE, 0);
static DEFINE_STATIC_PACKET(canvas2_a3_s0_f1_key_disable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_F1_KEY_DISABLE, 0);

static u8 CANVAS2_A3_S0_OSC[] = {
	0xF2,
	0xC3, 0xB4, 0x04, 0x14, 0x01, 0x20, 0x00, 0x10,
	0x10, 0x00, 0x14, 0xBC, 0x20, 0x00, 0x30, 0x0F,
	0xE0
};
static DECLARE_PKTUI(canvas2_a3_s0_osc) = {
	{ .offset = 1, .maptbl = &canvas2_a3_s0_maptbl[OSC_MAPTBL] },
	{ .offset = 1 + S6E3HAC_VFP_NS_OFS, .maptbl = &canvas2_a3_s0_maptbl[VFP_NS_MAPTBL] },
	{ .offset = 1 + S6E3HAC_VFP_HS_OFS, .maptbl = &canvas2_a3_s0_maptbl[VFP_HS_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_osc, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_OSC, 0);

/* only for use rev40~rev51 panel */
static DECLARE_PKTUI(canvas2_a3_s0_rev40_osc) = {
	{ .offset = 1, .maptbl = &canvas2_a3_s0_maptbl[OSC_REV40_MAPTBL] },
	{ .offset = 1 + S6E3HAC_VFP_NS_OFS, .maptbl = &canvas2_a3_s0_maptbl[VFP_NS_LE_REV51_MAPTBL] },
	{ .offset = 1 + S6E3HAC_VFP_HS_OFS, .maptbl = &canvas2_a3_s0_maptbl[VFP_HS_LE_REV51_MAPTBL] },
};
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_rev40_osc, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_OSC, 0);
/* only for use rev40~rev51 panel end */

static u8 CANVAS2_A3_S0_OSC_SEL[] = {
	0x6A,
	0x00, 0x0F,
};
static DEFINE_PKTUI(canvas2_a3_s0_osc_sel, &canvas2_a3_s0_maptbl[OSC_SEL_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_osc_sel, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_OSC_SEL, 0x12);

static u8 CANVAS2_A3_S0_HBM_TRANSITION[] = {
	0x53, 0x21
};

static DEFINE_PKTUI(canvas2_a3_s0_hbm_transition, &canvas2_a3_s0_maptbl[HBM_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_hbm_transition, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_HBM_TRANSITION, 0);

static u8 CANVAS2_A3_S0_ACL[] = {
	0x55, 0x02
};

static DEFINE_PKTUI(canvas2_a3_s0_acl_control, &canvas2_a3_s0_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_acl_control, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_ACL, 0);

static u8 CANVAS2_A3_S0_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(canvas2_a3_s0_wrdisbv, &canvas2_a3_s0_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_wrdisbv, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_WRDISBV, 0);

static u8 CANVAS2_A3_S0_WRDISBV_EXIT_LPM[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(canvas2_a3_s0_wrdisbv_exit_lpm, &canvas2_a3_s0_maptbl[GAMMA_MODE2_EXIT_LPM_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_wrdisbv_exit_lpm, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_WRDISBV_EXIT_LPM, 0);

static u8 CANVAS2_A3_S0_LPM_OFF_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_off_wrdisbv, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_OFF_WRDISBV, 0);

static u8 CANVAS2_A3_S0_LPM_OFF_TRANSITION[] = {
	0x53, 0x29
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_lpm_off_transition, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_LPM_OFF_TRANSITION, 0);

/* CANVAS2_A3_S0_HBM_SETTING_REV52: temporal code for rev52~ panel */
static u8 CANVAS2_A3_S0_HBM_SETTING_REV52[] = {
	0xB4, 0x03
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_hbm_setting_rev52, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_HBM_SETTING_REV52, 0xCA);

static DEFINE_COND(canvas2_a3_s0_cond_is_panel_state_not_lpm, is_panel_state_not_lpm);

#ifdef CONFIG_SUPPORT_MAFPC
static u8 CANVAS2_A3_S0_MAFPC_SR_PATH[] = {
	0x75, 0x40,
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mafpc_sr_path, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MAFPC_SR_PATH, 0);


static u8 CANVAS2_A3_S0_DEFAULT_SR_PATH[] = {
	0x75, 0x01,
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_default_sr_path, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_DEFAULT_SR_PATH, 0);

static DEFINE_STATIC_PACKET_WITH_OPTION(canvas2_mafpc_default_img, DSI_PKT_TYPE_WR_SR,
	S6E3HAC_MAFPC_DEFAULT_IMG, 0, PKT_OPTION_SR_ALIGN_12);

static u8 CANVAS2_A3_S0_MAFPC_ENABLE[] = {
	0x87,
	0x11, 0x09, 0x20, 0x00, 0x00, 0x81, 0x81, 0x81,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xf0, 0xf0, 0xf0,
};
static DEFINE_PKTUI(canvas2_a3_s0_mafpc_enable, &canvas2_a3_s0_maptbl[MAFPC_ENA_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_mafpc_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MAFPC_ENABLE, 0);

static u8 CANVAS2_A3_S0_REV51_MAFPC_ENABLE[] = {
	0x87,
	0x11, 0x09, 0x1C, 0x00, 0x00, 0x81, 0x81, 0x81,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xf0, 0xf0, 0xf0,
};
static DEFINE_PKTUI(canvas2_a3_s0_rev51_mafpc_enable, &canvas2_a3_s0_maptbl[MAFPC_ENA_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_rev51_mafpc_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_REV51_MAFPC_ENABLE, 0);

static u8 CANVAS2_A3_S0_MAFPC_DISABLE[] = {
	0x87, 0x00,
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mafpc_disable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MAFPC_DISABLE, 0);


static u8 CANVAS2_A3_S0_MAFPC_CRC_ON[] = {
	0xD8, 0x15,
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mafpc_crc_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MAFPC_CRC_ON, 0x27);

static u8 CANVAS2_A3_S0_MAFPC_CRC_BIST_ON[] = {
	0xBF,
	0x01, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mafpc_crc_bist_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MAFPC_CRC_BIST_ON, 0x00);


static u8 CANVAS2_A3_S0_MAFPC_CRC_BIST_OFF[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mafpc_crc_bist_off, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MAFPC_CRC_BIST_OFF, 0x00);


static u8 CANVAS2_A3_S0_MAFPC_CRC_ENABLE[] = {
	0x87,
	0x11, 0x09, 0x20, 0x00, 0x00, 0x81, 0x81, 0x81,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xF0, 0xF0, 0xF0
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mafpc_crc_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MAFPC_CRC_ENABLE, 0);

static u8 CANVAS2_A3_S0_REV51_MAFPC_CRC_ENABLE[] = {
	0x87,
	0x11, 0x09, 0x1c, 0x00, 0x00, 0x81, 0x81, 0x81,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xF0, 0xF0, 0xF0
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_rev51_mafpc_crc_enable, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_REV51_MAFPC_CRC_ENABLE, 0);

static u8 CANVAS2_A3_S0_MAFPC_CRC_MDNIE_OFF[] = {
	0xDD,
	0x00
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mafpc_crc_mdnie_off, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MAFPC_CRC_MDNIE_OFF, 0);


static u8 CANVAS2_A3_S0_MAFPC_CRC_MDNIE_ON[] = {
	0xDD,
	0x01
};
static DEFINE_STATIC_PACKET(canvas2_a3_s0_mafpc_crc_mdnie_on, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MAFPC_CRC_MDNIE_ON, 0);

static DEFINE_PANEL_MDELAY(canvas2_a3_s0_crc_wait, 34);

static u8 CANVAS2_A3_S0_MAFPC_SCALE[] = {
	0x87,
	0xFF, 0xFF, 0xFF,
};
static DEFINE_PKTUI(canvas2_a3_s0_mafpc_scale, &canvas2_a3_s0_maptbl[MAFPC_SCALE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(canvas2_a3_s0_mafpc_scale, DSI_PKT_TYPE_WR, CANVAS2_A3_S0_MAFPC_SCALE, 0x08);


static void *canvas2_a3_s0_mafpc_image_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_mafpc_sr_path),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&PKTINFO(canvas2_a3_s0_mafpc_disable),
	&DLYINFO(canvas2_a3_s0_wait_1_frame),
	&PKTINFO(canvas2_mafpc_default_img),
	&PKTINFO(canvas2_a3_s0_default_sr_path),
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_mafpc_on_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_mafpc_enable),
	&PKTINFO(canvas2_a3_s0_mafpc_scale),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_rev51_mafpc_on_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_rev51_mafpc_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_mafpc_off_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_mafpc_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_mafpc_check_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),

	&PKTINFO(canvas2_a3_s0_dummy_2c),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_on),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_bist_on),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_enable),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_mdnie_off),
	&PKTINFO(canvas2_a3_s0_display_on),
	&DLYINFO(canvas2_a3_s0_crc_wait),
	&s6e3hac_restbl[RES_MAFPC_CRC],
	&PKTINFO(canvas2_a3_s0_display_off),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_bist_off),
	&PKTINFO(canvas2_a3_s0_mafpc_disable),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_mdnie_on),
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};

static void *canvas2_a3_s0_rev51_mafpc_check_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),

	&PKTINFO(canvas2_a3_s0_dummy_2c),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_on),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_bist_on),
	&PKTINFO(canvas2_a3_s0_rev51_mafpc_crc_enable),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_mdnie_off),
	&PKTINFO(canvas2_a3_s0_display_on),
	&DLYINFO(canvas2_a3_s0_crc_wait),
	&s6e3hac_restbl[RES_MAFPC_CRC],
	&PKTINFO(canvas2_a3_s0_display_off),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_bist_off),
	&PKTINFO(canvas2_a3_s0_mafpc_disable),
	&PKTINFO(canvas2_a3_s0_mafpc_crc_mdnie_on),
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};




static DEFINE_PANEL_TIMER_MDELAY(canvas2_a3_s0_mafpc_delay, 120);
static DEFINE_PANEL_TIMER_BEGIN(canvas2_a3_s0_mafpc_delay,
		TIMER_DLYINFO(&canvas2_a3_s0_mafpc_delay));
#endif

static struct seqinfo SEQINFO(canvas2_a3_s0_set_bl_param_seq);
static struct seqinfo SEQINFO(canvas2_a3_s0_rev40_rev50_set_bl_param_seq);
static struct seqinfo SEQINFO(canvas2_a3_s0_rev40_set_fps_param_seq);
static struct seqinfo SEQINFO(canvas2_a3_s0_set_fps_param_seq);

static void *canvas2_a3_s0_err_fg_cmdtbl[] = {
#ifdef CONFIG_SEC_FACTORY
	&PKTINFO(canvas2_a3_s0_err_fg_1),
	&PKTINFO(canvas2_a3_s0_err_fg_2),
#else
	&PKTINFO(canvas2_a3_s0_err_fg),
#endif
};
static DEFINE_SEQINFO(canvas2_a3_s0_err_fg_seq, canvas2_a3_s0_err_fg_cmdtbl);


static void *canvas2_a3_s0_rev40_init_cmdtbl[] = {
	&DLYINFO(canvas2_a3_s0_wait_sleep_out_10msec),
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_dsc),
	&PKTINFO(canvas2_a3_s0_pps),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_dummy_2c),
	&PKTINFO(canvas2_a3_s0_sleep_out),
	&DLYINFO(canvas2_a3_s0_wait_sleep_out_10msec),

	&PKTINFO(canvas2_a3_s0_scaler),
	&PKTINFO(canvas2_a3_s0_caset),
	&PKTINFO(canvas2_a3_s0_paset),

/* temporary init code : for evt0 panel */
	&PKTINFO(canvas2_a3_s0_f1_key_enable),
	&PKTINFO(canvas2_a3_s0_e7_pre),
	&PKTINFO(canvas2_a3_s0_f2_ofs_5d),
	&PKTINFO(canvas2_a3_s0_f4_ofs_0d),
	&PKTINFO(canvas2_a3_s0_f2_ofs_23),
	&PKTINFO(canvas2_a3_s0_b5),
	&PKTINFO(canvas2_a3_s0_b9),
	&PKTINFO(canvas2_a3_s0_ce),
	&PKTINFO(canvas2_a3_s0_d0),
	&PKTINFO(canvas2_a3_s0_e5),
	&PKTINFO(canvas2_a3_s0_e5_ofs_05),
	&PKTINFO(canvas2_a3_s0_e8),
	&PKTINFO(canvas2_a3_s0_fd_ofs_01),
	&PKTINFO(canvas2_a3_s0_fd_ofs_0a),
	&PKTINFO(canvas2_a3_s0_fd_ofs_10),
	&PKTINFO(canvas2_a3_s0_fd_ofs_31),
	&PKTINFO(canvas2_a3_s0_fe),
	&PKTINFO(canvas2_a3_s0_fe_ofs_31),
	&PKTINFO(canvas2_a3_s0_fe_ofs_46),
	&PKTINFO(canvas2_a3_s0_ff_ofs_01),
	&PKTINFO(canvas2_a3_s0_ff_ofs_08),
	&PKTINFO(canvas2_a3_s0_90_ofs_11),
	&PKTINFO(canvas2_a3_s0_91_ofs_50),
	&PKTINFO(canvas2_a3_s0_93_ofs_0d),

	&PKTINFO(canvas2_a3_s0_e7),
	&PKTINFO(canvas2_a3_s0_85),
	&PKTINFO(canvas2_a3_s0_87),
	&PKTINFO(canvas2_a3_s0_dd),
	&PKTINFO(canvas2_a3_s0_e1),
	&PKTINFO(canvas2_a3_s0_e3),
	&PKTINFO(canvas2_a3_s0_f1_key_disable),
/* temporary init code : for evt0 panel end */

	&PKTINFO(canvas2_a3_s0_ffc_default),
	&PKTINFO(canvas2_a3_s0_tsp_hsync),
	&PKTINFO(canvas2_a3_s0_wacom_ltps_1),
	&PKTINFO(canvas2_a3_s0_wacom_ltps_2),
	&PKTINFO(canvas2_a3_s0_etc_set_slpout),
	&SEQINFO(canvas2_a3_s0_err_fg_seq),

	&PKTINFO(canvas2_a3_s0_dia_onoff),
	&PKTINFO(canvas2_a3_s0_dither),
	&PKTINFO(canvas2_a3_s0_smooth_dimming),
	&PKTINFO(canvas2_a3_s0_lpm_flicker_set_1),
	&PKTINFO(canvas2_a3_s0_lpm_flicker_set_2),

	/* lfd setting */
	&SEQINFO(canvas2_a3_s0_rev40_set_fps_param_seq),

	/* set brightness & fps */
	&SEQINFO(canvas2_a3_s0_rev40_rev50_set_bl_param_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),

#ifdef CONFIG_SUPPORT_MAFPC
	&TIMER_DLYINFO_BEGIN(canvas2_a3_s0_mafpc_delay),
	&PKTINFO(canvas2_a3_s0_mafpc_sr_path),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&PKTINFO(canvas2_mafpc_default_img),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&PKTINFO(canvas2_a3_s0_default_sr_path),
	&TIMER_DLYINFO(canvas2_a3_s0_mafpc_delay),
#endif
	&PKTINFO(canvas2_a3_s0_te_on),

	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	&SEQINFO(canvas2_a3_s0_copr_seqtbl[COPR_SET_SEQ]),
#endif
};

static void *canvas2_a3_s0_rev50_init_cmdtbl[] = {
	&DLYINFO(canvas2_a3_s0_wait_sleep_out_10msec),
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_dsc),
	&PKTINFO(canvas2_a3_s0_pps),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_dummy_2c),
	&PKTINFO(canvas2_a3_s0_sleep_out),
	&DLYINFO(canvas2_a3_s0_wait_sleep_out_10msec),

#ifdef CONFIG_SUPPORT_MAFPC
	&TIMER_DLYINFO_BEGIN(canvas2_a3_s0_mafpc_delay),
	&PKTINFO(canvas2_a3_s0_mafpc_sr_path),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&PKTINFO(canvas2_mafpc_default_img),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&PKTINFO(canvas2_a3_s0_default_sr_path),
	&TIMER_DLYINFO(canvas2_a3_s0_mafpc_delay),
#endif

	&PKTINFO(canvas2_a3_s0_scaler),
	&PKTINFO(canvas2_a3_s0_caset),
	&PKTINFO(canvas2_a3_s0_paset),

	&PKTINFO(canvas2_a3_s0_ffc_default),
	&PKTINFO(canvas2_a3_s0_tsp_hsync),
	&PKTINFO(canvas2_a3_s0_wacom_ltps_1),
	&PKTINFO(canvas2_a3_s0_wacom_ltps_2),
	&PKTINFO(canvas2_a3_s0_etc_set_slpout),
	&SEQINFO(canvas2_a3_s0_err_fg_seq),
	&PKTINFO(canvas2_a3_s0_dia_onoff),
	&PKTINFO(canvas2_a3_s0_dither),
	&PKTINFO(canvas2_a3_s0_smooth_dimming),
	&PKTINFO(canvas2_a3_s0_lpm_flicker_set_1),
	&PKTINFO(canvas2_a3_s0_lpm_flicker_set_2),

	/* lfd setting */
	&SEQINFO(canvas2_a3_s0_rev40_set_fps_param_seq),

	/* set brightness & fps */
	&SEQINFO(canvas2_a3_s0_rev40_rev50_set_bl_param_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),

	&PKTINFO(canvas2_a3_s0_te_on),

	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	&SEQINFO(canvas2_a3_s0_copr_seqtbl[COPR_SET_SEQ]),
#endif
};

static void *canvas2_a3_s0_init_cmdtbl[] = {
	&DLYINFO(canvas2_a3_s0_wait_sleep_out_10msec),
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_dsc),
	&PKTINFO(canvas2_a3_s0_pps),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_dummy_2c),
	&PKTINFO(canvas2_a3_s0_sleep_out),
	&DLYINFO(canvas2_a3_s0_wait_sleep_out_10msec),

#ifdef CONFIG_SUPPORT_MAFPC
	&TIMER_DLYINFO_BEGIN(canvas2_a3_s0_mafpc_delay),
	&PKTINFO(canvas2_a3_s0_mafpc_sr_path),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&PKTINFO(canvas2_mafpc_default_img),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&PKTINFO(canvas2_a3_s0_default_sr_path),
	&TIMER_DLYINFO(canvas2_a3_s0_mafpc_delay),
#endif

	&PKTINFO(canvas2_a3_s0_scaler),
	&PKTINFO(canvas2_a3_s0_caset),
	&PKTINFO(canvas2_a3_s0_paset),

	&PKTINFO(canvas2_a3_s0_ffc_default),
	&PKTINFO(canvas2_a3_s0_tsp_hsync),
	&PKTINFO(canvas2_a3_s0_etc_set_slpout),
	&SEQINFO(canvas2_a3_s0_err_fg_seq),
	&PKTINFO(canvas2_a3_s0_dia_onoff),
	&PKTINFO(canvas2_a3_s0_dither),
	&PKTINFO(canvas2_a3_s0_smooth_dimming),
	&PKTINFO(canvas2_a3_s0_lpm_flicker_set_1),
	&PKTINFO(canvas2_a3_s0_lpm_flicker_set_2),

	/* lfd setting */
	&SEQINFO(canvas2_a3_s0_set_fps_param_seq),

	/* set brightness & fps */
	&SEQINFO(canvas2_a3_s0_set_bl_param_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),

	&PKTINFO(canvas2_a3_s0_te_on),

	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	&SEQINFO(canvas2_a3_s0_copr_seqtbl[COPR_SET_SEQ]),
#endif
};

static void *canvas2_a3_s0_res_init_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&s6e3hac_restbl[RES_COORDINATE],
	&s6e3hac_restbl[RES_CODE],
	&s6e3hac_restbl[RES_DATE],
	&s6e3hac_restbl[RES_OCTA_ID],
	&s6e3hac_restbl[RES_VBIAS1],
	&s6e3hac_restbl[RES_VBIAS2],
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e3hac_restbl[RES_CHIP_ID],
	&s6e3hac_restbl[RES_SELF_DIAG],
	&s6e3hac_restbl[RES_ERR_FG],
	&s6e3hac_restbl[RES_DSI_ERR],
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
	&s6e3hac_restbl[RES_POC_CHKSUM],
#endif
	&s6e3hac_restbl[RES_GAMMA_MTP_0_NS],
	&s6e3hac_restbl[RES_GAMMA_MTP_1_NS],
	&s6e3hac_restbl[RES_GAMMA_MTP_2_NS],
	&s6e3hac_restbl[RES_GAMMA_MTP_3_NS],
	&s6e3hac_restbl[RES_GAMMA_MTP_4_NS],
	&s6e3hac_restbl[RES_GAMMA_MTP_5_NS],
	&s6e3hac_restbl[RES_GAMMA_MTP_6_NS],
	&s6e3hac_restbl[RES_GAMMA_MTP_0_HS],
	&s6e3hac_restbl[RES_GAMMA_MTP_1_HS],
	&s6e3hac_restbl[RES_GAMMA_MTP_2_HS],
	&s6e3hac_restbl[RES_GAMMA_MTP_3_HS],
	&s6e3hac_restbl[RES_GAMMA_MTP_4_HS],
	&s6e3hac_restbl[RES_GAMMA_MTP_5_HS],
	&s6e3hac_restbl[RES_GAMMA_MTP_6_HS],
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};

static void *canvas2_a3_s0_wrdisbv_param_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	&CONDINFO_S(canvas2_a3_s0_cond_is_brightdot_enabled),
	&PKTINFO(canvas2_a3_s0_brightdot_on_max_brightness),
	&CONDINFO_E(canvas2_a3_s0_cond_is_brightdot_enabled),
	&CONDINFO_S(canvas2_a3_s0_cond_is_brightdot_disabled),
	&PKTINFO(canvas2_a3_s0_wrdisbv),
	&CONDINFO_E(canvas2_a3_s0_cond_is_brightdot_disabled),
#else
	&PKTINFO(canvas2_a3_s0_wrdisbv),
#endif
};
static DEFINE_SEQINFO(canvas2_a3_s0_wrdisbv_param_seq, canvas2_a3_s0_wrdisbv_param_cmdtbl);

/* set_bl_param for latest(rev52~)*/
static void *canvas2_a3_s0_set_bl_param_cmdtbl[] = {
	&PKTINFO(canvas2_a3_s0_hbm_transition),
	&PKTINFO(canvas2_a3_s0_acl_control),
	&PKTINFO(canvas2_a3_s0_tset_elvss),
	&PKTINFO(canvas2_a3_s0_hbm_setting_rev52),
	&PKTINFO(canvas2_a3_s0_vbias),
	&SEQINFO(canvas2_a3_s0_wrdisbv_param_seq),
	&PKTINFO(canvas2_a3_s0_irc_mode),
	&PKTINFO(canvas2_a3_s0_irc_temporary),
#ifdef CONFIG_SUPPORT_XTALK_MODE
	&PKTINFO(canvas2_a3_s0_xtalk_vgh),
#endif
};
static DEFINE_SEQINFO(canvas2_a3_s0_set_bl_param_seq, canvas2_a3_s0_set_bl_param_cmdtbl);
/********************************************************************/

/* set_bl_param for rev40~rev51 */
static void *canvas2_a3_s0_rev40_rev50_set_bl_param_cmdtbl[] = {
	&PKTINFO(canvas2_a3_s0_hbm_transition),
	&PKTINFO(canvas2_a3_s0_acl_control),
	&PKTINFO(canvas2_a3_s0_tset_elvss),
	&PKTINFO(canvas2_a3_s0_vbias_rev40_rev50),
	&SEQINFO(canvas2_a3_s0_wrdisbv_param_seq),
	&PKTINFO(canvas2_a3_s0_irc_mode),
#ifdef CONFIG_SUPPORT_XTALK_MODE
	&PKTINFO(canvas2_a3_s0_xtalk_vgh),
#endif
};
static DEFINE_SEQINFO(canvas2_a3_s0_rev40_rev50_set_bl_param_seq, canvas2_a3_s0_rev40_rev50_set_bl_param_cmdtbl);
/********************************************************************/

/* set_bl cmd for latest */
static void *canvas2_a3_s0_set_bl_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&SEQINFO(canvas2_a3_s0_set_bl_param_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
#ifdef CONFIG_SUPPORT_MAFPC
	&PKTINFO(canvas2_a3_s0_mafpc_scale),
#endif
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};
/********************************************************************/

/* set_bl cmd for rev40~rev50 */
static void *canvas2_a3_s0_rev40_rev50_set_bl_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&SEQINFO(canvas2_a3_s0_rev40_rev50_set_bl_param_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
#ifdef CONFIG_SUPPORT_MAFPC
	&PKTINFO(canvas2_a3_s0_mafpc_scale),
#endif
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};
/********************************************************************/


static void *canvas2_a3_s0_set_gamma_param_cmdtbl[] = {
	/* gamma mtp setting */
	&PKTINFO(canvas2_a3_s0_gamma_mtp_0_ns),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_1_ns),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_2_ns),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_3_ns),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_4_ns),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_5_ns),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_6_ns),

	&PKTINFO(canvas2_a3_s0_gamma_mtp_0_hs),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_1_hs),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_2_hs),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_3_hs),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_4_hs),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_5_hs),
	&PKTINFO(canvas2_a3_s0_gamma_mtp_6_hs),
};
static DEFINE_SEQINFO(canvas2_a3_s0_set_gamma_param_seq,
		canvas2_a3_s0_set_gamma_param_cmdtbl);


static void *canvas2_a3_s0_set_hs_aor_param_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	&CONDINFO_S(canvas2_a3_s0_cond_is_brightdot_enabled),
	&PKTINFO(canvas2_a3_s0_brightdot_on_hs_aor),
	&CONDINFO_E(canvas2_a3_s0_cond_is_brightdot_enabled),
	&CONDINFO_S(canvas2_a3_s0_cond_is_brightdot_disabled),
	&PKTINFO(canvas2_a3_s0_aor_hs),
	&CONDINFO_E(canvas2_a3_s0_cond_is_brightdot_disabled),
#else
	&PKTINFO(canvas2_a3_s0_aor_hs),
#endif
};
static DEFINE_SEQINFO(canvas2_a3_s0_set_hs_aor_param_seq, canvas2_a3_s0_set_hs_aor_param_cmdtbl);

/* set_fps_param for latest */
static void *canvas2_a3_s0_set_fps_param_cmdtbl[] = {
	/* enable h/w te modulation if necessary */
	&PKTINFO(canvas2_a3_s0_set_te_frame_sel),
	/* fps & osc setting */
	&PKTINFO(canvas2_a3_s0_lfd_setting),
	&PKTINFO(canvas2_a3_s0_lfd_on),
	/*
	 * TODO
	 * don't tx gamma compensation command
	 * if old and new gamma command set are same.
	 */
	&SEQINFO(canvas2_a3_s0_set_gamma_param_seq),
	&PKTINFO(canvas2_a3_s0_osc),
	&PKTINFO(canvas2_a3_s0_osc_sel),
	&PKTINFO(canvas2_a3_s0_vtotal_ns_0),
	&PKTINFO(canvas2_a3_s0_vtotal_ns_1),
	&PKTINFO(canvas2_a3_s0_vtotal_hs_0),
	&PKTINFO(canvas2_a3_s0_vtotal_hs_1),
	&PKTINFO(canvas2_a3_s0_vfp_opt_ns),
	&PKTINFO(canvas2_a3_s0_vfp_opt_hs),
	&PKTINFO(canvas2_a3_s0_aor_ns),
	&SEQINFO(canvas2_a3_s0_set_hs_aor_param_seq),
	&PKTINFO(canvas2_a3_s0_fps),
};
static DEFINE_SEQINFO(canvas2_a3_s0_set_fps_param_seq,
		canvas2_a3_s0_set_fps_param_cmdtbl);
/********************************************************************/


static void *canvas2_a3_s0_set_hs_aor_param_le_rev51_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	&CONDINFO_S(canvas2_a3_s0_cond_is_brightdot_enabled),
	&PKTINFO(canvas2_a3_s0_brightdot_on_hs_aor),
	&CONDINFO_E(canvas2_a3_s0_cond_is_brightdot_enabled),
	&CONDINFO_S(canvas2_a3_s0_cond_is_brightdot_disabled),
	&PKTINFO(canvas2_a3_s0_aor_hs_le_rev51),
	&CONDINFO_E(canvas2_a3_s0_cond_is_brightdot_disabled),
#else
	&PKTINFO(canvas2_a3_s0_aor_hs_le_rev51),
#endif
};
static DEFINE_SEQINFO(canvas2_a3_s0_set_hs_aor_param_le_rev51_seq, canvas2_a3_s0_set_hs_aor_param_le_rev51_cmdtbl);

/* set_fps cmd for rev40~rev51 */
static void *canvas2_a3_s0_rev40_set_fps_param_cmdtbl[] = {
	/* enable h/w te modulation if necessary */
	&PKTINFO(canvas2_a3_s0_set_te_frame_sel),
	/* fps & osc setting */
	&PKTINFO(canvas2_a3_s0_lfd_setting),
	&PKTINFO(canvas2_a3_s0_lfd_on),
	/*
	 * TODO
	 * don't tx gamma compensation command
	 * if old and new gamma command set are same.
	 */
	&SEQINFO(canvas2_a3_s0_set_gamma_param_seq),
	&PKTINFO(canvas2_a3_s0_rev40_osc),
	&PKTINFO(canvas2_a3_s0_osc_sel),
	&PKTINFO(canvas2_a3_s0_vtotal_ns_0_le_rev51),
	&PKTINFO(canvas2_a3_s0_vtotal_ns_1_le_rev51),
	&PKTINFO(canvas2_a3_s0_vtotal_hs_0_le_rev51),
	&PKTINFO(canvas2_a3_s0_vtotal_hs_1_le_rev51),
	&PKTINFO(canvas2_a3_s0_vfp_opt_ns_le_rev51),
	&PKTINFO(canvas2_a3_s0_vfp_opt_hs_le_rev51),
	&PKTINFO(canvas2_a3_s0_aor_ns_le_rev51),
	&SEQINFO(canvas2_a3_s0_set_hs_aor_param_le_rev51_seq),
	&PKTINFO(canvas2_a3_s0_fps),
};
static DEFINE_SEQINFO(canvas2_a3_s0_rev40_set_fps_param_seq,
		canvas2_a3_s0_rev40_set_fps_param_cmdtbl);
/********************************************************************/


/* set_fps_cmd for latest */
static void *canvas2_a3_s0_set_fps_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&SEQINFO(canvas2_a3_s0_set_bl_param_seq),
	&DLYINFO(canvas2_a3_s0_wait_1_vsync),
	&SEQINFO(canvas2_a3_s0_set_fps_param_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static void *canvas2_a3_s0_display_mode_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&CONDINFO_S(canvas2_a3_s0_cond_is_panel_state_not_lpm),
	&SEQINFO(canvas2_a3_s0_set_bl_param_seq),
	&DLYINFO(canvas2_a3_s0_wait_1_vsync),
	&CONDINFO_E(canvas2_a3_s0_cond_is_panel_state_not_lpm),
#ifdef CONFIG_SUPPORT_DSU
	&PKTINFO(canvas2_a3_s0_dsc),
	&PKTINFO(canvas2_a3_s0_pps),
	&PKTINFO(canvas2_a3_s0_caset),
	&PKTINFO(canvas2_a3_s0_paset),
	&PKTINFO(canvas2_a3_s0_scaler),
#endif
	&CONDINFO_S(canvas2_a3_s0_cond_is_panel_state_not_lpm),
	&SEQINFO(canvas2_a3_s0_set_fps_param_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&CONDINFO_E(canvas2_a3_s0_cond_is_panel_state_not_lpm),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};
#endif
/********************************************************************/

/* set_fps cmd for rev40 ~ rev51 */
static void *canvas2_a3_s0_rev40_set_fps_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&SEQINFO(canvas2_a3_s0_rev40_rev50_set_bl_param_seq),
	&DLYINFO(canvas2_a3_s0_wait_1_vsync),
	&SEQINFO(canvas2_a3_s0_rev40_set_fps_param_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static void *canvas2_a3_s0_rev40_display_mode_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&CONDINFO_S(canvas2_a3_s0_cond_is_panel_state_not_lpm),
	&SEQINFO(canvas2_a3_s0_rev40_rev50_set_bl_param_seq),
	&DLYINFO(canvas2_a3_s0_wait_1_vsync),
	&CONDINFO_E(canvas2_a3_s0_cond_is_panel_state_not_lpm),
#ifdef CONFIG_SUPPORT_DSU
	&PKTINFO(canvas2_a3_s0_dsc),
	&PKTINFO(canvas2_a3_s0_pps),
	&PKTINFO(canvas2_a3_s0_caset),
	&PKTINFO(canvas2_a3_s0_paset),
	&PKTINFO(canvas2_a3_s0_scaler),
#endif
	&CONDINFO_S(canvas2_a3_s0_cond_is_panel_state_not_lpm),
	&SEQINFO(canvas2_a3_s0_rev40_set_fps_param_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&CONDINFO_E(canvas2_a3_s0_cond_is_panel_state_not_lpm),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};
#endif
/********************************************************************/

static void *canvas2_a3_s0_display_on_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_MAFPC
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_mafpc_enable),
	&PKTINFO(canvas2_a3_s0_mafpc_scale),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
#endif
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&PKTINFO(canvas2_a3_s0_display_on),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};

static void *canvas2_a3_s0_display_off_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_lfd_mode_display_off),
	&PKTINFO(canvas2_a3_s0_vaint_set_display_off),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&PKTINFO(canvas2_a3_s0_display_off),
	&DLYINFO(canvas2_a3_s0_wait_20msec),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};

static void *canvas2_a3_s0_exit_cmdtbl[] = {
#ifdef CONFIG_SUPPORT_AFC
	&SEQINFO(canvas2_a3_s0_mdnie_seqtbl[MDNIE_AFC_OFF_SEQ]),
	&DLYINFO(canvas2_a3_s0_wait_afc_off),
#endif
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
#ifdef CONFIG_DISPLAY_USE_INFO
	&s6e3hac_dmptbl[DUMP_ERR_FG],
	&s6e3hac_dmptbl[DUMP_DSI_ERR],
	&s6e3hac_dmptbl[DUMP_SELF_DIAG],
#endif
	&PKTINFO(canvas2_a3_s0_sleep_in),
	&DLYINFO(canvas2_a3_s0_wait_10msec),
	&PKTINFO(canvas2_a3_s0_etc_set_slpin),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
	&DLYINFO(canvas2_a3_s0_wait_sleep_in),
};

static void *canvas2_a3_s0_alpm_enter_delay_cmdtbl[] = {
	&DLYINFO(canvas2_a3_s0_wait_124msec),
};

static void *canvas2_a3_s0_alpm_enter_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	/* disable h/w te modulation */
	&PKTINFO(canvas2_a3_s0_clr_te_frame_sel),
	&PKTINFO(canvas2_a3_s0_lpm_enter_mode),
	&PKTINFO(canvas2_a3_s0_lpm_nit),
	/* execute if normal->lpm + */
	&CONDINFO_S(canvas2_a3_s0_cond_is_panel_state_not_lpm),
	&PKTINFO(canvas2_a3_s0_lpm_vbias_flicker_set),
	&CONDINFO_E(canvas2_a3_s0_cond_is_panel_state_not_lpm),
	/* execute if normal->lpm - */
	&PKTINFO(canvas2_a3_s0_lpm_vaint_on),
	&PKTINFO(canvas2_a3_s0_lpm_lfd),
	&PKTINFO(canvas2_a3_s0_lpm_vaint_on),
	&PKTINFO(canvas2_a3_s0_lpm_on),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	/* execute if normal->lpm + */
	&CONDINFO_S(canvas2_a3_s0_cond_is_panel_state_not_lpm),
	&DLYINFO(canvas2_a3_s0_wait_100msec),
	&CONDINFO_E(canvas2_a3_s0_cond_is_panel_state_not_lpm),
	/* execute if normal->lpm - */
	&PKTINFO(canvas2_a3_s0_lpm_vbias),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&DLYINFO(canvas2_a3_s0_wait_1_frame),
};

static void *canvas2_a3_s0_alpm_exit_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_lpm_off_aswire_off),
	&PKTINFO(canvas2_a3_s0_lpm_exit_mode),
	&PKTINFO(canvas2_a3_s0_lpm_vbias_flicker_exit),
	&PKTINFO(canvas2_a3_s0_lfd_setting),
	&DLYINFO(canvas2_a3_s0_wait_50msec),
	&PKTINFO(canvas2_a3_s0_hbm_transition),
	&PKTINFO(canvas2_a3_s0_tset_elvss),
	&PKTINFO(canvas2_a3_s0_wrdisbv_exit_lpm),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&DLYINFO(canvas2_a3_s0_wait_40msec),
	&PKTINFO(canvas2_a3_s0_lpm_vaint_off),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_rev52_alpm_enter_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	/* disable h/w te modulation */
	&PKTINFO(canvas2_a3_s0_clr_te_frame_sel),
	&PKTINFO(canvas2_a3_s0_lpm_enter_mode),
	&PKTINFO(canvas2_a3_s0_lpm_nit),
	&PKTINFO(canvas2_a3_s0_lpm_vbias),
	&PKTINFO(canvas2_a3_s0_lpm_vaint_on),
	&PKTINFO(canvas2_a3_s0_lpm_lfd),
	&PKTINFO(canvas2_a3_s0_lpm_vaint_on),
	&PKTINFO(canvas2_a3_s0_lpm_on_ltps_1),
	&PKTINFO(canvas2_a3_s0_lpm_on_ltps_2),
	&PKTINFO(canvas2_a3_s0_lpm_on),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&DLYINFO(canvas2_a3_s0_wait_1_frame),
};

static void *canvas2_a3_s0_rev52_alpm_exit_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_lpm_off_aswire_off),
	&PKTINFO(canvas2_a3_s0_lpm_exit_mode),
	&PKTINFO(canvas2_a3_s0_vbias),
	&PKTINFO(canvas2_a3_s0_lpm_vaint_off),

	&PKTINFO(canvas2_a3_s0_lfd_setting),
	&PKTINFO(canvas2_a3_s0_lpm_vaint_off),
	&PKTINFO(canvas2_a3_s0_lpm_off_ltps_1),
	&PKTINFO(canvas2_a3_s0_lpm_off_ltps_2),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),

	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_rev50_alpm_enter_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	/* disable h/w te modulation */
	&PKTINFO(canvas2_a3_s0_clr_te_frame_sel),
	/* guided for SDC */
	&PKTINFO(canvas2_a3_s0_wacom_restore_1),
	&PKTINFO(canvas2_a3_s0_wacom_restore_2),
	&PKTINFO(canvas2_a3_s0_wacom_restore_3),
	&PKTINFO(canvas2_a3_s0_wacom_restore_4),
	&PKTINFO(canvas2_a3_s0_lpm_on_ltps_3),
	&PKTINFO(canvas2_a3_s0_lpm_on_ltps_4),
	&PKTINFO(canvas2_a3_s0_lpm_on_ltps_5),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	/* guided for SDC end */

	&PKTINFO(canvas2_a3_s0_lpm_enter_mode),
	&PKTINFO(canvas2_a3_s0_lpm_nit),
	&PKTINFO(canvas2_a3_s0_lpm_vbias),
	&PKTINFO(canvas2_a3_s0_lpm_lfd),
	&PKTINFO(canvas2_a3_s0_lpm_vaint_on),
	&PKTINFO(canvas2_a3_s0_lpm_on_ltps_1),
	&PKTINFO(canvas2_a3_s0_lpm_on_ltps_2),
	&PKTINFO(canvas2_a3_s0_lpm_on),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&DLYINFO(canvas2_a3_s0_wait_1_frame),
};

static void *canvas2_a3_s0_rev50_alpm_exit_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_lpm_off_aswire_off),
	&PKTINFO(canvas2_a3_s0_lpm_exit_mode),
	&PKTINFO(canvas2_a3_s0_vbias_rev40_rev50),
	&PKTINFO(canvas2_a3_s0_lfd_setting),
	&PKTINFO(canvas2_a3_s0_lpm_vaint_off),
	&PKTINFO(canvas2_a3_s0_lpm_off_ltps_1),
	&PKTINFO(canvas2_a3_s0_lpm_off_ltps_2),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	/* guided for SDC */
	/* send dummy brightness */
	&PKTINFO(canvas2_a3_s0_lpm_off_transition),
	&PKTINFO(canvas2_a3_s0_tset_elvss),
	&PKTINFO(canvas2_a3_s0_lpm_off_wrdisbv),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	/* wacom porch */
	&PKTINFO(canvas2_a3_s0_rev40_osc),
	&PKTINFO(canvas2_a3_s0_wacom_ltps_1),
	&PKTINFO(canvas2_a3_s0_wacom_ltps_2),
	/* send ltps w/a */
	&PKTINFO(canvas2_a3_s0_lpm_off_ltps_3),
	&PKTINFO(canvas2_a3_s0_lpm_off_ltps_4),
	&PKTINFO(canvas2_a3_s0_lpm_off_ltps_5),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	/* guided for SDC end */
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

/* support for 40 panel */
static void *canvas2_a3_s0_rev40_alpm_enter_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	/* disable h/w te modulation */
	&PKTINFO(canvas2_a3_s0_clr_te_frame_sel),
	&PKTINFO(canvas2_a3_s0_lpm_enter_mode),
	&PKTINFO(canvas2_a3_s0_rev40_lpm_nit),
	&PKTINFO(canvas2_a3_s0_rev40_lpm_vbias),
	&PKTINFO(canvas2_a3_s0_lpm_vaint_on),
	&PKTINFO(canvas2_a3_s0_lpm_on),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&DLYINFO(canvas2_a3_s0_wait_1_frame),
};

static void *canvas2_a3_s0_rev40_alpm_exit_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_lpm_off_aswire_off),
	&PKTINFO(canvas2_a3_s0_lpm_exit_mode),
	&PKTINFO(canvas2_a3_s0_lpm_vaint_off),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&DLYINFO(canvas2_a3_s0_wait_1usec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};
/* support for 4x panel end*/

static void *canvas2_a3_s0_dia_onoff_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_dia_onoff),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_check_condition_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&s6e3hac_dmptbl[DUMP_RDDPM],
#ifdef CONFIG_SUPPORT_MAFPC
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&s6e3hac_dmptbl[DUMP_MAFPC],
	&s6e3hac_dmptbl[DUMP_MAFPC_FLASH],
	&s6e3hac_dmptbl[DUMP_SELF_MASK_CRC],
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
#endif
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

#ifdef CONFIG_DYNAMIC_FREQ
static void *canvas2_a3_s0_dynamic_ffc_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_ffc),
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_DSU
static void *canvas2_a3_s0_dsu_mode_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_dsc),
	&PKTINFO(canvas2_a3_s0_pps),
	&PKTINFO(canvas2_a3_s0_caset),
	&PKTINFO(canvas2_a3_s0_paset),
	&PKTINFO(canvas2_a3_s0_scaler),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};
#endif

static void *canvas2_a3_s0_mcd_on_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_mcd_on_00),
	&PKTINFO(canvas2_a3_s0_mcd_on_01),
	&PKTINFO(canvas2_a3_s0_mcd_on_02),
	&PKTINFO(canvas2_a3_s0_mcd_on_03),
	&PKTINFO(canvas2_a3_s0_mcd_on_04),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&PKTINFO(canvas2_a3_s0_mcd_on_05),
	&DLYINFO(canvas2_a3_s0_wait_100msec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_mcd_off_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_mcd_off_00),
	&PKTINFO(canvas2_a3_s0_mcd_off_01),
	&PKTINFO(canvas2_a3_s0_mcd_off_02),
	&PKTINFO(canvas2_a3_s0_mcd_off_03),
	&PKTINFO(canvas2_a3_s0_mcd_off_04),
	&PKTINFO(canvas2_a3_s0_mcd_off_05),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&DLYINFO(canvas2_a3_s0_wait_100msec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

#if 0
static void *canvas2_a3_s0_mcd_rs_on_cmdtbl[] = {
	&PKTINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_level1_key_enable),
	&PKTINFO(canvas2_a3_s0_display_off),
	&PKTINFO(canvas2_a3_s0_level1_key_disable),
	&PKTINFO(canvas2_a3_s0_mcd_rs_on_01),
	&PKTINFO(canvas2_a3_s0_mcd_rs_on_02),
	&PKTINFO(canvas2_a3_s0_mcd_rs_on_03),
	&PKTINFO(canvas2_a3_s0_mcd_rs_on_04),
	&PKTINFO(canvas2_a3_s0_mcd_rs_on_05),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_mcd_rs_on_06),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_mcd_rs_on_07),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_mcd_rs_on_08),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_mcd_rs_on_09),
};

static void *canvas2_a3_s0_mcd_rs_read_cmdtbl[] = {
	&PKTINFO(canvas2_a3_s0_mcd_rs_1_right_01),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	&PKTINFO(canvas2_a3_s0_mcd_rs_2_right_01),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	&PKTINFO(canvas2_a3_s0_mcd_rs_1_left_01),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	&PKTINFO(canvas2_a3_s0_mcd_rs_2_left_01),
	&DLYINFO(canvas2_a3_s0_wait_1msec),

	/* MCD_R ENABLE */
	&PKTINFO(canvas2_a3_s0_mcd_rs_right_02),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	/* MCD1_R */
	&PKTINFO(canvas2_a3_s0_mcd_rs_1_right_03),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	&PKTINFO(canvas2_a3_s0_mcd_rs_right_04),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	/* MCD2_R */
	&PKTINFO(canvas2_a3_s0_mcd_rs_2_right_03),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	&PKTINFO(canvas2_a3_s0_mcd_rs_right_04),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	/* MCD_R DISABLE */
	&PKTINFO(canvas2_a3_s0_mcd_rs_right_05),
	&DLYINFO(canvas2_a3_s0_wait_1msec),

	/* MCD_L ENABLE */
	&PKTINFO(canvas2_a3_s0_mcd_rs_left_02),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	/* MCD1_L */
	&PKTINFO(canvas2_a3_s0_mcd_rs_1_left_03),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	&PKTINFO(canvas2_a3_s0_mcd_rs_left_04),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	/* MCD2_L */
	&PKTINFO(canvas2_a3_s0_mcd_rs_2_left_03),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	&PKTINFO(canvas2_a3_s0_mcd_rs_left_04),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	/* MCD_L DISABLE */
	&PKTINFO(canvas2_a3_s0_mcd_rs_left_05),
	&DLYINFO(canvas2_a3_s0_wait_1msec),

	/* READ MCD RESISTANCE */
	&s6e3hac_restbl[RES_MCD_RESISTANCE],
};

static void *canvas2_a3_s0_mcd_rs_off_cmdtbl[] = {
	&PKTINFO(canvas2_a3_s0_mcd_rs_off_01),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_mcd_rs_off_02),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_mcd_rs_off_03),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_mcd_rs_off_04),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_mcd_rs_off_05),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_mcd_rs_off_06),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_mcd_rs_off_07),
	&DLYINFO(canvas2_a3_s0_wait_1_frame_in_60hz),
	&PKTINFO(canvas2_a3_s0_level3_key_disable),
	&PKTINFO(canvas2_a3_s0_level2_key_disable),
	&PKTINFO(canvas2_a3_s0_level1_key_enable),
	&PKTINFO(canvas2_a3_s0_display_on),
	&PKTINFO(canvas2_a3_s0_level1_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_MST
static void *canvas2_a3_s0_mst_on_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_mst_on_01),
	&PKTINFO(canvas2_a3_s0_mst_on_02),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_mst_off_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_mst_off_01),
	&PKTINFO(canvas2_a3_s0_mst_off_02),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_CCD_TEST
static void *canvas2_a3_s0_ccd_test_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_ccd_test_enable),
	&DLYINFO(canvas2_a3_s0_wait_1msec),
	&s6e3hac_restbl[RES_CCD_STATE],
	&PKTINFO(canvas2_a3_s0_ccd_test_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
static void *canvas2_a3_s0_dynamic_hlpm_on_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_dynamic_hlpm_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_dynamic_hlpm_off_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_dynamic_hlpm_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
static void *canvas2_a3_s0_gct_enter_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&PKTINFO(canvas2_a3_s0_sw_reset),
	&DLYINFO(canvas2_a3_s0_wait_120msec),
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_vddm_init),
	&PKTINFO(canvas2_a3_s0_sleep_out),
	&DLYINFO(canvas2_a3_s0_wait_sleep_out_10msec),
	&PKTINFO(canvas2_a3_s0_hop_chksum_on),
	&PKTINFO(canvas2_a3_s0_gram_dbv_max),
	&PKTINFO(canvas2_a3_s0_gram_bctrl_on),
	&PKTINFO(canvas2_a3_s0_gct_dsc),
	&PKTINFO(canvas2_a3_s0_gct_pps),
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};

static void *canvas2_a3_s0_gct_vddm_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_vddm_volt),
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
	&DLYINFO(canvas2_a3_s0_wait_vddm_update),
};

static void *canvas2_a3_s0_gct_img_0_update_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_gram_chksum_start),
	&PKTINFO(canvas2_a3_s0_gram_inv_img_pattern_on),
	&DLYINFO(canvas2_a3_s0_wait_20msec_gram_img_update),
	&PKTINFO(canvas2_a3_s0_gram_img_pattern_off),
	&DLYINFO(canvas2_a3_s0_wait_20msec_gram_img_update),
	&PKTINFO(canvas2_a3_s0_gram_img_pattern_on),
	&DLYINFO(canvas2_a3_s0_wait_gram_img_update),
	&s6e3hac_restbl[RES_GRAM_CHECKSUM],
//	PKTINFO(canvas2_a3_s0_gram_img_pattern_on),
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),

};

static void *canvas2_a3_s0_gct_img_1_update_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_gram_img_pattern_off),
	&DLYINFO(canvas2_a3_s0_wait_20msec_gram_img_update),
	&PKTINFO(canvas2_a3_s0_gram_img_pattern_on),
	&DLYINFO(canvas2_a3_s0_wait_gram_img_update),
	&s6e3hac_restbl[RES_GRAM_CHECKSUM],
	&PKTINFO(canvas2_a3_s0_gram_img_pattern_off),
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};

static void *canvas2_a3_s0_gct_exit_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&PKTINFO(canvas2_a3_s0_vddm_orig),
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void *canvas2_a3_s0_grayspot_on_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_grayspot_on_00),
	&PKTINFO(canvas2_a3_s0_grayspot_on_01),
	&PKTINFO(canvas2_a3_s0_grayspot_on_02),
	&PKTINFO(canvas2_a3_s0_grayspot_on_03),
	&PKTINFO(canvas2_a3_s0_grayspot_on_04),
	&PKTINFO(canvas2_a3_s0_grayspot_on_05),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&DLYINFO(canvas2_a3_s0_wait_100msec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};

static void *canvas2_a3_s0_grayspot_off_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&PKTINFO(canvas2_a3_s0_grayspot_off_00),
	&PKTINFO(canvas2_a3_s0_grayspot_off_01),
	&PKTINFO(canvas2_a3_s0_grayspot_off_02),
	&PKTINFO(canvas2_a3_s0_grayspot_off_03),
	&PKTINFO(canvas2_a3_s0_grayspot_off_04),
	&PKTINFO(canvas2_a3_s0_vbias),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&DLYINFO(canvas2_a3_s0_wait_100msec),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
static void *canvas2_a3_s0_brightdot_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&SEQINFO(canvas2_a3_s0_wrdisbv_param_seq),
	&DLYINFO(canvas2_a3_s0_wait_1_vsync),
	&SEQINFO(canvas2_a3_s0_set_hs_aor_param_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};

static void *canvas2_a3_s0_brightdot_le_rev51_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&SEQINFO(canvas2_a3_s0_wrdisbv_param_seq),
	&DLYINFO(canvas2_a3_s0_wait_1_vsync),
	&SEQINFO(canvas2_a3_s0_set_hs_aor_param_le_rev51_seq),
	&PKTINFO(canvas2_a3_s0_gamma_update_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};
#endif

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void *canvas2_a3_s0_cmdlog_dump_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&s6e3hac_dmptbl[DUMP_CMDLOG],
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
};
#endif

static void *canvas2_a3_s0_dump_cmdtbl[] = {
	&KEYINFO(canvas2_a3_s0_level1_key_enable),
	&KEYINFO(canvas2_a3_s0_level2_key_enable),
	&KEYINFO(canvas2_a3_s0_level3_key_enable),
	&s6e3hac_dmptbl[DUMP_RDDPM],
	&s6e3hac_dmptbl[DUMP_RDDSM],
	&s6e3hac_dmptbl[DUMP_ERR],
	&s6e3hac_dmptbl[DUMP_ERR_FG],
	&s6e3hac_dmptbl[DUMP_DSI_ERR],
	&s6e3hac_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(canvas2_a3_s0_level3_key_disable),
	&KEYINFO(canvas2_a3_s0_level2_key_disable),
	&KEYINFO(canvas2_a3_s0_level1_key_disable),
};

static void *canvas2_a3_s0_dummy_cmdtbl[] = {
	NULL,
};

#ifdef CONFIG_SUPPORT_GM2_FLASH
static void *canvas2_a3_s0_gm2_flash_res_init_cmdtbl[] = {
	&s6e3hac_restbl[RES_GM2_FLASH_VBIAS1],
	&s6e3hac_restbl[RES_GM2_FLASH_VBIAS2],
};
#endif

static struct seqinfo canvas2_a3_s0_rev40_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", canvas2_a3_s0_rev40_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", canvas2_a3_s0_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", canvas2_a3_s0_rev40_rev50_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", canvas2_a3_s0_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", canvas2_a3_s0_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", canvas2_a3_s0_exit_cmdtbl),
	[PANEL_MCD_ON_SEQ] = SEQINFO_INIT("mcd-on-seq", canvas2_a3_s0_mcd_on_cmdtbl),
	[PANEL_MCD_OFF_SEQ] = SEQINFO_INIT("mcd-off-seq", canvas2_a3_s0_mcd_off_cmdtbl),
	[PANEL_DIA_ONOFF_SEQ] = SEQINFO_INIT("dia-onoff-seq", canvas2_a3_s0_dia_onoff_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", canvas2_a3_s0_rev40_alpm_enter_cmdtbl),
	[PANEL_ALPM_DELAY_SEQ] = SEQINFO_INIT("alpm-enter-delay-seq", canvas2_a3_s0_alpm_enter_delay_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", canvas2_a3_s0_rev40_alpm_exit_cmdtbl),
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", canvas2_a3_s0_check_condition_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", canvas2_a3_s0_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_GM2_FLASH
	[PANEL_GM2_FLASH_RES_INIT_SEQ] = SEQINFO_INIT("gm2-flash-resource-init-seq", canvas2_a3_s0_gm2_flash_res_init_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	[PANEL_DYNAMIC_HLPM_ON_SEQ] = SEQINFO_INIT("dynamic-hlpm-on-seq", canvas2_a3_s0_dynamic_hlpm_on_cmdtbl),
	[PANEL_DYNAMIC_HLPM_OFF_SEQ] = SEQINFO_INIT("dynamic-hlpm-off-seq", canvas2_a3_s0_dynamic_hlpm_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DSU
	[PANEL_DSU_SEQ] = SEQINFO_INIT("dsu-mode-seq", canvas2_a3_s0_dsu_mode_cmdtbl),
#endif
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", canvas2_a3_s0_rev40_set_fps_cmdtbl),
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("display-mode-seq", canvas2_a3_s0_rev40_display_mode_cmdtbl),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	[PANEL_DYNAMIC_FFC_SEQ] = SEQINFO_INIT("dynamic-ffc-seq", canvas2_a3_s0_dynamic_ffc_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	[PANEL_MAFPC_IMG_SEQ] = SEQINFO_INIT("mafpc-img-seq", canvas2_a3_s0_mafpc_image_cmdtbl),
	[PANEL_MAFPC_ON_SEQ] = SEQINFO_INIT("mafpc-on-seq", canvas2_a3_s0_rev51_mafpc_on_cmdtbl),
	[PANEL_MAFPC_OFF_SEQ] = SEQINFO_INIT("mafpc-off-seq", canvas2_a3_s0_mafpc_off_cmdtbl),
	[PANEL_MAFPC_FAC_CHECKSUM] = SEQINFO_INIT("mafpc-check-seq", canvas2_a3_s0_rev51_mafpc_check_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MST
	[PANEL_MST_ON_SEQ] = SEQINFO_INIT("mst-on-seq", canvas2_a3_s0_mst_on_cmdtbl),
	[PANEL_MST_OFF_SEQ] = SEQINFO_INIT("mst-off-seq", canvas2_a3_s0_mst_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[PANEL_CCD_TEST_SEQ] = SEQINFO_INIT("ccd-test-seq", canvas2_a3_s0_ccd_test_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[PANEL_GRAYSPOT_ON_SEQ] = SEQINFO_INIT("grayspot-on-seq", canvas2_a3_s0_grayspot_on_cmdtbl),
	[PANEL_GRAYSPOT_OFF_SEQ] = SEQINFO_INIT("grayspot-off-seq", canvas2_a3_s0_grayspot_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	[PANEL_BRIGHTDOT_TEST_SEQ] = SEQINFO_INIT("brightdot-seq", canvas2_a3_s0_brightdot_le_rev51_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[PANEL_CMDLOG_DUMP_SEQ] = SEQINFO_INIT("cmdlog-dump-seq", canvas2_a3_s0_cmdlog_dump_cmdtbl),
#endif
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", canvas2_a3_s0_dummy_cmdtbl),
};

static struct seqinfo canvas2_a3_s0_rev50_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", canvas2_a3_s0_rev50_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", canvas2_a3_s0_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", canvas2_a3_s0_rev40_rev50_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", canvas2_a3_s0_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", canvas2_a3_s0_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", canvas2_a3_s0_exit_cmdtbl),
	[PANEL_MCD_ON_SEQ] = SEQINFO_INIT("mcd-on-seq", canvas2_a3_s0_mcd_on_cmdtbl),
	[PANEL_MCD_OFF_SEQ] = SEQINFO_INIT("mcd-off-seq", canvas2_a3_s0_mcd_off_cmdtbl),
	[PANEL_DIA_ONOFF_SEQ] = SEQINFO_INIT("dia-onoff-seq", canvas2_a3_s0_dia_onoff_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", canvas2_a3_s0_rev50_alpm_enter_cmdtbl),
	[PANEL_ALPM_DELAY_SEQ] = SEQINFO_INIT("alpm-enter-delay-seq", canvas2_a3_s0_alpm_enter_delay_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", canvas2_a3_s0_rev50_alpm_exit_cmdtbl),
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", canvas2_a3_s0_check_condition_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", canvas2_a3_s0_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_GM2_FLASH
	[PANEL_GM2_FLASH_RES_INIT_SEQ] = SEQINFO_INIT("gm2-flash-resource-init-seq", canvas2_a3_s0_gm2_flash_res_init_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	[PANEL_DYNAMIC_HLPM_ON_SEQ] = SEQINFO_INIT("dynamic-hlpm-on-seq", canvas2_a3_s0_dynamic_hlpm_on_cmdtbl),
	[PANEL_DYNAMIC_HLPM_OFF_SEQ] = SEQINFO_INIT("dynamic-hlpm-off-seq", canvas2_a3_s0_dynamic_hlpm_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DSU
	[PANEL_DSU_SEQ] = SEQINFO_INIT("dsu-mode-seq", canvas2_a3_s0_dsu_mode_cmdtbl),
#endif
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", canvas2_a3_s0_rev40_set_fps_cmdtbl),
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("display-mode-seq", canvas2_a3_s0_rev40_display_mode_cmdtbl),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	[PANEL_DYNAMIC_FFC_SEQ] = SEQINFO_INIT("dynamic-ffc-seq", canvas2_a3_s0_dynamic_ffc_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	[PANEL_MAFPC_IMG_SEQ] = SEQINFO_INIT("mafpc-img-seq", canvas2_a3_s0_mafpc_image_cmdtbl),
	[PANEL_MAFPC_ON_SEQ] = SEQINFO_INIT("mafpc-on-seq", canvas2_a3_s0_rev51_mafpc_on_cmdtbl),
	[PANEL_MAFPC_OFF_SEQ] = SEQINFO_INIT("mafpc-off-seq", canvas2_a3_s0_mafpc_off_cmdtbl),
	[PANEL_MAFPC_FAC_CHECKSUM] = SEQINFO_INIT("mafpc-check-seq", canvas2_a3_s0_rev51_mafpc_check_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MST
	[PANEL_MST_ON_SEQ] = SEQINFO_INIT("mst-on-seq", canvas2_a3_s0_mst_on_cmdtbl),
	[PANEL_MST_OFF_SEQ] = SEQINFO_INIT("mst-off-seq", canvas2_a3_s0_mst_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	[PANEL_GCT_ENTER_SEQ] = SEQINFO_INIT("gct-enter-seq", canvas2_a3_s0_gct_enter_cmdtbl),
	[PANEL_GCT_VDDM_SEQ] = SEQINFO_INIT("gct-vddm-seq", canvas2_a3_s0_gct_vddm_cmdtbl),
	[PANEL_GCT_IMG_0_UPDATE_SEQ] = SEQINFO_INIT("gct-img-0-update-seq", canvas2_a3_s0_gct_img_0_update_cmdtbl),
	[PANEL_GCT_IMG_1_UPDATE_SEQ] = SEQINFO_INIT("gct-img-1-update-seq", canvas2_a3_s0_gct_img_1_update_cmdtbl),
	[PANEL_GCT_EXIT_SEQ] = SEQINFO_INIT("gct-exit-seq", canvas2_a3_s0_gct_exit_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[PANEL_CCD_TEST_SEQ] = SEQINFO_INIT("ccd-test-seq", canvas2_a3_s0_ccd_test_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[PANEL_GRAYSPOT_ON_SEQ] = SEQINFO_INIT("grayspot-on-seq", canvas2_a3_s0_grayspot_on_cmdtbl),
	[PANEL_GRAYSPOT_OFF_SEQ] = SEQINFO_INIT("grayspot-off-seq", canvas2_a3_s0_grayspot_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	[PANEL_BRIGHTDOT_TEST_SEQ] = SEQINFO_INIT("brightdot-seq", canvas2_a3_s0_brightdot_le_rev51_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[PANEL_CMDLOG_DUMP_SEQ] = SEQINFO_INIT("cmdlog-dump-seq", canvas2_a3_s0_cmdlog_dump_cmdtbl),
#endif
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", canvas2_a3_s0_dummy_cmdtbl),
};

static struct seqinfo canvas2_a3_s0_rev52_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", canvas2_a3_s0_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", canvas2_a3_s0_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", canvas2_a3_s0_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", canvas2_a3_s0_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", canvas2_a3_s0_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", canvas2_a3_s0_exit_cmdtbl),
	[PANEL_MCD_ON_SEQ] = SEQINFO_INIT("mcd-on-seq", canvas2_a3_s0_mcd_on_cmdtbl),
	[PANEL_MCD_OFF_SEQ] = SEQINFO_INIT("mcd-off-seq", canvas2_a3_s0_mcd_off_cmdtbl),
	[PANEL_DIA_ONOFF_SEQ] = SEQINFO_INIT("dia-onoff-seq", canvas2_a3_s0_dia_onoff_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", canvas2_a3_s0_rev52_alpm_enter_cmdtbl),
	[PANEL_ALPM_DELAY_SEQ] = SEQINFO_INIT("alpm-enter-delay-seq", canvas2_a3_s0_alpm_enter_delay_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", canvas2_a3_s0_rev52_alpm_exit_cmdtbl),
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", canvas2_a3_s0_check_condition_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", canvas2_a3_s0_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_GM2_FLASH
	[PANEL_GM2_FLASH_RES_INIT_SEQ] = SEQINFO_INIT("gm2-flash-resource-init-seq", canvas2_a3_s0_gm2_flash_res_init_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	[PANEL_DYNAMIC_HLPM_ON_SEQ] = SEQINFO_INIT("dynamic-hlpm-on-seq", canvas2_a3_s0_dynamic_hlpm_on_cmdtbl),
	[PANEL_DYNAMIC_HLPM_OFF_SEQ] = SEQINFO_INIT("dynamic-hlpm-off-seq", canvas2_a3_s0_dynamic_hlpm_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DSU
	[PANEL_DSU_SEQ] = SEQINFO_INIT("dsu-mode-seq", canvas2_a3_s0_dsu_mode_cmdtbl),
#endif
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", canvas2_a3_s0_set_fps_cmdtbl),
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("display-mode-seq", canvas2_a3_s0_display_mode_cmdtbl),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	[PANEL_DYNAMIC_FFC_SEQ] = SEQINFO_INIT("dynamic-ffc-seq", canvas2_a3_s0_dynamic_ffc_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	[PANEL_MAFPC_IMG_SEQ] = SEQINFO_INIT("mafpc-img-seq", canvas2_a3_s0_mafpc_image_cmdtbl),
	[PANEL_MAFPC_ON_SEQ] = SEQINFO_INIT("mafpc-on-seq", canvas2_a3_s0_mafpc_on_cmdtbl),
	[PANEL_MAFPC_OFF_SEQ] = SEQINFO_INIT("mafpc-off-seq", canvas2_a3_s0_mafpc_off_cmdtbl),
	[PANEL_MAFPC_FAC_CHECKSUM] = SEQINFO_INIT("mafpc-check-seq", canvas2_a3_s0_mafpc_check_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MST
	[PANEL_MST_ON_SEQ] = SEQINFO_INIT("mst-on-seq", canvas2_a3_s0_mst_on_cmdtbl),
	[PANEL_MST_OFF_SEQ] = SEQINFO_INIT("mst-off-seq", canvas2_a3_s0_mst_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	[PANEL_GCT_ENTER_SEQ] = SEQINFO_INIT("gct-enter-seq", canvas2_a3_s0_gct_enter_cmdtbl),
	[PANEL_GCT_VDDM_SEQ] = SEQINFO_INIT("gct-vddm-seq", canvas2_a3_s0_gct_vddm_cmdtbl),
	[PANEL_GCT_IMG_0_UPDATE_SEQ] = SEQINFO_INIT("gct-img-0-update-seq", canvas2_a3_s0_gct_img_0_update_cmdtbl),
	[PANEL_GCT_IMG_1_UPDATE_SEQ] = SEQINFO_INIT("gct-img-1-update-seq", canvas2_a3_s0_gct_img_1_update_cmdtbl),
	[PANEL_GCT_EXIT_SEQ] = SEQINFO_INIT("gct-exit-seq", canvas2_a3_s0_gct_exit_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[PANEL_CCD_TEST_SEQ] = SEQINFO_INIT("ccd-test-seq", canvas2_a3_s0_ccd_test_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[PANEL_GRAYSPOT_ON_SEQ] = SEQINFO_INIT("grayspot-on-seq", canvas2_a3_s0_grayspot_on_cmdtbl),
	[PANEL_GRAYSPOT_OFF_SEQ] = SEQINFO_INIT("grayspot-off-seq", canvas2_a3_s0_grayspot_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	[PANEL_BRIGHTDOT_TEST_SEQ] = SEQINFO_INIT("brightdot-seq", canvas2_a3_s0_brightdot_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[PANEL_CMDLOG_DUMP_SEQ] = SEQINFO_INIT("cmdlog-dump-seq", canvas2_a3_s0_cmdlog_dump_cmdtbl),
#endif
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", canvas2_a3_s0_dummy_cmdtbl),
};

static struct seqinfo canvas2_a3_s0_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", canvas2_a3_s0_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", canvas2_a3_s0_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", canvas2_a3_s0_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", canvas2_a3_s0_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", canvas2_a3_s0_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", canvas2_a3_s0_exit_cmdtbl),
	[PANEL_MCD_ON_SEQ] = SEQINFO_INIT("mcd-on-seq", canvas2_a3_s0_mcd_on_cmdtbl),
	[PANEL_MCD_OFF_SEQ] = SEQINFO_INIT("mcd-off-seq", canvas2_a3_s0_mcd_off_cmdtbl),
	[PANEL_DIA_ONOFF_SEQ] = SEQINFO_INIT("dia-onoff-seq", canvas2_a3_s0_dia_onoff_cmdtbl),
	[PANEL_ALPM_ENTER_SEQ] = SEQINFO_INIT("alpm-enter-seq", canvas2_a3_s0_alpm_enter_cmdtbl),
	[PANEL_ALPM_DELAY_SEQ] = SEQINFO_INIT("alpm-enter-delay-seq", canvas2_a3_s0_alpm_enter_delay_cmdtbl),
	[PANEL_ALPM_EXIT_SEQ] = SEQINFO_INIT("alpm-exit-seq", canvas2_a3_s0_alpm_exit_cmdtbl),
	[PANEL_CHECK_CONDITION_SEQ] = SEQINFO_INIT("check-condition-seq", canvas2_a3_s0_check_condition_cmdtbl),
	[PANEL_DUMP_SEQ] = SEQINFO_INIT("dump-seq", canvas2_a3_s0_dump_cmdtbl),
#ifdef CONFIG_SUPPORT_GM2_FLASH
	[PANEL_GM2_FLASH_RES_INIT_SEQ] = SEQINFO_INIT("gm2-flash-resource-init-seq", canvas2_a3_s0_gm2_flash_res_init_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DYNAMIC_HLPM
	[PANEL_DYNAMIC_HLPM_ON_SEQ] = SEQINFO_INIT("dynamic-hlpm-on-seq", canvas2_a3_s0_dynamic_hlpm_on_cmdtbl),
	[PANEL_DYNAMIC_HLPM_OFF_SEQ] = SEQINFO_INIT("dynamic-hlpm-off-seq", canvas2_a3_s0_dynamic_hlpm_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DSU
	[PANEL_DSU_SEQ] = SEQINFO_INIT("dsu-mode-seq", canvas2_a3_s0_dsu_mode_cmdtbl),
#endif
	[PANEL_FPS_SEQ] = SEQINFO_INIT("set-fps-seq", canvas2_a3_s0_set_fps_cmdtbl),
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("display-mode-seq", canvas2_a3_s0_display_mode_cmdtbl),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	[PANEL_DYNAMIC_FFC_SEQ] = SEQINFO_INIT("dynamic-ffc-seq", canvas2_a3_s0_dynamic_ffc_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	[PANEL_MAFPC_IMG_SEQ] = SEQINFO_INIT("mafpc-img-seq", canvas2_a3_s0_mafpc_image_cmdtbl),
	[PANEL_MAFPC_ON_SEQ] = SEQINFO_INIT("mafpc-on-seq", canvas2_a3_s0_mafpc_on_cmdtbl),
	[PANEL_MAFPC_OFF_SEQ] = SEQINFO_INIT("mafpc-off-seq", canvas2_a3_s0_mafpc_off_cmdtbl),
	[PANEL_MAFPC_FAC_CHECKSUM] = SEQINFO_INIT("mafpc-check-seq", canvas2_a3_s0_mafpc_check_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_MST
	[PANEL_MST_ON_SEQ] = SEQINFO_INIT("mst-on-seq", canvas2_a3_s0_mst_on_cmdtbl),
	[PANEL_MST_OFF_SEQ] = SEQINFO_INIT("mst-off-seq", canvas2_a3_s0_mst_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAM_CHECKSUM
	[PANEL_GCT_ENTER_SEQ] = SEQINFO_INIT("gct-enter-seq", canvas2_a3_s0_gct_enter_cmdtbl),
	[PANEL_GCT_VDDM_SEQ] = SEQINFO_INIT("gct-vddm-seq", canvas2_a3_s0_gct_vddm_cmdtbl),
	[PANEL_GCT_IMG_0_UPDATE_SEQ] = SEQINFO_INIT("gct-img-0-update-seq", canvas2_a3_s0_gct_img_0_update_cmdtbl),
	[PANEL_GCT_IMG_1_UPDATE_SEQ] = SEQINFO_INIT("gct-img-1-update-seq", canvas2_a3_s0_gct_img_1_update_cmdtbl),
	[PANEL_GCT_EXIT_SEQ] = SEQINFO_INIT("gct-exit-seq", canvas2_a3_s0_gct_exit_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_CCD_TEST
	[PANEL_CCD_TEST_SEQ] = SEQINFO_INIT("ccd-test-seq", canvas2_a3_s0_ccd_test_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	[PANEL_GRAYSPOT_ON_SEQ] = SEQINFO_INIT("grayspot-on-seq", canvas2_a3_s0_grayspot_on_cmdtbl),
	[PANEL_GRAYSPOT_OFF_SEQ] = SEQINFO_INIT("grayspot-off-seq", canvas2_a3_s0_grayspot_off_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_BRIGHTDOT_TEST
	[PANEL_BRIGHTDOT_TEST_SEQ] = SEQINFO_INIT("brightdot-seq", canvas2_a3_s0_brightdot_cmdtbl),
#endif
#ifdef CONFIG_SUPPORT_DDI_CMDLOG
	[PANEL_CMDLOG_DUMP_SEQ] = SEQINFO_INIT("cmdlog-dump-seq", canvas2_a3_s0_cmdlog_dump_cmdtbl),
#endif
	[PANEL_DUMMY_SEQ] = SEQINFO_INIT("dummy-seq", canvas2_a3_s0_dummy_cmdtbl),
};

#ifdef CONFIG_SUPPORT_POC_SPI
struct spi_data *s6e3hac_canvas2_a3_s0_spi_data_list[] = {
	&w25q80_spi_data,
	&mx25r4035_spi_data,
};
#endif

struct common_panel_info s6e3hac_canvas2_a3_s0_rev40_panel_info = {
	.ldi_name = "s6e3hac",
	.name = "s6e3hac_canvas2_a3_s0_rev40",
	.model = "AMB675TG01",
	.vendor = "SDC",
	.id = 0x810440,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
#ifdef CONFIG_SEC_FACTORY
		.err_fg_powerdown = true,
#else
		.err_fg_powerdown = false,
#endif
		.support_vrr = true,
		.support_vrr_lfd = true,
	},
#ifdef CONFIG_SUPPORT_DSU
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3hac_canvas_default_resol),
		.resol = s6e3hac_canvas_default_resol,
	},
#endif
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e3hac_canvas2_display_modes,
#endif
	.vrrtbl = s6e3hac_canvas_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e3hac_canvas_default_vrrtbl),
	.maptbl = canvas2_a3_s0_maptbl,
	.nr_maptbl = ARRAY_SIZE(canvas2_a3_s0_maptbl),
	.seqtbl = canvas2_a3_s0_rev40_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(canvas2_a3_s0_rev40_seqtbl),
	.rditbl = s6e3hac_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3hac_rditbl),
	.restbl = s6e3hac_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3hac_restbl),
	.dumpinfo = s6e3hac_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3hac_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &s6e3hac_canvas2_a3_s0_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3hac_canvas2_a3_s0_panel_dimming_info,
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3hac_canvas2_a3_s0_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	.copr_data = &s6e3hac_canvas2_a3_s0_copr_data,
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &s6e3hac_canvas2_rev51_aod,
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
	.poc_data = &s6e3hac_canvas2_poc_data,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	.spi_data_tbl = s6e3hac_canvas2_a3_s0_spi_data_list,
	.nr_spi_data_tbl = ARRAY_SIZE(s6e3hac_canvas2_a3_s0_spi_data_list),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	.df_freq_tbl = c2_dynamic_freq_set,
#endif
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = &hac_profiler_tune,
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	.mafpc_info = &s6e3hac_canvas2_mafpc,
#endif
};

struct common_panel_info s6e3hac_canvas2_a3_s0_rev50_panel_info = {
	.ldi_name = "s6e3hac",
	.name = "s6e3hac_canvas2_a3_s0_rev50",
	.model = "AMB675TG01",
	.vendor = "SDC",
	.id = 0x810450,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
#ifdef CONFIG_SEC_FACTORY
		.err_fg_powerdown = true,
#else
		.err_fg_powerdown = false,
#endif
		.support_vrr = true,
		.support_vrr_lfd = true,
	},
#ifdef CONFIG_SUPPORT_DSU
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3hac_canvas_default_resol),
		.resol = s6e3hac_canvas_default_resol,
	},
#endif
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e3hac_canvas2_display_modes,
#endif
	.vrrtbl = s6e3hac_canvas_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e3hac_canvas_default_vrrtbl),
	.maptbl = canvas2_a3_s0_maptbl,
	.nr_maptbl = ARRAY_SIZE(canvas2_a3_s0_maptbl),
	.seqtbl = canvas2_a3_s0_rev50_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(canvas2_a3_s0_rev50_seqtbl),
	.rditbl = s6e3hac_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3hac_rditbl),
	.restbl = s6e3hac_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3hac_restbl),
	.dumpinfo = s6e3hac_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3hac_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &s6e3hac_canvas2_a3_s0_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3hac_canvas2_a3_s0_panel_dimming_info,
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3hac_canvas2_a3_s0_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	.copr_data = &s6e3hac_canvas2_a3_s0_copr_data,
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &s6e3hac_canvas2_rev51_aod,
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
	.poc_data = &s6e3hac_canvas2_poc_data,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	.spi_data_tbl = s6e3hac_canvas2_a3_s0_spi_data_list,
	.nr_spi_data_tbl = ARRAY_SIZE(s6e3hac_canvas2_a3_s0_spi_data_list),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	.df_freq_tbl = c2_dynamic_freq_set,
#endif
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = &hac_profiler_tune,
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	.mafpc_info = &s6e3hac_canvas2_mafpc,
#endif
};

struct common_panel_info s6e3hac_canvas2_a3_s0_rev52_panel_info = {
	.ldi_name = "s6e3hac",
	.name = "s6e3hac_canvas2_a3_s0_rev52",
	.model = "AMB675TG01",
	.vendor = "SDC",
	.id = 0x810452,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
#ifdef CONFIG_SEC_FACTORY
		.err_fg_powerdown = true,
#else
		.err_fg_powerdown = false,
#endif
		.support_vrr = true,
		.support_vrr_lfd = true,
	},
#ifdef CONFIG_SUPPORT_DSU
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3hac_canvas_default_resol),
		.resol = s6e3hac_canvas_default_resol,
	},
#endif
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e3hac_canvas2_display_modes,
#endif
	.vrrtbl = s6e3hac_canvas_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e3hac_canvas_default_vrrtbl),
	.maptbl = canvas2_a3_s0_maptbl,
	.nr_maptbl = ARRAY_SIZE(canvas2_a3_s0_maptbl),
	.seqtbl = canvas2_a3_s0_rev52_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(canvas2_a3_s0_rev52_seqtbl),
	.rditbl = s6e3hac_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3hac_rditbl),
	.restbl = s6e3hac_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3hac_restbl),
	.dumpinfo = s6e3hac_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3hac_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &s6e3hac_canvas2_a3_s0_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3hac_canvas2_a3_s0_panel_dimming_info,
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3hac_canvas2_a3_s0_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	.copr_data = &s6e3hac_canvas2_a3_s0_copr_data,
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &s6e3hac_canvas2_aod,
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
	.poc_data = &s6e3hac_canvas2_poc_data,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	.spi_data_tbl = s6e3hac_canvas2_a3_s0_spi_data_list,
	.nr_spi_data_tbl = ARRAY_SIZE(s6e3hac_canvas2_a3_s0_spi_data_list),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	.df_freq_tbl = c2_dynamic_freq_set,
#endif
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = &hac_profiler_tune,
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	.mafpc_info = &s6e3hac_canvas2_mafpc,
#endif
};

struct common_panel_info s6e3hac_canvas2_a3_s0_panel_info = {
	.ldi_name = "s6e3hac",
	.name = "s6e3hac_canvas2_a3_s0_default",
	.model = "AMB675TG01",
	.vendor = "SDC",
	.id = 0x810453,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_POINT_GPARA),
		.err_fg_recovery = false,
#ifdef CONFIG_SEC_FACTORY
		.err_fg_powerdown = true,
#else
		.err_fg_powerdown = false,
#endif
		.support_vrr = true,
		.support_vrr_lfd = true,
	},
#ifdef CONFIG_SUPPORT_DSU
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3hac_canvas_default_resol),
		.resol = s6e3hac_canvas_default_resol,
	},
#endif
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e3hac_canvas2_display_modes,
#endif
	.vrrtbl = s6e3hac_canvas_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e3hac_canvas_default_vrrtbl),
	.maptbl = canvas2_a3_s0_maptbl,
	.nr_maptbl = ARRAY_SIZE(canvas2_a3_s0_maptbl),
	.seqtbl = canvas2_a3_s0_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(canvas2_a3_s0_seqtbl),
	.rditbl = s6e3hac_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3hac_rditbl),
	.restbl = s6e3hac_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3hac_restbl),
	.dumpinfo = s6e3hac_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3hac_dmptbl),
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	.mdnie_tune = &s6e3hac_canvas2_a3_s0_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3hac_canvas2_a3_s0_panel_dimming_info,
#ifdef CONFIG_SUPPORT_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3hac_canvas2_a3_s0_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_EXYNOS_DECON_LCD_COPR
	.copr_data = &s6e3hac_canvas2_a3_s0_copr_data,
#endif
#ifdef CONFIG_EXTEND_LIVE_CLOCK
	.aod_tune = &s6e3hac_canvas2_aod,
#endif
#ifdef CONFIG_SUPPORT_DDI_FLASH
	.poc_data = &s6e3hac_canvas2_poc_data,
#endif
#ifdef CONFIG_SUPPORT_POC_SPI
	.spi_data_tbl = s6e3hac_canvas2_a3_s0_spi_data_list,
	.nr_spi_data_tbl = ARRAY_SIZE(s6e3hac_canvas2_a3_s0_spi_data_list),
#endif
#ifdef CONFIG_DYNAMIC_FREQ
	.df_freq_tbl = c2_dynamic_freq_set,
#endif
#ifdef CONFIG_SUPPORT_DISPLAY_PROFILER
	.profile_tune = &hac_profiler_tune,
#endif
#ifdef CONFIG_SUPPORT_MAFPC
	.mafpc_info = &s6e3hac_canvas2_mafpc,
#endif
};

static int __init s6e3hac_canvas2_a3_s0_panel_init(void)
{
	register_common_panel(&s6e3hac_canvas2_a3_s0_rev40_panel_info);
	register_common_panel(&s6e3hac_canvas2_a3_s0_rev50_panel_info);
	register_common_panel(&s6e3hac_canvas2_a3_s0_rev52_panel_info);
	register_common_panel(&s6e3hac_canvas2_a3_s0_panel_info);

	return 0;
}
arch_initcall(s6e3hac_canvas2_a3_s0_panel_init)
#endif /* __S6E3HAC_CANVAS2_A3_S0_PANEL_H__ */
