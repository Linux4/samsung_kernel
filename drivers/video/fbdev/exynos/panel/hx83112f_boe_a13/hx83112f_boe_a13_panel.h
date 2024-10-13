/*
 * linux/drivers/video/fbdev/exynos/panel/hx83112f_boe/hx83112f_boe_a13_panel.h
 *
 * Header file for HX83112F Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HX83112F_A13_PANEL_H__
#define __HX83112F_A13_PANEL_H__
#include "../panel_drv.h"
#include "hx83112f_boe.h"

#include "hx83112f_boe_a13_panel_dimming.h"
#include "hx83112f_boe_a13_panel_i2c.h"

#include "hx83112f_boe_a13_resol.h"

#undef __pn_name__
#define __pn_name__	a13

#undef __PN_NAME__
#define __PN_NAME__	A13

static struct seqinfo a13_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [HX83112F READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [HX83112F RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [HX83112F MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a13_brt_table[HX83112F_TOTAL_NR_LUMINANCE][1] = {
	{0},
	{2}, {2}, {3}, {3}, {4}, {4}, {5}, {5}, {6}, {6},	/* 1: 2 */
	{7}, {7}, {8}, {8}, {9}, {9}, {10}, {10}, {11}, {11},
	{12}, {12}, {13}, {13}, {14}, {14}, {15}, {15}, {16}, {16},
	{17}, {17}, {18}, {18}, {19}, {20}, {20}, {21}, {21}, {22},
	{23}, {23}, {24}, {24}, {25}, {26}, {26}, {27}, {27}, {28},
	{29}, {29}, {30}, {30}, {31}, {32}, {32}, {33}, {33}, {34},
	{35}, {35}, {36}, {36}, {37}, {38}, {38}, {39}, {39}, {40},
	{41}, {41}, {42}, {42}, {43}, {44}, {44}, {45}, {45}, {46},
	{47}, {48}, {48}, {49}, {50}, {51}, {52}, {52}, {53}, {54},
	{55}, {56}, {56}, {57}, {58}, {59}, {59}, {60}, {61}, {62},
	{63}, {63}, {64}, {65}, {66}, {67}, {67}, {68}, {69}, {70},
	{71}, {71}, {72}, {73}, {74}, {75}, {75}, {76}, {77}, {78},
	{78}, {79}, {80}, {81}, {82}, {82}, {83}, {84}, {85}, {86},	/* 128: 84 */
	{87}, {88}, {89}, {90}, {91}, {92}, {93}, {94}, {95}, {96},
	{97}, {98}, {99}, {100}, {101}, {102}, {103}, {104}, {105}, {106},
	{107}, {108}, {109}, {110}, {111}, {112}, {113}, {114}, {115}, {117},
	{118}, {119}, {120}, {121}, {122}, {123}, {124}, {125}, {126}, {127},
	{128}, {129}, {130}, {131}, {132}, {133}, {134}, {135}, {136}, {137},
	{138}, {139}, {140}, {141}, {142}, {143}, {144}, {145}, {146}, {147},
	{148}, {149}, {150}, {151}, {152}, {153}, {154}, {155}, {156}, {157},
	{158}, {159}, {160}, {161}, {162}, {163}, {164}, {165}, {166}, {167},
	{168}, {169}, {170}, {171}, {172}, {173}, {174}, {175}, {176}, {177},
	{178}, {179}, {180}, {182}, {183}, {184}, {185}, {186}, {187}, {188},
	{189}, {190}, {191}, {192}, {193}, {194}, {195}, {196}, {197}, {198},
	{199}, {200}, {201}, {202}, {203}, {204}, {205}, {206}, {207}, {208},
	{209}, {210}, {211}, {212}, {213}, {214}, {215}, {216}, {216}, {217},	/* 255: 213 */
	{218}, {219}, {220}, {221}, {221}, {222}, {223}, {224}, {225}, {226},
	{226}, {227}, {228}, {229}, {230}, {231}, {232}, {232}, {233}, {234},
	{235}, {236}, {237}, {237}, {238}, {239}, {240}, {241}, {241}, {242},
	{243}, {244}, {245}, {245}, {246}, {247}, {248}, {249}, {249}, {250},
	{251}, {252}, {252}, {253}, {254}, {255},				/* 306: 255 */
};


static struct maptbl a13_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(a13_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [HX83112F COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A13_SLEEP_OUT[] = { 0x11 };
static u8 A13_SLEEP_IN[] = { 0x10 };
static u8 A13_DISPLAY_OFF[] = { 0x28 };
static u8 A13_DISPLAY_ON[] = { 0x29 };

static u8 A13_BRIGHTNESS[] = {
	0x51,
	0xFF,
};

static u8 A13_BRIGHTNESS_MODE[] = {
	0x53,
	0x24,
};

static u8 A13_HX83112F_001[] = {
	0xB9,
	0x83, 0x11, 0x2F,
};

static u8 A13_HX83112F_002[] = {
	0xB1,
	0x40, 0x2B, 0x2B, 0x80, 0x80, 0xA8, 0xA8, 0x38, 0x1E, 0x06,
	0x06, 0x1A,
};

static u8 A13_HX83112F_003[] = {
	0xBD,
	0x01,
};

static u8 A13_HX83112F_004[] = {
	0xB1,
	0xA4, 0x01,
};

static u8 A13_HX83112F_005[] = {
	0xBD,
	0x02,
};

static u8 A13_HX83112F_006[] = {
	0xB1,
	0x2D, 0x2D,
};

static u8 A13_HX83112F_007[] = {
	0xBD,
	0x00,
};

static u8 A13_HX83112F_008[] = {
	0xB2,
	0x00, 0x02, 0x08, 0x90, 0x68, 0x00, 0x1C, 0x4C, 0x10, 0x08,
	0x00, 0xBC, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x14,
};

static u8 A13_HX83112F_009[] = {
	0xB4,
	0x00, 0x00, 0x00, 0xBF, 0x00, 0x69, 0x00, 0xB8, 0x00, 0x69,
	0x00, 0x69, 0x00, 0xB8, 0x00, 0x00, 0x04, 0x05, 0x00, 0x2F,
	0x00, 0x03, 0x05, 0x11, 0x00, 0x00, 0x00, 0x00, 0x83, 0x00,
	0xFF, 0x00, 0xFF, 0x02, 0x01, 0x01,
};

static u8 A13_HX83112F_010[] = {
	0xBD,
	0x02,
};

static u8 A13_HX83112F_011[] = {
	0xB4,
	0x92, 0x44, 0x12, 0x00, 0x22, 0x22, 0x11, 0x12, 0x88, 0x12,
	0x42,
};

static u8 A13_HX83112F_012[] = {
	0xBD,
	0x00,
};

static u8 A13_HX83112F_013[] = {
	0xE9,
	0xC5,
};

static u8 A13_HX83112F_014[] = {
	0xBA,
	0x11,
};

static u8 A13_HX83112F_015[] = {
	0xE9,
	0xC9,
};

static u8 A13_HX83112F_016[] = {
	0xBA,
	0x09, 0x3D,
};

static u8 A13_HX83112F_017[] = {
	0xE9,
	0x3F,
};

static u8 A13_HX83112F_018[] = {
	0xE9,
	0xCF,
};

static u8 A13_HX83112F_019[] = {
	0xBA,
	0x01,
};

static u8 A13_HX83112F_020[] = {
	0xE9,
	0x3F,
};

static u8 A13_HX83112F_021[] = {
	0xBD,
	0x01,
};

static u8 A13_HX83112F_022[] = {
	0xE9,
	0xC5,
};

static u8 A13_HX83112F_023[] = {
	0xBA,
	0x04,
};

static u8 A13_HX83112F_024[] = {
	0xE9,
	0x3F,
};

static u8 A13_HX83112F_025[] = {
	0xBD,
	0x00,
};

static u8 A13_HX83112F_026[] = {
	0xC7,
	0xF0, 0x20,
};

static u8 A13_HX83112F_027[] = {
	0xCB,
	0x5F, 0x21, 0x0A, 0x59,
};

static u8 A13_HX83112F_028[] = {
	0xCC,
	0x0A,
};

static u8 A13_HX83112F_029[] = {
	0xD1,
	0x07,
};

static u8 A13_HX83112F_030[] = {
	0xD3,
	0x81, 0x14, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x03, 0x03,
	0x14, 0x14, 0x08, 0x08, 0x08, 0x08, 0x22, 0x18, 0x32, 0x10,
	0x1B, 0x00, 0x1B, 0x32, 0x10, 0x11, 0x00, 0x11, 0x32, 0x10,
	0x08, 0x00, 0x08, 0x00, 0x00, 0x1E, 0x09, 0x69,
};

static u8 A13_HX83112F_031[] = {
	0xBD,
	0x01,
};

static u8 A13_HX83112F_032[] = {
	0xD2,
	0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x30,
};

static u8 A13_HX83112F_033[] = {
	0xD3,
	0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x24, 0x08, 0x01, 0x19,
};

static u8 A13_HX83112F_034[] = {
	0xBD,
	0x02,
};

static u8 A13_HX83112F_035[] = {
	0xD3,
	0x01, 0x01, 0x00, 0x01, 0x00, 0x0A, 0x0A, 0x0A, 0x0A, 0x01,
	0x03, 0x00, 0x00, 0x08, 0x02, 0x11, 0x01, 0x00, 0x00, 0x01,
	0x05, 0x78, 0x10,
};

static u8 A13_HX83112F_036[] = {
	0xBD,
	0x00,
};

static u8 A13_HX83112F_037[] = {
	0xD5,
	0x18, 0x18, 0x18, 0x18, 0x41, 0x41, 0x41, 0x41, 0x18, 0x18,
	0x18, 0x18, 0x31, 0x31, 0x30, 0x30, 0x2F, 0x2F, 0x31, 0x31,
	0x30, 0x30, 0x2F, 0x2F, 0x40, 0x40, 0x01, 0x01, 0x00, 0x00,
	0x03, 0x03, 0x02, 0x02, 0x24, 0x24, 0xCE, 0xCE, 0xCE, 0xCE,
	0xC0, 0xC0, 0x18, 0x18, 0x21, 0x21, 0x20, 0x20,
};

static u8 A13_HX83112F_038[] = {
	0xD6,
	0x18, 0x18, 0x18, 0x18, 0x41, 0x41, 0x41, 0x41, 0x18, 0x18,
	0x18, 0x18, 0x31, 0x31, 0x30, 0x30, 0x2F, 0x2F, 0x31, 0x31,
	0x30, 0x30, 0x2F, 0x2F, 0x40, 0x40, 0x02, 0x02, 0x03, 0x03,
	0x00, 0x00, 0x01, 0x01, 0x24, 0x24, 0xCE, 0xCE, 0xCE, 0xCE,
	0x18, 0x18, 0xC0, 0xC0, 0x20, 0x20, 0x21, 0x21,
};

static u8 A13_HX83112F_039[] = {
	0xD8,
	0xAA, 0xAA, 0xAA, 0xBF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA, 0xBF,
	0xFF, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};

static u8 A13_HX83112F_040[] = {
	0xBD,
	0x01,
};

static u8 A13_HX83112F_041[] = {
	0xD8,
	0xAF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xAF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA,
};

static u8 A13_HX83112F_042[] = {
	0xBD,
	0x02,
};

static u8 A13_HX83112F_043[] = {
	0xD8,
	0xAF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xAF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF,
};

static u8 A13_HX83112F_044[] = {
	0xBD,
	0x03,
};

static u8 A13_HX83112F_045[] = {
	0xD8,
	0xAA, 0xAF, 0xFF, 0xEA, 0xAA, 0xAA, 0xAA, 0xAF, 0xFF, 0xEA,
	0xAA, 0xAA, 0xAA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF,
};

static u8 A13_HX83112F_046[] = {
	0xBD,
	0x00,
};

static u8 A13_HX83112F_047[] = {
	0xE0,
	0x00, 0x13, 0x2D, 0x33, 0x3B, 0x75, 0x85, 0x8B, 0x8E, 0x8E,
	0x8F, 0x8C, 0x89, 0x88, 0x86, 0x85, 0x85, 0x87, 0x89, 0xA2,
	0xB4, 0x50, 0x76, 0x00, 0x13, 0x2D, 0x33, 0x3B, 0x75, 0x85,
	0x8B, 0x8E, 0x8E, 0x8F, 0x8C, 0x89, 0x88, 0x86, 0x85, 0x85,
	0x87, 0x89, 0xA2, 0xB4, 0x50, 0x76,
};

static u8 A13_HX83112F_048[] = {
	0xE7,
	0x41, 0x23, 0x10, 0x10, 0x1D, 0xD1, 0x22, 0xD1, 0x21, 0xD1,
	0x0F, 0x02, 0x00, 0x00, 0x02, 0x02, 0x12, 0x05, 0xFF, 0xFF,
	0x22, 0xD1, 0x21, 0xD1, 0x08, 0x00, 0x00, 0x48, 0x17, 0x03,
	0x69, 0x00, 0x00, 0x00, 0x4C, 0x4C, 0x1E, 0x00, 0x40, 0x01,
	0x00, 0x0C, 0x0A, 0x04,
};

static u8 A13_HX83112F_049[] = {
	0xBD,
	0x01,
};

static u8 A13_HX83112F_050[] = {
	0xE7,
	0x02, 0x50, 0xE8, 0x01, 0x3C, 0x07, 0x2D, 0x10, 0x00, 0x00,
	0x00, 0x02,
};

static u8 A13_HX83112F_051[] = {
	0xBD,
	0x02,
};

static u8 A13_HX83112F_052[] = {
	0xE7,
	0x02, 0x02, 0x02, 0x00, 0xB8, 0x01, 0x03, 0x01, 0x03, 0x01,
	0x03, 0x22, 0x20, 0x28, 0x40, 0x01, 0x00,
};

static u8 A13_HX83112F_053[] = {
	0xBD,
	0x03,
};

static u8 A13_HX83112F_054[] = {
	0xE7,
	0x01, 0x01,
};

static u8 A13_HX83112F_055[] = {
	0xBD,
	0x00,
};

static u8 A13_HX83112F_056[] = {
	0xE1,
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x68, 0x04, 0x38,
	0x00, 0x08, 0x02, 0x1C, 0x02, 0x1C, 0x02, 0x00, 0x02, 0x0E,
	0x00, 0x20, 0x00, 0xBB, 0x00, 0x07, 0x00, 0x0C, 0x0D, 0xB7,
	0x0C, 0xB7, 0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54,
	0x62, 0x69, 0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01, 0x02,
	0x01, 0x00, 0x09,
};

static u8 A13_HX83112F_057[] = {
	0xBD,
	0x01,
};

static u8 A13_HX83112F_058[] = {
	0xE1,
	0x40, 0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8, 0x1A,
	0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6, 0x2B, 0x34, 0x2B,
	0x74, 0x3B, 0x74, 0x63, 0xF4,
};

static u8 A13_HX83112F_059[] = {
	0xBD,
	0x03,
};

static u8 A13_HX83112F_060[] = {
	0xE1,
	0x00,
};

static u8 A13_HX83112F_061[] = {
	0xBD,
	0x00,
};

static u8 A13_HX83112F_062[] = {
	0xC0,
	0x35, 0x35, 0x00, 0x00, 0x2D, 0x1A, 0x66, 0x81,
};

static u8 A13_HX83112F_063[] = {
	0xBD,
	0x01,
};

static u8 A13_HX83112F_064[] = {
	0xC0,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
};

static u8 A13_HX83112F_065[] = {
	0xC7,
	0x44,
};

static u8 A13_HX83112F_066[] = {
	0xBF,
	0x0B,
};

static u8 A13_HX83112F_067[] = {
	0xBD,
	0x00,
};

static u8 A13_HX83112F_068[] = {
	0xBA,
	0x73, 0x03, 0xA8, 0x9A,
};

static u8 A13_HX83112F_069[] = {
	0xC9,
	0x00, 0x1E, 0x18, 0x6A,
};

static u8 A13_HX83112F_070[] = {
	0xBD,
	0x01,
};

static u8 A13_HX83112F_071[] = {
	0xE9,
	0xDD,
};

static u8 A13_HX83112F_072[] = {
	0xE4,
	0x0C, 0x08, 0x04, 0x54, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
	0x55, 0x00, 0x77,
};

static u8 A13_HX83112F_073[] = {
	0xE9,
	0x3F,
};

static u8 A13_HX83112F_074[] = {
	0xBD,
	0x03,
};

static u8 A13_HX83112F_075[] = {
	0xE4,
	0x98, 0xAD, 0xC1, 0xCB, 0xD6, 0xE1, 0xEA, 0xFA, 0xFF, 0xFF,
	0xFF, 0x03,
};

static u8 A13_HX83112F_076[] = {
	0xBD,
	0x00,
};

static u8 A13_HX83112F_077[] = {
	0xCE,
	0x00, 0x50, 0x20,
};

static u8 A13_HX83112F_078[] = {
	0x51,
	0x0F, 0xFF,
};

static u8 A13_HX83112F_079[] = {
	0x53,
	0x24,
};

static u8 A13_HX83112F_080[] = {
	0x55,
	0x01,
};

static u8 A13_HX83112F_081[] = {
	0xB9,
	0x00, 0x00, 0x00,
};

static DEFINE_STATIC_PACKET(a13_sleep_out, DSI_PKT_TYPE_WR, A13_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a13_sleep_in, DSI_PKT_TYPE_WR, A13_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a13_display_on, DSI_PKT_TYPE_WR, A13_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a13_display_off, DSI_PKT_TYPE_WR, A13_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(a13_brightness_mode, DSI_PKT_TYPE_WR, A13_BRIGHTNESS_MODE, 0);

static DEFINE_PKTUI(a13_brightness, &a13_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a13_brightness, DSI_PKT_TYPE_WR, A13_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(a13_hx83112f_boe_001, DSI_PKT_TYPE_WR, A13_HX83112F_001, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_002, DSI_PKT_TYPE_WR, A13_HX83112F_002, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_003, DSI_PKT_TYPE_WR, A13_HX83112F_003, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_004, DSI_PKT_TYPE_WR, A13_HX83112F_004, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_005, DSI_PKT_TYPE_WR, A13_HX83112F_005, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_006, DSI_PKT_TYPE_WR, A13_HX83112F_006, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_007, DSI_PKT_TYPE_WR, A13_HX83112F_007, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_008, DSI_PKT_TYPE_WR, A13_HX83112F_008, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_009, DSI_PKT_TYPE_WR, A13_HX83112F_009, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_010, DSI_PKT_TYPE_WR, A13_HX83112F_010, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_011, DSI_PKT_TYPE_WR, A13_HX83112F_011, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_012, DSI_PKT_TYPE_WR, A13_HX83112F_012, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_013, DSI_PKT_TYPE_WR, A13_HX83112F_013, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_014, DSI_PKT_TYPE_WR, A13_HX83112F_014, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_015, DSI_PKT_TYPE_WR, A13_HX83112F_015, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_016, DSI_PKT_TYPE_WR, A13_HX83112F_016, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_017, DSI_PKT_TYPE_WR, A13_HX83112F_017, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_018, DSI_PKT_TYPE_WR, A13_HX83112F_018, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_019, DSI_PKT_TYPE_WR, A13_HX83112F_019, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_020, DSI_PKT_TYPE_WR, A13_HX83112F_020, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_021, DSI_PKT_TYPE_WR, A13_HX83112F_021, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_022, DSI_PKT_TYPE_WR, A13_HX83112F_022, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_023, DSI_PKT_TYPE_WR, A13_HX83112F_023, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_024, DSI_PKT_TYPE_WR, A13_HX83112F_024, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_025, DSI_PKT_TYPE_WR, A13_HX83112F_025, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_026, DSI_PKT_TYPE_WR, A13_HX83112F_026, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_027, DSI_PKT_TYPE_WR, A13_HX83112F_027, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_028, DSI_PKT_TYPE_WR, A13_HX83112F_028, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_029, DSI_PKT_TYPE_WR, A13_HX83112F_029, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_030, DSI_PKT_TYPE_WR, A13_HX83112F_030, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_031, DSI_PKT_TYPE_WR, A13_HX83112F_031, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_032, DSI_PKT_TYPE_WR, A13_HX83112F_032, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_033, DSI_PKT_TYPE_WR, A13_HX83112F_033, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_034, DSI_PKT_TYPE_WR, A13_HX83112F_034, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_035, DSI_PKT_TYPE_WR, A13_HX83112F_035, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_036, DSI_PKT_TYPE_WR, A13_HX83112F_036, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_037, DSI_PKT_TYPE_WR, A13_HX83112F_037, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_038, DSI_PKT_TYPE_WR, A13_HX83112F_038, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_039, DSI_PKT_TYPE_WR, A13_HX83112F_039, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_040, DSI_PKT_TYPE_WR, A13_HX83112F_040, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_041, DSI_PKT_TYPE_WR, A13_HX83112F_041, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_042, DSI_PKT_TYPE_WR, A13_HX83112F_042, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_043, DSI_PKT_TYPE_WR, A13_HX83112F_043, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_044, DSI_PKT_TYPE_WR, A13_HX83112F_044, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_045, DSI_PKT_TYPE_WR, A13_HX83112F_045, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_046, DSI_PKT_TYPE_WR, A13_HX83112F_046, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_047, DSI_PKT_TYPE_WR, A13_HX83112F_047, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_048, DSI_PKT_TYPE_WR, A13_HX83112F_048, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_049, DSI_PKT_TYPE_WR, A13_HX83112F_049, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_050, DSI_PKT_TYPE_WR, A13_HX83112F_050, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_051, DSI_PKT_TYPE_WR, A13_HX83112F_051, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_052, DSI_PKT_TYPE_WR, A13_HX83112F_052, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_053, DSI_PKT_TYPE_WR, A13_HX83112F_053, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_054, DSI_PKT_TYPE_WR, A13_HX83112F_054, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_055, DSI_PKT_TYPE_WR, A13_HX83112F_055, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_056, DSI_PKT_TYPE_WR, A13_HX83112F_056, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_057, DSI_PKT_TYPE_WR, A13_HX83112F_057, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_058, DSI_PKT_TYPE_WR, A13_HX83112F_058, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_059, DSI_PKT_TYPE_WR, A13_HX83112F_059, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_060, DSI_PKT_TYPE_WR, A13_HX83112F_060, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_061, DSI_PKT_TYPE_WR, A13_HX83112F_061, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_062, DSI_PKT_TYPE_WR, A13_HX83112F_062, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_063, DSI_PKT_TYPE_WR, A13_HX83112F_063, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_064, DSI_PKT_TYPE_WR, A13_HX83112F_064, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_065, DSI_PKT_TYPE_WR, A13_HX83112F_065, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_066, DSI_PKT_TYPE_WR, A13_HX83112F_066, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_067, DSI_PKT_TYPE_WR, A13_HX83112F_067, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_068, DSI_PKT_TYPE_WR, A13_HX83112F_068, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_069, DSI_PKT_TYPE_WR, A13_HX83112F_069, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_070, DSI_PKT_TYPE_WR, A13_HX83112F_070, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_071, DSI_PKT_TYPE_WR, A13_HX83112F_071, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_072, DSI_PKT_TYPE_WR, A13_HX83112F_072, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_073, DSI_PKT_TYPE_WR, A13_HX83112F_073, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_074, DSI_PKT_TYPE_WR, A13_HX83112F_074, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_075, DSI_PKT_TYPE_WR, A13_HX83112F_075, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_076, DSI_PKT_TYPE_WR, A13_HX83112F_076, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_077, DSI_PKT_TYPE_WR, A13_HX83112F_077, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_078, DSI_PKT_TYPE_WR, A13_HX83112F_078, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_079, DSI_PKT_TYPE_WR, A13_HX83112F_079, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_080, DSI_PKT_TYPE_WR, A13_HX83112F_080, 0);
static DEFINE_STATIC_PACKET(a13_hx83112f_boe_081, DSI_PKT_TYPE_WR, A13_HX83112F_081, 0);

static DEFINE_PANEL_MDELAY(a13_wait_display_off, 1 * 17); /* 1 frame */
static DEFINE_PANEL_MDELAY(a13_wait_sleep_in, 4 * 17); /* 4 frame */
static DEFINE_PANEL_MDELAY(a13_wait_2msec, 2);
static DEFINE_PANEL_MDELAY(a13_wait_20msec, 20);
static DEFINE_PANEL_MDELAY(a13_wait_120msec, 120);
static DEFINE_PANEL_MDELAY(a13_wait_blic_off, 1);


static void *a13_init_cmdtbl[] = {
	&hx83112f_boe_restbl[RES_ID],
	&PKTINFO(a13_hx83112f_boe_001),
	&PKTINFO(a13_hx83112f_boe_002),
	&PKTINFO(a13_hx83112f_boe_003),
	&PKTINFO(a13_hx83112f_boe_004),
	&PKTINFO(a13_hx83112f_boe_005),
	&PKTINFO(a13_hx83112f_boe_006),
	&PKTINFO(a13_hx83112f_boe_007),
	&PKTINFO(a13_hx83112f_boe_008),
	&PKTINFO(a13_hx83112f_boe_009),
	&PKTINFO(a13_hx83112f_boe_010),
	&PKTINFO(a13_hx83112f_boe_011),
	&PKTINFO(a13_hx83112f_boe_012),
	&PKTINFO(a13_hx83112f_boe_013),
	&PKTINFO(a13_hx83112f_boe_014),
	&PKTINFO(a13_hx83112f_boe_015),
	&PKTINFO(a13_hx83112f_boe_016),
	&PKTINFO(a13_hx83112f_boe_017),
	&PKTINFO(a13_hx83112f_boe_018),
	&PKTINFO(a13_hx83112f_boe_019),
	&PKTINFO(a13_hx83112f_boe_020),
	&PKTINFO(a13_hx83112f_boe_021),
	&PKTINFO(a13_hx83112f_boe_022),
	&PKTINFO(a13_hx83112f_boe_023),
	&PKTINFO(a13_hx83112f_boe_024),
	&PKTINFO(a13_hx83112f_boe_025),
	&PKTINFO(a13_hx83112f_boe_026),
	&PKTINFO(a13_hx83112f_boe_027),
	&PKTINFO(a13_hx83112f_boe_028),
	&PKTINFO(a13_hx83112f_boe_029),
	&PKTINFO(a13_hx83112f_boe_030),
	&PKTINFO(a13_hx83112f_boe_031),
	&PKTINFO(a13_hx83112f_boe_032),
	&PKTINFO(a13_hx83112f_boe_033),
	&PKTINFO(a13_hx83112f_boe_034),
	&PKTINFO(a13_hx83112f_boe_035),
	&PKTINFO(a13_hx83112f_boe_036),
	&PKTINFO(a13_hx83112f_boe_037),
	&PKTINFO(a13_hx83112f_boe_038),
	&PKTINFO(a13_hx83112f_boe_039),
	&PKTINFO(a13_hx83112f_boe_040),
	&PKTINFO(a13_hx83112f_boe_041),
	&PKTINFO(a13_hx83112f_boe_042),
	&PKTINFO(a13_hx83112f_boe_043),
	&PKTINFO(a13_hx83112f_boe_044),
	&PKTINFO(a13_hx83112f_boe_045),
	&PKTINFO(a13_hx83112f_boe_046),
	&PKTINFO(a13_hx83112f_boe_047),
	&PKTINFO(a13_hx83112f_boe_048),
	&PKTINFO(a13_hx83112f_boe_049),
	&PKTINFO(a13_hx83112f_boe_050),
	&PKTINFO(a13_hx83112f_boe_051),
	&PKTINFO(a13_hx83112f_boe_052),
	&PKTINFO(a13_hx83112f_boe_053),
	&PKTINFO(a13_hx83112f_boe_054),
	&PKTINFO(a13_hx83112f_boe_055),
	&PKTINFO(a13_hx83112f_boe_056),
	&PKTINFO(a13_hx83112f_boe_057),
	&PKTINFO(a13_hx83112f_boe_058),
	&PKTINFO(a13_hx83112f_boe_059),
	&PKTINFO(a13_hx83112f_boe_060),
	&PKTINFO(a13_hx83112f_boe_061),
	&PKTINFO(a13_hx83112f_boe_062),
	&PKTINFO(a13_hx83112f_boe_063),
	&PKTINFO(a13_hx83112f_boe_064),
	&PKTINFO(a13_hx83112f_boe_065),
	&PKTINFO(a13_hx83112f_boe_066),
	&PKTINFO(a13_hx83112f_boe_067),
	&PKTINFO(a13_hx83112f_boe_068),
	&PKTINFO(a13_hx83112f_boe_069),
	&PKTINFO(a13_hx83112f_boe_070),
	&PKTINFO(a13_hx83112f_boe_071),
	&PKTINFO(a13_hx83112f_boe_072),
	&PKTINFO(a13_hx83112f_boe_073),
	&PKTINFO(a13_hx83112f_boe_074),
	&PKTINFO(a13_hx83112f_boe_075),
	&PKTINFO(a13_hx83112f_boe_076),
	&PKTINFO(a13_hx83112f_boe_077),
	&PKTINFO(a13_hx83112f_boe_078),
	&PKTINFO(a13_hx83112f_boe_079),
	&PKTINFO(a13_hx83112f_boe_080),
	&PKTINFO(a13_hx83112f_boe_081),
	&PKTINFO(a13_sleep_out),
	&DLYINFO(a13_wait_120msec),
};

static void *a13_res_init_cmdtbl[] = {
	&hx83112f_boe_restbl[RES_ID],
};

static void *a13_set_bl_cmdtbl[] = {
	&PKTINFO(a13_brightness), //51h
};

static void *a13_display_on_cmdtbl[] = {
	&PKTINFO(a13_display_on),
	&DLYINFO(a13_wait_20msec),
	&PKTINFO(a13_brightness_mode),
};

static void *a13_display_off_cmdtbl[] = {
	&PKTINFO(a13_display_off),
	&DLYINFO(a13_wait_display_off),
};

static void *a13_exit_cmdtbl[] = {
	&PKTINFO(a13_sleep_in),
};

/* ===================================================================================== */
/* ================================= [EA8076 I2C TABLE] ===========+++================= */
/* ===================================================================================== */
static u8 HX83112F_A13_I2C_INIT[] = {
	0x0C, 0x24,
	0x0D, 0x1E,
	0x0E, 0x1E,
	0x09, 0x99,
	0x02, 0x6B,
	0x03, 0x0D,
	0x11, 0x74,
	0x04, 0x03,
	0x05, 0xC2,
	0x10, 0x66,
	0x08, 0x13,
};

static u8 HX83112F_A13_I2C_EXIT_VSP[] = {
	0x09, 0x18,
};

static u8 HX83112F_A13_I2C_EXIT_VSN[] = {
	0x09, 0x9C,
};

static u8 HX83112F_A13_I2C_EXIT_BLEN[] = {
	0x08, 0x00,
};

static u8 HX83112F_A13_I2C_DUMP[] = {
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
};

static DEFINE_STATIC_PACKET(hx83112f_boe_a13_i2c_init, I2C_PKT_TYPE_WR, HX83112F_A13_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(hx83112f_boe_a13_i2c_exit_vsn, I2C_PKT_TYPE_WR, HX83112F_A13_I2C_EXIT_VSN, 0);
static DEFINE_STATIC_PACKET(hx83112f_boe_a13_i2c_exit_vsp, I2C_PKT_TYPE_WR, HX83112F_A13_I2C_EXIT_VSP, 0);
static DEFINE_STATIC_PACKET(hx83112f_boe_a13_i2c_exit_blen, I2C_PKT_TYPE_WR, HX83112F_A13_I2C_EXIT_BLEN, 0);
static DEFINE_STATIC_PACKET(hx83112f_boe_a13_i2c_dump, I2C_PKT_TYPE_RD, HX83112F_A13_I2C_DUMP, 0);

static void *hx83112f_boe_a13_init_cmdtbl[] = {
	&PKTINFO(hx83112f_boe_a13_i2c_init),
};

static void *hx83112f_boe_a13_exit_cmdtbl[] = {
	&PKTINFO(hx83112f_boe_a13_i2c_exit_vsn),
	&DLYINFO(a13_wait_blic_off),
	&PKTINFO(hx83112f_boe_a13_i2c_exit_vsp),
	&DLYINFO(a13_wait_blic_off),	
	&PKTINFO(hx83112f_boe_a13_i2c_exit_blen),
};

static void *hx83112f_boe_a13_dump_cmdtbl[] = {
	&PKTINFO(hx83112f_boe_a13_i2c_dump),
};


static struct seqinfo a13_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a13_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a13_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a13_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a13_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a13_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a13_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", hx83112f_boe_a13_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", hx83112f_boe_a13_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", hx83112f_boe_a13_dump_cmdtbl),
#endif
};

struct common_panel_info hx83112f_boe_a13_default_panel_info = {
	.ldi_name = "hx83112f_boe",
	.name = "hx83112f_boe_a13_default",
	.model = "BOE_6_517_inch",
	.vendor = "BOE",
	.id = 0x4B6230,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(hx83112f_boe_a13_resol),
		.resol = hx83112f_boe_a13_resol,
	},
	.maptbl = a13_maptbl,
	.nr_maptbl = ARRAY_SIZE(a13_maptbl),
	.seqtbl = a13_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a13_seqtbl),
	.rditbl = hx83112f_boe_rditbl,
	.nr_rditbl = ARRAY_SIZE(hx83112f_boe_rditbl),
	.restbl = hx83112f_boe_restbl,
	.nr_restbl = ARRAY_SIZE(hx83112f_boe_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&hx83112f_boe_a13_panel_dimming_info,
	},
	.i2c_data = &hx83112f_boe_a13_i2c_data,
};

static int __init hx83112f_boe_a13_panel_init(void)
{
	register_common_panel(&hx83112f_boe_a13_default_panel_info);

	return 0;
}
arch_initcall(hx83112f_boe_a13_panel_init)
#endif /* __HX83112F_A13_PANEL_H__ */
