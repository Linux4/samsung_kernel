/*
 * linux/drivers/video/fbdev/exynos/panel/nt36672c_tianma/nt36672c_tianma_a14_panel.h
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
#include "nt36672c_tianma.h"

#include "nt36672c_tianma_a14_panel_dimming.h"
#include "nt36672c_tianma_a14_panel_i2c.h"

#include "nt36672c_tianma_a14_resol.h"

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

static u8 A14_NT36672C_001[] = {
	0xFF,
	0x10,
};

static u8 A14_NT36672C_002[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_003[] = {
	0x36,
	0x00,
};

static u8 A14_NT36672C_004[] = {
	0x3B,
	0x03, 0x0C, 0x0A, 0x04, 0x04,
};

static u8 A14_NT36672C_005[] = {
	0xB0,
	0x00,
};

static u8 A14_NT36672C_006[] = {
	0xC0,
	0x00,
};

static u8 A14_NT36672C_007[] = {
	0xFF,
	0x20,
};

static u8 A14_NT36672C_008[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_009[] = {
	0x01,
	0x66,
};

static u8 A14_NT36672C_010[] = {
	0x06,
	0x64,
};

static u8 A14_NT36672C_011[] = {
	0x07,
	0x28,
};

static u8 A14_NT36672C_012[] = {
	0x17,
	0x55,
};

static u8 A14_NT36672C_013[] = {
	0x19,
	0x55,
};

static u8 A14_NT36672C_014[] = {
	0x1B,
	0x01,
};

static u8 A14_NT36672C_015[] = {
	0x2F,
	0x00,
};

static u8 A14_NT36672C_016[] = {
	0x5C,
	0x90,
};

static u8 A14_NT36672C_017[] = {
	0x5E,
	0x7E,
};

static u8 A14_NT36672C_018[] = {
	0x69,
	0xD0,
};

static u8 A14_NT36672C_019[] = {
	0xF2,
	0x64,
};

static u8 A14_NT36672C_020[] = {
	0xF3,
	0x44,
};

static u8 A14_NT36672C_021[] = {
	0xF4,
	0x64,
};

static u8 A14_NT36672C_022[] = {
	0xF5,
	0x44,
};

static u8 A14_NT36672C_023[] = {
	0xF6,
	0x64,
};

static u8 A14_NT36672C_024[] = {
	0xF7,
	0x44,
};

static u8 A14_NT36672C_025[] = {
	0xF8,
	0x64,
};

static u8 A14_NT36672C_026[] = {
	0xF9,
	0x44,
};

static u8 A14_NT36672C_027[] = {
	0xFF,
	0x23,
};

static u8 A14_NT36672C_028[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_029[] = {
	0x00,
	0x00,
};

static u8 A14_NT36672C_030[] = {
	0x07,
	0x60,
};

static u8 A14_NT36672C_031[] = {
	0x08,
	0x06,
};

static u8 A14_NT36672C_032[] = {
	0x09,
	0x1C,
};

static u8 A14_NT36672C_033[] = {
	0x0A,
	0x2B,
};

static u8 A14_NT36672C_034[] = {
	0x0B,
	0x2B,
};

static u8 A14_NT36672C_035[] = {
	0x0C,
	0x2B,
};

static u8 A14_NT36672C_036[] = {
	0x0D,
	0x00,
};

static u8 A14_NT36672C_037[] = {
	0x10,
	0x50,
};

static u8 A14_NT36672C_038[] = {
	0x11,
	0x01,
};

static u8 A14_NT36672C_039[] = {
	0x12,
	0x95,
};

static u8 A14_NT36672C_040[] = {
	0x15,
	0x68,
};

static u8 A14_NT36672C_041[] = {
	0x16,
	0x0B,
};

static u8 A14_NT36672C_042[] = {
	0x19,
	0x00,
};

static u8 A14_NT36672C_043[] = {
	0x1A,
	0x00,
};

static u8 A14_NT36672C_044[] = {
	0x1B,
	0x00,
};

static u8 A14_NT36672C_045[] = {
	0x1C,
	0x00,
};

static u8 A14_NT36672C_046[] = {
	0x1D,
	0x01,
};

static u8 A14_NT36672C_047[] = {
	0x1E,
	0x03,
};

static u8 A14_NT36672C_048[] = {
	0x1F,
	0x05,
};

static u8 A14_NT36672C_049[] = {
	0x20,
	0x0C,
};

static u8 A14_NT36672C_050[] = {
	0x21,
	0x13,
};

static u8 A14_NT36672C_051[] = {
	0x22,
	0x17,
};

static u8 A14_NT36672C_052[] = {
	0x23,
	0x1D,
};

static u8 A14_NT36672C_053[] = {
	0x24,
	0x23,
};

static u8 A14_NT36672C_054[] = {
	0x25,
	0x2C,
};

static u8 A14_NT36672C_055[] = {
	0x26,
	0x33,
};

static u8 A14_NT36672C_056[] = {
	0x27,
	0x39,
};

static u8 A14_NT36672C_057[] = {
	0x28,
	0x3F,
};

static u8 A14_NT36672C_058[] = {
	0x29,
	0x3F,
};

static u8 A14_NT36672C_059[] = {
	0x2A,
	0x3F,
};

static u8 A14_NT36672C_060[] = {
	0x2B,
	0x3F,
};

static u8 A14_NT36672C_061[] = {
	0x30,
	0xFF,
};

static u8 A14_NT36672C_062[] = {
	0x31,
	0xFE,
};

static u8 A14_NT36672C_063[] = {
	0x32,
	0xFD,
};

static u8 A14_NT36672C_064[] = {
	0x33,
	0xFC,
};

static u8 A14_NT36672C_065[] = {
	0x34,
	0xFB,
};

static u8 A14_NT36672C_066[] = {
	0x35,
	0xFA,
};

static u8 A14_NT36672C_067[] = {
	0x36,
	0xF9,
};

static u8 A14_NT36672C_068[] = {
	0x37,
	0xF7,
};

static u8 A14_NT36672C_069[] = {
	0x38,
	0xF5,
};

static u8 A14_NT36672C_070[] = {
	0x39,
	0xF3,
};

static u8 A14_NT36672C_071[] = {
	0x3A,
	0xF1,
};

static u8 A14_NT36672C_072[] = {
	0x3B,
	0xEE,
};

static u8 A14_NT36672C_073[] = {
	0x3D,
	0xEC,
};

static u8 A14_NT36672C_074[] = {
	0x3F,
	0xEA,
};

static u8 A14_NT36672C_075[] = {
	0x40,
	0xE8,
};

static u8 A14_NT36672C_076[] = {
	0x41,
	0xE6,
};

static u8 A14_NT36672C_077[] = {
	0x04,
	0x00,
};

static u8 A14_NT36672C_078[] = {
	0xA0,
	0x11,
};

static u8 A14_NT36672C_079[] = {
	0xFF,
	0x24,
};

static u8 A14_NT36672C_080[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_081[] = {
	0x4E,
	0x72,
};

static u8 A14_NT36672C_082[] = {
	0x4F,
	0x72,
};

static u8 A14_NT36672C_083[] = {
	0x53,
	0x72,
};

static u8 A14_NT36672C_084[] = {
	0x77,
	0x80,
};

static u8 A14_NT36672C_085[] = {
	0x79,
	0x03,
};

static u8 A14_NT36672C_086[] = {
	0x7A,
	0x02,
};

static u8 A14_NT36672C_087[] = {
	0x7B,
	0x20,
};

static u8 A14_NT36672C_088[] = {
	0x7D,
	0x06,
};

static u8 A14_NT36672C_089[] = {
	0x80,
	0x05,
};

static u8 A14_NT36672C_090[] = {
	0x81,
	0x05,
};

static u8 A14_NT36672C_091[] = {
	0x82,
	0x13,
};

static u8 A14_NT36672C_092[] = {
	0x84,
	0x31,
};

static u8 A14_NT36672C_093[] = {
	0x85,
	0x13,
};

static u8 A14_NT36672C_094[] = {
	0x86,
	0x22,
};

static u8 A14_NT36672C_095[] = {
	0x87,
	0x31,
};

static u8 A14_NT36672C_096[] = {
	0x90,
	0x13,
};

static u8 A14_NT36672C_097[] = {
	0x92,
	0x31,
};

static u8 A14_NT36672C_098[] = {
	0x93,
	0x13,
};

static u8 A14_NT36672C_099[] = {
	0x94,
	0x22,
};

static u8 A14_NT36672C_100[] = {
	0x95,
	0x31,
};

static u8 A14_NT36672C_101[] = {
	0x9C,
	0xF4,
};

static u8 A14_NT36672C_102[] = {
	0x9D,
	0x01,
};

static u8 A14_NT36672C_103[] = {
	0xA0,
	0x20,
};

static u8 A14_NT36672C_104[] = {
	0xA2,
	0x20,
};

static u8 A14_NT36672C_105[] = {
	0xA3,
	0x02,
};

static u8 A14_NT36672C_106[] = {
	0xA4,
	0x06,
};

static u8 A14_NT36672C_107[] = {
	0xA5,
	0x06,
};

static u8 A14_NT36672C_108[] = {
	0xC4,
	0x40,
};

static u8 A14_NT36672C_109[] = {
	0xC6,
	0x40,
};

static u8 A14_NT36672C_110[] = {
	0xC9,
	0x00,
};

static u8 A14_NT36672C_111[] = {
	0xD9,
	0x80,
};

static u8 A14_NT36672C_112[] = {
	0xE9,
	0x02,
};

static u8 A14_NT36672C_113[] = {
	0xFF,
	0x25,
};

static u8 A14_NT36672C_114[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_115[] = {
	0x0F,
	0x1B,
};

static u8 A14_NT36672C_116[] = {
	0x18,
	0x20,
};

static u8 A14_NT36672C_117[] = {
	0x19,
	0xE4,
};

static u8 A14_NT36672C_118[] = {
	0x21,
	0x00,
};

static u8 A14_NT36672C_119[] = {
	0x66,
	0x40,
};

static u8 A14_NT36672C_120[] = {
	0x67,
	0x29,
};

static u8 A14_NT36672C_121[] = {
	0x68,
	0x50,
};

static u8 A14_NT36672C_122[] = {
	0x69,
	0x20,
};

static u8 A14_NT36672C_123[] = {
	0x6B,
	0x00,
};

static u8 A14_NT36672C_124[] = {
	0x71,
	0x6D,
};

static u8 A14_NT36672C_125[] = {
	0x77,
	0x60,
};

static u8 A14_NT36672C_126[] = {
	0x78,
	0xA5,
};

static u8 A14_NT36672C_127[] = {
	0x7D,
	0x40,
};

static u8 A14_NT36672C_128[] = {
	0x7E,
	0x2D,
};

static u8 A14_NT36672C_129[] = {
	0x84,
	0x70,
};

static u8 A14_NT36672C_130[] = {
	0xC0,
	0x4D,
};

static u8 A14_NT36672C_131[] = {
	0xC1,
	0xA9,
};

static u8 A14_NT36672C_132[] = {
	0xC2,
	0xD2,
};

static u8 A14_NT36672C_133[] = {
	0xC4,
	0x11,
};

static u8 A14_NT36672C_134[] = {
	0xD6,
	0x80,
};

static u8 A14_NT36672C_135[] = {
	0xD7,
	0x02,
};

static u8 A14_NT36672C_136[] = {
	0xDA,
	0x02,
};

static u8 A14_NT36672C_137[] = {
	0xDD,
	0x02,
};

static u8 A14_NT36672C_138[] = {
	0xE0,
	0x02,
};

static u8 A14_NT36672C_139[] = {
	0xF0,
	0x00,
};

static u8 A14_NT36672C_140[] = {
	0xF1,
	0x04,
};

static u8 A14_NT36672C_141[] = {
	0xFF,
	0x26,
};

static u8 A14_NT36672C_142[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_143[] = {
	0x00,
	0x10,
};

static u8 A14_NT36672C_144[] = {
	0x01,
	0xF3,
};

static u8 A14_NT36672C_145[] = {
	0x03,
	0x00,
};

static u8 A14_NT36672C_146[] = {
	0x04,
	0xF3,
};

static u8 A14_NT36672C_147[] = {
	0x05,
	0x08,
};

static u8 A14_NT36672C_148[] = {
	0x06,
	0x21,
};

static u8 A14_NT36672C_149[] = {
	0x08,
	0x21,
};

static u8 A14_NT36672C_150[] = {
	0x14,
	0x06,
};

static u8 A14_NT36672C_151[] = {
	0x15,
	0x01,
};

static u8 A14_NT36672C_152[] = {
	0x74,
	0xAF,
};

static u8 A14_NT36672C_153[] = {
	0x81,
	0x10,
};

static u8 A14_NT36672C_154[] = {
	0x83,
	0x02,
};

static u8 A14_NT36672C_155[] = {
	0x84,
	0x04,
};

static u8 A14_NT36672C_156[] = {
	0x85,
	0x01,
};

static u8 A14_NT36672C_157[] = {
	0x86,
	0x04,
};

static u8 A14_NT36672C_158[] = {
	0x87,
	0x01,
};

static u8 A14_NT36672C_159[] = {
	0x88,
	0x42,
};

static u8 A14_NT36672C_160[] = {
	0x8A,
	0x1A,
};

static u8 A14_NT36672C_161[] = {
	0x8B,
	0x11,
};

static u8 A14_NT36672C_162[] = {
	0x8C,
	0x24,
};

static u8 A14_NT36672C_163[] = {
	0x8E,
	0x42,
};

static u8 A14_NT36672C_164[] = {
	0x8F,
	0x11,
};

static u8 A14_NT36672C_165[] = {
	0x90,
	0x11,
};

static u8 A14_NT36672C_166[] = {
	0x91,
	0x11,
};

static u8 A14_NT36672C_167[] = {
	0x9A,
	0x80,
};

static u8 A14_NT36672C_168[] = {
	0x9B,
	0x04,
};

static u8 A14_NT36672C_169[] = {
	0x9C,
	0x00,
};

static u8 A14_NT36672C_170[] = {
	0x9D,
	0x00,
};

static u8 A14_NT36672C_171[] = {
	0x9E,
	0x00,
};

static u8 A14_NT36672C_172[] = {
	0xFF,
	0x27,
};

static u8 A14_NT36672C_173[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_174[] = {
	0x01,
	0x68,
};

static u8 A14_NT36672C_175[] = {
	0x20,
	0x82,
};

static u8 A14_NT36672C_176[] = {
	0x21,
	0xED,
};

static u8 A14_NT36672C_177[] = {
	0x25,
	0x83,
};

static u8 A14_NT36672C_178[] = {
	0x26,
	0x3E,
};

static u8 A14_NT36672C_179[] = {
	0x6E,
	0x23,
};

static u8 A14_NT36672C_180[] = {
	0x6F,
	0x01,
};

static u8 A14_NT36672C_181[] = {
	0x70,
	0x00,
};

static u8 A14_NT36672C_182[] = {
	0x71,
	0x00,
};

static u8 A14_NT36672C_183[] = {
	0x72,
	0x00,
};

static u8 A14_NT36672C_184[] = {
	0x73,
	0x21,
};

static u8 A14_NT36672C_185[] = {
	0x74,
	0x03,
};

static u8 A14_NT36672C_186[] = {
	0x75,
	0x00,
};

static u8 A14_NT36672C_187[] = {
	0x76,
	0x00,
};

static u8 A14_NT36672C_188[] = {
	0x77,
	0x00,
};

static u8 A14_NT36672C_189[] = {
	0x7D,
	0x09,
};

static u8 A14_NT36672C_190[] = {
	0x7E,
	0x6B,
};

static u8 A14_NT36672C_191[] = {
	0x80,
	0x23,
};

static u8 A14_NT36672C_192[] = {
	0x82,
	0x09,
};

static u8 A14_NT36672C_193[] = {
	0x83,
	0x6B,
};

static u8 A14_NT36672C_194[] = {
	0x88,
	0x03,
};

static u8 A14_NT36672C_195[] = {
	0x89,
	0x01,
};

static u8 A14_NT36672C_196[] = {
	0xFF,
	0x2A,
};

static u8 A14_NT36672C_197[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_198[] = {
	0x00,
	0x91,
};

static u8 A14_NT36672C_199[] = {
	0x03,
	0x20,
};

static u8 A14_NT36672C_200[] = {
	0x07,
	0x64,
};

static u8 A14_NT36672C_201[] = {
	0x0A,
	0x70,
};

static u8 A14_NT36672C_202[] = {
	0x0C,
	0x09,
};

static u8 A14_NT36672C_203[] = {
	0x0D,
	0x40,
};

static u8 A14_NT36672C_204[] = {
	0x0E,
	0x02,
};

static u8 A14_NT36672C_205[] = {
	0x0F,
	0x00,
};

static u8 A14_NT36672C_206[] = {
	0x11,
	0xF0,
};

static u8 A14_NT36672C_207[] = {
	0x15,
	0x0F,
};

static u8 A14_NT36672C_208[] = {
	0x16,
	0x2D,
};

static u8 A14_NT36672C_209[] = {
	0x19,
	0x0F,
};

static u8 A14_NT36672C_210[] = {
	0x1A,
	0x01,
};

static u8 A14_NT36672C_211[] = {
	0x1B,
	0x0C,
};

static u8 A14_NT36672C_212[] = {
	0x1D,
	0x0A,
};

static u8 A14_NT36672C_213[] = {
	0x1E,
	0x82,
};

static u8 A14_NT36672C_214[] = {
	0x1F,
	0x82,
};

static u8 A14_NT36672C_215[] = {
	0x20,
	0x82,
};

static u8 A14_NT36672C_216[] = {
	0x27,
	0x00,
};

static u8 A14_NT36672C_217[] = {
	0x28,
	0x08,
};

static u8 A14_NT36672C_218[] = {
	0x29,
	0x1C,
};

static u8 A14_NT36672C_219[] = {
	0x2A,
	0x4C,
};

static u8 A14_NT36672C_220[] = {
	0x2B,
	0x00,
};

static u8 A14_NT36672C_221[] = {
	0x2D,
	0x09,
};

static u8 A14_NT36672C_222[] = {
	0x2F,
	0x03,
};

static u8 A14_NT36672C_223[] = {
	0x30,
	0x1E,
};

static u8 A14_NT36672C_224[] = {
	0x31,
	0x40,
};

static u8 A14_NT36672C_225[] = {
	0x33,
	0x2E,
};

static u8 A14_NT36672C_226[] = {
	0x34,
	0x74,
};

static u8 A14_NT36672C_227[] = {
	0x35,
	0x28,
};

static u8 A14_NT36672C_228[] = {
	0x36,
	0xC8,
};

static u8 A14_NT36672C_229[] = {
	0x37,
	0x72,
};

static u8 A14_NT36672C_230[] = {
	0x38,
	0x28,
};

static u8 A14_NT36672C_231[] = {
	0x39,
	0xC8,
};

static u8 A14_NT36672C_232[] = {
	0x3A,
	0x1E,
};

static u8 A14_NT36672C_233[] = {
	0xFF,
	0xE0,
};

static u8 A14_NT36672C_234[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_235[] = {
	0x35,
	0x82,
};

static u8 A14_NT36672C_236[] = {
	0x85,
	0x30,
};

static u8 A14_NT36672C_237[] = {
	0xFF,
	0xF0,
};

static u8 A14_NT36672C_238[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_239[] = {
	0x1C,
	0x01,
};

static u8 A14_NT36672C_240[] = {
	0x33,
	0x01,
};

static u8 A14_NT36672C_241[] = {
	0x5A,
	0x00,
};

static u8 A14_NT36672C_242[] = {
	0xD2,
	0x50,
};

static u8 A14_NT36672C_243[] = {
	0xFF,
	0xC0,
};

static u8 A14_NT36672C_244[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_245[] = {
	0x9C,
	0x11,
};

static u8 A14_NT36672C_246[] = {
	0x9D,
	0x11,
};

static u8 A14_NT36672C_247[] = {
	0xFF,
	0xD0,
};

static u8 A14_NT36672C_248[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_249[] = {
	0x25,
	0xA9,
};

static u8 A14_NT36672C_250[] = {
	0x53,
	0x22,
};

static u8 A14_NT36672C_251[] = {
	0x54,
	0x02,
};

static u8 A14_NT36672C_252[] = {
	0xFF,
	0x10,
};

static u8 A14_NT36672C_253[] = {
	0xFB,
	0x01,
};

static u8 A14_NT36672C_254[] = {
	0x53,
	0x2C,
};

static u8 A14_NT36672C_255[] = {
	0x55,
	0x01,
};

static u8 A14_NT36672C_256[] = {
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

static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_001, DSI_PKT_TYPE_WR, A14_NT36672C_001, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_002, DSI_PKT_TYPE_WR, A14_NT36672C_002, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_003, DSI_PKT_TYPE_WR, A14_NT36672C_003, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_004, DSI_PKT_TYPE_WR, A14_NT36672C_004, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_005, DSI_PKT_TYPE_WR, A14_NT36672C_005, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_006, DSI_PKT_TYPE_WR, A14_NT36672C_006, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_007, DSI_PKT_TYPE_WR, A14_NT36672C_007, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_008, DSI_PKT_TYPE_WR, A14_NT36672C_008, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_009, DSI_PKT_TYPE_WR, A14_NT36672C_009, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_010, DSI_PKT_TYPE_WR, A14_NT36672C_010, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_011, DSI_PKT_TYPE_WR, A14_NT36672C_011, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_012, DSI_PKT_TYPE_WR, A14_NT36672C_012, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_013, DSI_PKT_TYPE_WR, A14_NT36672C_013, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_014, DSI_PKT_TYPE_WR, A14_NT36672C_014, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_015, DSI_PKT_TYPE_WR, A14_NT36672C_015, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_016, DSI_PKT_TYPE_WR, A14_NT36672C_016, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_017, DSI_PKT_TYPE_WR, A14_NT36672C_017, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_018, DSI_PKT_TYPE_WR, A14_NT36672C_018, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_019, DSI_PKT_TYPE_WR, A14_NT36672C_019, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_020, DSI_PKT_TYPE_WR, A14_NT36672C_020, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_021, DSI_PKT_TYPE_WR, A14_NT36672C_021, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_022, DSI_PKT_TYPE_WR, A14_NT36672C_022, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_023, DSI_PKT_TYPE_WR, A14_NT36672C_023, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_024, DSI_PKT_TYPE_WR, A14_NT36672C_024, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_025, DSI_PKT_TYPE_WR, A14_NT36672C_025, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_026, DSI_PKT_TYPE_WR, A14_NT36672C_026, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_027, DSI_PKT_TYPE_WR, A14_NT36672C_027, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_028, DSI_PKT_TYPE_WR, A14_NT36672C_028, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_029, DSI_PKT_TYPE_WR, A14_NT36672C_029, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_030, DSI_PKT_TYPE_WR, A14_NT36672C_030, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_031, DSI_PKT_TYPE_WR, A14_NT36672C_031, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_032, DSI_PKT_TYPE_WR, A14_NT36672C_032, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_033, DSI_PKT_TYPE_WR, A14_NT36672C_033, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_034, DSI_PKT_TYPE_WR, A14_NT36672C_034, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_035, DSI_PKT_TYPE_WR, A14_NT36672C_035, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_036, DSI_PKT_TYPE_WR, A14_NT36672C_036, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_037, DSI_PKT_TYPE_WR, A14_NT36672C_037, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_038, DSI_PKT_TYPE_WR, A14_NT36672C_038, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_039, DSI_PKT_TYPE_WR, A14_NT36672C_039, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_040, DSI_PKT_TYPE_WR, A14_NT36672C_040, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_041, DSI_PKT_TYPE_WR, A14_NT36672C_041, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_042, DSI_PKT_TYPE_WR, A14_NT36672C_042, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_043, DSI_PKT_TYPE_WR, A14_NT36672C_043, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_044, DSI_PKT_TYPE_WR, A14_NT36672C_044, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_045, DSI_PKT_TYPE_WR, A14_NT36672C_045, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_046, DSI_PKT_TYPE_WR, A14_NT36672C_046, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_047, DSI_PKT_TYPE_WR, A14_NT36672C_047, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_048, DSI_PKT_TYPE_WR, A14_NT36672C_048, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_049, DSI_PKT_TYPE_WR, A14_NT36672C_049, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_050, DSI_PKT_TYPE_WR, A14_NT36672C_050, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_051, DSI_PKT_TYPE_WR, A14_NT36672C_051, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_052, DSI_PKT_TYPE_WR, A14_NT36672C_052, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_053, DSI_PKT_TYPE_WR, A14_NT36672C_053, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_054, DSI_PKT_TYPE_WR, A14_NT36672C_054, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_055, DSI_PKT_TYPE_WR, A14_NT36672C_055, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_056, DSI_PKT_TYPE_WR, A14_NT36672C_056, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_057, DSI_PKT_TYPE_WR, A14_NT36672C_057, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_058, DSI_PKT_TYPE_WR, A14_NT36672C_058, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_059, DSI_PKT_TYPE_WR, A14_NT36672C_059, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_060, DSI_PKT_TYPE_WR, A14_NT36672C_060, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_061, DSI_PKT_TYPE_WR, A14_NT36672C_061, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_062, DSI_PKT_TYPE_WR, A14_NT36672C_062, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_063, DSI_PKT_TYPE_WR, A14_NT36672C_063, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_064, DSI_PKT_TYPE_WR, A14_NT36672C_064, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_065, DSI_PKT_TYPE_WR, A14_NT36672C_065, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_066, DSI_PKT_TYPE_WR, A14_NT36672C_066, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_067, DSI_PKT_TYPE_WR, A14_NT36672C_067, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_068, DSI_PKT_TYPE_WR, A14_NT36672C_068, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_069, DSI_PKT_TYPE_WR, A14_NT36672C_069, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_070, DSI_PKT_TYPE_WR, A14_NT36672C_070, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_071, DSI_PKT_TYPE_WR, A14_NT36672C_071, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_072, DSI_PKT_TYPE_WR, A14_NT36672C_072, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_073, DSI_PKT_TYPE_WR, A14_NT36672C_073, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_074, DSI_PKT_TYPE_WR, A14_NT36672C_074, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_075, DSI_PKT_TYPE_WR, A14_NT36672C_075, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_076, DSI_PKT_TYPE_WR, A14_NT36672C_076, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_077, DSI_PKT_TYPE_WR, A14_NT36672C_077, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_078, DSI_PKT_TYPE_WR, A14_NT36672C_078, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_079, DSI_PKT_TYPE_WR, A14_NT36672C_079, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_080, DSI_PKT_TYPE_WR, A14_NT36672C_080, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_081, DSI_PKT_TYPE_WR, A14_NT36672C_081, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_082, DSI_PKT_TYPE_WR, A14_NT36672C_082, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_083, DSI_PKT_TYPE_WR, A14_NT36672C_083, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_084, DSI_PKT_TYPE_WR, A14_NT36672C_084, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_085, DSI_PKT_TYPE_WR, A14_NT36672C_085, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_086, DSI_PKT_TYPE_WR, A14_NT36672C_086, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_087, DSI_PKT_TYPE_WR, A14_NT36672C_087, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_088, DSI_PKT_TYPE_WR, A14_NT36672C_088, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_089, DSI_PKT_TYPE_WR, A14_NT36672C_089, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_090, DSI_PKT_TYPE_WR, A14_NT36672C_090, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_091, DSI_PKT_TYPE_WR, A14_NT36672C_091, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_092, DSI_PKT_TYPE_WR, A14_NT36672C_092, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_093, DSI_PKT_TYPE_WR, A14_NT36672C_093, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_094, DSI_PKT_TYPE_WR, A14_NT36672C_094, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_095, DSI_PKT_TYPE_WR, A14_NT36672C_095, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_096, DSI_PKT_TYPE_WR, A14_NT36672C_096, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_097, DSI_PKT_TYPE_WR, A14_NT36672C_097, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_098, DSI_PKT_TYPE_WR, A14_NT36672C_098, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_099, DSI_PKT_TYPE_WR, A14_NT36672C_099, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_100, DSI_PKT_TYPE_WR, A14_NT36672C_100, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_101, DSI_PKT_TYPE_WR, A14_NT36672C_101, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_102, DSI_PKT_TYPE_WR, A14_NT36672C_102, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_103, DSI_PKT_TYPE_WR, A14_NT36672C_103, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_104, DSI_PKT_TYPE_WR, A14_NT36672C_104, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_105, DSI_PKT_TYPE_WR, A14_NT36672C_105, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_106, DSI_PKT_TYPE_WR, A14_NT36672C_106, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_107, DSI_PKT_TYPE_WR, A14_NT36672C_107, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_108, DSI_PKT_TYPE_WR, A14_NT36672C_108, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_109, DSI_PKT_TYPE_WR, A14_NT36672C_109, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_110, DSI_PKT_TYPE_WR, A14_NT36672C_110, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_111, DSI_PKT_TYPE_WR, A14_NT36672C_111, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_112, DSI_PKT_TYPE_WR, A14_NT36672C_112, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_113, DSI_PKT_TYPE_WR, A14_NT36672C_113, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_114, DSI_PKT_TYPE_WR, A14_NT36672C_114, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_115, DSI_PKT_TYPE_WR, A14_NT36672C_115, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_116, DSI_PKT_TYPE_WR, A14_NT36672C_116, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_117, DSI_PKT_TYPE_WR, A14_NT36672C_117, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_118, DSI_PKT_TYPE_WR, A14_NT36672C_118, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_119, DSI_PKT_TYPE_WR, A14_NT36672C_119, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_120, DSI_PKT_TYPE_WR, A14_NT36672C_120, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_121, DSI_PKT_TYPE_WR, A14_NT36672C_121, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_122, DSI_PKT_TYPE_WR, A14_NT36672C_122, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_123, DSI_PKT_TYPE_WR, A14_NT36672C_123, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_124, DSI_PKT_TYPE_WR, A14_NT36672C_124, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_125, DSI_PKT_TYPE_WR, A14_NT36672C_125, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_126, DSI_PKT_TYPE_WR, A14_NT36672C_126, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_127, DSI_PKT_TYPE_WR, A14_NT36672C_127, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_128, DSI_PKT_TYPE_WR, A14_NT36672C_128, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_129, DSI_PKT_TYPE_WR, A14_NT36672C_129, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_130, DSI_PKT_TYPE_WR, A14_NT36672C_130, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_131, DSI_PKT_TYPE_WR, A14_NT36672C_131, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_132, DSI_PKT_TYPE_WR, A14_NT36672C_132, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_133, DSI_PKT_TYPE_WR, A14_NT36672C_133, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_134, DSI_PKT_TYPE_WR, A14_NT36672C_134, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_135, DSI_PKT_TYPE_WR, A14_NT36672C_135, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_136, DSI_PKT_TYPE_WR, A14_NT36672C_136, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_137, DSI_PKT_TYPE_WR, A14_NT36672C_137, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_138, DSI_PKT_TYPE_WR, A14_NT36672C_138, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_139, DSI_PKT_TYPE_WR, A14_NT36672C_139, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_140, DSI_PKT_TYPE_WR, A14_NT36672C_140, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_141, DSI_PKT_TYPE_WR, A14_NT36672C_141, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_142, DSI_PKT_TYPE_WR, A14_NT36672C_142, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_143, DSI_PKT_TYPE_WR, A14_NT36672C_143, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_144, DSI_PKT_TYPE_WR, A14_NT36672C_144, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_145, DSI_PKT_TYPE_WR, A14_NT36672C_145, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_146, DSI_PKT_TYPE_WR, A14_NT36672C_146, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_147, DSI_PKT_TYPE_WR, A14_NT36672C_147, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_148, DSI_PKT_TYPE_WR, A14_NT36672C_148, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_149, DSI_PKT_TYPE_WR, A14_NT36672C_149, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_150, DSI_PKT_TYPE_WR, A14_NT36672C_150, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_151, DSI_PKT_TYPE_WR, A14_NT36672C_151, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_152, DSI_PKT_TYPE_WR, A14_NT36672C_152, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_153, DSI_PKT_TYPE_WR, A14_NT36672C_153, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_154, DSI_PKT_TYPE_WR, A14_NT36672C_154, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_155, DSI_PKT_TYPE_WR, A14_NT36672C_155, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_156, DSI_PKT_TYPE_WR, A14_NT36672C_156, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_157, DSI_PKT_TYPE_WR, A14_NT36672C_157, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_158, DSI_PKT_TYPE_WR, A14_NT36672C_158, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_159, DSI_PKT_TYPE_WR, A14_NT36672C_159, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_160, DSI_PKT_TYPE_WR, A14_NT36672C_160, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_161, DSI_PKT_TYPE_WR, A14_NT36672C_161, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_162, DSI_PKT_TYPE_WR, A14_NT36672C_162, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_163, DSI_PKT_TYPE_WR, A14_NT36672C_163, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_164, DSI_PKT_TYPE_WR, A14_NT36672C_164, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_165, DSI_PKT_TYPE_WR, A14_NT36672C_165, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_166, DSI_PKT_TYPE_WR, A14_NT36672C_166, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_167, DSI_PKT_TYPE_WR, A14_NT36672C_167, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_168, DSI_PKT_TYPE_WR, A14_NT36672C_168, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_169, DSI_PKT_TYPE_WR, A14_NT36672C_169, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_170, DSI_PKT_TYPE_WR, A14_NT36672C_170, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_171, DSI_PKT_TYPE_WR, A14_NT36672C_171, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_172, DSI_PKT_TYPE_WR, A14_NT36672C_172, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_173, DSI_PKT_TYPE_WR, A14_NT36672C_173, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_174, DSI_PKT_TYPE_WR, A14_NT36672C_174, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_175, DSI_PKT_TYPE_WR, A14_NT36672C_175, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_176, DSI_PKT_TYPE_WR, A14_NT36672C_176, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_177, DSI_PKT_TYPE_WR, A14_NT36672C_177, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_178, DSI_PKT_TYPE_WR, A14_NT36672C_178, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_179, DSI_PKT_TYPE_WR, A14_NT36672C_179, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_180, DSI_PKT_TYPE_WR, A14_NT36672C_180, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_181, DSI_PKT_TYPE_WR, A14_NT36672C_181, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_182, DSI_PKT_TYPE_WR, A14_NT36672C_182, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_183, DSI_PKT_TYPE_WR, A14_NT36672C_183, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_184, DSI_PKT_TYPE_WR, A14_NT36672C_184, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_185, DSI_PKT_TYPE_WR, A14_NT36672C_185, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_186, DSI_PKT_TYPE_WR, A14_NT36672C_186, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_187, DSI_PKT_TYPE_WR, A14_NT36672C_187, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_188, DSI_PKT_TYPE_WR, A14_NT36672C_188, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_189, DSI_PKT_TYPE_WR, A14_NT36672C_189, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_190, DSI_PKT_TYPE_WR, A14_NT36672C_190, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_191, DSI_PKT_TYPE_WR, A14_NT36672C_191, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_192, DSI_PKT_TYPE_WR, A14_NT36672C_192, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_193, DSI_PKT_TYPE_WR, A14_NT36672C_193, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_194, DSI_PKT_TYPE_WR, A14_NT36672C_194, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_195, DSI_PKT_TYPE_WR, A14_NT36672C_195, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_196, DSI_PKT_TYPE_WR, A14_NT36672C_196, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_197, DSI_PKT_TYPE_WR, A14_NT36672C_197, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_198, DSI_PKT_TYPE_WR, A14_NT36672C_198, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_199, DSI_PKT_TYPE_WR, A14_NT36672C_199, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_200, DSI_PKT_TYPE_WR, A14_NT36672C_200, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_201, DSI_PKT_TYPE_WR, A14_NT36672C_201, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_202, DSI_PKT_TYPE_WR, A14_NT36672C_202, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_203, DSI_PKT_TYPE_WR, A14_NT36672C_203, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_204, DSI_PKT_TYPE_WR, A14_NT36672C_204, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_205, DSI_PKT_TYPE_WR, A14_NT36672C_205, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_206, DSI_PKT_TYPE_WR, A14_NT36672C_206, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_207, DSI_PKT_TYPE_WR, A14_NT36672C_207, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_208, DSI_PKT_TYPE_WR, A14_NT36672C_208, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_209, DSI_PKT_TYPE_WR, A14_NT36672C_209, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_210, DSI_PKT_TYPE_WR, A14_NT36672C_210, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_211, DSI_PKT_TYPE_WR, A14_NT36672C_211, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_212, DSI_PKT_TYPE_WR, A14_NT36672C_212, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_213, DSI_PKT_TYPE_WR, A14_NT36672C_213, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_214, DSI_PKT_TYPE_WR, A14_NT36672C_214, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_215, DSI_PKT_TYPE_WR, A14_NT36672C_215, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_216, DSI_PKT_TYPE_WR, A14_NT36672C_216, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_217, DSI_PKT_TYPE_WR, A14_NT36672C_217, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_218, DSI_PKT_TYPE_WR, A14_NT36672C_218, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_219, DSI_PKT_TYPE_WR, A14_NT36672C_219, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_220, DSI_PKT_TYPE_WR, A14_NT36672C_220, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_221, DSI_PKT_TYPE_WR, A14_NT36672C_221, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_222, DSI_PKT_TYPE_WR, A14_NT36672C_222, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_223, DSI_PKT_TYPE_WR, A14_NT36672C_223, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_224, DSI_PKT_TYPE_WR, A14_NT36672C_224, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_225, DSI_PKT_TYPE_WR, A14_NT36672C_225, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_226, DSI_PKT_TYPE_WR, A14_NT36672C_226, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_227, DSI_PKT_TYPE_WR, A14_NT36672C_227, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_228, DSI_PKT_TYPE_WR, A14_NT36672C_228, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_229, DSI_PKT_TYPE_WR, A14_NT36672C_229, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_230, DSI_PKT_TYPE_WR, A14_NT36672C_230, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_231, DSI_PKT_TYPE_WR, A14_NT36672C_231, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_232, DSI_PKT_TYPE_WR, A14_NT36672C_232, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_233, DSI_PKT_TYPE_WR, A14_NT36672C_233, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_234, DSI_PKT_TYPE_WR, A14_NT36672C_234, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_235, DSI_PKT_TYPE_WR, A14_NT36672C_235, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_236, DSI_PKT_TYPE_WR, A14_NT36672C_236, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_237, DSI_PKT_TYPE_WR, A14_NT36672C_237, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_238, DSI_PKT_TYPE_WR, A14_NT36672C_238, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_239, DSI_PKT_TYPE_WR, A14_NT36672C_239, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_240, DSI_PKT_TYPE_WR, A14_NT36672C_240, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_241, DSI_PKT_TYPE_WR, A14_NT36672C_241, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_242, DSI_PKT_TYPE_WR, A14_NT36672C_242, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_243, DSI_PKT_TYPE_WR, A14_NT36672C_243, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_244, DSI_PKT_TYPE_WR, A14_NT36672C_244, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_245, DSI_PKT_TYPE_WR, A14_NT36672C_245, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_246, DSI_PKT_TYPE_WR, A14_NT36672C_246, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_247, DSI_PKT_TYPE_WR, A14_NT36672C_247, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_248, DSI_PKT_TYPE_WR, A14_NT36672C_248, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_249, DSI_PKT_TYPE_WR, A14_NT36672C_249, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_250, DSI_PKT_TYPE_WR, A14_NT36672C_250, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_251, DSI_PKT_TYPE_WR, A14_NT36672C_251, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_252, DSI_PKT_TYPE_WR, A14_NT36672C_252, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_253, DSI_PKT_TYPE_WR, A14_NT36672C_253, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_254, DSI_PKT_TYPE_WR, A14_NT36672C_254, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_255, DSI_PKT_TYPE_WR, A14_NT36672C_255, 0);
static DEFINE_STATIC_PACKET(a14_nt36672c_tianma_256, DSI_PKT_TYPE_WR, A14_NT36672C_256, 0);

static DEFINE_PANEL_MDELAY(a14_wait_display_off, 1 * 17); /* 1 frame */
static DEFINE_PANEL_MDELAY(a14_wait_sleep_in, 4 * 17); /* 4 frame */
static DEFINE_PANEL_MDELAY(a14_wait_2msec, 2);
static DEFINE_PANEL_MDELAY(a14_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(a14_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(a14_wait_blic_off, 1);


static void *a14_init_cmdtbl[] = {
	&nt36672c_tianma_restbl[RES_ID],
	&PKTINFO(a14_nt36672c_tianma_001),
	&PKTINFO(a14_nt36672c_tianma_002),
	&PKTINFO(a14_nt36672c_tianma_003),
	&PKTINFO(a14_nt36672c_tianma_004),
	&PKTINFO(a14_nt36672c_tianma_005),
	&PKTINFO(a14_nt36672c_tianma_006),
	&PKTINFO(a14_nt36672c_tianma_007),
	&PKTINFO(a14_nt36672c_tianma_008),
	&PKTINFO(a14_nt36672c_tianma_009),
	&PKTINFO(a14_nt36672c_tianma_010),
	&PKTINFO(a14_nt36672c_tianma_011),
	&PKTINFO(a14_nt36672c_tianma_012),
	&PKTINFO(a14_nt36672c_tianma_013),
	&PKTINFO(a14_nt36672c_tianma_014),
	&PKTINFO(a14_nt36672c_tianma_015),
	&PKTINFO(a14_nt36672c_tianma_016),
	&PKTINFO(a14_nt36672c_tianma_017),
	&PKTINFO(a14_nt36672c_tianma_018),
	&PKTINFO(a14_nt36672c_tianma_019),
	&PKTINFO(a14_nt36672c_tianma_020),
	&PKTINFO(a14_nt36672c_tianma_021),
	&PKTINFO(a14_nt36672c_tianma_022),
	&PKTINFO(a14_nt36672c_tianma_023),
	&PKTINFO(a14_nt36672c_tianma_024),
	&PKTINFO(a14_nt36672c_tianma_025),
	&PKTINFO(a14_nt36672c_tianma_026),
	&PKTINFO(a14_nt36672c_tianma_027),
	&PKTINFO(a14_nt36672c_tianma_028),
	&PKTINFO(a14_nt36672c_tianma_029),
	&PKTINFO(a14_nt36672c_tianma_030),
	&PKTINFO(a14_nt36672c_tianma_031),
	&PKTINFO(a14_nt36672c_tianma_032),
	&PKTINFO(a14_nt36672c_tianma_033),
	&PKTINFO(a14_nt36672c_tianma_034),
	&PKTINFO(a14_nt36672c_tianma_035),
	&PKTINFO(a14_nt36672c_tianma_036),
	&PKTINFO(a14_nt36672c_tianma_037),
	&PKTINFO(a14_nt36672c_tianma_038),
	&PKTINFO(a14_nt36672c_tianma_039),
	&PKTINFO(a14_nt36672c_tianma_040),
	&PKTINFO(a14_nt36672c_tianma_041),
	&PKTINFO(a14_nt36672c_tianma_042),
	&PKTINFO(a14_nt36672c_tianma_043),
	&PKTINFO(a14_nt36672c_tianma_044),
	&PKTINFO(a14_nt36672c_tianma_045),
	&PKTINFO(a14_nt36672c_tianma_046),
	&PKTINFO(a14_nt36672c_tianma_047),
	&PKTINFO(a14_nt36672c_tianma_048),
	&PKTINFO(a14_nt36672c_tianma_049),
	&PKTINFO(a14_nt36672c_tianma_050),
	&PKTINFO(a14_nt36672c_tianma_051),
	&PKTINFO(a14_nt36672c_tianma_052),
	&PKTINFO(a14_nt36672c_tianma_053),
	&PKTINFO(a14_nt36672c_tianma_054),
	&PKTINFO(a14_nt36672c_tianma_055),
	&PKTINFO(a14_nt36672c_tianma_056),
	&PKTINFO(a14_nt36672c_tianma_057),
	&PKTINFO(a14_nt36672c_tianma_058),
	&PKTINFO(a14_nt36672c_tianma_059),
	&PKTINFO(a14_nt36672c_tianma_060),
	&PKTINFO(a14_nt36672c_tianma_061),
	&PKTINFO(a14_nt36672c_tianma_062),
	&PKTINFO(a14_nt36672c_tianma_063),
	&PKTINFO(a14_nt36672c_tianma_064),
	&PKTINFO(a14_nt36672c_tianma_065),
	&PKTINFO(a14_nt36672c_tianma_066),
	&PKTINFO(a14_nt36672c_tianma_067),
	&PKTINFO(a14_nt36672c_tianma_068),
	&PKTINFO(a14_nt36672c_tianma_069),
	&PKTINFO(a14_nt36672c_tianma_070),
	&PKTINFO(a14_nt36672c_tianma_071),
	&PKTINFO(a14_nt36672c_tianma_072),
	&PKTINFO(a14_nt36672c_tianma_073),
	&PKTINFO(a14_nt36672c_tianma_074),
	&PKTINFO(a14_nt36672c_tianma_075),
	&PKTINFO(a14_nt36672c_tianma_076),
	&PKTINFO(a14_nt36672c_tianma_077),
	&PKTINFO(a14_nt36672c_tianma_078),
	&PKTINFO(a14_nt36672c_tianma_079),
	&PKTINFO(a14_nt36672c_tianma_080),
	&PKTINFO(a14_nt36672c_tianma_081),
	&PKTINFO(a14_nt36672c_tianma_082),
	&PKTINFO(a14_nt36672c_tianma_083),
	&PKTINFO(a14_nt36672c_tianma_084),
	&PKTINFO(a14_nt36672c_tianma_085),
	&PKTINFO(a14_nt36672c_tianma_086),
	&PKTINFO(a14_nt36672c_tianma_087),
	&PKTINFO(a14_nt36672c_tianma_088),
	&PKTINFO(a14_nt36672c_tianma_089),
	&PKTINFO(a14_nt36672c_tianma_090),
	&PKTINFO(a14_nt36672c_tianma_091),
	&PKTINFO(a14_nt36672c_tianma_092),
	&PKTINFO(a14_nt36672c_tianma_093),
	&PKTINFO(a14_nt36672c_tianma_094),
	&PKTINFO(a14_nt36672c_tianma_095),
	&PKTINFO(a14_nt36672c_tianma_096),
	&PKTINFO(a14_nt36672c_tianma_097),
	&PKTINFO(a14_nt36672c_tianma_098),
	&PKTINFO(a14_nt36672c_tianma_099),
	&PKTINFO(a14_nt36672c_tianma_100),
	&PKTINFO(a14_nt36672c_tianma_101),
	&PKTINFO(a14_nt36672c_tianma_102),
	&PKTINFO(a14_nt36672c_tianma_103),
	&PKTINFO(a14_nt36672c_tianma_104),
	&PKTINFO(a14_nt36672c_tianma_105),
	&PKTINFO(a14_nt36672c_tianma_106),
	&PKTINFO(a14_nt36672c_tianma_107),
	&PKTINFO(a14_nt36672c_tianma_108),
	&PKTINFO(a14_nt36672c_tianma_109),
	&PKTINFO(a14_nt36672c_tianma_110),
	&PKTINFO(a14_nt36672c_tianma_111),
	&PKTINFO(a14_nt36672c_tianma_112),
	&PKTINFO(a14_nt36672c_tianma_113),
	&PKTINFO(a14_nt36672c_tianma_114),
	&PKTINFO(a14_nt36672c_tianma_115),
	&PKTINFO(a14_nt36672c_tianma_116),
	&PKTINFO(a14_nt36672c_tianma_117),
	&PKTINFO(a14_nt36672c_tianma_118),
	&PKTINFO(a14_nt36672c_tianma_119),
	&PKTINFO(a14_nt36672c_tianma_120),
	&PKTINFO(a14_nt36672c_tianma_121),
	&PKTINFO(a14_nt36672c_tianma_122),
	&PKTINFO(a14_nt36672c_tianma_123),
	&PKTINFO(a14_nt36672c_tianma_124),
	&PKTINFO(a14_nt36672c_tianma_125),
	&PKTINFO(a14_nt36672c_tianma_126),
	&PKTINFO(a14_nt36672c_tianma_127),
	&PKTINFO(a14_nt36672c_tianma_128),
	&PKTINFO(a14_nt36672c_tianma_129),
	&PKTINFO(a14_nt36672c_tianma_130),
	&PKTINFO(a14_nt36672c_tianma_131),
	&PKTINFO(a14_nt36672c_tianma_132),
	&PKTINFO(a14_nt36672c_tianma_133),
	&PKTINFO(a14_nt36672c_tianma_134),
	&PKTINFO(a14_nt36672c_tianma_135),
	&PKTINFO(a14_nt36672c_tianma_136),
	&PKTINFO(a14_nt36672c_tianma_137),
	&PKTINFO(a14_nt36672c_tianma_138),
	&PKTINFO(a14_nt36672c_tianma_139),
	&PKTINFO(a14_nt36672c_tianma_140),
	&PKTINFO(a14_nt36672c_tianma_141),
	&PKTINFO(a14_nt36672c_tianma_142),
	&PKTINFO(a14_nt36672c_tianma_143),
	&PKTINFO(a14_nt36672c_tianma_144),
	&PKTINFO(a14_nt36672c_tianma_145),
	&PKTINFO(a14_nt36672c_tianma_146),
	&PKTINFO(a14_nt36672c_tianma_147),
	&PKTINFO(a14_nt36672c_tianma_148),
	&PKTINFO(a14_nt36672c_tianma_149),
	&PKTINFO(a14_nt36672c_tianma_150),
	&PKTINFO(a14_nt36672c_tianma_151),
	&PKTINFO(a14_nt36672c_tianma_152),
	&PKTINFO(a14_nt36672c_tianma_153),
	&PKTINFO(a14_nt36672c_tianma_154),
	&PKTINFO(a14_nt36672c_tianma_155),
	&PKTINFO(a14_nt36672c_tianma_156),
	&PKTINFO(a14_nt36672c_tianma_157),
	&PKTINFO(a14_nt36672c_tianma_158),
	&PKTINFO(a14_nt36672c_tianma_159),
	&PKTINFO(a14_nt36672c_tianma_160),
	&PKTINFO(a14_nt36672c_tianma_161),
	&PKTINFO(a14_nt36672c_tianma_162),
	&PKTINFO(a14_nt36672c_tianma_163),
	&PKTINFO(a14_nt36672c_tianma_164),
	&PKTINFO(a14_nt36672c_tianma_165),
	&PKTINFO(a14_nt36672c_tianma_166),
	&PKTINFO(a14_nt36672c_tianma_167),
	&PKTINFO(a14_nt36672c_tianma_168),
	&PKTINFO(a14_nt36672c_tianma_169),
	&PKTINFO(a14_nt36672c_tianma_170),
	&PKTINFO(a14_nt36672c_tianma_171),
	&PKTINFO(a14_nt36672c_tianma_172),
	&PKTINFO(a14_nt36672c_tianma_173),
	&PKTINFO(a14_nt36672c_tianma_174),
	&PKTINFO(a14_nt36672c_tianma_175),
	&PKTINFO(a14_nt36672c_tianma_176),
	&PKTINFO(a14_nt36672c_tianma_177),
	&PKTINFO(a14_nt36672c_tianma_178),
	&PKTINFO(a14_nt36672c_tianma_179),
	&PKTINFO(a14_nt36672c_tianma_180),
	&PKTINFO(a14_nt36672c_tianma_181),
	&PKTINFO(a14_nt36672c_tianma_182),
	&PKTINFO(a14_nt36672c_tianma_183),
	&PKTINFO(a14_nt36672c_tianma_184),
	&PKTINFO(a14_nt36672c_tianma_185),
	&PKTINFO(a14_nt36672c_tianma_186),
	&PKTINFO(a14_nt36672c_tianma_187),
	&PKTINFO(a14_nt36672c_tianma_188),
	&PKTINFO(a14_nt36672c_tianma_189),
	&PKTINFO(a14_nt36672c_tianma_190),
	&PKTINFO(a14_nt36672c_tianma_191),
	&PKTINFO(a14_nt36672c_tianma_192),
	&PKTINFO(a14_nt36672c_tianma_193),
	&PKTINFO(a14_nt36672c_tianma_194),
	&PKTINFO(a14_nt36672c_tianma_195),
	&PKTINFO(a14_nt36672c_tianma_196),
	&PKTINFO(a14_nt36672c_tianma_197),
	&PKTINFO(a14_nt36672c_tianma_198),
	&PKTINFO(a14_nt36672c_tianma_199),
	&PKTINFO(a14_nt36672c_tianma_200),
	&PKTINFO(a14_nt36672c_tianma_201),
	&PKTINFO(a14_nt36672c_tianma_202),
	&PKTINFO(a14_nt36672c_tianma_203),
	&PKTINFO(a14_nt36672c_tianma_204),
	&PKTINFO(a14_nt36672c_tianma_205),
	&PKTINFO(a14_nt36672c_tianma_206),
	&PKTINFO(a14_nt36672c_tianma_207),
	&PKTINFO(a14_nt36672c_tianma_208),
	&PKTINFO(a14_nt36672c_tianma_209),
	&PKTINFO(a14_nt36672c_tianma_210),
	&PKTINFO(a14_nt36672c_tianma_211),
	&PKTINFO(a14_nt36672c_tianma_212),
	&PKTINFO(a14_nt36672c_tianma_213),
	&PKTINFO(a14_nt36672c_tianma_214),
	&PKTINFO(a14_nt36672c_tianma_215),
	&PKTINFO(a14_nt36672c_tianma_216),
	&PKTINFO(a14_nt36672c_tianma_217),
	&PKTINFO(a14_nt36672c_tianma_218),
	&PKTINFO(a14_nt36672c_tianma_219),
	&PKTINFO(a14_nt36672c_tianma_220),
	&PKTINFO(a14_nt36672c_tianma_221),
	&PKTINFO(a14_nt36672c_tianma_222),
	&PKTINFO(a14_nt36672c_tianma_223),
	&PKTINFO(a14_nt36672c_tianma_224),
	&PKTINFO(a14_nt36672c_tianma_225),
	&PKTINFO(a14_nt36672c_tianma_226),
	&PKTINFO(a14_nt36672c_tianma_227),
	&PKTINFO(a14_nt36672c_tianma_228),
	&PKTINFO(a14_nt36672c_tianma_229),
	&PKTINFO(a14_nt36672c_tianma_230),
	&PKTINFO(a14_nt36672c_tianma_231),
	&PKTINFO(a14_nt36672c_tianma_232),
	&PKTINFO(a14_nt36672c_tianma_233),
	&PKTINFO(a14_nt36672c_tianma_234),
	&PKTINFO(a14_nt36672c_tianma_235),
	&PKTINFO(a14_nt36672c_tianma_236),
	&PKTINFO(a14_nt36672c_tianma_237),
	&PKTINFO(a14_nt36672c_tianma_238),
	&PKTINFO(a14_nt36672c_tianma_239),
	&PKTINFO(a14_nt36672c_tianma_240),
	&PKTINFO(a14_nt36672c_tianma_241),
	&PKTINFO(a14_nt36672c_tianma_242),
	&PKTINFO(a14_nt36672c_tianma_243),
	&PKTINFO(a14_nt36672c_tianma_244),
	&PKTINFO(a14_nt36672c_tianma_245),
	&PKTINFO(a14_nt36672c_tianma_246),
	&PKTINFO(a14_nt36672c_tianma_247),
	&PKTINFO(a14_nt36672c_tianma_248),
	&PKTINFO(a14_nt36672c_tianma_249),
	&PKTINFO(a14_nt36672c_tianma_250),
	&PKTINFO(a14_nt36672c_tianma_251),
	&PKTINFO(a14_nt36672c_tianma_252),
	&PKTINFO(a14_nt36672c_tianma_253),
	&PKTINFO(a14_nt36672c_tianma_254),
	&PKTINFO(a14_nt36672c_tianma_255),
	&PKTINFO(a14_nt36672c_tianma_256),

	&PKTINFO(a14_sleep_out),
	&DLYINFO(a14_wait_100msec),
};

static void *a14_res_init_cmdtbl[] = {
	&nt36672c_tianma_restbl[RES_ID],
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

static DEFINE_STATIC_PACKET(nt36672c_tianma_a14_i2c_init, I2C_PKT_TYPE_WR, NT36672C_A14_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(nt36672c_tianma_a14_i2c_exit_blen, I2C_PKT_TYPE_WR, NT36672C_A14_I2C_EXIT_BLEN, 0);
static DEFINE_STATIC_PACKET(nt36672c_tianma_a14_i2c_dump, I2C_PKT_TYPE_RD, NT36672C_A14_I2C_DUMP, 0);

static void *nt36672c_tianma_a14_init_cmdtbl[] = {
	&PKTINFO(nt36672c_tianma_a14_i2c_init),
};

static void *nt36672c_tianma_a14_exit_cmdtbl[] = {
	&PKTINFO(nt36672c_tianma_a14_i2c_exit_blen),
};

static void *nt36672c_tianma_a14_dump_cmdtbl[] = {
	&PKTINFO(nt36672c_tianma_a14_i2c_dump),
};


static struct seqinfo a14_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a14_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a14_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a14_set_bl_cmdtbl),
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-on-seq", a14_display_on_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a14_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a14_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", nt36672c_tianma_a14_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", nt36672c_tianma_a14_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", nt36672c_tianma_a14_dump_cmdtbl),
#endif
};

struct common_panel_info nt36672c_tianma_a14_default_panel_info = {
	.ldi_name = "nt36672c_tianma",
	.name = "nt36672c_tianma_a14_default",
	.model = "TIANMA_6_517_inch",
	.vendor = "TMC",
	.id = 0x5B7240,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = true,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(nt36672c_tianma_a14_resol),
		.resol = nt36672c_tianma_a14_resol,
	},
	.maptbl = a14_maptbl,
	.nr_maptbl = ARRAY_SIZE(a14_maptbl),
	.seqtbl = a14_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a14_seqtbl),
	.rditbl = nt36672c_tianma_rditbl,
	.nr_rditbl = ARRAY_SIZE(nt36672c_tianma_rditbl),
	.restbl = nt36672c_tianma_restbl,
	.nr_restbl = ARRAY_SIZE(nt36672c_tianma_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&nt36672c_tianma_a14_panel_dimming_info,
	},
	.i2c_data = &nt36672c_tianma_a14_i2c_data,
};

int __init nt36672c_tianma_a14_panel_init(void)
{
	register_common_panel(&nt36672c_tianma_a14_default_panel_info);

	return 0;
}
//arch_initcall(nt36672c_tianma_a14_panel_init)
#endif /* __NT36672C_A14_PANEL_H__ */

