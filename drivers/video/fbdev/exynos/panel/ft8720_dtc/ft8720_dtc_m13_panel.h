/*
 * linux/drivers/video/fbdev/exynos/panel/ft8720_dtc/ft8720_dtc_m13_panel.h
 *
 * Header file for FT8720 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __FT8720_M13_PANEL_H__
#define __FT8720_M13_PANEL_H__
#include "../panel_drv.h"
#include "ft8720_dtc.h"

#include "ft8720_dtc_m13_panel_dimming.h"
#include "ft8720_dtc_m13_panel_i2c.h"

#include "ft8720_dtc_m13_resol.h"

#undef __pn_name__
#define __pn_name__	m13

#undef __PN_NAME__
#define __PN_NAME__	M13

static struct seqinfo m13_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [FT8720 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [FT8720 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [FT8720 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 m13_brt_table[FT8720_TOTAL_NR_LUMINANCE][1] = {
	{0},
	{1}, {1}, {2}, {2}, {3}, {3}, {4}, {5}, {5}, {6},	/* 1: 2 */
	{6}, {7}, {7}, {8}, {8}, {9}, {10}, {10}, {11}, {11},
	{12}, {12}, {13}, {13}, {14}, {14}, {15}, {15}, {16},
	{16}, {17}, {17}, {18}, {18}, {19}, {19}, {19}, {20},
	{20}, {21}, {21}, {22}, {22}, {23}, {23}, {24}, {24},
	{25}, {25}, {26}, {26}, {27}, {28}, {28}, {29}, {29},
	{30}, {31}, {31}, {32}, {32}, {33}, {33}, {34}, {34},
	{35}, {35}, {36}, {37}, {37}, {38}, {38}, {39}, {40},
	{41}, {42}, {43}, {43}, {44}, {45}, {45}, {46}, {47},
	{47}, {48}, {49}, {49}, {50}, {51}, {52}, {53}, {54},
	{55}, {56}, {57}, {57}, {58}, {58}, {59}, {60}, {60},
	{61}, {61}, {62}, {62}, {63}, {63}, {64}, {64}, {65},
	{65}, {66}, {66}, {67}, {67}, {67}, {68}, {68}, {69},
	{69}, {70}, {70}, {71}, {71}, {72}, {72}, {73}, {73},	/* 128: 73 */
	{74}, {75}, {76}, {77}, {78}, {79}, {80}, {81}, {82},
	{83}, {84}, {85}, {86}, {87}, {88}, {89}, {90}, {91},
	{92}, {93}, {94}, {95}, {96}, {97}, {98}, {99}, {100}, {101},
	{102}, {103}, {104}, {105}, {106}, {107}, {108}, {109}, {110}, {111},
	{112}, {113}, {114}, {115}, {116}, {117}, {118}, {119}, {119}, {120},
	{121}, {122}, {123}, {124}, {125}, {126}, {127}, {128}, {129}, {130},
	{131}, {132}, {133}, {134}, {135}, {136}, {137}, {138}, {138}, {140},
	{140}, {141}, {142}, {142}, {144}, {145}, {146}, {147}, {148}, {148},
	{149}, {150}, {151}, {152}, {153}, {154}, {155}, {156}, {156}, {157},
	{158}, {159}, {160}, {161}, {162}, {163}, {164}, {165}, {165}, {166},
	{167}, {168}, {169}, {170}, {171}, {172}, {173}, {173}, {174}, {175},
	{176}, {177}, {178}, {179}, {180}, {181}, {181}, {182}, {183}, {184},
	{185}, {186}, {187}, {188}, {189}, {190}, {190}, {191}, {192}, {193},	/* 255: 192 */
	{194}, {194}, {195}, {196}, {196}, {197}, {198}, {198}, {199}, {200},
	{200}, {201}, {202}, {202}, {203}, {204}, {204}, {205}, {206}, {206},
	{207}, {208}, {208}, {209}, {210}, {210}, {211}, {212}, {212}, {213},
	{214}, {214}, {215}, {216}, {216}, {217}, {218}, {218}, {219}, {220},
	{220}, {221}, {222}, {222}, {223}, {224}, {224}, {225}, {225}, {226},	/* 306: 226 */
};

/* S2DPS01 ONLY!!	Brightness Mode:	00: PWMI	01:I2C	*/
static u8 m13_blic_mode_table[FT8720_TOTAL_NR_LUMINANCE][1] = {
	[0] = { 0x01 },
	[1 ... FT8720_TOTAL_NR_LUMINANCE - 1] = { 0x00 },
};

static struct maptbl m13_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(m13_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
	[BLIC_MODE_MAPTBL] = DEFINE_2D_MAPTBL(m13_blic_mode_table, init_common_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [FT8720 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 M13_SLEEP_OUT[] = { 0x11 };
static u8 M13_SLEEP_IN[] = { 0x10 };
static u8 M13_DISPLAY_OFF[] = { 0x28 };
static u8 M13_DISPLAY_ON[] = { 0x29 };

static u8 M13_BRIGHTNESS[] = {
	0x51,
	0xFF,
};

static u8 M13_BRIGHTNESS_MODE[] = {
	0x53,
	0x2C,
};

static u8 M13_FT8720_001[] = {
	0x00,
	0x00,
};

static u8 M13_FT8720_002[] = {
	0xFF,
	0x87, 0x20, 0x01,
};

static u8 M13_FT8720_003[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_004[] = {
	0xFF,
	0x87, 0x20,
};

static u8 M13_FT8720_005[] = {
	0x00,
	0x00,
};

static u8 M13_FT8720_006[] = {
	0x2A,
	0x00, 0x00, 0x04, 0x37,
};

static u8 M13_FT8720_007[] = {
	0x00,
	0x00,
};

static u8 M13_FT8720_008[] = {
	0x2B,
	0x00, 0x00, 0x09, 0x67,
};

static u8 M13_FT8720_009[] = {
	0x00,
	0xA3,
};

static u8 M13_FT8720_010[] = {
	0xB3,
	0x09, 0x68, 0x00, 0x18,
};

static u8 M13_FT8720_011[] = {
	0x00,
	0x60,
};

static u8 M13_FT8720_012[] = {
	0xC0,
	0x00, 0x9E, 0x00, 0x31, 0x00, 0x11,
};

static u8 M13_FT8720_013[] = {
	0x00,
	0x70,
};

static u8 M13_FT8720_014[] = {
	0xC0,
	0x00, 0xC8, 0x00, 0xC8, 0x0D, 0x02, 0xB6,
};

static u8 M13_FT8720_015[] = {
	0x00,
	0x79,
};

static u8 M13_FT8720_016[] = {
	0xC0,
	0x15, 0x00, 0xF1,
};

static u8 M13_FT8720_017[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_018[] = {
	0xC0,
	0x00, 0x61, 0x00, 0x31, 0x00, 0x11,
};

static u8 M13_FT8720_019[] = {
	0x00,
	0x90,
};

static u8 M13_FT8720_020[] = {
	0xC0,
	0x00, 0x61, 0x00, 0x31, 0x00, 0x11,
};

static u8 M13_FT8720_021[] = {
	0x00,
	0xA0,
};

static u8 M13_FT8720_022[] = {
	0xC0,
	0x00, 0x92, 0x00, 0x31, 0x00, 0x11,
};

static u8 M13_FT8720_023[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_024[] = {
	0xC0,
	0x00, 0x93, 0x00, 0x31, 0x11,
};

static u8 M13_FT8720_025[] = {
	0x00,
	0xA3,
};

static u8 M13_FT8720_026[] = {
	0xC1,
	0x00, 0x46, 0x00, 0x28, 0x00, 0x02,
};

static u8 M13_FT8720_027[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_028[] = {
	0xCE,
	0x01, 0x81, 0xFF, 0xFF, 0x00, 0xC8, 0x00, 0xC8,
};

static u8 M13_FT8720_029[] = {
	0x00,
	0x8C,
};

static u8 M13_FT8720_030[] = {
	0xCE,
	0x00, 0x58, 0x00, 0x88,
};

static u8 M13_FT8720_031[] = {
	0x00,
	0x90,
};

static u8 M13_FT8720_032[] = {
	0xCE,
	0x00, 0xA7, 0x10, 0x43, 0x00, 0xA7, 0x80, 0xFF, 0xFF, 0x00,
	0x04, 0x00, 0x0C, 0x16,
};

static u8 M13_FT8720_033[] = {
	0x00,
	0xA0,
};

static u8 M13_FT8720_034[] = {
	0xCE,
	0x00, 0x00, 0x00,
};

static u8 M13_FT8720_035[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_036[] = {
	0xCE,
	0x22, 0x00, 0x00,
};

static u8 M13_FT8720_037[] = {
	0x00,
	0xE1,
};

static u8 M13_FT8720_038[] = {
	0xCE,
	0x0A, 0x02, 0xB6, 0x02, 0xB6,
};

static u8 M13_FT8720_039[] = {
	0x00,
	0xF1,
};

static u8 M13_FT8720_040[] = {
	0xCE,
	0x1F, 0x15,
};

static u8 M13_FT8720_041[] = {
	0x00,
	0xF4,
};

static u8 M13_FT8720_042[] = {
	0xCE,
	0x01, 0x2C, 0x00, 0xED,
};

static u8 M13_FT8720_043[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_044[] = {
	0xCF,
	0x00, 0x00, 0xB6, 0xBA,
};

static u8 M13_FT8720_045[] = {
	0x00,
	0xB5,
};

static u8 M13_FT8720_046[] = {
	0xCF,
	0x05, 0x05, 0x0D, 0x11,
};

static u8 M13_FT8720_047[] = {
	0x00,
	0xC0,
};

static u8 M13_FT8720_048[] = {
	0xCF,
	0x09, 0x09, 0x63, 0x67,
};

static u8 M13_FT8720_049[] = {
	0x00,
	0xC5,
};

static u8 M13_FT8720_050[] = {
	0xCF,
	0x09, 0x09, 0x69, 0x6D,
};

static u8 M13_FT8720_051[] = {
	0x00,
	0xF5,
};

static u8 M13_FT8720_052[] = {
	0xCF,
	0x00,
};

static u8 M13_FT8720_053[] = {
	0x00,
	0xD1,
};

static u8 M13_FT8720_054[] = {
	0xC1,
	0x08, 0x0E, 0x0B, 0x4D, 0x13, 0x6C, 0x05, 0x52, 0x07, 0x79,
	0x0C, 0xDB,
};

static u8 M13_FT8720_055[] = {
	0x00,
	0xE1,
};

static u8 M13_FT8720_056[] = {
	0xC1,
	0x0B, 0x4D,
};

static u8 M13_FT8720_057[] = {
	0x00,
	0xE4,
};

static u8 M13_FT8720_058[] = {
	0xCF,
	0x09, 0xAD, 0x09, 0xAC, 0x09, 0xAC, 0x09, 0xAC, 0x09, 0xAC,
	0x09, 0xAC,
};

static u8 M13_FT8720_059[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_060[] = {
	0xC1,
	0x00, 0x00,
};

static u8 M13_FT8720_061[] = {
	0x00,
	0x90,
};

static u8 M13_FT8720_062[] = {
	0xC1,
	0x03,
};

static u8 M13_FT8720_063[] = {
	0x00,
	0xCC,
};

static u8 M13_FT8720_064[] = {
	0xC1,
	0x18,
};

static u8 M13_FT8720_065[] = {
	0x00,
	0xE0,
};

static u8 M13_FT8720_066[] = {
	0xC1,
	0x00,
};

static u8 M13_FT8720_067[] = {
	0x00,
	0xF6,
};

static u8 M13_FT8720_068[] = {
	0xCF,
	0x3C,
};

static u8 M13_FT8720_069[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_070[] = {
	0xC2,
	0x82, 0x01, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00,
};

static u8 M13_FT8720_071[] = {
	0x00,
	0x90,
};

static u8 M13_FT8720_072[] = {
	0xC2,
	0x00, 0x00, 0x00, 0x00,
};

static u8 M13_FT8720_073[] = {
	0x00,
	0xA0,
};

static u8 M13_FT8720_074[] = {
	0xC2,
	0x00, 0x00, 0x00, 0x34, 0x91, 0x01, 0x00, 0x00, 0x34, 0x91,
	0x02, 0x00, 0x00, 0x34, 0x91,
};

static u8 M13_FT8720_075[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_076[] = {
	0xC2,
	0x03, 0x00, 0x00, 0x34, 0x91, 0x80, 0x08, 0x03, 0x02, 0x02,
};

static u8 M13_FT8720_077[] = {
	0x00,
	0xCA,
};

static u8 M13_FT8720_078[] = {
	0xC2,
	0x84, 0x08, 0x03, 0x01, 0x81,
};

static u8 M13_FT8720_079[] = {
	0x00,
	0xE0,
};

static u8 M13_FT8720_080[] = {
	0xC2,
	0x33, 0x33, 0x70, 0x00, 0x70,
};

static u8 M13_FT8720_081[] = {
	0x00,
	0xE8,
};

static u8 M13_FT8720_082[] = {
	0xC2,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 M13_FT8720_083[] = {
	0x00,
	0xD0,
};

static u8 M13_FT8720_084[] = {
	0xC3,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 M13_FT8720_085[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_086[] = {
	0xCB,
	0x00, 0x01, 0x00, 0x03, 0xFD, 0x01, 0x01, 0x00, 0x00, 0x00,
	0xFD, 0x01, 0x00, 0x03, 0x00, 0x00,
};

static u8 M13_FT8720_087[] = {
	0x00,
	0x90,
};

static u8 M13_FT8720_088[] = {
	0xCB,
	0x00, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 M13_FT8720_089[] = {
	0x00,
	0xA0,
};

static u8 M13_FT8720_090[] = {
	0xCB,
	0x00, 0x00, 0x00, 0x00,
};

static u8 M13_FT8720_091[] = {
	0x00,
	0xA4,
};

static u8 M13_FT8720_092[] = {
	0xCB,
	0x03, 0x00, 0x0C, 0x00,
};

static u8 M13_FT8720_093[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_094[] = {
	0xCB,
	0x13, 0x58, 0x05, 0x30,
};

static u8 M13_FT8720_095[] = {
	0x00,
	0xC0,
};

static u8 M13_FT8720_096[] = {
	0xCB,
	0x13, 0x58, 0x05, 0x30,
};

static u8 M13_FT8720_097[] = {
	0x00,
	0xD5,
};

static u8 M13_FT8720_098[] = {
	0xCB,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,
};

static u8 M13_FT8720_099[] = {
	0x00,
	0xE0,
};

static u8 M13_FT8720_100[] = {
	0xCB,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static u8 M13_FT8720_101[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_102[] = {
	0xCC,
	0x23, 0x12, 0x23, 0x1C, 0x23, 0x0A, 0x23, 0x23, 0x09, 0x08,
	0x07, 0x06, 0x23, 0x23, 0x23, 0x23,
};

static u8 M13_FT8720_103[] = {
	0x00,
	0x90,
};

static u8 M13_FT8720_104[] = {
	0xCC,
	0x23, 0x18, 0x16, 0x17, 0x23, 0x19, 0x1A, 0x1B,
};

static u8 M13_FT8720_105[] = {
	0x00,
	0xA0,
};

static u8 M13_FT8720_106[] = {
	0xCC,
	0x23, 0x12, 0x23, 0x1D, 0x23, 0x0E, 0x23, 0x23, 0x06, 0x07,
	0x08, 0x09, 0x23, 0x23, 0x23, 0x23,
};

static u8 M13_FT8720_107[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_108[] = {
	0xCC,
	0x23, 0x18, 0x16, 0x17, 0x23, 0x19, 0x1A, 0x1B,
};

static u8 M13_FT8720_109[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_110[] = {
	0xCD,
	0x23, 0x23, 0x23, 0x02, 0x23, 0x0A, 0x23, 0x23, 0x09, 0x08,
	0x07, 0x06, 0x23, 0x23, 0x23, 0x23,
};

static u8 M13_FT8720_111[] = {
	0x00,
	0x90,
};

static u8 M13_FT8720_112[] = {
	0xCD,
	0x23, 0x18, 0x16, 0x17, 0x23, 0x19, 0x1A, 0x1B,
};

static u8 M13_FT8720_113[] = {
	0x00,
	0xA0,
};

static u8 M13_FT8720_114[] = {
	0xCD,
	0x23, 0x23, 0x23, 0x02, 0x23, 0x0E, 0x23, 0x23, 0x06, 0x07,
	0x08, 0x09, 0x23, 0x23, 0x23, 0x23,
};

static u8 M13_FT8720_115[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_116[] = {
	0xCD,
	0x23, 0x18, 0x16, 0x17, 0x23, 0x19, 0x1A, 0x1B,
};

static u8 M13_FT8720_117[] = {
	0x00,
	0x86,
};

static u8 M13_FT8720_118[] = {
	0xC0,
	0x00, 0x00, 0x00, 0x01, 0x19, 0x05,
};

static u8 M13_FT8720_119[] = {
	0x00,
	0x96,
};

static u8 M13_FT8720_120[] = {
	0xC0,
	0x00, 0x00, 0x00, 0x01, 0x19, 0x05,
};

static u8 M13_FT8720_121[] = {
	0x00,
	0xA3,
};

static u8 M13_FT8720_122[] = {
	0xCE,
	0x00, 0x00, 0x00, 0x01, 0x2A, 0x07,
};

static u8 M13_FT8720_123[] = {
	0x00,
	0xB3,
};

static u8 M13_FT8720_124[] = {
	0xCE,
	0x00, 0x00, 0x00, 0x01, 0x2A, 0x07,
};

static u8 M13_FT8720_125[] = {
	0x00,
	0x66,
};

static u8 M13_FT8720_126[] = {
	0xC0,
	0x00, 0x00, 0x00, 0x01, 0x2A, 0x07,
};

static u8 M13_FT8720_127[] = {
	0x00,
	0x82,
};

static u8 M13_FT8720_128[] = {
	0xA7,
	0x10, 0x00,
};

static u8 M13_FT8720_129[] = {
	0x00,
	0x8D,
};

static u8 M13_FT8720_130[] = {
	0xA7,
	0x01,
};

static u8 M13_FT8720_131[] = {
	0x00,
	0x8F,
};

static u8 M13_FT8720_132[] = {
	0xA7,
	0x01,
};

static u8 M13_FT8720_133[] = {
	0x00,
	0x93,
};

static u8 M13_FT8720_134[] = {
	0xC5,
	0x37,
};

static u8 M13_FT8720_135[] = {
	0x00,
	0x97,
};

static u8 M13_FT8720_136[] = {
	0xC5,
	0x37,
};

static u8 M13_FT8720_137[] = {
	0x00,
	0x9A,
};

static u8 M13_FT8720_138[] = {
	0xC5,
	0x32,
};

static u8 M13_FT8720_139[] = {
	0x00,
	0x9C,
};

static u8 M13_FT8720_140[] = {
	0xC5,
	0x32,
};

static u8 M13_FT8720_141[] = {
	0x00,
	0xB6,
};

static u8 M13_FT8720_142[] = {
	0xC5,
	0x10, 0x10, 0x0E, 0x0E, 0x10, 0x10, 0x0E, 0x0E,
};

static u8 M13_FT8720_143[] = {
	0x00,
	0x88,
};

static u8 M13_FT8720_144[] = {
	0xC4,
	0x08,
};

static u8 M13_FT8720_145[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_146[] = {
	0xA7,
	0x03,
};

static u8 M13_FT8720_147[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_148[] = {
	0xC5,
	0xD1,
};

static u8 M13_FT8720_149[] = {
	0x00,
	0xB3,
};

static u8 M13_FT8720_150[] = {
	0xC5,
	0xD1,
};

static u8 M13_FT8720_151[] = {
	0x00,
	0x99,
};

static u8 M13_FT8720_152[] = {
	0xCF,
	0x50,
};

static u8 M13_FT8720_153[] = {
	0x00,
	0x8C,
};

static u8 M13_FT8720_154[] = {
	0xC3,
	0x00,
};

static u8 M13_FT8720_155[] = {
	0x00,
	0xA0,
};

static u8 M13_FT8720_156[] = {
	0xC3,
	0x35, 0x21,
};

static u8 M13_FT8720_157[] = {
	0x00,
	0xA4,
};

static u8 M13_FT8720_158[] = {
	0xC3,
	0x01, 0x20,
};

static u8 M13_FT8720_159[] = {
	0x00,
	0xAA,
};

static u8 M13_FT8720_160[] = {
	0xC3,
	0x21,
};

static u8 M13_FT8720_161[] = {
	0x00,
	0xAD,
};

static u8 M13_FT8720_162[] = {
	0xC3,
	0x01,
};

static u8 M13_FT8720_163[] = {
	0x00,
	0xAE,
};

static u8 M13_FT8720_164[] = {
	0xC3,
	0x20,
};

static u8 M13_FT8720_165[] = {
	0x00,
	0xB3,
};

static u8 M13_FT8720_166[] = {
	0xC3,
	0x21,
};

static u8 M13_FT8720_167[] = {
	0x00,
	0xB6,
};

static u8 M13_FT8720_168[] = {
	0xC3,
	0x01, 0x20,
};

static u8 M13_FT8720_169[] = {
	0x00,
	0xC3,
};

static u8 M13_FT8720_170[] = {
	0xC5,
	0xFF,
};

static u8 M13_FT8720_171[] = {
	0x00,
	0xA9,
};

static u8 M13_FT8720_172[] = {
	0xF5,
	0x8E,
};

static u8 M13_FT8720_173[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_174[] = {
	0xB3,
	0x00,
};

static u8 M13_FT8720_175[] = {
	0x00,
	0x83,
};

static u8 M13_FT8720_176[] = {
	0xB0,
	0x63,
};

static u8 M13_FT8720_177[] = {
	0x00,
	0x93,
};

static u8 M13_FT8720_178[] = {
	0xC4,
	0x08,
};

static u8 M13_FT8720_179[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_180[] = {
	0xB3,
	0x22,
};

static u8 M13_FT8720_181[] = {
	0x00,
	0x90,
};

static u8 M13_FT8720_182[] = {
	0xC3,
	0x08,
};

static u8 M13_FT8720_183[] = {
	0x00,
	0xFA,
};

static u8 M13_FT8720_184[] = {
	0xC2,
	0x26,
};

static u8 M13_FT8720_185[] = {
	0x00,
	0xCA,
};

static u8 M13_FT8720_186[] = {
	0xC0,
	0x80,
};

static u8 M13_FT8720_187[] = {
	0x00,
	0x82,
};

static u8 M13_FT8720_188[] = {
	0xF5,
	0x01,
};

static u8 M13_FT8720_189[] = {
	0x00,
	0x93,
};

static u8 M13_FT8720_190[] = {
	0xF5,
	0x01,
};

static u8 M13_FT8720_191[] = {
	0x00,
	0x9B,
};

static u8 M13_FT8720_192[] = {
	0xF5,
	0x49,
};

static u8 M13_FT8720_193[] = {
	0x00,
	0x9D,
};

static u8 M13_FT8720_194[] = {
	0xF5,
	0x49,
};

static u8 M13_FT8720_195[] = {
	0x00,
	0xBE,
};

static u8 M13_FT8720_196[] = {
	0xC5,
	0xF0, 0xF0,
};

static u8 M13_FT8720_197[] = {
	0x00,
	0x85,
};

static u8 M13_FT8720_198[] = {
	0xA7,
	0x01,
};

static u8 M13_FT8720_199[] = {
	0x00,
	0xDC,
};

static u8 M13_FT8720_200[] = {
	0xC3,
	0x37,
};

static u8 M13_FT8720_201[] = {
	0x00,
	0x8A,
};

static u8 M13_FT8720_202[] = {
	0xF5,
	0xC7,
};

static u8 M13_FT8720_203[] = {
	0x00,
	0x99,
};

static u8 M13_FT8720_204[] = {
	0xCF,
	0x50,
};

static u8 M13_FT8720_205[] = {
	0x00,
	0x9C,
};

static u8 M13_FT8720_206[] = {
	0xF5,
	0x00,
};

static u8 M13_FT8720_207[] = {
	0x00,
	0x9E,
};

static u8 M13_FT8720_208[] = {
	0xF5,
	0x00,
};

static u8 M13_FT8720_209[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_210[] = {
	0xC5,
	0xD0, 0x4A, 0x39, 0xD0, 0x4A, 0x0F,
};

static u8 M13_FT8720_211[] = {
	0x00,
	0xC2,
};

static u8 M13_FT8720_212[] = {
	0xF5,
	0x42,
};

static u8 M13_FT8720_213[] = {
	0x00,
	0xD4,
};

static u8 M13_FT8720_214[] = {
	0xCB,
	0x03,
};

static u8 M13_FT8720_215[] = {
	0x00,
	0xE8,
};

static u8 M13_FT8720_216[] = {
	0xC0,
	0x40,
};

static u8 M13_FT8720_217[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_218[] = {
	0xC5,
	0x88,
};

static u8 M13_FT8720_219[] = {
	0x1C,
	0x02,
};

static u8 M13_FT8720_220[] = {
	0x00,
	0xB0,
};

static u8 M13_FT8720_221[] = {
	0xCA,
	0x6B, 0x6B, 0x09,
};

static u8 M13_FT8720_222[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_223[] = {
	0xA4,
	0xCA,
};

static u8 M13_FT8720_224[] = {
	0x00,
	0x87,
};

static u8 M13_FT8720_225[] = {
	0xC5,
	0x0C,
};

static u8 M13_FT8720_226[] = {
	0x00,
	0x89,
};

static u8 M13_FT8720_227[] = {
	0xC5,
	0x0C, 0x0C, 0x0C,
};

static u8 M13_FT8720_228[] = {
	0x00,
	0x8C,
};

static u8 M13_FT8720_229[] = {
	0xC5,
	0x0C, 0x0C, 0x0C,
};

static u8 M13_FT8720_230[] = {
	0x00,
	0x84,
};

static u8 M13_FT8720_231[] = {
	0xB0,
	0xF8,
};

static u8 M13_FT8720_232[] = {
	0x00,
	0xB5,
};

static u8 M13_FT8720_233[] = {
	0xCA,
	0x00,
};

static u8 M13_FT8720_234[] = {
	0x00,
	0xA0,
};

static u8 M13_FT8720_235[] = {
	0xCA,
	0x08, 0x08, 0x08,
};

static u8 M13_FT8720_236[] = {
	0x00,
	0x80,
};

static u8 M13_FT8720_237[] = {
	0xCA,
	0xE6, 0xD1, 0xC1, 0xB4, 0xAB, 0xA2, 0x9B, 0x95, 0x8F, 0x8B,
	0x8D, 0x81,
};

static u8 M13_FT8720_238[] = {
	0x00,
	0x90,
};

static u8 M13_FT8720_239[] = {
	0xCA,
	0xFE, 0xFF, 0x66, 0xFC, 0xFF, 0xCC, 0xFA, 0xFF, 0x66,
};

static u8 M13_FT8720_240[] = {
	0x00,
	0x00,
};

static u8 M13_FT8720_241[] = {
	0xFF,
	0xFF, 0xFF, 0xFF,
};

static u8 M13_FT8720_242[] = {
	0x53,
	0x2C,
};

static u8 M13_FT8720_243[] = {
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

static DEFINE_STATIC_PACKET(m13_ft8720_dtc_001, DSI_PKT_TYPE_WR, M13_FT8720_001, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_002, DSI_PKT_TYPE_WR, M13_FT8720_002, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_003, DSI_PKT_TYPE_WR, M13_FT8720_003, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_004, DSI_PKT_TYPE_WR, M13_FT8720_004, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_005, DSI_PKT_TYPE_WR, M13_FT8720_005, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_006, DSI_PKT_TYPE_WR, M13_FT8720_006, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_007, DSI_PKT_TYPE_WR, M13_FT8720_007, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_008, DSI_PKT_TYPE_WR, M13_FT8720_008, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_009, DSI_PKT_TYPE_WR, M13_FT8720_009, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_010, DSI_PKT_TYPE_WR, M13_FT8720_010, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_011, DSI_PKT_TYPE_WR, M13_FT8720_011, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_012, DSI_PKT_TYPE_WR, M13_FT8720_012, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_013, DSI_PKT_TYPE_WR, M13_FT8720_013, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_014, DSI_PKT_TYPE_WR, M13_FT8720_014, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_015, DSI_PKT_TYPE_WR, M13_FT8720_015, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_016, DSI_PKT_TYPE_WR, M13_FT8720_016, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_017, DSI_PKT_TYPE_WR, M13_FT8720_017, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_018, DSI_PKT_TYPE_WR, M13_FT8720_018, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_019, DSI_PKT_TYPE_WR, M13_FT8720_019, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_020, DSI_PKT_TYPE_WR, M13_FT8720_020, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_021, DSI_PKT_TYPE_WR, M13_FT8720_021, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_022, DSI_PKT_TYPE_WR, M13_FT8720_022, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_023, DSI_PKT_TYPE_WR, M13_FT8720_023, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_024, DSI_PKT_TYPE_WR, M13_FT8720_024, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_025, DSI_PKT_TYPE_WR, M13_FT8720_025, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_026, DSI_PKT_TYPE_WR, M13_FT8720_026, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_027, DSI_PKT_TYPE_WR, M13_FT8720_027, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_028, DSI_PKT_TYPE_WR, M13_FT8720_028, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_029, DSI_PKT_TYPE_WR, M13_FT8720_029, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_030, DSI_PKT_TYPE_WR, M13_FT8720_030, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_031, DSI_PKT_TYPE_WR, M13_FT8720_031, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_032, DSI_PKT_TYPE_WR, M13_FT8720_032, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_033, DSI_PKT_TYPE_WR, M13_FT8720_033, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_034, DSI_PKT_TYPE_WR, M13_FT8720_034, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_035, DSI_PKT_TYPE_WR, M13_FT8720_035, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_036, DSI_PKT_TYPE_WR, M13_FT8720_036, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_037, DSI_PKT_TYPE_WR, M13_FT8720_037, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_038, DSI_PKT_TYPE_WR, M13_FT8720_038, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_039, DSI_PKT_TYPE_WR, M13_FT8720_039, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_040, DSI_PKT_TYPE_WR, M13_FT8720_040, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_041, DSI_PKT_TYPE_WR, M13_FT8720_041, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_042, DSI_PKT_TYPE_WR, M13_FT8720_042, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_043, DSI_PKT_TYPE_WR, M13_FT8720_043, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_044, DSI_PKT_TYPE_WR, M13_FT8720_044, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_045, DSI_PKT_TYPE_WR, M13_FT8720_045, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_046, DSI_PKT_TYPE_WR, M13_FT8720_046, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_047, DSI_PKT_TYPE_WR, M13_FT8720_047, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_048, DSI_PKT_TYPE_WR, M13_FT8720_048, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_049, DSI_PKT_TYPE_WR, M13_FT8720_049, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_050, DSI_PKT_TYPE_WR, M13_FT8720_050, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_051, DSI_PKT_TYPE_WR, M13_FT8720_051, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_052, DSI_PKT_TYPE_WR, M13_FT8720_052, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_053, DSI_PKT_TYPE_WR, M13_FT8720_053, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_054, DSI_PKT_TYPE_WR, M13_FT8720_054, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_055, DSI_PKT_TYPE_WR, M13_FT8720_055, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_056, DSI_PKT_TYPE_WR, M13_FT8720_056, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_057, DSI_PKT_TYPE_WR, M13_FT8720_057, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_058, DSI_PKT_TYPE_WR, M13_FT8720_058, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_059, DSI_PKT_TYPE_WR, M13_FT8720_059, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_060, DSI_PKT_TYPE_WR, M13_FT8720_060, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_061, DSI_PKT_TYPE_WR, M13_FT8720_061, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_062, DSI_PKT_TYPE_WR, M13_FT8720_062, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_063, DSI_PKT_TYPE_WR, M13_FT8720_063, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_064, DSI_PKT_TYPE_WR, M13_FT8720_064, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_065, DSI_PKT_TYPE_WR, M13_FT8720_065, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_066, DSI_PKT_TYPE_WR, M13_FT8720_066, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_067, DSI_PKT_TYPE_WR, M13_FT8720_067, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_068, DSI_PKT_TYPE_WR, M13_FT8720_068, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_069, DSI_PKT_TYPE_WR, M13_FT8720_069, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_070, DSI_PKT_TYPE_WR, M13_FT8720_070, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_071, DSI_PKT_TYPE_WR, M13_FT8720_071, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_072, DSI_PKT_TYPE_WR, M13_FT8720_072, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_073, DSI_PKT_TYPE_WR, M13_FT8720_073, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_074, DSI_PKT_TYPE_WR, M13_FT8720_074, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_075, DSI_PKT_TYPE_WR, M13_FT8720_075, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_076, DSI_PKT_TYPE_WR, M13_FT8720_076, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_077, DSI_PKT_TYPE_WR, M13_FT8720_077, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_078, DSI_PKT_TYPE_WR, M13_FT8720_078, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_079, DSI_PKT_TYPE_WR, M13_FT8720_079, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_080, DSI_PKT_TYPE_WR, M13_FT8720_080, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_081, DSI_PKT_TYPE_WR, M13_FT8720_081, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_082, DSI_PKT_TYPE_WR, M13_FT8720_082, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_083, DSI_PKT_TYPE_WR, M13_FT8720_083, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_084, DSI_PKT_TYPE_WR, M13_FT8720_084, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_085, DSI_PKT_TYPE_WR, M13_FT8720_085, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_086, DSI_PKT_TYPE_WR, M13_FT8720_086, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_087, DSI_PKT_TYPE_WR, M13_FT8720_087, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_088, DSI_PKT_TYPE_WR, M13_FT8720_088, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_089, DSI_PKT_TYPE_WR, M13_FT8720_089, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_090, DSI_PKT_TYPE_WR, M13_FT8720_090, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_091, DSI_PKT_TYPE_WR, M13_FT8720_091, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_092, DSI_PKT_TYPE_WR, M13_FT8720_092, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_093, DSI_PKT_TYPE_WR, M13_FT8720_093, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_094, DSI_PKT_TYPE_WR, M13_FT8720_094, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_095, DSI_PKT_TYPE_WR, M13_FT8720_095, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_096, DSI_PKT_TYPE_WR, M13_FT8720_096, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_097, DSI_PKT_TYPE_WR, M13_FT8720_097, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_098, DSI_PKT_TYPE_WR, M13_FT8720_098, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_099, DSI_PKT_TYPE_WR, M13_FT8720_099, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_100, DSI_PKT_TYPE_WR, M13_FT8720_100, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_101, DSI_PKT_TYPE_WR, M13_FT8720_101, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_102, DSI_PKT_TYPE_WR, M13_FT8720_102, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_103, DSI_PKT_TYPE_WR, M13_FT8720_103, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_104, DSI_PKT_TYPE_WR, M13_FT8720_104, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_105, DSI_PKT_TYPE_WR, M13_FT8720_105, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_106, DSI_PKT_TYPE_WR, M13_FT8720_106, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_107, DSI_PKT_TYPE_WR, M13_FT8720_107, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_108, DSI_PKT_TYPE_WR, M13_FT8720_108, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_109, DSI_PKT_TYPE_WR, M13_FT8720_109, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_110, DSI_PKT_TYPE_WR, M13_FT8720_110, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_111, DSI_PKT_TYPE_WR, M13_FT8720_111, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_112, DSI_PKT_TYPE_WR, M13_FT8720_112, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_113, DSI_PKT_TYPE_WR, M13_FT8720_113, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_114, DSI_PKT_TYPE_WR, M13_FT8720_114, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_115, DSI_PKT_TYPE_WR, M13_FT8720_115, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_116, DSI_PKT_TYPE_WR, M13_FT8720_116, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_117, DSI_PKT_TYPE_WR, M13_FT8720_117, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_118, DSI_PKT_TYPE_WR, M13_FT8720_118, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_119, DSI_PKT_TYPE_WR, M13_FT8720_119, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_120, DSI_PKT_TYPE_WR, M13_FT8720_120, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_121, DSI_PKT_TYPE_WR, M13_FT8720_121, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_122, DSI_PKT_TYPE_WR, M13_FT8720_122, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_123, DSI_PKT_TYPE_WR, M13_FT8720_123, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_124, DSI_PKT_TYPE_WR, M13_FT8720_124, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_125, DSI_PKT_TYPE_WR, M13_FT8720_125, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_126, DSI_PKT_TYPE_WR, M13_FT8720_126, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_127, DSI_PKT_TYPE_WR, M13_FT8720_127, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_128, DSI_PKT_TYPE_WR, M13_FT8720_128, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_129, DSI_PKT_TYPE_WR, M13_FT8720_129, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_130, DSI_PKT_TYPE_WR, M13_FT8720_130, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_131, DSI_PKT_TYPE_WR, M13_FT8720_131, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_132, DSI_PKT_TYPE_WR, M13_FT8720_132, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_133, DSI_PKT_TYPE_WR, M13_FT8720_133, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_134, DSI_PKT_TYPE_WR, M13_FT8720_134, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_135, DSI_PKT_TYPE_WR, M13_FT8720_135, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_136, DSI_PKT_TYPE_WR, M13_FT8720_136, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_137, DSI_PKT_TYPE_WR, M13_FT8720_137, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_138, DSI_PKT_TYPE_WR, M13_FT8720_138, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_139, DSI_PKT_TYPE_WR, M13_FT8720_139, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_140, DSI_PKT_TYPE_WR, M13_FT8720_140, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_141, DSI_PKT_TYPE_WR, M13_FT8720_141, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_142, DSI_PKT_TYPE_WR, M13_FT8720_142, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_143, DSI_PKT_TYPE_WR, M13_FT8720_143, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_144, DSI_PKT_TYPE_WR, M13_FT8720_144, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_145, DSI_PKT_TYPE_WR, M13_FT8720_145, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_146, DSI_PKT_TYPE_WR, M13_FT8720_146, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_147, DSI_PKT_TYPE_WR, M13_FT8720_147, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_148, DSI_PKT_TYPE_WR, M13_FT8720_148, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_149, DSI_PKT_TYPE_WR, M13_FT8720_149, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_150, DSI_PKT_TYPE_WR, M13_FT8720_150, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_151, DSI_PKT_TYPE_WR, M13_FT8720_151, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_152, DSI_PKT_TYPE_WR, M13_FT8720_152, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_153, DSI_PKT_TYPE_WR, M13_FT8720_153, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_154, DSI_PKT_TYPE_WR, M13_FT8720_154, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_155, DSI_PKT_TYPE_WR, M13_FT8720_155, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_156, DSI_PKT_TYPE_WR, M13_FT8720_156, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_157, DSI_PKT_TYPE_WR, M13_FT8720_157, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_158, DSI_PKT_TYPE_WR, M13_FT8720_158, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_159, DSI_PKT_TYPE_WR, M13_FT8720_159, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_160, DSI_PKT_TYPE_WR, M13_FT8720_160, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_161, DSI_PKT_TYPE_WR, M13_FT8720_161, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_162, DSI_PKT_TYPE_WR, M13_FT8720_162, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_163, DSI_PKT_TYPE_WR, M13_FT8720_163, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_164, DSI_PKT_TYPE_WR, M13_FT8720_164, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_165, DSI_PKT_TYPE_WR, M13_FT8720_165, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_166, DSI_PKT_TYPE_WR, M13_FT8720_166, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_167, DSI_PKT_TYPE_WR, M13_FT8720_167, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_168, DSI_PKT_TYPE_WR, M13_FT8720_168, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_169, DSI_PKT_TYPE_WR, M13_FT8720_169, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_170, DSI_PKT_TYPE_WR, M13_FT8720_170, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_171, DSI_PKT_TYPE_WR, M13_FT8720_171, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_172, DSI_PKT_TYPE_WR, M13_FT8720_172, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_173, DSI_PKT_TYPE_WR, M13_FT8720_173, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_174, DSI_PKT_TYPE_WR, M13_FT8720_174, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_175, DSI_PKT_TYPE_WR, M13_FT8720_175, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_176, DSI_PKT_TYPE_WR, M13_FT8720_176, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_177, DSI_PKT_TYPE_WR, M13_FT8720_177, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_178, DSI_PKT_TYPE_WR, M13_FT8720_178, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_179, DSI_PKT_TYPE_WR, M13_FT8720_179, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_180, DSI_PKT_TYPE_WR, M13_FT8720_180, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_181, DSI_PKT_TYPE_WR, M13_FT8720_181, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_182, DSI_PKT_TYPE_WR, M13_FT8720_182, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_183, DSI_PKT_TYPE_WR, M13_FT8720_183, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_184, DSI_PKT_TYPE_WR, M13_FT8720_184, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_185, DSI_PKT_TYPE_WR, M13_FT8720_185, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_186, DSI_PKT_TYPE_WR, M13_FT8720_186, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_187, DSI_PKT_TYPE_WR, M13_FT8720_187, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_188, DSI_PKT_TYPE_WR, M13_FT8720_188, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_189, DSI_PKT_TYPE_WR, M13_FT8720_189, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_190, DSI_PKT_TYPE_WR, M13_FT8720_190, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_191, DSI_PKT_TYPE_WR, M13_FT8720_191, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_192, DSI_PKT_TYPE_WR, M13_FT8720_192, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_193, DSI_PKT_TYPE_WR, M13_FT8720_193, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_194, DSI_PKT_TYPE_WR, M13_FT8720_194, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_195, DSI_PKT_TYPE_WR, M13_FT8720_195, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_196, DSI_PKT_TYPE_WR, M13_FT8720_196, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_197, DSI_PKT_TYPE_WR, M13_FT8720_197, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_198, DSI_PKT_TYPE_WR, M13_FT8720_198, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_199, DSI_PKT_TYPE_WR, M13_FT8720_199, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_200, DSI_PKT_TYPE_WR, M13_FT8720_200, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_201, DSI_PKT_TYPE_WR, M13_FT8720_201, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_202, DSI_PKT_TYPE_WR, M13_FT8720_202, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_203, DSI_PKT_TYPE_WR, M13_FT8720_203, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_204, DSI_PKT_TYPE_WR, M13_FT8720_204, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_205, DSI_PKT_TYPE_WR, M13_FT8720_205, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_206, DSI_PKT_TYPE_WR, M13_FT8720_206, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_207, DSI_PKT_TYPE_WR, M13_FT8720_207, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_208, DSI_PKT_TYPE_WR, M13_FT8720_208, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_209, DSI_PKT_TYPE_WR, M13_FT8720_209, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_210, DSI_PKT_TYPE_WR, M13_FT8720_210, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_211, DSI_PKT_TYPE_WR, M13_FT8720_211, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_212, DSI_PKT_TYPE_WR, M13_FT8720_212, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_213, DSI_PKT_TYPE_WR, M13_FT8720_213, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_214, DSI_PKT_TYPE_WR, M13_FT8720_214, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_215, DSI_PKT_TYPE_WR, M13_FT8720_215, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_216, DSI_PKT_TYPE_WR, M13_FT8720_216, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_217, DSI_PKT_TYPE_WR, M13_FT8720_217, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_218, DSI_PKT_TYPE_WR, M13_FT8720_218, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_219, DSI_PKT_TYPE_WR, M13_FT8720_219, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_220, DSI_PKT_TYPE_WR, M13_FT8720_220, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_221, DSI_PKT_TYPE_WR, M13_FT8720_221, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_222, DSI_PKT_TYPE_WR, M13_FT8720_222, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_223, DSI_PKT_TYPE_WR, M13_FT8720_223, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_224, DSI_PKT_TYPE_WR, M13_FT8720_224, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_225, DSI_PKT_TYPE_WR, M13_FT8720_225, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_226, DSI_PKT_TYPE_WR, M13_FT8720_226, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_227, DSI_PKT_TYPE_WR, M13_FT8720_227, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_228, DSI_PKT_TYPE_WR, M13_FT8720_228, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_229, DSI_PKT_TYPE_WR, M13_FT8720_229, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_230, DSI_PKT_TYPE_WR, M13_FT8720_230, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_231, DSI_PKT_TYPE_WR, M13_FT8720_231, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_232, DSI_PKT_TYPE_WR, M13_FT8720_232, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_233, DSI_PKT_TYPE_WR, M13_FT8720_233, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_234, DSI_PKT_TYPE_WR, M13_FT8720_234, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_235, DSI_PKT_TYPE_WR, M13_FT8720_235, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_236, DSI_PKT_TYPE_WR, M13_FT8720_236, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_237, DSI_PKT_TYPE_WR, M13_FT8720_237, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_238, DSI_PKT_TYPE_WR, M13_FT8720_238, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_239, DSI_PKT_TYPE_WR, M13_FT8720_239, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_240, DSI_PKT_TYPE_WR, M13_FT8720_240, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_241, DSI_PKT_TYPE_WR, M13_FT8720_241, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_242, DSI_PKT_TYPE_WR, M13_FT8720_242, 0);
static DEFINE_STATIC_PACKET(m13_ft8720_dtc_243, DSI_PKT_TYPE_WR, M13_FT8720_243, 0);



static DEFINE_PANEL_MDELAY(m13_wait_display_off, 1 * 17); /* 1 frame */
static DEFINE_PANEL_MDELAY(m13_wait_sleep_in, 4 * 17); /* 4 frame */
static DEFINE_PANEL_MDELAY(m13_wait_2msec, 2);
static DEFINE_PANEL_MDELAY(m13_wait_20msec, 20);
static DEFINE_PANEL_MDELAY(m13_wait_120msec, 120);
static DEFINE_PANEL_MDELAY(m13_wait_blic_off, 1);

static u8 M13_I2C_BLIC_MODE[] = {
	0x24, 0x00,	/* Brightness Mode	00: PWMI	01:I2C	*/
};
static DEFINE_PKTUI(m13_i2c_blic_mode, &m13_maptbl[BLIC_MODE_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(m13_i2c_blic_mode, I2C_PKT_TYPE_WR, M13_I2C_BLIC_MODE, 0);

static void *m13_init_cmdtbl[] = {
	&ft8720_dtc_restbl[RES_ID],
	&PKTINFO(m13_ft8720_dtc_001),
	&PKTINFO(m13_ft8720_dtc_002),
	&PKTINFO(m13_ft8720_dtc_003),
	&PKTINFO(m13_ft8720_dtc_004),
	&PKTINFO(m13_ft8720_dtc_005),
	&PKTINFO(m13_ft8720_dtc_006),
	&PKTINFO(m13_ft8720_dtc_007),
	&PKTINFO(m13_ft8720_dtc_008),
	&PKTINFO(m13_ft8720_dtc_009),
	&PKTINFO(m13_ft8720_dtc_010),
	&PKTINFO(m13_ft8720_dtc_011),
	&PKTINFO(m13_ft8720_dtc_012),
	&PKTINFO(m13_ft8720_dtc_013),
	&PKTINFO(m13_ft8720_dtc_014),
	&PKTINFO(m13_ft8720_dtc_015),
	&PKTINFO(m13_ft8720_dtc_016),
	&PKTINFO(m13_ft8720_dtc_017),
	&PKTINFO(m13_ft8720_dtc_018),
	&PKTINFO(m13_ft8720_dtc_019),
	&PKTINFO(m13_ft8720_dtc_020),
	&PKTINFO(m13_ft8720_dtc_021),
	&PKTINFO(m13_ft8720_dtc_022),
	&PKTINFO(m13_ft8720_dtc_023),
	&PKTINFO(m13_ft8720_dtc_024),
	&PKTINFO(m13_ft8720_dtc_025),
	&PKTINFO(m13_ft8720_dtc_026),
	&PKTINFO(m13_ft8720_dtc_027),
	&PKTINFO(m13_ft8720_dtc_028),
	&PKTINFO(m13_ft8720_dtc_029),
	&PKTINFO(m13_ft8720_dtc_030),
	&PKTINFO(m13_ft8720_dtc_031),
	&PKTINFO(m13_ft8720_dtc_032),
	&PKTINFO(m13_ft8720_dtc_033),
	&PKTINFO(m13_ft8720_dtc_034),
	&PKTINFO(m13_ft8720_dtc_035),
	&PKTINFO(m13_ft8720_dtc_036),
	&PKTINFO(m13_ft8720_dtc_037),
	&PKTINFO(m13_ft8720_dtc_038),
	&PKTINFO(m13_ft8720_dtc_039),
	&PKTINFO(m13_ft8720_dtc_040),
	&PKTINFO(m13_ft8720_dtc_041),
	&PKTINFO(m13_ft8720_dtc_042),
	&PKTINFO(m13_ft8720_dtc_043),
	&PKTINFO(m13_ft8720_dtc_044),
	&PKTINFO(m13_ft8720_dtc_045),
	&PKTINFO(m13_ft8720_dtc_046),
	&PKTINFO(m13_ft8720_dtc_047),
	&PKTINFO(m13_ft8720_dtc_048),
	&PKTINFO(m13_ft8720_dtc_049),
	&PKTINFO(m13_ft8720_dtc_050),
	&PKTINFO(m13_ft8720_dtc_051),
	&PKTINFO(m13_ft8720_dtc_052),
	&PKTINFO(m13_ft8720_dtc_053),
	&PKTINFO(m13_ft8720_dtc_054),
	&PKTINFO(m13_ft8720_dtc_055),
	&PKTINFO(m13_ft8720_dtc_056),
	&PKTINFO(m13_ft8720_dtc_057),
	&PKTINFO(m13_ft8720_dtc_058),
	&PKTINFO(m13_ft8720_dtc_059),
	&PKTINFO(m13_ft8720_dtc_060),
	&PKTINFO(m13_ft8720_dtc_061),
	&PKTINFO(m13_ft8720_dtc_062),
	&PKTINFO(m13_ft8720_dtc_063),
	&PKTINFO(m13_ft8720_dtc_064),
	&PKTINFO(m13_ft8720_dtc_065),
	&PKTINFO(m13_ft8720_dtc_066),
	&PKTINFO(m13_ft8720_dtc_067),
	&PKTINFO(m13_ft8720_dtc_068),
	&PKTINFO(m13_ft8720_dtc_069),
	&PKTINFO(m13_ft8720_dtc_070),
	&PKTINFO(m13_ft8720_dtc_071),
	&PKTINFO(m13_ft8720_dtc_072),
	&PKTINFO(m13_ft8720_dtc_073),
	&PKTINFO(m13_ft8720_dtc_074),
	&PKTINFO(m13_ft8720_dtc_075),
	&PKTINFO(m13_ft8720_dtc_076),
	&PKTINFO(m13_ft8720_dtc_077),
	&PKTINFO(m13_ft8720_dtc_078),
	&PKTINFO(m13_ft8720_dtc_079),
	&PKTINFO(m13_ft8720_dtc_080),
	&PKTINFO(m13_ft8720_dtc_081),
	&PKTINFO(m13_ft8720_dtc_082),
	&PKTINFO(m13_ft8720_dtc_083),
	&PKTINFO(m13_ft8720_dtc_084),
	&PKTINFO(m13_ft8720_dtc_085),
	&PKTINFO(m13_ft8720_dtc_086),
	&PKTINFO(m13_ft8720_dtc_087),
	&PKTINFO(m13_ft8720_dtc_088),
	&PKTINFO(m13_ft8720_dtc_089),
	&PKTINFO(m13_ft8720_dtc_090),
	&PKTINFO(m13_ft8720_dtc_091),
	&PKTINFO(m13_ft8720_dtc_092),
	&PKTINFO(m13_ft8720_dtc_093),
	&PKTINFO(m13_ft8720_dtc_094),
	&PKTINFO(m13_ft8720_dtc_095),
	&PKTINFO(m13_ft8720_dtc_096),
	&PKTINFO(m13_ft8720_dtc_097),
	&PKTINFO(m13_ft8720_dtc_098),
	&PKTINFO(m13_ft8720_dtc_099),
	&PKTINFO(m13_ft8720_dtc_100),
	&PKTINFO(m13_ft8720_dtc_101),
	&PKTINFO(m13_ft8720_dtc_102),
	&PKTINFO(m13_ft8720_dtc_103),
	&PKTINFO(m13_ft8720_dtc_104),
	&PKTINFO(m13_ft8720_dtc_105),
	&PKTINFO(m13_ft8720_dtc_106),
	&PKTINFO(m13_ft8720_dtc_107),
	&PKTINFO(m13_ft8720_dtc_108),
	&PKTINFO(m13_ft8720_dtc_109),
	&PKTINFO(m13_ft8720_dtc_110),
	&PKTINFO(m13_ft8720_dtc_111),
	&PKTINFO(m13_ft8720_dtc_112),
	&PKTINFO(m13_ft8720_dtc_113),
	&PKTINFO(m13_ft8720_dtc_114),
	&PKTINFO(m13_ft8720_dtc_115),
	&PKTINFO(m13_ft8720_dtc_116),
	&PKTINFO(m13_ft8720_dtc_117),
	&PKTINFO(m13_ft8720_dtc_118),
	&PKTINFO(m13_ft8720_dtc_119),
	&PKTINFO(m13_ft8720_dtc_120),
	&PKTINFO(m13_ft8720_dtc_121),
	&PKTINFO(m13_ft8720_dtc_122),
	&PKTINFO(m13_ft8720_dtc_123),
	&PKTINFO(m13_ft8720_dtc_124),
	&PKTINFO(m13_ft8720_dtc_125),
	&PKTINFO(m13_ft8720_dtc_126),
	&PKTINFO(m13_ft8720_dtc_127),
	&PKTINFO(m13_ft8720_dtc_128),
	&PKTINFO(m13_ft8720_dtc_129),
	&PKTINFO(m13_ft8720_dtc_130),
	&PKTINFO(m13_ft8720_dtc_131),
	&PKTINFO(m13_ft8720_dtc_132),
	&PKTINFO(m13_ft8720_dtc_133),
	&PKTINFO(m13_ft8720_dtc_134),
	&PKTINFO(m13_ft8720_dtc_135),
	&PKTINFO(m13_ft8720_dtc_136),
	&PKTINFO(m13_ft8720_dtc_137),
	&PKTINFO(m13_ft8720_dtc_138),
	&PKTINFO(m13_ft8720_dtc_139),
	&PKTINFO(m13_ft8720_dtc_140),
	&PKTINFO(m13_ft8720_dtc_141),
	&PKTINFO(m13_ft8720_dtc_142),
	&PKTINFO(m13_ft8720_dtc_143),
	&PKTINFO(m13_ft8720_dtc_144),
	&PKTINFO(m13_ft8720_dtc_145),
	&PKTINFO(m13_ft8720_dtc_146),
	&PKTINFO(m13_ft8720_dtc_147),
	&PKTINFO(m13_ft8720_dtc_148),
	&PKTINFO(m13_ft8720_dtc_149),
	&PKTINFO(m13_ft8720_dtc_150),
	&PKTINFO(m13_ft8720_dtc_151),
	&PKTINFO(m13_ft8720_dtc_152),
	&PKTINFO(m13_ft8720_dtc_153),
	&PKTINFO(m13_ft8720_dtc_154),
	&PKTINFO(m13_ft8720_dtc_155),
	&PKTINFO(m13_ft8720_dtc_156),
	&PKTINFO(m13_ft8720_dtc_157),
	&PKTINFO(m13_ft8720_dtc_158),
	&PKTINFO(m13_ft8720_dtc_159),
	&PKTINFO(m13_ft8720_dtc_160),
	&PKTINFO(m13_ft8720_dtc_161),
	&PKTINFO(m13_ft8720_dtc_162),
	&PKTINFO(m13_ft8720_dtc_163),
	&PKTINFO(m13_ft8720_dtc_164),
	&PKTINFO(m13_ft8720_dtc_165),
	&PKTINFO(m13_ft8720_dtc_166),
	&PKTINFO(m13_ft8720_dtc_167),
	&PKTINFO(m13_ft8720_dtc_168),
	&PKTINFO(m13_ft8720_dtc_169),
	&PKTINFO(m13_ft8720_dtc_170),
	&PKTINFO(m13_ft8720_dtc_171),
	&PKTINFO(m13_ft8720_dtc_172),
	&PKTINFO(m13_ft8720_dtc_173),
	&PKTINFO(m13_ft8720_dtc_174),
	&PKTINFO(m13_ft8720_dtc_175),
	&PKTINFO(m13_ft8720_dtc_176),
	&PKTINFO(m13_ft8720_dtc_177),
	&PKTINFO(m13_ft8720_dtc_178),
	&PKTINFO(m13_ft8720_dtc_179),
	&PKTINFO(m13_ft8720_dtc_180),
	&PKTINFO(m13_ft8720_dtc_181),
	&PKTINFO(m13_ft8720_dtc_182),
	&PKTINFO(m13_ft8720_dtc_183),
	&PKTINFO(m13_ft8720_dtc_184),
	&PKTINFO(m13_ft8720_dtc_185),
	&PKTINFO(m13_ft8720_dtc_186),
	&PKTINFO(m13_ft8720_dtc_187),
	&PKTINFO(m13_ft8720_dtc_188),
	&PKTINFO(m13_ft8720_dtc_189),
	&PKTINFO(m13_ft8720_dtc_190),
	&PKTINFO(m13_ft8720_dtc_191),
	&PKTINFO(m13_ft8720_dtc_192),
	&PKTINFO(m13_ft8720_dtc_193),
	&PKTINFO(m13_ft8720_dtc_194),
	&PKTINFO(m13_ft8720_dtc_195),
	&PKTINFO(m13_ft8720_dtc_196),
	&PKTINFO(m13_ft8720_dtc_197),
	&PKTINFO(m13_ft8720_dtc_198),
	&PKTINFO(m13_ft8720_dtc_199),
	&PKTINFO(m13_ft8720_dtc_200),
	&PKTINFO(m13_ft8720_dtc_201),
	&PKTINFO(m13_ft8720_dtc_202),
	&PKTINFO(m13_ft8720_dtc_203),
	&PKTINFO(m13_ft8720_dtc_204),
	&PKTINFO(m13_ft8720_dtc_205),
	&PKTINFO(m13_ft8720_dtc_206),
	&PKTINFO(m13_ft8720_dtc_207),
	&PKTINFO(m13_ft8720_dtc_208),
	&PKTINFO(m13_ft8720_dtc_209),
	&PKTINFO(m13_ft8720_dtc_210),
	&PKTINFO(m13_ft8720_dtc_211),
	&PKTINFO(m13_ft8720_dtc_212),
	&PKTINFO(m13_ft8720_dtc_213),
	&PKTINFO(m13_ft8720_dtc_214),
	&PKTINFO(m13_ft8720_dtc_215),
	&PKTINFO(m13_ft8720_dtc_216),
	&PKTINFO(m13_ft8720_dtc_217),
	&PKTINFO(m13_ft8720_dtc_218),
	&PKTINFO(m13_ft8720_dtc_219),
	&PKTINFO(m13_ft8720_dtc_220),
	&PKTINFO(m13_ft8720_dtc_221),
	&PKTINFO(m13_ft8720_dtc_222),
	&PKTINFO(m13_ft8720_dtc_223),
	&PKTINFO(m13_ft8720_dtc_224),
	&PKTINFO(m13_ft8720_dtc_225),
	&PKTINFO(m13_ft8720_dtc_226),
	&PKTINFO(m13_ft8720_dtc_227),
	&PKTINFO(m13_ft8720_dtc_228),
	&PKTINFO(m13_ft8720_dtc_229),
	&PKTINFO(m13_ft8720_dtc_230),
	&PKTINFO(m13_ft8720_dtc_231),
	&PKTINFO(m13_ft8720_dtc_232),
	&PKTINFO(m13_ft8720_dtc_233),
	&PKTINFO(m13_ft8720_dtc_234),
	&PKTINFO(m13_ft8720_dtc_235),
	&PKTINFO(m13_ft8720_dtc_236),
	&PKTINFO(m13_ft8720_dtc_237),
	&PKTINFO(m13_ft8720_dtc_238),
	&PKTINFO(m13_ft8720_dtc_239),
	&PKTINFO(m13_ft8720_dtc_240),
	&PKTINFO(m13_ft8720_dtc_241),
	&PKTINFO(m13_ft8720_dtc_242),
	&PKTINFO(m13_ft8720_dtc_243),
	&PKTINFO(m13_sleep_out),
	&DLYINFO(m13_wait_120msec),
};

static void *m13_res_init_cmdtbl[] = {
	&ft8720_dtc_restbl[RES_ID],
};

static void *m13_set_bl_cmdtbl[] = {
	&PKTINFO(m13_brightness),
	&PKTINFO(m13_i2c_blic_mode), /* S2DPS01 I2C or PWMI */
};

static void *m13_display_on_cmdtbl[] = {
	&PKTINFO(m13_display_on),
	&DLYINFO(m13_wait_20msec),
	&PKTINFO(m13_brightness_mode),
};

static void *m13_display_off_cmdtbl[] = {
	&PKTINFO(m13_display_off),
	&DLYINFO(m13_wait_20msec),
};

static void *m13_exit_cmdtbl[] = {
	&PKTINFO(m13_sleep_in),
};

/* ===================================================================================== */
/* ================================= [EA8076 I2C TABLE] ===========+++================= */
/* ===================================================================================== */
static u8 FT8720_M13_I2C_INIT[] = {
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

static u8 FT8720_M13_I2C_EXIT[] = {
	0x24, 0x01,	/* Brightness Mode = I2C*/
	0x20, 0x00,	/* Backlight Disable */
};

static u8 FT8720_M13_I2C_DUMP[] = {
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

static DEFINE_STATIC_PACKET(ft8720_dtc_m13_i2c_init, I2C_PKT_TYPE_WR, FT8720_M13_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(ft8720_dtc_m13_i2c_exit, I2C_PKT_TYPE_WR, FT8720_M13_I2C_EXIT, 0);
static DEFINE_STATIC_PACKET(ft8720_dtc_m13_i2c_dump, I2C_PKT_TYPE_RD, FT8720_M13_I2C_DUMP, 0);

static void *ft8720_dtc_m13_init_cmdtbl[] = {
	&PKTINFO(ft8720_dtc_m13_i2c_init),
};

static void *ft8720_dtc_m13_exit_cmdtbl[] = {
	&PKTINFO(ft8720_dtc_m13_i2c_exit),
};

static void *ft8720_dtc_m13_dump_cmdtbl[] = {
	&PKTINFO(ft8720_dtc_m13_i2c_dump),
};


static struct seqinfo m13_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", m13_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", m13_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", m13_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", m13_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", m13_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", m13_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", ft8720_dtc_m13_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", ft8720_dtc_m13_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", ft8720_dtc_m13_dump_cmdtbl),
#endif
};

struct common_panel_info ft8720_dtc_m13_default_panel_info = {
	.ldi_name = "ft8720_dtc",
	.name = "ft8720_dtc_m13_default",
	.model = "DTC_6_517_inch",
	.vendor = "DTC",
	.id = 0x4B1270,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
		.wait_tx_done = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(ft8720_dtc_m13_resol),
		.resol = ft8720_dtc_m13_resol,
	},
	.maptbl = m13_maptbl,
	.nr_maptbl = ARRAY_SIZE(m13_maptbl),
	.seqtbl = m13_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(m13_seqtbl),
	.rditbl = ft8720_dtc_rditbl,
	.nr_rditbl = ARRAY_SIZE(ft8720_dtc_rditbl),
	.restbl = ft8720_dtc_restbl,
	.nr_restbl = ARRAY_SIZE(ft8720_dtc_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&ft8720_dtc_m13_panel_dimming_info,
	},
	.i2c_data = &ft8720_dtc_m13_i2c_data,
};

static int __init ft8720_dtc_m13_panel_init(void)
{
	register_common_panel(&ft8720_dtc_m13_default_panel_info);

	return 0;
}
arch_initcall(ft8720_dtc_m13_panel_init)
#endif /* __FT8720_M13_PANEL_H__ */
