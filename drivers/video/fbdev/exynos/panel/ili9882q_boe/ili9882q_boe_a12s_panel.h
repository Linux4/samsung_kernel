/*
 * linux/drivers/video/fbdev/exynos/panel/ili9882q_boe/ili9882q_boe_a12s_panel.h
 *
 * Header file for ILI9882Q Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ILI9882Q_A12S_PANEL_H__
#define __ILI9882Q_A12S_PANEL_H__
#include "../panel_drv.h"
#include "ili9882q_boe.h"

#include "ili9882q_boe_a12s_panel_dimming.h"
#include "ili9882q_boe_a12s_panel_i2c.h"

#include "ili9882q_boe_a12s_resol.h"

#undef __pn_name__
#define __pn_name__	a12s

#undef __PN_NAME__
#define __PN_NAME__	A12S

static struct seqinfo a12s_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [ILI9882Q READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [ILI9882Q RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [ILI9882Q MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a12s_brt_table[ILI9882Q_TOTAL_NR_LUMINANCE][2] = {
	{0x00, 0x00},
	{0x00, 0x10},
	{0x00, 0x10},
	{0x00, 0x20},
	{0x00, 0x30},
	{0x00, 0x30},
	{0x00, 0x40},
	{0x00, 0x50},
	{0x00, 0x50},
	{0x00, 0x60},
	{0x00, 0x70},
	{0x00, 0x70},
	{0x00, 0x80},
	{0x00, 0x90},
	{0x00, 0x90},
	{0x00, 0xA0},
	{0x00, 0xB0},
	{0x00, 0xB0},
	{0x00, 0xC0},
	{0x00, 0xD0},
	{0x00, 0xE0},
	{0x00, 0xE0},
	{0x00, 0xF0},
	{0x01, 0x00},
	{0x01, 0x00},
	{0x01, 0x10},
	{0x01, 0x20},
	{0x01, 0x20},
	{0x01, 0x30},
	{0x01, 0x40},
	{0x01, 0x40},
	{0x01, 0x50},
	{0x01, 0x60},
	{0x01, 0x60},
	{0x01, 0x70},
	{0x01, 0x80},
	{0x01, 0x80},
	{0x01, 0x90},
	{0x01, 0xA0},
	{0x01, 0xB0},
	{0x01, 0xB0},
	{0x01, 0xC0},
	{0x01, 0xD0},
	{0x01, 0xD0},
	{0x01, 0xE0},
	{0x01, 0xF0},
	{0x01, 0xF0},
	{0x02, 0x00},
	{0x02, 0x10},
	{0x02, 0x10},
	{0x02, 0x20},
	{0x02, 0x30},
	{0x02, 0x30},
	{0x02, 0x40},
	{0x02, 0x50},
	{0x02, 0x50},
	{0x02, 0x60},
	{0x02, 0x70},
	{0x02, 0x80},
	{0x02, 0x80},
	{0x02, 0x90},
	{0x02, 0xA0},
	{0x02, 0xA0},
	{0x02, 0xB0},
	{0x02, 0xC0},
	{0x02, 0xC0},
	{0x02, 0xD0},
	{0x02, 0xE0},
	{0x02, 0xE0},
	{0x02, 0xF0},
	{0x03, 0x00},
	{0x03, 0x00},
	{0x03, 0x10},
	{0x03, 0x20},
	{0x03, 0x30},
	{0x03, 0x30},
	{0x03, 0x40},
	{0x03, 0x50},
	{0x03, 0x50},
	{0x03, 0x60},
	{0x03, 0x70},
	{0x03, 0x70},
	{0x03, 0x80},
	{0x03, 0x90},
	{0x03, 0x90},
	{0x03, 0xA0},
	{0x03, 0xB0},
	{0x03, 0xB0},
	{0x03, 0xC0},
	{0x03, 0xD0},
	{0x03, 0xD0},
	{0x03, 0xE0},
	{0x03, 0xF0},
	{0x04, 0x00},
	{0x04, 0x00},
	{0x04, 0x10},
	{0x04, 0x20},
	{0x04, 0x20},
	{0x04, 0x30},
	{0x04, 0x40},
	{0x04, 0x40},
	{0x04, 0x50},
	{0x04, 0x60},
	{0x04, 0x60},
	{0x04, 0x70},
	{0x04, 0x80},
	{0x04, 0x80},
	{0x04, 0x90},
	{0x04, 0xA0},
	{0x04, 0xA0},
	{0x04, 0xB0},
	{0x04, 0xC0},
	{0x04, 0xD0},
	{0x04, 0xD0},
	{0x04, 0xE0},
	{0x04, 0xF0},
	{0x04, 0xF0},
	{0x05, 0x00},
	{0x05, 0x10},
	{0x05, 0x10},
	{0x05, 0x20},
	{0x05, 0x30},
	{0x05, 0x30},
	{0x05, 0x40},
	{0x05, 0x50},
	{0x05, 0x50},
	{0x05, 0x60},
	{0x05, 0x70},
	{0x05, 0x80},
	{0x05, 0x80},
	{0x05, 0x90},
	{0x05, 0xA0},
	{0x05, 0xB0},
	{0x05, 0xC0},
	{0x05, 0xD0},
	{0x05, 0xE0},
	{0x05, 0xF0},
	{0x06, 0x00},
	{0x06, 0x10},
	{0x06, 0x20},
	{0x06, 0x30},
	{0x06, 0x40},
	{0x06, 0x50},
	{0x06, 0x60},
	{0x06, 0x70},
	{0x06, 0x80},
	{0x06, 0x90},
	{0x06, 0xA0},
	{0x06, 0xB0},
	{0x06, 0xC0},
	{0x06, 0xD0},
	{0x06, 0xE0},
	{0x06, 0xF0},
	{0x07, 0x00},
	{0x07, 0x10},
	{0x07, 0x20},
	{0x07, 0x30},
	{0x07, 0x40},
	{0x07, 0x50},
	{0x07, 0x60},
	{0x07, 0x60},
	{0x07, 0x70},
	{0x07, 0x80},
	{0x07, 0x90},
	{0x07, 0xA0},
	{0x07, 0xB0},
	{0x07, 0xC0},
	{0x07, 0xD0},
	{0x07, 0xE0},
	{0x07, 0xF0},
	{0x08, 0x00},
	{0x08, 0x10},
	{0x08, 0x20},
	{0x08, 0x30},
	{0x08, 0x40},
	{0x08, 0x50},
	{0x08, 0x60},
	{0x08, 0x70},
	{0x08, 0x80},
	{0x08, 0x90},
	{0x08, 0xA0},
	{0x08, 0xB0},
	{0x08, 0xC0},
	{0x08, 0xD0},
	{0x08, 0xE0},
	{0x08, 0xF0},
	{0x09, 0x00},
	{0x09, 0x10},
	{0x09, 0x20},
	{0x09, 0x30},
	{0x09, 0x40},
	{0x09, 0x50},
	{0x09, 0x50},
	{0x09, 0x60},
	{0x09, 0x70},
	{0x09, 0x80},
	{0x09, 0x90},
	{0x09, 0xA0},
	{0x09, 0xB0},
	{0x09, 0xC0},
	{0x09, 0xD0},
	{0x09, 0xE0},
	{0x09, 0xF0},
	{0x0A, 0x00},
	{0x0A, 0x10},
	{0x0A, 0x20},
	{0x0A, 0x30},
	{0x0A, 0x40},
	{0x0A, 0x50},
	{0x0A, 0x60},
	{0x0A, 0x70},
	{0x0A, 0x80},
	{0x0A, 0x90},
	{0x0A, 0xA0},
	{0x0A, 0xB0},
	{0x0A, 0xC0},
	{0x0A, 0xD0},
	{0x0A, 0xE0},
	{0x0A, 0xF0},
	{0x0B, 0x00},
	{0x0B, 0x10},
	{0x0B, 0x20},
	{0x0B, 0x30},
	{0x0B, 0x40},
	{0x0B, 0x40},
	{0x0B, 0x50},
	{0x0B, 0x60},
	{0x0B, 0x70},
	{0x0B, 0x80},
	{0x0B, 0x90},
	{0x0B, 0xA0},
	{0x0B, 0xB0},
	{0x0B, 0xC0},
	{0x0B, 0xD0},
	{0x0B, 0xE0},
	{0x0B, 0xF0},
	{0x0C, 0x00},
	{0x0C, 0x10},
	{0x0C, 0x20},
	{0x0C, 0x30},
	{0x0C, 0x40},
	{0x0C, 0x50},
	{0x0C, 0x60},
	{0x0C, 0x70},
	{0x0C, 0x80},
	{0x0C, 0x90},
	{0x0C, 0xA0},
	{0x0C, 0xB0},
	{0x0C, 0xC0},
	{0x0C, 0xD0},
	{0x0C, 0xE0},
	{0x0C, 0xF0},
	{0x0D, 0x00},
	{0x0D, 0x10},
	{0x0D, 0x20},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0D, 0x30},
	{0x0F, 0xE0},
};


static struct maptbl a12s_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(a12s_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [ILI9882Q COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A12S_SLEEP_OUT[] = { 0x11 };
static u8 A12S_SLEEP_IN[] = { 0x10 };
static u8 A12S_DISPLAY_OFF[] = { 0x28 };
static u8 A12S_DISPLAY_ON[] = { 0x29 };

static u8 A12S_BRIGHTNESS[] = {
	0x51,
	0x0F, 0xFF
};

static u8 A12S_BRIGHTNESS_MODE[] = {
	0x53,
	0x2C,
};

static u8 A12S_ILI9882Q_001[] = {
	0xFF,
	0x98, 0x82, 0x01,
};

static u8 A12S_ILI9882Q_002[] = {
	0x00,
	0x4A,
};

static u8 A12S_ILI9882Q_003[] = {
	0x01,
	0x33,
};

static u8 A12S_ILI9882Q_004[] = {
	0x02,
	0x39,
};

static u8 A12S_ILI9882Q_005[] = {
	0x03,
	0x00,
};

static u8 A12S_ILI9882Q_006[] = {
	0x08,
	0x86,
};

static u8 A12S_ILI9882Q_007[] = {
	0x09,
	0x01,
};

static u8 A12S_ILI9882Q_008[] = {
	0x0A,
	0x73,
};

static u8 A12S_ILI9882Q_009[] = {
	0x0C,
	0x39,
};

static u8 A12S_ILI9882Q_010[] = {
	0x0D,
	0x39,
};

static u8 A12S_ILI9882Q_011[] = {
	0x0E,
	0x00,
};

static u8 A12S_ILI9882Q_012[] = {
	0x0F,
	0x00,
};

static u8 A12S_ILI9882Q_013[] = {
	0x0B,
	0x00,
};

static u8 A12S_ILI9882Q_014[] = {
	0x28,
	0x4B,
};

static u8 A12S_ILI9882Q_015[] = {
	0x2A,
	0x4B,
};

static u8 A12S_ILI9882Q_016[] = {
	0x29,
	0x85,
};

static u8 A12S_ILI9882Q_017[] = {
	0x2B,
	0x85,
};

static u8 A12S_ILI9882Q_018[] = {
	0xEE,
	0x10,
};

static u8 A12S_ILI9882Q_019[] = {
	0xEF,
	0x00,
};

static u8 A12S_ILI9882Q_020[] = {
	0xF0,
	0x00,
};

static u8 A12S_ILI9882Q_021[] = {
	0x31,
	0x07,
};

static u8 A12S_ILI9882Q_022[] = {
	0x32,
	0x02,
};

static u8 A12S_ILI9882Q_023[] = {
	0x33,
	0x23,
};

static u8 A12S_ILI9882Q_024[] = {
	0x34,
	0x02,
};

static u8 A12S_ILI9882Q_025[] = {
	0x35,
	0x09,
};

static u8 A12S_ILI9882Q_026[] = {
	0x36,
	0x0B,
};

static u8 A12S_ILI9882Q_027[] = {
	0x37,
	0x22,
};

static u8 A12S_ILI9882Q_028[] = {
	0x38,
	0x22,
};

static u8 A12S_ILI9882Q_029[] = {
	0x39,
	0x15,
};

static u8 A12S_ILI9882Q_030[] = {
	0x3A,
	0x17,
};

static u8 A12S_ILI9882Q_031[] = {
	0x3B,
	0x11,
};

static u8 A12S_ILI9882Q_032[] = {
	0x3C,
	0x13,
};

static u8 A12S_ILI9882Q_033[] = {
	0x3D,
	0x07,
};

static u8 A12S_ILI9882Q_034[] = {
	0x3E,
	0x07,
};

static u8 A12S_ILI9882Q_035[] = {
	0x3F,
	0x07,
};

static u8 A12S_ILI9882Q_036[] = {
	0x40,
	0x07,
};

static u8 A12S_ILI9882Q_037[] = {
	0x41,
	0x07,
};

static u8 A12S_ILI9882Q_038[] = {
	0x42,
	0x07,
};

static u8 A12S_ILI9882Q_039[] = {
	0x43,
	0x07,
};

static u8 A12S_ILI9882Q_040[] = {
	0x44,
	0x07,
};

static u8 A12S_ILI9882Q_041[] = {
	0x45,
	0x07,
};

static u8 A12S_ILI9882Q_042[] = {
	0x46,
	0x07,
};

static u8 A12S_ILI9882Q_043[] = {
	0x47,
	0x07,
};

static u8 A12S_ILI9882Q_044[] = {
	0x48,
	0x02,
};

static u8 A12S_ILI9882Q_045[] = {
	0x49,
	0x23,
};

static u8 A12S_ILI9882Q_046[] = {
	0x4A,
	0x02,
};

static u8 A12S_ILI9882Q_047[] = {
	0x4B,
	0x08,
};

static u8 A12S_ILI9882Q_048[] = {
	0x4C,
	0x0A,
};

static u8 A12S_ILI9882Q_049[] = {
	0x4D,
	0x22,
};

static u8 A12S_ILI9882Q_050[] = {
	0x4E,
	0x22,
};

static u8 A12S_ILI9882Q_051[] = {
	0x4F,
	0x14,
};

static u8 A12S_ILI9882Q_052[] = {
	0x50,
	0x16,
};

static u8 A12S_ILI9882Q_053[] = {
	0x51,
	0x10,
};

static u8 A12S_ILI9882Q_054[] = {
	0x52,
	0x12,
};

static u8 A12S_ILI9882Q_055[] = {
	0x53,
	0x07,
};

static u8 A12S_ILI9882Q_056[] = {
	0x54,
	0x07,
};

static u8 A12S_ILI9882Q_057[] = {
	0x55,
	0x07,
};

static u8 A12S_ILI9882Q_058[] = {
	0x56,
	0x07,
};

static u8 A12S_ILI9882Q_059[] = {
	0x57,
	0x07,
};

static u8 A12S_ILI9882Q_060[] = {
	0x58,
	0x07,
};

static u8 A12S_ILI9882Q_061[] = {
	0x59,
	0x07,
};

static u8 A12S_ILI9882Q_062[] = {
	0x5A,
	0x07,
};

static u8 A12S_ILI9882Q_063[] = {
	0x5B,
	0x07,
};

static u8 A12S_ILI9882Q_064[] = {
	0x5C,
	0x07,
};

static u8 A12S_ILI9882Q_065[] = {
	0x61,
	0x07,
};

static u8 A12S_ILI9882Q_066[] = {
	0x62,
	0x22,
};

static u8 A12S_ILI9882Q_067[] = {
	0x63,
	0x23,
};

static u8 A12S_ILI9882Q_068[] = {
	0x64,
	0x02,
};

static u8 A12S_ILI9882Q_069[] = {
	0x65,
	0x0A,
};

static u8 A12S_ILI9882Q_070[] = {
	0x66,
	0x08,
};

static u8 A12S_ILI9882Q_071[] = {
	0x67,
	0x02,
};

static u8 A12S_ILI9882Q_072[] = {
	0x68,
	0x22,
};

static u8 A12S_ILI9882Q_073[] = {
	0x69,
	0x12,
};

static u8 A12S_ILI9882Q_074[] = {
	0x6A,
	0x10,
};

static u8 A12S_ILI9882Q_075[] = {
	0x6B,
	0x16,
};

static u8 A12S_ILI9882Q_076[] = {
	0x6C,
	0x14,
};

static u8 A12S_ILI9882Q_077[] = {
	0x6D,
	0x07,
};

static u8 A12S_ILI9882Q_078[] = {
	0x6E,
	0x07,
};

static u8 A12S_ILI9882Q_079[] = {
	0x6F,
	0x07,
};

static u8 A12S_ILI9882Q_080[] = {
	0x70,
	0x07,
};

static u8 A12S_ILI9882Q_081[] = {
	0x71,
	0x07,
};

static u8 A12S_ILI9882Q_082[] = {
	0x72,
	0x07,
};

static u8 A12S_ILI9882Q_083[] = {
	0x73,
	0x07,
};

static u8 A12S_ILI9882Q_084[] = {
	0x74,
	0x07,
};

static u8 A12S_ILI9882Q_085[] = {
	0x75,
	0x07,
};

static u8 A12S_ILI9882Q_086[] = {
	0x76,
	0x07,
};

static u8 A12S_ILI9882Q_087[] = {
	0x77,
	0x07,
};

static u8 A12S_ILI9882Q_088[] = {
	0x78,
	0x22,
};

static u8 A12S_ILI9882Q_089[] = {
	0x79,
	0x23,
};

static u8 A12S_ILI9882Q_090[] = {
	0x7A,
	0x02,
};

static u8 A12S_ILI9882Q_091[] = {
	0x7B,
	0x0B,
};

static u8 A12S_ILI9882Q_092[] = {
	0x7C,
	0x09,
};

static u8 A12S_ILI9882Q_093[] = {
	0x7D,
	0x02,
};

static u8 A12S_ILI9882Q_094[] = {
	0x7E,
	0x22,
};

static u8 A12S_ILI9882Q_095[] = {
	0x7F,
	0x13,
};

static u8 A12S_ILI9882Q_096[] = {
	0x80,
	0x11,
};

static u8 A12S_ILI9882Q_097[] = {
	0x81,
	0x17,
};

static u8 A12S_ILI9882Q_098[] = {
	0x82,
	0x15,
};

static u8 A12S_ILI9882Q_099[] = {
	0x83,
	0x07,
};

static u8 A12S_ILI9882Q_100[] = {
	0x84,
	0x07,
};

static u8 A12S_ILI9882Q_101[] = {
	0x85,
	0x07,
};

static u8 A12S_ILI9882Q_102[] = {
	0x86,
	0x07,
};

static u8 A12S_ILI9882Q_103[] = {
	0x87,
	0x07,
};

static u8 A12S_ILI9882Q_104[] = {
	0x88,
	0x07,
};

static u8 A12S_ILI9882Q_105[] = {
	0x89,
	0x07,
};

static u8 A12S_ILI9882Q_106[] = {
	0x8A,
	0x07,
};

static u8 A12S_ILI9882Q_107[] = {
	0x8B,
	0x07,
};

static u8 A12S_ILI9882Q_108[] = {
	0x8C,
	0x07,
};

static u8 A12S_ILI9882Q_109[] = {
	0xB9,
	0x10,
};

static u8 A12S_ILI9882Q_110[] = {
	0xD0,
	0x01,
};

static u8 A12S_ILI9882Q_111[] = {
	0xD1,
	0x00,
};

static u8 A12S_ILI9882Q_112[] = {
	0xE2,
	0x00,
};

static u8 A12S_ILI9882Q_113[] = {
	0xE6,
	0x22,
};

static u8 A12S_ILI9882Q_114[] = {
	0xE7,
	0x54,
};

static u8 A12S_ILI9882Q_115[] = {
	0xB0,
	0x33,
};

static u8 A12S_ILI9882Q_116[] = {
	0xB1,
	0x33,
};

static u8 A12S_ILI9882Q_117[] = {
	0xB2,
	0x00,
};

static u8 A12S_ILI9882Q_118[] = {
	0xE7,
	0x54,
};

static u8 A12S_ILI9882Q_119[] = {
	0xFF,
	0x98, 0x82, 0x02,
};

static u8 A12S_ILI9882Q_120[] = {
	0xF1,
	0x1C,
};

static u8 A12S_ILI9882Q_121[] = {
	0x4B,
	0x5A,
};

static u8 A12S_ILI9882Q_122[] = {
	0x50,
	0xCA,
};

static u8 A12S_ILI9882Q_123[] = {
	0x51,
	0x00,
};

static u8 A12S_ILI9882Q_124[] = {
	0x06,
	0x8D,
};

static u8 A12S_ILI9882Q_125[] = {
	0x0B,
	0xA0,
};

static u8 A12S_ILI9882Q_126[] = {
	0x0C,
	0x00,
};

static u8 A12S_ILI9882Q_127[] = {
	0x0D,
	0x14,
};

static u8 A12S_ILI9882Q_128[] = {
	0x0E,
	0xFA,
};

static u8 A12S_ILI9882Q_129[] = {
	0x4E,
	0x11,
};

static u8 A12S_ILI9882Q_130[] = {
	0x40,
	0x4E,
};

static u8 A12S_ILI9882Q_131[] = {
	0x4D,
	0xCE,
};

static u8 A12S_ILI9882Q_132[] = {
	0xF2,
	0x4A,
};

static u8 A12S_ILI9882Q_133[] = {
	0xFF,
	0x98, 0x82, 0x03,
};

static u8 A12S_ILI9882Q_134[] = {
	0x80,
	0x36,
};

static u8 A12S_ILI9882Q_135[] = {
	0x83,
	0x60,
};

static u8 A12S_ILI9882Q_136[] = {
	0x84,
	0x00,
};

static u8 A12S_ILI9882Q_137[] = {
	0x88,
	0xD9,
};

static u8 A12S_ILI9882Q_138[] = {
	0x89,
	0xE1,
};

static u8 A12S_ILI9882Q_139[] = {
	0x8A,
	0xE8,
};

static u8 A12S_ILI9882Q_140[] = {
	0x8B,
	0xF0,
};

static u8 A12S_ILI9882Q_141[] = {
	0xAF,
	0x18,
};

static u8 A12S_ILI9882Q_142[] = {
	0xC6,
	0x14,
};

static u8 A12S_ILI9882Q_143[] = {
	0xFF,
	0x98, 0x82, 0x05,
};

static u8 A12S_ILI9882Q_144[] = {
	0x03,
	0x01,
};

static u8 A12S_ILI9882Q_145[] = {
	0x04,
	0x4E,
};

static u8 A12S_ILI9882Q_146[] = {
	0x63,
	0x9C,
};

static u8 A12S_ILI9882Q_147[] = {
	0x64,
	0x92,
};

static u8 A12S_ILI9882Q_148[] = {
	0x68,
	0x8D,
};

static u8 A12S_ILI9882Q_149[] = {
	0x69,
	0x93,
};

static u8 A12S_ILI9882Q_150[] = {
	0x6A,
	0xC9,
};

static u8 A12S_ILI9882Q_151[] = {
	0x6B,
	0xBB,
};

static u8 A12S_ILI9882Q_152[] = {
	0x85,
	0x37,
};

static u8 A12S_ILI9882Q_153[] = {
	0xFF,
	0x98, 0x82, 0x06,
};

static u8 A12S_ILI9882Q_154[] = {
	0xD9,
	0x10,
};

static u8 A12S_ILI9882Q_155[] = {
	0xC0,
	0x40,
};

static u8 A12S_ILI9882Q_156[] = {
	0xC1,
	0x16,
};

static u8 A12S_ILI9882Q_157[] = {
	0x80,
	0x08,
};

static u8 A12S_ILI9882Q_158[] = {
	0xD6,
	0x00,
};

static u8 A12S_ILI9882Q_159[] = {
	0xD7,
	0x11,
};

static u8 A12S_ILI9882Q_160[] = {
	0x48,
	0x35,
};

static u8 A12S_ILI9882Q_161[] = {
	0xD5,
	0x20,
};

static u8 A12S_ILI9882Q_162[] = {
	0x08,
	0x21,
};

static u8 A12S_ILI9882Q_163[] = {
	0x78,
	0x12,
};

static u8 A12S_ILI9882Q_164[] = {
	0x69,
	0xDF,
};

static u8 A12S_ILI9882Q_165[] = {
	0xFF,
	0x98, 0x82, 0x08,
};

static u8 A12S_ILI9882Q_166[] = {
	0xE0,
	0x00, 0x00, 0x3E, 0x6D, 0xA9, 0x54, 0xDD, 0x07, 0x39, 0x62,
	0xA5, 0xA4, 0xDA, 0x0A, 0x38, 0xAA, 0x67, 0x9F, 0xC1, 0xEC,
	0xFF, 0x0F, 0x3D, 0x75, 0xA3, 0x03, 0xEC,
};

static u8 A12S_ILI9882Q_167[] = {
	0xE1,
	0x00, 0x00, 0x3E, 0x6D, 0xA9, 0x54, 0xDD, 0x07, 0x39, 0x62,
	0xA5, 0xA4, 0xDA, 0x0A, 0x38, 0xAA, 0x67, 0x9F, 0xC1, 0xEC,
	0xFF, 0x0F, 0x3D, 0x75, 0xA3, 0x03, 0xEC,
};

static u8 A12S_ILI9882Q_168[] = {
	0xFF,
	0x98, 0x82, 0x0B,
};

static u8 A12S_ILI9882Q_169[] = {
	0x9A,
	0x44,
};

static u8 A12S_ILI9882Q_170[] = {
	0x9B,
	0x73,
};

static u8 A12S_ILI9882Q_171[] = {
	0x9C,
	0x03,
};

static u8 A12S_ILI9882Q_172[] = {
	0x9D,
	0x03,
};

static u8 A12S_ILI9882Q_173[] = {
	0x9E,
	0x6F,
};

static u8 A12S_ILI9882Q_174[] = {
	0x9F,
	0x6F,
};

static u8 A12S_ILI9882Q_175[] = {
	0xAB,
	0xE0,
};

static u8 A12S_ILI9882Q_176[] = {
	0xAC,
	0x7F,
};

static u8 A12S_ILI9882Q_177[] = {
	0xAD,
	0x3F,
};

static u8 A12S_ILI9882Q_178[] = {
	0xFF,
	0x98, 0x82, 0x0E,
};

static u8 A12S_ILI9882Q_179[] = {
	0x11,
	0x10,
};

static u8 A12S_ILI9882Q_180[] = {
	0x13,
	0x10,
};

static u8 A12S_ILI9882Q_181[] = {
	0x00,
	0xA0,
};

static u8 A12S_ILI9882Q_182[] = {
	0xFF,
	0x98, 0x82, 0x00,
};

static u8 A12S_ILI9882Q_183[] = {
	0x51,
	0x0F, 0xFF,
};

static u8 A12S_ILI9882Q_184[] = {
	0x53,
	0x2C,
};

static u8 A12S_ILI9882Q_185[] = {
	0x55,
	0x01,
};

static u8 A12S_ILI9882Q_186[] = {
	0x35,
	0x00,
};


static DEFINE_STATIC_PACKET(a12s_sleep_out, DSI_PKT_TYPE_WR, A12S_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a12s_sleep_in, DSI_PKT_TYPE_WR, A12S_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a12s_display_on, DSI_PKT_TYPE_WR, A12S_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a12s_display_off, DSI_PKT_TYPE_WR, A12S_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(a12s_brightness_mode, DSI_PKT_TYPE_WR, A12S_BRIGHTNESS_MODE, 0);

static DEFINE_PKTUI(a12s_brightness, &a12s_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a12s_brightness, DSI_PKT_TYPE_WR, A12S_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_001, DSI_PKT_TYPE_WR, A12S_ILI9882Q_001, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_002, DSI_PKT_TYPE_WR, A12S_ILI9882Q_002, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_003, DSI_PKT_TYPE_WR, A12S_ILI9882Q_003, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_004, DSI_PKT_TYPE_WR, A12S_ILI9882Q_004, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_005, DSI_PKT_TYPE_WR, A12S_ILI9882Q_005, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_006, DSI_PKT_TYPE_WR, A12S_ILI9882Q_006, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_007, DSI_PKT_TYPE_WR, A12S_ILI9882Q_007, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_008, DSI_PKT_TYPE_WR, A12S_ILI9882Q_008, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_009, DSI_PKT_TYPE_WR, A12S_ILI9882Q_009, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_010, DSI_PKT_TYPE_WR, A12S_ILI9882Q_010, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_011, DSI_PKT_TYPE_WR, A12S_ILI9882Q_011, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_012, DSI_PKT_TYPE_WR, A12S_ILI9882Q_012, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_013, DSI_PKT_TYPE_WR, A12S_ILI9882Q_013, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_014, DSI_PKT_TYPE_WR, A12S_ILI9882Q_014, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_015, DSI_PKT_TYPE_WR, A12S_ILI9882Q_015, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_016, DSI_PKT_TYPE_WR, A12S_ILI9882Q_016, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_017, DSI_PKT_TYPE_WR, A12S_ILI9882Q_017, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_018, DSI_PKT_TYPE_WR, A12S_ILI9882Q_018, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_019, DSI_PKT_TYPE_WR, A12S_ILI9882Q_019, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_020, DSI_PKT_TYPE_WR, A12S_ILI9882Q_020, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_021, DSI_PKT_TYPE_WR, A12S_ILI9882Q_021, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_022, DSI_PKT_TYPE_WR, A12S_ILI9882Q_022, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_023, DSI_PKT_TYPE_WR, A12S_ILI9882Q_023, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_024, DSI_PKT_TYPE_WR, A12S_ILI9882Q_024, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_025, DSI_PKT_TYPE_WR, A12S_ILI9882Q_025, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_026, DSI_PKT_TYPE_WR, A12S_ILI9882Q_026, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_027, DSI_PKT_TYPE_WR, A12S_ILI9882Q_027, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_028, DSI_PKT_TYPE_WR, A12S_ILI9882Q_028, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_029, DSI_PKT_TYPE_WR, A12S_ILI9882Q_029, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_030, DSI_PKT_TYPE_WR, A12S_ILI9882Q_030, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_031, DSI_PKT_TYPE_WR, A12S_ILI9882Q_031, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_032, DSI_PKT_TYPE_WR, A12S_ILI9882Q_032, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_033, DSI_PKT_TYPE_WR, A12S_ILI9882Q_033, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_034, DSI_PKT_TYPE_WR, A12S_ILI9882Q_034, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_035, DSI_PKT_TYPE_WR, A12S_ILI9882Q_035, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_036, DSI_PKT_TYPE_WR, A12S_ILI9882Q_036, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_037, DSI_PKT_TYPE_WR, A12S_ILI9882Q_037, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_038, DSI_PKT_TYPE_WR, A12S_ILI9882Q_038, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_039, DSI_PKT_TYPE_WR, A12S_ILI9882Q_039, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_040, DSI_PKT_TYPE_WR, A12S_ILI9882Q_040, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_041, DSI_PKT_TYPE_WR, A12S_ILI9882Q_041, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_042, DSI_PKT_TYPE_WR, A12S_ILI9882Q_042, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_043, DSI_PKT_TYPE_WR, A12S_ILI9882Q_043, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_044, DSI_PKT_TYPE_WR, A12S_ILI9882Q_044, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_045, DSI_PKT_TYPE_WR, A12S_ILI9882Q_045, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_046, DSI_PKT_TYPE_WR, A12S_ILI9882Q_046, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_047, DSI_PKT_TYPE_WR, A12S_ILI9882Q_047, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_048, DSI_PKT_TYPE_WR, A12S_ILI9882Q_048, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_049, DSI_PKT_TYPE_WR, A12S_ILI9882Q_049, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_050, DSI_PKT_TYPE_WR, A12S_ILI9882Q_050, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_051, DSI_PKT_TYPE_WR, A12S_ILI9882Q_051, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_052, DSI_PKT_TYPE_WR, A12S_ILI9882Q_052, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_053, DSI_PKT_TYPE_WR, A12S_ILI9882Q_053, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_054, DSI_PKT_TYPE_WR, A12S_ILI9882Q_054, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_055, DSI_PKT_TYPE_WR, A12S_ILI9882Q_055, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_056, DSI_PKT_TYPE_WR, A12S_ILI9882Q_056, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_057, DSI_PKT_TYPE_WR, A12S_ILI9882Q_057, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_058, DSI_PKT_TYPE_WR, A12S_ILI9882Q_058, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_059, DSI_PKT_TYPE_WR, A12S_ILI9882Q_059, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_060, DSI_PKT_TYPE_WR, A12S_ILI9882Q_060, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_061, DSI_PKT_TYPE_WR, A12S_ILI9882Q_061, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_062, DSI_PKT_TYPE_WR, A12S_ILI9882Q_062, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_063, DSI_PKT_TYPE_WR, A12S_ILI9882Q_063, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_064, DSI_PKT_TYPE_WR, A12S_ILI9882Q_064, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_065, DSI_PKT_TYPE_WR, A12S_ILI9882Q_065, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_066, DSI_PKT_TYPE_WR, A12S_ILI9882Q_066, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_067, DSI_PKT_TYPE_WR, A12S_ILI9882Q_067, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_068, DSI_PKT_TYPE_WR, A12S_ILI9882Q_068, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_069, DSI_PKT_TYPE_WR, A12S_ILI9882Q_069, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_070, DSI_PKT_TYPE_WR, A12S_ILI9882Q_070, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_071, DSI_PKT_TYPE_WR, A12S_ILI9882Q_071, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_072, DSI_PKT_TYPE_WR, A12S_ILI9882Q_072, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_073, DSI_PKT_TYPE_WR, A12S_ILI9882Q_073, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_074, DSI_PKT_TYPE_WR, A12S_ILI9882Q_074, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_075, DSI_PKT_TYPE_WR, A12S_ILI9882Q_075, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_076, DSI_PKT_TYPE_WR, A12S_ILI9882Q_076, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_077, DSI_PKT_TYPE_WR, A12S_ILI9882Q_077, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_078, DSI_PKT_TYPE_WR, A12S_ILI9882Q_078, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_079, DSI_PKT_TYPE_WR, A12S_ILI9882Q_079, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_080, DSI_PKT_TYPE_WR, A12S_ILI9882Q_080, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_081, DSI_PKT_TYPE_WR, A12S_ILI9882Q_081, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_082, DSI_PKT_TYPE_WR, A12S_ILI9882Q_082, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_083, DSI_PKT_TYPE_WR, A12S_ILI9882Q_083, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_084, DSI_PKT_TYPE_WR, A12S_ILI9882Q_084, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_085, DSI_PKT_TYPE_WR, A12S_ILI9882Q_085, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_086, DSI_PKT_TYPE_WR, A12S_ILI9882Q_086, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_087, DSI_PKT_TYPE_WR, A12S_ILI9882Q_087, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_088, DSI_PKT_TYPE_WR, A12S_ILI9882Q_088, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_089, DSI_PKT_TYPE_WR, A12S_ILI9882Q_089, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_090, DSI_PKT_TYPE_WR, A12S_ILI9882Q_090, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_091, DSI_PKT_TYPE_WR, A12S_ILI9882Q_091, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_092, DSI_PKT_TYPE_WR, A12S_ILI9882Q_092, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_093, DSI_PKT_TYPE_WR, A12S_ILI9882Q_093, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_094, DSI_PKT_TYPE_WR, A12S_ILI9882Q_094, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_095, DSI_PKT_TYPE_WR, A12S_ILI9882Q_095, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_096, DSI_PKT_TYPE_WR, A12S_ILI9882Q_096, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_097, DSI_PKT_TYPE_WR, A12S_ILI9882Q_097, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_098, DSI_PKT_TYPE_WR, A12S_ILI9882Q_098, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_099, DSI_PKT_TYPE_WR, A12S_ILI9882Q_099, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_100, DSI_PKT_TYPE_WR, A12S_ILI9882Q_100, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_101, DSI_PKT_TYPE_WR, A12S_ILI9882Q_101, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_102, DSI_PKT_TYPE_WR, A12S_ILI9882Q_102, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_103, DSI_PKT_TYPE_WR, A12S_ILI9882Q_103, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_104, DSI_PKT_TYPE_WR, A12S_ILI9882Q_104, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_105, DSI_PKT_TYPE_WR, A12S_ILI9882Q_105, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_106, DSI_PKT_TYPE_WR, A12S_ILI9882Q_106, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_107, DSI_PKT_TYPE_WR, A12S_ILI9882Q_107, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_108, DSI_PKT_TYPE_WR, A12S_ILI9882Q_108, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_109, DSI_PKT_TYPE_WR, A12S_ILI9882Q_109, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_110, DSI_PKT_TYPE_WR, A12S_ILI9882Q_110, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_111, DSI_PKT_TYPE_WR, A12S_ILI9882Q_111, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_112, DSI_PKT_TYPE_WR, A12S_ILI9882Q_112, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_113, DSI_PKT_TYPE_WR, A12S_ILI9882Q_113, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_114, DSI_PKT_TYPE_WR, A12S_ILI9882Q_114, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_115, DSI_PKT_TYPE_WR, A12S_ILI9882Q_115, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_116, DSI_PKT_TYPE_WR, A12S_ILI9882Q_116, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_117, DSI_PKT_TYPE_WR, A12S_ILI9882Q_117, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_118, DSI_PKT_TYPE_WR, A12S_ILI9882Q_118, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_119, DSI_PKT_TYPE_WR, A12S_ILI9882Q_119, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_120, DSI_PKT_TYPE_WR, A12S_ILI9882Q_120, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_121, DSI_PKT_TYPE_WR, A12S_ILI9882Q_121, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_122, DSI_PKT_TYPE_WR, A12S_ILI9882Q_122, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_123, DSI_PKT_TYPE_WR, A12S_ILI9882Q_123, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_124, DSI_PKT_TYPE_WR, A12S_ILI9882Q_124, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_125, DSI_PKT_TYPE_WR, A12S_ILI9882Q_125, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_126, DSI_PKT_TYPE_WR, A12S_ILI9882Q_126, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_127, DSI_PKT_TYPE_WR, A12S_ILI9882Q_127, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_128, DSI_PKT_TYPE_WR, A12S_ILI9882Q_128, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_129, DSI_PKT_TYPE_WR, A12S_ILI9882Q_129, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_130, DSI_PKT_TYPE_WR, A12S_ILI9882Q_130, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_131, DSI_PKT_TYPE_WR, A12S_ILI9882Q_131, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_132, DSI_PKT_TYPE_WR, A12S_ILI9882Q_132, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_133, DSI_PKT_TYPE_WR, A12S_ILI9882Q_133, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_134, DSI_PKT_TYPE_WR, A12S_ILI9882Q_134, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_135, DSI_PKT_TYPE_WR, A12S_ILI9882Q_135, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_136, DSI_PKT_TYPE_WR, A12S_ILI9882Q_136, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_137, DSI_PKT_TYPE_WR, A12S_ILI9882Q_137, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_138, DSI_PKT_TYPE_WR, A12S_ILI9882Q_138, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_139, DSI_PKT_TYPE_WR, A12S_ILI9882Q_139, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_140, DSI_PKT_TYPE_WR, A12S_ILI9882Q_140, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_141, DSI_PKT_TYPE_WR, A12S_ILI9882Q_141, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_142, DSI_PKT_TYPE_WR, A12S_ILI9882Q_142, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_143, DSI_PKT_TYPE_WR, A12S_ILI9882Q_143, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_144, DSI_PKT_TYPE_WR, A12S_ILI9882Q_144, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_145, DSI_PKT_TYPE_WR, A12S_ILI9882Q_145, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_146, DSI_PKT_TYPE_WR, A12S_ILI9882Q_146, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_147, DSI_PKT_TYPE_WR, A12S_ILI9882Q_147, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_148, DSI_PKT_TYPE_WR, A12S_ILI9882Q_148, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_149, DSI_PKT_TYPE_WR, A12S_ILI9882Q_149, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_150, DSI_PKT_TYPE_WR, A12S_ILI9882Q_150, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_151, DSI_PKT_TYPE_WR, A12S_ILI9882Q_151, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_152, DSI_PKT_TYPE_WR, A12S_ILI9882Q_152, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_153, DSI_PKT_TYPE_WR, A12S_ILI9882Q_153, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_154, DSI_PKT_TYPE_WR, A12S_ILI9882Q_154, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_155, DSI_PKT_TYPE_WR, A12S_ILI9882Q_155, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_156, DSI_PKT_TYPE_WR, A12S_ILI9882Q_156, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_157, DSI_PKT_TYPE_WR, A12S_ILI9882Q_157, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_158, DSI_PKT_TYPE_WR, A12S_ILI9882Q_158, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_159, DSI_PKT_TYPE_WR, A12S_ILI9882Q_159, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_160, DSI_PKT_TYPE_WR, A12S_ILI9882Q_160, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_161, DSI_PKT_TYPE_WR, A12S_ILI9882Q_161, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_162, DSI_PKT_TYPE_WR, A12S_ILI9882Q_162, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_163, DSI_PKT_TYPE_WR, A12S_ILI9882Q_163, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_164, DSI_PKT_TYPE_WR, A12S_ILI9882Q_164, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_165, DSI_PKT_TYPE_WR, A12S_ILI9882Q_165, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_166, DSI_PKT_TYPE_WR, A12S_ILI9882Q_166, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_167, DSI_PKT_TYPE_WR, A12S_ILI9882Q_167, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_168, DSI_PKT_TYPE_WR, A12S_ILI9882Q_168, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_169, DSI_PKT_TYPE_WR, A12S_ILI9882Q_169, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_170, DSI_PKT_TYPE_WR, A12S_ILI9882Q_170, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_171, DSI_PKT_TYPE_WR, A12S_ILI9882Q_171, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_172, DSI_PKT_TYPE_WR, A12S_ILI9882Q_172, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_173, DSI_PKT_TYPE_WR, A12S_ILI9882Q_173, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_174, DSI_PKT_TYPE_WR, A12S_ILI9882Q_174, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_175, DSI_PKT_TYPE_WR, A12S_ILI9882Q_175, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_176, DSI_PKT_TYPE_WR, A12S_ILI9882Q_176, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_177, DSI_PKT_TYPE_WR, A12S_ILI9882Q_177, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_178, DSI_PKT_TYPE_WR, A12S_ILI9882Q_178, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_179, DSI_PKT_TYPE_WR, A12S_ILI9882Q_179, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_180, DSI_PKT_TYPE_WR, A12S_ILI9882Q_180, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_181, DSI_PKT_TYPE_WR, A12S_ILI9882Q_181, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_182, DSI_PKT_TYPE_WR, A12S_ILI9882Q_182, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_183, DSI_PKT_TYPE_WR, A12S_ILI9882Q_183, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_184, DSI_PKT_TYPE_WR, A12S_ILI9882Q_184, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_185, DSI_PKT_TYPE_WR, A12S_ILI9882Q_185, 0);
static DEFINE_STATIC_PACKET(a12s_ili9882q_boe_186, DSI_PKT_TYPE_WR, A12S_ILI9882Q_186, 0);





static DEFINE_PANEL_MDELAY(a12s_wait_display_off, 1 * 17); /* 1 frame */
static DEFINE_PANEL_MDELAY(a12s_wait_sleep_in, 4 * 17); /* 4 frame */
static DEFINE_PANEL_MDELAY(a12s_wait_60msec, 60);
static DEFINE_PANEL_MDELAY(a12s_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(a12s_wait_blic_off, 1);


static void *a12s_init_cmdtbl[] = {
	&ili9882q_boe_restbl[RES_ID],
	&PKTINFO(a12s_ili9882q_boe_001),
	&PKTINFO(a12s_ili9882q_boe_002),
	&PKTINFO(a12s_ili9882q_boe_003),
	&PKTINFO(a12s_ili9882q_boe_004),
	&PKTINFO(a12s_ili9882q_boe_005),
	&PKTINFO(a12s_ili9882q_boe_006),
	&PKTINFO(a12s_ili9882q_boe_007),
	&PKTINFO(a12s_ili9882q_boe_008),
	&PKTINFO(a12s_ili9882q_boe_009),
	&PKTINFO(a12s_ili9882q_boe_010),
	&PKTINFO(a12s_ili9882q_boe_011),
	&PKTINFO(a12s_ili9882q_boe_012),
	&PKTINFO(a12s_ili9882q_boe_013),
	&PKTINFO(a12s_ili9882q_boe_014),
	&PKTINFO(a12s_ili9882q_boe_015),
	&PKTINFO(a12s_ili9882q_boe_016),
	&PKTINFO(a12s_ili9882q_boe_017),
	&PKTINFO(a12s_ili9882q_boe_018),
	&PKTINFO(a12s_ili9882q_boe_019),
	&PKTINFO(a12s_ili9882q_boe_020),
	&PKTINFO(a12s_ili9882q_boe_021),
	&PKTINFO(a12s_ili9882q_boe_022),
	&PKTINFO(a12s_ili9882q_boe_023),
	&PKTINFO(a12s_ili9882q_boe_024),
	&PKTINFO(a12s_ili9882q_boe_025),
	&PKTINFO(a12s_ili9882q_boe_026),
	&PKTINFO(a12s_ili9882q_boe_027),
	&PKTINFO(a12s_ili9882q_boe_028),
	&PKTINFO(a12s_ili9882q_boe_029),
	&PKTINFO(a12s_ili9882q_boe_030),
	&PKTINFO(a12s_ili9882q_boe_031),
	&PKTINFO(a12s_ili9882q_boe_032),
	&PKTINFO(a12s_ili9882q_boe_033),
	&PKTINFO(a12s_ili9882q_boe_034),
	&PKTINFO(a12s_ili9882q_boe_035),
	&PKTINFO(a12s_ili9882q_boe_036),
	&PKTINFO(a12s_ili9882q_boe_037),
	&PKTINFO(a12s_ili9882q_boe_038),
	&PKTINFO(a12s_ili9882q_boe_039),
	&PKTINFO(a12s_ili9882q_boe_040),
	&PKTINFO(a12s_ili9882q_boe_041),
	&PKTINFO(a12s_ili9882q_boe_042),
	&PKTINFO(a12s_ili9882q_boe_043),
	&PKTINFO(a12s_ili9882q_boe_044),
	&PKTINFO(a12s_ili9882q_boe_045),
	&PKTINFO(a12s_ili9882q_boe_046),
	&PKTINFO(a12s_ili9882q_boe_047),
	&PKTINFO(a12s_ili9882q_boe_048),
	&PKTINFO(a12s_ili9882q_boe_049),
	&PKTINFO(a12s_ili9882q_boe_050),
	&PKTINFO(a12s_ili9882q_boe_051),
	&PKTINFO(a12s_ili9882q_boe_052),
	&PKTINFO(a12s_ili9882q_boe_053),
	&PKTINFO(a12s_ili9882q_boe_054),
	&PKTINFO(a12s_ili9882q_boe_055),
	&PKTINFO(a12s_ili9882q_boe_056),
	&PKTINFO(a12s_ili9882q_boe_057),
	&PKTINFO(a12s_ili9882q_boe_058),
	&PKTINFO(a12s_ili9882q_boe_059),
	&PKTINFO(a12s_ili9882q_boe_060),
	&PKTINFO(a12s_ili9882q_boe_061),
	&PKTINFO(a12s_ili9882q_boe_062),
	&PKTINFO(a12s_ili9882q_boe_063),
	&PKTINFO(a12s_ili9882q_boe_064),
	&PKTINFO(a12s_ili9882q_boe_065),
	&PKTINFO(a12s_ili9882q_boe_066),
	&PKTINFO(a12s_ili9882q_boe_067),
	&PKTINFO(a12s_ili9882q_boe_068),
	&PKTINFO(a12s_ili9882q_boe_069),
	&PKTINFO(a12s_ili9882q_boe_070),
	&PKTINFO(a12s_ili9882q_boe_071),
	&PKTINFO(a12s_ili9882q_boe_072),
	&PKTINFO(a12s_ili9882q_boe_073),
	&PKTINFO(a12s_ili9882q_boe_074),
	&PKTINFO(a12s_ili9882q_boe_075),
	&PKTINFO(a12s_ili9882q_boe_076),
	&PKTINFO(a12s_ili9882q_boe_077),
	&PKTINFO(a12s_ili9882q_boe_078),
	&PKTINFO(a12s_ili9882q_boe_079),
	&PKTINFO(a12s_ili9882q_boe_080),
	&PKTINFO(a12s_ili9882q_boe_081),
	&PKTINFO(a12s_ili9882q_boe_082),
	&PKTINFO(a12s_ili9882q_boe_083),
	&PKTINFO(a12s_ili9882q_boe_084),
	&PKTINFO(a12s_ili9882q_boe_085),
	&PKTINFO(a12s_ili9882q_boe_086),
	&PKTINFO(a12s_ili9882q_boe_087),
	&PKTINFO(a12s_ili9882q_boe_088),
	&PKTINFO(a12s_ili9882q_boe_089),
	&PKTINFO(a12s_ili9882q_boe_090),
	&PKTINFO(a12s_ili9882q_boe_091),
	&PKTINFO(a12s_ili9882q_boe_092),
	&PKTINFO(a12s_ili9882q_boe_093),
	&PKTINFO(a12s_ili9882q_boe_094),
	&PKTINFO(a12s_ili9882q_boe_095),
	&PKTINFO(a12s_ili9882q_boe_096),
	&PKTINFO(a12s_ili9882q_boe_097),
	&PKTINFO(a12s_ili9882q_boe_098),
	&PKTINFO(a12s_ili9882q_boe_099),
	&PKTINFO(a12s_ili9882q_boe_100),
	&PKTINFO(a12s_ili9882q_boe_101),
	&PKTINFO(a12s_ili9882q_boe_102),
	&PKTINFO(a12s_ili9882q_boe_103),
	&PKTINFO(a12s_ili9882q_boe_104),
	&PKTINFO(a12s_ili9882q_boe_105),
	&PKTINFO(a12s_ili9882q_boe_106),
	&PKTINFO(a12s_ili9882q_boe_107),
	&PKTINFO(a12s_ili9882q_boe_108),
	&PKTINFO(a12s_ili9882q_boe_109),
	&PKTINFO(a12s_ili9882q_boe_110),
	&PKTINFO(a12s_ili9882q_boe_111),
	&PKTINFO(a12s_ili9882q_boe_112),
	&PKTINFO(a12s_ili9882q_boe_113),
	&PKTINFO(a12s_ili9882q_boe_114),
	&PKTINFO(a12s_ili9882q_boe_115),
	&PKTINFO(a12s_ili9882q_boe_116),
	&PKTINFO(a12s_ili9882q_boe_117),
	&PKTINFO(a12s_ili9882q_boe_118),
	&PKTINFO(a12s_ili9882q_boe_119),
	&PKTINFO(a12s_ili9882q_boe_120),
	&PKTINFO(a12s_ili9882q_boe_121),
	&PKTINFO(a12s_ili9882q_boe_122),
	&PKTINFO(a12s_ili9882q_boe_123),
	&PKTINFO(a12s_ili9882q_boe_124),
	&PKTINFO(a12s_ili9882q_boe_125),
	&PKTINFO(a12s_ili9882q_boe_126),
	&PKTINFO(a12s_ili9882q_boe_127),
	&PKTINFO(a12s_ili9882q_boe_128),
	&PKTINFO(a12s_ili9882q_boe_129),
	&PKTINFO(a12s_ili9882q_boe_130),
	&PKTINFO(a12s_ili9882q_boe_131),
	&PKTINFO(a12s_ili9882q_boe_132),
	&PKTINFO(a12s_ili9882q_boe_133),
	&PKTINFO(a12s_ili9882q_boe_134),
	&PKTINFO(a12s_ili9882q_boe_135),
	&PKTINFO(a12s_ili9882q_boe_136),
	&PKTINFO(a12s_ili9882q_boe_137),
	&PKTINFO(a12s_ili9882q_boe_138),
	&PKTINFO(a12s_ili9882q_boe_139),
	&PKTINFO(a12s_ili9882q_boe_140),
	&PKTINFO(a12s_ili9882q_boe_141),
	&PKTINFO(a12s_ili9882q_boe_142),
	&PKTINFO(a12s_ili9882q_boe_143),
	&PKTINFO(a12s_ili9882q_boe_144),
	&PKTINFO(a12s_ili9882q_boe_145),
	&PKTINFO(a12s_ili9882q_boe_146),
	&PKTINFO(a12s_ili9882q_boe_147),
	&PKTINFO(a12s_ili9882q_boe_148),
	&PKTINFO(a12s_ili9882q_boe_149),
	&PKTINFO(a12s_ili9882q_boe_150),
	&PKTINFO(a12s_ili9882q_boe_151),
	&PKTINFO(a12s_ili9882q_boe_152),
	&PKTINFO(a12s_ili9882q_boe_153),
	&PKTINFO(a12s_ili9882q_boe_154),
	&PKTINFO(a12s_ili9882q_boe_155),
	&PKTINFO(a12s_ili9882q_boe_156),
	&PKTINFO(a12s_ili9882q_boe_157),
	&PKTINFO(a12s_ili9882q_boe_158),
	&PKTINFO(a12s_ili9882q_boe_159),
	&PKTINFO(a12s_ili9882q_boe_160),
	&PKTINFO(a12s_ili9882q_boe_161),
	&PKTINFO(a12s_ili9882q_boe_162),
	&PKTINFO(a12s_ili9882q_boe_163),
	&PKTINFO(a12s_ili9882q_boe_164),
	&PKTINFO(a12s_ili9882q_boe_165),
	&PKTINFO(a12s_ili9882q_boe_166),
	&PKTINFO(a12s_ili9882q_boe_167),
	&PKTINFO(a12s_ili9882q_boe_168),
	&PKTINFO(a12s_ili9882q_boe_169),
	&PKTINFO(a12s_ili9882q_boe_170),
	&PKTINFO(a12s_ili9882q_boe_171),
	&PKTINFO(a12s_ili9882q_boe_172),
	&PKTINFO(a12s_ili9882q_boe_173),
	&PKTINFO(a12s_ili9882q_boe_174),
	&PKTINFO(a12s_ili9882q_boe_175),
	&PKTINFO(a12s_ili9882q_boe_176),
	&PKTINFO(a12s_ili9882q_boe_177),
	&PKTINFO(a12s_ili9882q_boe_178),
	&PKTINFO(a12s_ili9882q_boe_179),
	&PKTINFO(a12s_ili9882q_boe_180),
	&PKTINFO(a12s_ili9882q_boe_181),
	&PKTINFO(a12s_ili9882q_boe_182),
	&PKTINFO(a12s_ili9882q_boe_183),
	&PKTINFO(a12s_ili9882q_boe_184),
	&PKTINFO(a12s_ili9882q_boe_185),
	&PKTINFO(a12s_ili9882q_boe_186),
	&PKTINFO(a12s_sleep_out),
	&DLYINFO(a12s_wait_60msec),
};

static void *a12s_res_init_cmdtbl[] = {
	&ili9882q_boe_restbl[RES_ID],
};

static void *a12s_set_bl_cmdtbl[] = {
	&PKTINFO(a12s_brightness), //51h
};

static void *a12s_display_on_cmdtbl[] = {
	&PKTINFO(a12s_display_on),
	&PKTINFO(a12s_brightness_mode),
};

static void *a12s_display_off_cmdtbl[] = {
	&PKTINFO(a12s_display_off),
	&DLYINFO(a12s_wait_display_off),
};

static void *a12s_exit_cmdtbl[] = {
	&PKTINFO(a12s_sleep_in),
};

/* ===================================================================================== */
/* ================================= [EA8076 I2C TABLE] ===========+++================= */
/* ===================================================================================== */
static u8 ILI9882Q_A12S_I2C_INIT[] = {
	0x0C, 0x2C,
	0x0D, 0x26,
	0x0E, 0x26,
	0x09, 0xBE,
	0x02, 0x6B,
	0x03, 0x2F,
	0x11, 0x74,
	0x04, 0x05,
	0x05, 0xCC,
	0x10, 0x67,
	0x08, 0x13,
};

static u8 ILI9882Q_A12S_I2C_EXIT_VSN[] = {
	0x09, 0xBC,
};

static u8 ILI9882Q_A12S_I2C_EXIT_VSP[] = {
	0x09, 0xB8,
};

static DEFINE_STATIC_PACKET(ili9882q_boe_a12s_i2c_init, I2C_PKT_TYPE_WR, ILI9882Q_A12S_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(ili9882q_boe_a12s_i2c_exit_vsn, I2C_PKT_TYPE_WR, ILI9882Q_A12S_I2C_EXIT_VSN, 0);
static DEFINE_STATIC_PACKET(ili9882q_boe_a12s_i2c_exit_vsp, I2C_PKT_TYPE_WR, ILI9882Q_A12S_I2C_EXIT_VSP, 0);
static DEFINE_STATIC_PACKET(ili9882q_boe_a12s_i2c_dump, I2C_PKT_TYPE_RD, ILI9882Q_A12S_I2C_INIT, 0);

static void *ili9882q_boe_a12s_init_cmdtbl[] = {
	&PKTINFO(ili9882q_boe_a12s_i2c_init),
};

static void *ili9882q_boe_a12s_exit_cmdtbl[] = {
	&PKTINFO(ili9882q_boe_a12s_i2c_exit_vsn),
	&DLYINFO(a12s_wait_blic_off),
	&PKTINFO(ili9882q_boe_a12s_i2c_exit_vsp),
};

static void *ili9882q_boe_a12s_dump_cmdtbl[] = {
	&PKTINFO(ili9882q_boe_a12s_i2c_dump),
};


static struct seqinfo a12s_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a12s_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a12s_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a12s_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a12s_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a12s_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a12s_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", ili9882q_boe_a12s_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", ili9882q_boe_a12s_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", ili9882q_boe_a12s_dump_cmdtbl),
#endif
};

struct common_panel_info ili9882q_boe_a12s_default_panel_info = {
	.ldi_name = "ili9882q_boe",
	.name = "ili9882q_boe_a12s_default",
	.model = "BOE_6_517_inch",
	.vendor = "BOE",
	.id = 0x3A6250,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(ili9882q_boe_a12s_resol),
		.resol = ili9882q_boe_a12s_resol,
	},
	.maptbl = a12s_maptbl,
	.nr_maptbl = ARRAY_SIZE(a12s_maptbl),
	.seqtbl = a12s_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a12s_seqtbl),
	.rditbl = ili9882q_boe_rditbl,
	.nr_rditbl = ARRAY_SIZE(ili9882q_boe_rditbl),
	.restbl = ili9882q_boe_restbl,
	.nr_restbl = ARRAY_SIZE(ili9882q_boe_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&ili9882q_boe_a12s_panel_dimming_info,
	},
	.i2c_data = &ili9882q_boe_a12s_i2c_data,
};

static DEFINE_PANEL_MDELAY(a12s_wait_i2c_init, 2);

static u8 A12S_KTZ8864_I2C_INIT_1[] = {
	0x0C, 0x2C,
	0x0D, 0x26,
	0x0E, 0x26,
	0x09, 0x9C,
};

static u8 A12S_KTZ8864_I2C_INIT_2[] = {
	0x09, 0x9E,
	0x02, 0x6B,
	0x03, 0x0D,
	0x11, 0x74,
	0x04, 0x05,
	0x05, 0xCC,
	0x10, 0x67,
	0x08, 0x13,
};

static u8 A12S_KTZ8864_I2C_EXIT_VSN[] = {
	0x09, 0x9C,
};

static u8 A12S_KTZ8864_I2C_EXIT_VSP[] = {
	0x09, 0x18,
};

static u8 A12S_KTZ8864_I2C_EXIT_BLEN[] = {
	0x08, 0x00,
};

static DEFINE_STATIC_PACKET(a12s_ktz8864_i2c_init_1, I2C_PKT_TYPE_WR, A12S_KTZ8864_I2C_INIT_1, 0);
static DEFINE_STATIC_PACKET(a12s_ktz8864_i2c_init_2, I2C_PKT_TYPE_WR, A12S_KTZ8864_I2C_INIT_2, 0);
static DEFINE_STATIC_PACKET(a12s_ktz8864_i2c_exit_vsn, I2C_PKT_TYPE_WR, A12S_KTZ8864_I2C_EXIT_VSN, 0);
static DEFINE_STATIC_PACKET(a12s_ktz8864_i2c_exit_vsp, I2C_PKT_TYPE_WR, A12S_KTZ8864_I2C_EXIT_VSP, 0);
static DEFINE_STATIC_PACKET(a12s_ktz8864_i2c_exit_blen, I2C_PKT_TYPE_WR, A12S_KTZ8864_I2C_EXIT_BLEN, 0);

static void *a12s_ktz8864_init_cmdtbl[] = {
	&PKTINFO(a12s_ktz8864_i2c_init_1),
	&DLYINFO(a12s_wait_i2c_init),
	&PKTINFO(a12s_ktz8864_i2c_init_2),
};

static void *a12s_ktz8864_exit_cmdtbl[] = {
	&PKTINFO(a12s_ktz8864_i2c_exit_vsn),
	&DLYINFO(a12s_wait_blic_off),
	&PKTINFO(a12s_ktz8864_i2c_exit_vsp),
	&DLYINFO(a12s_wait_blic_off),
	&PKTINFO(a12s_ktz8864_i2c_exit_blen),
};

static struct seqinfo a12s_blic_ktz8864_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq-ktz8864", a12s_ktz8864_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq-ktz8864", a12s_ktz8864_exit_cmdtbl),
};


static int __init a12s_blic_init(void)
{
	if (get_blic_type() == 1) {
		pr_info("%s BLIC_TYPE_KINETIC_KTZ8864\n", __func__);
		a12s_seqtbl[PANEL_I2C_INIT_SEQ] = a12s_blic_ktz8864_seqtbl[PANEL_I2C_INIT_SEQ];
		a12s_seqtbl[PANEL_I2C_EXIT_SEQ] = a12s_blic_ktz8864_seqtbl[PANEL_I2C_EXIT_SEQ];
	}

	return 0;
}

static int __init ili9882q_boe_a12s_panel_init(void)
{
	a12s_blic_init();

	register_common_panel(&ili9882q_boe_a12s_default_panel_info);

	return 0;
}
arch_initcall(ili9882q_boe_a12s_panel_init)
#endif /* __ILI9882Q_A12S_PANEL_H__ */
