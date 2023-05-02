/*
 * linux/drivers/video/fbdev/exynos/panel/nt36525c_dtc/nt36525c_dtc_a04s_panel.h
 *
 * Header file for NT36525C Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NT36525C_A04S_PANEL_H__
#define __NT36525C_A04S_PANEL_H__
#include "../panel_drv.h"
#include "nt36525c_dtc.h"

#include "nt36525c_dtc_a04s_panel_dimming.h"
#include "nt36525c_dtc_a04s_panel_i2c.h"

#include "nt36525c_dtc_a04s_resol.h"

#undef __pn_name__
#define __pn_name__	a04s

#undef __PN_NAME__
#define __PN_NAME__	A04S

static struct seqinfo a04s_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [NT36525C READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [NT36525C RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [NT36525C MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a04s_brt_table[NT36525C_TOTAL_NR_LUMINANCE][1] = {
	{0},
	{1}, {1}, {2}, {3}, {3}, {4}, {5}, {5}, {6}, {7}, /* 1: 1 */
	{7}, {8}, {9}, {9}, {10}, {11}, {11}, {12}, {13}, {14},
	{14}, {15}, {16}, {16}, {17}, {18}, {18}, {19}, {20}, {20},
	{21}, {22}, {22}, {23}, {24}, {24}, {25}, {26}, {27}, {27},
	{28}, {29}, {29}, {30}, {31}, {31}, {32}, {33}, {33}, {34},
	{35}, {35}, {36}, {37}, {37}, {38}, {39}, {40}, {40}, {41},
	{42}, {42}, {43}, {44}, {44}, {45}, {46}, {46}, {47}, {48},
	{48}, {49}, {50}, {51}, {51}, {52}, {53}, {53}, {54}, {55},
	{55}, {56}, {57}, {57}, {58}, {59}, {59}, {60}, {61}, {61},
	{62}, {63}, {64}, {64}, {65}, {66}, {66}, {67}, {68}, {68},
	{69}, {70}, {70}, {71}, {72}, {72}, {73}, {74}, {74}, {75},
	{76}, {77}, {77}, {78}, {79}, {79}, {80}, {81}, {81}, {82},
	{83}, {83}, {84}, {85}, {85}, {86}, {87}, {88}, {88}, {89}, /* 128: 88 */
	{90}, {91}, {92}, {93}, {94}, {95}, {96}, {97}, {98}, {99},
	{100}, {101}, {102}, {103}, {104}, {105}, {106}, {107}, {108}, {109},
	{110}, {111}, {112}, {113}, {114}, {115}, {116}, {117}, {118}, {118},
	{119}, {120}, {121}, {122}, {123}, {124}, {125}, {126}, {127}, {128},
	{129}, {130}, {131}, {132}, {133}, {134}, {135}, {136}, {137}, {138},
	{139}, {140}, {141}, {142}, {143}, {144}, {145}, {146}, {147}, {148},
	{149}, {149}, {150}, {151}, {152}, {153}, {154}, {155}, {156}, {157},
	{158}, {159}, {160}, {161}, {162}, {163}, {164}, {165}, {166}, {167},
	{168}, {169}, {170}, {171}, {172}, {173}, {174}, {175}, {176}, {177},
	{178}, {179}, {180}, {180}, {181}, {182}, {183}, {184}, {185}, {186},
	{187}, {188}, {189}, {190}, {191}, {192}, {193}, {194}, {195}, {196},
	{197}, {198}, {199}, {200}, {201}, {202}, {203}, {204}, {205}, {206},
	{207}, {208}, {209}, {210}, {211}, {211}, {211}, {211}, {211}, {211}, /* 255: 211 */
	{211}, {211}, {211}, {211}, {211}, {211}, {211}, {211}, {211}, {211},
	{211}, {211}, {211}, {211}, {211}, {211}, {211}, {211}, {211}, {211},
	{211}, {211}, {211}, {211}, {211}, {211}, {211}, {211}, {211}, {211},
	{211}, {211}, {211}, {211}, {211}, {211}, {211}, {211}, {211}, {211},
	{211}, {211}, {211}, {211}, {211}, {254}, /* 306: 255 */
};



static struct maptbl a04s_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(a04s_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [NT36525C COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A04S_SLEEP_OUT[] = { 0x11 };
static u8 A04S_SLEEP_IN[] = { 0x10 };
static u8 A04S_DISPLAY_OFF[] = { 0x28 };
static u8 A04S_DISPLAY_ON[] = { 0x29 };

static u8 A04S_BRIGHTNESS[] = {
	0x51,
	0xFF,
};

static u8 A04S_BRIGHTNESS_MODE[] = {
	0x53,
	0x2C,
};

static u8 A04S_NT36525C_DTC_001[] = {
	0xFF,
	0x20,
};

static u8 A04S_NT36525C_DTC_002[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_DTC_003[] = {
	0x03,
	0x54,
};

static u8 A04S_NT36525C_DTC_004[] = {
	0x05,
	0xA9,
};

static u8 A04S_NT36525C_DTC_005[] = {
	0x07,
	0x69,
};

static u8 A04S_NT36525C_DTC_006[] = {
	0x08,
	0xB7,
};

static u8 A04S_NT36525C_DTC_007[] = {
	0x0E,
	0x87,
};

static u8 A04S_NT36525C_DTC_008[] = {
	0x0F,
	0x55,
};

static u8 A04S_NT36525C_DTC_009[] = {
	0x69,
	0xA9,
};

static u8 A04S_NT36525C_DTC_010[] = {
	0x94,
	0xC0,
};

static u8 A04S_NT36525C_DTC_011[] = {
	0x95,
	0x09,
};

static u8 A04S_NT36525C_DTC_012[] = {
	0x96,
	0x09,
};

static u8 A04S_NT36525C_DTC_013[] = {
	0xFF,
	0x23,
};

static u8 A04S_NT36525C_DTC_014[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_DTC_015[] = {
	0x04,
	0x00,
};

static u8 A04S_NT36525C_DTC_016[] = {
	0x08,
	0x10,
};

static u8 A04S_NT36525C_DTC_017[] = {
	0x09,
	0x2F,
};

static u8 A04S_NT36525C_DTC_018[] = {
	0x12,
	0xB4,
};

static u8 A04S_NT36525C_DTC_019[] = {
	0x15,
	0xE9,
};

static u8 A04S_NT36525C_DTC_020[] = {
	0x16,
	0x0B,
};

static u8 A04S_NT36525C_DTC_021[] = {
	0x19,
	0x00,
};

static u8 A04S_NT36525C_DTC_022[] = {
	0x1A,
	0x00,
};

static u8 A04S_NT36525C_DTC_023[] = {
	0x1B,
	0x08,
};

static u8 A04S_NT36525C_DTC_024[] = {
	0x1C,
	0x0A,
};

static u8 A04S_NT36525C_DTC_025[] = {
	0x1D,
	0x0C,
};

static u8 A04S_NT36525C_DTC_026[] = {
	0x1E,
	0x12,
};

static u8 A04S_NT36525C_DTC_027[] = {
	0x1F,
	0x16,
};

static u8 A04S_NT36525C_DTC_028[] = {
	0x20,
	0x1A,
};

static u8 A04S_NT36525C_DTC_029[] = {
	0x21,
	0x1C,
};

static u8 A04S_NT36525C_DTC_030[] = {
	0x22,
	0x20,
};

static u8 A04S_NT36525C_DTC_031[] = {
	0x23,
	0x24,
};

static u8 A04S_NT36525C_DTC_032[] = {
	0x24,
	0x28,
};

static u8 A04S_NT36525C_DTC_033[] = {
	0x25,
	0x2C,
};

static u8 A04S_NT36525C_DTC_034[] = {
	0x26,
	0x30,
};

static u8 A04S_NT36525C_DTC_035[] = {
	0x27,
	0x38,
};

static u8 A04S_NT36525C_DTC_036[] = {
	0x28,
	0x3C,
};

static u8 A04S_NT36525C_DTC_037[] = {
	0x29,
	0x10,
};

static u8 A04S_NT36525C_DTC_038[] = {
	0x30,
	0xFF,
};

static u8 A04S_NT36525C_DTC_039[] = {
	0x31,
	0xFF,
};

static u8 A04S_NT36525C_DTC_040[] = {
	0x32,
	0xFE,
};

static u8 A04S_NT36525C_DTC_041[] = {
	0x33,
	0xFD,
};

static u8 A04S_NT36525C_DTC_042[] = {
	0x34,
	0xFD,
};

static u8 A04S_NT36525C_DTC_043[] = {
	0x35,
	0xFC,
};

static u8 A04S_NT36525C_DTC_044[] = {
	0x36,
	0xFB,
};

static u8 A04S_NT36525C_DTC_045[] = {
	0x37,
	0xF9,
};

static u8 A04S_NT36525C_DTC_046[] = {
	0x38,
	0xF7,
};

static u8 A04S_NT36525C_DTC_047[] = {
	0x39,
	0xF3,
};

static u8 A04S_NT36525C_DTC_048[] = {
	0x3A,
	0xEA,
};

static u8 A04S_NT36525C_DTC_049[] = {
	0x3B,
	0xE6,
};

static u8 A04S_NT36525C_DTC_050[] = {
	0x3D,
	0xE0,
};

static u8 A04S_NT36525C_DTC_051[] = {
	0x3F,
	0xDD,
};

static u8 A04S_NT36525C_DTC_052[] = {
	0x40,
	0xDB,
};

static u8 A04S_NT36525C_DTC_053[] = {
	0x41,
	0xD9,
};

static u8 A04S_NT36525C_DTC_054[] = {
	0xFF,
	0x24,
};

static u8 A04S_NT36525C_DTC_055[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_DTC_056[] = {
	0x00,
	0x05,
};

static u8 A04S_NT36525C_DTC_057[] = {
	0x01,
	0x1F,
};

static u8 A04S_NT36525C_DTC_058[] = {
	0x02,
	0x00,
};

static u8 A04S_NT36525C_DTC_059[] = {
	0x03,
	0x00,
};

static u8 A04S_NT36525C_DTC_060[] = {
	0x04,
	0x1E,
};

static u8 A04S_NT36525C_DTC_061[] = {
	0x05,
	0x00,
};

static u8 A04S_NT36525C_DTC_062[] = {
	0x06,
	0x00,
};

static u8 A04S_NT36525C_DTC_063[] = {
	0x07,
	0x0F,
};

static u8 A04S_NT36525C_DTC_064[] = {
	0x08,
	0x0F,
};

static u8 A04S_NT36525C_DTC_065[] = {
	0x09,
	0x0E,
};

static u8 A04S_NT36525C_DTC_066[] = {
	0x0A,
	0x0E,
};

static u8 A04S_NT36525C_DTC_067[] = {
	0x0B,
	0x0D,
};

static u8 A04S_NT36525C_DTC_068[] = {
	0x0C,
	0x0D,
};

static u8 A04S_NT36525C_DTC_069[] = {
	0x0D,
	0x0C,
};

static u8 A04S_NT36525C_DTC_070[] = {
	0x0E,
	0x0C,
};

static u8 A04S_NT36525C_DTC_071[] = {
	0x0F,
	0x04,
};

static u8 A04S_NT36525C_DTC_072[] = {
	0x10,
	0x00,
};

static u8 A04S_NT36525C_DTC_073[] = {
	0x11,
	0x00,
};

static u8 A04S_NT36525C_DTC_074[] = {
	0x12,
	0x00,
};

static u8 A04S_NT36525C_DTC_075[] = {
	0x13,
	0x00,
};

static u8 A04S_NT36525C_DTC_076[] = {
	0x14,
	0x00,
};

static u8 A04S_NT36525C_DTC_077[] = {
	0x15,
	0x00,
};

static u8 A04S_NT36525C_DTC_078[] = {
	0x16,
	0x05,
};

static u8 A04S_NT36525C_DTC_079[] = {
	0x17,
	0x1F,
};

static u8 A04S_NT36525C_DTC_080[] = {
	0x18,
	0x00,
};

static u8 A04S_NT36525C_DTC_081[] = {
	0x19,
	0x00,
};

static u8 A04S_NT36525C_DTC_082[] = {
	0x1A,
	0x1E,
};

static u8 A04S_NT36525C_DTC_083[] = {
	0x1B,
	0x00,
};

static u8 A04S_NT36525C_DTC_084[] = {
	0x1C,
	0x00,
};

static u8 A04S_NT36525C_DTC_085[] = {
	0x1D,
	0x0F,
};

static u8 A04S_NT36525C_DTC_086[] = {
	0x1E,
	0x0F,
};

static u8 A04S_NT36525C_DTC_087[] = {
	0x1F,
	0x0E,
};

static u8 A04S_NT36525C_DTC_088[] = {
	0x20,
	0x0E,
};

static u8 A04S_NT36525C_DTC_089[] = {
	0x21,
	0x0D,
};

static u8 A04S_NT36525C_DTC_090[] = {
	0x22,
	0x0D,
};

static u8 A04S_NT36525C_DTC_091[] = {
	0x23,
	0x0C,
};

static u8 A04S_NT36525C_DTC_092[] = {
	0x24,
	0x0C,
};

static u8 A04S_NT36525C_DTC_093[] = {
	0x25,
	0x04,
};

static u8 A04S_NT36525C_DTC_094[] = {
	0x26,
	0x00,
};

static u8 A04S_NT36525C_DTC_095[] = {
	0x27,
	0x00,
};

static u8 A04S_NT36525C_DTC_096[] = {
	0x28,
	0x00,
};

static u8 A04S_NT36525C_DTC_097[] = {
	0x29,
	0x00,
};

static u8 A04S_NT36525C_DTC_098[] = {
	0x2A,
	0x00,
};

static u8 A04S_NT36525C_DTC_099[] = {
	0x2B,
	0x00,
};

static u8 A04S_NT36525C_DTC_100[] = {
	0x2F,
	0x06,
};

static u8 A04S_NT36525C_DTC_101[] = {
	0x30,
	0x40,
};

static u8 A04S_NT36525C_DTC_102[] = {
	0x33,
	0x40,
};

static u8 A04S_NT36525C_DTC_103[] = {
	0x34,
	0x06,
};

static u8 A04S_NT36525C_DTC_104[] = {
	0x37,
	0x44,
};

static u8 A04S_NT36525C_DTC_105[] = {
	0x3A,
	0x08,
};

static u8 A04S_NT36525C_DTC_106[] = {
	0x3B,
	0x80,
};

static u8 A04S_NT36525C_DTC_107[] = {
	0x3D,
	0x92,
};

static u8 A04S_NT36525C_DTC_108[] = {
	0x4D,
	0x12,
};

static u8 A04S_NT36525C_DTC_109[] = {
	0x4E,
	0x34,
};

static u8 A04S_NT36525C_DTC_110[] = {
	0x51,
	0x43,
};

static u8 A04S_NT36525C_DTC_111[] = {
	0x52,
	0x21,
};

static u8 A04S_NT36525C_DTC_112[] = {
	0x55,
	0x83,
};

static u8 A04S_NT36525C_DTC_113[] = {
	0x56,
	0x44,
};

static u8 A04S_NT36525C_DTC_114[] = {
	0x5A,
	0x82,
};

static u8 A04S_NT36525C_DTC_115[] = {
	0x5B,
	0x80,
};

static u8 A04S_NT36525C_DTC_116[] = {
	0x5C,
	0x9F,
};

static u8 A04S_NT36525C_DTC_117[] = {
	0x5D,
	0x08,
};

static u8 A04S_NT36525C_DTC_118[] = {
	0x5E,
	0x08,
};

static u8 A04S_NT36525C_DTC_119[] = {
	0x60,
	0x80,
};

static u8 A04S_NT36525C_DTC_120[] = {
	0x61,
	0x90,
};

static u8 A04S_NT36525C_DTC_121[] = {
	0x64,
	0x11,
};

static u8 A04S_NT36525C_DTC_122[] = {
	0x92,
	0x91,
};

static u8 A04S_NT36525C_DTC_123[] = {
	0x93,
	0x1C,
};

static u8 A04S_NT36525C_DTC_124[] = {
	0x94,
	0x08,
};

static u8 A04S_NT36525C_DTC_125[] = {
	0xAB,
	0x00,
};

static u8 A04S_NT36525C_DTC_126[] = {
	0xAD,
	0x00,
};

static u8 A04S_NT36525C_DTC_127[] = {
	0xB0,
	0x14,
};

static u8 A04S_NT36525C_DTC_128[] = {
	0xB1,
	0x7E,
};

static u8 A04S_NT36525C_DTC_129[] = {
	0xC2,
	0x86,
};

static u8 A04S_NT36525C_DTC_130[] = {
	0xFF,
	0x25,
};

static u8 A04S_NT36525C_DTC_131[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_DTC_132[] = {
	0x02,
	0x10,
};

static u8 A04S_NT36525C_DTC_133[] = {
	0x0A,
	0x81,
};

static u8 A04S_NT36525C_DTC_134[] = {
	0x0B,
	0xB7,
};

static u8 A04S_NT36525C_DTC_135[] = {
	0x0C,
	0x01,
};

static u8 A04S_NT36525C_DTC_136[] = {
	0x17,
	0x82,
};

static u8 A04S_NT36525C_DTC_137[] = {
	0x18,
	0x06,
};

static u8 A04S_NT36525C_DTC_138[] = {
	0x19,
	0x0F,
};

static u8 A04S_NT36525C_DTC_139[] = {
	0xFF,
	0x26,
};

static u8 A04S_NT36525C_DTC_140[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_DTC_141[] = {
	0x00,
	0xA0,
};

static u8 A04S_NT36525C_DTC_142[] = {
	0x45,
	0x8E,
};

static u8 A04S_NT36525C_DTC_143[] = {
	0xFF,
	0x27,
};

static u8 A04S_NT36525C_DTC_144[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_DTC_145[] = {
	0x13,
	0x00,
};

static u8 A04S_NT36525C_DTC_146[] = {
	0x15,
	0xB4,
};

static u8 A04S_NT36525C_DTC_147[] = {
	0x1F,
	0x55,
};

static u8 A04S_NT36525C_DTC_148[] = {
	0x26,
	0x0F,
};

static u8 A04S_NT36525C_DTC_149[] = {
	0xC0,
	0x19,
};

static u8 A04S_NT36525C_DTC_150[] = {
	0xC1,
	0x44,
};

static u8 A04S_NT36525C_DTC_151[] = {
	0xC2,
	0x10,
};

static u8 A04S_NT36525C_DTC_152[] = {
	0xC3,
	0x00,
};

static u8 A04S_NT36525C_DTC_153[] = {
	0xC4,
	0x44,
};

static u8 A04S_NT36525C_DTC_154[] = {
	0xC5,
	0x00,
};

static u8 A04S_NT36525C_DTC_155[] = {
	0xC6,
	0x00,
};

static u8 A04S_NT36525C_DTC_156[] = {
	0xFF,
	0xD0,
};

static u8 A04S_NT36525C_DTC_157[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_DTC_158[] = {
	0x05,
	0x07,
};

static u8 A04S_NT36525C_DTC_159[] = {
	0x09,
	0xF0,
};

static u8 A04S_NT36525C_DTC_160[] = {
	0x25,
	0xA9,
};

static u8 A04S_NT36525C_DTC_161[] = {
	0x28,
	0x70,
};

static u8 A04S_NT36525C_DTC_162[] = {
	0xFF,
	0x10,
};

static u8 A04S_NT36525C_DTC_163[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_DTC_164[] = {
	0xBA,
	0x02,
};

static u8 A04S_NT36525C_DTC_165[] = {
	0xBB,
	0x13,
};

static u8 A04S_NT36525C_DTC_166[] = {
	0x35,
	0x00,
};

static u8 A04S_NT36525C_DTC_167[] = {
	0x51,
	0xFF,
};

static u8 A04S_NT36525C_DTC_168[] = {
	0x53,
	0x2C,
};

static u8 A04S_NT36525C_DTC_169[] = {
	0x55,
	0x00,
};

static u8 A04S_NT36525C_DTC_170[] = {
	0x68,
	0x00, 0x01,
};

static DEFINE_STATIC_PACKET(a04s_sleep_out, DSI_PKT_TYPE_WR, A04S_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a04s_sleep_in, DSI_PKT_TYPE_WR, A04S_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a04s_display_on, DSI_PKT_TYPE_WR, A04S_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a04s_display_off, DSI_PKT_TYPE_WR, A04S_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(a04s_brightness_mode, DSI_PKT_TYPE_WR, A04S_BRIGHTNESS_MODE, 0);

static DEFINE_PKTUI(a04s_brightness, &a04s_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a04s_brightness, DSI_PKT_TYPE_WR, A04S_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_001, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_001, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_002, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_002, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_003, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_003, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_004, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_004, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_005, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_005, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_006, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_006, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_007, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_007, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_008, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_008, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_009, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_009, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_010, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_010, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_011, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_011, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_012, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_012, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_013, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_013, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_014, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_014, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_015, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_015, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_016, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_016, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_017, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_017, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_018, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_018, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_019, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_019, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_020, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_020, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_021, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_021, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_022, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_022, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_023, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_023, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_024, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_024, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_025, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_025, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_026, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_026, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_027, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_027, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_028, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_028, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_029, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_029, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_030, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_030, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_031, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_031, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_032, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_032, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_033, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_033, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_034, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_034, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_035, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_035, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_036, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_036, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_037, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_037, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_038, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_038, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_039, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_039, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_040, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_040, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_041, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_041, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_042, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_042, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_043, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_043, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_044, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_044, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_045, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_045, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_046, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_046, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_047, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_047, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_048, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_048, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_049, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_049, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_050, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_050, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_051, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_051, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_052, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_052, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_053, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_053, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_054, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_054, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_055, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_055, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_056, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_056, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_057, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_057, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_058, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_058, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_059, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_059, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_060, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_060, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_061, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_061, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_062, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_062, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_063, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_063, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_064, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_064, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_065, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_065, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_066, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_066, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_067, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_067, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_068, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_068, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_069, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_069, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_070, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_070, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_071, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_071, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_072, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_072, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_073, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_073, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_074, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_074, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_075, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_075, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_076, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_076, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_077, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_077, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_078, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_078, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_079, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_079, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_080, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_080, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_081, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_081, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_082, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_082, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_083, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_083, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_084, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_084, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_085, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_085, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_086, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_086, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_087, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_087, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_088, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_088, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_089, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_089, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_090, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_090, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_091, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_091, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_092, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_092, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_093, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_093, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_094, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_094, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_095, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_095, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_096, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_096, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_097, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_097, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_098, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_098, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_099, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_099, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_100, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_100, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_101, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_101, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_102, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_102, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_103, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_103, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_104, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_104, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_105, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_105, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_106, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_106, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_107, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_107, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_108, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_108, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_109, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_109, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_110, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_110, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_111, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_111, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_112, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_112, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_113, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_113, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_114, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_114, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_115, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_115, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_116, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_116, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_117, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_117, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_118, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_118, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_119, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_119, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_120, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_120, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_121, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_121, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_122, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_122, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_123, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_123, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_124, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_124, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_125, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_125, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_126, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_126, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_127, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_127, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_128, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_128, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_129, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_129, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_130, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_130, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_131, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_131, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_132, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_132, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_133, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_133, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_134, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_134, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_135, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_135, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_136, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_136, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_137, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_137, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_138, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_138, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_139, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_139, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_140, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_140, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_141, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_141, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_142, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_142, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_143, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_143, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_144, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_144, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_145, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_145, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_146, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_146, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_147, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_147, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_148, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_148, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_149, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_149, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_150, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_150, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_151, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_151, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_152, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_152, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_153, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_153, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_154, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_154, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_155, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_155, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_156, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_156, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_157, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_157, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_158, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_158, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_159, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_159, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_160, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_160, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_161, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_161, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_162, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_162, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_163, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_163, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_164, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_164, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_165, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_165, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_166, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_166, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_167, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_167, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_168, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_168, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_169, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_169, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_dtc_170, DSI_PKT_TYPE_WR, A04S_NT36525C_DTC_170, 0);
static DEFINE_PANEL_MDELAY(a04s_wait_display_off, 20);
static DEFINE_PANEL_MDELAY(a04s_wait_sleep_in, 4 * 17); /* 4 frame */
static DEFINE_PANEL_MDELAY(a04s_wait_2msec, 2);
static DEFINE_PANEL_MDELAY(a04s_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(a04s_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(a04s_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(a04s_wait_blic_off, 1);


static void *a04s_init_cmdtbl[] = {
	&nt36525c_dtc_restbl[RES_ID],
	&PKTINFO(a04s_nt36525c_dtc_001),
	&PKTINFO(a04s_nt36525c_dtc_002),
	&PKTINFO(a04s_nt36525c_dtc_003),
	&PKTINFO(a04s_nt36525c_dtc_004),
	&PKTINFO(a04s_nt36525c_dtc_005),
	&PKTINFO(a04s_nt36525c_dtc_006),
	&PKTINFO(a04s_nt36525c_dtc_007),
	&PKTINFO(a04s_nt36525c_dtc_008),
	&PKTINFO(a04s_nt36525c_dtc_009),
	&PKTINFO(a04s_nt36525c_dtc_010),
	&PKTINFO(a04s_nt36525c_dtc_011),
	&PKTINFO(a04s_nt36525c_dtc_012),
	&PKTINFO(a04s_nt36525c_dtc_013),
	&PKTINFO(a04s_nt36525c_dtc_014),
	&PKTINFO(a04s_nt36525c_dtc_015),
	&PKTINFO(a04s_nt36525c_dtc_016),
	&PKTINFO(a04s_nt36525c_dtc_017),
	&PKTINFO(a04s_nt36525c_dtc_018),
	&PKTINFO(a04s_nt36525c_dtc_019),
	&PKTINFO(a04s_nt36525c_dtc_020),
	&PKTINFO(a04s_nt36525c_dtc_021),
	&PKTINFO(a04s_nt36525c_dtc_022),
	&PKTINFO(a04s_nt36525c_dtc_023),
	&PKTINFO(a04s_nt36525c_dtc_024),
	&PKTINFO(a04s_nt36525c_dtc_025),
	&PKTINFO(a04s_nt36525c_dtc_026),
	&PKTINFO(a04s_nt36525c_dtc_027),
	&PKTINFO(a04s_nt36525c_dtc_028),
	&PKTINFO(a04s_nt36525c_dtc_029),
	&PKTINFO(a04s_nt36525c_dtc_030),
	&PKTINFO(a04s_nt36525c_dtc_031),
	&PKTINFO(a04s_nt36525c_dtc_032),
	&PKTINFO(a04s_nt36525c_dtc_033),
	&PKTINFO(a04s_nt36525c_dtc_034),
	&PKTINFO(a04s_nt36525c_dtc_035),
	&PKTINFO(a04s_nt36525c_dtc_036),
	&PKTINFO(a04s_nt36525c_dtc_037),
	&PKTINFO(a04s_nt36525c_dtc_038),
	&PKTINFO(a04s_nt36525c_dtc_039),
	&PKTINFO(a04s_nt36525c_dtc_040),
	&PKTINFO(a04s_nt36525c_dtc_041),
	&PKTINFO(a04s_nt36525c_dtc_042),
	&PKTINFO(a04s_nt36525c_dtc_043),
	&PKTINFO(a04s_nt36525c_dtc_044),
	&PKTINFO(a04s_nt36525c_dtc_045),
	&PKTINFO(a04s_nt36525c_dtc_046),
	&PKTINFO(a04s_nt36525c_dtc_047),
	&PKTINFO(a04s_nt36525c_dtc_048),
	&PKTINFO(a04s_nt36525c_dtc_049),
	&PKTINFO(a04s_nt36525c_dtc_050),
	&PKTINFO(a04s_nt36525c_dtc_051),
	&PKTINFO(a04s_nt36525c_dtc_052),
	&PKTINFO(a04s_nt36525c_dtc_053),
	&PKTINFO(a04s_nt36525c_dtc_054),
	&PKTINFO(a04s_nt36525c_dtc_055),
	&PKTINFO(a04s_nt36525c_dtc_056),
	&PKTINFO(a04s_nt36525c_dtc_057),
	&PKTINFO(a04s_nt36525c_dtc_058),
	&PKTINFO(a04s_nt36525c_dtc_059),
	&PKTINFO(a04s_nt36525c_dtc_060),
	&PKTINFO(a04s_nt36525c_dtc_061),
	&PKTINFO(a04s_nt36525c_dtc_062),
	&PKTINFO(a04s_nt36525c_dtc_063),
	&PKTINFO(a04s_nt36525c_dtc_064),
	&PKTINFO(a04s_nt36525c_dtc_065),
	&PKTINFO(a04s_nt36525c_dtc_066),
	&PKTINFO(a04s_nt36525c_dtc_067),
	&PKTINFO(a04s_nt36525c_dtc_068),
	&PKTINFO(a04s_nt36525c_dtc_069),
	&PKTINFO(a04s_nt36525c_dtc_070),
	&PKTINFO(a04s_nt36525c_dtc_071),
	&PKTINFO(a04s_nt36525c_dtc_072),
	&PKTINFO(a04s_nt36525c_dtc_073),
	&PKTINFO(a04s_nt36525c_dtc_074),
	&PKTINFO(a04s_nt36525c_dtc_075),
	&PKTINFO(a04s_nt36525c_dtc_076),
	&PKTINFO(a04s_nt36525c_dtc_077),
	&PKTINFO(a04s_nt36525c_dtc_078),
	&PKTINFO(a04s_nt36525c_dtc_079),
	&PKTINFO(a04s_nt36525c_dtc_080),
	&PKTINFO(a04s_nt36525c_dtc_081),
	&PKTINFO(a04s_nt36525c_dtc_082),
	&PKTINFO(a04s_nt36525c_dtc_083),
	&PKTINFO(a04s_nt36525c_dtc_084),
	&PKTINFO(a04s_nt36525c_dtc_085),
	&PKTINFO(a04s_nt36525c_dtc_086),
	&PKTINFO(a04s_nt36525c_dtc_087),
	&PKTINFO(a04s_nt36525c_dtc_088),
	&PKTINFO(a04s_nt36525c_dtc_089),
	&PKTINFO(a04s_nt36525c_dtc_090),
	&PKTINFO(a04s_nt36525c_dtc_091),
	&PKTINFO(a04s_nt36525c_dtc_092),
	&PKTINFO(a04s_nt36525c_dtc_093),
	&PKTINFO(a04s_nt36525c_dtc_094),
	&PKTINFO(a04s_nt36525c_dtc_095),
	&PKTINFO(a04s_nt36525c_dtc_096),
	&PKTINFO(a04s_nt36525c_dtc_097),
	&PKTINFO(a04s_nt36525c_dtc_098),
	&PKTINFO(a04s_nt36525c_dtc_099),
	&PKTINFO(a04s_nt36525c_dtc_100),
	&PKTINFO(a04s_nt36525c_dtc_101),
	&PKTINFO(a04s_nt36525c_dtc_102),
	&PKTINFO(a04s_nt36525c_dtc_103),
	&PKTINFO(a04s_nt36525c_dtc_104),
	&PKTINFO(a04s_nt36525c_dtc_105),
	&PKTINFO(a04s_nt36525c_dtc_106),
	&PKTINFO(a04s_nt36525c_dtc_107),
	&PKTINFO(a04s_nt36525c_dtc_108),
	&PKTINFO(a04s_nt36525c_dtc_109),
	&PKTINFO(a04s_nt36525c_dtc_110),
	&PKTINFO(a04s_nt36525c_dtc_111),
	&PKTINFO(a04s_nt36525c_dtc_112),
	&PKTINFO(a04s_nt36525c_dtc_113),
	&PKTINFO(a04s_nt36525c_dtc_114),
	&PKTINFO(a04s_nt36525c_dtc_115),
	&PKTINFO(a04s_nt36525c_dtc_116),
	&PKTINFO(a04s_nt36525c_dtc_117),
	&PKTINFO(a04s_nt36525c_dtc_118),
	&PKTINFO(a04s_nt36525c_dtc_119),
	&PKTINFO(a04s_nt36525c_dtc_120),
	&PKTINFO(a04s_nt36525c_dtc_121),
	&PKTINFO(a04s_nt36525c_dtc_122),
	&PKTINFO(a04s_nt36525c_dtc_123),
	&PKTINFO(a04s_nt36525c_dtc_124),
	&PKTINFO(a04s_nt36525c_dtc_125),
	&PKTINFO(a04s_nt36525c_dtc_126),
	&PKTINFO(a04s_nt36525c_dtc_127),
	&PKTINFO(a04s_nt36525c_dtc_128),
	&PKTINFO(a04s_nt36525c_dtc_129),
	&PKTINFO(a04s_nt36525c_dtc_130),
	&PKTINFO(a04s_nt36525c_dtc_131),
	&PKTINFO(a04s_nt36525c_dtc_132),
	&PKTINFO(a04s_nt36525c_dtc_133),
	&PKTINFO(a04s_nt36525c_dtc_134),
	&PKTINFO(a04s_nt36525c_dtc_135),
	&PKTINFO(a04s_nt36525c_dtc_136),
	&PKTINFO(a04s_nt36525c_dtc_137),
	&PKTINFO(a04s_nt36525c_dtc_138),
	&PKTINFO(a04s_nt36525c_dtc_139),
	&PKTINFO(a04s_nt36525c_dtc_140),
	&PKTINFO(a04s_nt36525c_dtc_141),
	&PKTINFO(a04s_nt36525c_dtc_142),
	&PKTINFO(a04s_nt36525c_dtc_143),
	&PKTINFO(a04s_nt36525c_dtc_144),
	&PKTINFO(a04s_nt36525c_dtc_145),
	&PKTINFO(a04s_nt36525c_dtc_146),
	&PKTINFO(a04s_nt36525c_dtc_147),
	&PKTINFO(a04s_nt36525c_dtc_148),
	&PKTINFO(a04s_nt36525c_dtc_149),
	&PKTINFO(a04s_nt36525c_dtc_150),
	&PKTINFO(a04s_nt36525c_dtc_151),
	&PKTINFO(a04s_nt36525c_dtc_152),
	&PKTINFO(a04s_nt36525c_dtc_153),
	&PKTINFO(a04s_nt36525c_dtc_154),
	&PKTINFO(a04s_nt36525c_dtc_155),
	&PKTINFO(a04s_nt36525c_dtc_156),
	&PKTINFO(a04s_nt36525c_dtc_157),
	&PKTINFO(a04s_nt36525c_dtc_158),
	&PKTINFO(a04s_nt36525c_dtc_159),
	&PKTINFO(a04s_nt36525c_dtc_160),
	&PKTINFO(a04s_nt36525c_dtc_161),
	&PKTINFO(a04s_nt36525c_dtc_162),
	&PKTINFO(a04s_nt36525c_dtc_163),
	&PKTINFO(a04s_nt36525c_dtc_164),
	&PKTINFO(a04s_nt36525c_dtc_165),
	&PKTINFO(a04s_nt36525c_dtc_166),
	&PKTINFO(a04s_nt36525c_dtc_167),
	&PKTINFO(a04s_nt36525c_dtc_168),
	&PKTINFO(a04s_nt36525c_dtc_169),
	&PKTINFO(a04s_nt36525c_dtc_170),
	&PKTINFO(a04s_sleep_out),
	&DLYINFO(a04s_wait_100msec),
};

static void *a04s_res_init_cmdtbl[] = {
	&nt36525c_dtc_restbl[RES_ID],
};

static void *a04s_set_bl_cmdtbl[] = {
	&PKTINFO(a04s_brightness), //51h
};

static void *a04s_display_on_cmdtbl[] = {
	&PKTINFO(a04s_display_on),
	&DLYINFO(a04s_wait_40msec),
	&PKTINFO(a04s_brightness_mode),
};

static void *a04s_display_off_cmdtbl[] = {
	&PKTINFO(a04s_display_off),
	&DLYINFO(a04s_wait_display_off),
};

static void *a04s_exit_cmdtbl[] = {
	&PKTINFO(a04s_sleep_in),
};

/* ===================================================================================== */
/* ================================= [EA8076 I2C TABLE] ===========+++================= */
/* ===================================================================================== */
static u8 NT36525C_A04S_I2C_INIT[] = {
	0x0C, 0x2C,
	0x0D, 0x26,
	0x0E, 0x26,
	0x09, 0x99,
	0x02, 0x6B,
	0x03, 0x0D,
	0x11, 0x74,
	0x04, 0x05,
	0x05, 0xCC,
	0x10, 0x67,
	0x08, 0x13,
};
static u8 NT36525C_A04S_I2C_EXIT_VSP[] = {
	0x09, 0x18,
};

static u8 NT36525C_A04S_I2C_EXIT_BLEN[] = {
	0x08, 0x00,
};

static u8 NT36525C_A04S_I2C_DUMP[] = {
	0x0C, 0x2C,
	0x0D, 0x26,
	0x0E, 0x26,
	0x09, 0xBE,
	0x02, 0x6B,
	0x03, 0x0D,
	0x11, 0x74,
	0x04, 0x05,
	0x05, 0xCC,
	0x10, 0x67,
	0x08, 0x13,
};

static DEFINE_STATIC_PACKET(nt36525c_dtc_a04s_i2c_init, I2C_PKT_TYPE_WR, NT36525C_A04S_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(nt36525c_dtc_a04s_i2c_exit_vsp, I2C_PKT_TYPE_WR, NT36525C_A04S_I2C_EXIT_VSP, 0);
static DEFINE_STATIC_PACKET(nt36525c_dtc_a04s_i2c_exit_blen, I2C_PKT_TYPE_WR, NT36525C_A04S_I2C_EXIT_BLEN, 0);
static DEFINE_STATIC_PACKET(nt36525c_dtc_a04s_i2c_dump, I2C_PKT_TYPE_RD, NT36525C_A04S_I2C_DUMP, 0);

static void *nt36525c_dtc_a04s_init_cmdtbl[] = {
	&PKTINFO(nt36525c_dtc_a04s_i2c_init),
};

static void *nt36525c_dtc_a04s_exit_cmdtbl[] = {
	&PKTINFO(nt36525c_dtc_a04s_i2c_exit_vsp),
	&PKTINFO(nt36525c_dtc_a04s_i2c_exit_blen),
};

static void *nt36525c_dtc_a04s_dump_cmdtbl[] = {
	&PKTINFO(nt36525c_dtc_a04s_i2c_dump),
};

static struct seqinfo a04s_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a04s_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a04s_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a04s_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a04s_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a04s_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a04s_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", nt36525c_dtc_a04s_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", nt36525c_dtc_a04s_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", nt36525c_dtc_a04s_dump_cmdtbl),
#endif
};

struct common_panel_info nt36525c_dtc_a04s_default_panel_info = {
	.ldi_name = "nt36525c_dtc",
	.name = "nt36525c_dtc_a04s_default",
	.model = "DTC_6_517_inch",
	.vendor = "DTC",
	.id = 0x5A8240,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
		.delay_cmd = 0xFF,
		.delay_duration = 1,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(nt36525c_dtc_a04s_resol),
		.resol = nt36525c_dtc_a04s_resol,
	},
	.maptbl = a04s_maptbl,
	.nr_maptbl = ARRAY_SIZE(a04s_maptbl),
	.seqtbl = a04s_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a04s_seqtbl),
	.rditbl = nt36525c_dtc_rditbl,
	.nr_rditbl = ARRAY_SIZE(nt36525c_dtc_rditbl),
	.restbl = nt36525c_dtc_restbl,
	.nr_restbl = ARRAY_SIZE(nt36525c_dtc_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&nt36525c_dtc_a04s_panel_dimming_info,
	},
	.i2c_data = &nt36525c_dtc_a04s_i2c_data,
};

static int __init nt36525c_dtc_a04s_panel_init(void)
{
	register_common_panel(&nt36525c_dtc_a04s_default_panel_info);

	return 0;
}
arch_initcall(nt36525c_dtc_a04s_panel_init)
#endif /* __NT36525C_A04S_PANEL_H__ */
