/*
 * linux/drivers/video/fbdev/exynos/panel/nt36525/nt36525_xcover5_panel.h
 *
 * Header file for NT36525 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NT36525_XCOVER5_PANEL_H__
#define __NT36525_XCOVER5_PANEL_H__
#include "../panel_drv.h"
#include "nt36525.h"

#include "nt36525_xcover5_panel_dimming.h"
#include "nt36525_xcover5_panel_i2c.h"

#include "nt36525_xcover5_resol.h"

#undef __pn_name__
#define __pn_name__	xcover5

#undef __PN_NAME__
#define __PN_NAME__	XCOVER5

static struct seqinfo xcover5_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [NT36525 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [NT36525 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [NT36525 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 xcover5_brt_table[NT36525_TOTAL_NR_LUMINANCE][1] = {
	{0},
	{1}, {1}, {2}, {3}, {3}, {4}, {5}, {5}, {6}, {7}, /* 1 : 1 */
	{7}, {8}, {9}, {9}, {10}, {11}, {11}, {12}, {13}, {14},
	{14}, {15}, {16}, {16}, {17}, {18}, {18}, {19}, {20}, {20},
	{21}, {22}, {22}, {23}, {23}, {24}, {24}, {25}, {26}, {27},
	{27}, {28}, {28}, {29}, {30}, {30}, {31}, {31}, {32}, {33},
	{33}, {34}, {34}, {35}, {36}, {36}, {37}, {37}, {38}, {39},
	{39}, {40}, {40}, {41}, {42}, {42}, {43}, {43}, {44}, {45},
	{45}, {46}, {46}, {47}, {48}, {48}, {49}, {49}, {50}, {51},
	{51}, {52}, {52}, {53}, {54}, {54}, {55}, {55}, {56}, {56},
	{57}, {58}, {58}, {59}, {59}, {60}, {61}, {61}, {62}, {62},
	{63}, {64}, {64}, {65}, {65}, {66}, {67}, {67}, {68}, {68},
	{69}, {70}, {70}, {71}, {71}, {72}, {73}, {73}, {74}, {74},
	{75}, {76}, {77}, {78}, {79}, {80}, {81}, {82}, {83}, {84},	/* 128 : 82 */
	{85}, {86}, {87}, {88}, {89}, {90}, {91}, {92}, {93}, {94},
	{95}, {96}, {97}, {98}, {99}, {100}, {101}, {102}, {103}, {104},
	{105}, {106}, {107}, {108}, {109}, {110}, {111}, {112}, {113}, {114},
	{115}, {116}, {117}, {118}, {119}, {120}, {121}, {122}, {123}, {124},
	{125}, {126}, {127}, {128}, {129}, {130}, {131}, {132}, {133}, {134},
	{135}, {136}, {137}, {138}, {139}, {140}, {141}, {142}, {143}, {144},
	{145}, {146}, {147}, {148}, {149}, {150}, {151}, {152}, {153}, {154},
	{155}, {157}, {158}, {159}, {160}, {161}, {162}, {163}, {164}, {165},
	{166}, {167}, {168}, {169}, {170}, {171}, {172}, {173}, {174}, {175},
	{176}, {177}, {178}, {179}, {180}, {181}, {182}, {183}, {184}, {185},
	{186}, {187}, {188}, {189}, {191}, {192}, {193}, {194}, {195}, {196},
	{197}, {198}, {199}, {200}, {201}, {202}, {203}, {204}, {205}, {206},
	{207}, {208}, {209}, {210}, {211}, {212}, {213}, {214}, {215}, {215},		/* 255: 211 */
	{216}, {217}, {218}, {219}, {220}, {220}, {221}, {222}, {223}, {224},
	{225}, {225}, {226}, {227}, {228}, {229}, {230}, {231}, {231}, {232},
	{233}, {234}, {235}, {235}, {236}, {237}, {238}, {239}, {239}, {240},
	{241}, {242}, {243}, {243}, {244}, {245}, {246}, {247}, {248}, {249},
	{250}, {251}, {252}, {253}, {254}, {255}, /* 306: 255 */
};


static struct maptbl xcover5_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(xcover5_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [NT36525 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 XCOVER5_SLEEP_OUT[] = { 0x11 };
static u8 XCOVER5_SLEEP_IN[] = { 0x10 };
static u8 XCOVER5_DISPLAY_OFF[] = { 0x28 };
static u8 XCOVER5_DISPLAY_ON[] = { 0x29 };

static u8 XCOVER5_BRIGHTNESS[] = {
	0x51,
	0xFF,
};

static u8 SEQ_NT36525_01[] = {
	0xFF,
	0x20,
};

static u8 SEQ_NT36525_02[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36525_03[] = {
	0x00,
	0x01,
};

static u8 SEQ_NT36525_04[] = {
	0x01,
	0x55,
};

static u8 SEQ_NT36525_05[] = {
	0x03,
	0x55,
};

static u8 SEQ_NT36525_06[] = {
	0x05,
	0xA9,
};

static u8 SEQ_NT36525_07[] = {
	0x07,
	0x73,
};

static u8 SEQ_NT36525_08[] = {
	0x08,
	0xC1,
};

static u8 SEQ_NT36525_09[] = {
	0x0E,
	0x91,
};

static u8 SEQ_NT36525_10[] = {
	0x0F,
	0x5F,
};

static u8 SEQ_NT36525_11[] = {
	0x1F,
	0x00,
};

static u8 SEQ_NT36525_12[] = {
	0x69,
	0xA9,
};

static u8 SEQ_NT36525_13[] = {
	0x94,
	0x00,
};

static u8 SEQ_NT36525_14[] = {
	0x95,
	0xEB,
};

static u8 SEQ_NT36525_15[] = {
	0x96,
	0xEB,
};

static u8 SEQ_NT36525_16[] = {
	0xFF,
	0x24,
};

static u8 SEQ_NT36525_17[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36525_18[] = {
	0x00,
	0x00,
};

static u8 SEQ_NT36525_19[] = {
	0x04,
	0x21,
};

static u8 SEQ_NT36525_20[] = {
	0x06,
	0x22,
};

static u8 SEQ_NT36525_21[] = {
	0x07,
	0x20,
};

static u8 SEQ_NT36525_22[] = {
	0x08,
	0x1D,
};

static u8 SEQ_NT36525_23[] = {
	0x0A,
	0x0C,
};

static u8 SEQ_NT36525_24[] = {
	0x0B,
	0x0D,
};

static u8 SEQ_NT36525_25[] = {
	0x0C,
	0x0E,
};

static u8 SEQ_NT36525_26[] = {
	0x0D,
	0x0F,
};

static u8 SEQ_NT36525_27[] = {
	0x0F,
	0x04,
};

static u8 SEQ_NT36525_28[] = {
	0x10,
	0x05,
};

static u8 SEQ_NT36525_29[] = {
	0x12,
	0x1E,
};

static u8 SEQ_NT36525_30[] = {
	0x13,
	0x1E,
};

static u8 SEQ_NT36525_31[] = {
	0x14,
	0x1E,
};

static u8 SEQ_NT36525_32[] = {
	0x16,
	0x00,
};

static u8 SEQ_NT36525_33[] = {
	0x1A,
	0x21,
};

static u8 SEQ_NT36525_34[] = {
	0x1C,
	0x22,
};

static u8 SEQ_NT36525_35[] = {
	0x1D,
	0x20,
};

static u8 SEQ_NT36525_36[] = {
	0x1E,
	0x1D,
};

static u8 SEQ_NT36525_37[] = {
	0x20,
	0x0C,
};

static u8 SEQ_NT36525_38[] = {
	0x21,
	0x0D,
};

static u8 SEQ_NT36525_39[] = {
	0x22,
	0x0E,
};

static u8 SEQ_NT36525_40[] = {
	0x23,
	0x0F,
};

static u8 SEQ_NT36525_41[] = {
	0x25,
	0x04,
};

static u8 SEQ_NT36525_42[] = {
	0x26,
	0x05,
};

static u8 SEQ_NT36525_43[] = {
	0x28,
	0x1E,
};

static u8 SEQ_NT36525_44[] = {
	0x29,
	0x1E,
};

static u8 SEQ_NT36525_45[] = {
	0x2A,
	0x1E,
};

static u8 SEQ_NT36525_46[] = {
	0x2F,
	0x09,
};

static u8 SEQ_NT36525_47[] = {
	0x30,
	0x07,
};

static u8 SEQ_NT36525_48[] = {
	0x33,
	0x07,
};

static u8 SEQ_NT36525_49[] = {
	0x34,
	0x09,
};

static u8 SEQ_NT36525_50[] = {
	0x37,
	0x33,
};

static u8 SEQ_NT36525_51[] = {
	0x39,
	0x00,
};

static u8 SEQ_NT36525_52[] = {
	0x3A,
	0x05,
};

static u8 SEQ_NT36525_53[] = {
	0x3B,
	0x94,
};

static u8 SEQ_NT36525_54[] = {
	0x3D,
	0x92,
};

static u8 SEQ_NT36525_55[] = {
	0x4D,
	0x12,
};

static u8 SEQ_NT36525_56[] = {
	0x4E,
	0x34,
};

static u8 SEQ_NT36525_57[] = {
	0x51,
	0x43,
};

static u8 SEQ_NT36525_58[] = {
	0x52,
	0x21,
};

static u8 SEQ_NT36525_59[] = {
	0x55,
	0x86,
};

static u8 SEQ_NT36525_60[] = {
	0x56,
	0x34,
};

static u8 SEQ_NT36525_61[] = {
	0x5A,
	0x05,
};

static u8 SEQ_NT36525_62[] = {
	0x5B,
	0x94,
};

static u8 SEQ_NT36525_63[] = {
	0x5C,
	0xC0,
};

static u8 SEQ_NT36525_64[] = {
	0x5D,
	0x06,
};

static u8 SEQ_NT36525_65[] = {
	0x5E,
	0x0C,
};

static u8 SEQ_NT36525_66[] = {
	0x5F,
	0x00,
};

static u8 SEQ_NT36525_67[] = {
	0x60,
	0x80,
};

static u8 SEQ_NT36525_68[] = {
	0x61,
	0x72,
};

static u8 SEQ_NT36525_69[] = {
	0x64,
	0x11,
};

static u8 SEQ_NT36525_70[] = {
	0x82,
	0x0A,
};

static u8 SEQ_NT36525_71[] = {
	0x83,
	0x0C,
};

static u8 SEQ_NT36525_72[] = {
	0x85,
	0x00,
};

static u8 SEQ_NT36525_73[] = {
	0x86,
	0x51,
};

static u8 SEQ_NT36525_74[] = {
	0x92,
	0xBA,
};

static u8 SEQ_NT36525_75[] = {
	0x93,
	0x12,
};

static u8 SEQ_NT36525_76[] = {
	0x94,
	0x0A,
};

static u8 SEQ_NT36525_77[] = {
	0xAB,
	0x00,
};

static u8 SEQ_NT36525_78[] = {
	0xAC,
	0x00,
};

static u8 SEQ_NT36525_79[] = {
	0xAD,
	0x00,
};

static u8 SEQ_NT36525_80[] = {
	0xAF,
	0x04,
};

static u8 SEQ_NT36525_81[] = {
	0xB0,
	0x05,
};

static u8 SEQ_NT36525_82[] = {
	0xB1,
	0xB5,
};

static u8 SEQ_NT36525_83[] = {
	0xFF,
	0x25,
};

static u8 SEQ_NT36525_84[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36525_85[] = {
	0x0A,
	0x82,
};

static u8 SEQ_NT36525_86[] = {
	0x0B,
	0x50,
};

static u8 SEQ_NT36525_87[] = {
	0x0C,
	0x01,
};

static u8 SEQ_NT36525_88[] = {
	0x17,
	0x82,
};

static u8 SEQ_NT36525_89[] = {
	0x18,
	0x06,
};

static u8 SEQ_NT36525_90[] = {
	0x19,
	0x0F,
};

static u8 SEQ_NT36525_91[] = {
	0x58,
	0x00,
};

static u8 SEQ_NT36525_92[] = {
	0x59,
	0x00,
};

static u8 SEQ_NT36525_93[] = {
	0x5A,
	0x40,
};

static u8 SEQ_NT36525_94[] = {
	0x5B,
	0x80,
};

static u8 SEQ_NT36525_95[] = {
	0x5C,
	0x00,
};

static u8 SEQ_NT36525_96[] = {
	0x5D,
	0x05,
};

static u8 SEQ_NT36525_97[] = {
	0x5E,
	0x94,
};

static u8 SEQ_NT36525_98[] = {
	0x5F,
	0x05,
};

static u8 SEQ_NT36525_99[] = {
	0x60,
	0x94,
};

static u8 SEQ_NT36525_100[] = {
	0x61,
	0x05,
};

static u8 SEQ_NT36525_101[] = {
	0x62,
	0x94,
};

static u8 SEQ_NT36525_102[] = {
	0x65,
	0x05,
};

static u8 SEQ_NT36525_103[] = {
	0x66,
	0xB5,
};

static u8 SEQ_NT36525_104[] = {
	0xC0,
	0x03,
};

static u8 SEQ_NT36525_105[] = {
	0xCA,
	0x1C,
};

static u8 SEQ_NT36525_106[] = {
	0xCC,
	0x1C,
};

static u8 SEQ_NT36525_107[] = {
	0xD3,
	0x11,
};

static u8 SEQ_NT36525_108[] = {
	0xD4,
	0xC8,
};

static u8 SEQ_NT36525_109[] = {
	0xD5,
	0x11,
};

static u8 SEQ_NT36525_110[] = {
	0xD6,
	0x1C,
};

static u8 SEQ_NT36525_111[] = {
	0xD7,
	0x11,
};

static u8 SEQ_NT36525_112[] = {
	0xFF,
	0x26,
};

static u8 SEQ_NT36525_113[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36525_114[] = {
	0x00,
	0xA0,
};

static u8 SEQ_NT36525_115[] = {
	0xFF,
	0x27,
};

static u8 SEQ_NT36525_116[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36525_117[] = {
	0x13,
	0x00,
};

static u8 SEQ_NT36525_118[] = {
	0x15,
	0xB4,
};

static u8 SEQ_NT36525_119[] = {
	0x1F,
	0x55,
};

static u8 SEQ_NT36525_120[] = {
	0x26,
	0x0F,
};

static u8 SEQ_NT36525_121[] = {
	0xC0,
	0x18,
};

static u8 SEQ_NT36525_122[] = {
	0xC1,
	0xE0,
};

static u8 SEQ_NT36525_123[] = {
	0xC2,
	0x00,
};

static u8 SEQ_NT36525_124[] = {
	0xC3,
	0x00,
};

static u8 SEQ_NT36525_125[] = {
	0xC4,
	0xE0,
};

static u8 SEQ_NT36525_126[] = {
	0xC5,
	0x00,
};

static u8 SEQ_NT36525_127[] = {
	0xC6,
	0x00,
};

static u8 SEQ_NT36525_128[] = {
	0xC7,
	0x03,
};

static u8 SEQ_NT36525_129[] = {
	0xFF,
	0x23,
};

static u8 SEQ_NT36525_130[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36525_131[] = {
	0x07,
	0x20,
};

static u8 SEQ_NT36525_132[] = {
	0x08,
	0x10,
};

static u8 SEQ_NT36525_133[] = {
	0x12,
	0xA6,
};

static u8 SEQ_NT36525_134[] = {
	0x15,
	0xFC,
};

static u8 SEQ_NT36525_135[] = {
	0x16,
	0x0B,
};

static u8 SEQ_NT36525_136[] = {
	0x04,
	0x00,
};

static u8 SEQ_NT36525_137[] = {
	0x19,
	0x00,
};

static u8 SEQ_NT36525_138[] = {
	0x1A,
	0x00,
};

static u8 SEQ_NT36525_139[] = {
	0x1B,
	0x08,
};

static u8 SEQ_NT36525_140[] = {
	0x1C,
	0x0A,
};

static u8 SEQ_NT36525_141[] = {
	0x1D,
	0x0C,
};

static u8 SEQ_NT36525_142[] = {
	0x1E,
	0x12,
};

static u8 SEQ_NT36525_143[] = {
	0x1F,
	0x16,
};

static u8 SEQ_NT36525_144[] = {
	0x20,
	0x1A,
};

static u8 SEQ_NT36525_145[] = {
	0x21,
	0x1C,
};

static u8 SEQ_NT36525_146[] = {
	0x22,
	0x20,
};

static u8 SEQ_NT36525_147[] = {
	0x23,
	0x24,
};

static u8 SEQ_NT36525_148[] = {
	0x24,
	0x28,
};

static u8 SEQ_NT36525_149[] = {
	0x25,
	0x2C,
};

static u8 SEQ_NT36525_150[] = {
	0x26,
	0x30,
};

static u8 SEQ_NT36525_151[] = {
	0x27,
	0x38,
};

static u8 SEQ_NT36525_152[] = {
	0x28,
	0x3C,
};

static u8 SEQ_NT36525_153[] = {
	0x29,
	0x10,
};

static u8 SEQ_NT36525_154[] = {
	0x30,
	0xFF,
};

static u8 SEQ_NT36525_155[] = {
	0x31,
	0xFF,
};

static u8 SEQ_NT36525_156[] = {
	0x32,
	0xFE,
};

static u8 SEQ_NT36525_157[] = {
	0x33,
	0xFD,
};

static u8 SEQ_NT36525_158[] = {
	0x34,
	0xFD,
};

static u8 SEQ_NT36525_159[] = {
	0x35,
	0xFC,
};

static u8 SEQ_NT36525_160[] = {
	0x36,
	0xFB,
};

static u8 SEQ_NT36525_161[] = {
	0x37,
	0xF9,
};

static u8 SEQ_NT36525_162[] = {
	0x38,
	0xF7,
};

static u8 SEQ_NT36525_163[] = {
	0x39,
	0xF3,
};

static u8 SEQ_NT36525_164[] = {
	0x3A,
	0xEA,
};

static u8 SEQ_NT36525_165[] = {
	0x3B,
	0xE6,
};

static u8 SEQ_NT36525_166[] = {
	0x3D,
	0xE0,
};

static u8 SEQ_NT36525_167[] = {
	0x3F,
	0xDD,
};

static u8 SEQ_NT36525_168[] = {
	0x40,
	0xDB,
};

static u8 SEQ_NT36525_169[] = {
	0x41,
	0xD9,
};

static u8 SEQ_NT36525_170[] = {
	0xFF,
	0x20,
};

static u8 SEQ_NT36525_171[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36525_172[] = {
	0xB0,
	0x00, 0x08, 0x00, 0x18, 0x00, 0x34, 0x00, 0x4C, 0x00, 0x61,
	0x00, 0x74, 0x00, 0x86, 0x00, 0x96,
};

static u8 SEQ_NT36525_173[] = {
	0xB1,
	0x00, 0xA5, 0x00, 0xDB, 0x01, 0x02, 0x01, 0x43, 0x01, 0x71,
	0x01, 0xBD, 0x01, 0xF7, 0x01, 0xFA,
};

static u8 SEQ_NT36525_174[] = {
	0xB2,
	0x02, 0x34, 0x02, 0x6F, 0x02, 0x98, 0x02, 0xCC, 0x02, 0xF1,
	0x03, 0x1D, 0x03, 0x2D, 0x03, 0x3B,
};

static u8 SEQ_NT36525_175[] = {
	0xB3,
	0x03, 0x4D, 0x03, 0x60, 0x03, 0x7A, 0x03, 0x94, 0x03, 0xA6,
	0x03, 0xA7,
};

static u8 SEQ_NT36525_176[] = {
	0xB4,
	0x00, 0x08, 0x00, 0x18, 0x00, 0x34, 0x00, 0x4C, 0x00, 0x61,
	0x00, 0x74, 0x00, 0x86, 0x00, 0x96,
};

static u8 SEQ_NT36525_177[] = {
	0xB5,
	0x00, 0xA5, 0x00, 0xDB, 0x01, 0x02, 0x01, 0x43, 0x01, 0x71,
	0x01, 0xBD, 0x01, 0xF7, 0x01, 0xFA,
};

static u8 SEQ_NT36525_178[] = {
	0xB6,
	0x02, 0x34, 0x02, 0x6F, 0x02, 0x98, 0x02, 0xCC, 0x02, 0xF1,
	0x03, 0x1D, 0x03, 0x2D, 0x03, 0x3B,
};

static u8 SEQ_NT36525_179[] = {
	0xB7,
	0x03, 0x4D, 0x03, 0x60, 0x03, 0x7A, 0x03, 0x94, 0x03, 0xA6,
	0x03, 0xA7,
};

static u8 SEQ_NT36525_180[] = {
	0xB8,
	0x00, 0x08, 0x00, 0x18, 0x00, 0x34, 0x00, 0x4C, 0x00, 0x61,
	0x00, 0x74, 0x00, 0x86, 0x00, 0x96,
};

static u8 SEQ_NT36525_181[] = {
	0xB9,
	0x00, 0xA5, 0x00, 0xDB, 0x01, 0x02, 0x01, 0x43, 0x01, 0x71,
	0x01, 0xBD, 0x01, 0xF7, 0x01, 0xFA,
};

static u8 SEQ_NT36525_182[] = {
	0xBA,
	0x02, 0x34, 0x02, 0x6F, 0x02, 0x98, 0x02, 0xCC, 0x02, 0xF1,
	0x03, 0x1D, 0x03, 0x2D, 0x03, 0x3B,
};

static u8 SEQ_NT36525_183[] = {
	0xBB,
	0x03, 0x4D, 0x03, 0x60, 0x03, 0x7A, 0x03, 0x94, 0x03, 0xA6,
	0x03, 0xA7,
};

static u8 SEQ_NT36525_184[] = {
	0xFF,
	0x21,
};

static u8 SEQ_NT36525_185[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36525_186[] = {
	0xB0,
	0x00, 0x00, 0x00, 0x10, 0x00, 0x2C, 0x00, 0x44, 0x00, 0x59,
	0x00, 0x6C, 0x00, 0x7E, 0x00, 0x8E,
};

static u8 SEQ_NT36525_187[] = {
	0xB1,
	0x00, 0x9D, 0x00, 0xD3, 0x00, 0xFA, 0x01, 0x3B, 0x01, 0x69,
	0x01, 0xB5, 0x01, 0xEF, 0x01, 0xF2,
};

static u8 SEQ_NT36525_188[] = {
	0xB2,
	0x02, 0x2C, 0x02, 0x79, 0x02, 0xAA, 0x02, 0xE6, 0x03, 0x0F,
	0x03, 0x3F, 0x03, 0x4F, 0x03, 0x5F,
};

static u8 SEQ_NT36525_189[] = {
	0xB3,
	0x03, 0x71, 0x03, 0x86, 0x03, 0xA0, 0x03, 0xBC, 0x03, 0xCE,
	0x03, 0xD9,
};

static u8 SEQ_NT36525_190[] = {
	0xB4,
	0x00, 0x00, 0x00, 0x10, 0x00, 0x2C, 0x00, 0x44, 0x00, 0x59,
	0x00, 0x6C, 0x00, 0x7E, 0x00, 0x8E,
};

static u8 SEQ_NT36525_191[] = {
	0xB5,
	0x00, 0x9D, 0x00, 0xD3, 0x00, 0xFA, 0x01, 0x3B, 0x01, 0x69,
	0x01, 0xB5, 0x01, 0xEF, 0x01, 0xF2,
};

static u8 SEQ_NT36525_192[] = {
	0xB6,
	0x02, 0x2C, 0x02, 0x79, 0x02, 0xAA, 0x02, 0xE6, 0x03, 0x0F,
	0x03, 0x3F, 0x03, 0x4F, 0x03, 0x5F,
};

static u8 SEQ_NT36525_193[] = {
	0xB7,
	0x03, 0x71, 0x03, 0x86, 0x03, 0xA0, 0x03, 0xBC, 0x03, 0xCE,
	0x03, 0xD9,
};

static u8 SEQ_NT36525_194[] = {
	0xB8,
	0x00, 0x00, 0x00, 0x10, 0x00, 0x2C, 0x00, 0x44, 0x00, 0x59,
	0x00, 0x6C, 0x00, 0x7E, 0x00, 0x8E,
};

static u8 SEQ_NT36525_195[] = {
	0xB9,
	0x00, 0x9D, 0x00, 0xD3, 0x00, 0xFA, 0x01, 0x3B, 0x01, 0x69,
	0x01, 0xB5, 0x01, 0xEF, 0x01, 0xF2,
};

static u8 SEQ_NT36525_196[] = {
	0xBA,
	0x02, 0x2C, 0x02, 0x79, 0x02, 0xAA, 0x02, 0xE6, 0x03, 0x0F,
	0x03, 0x3F, 0x03, 0x4F, 0x03, 0x5F,
};

static u8 SEQ_NT36525_197[] = {
	0xBB,
	0x03, 0x71, 0x03, 0x86, 0x03, 0xA0, 0x03, 0xBC, 0x03, 0xCE,
	0x03, 0xD9,
};

static u8 SEQ_NT36525_198[] = {
	0xFF,
	0xD0,
};

static u8 SEQ_NT36525_199[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36525_200[] = {
	0x25,
	0xA9,
};

static u8 SEQ_NT36525_201[] = {
	0xFF,
	0x10,
};

static u8 SEQ_NT36525_202[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36525_203[] = {
	0xBA,
	0x02,
};

static u8 SEQ_NT36525_204[] = {
	0x55,
	0x01,
};

static u8 SEQ_NT36525_205[] = {
	0x53,
	0x2C,
};

static u8 SEQ_NT36525_206[] = {
	0x51,
	0x00, 0x00,
};

static u8 SEQ_NT36525_207[] = {
	0x68,
	0x00, 0x01,
};

static DEFINE_STATIC_PACKET(xcover5_sleep_out, DSI_PKT_TYPE_WR, XCOVER5_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(xcover5_sleep_in, DSI_PKT_TYPE_WR, XCOVER5_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(xcover5_display_on, DSI_PKT_TYPE_WR, XCOVER5_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(xcover5_display_off, DSI_PKT_TYPE_WR, XCOVER5_DISPLAY_OFF, 0);

static DEFINE_PKTUI(xcover5_brightness, &xcover5_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(xcover5_brightness, DSI_PKT_TYPE_WR, XCOVER5_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(seq_nt36525_01, DSI_PKT_TYPE_WR, SEQ_NT36525_01, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_02, DSI_PKT_TYPE_WR, SEQ_NT36525_02, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_03, DSI_PKT_TYPE_WR, SEQ_NT36525_03, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_04, DSI_PKT_TYPE_WR, SEQ_NT36525_04, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_05, DSI_PKT_TYPE_WR, SEQ_NT36525_05, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_06, DSI_PKT_TYPE_WR, SEQ_NT36525_06, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_07, DSI_PKT_TYPE_WR, SEQ_NT36525_07, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_08, DSI_PKT_TYPE_WR, SEQ_NT36525_08, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_09, DSI_PKT_TYPE_WR, SEQ_NT36525_09, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_10, DSI_PKT_TYPE_WR, SEQ_NT36525_10, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_11, DSI_PKT_TYPE_WR, SEQ_NT36525_11, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_12, DSI_PKT_TYPE_WR, SEQ_NT36525_12, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_13, DSI_PKT_TYPE_WR, SEQ_NT36525_13, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_14, DSI_PKT_TYPE_WR, SEQ_NT36525_14, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_15, DSI_PKT_TYPE_WR, SEQ_NT36525_15, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_16, DSI_PKT_TYPE_WR, SEQ_NT36525_16, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_17, DSI_PKT_TYPE_WR, SEQ_NT36525_17, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_18, DSI_PKT_TYPE_WR, SEQ_NT36525_18, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_19, DSI_PKT_TYPE_WR, SEQ_NT36525_19, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_20, DSI_PKT_TYPE_WR, SEQ_NT36525_20, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_21, DSI_PKT_TYPE_WR, SEQ_NT36525_21, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_22, DSI_PKT_TYPE_WR, SEQ_NT36525_22, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_23, DSI_PKT_TYPE_WR, SEQ_NT36525_23, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_24, DSI_PKT_TYPE_WR, SEQ_NT36525_24, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_25, DSI_PKT_TYPE_WR, SEQ_NT36525_25, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_26, DSI_PKT_TYPE_WR, SEQ_NT36525_26, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_27, DSI_PKT_TYPE_WR, SEQ_NT36525_27, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_28, DSI_PKT_TYPE_WR, SEQ_NT36525_28, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_29, DSI_PKT_TYPE_WR, SEQ_NT36525_29, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_30, DSI_PKT_TYPE_WR, SEQ_NT36525_30, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_31, DSI_PKT_TYPE_WR, SEQ_NT36525_31, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_32, DSI_PKT_TYPE_WR, SEQ_NT36525_32, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_33, DSI_PKT_TYPE_WR, SEQ_NT36525_33, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_34, DSI_PKT_TYPE_WR, SEQ_NT36525_34, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_35, DSI_PKT_TYPE_WR, SEQ_NT36525_35, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_36, DSI_PKT_TYPE_WR, SEQ_NT36525_36, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_37, DSI_PKT_TYPE_WR, SEQ_NT36525_37, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_38, DSI_PKT_TYPE_WR, SEQ_NT36525_38, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_39, DSI_PKT_TYPE_WR, SEQ_NT36525_39, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_40, DSI_PKT_TYPE_WR, SEQ_NT36525_40, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_41, DSI_PKT_TYPE_WR, SEQ_NT36525_41, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_42, DSI_PKT_TYPE_WR, SEQ_NT36525_42, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_43, DSI_PKT_TYPE_WR, SEQ_NT36525_43, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_44, DSI_PKT_TYPE_WR, SEQ_NT36525_44, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_45, DSI_PKT_TYPE_WR, SEQ_NT36525_45, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_46, DSI_PKT_TYPE_WR, SEQ_NT36525_46, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_47, DSI_PKT_TYPE_WR, SEQ_NT36525_47, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_48, DSI_PKT_TYPE_WR, SEQ_NT36525_48, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_49, DSI_PKT_TYPE_WR, SEQ_NT36525_49, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_50, DSI_PKT_TYPE_WR, SEQ_NT36525_50, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_51, DSI_PKT_TYPE_WR, SEQ_NT36525_51, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_52, DSI_PKT_TYPE_WR, SEQ_NT36525_52, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_53, DSI_PKT_TYPE_WR, SEQ_NT36525_53, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_54, DSI_PKT_TYPE_WR, SEQ_NT36525_54, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_55, DSI_PKT_TYPE_WR, SEQ_NT36525_55, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_56, DSI_PKT_TYPE_WR, SEQ_NT36525_56, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_57, DSI_PKT_TYPE_WR, SEQ_NT36525_57, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_58, DSI_PKT_TYPE_WR, SEQ_NT36525_58, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_59, DSI_PKT_TYPE_WR, SEQ_NT36525_59, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_60, DSI_PKT_TYPE_WR, SEQ_NT36525_60, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_61, DSI_PKT_TYPE_WR, SEQ_NT36525_61, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_62, DSI_PKT_TYPE_WR, SEQ_NT36525_62, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_63, DSI_PKT_TYPE_WR, SEQ_NT36525_63, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_64, DSI_PKT_TYPE_WR, SEQ_NT36525_64, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_65, DSI_PKT_TYPE_WR, SEQ_NT36525_65, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_66, DSI_PKT_TYPE_WR, SEQ_NT36525_66, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_67, DSI_PKT_TYPE_WR, SEQ_NT36525_67, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_68, DSI_PKT_TYPE_WR, SEQ_NT36525_68, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_69, DSI_PKT_TYPE_WR, SEQ_NT36525_69, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_70, DSI_PKT_TYPE_WR, SEQ_NT36525_70, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_71, DSI_PKT_TYPE_WR, SEQ_NT36525_71, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_72, DSI_PKT_TYPE_WR, SEQ_NT36525_72, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_73, DSI_PKT_TYPE_WR, SEQ_NT36525_73, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_74, DSI_PKT_TYPE_WR, SEQ_NT36525_74, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_75, DSI_PKT_TYPE_WR, SEQ_NT36525_75, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_76, DSI_PKT_TYPE_WR, SEQ_NT36525_76, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_77, DSI_PKT_TYPE_WR, SEQ_NT36525_77, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_78, DSI_PKT_TYPE_WR, SEQ_NT36525_78, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_79, DSI_PKT_TYPE_WR, SEQ_NT36525_79, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_80, DSI_PKT_TYPE_WR, SEQ_NT36525_80, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_81, DSI_PKT_TYPE_WR, SEQ_NT36525_81, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_82, DSI_PKT_TYPE_WR, SEQ_NT36525_82, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_83, DSI_PKT_TYPE_WR, SEQ_NT36525_83, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_84, DSI_PKT_TYPE_WR, SEQ_NT36525_84, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_85, DSI_PKT_TYPE_WR, SEQ_NT36525_85, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_86, DSI_PKT_TYPE_WR, SEQ_NT36525_86, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_87, DSI_PKT_TYPE_WR, SEQ_NT36525_87, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_88, DSI_PKT_TYPE_WR, SEQ_NT36525_88, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_89, DSI_PKT_TYPE_WR, SEQ_NT36525_89, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_90, DSI_PKT_TYPE_WR, SEQ_NT36525_90, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_91, DSI_PKT_TYPE_WR, SEQ_NT36525_91, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_92, DSI_PKT_TYPE_WR, SEQ_NT36525_92, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_93, DSI_PKT_TYPE_WR, SEQ_NT36525_93, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_94, DSI_PKT_TYPE_WR, SEQ_NT36525_94, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_95, DSI_PKT_TYPE_WR, SEQ_NT36525_95, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_96, DSI_PKT_TYPE_WR, SEQ_NT36525_96, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_97, DSI_PKT_TYPE_WR, SEQ_NT36525_97, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_98, DSI_PKT_TYPE_WR, SEQ_NT36525_98, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_99, DSI_PKT_TYPE_WR, SEQ_NT36525_99, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_100, DSI_PKT_TYPE_WR, SEQ_NT36525_100, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_101, DSI_PKT_TYPE_WR, SEQ_NT36525_101, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_102, DSI_PKT_TYPE_WR, SEQ_NT36525_102, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_103, DSI_PKT_TYPE_WR, SEQ_NT36525_103, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_104, DSI_PKT_TYPE_WR, SEQ_NT36525_104, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_105, DSI_PKT_TYPE_WR, SEQ_NT36525_105, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_106, DSI_PKT_TYPE_WR, SEQ_NT36525_106, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_107, DSI_PKT_TYPE_WR, SEQ_NT36525_107, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_108, DSI_PKT_TYPE_WR, SEQ_NT36525_108, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_109, DSI_PKT_TYPE_WR, SEQ_NT36525_109, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_110, DSI_PKT_TYPE_WR, SEQ_NT36525_110, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_111, DSI_PKT_TYPE_WR, SEQ_NT36525_111, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_112, DSI_PKT_TYPE_WR, SEQ_NT36525_112, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_113, DSI_PKT_TYPE_WR, SEQ_NT36525_113, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_114, DSI_PKT_TYPE_WR, SEQ_NT36525_114, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_115, DSI_PKT_TYPE_WR, SEQ_NT36525_115, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_116, DSI_PKT_TYPE_WR, SEQ_NT36525_116, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_117, DSI_PKT_TYPE_WR, SEQ_NT36525_117, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_118, DSI_PKT_TYPE_WR, SEQ_NT36525_118, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_119, DSI_PKT_TYPE_WR, SEQ_NT36525_119, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_120, DSI_PKT_TYPE_WR, SEQ_NT36525_120, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_121, DSI_PKT_TYPE_WR, SEQ_NT36525_121, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_122, DSI_PKT_TYPE_WR, SEQ_NT36525_122, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_123, DSI_PKT_TYPE_WR, SEQ_NT36525_123, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_124, DSI_PKT_TYPE_WR, SEQ_NT36525_124, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_125, DSI_PKT_TYPE_WR, SEQ_NT36525_125, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_126, DSI_PKT_TYPE_WR, SEQ_NT36525_126, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_127, DSI_PKT_TYPE_WR, SEQ_NT36525_127, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_128, DSI_PKT_TYPE_WR, SEQ_NT36525_128, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_129, DSI_PKT_TYPE_WR, SEQ_NT36525_129, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_130, DSI_PKT_TYPE_WR, SEQ_NT36525_130, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_131, DSI_PKT_TYPE_WR, SEQ_NT36525_131, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_132, DSI_PKT_TYPE_WR, SEQ_NT36525_132, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_133, DSI_PKT_TYPE_WR, SEQ_NT36525_133, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_134, DSI_PKT_TYPE_WR, SEQ_NT36525_134, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_135, DSI_PKT_TYPE_WR, SEQ_NT36525_135, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_136, DSI_PKT_TYPE_WR, SEQ_NT36525_136, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_137, DSI_PKT_TYPE_WR, SEQ_NT36525_137, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_138, DSI_PKT_TYPE_WR, SEQ_NT36525_138, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_139, DSI_PKT_TYPE_WR, SEQ_NT36525_139, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_140, DSI_PKT_TYPE_WR, SEQ_NT36525_140, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_141, DSI_PKT_TYPE_WR, SEQ_NT36525_141, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_142, DSI_PKT_TYPE_WR, SEQ_NT36525_142, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_143, DSI_PKT_TYPE_WR, SEQ_NT36525_143, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_144, DSI_PKT_TYPE_WR, SEQ_NT36525_144, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_145, DSI_PKT_TYPE_WR, SEQ_NT36525_145, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_146, DSI_PKT_TYPE_WR, SEQ_NT36525_146, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_147, DSI_PKT_TYPE_WR, SEQ_NT36525_147, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_148, DSI_PKT_TYPE_WR, SEQ_NT36525_148, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_149, DSI_PKT_TYPE_WR, SEQ_NT36525_149, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_150, DSI_PKT_TYPE_WR, SEQ_NT36525_150, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_151, DSI_PKT_TYPE_WR, SEQ_NT36525_151, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_152, DSI_PKT_TYPE_WR, SEQ_NT36525_152, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_153, DSI_PKT_TYPE_WR, SEQ_NT36525_153, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_154, DSI_PKT_TYPE_WR, SEQ_NT36525_154, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_155, DSI_PKT_TYPE_WR, SEQ_NT36525_155, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_156, DSI_PKT_TYPE_WR, SEQ_NT36525_156, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_157, DSI_PKT_TYPE_WR, SEQ_NT36525_157, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_158, DSI_PKT_TYPE_WR, SEQ_NT36525_158, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_159, DSI_PKT_TYPE_WR, SEQ_NT36525_159, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_160, DSI_PKT_TYPE_WR, SEQ_NT36525_160, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_161, DSI_PKT_TYPE_WR, SEQ_NT36525_161, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_162, DSI_PKT_TYPE_WR, SEQ_NT36525_162, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_163, DSI_PKT_TYPE_WR, SEQ_NT36525_163, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_164, DSI_PKT_TYPE_WR, SEQ_NT36525_164, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_165, DSI_PKT_TYPE_WR, SEQ_NT36525_165, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_166, DSI_PKT_TYPE_WR, SEQ_NT36525_166, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_167, DSI_PKT_TYPE_WR, SEQ_NT36525_167, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_168, DSI_PKT_TYPE_WR, SEQ_NT36525_168, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_169, DSI_PKT_TYPE_WR, SEQ_NT36525_169, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_170, DSI_PKT_TYPE_WR, SEQ_NT36525_170, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_171, DSI_PKT_TYPE_WR, SEQ_NT36525_171, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_172, DSI_PKT_TYPE_WR, SEQ_NT36525_172, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_173, DSI_PKT_TYPE_WR, SEQ_NT36525_173, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_174, DSI_PKT_TYPE_WR, SEQ_NT36525_174, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_175, DSI_PKT_TYPE_WR, SEQ_NT36525_175, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_176, DSI_PKT_TYPE_WR, SEQ_NT36525_176, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_177, DSI_PKT_TYPE_WR, SEQ_NT36525_177, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_178, DSI_PKT_TYPE_WR, SEQ_NT36525_178, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_179, DSI_PKT_TYPE_WR, SEQ_NT36525_179, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_180, DSI_PKT_TYPE_WR, SEQ_NT36525_180, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_181, DSI_PKT_TYPE_WR, SEQ_NT36525_181, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_182, DSI_PKT_TYPE_WR, SEQ_NT36525_182, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_183, DSI_PKT_TYPE_WR, SEQ_NT36525_183, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_184, DSI_PKT_TYPE_WR, SEQ_NT36525_184, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_185, DSI_PKT_TYPE_WR, SEQ_NT36525_185, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_186, DSI_PKT_TYPE_WR, SEQ_NT36525_186, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_187, DSI_PKT_TYPE_WR, SEQ_NT36525_187, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_188, DSI_PKT_TYPE_WR, SEQ_NT36525_188, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_189, DSI_PKT_TYPE_WR, SEQ_NT36525_189, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_190, DSI_PKT_TYPE_WR, SEQ_NT36525_190, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_191, DSI_PKT_TYPE_WR, SEQ_NT36525_191, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_192, DSI_PKT_TYPE_WR, SEQ_NT36525_192, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_193, DSI_PKT_TYPE_WR, SEQ_NT36525_193, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_194, DSI_PKT_TYPE_WR, SEQ_NT36525_194, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_195, DSI_PKT_TYPE_WR, SEQ_NT36525_195, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_196, DSI_PKT_TYPE_WR, SEQ_NT36525_196, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_197, DSI_PKT_TYPE_WR, SEQ_NT36525_197, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_198, DSI_PKT_TYPE_WR, SEQ_NT36525_198, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_199, DSI_PKT_TYPE_WR, SEQ_NT36525_199, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_200, DSI_PKT_TYPE_WR, SEQ_NT36525_200, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_201, DSI_PKT_TYPE_WR, SEQ_NT36525_201, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_202, DSI_PKT_TYPE_WR, SEQ_NT36525_202, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_203, DSI_PKT_TYPE_WR, SEQ_NT36525_203, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_204, DSI_PKT_TYPE_WR, SEQ_NT36525_204, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_205, DSI_PKT_TYPE_WR, SEQ_NT36525_205, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_206, DSI_PKT_TYPE_WR, SEQ_NT36525_206, 0);
static DEFINE_STATIC_PACKET(seq_nt36525_207, DSI_PKT_TYPE_WR, SEQ_NT36525_207, 0);



static DEFINE_PANEL_MDELAY(xcover5_wait_display_off, 20);	/* 1 frame */
static DEFINE_PANEL_MDELAY(xcover5_wait_sleep_in, 60);
static DEFINE_PANEL_MDELAY(xcover5_wait_sleep_out, 100);	/* 6 frame */
static DEFINE_PANEL_MDELAY(xcover5_wait_blic_off, 1);
static DEFINE_PANEL_MDELAY(xcover5_wait_1ms, 1);


static void *xcover5_init_cmdtbl[] = {
	&nt36525_restbl[RES_ID],
	&PKTINFO(seq_nt36525_01),
	&DLYINFO(xcover5_wait_1ms),
	&PKTINFO(seq_nt36525_02),
	&PKTINFO(seq_nt36525_03),
	&PKTINFO(seq_nt36525_04),
	&PKTINFO(seq_nt36525_05),
	&PKTINFO(seq_nt36525_06),
	&PKTINFO(seq_nt36525_07),
	&PKTINFO(seq_nt36525_08),
	&PKTINFO(seq_nt36525_09),
	&PKTINFO(seq_nt36525_10),
	&PKTINFO(seq_nt36525_11),
	&PKTINFO(seq_nt36525_12),
	&PKTINFO(seq_nt36525_13),
	&PKTINFO(seq_nt36525_14),
	&PKTINFO(seq_nt36525_15),
	&PKTINFO(seq_nt36525_16),
	&DLYINFO(xcover5_wait_1ms),
	&PKTINFO(seq_nt36525_17),
	&PKTINFO(seq_nt36525_18),
	&PKTINFO(seq_nt36525_19),
	&PKTINFO(seq_nt36525_20),
	&PKTINFO(seq_nt36525_21),
	&PKTINFO(seq_nt36525_22),
	&PKTINFO(seq_nt36525_23),
	&PKTINFO(seq_nt36525_24),
	&PKTINFO(seq_nt36525_25),
	&PKTINFO(seq_nt36525_26),
	&PKTINFO(seq_nt36525_27),
	&PKTINFO(seq_nt36525_28),
	&PKTINFO(seq_nt36525_29),
	&PKTINFO(seq_nt36525_30),
	&PKTINFO(seq_nt36525_31),
	&PKTINFO(seq_nt36525_32),
	&PKTINFO(seq_nt36525_33),
	&PKTINFO(seq_nt36525_34),
	&PKTINFO(seq_nt36525_35),
	&PKTINFO(seq_nt36525_36),
	&PKTINFO(seq_nt36525_37),
	&PKTINFO(seq_nt36525_38),
	&PKTINFO(seq_nt36525_39),
	&PKTINFO(seq_nt36525_40),
	&PKTINFO(seq_nt36525_41),
	&PKTINFO(seq_nt36525_42),
	&PKTINFO(seq_nt36525_43),
	&PKTINFO(seq_nt36525_44),
	&PKTINFO(seq_nt36525_45),
	&PKTINFO(seq_nt36525_46),
	&PKTINFO(seq_nt36525_47),
	&PKTINFO(seq_nt36525_48),
	&PKTINFO(seq_nt36525_49),
	&PKTINFO(seq_nt36525_50),
	&PKTINFO(seq_nt36525_51),
	&PKTINFO(seq_nt36525_52),
	&PKTINFO(seq_nt36525_53),
	&PKTINFO(seq_nt36525_54),
	&PKTINFO(seq_nt36525_55),
	&PKTINFO(seq_nt36525_56),
	&PKTINFO(seq_nt36525_57),
	&PKTINFO(seq_nt36525_58),
	&PKTINFO(seq_nt36525_59),
	&PKTINFO(seq_nt36525_60),
	&PKTINFO(seq_nt36525_61),
	&PKTINFO(seq_nt36525_62),
	&PKTINFO(seq_nt36525_63),
	&PKTINFO(seq_nt36525_64),
	&PKTINFO(seq_nt36525_65),
	&PKTINFO(seq_nt36525_66),
	&PKTINFO(seq_nt36525_67),
	&PKTINFO(seq_nt36525_68),
	&PKTINFO(seq_nt36525_69),
	&PKTINFO(seq_nt36525_70),
	&PKTINFO(seq_nt36525_71),
	&PKTINFO(seq_nt36525_72),
	&PKTINFO(seq_nt36525_73),
	&PKTINFO(seq_nt36525_74),
	&PKTINFO(seq_nt36525_75),
	&PKTINFO(seq_nt36525_76),
	&PKTINFO(seq_nt36525_77),
	&PKTINFO(seq_nt36525_78),
	&PKTINFO(seq_nt36525_79),
	&PKTINFO(seq_nt36525_80),
	&PKTINFO(seq_nt36525_81),
	&PKTINFO(seq_nt36525_82),
	&PKTINFO(seq_nt36525_83),
	&DLYINFO(xcover5_wait_1ms),
	&PKTINFO(seq_nt36525_84),
	&PKTINFO(seq_nt36525_85),
	&PKTINFO(seq_nt36525_86),
	&PKTINFO(seq_nt36525_87),
	&PKTINFO(seq_nt36525_88),
	&PKTINFO(seq_nt36525_89),
	&PKTINFO(seq_nt36525_90),
	&PKTINFO(seq_nt36525_91),
	&PKTINFO(seq_nt36525_92),
	&PKTINFO(seq_nt36525_93),
	&PKTINFO(seq_nt36525_94),
	&PKTINFO(seq_nt36525_95),
	&PKTINFO(seq_nt36525_96),
	&PKTINFO(seq_nt36525_97),
	&PKTINFO(seq_nt36525_98),
	&PKTINFO(seq_nt36525_99),
	&PKTINFO(seq_nt36525_100),
	&PKTINFO(seq_nt36525_101),
	&PKTINFO(seq_nt36525_102),
	&PKTINFO(seq_nt36525_103),
	&PKTINFO(seq_nt36525_104),
	&PKTINFO(seq_nt36525_105),
	&PKTINFO(seq_nt36525_106),
	&PKTINFO(seq_nt36525_107),
	&PKTINFO(seq_nt36525_108),
	&PKTINFO(seq_nt36525_109),
	&PKTINFO(seq_nt36525_110),
	&PKTINFO(seq_nt36525_111),
	&PKTINFO(seq_nt36525_112),
	&DLYINFO(xcover5_wait_1ms),
	&PKTINFO(seq_nt36525_113),
	&PKTINFO(seq_nt36525_114),
	&PKTINFO(seq_nt36525_115),
	&DLYINFO(xcover5_wait_1ms),
	&PKTINFO(seq_nt36525_116),
	&PKTINFO(seq_nt36525_117),
	&PKTINFO(seq_nt36525_118),
	&PKTINFO(seq_nt36525_119),
	&PKTINFO(seq_nt36525_120),
	&PKTINFO(seq_nt36525_121),
	&PKTINFO(seq_nt36525_122),
	&PKTINFO(seq_nt36525_123),
	&PKTINFO(seq_nt36525_124),
	&PKTINFO(seq_nt36525_125),
	&PKTINFO(seq_nt36525_126),
	&PKTINFO(seq_nt36525_127),
	&PKTINFO(seq_nt36525_128),
	&PKTINFO(seq_nt36525_129),
	&DLYINFO(xcover5_wait_1ms),
	&PKTINFO(seq_nt36525_130),
	&PKTINFO(seq_nt36525_131),
	&PKTINFO(seq_nt36525_132),
	&PKTINFO(seq_nt36525_133),
	&PKTINFO(seq_nt36525_134),
	&PKTINFO(seq_nt36525_135),
	&PKTINFO(seq_nt36525_136),
	&PKTINFO(seq_nt36525_137),
	&PKTINFO(seq_nt36525_138),
	&PKTINFO(seq_nt36525_139),
	&PKTINFO(seq_nt36525_140),
	&PKTINFO(seq_nt36525_141),
	&PKTINFO(seq_nt36525_142),
	&PKTINFO(seq_nt36525_143),
	&PKTINFO(seq_nt36525_144),
	&PKTINFO(seq_nt36525_145),
	&PKTINFO(seq_nt36525_146),
	&PKTINFO(seq_nt36525_147),
	&PKTINFO(seq_nt36525_148),
	&PKTINFO(seq_nt36525_149),
	&PKTINFO(seq_nt36525_150),
	&PKTINFO(seq_nt36525_151),
	&PKTINFO(seq_nt36525_152),
	&PKTINFO(seq_nt36525_153),
	&PKTINFO(seq_nt36525_154),
	&PKTINFO(seq_nt36525_155),
	&PKTINFO(seq_nt36525_156),
	&PKTINFO(seq_nt36525_157),
	&PKTINFO(seq_nt36525_158),
	&PKTINFO(seq_nt36525_159),
	&PKTINFO(seq_nt36525_160),
	&PKTINFO(seq_nt36525_161),
	&PKTINFO(seq_nt36525_162),
	&PKTINFO(seq_nt36525_163),
	&PKTINFO(seq_nt36525_164),
	&PKTINFO(seq_nt36525_165),
	&PKTINFO(seq_nt36525_166),
	&PKTINFO(seq_nt36525_167),
	&PKTINFO(seq_nt36525_168),
	&PKTINFO(seq_nt36525_169),
	&PKTINFO(seq_nt36525_170),
	&DLYINFO(xcover5_wait_1ms),
	&PKTINFO(seq_nt36525_171),
	&PKTINFO(seq_nt36525_172),
	&PKTINFO(seq_nt36525_173),
	&PKTINFO(seq_nt36525_174),
	&PKTINFO(seq_nt36525_175),
	&PKTINFO(seq_nt36525_176),
	&PKTINFO(seq_nt36525_177),
	&PKTINFO(seq_nt36525_178),
	&PKTINFO(seq_nt36525_179),
	&PKTINFO(seq_nt36525_180),
	&PKTINFO(seq_nt36525_181),
	&PKTINFO(seq_nt36525_182),
	&PKTINFO(seq_nt36525_183),
	&PKTINFO(seq_nt36525_184),
	&DLYINFO(xcover5_wait_1ms),
	&PKTINFO(seq_nt36525_185),
	&PKTINFO(seq_nt36525_186),
	&PKTINFO(seq_nt36525_187),
	&PKTINFO(seq_nt36525_188),
	&PKTINFO(seq_nt36525_189),
	&PKTINFO(seq_nt36525_190),
	&PKTINFO(seq_nt36525_191),
	&PKTINFO(seq_nt36525_192),
	&PKTINFO(seq_nt36525_193),
	&PKTINFO(seq_nt36525_194),
	&PKTINFO(seq_nt36525_195),
	&PKTINFO(seq_nt36525_196),
	&PKTINFO(seq_nt36525_197),
	&PKTINFO(seq_nt36525_198),
	&DLYINFO(xcover5_wait_1ms),
	&PKTINFO(seq_nt36525_199),
	&PKTINFO(seq_nt36525_200),
	&PKTINFO(seq_nt36525_201),
	&DLYINFO(xcover5_wait_1ms),
	&PKTINFO(seq_nt36525_202),
	&PKTINFO(seq_nt36525_203),
	&PKTINFO(seq_nt36525_204),
	&PKTINFO(seq_nt36525_205),
	&PKTINFO(seq_nt36525_206),
	&PKTINFO(seq_nt36525_207),
	&PKTINFO(xcover5_sleep_out),
	&DLYINFO(xcover5_wait_sleep_out),
	&PKTINFO(xcover5_display_on),
};

static void *xcover5_res_init_cmdtbl[] = {
	&nt36525_restbl[RES_ID],
};

static void *xcover5_set_bl_cmdtbl[] = {
	&PKTINFO(xcover5_brightness), //51h
};

static void *xcover5_display_off_cmdtbl[] = {
	&PKTINFO(xcover5_display_off),
	&DLYINFO(xcover5_wait_display_off),
};

static void *xcover5_exit_cmdtbl[] = {
	&PKTINFO(xcover5_sleep_in),
	&DLYINFO(xcover5_wait_sleep_in),
};

/* ===================================================================================== */
/* ================================= [EA8076 I2C TABLE] ===========+++================= */
/* ===================================================================================== */
static u8 NT36525_XCOVER5_I2C_INIT[] = {
	0x0C, 0x2C,
	0x0D, 0x26,
	0x0E, 0x26,
	0x09, 0xBE,
	0x02, 0x4B,
	0x03, 0x0D,
	0x11, 0x74,
	0x04, 0x01,
	0x05, 0xE1,
	0x10, 0x67,
	0x08, 0x13,
};

static u8 NT36525_XCOVER5_I2C_EXIT_VSN[] = {
	0x09, 0xBC,
};

static u8 NT36525_XCOVER5_I2C_EXIT_VSP[] = {
	0x09, 0xB8,
};

static DEFINE_STATIC_PACKET(nt36525_xcover5_i2c_init, I2C_PKT_TYPE_WR, NT36525_XCOVER5_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(nt36525_xcover5_i2c_exit_vsn, I2C_PKT_TYPE_WR, NT36525_XCOVER5_I2C_EXIT_VSN, 0);
static DEFINE_STATIC_PACKET(nt36525_xcover5_i2c_exit_vsp, I2C_PKT_TYPE_WR, NT36525_XCOVER5_I2C_EXIT_VSP, 0);
static DEFINE_STATIC_PACKET(nt36525_xcover5_i2c_dump, I2C_PKT_TYPE_RD, NT36525_XCOVER5_I2C_INIT, 0);

static void *nt36525_xcover5_init_cmdtbl[] = {
	&PKTINFO(nt36525_xcover5_i2c_init),
};

static void *nt36525_xcover5_exit_cmdtbl[] = {
	&PKTINFO(nt36525_xcover5_i2c_exit_vsn),
	&DLYINFO(xcover5_wait_blic_off),
	&PKTINFO(nt36525_xcover5_i2c_exit_vsp),
};

static void *nt36525_xcover5_dump_cmdtbl[] = {
	&PKTINFO(nt36525_xcover5_i2c_dump),
};


static struct seqinfo xcover5_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", xcover5_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", xcover5_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", xcover5_set_bl_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", xcover5_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", xcover5_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", nt36525_xcover5_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", nt36525_xcover5_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", nt36525_xcover5_dump_cmdtbl),
#endif

};

struct common_panel_info nt36525_xcover5_default_panel_info = {
	.ldi_name = "nt36525",
	.name = "nt36525_xcover5_default",
	.model = "CSO_5_31_inch",
	.vendor = "CSO",
	.id = 0x8AA240,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.init_seq_by_lpdt = 1,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(nt36525_xcover5_resol),
		.resol = nt36525_xcover5_resol,
	},
	.maptbl = xcover5_maptbl,
	.nr_maptbl = ARRAY_SIZE(xcover5_maptbl),
	.seqtbl = xcover5_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(xcover5_seqtbl),
	.rditbl = nt36525_rditbl,
	.nr_rditbl = ARRAY_SIZE(nt36525_rditbl),
	.restbl = nt36525_restbl,
	.nr_restbl = ARRAY_SIZE(nt36525_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&nt36525_xcover5_panel_dimming_info,
	},
	.i2c_data = &nt36525_xcover5_i2c_data,
};

static int __init nt36525_xcover5_panel_init(void)
{
	register_common_panel(&nt36525_xcover5_default_panel_info);

	return 0;
}
arch_initcall(nt36525_xcover5_panel_init)
#endif /* __NT36525_XCOVER5_PANEL_H__ */
