/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fac/s6e3fac_mu1s_panel.h
 *
 * Header file for S6E3FAC Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAC_MU1S_PANEL_H__
#define __S6E3FAC_MU1S_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "oled_common.h"
#include "oled_common_cond.h"
#include "oled_property.h"
#include "s6e3fac.h"
#include "s6e3fac_dimming.h"
#ifdef CONFIG_USDM_MDNIE
#include "s6e3fac_mu1s_panel_mdnie.h"
#endif
#ifdef CONFIG_USDM_PANEL_COPR
#include "s6e3fac_mu1s_panel_copr.h"
#endif
#include "s6e3fac_mu1s_panel_dimming.h"
#ifdef CONFIG_USDM_PANEL_HMD
#include "s6e3fac_mu1s_panel_hmd_dimming.h"
#endif
#ifdef CONFIG_USDM_PANEL_AOD_BL
#include "s6e3fac_mu1s_panel_aod_dimming.h"
#endif
#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
#include "s6e3fac_mu1s_aod_panel.h"
#include "../aod/aod_drv.h"
#endif

#include "s6e3fac_mu1s_resol.h"

#ifdef CONFIG_USDM_PANEL_MAFPC
#include "s6e3fac_mu1s_abc_data.h"
#endif

#undef __pn_name__
#define __pn_name__	mu1s

#undef __PN_NAME__
#define __PN_NAME__	MU1S

/* ===================================================================================== */
/* ============================= [S6E3FAC READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [S6E3FAC RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [S6E3FAC MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 mu1s_brt_table[S6E3FAC_MU1S_TOTAL_STEP][2] = {
	/* Normal 5x51+1 */
	{ 0x00, 0x04 }, { 0x00, 0x06 }, { 0x00, 0x08 }, { 0x00, 0x0A }, { 0x00, 0x0D },
	{ 0x00, 0x10 }, { 0x00, 0x14 }, { 0x00, 0x17 }, { 0x00, 0x1B }, { 0x00, 0x1E },
	{ 0x00, 0x22 }, { 0x00, 0x26 }, { 0x00, 0x2A }, { 0x00, 0x2F }, { 0x00, 0x33 },
	{ 0x00, 0x37 }, { 0x00, 0x3C }, { 0x00, 0x41 }, { 0x00, 0x45 }, { 0x00, 0x4A },
	{ 0x00, 0x4F }, { 0x00, 0x54 }, { 0x00, 0x59 }, { 0x00, 0x5E }, { 0x00, 0x63 },
	{ 0x00, 0x68 }, { 0x00, 0x6D }, { 0x00, 0x72 }, { 0x00, 0x78 }, { 0x00, 0x7D },
	{ 0x00, 0x83 }, { 0x00, 0x88 }, { 0x00, 0x8E }, { 0x00, 0x93 }, { 0x00, 0x99 },
	{ 0x00, 0x9F }, { 0x00, 0xA4 }, { 0x00, 0xAA }, { 0x00, 0xB0 }, { 0x00, 0xB6 },
	{ 0x00, 0xBC }, { 0x00, 0xC2 }, { 0x00, 0xC8 }, { 0x00, 0xCE }, { 0x00, 0xD4 },
	{ 0x00, 0xDB }, { 0x00, 0xE1 }, { 0x00, 0xE7 }, { 0x00, 0xED }, { 0x00, 0xF4 },
	{ 0x00, 0xFA }, { 0x01, 0x00 }, { 0x01, 0x07 }, { 0x01, 0x0D }, { 0x01, 0x14 },
	{ 0x01, 0x1A }, { 0x01, 0x21 }, { 0x01, 0x28 }, { 0x01, 0x2E }, { 0x01, 0x35 },
	{ 0x01, 0x3C }, { 0x01, 0x43 }, { 0x01, 0x49 }, { 0x01, 0x50 }, { 0x01, 0x57 },
	{ 0x01, 0x5E }, { 0x01, 0x66 }, { 0x01, 0x6D }, { 0x01, 0x74 }, { 0x01, 0x7B },
	{ 0x01, 0x83 }, { 0x01, 0x8A }, { 0x01, 0x91 }, { 0x01, 0x99 }, { 0x01, 0xA0 },
	{ 0x01, 0xA8 }, { 0x01, 0xAF }, { 0x01, 0xB7 }, { 0x01, 0xBE }, { 0x01, 0xC6 },
	{ 0x01, 0xCD }, { 0x01, 0xD5 }, { 0x01, 0xDD }, { 0x01, 0xE4 }, { 0x01, 0xEC },
	{ 0x01, 0xF4 }, { 0x01, 0xFB }, { 0x02, 0x03 }, { 0x02, 0x0B }, { 0x02, 0x13 },
	{ 0x02, 0x1B }, { 0x02, 0x23 }, { 0x02, 0x2B }, { 0x02, 0x33 }, { 0x02, 0x3B },
	{ 0x02, 0x43 }, { 0x02, 0x4B }, { 0x02, 0x53 }, { 0x02, 0x5B }, { 0x02, 0x63 },
	{ 0x02, 0x6B }, { 0x02, 0x73 }, { 0x02, 0x7B }, { 0x02, 0x83 }, { 0x02, 0x8C },
	{ 0x02, 0x94 }, { 0x02, 0x9C }, { 0x02, 0xA5 }, { 0x02, 0xAD }, { 0x02, 0xB5 },
	{ 0x02, 0xBE }, { 0x02, 0xC6 }, { 0x02, 0xCE }, { 0x02, 0xD7 }, { 0x02, 0xDF },
	{ 0x02, 0xE8 }, { 0x02, 0xF0 }, { 0x02, 0xF9 }, { 0x03, 0x01 }, { 0x03, 0x09 },
	{ 0x03, 0x11 }, { 0x03, 0x1A }, { 0x03, 0x22 }, { 0x03, 0x2A }, { 0x03, 0x32 },
	{ 0x03, 0x3B }, { 0x03, 0x43 }, { 0x03, 0x4B }, { 0x03, 0x54 }, { 0x03, 0x5C },
	{ 0x03, 0x65 }, { 0x03, 0x6D }, { 0x03, 0x75 }, { 0x03, 0x7E }, { 0x03, 0x86 },
	{ 0x03, 0x8F }, { 0x03, 0x97 }, { 0x03, 0xA0 }, { 0x03, 0xA8 }, { 0x03, 0xB1 },
	{ 0x03, 0xBA }, { 0x03, 0xC2 }, { 0x03, 0xCB }, { 0x03, 0xD4 }, { 0x03, 0xDC },
	{ 0x03, 0xE5 }, { 0x03, 0xEE }, { 0x03, 0xF6 }, { 0x03, 0xFF }, { 0x04, 0x08 },
	{ 0x04, 0x11 }, { 0x04, 0x19 }, { 0x04, 0x22 }, { 0x04, 0x2B }, { 0x04, 0x34 },
	{ 0x04, 0x3D }, { 0x04, 0x46 }, { 0x04, 0x4E }, { 0x04, 0x57 }, { 0x04, 0x60 },
	{ 0x04, 0x69 }, { 0x04, 0x72 }, { 0x04, 0x7B }, { 0x04, 0x84 }, { 0x04, 0x8D },
	{ 0x04, 0x96 }, { 0x04, 0x9F }, { 0x04, 0xA8 }, { 0x04, 0xB1 }, { 0x04, 0xBA },
	{ 0x04, 0xC4 }, { 0x04, 0xCD }, { 0x04, 0xD6 }, { 0x04, 0xDF }, { 0x04, 0xE8 },
	{ 0x04, 0xF1 }, { 0x04, 0xFB }, { 0x05, 0x04 }, { 0x05, 0x0D }, { 0x05, 0x16 },
	{ 0x05, 0x20 }, { 0x05, 0x29 }, { 0x05, 0x32 }, { 0x05, 0x3B }, { 0x05, 0x45 },
	{ 0x05, 0x4E }, { 0x05, 0x57 }, { 0x05, 0x61 }, { 0x05, 0x6A }, { 0x05, 0x74 },
	{ 0x05, 0x7D }, { 0x05, 0x86 }, { 0x05, 0x90 }, { 0x05, 0x99 }, { 0x05, 0xA3 },
	{ 0x05, 0xAC }, { 0x05, 0xB6 }, { 0x05, 0xBF }, { 0x05, 0xC9 }, { 0x05, 0xD2 },
	{ 0x05, 0xDC }, { 0x05, 0xE6 }, { 0x05, 0xEF }, { 0x05, 0xF9 }, { 0x06, 0x02 },
	{ 0x06, 0x0C }, { 0x06, 0x16 }, { 0x06, 0x1F }, { 0x06, 0x29 }, { 0x06, 0x33 },
	{ 0x06, 0x3C }, { 0x06, 0x46 }, { 0x06, 0x50 }, { 0x06, 0x5A }, { 0x06, 0x63 },
	{ 0x06, 0x6D }, { 0x06, 0x77 }, { 0x06, 0x81 }, { 0x06, 0x8B }, { 0x06, 0x94 },
	{ 0x06, 0x9E }, { 0x06, 0xA8 }, { 0x06, 0xB2 }, { 0x06, 0xBC }, { 0x06, 0xC6 },
	{ 0x06, 0xD0 }, { 0x06, 0xDA }, { 0x06, 0xE4 }, { 0x06, 0xEE }, { 0x06, 0xF7 },
	{ 0x07, 0x01 }, { 0x07, 0x0B }, { 0x07, 0x15 }, { 0x07, 0x1F }, { 0x07, 0x2A },
	{ 0x07, 0x34 }, { 0x07, 0x3E }, { 0x07, 0x48 }, { 0x07, 0x52 }, { 0x07, 0x5C },
	{ 0x07, 0x66 }, { 0x07, 0x70 }, { 0x07, 0x7A }, { 0x07, 0x84 }, { 0x07, 0x8E },
	{ 0x07, 0x99 }, { 0x07, 0xA3 }, { 0x07, 0xAD }, { 0x07, 0xB7 }, { 0x07, 0xC1 },
	{ 0x07, 0xCC }, { 0x07, 0xD6 }, { 0x07, 0xE0 }, { 0x07, 0xEA }, { 0x07, 0xF5 },
	{ 0x07, 0xFF },
	/* HBM 5x204 */
	{ 0x08, 0x06 }, { 0x08, 0x0B }, { 0x08, 0x11 }, { 0x08, 0x17 }, { 0x08, 0x1D },
	{ 0x08, 0x22 }, { 0x08, 0x28 }, { 0x08, 0x2E }, { 0x08, 0x34 }, { 0x08, 0x39 },
	{ 0x08, 0x3F }, { 0x08, 0x45 }, { 0x08, 0x4B }, { 0x08, 0x50 }, { 0x08, 0x56 },
	{ 0x08, 0x5C }, { 0x08, 0x61 }, { 0x08, 0x67 }, { 0x08, 0x6D }, { 0x08, 0x73 },
	{ 0x08, 0x78 }, { 0x08, 0x7E }, { 0x08, 0x84 }, { 0x08, 0x8A }, { 0x08, 0x8F },
	{ 0x08, 0x95 }, { 0x08, 0x9B }, { 0x08, 0xA1 }, { 0x08, 0xA6 }, { 0x08, 0xAC },
	{ 0x08, 0xB2 }, { 0x08, 0xB7 }, { 0x08, 0xBD }, { 0x08, 0xC3 }, { 0x08, 0xC9 },
	{ 0x08, 0xCE }, { 0x08, 0xD4 }, { 0x08, 0xDA }, { 0x08, 0xE0 }, { 0x08, 0xE5 },
	{ 0x08, 0xEB }, { 0x08, 0xF1 }, { 0x08, 0xF7 }, { 0x08, 0xFC }, { 0x09, 0x02 },
	{ 0x09, 0x08 }, { 0x09, 0x0D }, { 0x09, 0x13 }, { 0x09, 0x19 }, { 0x09, 0x1F },
	{ 0x09, 0x24 }, { 0x09, 0x2A }, { 0x09, 0x30 }, { 0x09, 0x36 }, { 0x09, 0x3B },
	{ 0x09, 0x41 }, { 0x09, 0x47 }, { 0x09, 0x4D }, { 0x09, 0x52 }, { 0x09, 0x58 },
	{ 0x09, 0x5E }, { 0x09, 0x64 }, { 0x09, 0x69 }, { 0x09, 0x6F }, { 0x09, 0x75 },
	{ 0x09, 0x7A }, { 0x09, 0x80 }, { 0x09, 0x86 }, { 0x09, 0x8C }, { 0x09, 0x91 },
	{ 0x09, 0x97 }, { 0x09, 0x9D }, { 0x09, 0xA3 }, { 0x09, 0xA8 }, { 0x09, 0xAE },
	{ 0x09, 0xB4 }, { 0x09, 0xBA }, { 0x09, 0xBF }, { 0x09, 0xC5 }, { 0x09, 0xCB },
	{ 0x09, 0xD0 }, { 0x09, 0xD6 }, { 0x09, 0xDC }, { 0x09, 0xE2 }, { 0x09, 0xE7 },
	{ 0x09, 0xED }, { 0x09, 0xF3 }, { 0x09, 0xF9 }, { 0x09, 0xFE }, { 0x0A, 0x04 },
	{ 0x0A, 0x0A }, { 0x0A, 0x10 }, { 0x0A, 0x15 }, { 0x0A, 0x1B }, { 0x0A, 0x21 },
	{ 0x0A, 0x26 }, { 0x0A, 0x2C }, { 0x0A, 0x32 }, { 0x0A, 0x38 }, { 0x0A, 0x3D },
	{ 0x0A, 0x43 }, { 0x0A, 0x49 }, { 0x0A, 0x4F }, { 0x0A, 0x54 }, { 0x0A, 0x5A },
	{ 0x0A, 0x60 }, { 0x0A, 0x66 }, { 0x0A, 0x6B }, { 0x0A, 0x71 }, { 0x0A, 0x77 },
	{ 0x0A, 0x7C }, { 0x0A, 0x82 }, { 0x0A, 0x88 }, { 0x0A, 0x8E }, { 0x0A, 0x93 },
	{ 0x0A, 0x99 }, { 0x0A, 0x9F }, { 0x0A, 0xA5 }, { 0x0A, 0xAA }, { 0x0A, 0xB0 },
	{ 0x0A, 0xB6 }, { 0x0A, 0xBC }, { 0x0A, 0xC1 }, { 0x0A, 0xC7 }, { 0x0A, 0xCD },
	{ 0x0A, 0xD2 }, { 0x0A, 0xD8 }, { 0x0A, 0xDE }, { 0x0A, 0xE4 }, { 0x0A, 0xE9 },
	{ 0x0A, 0xEF }, { 0x0A, 0xF5 }, { 0x0A, 0xFB }, { 0x0B, 0x00 }, { 0x0B, 0x06 },
	{ 0x0B, 0x0C }, { 0x0B, 0x12 }, { 0x0B, 0x17 }, { 0x0B, 0x1D }, { 0x0B, 0x23 },
	{ 0x0B, 0x28 }, { 0x0B, 0x2E }, { 0x0B, 0x34 }, { 0x0B, 0x3A }, { 0x0B, 0x3F },
	{ 0x0B, 0x45 }, { 0x0B, 0x4B }, { 0x0B, 0x51 }, { 0x0B, 0x56 }, { 0x0B, 0x5C },
	{ 0x0B, 0x62 }, { 0x0B, 0x68 }, { 0x0B, 0x6D }, { 0x0B, 0x73 }, { 0x0B, 0x79 },
	{ 0x0B, 0x7E }, { 0x0B, 0x84 }, { 0x0B, 0x8A }, { 0x0B, 0x90 }, { 0x0B, 0x95 },
	{ 0x0B, 0x9B }, { 0x0B, 0xA1 }, { 0x0B, 0xA7 }, { 0x0B, 0xAC }, { 0x0B, 0xB2 },
	{ 0x0B, 0xB8 }, { 0x0B, 0xBE }, { 0x0B, 0xC3 }, { 0x0B, 0xC9 }, { 0x0B, 0xCF },
	{ 0x0B, 0xD4 }, { 0x0B, 0xDA }, { 0x0B, 0xE0 }, { 0x0B, 0xE6 }, { 0x0B, 0xEB },
	{ 0x0B, 0xF1 }, { 0x0B, 0xF7 }, { 0x0B, 0xFD }, { 0x0C, 0x02 }, { 0x0C, 0x08 },
	{ 0x0C, 0x0E }, { 0x0C, 0x14 }, { 0x0C, 0x19 }, { 0x0C, 0x1F }, { 0x0C, 0x25 },
	{ 0x0C, 0x2B }, { 0x0C, 0x30 }, { 0x0C, 0x36 }, { 0x0C, 0x3C }, { 0x0C, 0x41 },
	{ 0x0C, 0x47 }, { 0x0C, 0x4D }, { 0x0C, 0x53 }, { 0x0C, 0x58 }, { 0x0C, 0x5E },
	{ 0x0C, 0x64 }, { 0x0C, 0x6A }, { 0x0C, 0x6F }, { 0x0C, 0x75 }, { 0x0C, 0x7B },
	{ 0x0C, 0x81 }, { 0x0C, 0x86 }, { 0x0C, 0x8C }, { 0x0C, 0x92 }, { 0x0C, 0x97 },
	{ 0x0C, 0x9D }, { 0x0C, 0xA3 }, { 0x0C, 0xA9 }, { 0x0C, 0xAE }, { 0x0C, 0xB4 },
	{ 0x0C, 0xBA }, { 0x0C, 0xC0 }, { 0x0C, 0xC5 }, { 0x0C, 0xCB }, { 0x0C, 0xD1 },
	{ 0x0C, 0xD7 }, { 0x0C, 0xDC }, { 0x0C, 0xE2 }, { 0x0C, 0xE8 }, { 0x0C, 0xED },
	{ 0x0C, 0xF3 }, { 0x0C, 0xF9 }, { 0x0C, 0xFF }, { 0x0D, 0x04 }, { 0x0D, 0x0A },
	{ 0x0D, 0x10 }, { 0x0D, 0x16 }, { 0x0D, 0x1B }, { 0x0D, 0x21 }, { 0x0D, 0x27 },
	{ 0x0D, 0x2D }, { 0x0D, 0x32 }, { 0x0D, 0x38 }, { 0x0D, 0x3E }, { 0x0D, 0x43 },
	{ 0x0D, 0x49 }, { 0x0D, 0x4F }, { 0x0D, 0x55 }, { 0x0D, 0x5A }, { 0x0D, 0x60 },
	{ 0x0D, 0x66 }, { 0x0D, 0x6C }, { 0x0D, 0x71 }, { 0x0D, 0x77 }, { 0x0D, 0x7D },
	{ 0x0D, 0x83 }, { 0x0D, 0x88 }, { 0x0D, 0x8E }, { 0x0D, 0x94 }, { 0x0D, 0x99 },
	{ 0x0D, 0x9F }, { 0x0D, 0xA5 }, { 0x0D, 0xAB }, { 0x0D, 0xB0 }, { 0x0D, 0xB6 },
	{ 0x0D, 0xBC }, { 0x0D, 0xC2 }, { 0x0D, 0xC7 }, { 0x0D, 0xCD }, { 0x0D, 0xD3 },
	{ 0x0D, 0xD9 }, { 0x0D, 0xDE }, { 0x0D, 0xE4 }, { 0x0D, 0xEA }, { 0x0D, 0xEF },
	{ 0x0D, 0xF5 }, { 0x0D, 0xFB }, { 0x0E, 0x01 }, { 0x0E, 0x06 }, { 0x0E, 0x0C },
	{ 0x0E, 0x12 }, { 0x0E, 0x18 }, { 0x0E, 0x1D }, { 0x0E, 0x23 }, { 0x0E, 0x29 },
	{ 0x0E, 0x2F }, { 0x0E, 0x34 }, { 0x0E, 0x3A }, { 0x0E, 0x40 }, { 0x0E, 0x45 },
	{ 0x0E, 0x4B }, { 0x0E, 0x51 }, { 0x0E, 0x57 }, { 0x0E, 0x5C }, { 0x0E, 0x62 },
	{ 0x0E, 0x68 }, { 0x0E, 0x6E }, { 0x0E, 0x73 }, { 0x0E, 0x79 }, { 0x0E, 0x7F },
	{ 0x0E, 0x85 }, { 0x0E, 0x8A }, { 0x0E, 0x90 }, { 0x0E, 0x96 }, { 0x0E, 0x9B },
	{ 0x0E, 0xA1 }, { 0x0E, 0xA7 }, { 0x0E, 0xAD }, { 0x0E, 0xB2 }, { 0x0E, 0xB8 },
	{ 0x0E, 0xBE }, { 0x0E, 0xC4 }, { 0x0E, 0xC9 }, { 0x0E, 0xCF }, { 0x0E, 0xD5 },
	{ 0x0E, 0xDB }, { 0x0E, 0xE0 }, { 0x0E, 0xE6 }, { 0x0E, 0xEC }, { 0x0E, 0xF2 },
	{ 0x0E, 0xF7 }, { 0x0E, 0xFD }, { 0x0F, 0x03 }, { 0x0F, 0x08 }, { 0x0F, 0x0E },
	{ 0x0F, 0x14 }, { 0x0F, 0x1A }, { 0x0F, 0x1F }, { 0x0F, 0x25 }, { 0x0F, 0x2B },
	{ 0x0F, 0x31 }, { 0x0F, 0x36 }, { 0x0F, 0x3C }, { 0x0F, 0x42 }, { 0x0F, 0x48 },
	{ 0x0F, 0x4D }, { 0x0F, 0x53 }, { 0x0F, 0x59 }, { 0x0F, 0x5E }, { 0x0F, 0x64 },
	{ 0x0F, 0x6A }, { 0x0F, 0x70 }, { 0x0F, 0x75 }, { 0x0F, 0x7B }, { 0x0F, 0x81 },
	{ 0x0F, 0x87 }, { 0x0F, 0x8C }, { 0x0F, 0x92 }, { 0x0F, 0x98 }, { 0x0F, 0x9E },
	{ 0x0F, 0xA3 }, { 0x0F, 0xA9 }, { 0x0F, 0xAF }, { 0x0F, 0xB4 }, { 0x0F, 0xBA },
	{ 0x0F, 0xC0 }, { 0x0F, 0xC6 }, { 0x0F, 0xCB }, { 0x0F, 0xD1 }, { 0x0F, 0xD7 },
	{ 0x0F, 0xDD }, { 0x0F, 0xE2 }, { 0x0F, 0xE8 }, { 0x0F, 0xEE }, { 0x0F, 0xF4 },
	{ 0x0F, 0xF9 }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
	{ 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF }, { 0x0F, 0xFF },
};

#ifdef CONFIG_USDM_PANEL_HMD
static u8 mu1s_hmd_brt_table[S6E3FAC_HMD_TOTAL_STEP][2] = {
	/* Normal 5x51+1 */
	{ 0x01, 0x99 }, { 0x01, 0x9F }, { 0x01, 0xA6 }, { 0x01, 0xAC }, { 0x01, 0xB3 },
	{ 0x01, 0xB9 }, { 0x01, 0xC0 }, { 0x01, 0xC6 }, { 0x01, 0xCC }, { 0x01, 0xD3 },
	{ 0x01, 0xD9 }, { 0x01, 0xE0 }, { 0x01, 0xE6 }, { 0x01, 0xED }, { 0x01, 0xF3 },
	{ 0x01, 0xF9 }, { 0x02, 0x00 }, { 0x02, 0x06 }, { 0x02, 0x0D }, { 0x02, 0x13 },
	{ 0x02, 0x19 }, { 0x02, 0x20 }, { 0x02, 0x26 }, { 0x02, 0x2D }, { 0x02, 0x33 },
	{ 0x02, 0x3A }, { 0x02, 0x40 }, { 0x02, 0x46 }, { 0x02, 0x4D }, { 0x02, 0x53 },
	{ 0x02, 0x5A }, { 0x02, 0x60 }, { 0x02, 0x67 }, { 0x02, 0x6D }, { 0x02, 0x73 },
	{ 0x02, 0x7A }, { 0x02, 0x80 }, { 0x02, 0x87 }, { 0x02, 0x8D }, { 0x02, 0x94 },
	{ 0x02, 0x9A }, { 0x02, 0xA0 }, { 0x02, 0xA7 }, { 0x02, 0xAD }, { 0x02, 0xB4 },
	{ 0x02, 0xBA }, { 0x02, 0xC0 }, { 0x02, 0xC7 }, { 0x02, 0xCD }, { 0x02, 0xD4 },
	{ 0x02, 0xDA }, { 0x02, 0xE1 }, { 0x02, 0xE7 }, { 0x02, 0xED }, { 0x02, 0xF4 },
	{ 0x02, 0xFA }, { 0x03, 0x01 }, { 0x03, 0x07 }, { 0x03, 0x0E }, { 0x03, 0x14 },
	{ 0x03, 0x1A }, { 0x03, 0x21 }, { 0x03, 0x27 }, { 0x03, 0x2E }, { 0x03, 0x34 },
	{ 0x03, 0x3B }, { 0x03, 0x41 }, { 0x03, 0x47 }, { 0x03, 0x4E }, { 0x03, 0x54 },
	{ 0x03, 0x5B }, { 0x03, 0x61 }, { 0x03, 0x67 }, { 0x03, 0x6E }, { 0x03, 0x74 },
	{ 0x03, 0x7B }, { 0x03, 0x81 }, { 0x03, 0x88 }, { 0x03, 0x8E }, { 0x03, 0x94 },
	{ 0x03, 0x9B }, { 0x03, 0xA1 }, { 0x03, 0xA8 }, { 0x03, 0xAE }, { 0x03, 0xB5 },
	{ 0x03, 0xBB }, { 0x03, 0xC1 }, { 0x03, 0xC8 }, { 0x03, 0xCE }, { 0x03, 0xD5 },
	{ 0x03, 0xDB }, { 0x03, 0xE2 }, { 0x03, 0xE8 }, { 0x03, 0xEE }, { 0x03, 0xF5 },
	{ 0x03, 0xFB }, { 0x04, 0x02 }, { 0x04, 0x08 }, { 0x04, 0x0F }, { 0x04, 0x15 },
	{ 0x04, 0x1B }, { 0x04, 0x22 }, { 0x04, 0x28 }, { 0x04, 0x2F }, { 0x04, 0x35 },
	{ 0x04, 0x3B }, { 0x04, 0x42 }, { 0x04, 0x48 }, { 0x04, 0x4F }, { 0x04, 0x55 },
	{ 0x04, 0x5C }, { 0x04, 0x62 }, { 0x04, 0x68 }, { 0x04, 0x6F }, { 0x04, 0x75 },
	{ 0x04, 0x7C }, { 0x04, 0x82 }, { 0x04, 0x89 }, { 0x04, 0x8F }, { 0x04, 0x95 },
	{ 0x04, 0x9C }, { 0x04, 0xA2 }, { 0x04, 0xA9 }, { 0x04, 0xAF }, { 0x04, 0xB6 },
	{ 0x04, 0xBC }, { 0x04, 0xC2 }, { 0x04, 0xC9 }, { 0x04, 0xCF }, { 0x04, 0xD6 },
	{ 0x04, 0xDC }, { 0x04, 0xE2 }, { 0x04, 0xE9 }, { 0x04, 0xEF }, { 0x04, 0xF6 },
	{ 0x04, 0xFC }, { 0x05, 0x03 }, { 0x05, 0x09 }, { 0x05, 0x0F }, { 0x05, 0x16 },
	{ 0x05, 0x1C }, { 0x05, 0x23 }, { 0x05, 0x29 }, { 0x05, 0x30 }, { 0x05, 0x36 },
	{ 0x05, 0x3C }, { 0x05, 0x43 }, { 0x05, 0x49 }, { 0x05, 0x50 }, { 0x05, 0x56 },
	{ 0x05, 0x5D }, { 0x05, 0x63 }, { 0x05, 0x69 }, { 0x05, 0x70 }, { 0x05, 0x76 },
	{ 0x05, 0x7D }, { 0x05, 0x83 }, { 0x05, 0x89 }, { 0x05, 0x90 }, { 0x05, 0x96 },
	{ 0x05, 0x9D }, { 0x05, 0xA3 }, { 0x05, 0xAA }, { 0x05, 0xB0 }, { 0x05, 0xB6 },
	{ 0x05, 0xBD }, { 0x05, 0xC3 }, { 0x05, 0xCA }, { 0x05, 0xD0 }, { 0x05, 0xD7 },
	{ 0x05, 0xDD }, { 0x05, 0xE3 }, { 0x05, 0xEA }, { 0x05, 0xF0 }, { 0x05, 0xF7 },
	{ 0x05, 0xFD }, { 0x06, 0x04 }, { 0x06, 0x0A }, { 0x06, 0x10 }, { 0x06, 0x17 },
	{ 0x06, 0x1D }, { 0x06, 0x24 }, { 0x06, 0x2A }, { 0x06, 0x31 }, { 0x06, 0x37 },
	{ 0x06, 0x3D }, { 0x06, 0x44 }, { 0x06, 0x4A }, { 0x06, 0x51 }, { 0x06, 0x57 },
	{ 0x06, 0x5D }, { 0x06, 0x64 }, { 0x06, 0x6A }, { 0x06, 0x71 }, { 0x06, 0x77 },
	{ 0x06, 0x7E }, { 0x06, 0x84 }, { 0x06, 0x8A }, { 0x06, 0x91 }, { 0x06, 0x97 },
	{ 0x06, 0x9E }, { 0x06, 0xA4 }, { 0x06, 0xAB }, { 0x06, 0xB1 }, { 0x06, 0xB7 },
	{ 0x06, 0xBE }, { 0x06, 0xC4 }, { 0x06, 0xCB }, { 0x06, 0xD1 }, { 0x06, 0xD8 },
	{ 0x06, 0xDE }, { 0x06, 0xE4 }, { 0x06, 0xEB }, { 0x06, 0xF1 }, { 0x06, 0xF8 },
	{ 0x06, 0xFE }, { 0x07, 0x04 }, { 0x07, 0x0B }, { 0x07, 0x11 }, { 0x07, 0x18 },
	{ 0x07, 0x1E }, { 0x07, 0x25 }, { 0x07, 0x2B }, { 0x07, 0x31 }, { 0x07, 0x38 },
	{ 0x07, 0x3E }, { 0x07, 0x45 }, { 0x07, 0x4B }, { 0x07, 0x52 }, { 0x07, 0x58 },
	{ 0x07, 0x5E }, { 0x07, 0x65 }, { 0x07, 0x6B }, { 0x07, 0x72 }, { 0x07, 0x78 },
	{ 0x07, 0x7F }, { 0x07, 0x85 }, { 0x07, 0x8B }, { 0x07, 0x92 }, { 0x07, 0x98 },
	{ 0x07, 0x9F }, { 0x07, 0xA5 }, { 0x07, 0xAB }, { 0x07, 0xB2 }, { 0x07, 0xB8 },
	{ 0x07, 0xBF }, { 0x07, 0xC5 }, { 0x07, 0xCC }, { 0x07, 0xD2 }, { 0x07, 0xD8 },
	{ 0x07, 0xDF }, { 0x07, 0xE5 }, { 0x07, 0xEC }, { 0x07, 0xF2 }, { 0x07, 0xF9 },
	{ 0x07, 0xFF },
};
#endif

static u8 mu1s_dimming_frame_table[MAX_S6E3FAC_SMOOTH_DIM][MAX_S6E3FAC_VRR][1] = {
	[S6E3FAC_SMOOTH_DIM_USE] = {
		[S6E3FAC_VRR_60NS ... S6E3FAC_VRR_120HS - 1] = { 0x0F },
		[S6E3FAC_VRR_120HS ... S6E3FAC_VRR_96HS - 1] = { 0x0F },
		[S6E3FAC_VRR_96HS ... S6E3FAC_VRR_60HS - 1] = { 0x00 },
		[S6E3FAC_VRR_60HS] = { 0x00 },
		[S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1] = { 0x00 },
		[S6E3FAC_VRR_48HS] = { 0x00 },
		[S6E3FAC_VRR_24HS_48HS_TE_HW_SKIP_1] = { 0x00 },
		[S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4] = { 0x00 },
	},
	[S6E3FAC_SMOOTH_DIM_NOT_USE] = {
		[0 ... MAX_S6E3FAC_VRR - 1] = { 0x00 },
	},
};

static u8 mu1s_dimming_transition_table[MAX_S6E3FAC_SMOOTH_DIM][MAX_S6E3FAC_VRR][1] = {
	[S6E3FAC_SMOOTH_DIM_USE] = {
		[S6E3FAC_VRR_60NS ... S6E3FAC_VRR_120HS - 1] = { 0x28 },
		[S6E3FAC_VRR_120HS ... S6E3FAC_VRR_96HS - 1] = { 0x28 },
		[S6E3FAC_VRR_96HS ... MAX_S6E3FAC_VRR - 1] = { 0x20 },
	},
	[S6E3FAC_SMOOTH_DIM_NOT_USE] = {
		[0 ... MAX_S6E3FAC_VRR - 1] = { 0x20 },
	},
};

static u8 mu1s_dimming_transition_2_table[MAX_S6E3FAC_SMOOTH_DIM][MAX_S6E3FAC_VRR][1] = {
	[S6E3FAC_SMOOTH_DIM_USE] = {
		[S6E3FAC_VRR_60NS ... S6E3FAC_VRR_120HS - 1] = { 0x70 },
		[S6E3FAC_VRR_120HS ... S6E3FAC_VRR_96HS - 1] = { 0x70 },
		[S6E3FAC_VRR_96HS ... MAX_S6E3FAC_VRR - 1] = { 0x20 },
	},
	[S6E3FAC_SMOOTH_DIM_NOT_USE] = {
		[0 ... MAX_S6E3FAC_VRR - 1] = { 0x20 },
	},
};

static u8 mu1s_acl_opr_table[MAX_OLED_BR_HBM][MAX_S6E3FAC_ACL_OPR][1] = {
	[OLED_BR_HBM_OFF] = {
		[S6E3FAC_ACL_OPR_0] = { 0x00 }, /* adaptive_control 0, 0% */
		[S6E3FAC_ACL_OPR_1] = { 0x01 }, /* adaptive_control 1, 8% */
		[S6E3FAC_ACL_OPR_2] = { 0x03 }, /* adaptive_control 2, 30% */
		[S6E3FAC_ACL_OPR_3] = { 0x02 }, /* adaptive_control 3, 15% */
	},
	[OLED_BR_HBM_ON] = {
		[S6E3FAC_ACL_OPR_0] = { 0x00 }, /* adaptive_control 0, 0% */
		[S6E3FAC_ACL_OPR_1] = { 0x01 }, /* adaptive_control 1, 8% */
		[S6E3FAC_ACL_OPR_2] = { 0x01 }, /* adaptive_control 2, 8% */
		[S6E3FAC_ACL_OPR_3] = { 0x01 }, /* adaptive_control 3, 8% */
	},
};

static u8 mu1s_tset_table[256][1] = {
    /* 0 ~ 127 */
    { 0x00 }, { 0x01 }, { 0x02 }, { 0x03 }, { 0x04 }, { 0x05 }, { 0x06 }, { 0x07 },
    { 0x08 }, { 0x09 }, { 0x0A }, { 0x0B }, { 0x0C }, { 0x0D }, { 0x0E }, { 0x0F },
    { 0x10 }, { 0x11 }, { 0x12 }, { 0x13 }, { 0x14 }, { 0x15 }, { 0x16 }, { 0x17 },
    { 0x18 }, { 0x19 }, { 0x1A }, { 0x1B }, { 0x1C }, { 0x1D }, { 0x1E }, { 0x1F },
    { 0x20 }, { 0x21 }, { 0x22 }, { 0x23 }, { 0x24 }, { 0x25 }, { 0x26 }, { 0x27 },
    { 0x28 }, { 0x29 }, { 0x2A }, { 0x2B }, { 0x2C }, { 0x2D }, { 0x2E }, { 0x2F },
    { 0x30 }, { 0x31 }, { 0x32 }, { 0x33 }, { 0x34 }, { 0x35 }, { 0x36 }, { 0x37 },
    { 0x38 }, { 0x39 }, { 0x3A }, { 0x3B }, { 0x3C }, { 0x3D }, { 0x3E }, { 0x3F },
    { 0x40 }, { 0x41 }, { 0x42 }, { 0x43 }, { 0x44 }, { 0x45 }, { 0x46 }, { 0x47 },
    { 0x48 }, { 0x49 }, { 0x4A }, { 0x4B }, { 0x4C }, { 0x4D }, { 0x4E }, { 0x4F },
    { 0x50 }, { 0x51 }, { 0x52 }, { 0x53 }, { 0x54 }, { 0x55 }, { 0x56 }, { 0x57 },
    { 0x58 }, { 0x59 }, { 0x5A }, { 0x5B }, { 0x5C }, { 0x5D }, { 0x5E }, { 0x5F },
    { 0x60 }, { 0x61 }, { 0x62 }, { 0x63 }, { 0x64 }, { 0x65 }, { 0x66 }, { 0x67 },
    { 0x68 }, { 0x69 }, { 0x6A }, { 0x6B }, { 0x6C }, { 0x6D }, { 0x6E }, { 0x6F },
    { 0x70 }, { 0x71 }, { 0x72 }, { 0x73 }, { 0x74 }, { 0x75 }, { 0x76 }, { 0x77 },
    { 0x78 }, { 0x79 }, { 0x7A }, { 0x7B }, { 0x7C }, { 0x7D }, { 0x7E }, { 0x7F },
    /* 0 ~ -127 */
    { 0x80 }, { 0x81 }, { 0x82 }, { 0x83 }, { 0x84 }, { 0x85 }, { 0x86 }, { 0x87 },
    { 0x88 }, { 0x89 }, { 0x8A }, { 0x8B }, { 0x8C }, { 0x8D }, { 0x8E }, { 0x8F },
    { 0x90 }, { 0x91 }, { 0x92 }, { 0x93 }, { 0x94 }, { 0x95 }, { 0x96 }, { 0x97 },
    { 0x98 }, { 0x99 }, { 0x9A }, { 0x9B }, { 0x9C }, { 0x9D }, { 0x9E }, { 0x9F },
    { 0xA0 }, { 0xA1 }, { 0xA2 }, { 0xA3 }, { 0xA4 }, { 0xA5 }, { 0xA6 }, { 0xA7 },
    { 0xA8 }, { 0xA9 }, { 0xAA }, { 0xAB }, { 0xAC }, { 0xAD }, { 0xAE }, { 0xAF },
    { 0xB0 }, { 0xB1 }, { 0xB2 }, { 0xB3 }, { 0xB4 }, { 0xB5 }, { 0xB6 }, { 0xB7 },
    { 0xB8 }, { 0xB9 }, { 0xBA }, { 0xBB }, { 0xBC }, { 0xBD }, { 0xBE }, { 0xBF },
    { 0xC0 }, { 0xC1 }, { 0xC2 }, { 0xC3 }, { 0xC4 }, { 0xC5 }, { 0xC6 }, { 0xC7 },
    { 0xC8 }, { 0xC9 }, { 0xCA }, { 0xCB }, { 0xCC }, { 0xCD }, { 0xCE }, { 0xCF },
    { 0xD0 }, { 0xD1 }, { 0xD2 }, { 0xD3 }, { 0xD4 }, { 0xD5 }, { 0xD6 }, { 0xD7 },
    { 0xD8 }, { 0xD9 }, { 0xDA }, { 0xDB }, { 0xDC }, { 0xDD }, { 0xDE }, { 0xDF },
    { 0xE0 }, { 0xE1 }, { 0xE2 }, { 0xE3 }, { 0xE4 }, { 0xE5 }, { 0xE6 }, { 0xE7 },
    { 0xE8 }, { 0xE9 }, { 0xEA }, { 0xEB }, { 0xEC }, { 0xED }, { 0xEE }, { 0xEF },
    { 0xF0 }, { 0xF1 }, { 0xF2 }, { 0xF3 }, { 0xF4 }, { 0xF5 }, { 0xF6 }, { 0xF7 },
    { 0xF8 }, { 0xF9 }, { 0xFA }, { 0xFB }, { 0xFC }, { 0xFD }, { 0xFE }, { 0xFF },
};

static u8 mu1s_lpm_nit_aor_table[4][2] = {
	/* LPM 1NIT */
	{ 0x00, 0x22 },
	/* LPM 10NIT */
	{ 0x01, 0x55 },
	/* LPM 30NIT */
	{ 0x04, 0x00 },
	/* LPM 60NIT */
	{ 0x07, 0xFF },
};

static u8 mu1s_lpm_wrdisbv_table[4][2] = {
	/* LPM 2NIT */
	{ 0x00, 0x08 },
	/* LPM 10NIT */
	{ 0x00, 0x2A },
	/* LPM 30NIT */
	{ 0x00, 0x7B },
	/* LPM 60NIT */
	{ 0x00, 0xF7 },
};

static u8 mu1s_dia_onoff_table[][1] = {
	{ 0x00 }, /* dia off */
	{ 0x02 }, /* dia on */
};

static u8 mu1s_irc_mode_table[MAX_IRC_MODE][35] = {
	[IRC_MODE_MODERATO] = {
		0x65, 0xFF, 0x01, 0x00, 0x80, 0x01, 0xFF, 0x10,
		0xB8, 0xFF, 0x10, 0xFF, 0x64, 0x86, 0x0B, 0x06,
		0x06, 0x06, 0x1B, 0x1B, 0x1B, 0x24, 0x24, 0x24,
		0x2A, 0x2A, 0x2A, 0x2D, 0x2D, 0x2D, 0x9B, 0x2F,
		0xE0, 0x18, 0x64
	},
	[IRC_MODE_FLAT_GAMMA] = {
		0x25, 0xB2, 0x03, 0x5D, 0x80, 0x01, 0xFF, 0x10,
		0xB8, 0xFF, 0x10, 0xFF, 0x64, 0x86, 0x0B, 0x06,
		0x06, 0x06, 0x1B, 0x1B, 0x1B, 0x24, 0x24, 0x24,
		0x2A, 0x2A, 0x2A, 0x2D, 0x2D, 0x2D, 0x9B, 0x2F,
		0xE0, 0x18, 0x64
	},
};

static u8 mu1s_aid_table[MAX_VRR_MODE][1] = {
	[VRR_NORMAL_MODE] = { 0x41 },
	[VRR_HS_MODE] = { 0x21 },
};

static u8 mu1s_osc_table[MAX_S6E3FAC_VRR][1] = {
	[S6E3FAC_VRR_60NS] = { 0x01 },
	[S6E3FAC_VRR_48NS] = { 0x01 },
	[S6E3FAC_VRR_120HS ... S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11] = { 0x01 },
	[S6E3FAC_VRR_96HS ... S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x01 },
	[S6E3FAC_VRR_60HS ... S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1] = { 0x00 },
	[S6E3FAC_VRR_48HS ... S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4] = { 0x00 },
};

static u8 mu1s_fps_table[MAX_S6E3FAC_VRR][1] = {
	[S6E3FAC_VRR_60NS] = { 0x18 },
	[S6E3FAC_VRR_48NS] = { 0x18 },
	[S6E3FAC_VRR_120HS ... S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11] = { 0x00 },
	[S6E3FAC_VRR_96HS ... S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x00 },
	[S6E3FAC_VRR_60HS ... S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1] = { 0x08 },
	[S6E3FAC_VRR_48HS ... S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4] = { 0x08 },
};

static u8 mu1s_vfp_table[MAX_S6E3FAC_VRR][2] = {
	[S6E3FAC_VRR_60NS] = { 0x00, 0x0C },
	[S6E3FAC_VRR_48NS] = { 0x02, 0x5C },
	[S6E3FAC_VRR_120HS ... S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11] = { 0x00, 0x0C },
	[S6E3FAC_VRR_96HS ... S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x02, 0x5C },
	[S6E3FAC_VRR_60HS ... S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1] = { 0x00, 0x0C },
	[S6E3FAC_VRR_48HS ... S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4] = { 0x02, 0x5C },
};

static u8 mu1s_aor_table[MAX_S6E3FAC_VRR][32] = {
	[S6E3FAC_VRR_60NS] = {
		0x09, 0x40, 0x09, 0x40, 0x09, 0x40, 0x09, 0x40,
		0x09, 0x40, 0x09, 0x40, 0x09, 0x40, 0x09, 0x40,
		0x09, 0x40, 0x09, 0x40, 0x09, 0x40, 0x08, 0x54,
		0x08, 0x54, 0x01, 0x7A, 0x01, 0x7A, 0x01, 0x7A
	},
	[S6E3FAC_VRR_48NS] = {
		0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90,
		0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90,
		0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90, 0x0A, 0x68,
		0x0A, 0x68, 0x01, 0xDA, 0x01, 0xDA, 0x01, 0xDA
	},
	[S6E3FAC_VRR_120HS ... S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11] = {
		0x09, 0x40, 0x09, 0x40, 0x09, 0x40, 0x09, 0x40,
		0x09, 0x40, 0x09, 0x40, 0x09, 0x40, 0x09, 0x40,
		0x09, 0x40, 0x09, 0x40, 0x09, 0x40, 0x08, 0x54,
		0x08, 0x54, 0x01, 0x7A, 0x01, 0x7A, 0x01, 0x7A
	},
	[S6E3FAC_VRR_96HS ... S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1] = {
		0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90,
		0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90,
		0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90, 0x0A, 0x68,
		0x0A, 0x68, 0x01, 0xDA, 0x01, 0xDA, 0x01, 0xDA
	},
	[S6E3FAC_VRR_60HS ... S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1] = {
		0x09, 0x40, 0x09, 0x40, 0x09, 0x40, 0x09, 0x40,
		0x09, 0x40, 0x09, 0x40, 0x09, 0x40, 0x09, 0x40,
		0x09, 0x40, 0x09, 0x40, 0x09, 0x40, 0x08, 0x54,
		0x08, 0x54, 0x01, 0x7A, 0x01, 0x7A, 0x01, 0x7A
	},
	[S6E3FAC_VRR_48HS ... S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4] = {
		0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90,
		0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90,
		0x0B, 0x90, 0x0B, 0x90, 0x0B, 0x90, 0x0A, 0x68,
		0x0A, 0x68, 0x01, 0xDA, 0x01, 0xDA, 0x01, 0xDA
	},
};

static u8 mu1s_te_fixed_mode_timing_table[MAX_S6E3FAC_VRR][2] = {
	[S6E3FAC_VRR_60NS] = { 0x09, 0x12 },
	[S6E3FAC_VRR_48NS] = { 0x09, 0x12 },
	[S6E3FAC_VRR_120HS] = { 0x09, 0x12 },
	[S6E3FAC_VRR_60HS_120HS_TE_HW_SKIP_1] = { 0x09, 0x12 },
	[S6E3FAC_VRR_30HS_120HS_TE_HW_SKIP_3] = { 0x09, 0x12 },
	[S6E3FAC_VRR_24HS_120HS_TE_HW_SKIP_4] = { 0x09, 0x12 },
	[S6E3FAC_VRR_10HS_120HS_TE_HW_SKIP_11] = { 0x09, 0x12 },
	[S6E3FAC_VRR_96HS] = { 0x09, 0x12 },
	[S6E3FAC_VRR_48HS_96HS_TE_HW_SKIP_1] = { 0x09, 0x12 },
	[S6E3FAC_VRR_60HS] = { 0x09, 0x23 },
	[S6E3FAC_VRR_30HS_60HS_TE_HW_SKIP_1] = { 0x09, 0x12 },
	[S6E3FAC_VRR_48HS] = { 0x09, 0x23 },
	[S6E3FAC_VRR_24HS_48HS_TE_HW_SKIP_1] = { 0x09, 0x12 },
	[S6E3FAC_VRR_10HS_48HS_TE_HW_SKIP_4] = { 0x09, 0x12 },
};

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
static u8 mu1s_vddm_table[MAX_VDDM][1] = {
	[VDDM_ORIG] = {0x00}, // VDDM ORIGINAL
	[VDDM_LV] = {0x04}, // VDDM LOW VOLTAGE
	[VDDM_HV] = {0x08}, // VDDM HIGH VOLTAGE
};
static u8 mu1s_gram_img_pattern_table[MAX_GCT_PATTERN][1] = {
	[GCT_PATTERN_NONE] = {0x00},
	[GCT_PATTERN_1] = {0x07},
	[GCT_PATTERN_2] = {0x05},
};
/* switch pattern_1 and pattern_2 */
static u8 mu1s_gram_inv_img_pattern_table[MAX_GCT_PATTERN][1] = {
	[GCT_PATTERN_NONE] = {0x00},
	[GCT_PATTERN_1] = {0x05},
	[GCT_PATTERN_2] = {0x07},
};
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 mu1s_vgh_table[][1] = {
	[XTALK_OFF] = { 0x19 }, /* VGH (x-talk off)*/
	[XTALK_ON] = { 0x11 }, /* VGH (x-talk on) */
};
#endif

#ifdef CONFIG_USDM_PANEL_MAFPC
static u8 mu1s_mafpc_ena_table[][1] = {
	{ 0x00 },
	{ 0x11 },
};
#endif

static u8 mu1s_osc_94500_set_table[MAX_S6E3FAC_DDI_REV][1] = {
	[S6E3FAC_DDI_REV_EVT0] = { 0x01 },
	[S6E3FAC_DDI_REV_EVT0_OSC] = { 0x03 },
	[S6E3FAC_DDI_REV_EVT1] = { 0x03 },
};

static u8 mu1s_osc_96500_set_table[MAX_S6E3FAC_DDI_REV][1] = {
	[S6E3FAC_DDI_REV_EVT0] = { 0x00 },
	[S6E3FAC_DDI_REV_EVT0_OSC] = { 0x02 },
	[S6E3FAC_DDI_REV_EVT1] = { 0x02 },
};

static u8 mu1s_ffc_table[MAX_S6E3FAC_MU1S_HS_CLK][2] = {
	[S6E3FAC_MU1S_HS_CLK_1362] = {0x2D, 0x58}, // FFC for HS: 1362
	[S6E3FAC_MU1S_HS_CLK_1328] = {0x2E, 0x82}, // FFC for HS: 1328
	[S6E3FAC_MU1S_HS_CLK_1368] = {0x2D, 0x25}, // FFC for HS: 1368
};

static struct maptbl mu1s_maptbl[MAX_MAPTBL] = {
	[GAMMA_MODE2_MAPTBL] = __OLED_MAPTBL_OVERRIDE_INIT_INITIALIZER(mu1s_brt_table, &DDI_FUNC(S6E3FAC_MAPTBL_INIT_GAMMA_MODE2_BRT), S6E3FAC_MU1S_BR_INDEX_PROPERTY),
#ifdef CONFIG_USDM_PANEL_HMD
	[HMD_GAMMA_MAPTBL] = __OLED_MAPTBL_OVERRIDE_INIT_INITIALIZER(mu1s_hmd_brt_table, &DDI_FUNC(S6E3FAC_MAPTBL_INIT_GAMMA_MODE2_HMD_BRT), S6E3FAC_MU1S_HMD_BR_INDEX_PROPERTY),
#endif
	[TSET_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_tset_table, OLED_TSET_PROPERTY),
	[DIMMING_FRAME_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_dimming_frame_table, S6E3FAC_SMOOTH_DIM_PROPERTY, S6E3FAC_VRR_PROPERTY),
	[DIMMING_TRANSITION_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_dimming_transition_table, S6E3FAC_SMOOTH_DIM_PROPERTY, S6E3FAC_VRR_PROPERTY),
	[DIMMING_TRANSITION_2_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_dimming_transition_2_table, S6E3FAC_SMOOTH_DIM_PROPERTY, S6E3FAC_VRR_PROPERTY),
	[ACL_OPR_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_acl_opr_table, OLED_BR_HBM_PROPERTY, S6E3FAC_ACL_OPR_PROPERTY),
	[ELVSS_CAL_OFFSET_MAPTBL] = __OLED_MAPTBL_COPY_ONLY_INITIALIZER(mu1s_elvss_cal_offset_table, &DDI_FUNC(S6E3FAC_MAPTBL_COPY_ELVSS_CAL)),
	[DIA_ONOFF_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_dia_onoff_table, PANEL_PROPERTY_DIA_MODE),
	[IRC_MODE_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_irc_mode_table, PANEL_PROPERTY_IRC_MODE),
	[LPM_NIT_MAPTBL] = __OLED_MAPTBL_OVERRIDE_INIT_INITIALIZER(mu1s_lpm_nit_aor_table, &DDI_FUNC(S6E3FAC_MAPTBL_INIT_LPM_BRT), S6E3FAC_MU1S_LPM_BR_INDEX_PROPERTY),
	[LPM_WRDISBV_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_lpm_wrdisbv_table, S6E3FAC_MU1S_LPM_BR_INDEX_PROPERTY),

	[FPS_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_fps_table, S6E3FAC_VRR_PROPERTY),
	[AID_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_aid_table, PANEL_PROPERTY_PANEL_REFRESH_MODE),
	[OSC_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_osc_table, S6E3FAC_VRR_PROPERTY),
	[VFP_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_vfp_table, S6E3FAC_VRR_PROPERTY),
	[AOR_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_aor_table, S6E3FAC_VRR_PROPERTY),
	[TE_FIXED_MODE_TIMING_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_te_fixed_mode_timing_table, S6E3FAC_VRR_PROPERTY),
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	[VDDM_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_vddm_table, PANEL_PROPERTY_GCT_VDDM),
	[GRAM_IMG_MAPTBL] = __OLED_MAPTBL_OVERRIDE_INIT_INITIALIZER(mu1s_gram_img_pattern_table, &DDI_FUNC(S6E3FAC_MAPTBL_INIT_GRAM_IMG_PATTERN), PANEL_PROPERTY_GCT_PATTERN),
	[GRAM_INV_IMG_MAPTBL] = __OLED_MAPTBL_OVERRIDE_INIT_INITIALIZER(mu1s_gram_inv_img_pattern_table, &DDI_FUNC(S6E3FAC_MAPTBL_INIT_GRAM_IMG_PATTERN), PANEL_PROPERTY_GCT_PATTERN),
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
	[VGH_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_vgh_table, PANEL_PROPERTY_XTALK_MODE),
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
	[MAFPC_ENA_MAPTBL] = __OLED_MAPTBL_COPY_ONLY_INITIALIZER(mu1s_mafpc_enable, &DDI_FUNC(S6E3FAC_MAPTBL_COPY_MAFPC_ENABLE)),
	[MAFPC_CTRL_MAPTBL] = __OLED_MAPTBL_COPY_ONLY_INITIALIZER(mu1s_mafpc_ctrl, &DDI_FUNC(S6E3FAC_MAPTBL_COPY_MAFPC_CTRL)),
	[MAFPC_SCALE_MAPTBL] = __OLED_MAPTBL_COPY_ONLY_INITIALIZER(mu1s_mafpc_scale, &DDI_FUNC(S6E3FAC_MAPTBL_COPY_MAFPC_SCALE)),
#endif
	[SET_OSC1_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_osc_94500_set_table, S6E3FAC_DDI_REV_PROPERTY),
	[SET_OSC2_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_osc_96500_set_table, S6E3FAC_DDI_REV_PROPERTY),
	[SET_FFC_MAPTBL] = __OLED_MAPTBL_DEFAULT_INITIALIZER(mu1s_ffc_table, S6E3FAC_MU1S_HS_CLK_PROPERTY),
};


/* ===================================================================================== */
/* ============================== [S6E3FAC COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 MU1S_KEY1_ENABLE[] = { 0x9F, 0xA5, 0xA5 };
static u8 MU1S_KEY2_ENABLE[] = { 0xF0, 0x5A, 0x5A };
static u8 MU1S_KEY3_ENABLE[] = { 0xFC, 0x5A, 0x5A };
static u8 MU1S_KEY1_DISABLE[] = { 0x9F, 0x5A, 0x5A };
static u8 MU1S_KEY2_DISABLE[] = { 0xF0, 0xA5, 0xA5 };
static u8 MU1S_KEY3_DISABLE[] = { 0xFC, 0xA5, 0xA5 };
static u8 MU1S_SLEEP_OUT[] = { 0x11 };
static u8 MU1S_SLEEP_IN[] = { 0x10 };
static u8 MU1S_DISPLAY_OFF[] = { 0x28 };
static u8 MU1S_DISPLAY_ON[] = { 0x29 };

static u8 MU1S_DSC[] = { 0x01 };
static u8 MU1S_PPS[] = {
	/* 1080 * 2340, 117 */
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
	0x00
};

static u8 MU1S_TE_ON[] = { 0x35, 0x00 };

static u8 MU1S_LPM_TE_SETTING[] = { 0xB9, 0x09, 0x2C, 0x00, 0x0F };

static u8 MU1S_TE_FIXED_SET_1[] = {
	0xBD,
	0x21, 0x81
};

static u8 MU1S_TE_FIXED_SET_2[] = {
	0xBD,
	0x00, 0x02
};

static u8 MU1S_SMOOTH_DIMMING_INIT[] = {
	0x94,
	0x01,
};

static u8 MU1S_SMOOTH_DIMMING[] = {
	0x94,
	0x18,
};

static u8 MU1S_TSET[] = {
	0xB1,
	0x01,	/* temperature 25 */
};

static u8 MU1S_ECC_ON[] = {
	0xF8, 0x00
};

static u8 MU1S_SSR_ON[] = {
	0xF6, 0x22
};

static u8 MU1S_SSR_SOURCE[] = {
	0xF6, 0x01
};

static u8 MU1S_SSR_SENSING[] = {
	0xF6, 0x25
};

static u8 MU1S_SSR_SETTING[] = {
	0xF4, 0x44
};

static u8 MU1S_ERR_FG_1[] = {
	0xE5, 0x15
};

static u8 MU1S_ERR_FG_2[] = {
	0xF4, 0x10
};

static u8 MU1S_ERR_FG_3[] = {
	0xED, 0x00, 0x40, 0x42, 0x01
};

static u8 MU1S_ISC[] = {
	0xF6, 0x40
};

static u8 MU1S_LPM_ENTER_NIT[] = {
	0x98, 0x01, 0x09, 0x40, 0x02, 0x01, 0x7A
};

static u8 MU1S_LPM_EXIT_NIT[] = {
	0x98, 0x00
};

static u8 MU1S_LPM_ENTER_OSC[] = {
	0xF2, 0xB2
};

static u8 MU1S_LPM_ENTER_FPS[] = {
	0x60, 0x00
};

static u8 MU1S_LPM_ENTER_SEAMLESS_CTRL[] = {
	0xBB, 0x38
};

static u8 MU1S_LPM_EXIT_SEAMLESS_CTRL[] = {
	0xBB, 0x30
};

static u8 MU1S_LPM_CONTROL2[] = {
	0x51, 0x07, 0xFF
};

static u8 MU1S_LPM_ON[] = {
	0x53, 0x24
};

#ifdef CONFIG_USDM_DDI_CMDLOG
static u8 MU1S_CMDLOG_ENABLE[] = { 0xF7, 0x80 };
static u8 MU1S_CMDLOG_DISABLE[] = { 0xF7, 0x00 };
static u8 MU1S_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x8F };
#else
static u8 MU1S_GAMMA_UPDATE_ENABLE[] = { 0xF7, 0x0F };
#endif

static u8 MU1S_TESTMODE_SOURCE_ON[] = { 0xF6, 0x00 };

static u8 MU1S_MCD_ON_01[] = { 0xF4, 0x1E };
static u8 MU1S_MCD_ON_02[] = { 0xF6, 0x02 };
static u8 MU1S_MCD_ON_03[] = { 0xF6, 0x00 };
static u8 MU1S_MCD_ON_04[] = { 0xCB, 0x40 };

static u8 MU1S_MCD_OFF_01[] = { 0xF4, 0x19 };
/* MU1S_MCD_OFF_02 is replaced to MU1S_TESTMODE_SOURCE_ON */
static u8 MU1S_MCD_OFF_03[] = { 0xF6, 0x08 };
static u8 MU1S_MCD_OFF_04[] = { 0xCB, 0x00 };

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static u8 MU1S_GRAYSPOT_ON_01[] = {
	0xF6, 0x02
};
static u8 MU1S_GRAYSPOT_ON_02[] = {
	0xF6, 0x00
};
static u8 MU1S_GRAYSPOT_ON_03[] = {
	0xB1, 0x19
};
static u8 MU1S_GRAYSPOT_ON_04[] = {
	0x94, 0x1F
};

/* MU1S_GRAYSPOT_OFF_01 is replaced to MU1S_TESTMODE_SOURCE_ON */
static u8 MU1S_GRAYSPOT_OFF_02[] = {
	0xF6, 0x08
};
static u8 MU1S_GRAYSPOT_OFF_03[] = {
	0xB1, 0x00
};
static u8 MU1S_GRAYSPOT_OFF_04[] = {
	0x94, 0x1F
};
#endif

/* Micro Crack Test Sequence */
#ifdef CONFIG_USDM_FACTORY_MST_TEST
static u8 MU1S_MST_ON_01[] = {
	0xCB,
	0x1B, 0x2E, 0x00, 0x00, 0x00, 0x10, 0x1B, 0x2E
};
static u8 MU1S_MST_ON_02[] = {
	0xF6, 0x00
};
static u8 MU1S_MST_ON_03[] = {
	0xF6,
	0xF7, 0x3C
};
static u8 MU1S_MST_ON_04[] = {
	0xF6, 0x00
};
static u8 MU1S_MST_ON_05[] = {
	0xBF,
	0x33, 0x25, 0xFF, 0x00, 0x00, 0x10
};

static u8 MU1S_MST_OFF_01[] = {
	0xCB,
	0x12, 0x38, 0x00, 0x00, 0x00, 0x10, 0x12, 0x38
};
/* MU1S_MST_OFF_02 is replaced to MU1S_TESTMODE_SOURCE_ON */
static u8 MU1S_MST_OFF_03[] = {
	0xF6,
	0xFF, 0x97
};
static u8 MU1S_MST_OFF_04[] = {
	0xF6, 0x08
};
static u8 MU1S_MST_OFF_05[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10
};
#endif

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
static u8 MU1S_SW_RESET[] = { 0x01 };
static u8 MU1S_GCT_DSC[] = { 0x9D, 0x01 };
static u8 MU1S_GCT_PPS[] = { 0x9E,
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
	0x00
};
static u8 MU1S_ECC_SETTING[] = { 0xF8, 0x00 };
static u8 MU1S_VDDM_INIT[] = { 0xFE, 0x14 };
static u8 MU1S_VDDM_INIT_2[] = { 0xFE, 0x7A };
static u8 MU1S_VDDM_ORIG[] = { 0xD7, 0x00 };
static u8 MU1S_VDDM_VOLT[] = { 0xD7, 0x00 };
static u8 MU1S_GCT_WRDISBV[] = { 0x51, 0x07, 0xFF };
static u8 MU1S_GCT_WRCTRLD[] = { 0x53, 0x20 };
static u8 MU1S_GRAM_CHKSUM_START[] = { 0x2C, 0x00 };
static u8 MU1S_GRAM_IMG_PATTERN_ON[] = { 0xBE, 0x00 };
static u8 MU1S_GRAM_INV_IMG_PATTERN_ON[] = { 0xBE, 0x00 };
static u8 MU1S_GRAM_IMG_PATTERN_OFF[] = { 0xBE, 0x00 };
#endif
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static u8 MU1S_DECODER_TEST_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 MU1S_DECODER_TEST_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x23 };
static u8 MU1S_DECODER_TEST_2C[] = { 0x2C, 0x00 };

static u8 MU1S_DECODER_TEST_1[] = { 0xBE, 0x05 };
static u8 MU1S_DECODER_TEST_2[] = { 0xD8, 0x13 };
static u8 MU1S_DECODER_TEST_RETURN_1[] = { 0xBE, 0x00 };
static u8 MU1S_DECODER_TEST_RETURN_2[] = { 0xD8, 0x00 };
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
static u8 MU1S_XTALK_MODE[] = { 0xF4, 0x19 };
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static u8 MU1S_CCD_ENABLE[] = { 0xCC, 0x03 };
static u8 MU1S_CCD_DISABLE[] = { 0xCC, 0x02 };
#endif

static DEFINE_STATIC_PACKET(mu1s_level1_key_enable, DSI_PKT_TYPE_WR, MU1S_KEY1_ENABLE, 0);
static DEFINE_STATIC_PACKET(mu1s_level2_key_enable, DSI_PKT_TYPE_WR, MU1S_KEY2_ENABLE, 0);
static DEFINE_STATIC_PACKET(mu1s_level3_key_enable, DSI_PKT_TYPE_WR, MU1S_KEY3_ENABLE, 0);
static DEFINE_STATIC_PACKET(mu1s_level1_key_disable, DSI_PKT_TYPE_WR, MU1S_KEY1_DISABLE, 0);
static DEFINE_STATIC_PACKET(mu1s_level2_key_disable, DSI_PKT_TYPE_WR, MU1S_KEY2_DISABLE, 0);
static DEFINE_STATIC_PACKET(mu1s_level3_key_disable, DSI_PKT_TYPE_WR, MU1S_KEY3_DISABLE, 0);

static DEFINE_STATIC_PACKET(mu1s_sleep_out, DSI_PKT_TYPE_WR, MU1S_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(mu1s_sleep_in, DSI_PKT_TYPE_WR, MU1S_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(mu1s_display_on, DSI_PKT_TYPE_WR, MU1S_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(mu1s_display_off, DSI_PKT_TYPE_WR, MU1S_DISPLAY_OFF, 0);

static DEFINE_STATIC_PACKET(mu1s_te_on, DSI_PKT_TYPE_WR, MU1S_TE_ON, 0);

static u8 MU1S_TE_SETTING_1[] = { 0xB9, 0x04 };
static DEFINE_STATIC_PACKET(mu1s_te_setting_1, DSI_PKT_TYPE_WR, MU1S_TE_SETTING_1, 0);
static u8 MU1S_TE_SETTING_2[] = { 0xB9, 0x09, 0x12, 0x00, 0x0F };
static DEFINE_PKTUI(mu1s_te_setting_2, &mu1s_maptbl[TE_FIXED_MODE_TIMING_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_te_setting_2, DSI_PKT_TYPE_WR, MU1S_TE_SETTING_2, 0);

static DEFINE_STATIC_PACKET(mu1s_lpm_te_setting, DSI_PKT_TYPE_WR, MU1S_LPM_TE_SETTING, 0);
static DEFINE_STATIC_PACKET(mu1s_te_fixed_set_1, DSI_PKT_TYPE_WR, MU1S_TE_FIXED_SET_1, 0);
static DEFINE_STATIC_PACKET(mu1s_te_fixed_set_2, DSI_PKT_TYPE_WR, MU1S_TE_FIXED_SET_2, 0x2F);

static DEFINE_STATIC_PACKET(mu1s_dsc, DSI_PKT_TYPE_WR_COMP, MU1S_DSC, 0);
static DEFINE_STATIC_PACKET(mu1s_pps, DSI_PKT_TYPE_WR_PPS, MU1S_PPS, 0);

static DEFINE_FUNC_BASED_COND(mu1s_cond_is_first_set_bl, &OLED_FUNC(OLED_COND_IS_FIRST_SET_BL));

static struct panel_expr_data is_120hs_rule_item[] = {
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 120, COND_RULE_MASK_ALL),
	PANEL_EXPR_OPERATOR(AND),
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE, EQ, VRR_HS_MODE, COND_RULE_MASK_ALL),
};
static DEFINE_CONDITION(mu1s_cond_is_120hs_based_fps, is_120hs_rule_item);

static struct panel_expr_data is_60hs_rule_item[] = {
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 60, COND_RULE_MASK_ALL),
	PANEL_EXPR_OPERATOR(AND),
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE, EQ, VRR_HS_MODE, COND_RULE_MASK_ALL),
};
static DEFINE_CONDITION(mu1s_cond_is_60hs_based_fps, is_60hs_rule_item);

static struct panel_expr_data is_60ns_rule_item[] = {
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 60, COND_RULE_MASK_ALL),
	PANEL_EXPR_OPERATOR(AND),
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE, EQ, VRR_NORMAL_MODE, COND_RULE_MASK_ALL),
};
static DEFINE_CONDITION(mu1s_cond_is_60ns_based_fps, is_60ns_rule_item);

static struct panel_expr_data is_96hs_60hs_48hs_rule_item[] = {
	/* 96hs */
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 96, COND_RULE_MASK_ALL),
	PANEL_EXPR_OPERATOR(AND),
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE, EQ, VRR_HS_MODE, COND_RULE_MASK_ALL),

	PANEL_EXPR_OPERATOR(OR),

	/* 60hs */
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 60, COND_RULE_MASK_ALL),
	PANEL_EXPR_OPERATOR(AND),
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE, EQ, VRR_HS_MODE, COND_RULE_MASK_ALL),

	PANEL_EXPR_OPERATOR(OR),

	/* 48hs */
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 48, COND_RULE_MASK_ALL),
	PANEL_EXPR_OPERATOR(AND),
	__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE, EQ, VRR_HS_MODE, COND_RULE_MASK_ALL),
};
static DEFINE_CONDITION(mu1s_cond_is_96hs_60hs_48hs_based_fps, is_96hs_60hs_48hs_rule_item);

static DEFINE_STATIC_PACKET(mu1s_lpm_enter_nit, DSI_PKT_TYPE_WR, MU1S_LPM_ENTER_NIT, 0x4E);

static u8 MU1S_LPM_CONTROL[] = { 0x98, 0x00 };
static DEFINE_STATIC_PACKET(mu1s_lpm_control, DSI_PKT_TYPE_WR, MU1S_LPM_CONTROL, 0x4E);
static DEFINE_STATIC_PACKET(mu1s_lpm_exit_nit, DSI_PKT_TYPE_WR, MU1S_LPM_EXIT_NIT, 0x4E);

static DEFINE_STATIC_PACKET(mu1s_lpm_enter_seamless_ctrl, DSI_PKT_TYPE_WR, MU1S_LPM_ENTER_SEAMLESS_CTRL, 0);
static DEFINE_STATIC_PACKET(mu1s_lpm_exit_seamless_ctrl, DSI_PKT_TYPE_WR, MU1S_LPM_EXIT_SEAMLESS_CTRL, 0);
static DEFINE_PKTUI(mu1s_lpm_control2, &mu1s_maptbl[LPM_NIT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_lpm_control2, DSI_PKT_TYPE_WR, MU1S_LPM_CONTROL2, 0);

static DEFINE_RULE_BASED_COND(mu1s_cond_is_panel_state_not_lpm,
		PANEL_PROPERTY_PANEL_STATE, NE, PANEL_STATE_ALPM);
static DEFINE_FUNC_BASED_COND(mu1s_cond_is_vsync_for_mode_change, &DDI_FUNC(S6E3FAC_COND_IS_VSYNC_FOR_MODE_CHANGE));

static DEFINE_STATIC_PACKET(mu1s_lpm_on, DSI_PKT_TYPE_WR, MU1S_LPM_ON, 0);

static DEFINE_STATIC_PACKET(mu1s_smooth_dimming_init, DSI_PKT_TYPE_WR, MU1S_SMOOTH_DIMMING_INIT, 0x0E);
static DEFINE_PKTUI(mu1s_smooth_dimming, &mu1s_maptbl[DIMMING_FRAME_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_smooth_dimming, DSI_PKT_TYPE_WR, MU1S_SMOOTH_DIMMING, 0x0E);

static u8 MU1S_DIA_ONOFF[] = { 0x92, 0x00 };
static DEFINE_PKTUI(mu1s_dia_onoff, &mu1s_maptbl[DIA_ONOFF_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_dia_onoff, DSI_PKT_TYPE_WR, MU1S_DIA_ONOFF, 0x195);

static u8 MU1S_IRC_MODE[] = {
	0x92,
	0x65, 0xFF, 0x01, 0x00, 0x80, 0x01, 0xFF, 0x10,
	0xB3, 0xFF, 0x10, 0xFF, 0x6A, 0x80, 0x0B, 0x05,
	0x05, 0x05, 0x15, 0x15, 0x15, 0x1C, 0x1C, 0x1C,
	0x21, 0x21, 0x21, 0x23, 0x23, 0x23, 0x9B, 0x2F,
	0xE0, 0x18, 0x64
};
static DEFINE_PKTUI(mu1s_irc_mode, &mu1s_maptbl[IRC_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_irc_mode, DSI_PKT_TYPE_WR, MU1S_IRC_MODE, 0x279);

static DEFINE_PKTUI(mu1s_tset, &mu1s_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_tset, DSI_PKT_TYPE_WR, MU1S_TSET, 0x01);
static DEFINE_STATIC_PACKET(mu1s_ecc_on, DSI_PKT_TYPE_WR, MU1S_ECC_ON, 0x02);
static DEFINE_STATIC_PACKET(mu1s_ssr_on, DSI_PKT_TYPE_WR, MU1S_SSR_ON, 0x09);
static DEFINE_STATIC_PACKET(mu1s_ssr_source, DSI_PKT_TYPE_WR, MU1S_SSR_SOURCE, 0x0F);
static DEFINE_STATIC_PACKET(mu1s_ssr_sensing, DSI_PKT_TYPE_WR, MU1S_SSR_SENSING, 0xAC);
static DEFINE_STATIC_PACKET(mu1s_ssr_setting, DSI_PKT_TYPE_WR, MU1S_SSR_SETTING, 0);

static DEFINE_STATIC_PACKET(mu1s_err_fg_1, DSI_PKT_TYPE_WR, MU1S_ERR_FG_1, 0);
static DEFINE_STATIC_PACKET(mu1s_err_fg_2, DSI_PKT_TYPE_WR, MU1S_ERR_FG_2, 0x3D);
static DEFINE_STATIC_PACKET(mu1s_err_fg_3, DSI_PKT_TYPE_WR, MU1S_ERR_FG_3, 0);
static DEFINE_STATIC_PACKET(mu1s_isc, DSI_PKT_TYPE_WR, MU1S_ISC, 0x07);

static DEFINE_STATIC_PACKET(mu1s_testmode_source_on, DSI_PKT_TYPE_WR, MU1S_TESTMODE_SOURCE_ON, 0x0F);

static DEFINE_STATIC_PACKET(mu1s_mcd_on_01, DSI_PKT_TYPE_WR, MU1S_MCD_ON_01, 0x0C);
static DEFINE_STATIC_PACKET(mu1s_mcd_on_02, DSI_PKT_TYPE_WR, MU1S_MCD_ON_02, 0x0F);
static DEFINE_STATIC_PACKET(mu1s_mcd_on_03, DSI_PKT_TYPE_WR, MU1S_MCD_ON_03, 0x9E);
static DEFINE_STATIC_PACKET(mu1s_mcd_on_04, DSI_PKT_TYPE_WR, MU1S_MCD_ON_04, 0xD2);

static DEFINE_STATIC_PACKET(mu1s_mcd_off_01, DSI_PKT_TYPE_WR, MU1S_MCD_OFF_01, 0x0C);
/* MU1S_MCD_OFF_02 is replaced to MU1S_TESTMODE_SOURCE_ON */
static DEFINE_STATIC_PACKET(mu1s_mcd_off_03, DSI_PKT_TYPE_WR, MU1S_MCD_OFF_03, 0x9E);
static DEFINE_STATIC_PACKET(mu1s_mcd_off_04, DSI_PKT_TYPE_WR, MU1S_MCD_OFF_04, 0xD2);

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static DEFINE_STATIC_PACKET(mu1s_grayspot_on_01, DSI_PKT_TYPE_WR, MU1S_GRAYSPOT_ON_01, 0x0F);
static DEFINE_STATIC_PACKET(mu1s_grayspot_on_02, DSI_PKT_TYPE_WR, MU1S_GRAYSPOT_ON_02, 0x9E);
static DEFINE_STATIC_PACKET(mu1s_grayspot_on_03, DSI_PKT_TYPE_WR, MU1S_GRAYSPOT_ON_03, 0x01);
static DEFINE_STATIC_PACKET(mu1s_grayspot_on_04, DSI_PKT_TYPE_WR, MU1S_GRAYSPOT_ON_04, 0x18);

/* MU1S_GRAYSPOT_OFF_01 is replaced to MU1S_TESTMODE_SOURCE_ON */
static DEFINE_STATIC_PACKET(mu1s_grayspot_off_02, DSI_PKT_TYPE_WR, MU1S_GRAYSPOT_OFF_02, 0x9E);
static DEFINE_PKTUI(mu1s_grayspot_off_03, &mu1s_maptbl[TSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_grayspot_off_03, DSI_PKT_TYPE_WR, MU1S_GRAYSPOT_OFF_03, 0x01);
static DEFINE_PKTUI(mu1s_grayspot_off_04, &mu1s_maptbl[ELVSS_CAL_OFFSET_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_grayspot_off_04, DSI_PKT_TYPE_WR, MU1S_GRAYSPOT_OFF_04, 0x18);
#endif

#ifdef CONFIG_USDM_FACTORY_MST_TEST
static DEFINE_STATIC_PACKET(mu1s_mst_on_01, DSI_PKT_TYPE_WR, MU1S_MST_ON_01, 0x8A);
static DEFINE_STATIC_PACKET(mu1s_mst_on_02, DSI_PKT_TYPE_WR, MU1S_MST_ON_02, 0x0F);
static DEFINE_STATIC_PACKET(mu1s_mst_on_03, DSI_PKT_TYPE_WR, MU1S_MST_ON_03, 0x20);
static DEFINE_STATIC_PACKET(mu1s_mst_on_04, DSI_PKT_TYPE_WR, MU1S_MST_ON_04, 0x9E);
static DEFINE_STATIC_PACKET(mu1s_mst_on_05, DSI_PKT_TYPE_WR, MU1S_MST_ON_05, 0);

static DEFINE_STATIC_PACKET(mu1s_mst_off_01, DSI_PKT_TYPE_WR, MU1S_MST_OFF_01, 0x8A);
/* MU1S_MST_OFF_02 is replaced to MU1S_TESTMODE_SOURCE_ON */
static DEFINE_STATIC_PACKET(mu1s_mst_off_03, DSI_PKT_TYPE_WR, MU1S_MST_OFF_03, 0x20);
static DEFINE_STATIC_PACKET(mu1s_mst_off_04, DSI_PKT_TYPE_WR, MU1S_MST_OFF_04, 0x9E);
static DEFINE_STATIC_PACKET(mu1s_mst_off_05, DSI_PKT_TYPE_WR, MU1S_MST_OFF_05, 0);
#endif

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
static DEFINE_STATIC_PACKET(mu1s_sw_reset, DSI_PKT_TYPE_WR, MU1S_SW_RESET, 0);
static DEFINE_STATIC_PACKET(mu1s_gct_dsc, DSI_PKT_TYPE_WR, MU1S_GCT_DSC, 0);
static DEFINE_STATIC_PACKET(mu1s_gct_pps, DSI_PKT_TYPE_WR, MU1S_GCT_PPS, 0);
static DEFINE_STATIC_PACKET(mu1s_vddm_orig, DSI_PKT_TYPE_WR, MU1S_VDDM_ORIG, 0x02);
static DEFINE_PKTUI(mu1s_vddm_volt, &mu1s_maptbl[VDDM_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_vddm_volt, DSI_PKT_TYPE_WR, MU1S_VDDM_VOLT, 0x02);
static DEFINE_STATIC_PACKET(mu1s_ecc_setting, DSI_PKT_TYPE_WR, MU1S_ECC_SETTING, 0x02);
static DEFINE_STATIC_PACKET(mu1s_vddm_init, DSI_PKT_TYPE_WR, MU1S_VDDM_INIT, 0x0C);
static DEFINE_STATIC_PACKET(mu1s_vddm_init_2, DSI_PKT_TYPE_WR, MU1S_VDDM_INIT_2, 0x1C);
static DEFINE_STATIC_PACKET(mu1s_gct_wrdisbv, DSI_PKT_TYPE_WR, MU1S_GCT_WRDISBV, 0);
static DEFINE_STATIC_PACKET(mu1s_gct_wrctrld, DSI_PKT_TYPE_WR, MU1S_GCT_WRCTRLD, 0);
static DEFINE_STATIC_PACKET(mu1s_gram_chksum_start, DSI_PKT_TYPE_WR, MU1S_GRAM_CHKSUM_START, 0);
static DEFINE_PKTUI(mu1s_gram_img_pattern_on, &mu1s_maptbl[GRAM_IMG_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_gram_img_pattern_on, DSI_PKT_TYPE_WR, MU1S_GRAM_IMG_PATTERN_ON, 0);
static DEFINE_PKTUI(mu1s_gram_inv_img_pattern_on, &mu1s_maptbl[GRAM_INV_IMG_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_gram_inv_img_pattern_on, DSI_PKT_TYPE_WR, MU1S_GRAM_INV_IMG_PATTERN_ON, 0);
static DEFINE_STATIC_PACKET(mu1s_gram_img_pattern_off, DSI_PKT_TYPE_WR, MU1S_GRAM_IMG_PATTERN_OFF, 0);
#endif
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static DEFINE_STATIC_PACKET(mu1s_decoder_test_caset, DSI_PKT_TYPE_WR, MU1S_DECODER_TEST_CASET, 0);
static DEFINE_STATIC_PACKET(mu1s_decoder_test_paset, DSI_PKT_TYPE_WR, MU1S_DECODER_TEST_PASET, 0);
static DEFINE_STATIC_PACKET(mu1s_decoder_test_2c, DSI_PKT_TYPE_WR, MU1S_DECODER_TEST_2C, 0);

static DEFINE_STATIC_PACKET(mu1s_decoder_test_1, DSI_PKT_TYPE_WR, MU1S_DECODER_TEST_1, 0);
static DEFINE_STATIC_PACKET(mu1s_decoder_test_2, DSI_PKT_TYPE_WR, MU1S_DECODER_TEST_2, 0x27);
static DEFINE_STATIC_PACKET(mu1s_decoder_test_return_1, DSI_PKT_TYPE_WR, MU1S_DECODER_TEST_RETURN_1, 0);
static DEFINE_STATIC_PACKET(mu1s_decoder_test_return_2, DSI_PKT_TYPE_WR, MU1S_DECODER_TEST_RETURN_2, 0x27);

#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
static DEFINE_PKTUI(mu1s_xtalk_mode, &mu1s_maptbl[VGH_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_xtalk_mode, DSI_PKT_TYPE_WR, MU1S_XTALK_MODE, 0x0C);
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static DEFINE_STATIC_PACKET(mu1s_ccd_test_enable, DSI_PKT_TYPE_WR, MU1S_CCD_ENABLE, 0);
static DEFINE_STATIC_PACKET(mu1s_ccd_test_disable, DSI_PKT_TYPE_WR, MU1S_CCD_DISABLE, 0);
#endif

#ifdef CONFIG_USDM_DDI_CMDLOG
static DEFINE_STATIC_PACKET(mu1s_cmdlog_enable, DSI_PKT_TYPE_WR, MU1S_CMDLOG_ENABLE, 0);
static DEFINE_STATIC_PACKET(mu1s_cmdlog_disable, DSI_PKT_TYPE_WR, MU1S_CMDLOG_DISABLE, 0);
#endif
static DEFINE_STATIC_PACKET(mu1s_gamma_update_enable, DSI_PKT_TYPE_WR, MU1S_GAMMA_UPDATE_ENABLE, 0);

static DEFINE_PANEL_MDELAY(mu1s_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(mu1s_wait_17msec, 17);
static DEFINE_PANEL_MDELAY(mu1s_wait_34msec, 34);
static DEFINE_PANEL_MDELAY(mu1s_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(mu1s_wait_200msec, 200);
static DEFINE_PANEL_MDELAY(mu1s_wait_sleep_out_20msec, 20);
static DEFINE_PANEL_MDELAY(mu1s_wait_fastdischarge_120msec, 120);
static DEFINE_PANEL_MDELAY(mu1s_wait_sleep_in, 120);
static DEFINE_PANEL_FRAME_DELAY(mu1s_wait_1_frame, 1);
static DEFINE_PANEL_UDELAY(mu1s_wait_1usec, 1);
static DEFINE_PANEL_UDELAY(mu1s_wait_100usec, 100);
static DEFINE_PANEL_MDELAY(mu1s_wait_1msec, 1);
static DEFINE_PANEL_VSYNC_DELAY(mu1s_wait_1_vsync, 1);

static DEFINE_PANEL_TIMER_MDELAY(mu1s_wait_sleep_out_110msec, 110);
static DEFINE_PANEL_TIMER_BEGIN(mu1s_wait_sleep_out_110msec,
		TIMER_DLYINFO(&mu1s_wait_sleep_out_110msec));

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
static DEFINE_PANEL_MDELAY(mu1s_wait_250msec, 250);
static DEFINE_PANEL_MDELAY(mu1s_wait_vddm_update, 50);
static DEFINE_PANEL_MDELAY(mu1s_wait_20msec_gram_img_update, 20);
static DEFINE_PANEL_MDELAY(mu1s_wait_gram_img_update, 200);
#endif

static DEFINE_PANEL_KEY(mu1s_level1_key_enable, CMD_LEVEL_1, KEY_ENABLE, &PKTINFO(mu1s_level1_key_enable));
static DEFINE_PANEL_KEY(mu1s_level2_key_enable, CMD_LEVEL_2, KEY_ENABLE, &PKTINFO(mu1s_level2_key_enable));
static DEFINE_PANEL_KEY(mu1s_level3_key_enable, CMD_LEVEL_3, KEY_ENABLE, &PKTINFO(mu1s_level3_key_enable));
static DEFINE_PANEL_KEY(mu1s_level1_key_disable, CMD_LEVEL_1, KEY_DISABLE, &PKTINFO(mu1s_level1_key_disable));
static DEFINE_PANEL_KEY(mu1s_level2_key_disable, CMD_LEVEL_2, KEY_DISABLE, &PKTINFO(mu1s_level2_key_disable));
static DEFINE_PANEL_KEY(mu1s_level3_key_disable, CMD_LEVEL_3, KEY_DISABLE, &PKTINFO(mu1s_level3_key_disable));

static u8 MU1S_DIMMING_TRANSITION[] = { 0x53, 0x20 };
static DEFINE_PKTUI(mu1s_dimming_transition, &mu1s_maptbl[DIMMING_TRANSITION_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_dimming_transition, DSI_PKT_TYPE_WR, MU1S_DIMMING_TRANSITION, 0);

static u8 MU1S_DIMMING_TRANSITION_2[] = { 0x94, 0x20 };
static DEFINE_PKTUI(mu1s_dimming_transition_2, &mu1s_maptbl[DIMMING_TRANSITION_2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_dimming_transition_2, DSI_PKT_TYPE_WR, MU1S_DIMMING_TRANSITION_2, 0x0D);

static u8 MU1S_ACL_OPR[] = {
	0x55,
	0x01
};
static DEFINE_PKTUI(mu1s_acl_opr, &mu1s_maptbl[ACL_OPR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_acl_opr, DSI_PKT_TYPE_WR, MU1S_ACL_OPR, 0);

static u8 MU1S_ACL_CONTROL[] = {
	0x98,
	0x55, 0x80, 0x01
};
static DEFINE_STATIC_PACKET(mu1s_acl_control, DSI_PKT_TYPE_WR, MU1S_ACL_CONTROL, 0x147);

static u8 MU1S_ACL_DIM[] = {
	0x98,
	0x00, 0x1F
};
static DEFINE_STATIC_PACKET(mu1s_acl_dim, DSI_PKT_TYPE_WR, MU1S_ACL_DIM, 0x156);

static u8 MU1S_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(mu1s_wrdisbv, &mu1s_maptbl[GAMMA_MODE2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_wrdisbv, DSI_PKT_TYPE_WR, MU1S_WRDISBV, 0);

static u8 MU1S_LPM_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(mu1s_lpm_wrdisbv, &mu1s_maptbl[LPM_WRDISBV_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_lpm_wrdisbv, DSI_PKT_TYPE_WR, MU1S_LPM_WRDISBV, 0);

static u8 MU1S_CASET[] = { 0x2A, 0x00, 0x00, 0x04, 0x37 };
static u8 MU1S_PASET[] = { 0x2B, 0x00, 0x00, 0x09, 0x23 };

static DEFINE_STATIC_PACKET(mu1s_caset, DSI_PKT_TYPE_WR, MU1S_CASET, 0);
static DEFINE_STATIC_PACKET(mu1s_paset, DSI_PKT_TYPE_WR, MU1S_PASET, 0);

#ifdef CONFIG_USDM_PANEL_HMD
static u8 MU1S_HMD_WRDISBV[] = {
	0x51, 0x03, 0xFF
};
static DEFINE_PKTUI(mu1s_hmd_wrdisbv, &mu1s_maptbl[HMD_GAMMA_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_hmd_wrdisbv, DSI_PKT_TYPE_WR, MU1S_HMD_WRDISBV, 0);

static u8 MU1S_HMD_SETTING[] = {
	0xCB,
	0xCF, 0xDB, 0x5B, 0x5B
};
static DEFINE_STATIC_PACKET(mu1s_hmd_setting, DSI_PKT_TYPE_WR, MU1S_HMD_SETTING, 0x14);

static u8 MU1S_HMD_ON_AOR[] = {
	0x9A,
	0x07, 0x9A
};
static DEFINE_STATIC_PACKET(mu1s_hmd_on_aor, DSI_PKT_TYPE_WR, MU1S_HMD_ON_AOR, 0xAF);

static u8 MU1S_HMD_ON_AOR_CYCLE[] = {
	0x9A, 0x01
};
static DEFINE_STATIC_PACKET(mu1s_hmd_on_aor_cycle, DSI_PKT_TYPE_WR, MU1S_HMD_ON_AOR_CYCLE, 0xC9);

static u8 MU1S_HMD_ON_LTPS[] = {
	0xCB,
	0x00, 0x11, 0x9D, 0x9D, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xDB, 0xDB, 0x9B, 0x8D, 0xDE, 0xD3, 0xD1,
	0xC3, 0xCF, 0xC4, 0xC1, 0xCE, 0xDB, 0x5B, 0x5B,
	0xDB, 0x9B, 0xCD, 0x9E, 0xD3, 0xD1, 0xC3, 0xCF,
	0xC4, 0xC1, 0xDB, 0xDB, 0xDB, 0xC0, 0x80, 0x00,
	0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x00, 0x03, 0x00, 0x0E, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(mu1s_hmd_on_ltps, DSI_PKT_TYPE_WR, MU1S_HMD_ON_LTPS, 0);

static u8 MU1S_HMD_OFF_AOR[] = {
	0x9A,
	0x01, 0x86
};
static DEFINE_STATIC_PACKET(mu1s_hmd_off_aor, DSI_PKT_TYPE_WR, MU1S_HMD_OFF_AOR, 0xAF);

static u8 MU1S_HMD_OFF_AOR_CYCLE[] = {
	0x9A, 0x41
};
static DEFINE_STATIC_PACKET(mu1s_hmd_off_aor_cycle, DSI_PKT_TYPE_WR, MU1S_HMD_OFF_AOR_CYCLE, 0xC9);

static u8 MU1S_HMD_OFF_LTPS[] = {
	0xCB,
	0x00, 0x11, 0x9D, 0x9D, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xDB, 0xDB, 0x9B, 0x8D, 0xDE, 0xD3, 0xD1,
	0xC3, 0xCE, 0xC4, 0xC1, 0xCF, 0xDB, 0x5B, 0x5B,
	0xDB, 0x9B, 0xCD, 0x9E, 0xD3, 0xD1, 0xC3, 0xCE,
	0xC4, 0xC1, 0xDB, 0xDB, 0xDB, 0xC0, 0x80, 0x00,
	0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x00, 0x0B, 0x00, 0x06, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(mu1s_hmd_off_ltps, DSI_PKT_TYPE_WR, MU1S_HMD_OFF_LTPS, 0);
#endif

static u8 MU1S_FPS[] = { 0x60, 0x00 };
static DEFINE_PKTUI(mu1s_fps, &mu1s_maptbl[FPS_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_fps, DSI_PKT_TYPE_WR, MU1S_FPS, 0);

static u8 MU1S_AID[] = { 0x9A, 0x21, };
static DEFINE_PKTUI(mu1s_aid, &mu1s_maptbl[AID_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_aid, DSI_PKT_TYPE_WR, MU1S_AID, 0xC9);

static u8 MU1S_AID_HBM[] = { 0x9A, 0x21, };
static DEFINE_PKTUI(mu1s_aid_hbm, &mu1s_maptbl[AID_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_aid_hbm, DSI_PKT_TYPE_WR, MU1S_AID_HBM, 0xCD);

static u8 MU1S_OSC[] = { 0xF2, 0x01 };
static DEFINE_PKTUI(mu1s_osc, &mu1s_maptbl[OSC_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_osc, DSI_PKT_TYPE_WR, MU1S_OSC, 0);

static u8 MU1S_VFP[] = { 0xF2, 0x00, 0x00 };
static DEFINE_PKTUI(mu1s_vfp_120hs, &mu1s_maptbl[VFP_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_vfp_120hs, DSI_PKT_TYPE_WR, MU1S_VFP, 0x08);

static DEFINE_PKTUI(mu1s_vfp_60ns, &mu1s_maptbl[VFP_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_vfp_60ns, DSI_PKT_TYPE_WR, MU1S_VFP, 0x11);

static DEFINE_PKTUI(mu1s_vfp_60hs, &mu1s_maptbl[VFP_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_vfp_60hs, DSI_PKT_TYPE_WR, MU1S_VFP, 0x08);

static u8 MU1S_AOR[] = {
	0x98,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static DEFINE_PKTUI(mu1s_aor_120hs, &mu1s_maptbl[AOR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_aor_120hs, DSI_PKT_TYPE_WR, MU1S_AOR, 0x8B);

static DEFINE_PKTUI(mu1s_aor_60hs, &mu1s_maptbl[AOR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_aor_60hs, DSI_PKT_TYPE_WR, MU1S_AOR, 0xAB);

static DEFINE_PKTUI(mu1s_aor_60ns, &mu1s_maptbl[AOR_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_aor_60ns, DSI_PKT_TYPE_WR, MU1S_AOR, 0x6B);

static u8 MU1S_FPS_120HS[] = { 0x60, 0x00 };
static DEFINE_STATIC_PACKET(mu1s_fps_120hs, DSI_PKT_TYPE_WR, MU1S_FPS_120HS, 0);

static u8 MU1S_OSC_120HS[] = { 0xF2, 0x01 };
static DEFINE_STATIC_PACKET(mu1s_osc_120hs, DSI_PKT_TYPE_WR, MU1S_OSC_120HS, 0);

static u8 MU1S_VDDR_SETTING[] = { 0xF2, 0x01 };
static DEFINE_STATIC_PACKET(mu1s_vddr_setting, DSI_PKT_TYPE_WR, MU1S_VDDR_SETTING, 0x19);

static u8 MU1S_AVOID_SANDSTORM[] = { 0xF2, 0xF0 };
static DEFINE_STATIC_PACKET(mu1s_avoid_sandstorm, DSI_PKT_TYPE_WR, MU1S_AVOID_SANDSTORM, 0x33);

#ifdef CONFIG_USDM_PANEL_MAFPC

static u8 MU1S_MAFPC_SR_PATH[] = {
	0x75, 0x40,
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_sr_path, DSI_PKT_TYPE_WR, MU1S_MAFPC_SR_PATH, 0);

static u8 MU1S_DEFAULT_SR_PATH[] = {
	0x75, 0x00,
};
static DEFINE_STATIC_PACKET(mu1s_default_sr_path, DSI_PKT_TYPE_WR, MU1S_DEFAULT_SR_PATH, 0);

static DEFINE_STATIC_PACKET_WITH_OPTION(mu1s_mafpc_default_img, DSI_PKT_TYPE_WR_SR,
	S6E3FAC_MAFPC_DEFAULT_IMG, 0, PKT_OPTION_SR_ALIGN_12);

static u8 MU1S_MAFPC_GRAM_SIZE[] = {
	0xF2,
	0x5A, 0x91, 0x12, 0x38
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_gram_size, DSI_PKT_TYPE_WR, MU1S_MAFPC_GRAM_SIZE, 0x4E);

static u8 MU1S_MAFPC_DISABLE[] = {
	0x87, 0x00,
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_disable, DSI_PKT_TYPE_WR, MU1S_MAFPC_DISABLE, 0);


static u8 MU1S_MAFPC_CRC_ON[] = {
	0xD8, 0x17,
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_crc_on, DSI_PKT_TYPE_WR, MU1S_MAFPC_CRC_ON, 0x27);

static u8 MU1S_MAFPC_CRC_BIST_ON[] = {
	0xBF,
	0x01, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_crc_bist_on, DSI_PKT_TYPE_WR, MU1S_MAFPC_CRC_BIST_ON, 0x00);

static u8 MU1S_MAFPC_CRC_BIST_OFF[] = {
	0xBF,
	0x00, 0x07, 0xFF, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_crc_bist_off, DSI_PKT_TYPE_WR, MU1S_MAFPC_CRC_BIST_OFF, 0x00);

static u8 MU1S_MAFPC_CRC_ENABLE[] = {
	0x87,
	0x71, 0x09, 0x0f, 0x03, 0x00, 0x00, 0x66, 0x66,
	0x66, 0xff, 0xff, 0xff, 0x33, 0x33, 0x33, 0x33,
	0x33, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_crc_enable, DSI_PKT_TYPE_WR, MU1S_MAFPC_CRC_ENABLE, 0);

static u8 MU1S_MAFPC_LUMINANCE_UPDATE[] = {
	0xF7, 0x01
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_luminance_update, DSI_PKT_TYPE_WR, MU1S_MAFPC_LUMINANCE_UPDATE, 0);

static u8 MU1S_MAFPC_CRC_MDNIE_OFF[] = {
	0xDD,
	0x00
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_crc_mdnie_off, DSI_PKT_TYPE_WR, MU1S_MAFPC_CRC_MDNIE_OFF, 0);

static u8 MU1S_MAFPC_CRC_MDNIE_ON[] = {
	0xDD,
	0x01
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_crc_mdnie_on, DSI_PKT_TYPE_WR, MU1S_MAFPC_CRC_MDNIE_ON, 0);

static u8 MU1S_MAFPC_SELF_MASK_OFF[] = {
	0x7A,
	0x00
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_self_mask_off, DSI_PKT_TYPE_WR, MU1S_MAFPC_SELF_MASK_OFF, 0);

static u8 MU1S_MAFPC_SELF_MASK_ON[] = {
	0x7A,
	0x05
};
static DEFINE_STATIC_PACKET(mu1s_mafpc_self_mask_on, DSI_PKT_TYPE_WR, MU1S_MAFPC_SELF_MASK_ON, 0);

static DEFINE_PANEL_MDELAY(mu1s_crc_wait, 34);

static void *mu1s_mafpc_image_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&PKTINFO(mu1s_mafpc_sr_path),
	&DLYINFO(mu1s_wait_1usec),
	&PKTINFO(mu1s_mafpc_disable),
	&DLYINFO(mu1s_wait_1_frame),
	&PKTINFO(mu1s_mafpc_gram_size),
	&PKTINFO(mu1s_mafpc_default_img),
	&DLYINFO(mu1s_wait_100usec),
	&PKTINFO(mu1s_default_sr_path),
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
};

static void *mu1s_mafpc_check_cmdtbl[] = {
	/* temporary enable in factory binary */
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&PKTINFO(mu1s_mafpc_sr_path),
	&DLYINFO(mu1s_wait_1usec),
	&PKTINFO(mu1s_mafpc_disable),
	&DLYINFO(mu1s_wait_1_frame),
	&PKTINFO(mu1s_mafpc_gram_size),
	&PKTINFO(mu1s_mafpc_default_img),
	&DLYINFO(mu1s_wait_100usec),
	&PKTINFO(mu1s_default_sr_path),
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
	&DLYINFO(mu1s_wait_100msec),
	/* end of temporary code */

	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),

	&PKTINFO(mu1s_mafpc_crc_on),
	&PKTINFO(mu1s_mafpc_crc_bist_on),
	&PKTINFO(mu1s_mafpc_crc_enable),
	&PKTINFO(mu1s_mafpc_luminance_update),
	&PKTINFO(mu1s_mafpc_self_mask_off),
	&PKTINFO(mu1s_mafpc_crc_mdnie_off),
	&DLYINFO(mu1s_crc_wait),
	&s6e3fac_restbl[RES_MAFPC_CRC],
	&PKTINFO(mu1s_mafpc_crc_bist_off),
	&PKTINFO(mu1s_mafpc_disable),
	&PKTINFO(mu1s_mafpc_self_mask_on),
	&PKTINFO(mu1s_mafpc_crc_mdnie_on),
	&PKTINFO(mu1s_display_on),
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static DEFINE_PANEL_TIMER_MDELAY(mu1s_mafpc_delay, 120);
static DEFINE_PANEL_TIMER_BEGIN(mu1s_mafpc_delay,
		TIMER_DLYINFO(&mu1s_mafpc_delay));

static u8 MU1S_MAFPC_ENABLE[] = {
	0x87,
	0x00, 0x09, 0x0f, 0x03, 0x00, 0x00, 0x66, 0x66,
	0x66, 0xff, 0xff, 0xff, 0x33, 0x33, 0x33, 0x33,
	0x33, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static DEFINE_PKTUI(mu1s_mafpc_enable, &mu1s_maptbl[MAFPC_ENA_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_mafpc_enable, DSI_PKT_TYPE_WR, MU1S_MAFPC_ENABLE, 0);
static u8 MU1S_MAFPC_SCALE[] = {
	0x87,
	0xFF, 0xFF, 0xFF,
};
static DEFINE_PKTUI(mu1s_mafpc_scale, &mu1s_maptbl[MAFPC_SCALE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_mafpc_scale, DSI_PKT_TYPE_WR, MU1S_MAFPC_SCALE, 0x09);
static void *mu1s_mafpc_on_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_mafpc_enable),
	&PKTINFO(mu1s_mafpc_scale),
	&KEYINFO(mu1s_level2_key_disable),
};

static void *mu1s_mafpc_off_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_mafpc_disable),
	&KEYINFO(mu1s_level2_key_disable),
};
#endif

static DEFINE_RULE_BASED_COND(mu1s_cond_is_osc_94500khz,
		PANEL_PROPERTY_OSC_FREQ, EQ, 94500);
static DEFINE_RULE_BASED_COND(mu1s_cond_is_osc_96500khz,
		PANEL_PROPERTY_OSC_FREQ, EQ, 96500);
static DEFINE_RULE_BASED_COND_WITH_MASK(mu1s_cond_is_ddi_evt0,
		PANEL_PROPERTY_PANEL_ID_3, EQ, (S6E3FAC_DDI_REV_EVT0 << 4), 0x30);
static DEFINE_RULE_BASED_COND(mu1s_cond_is_factory_mode,
		PANEL_PROPERTY_IS_FACTORY_MODE, EQ, 1);

/* ------------------------------------ commands for osc common start ------------------------------------ */
static u8 MU1S_OSC_EVT0_ONLY[] = {
	0xC5,
	0x00, 0x80, 0x1B, 0x25, 0xB2, 0x24, 0xEA
};
static DEFINE_STATIC_PACKET(mu1s_osc_evt0_only_01, DSI_PKT_TYPE_WR, MU1S_OSC_EVT0_ONLY, 0x01);
static DEFINE_STATIC_PACKET(mu1s_osc_evt0_only_25, DSI_PKT_TYPE_WR, MU1S_OSC_EVT0_ONLY, 0x25);

/* ------------------------------------ commands for osc common end ------------------------------------ */

/* ------------------------------------ commands for osc 94.5Mhz start ------------------------------------ */
static u8 MU1S_OSC_94500_SET[] = {
	0xC5,
	0x03
};
static DEFINE_PKTUI(mu1s_osc_94500_set, &mu1s_maptbl[SET_OSC1_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_osc_94500_set, DSI_PKT_TYPE_WR, MU1S_OSC_94500_SET, 0x00);

static DEFINE_PKTUI(mu1s_osc_94500_set_24, &mu1s_maptbl[SET_OSC1_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_osc_94500_set_24, DSI_PKT_TYPE_WR, MU1S_OSC_94500_SET, 0x24);

static u8 MU1S_OSC_94500_LTPS_8A[] = {
	0xCB,
	0x11, 0x37, 0x00, 0x00, 0x00, 0x10, 0x11, 0x37
};
static DEFINE_STATIC_PACKET(mu1s_osc_94500_ltps_8a, DSI_PKT_TYPE_WR, MU1S_OSC_94500_LTPS_8A, 0x8A);

static u8 MU1S_OSC_94500_LTPS_C0[] = {
	0xCB,
	0xB1, 0x0C, 0x1D, 0x00, 0x00, 0x00, 0x91, 0x0C, 0x1D
};
static DEFINE_STATIC_PACKET(mu1s_osc_94500_ltps_c0, DSI_PKT_TYPE_WR, MU1S_OSC_94500_LTPS_C0, 0xC0);

static u8 MU1S_OSC_94500_LTPS_119[] = {
	0xCB,
	0x11, 0x37, 0x00, 0x00, 0x00, 0x11, 0x37
};
static DEFINE_STATIC_PACKET(mu1s_osc_94500_ltps_119, DSI_PKT_TYPE_WR, MU1S_OSC_94500_LTPS_119, 0x119);

static u8 MU1S_OSC_94500_LTPS_14C[] = {
	0xCB,
	0x1D, 0x00, 0x00, 0x00, 0x12, 0x0C, 0x1D
};
static DEFINE_STATIC_PACKET(mu1s_osc_94500_ltps_14c, DSI_PKT_TYPE_WR, MU1S_OSC_94500_LTPS_14C, 0x14C);

static u8 MU1S_OSC_94500_LTPS_229[] = {
	0xCB,
	0x26, 0x25, 0x00, 0x00, 0x00, 0x26, 0x25
};
static DEFINE_STATIC_PACKET(mu1s_osc_94500_ltps_229, DSI_PKT_TYPE_WR, MU1S_OSC_94500_LTPS_229, 0x229);

static u8 MU1S_OSC_94500_LTPS_25C[] = {
	0xCB,
	0x2C, 0x00, 0x00, 0x00, 0x12, 0x06, 0x2C
};
static DEFINE_STATIC_PACKET(mu1s_osc_94500_ltps_25c, DSI_PKT_TYPE_WR, MU1S_OSC_94500_LTPS_25C, 0x25C);

static u8 MU1S_OSC_94500_LTPS_2B1[] = {
	0xCB,
	0x27, 0x1E, 0x00, 0x00, 0x00, 0x27, 0x1E
};
static DEFINE_STATIC_PACKET(mu1s_osc_94500_ltps_2b1, DSI_PKT_TYPE_WR, MU1S_OSC_94500_LTPS_2B1, 0x2B1);

static u8 MU1S_OSC_94500_LTPS_2E4[] = {
	0xCB,
	0x40, 0x00, 0x00, 0x00, 0x12, 0x03, 0x40
};
static DEFINE_STATIC_PACKET(mu1s_osc_94500_ltps_2e4, DSI_PKT_TYPE_WR, MU1S_OSC_94500_LTPS_2E4, 0x2E4);

static u8 MU1S_OSC_94500_LTPS_304[] = {
	0xCB,
	0x09, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x1C
};
static DEFINE_STATIC_PACKET(mu1s_osc_94500_ltps_304, DSI_PKT_TYPE_WR, MU1S_OSC_94500_LTPS_304, 0x304);

static u8 MU1S_OSC_94500_LTPS_317[] = {
	0xCB,
	0x09, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x1C
};
static DEFINE_STATIC_PACKET(mu1s_osc_94500_ltps_317, DSI_PKT_TYPE_WR, MU1S_OSC_94500_LTPS_317, 0x317);

/* ------------------------------------- commands for osc 94.5Mhz end ------------------------------------- */


/* ------------------------------------ commands for osc 96.5Mhz start ------------------------------------ */

static u8 MU1S_OSC_96500_SET[] = {
	0xC5,
	0x02
};
static DEFINE_PKTUI(mu1s_osc_96500_set, &mu1s_maptbl[SET_OSC2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_osc_96500_set, DSI_PKT_TYPE_WR, MU1S_OSC_96500_SET, 0x00);

static DEFINE_PKTUI(mu1s_osc_96500_set_24, &mu1s_maptbl[SET_OSC2_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_osc_96500_set_24, DSI_PKT_TYPE_WR, MU1S_OSC_96500_SET, 0x24);

static u8 MU1S_OSC_96500_LTPS_8A[] = {
	0xCB,
	0x12, 0x38, 0x00, 0x00, 0x00, 0x10, 0x12, 0x38
};
static DEFINE_STATIC_PACKET(mu1s_osc_96500_ltps_8a, DSI_PKT_TYPE_WR, MU1S_OSC_96500_LTPS_8A, 0x8A);

static u8 MU1S_OSC_96500_LTPS_C0[] = {
	0xCB,
	0xB1, 0x0C, 0x1E, 0x00, 0x00, 0x00, 0x91, 0x0C, 0x1E
};
static DEFINE_STATIC_PACKET(mu1s_osc_96500_ltps_c0, DSI_PKT_TYPE_WR, MU1S_OSC_96500_LTPS_C0, 0xC0);

static u8 MU1S_OSC_96500_LTPS_119[] = {
	0xCB,
	0x12, 0x38, 0x00, 0x00, 0x00, 0x12, 0x38
};
static DEFINE_STATIC_PACKET(mu1s_osc_96500_ltps_119, DSI_PKT_TYPE_WR, MU1S_OSC_96500_LTPS_119, 0x119);

static u8 MU1S_OSC_96500_LTPS_14C[] = {
	0xCB,
	0x1E, 0x00, 0x00, 0x00, 0x12, 0x0C, 0x1E
};
static DEFINE_STATIC_PACKET(mu1s_osc_96500_ltps_14c, DSI_PKT_TYPE_WR, MU1S_OSC_96500_LTPS_14C, 0x14C);

static u8 MU1S_OSC_96500_LTPS_229[] = {
	0xCB,
	0x27, 0x26, 0x00, 0x00, 0x00, 0x27, 0x26
};
static DEFINE_STATIC_PACKET(mu1s_osc_96500_ltps_229, DSI_PKT_TYPE_WR, MU1S_OSC_96500_LTPS_229, 0x229);

static u8 MU1S_OSC_96500_LTPS_25C[] = {
	0xCB,
	0x2D, 0x00, 0x00, 0x00, 0x12, 0x06, 0x2D
};
static DEFINE_STATIC_PACKET(mu1s_osc_96500_ltps_25c, DSI_PKT_TYPE_WR, MU1S_OSC_96500_LTPS_25C, 0x25C);

static u8 MU1S_OSC_96500_LTPS_2B1[] = {
	0xCB,
	0x28, 0x1E, 0x00, 0x00, 0x00, 0x28, 0x1E
};
static DEFINE_STATIC_PACKET(mu1s_osc_96500_ltps_2b1, DSI_PKT_TYPE_WR, MU1S_OSC_96500_LTPS_2B1, 0x2B1);

static u8 MU1S_OSC_96500_LTPS_2E4[] = {
	0xCB,
	0x41, 0x00, 0x00, 0x00, 0x12, 0x03, 0x41
};
static DEFINE_STATIC_PACKET(mu1s_osc_96500_ltps_2e4, DSI_PKT_TYPE_WR, MU1S_OSC_96500_LTPS_2E4, 0x2E4);

static u8 MU1S_OSC_96500_LTPS_304[] = {
	0xCB,
	0x0A, 0x00, 0x00, 0x00, 0x1D, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x1D
};
static DEFINE_STATIC_PACKET(mu1s_osc_96500_ltps_304, DSI_PKT_TYPE_WR, MU1S_OSC_96500_LTPS_304, 0x304);

static u8 MU1S_OSC_96500_LTPS_317[] = {
	0xCB,
	0x0A, 0x00, 0x00, 0x00, 0x1D, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x1D
};
static DEFINE_STATIC_PACKET(mu1s_osc_96500_ltps_317, DSI_PKT_TYPE_WR, MU1S_OSC_96500_LTPS_317, 0x317);

/* ------------------------------------ commands for osc 96.5Mhz end ------------------------------------ */

static void *mu1s_osc_cmdtbl[] = {
	&CONDINFO_IF(mu1s_cond_is_osc_94500khz),
		&PKTINFO(mu1s_osc_94500_set),
		&PKTINFO(mu1s_osc_94500_set_24),
		&CONDINFO_IF(mu1s_cond_is_ddi_evt0),
			&PKTINFO(mu1s_osc_evt0_only_01),
			&PKTINFO(mu1s_osc_evt0_only_25),
		&CONDINFO_FI(mu1s_cond_is_ddi_evt0),
		&PKTINFO(mu1s_osc_94500_ltps_8a),
		&PKTINFO(mu1s_osc_94500_ltps_c0),
		&PKTINFO(mu1s_osc_94500_ltps_119),
		&PKTINFO(mu1s_osc_94500_ltps_14c),
		&PKTINFO(mu1s_osc_94500_ltps_229),
		&PKTINFO(mu1s_osc_94500_ltps_25c),
		&PKTINFO(mu1s_osc_94500_ltps_2e4),
		&PKTINFO(mu1s_osc_94500_ltps_304),
		&PKTINFO(mu1s_osc_94500_ltps_317),
	&CONDINFO_FI(mu1s_cond_is_osc_94500khz),

	&CONDINFO_IF(mu1s_cond_is_osc_96500khz),
		&PKTINFO(mu1s_osc_96500_set),
		&PKTINFO(mu1s_osc_96500_set_24),
		&CONDINFO_IF(mu1s_cond_is_ddi_evt0),
			&PKTINFO(mu1s_osc_evt0_only_01),
			&PKTINFO(mu1s_osc_evt0_only_25),
		&CONDINFO_FI(mu1s_cond_is_ddi_evt0),
		&PKTINFO(mu1s_osc_96500_ltps_8a),
		&PKTINFO(mu1s_osc_96500_ltps_c0),
		&PKTINFO(mu1s_osc_96500_ltps_119),
		&PKTINFO(mu1s_osc_96500_ltps_14c),
		&PKTINFO(mu1s_osc_96500_ltps_229),
		&PKTINFO(mu1s_osc_96500_ltps_25c),
		&PKTINFO(mu1s_osc_96500_ltps_2e4),
		&PKTINFO(mu1s_osc_96500_ltps_304),
		&PKTINFO(mu1s_osc_96500_ltps_317),
	&CONDINFO_FI(mu1s_cond_is_osc_96500khz),
};

static struct seqinfo SEQINFO(mu1s_osc_seq);
static DEFINE_SEQINFO(mu1s_osc_seq, mu1s_osc_cmdtbl);


/* ------------------------------------ commands for FFC start ------------------------------------ */

static char MU1S_FFC_ENABLE[] = { 0xC5, 0x11 };
static DEFINE_STATIC_PACKET(mu1s_ffc_enable, DSI_PKT_TYPE_WR, MU1S_FFC_ENABLE, 0x36);

static u8 MU1S_FFC_SET1[] = { 0xC5, 0x2D, 0x58 };
static DEFINE_PKTUI(mu1s_ffc_set1, &mu1s_maptbl[SET_FFC_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_ffc_set1, DSI_PKT_TYPE_WR, MU1S_FFC_SET1, 0x3E);

static u8 MU1S_FFC_SET2[] = { 0xC5, 0x2D, 0x58 };
static DEFINE_PKTUI(mu1s_ffc_set2, &mu1s_maptbl[SET_FFC_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(mu1s_ffc_set2, DSI_PKT_TYPE_WR, MU1S_FFC_SET2, 0x56);

static void *mu1s_ffc_cmdtbl[] = {
	&KEYINFO(mu1s_level3_key_enable),
	/*
	&PKTINFO(mu1s_ffc_enable),
	&PKTINFO(mu1s_ffc_set1),
	&PKTINFO(mu1s_ffc_set2),
	*/
	&KEYINFO(mu1s_level3_key_disable),
};
/* ------------------------------------- commands for FFC end ------------------------------------- */

static struct seqinfo SEQINFO(mu1s_set_bl_param_seq);
static struct seqinfo SEQINFO(mu1s_set_fps_param_seq);
static struct seqinfo SEQINFO(mu1s_set_fps_120hs_param_seq);
//static struct seqinfo SEQINFO(mu1s_spsram_gamma_read_seq);

static void *mu1s_ssr_setting_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&PKTINFO(mu1s_ssr_sensing),
	&PKTINFO(mu1s_ssr_on),
	&PKTINFO(mu1s_ssr_source),
	&PKTINFO(mu1s_ssr_setting),
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
};
static DEFINE_SEQINFO(mu1s_ssr_setting_seq,
		mu1s_ssr_setting_cmdtbl);

static void *mu1s_common_setting_cmdtbl[] = {
	&PKTINFO(mu1s_te_on),
	&PKTINFO(mu1s_caset),
	&PKTINFO(mu1s_paset),
	/*
	&PKTINFO(mu1s_ffc_enable),
	&PKTINFO(mu1s_ffc_set1),
	&PKTINFO(mu1s_ffc_set2),
	*/
	&PKTINFO(mu1s_ecc_on),
	&PKTINFO(mu1s_err_fg_1),
	&PKTINFO(mu1s_err_fg_2),
	&PKTINFO(mu1s_err_fg_3),
	&PKTINFO(mu1s_dsc),
	&PKTINFO(mu1s_pps),
	&PKTINFO(mu1s_osc_120hs),
	&PKTINFO(mu1s_fps_120hs),
	&PKTINFO(mu1s_te_fixed_set_1),
	&PKTINFO(mu1s_te_fixed_set_2),
	&PKTINFO(mu1s_gamma_update_enable),
	&PKTINFO(mu1s_dia_onoff),
	&PKTINFO(mu1s_smooth_dimming_init),
	&PKTINFO(mu1s_vddr_setting),
	&PKTINFO(mu1s_avoid_sandstorm),
	&PKTINFO(mu1s_gamma_update_enable),
};

static DEFINE_SEQINFO(mu1s_common_setting_seq,
		mu1s_common_setting_cmdtbl);

static void *mu1s_init_cmdtbl[] = {
	&PKTINFO(mu1s_sleep_out),
	&DLYINFO(mu1s_wait_10msec),
	&SEQINFO(mu1s_ssr_setting_seq),
	&TIMER_DLYINFO_BEGIN(mu1s_wait_sleep_out_110msec),
#if defined(CONFIG_USDM_PANEL_MAFPC)
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&PKTINFO(mu1s_mafpc_sr_path),
	&DLYINFO(mu1s_wait_1usec),
	&PKTINFO(mu1s_mafpc_disable),
	&DLYINFO(mu1s_wait_1_frame),
	&PKTINFO(mu1s_mafpc_gram_size),
	&PKTINFO(mu1s_mafpc_default_img),
	&DLYINFO(mu1s_wait_100usec),
	&PKTINFO(mu1s_default_sr_path),
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
#endif
	&TIMER_DLYINFO(mu1s_wait_sleep_out_110msec),
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&SEQINFO(mu1s_common_setting_seq),
	&SEQINFO(mu1s_set_bl_param_seq),
	&SEQINFO(mu1s_set_fps_param_seq),
	&PKTINFO(mu1s_gamma_update_enable),
#ifdef CONFIG_USDM_PANEL_COPR
	&SEQINFO(mu1s_set_copr_seq),
#endif
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_res_init_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&s6e3fac_restbl[RES_COORDINATE],
	&s6e3fac_restbl[RES_CODE],
	&s6e3fac_restbl[RES_DATE],
	&s6e3fac_restbl[RES_OCTA_ID],
	&s6e3fac_restbl[RES_ELVSS_CAL_OFFSET],
#ifdef CONFIG_USDM_PANEL_DPUI
	&s6e3fac_restbl[RES_CHIP_ID],
	&s6e3fac_restbl[RES_SELF_DIAG],
	&s6e3fac_restbl[RES_ERR_FG],
	&s6e3fac_restbl[RES_DSI_ERR],
#endif
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_set_bl_param_cmdtbl[] = {
	&PKTINFO(mu1s_acl_control),
	&PKTINFO(mu1s_acl_dim),
	&PKTINFO(mu1s_acl_opr),
	&PKTINFO(mu1s_tset),
	&PKTINFO(mu1s_smooth_dimming),
	&PKTINFO(mu1s_dimming_transition),
	&PKTINFO(mu1s_dimming_transition_2),
	&PKTINFO(mu1s_wrdisbv),
	&PKTINFO(mu1s_irc_mode),
#if defined(CONFIG_USDM_PANEL_MAFPC)
	&PKTINFO(mu1s_mafpc_scale),
#endif
#ifdef CONFIG_SUPPORT_XTALK_MODE
	&CONDINFO_IF(mu1s_cond_is_factory_mode),
		&PKTINFO(mu1s_xtalk_mode),
	&CONDINFO_FI(mu1s_cond_is_factory_mode),
#endif
};

static DEFINE_SEQINFO(mu1s_set_bl_param_seq,
		mu1s_set_bl_param_cmdtbl);

static void *mu1s_set_bl_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&SEQINFO(mu1s_set_bl_param_seq),
	&PKTINFO(mu1s_gamma_update_enable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_set_fps_param_cmdtbl[] = {
	/* fps & osc setting */
	&PKTINFO(mu1s_osc),
	&PKTINFO(mu1s_fps),

	/* 120HS, 96HS */
	&CONDINFO_IF(mu1s_cond_is_120hs_based_fps),
		&PKTINFO(mu1s_aor_120hs),
		&PKTINFO(mu1s_vfp_120hs),
	&CONDINFO_FI(mu1s_cond_is_120hs_based_fps),

	/* 60NS, 48NS */
	&CONDINFO_IF(mu1s_cond_is_60ns_based_fps),
		&PKTINFO(mu1s_aor_60ns),
		&PKTINFO(mu1s_vfp_60ns),
	&CONDINFO_FI(mu1s_cond_is_60ns_based_fps),

	/* 60HS, 48HS */
	&CONDINFO_IF(mu1s_cond_is_60hs_based_fps),
		&PKTINFO(mu1s_aor_60hs),
		&PKTINFO(mu1s_vfp_60hs),
	&CONDINFO_FI(mu1s_cond_is_60hs_based_fps),
};

static DEFINE_SEQINFO(mu1s_set_fps_param_seq,
	mu1s_set_fps_param_cmdtbl);

static void *mu1s_set_fps_120hs_param_cmdtbl[] = {
	/* fps & osc setting */
	&PKTINFO(mu1s_osc_120hs),
	&PKTINFO(mu1s_fps_120hs),
	&PKTINFO(mu1s_aor_120hs),
	&PKTINFO(mu1s_vfp_120hs),
};

static DEFINE_SEQINFO(mu1s_set_fps_120hs_param_seq,
		mu1s_set_fps_120hs_param_cmdtbl);

#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
static void *mu1s_display_mode_cmdtbl[] = {
	&CONDINFO_IF(mu1s_cond_is_panel_state_not_lpm),
		&KEYINFO(mu1s_level1_key_enable),
		&KEYINFO(mu1s_level2_key_enable),
		&SEQINFO(mu1s_set_bl_param_seq),
		&SEQINFO(mu1s_set_fps_param_seq),
		&PKTINFO(mu1s_te_setting_1),
		&PKTINFO(mu1s_te_setting_2),
		&PKTINFO(mu1s_gamma_update_enable),
		&KEYINFO(mu1s_level2_key_disable),
		&KEYINFO(mu1s_level1_key_disable),
		&CONDINFO_IF(mu1s_cond_is_vsync_for_mode_change),
			&DLYINFO(mu1s_wait_1_vsync),
		&CONDINFO_FI(mu1s_cond_is_vsync_for_mode_change),
	&CONDINFO_FI(mu1s_cond_is_panel_state_not_lpm),
};
#endif

#ifdef CONFIG_USDM_PANEL_HMD
static void *mu1s_hmd_on_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_hmd_on_aor),
	&PKTINFO(mu1s_hmd_on_aor_cycle),
	&PKTINFO(mu1s_hmd_on_ltps),
	&PKTINFO(mu1s_gamma_update_enable),
	&KEYINFO(mu1s_level2_key_disable),
};

static void *mu1s_hmd_off_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_hmd_off_aor),
	&PKTINFO(mu1s_hmd_off_aor_cycle),
	&PKTINFO(mu1s_hmd_off_ltps),
	&PKTINFO(mu1s_gamma_update_enable),
	&KEYINFO(mu1s_level2_key_disable),
};

static void *mu1s_hmd_bl_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_hmd_wrdisbv),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};
#endif

static void *mu1s_display_on_cmdtbl[] = {
#if defined(CONFIG_USDM_PANEL_MAFPC)
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_mafpc_enable),
	&PKTINFO(mu1s_mafpc_scale),
	&KEYINFO(mu1s_level2_key_disable),
#endif
	&KEYINFO(mu1s_level1_key_enable),
	&PKTINFO(mu1s_display_on),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_display_off_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&PKTINFO(mu1s_display_off),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_exit_cmdtbl[] = {
 	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&s6e3fac_dmptbl[DUMP_RDDPM_SLEEP_IN],
#ifdef CONFIG_USDM_PANEL_DPUI
	&s6e3fac_dmptbl[DUMP_RDDSM],
	&s6e3fac_dmptbl[DUMP_ERR],
	&s6e3fac_dmptbl[DUMP_ERR_FG],
	&s6e3fac_dmptbl[DUMP_DSI_ERR],
	&s6e3fac_dmptbl[DUMP_SELF_DIAG],
#endif
	&s6e3fac_dmptbl[DUMP_SSR],
	&s6e3fac_dmptbl[DUMP_ECC],
	&s6e3fac_dmptbl[DUMP_FLASH_LOADED],
	&KEYINFO(mu1s_level2_key_disable),
	&PKTINFO(mu1s_sleep_in),
	&KEYINFO(mu1s_level1_key_disable),
	&DLYINFO(mu1s_wait_sleep_in),
};

static void *mu1s_dia_onoff_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_dia_onoff),
	&KEYINFO(mu1s_level2_key_disable),
};

static void *mu1s_alpm_init_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_lpm_enter_nit),
	&PKTINFO(mu1s_gamma_update_enable),
	&PKTINFO(mu1s_te_setting_1),
	&PKTINFO(mu1s_lpm_te_setting),
	&PKTINFO(mu1s_gamma_update_enable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
	&DLYINFO(mu1s_wait_17msec),
};

static void *mu1s_alpm_set_bl_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_lpm_enter_seamless_ctrl),
	&PKTINFO(mu1s_lpm_control),
	&PKTINFO(mu1s_lpm_control2),
	&PKTINFO(mu1s_lpm_on),
	&PKTINFO(mu1s_gamma_update_enable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_alpm_exit_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_lpm_exit_seamless_ctrl),
	&PKTINFO(mu1s_dimming_transition),
	&PKTINFO(mu1s_dimming_transition_2),
	&PKTINFO(mu1s_lpm_exit_nit),
	&PKTINFO(mu1s_lpm_wrdisbv),
	&PKTINFO(mu1s_gamma_update_enable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
	&DLYINFO(mu1s_wait_1_vsync),
};

static void *mu1s_alpm_exit_after_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&SEQINFO(mu1s_set_bl_param_seq),
	&SEQINFO(mu1s_set_fps_param_seq),
	&PKTINFO(mu1s_te_setting_1),
	&PKTINFO(mu1s_te_setting_2),
	&PKTINFO(mu1s_gamma_update_enable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_mcd_on_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_mcd_on_01),
	&PKTINFO(mu1s_mcd_on_02),
	&PKTINFO(mu1s_mcd_on_03),
	&PKTINFO(mu1s_mcd_on_04),
	&PKTINFO(mu1s_gamma_update_enable),
	&DLYINFO(mu1s_wait_100msec),
	&KEYINFO(mu1s_level2_key_disable),
};

static void *mu1s_mcd_off_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_mcd_off_01),
	&PKTINFO(mu1s_testmode_source_on),
	&PKTINFO(mu1s_mcd_off_03),
	&PKTINFO(mu1s_mcd_off_04),
	&PKTINFO(mu1s_gamma_update_enable),
	&DLYINFO(mu1s_wait_100msec),
	&KEYINFO(mu1s_level2_key_disable),
};

#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
static void *mu1s_grayspot_on_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_grayspot_on_01),
	&PKTINFO(mu1s_grayspot_on_02),
	&PKTINFO(mu1s_grayspot_on_03),
	&PKTINFO(mu1s_grayspot_on_04),
	&PKTINFO(mu1s_gamma_update_enable),
	&DLYINFO(mu1s_wait_100msec),
	&KEYINFO(mu1s_level2_key_disable),
};

static void *mu1s_grayspot_off_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_testmode_source_on),
	&PKTINFO(mu1s_grayspot_off_02),
	&PKTINFO(mu1s_grayspot_off_03),
	&PKTINFO(mu1s_grayspot_off_04),
	&PKTINFO(mu1s_gamma_update_enable),
	&DLYINFO(mu1s_wait_100msec),
	&KEYINFO(mu1s_level2_key_disable),
};
#endif

#ifdef CONFIG_USDM_FACTORY_MST_TEST
static void *mu1s_mst_on_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_mst_on_01),
	&PKTINFO(mu1s_mst_on_02),
	&PKTINFO(mu1s_mst_on_03),
	&PKTINFO(mu1s_mst_on_04),
	&PKTINFO(mu1s_mst_on_05),
	&PKTINFO(mu1s_gamma_update_enable),
	&KEYINFO(mu1s_level2_key_disable),
};

static void *mu1s_mst_off_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_mst_off_01),
	&PKTINFO(mu1s_testmode_source_on),
	&PKTINFO(mu1s_mst_off_03),
	&PKTINFO(mu1s_mst_off_04),
	&PKTINFO(mu1s_mst_off_05),
	&PKTINFO(mu1s_gamma_update_enable),
	&KEYINFO(mu1s_level2_key_disable),
};
#endif

#ifdef CONFIG_USDM_FACTORY_GCT_TEST
static void *mu1s_gct_enter_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&PKTINFO(mu1s_sw_reset),
	&DLYINFO(mu1s_wait_250msec),
	&KEYINFO(mu1s_level1_key_disable),
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&PKTINFO(mu1s_sleep_out),
	&DLYINFO(mu1s_wait_sleep_out_20msec),
	&PKTINFO(mu1s_ecc_setting),
	&PKTINFO(mu1s_vddm_init),
	&PKTINFO(mu1s_vddm_init_2),
	&PKTINFO(mu1s_gct_wrdisbv),
	&PKTINFO(mu1s_gct_wrctrld),
	&PKTINFO(mu1s_display_on),
	&PKTINFO(mu1s_gct_dsc),
	&PKTINFO(mu1s_gct_pps),
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_gct_vddm_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&PKTINFO(mu1s_vddm_volt),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
	&DLYINFO(mu1s_wait_vddm_update),
};

static void *mu1s_gct_img_0_update_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&PKTINFO(mu1s_gram_chksum_start),
	&PKTINFO(mu1s_gram_inv_img_pattern_on),
	&DLYINFO(mu1s_wait_20msec_gram_img_update),
	&PKTINFO(mu1s_gram_img_pattern_off),
	&DLYINFO(mu1s_wait_20msec_gram_img_update),
	&PKTINFO(mu1s_gram_img_pattern_on),
	&DLYINFO(mu1s_wait_gram_img_update),
	&PKTINFO(mu1s_gram_img_pattern_off),
	&s6e3fac_restbl[RES_GRAM_CHECKSUM],
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_gct_img_1_update_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&DLYINFO(mu1s_wait_20msec_gram_img_update),
	&PKTINFO(mu1s_gram_img_pattern_on),
	&DLYINFO(mu1s_wait_gram_img_update),
	&PKTINFO(mu1s_gram_img_pattern_off),
	&s6e3fac_restbl[RES_GRAM_CHECKSUM],
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_gct_exit_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&PKTINFO(mu1s_vddm_orig),
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};
#endif

#ifdef CONFIG_USDM_FACTORY_CCD_TEST
static void *mu1s_ccd_test_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&PKTINFO(mu1s_ccd_test_enable),
	&DLYINFO(mu1s_wait_1msec),
	&s6e3fac_restbl[RES_CCD_STATE],
	&PKTINFO(mu1s_ccd_test_disable),
	&KEYINFO(mu1s_level2_key_disable),
};
#endif

#ifdef CONFIG_USDM_FACTORY_SSR_TEST
static void *mu1s_ssr_test_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&s6e3fac_dmptbl[DUMP_SSR],
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
};
#endif

#ifdef CONFIG_USDM_FACTORY_ECC_TEST
static void *mu1s_ecc_test_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&s6e3fac_dmptbl[DUMP_ECC],
	&KEYINFO(mu1s_level2_key_disable),
};
#endif

#ifdef CONFIG_USDM_DDI_CMDLOG
static void *mu1s_cmdlog_dump_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&s6e3fac_dmptbl[DUMP_CMDLOG],
	&KEYINFO(mu1s_level2_key_disable),
};
#endif

static void *mu1s_check_condition_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&s6e3fac_dmptbl[DUMP_RDDPM],
	&s6e3fac_dmptbl[DUMP_SELF_DIAG],
#if defined(CONFIG_USDM_PANEL_MAFPC)
	&KEYINFO(mu1s_level3_key_enable),
	&s6e3fac_dmptbl[DUMP_MAFPC],
	&s6e3fac_dmptbl[DUMP_MAFPC_FLASH],
	//&s6e3fac_dmptbl[DUMP_SELF_MASK_CRC],
	&KEYINFO(mu1s_level3_key_disable),
#endif
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_dump_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&s6e3fac_dmptbl[DUMP_RDDPM],
	&s6e3fac_dmptbl[DUMP_RDDSM],
	&s6e3fac_dmptbl[DUMP_ERR],
	&s6e3fac_dmptbl[DUMP_ERR_FG],
	&s6e3fac_dmptbl[DUMP_DSI_ERR],
	&s6e3fac_dmptbl[DUMP_SELF_DIAG],
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};

static void *mu1s_boot_cmdtbl[] = { };

#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
static void *mu1s_decoder_test_cmdtbl[] = {
	&KEYINFO(mu1s_level1_key_enable),
	&KEYINFO(mu1s_level2_key_enable),
	&KEYINFO(mu1s_level3_key_enable),
	&PKTINFO(mu1s_decoder_test_caset),
	&PKTINFO(mu1s_decoder_test_paset),
	&PKTINFO(mu1s_decoder_test_2c),
	&PKTINFO(mu1s_decoder_test_1),
	&PKTINFO(mu1s_decoder_test_2),
	&DLYINFO(mu1s_wait_200msec),
	&s6e3fac_dmptbl[DUMP_DSC_CRC],
	&PKTINFO(mu1s_decoder_test_return_1),
	&PKTINFO(mu1s_decoder_test_return_2),
	&KEYINFO(mu1s_level3_key_disable),
	&KEYINFO(mu1s_level2_key_disable),
	&KEYINFO(mu1s_level1_key_disable),
};
#endif

static void *mu1s_flash_test_cmdtbl[] = {
	&KEYINFO(mu1s_level2_key_enable),
	&s6e3fac_dmptbl[DUMP_FLASH_LOADED],
	&KEYINFO(mu1s_level2_key_disable),
};

static void *mu1s_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo mu1s_seqtbl[] = {
	SEQINFO_INIT(PANEL_INIT_SEQ, mu1s_init_cmdtbl),
	SEQINFO_INIT(PANEL_BOOT_SEQ, mu1s_boot_cmdtbl),
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, mu1s_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_SET_BL_SEQ, mu1s_set_bl_cmdtbl),
#ifdef CONFIG_USDM_PANEL_HMD
	SEQINFO_INIT(PANEL_HMD_ON_SEQ, mu1s_hmd_on_cmdtbl),
	SEQINFO_INIT(PANEL_HMD_OFF_SEQ, mu1s_hmd_off_cmdtbl),
	SEQINFO_INIT(PANEL_HMD_BL_SEQ, mu1s_hmd_bl_cmdtbl),
#endif
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, mu1s_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, mu1s_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, mu1s_exit_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_INIT_SEQ, mu1s_alpm_init_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_SET_BL_SEQ, mu1s_alpm_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_EXIT_SEQ, mu1s_alpm_exit_cmdtbl),
	SEQINFO_INIT(PANEL_ALPM_EXIT_AFTER_SEQ, mu1s_alpm_exit_after_cmdtbl),
	SEQINFO_INIT(PANEL_DIA_ONOFF_SEQ, mu1s_dia_onoff_cmdtbl),
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, mu1s_display_mode_cmdtbl),
#endif
	SEQINFO_INIT(PANEL_MCD_ON_SEQ, mu1s_mcd_on_cmdtbl),
	SEQINFO_INIT(PANEL_MCD_OFF_SEQ, mu1s_mcd_off_cmdtbl),
#ifdef CONFIG_SUPPORT_GRAYSPOT_TEST
	SEQINFO_INIT(PANEL_GRAYSPOT_ON_SEQ, mu1s_grayspot_on_cmdtbl),
	SEQINFO_INIT(PANEL_GRAYSPOT_OFF_SEQ, mu1s_grayspot_off_cmdtbl),
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
	SEQINFO_INIT(PANEL_MAFPC_IMG_SEQ, mu1s_mafpc_image_cmdtbl),
	SEQINFO_INIT(PANEL_MAFPC_CHECKSUM_SEQ, mu1s_mafpc_check_cmdtbl),
	SEQINFO_INIT(PANEL_MAFPC_ON_SEQ, mu1s_mafpc_on_cmdtbl),
	SEQINFO_INIT(PANEL_MAFPC_OFF_SEQ, mu1s_mafpc_off_cmdtbl),
#endif
#ifdef CONFIG_USDM_FACTORY_MST_TEST
	SEQINFO_INIT(PANEL_MST_ON_SEQ, mu1s_mst_on_cmdtbl),
	SEQINFO_INIT(PANEL_MST_OFF_SEQ, mu1s_mst_off_cmdtbl),
#endif
#ifdef CONFIG_USDM_FACTORY_GCT_TEST
	SEQINFO_INIT(PANEL_GCT_ENTER_SEQ, mu1s_gct_enter_cmdtbl),
	SEQINFO_INIT(PANEL_GCT_VDDM_SEQ, mu1s_gct_vddm_cmdtbl),
	SEQINFO_INIT(PANEL_GCT_IMG_0_UPDATE_SEQ, mu1s_gct_img_0_update_cmdtbl),
	SEQINFO_INIT(PANEL_GCT_IMG_1_UPDATE_SEQ, mu1s_gct_img_1_update_cmdtbl),
	SEQINFO_INIT(PANEL_GCT_EXIT_SEQ, mu1s_gct_exit_cmdtbl),
#endif
#ifdef CONFIG_USDM_FACTORY_CCD_TEST
	SEQINFO_INIT(PANEL_CCD_TEST_SEQ, mu1s_ccd_test_cmdtbl),
#endif
	SEQINFO_INIT(PANEL_CHECK_CONDITION_SEQ, mu1s_check_condition_cmdtbl),
	SEQINFO_INIT(PANEL_DUMP_SEQ, mu1s_dump_cmdtbl),
#ifdef CONFIG_USDM_DDI_CMDLOG
	SEQINFO_INIT(PANEL_CMDLOG_DUMP_SEQ, mu1s_cmdlog_dump_cmdtbl),
#endif
	SEQINFO_INIT(PANEL_FFC_SEQ, mu1s_ffc_cmdtbl),
#ifdef CONFIG_USDM_FACTORY_SSR_TEST
	SEQINFO_INIT(PANEL_SSR_TEST_SEQ, mu1s_ssr_test_cmdtbl),
#endif
#ifdef CONFIG_USDM_FACTORY_ECC_TEST
	SEQINFO_INIT(PANEL_ECC_TEST_SEQ, mu1s_ecc_test_cmdtbl),
#endif
#ifdef CONFIG_USDM_FACTORY_DSC_CRC_TEST
	SEQINFO_INIT(PANEL_DECODER_TEST_SEQ, mu1s_decoder_test_cmdtbl),
#endif
	SEQINFO_INIT(PANEL_FLASH_TEST_SEQ, mu1s_flash_test_cmdtbl),
	SEQINFO_INIT(PANEL_DUMMY_SEQ, mu1s_dummy_cmdtbl),
};

struct common_panel_info s6e3fac_mu1s_panel_info = {
	.ldi_name = "s6e3fac",
	.name = "s6e3fac_mu1s",
	.model = "AMB675TG01",
	.vendor = "SDC",
	.id = 0x810000,
	.rev = 0,
	.ddi_props = {
		.gpara = (DDI_SUPPORT_WRITE_GPARA |
				DDI_SUPPORT_READ_GPARA | DDI_SUPPORT_POINT_GPARA | DDI_SUPPORT_2BYTE_GPARA),
		.err_fg_recovery = false,
		.support_vrr = true,
		.dft_osc_freq = 96500,
/* Todo the hs_clk must be synchronized with the value of LK,
 * It must be changed to be read and set in the LK.
 */
		.dft_dsi_freq = 1362000,
		.support_avoid_sandstorm = true,
		.evasion_disp_det = true,
	},
	.ddi_ops = {
		.ddi_init = s6e3fac_ddi_init,
		.gamma_flash_checksum = s6e3fac_do_gamma_flash_checksum,
		.get_cell_id = s6e3fac_get_cell_id,
		.get_octa_id = s6e3fac_get_octa_id,
		.get_manufacture_code = s6e3fac_get_manufacture_code,
		.get_manufacture_date = s6e3fac_get_manufacture_date,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(s6e3fac_mu1s_default_resol),
		.resol = s6e3fac_mu1s_default_resol,
	},
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.common_panel_modes = &s6e3fac_mu1s_display_modes,
#endif
	.vrrtbl = s6e3fac_mu1s_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(s6e3fac_mu1s_default_vrrtbl),
	.maptbl = mu1s_maptbl,
	.nr_maptbl = ARRAY_SIZE(mu1s_maptbl),
	.seqtbl = mu1s_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(mu1s_seqtbl),
	.rditbl = s6e3fac_rditbl,
	.nr_rditbl = ARRAY_SIZE(s6e3fac_rditbl),
	.restbl = s6e3fac_restbl,
	.nr_restbl = ARRAY_SIZE(s6e3fac_restbl),
	.dumpinfo = s6e3fac_dmptbl,
	.nr_dumpinfo = ARRAY_SIZE(s6e3fac_dmptbl),
#ifdef CONFIG_USDM_MDNIE
	.mdnie_tune = &s6e3fac_mu1s_mdnie_tune,
#endif
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &s6e3fac_mu1s_panel_dimming_info,
#ifdef CONFIG_USDM_PANEL_HMD
		[PANEL_BL_SUBDEV_TYPE_HMD] = &s6e3fac_mu1s_panel_hmd_dimming_info,
#endif
#ifdef CONFIG_USDM_PANEL_AOD_BL
		[PANEL_BL_SUBDEV_TYPE_AOD] = &s6e3fac_mu1s_panel_aod_dimming_info,
#endif
	},
#ifdef CONFIG_USDM_PANEL_COPR
	.copr_data = &s6e3fac_mu1s_copr_data,
#endif
#ifdef CONFIG_USDM_PANEL_SELF_DISPLAY
	.aod_tune = &s6e3fac_mu1s_aod,
#endif
#ifdef CONFIG_USDM_PANEL_MAFPC
	.mafpc_info = &s6e3fac_mu1s_mafpc,
#endif
};
#endif /* __S6E3FAC_MU1S_PANEL_H__ */
