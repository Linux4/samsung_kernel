/*
 * linux/drivers/video/fbdev/exynos/panel/ft8203_boe/ft8203_boe_t260_panel.h
 *
 * Header file for FT8203 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __FT8203_T260_PANEL_H__
#define __FT8203_T260_PANEL_H__
#include "../panel_drv.h"
#include "ft8203_boe.h"

#include "ft8203_boe_t260_panel_dimming.h"
#include "ft8203_boe_t260_panel_i2c.h"

#include "ft8203_boe_t260_resol.h"

#undef __pn_name__
#define __pn_name__	t260

#undef __PN_NAME__
#define __PN_NAME__	T260

static struct seqinfo t260_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [FT8203 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [FT8203 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [FT8203 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 t260_brt_table[FT8203_TOTAL_NR_LUMINANCE][2] = {
	{0x00, 0x00},
	{0x01, 0x0F}, /* MIN */
	{0x02, 0x0B},
	{0x03, 0x07},
	{0x04, 0x03},
	{0x04, 0x0F},
	{0x05, 0x0B},
	{0x06, 0x07},
	{0x07, 0x03},
	{0x07, 0x0F},
	{0x08, 0x0F},
	{0x09, 0x0B},
	{0x0A, 0x07},
	{0x0B, 0x03},
	{0x0B, 0x0F},
	{0x0C, 0x0B},
	{0x0D, 0x07},
	{0x0E, 0x05},
	{0x0F, 0x03},
	{0x0F, 0x0F},
	{0x10, 0x0B},
	{0x11, 0x07},
	{0x12, 0x03},
	{0x12, 0x0F},
	{0x13, 0x0B},
	{0x14, 0x07},
	{0x15, 0x03},
	{0x15, 0x0F},
	{0x16, 0x0F},
	{0x17, 0x0B},
	{0x18, 0x07},
	{0x19, 0x03},
	{0x19, 0x0F},
	{0x1A, 0x0B},
	{0x1B, 0x07},
	{0x1C, 0x05},
	{0x1D, 0x03},
	{0x1D, 0x0F},
	{0x1E, 0x0B},
	{0x1F, 0x07},
	{0x20, 0x03},
	{0x20, 0x0F},
	{0x21, 0x0B},
	{0x22, 0x07},
	{0x23, 0x03},
	{0x23, 0x0F},
	{0x24, 0x0F},
	{0x25, 0x0B},
	{0x26, 0x07},
	{0x27, 0x03},
	{0x27, 0x0F},
	{0x28, 0x0B},
	{0x29, 0x07},
	{0x2A, 0x05},
	{0x2B, 0x03},
	{0x2B, 0x0F},
	{0x2C, 0x0B},
	{0x2D, 0x07},
	{0x2E, 0x03},
	{0x2E, 0x0F},
	{0x2F, 0x0B},
	{0x30, 0x07},
	{0x31, 0x03},
	{0x31, 0x0F},
	{0x32, 0x0F},
	{0x33, 0x0B},
	{0x34, 0x07},
	{0x35, 0x03},
	{0x35, 0x0F},
	{0x36, 0x0B},
	{0x37, 0x07},
	{0x38, 0x05},
	{0x39, 0x03},
	{0x39, 0x0F},
	{0x3A, 0x0B},
	{0x3B, 0x07},
	{0x3C, 0x03},
	{0x3C, 0x0F},
	{0x3D, 0x0B},
	{0x3E, 0x07},
	{0x3F, 0x03},
	{0x3F, 0x0F},
	{0x40, 0x0F},
	{0x41, 0x0B},
	{0x42, 0x07},
	{0x43, 0x03},
	{0x43, 0x0F},
	{0x44, 0x0B},
	{0x45, 0x07},
	{0x46, 0x05},
	{0x47, 0x03},
	{0x47, 0x0F},
	{0x48, 0x0B},
	{0x49, 0x07},
	{0x4A, 0x03},
	{0x4A, 0x0F},
	{0x4B, 0x0B},
	{0x4C, 0x07},
	{0x4D, 0x03},
	{0x4D, 0x0F},
	{0x4E, 0x0F},
	{0x4F, 0x0B},
	{0x50, 0x07},
	{0x51, 0x03},
	{0x51, 0x0F},
	{0x52, 0x0B},
	{0x53, 0x07},
	{0x54, 0x05},
	{0x55, 0x03},
	{0x55, 0x0F},
	{0x56, 0x0B},
	{0x57, 0x07},
	{0x58, 0x03},
	{0x58, 0x0F},
	{0x59, 0x0B},
	{0x5A, 0x07},
	{0x5B, 0x03},
	{0x5B, 0x0F},
	{0x5C, 0x0F},
	{0x5D, 0x0B},
	{0x5E, 0x07},
	{0x5F, 0x03},
	{0x5F, 0x0F},
	{0x60, 0x0B},
	{0x61, 0x07},
	{0x62, 0x05},
	{0x63, 0x03},
	{0x63, 0x0F},
	{0x64, 0x0B}, /* DEFAULT */
	{0x65, 0x07},
	{0x66, 0x03},
	{0x66, 0x0F},
	{0x67, 0x0B},
	{0x68, 0x07},
	{0x69, 0x03},
	{0x69, 0x0F},
	{0x6A, 0x0F},
	{0x6B, 0x0B},
	{0x6C, 0x07},
	{0x6D, 0x03},
	{0x6D, 0x0F},
	{0x6E, 0x0B},
	{0x6F, 0x07},
	{0x70, 0x05},
	{0x71, 0x03},
	{0x71, 0x0F},
	{0x72, 0x0B},
	{0x73, 0x07},
	{0x74, 0x03},
	{0x74, 0x0F},
	{0x75, 0x0B},
	{0x76, 0x0B},
	{0x77, 0x07},
	{0x78, 0x03},
	{0x78, 0x0F},
	{0x79, 0x0B},
	{0x7A, 0x0B},
	{0x7B, 0x0B},
	{0x7C, 0x0B},
	{0x7D, 0x0B},
	{0x7E, 0x0B},
	{0x7F, 0x0B},
	{0x80, 0x0B},
	{0x81, 0x0B},
	{0x82, 0x0B},
	{0x83, 0x0B},
	{0x84, 0x0B},
	{0x85, 0x0B},
	{0x86, 0x0B},
	{0x87, 0x0B},
	{0x88, 0x0B},
	{0x89, 0x0B},
	{0x8A, 0x0B},
	{0x8B, 0x0B},
	{0x8C, 0x0B},
	{0x8D, 0x0B},
	{0x8E, 0x0B},
	{0x8F, 0x0B},
	{0x90, 0x0B},
	{0x91, 0x0B},
	{0x92, 0x0B},
	{0x93, 0x0B},
	{0x94, 0x0B},
	{0x95, 0x0B},
	{0x96, 0x0B},
	{0x97, 0x0B},
	{0x98, 0x0B},
	{0x99, 0x0B},
	{0x9A, 0x0B},
	{0x9B, 0x0B},
	{0x9C, 0x0B},
	{0x9D, 0x0B},
	{0x9E, 0x0B},
	{0x9F, 0x0B},
	{0xA0, 0x0B},
	{0xA1, 0x0B},
	{0xA2, 0x0B},
	{0xA3, 0x0B},
	{0xA4, 0x0B},
	{0xA5, 0x0B},
	{0xA6, 0x0B},
	{0xA7, 0x0B},
	{0xA8, 0x0B},
	{0xA9, 0x0B},
	{0xAA, 0x0B},
	{0xAB, 0x0B},
	{0xAC, 0x0B},
	{0xAD, 0x0B},
	{0xAE, 0x0B},
	{0xAF, 0x0B},
	{0xB0, 0x0B},
	{0xB1, 0x0B},
	{0xB2, 0x0B},
	{0xB3, 0x0B},
	{0xB4, 0x0B},
	{0xB5, 0x0B},
	{0xB6, 0x0B},
	{0xB7, 0x0B},
	{0xB8, 0x0B},
	{0xB9, 0x0B},
	{0xBA, 0x0B},
	{0xBB, 0x08},
	{0xBC, 0x05},
	{0xBD, 0x02},
	{0xBD, 0x0F},
	{0xBE, 0x0C},
	{0xBF, 0x09},
	{0xC0, 0x06},
	{0xC1, 0x03},
	{0xC2, 0x01},
	{0xC2, 0x0E},
	{0xC3, 0x0B},
	{0xC4, 0x08},
	{0xC5, 0x05},
	{0xC6, 0x02},
	{0xC6, 0x0F},
	{0xC7, 0x0C},
	{0xC8, 0x09},
	{0xC9, 0x07},
	{0xCA, 0x04},
	{0xCB, 0x01},
	{0xCB, 0x0E},
	{0xCC, 0x0B},
	{0xCD, 0x08},
	{0xCE, 0x05},
	{0xCF, 0x02},
	{0xCF, 0x0F},
	{0xD0, 0x0D},
	{0xD1, 0x06},
	{0xD1, 0x0F},
	{0xD2, 0x09},
	{0xD3, 0x02},
	{0xD3, 0x0B},
	{0xD4, 0x04},
	{0xD4, 0x0E},
	{0xD5, 0x07}, /* MAX */
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xD5, 0x07},
	{0xFD, 0x0D},	/*HBM*/
};


static struct maptbl t260_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(t260_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [FT8203 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 T260_SLEEP_OUT[] = { 0x11 };
static u8 T260_SLEEP_IN[] = { 0x10 };
static u8 T260_DISPLAY_OFF[] = { 0x28 };
static u8 T260_DISPLAY_ON[] = { 0x29 };

static u8 T260_BRIGHTNESS[] = {
	0x51,
	0x64, 0x0B,
};

static u8 T260_BRIGHTNESS_MODE[] = {
	0x53,
	0x24,
};

static u8 T260_TE_OFF[] = {
	0x35,
	0x00,
};

static u8 T260_FT8203_001[] = {
	0x00,
	0x00,
};

static u8 T260_FT8203_002[] = {
	0xFF,
	0x82, 0x01, 0x01,
};

static u8 T260_FT8203_003[] = {
	0x00,
	0x80,
};

static u8 T260_FT8203_004[] = {
	0xFF,
	0x82, 0x01,
};

static u8 T260_FT8203_005[] = {
	0x00,
	0x93,
};

static u8 T260_FT8203_006[] = {
	0xC5,
	0x66,
};

static u8 T260_FT8203_007[] = {
	0x00,
	0x97,
};

static u8 T260_FT8203_008[] = {
	0xC5,
	0x66,
};

static u8 T260_FT8203_009[] = {
	0x00,
	0x9E,
};

static u8 T260_FT8203_010[] = {
	0xC5,
	0x05,
};

static u8 T260_FT8203_011[] = {
	0x00,
	0x9A,
};

static u8 T260_FT8203_012[] = {
	0xC5,
	0xE1,
};

static u8 T260_FT8203_013[] = {
	0x00,
	0x9C,
};

static u8 T260_FT8203_014[] = {
	0xC5,
	0xE1,
};

static u8 T260_FT8203_015[] = {
	0x00,
	0xB6,
};

static u8 T260_FT8203_016[] = {
	0xC5,
	0x57, 0x57,
};

static u8 T260_FT8203_017[] = {
	0x00,
	0xB8,
};

static u8 T260_FT8203_018[] = {
	0xC5,
	0x57, 0x57,
};

static u8 T260_FT8203_019[] = {
	0x00,
	0xA0,
};

static u8 T260_FT8203_020[] = {
	0xA5,
	0x04,
};

static u8 T260_FT8203_021[] = {
	0x00,
	0x00,
};

static u8 T260_FT8203_022[] = {
	0xD8,
	0xC8, 0xC8,
};

static u8 T260_FT8203_023[] = {
	0x00,
	0x82,
};

static u8 T260_FT8203_024[] = {
	0xC5,
	0x95,
};

static u8 T260_FT8203_025[] = {
	0x00,
	0x83,
};

static u8 T260_FT8203_026[] = {
	0xC5,
	0x07,
};

static u8 T260_FT8203_027[] = {
	0x00,
	0x00,
};

static u8 T260_FT8203_028[] = {
	0xE1,
	0x05, 0x09, 0x13,
};

static u8 T260_FT8203_029[] = {
	0x00,
	0x03,
};

static u8 T260_FT8203_030[] = {
	0xE1,
	0x1F, 0x27, 0x2F,
};

static u8 T260_FT8203_031[] = {
	0x00,
	0x06,
};

static u8 T260_FT8203_032[] = {
	0xE1,
	0x3F, 0x4C, 0x4E,
};

static u8 T260_FT8203_033[] = {
	0x00,
	0x09,
};

static u8 T260_FT8203_034[] = {
	0xE1,
	0x5B, 0x60, 0x78,
};

static u8 T260_FT8203_035[] = {
	0x00,
	0x0C,
};

static u8 T260_FT8203_036[] = {
	0xE1,
	0x8C, 0x75, 0x73,
};

static u8 T260_FT8203_037[] = {
	0x00,
	0x0F,
};

static u8 T260_FT8203_038[] = {
	0xE1,
	0x67,
};

static u8 T260_FT8203_039[] = {
	0x00,
	0x10,
};

static u8 T260_FT8203_040[] = {
	0xE1,
	0x5D, 0x4D, 0x3D,
};

static u8 T260_FT8203_041[] = {
	0x00,
	0x13,
};

static u8 T260_FT8203_042[] = {
	0xE1,
	0x33, 0x2A, 0x1A,
};

static u8 T260_FT8203_043[] = {
	0x00,
	0x16,
};

static u8 T260_FT8203_044[] = {
	0xE1,
	0x07, 0x00,
};

static u8 T260_FT8203_045[] = {
	0x00,
	0x00,
};

static u8 T260_FT8203_046[] = {
	0xE2,
	0x05, 0x09, 0x13,
};

static u8 T260_FT8203_047[] = {
	0x00,
	0x03,
};

static u8 T260_FT8203_048[] = {
	0xE2,
	0x1F, 0x27, 0x2F,
};

static u8 T260_FT8203_049[] = {
	0x00,
	0x06,
};

static u8 T260_FT8203_050[] = {
	0xE2,
	0x3F, 0x4C, 0x4E,
};

static u8 T260_FT8203_051[] = {
	0x00,
	0x09,
};

static u8 T260_FT8203_052[] = {
	0xE2,
	0x5B, 0x60, 0x78,
};

static u8 T260_FT8203_053[] = {
	0x00,
	0x0C,
};

static u8 T260_FT8203_054[] = {
	0xE2,
	0x84, 0x6D, 0x6B,
};

static u8 T260_FT8203_055[] = {
	0x00,
	0x0F,
};

static u8 T260_FT8203_056[] = {
	0xE2,
	0x5F,
};

static u8 T260_FT8203_057[] = {
	0x00,
	0x10,
};

static u8 T260_FT8203_058[] = {
	0xE2,
	0x55, 0x4D, 0x3D,
};

static u8 T260_FT8203_059[] = {
	0x00,
	0x13,
};

static u8 T260_FT8203_060[] = {
	0xE2,
	0x33, 0x2A, 0x1A,
};

static u8 T260_FT8203_061[] = {
	0x00,
	0x16,
};

static u8 T260_FT8203_062[] = {
	0xE2,
	0x07, 0x00,
};

static u8 T260_FT8203_063[] = {
	0x00,
	0x80,
};

static u8 T260_FT8203_064[] = {
	0xA4,
	0x8C,
};

static u8 T260_FT8203_065[] = {
	0x00,
	0x84,
};

static u8 T260_FT8203_066[] = {
	0xB0,
	0x18,
};

static u8 T260_FT8203_067[] = {
	0x00,
	0xA0,
};

static u8 T260_FT8203_068[] = {
	0xF3,
	0x10,
};

static u8 T260_FT8203_069[] = {
	0x00,
	0xA1,
};

static u8 T260_FT8203_070[] = {
	0xB3,
	0x03, 0x20,
};

static u8 T260_FT8203_071[] = {
	0x00,
	0xA3,
};

static u8 T260_FT8203_072[] = {
	0xB3,
	0x05, 0x78,
};

static u8 T260_FT8203_073[] = {
	0x00,
	0xA5,
};

static u8 T260_FT8203_074[] = {
	0xB3,
	0x00, 0x13,
};

static u8 T260_FT8203_075[] = {
	0x00,
	0xD0,
};

static u8 T260_FT8203_076[] = {
	0xC1,
	0x30,
};

static u8 T260_FT8203_077[] = {
	0x00,
	0x80,
};

static u8 T260_FT8203_078[] = {
	0xCB,
	0x33, 0x33, 0x30,
};

static u8 T260_FT8203_079[] = {
	0x00,
	0x83,
};

static u8 T260_FT8203_080[] = {
	0xCB,
	0x33, 0x30, 0x33,
};

static u8 T260_FT8203_081[] = {
	0x00,
	0x86,
};

static u8 T260_FT8203_082[] = {
	0xCB,
	0x30,
};

static u8 T260_FT8203_083[] = {
	0x00,
	0x87,
};

static u8 T260_FT8203_084[] = {
	0xCB,
	0x30,
};

static u8 T260_FT8203_085[] = {
	0x00,
	0x88,
};

static u8 T260_FT8203_086[] = {
	0xCB,
	0x33, 0x33, 0x33,
};

static u8 T260_FT8203_087[] = {
	0x00,
	0x8B,
};

static u8 T260_FT8203_088[] = {
	0xCB,
	0x33, 0x33, 0x30,
};

static u8 T260_FT8203_089[] = {
	0x00,
	0x8E,
};

static u8 T260_FT8203_090[] = {
	0xCB,
	0x33, 0x33,
};

static u8 T260_FT8203_091[] = {
	0x00,
	0x90,
};

static u8 T260_FT8203_092[] = {
	0xCB,
	0x30, 0x33, 0x33,
};

static u8 T260_FT8203_093[] = {
	0x00,
	0x93,
};

static u8 T260_FT8203_094[] = {
	0xCB,
	0x33, 0x30, 0x30,
};

static u8 T260_FT8203_095[] = {
	0x00,
	0x96,
};

static u8 T260_FT8203_096[] = {
	0xCB,
	0x33,
};

static u8 T260_FT8203_097[] = {
	0x00,
	0x97,
};

static u8 T260_FT8203_098[] = {
	0xCB,
	0x33,
};

static u8 T260_FT8203_099[] = {
	0x00,
	0x98,
};

static u8 T260_FT8203_100[] = {
	0xCB,
	0x04, 0x04, 0x04,
};

static u8 T260_FT8203_101[] = {
	0x00,
	0x9B,
};

static u8 T260_FT8203_102[] = {
	0xCB,
	0x04, 0x04, 0x04,
};

static u8 T260_FT8203_103[] = {
	0x00,
	0x9E,
};

static u8 T260_FT8203_104[] = {
	0xCB,
	0x04, 0x04,
};

static u8 T260_FT8203_105[] = {
	0x00,
	0xA0,
};

static u8 T260_FT8203_106[] = {
	0xCB,
	0x04, 0x04, 0x04,
};

static u8 T260_FT8203_107[] = {
	0x00,
	0xA3,
};

static u8 T260_FT8203_108[] = {
	0xCB,
	0x04, 0x04, 0x04,
};

static u8 T260_FT8203_109[] = {
	0x00,
	0xA6,
};

static u8 T260_FT8203_110[] = {
	0xCB,
	0x04, 0x04,
};

static u8 T260_FT8203_111[] = {
	0x00,
	0xA8,
};

static u8 T260_FT8203_112[] = {
	0xCB,
	0x04, 0x04, 0x04,
};

static u8 T260_FT8203_113[] = {
	0x00,
	0xAB,
};

static u8 T260_FT8203_114[] = {
	0xCB,
	0x04, 0x04, 0x04,
};

static u8 T260_FT8203_115[] = {
	0x00,
	0xAE,
};

static u8 T260_FT8203_116[] = {
	0xCB,
	0x04, 0x04,
};

static u8 T260_FT8203_117[] = {
	0x00,
	0xB0,
};

static u8 T260_FT8203_118[] = {
	0xCB,
	0x00, 0x00, 0x00,
};

static u8 T260_FT8203_119[] = {
	0x00,
	0xB3,
};

static u8 T260_FT8203_120[] = {
	0xCB,
	0x00, 0x00, 0x00,
};

static u8 T260_FT8203_121[] = {
	0x00,
	0xB6,
};

static u8 T260_FT8203_122[] = {
	0xCB,
	0x00,
};

static u8 T260_FT8203_123[] = {
	0x00,
	0xB7,
};

static u8 T260_FT8203_124[] = {
	0xCB,
	0x00,
};

static u8 T260_FT8203_125[] = {
	0x00,
	0xB8,
};

static u8 T260_FT8203_126[] = {
	0xCB,
	0x00, 0x00, 0x00,
};

static u8 T260_FT8203_127[] = {
	0x00,
	0xBB,
};

static u8 T260_FT8203_128[] = {
	0xCB,
	0x00, 0x00, 0x00,
};

static u8 T260_FT8203_129[] = {
	0x00,
	0xBE,
};

static u8 T260_FT8203_130[] = {
	0xCB,
	0x00, 0x00,
};

static u8 T260_FT8203_131[] = {
	0x00,
	0xC0,
};

static u8 T260_FT8203_132[] = {
	0xCB,
	0x00, 0x00, 0x00,
};

static u8 T260_FT8203_133[] = {
	0x00,
	0xC3,
};

static u8 T260_FT8203_134[] = {
	0xCB,
	0x00, 0x00, 0x00,
};

static u8 T260_FT8203_135[] = {
	0x00,
	0xC6,
};

static u8 T260_FT8203_136[] = {
	0xCB,
	0x00,
};

static u8 T260_FT8203_137[] = {
	0x00,
	0xC7,
};

static u8 T260_FT8203_138[] = {
	0xCB,
	0x00,
};

static u8 T260_FT8203_139[] = {
	0x00,
	0x80,
};

static u8 T260_FT8203_140[] = {
	0xCC,
	0x00, 0x00, 0x07,
};

static u8 T260_FT8203_141[] = {
	0x00,
	0x83,
};

static u8 T260_FT8203_142[] = {
	0xCC,
	0x07, 0x09, 0x09,
};

static u8 T260_FT8203_143[] = {
	0x00,
	0x86,
};

static u8 T260_FT8203_144[] = {
	0xCC,
	0x2B, 0x2B,
};

static u8 T260_FT8203_145[] = {
	0x00,
	0x88,
};

static u8 T260_FT8203_146[] = {
	0xCC,
	0x01, 0x01, 0x34,
};

static u8 T260_FT8203_147[] = {
	0x00,
	0x8B,
};

static u8 T260_FT8203_148[] = {
	0xCC,
	0x34, 0x35, 0x35,
};

static u8 T260_FT8203_149[] = {
	0x00,
	0x8E,
};

static u8 T260_FT8203_150[] = {
	0xCC,
	0x17, 0x17,
};

static u8 T260_FT8203_151[] = {
	0x00,
	0x90,
};

static u8 T260_FT8203_152[] = {
	0xCC,
	0x18, 0x18, 0x19,
};

static u8 T260_FT8203_153[] = {
	0x00,
	0x93,
};

static u8 T260_FT8203_154[] = {
	0xCC,
	0x19, 0x1A, 0x1A,
};

static u8 T260_FT8203_155[] = {
	0x00,
	0x80,
};

static u8 T260_FT8203_156[] = {
	0xCD,
	0x00, 0x00, 0x08,
};

static u8 T260_FT8203_157[] = {
	0x00,
	0x83,
};

static u8 T260_FT8203_158[] = {
	0xCD,
	0x08, 0x0A, 0x0A,
};

static u8 T260_FT8203_159[] = {
	0x00,
	0x86,
};

static u8 T260_FT8203_160[] = {
	0xCD,
	0x2B, 0x2B,
};

static u8 T260_FT8203_161[] = {
	0x00,
	0x88,
};

static u8 T260_FT8203_162[] = {
	0xCD,
	0x01, 0x01, 0x34,
};

static u8 T260_FT8203_163[] = {
	0x00,
	0x8B,
};

static u8 T260_FT8203_164[] = {
	0xCD,
	0x34, 0x35, 0x35,
};

static u8 T260_FT8203_165[] = {
	0x00,
	0x8E,
};

static u8 T260_FT8203_166[] = {
	0xCD,
	0x13, 0x13,
};

static u8 T260_FT8203_167[] = {
	0x00,
	0x90,
};

static u8 T260_FT8203_168[] = {
	0xCD,
	0x14, 0x14, 0x15,
};

static u8 T260_FT8203_169[] = {
	0x00,
	0x93,
};

static u8 T260_FT8203_170[] = {
	0xCD,
	0x15, 0x16, 0x16,
};

static u8 T260_FT8203_171[] = {
	0x00,
	0xA0,
};

static u8 T260_FT8203_172[] = {
	0xCC,
	0x00, 0x00, 0x0A,
};

static u8 T260_FT8203_173[] = {
	0x00,
	0xA3,
};

static u8 T260_FT8203_174[] = {
	0xCC,
	0x0A, 0x08, 0x08,
};

static u8 T260_FT8203_175[] = {
	0x00,
	0xA6,
};

static u8 T260_FT8203_176[] = {
	0xCC,
	0x2B, 0x2B,
};

static u8 T260_FT8203_177[] = {
	0x00,
	0xA8,
};

static u8 T260_FT8203_178[] = {
	0xCC,
	0x01, 0x01, 0x34,
};

static u8 T260_FT8203_179[] = {
	0x00,
	0xAB,
};

static u8 T260_FT8203_180[] = {
	0xCC,
	0x34, 0x35, 0x35,
};

static u8 T260_FT8203_181[] = {
	0x00,
	0xAE,
};

static u8 T260_FT8203_182[] = {
	0xCC,
	0x16, 0x16,
};

static u8 T260_FT8203_183[] = {
	0x00,
	0xB0,
};

static u8 T260_FT8203_184[] = {
	0xCC,
	0x15, 0x15, 0x14,
};

static u8 T260_FT8203_185[] = {
	0x00,
	0xB3,
};

static u8 T260_FT8203_186[] = {
	0xCC,
	0x14, 0x13, 0x13,
};

static u8 T260_FT8203_187[] = {
	0x00,
	0xA0,
};

static u8 T260_FT8203_188[] = {
	0xCD,
	0x00, 0x00, 0x09,
};

static u8 T260_FT8203_189[] = {
	0x00,
	0xA3,
};

static u8 T260_FT8203_190[] = {
	0xCD,
	0x09, 0x07, 0x07,
};

static u8 T260_FT8203_191[] = {
	0x00,
	0xA6,
};

static u8 T260_FT8203_192[] = {
	0xCD,
	0x2B, 0x2B,
};

static u8 T260_FT8203_193[] = {
	0x00,
	0xA8,
};

static u8 T260_FT8203_194[] = {
	0xCD,
	0x01, 0x01, 0x34,
};

static u8 T260_FT8203_195[] = {
	0x00,
	0xAB,
};

static u8 T260_FT8203_196[] = {
	0xCD,
	0x34, 0x35, 0x35,
};

static u8 T260_FT8203_197[] = {
	0x00,
	0xAE,
};

static u8 T260_FT8203_198[] = {
	0xCD,
	0x1A, 0x1A,
};

static u8 T260_FT8203_199[] = {
	0x00,
	0xB0,
};

static u8 T260_FT8203_200[] = {
	0xCD,
	0x19, 0x19, 0x18,
};

static u8 T260_FT8203_201[] = {
	0x00,
	0xB3,
};

static u8 T260_FT8203_202[] = {
	0xCD,
	0x18, 0x17, 0x17,
};

static u8 T260_FT8203_203[] = {
	0x00,
	0x81,
};

static u8 T260_FT8203_204[] = {
	0xC2,
	0x40,
};

static u8 T260_FT8203_205[] = {
	0x00,
	0x90,
};

static u8 T260_FT8203_206[] = {
	0xC2,
	0x84, 0x01, 0x19,
};

static u8 T260_FT8203_207[] = {
	0x00,
	0x93,
};

static u8 T260_FT8203_208[] = {
	0xC2,
	0x87,
};

static u8 T260_FT8203_209[] = {
	0x00,
	0x94,
};

static u8 T260_FT8203_210[] = {
	0xC2,
	0x85, 0x01, 0x19,
};

static u8 T260_FT8203_211[] = {
	0x00,
	0x97,
};

static u8 T260_FT8203_212[] = {
	0xC2,
	0x87,
};

static u8 T260_FT8203_213[] = {
	0x00,
	0x98,
};

static u8 T260_FT8203_214[] = {
	0xC2,
	0x86, 0x01, 0x19,
};

static u8 T260_FT8203_215[] = {
	0x00,
	0x9B,
};

static u8 T260_FT8203_216[] = {
	0xC2,
	0x87,
};

static u8 T260_FT8203_217[] = {
	0x00,
	0x9C,
};

static u8 T260_FT8203_218[] = {
	0xC2,
	0x87, 0x01, 0x19,
};

static u8 T260_FT8203_219[] = {
	0x00,
	0x9F,
};

static u8 T260_FT8203_220[] = {
	0xC2,
	0x87,
};

static u8 T260_FT8203_221[] = {
	0x00,
	0xA0,
};

static u8 T260_FT8203_222[] = {
	0xC3,
	0x12, 0x00, 0x08,
};

static u8 T260_FT8203_223[] = {
	0x00,
	0xA3,
};

static u8 T260_FT8203_224[] = {
	0xC3,
	0x04, 0x94, 0x1A,
};

static u8 T260_FT8203_225[] = {
	0x00,
	0xA6,
};

static u8 T260_FT8203_226[] = {
	0xC3,
	0x00, 0x00,
};

static u8 T260_FT8203_227[] = {
	0x00,
	0xE0,
};

static u8 T260_FT8203_228[] = {
	0xC3,
	0x35, 0x40, 0x00,
};

static u8 T260_FT8203_229[] = {
	0x00,
	0xE3,
};

static u8 T260_FT8203_230[] = {
	0xC3,
	0x76,
};

static u8 T260_FT8203_231[] = {
	0x00,
	0xF0,
};

static u8 T260_FT8203_232[] = {
	0xCC,
	0x96, 0x1F, 0x00,
};

static u8 T260_FT8203_233[] = {
	0x00,
	0xF3,
};

static u8 T260_FT8203_234[] = {
	0xCC,
	0x00, 0x00,
};

static u8 T260_FT8203_235[] = {
	0x00,
	0x80,
};

static u8 T260_FT8203_236[] = {
	0xC3,
	0x85, 0x00, 0x13,
};

static u8 T260_FT8203_237[] = {
	0x00,
	0x83,
};

static u8 T260_FT8203_238[] = {
	0xC3,
	0xB4, 0xA5, 0x00,
};

static u8 T260_FT8203_239[] = {
	0x00,
	0x86,
};

static u8 T260_FT8203_240[] = {
	0xC3,
	0x00, 0x07,
};

static u8 T260_FT8203_241[] = {
	0x00,
	0x88,
};

static u8 T260_FT8203_242[] = {
	0xC3,
	0x87, 0x82, 0x13,
};

static u8 T260_FT8203_243[] = {
	0x00,
	0x8B,
};

static u8 T260_FT8203_244[] = {
	0xC3,
	0xB4, 0xA5, 0x00,
};

static u8 T260_FT8203_245[] = {
	0x00,
	0x8E,
};

static u8 T260_FT8203_246[] = {
	0xC3,
	0x00, 0x07,
};

static u8 T260_FT8203_247[] = {
	0x00,
	0x90,
};

static u8 T260_FT8203_248[] = {
	0xC3,
	0x81, 0x04, 0x13,
};

static u8 T260_FT8203_249[] = {
	0x00,
	0x93,
};

static u8 T260_FT8203_250[] = {
	0xC3,
	0xB4, 0xA5, 0x00,
};

static u8 T260_FT8203_251[] = {
	0x00,
	0x96,
};

static u8 T260_FT8203_252[] = {
	0xC3,
	0x00, 0x07,
};

static u8 T260_FT8203_253[] = {
	0x00,
	0x98,
};

static u8 T260_FT8203_254[] = {
	0xC3,
	0x83, 0x02, 0x13,
};

static u8 T260_FT8203_255[] = {
	0x00,
	0x9B,
};

static u8 T260_FT8203_256[] = {
	0xC3,
	0xB4, 0xA5, 0x00,
};

static u8 T260_FT8203_257[] = {
	0x00,
	0x9E,
};

static u8 T260_FT8203_258[] = {
	0xC3,
	0x00, 0x07,
};

static u8 T260_FT8203_259[] = {
	0x00,
	0xC0,
};

static u8 T260_FT8203_260[] = {
	0xCD,
	0x84, 0x01, 0x13,
};

static u8 T260_FT8203_261[] = {
	0x00,
	0xC3,
};

static u8 T260_FT8203_262[] = {
	0xCD,
	0xB4, 0xA5, 0x00,
};

static u8 T260_FT8203_263[] = {
	0x00,
	0xC6,
};

static u8 T260_FT8203_264[] = {
	0xCD,
	0x00, 0x07,
};

static u8 T260_FT8203_265[] = {
	0x00,
	0xC8,
};

static u8 T260_FT8203_266[] = {
	0xCD,
	0x86, 0x81, 0x13,
};

static u8 T260_FT8203_267[] = {
	0x00,
	0xCB,
};

static u8 T260_FT8203_268[] = {
	0xCD,
	0xB4, 0xA5, 0x00,
};

static u8 T260_FT8203_269[] = {
	0x00,
	0xCE,
};

static u8 T260_FT8203_270[] = {
	0xCD,
	0x00, 0x07,
};

static u8 T260_FT8203_271[] = {
	0x00,
	0xD0,
};

static u8 T260_FT8203_272[] = {
	0xCD,
	0x80, 0x05, 0x13,
};

static u8 T260_FT8203_273[] = {
	0x00,
	0xD3,
};

static u8 T260_FT8203_274[] = {
	0xCD,
	0xB4, 0xA5, 0x00,
};

static u8 T260_FT8203_275[] = {
	0x00,
	0xD6,
};

static u8 T260_FT8203_276[] = {
	0xCD,
	0x00, 0x07,
};

static u8 T260_FT8203_277[] = {
	0x00,
	0xD8,
};

static u8 T260_FT8203_278[] = {
	0xCD,
	0x82, 0x03, 0x13,
};

static u8 T260_FT8203_279[] = {
	0x00,
	0xDB,
};

static u8 T260_FT8203_280[] = {
	0xCD,
	0xB4, 0xA5, 0x00,
};

static u8 T260_FT8203_281[] = {
	0x00,
	0xDE,
};

static u8 T260_FT8203_282[] = {
	0xCD,
	0x00, 0x07,
};

static u8 T260_FT8203_283[] = {
	0x00,
	0x80,
};

static u8 T260_FT8203_284[] = {
	0xC0,
	0x00, 0xBA, 0x00,
};

static u8 T260_FT8203_285[] = {
	0x00,
	0x83,
};

static u8 T260_FT8203_286[] = {
	0xC0,
	0x10, 0x00, 0x10,
};

static u8 T260_FT8203_287[] = {
	0x00,
	0x90,
};

static u8 T260_FT8203_288[] = {
	0xC0,
	0x00, 0xBA, 0x00,
};

static u8 T260_FT8203_289[] = {
	0x00,
	0x93,
};

static u8 T260_FT8203_290[] = {
	0xC0,
	0x10, 0x00, 0x10,
};

static u8 T260_FT8203_291[] = {
	0x00,
	0xA0,
};

static u8 T260_FT8203_292[] = {
	0xC0,
	0x01, 0x5F, 0x00,
};

static u8 T260_FT8203_293[] = {
	0x00,
	0xA3,
};

static u8 T260_FT8203_294[] = {
	0xC0,
	0x10, 0x00, 0x10,
};

static u8 T260_FT8203_295[] = {
	0x00,
	0xB0,
};

static u8 T260_FT8203_296[] = {
	0xC0,
	0x00, 0xBA, 0x00,
};

static u8 T260_FT8203_297[] = {
	0x00,
	0xB3,
};

static u8 T260_FT8203_298[] = {
	0xC0,
	0x10, 0x10,
};

static u8 T260_FT8203_299[] = {
	0x00,
	0xA3,
};

static u8 T260_FT8203_300[] = {
	0xC1,
	0x2D, 0x18, 0x04,
};

static u8 T260_FT8203_301[] = {
	0x00,
	0x80,
};

static u8 T260_FT8203_302[] = {
	0xCE,
	0x01, 0x81, 0xFF,
};

static u8 T260_FT8203_303[] = {
	0x00,
	0x83,
};

static u8 T260_FT8203_304[] = {
	0xCE,
	0xFF, 0x00, 0x38,
};

static u8 T260_FT8203_305[] = {
	0x00,
	0x86,
};

static u8 T260_FT8203_306[] = {
	0xCE,
	0x00, 0x74, 0x00,
};

static u8 T260_FT8203_307[] = {
	0x00,
	0x89,
};

static u8 T260_FT8203_308[] = {
	0xCE,
	0x38, 0x00, 0x3A,
};

static u8 T260_FT8203_309[] = {
	0x00,
	0x90,
};

static u8 T260_FT8203_310[] = {
	0xCE,
	0x00, 0x78, 0x0C,
};

static u8 T260_FT8203_311[] = {
	0x00,
	0x93,
};

static u8 T260_FT8203_312[] = {
	0xCE,
	0x80, 0x00, 0x78,
};

static u8 T260_FT8203_313[] = {
	0x00,
	0x96,
};

static u8 T260_FT8203_314[] = {
	0xCE,
	0x80, 0xFF, 0xFF,
};

static u8 T260_FT8203_315[] = {
	0x00,
	0x99,
};

static u8 T260_FT8203_316[] = {
	0xCE,
	0x00, 0x06, 0x00,
};

static u8 T260_FT8203_317[] = {
	0x00,
	0x9C,
};

static u8 T260_FT8203_318[] = {
	0xCE,
	0x13, 0x08,
};

static u8 T260_FT8203_319[] = {
	0x00,
	0xA0,
};

static u8 T260_FT8203_320[] = {
	0xCE,
	0x10, 0x00, 0x00,
};

static u8 T260_FT8203_321[] = {
	0x00,
	0xB0,
};

static u8 T260_FT8203_322[] = {
	0xCE,
	0x75, 0x00, 0x00,
};

static u8 T260_FT8203_323[] = {
	0x00,
	0xD1,
};

static u8 T260_FT8203_324[] = {
	0xCE,
	0x00, 0x00, 0x01,
};

static u8 T260_FT8203_325[] = {
	0x00,
	0xD4,
};

static u8 T260_FT8203_326[] = {
	0xCE,
	0x00, 0x00, 0x00,
};

static u8 T260_FT8203_327[] = {
	0x00,
	0xD7,
};

static u8 T260_FT8203_328[] = {
	0xCE,
	0x00,
};

static u8 T260_FT8203_329[] = {
	0x00,
	0xE1,
};

static u8 T260_FT8203_330[] = {
	0xCE,
	0x09, 0x01, 0xF4,
};

static u8 T260_FT8203_331[] = {
	0x00,
	0xE4,
};

static u8 T260_FT8203_332[] = {
	0xCE,
	0x01, 0xF4, 0x00,
};

static u8 T260_FT8203_333[] = {
	0x00,
	0xE7,
};

static u8 T260_FT8203_334[] = {
	0xCE,
	0x00,
};

static u8 T260_FT8203_335[] = {
	0x00,
	0xF0,
};

static u8 T260_FT8203_336[] = {
	0xCE,
	0x80, 0x0D, 0x06,
};

static u8 T260_FT8203_337[] = {
	0x00,
	0xF3,
};

static u8 T260_FT8203_338[] = {
	0xCE,
	0x00, 0xB0, 0x01,
};

static u8 T260_FT8203_339[] = {
	0x00,
	0xF6,
};

static u8 T260_FT8203_340[] = {
	0xCE,
	0x98, 0x00, 0x20,
};

static u8 T260_FT8203_341[] = {
	0x00,
	0xF9,
};

static u8 T260_FT8203_342[] = {
	0xCE,
	0x27,
};

static u8 T260_FT8203_343[] = {
	0x00,
	0xB0,
};

static u8 T260_FT8203_344[] = {
	0xCF,
	0x00, 0x00, 0x46,
};

static u8 T260_FT8203_345[] = {
	0x00,
	0xB3,
};

static u8 T260_FT8203_346[] = {
	0xCF,
	0x4A,
};

static u8 T260_FT8203_347[] = {
	0x00,
	0xB5,
};

static u8 T260_FT8203_348[] = {
	0xCF,
	0x02, 0x02, 0xB6,
};

static u8 T260_FT8203_349[] = {
	0x00,
	0xB8,
};

static u8 T260_FT8203_350[] = {
	0xCF,
	0xBA,
};

static u8 T260_FT8203_351[] = {
	0x00,
	0xC0,
};

static u8 T260_FT8203_352[] = {
	0xCF,
	0x05, 0x05, 0x64,
};

static u8 T260_FT8203_353[] = {
	0x00,
	0xC3,
};

static u8 T260_FT8203_354[] = {
	0xCF,
	0x68,
};

static u8 T260_FT8203_355[] = {
	0x00,
	0xC5,
};

static u8 T260_FT8203_356[] = {
	0xCF,
	0x00, 0x05, 0x08,
};

static u8 T260_FT8203_357[] = {
	0x00,
	0xC8,
};

static u8 T260_FT8203_358[] = {
	0xCF,
	0x78,
};

static u8 T260_FT8203_359[] = {
	0x00,
	0xB0,
};

static u8 T260_FT8203_360[] = {
	0xCE,
	0x75,
};

static u8 T260_FT8203_361[] = {
	0x00,
	0xE0,
};

static u8 T260_FT8203_362[] = {
	0xCD,
	0xFF, 0xFF,
};

static u8 T260_FT8203_363[] = {
	0x00,
	0xC0,
};

static u8 T260_FT8203_364[] = {
	0xCC,
	0x01, 0xFE, 0x03,
};

static u8 T260_FT8203_365[] = {
	0x00,
	0xC3,
};

static u8 T260_FT8203_366[] = {
	0xCC,
	0xFC,
};

static u8 T260_FT8203_367[] = {
	0x00,
	0xC4,
};

static u8 T260_FT8203_368[] = {
	0xCC,
	0x07, 0xF8, 0xFF,
};

static u8 T260_FT8203_369[] = {
	0x00,
	0xC7,
};

static u8 T260_FT8203_370[] = {
	0xCC,
	0xFF,
};

static u8 T260_FT8203_371[] = {
	0x00,
	0xC8,
};

static u8 T260_FT8203_372[] = {
	0xCC,
	0xFF, 0xFF, 0x01,
};

static u8 T260_FT8203_373[] = {
	0x00,
	0xCB,
};

static u8 T260_FT8203_374[] = {
	0xCC,
	0xFE,
};

static u8 T260_FT8203_375[] = {
	0x00,
	0xCC,
};

static u8 T260_FT8203_376[] = {
	0xCC,
	0x01, 0xFE, 0x01,
};

static u8 T260_FT8203_377[] = {
	0x00,
	0xCF,
};

static u8 T260_FT8203_378[] = {
	0xCC,
	0xFE,
};

static u8 T260_FT8203_379[] = {
	0x00,
	0xD0,
};

static u8 T260_FT8203_380[] = {
	0xCC,
	0x00, 0x01, 0x02,
};

static u8 T260_FT8203_381[] = {
	0x00,
	0xD3,
};

static u8 T260_FT8203_382[] = {
	0xCC,
	0x03,
};

static u8 T260_FT8203_383[] = {
	0x00,
	0xD4,
};

static u8 T260_FT8203_384[] = {
	0xCC,
	0x03, 0x05, 0x06,
};

static u8 T260_FT8203_385[] = {
	0x00,
	0xD7,
};

static u8 T260_FT8203_386[] = {
	0xCC,
	0x07,
};

static u8 T260_FT8203_387[] = {
	0x00,
	0xD8,
};

static u8 T260_FT8203_388[] = {
	0xCC,
	0x01, 0xFE, 0x04,
};

static u8 T260_FT8203_389[] = {
	0x00,
	0xDB,
};

static u8 T260_FT8203_390[] = {
	0xCC,
	0x0F,
};

static u8 T260_FT8203_391[] = {
	0x00,
	0x90,
};

static u8 T260_FT8203_392[] = {
	0xC4,
	0x88,
};

static u8 T260_FT8203_393[] = {
	0x00,
	0x92,
};

static u8 T260_FT8203_394[] = {
	0xC4,
	0xC0,
};

static u8 T260_FT8203_395[] = {
	0x00,
	0xA8,
};

static u8 T260_FT8203_396[] = {
	0xC5,
	0x09,
};

static u8 T260_FT8203_397[] = {
	0x00,
	0xCB,
};

static u8 T260_FT8203_398[] = {
	0xC5,
	0x01,
};

static u8 T260_FT8203_399[] = {
	0x00,
	0xFD,
};

static u8 T260_FT8203_400[] = {
	0xCB,
	0x82,
};

static u8 T260_FT8203_401[] = {
	0x00,
	0x9F,
};

static u8 T260_FT8203_402[] = {
	0xC5,
	0x00,
};

static u8 T260_FT8203_403[] = {
	0x00,
	0x91,
};

static u8 T260_FT8203_404[] = {
	0xC5,
	0x4C,
};

static u8 T260_FT8203_405[] = {
	0x00,
	0xD7,
};

static u8 T260_FT8203_406[] = {
	0xCE,
	0x01,
};

static u8 T260_FT8203_407[] = {
	0x00,
	0x94,
};

static u8 T260_FT8203_408[] = {
	0xC5,
	0x46,
};

static u8 T260_FT8203_409[] = {
	0x00,
	0x98,
};

static u8 T260_FT8203_410[] = {
	0xC5,
	0x64,
};

static u8 T260_FT8203_411[] = {
	0x00,
	0x9B,
};

static u8 T260_FT8203_412[] = {
	0xC5,
	0x65,
};

static u8 T260_FT8203_413[] = {
	0x00,
	0x9D,
};

static u8 T260_FT8203_414[] = {
	0xC5,
	0x65,
};

static u8 T260_FT8203_415[] = {
	0x00,
	0x9A,
};

static u8 T260_FT8203_416[] = {
	0xCF,
	0xFF,
};

static u8 T260_FT8203_417[] = {
	0x00,
	0x8C,
};

static u8 T260_FT8203_418[] = {
	0xCF,
	0x40, 0x40,
};

static u8 T260_FT8203_419[] = {
	0x00,
	0x9A,
};

static u8 T260_FT8203_420[] = {
	0xF5,
	0x35,
};

static u8 T260_FT8203_421[] = {
	0x00,
	0xA2,
};

static u8 T260_FT8203_422[] = {
	0xF5,
	0x1F,
};

static u8 T260_FT8203_423[] = {
	0x00,
	0x94,
};

static u8 T260_FT8203_424[] = {
	0xC5,
	0x42,
};

static u8 T260_FT8203_425[] = {
	0x00,
	0x98,
};

static u8 T260_FT8203_426[] = {
	0xC5,
	0x24,
};

static u8 T260_FT8203_427[] = {
	0x00,
	0x9B,
};

static u8 T260_FT8203_428[] = {
	0xC5,
	0x25,
};

static u8 T260_FT8203_429[] = {
	0x00,
	0x9D,
};

static u8 T260_FT8203_430[] = {
	0xC5,
	0x25,
};

static u8 T260_FT8203_431[] = {
	0x00,
	0xB0,
};

static u8 T260_FT8203_432[] = {
	0xCA,
	0x09, 0x09, 0x04,
};

static u8 T260_FT8203_433[] = {
	0x00,
	0x00,
};

static u8 T260_FT8203_434[] = {
	0xEC,
	0x00, 0x04, 0x08,
};

static u8 T260_FT8203_435[] = {
	0x00,
	0x03,
};

static u8 T260_FT8203_436[] = {
	0xEC,
	0x0B, 0xC0, 0x0F,
};

static u8 T260_FT8203_437[] = {
	0x00,
	0x06,
};

static u8 T260_FT8203_438[] = {
	0xEC,
	0x13, 0x17, 0x1B,
};

static u8 T260_FT8203_439[] = {
	0x00,
	0x09,
};

static u8 T260_FT8203_440[] = {
	0xEC,
	0xBF, 0x1F, 0x27,
};

static u8 T260_FT8203_441[] = {
	0x00,
	0x0C,
};

static u8 T260_FT8203_442[] = {
	0xEC,
	0x2F, 0x37, 0x16,
};

static u8 T260_FT8203_443[] = {
	0x00,
	0x10,
};

static u8 T260_FT8203_444[] = {
	0xEC,
	0x3E, 0x46, 0x4E,
};

static u8 T260_FT8203_445[] = {
	0x00,
	0x13,
};

static u8 T260_FT8203_446[] = {
	0xEC,
	0x56, 0x6F, 0x5E,
};

static u8 T260_FT8203_447[] = {
	0x00,
	0x16,
};

static u8 T260_FT8203_448[] = {
	0xEC,
	0x66, 0x6E, 0x76,
};

static u8 T260_FT8203_449[] = {
	0x00,
	0x19,
};

static u8 T260_FT8203_450[] = {
	0xEC,
	0x05, 0x7E, 0x86,
};

static u8 T260_FT8203_451[] = {
	0x00,
	0x1C,
};

static u8 T260_FT8203_452[] = {
	0xEC,
	0x8D, 0x95, 0xF0,
};

static u8 T260_FT8203_453[] = {
	0x00,
	0x20,
};

static u8 T260_FT8203_454[] = {
	0xEC,
	0x9E, 0xA6, 0xAE,
};

static u8 T260_FT8203_455[] = {
	0x00,
	0x23,
};

static u8 T260_FT8203_456[] = {
	0xEC,
	0xB6, 0x00, 0xBE,
};

static u8 T260_FT8203_457[] = {
	0x00,
	0x26,
};

static u8 T260_FT8203_458[] = {
	0xEC,
	0xC6, 0xCE, 0xD6,
};

static u8 T260_FT8203_459[] = {
	0x00,
	0x29,
};

static u8 T260_FT8203_460[] = {
	0xEC,
	0x40, 0xDE, 0xE6,
};

static u8 T260_FT8203_461[] = {
	0x00,
	0x2C,
};

static u8 T260_FT8203_462[] = {
	0xEC,
	0xEE, 0xF6, 0xA5,
};

static u8 T260_FT8203_463[] = {
	0x00,
	0x30,
};

static u8 T260_FT8203_464[] = {
	0xEC,
	0xFA, 0xFC, 0xFD,
};

static u8 T260_FT8203_465[] = {
	0x00,
	0x33,
};

static u8 T260_FT8203_466[] = {
	0xEC,
	0x2A,
};

static u8 T260_FT8203_467[] = {
	0x00,
	0x40,
};

static u8 T260_FT8203_468[] = {
	0xEC,
	0x00, 0x04, 0x08,
};

static u8 T260_FT8203_469[] = {
	0x00,
	0x43,
};

static u8 T260_FT8203_470[] = {
	0xEC,
	0x0C, 0x00, 0x10,
};

static u8 T260_FT8203_471[] = {
	0x00,
	0x46,
};

static u8 T260_FT8203_472[] = {
	0xEC,
	0x14, 0x18, 0x1C,
};

static u8 T260_FT8203_473[] = {
	0x00,
	0x49,
};

static u8 T260_FT8203_474[] = {
	0xEC,
	0x00, 0x20, 0x28,
};

static u8 T260_FT8203_475[] = {
	0x00,
	0x4C,
};

static u8 T260_FT8203_476[] = {
	0xEC,
	0x30, 0x38, 0x00,
};

static u8 T260_FT8203_477[] = {
	0x00,
	0x50,
};

static u8 T260_FT8203_478[] = {
	0xEC,
	0x40, 0x48, 0x50,
};

static u8 T260_FT8203_479[] = {
	0x00,
	0x53,
};

static u8 T260_FT8203_480[] = {
	0xEC,
	0x58, 0x54, 0x60,
};

static u8 T260_FT8203_481[] = {
	0x00,
	0x56,
};

static u8 T260_FT8203_482[] = {
	0xEC,
	0x68, 0x70, 0x78,
};

static u8 T260_FT8203_483[] = {
	0x00,
	0x59,
};

static u8 T260_FT8203_484[] = {
	0xEC,
	0x55, 0x80, 0x88,
};

static u8 T260_FT8203_485[] = {
	0x00,
	0x5C,
};

static u8 T260_FT8203_486[] = {
	0xEC,
	0x90, 0x98, 0x55,
};

static u8 T260_FT8203_487[] = {
	0x00,
	0x60,
};

static u8 T260_FT8203_488[] = {
	0xEC,
	0xA0, 0xA8, 0xB0,
};

static u8 T260_FT8203_489[] = {
	0x00,
	0x63,
};

static u8 T260_FT8203_490[] = {
	0xEC,
	0xB8, 0x55, 0xC0,
};

static u8 T260_FT8203_491[] = {
	0x00,
	0x66,
};

static u8 T260_FT8203_492[] = {
	0xEC,
	0xC8, 0xD0, 0xD8,
};

static u8 T260_FT8203_493[] = {
	0x00,
	0x69,
};

static u8 T260_FT8203_494[] = {
	0xEC,
	0x05, 0xE0, 0xE8,
};

static u8 T260_FT8203_495[] = {
	0x00,
	0x6C,
};

static u8 T260_FT8203_496[] = {
	0xEC,
	0xF0, 0xF8, 0x00,
};

static u8 T260_FT8203_497[] = {
	0x00,
	0x70,
};

static u8 T260_FT8203_498[] = {
	0xEC,
	0xFC, 0xFE, 0xFF,
};

static u8 T260_FT8203_499[] = {
	0x00,
	0x73,
};

static u8 T260_FT8203_500[] = {
	0xEC,
	0x00,
};

static u8 T260_FT8203_501[] = {
	0x00,
	0x80,
};

static u8 T260_FT8203_502[] = {
	0xEC,
	0x00, 0x03, 0x07,
};

static u8 T260_FT8203_503[] = {
	0x00,
	0x83,
};

static u8 T260_FT8203_504[] = {
	0xEC,
	0x0B, 0xBC, 0x0F,
};

static u8 T260_FT8203_505[] = {
	0x00,
	0x86,
};

static u8 T260_FT8203_506[] = {
	0xEC,
	0x13, 0x17, 0x1B,
};

static u8 T260_FT8203_507[] = {
	0x00,
	0x89,
};

static u8 T260_FT8203_508[] = {
	0xEC,
	0x16, 0x1F, 0x27,
};

static u8 T260_FT8203_509[] = {
	0x00,
	0x8C,
};

static u8 T260_FT8203_510[] = {
	0xEC,
	0x2F, 0x37, 0x50,
};

static u8 T260_FT8203_511[] = {
	0x00,
	0x90,
};

static u8 T260_FT8203_512[] = {
	0xEC,
	0x3F, 0x47, 0x4F,
};

static u8 T260_FT8203_513[] = {
	0x00,
	0x93,
};

static u8 T260_FT8203_514[] = {
	0xEC,
	0x57, 0x01, 0x5E,
};

static u8 T260_FT8203_515[] = {
	0x00,
	0x96,
};

static u8 T260_FT8203_516[] = {
	0xEC,
	0x66, 0x6E, 0x76,
};

static u8 T260_FT8203_517[] = {
	0x00,
	0x99,
};

static u8 T260_FT8203_518[] = {
	0xEC,
	0x6B, 0x7E, 0x85,
};

static u8 T260_FT8203_519[] = {
	0x00,
	0x9C,
};

static u8 T260_FT8203_520[] = {
	0xEC,
	0x8D, 0x95, 0x18,
};

static u8 T260_FT8203_521[] = {
	0x00,
	0xA0,
};

static u8 T260_FT8203_522[] = {
	0xEC,
	0x9C, 0xA4, 0xAB,
};

static u8 T260_FT8203_523[] = {
	0x00,
	0xA3,
};

static u8 T260_FT8203_524[] = {
	0xEC,
	0xB3, 0x72, 0xBA,
};

static u8 T260_FT8203_525[] = {
	0x00,
	0xA6,
};

static u8 T260_FT8203_526[] = {
	0xEC,
	0xC2, 0xC9, 0xD1,
};

static u8 T260_FT8203_527[] = {
	0x00,
	0xA9,
};

static u8 T260_FT8203_528[] = {
	0xEC,
	0x77, 0xD8, 0xE0,
};

static u8 T260_FT8203_529[] = {
	0x00,
	0xAC,
};

static u8 T260_FT8203_530[] = {
	0xEC,
	0xE8, 0xEF, 0xCB,
};

static u8 T260_FT8203_531[] = {
	0x00,
	0xB0,
};

static u8 T260_FT8203_532[] = {
	0xEC,
	0xF3, 0xF5, 0xF6,
};

static u8 T260_FT8203_533[] = {
	0x00,
	0xB3,
};

static u8 T260_FT8203_534[] = {
	0xEC,
	0x2A,
};

static u8 T260_FT8203_535[] = {
	0x00,
	0xE6,
};

static u8 T260_FT8203_536[] = {
	0xC0,
	0x40,
};

static u8 T260_FT8203_537[] = {
	0x00,
	0x00,
};

static u8 T260_FT8203_538[] = {
	0xFF,
	0x00, 0x00, 0x00,
};

static u8 T260_FT8203_539[] = {
	0x00,
	0x80,
};

static u8 T260_FT8203_540[] = {
	0xFF,
	0x00, 0x00,
};

static DEFINE_STATIC_PACKET(t260_sleep_out, DSI_PKT_TYPE_WR, T260_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(t260_sleep_in, DSI_PKT_TYPE_WR, T260_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(t260_display_on, DSI_PKT_TYPE_WR, T260_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(t260_display_off, DSI_PKT_TYPE_WR, T260_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(t260_brightness_mode, DSI_PKT_TYPE_WR, T260_BRIGHTNESS_MODE, 0);
static DEFINE_STATIC_PACKET(t260_te_off, DSI_PKT_TYPE_WR, T260_TE_OFF, 0);

static DEFINE_PKTUI(t260_brightness, &t260_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(t260_brightness, DSI_PKT_TYPE_WR, T260_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(t260_ft8203_boe_001, DSI_PKT_TYPE_WR, T260_FT8203_001, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_002, DSI_PKT_TYPE_WR, T260_FT8203_002, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_003, DSI_PKT_TYPE_WR, T260_FT8203_003, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_004, DSI_PKT_TYPE_WR, T260_FT8203_004, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_005, DSI_PKT_TYPE_WR, T260_FT8203_005, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_006, DSI_PKT_TYPE_WR, T260_FT8203_006, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_007, DSI_PKT_TYPE_WR, T260_FT8203_007, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_008, DSI_PKT_TYPE_WR, T260_FT8203_008, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_009, DSI_PKT_TYPE_WR, T260_FT8203_009, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_010, DSI_PKT_TYPE_WR, T260_FT8203_010, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_011, DSI_PKT_TYPE_WR, T260_FT8203_011, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_012, DSI_PKT_TYPE_WR, T260_FT8203_012, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_013, DSI_PKT_TYPE_WR, T260_FT8203_013, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_014, DSI_PKT_TYPE_WR, T260_FT8203_014, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_015, DSI_PKT_TYPE_WR, T260_FT8203_015, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_016, DSI_PKT_TYPE_WR, T260_FT8203_016, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_017, DSI_PKT_TYPE_WR, T260_FT8203_017, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_018, DSI_PKT_TYPE_WR, T260_FT8203_018, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_019, DSI_PKT_TYPE_WR, T260_FT8203_019, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_020, DSI_PKT_TYPE_WR, T260_FT8203_020, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_021, DSI_PKT_TYPE_WR, T260_FT8203_021, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_022, DSI_PKT_TYPE_WR, T260_FT8203_022, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_023, DSI_PKT_TYPE_WR, T260_FT8203_023, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_024, DSI_PKT_TYPE_WR, T260_FT8203_024, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_025, DSI_PKT_TYPE_WR, T260_FT8203_025, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_026, DSI_PKT_TYPE_WR, T260_FT8203_026, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_027, DSI_PKT_TYPE_WR, T260_FT8203_027, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_028, DSI_PKT_TYPE_WR, T260_FT8203_028, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_029, DSI_PKT_TYPE_WR, T260_FT8203_029, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_030, DSI_PKT_TYPE_WR, T260_FT8203_030, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_031, DSI_PKT_TYPE_WR, T260_FT8203_031, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_032, DSI_PKT_TYPE_WR, T260_FT8203_032, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_033, DSI_PKT_TYPE_WR, T260_FT8203_033, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_034, DSI_PKT_TYPE_WR, T260_FT8203_034, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_035, DSI_PKT_TYPE_WR, T260_FT8203_035, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_036, DSI_PKT_TYPE_WR, T260_FT8203_036, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_037, DSI_PKT_TYPE_WR, T260_FT8203_037, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_038, DSI_PKT_TYPE_WR, T260_FT8203_038, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_039, DSI_PKT_TYPE_WR, T260_FT8203_039, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_040, DSI_PKT_TYPE_WR, T260_FT8203_040, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_041, DSI_PKT_TYPE_WR, T260_FT8203_041, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_042, DSI_PKT_TYPE_WR, T260_FT8203_042, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_043, DSI_PKT_TYPE_WR, T260_FT8203_043, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_044, DSI_PKT_TYPE_WR, T260_FT8203_044, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_045, DSI_PKT_TYPE_WR, T260_FT8203_045, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_046, DSI_PKT_TYPE_WR, T260_FT8203_046, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_047, DSI_PKT_TYPE_WR, T260_FT8203_047, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_048, DSI_PKT_TYPE_WR, T260_FT8203_048, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_049, DSI_PKT_TYPE_WR, T260_FT8203_049, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_050, DSI_PKT_TYPE_WR, T260_FT8203_050, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_051, DSI_PKT_TYPE_WR, T260_FT8203_051, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_052, DSI_PKT_TYPE_WR, T260_FT8203_052, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_053, DSI_PKT_TYPE_WR, T260_FT8203_053, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_054, DSI_PKT_TYPE_WR, T260_FT8203_054, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_055, DSI_PKT_TYPE_WR, T260_FT8203_055, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_056, DSI_PKT_TYPE_WR, T260_FT8203_056, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_057, DSI_PKT_TYPE_WR, T260_FT8203_057, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_058, DSI_PKT_TYPE_WR, T260_FT8203_058, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_059, DSI_PKT_TYPE_WR, T260_FT8203_059, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_060, DSI_PKT_TYPE_WR, T260_FT8203_060, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_061, DSI_PKT_TYPE_WR, T260_FT8203_061, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_062, DSI_PKT_TYPE_WR, T260_FT8203_062, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_063, DSI_PKT_TYPE_WR, T260_FT8203_063, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_064, DSI_PKT_TYPE_WR, T260_FT8203_064, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_065, DSI_PKT_TYPE_WR, T260_FT8203_065, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_066, DSI_PKT_TYPE_WR, T260_FT8203_066, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_067, DSI_PKT_TYPE_WR, T260_FT8203_067, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_068, DSI_PKT_TYPE_WR, T260_FT8203_068, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_069, DSI_PKT_TYPE_WR, T260_FT8203_069, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_070, DSI_PKT_TYPE_WR, T260_FT8203_070, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_071, DSI_PKT_TYPE_WR, T260_FT8203_071, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_072, DSI_PKT_TYPE_WR, T260_FT8203_072, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_073, DSI_PKT_TYPE_WR, T260_FT8203_073, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_074, DSI_PKT_TYPE_WR, T260_FT8203_074, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_075, DSI_PKT_TYPE_WR, T260_FT8203_075, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_076, DSI_PKT_TYPE_WR, T260_FT8203_076, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_077, DSI_PKT_TYPE_WR, T260_FT8203_077, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_078, DSI_PKT_TYPE_WR, T260_FT8203_078, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_079, DSI_PKT_TYPE_WR, T260_FT8203_079, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_080, DSI_PKT_TYPE_WR, T260_FT8203_080, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_081, DSI_PKT_TYPE_WR, T260_FT8203_081, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_082, DSI_PKT_TYPE_WR, T260_FT8203_082, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_083, DSI_PKT_TYPE_WR, T260_FT8203_083, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_084, DSI_PKT_TYPE_WR, T260_FT8203_084, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_085, DSI_PKT_TYPE_WR, T260_FT8203_085, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_086, DSI_PKT_TYPE_WR, T260_FT8203_086, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_087, DSI_PKT_TYPE_WR, T260_FT8203_087, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_088, DSI_PKT_TYPE_WR, T260_FT8203_088, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_089, DSI_PKT_TYPE_WR, T260_FT8203_089, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_090, DSI_PKT_TYPE_WR, T260_FT8203_090, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_091, DSI_PKT_TYPE_WR, T260_FT8203_091, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_092, DSI_PKT_TYPE_WR, T260_FT8203_092, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_093, DSI_PKT_TYPE_WR, T260_FT8203_093, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_094, DSI_PKT_TYPE_WR, T260_FT8203_094, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_095, DSI_PKT_TYPE_WR, T260_FT8203_095, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_096, DSI_PKT_TYPE_WR, T260_FT8203_096, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_097, DSI_PKT_TYPE_WR, T260_FT8203_097, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_098, DSI_PKT_TYPE_WR, T260_FT8203_098, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_099, DSI_PKT_TYPE_WR, T260_FT8203_099, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_100, DSI_PKT_TYPE_WR, T260_FT8203_100, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_101, DSI_PKT_TYPE_WR, T260_FT8203_101, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_102, DSI_PKT_TYPE_WR, T260_FT8203_102, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_103, DSI_PKT_TYPE_WR, T260_FT8203_103, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_104, DSI_PKT_TYPE_WR, T260_FT8203_104, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_105, DSI_PKT_TYPE_WR, T260_FT8203_105, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_106, DSI_PKT_TYPE_WR, T260_FT8203_106, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_107, DSI_PKT_TYPE_WR, T260_FT8203_107, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_108, DSI_PKT_TYPE_WR, T260_FT8203_108, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_109, DSI_PKT_TYPE_WR, T260_FT8203_109, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_110, DSI_PKT_TYPE_WR, T260_FT8203_110, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_111, DSI_PKT_TYPE_WR, T260_FT8203_111, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_112, DSI_PKT_TYPE_WR, T260_FT8203_112, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_113, DSI_PKT_TYPE_WR, T260_FT8203_113, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_114, DSI_PKT_TYPE_WR, T260_FT8203_114, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_115, DSI_PKT_TYPE_WR, T260_FT8203_115, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_116, DSI_PKT_TYPE_WR, T260_FT8203_116, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_117, DSI_PKT_TYPE_WR, T260_FT8203_117, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_118, DSI_PKT_TYPE_WR, T260_FT8203_118, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_119, DSI_PKT_TYPE_WR, T260_FT8203_119, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_120, DSI_PKT_TYPE_WR, T260_FT8203_120, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_121, DSI_PKT_TYPE_WR, T260_FT8203_121, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_122, DSI_PKT_TYPE_WR, T260_FT8203_122, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_123, DSI_PKT_TYPE_WR, T260_FT8203_123, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_124, DSI_PKT_TYPE_WR, T260_FT8203_124, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_125, DSI_PKT_TYPE_WR, T260_FT8203_125, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_126, DSI_PKT_TYPE_WR, T260_FT8203_126, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_127, DSI_PKT_TYPE_WR, T260_FT8203_127, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_128, DSI_PKT_TYPE_WR, T260_FT8203_128, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_129, DSI_PKT_TYPE_WR, T260_FT8203_129, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_130, DSI_PKT_TYPE_WR, T260_FT8203_130, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_131, DSI_PKT_TYPE_WR, T260_FT8203_131, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_132, DSI_PKT_TYPE_WR, T260_FT8203_132, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_133, DSI_PKT_TYPE_WR, T260_FT8203_133, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_134, DSI_PKT_TYPE_WR, T260_FT8203_134, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_135, DSI_PKT_TYPE_WR, T260_FT8203_135, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_136, DSI_PKT_TYPE_WR, T260_FT8203_136, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_137, DSI_PKT_TYPE_WR, T260_FT8203_137, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_138, DSI_PKT_TYPE_WR, T260_FT8203_138, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_139, DSI_PKT_TYPE_WR, T260_FT8203_139, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_140, DSI_PKT_TYPE_WR, T260_FT8203_140, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_141, DSI_PKT_TYPE_WR, T260_FT8203_141, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_142, DSI_PKT_TYPE_WR, T260_FT8203_142, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_143, DSI_PKT_TYPE_WR, T260_FT8203_143, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_144, DSI_PKT_TYPE_WR, T260_FT8203_144, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_145, DSI_PKT_TYPE_WR, T260_FT8203_145, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_146, DSI_PKT_TYPE_WR, T260_FT8203_146, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_147, DSI_PKT_TYPE_WR, T260_FT8203_147, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_148, DSI_PKT_TYPE_WR, T260_FT8203_148, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_149, DSI_PKT_TYPE_WR, T260_FT8203_149, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_150, DSI_PKT_TYPE_WR, T260_FT8203_150, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_151, DSI_PKT_TYPE_WR, T260_FT8203_151, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_152, DSI_PKT_TYPE_WR, T260_FT8203_152, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_153, DSI_PKT_TYPE_WR, T260_FT8203_153, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_154, DSI_PKT_TYPE_WR, T260_FT8203_154, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_155, DSI_PKT_TYPE_WR, T260_FT8203_155, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_156, DSI_PKT_TYPE_WR, T260_FT8203_156, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_157, DSI_PKT_TYPE_WR, T260_FT8203_157, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_158, DSI_PKT_TYPE_WR, T260_FT8203_158, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_159, DSI_PKT_TYPE_WR, T260_FT8203_159, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_160, DSI_PKT_TYPE_WR, T260_FT8203_160, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_161, DSI_PKT_TYPE_WR, T260_FT8203_161, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_162, DSI_PKT_TYPE_WR, T260_FT8203_162, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_163, DSI_PKT_TYPE_WR, T260_FT8203_163, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_164, DSI_PKT_TYPE_WR, T260_FT8203_164, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_165, DSI_PKT_TYPE_WR, T260_FT8203_165, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_166, DSI_PKT_TYPE_WR, T260_FT8203_166, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_167, DSI_PKT_TYPE_WR, T260_FT8203_167, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_168, DSI_PKT_TYPE_WR, T260_FT8203_168, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_169, DSI_PKT_TYPE_WR, T260_FT8203_169, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_170, DSI_PKT_TYPE_WR, T260_FT8203_170, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_171, DSI_PKT_TYPE_WR, T260_FT8203_171, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_172, DSI_PKT_TYPE_WR, T260_FT8203_172, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_173, DSI_PKT_TYPE_WR, T260_FT8203_173, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_174, DSI_PKT_TYPE_WR, T260_FT8203_174, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_175, DSI_PKT_TYPE_WR, T260_FT8203_175, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_176, DSI_PKT_TYPE_WR, T260_FT8203_176, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_177, DSI_PKT_TYPE_WR, T260_FT8203_177, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_178, DSI_PKT_TYPE_WR, T260_FT8203_178, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_179, DSI_PKT_TYPE_WR, T260_FT8203_179, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_180, DSI_PKT_TYPE_WR, T260_FT8203_180, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_181, DSI_PKT_TYPE_WR, T260_FT8203_181, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_182, DSI_PKT_TYPE_WR, T260_FT8203_182, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_183, DSI_PKT_TYPE_WR, T260_FT8203_183, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_184, DSI_PKT_TYPE_WR, T260_FT8203_184, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_185, DSI_PKT_TYPE_WR, T260_FT8203_185, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_186, DSI_PKT_TYPE_WR, T260_FT8203_186, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_187, DSI_PKT_TYPE_WR, T260_FT8203_187, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_188, DSI_PKT_TYPE_WR, T260_FT8203_188, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_189, DSI_PKT_TYPE_WR, T260_FT8203_189, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_190, DSI_PKT_TYPE_WR, T260_FT8203_190, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_191, DSI_PKT_TYPE_WR, T260_FT8203_191, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_192, DSI_PKT_TYPE_WR, T260_FT8203_192, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_193, DSI_PKT_TYPE_WR, T260_FT8203_193, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_194, DSI_PKT_TYPE_WR, T260_FT8203_194, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_195, DSI_PKT_TYPE_WR, T260_FT8203_195, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_196, DSI_PKT_TYPE_WR, T260_FT8203_196, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_197, DSI_PKT_TYPE_WR, T260_FT8203_197, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_198, DSI_PKT_TYPE_WR, T260_FT8203_198, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_199, DSI_PKT_TYPE_WR, T260_FT8203_199, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_200, DSI_PKT_TYPE_WR, T260_FT8203_200, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_201, DSI_PKT_TYPE_WR, T260_FT8203_201, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_202, DSI_PKT_TYPE_WR, T260_FT8203_202, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_203, DSI_PKT_TYPE_WR, T260_FT8203_203, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_204, DSI_PKT_TYPE_WR, T260_FT8203_204, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_205, DSI_PKT_TYPE_WR, T260_FT8203_205, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_206, DSI_PKT_TYPE_WR, T260_FT8203_206, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_207, DSI_PKT_TYPE_WR, T260_FT8203_207, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_208, DSI_PKT_TYPE_WR, T260_FT8203_208, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_209, DSI_PKT_TYPE_WR, T260_FT8203_209, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_210, DSI_PKT_TYPE_WR, T260_FT8203_210, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_211, DSI_PKT_TYPE_WR, T260_FT8203_211, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_212, DSI_PKT_TYPE_WR, T260_FT8203_212, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_213, DSI_PKT_TYPE_WR, T260_FT8203_213, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_214, DSI_PKT_TYPE_WR, T260_FT8203_214, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_215, DSI_PKT_TYPE_WR, T260_FT8203_215, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_216, DSI_PKT_TYPE_WR, T260_FT8203_216, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_217, DSI_PKT_TYPE_WR, T260_FT8203_217, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_218, DSI_PKT_TYPE_WR, T260_FT8203_218, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_219, DSI_PKT_TYPE_WR, T260_FT8203_219, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_220, DSI_PKT_TYPE_WR, T260_FT8203_220, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_221, DSI_PKT_TYPE_WR, T260_FT8203_221, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_222, DSI_PKT_TYPE_WR, T260_FT8203_222, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_223, DSI_PKT_TYPE_WR, T260_FT8203_223, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_224, DSI_PKT_TYPE_WR, T260_FT8203_224, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_225, DSI_PKT_TYPE_WR, T260_FT8203_225, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_226, DSI_PKT_TYPE_WR, T260_FT8203_226, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_227, DSI_PKT_TYPE_WR, T260_FT8203_227, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_228, DSI_PKT_TYPE_WR, T260_FT8203_228, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_229, DSI_PKT_TYPE_WR, T260_FT8203_229, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_230, DSI_PKT_TYPE_WR, T260_FT8203_230, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_231, DSI_PKT_TYPE_WR, T260_FT8203_231, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_232, DSI_PKT_TYPE_WR, T260_FT8203_232, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_233, DSI_PKT_TYPE_WR, T260_FT8203_233, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_234, DSI_PKT_TYPE_WR, T260_FT8203_234, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_235, DSI_PKT_TYPE_WR, T260_FT8203_235, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_236, DSI_PKT_TYPE_WR, T260_FT8203_236, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_237, DSI_PKT_TYPE_WR, T260_FT8203_237, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_238, DSI_PKT_TYPE_WR, T260_FT8203_238, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_239, DSI_PKT_TYPE_WR, T260_FT8203_239, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_240, DSI_PKT_TYPE_WR, T260_FT8203_240, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_241, DSI_PKT_TYPE_WR, T260_FT8203_241, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_242, DSI_PKT_TYPE_WR, T260_FT8203_242, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_243, DSI_PKT_TYPE_WR, T260_FT8203_243, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_244, DSI_PKT_TYPE_WR, T260_FT8203_244, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_245, DSI_PKT_TYPE_WR, T260_FT8203_245, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_246, DSI_PKT_TYPE_WR, T260_FT8203_246, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_247, DSI_PKT_TYPE_WR, T260_FT8203_247, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_248, DSI_PKT_TYPE_WR, T260_FT8203_248, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_249, DSI_PKT_TYPE_WR, T260_FT8203_249, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_250, DSI_PKT_TYPE_WR, T260_FT8203_250, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_251, DSI_PKT_TYPE_WR, T260_FT8203_251, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_252, DSI_PKT_TYPE_WR, T260_FT8203_252, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_253, DSI_PKT_TYPE_WR, T260_FT8203_253, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_254, DSI_PKT_TYPE_WR, T260_FT8203_254, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_255, DSI_PKT_TYPE_WR, T260_FT8203_255, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_256, DSI_PKT_TYPE_WR, T260_FT8203_256, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_257, DSI_PKT_TYPE_WR, T260_FT8203_257, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_258, DSI_PKT_TYPE_WR, T260_FT8203_258, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_259, DSI_PKT_TYPE_WR, T260_FT8203_259, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_260, DSI_PKT_TYPE_WR, T260_FT8203_260, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_261, DSI_PKT_TYPE_WR, T260_FT8203_261, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_262, DSI_PKT_TYPE_WR, T260_FT8203_262, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_263, DSI_PKT_TYPE_WR, T260_FT8203_263, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_264, DSI_PKT_TYPE_WR, T260_FT8203_264, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_265, DSI_PKT_TYPE_WR, T260_FT8203_265, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_266, DSI_PKT_TYPE_WR, T260_FT8203_266, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_267, DSI_PKT_TYPE_WR, T260_FT8203_267, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_268, DSI_PKT_TYPE_WR, T260_FT8203_268, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_269, DSI_PKT_TYPE_WR, T260_FT8203_269, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_270, DSI_PKT_TYPE_WR, T260_FT8203_270, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_271, DSI_PKT_TYPE_WR, T260_FT8203_271, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_272, DSI_PKT_TYPE_WR, T260_FT8203_272, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_273, DSI_PKT_TYPE_WR, T260_FT8203_273, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_274, DSI_PKT_TYPE_WR, T260_FT8203_274, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_275, DSI_PKT_TYPE_WR, T260_FT8203_275, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_276, DSI_PKT_TYPE_WR, T260_FT8203_276, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_277, DSI_PKT_TYPE_WR, T260_FT8203_277, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_278, DSI_PKT_TYPE_WR, T260_FT8203_278, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_279, DSI_PKT_TYPE_WR, T260_FT8203_279, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_280, DSI_PKT_TYPE_WR, T260_FT8203_280, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_281, DSI_PKT_TYPE_WR, T260_FT8203_281, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_282, DSI_PKT_TYPE_WR, T260_FT8203_282, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_283, DSI_PKT_TYPE_WR, T260_FT8203_283, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_284, DSI_PKT_TYPE_WR, T260_FT8203_284, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_285, DSI_PKT_TYPE_WR, T260_FT8203_285, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_286, DSI_PKT_TYPE_WR, T260_FT8203_286, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_287, DSI_PKT_TYPE_WR, T260_FT8203_287, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_288, DSI_PKT_TYPE_WR, T260_FT8203_288, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_289, DSI_PKT_TYPE_WR, T260_FT8203_289, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_290, DSI_PKT_TYPE_WR, T260_FT8203_290, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_291, DSI_PKT_TYPE_WR, T260_FT8203_291, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_292, DSI_PKT_TYPE_WR, T260_FT8203_292, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_293, DSI_PKT_TYPE_WR, T260_FT8203_293, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_294, DSI_PKT_TYPE_WR, T260_FT8203_294, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_295, DSI_PKT_TYPE_WR, T260_FT8203_295, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_296, DSI_PKT_TYPE_WR, T260_FT8203_296, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_297, DSI_PKT_TYPE_WR, T260_FT8203_297, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_298, DSI_PKT_TYPE_WR, T260_FT8203_298, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_299, DSI_PKT_TYPE_WR, T260_FT8203_299, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_300, DSI_PKT_TYPE_WR, T260_FT8203_300, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_301, DSI_PKT_TYPE_WR, T260_FT8203_301, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_302, DSI_PKT_TYPE_WR, T260_FT8203_302, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_303, DSI_PKT_TYPE_WR, T260_FT8203_303, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_304, DSI_PKT_TYPE_WR, T260_FT8203_304, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_305, DSI_PKT_TYPE_WR, T260_FT8203_305, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_306, DSI_PKT_TYPE_WR, T260_FT8203_306, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_307, DSI_PKT_TYPE_WR, T260_FT8203_307, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_308, DSI_PKT_TYPE_WR, T260_FT8203_308, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_309, DSI_PKT_TYPE_WR, T260_FT8203_309, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_310, DSI_PKT_TYPE_WR, T260_FT8203_310, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_311, DSI_PKT_TYPE_WR, T260_FT8203_311, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_312, DSI_PKT_TYPE_WR, T260_FT8203_312, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_313, DSI_PKT_TYPE_WR, T260_FT8203_313, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_314, DSI_PKT_TYPE_WR, T260_FT8203_314, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_315, DSI_PKT_TYPE_WR, T260_FT8203_315, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_316, DSI_PKT_TYPE_WR, T260_FT8203_316, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_317, DSI_PKT_TYPE_WR, T260_FT8203_317, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_318, DSI_PKT_TYPE_WR, T260_FT8203_318, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_319, DSI_PKT_TYPE_WR, T260_FT8203_319, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_320, DSI_PKT_TYPE_WR, T260_FT8203_320, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_321, DSI_PKT_TYPE_WR, T260_FT8203_321, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_322, DSI_PKT_TYPE_WR, T260_FT8203_322, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_323, DSI_PKT_TYPE_WR, T260_FT8203_323, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_324, DSI_PKT_TYPE_WR, T260_FT8203_324, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_325, DSI_PKT_TYPE_WR, T260_FT8203_325, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_326, DSI_PKT_TYPE_WR, T260_FT8203_326, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_327, DSI_PKT_TYPE_WR, T260_FT8203_327, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_328, DSI_PKT_TYPE_WR, T260_FT8203_328, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_329, DSI_PKT_TYPE_WR, T260_FT8203_329, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_330, DSI_PKT_TYPE_WR, T260_FT8203_330, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_331, DSI_PKT_TYPE_WR, T260_FT8203_331, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_332, DSI_PKT_TYPE_WR, T260_FT8203_332, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_333, DSI_PKT_TYPE_WR, T260_FT8203_333, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_334, DSI_PKT_TYPE_WR, T260_FT8203_334, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_335, DSI_PKT_TYPE_WR, T260_FT8203_335, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_336, DSI_PKT_TYPE_WR, T260_FT8203_336, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_337, DSI_PKT_TYPE_WR, T260_FT8203_337, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_338, DSI_PKT_TYPE_WR, T260_FT8203_338, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_339, DSI_PKT_TYPE_WR, T260_FT8203_339, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_340, DSI_PKT_TYPE_WR, T260_FT8203_340, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_341, DSI_PKT_TYPE_WR, T260_FT8203_341, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_342, DSI_PKT_TYPE_WR, T260_FT8203_342, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_343, DSI_PKT_TYPE_WR, T260_FT8203_343, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_344, DSI_PKT_TYPE_WR, T260_FT8203_344, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_345, DSI_PKT_TYPE_WR, T260_FT8203_345, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_346, DSI_PKT_TYPE_WR, T260_FT8203_346, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_347, DSI_PKT_TYPE_WR, T260_FT8203_347, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_348, DSI_PKT_TYPE_WR, T260_FT8203_348, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_349, DSI_PKT_TYPE_WR, T260_FT8203_349, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_350, DSI_PKT_TYPE_WR, T260_FT8203_350, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_351, DSI_PKT_TYPE_WR, T260_FT8203_351, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_352, DSI_PKT_TYPE_WR, T260_FT8203_352, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_353, DSI_PKT_TYPE_WR, T260_FT8203_353, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_354, DSI_PKT_TYPE_WR, T260_FT8203_354, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_355, DSI_PKT_TYPE_WR, T260_FT8203_355, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_356, DSI_PKT_TYPE_WR, T260_FT8203_356, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_357, DSI_PKT_TYPE_WR, T260_FT8203_357, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_358, DSI_PKT_TYPE_WR, T260_FT8203_358, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_359, DSI_PKT_TYPE_WR, T260_FT8203_359, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_360, DSI_PKT_TYPE_WR, T260_FT8203_360, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_361, DSI_PKT_TYPE_WR, T260_FT8203_361, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_362, DSI_PKT_TYPE_WR, T260_FT8203_362, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_363, DSI_PKT_TYPE_WR, T260_FT8203_363, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_364, DSI_PKT_TYPE_WR, T260_FT8203_364, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_365, DSI_PKT_TYPE_WR, T260_FT8203_365, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_366, DSI_PKT_TYPE_WR, T260_FT8203_366, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_367, DSI_PKT_TYPE_WR, T260_FT8203_367, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_368, DSI_PKT_TYPE_WR, T260_FT8203_368, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_369, DSI_PKT_TYPE_WR, T260_FT8203_369, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_370, DSI_PKT_TYPE_WR, T260_FT8203_370, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_371, DSI_PKT_TYPE_WR, T260_FT8203_371, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_372, DSI_PKT_TYPE_WR, T260_FT8203_372, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_373, DSI_PKT_TYPE_WR, T260_FT8203_373, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_374, DSI_PKT_TYPE_WR, T260_FT8203_374, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_375, DSI_PKT_TYPE_WR, T260_FT8203_375, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_376, DSI_PKT_TYPE_WR, T260_FT8203_376, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_377, DSI_PKT_TYPE_WR, T260_FT8203_377, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_378, DSI_PKT_TYPE_WR, T260_FT8203_378, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_379, DSI_PKT_TYPE_WR, T260_FT8203_379, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_380, DSI_PKT_TYPE_WR, T260_FT8203_380, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_381, DSI_PKT_TYPE_WR, T260_FT8203_381, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_382, DSI_PKT_TYPE_WR, T260_FT8203_382, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_383, DSI_PKT_TYPE_WR, T260_FT8203_383, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_384, DSI_PKT_TYPE_WR, T260_FT8203_384, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_385, DSI_PKT_TYPE_WR, T260_FT8203_385, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_386, DSI_PKT_TYPE_WR, T260_FT8203_386, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_387, DSI_PKT_TYPE_WR, T260_FT8203_387, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_388, DSI_PKT_TYPE_WR, T260_FT8203_388, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_389, DSI_PKT_TYPE_WR, T260_FT8203_389, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_390, DSI_PKT_TYPE_WR, T260_FT8203_390, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_391, DSI_PKT_TYPE_WR, T260_FT8203_391, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_392, DSI_PKT_TYPE_WR, T260_FT8203_392, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_393, DSI_PKT_TYPE_WR, T260_FT8203_393, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_394, DSI_PKT_TYPE_WR, T260_FT8203_394, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_395, DSI_PKT_TYPE_WR, T260_FT8203_395, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_396, DSI_PKT_TYPE_WR, T260_FT8203_396, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_397, DSI_PKT_TYPE_WR, T260_FT8203_397, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_398, DSI_PKT_TYPE_WR, T260_FT8203_398, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_399, DSI_PKT_TYPE_WR, T260_FT8203_399, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_400, DSI_PKT_TYPE_WR, T260_FT8203_400, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_401, DSI_PKT_TYPE_WR, T260_FT8203_401, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_402, DSI_PKT_TYPE_WR, T260_FT8203_402, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_403, DSI_PKT_TYPE_WR, T260_FT8203_403, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_404, DSI_PKT_TYPE_WR, T260_FT8203_404, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_405, DSI_PKT_TYPE_WR, T260_FT8203_405, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_406, DSI_PKT_TYPE_WR, T260_FT8203_406, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_407, DSI_PKT_TYPE_WR, T260_FT8203_407, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_408, DSI_PKT_TYPE_WR, T260_FT8203_408, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_409, DSI_PKT_TYPE_WR, T260_FT8203_409, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_410, DSI_PKT_TYPE_WR, T260_FT8203_410, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_411, DSI_PKT_TYPE_WR, T260_FT8203_411, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_412, DSI_PKT_TYPE_WR, T260_FT8203_412, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_413, DSI_PKT_TYPE_WR, T260_FT8203_413, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_414, DSI_PKT_TYPE_WR, T260_FT8203_414, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_415, DSI_PKT_TYPE_WR, T260_FT8203_415, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_416, DSI_PKT_TYPE_WR, T260_FT8203_416, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_417, DSI_PKT_TYPE_WR, T260_FT8203_417, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_418, DSI_PKT_TYPE_WR, T260_FT8203_418, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_419, DSI_PKT_TYPE_WR, T260_FT8203_419, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_420, DSI_PKT_TYPE_WR, T260_FT8203_420, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_421, DSI_PKT_TYPE_WR, T260_FT8203_421, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_422, DSI_PKT_TYPE_WR, T260_FT8203_422, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_423, DSI_PKT_TYPE_WR, T260_FT8203_423, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_424, DSI_PKT_TYPE_WR, T260_FT8203_424, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_425, DSI_PKT_TYPE_WR, T260_FT8203_425, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_426, DSI_PKT_TYPE_WR, T260_FT8203_426, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_427, DSI_PKT_TYPE_WR, T260_FT8203_427, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_428, DSI_PKT_TYPE_WR, T260_FT8203_428, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_429, DSI_PKT_TYPE_WR, T260_FT8203_429, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_430, DSI_PKT_TYPE_WR, T260_FT8203_430, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_431, DSI_PKT_TYPE_WR, T260_FT8203_431, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_432, DSI_PKT_TYPE_WR, T260_FT8203_432, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_433, DSI_PKT_TYPE_WR, T260_FT8203_433, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_434, DSI_PKT_TYPE_WR, T260_FT8203_434, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_435, DSI_PKT_TYPE_WR, T260_FT8203_435, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_436, DSI_PKT_TYPE_WR, T260_FT8203_436, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_437, DSI_PKT_TYPE_WR, T260_FT8203_437, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_438, DSI_PKT_TYPE_WR, T260_FT8203_438, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_439, DSI_PKT_TYPE_WR, T260_FT8203_439, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_440, DSI_PKT_TYPE_WR, T260_FT8203_440, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_441, DSI_PKT_TYPE_WR, T260_FT8203_441, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_442, DSI_PKT_TYPE_WR, T260_FT8203_442, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_443, DSI_PKT_TYPE_WR, T260_FT8203_443, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_444, DSI_PKT_TYPE_WR, T260_FT8203_444, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_445, DSI_PKT_TYPE_WR, T260_FT8203_445, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_446, DSI_PKT_TYPE_WR, T260_FT8203_446, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_447, DSI_PKT_TYPE_WR, T260_FT8203_447, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_448, DSI_PKT_TYPE_WR, T260_FT8203_448, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_449, DSI_PKT_TYPE_WR, T260_FT8203_449, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_450, DSI_PKT_TYPE_WR, T260_FT8203_450, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_451, DSI_PKT_TYPE_WR, T260_FT8203_451, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_452, DSI_PKT_TYPE_WR, T260_FT8203_452, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_453, DSI_PKT_TYPE_WR, T260_FT8203_453, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_454, DSI_PKT_TYPE_WR, T260_FT8203_454, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_455, DSI_PKT_TYPE_WR, T260_FT8203_455, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_456, DSI_PKT_TYPE_WR, T260_FT8203_456, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_457, DSI_PKT_TYPE_WR, T260_FT8203_457, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_458, DSI_PKT_TYPE_WR, T260_FT8203_458, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_459, DSI_PKT_TYPE_WR, T260_FT8203_459, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_460, DSI_PKT_TYPE_WR, T260_FT8203_460, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_461, DSI_PKT_TYPE_WR, T260_FT8203_461, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_462, DSI_PKT_TYPE_WR, T260_FT8203_462, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_463, DSI_PKT_TYPE_WR, T260_FT8203_463, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_464, DSI_PKT_TYPE_WR, T260_FT8203_464, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_465, DSI_PKT_TYPE_WR, T260_FT8203_465, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_466, DSI_PKT_TYPE_WR, T260_FT8203_466, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_467, DSI_PKT_TYPE_WR, T260_FT8203_467, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_468, DSI_PKT_TYPE_WR, T260_FT8203_468, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_469, DSI_PKT_TYPE_WR, T260_FT8203_469, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_470, DSI_PKT_TYPE_WR, T260_FT8203_470, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_471, DSI_PKT_TYPE_WR, T260_FT8203_471, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_472, DSI_PKT_TYPE_WR, T260_FT8203_472, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_473, DSI_PKT_TYPE_WR, T260_FT8203_473, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_474, DSI_PKT_TYPE_WR, T260_FT8203_474, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_475, DSI_PKT_TYPE_WR, T260_FT8203_475, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_476, DSI_PKT_TYPE_WR, T260_FT8203_476, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_477, DSI_PKT_TYPE_WR, T260_FT8203_477, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_478, DSI_PKT_TYPE_WR, T260_FT8203_478, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_479, DSI_PKT_TYPE_WR, T260_FT8203_479, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_480, DSI_PKT_TYPE_WR, T260_FT8203_480, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_481, DSI_PKT_TYPE_WR, T260_FT8203_481, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_482, DSI_PKT_TYPE_WR, T260_FT8203_482, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_483, DSI_PKT_TYPE_WR, T260_FT8203_483, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_484, DSI_PKT_TYPE_WR, T260_FT8203_484, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_485, DSI_PKT_TYPE_WR, T260_FT8203_485, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_486, DSI_PKT_TYPE_WR, T260_FT8203_486, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_487, DSI_PKT_TYPE_WR, T260_FT8203_487, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_488, DSI_PKT_TYPE_WR, T260_FT8203_488, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_489, DSI_PKT_TYPE_WR, T260_FT8203_489, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_490, DSI_PKT_TYPE_WR, T260_FT8203_490, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_491, DSI_PKT_TYPE_WR, T260_FT8203_491, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_492, DSI_PKT_TYPE_WR, T260_FT8203_492, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_493, DSI_PKT_TYPE_WR, T260_FT8203_493, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_494, DSI_PKT_TYPE_WR, T260_FT8203_494, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_495, DSI_PKT_TYPE_WR, T260_FT8203_495, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_496, DSI_PKT_TYPE_WR, T260_FT8203_496, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_497, DSI_PKT_TYPE_WR, T260_FT8203_497, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_498, DSI_PKT_TYPE_WR, T260_FT8203_498, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_499, DSI_PKT_TYPE_WR, T260_FT8203_499, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_500, DSI_PKT_TYPE_WR, T260_FT8203_500, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_501, DSI_PKT_TYPE_WR, T260_FT8203_501, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_502, DSI_PKT_TYPE_WR, T260_FT8203_502, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_503, DSI_PKT_TYPE_WR, T260_FT8203_503, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_504, DSI_PKT_TYPE_WR, T260_FT8203_504, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_505, DSI_PKT_TYPE_WR, T260_FT8203_505, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_506, DSI_PKT_TYPE_WR, T260_FT8203_506, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_507, DSI_PKT_TYPE_WR, T260_FT8203_507, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_508, DSI_PKT_TYPE_WR, T260_FT8203_508, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_509, DSI_PKT_TYPE_WR, T260_FT8203_509, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_510, DSI_PKT_TYPE_WR, T260_FT8203_510, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_511, DSI_PKT_TYPE_WR, T260_FT8203_511, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_512, DSI_PKT_TYPE_WR, T260_FT8203_512, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_513, DSI_PKT_TYPE_WR, T260_FT8203_513, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_514, DSI_PKT_TYPE_WR, T260_FT8203_514, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_515, DSI_PKT_TYPE_WR, T260_FT8203_515, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_516, DSI_PKT_TYPE_WR, T260_FT8203_516, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_517, DSI_PKT_TYPE_WR, T260_FT8203_517, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_518, DSI_PKT_TYPE_WR, T260_FT8203_518, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_519, DSI_PKT_TYPE_WR, T260_FT8203_519, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_520, DSI_PKT_TYPE_WR, T260_FT8203_520, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_521, DSI_PKT_TYPE_WR, T260_FT8203_521, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_522, DSI_PKT_TYPE_WR, T260_FT8203_522, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_523, DSI_PKT_TYPE_WR, T260_FT8203_523, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_524, DSI_PKT_TYPE_WR, T260_FT8203_524, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_525, DSI_PKT_TYPE_WR, T260_FT8203_525, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_526, DSI_PKT_TYPE_WR, T260_FT8203_526, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_527, DSI_PKT_TYPE_WR, T260_FT8203_527, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_528, DSI_PKT_TYPE_WR, T260_FT8203_528, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_529, DSI_PKT_TYPE_WR, T260_FT8203_529, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_530, DSI_PKT_TYPE_WR, T260_FT8203_530, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_531, DSI_PKT_TYPE_WR, T260_FT8203_531, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_532, DSI_PKT_TYPE_WR, T260_FT8203_532, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_533, DSI_PKT_TYPE_WR, T260_FT8203_533, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_534, DSI_PKT_TYPE_WR, T260_FT8203_534, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_535, DSI_PKT_TYPE_WR, T260_FT8203_535, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_536, DSI_PKT_TYPE_WR, T260_FT8203_536, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_537, DSI_PKT_TYPE_WR, T260_FT8203_537, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_538, DSI_PKT_TYPE_WR, T260_FT8203_538, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_539, DSI_PKT_TYPE_WR, T260_FT8203_539, 0);
static DEFINE_STATIC_PACKET(t260_ft8203_boe_540, DSI_PKT_TYPE_WR, T260_FT8203_540, 0);

static DEFINE_PANEL_MDELAY(t260_wait_display_off, 1 * 17); /* 1 frame */
static DEFINE_PANEL_MDELAY(t260_wait_120msec, 120);
static DEFINE_PANEL_MDELAY(t260_wait_5msec, 5);
#if 0
static DEFINE_PANEL_MDELAY(t260_wait_blic_off, 1);
#endif

static void *t260_init_cmdtbl[] = {
	&ft8203_boe_restbl[RES_ID],
	&PKTINFO(t260_ft8203_boe_001),
	&PKTINFO(t260_ft8203_boe_002),
	&PKTINFO(t260_ft8203_boe_003),
	&PKTINFO(t260_ft8203_boe_004),
	&PKTINFO(t260_ft8203_boe_005),
	&PKTINFO(t260_ft8203_boe_006),
	&PKTINFO(t260_ft8203_boe_007),
	&PKTINFO(t260_ft8203_boe_008),
	&PKTINFO(t260_ft8203_boe_009),
	&PKTINFO(t260_ft8203_boe_010),
	&PKTINFO(t260_ft8203_boe_011),
	&PKTINFO(t260_ft8203_boe_012),
	&PKTINFO(t260_ft8203_boe_013),
	&PKTINFO(t260_ft8203_boe_014),
	&PKTINFO(t260_ft8203_boe_015),
	&PKTINFO(t260_ft8203_boe_016),
	&PKTINFO(t260_ft8203_boe_017),
	&PKTINFO(t260_ft8203_boe_018),
	&PKTINFO(t260_ft8203_boe_019),
	&PKTINFO(t260_ft8203_boe_020),
	&PKTINFO(t260_ft8203_boe_021),
	&PKTINFO(t260_ft8203_boe_022),
	&PKTINFO(t260_ft8203_boe_023),
	&PKTINFO(t260_ft8203_boe_024),
	&PKTINFO(t260_ft8203_boe_025),
	&PKTINFO(t260_ft8203_boe_026),
	&PKTINFO(t260_ft8203_boe_027),
	&PKTINFO(t260_ft8203_boe_028),
	&PKTINFO(t260_ft8203_boe_029),
	&PKTINFO(t260_ft8203_boe_030),
	&PKTINFO(t260_ft8203_boe_031),
	&PKTINFO(t260_ft8203_boe_032),
	&PKTINFO(t260_ft8203_boe_033),
	&PKTINFO(t260_ft8203_boe_034),
	&PKTINFO(t260_ft8203_boe_035),
	&PKTINFO(t260_ft8203_boe_036),
	&PKTINFO(t260_ft8203_boe_037),
	&PKTINFO(t260_ft8203_boe_038),
	&PKTINFO(t260_ft8203_boe_039),
	&PKTINFO(t260_ft8203_boe_040),
	&PKTINFO(t260_ft8203_boe_041),
	&PKTINFO(t260_ft8203_boe_042),
	&PKTINFO(t260_ft8203_boe_043),
	&PKTINFO(t260_ft8203_boe_044),
	&PKTINFO(t260_ft8203_boe_045),
	&PKTINFO(t260_ft8203_boe_046),
	&PKTINFO(t260_ft8203_boe_047),
	&PKTINFO(t260_ft8203_boe_048),
	&PKTINFO(t260_ft8203_boe_049),
	&PKTINFO(t260_ft8203_boe_050),
	&PKTINFO(t260_ft8203_boe_051),
	&PKTINFO(t260_ft8203_boe_052),
	&PKTINFO(t260_ft8203_boe_053),
	&PKTINFO(t260_ft8203_boe_054),
	&PKTINFO(t260_ft8203_boe_055),
	&PKTINFO(t260_ft8203_boe_056),
	&PKTINFO(t260_ft8203_boe_057),
	&PKTINFO(t260_ft8203_boe_058),
	&PKTINFO(t260_ft8203_boe_059),
	&PKTINFO(t260_ft8203_boe_060),
	&PKTINFO(t260_ft8203_boe_061),
	&PKTINFO(t260_ft8203_boe_062),
	&PKTINFO(t260_ft8203_boe_063),
	&PKTINFO(t260_ft8203_boe_064),
	&PKTINFO(t260_ft8203_boe_065),
	&PKTINFO(t260_ft8203_boe_066),
	&PKTINFO(t260_ft8203_boe_067),
	&PKTINFO(t260_ft8203_boe_068),
	&PKTINFO(t260_ft8203_boe_069),
	&PKTINFO(t260_ft8203_boe_070),
	&PKTINFO(t260_ft8203_boe_071),
	&PKTINFO(t260_ft8203_boe_072),
	&PKTINFO(t260_ft8203_boe_073),
	&PKTINFO(t260_ft8203_boe_074),
	&PKTINFO(t260_ft8203_boe_075),
	&PKTINFO(t260_ft8203_boe_076),
	&PKTINFO(t260_ft8203_boe_077),
	&PKTINFO(t260_ft8203_boe_078),
	&PKTINFO(t260_ft8203_boe_079),
	&PKTINFO(t260_ft8203_boe_080),
	&PKTINFO(t260_ft8203_boe_081),
	&PKTINFO(t260_ft8203_boe_082),
	&PKTINFO(t260_ft8203_boe_083),
	&PKTINFO(t260_ft8203_boe_084),
	&PKTINFO(t260_ft8203_boe_085),
	&PKTINFO(t260_ft8203_boe_086),
	&PKTINFO(t260_ft8203_boe_087),
	&PKTINFO(t260_ft8203_boe_088),
	&PKTINFO(t260_ft8203_boe_089),
	&PKTINFO(t260_ft8203_boe_090),
	&PKTINFO(t260_ft8203_boe_091),
	&PKTINFO(t260_ft8203_boe_092),
	&PKTINFO(t260_ft8203_boe_093),
	&PKTINFO(t260_ft8203_boe_094),
	&PKTINFO(t260_ft8203_boe_095),
	&PKTINFO(t260_ft8203_boe_096),
	&PKTINFO(t260_ft8203_boe_097),
	&PKTINFO(t260_ft8203_boe_098),
	&PKTINFO(t260_ft8203_boe_099),
	&PKTINFO(t260_ft8203_boe_100),
	&PKTINFO(t260_ft8203_boe_101),
	&PKTINFO(t260_ft8203_boe_102),
	&PKTINFO(t260_ft8203_boe_103),
	&PKTINFO(t260_ft8203_boe_104),
	&PKTINFO(t260_ft8203_boe_105),
	&PKTINFO(t260_ft8203_boe_106),
	&PKTINFO(t260_ft8203_boe_107),
	&PKTINFO(t260_ft8203_boe_108),
	&PKTINFO(t260_ft8203_boe_109),
	&PKTINFO(t260_ft8203_boe_110),
	&PKTINFO(t260_ft8203_boe_111),
	&PKTINFO(t260_ft8203_boe_112),
	&PKTINFO(t260_ft8203_boe_113),
	&PKTINFO(t260_ft8203_boe_114),
	&PKTINFO(t260_ft8203_boe_115),
	&PKTINFO(t260_ft8203_boe_116),
	&PKTINFO(t260_ft8203_boe_117),
	&PKTINFO(t260_ft8203_boe_118),
	&PKTINFO(t260_ft8203_boe_119),
	&PKTINFO(t260_ft8203_boe_120),
	&PKTINFO(t260_ft8203_boe_121),
	&PKTINFO(t260_ft8203_boe_122),
	&PKTINFO(t260_ft8203_boe_123),
	&PKTINFO(t260_ft8203_boe_124),
	&PKTINFO(t260_ft8203_boe_125),
	&PKTINFO(t260_ft8203_boe_126),
	&PKTINFO(t260_ft8203_boe_127),
	&PKTINFO(t260_ft8203_boe_128),
	&PKTINFO(t260_ft8203_boe_129),
	&PKTINFO(t260_ft8203_boe_130),
	&PKTINFO(t260_ft8203_boe_131),
	&PKTINFO(t260_ft8203_boe_132),
	&PKTINFO(t260_ft8203_boe_133),
	&PKTINFO(t260_ft8203_boe_134),
	&PKTINFO(t260_ft8203_boe_135),
	&PKTINFO(t260_ft8203_boe_136),
	&PKTINFO(t260_ft8203_boe_137),
	&PKTINFO(t260_ft8203_boe_138),
	&PKTINFO(t260_ft8203_boe_139),
	&PKTINFO(t260_ft8203_boe_140),
	&PKTINFO(t260_ft8203_boe_141),
	&PKTINFO(t260_ft8203_boe_142),
	&PKTINFO(t260_ft8203_boe_143),
	&PKTINFO(t260_ft8203_boe_144),
	&PKTINFO(t260_ft8203_boe_145),
	&PKTINFO(t260_ft8203_boe_146),
	&PKTINFO(t260_ft8203_boe_147),
	&PKTINFO(t260_ft8203_boe_148),
	&PKTINFO(t260_ft8203_boe_149),
	&PKTINFO(t260_ft8203_boe_150),
	&PKTINFO(t260_ft8203_boe_151),
	&PKTINFO(t260_ft8203_boe_152),
	&PKTINFO(t260_ft8203_boe_153),
	&PKTINFO(t260_ft8203_boe_154),
	&PKTINFO(t260_ft8203_boe_155),
	&PKTINFO(t260_ft8203_boe_156),
	&PKTINFO(t260_ft8203_boe_157),
	&PKTINFO(t260_ft8203_boe_158),
	&PKTINFO(t260_ft8203_boe_159),
	&PKTINFO(t260_ft8203_boe_160),
	&PKTINFO(t260_ft8203_boe_161),
	&PKTINFO(t260_ft8203_boe_162),
	&PKTINFO(t260_ft8203_boe_163),
	&PKTINFO(t260_ft8203_boe_164),
	&PKTINFO(t260_ft8203_boe_165),
	&PKTINFO(t260_ft8203_boe_166),
	&PKTINFO(t260_ft8203_boe_167),
	&PKTINFO(t260_ft8203_boe_168),
	&PKTINFO(t260_ft8203_boe_169),
	&PKTINFO(t260_ft8203_boe_170),
	&PKTINFO(t260_ft8203_boe_171),
	&PKTINFO(t260_ft8203_boe_172),
	&PKTINFO(t260_ft8203_boe_173),
	&PKTINFO(t260_ft8203_boe_174),
	&PKTINFO(t260_ft8203_boe_175),
	&PKTINFO(t260_ft8203_boe_176),
	&PKTINFO(t260_ft8203_boe_177),
	&PKTINFO(t260_ft8203_boe_178),
	&PKTINFO(t260_ft8203_boe_179),
	&PKTINFO(t260_ft8203_boe_180),
	&PKTINFO(t260_ft8203_boe_181),
	&PKTINFO(t260_ft8203_boe_182),
	&PKTINFO(t260_ft8203_boe_183),
	&PKTINFO(t260_ft8203_boe_184),
	&PKTINFO(t260_ft8203_boe_185),
	&PKTINFO(t260_ft8203_boe_186),
	&PKTINFO(t260_ft8203_boe_187),
	&PKTINFO(t260_ft8203_boe_188),
	&PKTINFO(t260_ft8203_boe_189),
	&PKTINFO(t260_ft8203_boe_190),
	&PKTINFO(t260_ft8203_boe_191),
	&PKTINFO(t260_ft8203_boe_192),
	&PKTINFO(t260_ft8203_boe_193),
	&PKTINFO(t260_ft8203_boe_194),
	&PKTINFO(t260_ft8203_boe_195),
	&PKTINFO(t260_ft8203_boe_196),
	&PKTINFO(t260_ft8203_boe_197),
	&PKTINFO(t260_ft8203_boe_198),
	&PKTINFO(t260_ft8203_boe_199),
	&PKTINFO(t260_ft8203_boe_200),
	&PKTINFO(t260_ft8203_boe_201),
	&PKTINFO(t260_ft8203_boe_202),
	&PKTINFO(t260_ft8203_boe_203),
	&PKTINFO(t260_ft8203_boe_204),
	&PKTINFO(t260_ft8203_boe_205),
	&PKTINFO(t260_ft8203_boe_206),
	&PKTINFO(t260_ft8203_boe_207),
	&PKTINFO(t260_ft8203_boe_208),
	&PKTINFO(t260_ft8203_boe_209),
	&PKTINFO(t260_ft8203_boe_210),
	&PKTINFO(t260_ft8203_boe_211),
	&PKTINFO(t260_ft8203_boe_212),
	&PKTINFO(t260_ft8203_boe_213),
	&PKTINFO(t260_ft8203_boe_214),
	&PKTINFO(t260_ft8203_boe_215),
	&PKTINFO(t260_ft8203_boe_216),
	&PKTINFO(t260_ft8203_boe_217),
	&PKTINFO(t260_ft8203_boe_218),
	&PKTINFO(t260_ft8203_boe_219),
	&PKTINFO(t260_ft8203_boe_220),
	&PKTINFO(t260_ft8203_boe_221),
	&PKTINFO(t260_ft8203_boe_222),
	&PKTINFO(t260_ft8203_boe_223),
	&PKTINFO(t260_ft8203_boe_224),
	&PKTINFO(t260_ft8203_boe_225),
	&PKTINFO(t260_ft8203_boe_226),
	&PKTINFO(t260_ft8203_boe_227),
	&PKTINFO(t260_ft8203_boe_228),
	&PKTINFO(t260_ft8203_boe_229),
	&PKTINFO(t260_ft8203_boe_230),
	&PKTINFO(t260_ft8203_boe_231),
	&PKTINFO(t260_ft8203_boe_232),
	&PKTINFO(t260_ft8203_boe_233),
	&PKTINFO(t260_ft8203_boe_234),
	&PKTINFO(t260_ft8203_boe_235),
	&PKTINFO(t260_ft8203_boe_236),
	&PKTINFO(t260_ft8203_boe_237),
	&PKTINFO(t260_ft8203_boe_238),
	&PKTINFO(t260_ft8203_boe_239),
	&PKTINFO(t260_ft8203_boe_240),
	&PKTINFO(t260_ft8203_boe_241),
	&PKTINFO(t260_ft8203_boe_242),
	&PKTINFO(t260_ft8203_boe_243),
	&PKTINFO(t260_ft8203_boe_244),
	&PKTINFO(t260_ft8203_boe_245),
	&PKTINFO(t260_ft8203_boe_246),
	&PKTINFO(t260_ft8203_boe_247),
	&PKTINFO(t260_ft8203_boe_248),
	&PKTINFO(t260_ft8203_boe_249),
	&PKTINFO(t260_ft8203_boe_250),
	&PKTINFO(t260_ft8203_boe_251),
	&PKTINFO(t260_ft8203_boe_252),
	&PKTINFO(t260_ft8203_boe_253),
	&PKTINFO(t260_ft8203_boe_254),
	&PKTINFO(t260_ft8203_boe_255),
	&PKTINFO(t260_ft8203_boe_256),
	&PKTINFO(t260_ft8203_boe_257),
	&PKTINFO(t260_ft8203_boe_258),
	&PKTINFO(t260_ft8203_boe_259),
	&PKTINFO(t260_ft8203_boe_260),
	&PKTINFO(t260_ft8203_boe_261),
	&PKTINFO(t260_ft8203_boe_262),
	&PKTINFO(t260_ft8203_boe_263),
	&PKTINFO(t260_ft8203_boe_264),
	&PKTINFO(t260_ft8203_boe_265),
	&PKTINFO(t260_ft8203_boe_266),
	&PKTINFO(t260_ft8203_boe_267),
	&PKTINFO(t260_ft8203_boe_268),
	&PKTINFO(t260_ft8203_boe_269),
	&PKTINFO(t260_ft8203_boe_270),
	&PKTINFO(t260_ft8203_boe_271),
	&PKTINFO(t260_ft8203_boe_272),
	&PKTINFO(t260_ft8203_boe_273),
	&PKTINFO(t260_ft8203_boe_274),
	&PKTINFO(t260_ft8203_boe_275),
	&PKTINFO(t260_ft8203_boe_276),
	&PKTINFO(t260_ft8203_boe_277),
	&PKTINFO(t260_ft8203_boe_278),
	&PKTINFO(t260_ft8203_boe_279),
	&PKTINFO(t260_ft8203_boe_280),
	&PKTINFO(t260_ft8203_boe_281),
	&PKTINFO(t260_ft8203_boe_282),
	&PKTINFO(t260_ft8203_boe_283),
	&PKTINFO(t260_ft8203_boe_284),
	&PKTINFO(t260_ft8203_boe_285),
	&PKTINFO(t260_ft8203_boe_286),
	&PKTINFO(t260_ft8203_boe_287),
	&PKTINFO(t260_ft8203_boe_288),
	&PKTINFO(t260_ft8203_boe_289),
	&PKTINFO(t260_ft8203_boe_290),
	&PKTINFO(t260_ft8203_boe_291),
	&PKTINFO(t260_ft8203_boe_292),
	&PKTINFO(t260_ft8203_boe_293),
	&PKTINFO(t260_ft8203_boe_294),
	&PKTINFO(t260_ft8203_boe_295),
	&PKTINFO(t260_ft8203_boe_296),
	&PKTINFO(t260_ft8203_boe_297),
	&PKTINFO(t260_ft8203_boe_298),
	&PKTINFO(t260_ft8203_boe_299),
	&PKTINFO(t260_ft8203_boe_300),
	&PKTINFO(t260_ft8203_boe_301),
	&PKTINFO(t260_ft8203_boe_302),
	&PKTINFO(t260_ft8203_boe_303),
	&PKTINFO(t260_ft8203_boe_304),
	&PKTINFO(t260_ft8203_boe_305),
	&PKTINFO(t260_ft8203_boe_306),
	&PKTINFO(t260_ft8203_boe_307),
	&PKTINFO(t260_ft8203_boe_308),
	&PKTINFO(t260_ft8203_boe_309),
	&PKTINFO(t260_ft8203_boe_310),
	&PKTINFO(t260_ft8203_boe_311),
	&PKTINFO(t260_ft8203_boe_312),
	&PKTINFO(t260_ft8203_boe_313),
	&PKTINFO(t260_ft8203_boe_314),
	&PKTINFO(t260_ft8203_boe_315),
	&PKTINFO(t260_ft8203_boe_316),
	&PKTINFO(t260_ft8203_boe_317),
	&PKTINFO(t260_ft8203_boe_318),
	&PKTINFO(t260_ft8203_boe_319),
	&PKTINFO(t260_ft8203_boe_320),
	&PKTINFO(t260_ft8203_boe_321),
	&PKTINFO(t260_ft8203_boe_322),
	&PKTINFO(t260_ft8203_boe_323),
	&PKTINFO(t260_ft8203_boe_324),
	&PKTINFO(t260_ft8203_boe_325),
	&PKTINFO(t260_ft8203_boe_326),
	&PKTINFO(t260_ft8203_boe_327),
	&PKTINFO(t260_ft8203_boe_328),
	&PKTINFO(t260_ft8203_boe_329),
	&PKTINFO(t260_ft8203_boe_330),
	&PKTINFO(t260_ft8203_boe_331),
	&PKTINFO(t260_ft8203_boe_332),
	&PKTINFO(t260_ft8203_boe_333),
	&PKTINFO(t260_ft8203_boe_334),
	&PKTINFO(t260_ft8203_boe_335),
	&PKTINFO(t260_ft8203_boe_336),
	&PKTINFO(t260_ft8203_boe_337),
	&PKTINFO(t260_ft8203_boe_338),
	&PKTINFO(t260_ft8203_boe_339),
	&PKTINFO(t260_ft8203_boe_340),
	&PKTINFO(t260_ft8203_boe_341),
	&PKTINFO(t260_ft8203_boe_342),
	&PKTINFO(t260_ft8203_boe_343),
	&PKTINFO(t260_ft8203_boe_344),
	&PKTINFO(t260_ft8203_boe_345),
	&PKTINFO(t260_ft8203_boe_346),
	&PKTINFO(t260_ft8203_boe_347),
	&PKTINFO(t260_ft8203_boe_348),
	&PKTINFO(t260_ft8203_boe_349),
	&PKTINFO(t260_ft8203_boe_350),
	&PKTINFO(t260_ft8203_boe_351),
	&PKTINFO(t260_ft8203_boe_352),
	&PKTINFO(t260_ft8203_boe_353),
	&PKTINFO(t260_ft8203_boe_354),
	&PKTINFO(t260_ft8203_boe_355),
	&PKTINFO(t260_ft8203_boe_356),
	&PKTINFO(t260_ft8203_boe_357),
	&PKTINFO(t260_ft8203_boe_358),
	&PKTINFO(t260_ft8203_boe_359),
	&PKTINFO(t260_ft8203_boe_360),
	&PKTINFO(t260_ft8203_boe_361),
	&PKTINFO(t260_ft8203_boe_362),
	&PKTINFO(t260_ft8203_boe_363),
	&PKTINFO(t260_ft8203_boe_364),
	&PKTINFO(t260_ft8203_boe_365),
	&PKTINFO(t260_ft8203_boe_366),
	&PKTINFO(t260_ft8203_boe_367),
	&PKTINFO(t260_ft8203_boe_368),
	&PKTINFO(t260_ft8203_boe_369),
	&PKTINFO(t260_ft8203_boe_370),
	&PKTINFO(t260_ft8203_boe_371),
	&PKTINFO(t260_ft8203_boe_372),
	&PKTINFO(t260_ft8203_boe_373),
	&PKTINFO(t260_ft8203_boe_374),
	&PKTINFO(t260_ft8203_boe_375),
	&PKTINFO(t260_ft8203_boe_376),
	&PKTINFO(t260_ft8203_boe_377),
	&PKTINFO(t260_ft8203_boe_378),
	&PKTINFO(t260_ft8203_boe_379),
	&PKTINFO(t260_ft8203_boe_380),
	&PKTINFO(t260_ft8203_boe_381),
	&PKTINFO(t260_ft8203_boe_382),
	&PKTINFO(t260_ft8203_boe_383),
	&PKTINFO(t260_ft8203_boe_384),
	&PKTINFO(t260_ft8203_boe_385),
	&PKTINFO(t260_ft8203_boe_386),
	&PKTINFO(t260_ft8203_boe_387),
	&PKTINFO(t260_ft8203_boe_388),
	&PKTINFO(t260_ft8203_boe_389),
	&PKTINFO(t260_ft8203_boe_390),
	&PKTINFO(t260_ft8203_boe_391),
	&PKTINFO(t260_ft8203_boe_392),
	&PKTINFO(t260_ft8203_boe_393),
	&PKTINFO(t260_ft8203_boe_394),
	&PKTINFO(t260_ft8203_boe_395),
	&PKTINFO(t260_ft8203_boe_396),
	&PKTINFO(t260_ft8203_boe_397),
	&PKTINFO(t260_ft8203_boe_398),
	&PKTINFO(t260_ft8203_boe_399),
	&PKTINFO(t260_ft8203_boe_400),
	&PKTINFO(t260_ft8203_boe_401),
	&PKTINFO(t260_ft8203_boe_402),
	&PKTINFO(t260_ft8203_boe_403),
	&PKTINFO(t260_ft8203_boe_404),
	&PKTINFO(t260_ft8203_boe_405),
	&PKTINFO(t260_ft8203_boe_406),
	&PKTINFO(t260_ft8203_boe_407),
	&PKTINFO(t260_ft8203_boe_408),
	&PKTINFO(t260_ft8203_boe_409),
	&PKTINFO(t260_ft8203_boe_410),
	&PKTINFO(t260_ft8203_boe_411),
	&PKTINFO(t260_ft8203_boe_412),
	&PKTINFO(t260_ft8203_boe_413),
	&PKTINFO(t260_ft8203_boe_414),
	&PKTINFO(t260_ft8203_boe_415),
	&PKTINFO(t260_ft8203_boe_416),
	&PKTINFO(t260_ft8203_boe_417),
	&PKTINFO(t260_ft8203_boe_418),
	&PKTINFO(t260_ft8203_boe_419),
	&PKTINFO(t260_ft8203_boe_420),
	&PKTINFO(t260_ft8203_boe_421),
	&PKTINFO(t260_ft8203_boe_422),
	&PKTINFO(t260_ft8203_boe_423),
	&PKTINFO(t260_ft8203_boe_424),
	&PKTINFO(t260_ft8203_boe_425),
	&PKTINFO(t260_ft8203_boe_426),
	&PKTINFO(t260_ft8203_boe_427),
	&PKTINFO(t260_ft8203_boe_428),
	&PKTINFO(t260_ft8203_boe_429),
	&PKTINFO(t260_ft8203_boe_430),
	&PKTINFO(t260_ft8203_boe_431),
	&PKTINFO(t260_ft8203_boe_432),
	&PKTINFO(t260_ft8203_boe_433),
	&PKTINFO(t260_ft8203_boe_434),
	&PKTINFO(t260_ft8203_boe_435),
	&PKTINFO(t260_ft8203_boe_436),
	&PKTINFO(t260_ft8203_boe_437),
	&PKTINFO(t260_ft8203_boe_438),
	&PKTINFO(t260_ft8203_boe_439),
	&PKTINFO(t260_ft8203_boe_440),
	&PKTINFO(t260_ft8203_boe_441),
	&PKTINFO(t260_ft8203_boe_442),
	&PKTINFO(t260_ft8203_boe_443),
	&PKTINFO(t260_ft8203_boe_444),
	&PKTINFO(t260_ft8203_boe_445),
	&PKTINFO(t260_ft8203_boe_446),
	&PKTINFO(t260_ft8203_boe_447),
	&PKTINFO(t260_ft8203_boe_448),
	&PKTINFO(t260_ft8203_boe_449),
	&PKTINFO(t260_ft8203_boe_450),
	&PKTINFO(t260_ft8203_boe_451),
	&PKTINFO(t260_ft8203_boe_452),
	&PKTINFO(t260_ft8203_boe_453),
	&PKTINFO(t260_ft8203_boe_454),
	&PKTINFO(t260_ft8203_boe_455),
	&PKTINFO(t260_ft8203_boe_456),
	&PKTINFO(t260_ft8203_boe_457),
	&PKTINFO(t260_ft8203_boe_458),
	&PKTINFO(t260_ft8203_boe_459),
	&PKTINFO(t260_ft8203_boe_460),
	&PKTINFO(t260_ft8203_boe_461),
	&PKTINFO(t260_ft8203_boe_462),
	&PKTINFO(t260_ft8203_boe_463),
	&PKTINFO(t260_ft8203_boe_464),
	&PKTINFO(t260_ft8203_boe_465),
	&PKTINFO(t260_ft8203_boe_466),
	&PKTINFO(t260_ft8203_boe_467),
	&PKTINFO(t260_ft8203_boe_468),
	&PKTINFO(t260_ft8203_boe_469),
	&PKTINFO(t260_ft8203_boe_470),
	&PKTINFO(t260_ft8203_boe_471),
	&PKTINFO(t260_ft8203_boe_472),
	&PKTINFO(t260_ft8203_boe_473),
	&PKTINFO(t260_ft8203_boe_474),
	&PKTINFO(t260_ft8203_boe_475),
	&PKTINFO(t260_ft8203_boe_476),
	&PKTINFO(t260_ft8203_boe_477),
	&PKTINFO(t260_ft8203_boe_478),
	&PKTINFO(t260_ft8203_boe_479),
	&PKTINFO(t260_ft8203_boe_480),
	&PKTINFO(t260_ft8203_boe_481),
	&PKTINFO(t260_ft8203_boe_482),
	&PKTINFO(t260_ft8203_boe_483),
	&PKTINFO(t260_ft8203_boe_484),
	&PKTINFO(t260_ft8203_boe_485),
	&PKTINFO(t260_ft8203_boe_486),
	&PKTINFO(t260_ft8203_boe_487),
	&PKTINFO(t260_ft8203_boe_488),
	&PKTINFO(t260_ft8203_boe_489),
	&PKTINFO(t260_ft8203_boe_490),
	&PKTINFO(t260_ft8203_boe_491),
	&PKTINFO(t260_ft8203_boe_492),
	&PKTINFO(t260_ft8203_boe_493),
	&PKTINFO(t260_ft8203_boe_494),
	&PKTINFO(t260_ft8203_boe_495),
	&PKTINFO(t260_ft8203_boe_496),
	&PKTINFO(t260_ft8203_boe_497),
	&PKTINFO(t260_ft8203_boe_498),
	&PKTINFO(t260_ft8203_boe_499),
	&PKTINFO(t260_ft8203_boe_500),
	&PKTINFO(t260_ft8203_boe_501),
	&PKTINFO(t260_ft8203_boe_502),
	&PKTINFO(t260_ft8203_boe_503),
	&PKTINFO(t260_ft8203_boe_504),
	&PKTINFO(t260_ft8203_boe_505),
	&PKTINFO(t260_ft8203_boe_506),
	&PKTINFO(t260_ft8203_boe_507),
	&PKTINFO(t260_ft8203_boe_508),
	&PKTINFO(t260_ft8203_boe_509),
	&PKTINFO(t260_ft8203_boe_510),
	&PKTINFO(t260_ft8203_boe_511),
	&PKTINFO(t260_ft8203_boe_512),
	&PKTINFO(t260_ft8203_boe_513),
	&PKTINFO(t260_ft8203_boe_514),
	&PKTINFO(t260_ft8203_boe_515),
	&PKTINFO(t260_ft8203_boe_516),
	&PKTINFO(t260_ft8203_boe_517),
	&PKTINFO(t260_ft8203_boe_518),
	&PKTINFO(t260_ft8203_boe_519),
	&PKTINFO(t260_ft8203_boe_520),
	&PKTINFO(t260_ft8203_boe_521),
	&PKTINFO(t260_ft8203_boe_522),
	&PKTINFO(t260_ft8203_boe_523),
	&PKTINFO(t260_ft8203_boe_524),
	&PKTINFO(t260_ft8203_boe_525),
	&PKTINFO(t260_ft8203_boe_526),
	&PKTINFO(t260_ft8203_boe_527),
	&PKTINFO(t260_ft8203_boe_528),
	&PKTINFO(t260_ft8203_boe_529),
	&PKTINFO(t260_ft8203_boe_530),
	&PKTINFO(t260_ft8203_boe_531),
	&PKTINFO(t260_ft8203_boe_532),
	&PKTINFO(t260_ft8203_boe_533),
	&PKTINFO(t260_ft8203_boe_534),
	&PKTINFO(t260_ft8203_boe_535),
	&PKTINFO(t260_ft8203_boe_536),
	&PKTINFO(t260_ft8203_boe_537),
	&PKTINFO(t260_ft8203_boe_538),
	&PKTINFO(t260_ft8203_boe_539),
	&PKTINFO(t260_ft8203_boe_540),
	&PKTINFO(t260_sleep_out),
	&DLYINFO(t260_wait_120msec),
};

static void *t260_res_init_cmdtbl[] = {
	&ft8203_boe_restbl[RES_ID],
};

static void *t260_set_bl_cmdtbl[] = {
	&PKTINFO(t260_brightness), //51h
};

static void *t260_display_on_cmdtbl[] = {
	&PKTINFO(t260_display_on),
	&PKTINFO(t260_te_off),
	&PKTINFO(t260_brightness_mode),
};

static void *t260_display_off_cmdtbl[] = {
	&PKTINFO(t260_display_off),
};

static void *t260_exit_cmdtbl[] = {
	&PKTINFO(t260_sleep_in),
};
#if 1
/* ===================================================================================== */
/* ================================= [EA8076 I2C TABLE] ===========+++================= */
/* ===================================================================================== */
static u8 FT8203_T260_I2C_INIT[] = {
	0x04, 0x00,
	0x05, 0x27,
	0x06, 0x08,
	0x08, 0x08,
	0x09, 0x08,
	0x0D, 0xB4,
};

static u8 FT8203_T260_I2C_EXIT_VSN[] = {
	//0x09, 0xBC,
	0x05, 0x20,
};

static u8 FT8203_T260_I2C_EXIT_VSP[] = {
	0x05, 0x20,
};

static DEFINE_STATIC_PACKET(ft8203_boe_t260_i2c_init, I2C_PKT_TYPE_WR, FT8203_T260_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(ft8203_boe_t260_i2c_exit_vsn, I2C_PKT_TYPE_WR, FT8203_T260_I2C_EXIT_VSN, 0);
static DEFINE_STATIC_PACKET(ft8203_boe_t260_i2c_exit_vsp, I2C_PKT_TYPE_WR, FT8203_T260_I2C_EXIT_VSP, 0);
static DEFINE_STATIC_PACKET(ft8203_boe_t260_i2c_dump, I2C_PKT_TYPE_RD, FT8203_T260_I2C_INIT, 0);

static void *ft8203_boe_t260_init_cmdtbl[] = {
	&PKTINFO(ft8203_boe_t260_i2c_init),
};

static void *ft8203_boe_t260_exit_cmdtbl[] = {
	&PKTINFO(ft8203_boe_t260_i2c_exit_vsn),
};

static void *ft8203_boe_t260_dump_cmdtbl[] = {
	&PKTINFO(ft8203_boe_t260_i2c_dump),
};
#endif

static struct seqinfo t260_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", t260_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", t260_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", t260_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", t260_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", t260_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", t260_exit_cmdtbl),

#if 0
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", ft8203_boe_t260_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", ft8203_boe_t260_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", ft8203_boe_t260_dump_cmdtbl),
#endif
#endif

};

struct common_panel_info ft8203_boe_t260_default_panel_info = {
	.ldi_name = "ft8203_boe",
	.name = "ft8203_boe_t260_default",
	.model = "BOE_7_45_inch",
	.vendor = "BOE",
	.id = 0x856270,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
		//.delay_cmd_pre = 0x00,
		//.delay_duration_pre = 1,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(ft8203_boe_t260_resol),
		.resol = ft8203_boe_t260_resol,
	},
	.maptbl = t260_maptbl,
	.nr_maptbl = ARRAY_SIZE(t260_maptbl),
	.seqtbl = t260_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(t260_seqtbl),
	.rditbl = ft8203_boe_rditbl,
	.nr_rditbl = ARRAY_SIZE(ft8203_boe_rditbl),
	.restbl = ft8203_boe_restbl,
	.nr_restbl = ARRAY_SIZE(ft8203_boe_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&ft8203_boe_t260_panel_dimming_info,
	},
	.i2c_data = &ft8203_boe_t260_i2c_data,
};

#define __XX_DBV2REG_BIT_SHIFT(__XX)	\
__XX(51H_PARA1,	__XX_FROM(11:4),	__XX_TO(7:0)	)	\
__XX(51H_PARA2,	__XX_FROM(3:0),	__XX_TO(3:0)	)	\

__XX_DBV2REG_INIT(__XX_DBV2REG_BIT_SHIFT);
#undef __XX_DBV2REG_BIT_SHIFT

static int __init ft8203_boe_t260_panel_init(void)
{
	__XX_DBV2REG_BRIGHTNESS_INIT(ft8203_boe_t260_default_panel_info, __XX_DBV2REG_BIT_SHIFT_INIT, NULL);

	register_common_panel(&ft8203_boe_t260_default_panel_info);

	return 0;
}
arch_initcall(ft8203_boe_t260_panel_init)
#endif /* __FT8203_T260_PANEL_H__ */


