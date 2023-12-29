/*
 * linux/drivers/video/fbdev/exynos/panel/ft8720_skyworth/ft8720_skyworth_a14_panel.h
 *
 * Header file for FT8720 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __FT8720_A14_PANEL_H__
#define __FT8720_A14_PANEL_H__
#include "../panel_drv.h"
#include "ft8720_skyworth.h"

#include "ft8720_skyworth_a14_panel_dimming.h"
#include "ft8720_skyworth_a14_panel_i2c.h"

#include "ft8720_skyworth_a14_resol.h"

#undef __pn_name__
#define __pn_name__	a14

#undef __PN_NAME__
#define __PN_NAME__	A14

static struct seqinfo a14_seqtbl[MAX_PANEL_SEQ];


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

static u8 a14_brt_table[FT8720_TOTAL_NR_LUMINANCE][1] = {
	{0},
	{1}, {1}, {2}, {2}, {3}, {3}, {4}, {4}, {5}, {5},  /* 1: 1 */
	{6}, {6}, {7}, {7}, {8}, {8}, {9}, {9}, {10}, {10},
	{11}, {11}, {12}, {12}, {13}, {13}, {14}, {14}, {15}, {15},
	{16}, {16}, {17}, {17}, {18}, {19}, {19}, {20}, {20}, {21},
	{22}, {22}, {23}, {23}, {24}, {25}, {25}, {26}, {26}, {27},
	{28}, {28}, {29}, {29}, {30}, {31}, {31}, {32}, {32}, {33},
	{34}, {34}, {35}, {35}, {36}, {37}, {37}, {38}, {38}, {39},
	{40}, {40}, {41}, {41}, {42}, {43}, {43}, {44}, {44}, {45},
	{46}, {47}, {47}, {48}, {49}, {50}, {51}, {51}, {52}, {53},
	{54}, {55}, {55}, {56}, {57}, {58}, {58}, {59}, {60}, {61},
	{62}, {62}, {63}, {64}, {65}, {66}, {66}, {67}, {68}, {69},
	{70}, {70}, {71}, {72}, {73}, {74}, {74}, {75}, {76}, {77},
	{77}, {78}, {79}, {80}, {81}, {81}, {82}, {83}, {84}, {85},  /* 128: 83 */
	{86}, {87}, {88}, {89}, {90}, {91}, {92}, {93}, {94}, {95},
	{96}, {97}, {98}, {99}, {100}, {101}, {102}, {103}, {104}, {105},
	{106}, {107}, {108}, {109}, {110}, {111}, {112}, {113}, {114}, {116},
	{117}, {118}, {119}, {120}, {121}, {122}, {123}, {124}, {125}, {126},
	{127}, {128}, {129}, {130}, {131}, {132}, {133}, {134}, {135}, {136},
	{137}, {138}, {139}, {140}, {141}, {142}, {143}, {144}, {145}, {146},
	{147}, {148}, {149}, {150}, {151}, {152}, {153}, {154}, {155}, {156},
	{157}, {158}, {159}, {160}, {161}, {162}, {163}, {164}, {165}, {166},
	{167}, {168}, {169}, {170}, {171}, {172}, {173}, {174}, {175}, {176},
	{177}, {178}, {179}, {181}, {182}, {183}, {184}, {185}, {186}, {187},
	{188}, {189}, {190}, {191}, {192}, {193}, {194}, {195}, {196}, {197},
	{198}, {199}, {200}, {201}, {202}, {203}, {204}, {205}, {206}, {207},
	{208}, {209}, {210}, {211}, {212}, {213}, {214}, {215}, {215}, {216}, 	/* 255 : 212 */
	{217}, {218}, {219}, {220}, {220}, {221}, {222}, {223}, {224}, {225},
	{225}, {226}, {227}, {228}, {229}, {230}, {231}, {231}, {232}, {233},
	{234}, {235}, {236}, {236}, {237}, {238}, {239}, {240}, {241}, {242},
	{242}, {243}, {244}, {245}, {246}, {247}, {247}, {248}, {249}, {250},
	{251}, {252}, {252}, {253}, {254}, {255}, 				/* 306 : 255 */
};


static struct maptbl a14_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(a14_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [FT8720 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A14_SLEEP_OUT[] = { 0x11 };
static u8 A14_SLEEP_IN[] = { 0x10 };
static u8 A14_DISPLAY_OFF[] = { 0x28 };
static u8 A14_DISPLAY_ON[] = { 0x29 };

static u8 A14_BRIGHTNESS[] = {
	0x51,
	0xFF,
};

static u8 A14_BRIGHTNESS_0[] = {
	0x51,
	0x00,
};

static u8 A14_BRIGHTNESS_MODE[] = {
	0x53,
	0x24,
};

static u8 A14_CABC_MODE[] = {
	0x55,
	0x01,
};

static u8 A14_TE_ON[] = {
	0x35,
	0x00,
};

static u8 A14_FT8720_001[] = {
	0x00,
	0x00,
};

static u8 A14_FT8720_002[] = {
	0xFF,
	0x87, 0x20, 0x01,
};

static u8 A14_FT8720_003[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_004[] = {
	0xFF,
	0x87, 0x20,
};

static u8 A14_FT8720_005[] = {
	0x00,
	0xA3,
};

static u8 A14_FT8720_006[] = {
	0xB3,
	0x09, 0x68,
};

static u8 A14_FT8720_007[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_008[] = {
	0xC0,
	0x00, 0x61, 0x00, 0x21, 0x00, 0x0F,
};

static u8 A14_FT8720_009[] = {
	0x00,
	0x90,
};

static u8 A14_FT8720_010[] = {
	0xC0,
	0x00, 0x61, 0x00, 0x21, 0x00, 0x0F,
};

static u8 A14_FT8720_011[] = {
	0x00,
	0xA0,
};

static u8 A14_FT8720_012[] = {
	0xC0,
	0x00, 0x9E, 0x00, 0x21, 0x00, 0x0F,
};

static u8 A14_FT8720_013[] = {
	0x00,
	0xB0,
};

static u8 A14_FT8720_014[] = {
	0xC0,
	0x00, 0xBC, 0x00, 0x21, 0x0F,
};

static u8 A14_FT8720_015[] = {
	0x00,
	0x60,
};

static u8 A14_FT8720_016[] = {
	0xC0,
	0x00, 0xA0, 0x00, 0x21, 0x00, 0x0F,
};

static u8 A14_FT8720_017[] = {
	0x00,
	0x70,
};

static u8 A14_FT8720_018[] = {
	0xC0,
	0x00, 0xC8, 0x00, 0xC8, 0x0B, 0x02, 0xB6, 0x00, 0x00, 0x15,
	0x00, 0xD1,
};

static u8 A14_FT8720_019[] = {
	0x00,
	0xA3,
};

static u8 A14_FT8720_020[] = {
	0xC1,
	0x00, 0x52, 0x00, 0x2B, 0x00, 0x02,
};

static u8 A14_FT8720_021[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_022[] = {
	0xCE,
	0x01, 0x81, 0xFF, 0xFF, 0x00, 0xB4, 0x00, 0xC8, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xB8, 0x00, 0xC8,
};

static u8 A14_FT8720_023[] = {
	0x00,
	0x90,
};

static u8 A14_FT8720_024[] = {
	0xCE,
	0x00, 0xA7, 0x10, 0x43, 0x00, 0xA7, 0x80, 0xFF, 0xFF, 0x00,
	0x07, 0x08, 0x0F, 0x0E, 0x00,
};

static u8 A14_FT8720_025[] = {
	0x00,
	0xA0,
};

static u8 A14_FT8720_026[] = {
	0xCE,
	0x00, 0x00, 0x00,
};

static u8 A14_FT8720_027[] = {
	0x00,
	0xB0,
};

static u8 A14_FT8720_028[] = {
	0xCE,
	0x22, 0x00, 0x00,
};

static u8 A14_FT8720_029[] = {
	0x00,
	0xD1,
};

static u8 A14_FT8720_030[] = {
	0xCE,
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
};

static u8 A14_FT8720_031[] = {
	0x00,
	0xE1,
};

static u8 A14_FT8720_032[] = {
	0xCE,
	0x04, 0x02, 0xB6, 0x02, 0xB6, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,
};

static u8 A14_FT8720_033[] = {
	0x00,
	0xF1,
};

static u8 A14_FT8720_034[] = {
	0xCE,
	0x1F, 0x15, 0x00, 0x01, 0x28, 0x00, 0xEB, 0x00, 0x00,
};

static u8 A14_FT8720_035[] = {
	0x00,
	0xB0,
};

static u8 A14_FT8720_036[] = {
	0xCF,
	0x00, 0x00, 0x97, 0x9B,
};

static u8 A14_FT8720_037[] = {
	0x00,
	0xB5,
};

static u8 A14_FT8720_038[] = {
	0xCF,
	0x04, 0x05, 0xFD, 0x01,
};

static u8 A14_FT8720_039[] = {
	0x00,
	0xC0,
};

static u8 A14_FT8720_040[] = {
	0xCF,
	0x09, 0x09, 0x63, 0x67,
};

static u8 A14_FT8720_041[] = {
	0x00,
	0xC5,
};

static u8 A14_FT8720_042[] = {
	0xCF,
	0x09, 0x09, 0x69, 0x6D,
};

static u8 A14_FT8720_043[] = {
	0x00,
	0xD1,
};

static u8 A14_FT8720_044[] = {
	0xC1,
	0x0A, 0xA6, 0x0E, 0xC8, 0x19, 0x67, 0x07, 0xE3, 0x0B, 0x11,
	0x13, 0x04,
};

static u8 A14_FT8720_045[] = {
	0x00,
	0xE1,
};

static u8 A14_FT8720_046[] = {
	0xC1,
	0x0E, 0xC8,
};

static u8 A14_FT8720_047[] = {
	0x00,
	0xE4,
};

static u8 A14_FT8720_048[] = {
	0xCF,
	0x09, 0xED, 0x09, 0xEC, 0x09, 0xEC, 0x09, 0xEC, 0x09, 0xEC,
	0x09, 0xEC,
};

static u8 A14_FT8720_049[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_050[] = {
	0xC1,
	0x00, 0x00,
};

static u8 A14_FT8720_051[] = {
	0x00,
	0x90,
};

static u8 A14_FT8720_052[] = {
	0xC1,
	0x03,
};

static u8 A14_FT8720_053[] = {
	0x00,
	0xF5,
};

static u8 A14_FT8720_054[] = {
	0xCF,
	0x02,
};

static u8 A14_FT8720_055[] = {
	0x00,
	0xF6,
};

static u8 A14_FT8720_056[] = {
	0xCF,
	0x3C,
};

static u8 A14_FT8720_057[] = {
	0x00,
	0xF1,
};

static u8 A14_FT8720_058[] = {
	0xCF,
	0x3C,
};

static u8 A14_FT8720_059[] = {
	0x00,
	0xF0,
};

static u8 A14_FT8720_060[] = {
	0xC1,
	0x00,
};

static u8 A14_FT8720_061[] = {
	0x00,
	0xCC,
};

static u8 A14_FT8720_062[] = {
	0xC1,
	0x18,
};

static u8 A14_FT8720_063[] = {
	0x00,
	0x91,
};

static u8 A14_FT8720_064[] = {
	0xC4,
	0x08,
};

static u8 A14_FT8720_065[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_066[] = {
	0xC5,
	0x88,
};

static u8 A14_FT8720_067[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_068[] = {
	0xC2,
	0x81, 0x00, 0x02, 0x90, 0x00, 0x00, 0x00, 0x00,
};

static u8 A14_FT8720_069[] = {
	0x00,
	0x90,
};

static u8 A14_FT8720_070[] = {
	0xC2,
	0x80, 0x00, 0x04, 0x90,
};

static u8 A14_FT8720_071[] = {
	0x00,
	0xA0,
};

static u8 A14_FT8720_072[] = {
	0xC2,
	0x00, 0x04, 0x00, 0x04, 0x90, 0x01, 0x05, 0x00, 0x04, 0x90,
	0x02, 0x06, 0x00, 0x04, 0x90,
};

static u8 A14_FT8720_073[] = {
	0x00,
	0xB0,
};

static u8 A14_FT8720_074[] = {
	0xC2,
	0x03, 0x04, 0x00, 0x04, 0x90, 0x04, 0x05, 0x00, 0x04, 0x90,
};

static u8 A14_FT8720_075[] = {
	0x00,
	0xE0,
};

static u8 A14_FT8720_076[] = {
	0xC2,
	0x44, 0x44, 0x40, 0x00,
};

static u8 A14_FT8720_077[] = {
	0x00,
	0xE8,
};

static u8 A14_FT8720_078[] = {
	0xC2,
	0x0B, 0x00, 0x05, 0x03, 0x02, 0x8A, 0x00, 0x00,
};

static u8 A14_FT8720_079[] = {
	0x00,
	0xD0,
};

static u8 A14_FT8720_080[] = {
	0xC3,
	0x35, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00, 0x35, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 A14_FT8720_081[] = {
	0x00,
	0xE0,
};

static u8 A14_FT8720_082[] = {
	0xC3,
	0x35, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00, 0x00, 0x35, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 A14_FT8720_083[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_084[] = {
	0xCB,
	0x00, 0x01, 0x01, 0x01, 0xFD, 0xFD, 0x00, 0xFD, 0x03, 0xFE,
	0xFD, 0x01, 0x00, 0x00, 0x00, 0x03,
};

static u8 A14_FT8720_085[] = {
	0x00,
	0x90,
};

static u8 A14_FT8720_086[] = {
	0xCB,
	0x00, 0x00, 0x0F, 0x00, 0x0C, 0x0C, 0x00, 0x00, 0xFF, 0xFF,
	0x00, 0x7F, 0x00, 0x00, 0x00, 0xFF,
};

static u8 A14_FT8720_087[] = {
	0x00,
	0xA0,
};

static u8 A14_FT8720_088[] = {
	0xCB,
	0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static u8 A14_FT8720_089[] = {
	0x00,
	0xB0,
};

static u8 A14_FT8720_090[] = {
	0xCB,
	0x40, 0xD3, 0x3D, 0x00,
};

static u8 A14_FT8720_091[] = {
	0x00,
	0xC0,
};

static u8 A14_FT8720_092[] = {
	0xCB,
	0x40, 0x00, 0xF1, 0x00,
};

static u8 A14_FT8720_093[] = {
	0x00,
	0xD5,
};

static u8 A14_FT8720_094[] = {
	0xCB,
	0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83,
	0x00,
};

static u8 A14_FT8720_095[] = {
	0x00,
	0xE0,
};

static u8 A14_FT8720_096[] = {
	0xCB,
	0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83, 0x83, 0x00, 0x83,
	0x83, 0x00, 0x83,
};

static u8 A14_FT8720_097[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_098[] = {
	0xCC,
	0x20, 0x1C, 0x1E, 0x1F, 0x1C, 0x12, 0x02, 0x0A, 0x09, 0x08,
	0x07, 0x06, 0x18, 0x18, 0x17, 0x17,
};

static u8 A14_FT8720_099[] = {
	0x00,
	0x90,
};

static u8 A14_FT8720_100[] = {
	0xCC,
	0x16, 0x16, 0x18, 0x18, 0x17, 0x17, 0x16, 0x16,
};

static u8 A14_FT8720_101[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_102[] = {
	0xCD,
	0x1C, 0x1C, 0x1E, 0x1F, 0x1C, 0x12, 0x02, 0x0A, 0x09, 0x08,
	0x07, 0x06, 0x18, 0x18, 0x17, 0x17,
};

static u8 A14_FT8720_103[] = {
	0x00,
	0x90,
};

static u8 A14_FT8720_104[] = {
	0xCD,
	0x16, 0x16, 0x18, 0x18, 0x17, 0x17, 0x16, 0x16,
};

static u8 A14_FT8720_105[] = {
	0x00,
	0xA0,
};

static u8 A14_FT8720_106[] = {
	0xCC,
	0x20, 0x1C, 0x1E, 0x1F, 0x1C, 0x02, 0x12, 0x09, 0x0A, 0x06,
	0x07, 0x08, 0x18, 0x18, 0x17, 0x17,
};

static u8 A14_FT8720_107[] = {
	0x00,
	0xB0,
};

static u8 A14_FT8720_108[] = {
	0xCC,
	0x16, 0x16, 0x18, 0x18, 0x17, 0x17, 0x16, 0x16,
};

static u8 A14_FT8720_109[] = {
	0x00,
	0xA0,
};

static u8 A14_FT8720_110[] = {
	0xCD,
	0x1C, 0x1C, 0x1E, 0x1F, 0x22, 0x02, 0x12, 0x09, 0x0A, 0x06,
	0x07, 0x08, 0x18, 0x18, 0x17, 0x17,
};

static u8 A14_FT8720_111[] = {
	0x00,
	0xB0,
};

static u8 A14_FT8720_112[] = {
	0xCD,
	0x16, 0x16, 0x18, 0x18, 0x17, 0x17, 0x16, 0x16,
};

static u8 A14_FT8720_113[] = {
	0x00,
	0x86,
};

static u8 A14_FT8720_114[] = {
	0xC0,
	0x00, 0x00, 0x00, 0x01, 0x19, 0x05,
};

static u8 A14_FT8720_115[] = {
	0x00,
	0x96,
};

static u8 A14_FT8720_116[] = {
	0xC0,
	0x00, 0x00, 0x00, 0x01, 0x19, 0x05,
};

static u8 A14_FT8720_117[] = {
	0x00,
	0xA6,
};

static u8 A14_FT8720_118[] = {
	0xC0,
	0x00, 0x00, 0x00, 0x01, 0x19, 0x05,
};

static u8 A14_FT8720_119[] = {
	0x00,
	0xA3,
};

static u8 A14_FT8720_120[] = {
	0xCE,
	0x00, 0x00, 0x00, 0x01, 0x29, 0x06,
};

static u8 A14_FT8720_121[] = {
	0x00,
	0xB3,
};

static u8 A14_FT8720_122[] = {
	0xCE,
	0x00, 0x00, 0x00, 0x01, 0x29, 0x06,
};

static u8 A14_FT8720_123[] = {
	0x00,
	0x69,
};

static u8 A14_FT8720_124[] = {
	0xC0,
	0x01, 0x29, 0x06,
};

static u8 A14_FT8720_125[] = {
	0x00,
	0x82,
};

static u8 A14_FT8720_126[] = {
	0xA7,
	0x10, 0x00,
};

static u8 A14_FT8720_127[] = {
	0x00,
	0x8D,
};

static u8 A14_FT8720_128[] = {
	0xA7,
	0x01,
};

static u8 A14_FT8720_129[] = {
	0x00,
	0x8F,
};

static u8 A14_FT8720_130[] = {
	0xA7,
	0x01,
};

static u8 A14_FT8720_131[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_132[] = {
	0xA7,
	0x03,
};

static u8 A14_FT8720_133[] = {
	0x00,
	0x93,
};

static u8 A14_FT8720_134[] = {
	0xC5,
	0x2D,
};

static u8 A14_FT8720_135[] = {
	0x00,
	0x97,
};

static u8 A14_FT8720_136[] = {
	0xC5,
	0x2D,
};

static u8 A14_FT8720_137[] = {
	0x00,
	0x9A,
};

static u8 A14_FT8720_138[] = {
	0xC5,
	0x19,
};

static u8 A14_FT8720_139[] = {
	0x00,
	0x9C,
};

static u8 A14_FT8720_140[] = {
	0xC5,
	0x19,
};

static u8 A14_FT8720_141[] = {
	0x00,
	0xB6,
};

static u8 A14_FT8720_142[] = {
	0xC5,
	0x14, 0x14, 0x0A, 0x0A, 0x14, 0x14, 0x0A, 0x0A,
};

static u8 A14_FT8720_143[] = {
	0x00,
	0x82,
};

static u8 A14_FT8720_144[] = {
	0xF5,
	0x01,
};

static u8 A14_FT8720_145[] = {
	0x00,
	0x93,
};

static u8 A14_FT8720_146[] = {
	0xF5,
	0x01,
};

static u8 A14_FT8720_147[] = {
	0x00,
	0x9B,
};

static u8 A14_FT8720_148[] = {
	0xF5,
	0x49,
};

static u8 A14_FT8720_149[] = {
	0x00,
	0x9D,
};

static u8 A14_FT8720_150[] = {
	0xF5,
	0x49,
};

static u8 A14_FT8720_151[] = {
	0x00,
	0xBE,
};

static u8 A14_FT8720_152[] = {
	0xC5,
	0xF0, 0xF0,
};

static u8 A14_FT8720_153[] = {
	0x00,
	0x85,
};

static u8 A14_FT8720_154[] = {
	0xA7,
	0x01,
};

static u8 A14_FT8720_155[] = {
	0x00,
	0xDC,
};

static u8 A14_FT8720_156[] = {
	0xC3,
	0x37,
};

static u8 A14_FT8720_157[] = {
	0x00,
	0x8A,
};

static u8 A14_FT8720_158[] = {
	0xF5,
	0xC7,
};

static u8 A14_FT8720_159[] = {
	0x00,
	0xB1,
};

static u8 A14_FT8720_160[] = {
	0xF5,
	0x1F,
};

static u8 A14_FT8720_161[] = {
	0x00,
	0xB7,
};

static u8 A14_FT8720_162[] = {
	0xF5,
	0x1F,
};

static u8 A14_FT8720_163[] = {
	0x00,
	0x99,
};

static u8 A14_FT8720_164[] = {
	0xCF,
	0x50,
};

static u8 A14_FT8720_165[] = {
	0x00,
	0x9C,
};

static u8 A14_FT8720_166[] = {
	0xF5,
	0x00,
};

static u8 A14_FT8720_167[] = {
	0x00,
	0x9E,
};

static u8 A14_FT8720_168[] = {
	0xF5,
	0x00,
};

static u8 A14_FT8720_169[] = {
	0x00,
	0xB0,
};

static u8 A14_FT8720_170[] = {
	0xC5,
	0xD0, 0x4A, 0x39, 0xD0, 0x4A, 0x0E,
};

static u8 A14_FT8720_171[] = {
	0x00,
	0xC2,
};

static u8 A14_FT8720_172[] = {
	0xF5,
	0x42,
};

static u8 A14_FT8720_173[] = {
	0x00,
	0xE8,
};

static u8 A14_FT8720_174[] = {
	0xC0,
	0x40,
};

static u8 A14_FT8720_175[] = {
	0x00,
	0x00,
};

static u8 A14_FT8720_176[] = {
	0xD8,
	0x2F, 0x2F,
};

static u8 A14_FT8720_177[] = {
	0x00,
};

static u8 A14_FT8720_178[] = {
	0xD9,
};

static u8 A14_FT8720_179[] = {
	0x00,
};

static u8 A14_FT8720_180[] = {
	0xD9,
};

static u8 A14_FT8720_181[] = {
	0x00,
	0x00,
};

static u8 A14_FT8720_182[] = {
	0xE1,
	0x00, 0x01, 0x05, 0x0D, 0x32, 0x1C, 0x23, 0x2A, 0x34, 0x30,
	0x3C, 0x42, 0x48, 0x4C, 0x23, 0x51, 0x58, 0x5F, 0x65, 0x21,
	0x6B, 0x72, 0x79, 0x80, 0x83, 0x8A, 0x8F, 0x96, 0x9D, 0x71,
	0xA5, 0xB0, 0xBF, 0xC8, 0xF2, 0xD4, 0xE6, 0xF5, 0xFE, 0xF3,
};

static u8 A14_FT8720_183[] = {
	0x00,
	0x30,
};

static u8 A14_FT8720_184[] = {
	0xE1,
	0x15, 0x15, 0x15, 0x17, 0x1E, 0x1C, 0x23, 0x2A, 0x34, 0x30,
	0x3C, 0x42, 0x48, 0x4C, 0x23, 0x51, 0x58, 0x5F, 0x65, 0x21,
	0x6B, 0x72, 0x79, 0x80, 0x83, 0x8A, 0x8F, 0x96, 0x9D, 0x71,
	0xA5, 0xB0, 0xBF, 0xC8, 0xF2, 0xD4, 0xE6, 0xF5, 0xFE, 0xF3,
};

static u8 A14_FT8720_185[] = {
	0x00,
	0x60,
};

static u8 A14_FT8720_186[] = {
	0xE1,
	0x00, 0x01, 0x05, 0x0D, 0x32, 0x1C, 0x23, 0x2A, 0x34, 0x30,
	0x3C, 0x42, 0x48, 0x4C, 0x23, 0x51, 0x58, 0x5F, 0x65, 0x21,
	0x6B, 0x72, 0x79, 0x80, 0x83, 0x8A, 0x8F, 0x96, 0x9D, 0x71,
	0xA5, 0xB0, 0xBF, 0xC8, 0xF2, 0xD4, 0xE6, 0xF5, 0xFE, 0xF3,
};

static u8 A14_FT8720_187[] = {
	0x00,
	0x90,
};

static u8 A14_FT8720_188[] = {
	0xE1,
	0x15, 0x15, 0x15, 0x17, 0x1E, 0x1C, 0x23, 0x2A, 0x34, 0x30,
	0x3C, 0x42, 0x48, 0x4C, 0x23, 0x51, 0x58, 0x5F, 0x65, 0x21,
	0x6B, 0x72, 0x79, 0x80, 0x83, 0x8A, 0x8F, 0x96, 0x9D, 0x71,
	0xA5, 0xB0, 0xBF, 0xC8, 0xF2, 0xD4, 0xE6, 0xF5, 0xFE, 0xF3,
};

static u8 A14_FT8720_189[] = {
	0x00,
	0xC0,
};

static u8 A14_FT8720_190[] = {
	0xE1,
	0x00, 0x01, 0x05, 0x0D, 0x32, 0x1C, 0x23, 0x2A, 0x34, 0x30,
	0x3C, 0x42, 0x48, 0x4C, 0x23, 0x51, 0x58, 0x5F, 0x65, 0x21,
	0x6B, 0x72, 0x79, 0x80, 0x83, 0x8A, 0x8F, 0x96, 0x9D, 0x71,
	0xA5, 0xB0, 0xBF, 0xC8, 0xF2, 0xD4, 0xE6, 0xF5, 0xFE, 0xF3,
};

static u8 A14_FT8720_191[] = {
	0x00,
	0xF0,
};

static u8 A14_FT8720_192[] = {
	0xE1,
	0x15, 0x15, 0x15, 0x17, 0x1E, 0x1C, 0x23, 0x2A, 0x34, 0x30,
	0x3C, 0x42, 0x48, 0x4C, 0x23, 0x51,
};

static u8 A14_FT8720_193[] = {
	0x00,
	0x00,
};

static u8 A14_FT8720_194[] = {
	0xE2,
	0x58, 0x5F, 0x65, 0x21, 0x6B, 0x72, 0x79, 0x80, 0x83, 0x8A,
	0x8F, 0x96, 0x9D, 0x71, 0xA5, 0xB0, 0xBF, 0xC8, 0xF2, 0xD4,
	0xE6, 0xF5, 0xFE, 0xF3,
};

static u8 A14_FT8720_195[] = {
	0x00,
	0xE0,
};

static u8 A14_FT8720_196[] = {
	0xCF,
	0x34,
};

static u8 A14_FT8720_197[] = {
	0x00,
	0x85,
};

static u8 A14_FT8720_198[] = {
	0xA7,
	0x41,
};

static u8 A14_FT8720_199[] = {
	0x00,
	0xB0,
};

static u8 A14_FT8720_200[] = {
	0xB3,
	0x00,
};

static u8 A14_FT8720_201[] = {
	0x00,
	0x83,
};

static u8 A14_FT8720_202[] = {
	0xB0,
	0x63,
};

static u8 A14_FT8720_203[] = {
	0x00,
	0x93,
};

static u8 A14_FT8720_204[] = {
	0xC4,
	0x08,
};

static u8 A14_FT8720_205[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_206[] = {
	0xB3,
	0x22,
};

static u8 A14_FT8720_207[] = {
	0x00,
	0xA1,
};

static u8 A14_FT8720_208[] = {
	0xC5,
	0x40,
};

static u8 A14_FT8720_209[] = {
	0x00,
	0x9D,
};

static u8 A14_FT8720_210[] = {
	0xC5,
	0x44,
};

static u8 A14_FT8720_211[] = {
	0x00,
	0x94,
};

static u8 A14_FT8720_212[] = {
	0xC5,
	0x04,
};

static u8 A14_FT8720_213[] = {
	0x00,
	0x98,
};

static u8 A14_FT8720_214[] = {
	0xC5,
	0x24,
};

static u8 A14_FT8720_215[] = {
	0x00,
	0xB0,
};

static u8 A14_FT8720_216[] = {
	0xCA,
	0x6B, 0x6B, 0x09,
};

static u8 A14_FT8720_217[] = {
	0x00,
	0x80,
};

static u8 A14_FT8720_218[] = {
	0xCA,
	0xE9, 0xD6, 0xC2, 0xB6, 0xAD, 0xA2, 0x9B, 0x94, 0x8F, 0x8B,
	0x87, 0x83,
};

static u8 A14_FT8720_219[] = {
	0x00,
	0x90,
};

static u8 A14_FT8720_220[] = {
	0xCA,
	0xFE, 0xFF, 0x66, 0xFC, 0xFF, 0xCC, 0xFA, 0xFF, 0x66,
};

static u8 A14_FT8720_221[] = {
	0x1C,
	0x02,
};

static u8 A14_FT8720_222[] = {
	0x00,
	0x00,
};

static u8 A14_FT8720_223[] = {
	0xFF,
	0xFF, 0xFF, 0xFF,
};

static DEFINE_STATIC_PACKET(a14_sleep_out, DSI_PKT_TYPE_WR, A14_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a14_sleep_in, DSI_PKT_TYPE_WR, A14_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a14_display_on, DSI_PKT_TYPE_WR, A14_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a14_display_off, DSI_PKT_TYPE_WR, A14_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(a14_brightness_mode, DSI_PKT_TYPE_WR, A14_BRIGHTNESS_MODE, 0);
static DEFINE_STATIC_PACKET(a14_cabc_mode, DSI_PKT_TYPE_WR, A14_CABC_MODE, 0);
static DEFINE_STATIC_PACKET(a14_te_on, DSI_PKT_TYPE_WR, A14_TE_ON, 0);

static DEFINE_PKTUI(a14_brightness, &a14_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a14_brightness, DSI_PKT_TYPE_WR, A14_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_001, DSI_PKT_TYPE_WR, A14_FT8720_001, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_002, DSI_PKT_TYPE_WR, A14_FT8720_002, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_003, DSI_PKT_TYPE_WR, A14_FT8720_003, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_004, DSI_PKT_TYPE_WR, A14_FT8720_004, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_005, DSI_PKT_TYPE_WR, A14_FT8720_005, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_006, DSI_PKT_TYPE_WR, A14_FT8720_006, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_007, DSI_PKT_TYPE_WR, A14_FT8720_007, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_008, DSI_PKT_TYPE_WR, A14_FT8720_008, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_009, DSI_PKT_TYPE_WR, A14_FT8720_009, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_010, DSI_PKT_TYPE_WR, A14_FT8720_010, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_011, DSI_PKT_TYPE_WR, A14_FT8720_011, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_012, DSI_PKT_TYPE_WR, A14_FT8720_012, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_013, DSI_PKT_TYPE_WR, A14_FT8720_013, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_014, DSI_PKT_TYPE_WR, A14_FT8720_014, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_015, DSI_PKT_TYPE_WR, A14_FT8720_015, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_016, DSI_PKT_TYPE_WR, A14_FT8720_016, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_017, DSI_PKT_TYPE_WR, A14_FT8720_017, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_018, DSI_PKT_TYPE_WR, A14_FT8720_018, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_019, DSI_PKT_TYPE_WR, A14_FT8720_019, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_020, DSI_PKT_TYPE_WR, A14_FT8720_020, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_021, DSI_PKT_TYPE_WR, A14_FT8720_021, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_022, DSI_PKT_TYPE_WR, A14_FT8720_022, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_023, DSI_PKT_TYPE_WR, A14_FT8720_023, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_024, DSI_PKT_TYPE_WR, A14_FT8720_024, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_025, DSI_PKT_TYPE_WR, A14_FT8720_025, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_026, DSI_PKT_TYPE_WR, A14_FT8720_026, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_027, DSI_PKT_TYPE_WR, A14_FT8720_027, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_028, DSI_PKT_TYPE_WR, A14_FT8720_028, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_029, DSI_PKT_TYPE_WR, A14_FT8720_029, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_030, DSI_PKT_TYPE_WR, A14_FT8720_030, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_031, DSI_PKT_TYPE_WR, A14_FT8720_031, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_032, DSI_PKT_TYPE_WR, A14_FT8720_032, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_033, DSI_PKT_TYPE_WR, A14_FT8720_033, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_034, DSI_PKT_TYPE_WR, A14_FT8720_034, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_035, DSI_PKT_TYPE_WR, A14_FT8720_035, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_036, DSI_PKT_TYPE_WR, A14_FT8720_036, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_037, DSI_PKT_TYPE_WR, A14_FT8720_037, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_038, DSI_PKT_TYPE_WR, A14_FT8720_038, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_039, DSI_PKT_TYPE_WR, A14_FT8720_039, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_040, DSI_PKT_TYPE_WR, A14_FT8720_040, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_041, DSI_PKT_TYPE_WR, A14_FT8720_041, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_042, DSI_PKT_TYPE_WR, A14_FT8720_042, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_043, DSI_PKT_TYPE_WR, A14_FT8720_043, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_044, DSI_PKT_TYPE_WR, A14_FT8720_044, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_045, DSI_PKT_TYPE_WR, A14_FT8720_045, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_046, DSI_PKT_TYPE_WR, A14_FT8720_046, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_047, DSI_PKT_TYPE_WR, A14_FT8720_047, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_048, DSI_PKT_TYPE_WR, A14_FT8720_048, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_049, DSI_PKT_TYPE_WR, A14_FT8720_049, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_050, DSI_PKT_TYPE_WR, A14_FT8720_050, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_051, DSI_PKT_TYPE_WR, A14_FT8720_051, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_052, DSI_PKT_TYPE_WR, A14_FT8720_052, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_053, DSI_PKT_TYPE_WR, A14_FT8720_053, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_054, DSI_PKT_TYPE_WR, A14_FT8720_054, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_055, DSI_PKT_TYPE_WR, A14_FT8720_055, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_056, DSI_PKT_TYPE_WR, A14_FT8720_056, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_057, DSI_PKT_TYPE_WR, A14_FT8720_057, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_058, DSI_PKT_TYPE_WR, A14_FT8720_058, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_059, DSI_PKT_TYPE_WR, A14_FT8720_059, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_060, DSI_PKT_TYPE_WR, A14_FT8720_060, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_061, DSI_PKT_TYPE_WR, A14_FT8720_061, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_062, DSI_PKT_TYPE_WR, A14_FT8720_062, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_063, DSI_PKT_TYPE_WR, A14_FT8720_063, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_064, DSI_PKT_TYPE_WR, A14_FT8720_064, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_065, DSI_PKT_TYPE_WR, A14_FT8720_065, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_066, DSI_PKT_TYPE_WR, A14_FT8720_066, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_067, DSI_PKT_TYPE_WR, A14_FT8720_067, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_068, DSI_PKT_TYPE_WR, A14_FT8720_068, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_069, DSI_PKT_TYPE_WR, A14_FT8720_069, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_070, DSI_PKT_TYPE_WR, A14_FT8720_070, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_071, DSI_PKT_TYPE_WR, A14_FT8720_071, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_072, DSI_PKT_TYPE_WR, A14_FT8720_072, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_073, DSI_PKT_TYPE_WR, A14_FT8720_073, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_074, DSI_PKT_TYPE_WR, A14_FT8720_074, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_075, DSI_PKT_TYPE_WR, A14_FT8720_075, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_076, DSI_PKT_TYPE_WR, A14_FT8720_076, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_077, DSI_PKT_TYPE_WR, A14_FT8720_077, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_078, DSI_PKT_TYPE_WR, A14_FT8720_078, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_079, DSI_PKT_TYPE_WR, A14_FT8720_079, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_080, DSI_PKT_TYPE_WR, A14_FT8720_080, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_081, DSI_PKT_TYPE_WR, A14_FT8720_081, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_082, DSI_PKT_TYPE_WR, A14_FT8720_082, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_083, DSI_PKT_TYPE_WR, A14_FT8720_083, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_084, DSI_PKT_TYPE_WR, A14_FT8720_084, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_085, DSI_PKT_TYPE_WR, A14_FT8720_085, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_086, DSI_PKT_TYPE_WR, A14_FT8720_086, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_087, DSI_PKT_TYPE_WR, A14_FT8720_087, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_088, DSI_PKT_TYPE_WR, A14_FT8720_088, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_089, DSI_PKT_TYPE_WR, A14_FT8720_089, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_090, DSI_PKT_TYPE_WR, A14_FT8720_090, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_091, DSI_PKT_TYPE_WR, A14_FT8720_091, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_092, DSI_PKT_TYPE_WR, A14_FT8720_092, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_093, DSI_PKT_TYPE_WR, A14_FT8720_093, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_094, DSI_PKT_TYPE_WR, A14_FT8720_094, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_095, DSI_PKT_TYPE_WR, A14_FT8720_095, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_096, DSI_PKT_TYPE_WR, A14_FT8720_096, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_097, DSI_PKT_TYPE_WR, A14_FT8720_097, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_098, DSI_PKT_TYPE_WR, A14_FT8720_098, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_099, DSI_PKT_TYPE_WR, A14_FT8720_099, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_100, DSI_PKT_TYPE_WR, A14_FT8720_100, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_101, DSI_PKT_TYPE_WR, A14_FT8720_101, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_102, DSI_PKT_TYPE_WR, A14_FT8720_102, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_103, DSI_PKT_TYPE_WR, A14_FT8720_103, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_104, DSI_PKT_TYPE_WR, A14_FT8720_104, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_105, DSI_PKT_TYPE_WR, A14_FT8720_105, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_106, DSI_PKT_TYPE_WR, A14_FT8720_106, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_107, DSI_PKT_TYPE_WR, A14_FT8720_107, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_108, DSI_PKT_TYPE_WR, A14_FT8720_108, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_109, DSI_PKT_TYPE_WR, A14_FT8720_109, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_110, DSI_PKT_TYPE_WR, A14_FT8720_110, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_111, DSI_PKT_TYPE_WR, A14_FT8720_111, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_112, DSI_PKT_TYPE_WR, A14_FT8720_112, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_113, DSI_PKT_TYPE_WR, A14_FT8720_113, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_114, DSI_PKT_TYPE_WR, A14_FT8720_114, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_115, DSI_PKT_TYPE_WR, A14_FT8720_115, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_116, DSI_PKT_TYPE_WR, A14_FT8720_116, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_117, DSI_PKT_TYPE_WR, A14_FT8720_117, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_118, DSI_PKT_TYPE_WR, A14_FT8720_118, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_119, DSI_PKT_TYPE_WR, A14_FT8720_119, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_120, DSI_PKT_TYPE_WR, A14_FT8720_120, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_121, DSI_PKT_TYPE_WR, A14_FT8720_121, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_122, DSI_PKT_TYPE_WR, A14_FT8720_122, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_123, DSI_PKT_TYPE_WR, A14_FT8720_123, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_124, DSI_PKT_TYPE_WR, A14_FT8720_124, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_125, DSI_PKT_TYPE_WR, A14_FT8720_125, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_126, DSI_PKT_TYPE_WR, A14_FT8720_126, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_127, DSI_PKT_TYPE_WR, A14_FT8720_127, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_128, DSI_PKT_TYPE_WR, A14_FT8720_128, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_129, DSI_PKT_TYPE_WR, A14_FT8720_129, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_130, DSI_PKT_TYPE_WR, A14_FT8720_130, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_131, DSI_PKT_TYPE_WR, A14_FT8720_131, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_132, DSI_PKT_TYPE_WR, A14_FT8720_132, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_133, DSI_PKT_TYPE_WR, A14_FT8720_133, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_134, DSI_PKT_TYPE_WR, A14_FT8720_134, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_135, DSI_PKT_TYPE_WR, A14_FT8720_135, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_136, DSI_PKT_TYPE_WR, A14_FT8720_136, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_137, DSI_PKT_TYPE_WR, A14_FT8720_137, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_138, DSI_PKT_TYPE_WR, A14_FT8720_138, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_139, DSI_PKT_TYPE_WR, A14_FT8720_139, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_140, DSI_PKT_TYPE_WR, A14_FT8720_140, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_141, DSI_PKT_TYPE_WR, A14_FT8720_141, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_142, DSI_PKT_TYPE_WR, A14_FT8720_142, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_143, DSI_PKT_TYPE_WR, A14_FT8720_143, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_144, DSI_PKT_TYPE_WR, A14_FT8720_144, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_145, DSI_PKT_TYPE_WR, A14_FT8720_145, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_146, DSI_PKT_TYPE_WR, A14_FT8720_146, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_147, DSI_PKT_TYPE_WR, A14_FT8720_147, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_148, DSI_PKT_TYPE_WR, A14_FT8720_148, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_149, DSI_PKT_TYPE_WR, A14_FT8720_149, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_150, DSI_PKT_TYPE_WR, A14_FT8720_150, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_151, DSI_PKT_TYPE_WR, A14_FT8720_151, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_152, DSI_PKT_TYPE_WR, A14_FT8720_152, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_153, DSI_PKT_TYPE_WR, A14_FT8720_153, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_154, DSI_PKT_TYPE_WR, A14_FT8720_154, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_155, DSI_PKT_TYPE_WR, A14_FT8720_155, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_156, DSI_PKT_TYPE_WR, A14_FT8720_156, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_157, DSI_PKT_TYPE_WR, A14_FT8720_157, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_158, DSI_PKT_TYPE_WR, A14_FT8720_158, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_159, DSI_PKT_TYPE_WR, A14_FT8720_159, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_160, DSI_PKT_TYPE_WR, A14_FT8720_160, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_161, DSI_PKT_TYPE_WR, A14_FT8720_161, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_162, DSI_PKT_TYPE_WR, A14_FT8720_162, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_163, DSI_PKT_TYPE_WR, A14_FT8720_163, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_164, DSI_PKT_TYPE_WR, A14_FT8720_164, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_165, DSI_PKT_TYPE_WR, A14_FT8720_165, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_166, DSI_PKT_TYPE_WR, A14_FT8720_166, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_167, DSI_PKT_TYPE_WR, A14_FT8720_167, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_168, DSI_PKT_TYPE_WR, A14_FT8720_168, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_169, DSI_PKT_TYPE_WR, A14_FT8720_169, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_170, DSI_PKT_TYPE_WR, A14_FT8720_170, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_171, DSI_PKT_TYPE_WR, A14_FT8720_171, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_172, DSI_PKT_TYPE_WR, A14_FT8720_172, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_173, DSI_PKT_TYPE_WR, A14_FT8720_173, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_174, DSI_PKT_TYPE_WR, A14_FT8720_174, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_175, DSI_PKT_TYPE_WR, A14_FT8720_175, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_176, DSI_PKT_TYPE_WR, A14_FT8720_176, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_177, DSI_PKT_TYPE_WR, A14_FT8720_177, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_178, DSI_PKT_TYPE_WR, A14_FT8720_178, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_179, DSI_PKT_TYPE_WR, A14_FT8720_179, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_180, DSI_PKT_TYPE_WR, A14_FT8720_180, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_181, DSI_PKT_TYPE_WR, A14_FT8720_181, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_182, DSI_PKT_TYPE_WR, A14_FT8720_182, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_183, DSI_PKT_TYPE_WR, A14_FT8720_183, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_184, DSI_PKT_TYPE_WR, A14_FT8720_184, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_185, DSI_PKT_TYPE_WR, A14_FT8720_185, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_186, DSI_PKT_TYPE_WR, A14_FT8720_186, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_187, DSI_PKT_TYPE_WR, A14_FT8720_187, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_188, DSI_PKT_TYPE_WR, A14_FT8720_188, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_189, DSI_PKT_TYPE_WR, A14_FT8720_189, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_190, DSI_PKT_TYPE_WR, A14_FT8720_190, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_191, DSI_PKT_TYPE_WR, A14_FT8720_191, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_192, DSI_PKT_TYPE_WR, A14_FT8720_192, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_193, DSI_PKT_TYPE_WR, A14_FT8720_193, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_194, DSI_PKT_TYPE_WR, A14_FT8720_194, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_195, DSI_PKT_TYPE_WR, A14_FT8720_195, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_196, DSI_PKT_TYPE_WR, A14_FT8720_196, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_197, DSI_PKT_TYPE_WR, A14_FT8720_197, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_198, DSI_PKT_TYPE_WR, A14_FT8720_198, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_199, DSI_PKT_TYPE_WR, A14_FT8720_199, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_200, DSI_PKT_TYPE_WR, A14_FT8720_200, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_201, DSI_PKT_TYPE_WR, A14_FT8720_201, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_202, DSI_PKT_TYPE_WR, A14_FT8720_202, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_203, DSI_PKT_TYPE_WR, A14_FT8720_203, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_204, DSI_PKT_TYPE_WR, A14_FT8720_204, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_205, DSI_PKT_TYPE_WR, A14_FT8720_205, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_206, DSI_PKT_TYPE_WR, A14_FT8720_206, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_207, DSI_PKT_TYPE_WR, A14_FT8720_207, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_208, DSI_PKT_TYPE_WR, A14_FT8720_208, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_209, DSI_PKT_TYPE_WR, A14_FT8720_209, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_210, DSI_PKT_TYPE_WR, A14_FT8720_210, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_211, DSI_PKT_TYPE_WR, A14_FT8720_211, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_212, DSI_PKT_TYPE_WR, A14_FT8720_212, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_213, DSI_PKT_TYPE_WR, A14_FT8720_213, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_214, DSI_PKT_TYPE_WR, A14_FT8720_214, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_215, DSI_PKT_TYPE_WR, A14_FT8720_215, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_216, DSI_PKT_TYPE_WR, A14_FT8720_216, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_217, DSI_PKT_TYPE_WR, A14_FT8720_217, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_218, DSI_PKT_TYPE_WR, A14_FT8720_218, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_219, DSI_PKT_TYPE_WR, A14_FT8720_219, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_220, DSI_PKT_TYPE_WR, A14_FT8720_220, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_221, DSI_PKT_TYPE_WR, A14_FT8720_221, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_222, DSI_PKT_TYPE_WR, A14_FT8720_222, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_skyworth_223, DSI_PKT_TYPE_WR, A14_FT8720_223, 0);
static DEFINE_STATIC_PACKET(a14_ft8720_bightness_0, DSI_PKT_TYPE_WR, A14_BRIGHTNESS_0, 0);


static DEFINE_PANEL_MDELAY(a14_wait_display_off, 1 * 17); /* 1 frame */
static DEFINE_PANEL_MDELAY(a14_wait_sleep_in, 4 * 17); /* 4 frame */
static DEFINE_PANEL_MDELAY(a14_wait_2msec, 2);
static DEFINE_PANEL_MDELAY(a14_wait_20msec, 20);
static DEFINE_PANEL_MDELAY(a14_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(a14_wait_120msec, 120);
static DEFINE_PANEL_MDELAY(a14_wait_blic_off, 1);


static void *a14_init_cmdtbl[] = {
	&ft8720_skyworth_restbl[RES_ID],
	&PKTINFO(a14_ft8720_skyworth_001),
	&PKTINFO(a14_ft8720_skyworth_002),
	&PKTINFO(a14_ft8720_skyworth_003),
	&PKTINFO(a14_ft8720_skyworth_004),
	&PKTINFO(a14_ft8720_skyworth_005),
	&PKTINFO(a14_ft8720_skyworth_006),
	&PKTINFO(a14_ft8720_skyworth_007),
	&PKTINFO(a14_ft8720_skyworth_008),
	&PKTINFO(a14_ft8720_skyworth_009),
	&PKTINFO(a14_ft8720_skyworth_010),
	&PKTINFO(a14_ft8720_skyworth_011),
	&PKTINFO(a14_ft8720_skyworth_012),
	&PKTINFO(a14_ft8720_skyworth_013),
	&PKTINFO(a14_ft8720_skyworth_014),
	&PKTINFO(a14_ft8720_skyworth_015),
	&PKTINFO(a14_ft8720_skyworth_016),
	&PKTINFO(a14_ft8720_skyworth_017),
	&PKTINFO(a14_ft8720_skyworth_018),
	&PKTINFO(a14_ft8720_skyworth_019),
	&PKTINFO(a14_ft8720_skyworth_020),
	&PKTINFO(a14_ft8720_skyworth_021),
	&PKTINFO(a14_ft8720_skyworth_022),
	&PKTINFO(a14_ft8720_skyworth_023),
	&PKTINFO(a14_ft8720_skyworth_024),
	&PKTINFO(a14_ft8720_skyworth_025),
	&PKTINFO(a14_ft8720_skyworth_026),
	&PKTINFO(a14_ft8720_skyworth_027),
	&PKTINFO(a14_ft8720_skyworth_028),
	&PKTINFO(a14_ft8720_skyworth_029),
	&PKTINFO(a14_ft8720_skyworth_030),
	&PKTINFO(a14_ft8720_skyworth_031),
	&PKTINFO(a14_ft8720_skyworth_032),
	&PKTINFO(a14_ft8720_skyworth_033),
	&PKTINFO(a14_ft8720_skyworth_034),
	&PKTINFO(a14_ft8720_skyworth_035),
	&PKTINFO(a14_ft8720_skyworth_036),
	&PKTINFO(a14_ft8720_skyworth_037),
	&PKTINFO(a14_ft8720_skyworth_038),
	&PKTINFO(a14_ft8720_skyworth_039),
	&PKTINFO(a14_ft8720_skyworth_040),
	&PKTINFO(a14_ft8720_skyworth_041),
	&PKTINFO(a14_ft8720_skyworth_042),
	&PKTINFO(a14_ft8720_skyworth_043),
	&PKTINFO(a14_ft8720_skyworth_044),
	&PKTINFO(a14_ft8720_skyworth_045),
	&PKTINFO(a14_ft8720_skyworth_046),
	&PKTINFO(a14_ft8720_skyworth_047),
	&PKTINFO(a14_ft8720_skyworth_048),
	&PKTINFO(a14_ft8720_skyworth_049),
	&PKTINFO(a14_ft8720_skyworth_050),
	&PKTINFO(a14_ft8720_skyworth_051),
	&PKTINFO(a14_ft8720_skyworth_052),
	&PKTINFO(a14_ft8720_skyworth_053),
	&PKTINFO(a14_ft8720_skyworth_054),
	&PKTINFO(a14_ft8720_skyworth_055),
	&PKTINFO(a14_ft8720_skyworth_056),
	&PKTINFO(a14_ft8720_skyworth_057),
	&PKTINFO(a14_ft8720_skyworth_058),
	&PKTINFO(a14_ft8720_skyworth_059),
	&PKTINFO(a14_ft8720_skyworth_060),
	&PKTINFO(a14_ft8720_skyworth_061),
	&PKTINFO(a14_ft8720_skyworth_062),
	&PKTINFO(a14_ft8720_skyworth_063),
	&PKTINFO(a14_ft8720_skyworth_064),
	&PKTINFO(a14_ft8720_skyworth_065),
	&PKTINFO(a14_ft8720_skyworth_066),
	&PKTINFO(a14_ft8720_skyworth_067),
	&PKTINFO(a14_ft8720_skyworth_068),
	&PKTINFO(a14_ft8720_skyworth_069),
	&PKTINFO(a14_ft8720_skyworth_070),
	&PKTINFO(a14_ft8720_skyworth_071),
	&PKTINFO(a14_ft8720_skyworth_072),
	&PKTINFO(a14_ft8720_skyworth_073),
	&PKTINFO(a14_ft8720_skyworth_074),
	&PKTINFO(a14_ft8720_skyworth_075),
	&PKTINFO(a14_ft8720_skyworth_076),
	&PKTINFO(a14_ft8720_skyworth_077),
	&PKTINFO(a14_ft8720_skyworth_078),
	&PKTINFO(a14_ft8720_skyworth_079),
	&PKTINFO(a14_ft8720_skyworth_080),
	&PKTINFO(a14_ft8720_skyworth_081),
	&PKTINFO(a14_ft8720_skyworth_082),
	&PKTINFO(a14_ft8720_skyworth_083),
	&PKTINFO(a14_ft8720_skyworth_084),
	&PKTINFO(a14_ft8720_skyworth_085),
	&PKTINFO(a14_ft8720_skyworth_086),
	&PKTINFO(a14_ft8720_skyworth_087),
	&PKTINFO(a14_ft8720_skyworth_088),
	&PKTINFO(a14_ft8720_skyworth_089),
	&PKTINFO(a14_ft8720_skyworth_090),
	&PKTINFO(a14_ft8720_skyworth_091),
	&PKTINFO(a14_ft8720_skyworth_092),
	&PKTINFO(a14_ft8720_skyworth_093),
	&PKTINFO(a14_ft8720_skyworth_094),
	&PKTINFO(a14_ft8720_skyworth_095),
	&PKTINFO(a14_ft8720_skyworth_096),
	&PKTINFO(a14_ft8720_skyworth_097),
	&PKTINFO(a14_ft8720_skyworth_098),
	&PKTINFO(a14_ft8720_skyworth_099),
	&PKTINFO(a14_ft8720_skyworth_100),
	&PKTINFO(a14_ft8720_skyworth_101),
	&PKTINFO(a14_ft8720_skyworth_102),
	&PKTINFO(a14_ft8720_skyworth_103),
	&PKTINFO(a14_ft8720_skyworth_104),
	&PKTINFO(a14_ft8720_skyworth_105),
	&PKTINFO(a14_ft8720_skyworth_106),
	&PKTINFO(a14_ft8720_skyworth_107),
	&PKTINFO(a14_ft8720_skyworth_108),
	&PKTINFO(a14_ft8720_skyworth_109),
	&PKTINFO(a14_ft8720_skyworth_110),
	&PKTINFO(a14_ft8720_skyworth_111),
	&PKTINFO(a14_ft8720_skyworth_112),
	&PKTINFO(a14_ft8720_skyworth_113),
	&PKTINFO(a14_ft8720_skyworth_114),
	&PKTINFO(a14_ft8720_skyworth_115),
	&PKTINFO(a14_ft8720_skyworth_116),
	&PKTINFO(a14_ft8720_skyworth_117),
	&PKTINFO(a14_ft8720_skyworth_118),
	&PKTINFO(a14_ft8720_skyworth_119),
	&PKTINFO(a14_ft8720_skyworth_120),
	&PKTINFO(a14_ft8720_skyworth_121),
	&PKTINFO(a14_ft8720_skyworth_122),
	&PKTINFO(a14_ft8720_skyworth_123),
	&PKTINFO(a14_ft8720_skyworth_124),
	&PKTINFO(a14_ft8720_skyworth_125),
	&PKTINFO(a14_ft8720_skyworth_126),
	&PKTINFO(a14_ft8720_skyworth_127),
	&PKTINFO(a14_ft8720_skyworth_128),
	&PKTINFO(a14_ft8720_skyworth_129),
	&PKTINFO(a14_ft8720_skyworth_130),
	&PKTINFO(a14_ft8720_skyworth_131),
	&PKTINFO(a14_ft8720_skyworth_132),
	&PKTINFO(a14_ft8720_skyworth_133),
	&PKTINFO(a14_ft8720_skyworth_134),
	&PKTINFO(a14_ft8720_skyworth_135),
	&PKTINFO(a14_ft8720_skyworth_136),
	&PKTINFO(a14_ft8720_skyworth_137),
	&PKTINFO(a14_ft8720_skyworth_138),
	&PKTINFO(a14_ft8720_skyworth_139),
	&PKTINFO(a14_ft8720_skyworth_140),
	&PKTINFO(a14_ft8720_skyworth_141),
	&PKTINFO(a14_ft8720_skyworth_142),
	&PKTINFO(a14_ft8720_skyworth_143),
	&PKTINFO(a14_ft8720_skyworth_144),
	&PKTINFO(a14_ft8720_skyworth_145),
	&PKTINFO(a14_ft8720_skyworth_146),
	&PKTINFO(a14_ft8720_skyworth_147),
	&PKTINFO(a14_ft8720_skyworth_148),
	&PKTINFO(a14_ft8720_skyworth_149),
	&PKTINFO(a14_ft8720_skyworth_150),
	&PKTINFO(a14_ft8720_skyworth_151),
	&PKTINFO(a14_ft8720_skyworth_152),
	&PKTINFO(a14_ft8720_skyworth_153),
	&PKTINFO(a14_ft8720_skyworth_154),
	&PKTINFO(a14_ft8720_skyworth_155),
	&PKTINFO(a14_ft8720_skyworth_156),
	&PKTINFO(a14_ft8720_skyworth_157),
	&PKTINFO(a14_ft8720_skyworth_158),
	&PKTINFO(a14_ft8720_skyworth_159),
	&PKTINFO(a14_ft8720_skyworth_160),
	&PKTINFO(a14_ft8720_skyworth_161),
	&PKTINFO(a14_ft8720_skyworth_162),
	&PKTINFO(a14_ft8720_skyworth_163),
	&PKTINFO(a14_ft8720_skyworth_164),
	&PKTINFO(a14_ft8720_skyworth_165),
	&PKTINFO(a14_ft8720_skyworth_166),
	&PKTINFO(a14_ft8720_skyworth_167),
	&PKTINFO(a14_ft8720_skyworth_168),
	&PKTINFO(a14_ft8720_skyworth_169),
	&PKTINFO(a14_ft8720_skyworth_170),
	&PKTINFO(a14_ft8720_skyworth_171),
	&PKTINFO(a14_ft8720_skyworth_172),
	&PKTINFO(a14_ft8720_skyworth_173),
	&PKTINFO(a14_ft8720_skyworth_174),
	&PKTINFO(a14_ft8720_skyworth_175),
	&PKTINFO(a14_ft8720_skyworth_176),
	&PKTINFO(a14_ft8720_skyworth_177),
	&PKTINFO(a14_ft8720_skyworth_178),
	&PKTINFO(a14_ft8720_skyworth_179),
	&PKTINFO(a14_ft8720_skyworth_180),
	&PKTINFO(a14_ft8720_skyworth_181),
	&PKTINFO(a14_ft8720_skyworth_182),
	&PKTINFO(a14_ft8720_skyworth_183),
	&PKTINFO(a14_ft8720_skyworth_184),
	&PKTINFO(a14_ft8720_skyworth_185),
	&PKTINFO(a14_ft8720_skyworth_186),
	&PKTINFO(a14_ft8720_skyworth_187),
	&PKTINFO(a14_ft8720_skyworth_188),
	&PKTINFO(a14_ft8720_skyworth_189),
	&PKTINFO(a14_ft8720_skyworth_190),
	&PKTINFO(a14_ft8720_skyworth_191),
	&PKTINFO(a14_ft8720_skyworth_192),
	&PKTINFO(a14_ft8720_skyworth_193),
	&PKTINFO(a14_ft8720_skyworth_194),
	&PKTINFO(a14_ft8720_skyworth_195),
	&PKTINFO(a14_ft8720_skyworth_196),
	&PKTINFO(a14_ft8720_skyworth_197),
	&PKTINFO(a14_ft8720_skyworth_198),
	&PKTINFO(a14_ft8720_skyworth_199),
	&PKTINFO(a14_ft8720_skyworth_200),
	&PKTINFO(a14_ft8720_skyworth_201),
	&PKTINFO(a14_ft8720_skyworth_202),
	&PKTINFO(a14_ft8720_skyworth_203),
	&PKTINFO(a14_ft8720_skyworth_204),
	&PKTINFO(a14_ft8720_skyworth_205),
	&PKTINFO(a14_ft8720_skyworth_206),
	&PKTINFO(a14_ft8720_skyworth_207),
	&PKTINFO(a14_ft8720_skyworth_208),
	&PKTINFO(a14_ft8720_skyworth_209),
	&PKTINFO(a14_ft8720_skyworth_210),
	&PKTINFO(a14_ft8720_skyworth_211),
	&PKTINFO(a14_ft8720_skyworth_212),
	&PKTINFO(a14_ft8720_skyworth_213),
	&PKTINFO(a14_ft8720_skyworth_214),
	&PKTINFO(a14_ft8720_skyworth_215),
	&PKTINFO(a14_ft8720_skyworth_216),
	&PKTINFO(a14_ft8720_skyworth_217),
	&PKTINFO(a14_ft8720_skyworth_218),
	&PKTINFO(a14_ft8720_skyworth_219),
	&PKTINFO(a14_ft8720_skyworth_220),
	&PKTINFO(a14_ft8720_skyworth_221),
	&PKTINFO(a14_ft8720_skyworth_222),
	&PKTINFO(a14_ft8720_skyworth_223),
	&PKTINFO(a14_ft8720_bightness_0),

	&PKTINFO(a14_sleep_out),
	&DLYINFO(a14_wait_120msec),
};

static void *a14_res_init_cmdtbl[] = {
	&ft8720_skyworth_restbl[RES_ID],
};

static void *a14_set_bl_cmdtbl[] = {
	&PKTINFO(a14_brightness), //51h
};

static void *a14_display_on_cmdtbl[] = {
	&PKTINFO(a14_display_on),
	&DLYINFO(a14_wait_20msec),
	&PKTINFO(a14_brightness_mode),
	&PKTINFO(a14_cabc_mode),
	&PKTINFO(a14_te_on),
};

static void *a14_display_off_cmdtbl[] = {
	&PKTINFO(a14_display_off),
	&DLYINFO(a14_wait_display_off),
};

static void *a14_exit_cmdtbl[] = {
	&PKTINFO(a14_sleep_in),
};

/* ===================================================================================== */
/* ================================= [EA8076 I2C TABLE] ===========+++================= */
/* ===================================================================================== */
static u8 FT8720_A14_I2C_INIT[] = {
	0x0C, 0x24,
	0x0D, 0x1E,
	0x0E, 0x1E,
	0x09, 0x99,
	0x02, 0x6B,
	0x03, 0x24,
	0x11, 0x74,
	0x04, 0x02,
	0x05, 0xA3,
	0x10, 0x66,
	0x08, 0x13,
};

static u8 FT8720_A14_I2C_EXIT_BLEN[] = {
	0x08, 0x00,
};

static u8 FT8720_A14_I2C_DUMP[] = {
	0x0C, 0x24,
	0x0D, 0x1E,
	0x0E, 0x1E,
	0x09, 0x99,
	0x02, 0x6B,
	0x03, 0x0D,
	0x11, 0x74,
	0x04, 0x00,
	0x05, 0xD6,
	0x10, 0x66,
	0x08, 0x13,
};

static DEFINE_STATIC_PACKET(ft8720_skyworth_a14_i2c_init, I2C_PKT_TYPE_WR, FT8720_A14_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(ft8720_skyworth_a14_i2c_exit_blen, I2C_PKT_TYPE_WR, FT8720_A14_I2C_EXIT_BLEN, 0);
static DEFINE_STATIC_PACKET(ft8720_skyworth_a14_i2c_dump, I2C_PKT_TYPE_RD, FT8720_A14_I2C_DUMP, 0);

static void *ft8720_skyworth_a14_init_cmdtbl[] = {
	&PKTINFO(ft8720_skyworth_a14_i2c_init),
};

static void *ft8720_skyworth_a14_exit_cmdtbl[] = {
	&PKTINFO(ft8720_skyworth_a14_i2c_exit_blen),
};

static void *ft8720_skyworth_a14_dump_cmdtbl[] = {
	&PKTINFO(ft8720_skyworth_a14_i2c_dump),
};


static struct seqinfo a14_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a14_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a14_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a14_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a14_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a14_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a14_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", ft8720_skyworth_a14_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", ft8720_skyworth_a14_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", ft8720_skyworth_a14_dump_cmdtbl),
#endif
};

struct common_panel_info ft8720_skyworth_a14_default_panel_info = {
	.ldi_name = "ft8720_skyworth",
	.name = "ft8720_skyworth_a14_default",
	.model = "SKYWORTH_6_517_inch",
	.vendor = "SKW",
	.id = 0x8BC270,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
		.wait_tx_done = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(ft8720_skyworth_a14_resol),
		.resol = ft8720_skyworth_a14_resol,
	},
	.maptbl = a14_maptbl,
	.nr_maptbl = ARRAY_SIZE(a14_maptbl),
	.seqtbl = a14_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a14_seqtbl),
	.rditbl = ft8720_skyworth_rditbl,
	.nr_rditbl = ARRAY_SIZE(ft8720_skyworth_rditbl),
	.restbl = ft8720_skyworth_restbl,
	.nr_restbl = ARRAY_SIZE(ft8720_skyworth_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&ft8720_skyworth_a14_panel_dimming_info,
	},
	.i2c_data = &ft8720_skyworth_a14_i2c_data,
};

int __init ft8720_skyworth_a14_panel_init(void)
{
	register_common_panel(&ft8720_skyworth_a14_default_panel_info);

	return 0;
}
//arch_initcall(ft8720_skyworth_a14_panel_init)
#endif /* __FT8720_A14_PANEL_H__ */

