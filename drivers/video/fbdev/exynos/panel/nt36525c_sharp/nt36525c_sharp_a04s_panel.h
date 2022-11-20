/*
 * linux/drivers/video/fbdev/exynos/panel/nt36525c_sharp/nt36525c_sharp_a04s_panel.h
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
#include "nt36525c_sharp.h"

#include "nt36525c_sharp_a04s_panel_dimming.h"
#include "nt36525c_sharp_a04s_panel_i2c.h"

#include "nt36525c_sharp_a04s_resol.h"

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

static u8 A04S_NT36525C_001[] = {
	0xFF,
	0x20,
};

static u8 A04S_NT36525C_002[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_003[] = {
	0x00,
	0x01,
};

static u8 A04S_NT36525C_004[] = {
	0x01,
	0x55,
};

static u8 A04S_NT36525C_005[] = {
	0x03,
	0x55,
};

static u8 A04S_NT36525C_006[] = {
	0x05,
	0xB1,
};

static u8 A04S_NT36525C_007[] = {
	0x07,
	0x69,
};

static u8 A04S_NT36525C_008[] = {
	0x08,
	0xDF,
};

static u8 A04S_NT36525C_009[] = {
	0x0E,
	0x87,
};

static u8 A04S_NT36525C_010[] = {
	0x0F,
	0x7D,
};

static u8 A04S_NT36525C_011[] = {
	0x1F,
	0x00,
};

static u8 A04S_NT36525C_012[] = {
	0x69,
	0xA9,
};

static u8 A04S_NT36525C_013[] = {
	0x94,
	0x00,
};

static u8 A04S_NT36525C_014[] = {
	0x95,
	0xD7,
};

static u8 A04S_NT36525C_015[] = {
	0x96,
	0xD7,
};

static u8 A04S_NT36525C_016[] = {
	0xFF,
	0x24,
};

static u8 A04S_NT36525C_017[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_018[] = {
	0x00,
	0x1E,
};

static u8 A04S_NT36525C_019[] = {
	0x02,
	0x1E,
};

static u8 A04S_NT36525C_020[] = {
	0x06,
	0x08,
};

static u8 A04S_NT36525C_021[] = {
	0x07,
	0x04,
};

static u8 A04S_NT36525C_022[] = {
	0x08,
	0x20,
};

static u8 A04S_NT36525C_023[] = {
	0x0A,
	0x0F,
};

static u8 A04S_NT36525C_024[] = {
	0x0B,
	0x0E,
};

static u8 A04S_NT36525C_025[] = {
	0x0C,
	0x0D,
};

static u8 A04S_NT36525C_026[] = {
	0x0D,
	0x0C,
};

static u8 A04S_NT36525C_027[] = {
	0x12,
	0x1E,
};

static u8 A04S_NT36525C_028[] = {
	0x13,
	0x1E,
};

static u8 A04S_NT36525C_029[] = {
	0x14,
	0x1E,
};

static u8 A04S_NT36525C_030[] = {
	0x16,
	0x1E,
};

static u8 A04S_NT36525C_031[] = {
	0x18,
	0x1E,
};

static u8 A04S_NT36525C_032[] = {
	0x1C,
	0x08,
};

static u8 A04S_NT36525C_033[] = {
	0x1D,
	0x04,
};

static u8 A04S_NT36525C_034[] = {
	0x1E,
	0x20,
};

static u8 A04S_NT36525C_035[] = {
	0x20,
	0x0F,
};

static u8 A04S_NT36525C_036[] = {
	0x21,
	0x0E,
};

static u8 A04S_NT36525C_037[] = {
	0x22,
	0x0D,
};

static u8 A04S_NT36525C_038[] = {
	0x23,
	0x0C,
};

static u8 A04S_NT36525C_039[] = {
	0x28,
	0x1E,
};

static u8 A04S_NT36525C_040[] = {
	0x29,
	0x1E,
};

static u8 A04S_NT36525C_041[] = {
	0x2A,
	0x1E,
};

static u8 A04S_NT36525C_042[] = {
	0x2F,
	0x06,
};

static u8 A04S_NT36525C_043[] = {
	0x37,
	0x66,
};

static u8 A04S_NT36525C_044[] = {
	0x39,
	0x00,
};

static u8 A04S_NT36525C_045[] = {
	0x3A,
	0x82,
};

static u8 A04S_NT36525C_046[] = {
	0x3B,
	0x90,
};

static u8 A04S_NT36525C_047[] = {
	0x3D,
	0x91,
};

static u8 A04S_NT36525C_048[] = {
	0x3F,
	0x47,
};

static u8 A04S_NT36525C_049[] = {
	0x47,
	0x44,
};

static u8 A04S_NT36525C_050[] = {
	0x49,
	0x00,
};

static u8 A04S_NT36525C_051[] = {
	0x4A,
	0x82,
};

static u8 A04S_NT36525C_052[] = {
	0x4B,
	0x90,
};

static u8 A04S_NT36525C_053[] = {
	0x4C,
	0x91,
};

static u8 A04S_NT36525C_054[] = {
	0x4D,
	0x12,
};

static u8 A04S_NT36525C_055[] = {
	0x4E,
	0x34,
};

static u8 A04S_NT36525C_056[] = {
	0x55,
	0x83,
};

static u8 A04S_NT36525C_057[] = {
	0x56,
	0x44,
};

static u8 A04S_NT36525C_058[] = {
	0x5A,
	0x82,
};

static u8 A04S_NT36525C_059[] = {
	0x5B,
	0x90,
};

static u8 A04S_NT36525C_060[] = {
	0x5C,
	0x00,
};

static u8 A04S_NT36525C_061[] = {
	0x5D,
	0x00,
};

static u8 A04S_NT36525C_062[] = {
	0x5E,
	0x06,
};

static u8 A04S_NT36525C_063[] = {
	0x5F,
	0x00,
};

static u8 A04S_NT36525C_064[] = {
	0x60,
	0x80,
};

static u8 A04S_NT36525C_065[] = {
	0x61,
	0x90,
};

static u8 A04S_NT36525C_066[] = {
	0x64,
	0x11,
};

static u8 A04S_NT36525C_067[] = {
	0x85,
	0x00,
};

static u8 A04S_NT36525C_068[] = {
	0x86,
	0x51,
};

static u8 A04S_NT36525C_069[] = {
	0x92,
	0x91,
};

static u8 A04S_NT36525C_070[] = {
	0x93,
	0x0D,
};

static u8 A04S_NT36525C_071[] = {
	0x94,
	0x08,
};

static u8 A04S_NT36525C_072[] = {
	0xAB,
	0x00,
};

static u8 A04S_NT36525C_073[] = {
	0xAC,
	0x00,
};

static u8 A04S_NT36525C_074[] = {
	0xAD,
	0x00,
};

static u8 A04S_NT36525C_075[] = {
	0xAF,
	0x04,
};

static u8 A04S_NT36525C_076[] = {
	0xB0,
	0x05,
};

static u8 A04S_NT36525C_077[] = {
	0xB1,
	0x8C,
};

static u8 A04S_NT36525C_078[] = {
	0xC2,
	0x86,
};

static u8 A04S_NT36525C_079[] = {
	0xFF,
	0x25,
};

static u8 A04S_NT36525C_080[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_081[] = {
	0x02,
	0x10,
};

static u8 A04S_NT36525C_082[] = {
	0x0A,
	0x81,
};

static u8 A04S_NT36525C_083[] = {
	0x0B,
	0xBD,
};

static u8 A04S_NT36525C_084[] = {
	0x0C,
	0x01,
};

static u8 A04S_NT36525C_085[] = {
	0x17,
	0x82,
};

static u8 A04S_NT36525C_086[] = {
	0x18,
	0x06,
};

static u8 A04S_NT36525C_087[] = {
	0x19,
	0x0F,
};

static u8 A04S_NT36525C_088[] = {
	0x58,
	0x00,
};

static u8 A04S_NT36525C_089[] = {
	0x59,
	0x00,
};

static u8 A04S_NT36525C_090[] = {
	0x5A,
	0x40,
};

static u8 A04S_NT36525C_091[] = {
	0x5B,
	0x80,
};

static u8 A04S_NT36525C_092[] = {
	0x5C,
	0x00,
};

static u8 A04S_NT36525C_093[] = {
	0x5D,
	0x82,
};

static u8 A04S_NT36525C_094[] = {
	0x5E,
	0x90,
};

static u8 A04S_NT36525C_095[] = {
	0x5F,
	0x82,
};

static u8 A04S_NT36525C_096[] = {
	0x60,
	0x90,
};

static u8 A04S_NT36525C_097[] = {
	0x61,
	0x82,
};

static u8 A04S_NT36525C_098[] = {
	0x62,
	0x90,
};

static u8 A04S_NT36525C_099[] = {
	0x65,
	0x05,
};

static u8 A04S_NT36525C_100[] = {
	0x66,
	0x8C,
};

static u8 A04S_NT36525C_101[] = {
	0xC0,
	0x03,
};

static u8 A04S_NT36525C_102[] = {
	0xCA,
	0x1C,
};

static u8 A04S_NT36525C_103[] = {
	0xCB,
	0x18,
};

static u8 A04S_NT36525C_104[] = {
	0xCC,
	0x1C,
};

static u8 A04S_NT36525C_105[] = {
	0xD4,
	0xCC,
};

static u8 A04S_NT36525C_106[] = {
	0xD5,
	0x11,
};

static u8 A04S_NT36525C_107[] = {
	0xD6,
	0x18,
};

static u8 A04S_NT36525C_108[] = {
	0xFF,
	0x26,
};

static u8 A04S_NT36525C_109[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_110[] = {
	0x00,
	0xA0,
};

static u8 A04S_NT36525C_111[] = {
	0xFF,
	0x27,
};

static u8 A04S_NT36525C_112[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_113[] = {
	0x13,
	0x00,
};

static u8 A04S_NT36525C_114[] = {
	0x15,
	0xB4,
};

static u8 A04S_NT36525C_115[] = {
	0x1F,
	0x55,
};

static u8 A04S_NT36525C_116[] = {
	0x26,
	0x0F,
};

static u8 A04S_NT36525C_117[] = {
	0xC0,
	0x19,
};

static u8 A04S_NT36525C_118[] = {
	0xC1,
	0x3A,
};

static u8 A04S_NT36525C_119[] = {
	0xC2,
	0x10,
};

static u8 A04S_NT36525C_120[] = {
	0xC3,
	0x00,
};

static u8 A04S_NT36525C_121[] = {
	0xC4,
	0x3A,
};

static u8 A04S_NT36525C_122[] = {
	0xC5,
	0x00,
};

static u8 A04S_NT36525C_123[] = {
	0xC6,
	0x00,
};

static u8 A04S_NT36525C_124[] = {
	0xC7,
	0x03,
};

static u8 A04S_NT36525C_125[] = {
	0xFF,
	0x23,
};

static u8 A04S_NT36525C_126[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_127[] = {
	0x08,
	0x14,
};

static u8 A04S_NT36525C_128[] = {
	0x09,
	0xE4,
};

static u8 A04S_NT36525C_129[] = {
	0x12,
	0xB4,
};

static u8 A04S_NT36525C_130[] = {
	0x15,
	0xE9,
};

static u8 A04S_NT36525C_131[] = {
	0x16,
	0x0B,
};

static u8 A04S_NT36525C_132[] = {
	0xFF,
	0x10,
};

static u8 A04S_NT36525C_133[] = {
	0xFB,
	0x01,
};

static u8 A04S_NT36525C_134[] = {
	0xBA,
	0x02,
};

static u8 A04S_NT36525C_135[] = {
	0xBB,
	0x13,
};

static u8 A04S_NT36525C_136[] = {
	0x53,
	0x2C,
};

static DEFINE_STATIC_PACKET(a04s_sleep_out, DSI_PKT_TYPE_WR, A04S_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a04s_sleep_in, DSI_PKT_TYPE_WR, A04S_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a04s_display_on, DSI_PKT_TYPE_WR, A04S_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a04s_display_off, DSI_PKT_TYPE_WR, A04S_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(a04s_brightness_mode, DSI_PKT_TYPE_WR, A04S_BRIGHTNESS_MODE, 0);

static DEFINE_PKTUI(a04s_brightness, &a04s_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a04s_brightness, DSI_PKT_TYPE_WR, A04S_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_001, DSI_PKT_TYPE_WR, A04S_NT36525C_001, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_002, DSI_PKT_TYPE_WR, A04S_NT36525C_002, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_003, DSI_PKT_TYPE_WR, A04S_NT36525C_003, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_004, DSI_PKT_TYPE_WR, A04S_NT36525C_004, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_005, DSI_PKT_TYPE_WR, A04S_NT36525C_005, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_006, DSI_PKT_TYPE_WR, A04S_NT36525C_006, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_007, DSI_PKT_TYPE_WR, A04S_NT36525C_007, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_008, DSI_PKT_TYPE_WR, A04S_NT36525C_008, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_009, DSI_PKT_TYPE_WR, A04S_NT36525C_009, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_010, DSI_PKT_TYPE_WR, A04S_NT36525C_010, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_011, DSI_PKT_TYPE_WR, A04S_NT36525C_011, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_012, DSI_PKT_TYPE_WR, A04S_NT36525C_012, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_013, DSI_PKT_TYPE_WR, A04S_NT36525C_013, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_014, DSI_PKT_TYPE_WR, A04S_NT36525C_014, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_015, DSI_PKT_TYPE_WR, A04S_NT36525C_015, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_016, DSI_PKT_TYPE_WR, A04S_NT36525C_016, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_017, DSI_PKT_TYPE_WR, A04S_NT36525C_017, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_018, DSI_PKT_TYPE_WR, A04S_NT36525C_018, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_019, DSI_PKT_TYPE_WR, A04S_NT36525C_019, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_020, DSI_PKT_TYPE_WR, A04S_NT36525C_020, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_021, DSI_PKT_TYPE_WR, A04S_NT36525C_021, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_022, DSI_PKT_TYPE_WR, A04S_NT36525C_022, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_023, DSI_PKT_TYPE_WR, A04S_NT36525C_023, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_024, DSI_PKT_TYPE_WR, A04S_NT36525C_024, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_025, DSI_PKT_TYPE_WR, A04S_NT36525C_025, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_026, DSI_PKT_TYPE_WR, A04S_NT36525C_026, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_027, DSI_PKT_TYPE_WR, A04S_NT36525C_027, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_028, DSI_PKT_TYPE_WR, A04S_NT36525C_028, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_029, DSI_PKT_TYPE_WR, A04S_NT36525C_029, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_030, DSI_PKT_TYPE_WR, A04S_NT36525C_030, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_031, DSI_PKT_TYPE_WR, A04S_NT36525C_031, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_032, DSI_PKT_TYPE_WR, A04S_NT36525C_032, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_033, DSI_PKT_TYPE_WR, A04S_NT36525C_033, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_034, DSI_PKT_TYPE_WR, A04S_NT36525C_034, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_035, DSI_PKT_TYPE_WR, A04S_NT36525C_035, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_036, DSI_PKT_TYPE_WR, A04S_NT36525C_036, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_037, DSI_PKT_TYPE_WR, A04S_NT36525C_037, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_038, DSI_PKT_TYPE_WR, A04S_NT36525C_038, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_039, DSI_PKT_TYPE_WR, A04S_NT36525C_039, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_040, DSI_PKT_TYPE_WR, A04S_NT36525C_040, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_041, DSI_PKT_TYPE_WR, A04S_NT36525C_041, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_042, DSI_PKT_TYPE_WR, A04S_NT36525C_042, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_043, DSI_PKT_TYPE_WR, A04S_NT36525C_043, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_044, DSI_PKT_TYPE_WR, A04S_NT36525C_044, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_045, DSI_PKT_TYPE_WR, A04S_NT36525C_045, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_046, DSI_PKT_TYPE_WR, A04S_NT36525C_046, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_047, DSI_PKT_TYPE_WR, A04S_NT36525C_047, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_048, DSI_PKT_TYPE_WR, A04S_NT36525C_048, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_049, DSI_PKT_TYPE_WR, A04S_NT36525C_049, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_050, DSI_PKT_TYPE_WR, A04S_NT36525C_050, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_051, DSI_PKT_TYPE_WR, A04S_NT36525C_051, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_052, DSI_PKT_TYPE_WR, A04S_NT36525C_052, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_053, DSI_PKT_TYPE_WR, A04S_NT36525C_053, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_054, DSI_PKT_TYPE_WR, A04S_NT36525C_054, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_055, DSI_PKT_TYPE_WR, A04S_NT36525C_055, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_056, DSI_PKT_TYPE_WR, A04S_NT36525C_056, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_057, DSI_PKT_TYPE_WR, A04S_NT36525C_057, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_058, DSI_PKT_TYPE_WR, A04S_NT36525C_058, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_059, DSI_PKT_TYPE_WR, A04S_NT36525C_059, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_060, DSI_PKT_TYPE_WR, A04S_NT36525C_060, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_061, DSI_PKT_TYPE_WR, A04S_NT36525C_061, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_062, DSI_PKT_TYPE_WR, A04S_NT36525C_062, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_063, DSI_PKT_TYPE_WR, A04S_NT36525C_063, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_064, DSI_PKT_TYPE_WR, A04S_NT36525C_064, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_065, DSI_PKT_TYPE_WR, A04S_NT36525C_065, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_066, DSI_PKT_TYPE_WR, A04S_NT36525C_066, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_067, DSI_PKT_TYPE_WR, A04S_NT36525C_067, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_068, DSI_PKT_TYPE_WR, A04S_NT36525C_068, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_069, DSI_PKT_TYPE_WR, A04S_NT36525C_069, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_070, DSI_PKT_TYPE_WR, A04S_NT36525C_070, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_071, DSI_PKT_TYPE_WR, A04S_NT36525C_071, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_072, DSI_PKT_TYPE_WR, A04S_NT36525C_072, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_073, DSI_PKT_TYPE_WR, A04S_NT36525C_073, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_074, DSI_PKT_TYPE_WR, A04S_NT36525C_074, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_075, DSI_PKT_TYPE_WR, A04S_NT36525C_075, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_076, DSI_PKT_TYPE_WR, A04S_NT36525C_076, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_077, DSI_PKT_TYPE_WR, A04S_NT36525C_077, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_078, DSI_PKT_TYPE_WR, A04S_NT36525C_078, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_079, DSI_PKT_TYPE_WR, A04S_NT36525C_079, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_080, DSI_PKT_TYPE_WR, A04S_NT36525C_080, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_081, DSI_PKT_TYPE_WR, A04S_NT36525C_081, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_082, DSI_PKT_TYPE_WR, A04S_NT36525C_082, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_083, DSI_PKT_TYPE_WR, A04S_NT36525C_083, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_084, DSI_PKT_TYPE_WR, A04S_NT36525C_084, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_085, DSI_PKT_TYPE_WR, A04S_NT36525C_085, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_086, DSI_PKT_TYPE_WR, A04S_NT36525C_086, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_087, DSI_PKT_TYPE_WR, A04S_NT36525C_087, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_088, DSI_PKT_TYPE_WR, A04S_NT36525C_088, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_089, DSI_PKT_TYPE_WR, A04S_NT36525C_089, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_090, DSI_PKT_TYPE_WR, A04S_NT36525C_090, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_091, DSI_PKT_TYPE_WR, A04S_NT36525C_091, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_092, DSI_PKT_TYPE_WR, A04S_NT36525C_092, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_093, DSI_PKT_TYPE_WR, A04S_NT36525C_093, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_094, DSI_PKT_TYPE_WR, A04S_NT36525C_094, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_095, DSI_PKT_TYPE_WR, A04S_NT36525C_095, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_096, DSI_PKT_TYPE_WR, A04S_NT36525C_096, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_097, DSI_PKT_TYPE_WR, A04S_NT36525C_097, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_098, DSI_PKT_TYPE_WR, A04S_NT36525C_098, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_099, DSI_PKT_TYPE_WR, A04S_NT36525C_099, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_100, DSI_PKT_TYPE_WR, A04S_NT36525C_100, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_101, DSI_PKT_TYPE_WR, A04S_NT36525C_101, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_102, DSI_PKT_TYPE_WR, A04S_NT36525C_102, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_103, DSI_PKT_TYPE_WR, A04S_NT36525C_103, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_104, DSI_PKT_TYPE_WR, A04S_NT36525C_104, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_105, DSI_PKT_TYPE_WR, A04S_NT36525C_105, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_106, DSI_PKT_TYPE_WR, A04S_NT36525C_106, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_107, DSI_PKT_TYPE_WR, A04S_NT36525C_107, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_108, DSI_PKT_TYPE_WR, A04S_NT36525C_108, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_109, DSI_PKT_TYPE_WR, A04S_NT36525C_109, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_110, DSI_PKT_TYPE_WR, A04S_NT36525C_110, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_111, DSI_PKT_TYPE_WR, A04S_NT36525C_111, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_112, DSI_PKT_TYPE_WR, A04S_NT36525C_112, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_113, DSI_PKT_TYPE_WR, A04S_NT36525C_113, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_114, DSI_PKT_TYPE_WR, A04S_NT36525C_114, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_115, DSI_PKT_TYPE_WR, A04S_NT36525C_115, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_116, DSI_PKT_TYPE_WR, A04S_NT36525C_116, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_117, DSI_PKT_TYPE_WR, A04S_NT36525C_117, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_118, DSI_PKT_TYPE_WR, A04S_NT36525C_118, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_119, DSI_PKT_TYPE_WR, A04S_NT36525C_119, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_120, DSI_PKT_TYPE_WR, A04S_NT36525C_120, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_121, DSI_PKT_TYPE_WR, A04S_NT36525C_121, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_122, DSI_PKT_TYPE_WR, A04S_NT36525C_122, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_123, DSI_PKT_TYPE_WR, A04S_NT36525C_123, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_124, DSI_PKT_TYPE_WR, A04S_NT36525C_124, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_125, DSI_PKT_TYPE_WR, A04S_NT36525C_125, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_126, DSI_PKT_TYPE_WR, A04S_NT36525C_126, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_127, DSI_PKT_TYPE_WR, A04S_NT36525C_127, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_128, DSI_PKT_TYPE_WR, A04S_NT36525C_128, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_129, DSI_PKT_TYPE_WR, A04S_NT36525C_129, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_130, DSI_PKT_TYPE_WR, A04S_NT36525C_130, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_131, DSI_PKT_TYPE_WR, A04S_NT36525C_131, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_132, DSI_PKT_TYPE_WR, A04S_NT36525C_132, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_133, DSI_PKT_TYPE_WR, A04S_NT36525C_133, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_134, DSI_PKT_TYPE_WR, A04S_NT36525C_134, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_135, DSI_PKT_TYPE_WR, A04S_NT36525C_135, 0);
static DEFINE_STATIC_PACKET(a04s_nt36525c_sharp_136, DSI_PKT_TYPE_WR, A04S_NT36525C_136, 0);

static DEFINE_PANEL_MDELAY(a04s_wait_display_off, 20);
static DEFINE_PANEL_MDELAY(a04s_wait_sleep_in, 4 * 17); /* 4 frame */
static DEFINE_PANEL_MDELAY(a04s_wait_2msec, 2);
static DEFINE_PANEL_MDELAY(a04s_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(a04s_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(a04s_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(a04s_wait_blic_off, 1);


static void *a04s_init_cmdtbl[] = {
	&nt36525c_sharp_restbl[RES_ID],
	&PKTINFO(a04s_nt36525c_sharp_001),
	&DLYINFO(a04s_wait_1msec),
	&PKTINFO(a04s_nt36525c_sharp_002),
	&PKTINFO(a04s_nt36525c_sharp_003),
	&PKTINFO(a04s_nt36525c_sharp_004),
	&PKTINFO(a04s_nt36525c_sharp_005),
	&PKTINFO(a04s_nt36525c_sharp_006),
	&PKTINFO(a04s_nt36525c_sharp_007),
	&PKTINFO(a04s_nt36525c_sharp_008),
	&PKTINFO(a04s_nt36525c_sharp_009),
	&PKTINFO(a04s_nt36525c_sharp_010),
	&PKTINFO(a04s_nt36525c_sharp_011),
	&PKTINFO(a04s_nt36525c_sharp_012),
	&PKTINFO(a04s_nt36525c_sharp_013),
	&PKTINFO(a04s_nt36525c_sharp_014),
	&PKTINFO(a04s_nt36525c_sharp_015),
	&PKTINFO(a04s_nt36525c_sharp_016),
	&DLYINFO(a04s_wait_1msec),
	&PKTINFO(a04s_nt36525c_sharp_017),
	&PKTINFO(a04s_nt36525c_sharp_018),
	&PKTINFO(a04s_nt36525c_sharp_019),
	&PKTINFO(a04s_nt36525c_sharp_020),
	&PKTINFO(a04s_nt36525c_sharp_021),
	&PKTINFO(a04s_nt36525c_sharp_022),
	&PKTINFO(a04s_nt36525c_sharp_023),
	&PKTINFO(a04s_nt36525c_sharp_024),
	&PKTINFO(a04s_nt36525c_sharp_025),
	&PKTINFO(a04s_nt36525c_sharp_026),
	&PKTINFO(a04s_nt36525c_sharp_027),
	&PKTINFO(a04s_nt36525c_sharp_028),
	&PKTINFO(a04s_nt36525c_sharp_029),
	&PKTINFO(a04s_nt36525c_sharp_030),
	&PKTINFO(a04s_nt36525c_sharp_031),
	&PKTINFO(a04s_nt36525c_sharp_032),
	&PKTINFO(a04s_nt36525c_sharp_033),
	&PKTINFO(a04s_nt36525c_sharp_034),
	&PKTINFO(a04s_nt36525c_sharp_035),
	&PKTINFO(a04s_nt36525c_sharp_036),
	&PKTINFO(a04s_nt36525c_sharp_037),
	&PKTINFO(a04s_nt36525c_sharp_038),
	&PKTINFO(a04s_nt36525c_sharp_039),
	&PKTINFO(a04s_nt36525c_sharp_040),
	&PKTINFO(a04s_nt36525c_sharp_041),
	&PKTINFO(a04s_nt36525c_sharp_042),
	&PKTINFO(a04s_nt36525c_sharp_043),
	&PKTINFO(a04s_nt36525c_sharp_044),
	&PKTINFO(a04s_nt36525c_sharp_045),
	&PKTINFO(a04s_nt36525c_sharp_046),
	&PKTINFO(a04s_nt36525c_sharp_047),
	&PKTINFO(a04s_nt36525c_sharp_048),
	&PKTINFO(a04s_nt36525c_sharp_049),
	&PKTINFO(a04s_nt36525c_sharp_050),
	&PKTINFO(a04s_nt36525c_sharp_051),
	&PKTINFO(a04s_nt36525c_sharp_052),
	&PKTINFO(a04s_nt36525c_sharp_053),
	&PKTINFO(a04s_nt36525c_sharp_054),
	&PKTINFO(a04s_nt36525c_sharp_055),
	&PKTINFO(a04s_nt36525c_sharp_056),
	&PKTINFO(a04s_nt36525c_sharp_057),
	&PKTINFO(a04s_nt36525c_sharp_058),
	&PKTINFO(a04s_nt36525c_sharp_059),
	&PKTINFO(a04s_nt36525c_sharp_060),
	&PKTINFO(a04s_nt36525c_sharp_061),
	&PKTINFO(a04s_nt36525c_sharp_062),
	&PKTINFO(a04s_nt36525c_sharp_063),
	&PKTINFO(a04s_nt36525c_sharp_064),
	&PKTINFO(a04s_nt36525c_sharp_065),
	&PKTINFO(a04s_nt36525c_sharp_066),
	&PKTINFO(a04s_nt36525c_sharp_067),
	&PKTINFO(a04s_nt36525c_sharp_068),
	&PKTINFO(a04s_nt36525c_sharp_069),
	&PKTINFO(a04s_nt36525c_sharp_070),
	&PKTINFO(a04s_nt36525c_sharp_071),
	&PKTINFO(a04s_nt36525c_sharp_072),
	&PKTINFO(a04s_nt36525c_sharp_073),
	&PKTINFO(a04s_nt36525c_sharp_074),
	&PKTINFO(a04s_nt36525c_sharp_075),
	&PKTINFO(a04s_nt36525c_sharp_076),
	&PKTINFO(a04s_nt36525c_sharp_077),
	&PKTINFO(a04s_nt36525c_sharp_078),
	&PKTINFO(a04s_nt36525c_sharp_079),
	&DLYINFO(a04s_wait_1msec),
	&PKTINFO(a04s_nt36525c_sharp_080),
	&PKTINFO(a04s_nt36525c_sharp_081),
	&PKTINFO(a04s_nt36525c_sharp_082),
	&PKTINFO(a04s_nt36525c_sharp_083),
	&PKTINFO(a04s_nt36525c_sharp_084),
	&PKTINFO(a04s_nt36525c_sharp_085),
	&PKTINFO(a04s_nt36525c_sharp_086),
	&PKTINFO(a04s_nt36525c_sharp_087),
	&PKTINFO(a04s_nt36525c_sharp_088),
	&PKTINFO(a04s_nt36525c_sharp_089),
	&PKTINFO(a04s_nt36525c_sharp_090),
	&PKTINFO(a04s_nt36525c_sharp_091),
	&PKTINFO(a04s_nt36525c_sharp_092),
	&PKTINFO(a04s_nt36525c_sharp_093),
	&PKTINFO(a04s_nt36525c_sharp_094),
	&PKTINFO(a04s_nt36525c_sharp_095),
	&PKTINFO(a04s_nt36525c_sharp_096),
	&PKTINFO(a04s_nt36525c_sharp_097),
	&PKTINFO(a04s_nt36525c_sharp_098),
	&PKTINFO(a04s_nt36525c_sharp_099),
	&PKTINFO(a04s_nt36525c_sharp_100),
	&PKTINFO(a04s_nt36525c_sharp_101),
	&PKTINFO(a04s_nt36525c_sharp_102),
	&PKTINFO(a04s_nt36525c_sharp_103),
	&PKTINFO(a04s_nt36525c_sharp_104),
	&PKTINFO(a04s_nt36525c_sharp_105),
	&PKTINFO(a04s_nt36525c_sharp_106),
	&PKTINFO(a04s_nt36525c_sharp_107),
	&PKTINFO(a04s_nt36525c_sharp_108),
	&DLYINFO(a04s_wait_1msec),
	&PKTINFO(a04s_nt36525c_sharp_109),
	&PKTINFO(a04s_nt36525c_sharp_110),
	&PKTINFO(a04s_nt36525c_sharp_111),
	&DLYINFO(a04s_wait_1msec),
	&PKTINFO(a04s_nt36525c_sharp_112),
	&PKTINFO(a04s_nt36525c_sharp_113),
	&PKTINFO(a04s_nt36525c_sharp_114),
	&PKTINFO(a04s_nt36525c_sharp_115),
	&PKTINFO(a04s_nt36525c_sharp_116),
	&PKTINFO(a04s_nt36525c_sharp_117),
	&PKTINFO(a04s_nt36525c_sharp_118),
	&PKTINFO(a04s_nt36525c_sharp_119),
	&PKTINFO(a04s_nt36525c_sharp_120),
	&PKTINFO(a04s_nt36525c_sharp_121),
	&PKTINFO(a04s_nt36525c_sharp_122),
	&PKTINFO(a04s_nt36525c_sharp_123),
	&PKTINFO(a04s_nt36525c_sharp_124),
	&PKTINFO(a04s_nt36525c_sharp_125),
	&DLYINFO(a04s_wait_1msec),
	&PKTINFO(a04s_nt36525c_sharp_126),
	&PKTINFO(a04s_nt36525c_sharp_127),
	&PKTINFO(a04s_nt36525c_sharp_128),
	&PKTINFO(a04s_nt36525c_sharp_129),
	&PKTINFO(a04s_nt36525c_sharp_130),
	&PKTINFO(a04s_nt36525c_sharp_131),
	&PKTINFO(a04s_nt36525c_sharp_132),
	&DLYINFO(a04s_wait_1msec),
	&PKTINFO(a04s_nt36525c_sharp_133),
	&PKTINFO(a04s_nt36525c_sharp_134),
	&PKTINFO(a04s_nt36525c_sharp_135),
	&PKTINFO(a04s_nt36525c_sharp_136),
	&PKTINFO(a04s_sleep_out),
	&DLYINFO(a04s_wait_100msec),
};

static void *a04s_res_init_cmdtbl[] = {
	&nt36525c_sharp_restbl[RES_ID],
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
	0x09, 0xBE,
	0x02, 0x6B,
	0x03, 0x0D,
	0x11, 0x74,
	0x04, 0x05,
	0x05, 0xCC,
	0x10, 0x67,
	0x08, 0x13,
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

static DEFINE_STATIC_PACKET(nt36525c_sharp_a04s_i2c_init, I2C_PKT_TYPE_WR, NT36525C_A04S_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(nt36525c_sharp_a04s_i2c_exit_blen, I2C_PKT_TYPE_WR, NT36525C_A04S_I2C_EXIT_BLEN, 0);
static DEFINE_STATIC_PACKET(nt36525c_sharp_a04s_i2c_dump, I2C_PKT_TYPE_RD, NT36525C_A04S_I2C_DUMP, 0);

static void *nt36525c_sharp_a04s_init_cmdtbl[] = {
	&PKTINFO(nt36525c_sharp_a04s_i2c_init),
};

static void *nt36525c_sharp_a04s_exit_cmdtbl[] = {
	&PKTINFO(nt36525c_sharp_a04s_i2c_exit_blen),
};

static void *nt36525c_sharp_a04s_dump_cmdtbl[] = {
	&PKTINFO(nt36525c_sharp_a04s_i2c_dump),
};

static struct seqinfo a04s_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a04s_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a04s_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a04s_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a04s_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a04s_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a04s_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", nt36525c_sharp_a04s_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", nt36525c_sharp_a04s_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", nt36525c_sharp_a04s_dump_cmdtbl),
#endif
};

struct common_panel_info nt36525c_sharp_a04s_default_panel_info = {
	.ldi_name = "nt36525c_sharp",
	.name = "nt36525c_sharp_a04s_default",
	.model = "SHARP_6_517_inch",
	.vendor = "SHARP",
	.id = 0x2AF244,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(nt36525c_sharp_a04s_resol),
		.resol = nt36525c_sharp_a04s_resol,
	},
	.maptbl = a04s_maptbl,
	.nr_maptbl = ARRAY_SIZE(a04s_maptbl),
	.seqtbl = a04s_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a04s_seqtbl),
	.rditbl = nt36525c_sharp_rditbl,
	.nr_rditbl = ARRAY_SIZE(nt36525c_sharp_rditbl),
	.restbl = nt36525c_sharp_restbl,
	.nr_restbl = ARRAY_SIZE(nt36525c_sharp_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&nt36525c_sharp_a04s_panel_dimming_info,
	},
	.i2c_data = &nt36525c_sharp_a04s_i2c_data,
};

static int __init nt36525c_sharp_a04s_panel_init(void)
{
	register_common_panel(&nt36525c_sharp_a04s_default_panel_info);

	return 0;
}
arch_initcall(nt36525c_sharp_a04s_panel_init)
#endif /* __NT36525C_A04S_PANEL_H__ */
