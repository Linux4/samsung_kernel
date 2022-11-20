/*
 * linux/drivers/video/fbdev/exynos/panel/ili7807_boe/ili7807_boe_m13_panel.h
 *
 * Header file for ILI7807 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ILI7807_M13_PANEL_H__
#define __ILI7807_M13_PANEL_H__
#include "../panel_drv.h"
#include "ili7807_boe.h"

#include "ili7807_boe_m13_panel_dimming.h"
#include "ili7807_boe_m13_panel_i2c.h"

#include "ili7807_boe_m13_resol.h"

#undef __pn_name__
#define __pn_name__	m13

#undef __PN_NAME__
#define __PN_NAME__	M13

static struct seqinfo m13_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [ILI7807 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [ILI7807 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [ILI7807 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 m13_brt_table[ILI7807_TOTAL_NR_LUMINANCE][2] = {
	{0x00, 0x00},
	{0x00, 0x10}, {0x00, 0x10}, {0x00, 0x20}, {0x00, 0x20}, {0x00, 0x30}, {0x00, 0x30}, {0x00, 0x40}, {0x00, 0x40}, {0x00, 0x50}, {0x00, 0x50},
	{0x00, 0x60}, {0x00, 0x60}, {0x00, 0x70}, {0x00, 0x70}, {0x00, 0x80}, {0x00, 0x80}, {0x00, 0x90}, {0x00, 0x90}, {0x00, 0xA0}, {0x00, 0xA0},
	{0x00, 0xA0}, {0x00, 0xB0}, {0x00, 0xB0}, {0x00, 0xC0}, {0x00, 0xC0}, {0x00, 0xD0}, {0x00, 0xD0}, {0x00, 0xE0}, {0x00, 0xE0}, {0x00, 0xF0},
	{0x00, 0xF0}, {0x01, 0x00}, {0x01, 0x00}, {0x01, 0x10}, {0x01, 0x10}, {0x01, 0x20}, {0x01, 0x20}, {0x01, 0x30}, {0x01, 0x30}, {0x01, 0x40},
	{0x01, 0x40}, {0x01, 0x40}, {0x01, 0x50}, {0x01, 0x50}, {0x01, 0x60}, {0x01, 0x60}, {0x01, 0x70}, {0x01, 0x70}, {0x01, 0x80}, {0x01, 0x80},
	{0x01, 0x90}, {0x01, 0x90}, {0x01, 0xA0}, {0x01, 0xA0}, {0x01, 0xB0}, {0x01, 0xB0}, {0x01, 0xC0}, {0x01, 0xC0}, {0x01, 0xD0}, {0x01, 0xD0},
	{0x01, 0xE0}, {0x01, 0xE0}, {0x01, 0xF0}, {0x02, 0x00}, {0x02, 0x00}, {0x02, 0x10}, {0x02, 0x20}, {0x02, 0x20}, {0x02, 0x30}, {0x02, 0x30},
	{0x02, 0x40}, {0x02, 0x50}, {0x02, 0x50}, {0x02, 0x60}, {0x02, 0x70}, {0x02, 0x70}, {0x02, 0x80}, {0x02, 0x90}, {0x02, 0x90}, {0x02, 0xA0},
	{0x02, 0xB0}, {0x02, 0xB0}, {0x02, 0xC0}, {0x02, 0xD0}, {0x02, 0xD0}, {0x02, 0xE0}, {0x02, 0xE0}, {0x02, 0xF0}, {0x03, 0x00}, {0x03, 0x00},
	{0x03, 0x10}, {0x03, 0x20}, {0x03, 0x20}, {0x03, 0x30}, {0x03, 0x40}, {0x03, 0x40}, {0x03, 0x50}, {0x03, 0x60}, {0x03, 0x60}, {0x03, 0x70},
	{0x03, 0x80}, {0x03, 0x80}, {0x03, 0x90}, {0x03, 0x90}, {0x03, 0xA0}, {0x03, 0xB0}, {0x03, 0xB0}, {0x03, 0xC0}, {0x03, 0xD0}, {0x03, 0xD0},
	{0x03, 0xE0}, {0x03, 0xF0}, {0x03, 0xF0}, {0x04, 0x00}, {0x04, 0x10}, {0x04, 0x10}, {0x04, 0x20}, {0x04, 0x30}, {0x04, 0x30}, {0x04, 0x40},
	{0x04, 0x40}, {0x04, 0x50}, {0x04, 0x60}, {0x04, 0x60}, {0x04, 0x70}, {0x04, 0x80}, {0x04, 0x80}, {0x04, 0x90}, {0x04, 0xA0}, {0x04, 0xB0},
	{0x04, 0xC0}, {0x04, 0xD0}, {0x04, 0xD0}, {0x04, 0xE0}, {0x04, 0xF0}, {0x05, 0x00}, {0x05, 0x10}, {0x05, 0x20}, {0x05, 0x30}, {0x05, 0x40},
	{0x05, 0x40}, {0x05, 0x50}, {0x05, 0x60}, {0x05, 0x70}, {0x05, 0x80}, {0x05, 0x90}, {0x05, 0xA0}, {0x05, 0xB0}, {0x05, 0xB0}, {0x05, 0xC0},
	{0x05, 0xD0}, {0x05, 0xE0}, {0x05, 0xF0}, {0x06, 0x00}, {0x06, 0x10}, {0x06, 0x20}, {0x06, 0x20}, {0x06, 0x30}, {0x06, 0x40}, {0x06, 0x50},
	{0x06, 0x60}, {0x06, 0x70}, {0x06, 0x80}, {0x06, 0x90}, {0x06, 0x90}, {0x06, 0xA0}, {0x06, 0xB0}, {0x06, 0xC0}, {0x06, 0xD0}, {0x06, 0xE0},
	{0x06, 0xF0}, {0x07, 0x00}, {0x07, 0x00}, {0x07, 0x10}, {0x07, 0x20}, {0x07, 0x30}, {0x07, 0x40}, {0x07, 0x50}, {0x07, 0x60}, {0x07, 0x70},
	{0x07, 0x70}, {0x07, 0x80}, {0x07, 0x90}, {0x07, 0xA0}, {0x07, 0xB0}, {0x07, 0xC0}, {0x07, 0xD0}, {0x07, 0xE0}, {0x07, 0xE0}, {0x07, 0xF0},
	{0x08, 0x00}, {0x08, 0x10}, {0x08, 0x20}, {0x08, 0x30}, {0x08, 0x40}, {0x08, 0x50}, {0x08, 0x60}, {0x08, 0x70}, {0x08, 0x70}, {0x08, 0x80},
	{0x08, 0x90}, {0x08, 0xA0}, {0x08, 0xB0}, {0x08, 0xC0}, {0x08, 0xD0}, {0x08, 0xE0}, {0x08, 0xF0}, {0x09, 0x00}, {0x09, 0x10}, {0x09, 0x20},
	{0x09, 0x20}, {0x09, 0x30}, {0x09, 0x40}, {0x09, 0x50}, {0x09, 0x60}, {0x09, 0x70}, {0x09, 0x80}, {0x09, 0x90}, {0x09, 0xA0}, {0x09, 0xB0},
	{0x09, 0xC0}, {0x09, 0xD0}, {0x09, 0xE0}, {0x09, 0xE0}, {0x09, 0xF0}, {0x0A, 0x00}, {0x0A, 0x10}, {0x0A, 0x20}, {0x0A, 0x30}, {0x0A, 0x40},
	{0x0A, 0x50}, {0x0A, 0x60}, {0x0A, 0x70}, {0x0A, 0x80}, {0x0A, 0x90}, {0x0A, 0xA0}, {0x0A, 0xA0}, {0x0A, 0xB0}, {0x0A, 0xC0}, {0x0A, 0xD0},
	{0x0A, 0xE0}, {0x0A, 0xF0}, {0x0B, 0x00}, {0x0B, 0x10}, {0x0B, 0x20}, {0x0B, 0x30}, {0x0B, 0x40}, {0x0B, 0x50}, {0x0B, 0x50}, {0x0B, 0x60},
	{0x0B, 0x70}, {0x0B, 0x80}, {0x0B, 0x90}, {0x0B, 0xA0}, {0x0B, 0xB0}, {0x0B, 0xC0}, {0x0B, 0xD0}, {0x0B, 0xD0}, {0x0B, 0xE0}, {0x0B, 0xF0},
	{0x0C, 0x00}, {0x0C, 0x00}, {0x0C, 0x10}, {0x0C, 0x20}, {0x0C, 0x30}, {0x0C, 0x30}, {0x0C, 0x40}, {0x0C, 0x50}, {0x0C, 0x60}, {0x0C, 0x60},
	{0x0C, 0x70}, {0x0C, 0x80}, {0x0C, 0x90}, {0x0C, 0xA0}, {0x0C, 0xA0}, {0x0C, 0xB0}, {0x0C, 0xC0}, {0x0C, 0xD0}, {0x0C, 0xD0}, {0x0C, 0xE0},
	{0x0C, 0xF0}, {0x0D, 0x00}, {0x0D, 0x00}, {0x0D, 0x10}, {0x0D, 0x20}, {0x0D, 0x30}, {0x0D, 0x30}, {0x0D, 0x40}, {0x0D, 0x50}, {0x0D, 0x60},
	{0x0D, 0x70}, {0x0D, 0x70}, {0x0D, 0x80}, {0x0D, 0x90}, {0x0D, 0xA0}, {0x0D, 0xA0}, {0x0D, 0xB0}, {0x0D, 0xC0}, {0x0D, 0xD0}, {0x0D, 0xD0},
	{0x0D, 0xE0}, {0x0D, 0xF0}, {0x0E, 0x00}, {0x0E, 0x00}, {0x0E, 0x10}, {0x0E, 0x20},
};


static struct maptbl m13_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(m13_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [ILI7807 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 M13_SLEEP_OUT[] = { 0x11 };
static u8 M13_SLEEP_IN[] = { 0x10 };
static u8 M13_DISPLAY_OFF[] = { 0x28 };
static u8 M13_DISPLAY_ON[] = { 0x29 };

static u8 M13_BRIGHTNESS[] = {
	0x51,
	0x0F, 0xF0,
};

static u8 M13_BRIGHTNESS_MODE[] = {
	0x53,
	0x2C,
};

static u8 M13_ILI7807_001[] = {
	0xFF,
	0x78, 0x07, 0x01,
};

static u8 M13_ILI7807_002[] = {
	0x00,
	0x62,
};

static u8 M13_ILI7807_003[] = {
	0x01,
	0x11,
};

static u8 M13_ILI7807_004[] = {
	0x02,
	0x00,
};

static u8 M13_ILI7807_005[] = {
	0x03,
	0x00,
};

static u8 M13_ILI7807_006[] = {
	0x04,
	0x00,
};

static u8 M13_ILI7807_007[] = {
	0x05,
	0x00,
};

static u8 M13_ILI7807_008[] = {
	0x06,
	0x00,
};

static u8 M13_ILI7807_009[] = {
	0x07,
	0x00,
};

static u8 M13_ILI7807_010[] = {
	0x08,
	0xA9,
};

static u8 M13_ILI7807_011[] = {
	0x09,
	0x0A,
};

static u8 M13_ILI7807_012[] = {
	0x0A,
	0x30,
};

static u8 M13_ILI7807_013[] = {
	0x0B,
	0x00,
};

static u8 M13_ILI7807_014[] = {
	0x0C,
	0x00,
};

static u8 M13_ILI7807_015[] = {
	0x0D,
	0x00,
};

static u8 M13_ILI7807_016[] = {
	0x0E,
	0x00,
};

static u8 M13_ILI7807_017[] = {
	0x31,
	0x07,
};

static u8 M13_ILI7807_018[] = {
	0x32,
	0x02,
};

static u8 M13_ILI7807_019[] = {
	0x33,
	0x41,
};

static u8 M13_ILI7807_020[] = {
	0x34,
	0x41,
};

static u8 M13_ILI7807_021[] = {
	0x35,
	0x28,
};

static u8 M13_ILI7807_022[] = {
	0x36,
	0x28,
};

static u8 M13_ILI7807_023[] = {
	0x37,
	0x30,
};

static u8 M13_ILI7807_024[] = {
	0x38,
	0x2F,
};

static u8 M13_ILI7807_025[] = {
	0x39,
	0x2E,
};

static u8 M13_ILI7807_026[] = {
	0x3A,
	0x30,
};

static u8 M13_ILI7807_027[] = {
	0x3B,
	0x2F,
};

static u8 M13_ILI7807_028[] = {
	0x3C,
	0x2E,
};

static u8 M13_ILI7807_029[] = {
	0x3D,
	0x25,
};

static u8 M13_ILI7807_030[] = {
	0x3E,
	0x11,
};

static u8 M13_ILI7807_031[] = {
	0x3F,
	0x10,
};

static u8 M13_ILI7807_032[] = {
	0x40,
	0x13,
};

static u8 M13_ILI7807_033[] = {
	0x41,
	0x12,
};

static u8 M13_ILI7807_034[] = {
	0x42,
	0x2C,
};

static u8 M13_ILI7807_035[] = {
	0x43,
	0x40,
};

static u8 M13_ILI7807_036[] = {
	0x44,
	0x40,
};

static u8 M13_ILI7807_037[] = {
	0x45,
	0x01,
};

static u8 M13_ILI7807_038[] = {
	0x46,
	0x00,
};

static u8 M13_ILI7807_039[] = {
	0x47,
	0x09,
};

static u8 M13_ILI7807_040[] = {
	0x48,
	0x08,
};

static u8 M13_ILI7807_041[] = {
	0x49,
	0x07,
};

static u8 M13_ILI7807_042[] = {
	0x4A,
	0x02,
};

static u8 M13_ILI7807_043[] = {
	0x4B,
	0x41,
};

static u8 M13_ILI7807_044[] = {
	0x4C,
	0x41,
};

static u8 M13_ILI7807_045[] = {
	0x4D,
	0x28,
};

static u8 M13_ILI7807_046[] = {
	0x4E,
	0x28,
};

static u8 M13_ILI7807_047[] = {
	0x4F,
	0x30,
};

static u8 M13_ILI7807_048[] = {
	0x50,
	0x2F,
};

static u8 M13_ILI7807_049[] = {
	0x51,
	0x2E,
};

static u8 M13_ILI7807_050[] = {
	0x52,
	0x30,
};

static u8 M13_ILI7807_051[] = {
	0x53,
	0x2F,
};

static u8 M13_ILI7807_052[] = {
	0x54,
	0x2E,
};

static u8 M13_ILI7807_053[] = {
	0x55,
	0x25,
};

static u8 M13_ILI7807_054[] = {
	0x56,
	0x11,
};

static u8 M13_ILI7807_055[] = {
	0x57,
	0x10,
};

static u8 M13_ILI7807_056[] = {
	0x58,
	0x13,
};

static u8 M13_ILI7807_057[] = {
	0x59,
	0x12,
};

static u8 M13_ILI7807_058[] = {
	0x5A,
	0x2C,
};

static u8 M13_ILI7807_059[] = {
	0x5B,
	0x40,
};

static u8 M13_ILI7807_060[] = {
	0x5C,
	0x40,
};

static u8 M13_ILI7807_061[] = {
	0x5D,
	0x01,
};

static u8 M13_ILI7807_062[] = {
	0x5E,
	0x00,
};

static u8 M13_ILI7807_063[] = {
	0x5F,
	0x09,
};

static u8 M13_ILI7807_064[] = {
	0x60,
	0x08,
};

static u8 M13_ILI7807_065[] = {
	0x61,
	0x07,
};

static u8 M13_ILI7807_066[] = {
	0x62,
	0x02,
};

static u8 M13_ILI7807_067[] = {
	0x63,
	0x41,
};

static u8 M13_ILI7807_068[] = {
	0x64,
	0x41,
};

static u8 M13_ILI7807_069[] = {
	0x65,
	0x28,
};

static u8 M13_ILI7807_070[] = {
	0x66,
	0x28,
};

static u8 M13_ILI7807_071[] = {
	0x67,
	0x30,
};

static u8 M13_ILI7807_072[] = {
	0x68,
	0x2F,
};

static u8 M13_ILI7807_073[] = {
	0x69,
	0x2E,
};

static u8 M13_ILI7807_074[] = {
	0x6A,
	0x30,
};

static u8 M13_ILI7807_075[] = {
	0x6B,
	0x2F,
};

static u8 M13_ILI7807_076[] = {
	0x6C,
	0x2E,
};

static u8 M13_ILI7807_077[] = {
	0x6D,
	0x25,
};

static u8 M13_ILI7807_078[] = {
	0x6E,
	0x12,
};

static u8 M13_ILI7807_079[] = {
	0x6F,
	0x13,
};

static u8 M13_ILI7807_080[] = {
	0x70,
	0x10,
};

static u8 M13_ILI7807_081[] = {
	0x71,
	0x11,
};

static u8 M13_ILI7807_082[] = {
	0x72,
	0x2C,
};

static u8 M13_ILI7807_083[] = {
	0x73,
	0x40,
};

static u8 M13_ILI7807_084[] = {
	0x74,
	0x40,
};

static u8 M13_ILI7807_085[] = {
	0x75,
	0x01,
};

static u8 M13_ILI7807_086[] = {
	0x76,
	0x00,
};

static u8 M13_ILI7807_087[] = {
	0x77,
	0x08,
};

static u8 M13_ILI7807_088[] = {
	0x78,
	0x09,
};

static u8 M13_ILI7807_089[] = {
	0x79,
	0x07,
};

static u8 M13_ILI7807_090[] = {
	0x7A,
	0x02,
};

static u8 M13_ILI7807_091[] = {
	0x7B,
	0x41,
};

static u8 M13_ILI7807_092[] = {
	0x7C,
	0x41,
};

static u8 M13_ILI7807_093[] = {
	0x7D,
	0x28,
};

static u8 M13_ILI7807_094[] = {
	0x7E,
	0x28,
};

static u8 M13_ILI7807_095[] = {
	0x7F,
	0x30,
};

static u8 M13_ILI7807_096[] = {
	0x80,
	0x2F,
};

static u8 M13_ILI7807_097[] = {
	0x81,
	0x2E,
};

static u8 M13_ILI7807_098[] = {
	0x82,
	0x30,
};

static u8 M13_ILI7807_099[] = {
	0x83,
	0x2F,
};

static u8 M13_ILI7807_100[] = {
	0x84,
	0x2E,
};

static u8 M13_ILI7807_101[] = {
	0x85,
	0x25,
};

static u8 M13_ILI7807_102[] = {
	0x86,
	0x12,
};

static u8 M13_ILI7807_103[] = {
	0x87,
	0x13,
};

static u8 M13_ILI7807_104[] = {
	0x88,
	0x10,
};

static u8 M13_ILI7807_105[] = {
	0x89,
	0x11,
};

static u8 M13_ILI7807_106[] = {
	0x8A,
	0x2C,
};

static u8 M13_ILI7807_107[] = {
	0x8B,
	0x40,
};

static u8 M13_ILI7807_108[] = {
	0x8C,
	0x40,
};

static u8 M13_ILI7807_109[] = {
	0x8D,
	0x01,
};

static u8 M13_ILI7807_110[] = {
	0x8E,
	0x00,
};

static u8 M13_ILI7807_111[] = {
	0x8F,
	0x08,
};

static u8 M13_ILI7807_112[] = {
	0x90,
	0x09,
};

static u8 M13_ILI7807_113[] = {
	0xA0,
	0x4C,
};

static u8 M13_ILI7807_114[] = {
	0xA1,
	0x4A,
};

static u8 M13_ILI7807_115[] = {
	0xA2,
	0x00,
};

static u8 M13_ILI7807_116[] = {
	0xA3,
	0x00,
};

static u8 M13_ILI7807_117[] = {
	0xA7,
	0x10,
};

static u8 M13_ILI7807_118[] = {
	0xAA,
	0x00,
};

static u8 M13_ILI7807_119[] = {
	0xAB,
	0x00,
};

static u8 M13_ILI7807_120[] = {
	0xAC,
	0x00,
};

static u8 M13_ILI7807_121[] = {
	0xAE,
	0x00,
};

static u8 M13_ILI7807_122[] = {
	0xB0,
	0x20,
};

static u8 M13_ILI7807_123[] = {
	0xB1,
	0x00,
};

static u8 M13_ILI7807_124[] = {
	0xB2,
	0x01,
};

static u8 M13_ILI7807_125[] = {
	0xB3,
	0x04,
};

static u8 M13_ILI7807_126[] = {
	0xB4,
	0x05,
};

static u8 M13_ILI7807_127[] = {
	0xB5,
	0x00,
};

static u8 M13_ILI7807_128[] = {
	0xB6,
	0x00,
};

static u8 M13_ILI7807_129[] = {
	0xB7,
	0x00,
};

static u8 M13_ILI7807_130[] = {
	0xB8,
	0x00,
};

static u8 M13_ILI7807_131[] = {
	0xC0,
	0x0C,
};

static u8 M13_ILI7807_132[] = {
	0xC1,
	0x60,
};

static u8 M13_ILI7807_133[] = {
	0xC2,
	0x00,
};

static u8 M13_ILI7807_134[] = {
	0xCA,
	0x01,
};

static u8 M13_ILI7807_135[] = {
	0xD0,
	0x01,
};

static u8 M13_ILI7807_136[] = {
	0xD1,
	0x00,
};

static u8 M13_ILI7807_137[] = {
	0xD2,
	0x10,
};

static u8 M13_ILI7807_138[] = {
	0xD3,
	0x41,
};

static u8 M13_ILI7807_139[] = {
	0xD4,
	0x89,
};

static u8 M13_ILI7807_140[] = {
	0xD5,
	0x16,
};

static u8 M13_ILI7807_141[] = {
	0xD6,
	0x49,
};

static u8 M13_ILI7807_142[] = {
	0xD7,
	0x40,
};

static u8 M13_ILI7807_143[] = {
	0xD8,
	0x09,
};

static u8 M13_ILI7807_144[] = {
	0xD9,
	0x96,
};

static u8 M13_ILI7807_145[] = {
	0xDA,
	0xAA,
};

static u8 M13_ILI7807_146[] = {
	0xDB,
	0xAA,
};

static u8 M13_ILI7807_147[] = {
	0xDC,
	0x8A,
};

static u8 M13_ILI7807_148[] = {
	0xDD,
	0xA8,
};

static u8 M13_ILI7807_149[] = {
	0xDE,
	0x05,
};

static u8 M13_ILI7807_150[] = {
	0xDF,
	0x42,
};

static u8 M13_ILI7807_151[] = {
	0xE0,
	0x1E,
};

static u8 M13_ILI7807_152[] = {
	0xE1,
	0x68,
};

static u8 M13_ILI7807_153[] = {
	0xE2,
	0x07,
};

static u8 M13_ILI7807_154[] = {
	0xE3,
	0x11,
};

static u8 M13_ILI7807_155[] = {
	0xE4,
	0x42,
};

static u8 M13_ILI7807_156[] = {
	0xE5,
	0x4F,
};

static u8 M13_ILI7807_157[] = {
	0xE6,
	0x2A,
};

static u8 M13_ILI7807_158[] = {
	0xE7,
	0x0C,
};

static u8 M13_ILI7807_159[] = {
	0xE8,
	0x00,
};

static u8 M13_ILI7807_160[] = {
	0xE9,
	0x00,
};

static u8 M13_ILI7807_161[] = {
	0xEA,
	0x00,
};

static u8 M13_ILI7807_162[] = {
	0xEB,
	0x00,
};

static u8 M13_ILI7807_163[] = {
	0xEC,
	0x80,
};

static u8 M13_ILI7807_164[] = {
	0xED,
	0x56,
};

static u8 M13_ILI7807_165[] = {
	0xEE,
	0x00,
};

static u8 M13_ILI7807_166[] = {
	0xEF,
	0x32,
};

static u8 M13_ILI7807_167[] = {
	0xF0,
	0x00,
};

static u8 M13_ILI7807_168[] = {
	0xF1,
	0xC0,
};

static u8 M13_ILI7807_169[] = {
	0xF2,
	0xFF,
};

static u8 M13_ILI7807_170[] = {
	0xF3,
	0xFF,
};

static u8 M13_ILI7807_171[] = {
	0xF4,
	0x10,
};

static u8 M13_ILI7807_172[] = {
	0xFF,
	0x78, 0x07, 0x03,
};

static u8 M13_ILI7807_173[] = {
	0x80,
	0x36,
};

static u8 M13_ILI7807_174[] = {
	0x83,
	0x60,
};

static u8 M13_ILI7807_175[] = {
	0x84,
	0x0E,
};

static u8 M13_ILI7807_176[] = {
	0x88,
	0xE6,
};

static u8 M13_ILI7807_177[] = {
	0x89,
	0xEE,
};

static u8 M13_ILI7807_178[] = {
	0x8A,
	0xF6,
};

static u8 M13_ILI7807_179[] = {
	0x8B,
	0xF6,
};

static u8 M13_ILI7807_180[] = {
	0xC6,
	0x14,
};

static u8 M13_ILI7807_181[] = {
	0xFF,
	0x78, 0x07, 0x11,
};

static u8 M13_ILI7807_182[] = {
	0x38,
	0x00,
};

static u8 M13_ILI7807_183[] = {
	0x39,
	0x04,
};

static u8 M13_ILI7807_184[] = {
	0x50,
	0x15,
};

static u8 M13_ILI7807_185[] = {
	0x51,
	0x05,
};

static u8 M13_ILI7807_186[] = {
	0xFF,
	0x78, 0x07, 0x02,
};

static u8 M13_ILI7807_187[] = {
	0x1B,
	0x02,
};

static u8 M13_ILI7807_188[] = {
	0x40,
	0x08,
};

static u8 M13_ILI7807_189[] = {
	0x41,
	0x00,
};

static u8 M13_ILI7807_190[] = {
	0x42,
	0x08,
};

static u8 M13_ILI7807_191[] = {
	0x43,
	0x24,
};

static u8 M13_ILI7807_192[] = {
	0x46,
	0x21,
};

static u8 M13_ILI7807_193[] = {
	0x47,
	0x03,
};

static u8 M13_ILI7807_194[] = {
	0x4F,
	0x01,
};

static u8 M13_ILI7807_195[] = {
	0x4C,
	0x00,
};

static u8 M13_ILI7807_196[] = {
	0x53,
	0x08,
};

static u8 M13_ILI7807_197[] = {
	0x5B,
	0x50,
};

static u8 M13_ILI7807_198[] = {
	0x76,
	0x1F,
};

static u8 M13_ILI7807_199[] = {
	0x80,
	0x25,
};

static u8 M13_ILI7807_200[] = {
	0x06,
	0x6A,
};

static u8 M13_ILI7807_201[] = {
	0x08,
	0x00,
};

static u8 M13_ILI7807_202[] = {
	0x0E,
	0x2C,
};

static u8 M13_ILI7807_203[] = {
	0x0F,
	0x28,
};

static u8 M13_ILI7807_204[] = {
	0xFF,
	0x78, 0x07, 0x12,
};

static u8 M13_ILI7807_205[] = {
	0x48,
	0x04,
};

static u8 M13_ILI7807_206[] = {
	0x49,
	0x00,
};

static u8 M13_ILI7807_207[] = {
	0x4A,
	0x06,
};

static u8 M13_ILI7807_208[] = {
	0x4B,
	0x20,
};

static u8 M13_ILI7807_209[] = {
	0x4E,
	0x05,
};

static u8 M13_ILI7807_210[] = {
	0x52,
	0x1F,
};

static u8 M13_ILI7807_211[] = {
	0x53,
	0x25,
};

static u8 M13_ILI7807_212[] = {
	0xC8,
	0x47,
};

static u8 M13_ILI7807_213[] = {
	0xC9,
	0x00,
};

static u8 M13_ILI7807_214[] = {
	0xCA,
	0x2C,
};

static u8 M13_ILI7807_215[] = {
	0xCB,
	0x28,
};

static u8 M13_ILI7807_216[] = {
	0xFF,
	0x78, 0x07, 0x04,
};

static u8 M13_ILI7807_217[] = {
	0xBD,
	0x01,
};

static u8 M13_ILI7807_218[] = {
	0xFF,
	0x78, 0x07, 0x05,
};

static u8 M13_ILI7807_219[] = {
	0x72,
	0x56,
};

static u8 M13_ILI7807_220[] = {
	0x74,
	0x56,
};

static u8 M13_ILI7807_221[] = {
	0x76,
	0x51,
};

static u8 M13_ILI7807_222[] = {
	0x7A,
	0x51,
};

static u8 M13_ILI7807_223[] = {
	0x7B,
	0x7E,
};

static u8 M13_ILI7807_224[] = {
	0x7C,
	0x7E,
};

static u8 M13_ILI7807_225[] = {
	0x46,
	0x55,
};

static u8 M13_ILI7807_226[] = {
	0x47,
	0x55,
};

static u8 M13_ILI7807_227[] = {
	0xB5,
	0x55,
};

static u8 M13_ILI7807_228[] = {
	0xB7,
	0x55,
};

static u8 M13_ILI7807_229[] = {
	0xC6,
	0x1B,
};

static u8 M13_ILI7807_230[] = {
	0x56,
	0xFF,
};

static u8 M13_ILI7807_231[] = {
	0x3E,
	0x50,
};

static u8 M13_ILI7807_232[] = {
	0xFF,
	0x78, 0x07, 0x06,
};

static u8 M13_ILI7807_233[] = {
	0xC0,
	0x68,
};

static u8 M13_ILI7807_234[] = {
	0xC1,
	0x19,
};

static u8 M13_ILI7807_235[] = {
	0xC3,
	0x06,
};

static u8 M13_ILI7807_236[] = {
	0x13,
	0x13,
};

static u8 M13_ILI7807_237[] = {
	0xFF,
	0x78, 0x07, 0x07,
};

static u8 M13_ILI7807_238[] = {
	0x29,
	0x80,
};

static u8 M13_ILI7807_239[] = {
	0xFF,
	0x78, 0x07, 0x17,
};

static u8 M13_ILI7807_240[] = {
	0x20,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x89, 0x30,
	0x80, 0x09, 0x68, 0x04, 0x38, 0x00, 0x08, 0x02, 0x1C, 0x02,
	0x1C, 0x00, 0xAA, 0x02, 0x0E, 0x00, 0x20, 0x00, 0x2B, 0x00,
	0x07, 0x00, 0x0C, 0x0D, 0xB7, 0x0C, 0xB7, 0x18, 0x00, 0x1B,
	0xA0, 0x03, 0x0C, 0x20, 0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E,
	0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79,
	0x7B, 0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40, 0x09,
	0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8, 0x1A, 0x38, 0x1A,
	0x78, 0x1A, 0xB6, 0x2A, 0xF6, 0x2B, 0x34, 0x2B, 0x74, 0x3B,
	0x74, 0x6B, 0xF4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static u8 M13_ILI7807_241[] = {
	0xFF,
	0x78, 0x07, 0x08,
};

static u8 M13_ILI7807_242[] = {
	0xE0,
	0x00, 0x00, 0x16, 0x3E, 0x00, 0x79, 0xA4, 0xC6, 0x14, 0xFC,
	0x27, 0x69, 0x25, 0x9A, 0xE6, 0x23, 0x2A, 0x5D, 0x9B, 0xC5,
	0x3E, 0xF8, 0x1B, 0x4C, 0x3F, 0x65, 0x8A, 0xBD, 0x0F, 0xCE,
	0xD7,
};

static u8 M13_ILI7807_243[] = {
	0xE1,
	0x00, 0x00, 0x16, 0x3E, 0x00, 0x79, 0xA4, 0xC6, 0x14, 0xFC,
	0x27, 0x69, 0x25, 0x9A, 0xE6, 0x23, 0x2A, 0x5D, 0x9B, 0xC5,
	0x3E, 0xF8, 0x1B, 0x4C, 0x3F, 0x65, 0x8A, 0xBD, 0x0F, 0xCE,
	0xD7,
};

static u8 M13_ILI7807_244[] = {
	0xFF,
	0x78, 0x07, 0x0B,
};

static u8 M13_ILI7807_245[] = {
	0x94,
	0x88,
};

static u8 M13_ILI7807_246[] = {
	0x95,
	0x21,
};

static u8 M13_ILI7807_247[] = {
	0x96,
	0x06,
};

static u8 M13_ILI7807_248[] = {
	0x97,
	0x06,
};

static u8 M13_ILI7807_249[] = {
	0x98,
	0xCB,
};

static u8 M13_ILI7807_250[] = {
	0x99,
	0xCB,
};

static u8 M13_ILI7807_251[] = {
	0x9A,
	0x46,
};

static u8 M13_ILI7807_252[] = {
	0x9B,
	0xAE,
};

static u8 M13_ILI7807_253[] = {
	0x9C,
	0x05,
};

static u8 M13_ILI7807_254[] = {
	0x9D,
	0x05,
};

static u8 M13_ILI7807_255[] = {
	0x9E,
	0xA7,
};

static u8 M13_ILI7807_256[] = {
	0x9F,
	0xA7,
};

static u8 M13_ILI7807_257[] = {
	0xC6,
	0x85,
};

static u8 M13_ILI7807_258[] = {
	0xC7,
	0x6C,
};

static u8 M13_ILI7807_259[] = {
	0xC8,
	0x04,
};

static u8 M13_ILI7807_260[] = {
	0xC9,
	0x04,
};

static u8 M13_ILI7807_261[] = {
	0xCA,
	0x87,
};

static u8 M13_ILI7807_262[] = {
	0xCB,
	0x87,
};

static u8 M13_ILI7807_263[] = {
	0xD8,
	0x08,
};

static u8 M13_ILI7807_264[] = {
	0xD9,
	0xEA,
};

static u8 M13_ILI7807_265[] = {
	0xDA,
	0x06,
};

static u8 M13_ILI7807_266[] = {
	0xDB,
	0x06,
};

static u8 M13_ILI7807_267[] = {
	0xDC,
	0xDE,
};

static u8 M13_ILI7807_268[] = {
	0xDD,
	0xDE,
};

static u8 M13_ILI7807_269[] = {
	0xAA,
	0x12,
};

static u8 M13_ILI7807_270[] = {
	0xAB,
	0xE0,
};

static u8 M13_ILI7807_271[] = {
	0xFF,
	0x78, 0x07, 0x0C,
};

static u8 M13_ILI7807_272[] = {
	0x80,
	0x2E,
};

static u8 M13_ILI7807_273[] = {
	0x81,
	0xF0,
};

static u8 M13_ILI7807_274[] = {
	0x82,
	0x2E,
};

static u8 M13_ILI7807_275[] = {
	0x83,
	0xF1,
};

static u8 M13_ILI7807_276[] = {
	0x40,
	0x3F,
};

static u8 M13_ILI7807_277[] = {
	0x41,
	0x98,
};

static u8 M13_ILI7807_278[] = {
	0x42,
	0x3F,
};

static u8 M13_ILI7807_279[] = {
	0x43,
	0x9A,
};

static u8 M13_ILI7807_280[] = {
	0x44,
	0x3F,
};

static u8 M13_ILI7807_281[] = {
	0x45,
	0xA0,
};

static u8 M13_ILI7807_282[] = {
	0x46,
	0x3F,
};

static u8 M13_ILI7807_283[] = {
	0x47,
	0x96,
};

static u8 M13_ILI7807_284[] = {
	0x48,
	0x3F,
};

static u8 M13_ILI7807_285[] = {
	0x49,
	0x9C,
};

static u8 M13_ILI7807_286[] = {
	0x4A,
	0x3F,
};

static u8 M13_ILI7807_287[] = {
	0x4B,
	0x9E,
};

static u8 M13_ILI7807_288[] = {
	0xFF,
	0x78, 0x07, 0x0E,
};

static u8 M13_ILI7807_289[] = {
	0x00,
	0xA3,
};

static u8 M13_ILI7807_290[] = {
	0x02,
	0x0F,
};

static u8 M13_ILI7807_291[] = {
	0x04,
	0x06,
};

static u8 M13_ILI7807_292[] = {
	0x13,
	0x04,
};

static u8 M13_ILI7807_293[] = {
	0xB0,
	0x21,
};

static u8 M13_ILI7807_294[] = {
	0xC0,
	0x12,
};

static u8 M13_ILI7807_295[] = {
	0x05,
	0x20,
};

static u8 M13_ILI7807_296[] = {
	0x41,
	0x14,
};

static u8 M13_ILI7807_297[] = {
	0x42,
	0x02,
};

static u8 M13_ILI7807_298[] = {
	0x43,
	0x14,
};

static u8 M13_ILI7807_299[] = {
	0x44,
	0x82,
};

static u8 M13_ILI7807_300[] = {
	0x40,
	0x07,
};

static u8 M13_ILI7807_301[] = {
	0x45,
	0x0B,
};

static u8 M13_ILI7807_302[] = {
	0x46,
	0x1C,
};

static u8 M13_ILI7807_303[] = {
	0x47,
	0x10,
};

static u8 M13_ILI7807_304[] = {
	0x49,
	0x2D,
};

static u8 M13_ILI7807_305[] = {
	0xB1,
	0x61,
};

static u8 M13_ILI7807_306[] = {
	0xC8,
	0x61,
};

static u8 M13_ILI7807_307[] = {
	0xC9,
	0x61,
};

static u8 M13_ILI7807_308[] = {
	0x4D,
	0xC0,
};

static u8 M13_ILI7807_309[] = {
	0x50,
	0x00,
};

static u8 M13_ILI7807_310[] = {
	0x4B,
	0x01,
};

static u8 M13_ILI7807_311[] = {
	0xE0,
	0x08,
};

static u8 M13_ILI7807_312[] = {
	0xE1,
	0x00,
};

static u8 M13_ILI7807_313[] = {
	0xE2,
	0x08,
};

static u8 M13_ILI7807_314[] = {
	0xE3,
	0x24,
};

static u8 M13_ILI7807_315[] = {
	0xE5,
	0x08,
};

static u8 M13_ILI7807_316[] = {
	0xFF,
	0x78, 0x07, 0x1E,
};

static u8 M13_ILI7807_317[] = {
	0xBD,
	0x01,
};

static u8 M13_ILI7807_318[] = {
	0xB1,
	0x1F,
};

static u8 M13_ILI7807_319[] = {
	0xC9,
	0x02,
};

static u8 M13_ILI7807_320[] = {
	0xC0,
	0x1F,
};

static u8 M13_ILI7807_321[] = {
	0x20,
	0x08,
};

static u8 M13_ILI7807_322[] = {
	0x21,
	0x00,
};

static u8 M13_ILI7807_323[] = {
	0x22,
	0x06,
};

static u8 M13_ILI7807_324[] = {
	0x23,
	0x20,
};

static u8 M13_ILI7807_325[] = {
	0x24,
	0x05,
};

static u8 M13_ILI7807_326[] = {
	0x61,
	0x1E,
};

static u8 M13_ILI7807_327[] = {
	0x62,
	0x03,
};

static u8 M13_ILI7807_328[] = {
	0x63,
	0x1E,
};

static u8 M13_ILI7807_329[] = {
	0x64,
	0x83,
};

static u8 M13_ILI7807_330[] = {
	0x60,
	0x07,
};

static u8 M13_ILI7807_331[] = {
	0x65,
	0x0A,
};

static u8 M13_ILI7807_332[] = {
	0x66,
	0xCF,
};

static u8 M13_ILI7807_333[] = {
	0x67,
	0x10,
};

static u8 M13_ILI7807_334[] = {
	0x69,
	0x2D,
};

static u8 M13_ILI7807_335[] = {
	0x16,
	0x3E,
};

static u8 M13_ILI7807_336[] = {
	0x1E,
	0x3E,
};

static u8 M13_ILI7807_337[] = {
	0x1F,
	0x3E,
};

static u8 M13_ILI7807_338[] = {
	0x6D,
	0x7A,
};

static u8 M13_ILI7807_339[] = {
	0x70,
	0x00,
};

static u8 M13_ILI7807_340[] = {
	0x6B,
	0x05,
};

static u8 M13_ILI7807_341[] = {
	0xFF,
	0x78, 0x07, 0x00,
};

static u8 M13_ILI7807_342[] = {
	0x35,
	0x00,
};

static u8 M13_ILI7807_343[] = {
	0x51,
	0x0F, 0xF0,
};

static u8 M13_ILI7807_344[] = {
	0x53,
	0x2C,
};

static u8 M13_ILI7807_345[] = {
	0x55,
	0x01,
};

static DEFINE_STATIC_PACKET(m13_sleep_out, DSI_PKT_TYPE_WR, M13_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(m13_sleep_in, DSI_PKT_TYPE_WR, M13_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(m13_display_on, DSI_PKT_TYPE_WR, M13_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(m13_display_off, DSI_PKT_TYPE_WR, M13_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(m13_brightness_mode, DSI_PKT_TYPE_WR, M13_BRIGHTNESS_MODE, 0);

static DEFINE_PKTUI(m13_brightness, &m13_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(m13_brightness, DSI_PKT_TYPE_WR, M13_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(m13_ili7807_boe_001, DSI_PKT_TYPE_WR, M13_ILI7807_001, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_002, DSI_PKT_TYPE_WR, M13_ILI7807_002, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_003, DSI_PKT_TYPE_WR, M13_ILI7807_003, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_004, DSI_PKT_TYPE_WR, M13_ILI7807_004, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_005, DSI_PKT_TYPE_WR, M13_ILI7807_005, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_006, DSI_PKT_TYPE_WR, M13_ILI7807_006, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_007, DSI_PKT_TYPE_WR, M13_ILI7807_007, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_008, DSI_PKT_TYPE_WR, M13_ILI7807_008, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_009, DSI_PKT_TYPE_WR, M13_ILI7807_009, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_010, DSI_PKT_TYPE_WR, M13_ILI7807_010, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_011, DSI_PKT_TYPE_WR, M13_ILI7807_011, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_012, DSI_PKT_TYPE_WR, M13_ILI7807_012, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_013, DSI_PKT_TYPE_WR, M13_ILI7807_013, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_014, DSI_PKT_TYPE_WR, M13_ILI7807_014, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_015, DSI_PKT_TYPE_WR, M13_ILI7807_015, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_016, DSI_PKT_TYPE_WR, M13_ILI7807_016, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_017, DSI_PKT_TYPE_WR, M13_ILI7807_017, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_018, DSI_PKT_TYPE_WR, M13_ILI7807_018, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_019, DSI_PKT_TYPE_WR, M13_ILI7807_019, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_020, DSI_PKT_TYPE_WR, M13_ILI7807_020, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_021, DSI_PKT_TYPE_WR, M13_ILI7807_021, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_022, DSI_PKT_TYPE_WR, M13_ILI7807_022, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_023, DSI_PKT_TYPE_WR, M13_ILI7807_023, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_024, DSI_PKT_TYPE_WR, M13_ILI7807_024, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_025, DSI_PKT_TYPE_WR, M13_ILI7807_025, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_026, DSI_PKT_TYPE_WR, M13_ILI7807_026, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_027, DSI_PKT_TYPE_WR, M13_ILI7807_027, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_028, DSI_PKT_TYPE_WR, M13_ILI7807_028, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_029, DSI_PKT_TYPE_WR, M13_ILI7807_029, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_030, DSI_PKT_TYPE_WR, M13_ILI7807_030, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_031, DSI_PKT_TYPE_WR, M13_ILI7807_031, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_032, DSI_PKT_TYPE_WR, M13_ILI7807_032, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_033, DSI_PKT_TYPE_WR, M13_ILI7807_033, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_034, DSI_PKT_TYPE_WR, M13_ILI7807_034, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_035, DSI_PKT_TYPE_WR, M13_ILI7807_035, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_036, DSI_PKT_TYPE_WR, M13_ILI7807_036, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_037, DSI_PKT_TYPE_WR, M13_ILI7807_037, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_038, DSI_PKT_TYPE_WR, M13_ILI7807_038, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_039, DSI_PKT_TYPE_WR, M13_ILI7807_039, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_040, DSI_PKT_TYPE_WR, M13_ILI7807_040, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_041, DSI_PKT_TYPE_WR, M13_ILI7807_041, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_042, DSI_PKT_TYPE_WR, M13_ILI7807_042, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_043, DSI_PKT_TYPE_WR, M13_ILI7807_043, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_044, DSI_PKT_TYPE_WR, M13_ILI7807_044, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_045, DSI_PKT_TYPE_WR, M13_ILI7807_045, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_046, DSI_PKT_TYPE_WR, M13_ILI7807_046, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_047, DSI_PKT_TYPE_WR, M13_ILI7807_047, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_048, DSI_PKT_TYPE_WR, M13_ILI7807_048, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_049, DSI_PKT_TYPE_WR, M13_ILI7807_049, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_050, DSI_PKT_TYPE_WR, M13_ILI7807_050, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_051, DSI_PKT_TYPE_WR, M13_ILI7807_051, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_052, DSI_PKT_TYPE_WR, M13_ILI7807_052, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_053, DSI_PKT_TYPE_WR, M13_ILI7807_053, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_054, DSI_PKT_TYPE_WR, M13_ILI7807_054, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_055, DSI_PKT_TYPE_WR, M13_ILI7807_055, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_056, DSI_PKT_TYPE_WR, M13_ILI7807_056, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_057, DSI_PKT_TYPE_WR, M13_ILI7807_057, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_058, DSI_PKT_TYPE_WR, M13_ILI7807_058, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_059, DSI_PKT_TYPE_WR, M13_ILI7807_059, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_060, DSI_PKT_TYPE_WR, M13_ILI7807_060, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_061, DSI_PKT_TYPE_WR, M13_ILI7807_061, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_062, DSI_PKT_TYPE_WR, M13_ILI7807_062, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_063, DSI_PKT_TYPE_WR, M13_ILI7807_063, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_064, DSI_PKT_TYPE_WR, M13_ILI7807_064, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_065, DSI_PKT_TYPE_WR, M13_ILI7807_065, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_066, DSI_PKT_TYPE_WR, M13_ILI7807_066, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_067, DSI_PKT_TYPE_WR, M13_ILI7807_067, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_068, DSI_PKT_TYPE_WR, M13_ILI7807_068, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_069, DSI_PKT_TYPE_WR, M13_ILI7807_069, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_070, DSI_PKT_TYPE_WR, M13_ILI7807_070, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_071, DSI_PKT_TYPE_WR, M13_ILI7807_071, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_072, DSI_PKT_TYPE_WR, M13_ILI7807_072, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_073, DSI_PKT_TYPE_WR, M13_ILI7807_073, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_074, DSI_PKT_TYPE_WR, M13_ILI7807_074, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_075, DSI_PKT_TYPE_WR, M13_ILI7807_075, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_076, DSI_PKT_TYPE_WR, M13_ILI7807_076, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_077, DSI_PKT_TYPE_WR, M13_ILI7807_077, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_078, DSI_PKT_TYPE_WR, M13_ILI7807_078, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_079, DSI_PKT_TYPE_WR, M13_ILI7807_079, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_080, DSI_PKT_TYPE_WR, M13_ILI7807_080, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_081, DSI_PKT_TYPE_WR, M13_ILI7807_081, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_082, DSI_PKT_TYPE_WR, M13_ILI7807_082, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_083, DSI_PKT_TYPE_WR, M13_ILI7807_083, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_084, DSI_PKT_TYPE_WR, M13_ILI7807_084, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_085, DSI_PKT_TYPE_WR, M13_ILI7807_085, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_086, DSI_PKT_TYPE_WR, M13_ILI7807_086, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_087, DSI_PKT_TYPE_WR, M13_ILI7807_087, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_088, DSI_PKT_TYPE_WR, M13_ILI7807_088, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_089, DSI_PKT_TYPE_WR, M13_ILI7807_089, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_090, DSI_PKT_TYPE_WR, M13_ILI7807_090, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_091, DSI_PKT_TYPE_WR, M13_ILI7807_091, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_092, DSI_PKT_TYPE_WR, M13_ILI7807_092, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_093, DSI_PKT_TYPE_WR, M13_ILI7807_093, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_094, DSI_PKT_TYPE_WR, M13_ILI7807_094, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_095, DSI_PKT_TYPE_WR, M13_ILI7807_095, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_096, DSI_PKT_TYPE_WR, M13_ILI7807_096, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_097, DSI_PKT_TYPE_WR, M13_ILI7807_097, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_098, DSI_PKT_TYPE_WR, M13_ILI7807_098, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_099, DSI_PKT_TYPE_WR, M13_ILI7807_099, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_100, DSI_PKT_TYPE_WR, M13_ILI7807_100, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_101, DSI_PKT_TYPE_WR, M13_ILI7807_101, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_102, DSI_PKT_TYPE_WR, M13_ILI7807_102, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_103, DSI_PKT_TYPE_WR, M13_ILI7807_103, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_104, DSI_PKT_TYPE_WR, M13_ILI7807_104, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_105, DSI_PKT_TYPE_WR, M13_ILI7807_105, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_106, DSI_PKT_TYPE_WR, M13_ILI7807_106, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_107, DSI_PKT_TYPE_WR, M13_ILI7807_107, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_108, DSI_PKT_TYPE_WR, M13_ILI7807_108, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_109, DSI_PKT_TYPE_WR, M13_ILI7807_109, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_110, DSI_PKT_TYPE_WR, M13_ILI7807_110, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_111, DSI_PKT_TYPE_WR, M13_ILI7807_111, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_112, DSI_PKT_TYPE_WR, M13_ILI7807_112, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_113, DSI_PKT_TYPE_WR, M13_ILI7807_113, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_114, DSI_PKT_TYPE_WR, M13_ILI7807_114, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_115, DSI_PKT_TYPE_WR, M13_ILI7807_115, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_116, DSI_PKT_TYPE_WR, M13_ILI7807_116, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_117, DSI_PKT_TYPE_WR, M13_ILI7807_117, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_118, DSI_PKT_TYPE_WR, M13_ILI7807_118, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_119, DSI_PKT_TYPE_WR, M13_ILI7807_119, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_120, DSI_PKT_TYPE_WR, M13_ILI7807_120, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_121, DSI_PKT_TYPE_WR, M13_ILI7807_121, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_122, DSI_PKT_TYPE_WR, M13_ILI7807_122, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_123, DSI_PKT_TYPE_WR, M13_ILI7807_123, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_124, DSI_PKT_TYPE_WR, M13_ILI7807_124, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_125, DSI_PKT_TYPE_WR, M13_ILI7807_125, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_126, DSI_PKT_TYPE_WR, M13_ILI7807_126, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_127, DSI_PKT_TYPE_WR, M13_ILI7807_127, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_128, DSI_PKT_TYPE_WR, M13_ILI7807_128, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_129, DSI_PKT_TYPE_WR, M13_ILI7807_129, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_130, DSI_PKT_TYPE_WR, M13_ILI7807_130, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_131, DSI_PKT_TYPE_WR, M13_ILI7807_131, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_132, DSI_PKT_TYPE_WR, M13_ILI7807_132, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_133, DSI_PKT_TYPE_WR, M13_ILI7807_133, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_134, DSI_PKT_TYPE_WR, M13_ILI7807_134, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_135, DSI_PKT_TYPE_WR, M13_ILI7807_135, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_136, DSI_PKT_TYPE_WR, M13_ILI7807_136, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_137, DSI_PKT_TYPE_WR, M13_ILI7807_137, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_138, DSI_PKT_TYPE_WR, M13_ILI7807_138, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_139, DSI_PKT_TYPE_WR, M13_ILI7807_139, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_140, DSI_PKT_TYPE_WR, M13_ILI7807_140, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_141, DSI_PKT_TYPE_WR, M13_ILI7807_141, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_142, DSI_PKT_TYPE_WR, M13_ILI7807_142, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_143, DSI_PKT_TYPE_WR, M13_ILI7807_143, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_144, DSI_PKT_TYPE_WR, M13_ILI7807_144, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_145, DSI_PKT_TYPE_WR, M13_ILI7807_145, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_146, DSI_PKT_TYPE_WR, M13_ILI7807_146, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_147, DSI_PKT_TYPE_WR, M13_ILI7807_147, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_148, DSI_PKT_TYPE_WR, M13_ILI7807_148, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_149, DSI_PKT_TYPE_WR, M13_ILI7807_149, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_150, DSI_PKT_TYPE_WR, M13_ILI7807_150, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_151, DSI_PKT_TYPE_WR, M13_ILI7807_151, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_152, DSI_PKT_TYPE_WR, M13_ILI7807_152, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_153, DSI_PKT_TYPE_WR, M13_ILI7807_153, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_154, DSI_PKT_TYPE_WR, M13_ILI7807_154, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_155, DSI_PKT_TYPE_WR, M13_ILI7807_155, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_156, DSI_PKT_TYPE_WR, M13_ILI7807_156, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_157, DSI_PKT_TYPE_WR, M13_ILI7807_157, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_158, DSI_PKT_TYPE_WR, M13_ILI7807_158, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_159, DSI_PKT_TYPE_WR, M13_ILI7807_159, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_160, DSI_PKT_TYPE_WR, M13_ILI7807_160, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_161, DSI_PKT_TYPE_WR, M13_ILI7807_161, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_162, DSI_PKT_TYPE_WR, M13_ILI7807_162, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_163, DSI_PKT_TYPE_WR, M13_ILI7807_163, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_164, DSI_PKT_TYPE_WR, M13_ILI7807_164, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_165, DSI_PKT_TYPE_WR, M13_ILI7807_165, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_166, DSI_PKT_TYPE_WR, M13_ILI7807_166, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_167, DSI_PKT_TYPE_WR, M13_ILI7807_167, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_168, DSI_PKT_TYPE_WR, M13_ILI7807_168, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_169, DSI_PKT_TYPE_WR, M13_ILI7807_169, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_170, DSI_PKT_TYPE_WR, M13_ILI7807_170, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_171, DSI_PKT_TYPE_WR, M13_ILI7807_171, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_172, DSI_PKT_TYPE_WR, M13_ILI7807_172, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_173, DSI_PKT_TYPE_WR, M13_ILI7807_173, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_174, DSI_PKT_TYPE_WR, M13_ILI7807_174, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_175, DSI_PKT_TYPE_WR, M13_ILI7807_175, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_176, DSI_PKT_TYPE_WR, M13_ILI7807_176, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_177, DSI_PKT_TYPE_WR, M13_ILI7807_177, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_178, DSI_PKT_TYPE_WR, M13_ILI7807_178, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_179, DSI_PKT_TYPE_WR, M13_ILI7807_179, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_180, DSI_PKT_TYPE_WR, M13_ILI7807_180, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_181, DSI_PKT_TYPE_WR, M13_ILI7807_181, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_182, DSI_PKT_TYPE_WR, M13_ILI7807_182, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_183, DSI_PKT_TYPE_WR, M13_ILI7807_183, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_184, DSI_PKT_TYPE_WR, M13_ILI7807_184, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_185, DSI_PKT_TYPE_WR, M13_ILI7807_185, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_186, DSI_PKT_TYPE_WR, M13_ILI7807_186, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_187, DSI_PKT_TYPE_WR, M13_ILI7807_187, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_188, DSI_PKT_TYPE_WR, M13_ILI7807_188, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_189, DSI_PKT_TYPE_WR, M13_ILI7807_189, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_190, DSI_PKT_TYPE_WR, M13_ILI7807_190, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_191, DSI_PKT_TYPE_WR, M13_ILI7807_191, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_192, DSI_PKT_TYPE_WR, M13_ILI7807_192, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_193, DSI_PKT_TYPE_WR, M13_ILI7807_193, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_194, DSI_PKT_TYPE_WR, M13_ILI7807_194, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_195, DSI_PKT_TYPE_WR, M13_ILI7807_195, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_196, DSI_PKT_TYPE_WR, M13_ILI7807_196, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_197, DSI_PKT_TYPE_WR, M13_ILI7807_197, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_198, DSI_PKT_TYPE_WR, M13_ILI7807_198, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_199, DSI_PKT_TYPE_WR, M13_ILI7807_199, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_200, DSI_PKT_TYPE_WR, M13_ILI7807_200, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_201, DSI_PKT_TYPE_WR, M13_ILI7807_201, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_202, DSI_PKT_TYPE_WR, M13_ILI7807_202, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_203, DSI_PKT_TYPE_WR, M13_ILI7807_203, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_204, DSI_PKT_TYPE_WR, M13_ILI7807_204, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_205, DSI_PKT_TYPE_WR, M13_ILI7807_205, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_206, DSI_PKT_TYPE_WR, M13_ILI7807_206, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_207, DSI_PKT_TYPE_WR, M13_ILI7807_207, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_208, DSI_PKT_TYPE_WR, M13_ILI7807_208, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_209, DSI_PKT_TYPE_WR, M13_ILI7807_209, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_210, DSI_PKT_TYPE_WR, M13_ILI7807_210, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_211, DSI_PKT_TYPE_WR, M13_ILI7807_211, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_212, DSI_PKT_TYPE_WR, M13_ILI7807_212, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_213, DSI_PKT_TYPE_WR, M13_ILI7807_213, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_214, DSI_PKT_TYPE_WR, M13_ILI7807_214, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_215, DSI_PKT_TYPE_WR, M13_ILI7807_215, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_216, DSI_PKT_TYPE_WR, M13_ILI7807_216, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_217, DSI_PKT_TYPE_WR, M13_ILI7807_217, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_218, DSI_PKT_TYPE_WR, M13_ILI7807_218, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_219, DSI_PKT_TYPE_WR, M13_ILI7807_219, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_220, DSI_PKT_TYPE_WR, M13_ILI7807_220, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_221, DSI_PKT_TYPE_WR, M13_ILI7807_221, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_222, DSI_PKT_TYPE_WR, M13_ILI7807_222, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_223, DSI_PKT_TYPE_WR, M13_ILI7807_223, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_224, DSI_PKT_TYPE_WR, M13_ILI7807_224, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_225, DSI_PKT_TYPE_WR, M13_ILI7807_225, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_226, DSI_PKT_TYPE_WR, M13_ILI7807_226, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_227, DSI_PKT_TYPE_WR, M13_ILI7807_227, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_228, DSI_PKT_TYPE_WR, M13_ILI7807_228, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_229, DSI_PKT_TYPE_WR, M13_ILI7807_229, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_230, DSI_PKT_TYPE_WR, M13_ILI7807_230, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_231, DSI_PKT_TYPE_WR, M13_ILI7807_231, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_232, DSI_PKT_TYPE_WR, M13_ILI7807_232, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_233, DSI_PKT_TYPE_WR, M13_ILI7807_233, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_234, DSI_PKT_TYPE_WR, M13_ILI7807_234, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_235, DSI_PKT_TYPE_WR, M13_ILI7807_235, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_236, DSI_PKT_TYPE_WR, M13_ILI7807_236, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_237, DSI_PKT_TYPE_WR, M13_ILI7807_237, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_238, DSI_PKT_TYPE_WR, M13_ILI7807_238, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_239, DSI_PKT_TYPE_WR, M13_ILI7807_239, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_240, DSI_PKT_TYPE_WR, M13_ILI7807_240, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_241, DSI_PKT_TYPE_WR, M13_ILI7807_241, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_242, DSI_PKT_TYPE_WR, M13_ILI7807_242, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_243, DSI_PKT_TYPE_WR, M13_ILI7807_243, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_244, DSI_PKT_TYPE_WR, M13_ILI7807_244, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_245, DSI_PKT_TYPE_WR, M13_ILI7807_245, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_246, DSI_PKT_TYPE_WR, M13_ILI7807_246, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_247, DSI_PKT_TYPE_WR, M13_ILI7807_247, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_248, DSI_PKT_TYPE_WR, M13_ILI7807_248, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_249, DSI_PKT_TYPE_WR, M13_ILI7807_249, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_250, DSI_PKT_TYPE_WR, M13_ILI7807_250, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_251, DSI_PKT_TYPE_WR, M13_ILI7807_251, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_252, DSI_PKT_TYPE_WR, M13_ILI7807_252, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_253, DSI_PKT_TYPE_WR, M13_ILI7807_253, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_254, DSI_PKT_TYPE_WR, M13_ILI7807_254, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_255, DSI_PKT_TYPE_WR, M13_ILI7807_255, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_256, DSI_PKT_TYPE_WR, M13_ILI7807_256, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_257, DSI_PKT_TYPE_WR, M13_ILI7807_257, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_258, DSI_PKT_TYPE_WR, M13_ILI7807_258, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_259, DSI_PKT_TYPE_WR, M13_ILI7807_259, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_260, DSI_PKT_TYPE_WR, M13_ILI7807_260, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_261, DSI_PKT_TYPE_WR, M13_ILI7807_261, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_262, DSI_PKT_TYPE_WR, M13_ILI7807_262, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_263, DSI_PKT_TYPE_WR, M13_ILI7807_263, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_264, DSI_PKT_TYPE_WR, M13_ILI7807_264, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_265, DSI_PKT_TYPE_WR, M13_ILI7807_265, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_266, DSI_PKT_TYPE_WR, M13_ILI7807_266, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_267, DSI_PKT_TYPE_WR, M13_ILI7807_267, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_268, DSI_PKT_TYPE_WR, M13_ILI7807_268, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_269, DSI_PKT_TYPE_WR, M13_ILI7807_269, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_270, DSI_PKT_TYPE_WR, M13_ILI7807_270, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_271, DSI_PKT_TYPE_WR, M13_ILI7807_271, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_272, DSI_PKT_TYPE_WR, M13_ILI7807_272, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_273, DSI_PKT_TYPE_WR, M13_ILI7807_273, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_274, DSI_PKT_TYPE_WR, M13_ILI7807_274, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_275, DSI_PKT_TYPE_WR, M13_ILI7807_275, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_276, DSI_PKT_TYPE_WR, M13_ILI7807_276, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_277, DSI_PKT_TYPE_WR, M13_ILI7807_277, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_278, DSI_PKT_TYPE_WR, M13_ILI7807_278, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_279, DSI_PKT_TYPE_WR, M13_ILI7807_279, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_280, DSI_PKT_TYPE_WR, M13_ILI7807_280, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_281, DSI_PKT_TYPE_WR, M13_ILI7807_281, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_282, DSI_PKT_TYPE_WR, M13_ILI7807_282, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_283, DSI_PKT_TYPE_WR, M13_ILI7807_283, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_284, DSI_PKT_TYPE_WR, M13_ILI7807_284, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_285, DSI_PKT_TYPE_WR, M13_ILI7807_285, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_286, DSI_PKT_TYPE_WR, M13_ILI7807_286, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_287, DSI_PKT_TYPE_WR, M13_ILI7807_287, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_288, DSI_PKT_TYPE_WR, M13_ILI7807_288, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_289, DSI_PKT_TYPE_WR, M13_ILI7807_289, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_290, DSI_PKT_TYPE_WR, M13_ILI7807_290, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_291, DSI_PKT_TYPE_WR, M13_ILI7807_291, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_292, DSI_PKT_TYPE_WR, M13_ILI7807_292, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_293, DSI_PKT_TYPE_WR, M13_ILI7807_293, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_294, DSI_PKT_TYPE_WR, M13_ILI7807_294, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_295, DSI_PKT_TYPE_WR, M13_ILI7807_295, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_296, DSI_PKT_TYPE_WR, M13_ILI7807_296, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_297, DSI_PKT_TYPE_WR, M13_ILI7807_297, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_298, DSI_PKT_TYPE_WR, M13_ILI7807_298, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_299, DSI_PKT_TYPE_WR, M13_ILI7807_299, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_300, DSI_PKT_TYPE_WR, M13_ILI7807_300, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_301, DSI_PKT_TYPE_WR, M13_ILI7807_301, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_302, DSI_PKT_TYPE_WR, M13_ILI7807_302, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_303, DSI_PKT_TYPE_WR, M13_ILI7807_303, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_304, DSI_PKT_TYPE_WR, M13_ILI7807_304, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_305, DSI_PKT_TYPE_WR, M13_ILI7807_305, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_306, DSI_PKT_TYPE_WR, M13_ILI7807_306, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_307, DSI_PKT_TYPE_WR, M13_ILI7807_307, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_308, DSI_PKT_TYPE_WR, M13_ILI7807_308, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_309, DSI_PKT_TYPE_WR, M13_ILI7807_309, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_310, DSI_PKT_TYPE_WR, M13_ILI7807_310, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_311, DSI_PKT_TYPE_WR, M13_ILI7807_311, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_312, DSI_PKT_TYPE_WR, M13_ILI7807_312, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_313, DSI_PKT_TYPE_WR, M13_ILI7807_313, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_314, DSI_PKT_TYPE_WR, M13_ILI7807_314, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_315, DSI_PKT_TYPE_WR, M13_ILI7807_315, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_316, DSI_PKT_TYPE_WR, M13_ILI7807_316, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_317, DSI_PKT_TYPE_WR, M13_ILI7807_317, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_318, DSI_PKT_TYPE_WR, M13_ILI7807_318, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_319, DSI_PKT_TYPE_WR, M13_ILI7807_319, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_320, DSI_PKT_TYPE_WR, M13_ILI7807_320, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_321, DSI_PKT_TYPE_WR, M13_ILI7807_321, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_322, DSI_PKT_TYPE_WR, M13_ILI7807_322, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_323, DSI_PKT_TYPE_WR, M13_ILI7807_323, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_324, DSI_PKT_TYPE_WR, M13_ILI7807_324, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_325, DSI_PKT_TYPE_WR, M13_ILI7807_325, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_326, DSI_PKT_TYPE_WR, M13_ILI7807_326, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_327, DSI_PKT_TYPE_WR, M13_ILI7807_327, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_328, DSI_PKT_TYPE_WR, M13_ILI7807_328, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_329, DSI_PKT_TYPE_WR, M13_ILI7807_329, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_330, DSI_PKT_TYPE_WR, M13_ILI7807_330, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_331, DSI_PKT_TYPE_WR, M13_ILI7807_331, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_332, DSI_PKT_TYPE_WR, M13_ILI7807_332, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_333, DSI_PKT_TYPE_WR, M13_ILI7807_333, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_334, DSI_PKT_TYPE_WR, M13_ILI7807_334, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_335, DSI_PKT_TYPE_WR, M13_ILI7807_335, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_336, DSI_PKT_TYPE_WR, M13_ILI7807_336, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_337, DSI_PKT_TYPE_WR, M13_ILI7807_337, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_338, DSI_PKT_TYPE_WR, M13_ILI7807_338, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_339, DSI_PKT_TYPE_WR, M13_ILI7807_339, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_340, DSI_PKT_TYPE_WR, M13_ILI7807_340, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_341, DSI_PKT_TYPE_WR, M13_ILI7807_341, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_342, DSI_PKT_TYPE_WR, M13_ILI7807_342, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_343, DSI_PKT_TYPE_WR, M13_ILI7807_343, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_344, DSI_PKT_TYPE_WR, M13_ILI7807_344, 0);
static DEFINE_STATIC_PACKET(m13_ili7807_boe_345, DSI_PKT_TYPE_WR, M13_ILI7807_345, 0);

static DEFINE_PANEL_MDELAY(m13_wait_display_off, 20);
static DEFINE_PANEL_MDELAY(m13_wait_2msec, 2);
static DEFINE_PANEL_MDELAY(m13_wait_50msec, 50);
static DEFINE_PANEL_MDELAY(m13_wait_60msec, 60);
static DEFINE_PANEL_MDELAY(m13_wait_blic_off, 1);


static void *m13_init_cmdtbl[] = {
	&ili7807_boe_restbl[RES_ID],
	&PKTINFO(m13_ili7807_boe_001),
	&PKTINFO(m13_ili7807_boe_002),
	&PKTINFO(m13_ili7807_boe_003),
	&PKTINFO(m13_ili7807_boe_004),
	&PKTINFO(m13_ili7807_boe_005),
	&PKTINFO(m13_ili7807_boe_006),
	&PKTINFO(m13_ili7807_boe_007),
	&PKTINFO(m13_ili7807_boe_008),
	&PKTINFO(m13_ili7807_boe_009),
	&PKTINFO(m13_ili7807_boe_010),
	&PKTINFO(m13_ili7807_boe_011),
	&PKTINFO(m13_ili7807_boe_012),
	&PKTINFO(m13_ili7807_boe_013),
	&PKTINFO(m13_ili7807_boe_014),
	&PKTINFO(m13_ili7807_boe_015),
	&PKTINFO(m13_ili7807_boe_016),
	&PKTINFO(m13_ili7807_boe_017),
	&PKTINFO(m13_ili7807_boe_018),
	&PKTINFO(m13_ili7807_boe_019),
	&PKTINFO(m13_ili7807_boe_020),
	&PKTINFO(m13_ili7807_boe_021),
	&PKTINFO(m13_ili7807_boe_022),
	&PKTINFO(m13_ili7807_boe_023),
	&PKTINFO(m13_ili7807_boe_024),
	&PKTINFO(m13_ili7807_boe_025),
	&PKTINFO(m13_ili7807_boe_026),
	&PKTINFO(m13_ili7807_boe_027),
	&PKTINFO(m13_ili7807_boe_028),
	&PKTINFO(m13_ili7807_boe_029),
	&PKTINFO(m13_ili7807_boe_030),
	&PKTINFO(m13_ili7807_boe_031),
	&PKTINFO(m13_ili7807_boe_032),
	&PKTINFO(m13_ili7807_boe_033),
	&PKTINFO(m13_ili7807_boe_034),
	&PKTINFO(m13_ili7807_boe_035),
	&PKTINFO(m13_ili7807_boe_036),
	&PKTINFO(m13_ili7807_boe_037),
	&PKTINFO(m13_ili7807_boe_038),
	&PKTINFO(m13_ili7807_boe_039),
	&PKTINFO(m13_ili7807_boe_040),
	&PKTINFO(m13_ili7807_boe_041),
	&PKTINFO(m13_ili7807_boe_042),
	&PKTINFO(m13_ili7807_boe_043),
	&PKTINFO(m13_ili7807_boe_044),
	&PKTINFO(m13_ili7807_boe_045),
	&PKTINFO(m13_ili7807_boe_046),
	&PKTINFO(m13_ili7807_boe_047),
	&PKTINFO(m13_ili7807_boe_048),
	&PKTINFO(m13_ili7807_boe_049),
	&PKTINFO(m13_ili7807_boe_050),
	&PKTINFO(m13_ili7807_boe_051),
	&PKTINFO(m13_ili7807_boe_052),
	&PKTINFO(m13_ili7807_boe_053),
	&PKTINFO(m13_ili7807_boe_054),
	&PKTINFO(m13_ili7807_boe_055),
	&PKTINFO(m13_ili7807_boe_056),
	&PKTINFO(m13_ili7807_boe_057),
	&PKTINFO(m13_ili7807_boe_058),
	&PKTINFO(m13_ili7807_boe_059),
	&PKTINFO(m13_ili7807_boe_060),
	&PKTINFO(m13_ili7807_boe_061),
	&PKTINFO(m13_ili7807_boe_062),
	&PKTINFO(m13_ili7807_boe_063),
	&PKTINFO(m13_ili7807_boe_064),
	&PKTINFO(m13_ili7807_boe_065),
	&PKTINFO(m13_ili7807_boe_066),
	&PKTINFO(m13_ili7807_boe_067),
	&PKTINFO(m13_ili7807_boe_068),
	&PKTINFO(m13_ili7807_boe_069),
	&PKTINFO(m13_ili7807_boe_070),
	&PKTINFO(m13_ili7807_boe_071),
	&PKTINFO(m13_ili7807_boe_072),
	&PKTINFO(m13_ili7807_boe_073),
	&PKTINFO(m13_ili7807_boe_074),
	&PKTINFO(m13_ili7807_boe_075),
	&PKTINFO(m13_ili7807_boe_076),
	&PKTINFO(m13_ili7807_boe_077),
	&PKTINFO(m13_ili7807_boe_078),
	&PKTINFO(m13_ili7807_boe_079),
	&PKTINFO(m13_ili7807_boe_080),
	&PKTINFO(m13_ili7807_boe_081),
	&PKTINFO(m13_ili7807_boe_082),
	&PKTINFO(m13_ili7807_boe_083),
	&PKTINFO(m13_ili7807_boe_084),
	&PKTINFO(m13_ili7807_boe_085),
	&PKTINFO(m13_ili7807_boe_086),
	&PKTINFO(m13_ili7807_boe_087),
	&PKTINFO(m13_ili7807_boe_088),
	&PKTINFO(m13_ili7807_boe_089),
	&PKTINFO(m13_ili7807_boe_090),
	&PKTINFO(m13_ili7807_boe_091),
	&PKTINFO(m13_ili7807_boe_092),
	&PKTINFO(m13_ili7807_boe_093),
	&PKTINFO(m13_ili7807_boe_094),
	&PKTINFO(m13_ili7807_boe_095),
	&PKTINFO(m13_ili7807_boe_096),
	&PKTINFO(m13_ili7807_boe_097),
	&PKTINFO(m13_ili7807_boe_098),
	&PKTINFO(m13_ili7807_boe_099),
	&PKTINFO(m13_ili7807_boe_100),
	&PKTINFO(m13_ili7807_boe_101),
	&PKTINFO(m13_ili7807_boe_102),
	&PKTINFO(m13_ili7807_boe_103),
	&PKTINFO(m13_ili7807_boe_104),
	&PKTINFO(m13_ili7807_boe_105),
	&PKTINFO(m13_ili7807_boe_106),
	&PKTINFO(m13_ili7807_boe_107),
	&PKTINFO(m13_ili7807_boe_108),
	&PKTINFO(m13_ili7807_boe_109),
	&PKTINFO(m13_ili7807_boe_110),
	&PKTINFO(m13_ili7807_boe_111),
	&PKTINFO(m13_ili7807_boe_112),
	&PKTINFO(m13_ili7807_boe_113),
	&PKTINFO(m13_ili7807_boe_114),
	&PKTINFO(m13_ili7807_boe_115),
	&PKTINFO(m13_ili7807_boe_116),
	&PKTINFO(m13_ili7807_boe_117),
	&PKTINFO(m13_ili7807_boe_118),
	&PKTINFO(m13_ili7807_boe_119),
	&PKTINFO(m13_ili7807_boe_120),
	&PKTINFO(m13_ili7807_boe_121),
	&PKTINFO(m13_ili7807_boe_122),
	&PKTINFO(m13_ili7807_boe_123),
	&PKTINFO(m13_ili7807_boe_124),
	&PKTINFO(m13_ili7807_boe_125),
	&PKTINFO(m13_ili7807_boe_126),
	&PKTINFO(m13_ili7807_boe_127),
	&PKTINFO(m13_ili7807_boe_128),
	&PKTINFO(m13_ili7807_boe_129),
	&PKTINFO(m13_ili7807_boe_130),
	&PKTINFO(m13_ili7807_boe_131),
	&PKTINFO(m13_ili7807_boe_132),
	&PKTINFO(m13_ili7807_boe_133),
	&PKTINFO(m13_ili7807_boe_134),
	&PKTINFO(m13_ili7807_boe_135),
	&PKTINFO(m13_ili7807_boe_136),
	&PKTINFO(m13_ili7807_boe_137),
	&PKTINFO(m13_ili7807_boe_138),
	&PKTINFO(m13_ili7807_boe_139),
	&PKTINFO(m13_ili7807_boe_140),
	&PKTINFO(m13_ili7807_boe_141),
	&PKTINFO(m13_ili7807_boe_142),
	&PKTINFO(m13_ili7807_boe_143),
	&PKTINFO(m13_ili7807_boe_144),
	&PKTINFO(m13_ili7807_boe_145),
	&PKTINFO(m13_ili7807_boe_146),
	&PKTINFO(m13_ili7807_boe_147),
	&PKTINFO(m13_ili7807_boe_148),
	&PKTINFO(m13_ili7807_boe_149),
	&PKTINFO(m13_ili7807_boe_150),
	&PKTINFO(m13_ili7807_boe_151),
	&PKTINFO(m13_ili7807_boe_152),
	&PKTINFO(m13_ili7807_boe_153),
	&PKTINFO(m13_ili7807_boe_154),
	&PKTINFO(m13_ili7807_boe_155),
	&PKTINFO(m13_ili7807_boe_156),
	&PKTINFO(m13_ili7807_boe_157),
	&PKTINFO(m13_ili7807_boe_158),
	&PKTINFO(m13_ili7807_boe_159),
	&PKTINFO(m13_ili7807_boe_160),
	&PKTINFO(m13_ili7807_boe_161),
	&PKTINFO(m13_ili7807_boe_162),
	&PKTINFO(m13_ili7807_boe_163),
	&PKTINFO(m13_ili7807_boe_164),
	&PKTINFO(m13_ili7807_boe_165),
	&PKTINFO(m13_ili7807_boe_166),
	&PKTINFO(m13_ili7807_boe_167),
	&PKTINFO(m13_ili7807_boe_168),
	&PKTINFO(m13_ili7807_boe_169),
	&PKTINFO(m13_ili7807_boe_170),
	&PKTINFO(m13_ili7807_boe_171),
	&PKTINFO(m13_ili7807_boe_172),
	&PKTINFO(m13_ili7807_boe_173),
	&PKTINFO(m13_ili7807_boe_174),
	&PKTINFO(m13_ili7807_boe_175),
	&PKTINFO(m13_ili7807_boe_176),
	&PKTINFO(m13_ili7807_boe_177),
	&PKTINFO(m13_ili7807_boe_178),
	&PKTINFO(m13_ili7807_boe_179),
	&PKTINFO(m13_ili7807_boe_180),
	&PKTINFO(m13_ili7807_boe_181),
	&PKTINFO(m13_ili7807_boe_182),
	&PKTINFO(m13_ili7807_boe_183),
	&PKTINFO(m13_ili7807_boe_184),
	&PKTINFO(m13_ili7807_boe_185),
	&PKTINFO(m13_ili7807_boe_186),
	&PKTINFO(m13_ili7807_boe_187),
	&PKTINFO(m13_ili7807_boe_188),
	&PKTINFO(m13_ili7807_boe_189),
	&PKTINFO(m13_ili7807_boe_190),
	&PKTINFO(m13_ili7807_boe_191),
	&PKTINFO(m13_ili7807_boe_192),
	&PKTINFO(m13_ili7807_boe_193),
	&PKTINFO(m13_ili7807_boe_194),
	&PKTINFO(m13_ili7807_boe_195),
	&PKTINFO(m13_ili7807_boe_196),
	&PKTINFO(m13_ili7807_boe_197),
	&PKTINFO(m13_ili7807_boe_198),
	&PKTINFO(m13_ili7807_boe_199),
	&PKTINFO(m13_ili7807_boe_200),
	&PKTINFO(m13_ili7807_boe_201),
	&PKTINFO(m13_ili7807_boe_202),
	&PKTINFO(m13_ili7807_boe_203),
	&PKTINFO(m13_ili7807_boe_204),
	&PKTINFO(m13_ili7807_boe_205),
	&PKTINFO(m13_ili7807_boe_206),
	&PKTINFO(m13_ili7807_boe_207),
	&PKTINFO(m13_ili7807_boe_208),
	&PKTINFO(m13_ili7807_boe_209),
	&PKTINFO(m13_ili7807_boe_210),
	&PKTINFO(m13_ili7807_boe_211),
	&PKTINFO(m13_ili7807_boe_212),
	&PKTINFO(m13_ili7807_boe_213),
	&PKTINFO(m13_ili7807_boe_214),
	&PKTINFO(m13_ili7807_boe_215),
	&PKTINFO(m13_ili7807_boe_216),
	&PKTINFO(m13_ili7807_boe_217),
	&PKTINFO(m13_ili7807_boe_218),
	&PKTINFO(m13_ili7807_boe_219),
	&PKTINFO(m13_ili7807_boe_220),
	&PKTINFO(m13_ili7807_boe_221),
	&PKTINFO(m13_ili7807_boe_222),
	&PKTINFO(m13_ili7807_boe_223),
	&PKTINFO(m13_ili7807_boe_224),
	&PKTINFO(m13_ili7807_boe_225),
	&PKTINFO(m13_ili7807_boe_226),
	&PKTINFO(m13_ili7807_boe_227),
	&PKTINFO(m13_ili7807_boe_228),
	&PKTINFO(m13_ili7807_boe_229),
	&PKTINFO(m13_ili7807_boe_230),
	&PKTINFO(m13_ili7807_boe_231),
	&PKTINFO(m13_ili7807_boe_232),
	&PKTINFO(m13_ili7807_boe_233),
	&PKTINFO(m13_ili7807_boe_234),
	&PKTINFO(m13_ili7807_boe_235),
	&PKTINFO(m13_ili7807_boe_236),
	&PKTINFO(m13_ili7807_boe_237),
	&PKTINFO(m13_ili7807_boe_238),
	&PKTINFO(m13_ili7807_boe_239),
	&PKTINFO(m13_ili7807_boe_240),
	&PKTINFO(m13_ili7807_boe_241),
	&PKTINFO(m13_ili7807_boe_242),
	&PKTINFO(m13_ili7807_boe_243),
	&PKTINFO(m13_ili7807_boe_244),
	&PKTINFO(m13_ili7807_boe_245),
	&PKTINFO(m13_ili7807_boe_246),
	&PKTINFO(m13_ili7807_boe_247),
	&PKTINFO(m13_ili7807_boe_248),
	&PKTINFO(m13_ili7807_boe_249),
	&PKTINFO(m13_ili7807_boe_250),
	&PKTINFO(m13_ili7807_boe_251),
	&PKTINFO(m13_ili7807_boe_252),
	&PKTINFO(m13_ili7807_boe_253),
	&PKTINFO(m13_ili7807_boe_254),
	&PKTINFO(m13_ili7807_boe_255),
	&PKTINFO(m13_ili7807_boe_256),
	&PKTINFO(m13_ili7807_boe_257),
	&PKTINFO(m13_ili7807_boe_258),
	&PKTINFO(m13_ili7807_boe_259),
	&PKTINFO(m13_ili7807_boe_260),
	&PKTINFO(m13_ili7807_boe_261),
	&PKTINFO(m13_ili7807_boe_262),
	&PKTINFO(m13_ili7807_boe_263),
	&PKTINFO(m13_ili7807_boe_264),
	&PKTINFO(m13_ili7807_boe_265),
	&PKTINFO(m13_ili7807_boe_266),
	&PKTINFO(m13_ili7807_boe_267),
	&PKTINFO(m13_ili7807_boe_268),
	&PKTINFO(m13_ili7807_boe_269),
	&PKTINFO(m13_ili7807_boe_270),
	&PKTINFO(m13_ili7807_boe_271),
	&PKTINFO(m13_ili7807_boe_272),
	&PKTINFO(m13_ili7807_boe_273),
	&PKTINFO(m13_ili7807_boe_274),
	&PKTINFO(m13_ili7807_boe_275),
	&PKTINFO(m13_ili7807_boe_276),
	&PKTINFO(m13_ili7807_boe_277),
	&PKTINFO(m13_ili7807_boe_278),
	&PKTINFO(m13_ili7807_boe_279),
	&PKTINFO(m13_ili7807_boe_280),
	&PKTINFO(m13_ili7807_boe_281),
	&PKTINFO(m13_ili7807_boe_282),
	&PKTINFO(m13_ili7807_boe_283),
	&PKTINFO(m13_ili7807_boe_284),
	&PKTINFO(m13_ili7807_boe_285),
	&PKTINFO(m13_ili7807_boe_286),
	&PKTINFO(m13_ili7807_boe_287),
	&PKTINFO(m13_ili7807_boe_288),
	&PKTINFO(m13_ili7807_boe_289),
	&PKTINFO(m13_ili7807_boe_290),
	&PKTINFO(m13_ili7807_boe_291),
	&PKTINFO(m13_ili7807_boe_292),
	&PKTINFO(m13_ili7807_boe_293),
	&PKTINFO(m13_ili7807_boe_294),
	&PKTINFO(m13_ili7807_boe_295),
	&PKTINFO(m13_ili7807_boe_296),
	&PKTINFO(m13_ili7807_boe_297),
	&PKTINFO(m13_ili7807_boe_298),
	&PKTINFO(m13_ili7807_boe_299),
	&PKTINFO(m13_ili7807_boe_300),
	&PKTINFO(m13_ili7807_boe_301),
	&PKTINFO(m13_ili7807_boe_302),
	&PKTINFO(m13_ili7807_boe_303),
	&PKTINFO(m13_ili7807_boe_304),
	&PKTINFO(m13_ili7807_boe_305),
	&PKTINFO(m13_ili7807_boe_306),
	&PKTINFO(m13_ili7807_boe_307),
	&PKTINFO(m13_ili7807_boe_308),
	&PKTINFO(m13_ili7807_boe_309),
	&PKTINFO(m13_ili7807_boe_310),
	&PKTINFO(m13_ili7807_boe_311),
	&PKTINFO(m13_ili7807_boe_312),
	&PKTINFO(m13_ili7807_boe_313),
	&PKTINFO(m13_ili7807_boe_314),
	&PKTINFO(m13_ili7807_boe_315),
	&PKTINFO(m13_ili7807_boe_316),
	&PKTINFO(m13_ili7807_boe_317),
	&PKTINFO(m13_ili7807_boe_318),
	&PKTINFO(m13_ili7807_boe_319),
	&PKTINFO(m13_ili7807_boe_320),
	&PKTINFO(m13_ili7807_boe_321),
	&PKTINFO(m13_ili7807_boe_322),
	&PKTINFO(m13_ili7807_boe_323),
	&PKTINFO(m13_ili7807_boe_324),
	&PKTINFO(m13_ili7807_boe_325),
	&PKTINFO(m13_ili7807_boe_326),
	&PKTINFO(m13_ili7807_boe_327),
	&PKTINFO(m13_ili7807_boe_328),
	&PKTINFO(m13_ili7807_boe_329),
	&PKTINFO(m13_ili7807_boe_330),
	&PKTINFO(m13_ili7807_boe_331),
	&PKTINFO(m13_ili7807_boe_332),
	&PKTINFO(m13_ili7807_boe_333),
	&PKTINFO(m13_ili7807_boe_334),
	&PKTINFO(m13_ili7807_boe_335),
	&PKTINFO(m13_ili7807_boe_336),
	&PKTINFO(m13_ili7807_boe_337),
	&PKTINFO(m13_ili7807_boe_338),
	&PKTINFO(m13_ili7807_boe_339),
	&PKTINFO(m13_ili7807_boe_340),
	&PKTINFO(m13_ili7807_boe_341),
	&PKTINFO(m13_ili7807_boe_342),
	&PKTINFO(m13_ili7807_boe_343),
	&PKTINFO(m13_ili7807_boe_344),
	&PKTINFO(m13_ili7807_boe_345),
	&PKTINFO(m13_sleep_out),
	&DLYINFO(m13_wait_60msec),
	&PKTINFO(m13_display_on),
};

static void *m13_res_init_cmdtbl[] = {
	&ili7807_boe_restbl[RES_ID],
};

static void *m13_set_bl_cmdtbl[] = {
	&PKTINFO(m13_brightness), //51h
};

static void *m13_display_on_cmdtbl[] = {
	&DLYINFO(m13_wait_50msec),
	&PKTINFO(m13_brightness_mode),
};

static void *m13_display_off_cmdtbl[] = {
	&PKTINFO(m13_display_off),
	&DLYINFO(m13_wait_display_off),
};

static void *m13_exit_cmdtbl[] = {
	&PKTINFO(m13_sleep_in),
};

/* ===================================================================================== */
/* ================================= [EA8076 I2C TABLE] ===========+++================= */
/* ===================================================================================== */

/* S2dps01 W/A */
/* PWM output is off: B/L mode should be I2C ctrl */
/* PWM output is on: B/L mode should be PWMI ctrl */

/* Indeed, below 2cmds should run after PWM on, but we turn on here. */


static u8 ILI7807_M13_I2C_INIT[] = {
	0x1C, 0x13,
	0x1D, 0xCF,
	0x1E, 0x70,
	0x1F, 0x0A,
	0x21, 0x0F,
	0x22, 0x00,
	0x23, 0x00,
	0x24, 0x01,
	0x25, 0x01,
	0x26, 0x02,
	0x11, 0xDF,
	0x20, 0x01,	/* Backlight Enable */
	0x24, 0x00,	/* Brightness Mode = PWMI */
};

static u8 ILI7807_M13_I2C_EXIT[] = {
	0x24, 0x01,	/* Brightness Mode = I2C*/
	0x20, 0x00,	/* Backlight Disable */
};

static u8 ILI7807_M13_I2C_DUMP[] = {
	0x1C, 0x00,
	0x1D, 0x00,
	0x1E, 0x00,
	0x1F, 0x00,
	0x21, 0x00,
	0x22, 0x00,
	0x23, 0x00,
	0x24, 0x00,
	0x25, 0x00,
	0x26, 0x00,
	0x11, 0x00,
	0x20, 0x00,
	0x24, 0x00,
};

static DEFINE_STATIC_PACKET(ili7807_boe_m13_i2c_init, I2C_PKT_TYPE_WR, ILI7807_M13_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(ili7807_boe_m13_i2c_exit, I2C_PKT_TYPE_WR, ILI7807_M13_I2C_EXIT, 0);
static DEFINE_STATIC_PACKET(ili7807_boe_m13_i2c_dump, I2C_PKT_TYPE_RD, ILI7807_M13_I2C_DUMP, 0);

static void *ili7807_boe_m13_init_cmdtbl[] = {
	&PKTINFO(ili7807_boe_m13_i2c_init),
};

static void *ili7807_boe_m13_exit_cmdtbl[] = {
	&PKTINFO(ili7807_boe_m13_i2c_exit),
};

static void *ili7807_boe_m13_dump_cmdtbl[] = {
	&PKTINFO(ili7807_boe_m13_i2c_dump),
};


static struct seqinfo m13_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", m13_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", m13_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", m13_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", m13_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", m13_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", m13_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", ili7807_boe_m13_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", ili7807_boe_m13_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", ili7807_boe_m13_dump_cmdtbl),
#endif
};

struct common_panel_info ili7807_boe_m13_default_panel_info = {
	.ldi_name = "ili7807_boe",
	.name = "ili7807_boe_m13_default",
	.model = "BOE_6_517_inch",
	.vendor = "BOE",
	.id = 0x4B6250,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(ili7807_boe_m13_resol),
		.resol = ili7807_boe_m13_resol,
	},
	.maptbl = m13_maptbl,
	.nr_maptbl = ARRAY_SIZE(m13_maptbl),
	.seqtbl = m13_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(m13_seqtbl),
	.rditbl = ili7807_boe_rditbl,
	.nr_rditbl = ARRAY_SIZE(ili7807_boe_rditbl),
	.restbl = ili7807_boe_restbl,
	.nr_restbl = ARRAY_SIZE(ili7807_boe_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&ili7807_boe_m13_panel_dimming_info,
	},
	.i2c_data = &ili7807_boe_m13_i2c_data,
};

static int __init ili7807_boe_m13_panel_init(void)
{
	register_common_panel(&ili7807_boe_m13_default_panel_info);

	return 0;
}
arch_initcall(ili7807_boe_m13_panel_init)
#endif /* __ILI7807_M13_PANEL_H__ */
