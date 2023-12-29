/*
 * linux/drivers/video/fbdev/exynos/panel/nt36672c_csot/nt36672c_csot_a14_panel.h
 *
 * Header file for NT36672C Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NT36672C_A14_PANEL_H__
#define __NT36672C_A14_PANEL_H__
#include "../panel_drv.h"
#include "nt36672c_csot.h"

#include "nt36672c_csot_a14_panel_dimming.h"
#include "nt36672c_csot_a14_panel_i2c.h"

#include "nt36672c_csot_a14_resol.h"

#undef __pn_name__
#define __pn_name__	a14

#undef __PN_NAME__
#define __PN_NAME__	A14

static struct seqinfo a14_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [NT36672C READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [NT36672C RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [NT36672C MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a14_brt_table[NT36672C_TOTAL_NR_LUMINANCE][1] = {
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
/* ============================== [NT36672C COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A14_SLEEP_OUT[] = { 0x11 };
static u8 A14_SLEEP_IN[] = { 0x10 };
static u8 A14_DISPLAY_OFF[] = { 0x28 };
static u8 A14_DISPLAY_ON[] = { 0x29 };

static u8 A14_BRIGHTNESS[] = {
	0x51,
	0xFF,
};

static u8 A14_BRIGHTNESS_MODE[] = {
	0x53,
	0x24,
};

static u8 A14_NT36672C_CSOT_001[] = {
	0xFF,
	0x10,
};

static u8 A14_NT36672C_CSOT_002[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_003[] = {
	0xB0,
	0x00,
};

static u8 A14_NT36672C_CSOT_004[] = {
	0xC1,
	0x89, 0x28, 0x00, 0x08, 0x00, 0xAA, 0x02, 0x0E, 0x00, 0x2B,
	0x00, 0x07, 0x0D, 0xB7, 0x0C, 0xB7,
};

static u8 A14_NT36672C_CSOT_005[] = {
	0xC2,
	0x1B, 0xA0,
};

static u8 A14_NT36672C_CSOT_006[] = {
	0xFF,
	0x20,
};

static u8 A14_NT36672C_CSOT_007[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_008[] = {
	0x01,
	0x66,
};

static u8 A14_NT36672C_CSOT_009[] = {
	0x06,
	0x64,
};

static u8 A14_NT36672C_CSOT_010[] = {
	0x07,
	0x28,
};

static u8 A14_NT36672C_CSOT_011[] = {
	0x17,
	0x66,
};

static u8 A14_NT36672C_CSOT_012[] = {
	0x1B,
	0x01,
};

static u8 A14_NT36672C_CSOT_013[] = {
	0x1F,
	0x02,
};

static u8 A14_NT36672C_CSOT_014[] = {
	0x20,
	0x03,
};

static u8 A14_NT36672C_CSOT_015[] = {
	0x5C,
	0x90,
};

static u8 A14_NT36672C_CSOT_016[] = {
	0x5E,
	0x7E,
};

static u8 A14_NT36672C_CSOT_017[] = {
	0x69,
	0xD0,
};

static u8 A14_NT36672C_CSOT_018[] = {
	0x95,
	0xD1,
};

static u8 A14_NT36672C_CSOT_019[] = {
	0x96,
	0xD1,
};

static u8 A14_NT36672C_CSOT_020[] = {
	0xF2,
	0x66,
};

static u8 A14_NT36672C_CSOT_021[] = {
	0xF3,
	0x54,
};

static u8 A14_NT36672C_CSOT_022[] = {
	0xF4,
	0x66,
};

static u8 A14_NT36672C_CSOT_023[] = {
	0xF5,
	0x54,
};

static u8 A14_NT36672C_CSOT_024[] = {
	0xF6,
	0x66,
};

static u8 A14_NT36672C_CSOT_025[] = {
	0xF7,
	0x54,
};

static u8 A14_NT36672C_CSOT_026[] = {
	0xF8,
	0x66,
};

static u8 A14_NT36672C_CSOT_027[] = {
	0xF9,
	0x54,
};

static u8 A14_NT36672C_CSOT_028[] = {
	0xFF,
	0x21,
};

static u8 A14_NT36672C_CSOT_029[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_030[] = {
	0xFF,
	0x24,
};

static u8 A14_NT36672C_CSOT_031[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_032[] = {
	0x00,
	0x26,
};

static u8 A14_NT36672C_CSOT_033[] = {
	0x01,
	0x13,
};

static u8 A14_NT36672C_CSOT_034[] = {
	0x02,
	0x27,
};

static u8 A14_NT36672C_CSOT_035[] = {
	0x03,
	0x15,
};

static u8 A14_NT36672C_CSOT_036[] = {
	0x04,
	0x28,
};

static u8 A14_NT36672C_CSOT_037[] = {
	0x05,
	0x17,
};

static u8 A14_NT36672C_CSOT_038[] = {
	0x07,
	0x24,
};

static u8 A14_NT36672C_CSOT_039[] = {
	0x08,
	0x24,
};

static u8 A14_NT36672C_CSOT_040[] = {
	0x0A,
	0x22,
};

static u8 A14_NT36672C_CSOT_041[] = {
	0x0C,
	0x10,
};

static u8 A14_NT36672C_CSOT_042[] = {
	0x0D,
	0x0F,
};

static u8 A14_NT36672C_CSOT_043[] = {
	0x0E,
	0x01,
};

static u8 A14_NT36672C_CSOT_044[] = {
	0x10,
	0x2D,
};

static u8 A14_NT36672C_CSOT_045[] = {
	0x11,
	0x2F,
};

static u8 A14_NT36672C_CSOT_046[] = {
	0x12,
	0x31,
};

static u8 A14_NT36672C_CSOT_047[] = {
	0x13,
	0x33,
};

static u8 A14_NT36672C_CSOT_048[] = {
	0x15,
	0x0B,
};

static u8 A14_NT36672C_CSOT_049[] = {
	0x17,
	0x0C,
};

static u8 A14_NT36672C_CSOT_050[] = {
	0x18,
	0x26,
};

static u8 A14_NT36672C_CSOT_051[] = {
	0x19,
	0x13,
};

static u8 A14_NT36672C_CSOT_052[] = {
	0x1A,
	0x27,
};

static u8 A14_NT36672C_CSOT_053[] = {
	0x1B,
	0x15,
};

static u8 A14_NT36672C_CSOT_054[] = {
	0x1C,
	0x28,
};

static u8 A14_NT36672C_CSOT_055[] = {
	0x1D,
	0x17,
};

static u8 A14_NT36672C_CSOT_056[] = {
	0x1F,
	0x24,
};

static u8 A14_NT36672C_CSOT_057[] = {
	0x20,
	0x24,
};

static u8 A14_NT36672C_CSOT_058[] = {
	0x22,
	0x22,
};

static u8 A14_NT36672C_CSOT_059[] = {
	0x24,
	0x10,
};

static u8 A14_NT36672C_CSOT_060[] = {
	0x25,
	0x0F,
};

static u8 A14_NT36672C_CSOT_061[] = {
	0x26,
	0x01,
};

static u8 A14_NT36672C_CSOT_062[] = {
	0x28,
	0x2C,
};

static u8 A14_NT36672C_CSOT_063[] = {
	0x29,
	0x2E,
};

static u8 A14_NT36672C_CSOT_064[] = {
	0x2A,
	0x30,
};

static u8 A14_NT36672C_CSOT_065[] = {
	0x2B,
	0x32,
};

static u8 A14_NT36672C_CSOT_066[] = {
	0x2F,
	0x0B,
};

static u8 A14_NT36672C_CSOT_067[] = {
	0x31,
	0x0C,
};

static u8 A14_NT36672C_CSOT_068[] = {
	0x32,
	0x09,
};

static u8 A14_NT36672C_CSOT_069[] = {
	0x33,
	0x03,
};

static u8 A14_NT36672C_CSOT_070[] = {
	0x34,
	0x03,
};

static u8 A14_NT36672C_CSOT_071[] = {
	0x35,
	0x07,
};

static u8 A14_NT36672C_CSOT_072[] = {
	0x36,
	0x3C,
};

static u8 A14_NT36672C_CSOT_073[] = {
	0x4E,
	0x37,
};

static u8 A14_NT36672C_CSOT_074[] = {
	0x4F,
	0x37,
};

static u8 A14_NT36672C_CSOT_075[] = {
	0x53,
	0x37,
};

static u8 A14_NT36672C_CSOT_076[] = {
	0x77,
	0x80,
};

static u8 A14_NT36672C_CSOT_077[] = {
	0x79,
	0x22,
};

static u8 A14_NT36672C_CSOT_078[] = {
	0x7A,
	0x03,
};

static u8 A14_NT36672C_CSOT_079[] = {
	0x7B,
	0x8E,
};

static u8 A14_NT36672C_CSOT_080[] = {
	0x7D,
	0x04,
};

static u8 A14_NT36672C_CSOT_081[] = {
	0x80,
	0x04,
};

static u8 A14_NT36672C_CSOT_082[] = {
	0x81,
	0x04,
};

static u8 A14_NT36672C_CSOT_083[] = {
	0x82,
	0x13,
};

static u8 A14_NT36672C_CSOT_084[] = {
	0x84,
	0x31,
};

static u8 A14_NT36672C_CSOT_085[] = {
	0x85,
	0x13,
};

static u8 A14_NT36672C_CSOT_086[] = {
	0x86,
	0x22,
};

static u8 A14_NT36672C_CSOT_087[] = {
	0x87,
	0x31,
};

static u8 A14_NT36672C_CSOT_088[] = {
	0x90,
	0x13,
};

static u8 A14_NT36672C_CSOT_089[] = {
	0x92,
	0x31,
};

static u8 A14_NT36672C_CSOT_090[] = {
	0x93,
	0x13,
};

static u8 A14_NT36672C_CSOT_091[] = {
	0x94,
	0x22,
};

static u8 A14_NT36672C_CSOT_092[] = {
	0x95,
	0x31,
};

static u8 A14_NT36672C_CSOT_093[] = {
	0x9C,
	0xF4,
};

static u8 A14_NT36672C_CSOT_094[] = {
	0x9D,
	0x01,
};

static u8 A14_NT36672C_CSOT_095[] = {
	0xA0,
	0x0E,
};

static u8 A14_NT36672C_CSOT_096[] = {
	0xA2,
	0x0E,
};

static u8 A14_NT36672C_CSOT_097[] = {
	0xA3,
	0x03,
};

static u8 A14_NT36672C_CSOT_098[] = {
	0xA4,
	0x04,
};

static u8 A14_NT36672C_CSOT_099[] = {
	0xA5,
	0x04,
};

static u8 A14_NT36672C_CSOT_100[] = {
	0xC4,
	0x80,
};

static u8 A14_NT36672C_CSOT_101[] = {
	0xC6,
	0xC0,
};

static u8 A14_NT36672C_CSOT_102[] = {
	0xC9,
	0x00,
};

static u8 A14_NT36672C_CSOT_103[] = {
	0xD9,
	0x80,
};

static u8 A14_NT36672C_CSOT_104[] = {
	0xE9,
	0x03,
};

static u8 A14_NT36672C_CSOT_105[] = {
	0xFF,
	0x25,
};

static u8 A14_NT36672C_CSOT_106[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_107[] = {
	0x0F,
	0x1B,
};

static u8 A14_NT36672C_CSOT_108[] = {
	0x18,
	0x22,
};

static u8 A14_NT36672C_CSOT_109[] = {
	0x19,
	0xE4,
};

static u8 A14_NT36672C_CSOT_110[] = {
	0x20,
	0x03,
};

static u8 A14_NT36672C_CSOT_111[] = {
	0x21,
	0x40,
};

static u8 A14_NT36672C_CSOT_112[] = {
	0x63,
	0x8F,
};

static u8 A14_NT36672C_CSOT_113[] = {
	0x66,
	0x5D,
};

static u8 A14_NT36672C_CSOT_114[] = {
	0x67,
	0x16,
};

static u8 A14_NT36672C_CSOT_115[] = {
	0x68,
	0x58,
};

static u8 A14_NT36672C_CSOT_116[] = {
	0x69,
	0x10,
};

static u8 A14_NT36672C_CSOT_117[] = {
	0x6B,
	0x00,
};

static u8 A14_NT36672C_CSOT_118[] = {
	0x70,
	0xE5,
};

static u8 A14_NT36672C_CSOT_119[] = {
	0x71,
	0x6D,
};

static u8 A14_NT36672C_CSOT_120[] = {
	0x77,
	0x62,
};

static u8 A14_NT36672C_CSOT_121[] = {
	0x7E,
	0x2D,
};

static u8 A14_NT36672C_CSOT_122[] = {
	0x84,
	0x78,
};

static u8 A14_NT36672C_CSOT_123[] = {
	0x85,
	0x75,
};

static u8 A14_NT36672C_CSOT_124[] = {
	0x8D,
	0x04,
};

static u8 A14_NT36672C_CSOT_125[] = {
	0xC1,
	0xA9,
};

static u8 A14_NT36672C_CSOT_126[] = {
	0xC2,
	0x5A,
};

static u8 A14_NT36672C_CSOT_127[] = {
	0xC3,
	0x07,
};

static u8 A14_NT36672C_CSOT_128[] = {
	0xC4,
	0x11,
};

static u8 A14_NT36672C_CSOT_129[] = {
	0xC6,
	0x11,
};

static u8 A14_NT36672C_CSOT_130[] = {
	0xEF,
	0x28,
};

static u8 A14_NT36672C_CSOT_131[] = {
	0xF0,
	0x05,
};

static u8 A14_NT36672C_CSOT_132[] = {
	0xF1,
	0x14,
};

static u8 A14_NT36672C_CSOT_133[] = {
	0xFF,
	0x26,
};

static u8 A14_NT36672C_CSOT_134[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_135[] = {
	0x00,
	0x11,
};

static u8 A14_NT36672C_CSOT_136[] = {
	0x01,
	0x28,
};

static u8 A14_NT36672C_CSOT_137[] = {
	0x03,
	0x01,
};

static u8 A14_NT36672C_CSOT_138[] = {
	0x04,
	0x28,
};

static u8 A14_NT36672C_CSOT_139[] = {
	0x05,
	0x08,
};

static u8 A14_NT36672C_CSOT_140[] = {
	0x06,
	0x13,
};

static u8 A14_NT36672C_CSOT_141[] = {
	0x08,
	0x13,
};

static u8 A14_NT36672C_CSOT_142[] = {
	0x14,
	0x06,
};

static u8 A14_NT36672C_CSOT_143[] = {
	0x15,
	0x01,
};

static u8 A14_NT36672C_CSOT_144[] = {
	0x74,
	0xAF,
};

static u8 A14_NT36672C_CSOT_145[] = {
	0x81,
	0x0E,
};

static u8 A14_NT36672C_CSOT_146[] = {
	0x83,
	0x03,
};

static u8 A14_NT36672C_CSOT_147[] = {
	0x84,
	0x03,
};

static u8 A14_NT36672C_CSOT_148[] = {
	0x85,
	0x01,
};

static u8 A14_NT36672C_CSOT_149[] = {
	0x86,
	0x03,
};

static u8 A14_NT36672C_CSOT_150[] = {
	0x87,
	0x01,
};

static u8 A14_NT36672C_CSOT_151[] = {
	0x88,
	0x07,
};

static u8 A14_NT36672C_CSOT_152[] = {
	0x8A,
	0x1A,
};

static u8 A14_NT36672C_CSOT_153[] = {
	0x8B,
	0x11,
};

static u8 A14_NT36672C_CSOT_154[] = {
	0x8C,
	0x24,
};

static u8 A14_NT36672C_CSOT_155[] = {
	0x8E,
	0x42,
};

static u8 A14_NT36672C_CSOT_156[] = {
	0x8F,
	0x11,
};

static u8 A14_NT36672C_CSOT_157[] = {
	0x90,
	0x11,
};

static u8 A14_NT36672C_CSOT_158[] = {
	0x91,
	0x11,
};

static u8 A14_NT36672C_CSOT_159[] = {
	0x9A,
	0x80,
};

static u8 A14_NT36672C_CSOT_160[] = {
	0x9B,
	0x04,
};

static u8 A14_NT36672C_CSOT_161[] = {
	0x9C,
	0x00,
};

static u8 A14_NT36672C_CSOT_162[] = {
	0x9D,
	0x00,
};

static u8 A14_NT36672C_CSOT_163[] = {
	0x9E,
	0x00,
};

static u8 A14_NT36672C_CSOT_164[] = {
	0xFF,
	0x27,
};

static u8 A14_NT36672C_CSOT_165[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_166[] = {
	0x01,
	0x68,
};

static u8 A14_NT36672C_CSOT_167[] = {
	0x20,
	0x81,
};

static u8 A14_NT36672C_CSOT_168[] = {
	0x21,
	0x76,
};

static u8 A14_NT36672C_CSOT_169[] = {
	0x25,
	0x81,
};

static u8 A14_NT36672C_CSOT_170[] = {
	0x26,
	0x9F,
};

static u8 A14_NT36672C_CSOT_171[] = {
	0x6E,
	0x12,
};

static u8 A14_NT36672C_CSOT_172[] = {
	0x6F,
	0x00,
};

static u8 A14_NT36672C_CSOT_173[] = {
	0x70,
	0x00,
};

static u8 A14_NT36672C_CSOT_174[] = {
	0x71,
	0x00,
};

static u8 A14_NT36672C_CSOT_175[] = {
	0x72,
	0x00,
};

static u8 A14_NT36672C_CSOT_176[] = {
	0x73,
	0x76,
};

static u8 A14_NT36672C_CSOT_177[] = {
	0x74,
	0x10,
};

static u8 A14_NT36672C_CSOT_178[] = {
	0x75,
	0x32,
};

static u8 A14_NT36672C_CSOT_179[] = {
	0x76,
	0x54,
};

static u8 A14_NT36672C_CSOT_180[] = {
	0x77,
	0x00,
};

static u8 A14_NT36672C_CSOT_181[] = {
	0x7D,
	0x09,
};

static u8 A14_NT36672C_CSOT_182[] = {
	0x7E,
	0x6B,
};

static u8 A14_NT36672C_CSOT_183[] = {
	0x80,
	0x27,
};

static u8 A14_NT36672C_CSOT_184[] = {
	0x82,
	0x09,
};

static u8 A14_NT36672C_CSOT_185[] = {
	0x83,
	0x6B,
};

static u8 A14_NT36672C_CSOT_186[] = {
	0x88,
	0x03,
};

static u8 A14_NT36672C_CSOT_187[] = {
	0x89,
	0x03,
};

static u8 A14_NT36672C_CSOT_188[] = {
	0xE3,
	0x01,
};

static u8 A14_NT36672C_CSOT_189[] = {
	0xE4,
	0xF3,
};

static u8 A14_NT36672C_CSOT_190[] = {
	0xE5,
	0x02,
};

static u8 A14_NT36672C_CSOT_191[] = {
	0xE6,
	0xED,
};

static u8 A14_NT36672C_CSOT_192[] = {
	0xE9,
	0x02,
};

static u8 A14_NT36672C_CSOT_193[] = {
	0xEA,
	0x29,
};

static u8 A14_NT36672C_CSOT_194[] = {
	0xEB,
	0x03,
};

static u8 A14_NT36672C_CSOT_195[] = {
	0xEC,
	0x3E,
};

static u8 A14_NT36672C_CSOT_196[] = {
	0xFF,
	0x2A,
};

static u8 A14_NT36672C_CSOT_197[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_198[] = {
	0x00,
	0x91,
};

static u8 A14_NT36672C_CSOT_199[] = {
	0x03,
	0x20,
};

static u8 A14_NT36672C_CSOT_200[] = {
	0x06,
	0x06,
};

static u8 A14_NT36672C_CSOT_201[] = {
	0x07,
	0x50,
};

static u8 A14_NT36672C_CSOT_202[] = {
	0x0A,
	0x60,
};

static u8 A14_NT36672C_CSOT_203[] = {
	0x0C,
	0x04,
};

static u8 A14_NT36672C_CSOT_204[] = {
	0x0D,
	0x40,
};

static u8 A14_NT36672C_CSOT_205[] = {
	0x0F,
	0x01,
};

static u8 A14_NT36672C_CSOT_206[] = {
	0x11,
	0xE1,
};

static u8 A14_NT36672C_CSOT_207[] = {
	0x15,
	0x12,
};

static u8 A14_NT36672C_CSOT_208[] = {
	0x16,
	0x8D,
};

static u8 A14_NT36672C_CSOT_209[] = {
	0x19,
	0x12,
};

static u8 A14_NT36672C_CSOT_210[] = {
	0x1A,
	0x61,
};

static u8 A14_NT36672C_CSOT_211[] = {
	0x1B,
	0x0C,
};

static u8 A14_NT36672C_CSOT_212[] = {
	0x1D,
	0x0A,
};

static u8 A14_NT36672C_CSOT_213[] = {
	0x1E,
	0x3F,
};

static u8 A14_NT36672C_CSOT_214[] = {
	0x1F,
	0x48,
};

static u8 A14_NT36672C_CSOT_215[] = {
	0x20,
	0x3F,
};

static u8 A14_NT36672C_CSOT_216[] = {
	0x28,
	0xFD,
};

static u8 A14_NT36672C_CSOT_217[] = {
	0x29,
	0x0B,
};

static u8 A14_NT36672C_CSOT_218[] = {
	0x2A,
	0x1B,
};

static u8 A14_NT36672C_CSOT_219[] = {
	0x2D,
	0x0B,
};

static u8 A14_NT36672C_CSOT_220[] = {
	0x2F,
	0x01,
};

static u8 A14_NT36672C_CSOT_221[] = {
	0x30,
	0x85,
};

static u8 A14_NT36672C_CSOT_222[] = {
	0x31,
	0xB4,
};

static u8 A14_NT36672C_CSOT_223[] = {
	0x33,
	0x22,
};

static u8 A14_NT36672C_CSOT_224[] = {
	0x34,
	0xFF,
};

static u8 A14_NT36672C_CSOT_225[] = {
	0x35,
	0x3F,
};

static u8 A14_NT36672C_CSOT_226[] = {
	0x36,
	0x05,
};

static u8 A14_NT36672C_CSOT_227[] = {
	0x37,
	0xF9,
};

static u8 A14_NT36672C_CSOT_228[] = {
	0x38,
	0x44,
};

static u8 A14_NT36672C_CSOT_229[] = {
	0x39,
	0x00,
};

static u8 A14_NT36672C_CSOT_230[] = {
	0x3A,
	0x85,
};

static u8 A14_NT36672C_CSOT_231[] = {
	0x45,
	0x04,
};

static u8 A14_NT36672C_CSOT_232[] = {
	0x46,
	0x40,
};

static u8 A14_NT36672C_CSOT_233[] = {
	0x48,
	0x01,
};

static u8 A14_NT36672C_CSOT_234[] = {
	0x4A,
	0xE1,
};

static u8 A14_NT36672C_CSOT_235[] = {
	0x4E,
	0x12,
};

static u8 A14_NT36672C_CSOT_236[] = {
	0x4F,
	0x8D,
};

static u8 A14_NT36672C_CSOT_237[] = {
	0x52,
	0x12,
};

static u8 A14_NT36672C_CSOT_238[] = {
	0x53,
	0x61,
};

static u8 A14_NT36672C_CSOT_239[] = {
	0x54,
	0x0C,
};

static u8 A14_NT36672C_CSOT_240[] = {
	0x56,
	0x0A,
};

static u8 A14_NT36672C_CSOT_241[] = {
	0x57,
	0x57,
};

static u8 A14_NT36672C_CSOT_242[] = {
	0x58,
	0x61,
};

static u8 A14_NT36672C_CSOT_243[] = {
	0x59,
	0x57,
};

static u8 A14_NT36672C_CSOT_244[] = {
	0x7A,
	0x09,
};

static u8 A14_NT36672C_CSOT_245[] = {
	0x7B,
	0x40,
};

static u8 A14_NT36672C_CSOT_246[] = {
	0x7F,
	0xF0,
};

static u8 A14_NT36672C_CSOT_247[] = {
	0x83,
	0x12,
};

static u8 A14_NT36672C_CSOT_248[] = {
	0x84,
	0xBD,
};

static u8 A14_NT36672C_CSOT_249[] = {
	0x87,
	0x12,
};

static u8 A14_NT36672C_CSOT_250[] = {
	0x88,
	0x61,
};

static u8 A14_NT36672C_CSOT_251[] = {
	0x89,
	0x0C,
};

static u8 A14_NT36672C_CSOT_252[] = {
	0x8B,
	0x0A,
};

static u8 A14_NT36672C_CSOT_253[] = {
	0x8C,
	0x7E,
};

static u8 A14_NT36672C_CSOT_254[] = {
	0x8D,
	0x7E,
};

static u8 A14_NT36672C_CSOT_255[] = {
	0x8E,
	0x7E,
};

static u8 A14_NT36672C_CSOT_256[] = {
	0xFF,
	0x2C,
};

static u8 A14_NT36672C_CSOT_257[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_258[] = {
	0x03,
	0x15,
};

static u8 A14_NT36672C_CSOT_259[] = {
	0x04,
	0x15,
};

static u8 A14_NT36672C_CSOT_260[] = {
	0x05,
	0x15,
};

static u8 A14_NT36672C_CSOT_261[] = {
	0x0D,
	0x06,
};

static u8 A14_NT36672C_CSOT_262[] = {
	0x0E,
	0x56,
};

static u8 A14_NT36672C_CSOT_263[] = {
	0x17,
	0x4E,
};

static u8 A14_NT36672C_CSOT_264[] = {
	0x18,
	0x4E,
};

static u8 A14_NT36672C_CSOT_265[] = {
	0x19,
	0x4E,
};

static u8 A14_NT36672C_CSOT_266[] = {
	0x2D,
	0xAF,
};

static u8 A14_NT36672C_CSOT_267[] = {
	0x2F,
	0x11,
};

static u8 A14_NT36672C_CSOT_268[] = {
	0x30,
	0x28,
};

static u8 A14_NT36672C_CSOT_269[] = {
	0x32,
	0x01,
};

static u8 A14_NT36672C_CSOT_270[] = {
	0x33,
	0x28,
};

static u8 A14_NT36672C_CSOT_271[] = {
	0x35,
	0x19,
};

static u8 A14_NT36672C_CSOT_272[] = {
	0x37,
	0x19,
};

static u8 A14_NT36672C_CSOT_273[] = {
	0x4D,
	0x15,
};

static u8 A14_NT36672C_CSOT_274[] = {
	0x4E,
	0x04,
};

static u8 A14_NT36672C_CSOT_275[] = {
	0x4F,
	0x09,
};

static u8 A14_NT36672C_CSOT_276[] = {
	0x56,
	0x1B,
};

static u8 A14_NT36672C_CSOT_277[] = {
	0x58,
	0x1B,
};

static u8 A14_NT36672C_CSOT_278[] = {
	0x59,
	0x1B,
};

static u8 A14_NT36672C_CSOT_279[] = {
	0x62,
	0x6D,
};

static u8 A14_NT36672C_CSOT_280[] = {
	0x6B,
	0x6A,
};

static u8 A14_NT36672C_CSOT_281[] = {
	0x6C,
	0x6A,
};

static u8 A14_NT36672C_CSOT_282[] = {
	0x6D,
	0x6A,
};

static u8 A14_NT36672C_CSOT_283[] = {
	0x80,
	0xAF,
};

static u8 A14_NT36672C_CSOT_284[] = {
	0x81,
	0x11,
};

static u8 A14_NT36672C_CSOT_285[] = {
	0x82,
	0x29,
};

static u8 A14_NT36672C_CSOT_286[] = {
	0x84,
	0x01,
};

static u8 A14_NT36672C_CSOT_287[] = {
	0x85,
	0x29,
};

static u8 A14_NT36672C_CSOT_288[] = {
	0x87,
	0x20,
};

static u8 A14_NT36672C_CSOT_289[] = {
	0x89,
	0x20,
};

static u8 A14_NT36672C_CSOT_290[] = {
	0x9E,
	0x04,
};

static u8 A14_NT36672C_CSOT_291[] = {
	0x9F,
	0x1E,
};

static u8 A14_NT36672C_CSOT_292[] = {
	0xFF,
	0xE0,
};

static u8 A14_NT36672C_CSOT_293[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_294[] = {
	0x35,
	0x82,
};

static u8 A14_NT36672C_CSOT_295[] = {
	0xFF,
	0xF0,
};

static u8 A14_NT36672C_CSOT_296[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_297[] = {
	0x1C,
	0x01,
};

static u8 A14_NT36672C_CSOT_298[] = {
	0x33,
	0x01,
};

static u8 A14_NT36672C_CSOT_299[] = {
	0x5A,
	0x00,
};

static u8 A14_NT36672C_CSOT_300[] = {
	0xFF,
	0xD0,
};

static u8 A14_NT36672C_CSOT_301[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_302[] = {
	0x53,
	0x22,
};

static u8 A14_NT36672C_CSOT_303[] = {
	0x54,
	0x02,
};

static u8 A14_NT36672C_CSOT_304[] = {
	0xFF,
	0xC0,
};

static u8 A14_NT36672C_CSOT_305[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_306[] = {
	0x9C,
	0x11,
};

static u8 A14_NT36672C_CSOT_307[] = {
	0x9D,
	0x11,
};

static u8 A14_NT36672C_CSOT_308[] = {
	0xFF,
	0x2B,
};

static u8 A14_NT36672C_CSOT_309[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_310[] = {
	0xB7,
	0x1A,
};

static u8 A14_NT36672C_CSOT_311[] = {
	0xB8,
	0x15,
};

static u8 A14_NT36672C_CSOT_312[] = {
	0xC0,
	0x03,
};

static u8 A14_NT36672C_CSOT_313[] = {
	0xFF,
};

static u8 A14_NT36672C_CSOT_314[] = {
	0xFF,
	0xF0,
};

static u8 A14_NT36672C_CSOT_315[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_316[] = {
	0xD2,
	0x50,
};

static u8 A14_NT36672C_CSOT_317[] = {
	0xFF,
	0x23,
};

static u8 A14_NT36672C_CSOT_318[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_319[] = {
	0x00,
	0x00,
};

static u8 A14_NT36672C_CSOT_320[] = {
	0x07,
	0x60,
};

static u8 A14_NT36672C_CSOT_321[] = {
	0x08,
	0x06,
};

static u8 A14_NT36672C_CSOT_322[] = {
	0x09,
	0x1C,
};

static u8 A14_NT36672C_CSOT_323[] = {
	0x0A,
	0x2B,
};

static u8 A14_NT36672C_CSOT_324[] = {
	0x0B,
	0x2B,
};

static u8 A14_NT36672C_CSOT_325[] = {
	0x0C,
	0x2B,
};

static u8 A14_NT36672C_CSOT_326[] = {
	0x0D,
	0x00,
};

static u8 A14_NT36672C_CSOT_327[] = {
	0x10,
	0x50,
};

static u8 A14_NT36672C_CSOT_328[] = {
	0x11,
	0x01,
};

static u8 A14_NT36672C_CSOT_329[] = {
	0x12,
	0x95,
};

static u8 A14_NT36672C_CSOT_330[] = {
	0x15,
	0x68,
};

static u8 A14_NT36672C_CSOT_331[] = {
	0x16,
	0x0B,
};

static u8 A14_NT36672C_CSOT_332[] = {
	0x19,
	0x00,
};

static u8 A14_NT36672C_CSOT_333[] = {
	0x1A,
	0x00,
};

static u8 A14_NT36672C_CSOT_334[] = {
	0x1B,
	0x00,
};

static u8 A14_NT36672C_CSOT_335[] = {
	0x1C,
	0x00,
};

static u8 A14_NT36672C_CSOT_336[] = {
	0x1D,
	0x01,
};

static u8 A14_NT36672C_CSOT_337[] = {
	0x1E,
	0x03,
};

static u8 A14_NT36672C_CSOT_338[] = {
	0x1F,
	0x05,
};

static u8 A14_NT36672C_CSOT_339[] = {
	0x20,
	0x0C,
};

static u8 A14_NT36672C_CSOT_340[] = {
	0x21,
	0x13,
};

static u8 A14_NT36672C_CSOT_341[] = {
	0x22,
	0x17,
};

static u8 A14_NT36672C_CSOT_342[] = {
	0x23,
	0x1D,
};

static u8 A14_NT36672C_CSOT_343[] = {
	0x24,
	0x23,
};

static u8 A14_NT36672C_CSOT_344[] = {
	0x25,
	0x2C,
};

static u8 A14_NT36672C_CSOT_345[] = {
	0x26,
	0x33,
};

static u8 A14_NT36672C_CSOT_346[] = {
	0x27,
	0x39,
};

static u8 A14_NT36672C_CSOT_347[] = {
	0x28,
	0x3F,
};

static u8 A14_NT36672C_CSOT_348[] = {
	0x29,
	0x3F,
};

static u8 A14_NT36672C_CSOT_349[] = {
	0x2A,
	0x3F,
};

static u8 A14_NT36672C_CSOT_350[] = {
	0x2B,
	0x3F,
};

static u8 A14_NT36672C_CSOT_351[] = {
	0x30,
	0xFF,
};

static u8 A14_NT36672C_CSOT_352[] = {
	0x31,
	0xFE,
};

static u8 A14_NT36672C_CSOT_353[] = {
	0x32,
	0xFD,
};

static u8 A14_NT36672C_CSOT_354[] = {
	0x33,
	0xFC,
};

static u8 A14_NT36672C_CSOT_355[] = {
	0x34,
	0xFB,
};

static u8 A14_NT36672C_CSOT_356[] = {
	0x35,
	0xFA,
};

static u8 A14_NT36672C_CSOT_357[] = {
	0x36,
	0xF9,
};

static u8 A14_NT36672C_CSOT_358[] = {
	0x37,
	0xF7,
};

static u8 A14_NT36672C_CSOT_359[] = {
	0x38,
	0xF5,
};

static u8 A14_NT36672C_CSOT_360[] = {
	0x39,
	0xF3,
};

static u8 A14_NT36672C_CSOT_361[] = {
	0x3A,
	0xF1,
};

static u8 A14_NT36672C_CSOT_362[] = {
	0x3B,
	0xEE,
};

static u8 A14_NT36672C_CSOT_363[] = {
	0x3D,
	0xEC,
};

static u8 A14_NT36672C_CSOT_364[] = {
	0x3F,
	0xEA,
};

static u8 A14_NT36672C_CSOT_365[] = {
	0x40,
	0xE8,
};

static u8 A14_NT36672C_CSOT_366[] = {
	0x41,
	0xE6,
};

static u8 A14_NT36672C_CSOT_367[] = {
	0x04,
	0x00,
};

static u8 A14_NT36672C_CSOT_368[] = {
	0xA0,
	0x11,
};

static u8 A14_NT36672C_CSOT_369[] = {
	0xFF,
	0xD0,
};

static u8 A14_NT36672C_CSOT_370[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_371[] = {
	0x25,
	0xA9,
};

static u8 A14_NT36672C_CSOT_372[] = {
	0xFF,
	0x10,
};

static u8 A14_NT36672C_CSOT_373[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_CSOT_374[] = {
	0x53,
	0x2C,
};

static u8 A14_NT36672C_CSOT_375[] = {
	0x55,
	0x01,
};

static u8 A14_NT36672C_CSOT_376[] = {
	0x68,
	0x00, 0x01,
};


static DEFINE_STATIC_PACKET(a14_sleep_out, DSI_PKT_TYPE_WR, A14_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a14_sleep_in, DSI_PKT_TYPE_WR, A14_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a14_display_on, DSI_PKT_TYPE_WR, A14_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a14_display_off, DSI_PKT_TYPE_WR, A14_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(a14_brightness_mode, DSI_PKT_TYPE_WR, A14_BRIGHTNESS_MODE, 0);

static DEFINE_PKTUI(a14_brightness, &a14_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a14_brightness, DSI_PKT_TYPE_WR, A14_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(a14_nt36672c_csot_001, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_001, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_002, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_002, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_003, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_003, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_004, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_004, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_005, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_005, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_006, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_006, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_007, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_007, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_008, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_008, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_009, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_009, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_010, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_010, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_011, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_011, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_012, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_012, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_013, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_013, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_014, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_014, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_015, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_015, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_016, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_016, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_017, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_017, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_018, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_018, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_019, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_019, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_020, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_020, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_021, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_021, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_022, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_022, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_023, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_023, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_024, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_024, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_025, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_025, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_026, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_026, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_027, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_027, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_028, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_028, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_029, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_029, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_030, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_030, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_031, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_031, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_032, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_032, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_033, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_033, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_034, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_034, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_035, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_035, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_036, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_036, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_037, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_037, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_038, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_038, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_039, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_039, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_040, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_040, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_041, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_041, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_042, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_042, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_043, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_043, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_044, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_044, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_045, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_045, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_046, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_046, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_047, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_047, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_048, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_048, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_049, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_049, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_050, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_050, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_051, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_051, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_052, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_052, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_053, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_053, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_054, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_054, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_055, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_055, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_056, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_056, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_057, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_057, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_058, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_058, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_059, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_059, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_060, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_060, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_061, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_061, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_062, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_062, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_063, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_063, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_064, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_064, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_065, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_065, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_066, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_066, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_067, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_067, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_068, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_068, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_069, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_069, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_070, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_070, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_071, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_071, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_072, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_072, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_073, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_073, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_074, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_074, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_075, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_075, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_076, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_076, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_077, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_077, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_078, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_078, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_079, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_079, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_080, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_080, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_081, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_081, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_082, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_082, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_083, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_083, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_084, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_084, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_085, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_085, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_086, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_086, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_087, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_087, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_088, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_088, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_089, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_089, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_090, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_090, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_091, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_091, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_092, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_092, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_093, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_093, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_094, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_094, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_095, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_095, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_096, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_096, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_097, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_097, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_098, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_098, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_099, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_099, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_100, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_100, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_101, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_101, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_102, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_102, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_103, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_103, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_104, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_104, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_105, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_105, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_106, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_106, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_107, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_107, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_108, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_108, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_109, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_109, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_110, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_110, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_111, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_111, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_112, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_112, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_113, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_113, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_114, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_114, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_115, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_115, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_116, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_116, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_117, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_117, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_118, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_118, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_119, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_119, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_120, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_120, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_121, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_121, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_122, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_122, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_123, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_123, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_124, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_124, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_125, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_125, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_126, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_126, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_127, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_127, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_128, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_128, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_129, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_129, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_130, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_130, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_131, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_131, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_132, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_132, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_133, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_133, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_134, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_134, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_135, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_135, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_136, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_136, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_137, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_137, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_138, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_138, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_139, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_139, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_140, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_140, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_141, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_141, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_142, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_142, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_143, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_143, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_144, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_144, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_145, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_145, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_146, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_146, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_147, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_147, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_148, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_148, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_149, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_149, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_150, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_150, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_151, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_151, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_152, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_152, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_153, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_153, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_154, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_154, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_155, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_155, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_156, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_156, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_157, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_157, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_158, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_158, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_159, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_159, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_160, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_160, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_161, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_161, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_162, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_162, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_163, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_163, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_164, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_164, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_165, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_165, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_166, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_166, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_167, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_167, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_168, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_168, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_169, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_169, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_170, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_170, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_171, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_171, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_172, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_172, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_173, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_173, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_174, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_174, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_175, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_175, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_176, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_176, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_177, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_177, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_178, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_178, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_179, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_179, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_180, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_180, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_181, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_181, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_182, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_182, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_183, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_183, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_184, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_184, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_185, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_185, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_186, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_186, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_187, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_187, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_188, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_188, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_189, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_189, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_190, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_190, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_191, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_191, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_192, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_192, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_193, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_193, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_194, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_194, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_195, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_195, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_196, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_196, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_197, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_197, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_198, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_198, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_199, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_199, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_200, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_200, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_201, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_201, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_202, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_202, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_203, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_203, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_204, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_204, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_205, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_205, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_206, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_206, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_207, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_207, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_208, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_208, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_209, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_209, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_210, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_210, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_211, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_211, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_212, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_212, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_213, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_213, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_214, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_214, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_215, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_215, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_216, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_216, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_217, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_217, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_218, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_218, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_219, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_219, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_220, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_220, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_221, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_221, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_222, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_222, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_223, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_223, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_224, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_224, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_225, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_225, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_226, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_226, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_227, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_227, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_228, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_228, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_229, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_229, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_230, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_230, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_231, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_231, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_232, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_232, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_233, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_233, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_234, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_234, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_235, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_235, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_236, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_236, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_237, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_237, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_238, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_238, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_239, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_239, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_240, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_240, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_241, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_241, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_242, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_242, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_243, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_243, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_244, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_244, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_245, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_245, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_246, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_246, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_247, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_247, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_248, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_248, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_249, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_249, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_250, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_250, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_251, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_251, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_252, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_252, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_253, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_253, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_254, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_254, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_255, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_255, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_256, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_256, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_257, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_257, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_258, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_258, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_259, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_259, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_260, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_260, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_261, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_261, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_262, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_262, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_263, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_263, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_264, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_264, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_265, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_265, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_266, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_266, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_267, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_267, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_268, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_268, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_269, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_269, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_270, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_270, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_271, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_271, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_272, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_272, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_273, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_273, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_274, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_274, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_275, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_275, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_276, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_276, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_277, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_277, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_278, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_278, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_279, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_279, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_280, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_280, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_281, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_281, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_282, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_282, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_283, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_283, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_284, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_284, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_285, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_285, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_286, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_286, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_287, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_287, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_288, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_288, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_289, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_289, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_290, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_290, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_291, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_291, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_292, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_292, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_293, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_293, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_294, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_294, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_295, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_295, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_296, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_296, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_297, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_297, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_298, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_298, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_299, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_299, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_300, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_300, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_301, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_301, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_302, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_302, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_303, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_303, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_304, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_304, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_305, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_305, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_306, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_306, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_307, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_307, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_308, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_308, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_309, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_309, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_310, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_310, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_311, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_311, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_312, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_312, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_313, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_313, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_314, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_314, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_315, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_315, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_316, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_316, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_317, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_317, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_318, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_318, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_319, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_319, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_320, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_320, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_321, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_321, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_322, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_322, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_323, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_323, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_324, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_324, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_325, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_325, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_326, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_326, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_327, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_327, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_328, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_328, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_329, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_329, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_330, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_330, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_331, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_331, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_332, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_332, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_333, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_333, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_334, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_334, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_335, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_335, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_336, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_336, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_337, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_337, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_338, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_338, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_339, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_339, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_340, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_340, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_341, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_341, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_342, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_342, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_343, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_343, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_344, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_344, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_345, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_345, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_346, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_346, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_347, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_347, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_348, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_348, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_349, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_349, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_350, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_350, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_351, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_351, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_352, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_352, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_353, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_353, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_354, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_354, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_355, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_355, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_356, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_356, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_357, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_357, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_358, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_358, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_359, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_359, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_360, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_360, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_361, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_361, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_362, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_362, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_363, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_363, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_364, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_364, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_365, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_365, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_366, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_366, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_367, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_367, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_368, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_368, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_369, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_369, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_370, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_370, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_371, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_371, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_372, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_372, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_373, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_373, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_374, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_374, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_375, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_375, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_csot_376, DSI_PKT_TYPE_WR, A14_NT36672C_CSOT_376, 0);

static DEFINE_PANEL_MDELAY(a14_wait_display_off, 1 * 17); /* 1 frame */
static DEFINE_PANEL_MDELAY(a14_wait_sleep_in, 4 * 17); /* 4 frame */
static DEFINE_PANEL_MDELAY(a14_wait_2msec, 2);
static DEFINE_PANEL_MDELAY(a14_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(a14_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(a14_wait_blic_off, 1);


static void *a14_init_cmdtbl[] = {
	&nt36672c_csot_restbl[RES_ID],
	&PKTINFO(a14_nt36672c_csot_001),
	&PKTINFO(a14_nt36672c_csot_002),
	&PKTINFO(a14_nt36672c_csot_003),
	&PKTINFO(a14_nt36672c_csot_004),
	&PKTINFO(a14_nt36672c_csot_005),
	&PKTINFO(a14_nt36672c_csot_006),
	&PKTINFO(a14_nt36672c_csot_007),
	&PKTINFO(a14_nt36672c_csot_008),
	&PKTINFO(a14_nt36672c_csot_009),
	&PKTINFO(a14_nt36672c_csot_010),
	&PKTINFO(a14_nt36672c_csot_011),
	&PKTINFO(a14_nt36672c_csot_012),
	&PKTINFO(a14_nt36672c_csot_013),
	&PKTINFO(a14_nt36672c_csot_014),
	&PKTINFO(a14_nt36672c_csot_015),
	&PKTINFO(a14_nt36672c_csot_016),
	&PKTINFO(a14_nt36672c_csot_017),
	&PKTINFO(a14_nt36672c_csot_018),
	&PKTINFO(a14_nt36672c_csot_019),
	&PKTINFO(a14_nt36672c_csot_020),
	&PKTINFO(a14_nt36672c_csot_021),
	&PKTINFO(a14_nt36672c_csot_022),
	&PKTINFO(a14_nt36672c_csot_023),
	&PKTINFO(a14_nt36672c_csot_024),
	&PKTINFO(a14_nt36672c_csot_025),
	&PKTINFO(a14_nt36672c_csot_026),
	&PKTINFO(a14_nt36672c_csot_027),
	&PKTINFO(a14_nt36672c_csot_028),
	&PKTINFO(a14_nt36672c_csot_029),
	&PKTINFO(a14_nt36672c_csot_030),
	&PKTINFO(a14_nt36672c_csot_031),
	&PKTINFO(a14_nt36672c_csot_032),
	&PKTINFO(a14_nt36672c_csot_033),
	&PKTINFO(a14_nt36672c_csot_034),
	&PKTINFO(a14_nt36672c_csot_035),
	&PKTINFO(a14_nt36672c_csot_036),
	&PKTINFO(a14_nt36672c_csot_037),
	&PKTINFO(a14_nt36672c_csot_038),
	&PKTINFO(a14_nt36672c_csot_039),
	&PKTINFO(a14_nt36672c_csot_040),
	&PKTINFO(a14_nt36672c_csot_041),
	&PKTINFO(a14_nt36672c_csot_042),
	&PKTINFO(a14_nt36672c_csot_043),
	&PKTINFO(a14_nt36672c_csot_044),
	&PKTINFO(a14_nt36672c_csot_045),
	&PKTINFO(a14_nt36672c_csot_046),
	&PKTINFO(a14_nt36672c_csot_047),
	&PKTINFO(a14_nt36672c_csot_048),
	&PKTINFO(a14_nt36672c_csot_049),
	&PKTINFO(a14_nt36672c_csot_050),
	&PKTINFO(a14_nt36672c_csot_051),
	&PKTINFO(a14_nt36672c_csot_052),
	&PKTINFO(a14_nt36672c_csot_053),
	&PKTINFO(a14_nt36672c_csot_054),
	&PKTINFO(a14_nt36672c_csot_055),
	&PKTINFO(a14_nt36672c_csot_056),
	&PKTINFO(a14_nt36672c_csot_057),
	&PKTINFO(a14_nt36672c_csot_058),
	&PKTINFO(a14_nt36672c_csot_059),
	&PKTINFO(a14_nt36672c_csot_060),
	&PKTINFO(a14_nt36672c_csot_061),
	&PKTINFO(a14_nt36672c_csot_062),
	&PKTINFO(a14_nt36672c_csot_063),
	&PKTINFO(a14_nt36672c_csot_064),
	&PKTINFO(a14_nt36672c_csot_065),
	&PKTINFO(a14_nt36672c_csot_066),
	&PKTINFO(a14_nt36672c_csot_067),
	&PKTINFO(a14_nt36672c_csot_068),
	&PKTINFO(a14_nt36672c_csot_069),
	&PKTINFO(a14_nt36672c_csot_070),
	&PKTINFO(a14_nt36672c_csot_071),
	&PKTINFO(a14_nt36672c_csot_072),
	&PKTINFO(a14_nt36672c_csot_073),
	&PKTINFO(a14_nt36672c_csot_074),
	&PKTINFO(a14_nt36672c_csot_075),
	&PKTINFO(a14_nt36672c_csot_076),
	&PKTINFO(a14_nt36672c_csot_077),
	&PKTINFO(a14_nt36672c_csot_078),
	&PKTINFO(a14_nt36672c_csot_079),
	&PKTINFO(a14_nt36672c_csot_080),
	&PKTINFO(a14_nt36672c_csot_081),
	&PKTINFO(a14_nt36672c_csot_082),
	&PKTINFO(a14_nt36672c_csot_083),
	&PKTINFO(a14_nt36672c_csot_084),
	&PKTINFO(a14_nt36672c_csot_085),
	&PKTINFO(a14_nt36672c_csot_086),
	&PKTINFO(a14_nt36672c_csot_087),
	&PKTINFO(a14_nt36672c_csot_088),
	&PKTINFO(a14_nt36672c_csot_089),
	&PKTINFO(a14_nt36672c_csot_090),
	&PKTINFO(a14_nt36672c_csot_091),
	&PKTINFO(a14_nt36672c_csot_092),
	&PKTINFO(a14_nt36672c_csot_093),
	&PKTINFO(a14_nt36672c_csot_094),
	&PKTINFO(a14_nt36672c_csot_095),
	&PKTINFO(a14_nt36672c_csot_096),
	&PKTINFO(a14_nt36672c_csot_097),
	&PKTINFO(a14_nt36672c_csot_098),
	&PKTINFO(a14_nt36672c_csot_099),
	&PKTINFO(a14_nt36672c_csot_100),
	&PKTINFO(a14_nt36672c_csot_101),
	&PKTINFO(a14_nt36672c_csot_102),
	&PKTINFO(a14_nt36672c_csot_103),
	&PKTINFO(a14_nt36672c_csot_104),
	&PKTINFO(a14_nt36672c_csot_105),
	&PKTINFO(a14_nt36672c_csot_106),
	&PKTINFO(a14_nt36672c_csot_107),
	&PKTINFO(a14_nt36672c_csot_108),
	&PKTINFO(a14_nt36672c_csot_109),
	&PKTINFO(a14_nt36672c_csot_110),
	&PKTINFO(a14_nt36672c_csot_111),
	&PKTINFO(a14_nt36672c_csot_112),
	&PKTINFO(a14_nt36672c_csot_113),
	&PKTINFO(a14_nt36672c_csot_114),
	&PKTINFO(a14_nt36672c_csot_115),
	&PKTINFO(a14_nt36672c_csot_116),
	&PKTINFO(a14_nt36672c_csot_117),
	&PKTINFO(a14_nt36672c_csot_118),
	&PKTINFO(a14_nt36672c_csot_119),
	&PKTINFO(a14_nt36672c_csot_120),
	&PKTINFO(a14_nt36672c_csot_121),
	&PKTINFO(a14_nt36672c_csot_122),
	&PKTINFO(a14_nt36672c_csot_123),
	&PKTINFO(a14_nt36672c_csot_124),
	&PKTINFO(a14_nt36672c_csot_125),
	&PKTINFO(a14_nt36672c_csot_126),
	&PKTINFO(a14_nt36672c_csot_127),
	&PKTINFO(a14_nt36672c_csot_128),
	&PKTINFO(a14_nt36672c_csot_129),
	&PKTINFO(a14_nt36672c_csot_130),
	&PKTINFO(a14_nt36672c_csot_131),
	&PKTINFO(a14_nt36672c_csot_132),
	&PKTINFO(a14_nt36672c_csot_133),
	&PKTINFO(a14_nt36672c_csot_134),
	&PKTINFO(a14_nt36672c_csot_135),
	&PKTINFO(a14_nt36672c_csot_136),
	&PKTINFO(a14_nt36672c_csot_137),
	&PKTINFO(a14_nt36672c_csot_138),
	&PKTINFO(a14_nt36672c_csot_139),
	&PKTINFO(a14_nt36672c_csot_140),
	&PKTINFO(a14_nt36672c_csot_141),
	&PKTINFO(a14_nt36672c_csot_142),
	&PKTINFO(a14_nt36672c_csot_143),
	&PKTINFO(a14_nt36672c_csot_144),
	&PKTINFO(a14_nt36672c_csot_145),
	&PKTINFO(a14_nt36672c_csot_146),
	&PKTINFO(a14_nt36672c_csot_147),
	&PKTINFO(a14_nt36672c_csot_148),
	&PKTINFO(a14_nt36672c_csot_149),
	&PKTINFO(a14_nt36672c_csot_150),
	&PKTINFO(a14_nt36672c_csot_151),
	&PKTINFO(a14_nt36672c_csot_152),
	&PKTINFO(a14_nt36672c_csot_153),
	&PKTINFO(a14_nt36672c_csot_154),
	&PKTINFO(a14_nt36672c_csot_155),
	&PKTINFO(a14_nt36672c_csot_156),
	&PKTINFO(a14_nt36672c_csot_157),
	&PKTINFO(a14_nt36672c_csot_158),
	&PKTINFO(a14_nt36672c_csot_159),
	&PKTINFO(a14_nt36672c_csot_160),
	&PKTINFO(a14_nt36672c_csot_161),
	&PKTINFO(a14_nt36672c_csot_162),
	&PKTINFO(a14_nt36672c_csot_163),
	&PKTINFO(a14_nt36672c_csot_164),
	&PKTINFO(a14_nt36672c_csot_165),
	&PKTINFO(a14_nt36672c_csot_166),
	&PKTINFO(a14_nt36672c_csot_167),
	&PKTINFO(a14_nt36672c_csot_168),
	&PKTINFO(a14_nt36672c_csot_169),
	&PKTINFO(a14_nt36672c_csot_170),
	&PKTINFO(a14_nt36672c_csot_171),
	&PKTINFO(a14_nt36672c_csot_172),
	&PKTINFO(a14_nt36672c_csot_173),
	&PKTINFO(a14_nt36672c_csot_174),
	&PKTINFO(a14_nt36672c_csot_175),
	&PKTINFO(a14_nt36672c_csot_176),
	&PKTINFO(a14_nt36672c_csot_177),
	&PKTINFO(a14_nt36672c_csot_178),
	&PKTINFO(a14_nt36672c_csot_179),
	&PKTINFO(a14_nt36672c_csot_180),
	&PKTINFO(a14_nt36672c_csot_181),
	&PKTINFO(a14_nt36672c_csot_182),
	&PKTINFO(a14_nt36672c_csot_183),
	&PKTINFO(a14_nt36672c_csot_184),
	&PKTINFO(a14_nt36672c_csot_185),
	&PKTINFO(a14_nt36672c_csot_186),
	&PKTINFO(a14_nt36672c_csot_187),
	&PKTINFO(a14_nt36672c_csot_188),
	&PKTINFO(a14_nt36672c_csot_189),
	&PKTINFO(a14_nt36672c_csot_190),
	&PKTINFO(a14_nt36672c_csot_191),
	&PKTINFO(a14_nt36672c_csot_192),
	&PKTINFO(a14_nt36672c_csot_193),
	&PKTINFO(a14_nt36672c_csot_194),
	&PKTINFO(a14_nt36672c_csot_195),
	&PKTINFO(a14_nt36672c_csot_196),
	&PKTINFO(a14_nt36672c_csot_197),
	&PKTINFO(a14_nt36672c_csot_198),
	&PKTINFO(a14_nt36672c_csot_199),
	&PKTINFO(a14_nt36672c_csot_200),
	&PKTINFO(a14_nt36672c_csot_201),
	&PKTINFO(a14_nt36672c_csot_202),
	&PKTINFO(a14_nt36672c_csot_203),
	&PKTINFO(a14_nt36672c_csot_204),
	&PKTINFO(a14_nt36672c_csot_205),
	&PKTINFO(a14_nt36672c_csot_206),
	&PKTINFO(a14_nt36672c_csot_207),
	&PKTINFO(a14_nt36672c_csot_208),
	&PKTINFO(a14_nt36672c_csot_209),
	&PKTINFO(a14_nt36672c_csot_210),
	&PKTINFO(a14_nt36672c_csot_211),
	&PKTINFO(a14_nt36672c_csot_212),
	&PKTINFO(a14_nt36672c_csot_213),
	&PKTINFO(a14_nt36672c_csot_214),
	&PKTINFO(a14_nt36672c_csot_215),
	&PKTINFO(a14_nt36672c_csot_216),
	&PKTINFO(a14_nt36672c_csot_217),
	&PKTINFO(a14_nt36672c_csot_218),
	&PKTINFO(a14_nt36672c_csot_219),
	&PKTINFO(a14_nt36672c_csot_220),
	&PKTINFO(a14_nt36672c_csot_221),
	&PKTINFO(a14_nt36672c_csot_222),
	&PKTINFO(a14_nt36672c_csot_223),
	&PKTINFO(a14_nt36672c_csot_224),
	&PKTINFO(a14_nt36672c_csot_225),
	&PKTINFO(a14_nt36672c_csot_226),
	&PKTINFO(a14_nt36672c_csot_227),
	&PKTINFO(a14_nt36672c_csot_228),
	&PKTINFO(a14_nt36672c_csot_229),
	&PKTINFO(a14_nt36672c_csot_230),
	&PKTINFO(a14_nt36672c_csot_231),
	&PKTINFO(a14_nt36672c_csot_232),
	&PKTINFO(a14_nt36672c_csot_233),
	&PKTINFO(a14_nt36672c_csot_234),
	&PKTINFO(a14_nt36672c_csot_235),
	&PKTINFO(a14_nt36672c_csot_236),
	&PKTINFO(a14_nt36672c_csot_237),
	&PKTINFO(a14_nt36672c_csot_238),
	&PKTINFO(a14_nt36672c_csot_239),
	&PKTINFO(a14_nt36672c_csot_240),
	&PKTINFO(a14_nt36672c_csot_241),
	&PKTINFO(a14_nt36672c_csot_242),
	&PKTINFO(a14_nt36672c_csot_243),
	&PKTINFO(a14_nt36672c_csot_244),
	&PKTINFO(a14_nt36672c_csot_245),
	&PKTINFO(a14_nt36672c_csot_246),
	&PKTINFO(a14_nt36672c_csot_247),
	&PKTINFO(a14_nt36672c_csot_248),
	&PKTINFO(a14_nt36672c_csot_249),
	&PKTINFO(a14_nt36672c_csot_250),
	&PKTINFO(a14_nt36672c_csot_251),
	&PKTINFO(a14_nt36672c_csot_252),
	&PKTINFO(a14_nt36672c_csot_253),
	&PKTINFO(a14_nt36672c_csot_254),
	&PKTINFO(a14_nt36672c_csot_255),
	&PKTINFO(a14_nt36672c_csot_256),
	&PKTINFO(a14_nt36672c_csot_257),
	&PKTINFO(a14_nt36672c_csot_258),
	&PKTINFO(a14_nt36672c_csot_259),
	&PKTINFO(a14_nt36672c_csot_260),
	&PKTINFO(a14_nt36672c_csot_261),
	&PKTINFO(a14_nt36672c_csot_262),
	&PKTINFO(a14_nt36672c_csot_263),
	&PKTINFO(a14_nt36672c_csot_264),
	&PKTINFO(a14_nt36672c_csot_265),
	&PKTINFO(a14_nt36672c_csot_266),
	&PKTINFO(a14_nt36672c_csot_267),
	&PKTINFO(a14_nt36672c_csot_268),
	&PKTINFO(a14_nt36672c_csot_269),
	&PKTINFO(a14_nt36672c_csot_270),
	&PKTINFO(a14_nt36672c_csot_271),
	&PKTINFO(a14_nt36672c_csot_272),
	&PKTINFO(a14_nt36672c_csot_273),
	&PKTINFO(a14_nt36672c_csot_274),
	&PKTINFO(a14_nt36672c_csot_275),
	&PKTINFO(a14_nt36672c_csot_276),
	&PKTINFO(a14_nt36672c_csot_277),
	&PKTINFO(a14_nt36672c_csot_278),
	&PKTINFO(a14_nt36672c_csot_279),
	&PKTINFO(a14_nt36672c_csot_280),
	&PKTINFO(a14_nt36672c_csot_281),
	&PKTINFO(a14_nt36672c_csot_282),
	&PKTINFO(a14_nt36672c_csot_283),
	&PKTINFO(a14_nt36672c_csot_284),
	&PKTINFO(a14_nt36672c_csot_285),
	&PKTINFO(a14_nt36672c_csot_286),
	&PKTINFO(a14_nt36672c_csot_287),
	&PKTINFO(a14_nt36672c_csot_288),
	&PKTINFO(a14_nt36672c_csot_289),
	&PKTINFO(a14_nt36672c_csot_290),
	&PKTINFO(a14_nt36672c_csot_291),
	&PKTINFO(a14_nt36672c_csot_292),
	&PKTINFO(a14_nt36672c_csot_293),
	&PKTINFO(a14_nt36672c_csot_294),
	&PKTINFO(a14_nt36672c_csot_295),
	&PKTINFO(a14_nt36672c_csot_296),
	&PKTINFO(a14_nt36672c_csot_297),
	&PKTINFO(a14_nt36672c_csot_298),
	&PKTINFO(a14_nt36672c_csot_299),
	&PKTINFO(a14_nt36672c_csot_300),
	&PKTINFO(a14_nt36672c_csot_301),
	&PKTINFO(a14_nt36672c_csot_302),
	&PKTINFO(a14_nt36672c_csot_303),
	&PKTINFO(a14_nt36672c_csot_304),
	&PKTINFO(a14_nt36672c_csot_305),
	&PKTINFO(a14_nt36672c_csot_306),
	&PKTINFO(a14_nt36672c_csot_307),
	&PKTINFO(a14_nt36672c_csot_308),
	&PKTINFO(a14_nt36672c_csot_309),
	&PKTINFO(a14_nt36672c_csot_310),
	&PKTINFO(a14_nt36672c_csot_311),
	&PKTINFO(a14_nt36672c_csot_312),
	&PKTINFO(a14_nt36672c_csot_313),
	&PKTINFO(a14_nt36672c_csot_314),
	&PKTINFO(a14_nt36672c_csot_315),
	&PKTINFO(a14_nt36672c_csot_316),
	&PKTINFO(a14_nt36672c_csot_317),
	&PKTINFO(a14_nt36672c_csot_318),
	&PKTINFO(a14_nt36672c_csot_319),
	&PKTINFO(a14_nt36672c_csot_320),
	&PKTINFO(a14_nt36672c_csot_321),
	&PKTINFO(a14_nt36672c_csot_322),
	&PKTINFO(a14_nt36672c_csot_323),
	&PKTINFO(a14_nt36672c_csot_324),
	&PKTINFO(a14_nt36672c_csot_325),
	&PKTINFO(a14_nt36672c_csot_326),
	&PKTINFO(a14_nt36672c_csot_327),
	&PKTINFO(a14_nt36672c_csot_328),
	&PKTINFO(a14_nt36672c_csot_329),
	&PKTINFO(a14_nt36672c_csot_330),
	&PKTINFO(a14_nt36672c_csot_331),
	&PKTINFO(a14_nt36672c_csot_332),
	&PKTINFO(a14_nt36672c_csot_333),
	&PKTINFO(a14_nt36672c_csot_334),
	&PKTINFO(a14_nt36672c_csot_335),
	&PKTINFO(a14_nt36672c_csot_336),
	&PKTINFO(a14_nt36672c_csot_337),
	&PKTINFO(a14_nt36672c_csot_338),
	&PKTINFO(a14_nt36672c_csot_339),
	&PKTINFO(a14_nt36672c_csot_340),
	&PKTINFO(a14_nt36672c_csot_341),
	&PKTINFO(a14_nt36672c_csot_342),
	&PKTINFO(a14_nt36672c_csot_343),
	&PKTINFO(a14_nt36672c_csot_344),
	&PKTINFO(a14_nt36672c_csot_345),
	&PKTINFO(a14_nt36672c_csot_346),
	&PKTINFO(a14_nt36672c_csot_347),
	&PKTINFO(a14_nt36672c_csot_348),
	&PKTINFO(a14_nt36672c_csot_349),
	&PKTINFO(a14_nt36672c_csot_350),
	&PKTINFO(a14_nt36672c_csot_351),
	&PKTINFO(a14_nt36672c_csot_352),
	&PKTINFO(a14_nt36672c_csot_353),
	&PKTINFO(a14_nt36672c_csot_354),
	&PKTINFO(a14_nt36672c_csot_355),
	&PKTINFO(a14_nt36672c_csot_356),
	&PKTINFO(a14_nt36672c_csot_357),
	&PKTINFO(a14_nt36672c_csot_358),
	&PKTINFO(a14_nt36672c_csot_359),
	&PKTINFO(a14_nt36672c_csot_360),
	&PKTINFO(a14_nt36672c_csot_361),
	&PKTINFO(a14_nt36672c_csot_362),
	&PKTINFO(a14_nt36672c_csot_363),
	&PKTINFO(a14_nt36672c_csot_364),
	&PKTINFO(a14_nt36672c_csot_365),
	&PKTINFO(a14_nt36672c_csot_366),
	&PKTINFO(a14_nt36672c_csot_367),
	&PKTINFO(a14_nt36672c_csot_368),
	&PKTINFO(a14_nt36672c_csot_369),
	&PKTINFO(a14_nt36672c_csot_370),
	&PKTINFO(a14_nt36672c_csot_371),
	&PKTINFO(a14_nt36672c_csot_372),
	&PKTINFO(a14_nt36672c_csot_373),
	&PKTINFO(a14_nt36672c_csot_374),
	&PKTINFO(a14_nt36672c_csot_375),
	&PKTINFO(a14_nt36672c_csot_376),

	&PKTINFO(a14_sleep_out),
	&DLYINFO(a14_wait_100msec),
};

static void *a14_res_init_cmdtbl[] = {
	&nt36672c_csot_restbl[RES_ID],
};

static void *a14_set_bl_cmdtbl[] = {
	&PKTINFO(a14_brightness), //51h
};

static void *a14_display_on_cmdtbl[] = {
	&PKTINFO(a14_display_on),
	&DLYINFO(a14_wait_40msec),
	&PKTINFO(a14_brightness_mode),
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
static u8 NT36672C_A14_I2C_INIT[] = {
	0x0C, 0x24,
	0x0D, 0x1E,
	0x0E, 0x1E,
	0x09, 0x99,
	0x02, 0x6B,
	0x03, 0x26,
	0x11, 0x74,
	0x04, 0x03,
	0x05, 0xC2,
	0x10, 0x66,
	0x08, 0x13,
};

static u8 NT36672C_A14_I2C_EXIT_BLEN[] = {
	0x08, 0x00,
};

static u8 NT36672C_A14_I2C_DUMP[] = {
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

static DEFINE_STATIC_PACKET(nt36672c_csot_a14_i2c_init, I2C_PKT_TYPE_WR, NT36672C_A14_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(nt36672c_csot_a14_i2c_exit_blen, I2C_PKT_TYPE_WR, NT36672C_A14_I2C_EXIT_BLEN, 0);
static DEFINE_STATIC_PACKET(nt36672c_csot_a14_i2c_dump, I2C_PKT_TYPE_RD, NT36672C_A14_I2C_DUMP, 0);

static void *nt36672c_csot_a14_init_cmdtbl[] = {
	&PKTINFO(nt36672c_csot_a14_i2c_init),
};

static void *nt36672c_csot_a14_exit_cmdtbl[] = {
	&PKTINFO(nt36672c_csot_a14_i2c_exit_blen),
};

static void *nt36672c_csot_a14_dump_cmdtbl[] = {
	&PKTINFO(nt36672c_csot_a14_i2c_dump),
};


static struct seqinfo a14_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a14_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a14_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a14_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a14_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a14_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a14_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", nt36672c_csot_a14_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", nt36672c_csot_a14_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", nt36672c_csot_a14_dump_cmdtbl),
#endif
};

struct common_panel_info nt36672c_csot_a14_default_panel_info = {
	.ldi_name = "nt36672c_csot",
	.name = "nt36672c_csot_a14_default",
	.model = "CSOT_6_517_inch",
	.vendor = "CSO",
	.id = 0x7BF240,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(nt36672c_csot_a14_resol),
		.resol = nt36672c_csot_a14_resol,
	},
	.maptbl = a14_maptbl,
	.nr_maptbl = ARRAY_SIZE(a14_maptbl),
	.seqtbl = a14_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a14_seqtbl),
	.rditbl = nt36672c_csot_rditbl,
	.nr_rditbl = ARRAY_SIZE(nt36672c_csot_rditbl),
	.restbl = nt36672c_csot_restbl,
	.nr_restbl = ARRAY_SIZE(nt36672c_csot_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&nt36672c_csot_a14_panel_dimming_info,
	},
	.i2c_data = &nt36672c_csot_a14_i2c_data,
};

int __init nt36672c_csot_a14_panel_init(void)
{
	register_common_panel(&nt36672c_csot_a14_default_panel_info);

	return 0;
}
//arch_initcall(nt36672c_csot_a14_panel_init)
#endif /* __NT36672C_A14_PANEL_H__ */

