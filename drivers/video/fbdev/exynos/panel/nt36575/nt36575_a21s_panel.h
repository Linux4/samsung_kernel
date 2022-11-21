/*
 * linux/drivers/video/fbdev/exynos/panel/nt36575/nt36575_a21s_panel.h
 *
 * Header file for NT36575 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NT36575_A21S_PANEL_H__
#define __NT36575_A21S_PANEL_H__
#include "../panel_drv.h"
#include "nt36575.h"

#include "nt36575_a21s_panel_dimming.h"
#include "nt36575_a21s_panel_i2c.h"

#include "nt36575_a21s_resol.h"

#undef __pn_name__
#define __pn_name__	a21s

#undef __PN_NAME__
#define __PN_NAME__	A21S

static struct seqinfo a21s_seqtbl[MAX_PANEL_SEQ];


/* ===================================================================================== */
/* ============================= [NT36575 READ INFO TABLE] ============================= */
/* ===================================================================================== */
/* <READINFO TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================= [NT36575 RESOURCE TABLE] ============================== */
/* ===================================================================================== */
/* <RESOURCE TABLE> IS DEPENDENT ON LDI. IF YOU NEED, DEFINE PANEL's RESOURCE TABLE */


/* ===================================================================================== */
/* ============================== [NT36575 MAPPING TABLE] ============================== */
/* ===================================================================================== */

static u8 a21s_brt_table[NT36575_TOTAL_NR_LUMINANCE][1] = {
	{0},
	{1}, {1}, {2}, {2}, {3}, {3}, {4}, {4}, {5}, {5}, /* 1: 1 */
	{6}, {6}, {7}, {8}, {8}, {9}, {9}, {10}, {10}, {11},
	{11}, {12}, {12}, {13}, {14}, {14}, {15}, {15}, {16}, {16},
	{17}, {17}, {18}, {18}, {19}, {20}, {20}, {21}, {21}, {22},
	{22}, {23}, {23}, {24}, {24}, {25}, {25}, {26}, {27}, {27},
	{28}, {28}, {29}, {29}, {30}, {30}, {31}, {31}, {32}, {33},
	{33}, {34}, {34}, {35}, {35}, {36}, {36}, {37}, {37}, {38},
	{39}, {39}, {40}, {40}, {41}, {41}, {42}, {42}, {43}, {43},
	{44}, {45}, {45}, {46}, {46}, {47}, {47}, {48}, {48}, {49},
	{49}, {50}, {50}, {51}, {52}, {52}, {53}, {53}, {54}, {54},
	{55}, {55}, {56}, {56}, {57}, {58}, {58}, {59}, {59}, {60},
	{60}, {61}, {61}, {62}, {62}, {63}, {64}, {64}, {65}, {65},
	{66}, {66}, {67}, {67}, {68}, {68}, {69}, {70}, {70}, {71}, /* 128: 70 */
	{72}, {73}, {74}, {75}, {76}, {77}, {78}, {79}, {80}, {81},
	{82}, {83}, {84}, {85}, {86}, {87}, {88}, {89}, {90}, {91},
	{92}, {93}, {94}, {95}, {96}, {97}, {98}, {99}, {100}, {100},
	{101}, {102}, {103}, {104}, {105}, {106}, {107}, {108}, {109}, {110},
	{111}, {112}, {113}, {114}, {115}, {116}, {117}, {118}, {119}, {120},
	{121}, {122}, {123}, {124}, {125}, {126}, {127}, {128}, {129}, {130},
	{131}, {131}, {132}, {133}, {134}, {135}, {136}, {137}, {138}, {139},
	{140}, {141}, {142}, {143}, {144}, {145}, {146}, {147}, {148}, {149},
	{150}, {151}, {152}, {153}, {154}, {155}, {156}, {157}, {158}, {159},
	{160}, {161}, {162}, {162}, {163}, {164}, {165}, {166}, {167}, {168},
	{169}, {170}, {171}, {172}, {173}, {174}, {175}, {176}, {177}, {178},
	{179}, {180}, {181}, {182}, {183}, {184}, {185}, {186}, {187}, {188},
	{189}, {190}, {191}, {192}, {193}, {193}, {193}, {193}, {193}, {193}, /* 255: 193 */
	{193}, {193}, {193}, {193}, {193}, {193}, {193}, {193}, {193}, {193},
	{193}, {193}, {193}, {193}, {193}, {193}, {193}, {193}, {193}, {193},
	{193}, {193}, {193}, {193}, {193}, {193}, {193}, {193}, {193}, {193},
	{193}, {193}, {193}, {193}, {193}, {193}, {193}, {193}, {193}, {193},
	{193}, {193}, {193}, {193}, {193}, {230},
};

static struct maptbl a21s_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(a21s_brt_table, init_brightness_table, getidx_brt_table, copy_common_maptbl),
};

/* ===================================================================================== */
/* ============================== [NT36575 COMMAND TABLE] ============================== */
/* ===================================================================================== */
static u8 A21S_SLEEP_OUT[] = { 0x11 };
static u8 A21S_SLEEP_IN[] = { 0x10 };
static u8 A21S_DISPLAY_OFF[] = { 0x28 };
static u8 A21S_DISPLAY_ON[] = { 0x29 };

static u8 A21S_BRIGHTNESS_ZERO[] = {
	0x51,
	0x00, 0x00
};

static u8 A21S_BRIGHTNESS[] = {
	0x51,
	0xFF
};

static u8 A21S_BRIGHTNESS_MODE[] = {
	0x53,
	0x24,
};

static u8 A21S_BRIGHTNESS_CABC_MODE[] = {
	0x55,
	0x00,
};

/* Made by LCD.helper.200122.xlsm */

static unsigned char SEQ_NT36575_01[] = {
	0xFF,
	0x10,
};

static unsigned char SEQ_NT36575_02[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_03[] = {
	0xB0,
	0x00,
};

static unsigned char SEQ_NT36575_04[] = {
	0xFF,
	0x20,
};

static unsigned char SEQ_NT36575_05[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_06[] = {
	0x06,
	0x50,
};

static unsigned char SEQ_NT36575_07[] = {
	0x07,
	0x28,
};

static unsigned char SEQ_NT36575_08[] = {
	0x69,
	0x92,
};

static unsigned char SEQ_NT36575_09[] = {
	0x95,
	0xD1,
};

static unsigned char SEQ_NT36575_10[] = {
	0x96,
	0xD1,
};

static unsigned char SEQ_NT36575_11[] = {
	0xF2,
	0x54,
};

static unsigned char SEQ_NT36575_12[] = {
	0xF4,
	0x54,
};

static unsigned char SEQ_NT36575_13[] = {
	0xF6,
	0x54,
};

static unsigned char SEQ_NT36575_14[] = {
	0xF8,
	0x54,
};

static unsigned char SEQ_NT36575_15[] = {
	0xFF,
	0x24,
};

static unsigned char SEQ_NT36575_16[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_17[] = {
	0x00,
	0x13,
};

static unsigned char SEQ_NT36575_18[] = {
	0x01,
	0x15,
};

static unsigned char SEQ_NT36575_19[] = {
	0x02,
	0x17,
};

static unsigned char SEQ_NT36575_20[] = {
	0x03,
	0x26,
};

static unsigned char SEQ_NT36575_21[] = {
	0x04,
	0x27,
};

static unsigned char SEQ_NT36575_22[] = {
	0x05,
	0x28,
};

static unsigned char SEQ_NT36575_23[] = {
	0x07,
	0x24,
};

static unsigned char SEQ_NT36575_24[] = {
	0x08,
	0x24,
};

static unsigned char SEQ_NT36575_25[] = {
	0x0A,
	0x22,
};

static unsigned char SEQ_NT36575_26[] = {
	0x0C,
	0x00,
};

static unsigned char SEQ_NT36575_27[] = {
	0x0D,
	0x0F,
};

static unsigned char SEQ_NT36575_28[] = {
	0x0E,
	0x01,
};

static unsigned char SEQ_NT36575_29[] = {
	0x10,
	0x03,
};

static unsigned char SEQ_NT36575_30[] = {
	0x11,
	0x04,
};

static unsigned char SEQ_NT36575_31[] = {
	0x12,
	0x05,
};

static unsigned char SEQ_NT36575_32[] = {
	0x13,
	0x06,
};

static unsigned char SEQ_NT36575_33[] = {
	0x15,
	0x0B,
};

static unsigned char SEQ_NT36575_34[] = {
	0x17,
	0x0C,
};

static unsigned char SEQ_NT36575_35[] = {
	0x18,
	0x13,
};

static unsigned char SEQ_NT36575_36[] = {
	0x19,
	0x15,
};

static unsigned char SEQ_NT36575_37[] = {
	0x1A,
	0x17,
};

static unsigned char SEQ_NT36575_38[] = {
	0x1B,
	0x26,
};

static unsigned char SEQ_NT36575_39[] = {
	0x1C,
	0x27,
};

static unsigned char SEQ_NT36575_40[] = {
	0x1D,
	0x28,
};

static unsigned char SEQ_NT36575_41[] = {
	0x1F,
	0x24,
};

static unsigned char SEQ_NT36575_42[] = {
	0x20,
	0x24,
};

static unsigned char SEQ_NT36575_43[] = {
	0x22,
	0x22,
};

static unsigned char SEQ_NT36575_44[] = {
	0x24,
	0x00,
};

static unsigned char SEQ_NT36575_45[] = {
	0x25,
	0x0F,
};

static unsigned char SEQ_NT36575_46[] = {
	0x26,
	0x01,
};

static unsigned char SEQ_NT36575_47[] = {
	0x28,
	0x03,
};

static unsigned char SEQ_NT36575_48[] = {
	0x29,
	0x04,
};

static unsigned char SEQ_NT36575_49[] = {
	0x2A,
	0x05,
};

static unsigned char SEQ_NT36575_50[] = {
	0x2B,
	0x06,
};

static unsigned char SEQ_NT36575_51[] = {
	0x2F,
	0x0B,
};

static unsigned char SEQ_NT36575_52[] = {
	0x31,
	0x0C,
};

static unsigned char SEQ_NT36575_53[] = {
	0x32,
	0x09,
};

static unsigned char SEQ_NT36575_54[] = {
	0x33,
	0x03,
};

static unsigned char SEQ_NT36575_55[] = {
	0x34,
	0x03,
};

static unsigned char SEQ_NT36575_56[] = {
	0x35,
	0x01, 0x00
};

static unsigned char SEQ_NT36575_57[] = {
	0x36,
	0xB4,
};

static unsigned char SEQ_NT36575_58[] = {
	0x45,
	0x04,
};

static unsigned char SEQ_NT36575_59[] = {
	0x47,
	0x04,
};

static unsigned char SEQ_NT36575_60[] = {
	0x4B,
	0x49,
};

static unsigned char SEQ_NT36575_61[] = {
	0x4E,
	0xB0,
};

static unsigned char SEQ_NT36575_62[] = {
	0x4F,
	0xB0,
};

static unsigned char SEQ_NT36575_63[] = {
	0x50,
	0x02,
};

static unsigned char SEQ_NT36575_64[] = {
	0x53,
	0xB0,
};

static unsigned char SEQ_NT36575_65[] = {
	0x79,
	0x22,
};

static unsigned char SEQ_NT36575_66[] = {
	0x7A,
	0x83,
};

static unsigned char SEQ_NT36575_67[] = {
	0x7B,
	0x9A,
};

static unsigned char SEQ_NT36575_68[] = {
	0x7D,
	0x03,
};

static unsigned char SEQ_NT36575_69[] = {
	0x80,
	0x03,
};

static unsigned char SEQ_NT36575_70[] = {
	0x81,
	0x03,
};

static unsigned char SEQ_NT36575_71[] = {
	0x82,
	0x16,
};

static unsigned char SEQ_NT36575_72[] = {
	0x83,
	0x25,
};

static unsigned char SEQ_NT36575_73[] = {
	0x84,
	0x34,
};

static unsigned char SEQ_NT36575_74[] = {
	0x85,
	0x43,
};

static unsigned char SEQ_NT36575_75[] = {
	0x86,
	0x52,
};

static unsigned char SEQ_NT36575_76[] = {
	0x87,
	0x61,
};

static unsigned char SEQ_NT36575_77[] = {
	0x90,
	0x43,
};

static unsigned char SEQ_NT36575_78[] = {
	0x91,
	0x52,
};

static unsigned char SEQ_NT36575_79[] = {
	0x92,
	0x61,
};

static unsigned char SEQ_NT36575_80[] = {
	0x93,
	0x16,
};

static unsigned char SEQ_NT36575_81[] = {
	0x94,
	0x25,
};

static unsigned char SEQ_NT36575_82[] = {
	0x95,
	0x34,
};

static unsigned char SEQ_NT36575_83[] = {
	0x9C,
	0xF4,
};

static unsigned char SEQ_NT36575_84[] = {
	0x9D,
	0x01,
};

static unsigned char SEQ_NT36575_85[] = {
	0xA0,
	0x1A,
};

static unsigned char SEQ_NT36575_86[] = {
	0xA2,
	0x1A,
};

static unsigned char SEQ_NT36575_87[] = {
	0xA3,
	0x03,
};

static unsigned char SEQ_NT36575_88[] = {
	0xA4,
	0x03,
};

static unsigned char SEQ_NT36575_89[] = {
	0xA5,
	0x03,
};

static unsigned char SEQ_NT36575_90[] = {
	0xAA,
	0x05,
};

static unsigned char SEQ_NT36575_91[] = {
	0xAD,
	0x02,
};

static unsigned char SEQ_NT36575_92[] = {
	0xB0,
	0x0B,
};

static unsigned char SEQ_NT36575_93[] = {
	0xB3,
	0x08,
};

static unsigned char SEQ_NT36575_94[] = {
	0xB6,
	0x11,
};

static unsigned char SEQ_NT36575_95[] = {
	0xB9,
	0x0E,
};

static unsigned char SEQ_NT36575_96[] = {
	0xBC,
	0x17,
};

static unsigned char SEQ_NT36575_97[] = {
	0xBF,
	0x14,
};

static unsigned char SEQ_NT36575_98[] = {
	0xC4,
	0xC0,
};

static unsigned char SEQ_NT36575_99[] = {
	0xCA,
	0xC9,
};

static unsigned char SEQ_NT36575_100[] = {
	0xD9,
	0x80,
};

static unsigned char SEQ_NT36575_101[] = {
	0xE5,
	0x06,
};

static unsigned char SEQ_NT36575_102[] = {
	0xE6,
	0x80,
};

static unsigned char SEQ_NT36575_103[] = {
	0xE9,
	0x03,
};

static unsigned char SEQ_NT36575_104[] = {
	0xED,
	0xC0,
};

static unsigned char SEQ_NT36575_105[] = {
	0xFF,
	0x25,
};

static unsigned char SEQ_NT36575_106[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_107[] = {
	0x67,
	0x21,
};

static unsigned char SEQ_NT36575_108[] = {
	0x68,
	0x58,
};

static unsigned char SEQ_NT36575_109[] = {
	0x69,
	0x10,
};

static unsigned char SEQ_NT36575_110[] = {
	0x6B,
	0x00,
};

static unsigned char SEQ_NT36575_111[] = {
	0x71,
	0x6D,
};

static unsigned char SEQ_NT36575_112[] = {
	0x77,
	0x72,
};

static unsigned char SEQ_NT36575_113[] = {
	0x7E,
	0x2D,
};

static unsigned char SEQ_NT36575_114[] = {
	0xC3,
	0x07,
};

static unsigned char SEQ_NT36575_115[] = {
	0xF0,
	0x05,
};

static unsigned char SEQ_NT36575_116[] = {
	0xF1,
	0x04,
};

static unsigned char SEQ_NT36575_117[] = {
	0xFF,
	0x26,
};

static unsigned char SEQ_NT36575_118[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_119[] = {
	0x01,
	0x24,
};

static unsigned char SEQ_NT36575_120[] = {
	0x04,
	0x24,
};

static unsigned char SEQ_NT36575_121[] = {
	0x05,
	0x08,
};

static unsigned char SEQ_NT36575_122[] = {
	0x06,
	0x2F,
};

static unsigned char SEQ_NT36575_123[] = {
	0x08,
	0x2F,
};

static unsigned char SEQ_NT36575_124[] = {
	0x74,
	0xAF,
};

static unsigned char SEQ_NT36575_125[] = {
	0x81,
	0x1A,
};

static unsigned char SEQ_NT36575_126[] = {
	0x83,
	0x03,
};

static unsigned char SEQ_NT36575_127[] = {
	0x84,
	0x02,
};

static unsigned char SEQ_NT36575_128[] = {
	0x85,
	0x01,
};

static unsigned char SEQ_NT36575_129[] = {
	0x86,
	0x02,
};

static unsigned char SEQ_NT36575_130[] = {
	0x87,
	0x01,
};

static unsigned char SEQ_NT36575_131[] = {
	0x88,
	0x08,
};

static unsigned char SEQ_NT36575_132[] = {
	0x8A,
	0x1A,
};

static unsigned char SEQ_NT36575_133[] = {
	0x8C,
	0x54,
};

static unsigned char SEQ_NT36575_134[] = {
	0x8D,
	0x63,
};

static unsigned char SEQ_NT36575_135[] = {
	0x8E,
	0x72,
};

static unsigned char SEQ_NT36575_136[] = {
	0x8F,
	0x27,
};

static unsigned char SEQ_NT36575_137[] = {
	0x90,
	0x36,
};

static unsigned char SEQ_NT36575_138[] = {
	0x91,
	0x45,
};

static unsigned char SEQ_NT36575_139[] = {
	0x9A,
	0x80,
};

static unsigned char SEQ_NT36575_140[] = {
	0x9B,
	0x04,
};

static unsigned char SEQ_NT36575_141[] = {
	0x9C,
	0x00,
};

static unsigned char SEQ_NT36575_142[] = {
	0x9D,
	0x00,
};

static unsigned char SEQ_NT36575_143[] = {
	0x9E,
	0x00,
};

static unsigned char SEQ_NT36575_144[] = {
	0xFF,
	0x27,
};

static unsigned char SEQ_NT36575_145[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_146[] = {
	0x00,
	0x62,
};

static unsigned char SEQ_NT36575_147[] = {
	0x01,
	0x40,
};

static unsigned char SEQ_NT36575_148[] = {
	0x02,
	0xD0,
};

static unsigned char SEQ_NT36575_149[] = {
	0x20,
	0x80,
};

static unsigned char SEQ_NT36575_150[] = {
	0x21,
	0xDB,
};

static unsigned char SEQ_NT36575_151[] = {
	0x25,
	0x82,
};

static unsigned char SEQ_NT36575_152[] = {
	0x26,
	0xBD,
};

static unsigned char SEQ_NT36575_153[] = {
	0x88,
	0x03,
};

static unsigned char SEQ_NT36575_154[] = {
	0x89,
	0x03,
};

static unsigned char SEQ_NT36575_155[] = {
	0x8A,
	0x02,
};

static unsigned char SEQ_NT36575_156[] = {
	0xF9,
	0x00,
};

static unsigned char SEQ_NT36575_157[] = {
	0xFF,
	0x2A,
};

static unsigned char SEQ_NT36575_158[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_159[] = {
	0x07,
	0x64,
};

static unsigned char SEQ_NT36575_160[] = {
	0x0A,
	0x30,
};

static unsigned char SEQ_NT36575_161[] = {
	0x11,
	0xA0,
};

static unsigned char SEQ_NT36575_162[] = {
	0x15,
	0x12,
};

static unsigned char SEQ_NT36575_163[] = {
	0x16,
	0x44,
};

static unsigned char SEQ_NT36575_164[] = {
	0x19,
	0x12,
};

static unsigned char SEQ_NT36575_165[] = {
	0x1A,
	0x18,
};

static unsigned char SEQ_NT36575_166[] = {
	0x1E,
	0xB8,
};

static unsigned char SEQ_NT36575_167[] = {
	0x1F,
	0xB8,
};

static unsigned char SEQ_NT36575_168[] = {
	0x20,
	0xB8,
};

static unsigned char SEQ_NT36575_169[] = {
	0x45,
	0xA0,
};

static unsigned char SEQ_NT36575_170[] = {
	0x75,
	0x6A,
};

static unsigned char SEQ_NT36575_171[] = {
	0xA5,
	0x50,
};

static unsigned char SEQ_NT36575_172[] = {
	0xD8,
	0x40,
};

static unsigned char SEQ_NT36575_173[] = {
	0xFF,
	0x2C,
};

static unsigned char SEQ_NT36575_174[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_175[] = {
	0xFF,
	0xE0,
};

static unsigned char SEQ_NT36575_176[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_177[] = {
	0xFF,
	0xF0,
};

static unsigned char SEQ_NT36575_178[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_179[] = {
	0xBB,
	0x40,
};

static unsigned char SEQ_NT36575_180[] = {
	0xFF,
	0xD0,
};

static unsigned char SEQ_NT36575_181[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_182[] = {
	0xFF,
	0x20,
};

static unsigned char SEQ_NT36575_183[] = {
	0xFB,
	0x21,
};

static unsigned char SEQ_NT36575_184[] = {
	0xB0,
	0x00, 0x00, 0x00, 0x33, 0x00, 0x69, 0x00, 0x90, 0x00, 0xAF,
	0x00, 0xC9, 0x00, 0xE0, 0x00, 0xF3,
};

static unsigned char SEQ_NT36575_185[] = {
	0xB1,
	0x01, 0x07, 0x01, 0x40, 0x01, 0x68, 0x01, 0xA4, 0x01, 0xD4,
	0x02, 0x19, 0x02, 0x50, 0x02, 0x52,
};

static unsigned char SEQ_NT36575_186[] = {
	0xB2,
	0x02, 0x85, 0x02, 0xC1, 0x02, 0xE7, 0x03, 0x17, 0x03, 0x36,
	0x03, 0x5B, 0x03, 0x68, 0x03, 0x75,
};

static unsigned char SEQ_NT36575_187[] = {
	0xB3,
	0x03, 0x84, 0x03, 0x95, 0x03, 0xA5, 0x03, 0xB3, 0x03, 0xD4,
	0x03, 0xD8, 0x00, 0x00,
};

static unsigned char SEQ_NT36575_188[] = {
	0xB4,
	0x00, 0x00, 0x00, 0x33, 0x00, 0x69, 0x00, 0x90, 0x00, 0xAF,
	0x00, 0xC9, 0x00, 0xE0, 0x00, 0xF3,
};

static unsigned char SEQ_NT36575_189[] = {
	0xB5,
	0x01, 0x07, 0x01, 0x40, 0x01, 0x68, 0x01, 0xA4, 0x01, 0xD4,
	0x02, 0x19, 0x02, 0x50, 0x02, 0x52,
};

static unsigned char SEQ_NT36575_190[] = {
	0xB6,
	0x02, 0x85, 0x02, 0xC1, 0x02, 0xE7, 0x03, 0x17, 0x03, 0x36,
	0x03, 0x5B, 0x03, 0x68, 0x03, 0x75,
};

static unsigned char SEQ_NT36575_191[] = {
	0xB7,
	0x03, 0x84, 0x03, 0x95, 0x03, 0xA5, 0x03, 0xB3, 0x03, 0xD4,
	0x03, 0xD8, 0x00, 0x00,
};

static unsigned char SEQ_NT36575_192[] = {
	0xB8,
	0x00, 0x00, 0x00, 0x33, 0x00, 0x69, 0x00, 0x90, 0x00, 0xAF,
	0x00, 0xC9, 0x00, 0xE0, 0x00, 0xF3,
};

static unsigned char SEQ_NT36575_193[] = {
	0xB9,
	0x01, 0x07, 0x01, 0x40, 0x01, 0x68, 0x01, 0xA4, 0x01, 0xD4,
	0x02, 0x19, 0x02, 0x50, 0x02, 0x52,
};

static unsigned char SEQ_NT36575_194[] = {
	0xBA,
	0x02, 0x85, 0x02, 0xC1, 0x02, 0xE7, 0x03, 0x17, 0x03, 0x36,
	0x03, 0x5B, 0x03, 0x68, 0x03, 0x75,
};

static unsigned char SEQ_NT36575_195[] = {
	0xBB,
	0x03, 0x84, 0x03, 0x95, 0x03, 0xA5, 0x03, 0xB3, 0x03, 0xD4,
	0x03, 0xD8, 0x00, 0x00,
};

static unsigned char SEQ_NT36575_196[] = {
	0xFF,
	0x21,
};

static unsigned char SEQ_NT36575_197[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_198[] = {
	0xB0,
	0x00, 0x00, 0x00, 0x33, 0x00, 0x69, 0x00, 0x90, 0x00, 0xAF,
	0x00, 0xC9, 0x00, 0xE0, 0x00, 0xF3,
};

static unsigned char SEQ_NT36575_199[] = {
	0xB1,
	0x01, 0x07, 0x01, 0x40, 0x01, 0x68, 0x01, 0xA4, 0x01, 0xD4,
	0x02, 0x19, 0x02, 0x50, 0x02, 0x52,
};

static unsigned char SEQ_NT36575_200[] = {
	0xB2,
	0x02, 0x85, 0x02, 0xC1, 0x02, 0xE7, 0x03, 0x17, 0x03, 0x36,
	0x03, 0x5B, 0x03, 0x68, 0x03, 0x75,
};

static unsigned char SEQ_NT36575_201[] = {
	0xB3,
	0x03, 0x84, 0x03, 0x95, 0x03, 0xA5, 0x03, 0xB3, 0x03, 0xD4,
	0x03, 0xD8, 0x00, 0x00,
};

static unsigned char SEQ_NT36575_202[] = {
	0xB4,
	0x00, 0x00, 0x00, 0x33, 0x00, 0x69, 0x00, 0x90, 0x00, 0xAF,
	0x00, 0xC9, 0x00, 0xE0, 0x00, 0xF3,
};

static unsigned char SEQ_NT36575_203[] = {
	0xB5,
	0x01, 0x07, 0x01, 0x40, 0x01, 0x68, 0x01, 0xA4, 0x01, 0xD4,
	0x02, 0x19, 0x02, 0x50, 0x02, 0x52,
};

static unsigned char SEQ_NT36575_204[] = {
	0xB6,
	0x02, 0x85, 0x02, 0xC1, 0x02, 0xE7, 0x03, 0x17, 0x03, 0x36,
	0x03, 0x5B, 0x03, 0x68, 0x03, 0x75,
};

static unsigned char SEQ_NT36575_205[] = {
	0xB7,
	0x03, 0x84, 0x03, 0x95, 0x03, 0xA5, 0x03, 0xB3, 0x03, 0xD4,
	0x03, 0xD8, 0x00, 0x00,
};

static unsigned char SEQ_NT36575_206[] = {
	0xB8,
	0x00, 0x00, 0x00, 0x33, 0x00, 0x69, 0x00, 0x90, 0x00, 0xAF,
	0x00, 0xC9, 0x00, 0xE0, 0x00, 0xF3,
};

static unsigned char SEQ_NT36575_207[] = {
	0xB9,
	0x01, 0x07, 0x01, 0x40, 0x01, 0x68, 0x01, 0xA4, 0x01, 0xD4,
	0x02, 0x19, 0x02, 0x50, 0x02, 0x52,
};

static unsigned char SEQ_NT36575_208[] = {
	0xBA,
	0x02, 0x85, 0x02, 0xC1, 0x02, 0xE7, 0x03, 0x17, 0x03, 0x36,
	0x03, 0x5B, 0x03, 0x68, 0x03, 0x75,
};

static unsigned char SEQ_NT36575_209[] = {
	0xBB,
	0x03, 0x84, 0x03, 0x95, 0x03, 0xA5, 0x03, 0xB3, 0x03, 0xD4,
	0x03, 0xD8, 0x00, 0x00,
};

static unsigned char SEQ_NT36575_210[] = {
	0xFF,
	0x23,
};

static unsigned char SEQ_NT36575_211[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_212[] = {
	0x07,
	0x60,
};

static unsigned char SEQ_NT36575_213[] = {
	0x08,
	0x06
};

static unsigned char SEQ_NT36575_214[] = {
	0x09,
	0x1E
};

static unsigned char SEQ_NT36575_215[] = {
	0xFF,
	0xD0,
};

static unsigned char SEQ_NT36575_216[] = {
	0xFB,
	0x01,
};

static unsigned char SEQ_NT36575_217[] = {
	0x25,
	0xA9,
};

static DEFINE_STATIC_PACKET(a21s_sleep_out, DSI_PKT_TYPE_WR, A21S_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(a21s_sleep_in, DSI_PKT_TYPE_WR, A21S_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(a21s_display_on, DSI_PKT_TYPE_WR, A21S_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(a21s_display_off, DSI_PKT_TYPE_WR, A21S_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(a21s_brightness_mode, DSI_PKT_TYPE_WR, A21S_BRIGHTNESS_MODE, 0);

static DEFINE_PKTUI(a21s_brightness, &a21s_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(a21s_brightness, DSI_PKT_TYPE_WR, A21S_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(a21s_nt36575_01, DSI_PKT_TYPE_WR, SEQ_NT36575_01 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_02, DSI_PKT_TYPE_WR, SEQ_NT36575_02 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_03, DSI_PKT_TYPE_WR, SEQ_NT36575_03 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_04, DSI_PKT_TYPE_WR, SEQ_NT36575_04 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_05, DSI_PKT_TYPE_WR, SEQ_NT36575_05 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_06, DSI_PKT_TYPE_WR, SEQ_NT36575_06 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_07, DSI_PKT_TYPE_WR, SEQ_NT36575_07 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_08, DSI_PKT_TYPE_WR, SEQ_NT36575_08 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_09, DSI_PKT_TYPE_WR, SEQ_NT36575_09 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_10, DSI_PKT_TYPE_WR, SEQ_NT36575_10 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_11, DSI_PKT_TYPE_WR, SEQ_NT36575_11 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_12, DSI_PKT_TYPE_WR, SEQ_NT36575_12 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_13, DSI_PKT_TYPE_WR, SEQ_NT36575_13 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_14, DSI_PKT_TYPE_WR, SEQ_NT36575_14 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_15, DSI_PKT_TYPE_WR, SEQ_NT36575_15 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_16, DSI_PKT_TYPE_WR, SEQ_NT36575_16 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_17, DSI_PKT_TYPE_WR, SEQ_NT36575_17 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_18, DSI_PKT_TYPE_WR, SEQ_NT36575_18 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_19, DSI_PKT_TYPE_WR, SEQ_NT36575_19 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_20, DSI_PKT_TYPE_WR, SEQ_NT36575_20 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_21, DSI_PKT_TYPE_WR, SEQ_NT36575_21 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_22, DSI_PKT_TYPE_WR, SEQ_NT36575_22 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_23, DSI_PKT_TYPE_WR, SEQ_NT36575_23 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_24, DSI_PKT_TYPE_WR, SEQ_NT36575_24 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_25, DSI_PKT_TYPE_WR, SEQ_NT36575_25 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_26, DSI_PKT_TYPE_WR, SEQ_NT36575_26 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_27, DSI_PKT_TYPE_WR, SEQ_NT36575_27 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_28, DSI_PKT_TYPE_WR, SEQ_NT36575_28 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_29, DSI_PKT_TYPE_WR, SEQ_NT36575_29 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_30, DSI_PKT_TYPE_WR, SEQ_NT36575_30 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_31, DSI_PKT_TYPE_WR, SEQ_NT36575_31 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_32, DSI_PKT_TYPE_WR, SEQ_NT36575_32 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_33, DSI_PKT_TYPE_WR, SEQ_NT36575_33 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_34, DSI_PKT_TYPE_WR, SEQ_NT36575_34 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_35, DSI_PKT_TYPE_WR, SEQ_NT36575_35 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_36, DSI_PKT_TYPE_WR, SEQ_NT36575_36 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_37, DSI_PKT_TYPE_WR, SEQ_NT36575_37 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_38, DSI_PKT_TYPE_WR, SEQ_NT36575_38 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_39, DSI_PKT_TYPE_WR, SEQ_NT36575_39 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_40, DSI_PKT_TYPE_WR, SEQ_NT36575_40 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_41, DSI_PKT_TYPE_WR, SEQ_NT36575_41 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_42, DSI_PKT_TYPE_WR, SEQ_NT36575_42 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_43, DSI_PKT_TYPE_WR, SEQ_NT36575_43 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_44, DSI_PKT_TYPE_WR, SEQ_NT36575_44 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_45, DSI_PKT_TYPE_WR, SEQ_NT36575_45 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_46, DSI_PKT_TYPE_WR, SEQ_NT36575_46 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_47, DSI_PKT_TYPE_WR, SEQ_NT36575_47 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_48, DSI_PKT_TYPE_WR, SEQ_NT36575_48 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_49, DSI_PKT_TYPE_WR, SEQ_NT36575_49 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_50, DSI_PKT_TYPE_WR, SEQ_NT36575_50 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_51, DSI_PKT_TYPE_WR, SEQ_NT36575_51 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_52, DSI_PKT_TYPE_WR, SEQ_NT36575_52 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_53, DSI_PKT_TYPE_WR, SEQ_NT36575_53 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_54, DSI_PKT_TYPE_WR, SEQ_NT36575_54 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_55, DSI_PKT_TYPE_WR, SEQ_NT36575_55 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_56, DSI_PKT_TYPE_WR, SEQ_NT36575_56 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_57, DSI_PKT_TYPE_WR, SEQ_NT36575_57 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_58, DSI_PKT_TYPE_WR, SEQ_NT36575_58 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_59, DSI_PKT_TYPE_WR, SEQ_NT36575_59 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_60, DSI_PKT_TYPE_WR, SEQ_NT36575_60 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_61, DSI_PKT_TYPE_WR, SEQ_NT36575_61 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_62, DSI_PKT_TYPE_WR, SEQ_NT36575_62 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_63, DSI_PKT_TYPE_WR, SEQ_NT36575_63 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_64, DSI_PKT_TYPE_WR, SEQ_NT36575_64 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_65, DSI_PKT_TYPE_WR, SEQ_NT36575_65 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_66, DSI_PKT_TYPE_WR, SEQ_NT36575_66 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_67, DSI_PKT_TYPE_WR, SEQ_NT36575_67 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_68, DSI_PKT_TYPE_WR, SEQ_NT36575_68 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_69, DSI_PKT_TYPE_WR, SEQ_NT36575_69 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_70, DSI_PKT_TYPE_WR, SEQ_NT36575_70 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_71, DSI_PKT_TYPE_WR, SEQ_NT36575_71 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_72, DSI_PKT_TYPE_WR, SEQ_NT36575_72 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_73, DSI_PKT_TYPE_WR, SEQ_NT36575_73 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_74, DSI_PKT_TYPE_WR, SEQ_NT36575_74 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_75, DSI_PKT_TYPE_WR, SEQ_NT36575_75 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_76, DSI_PKT_TYPE_WR, SEQ_NT36575_76 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_77, DSI_PKT_TYPE_WR, SEQ_NT36575_77 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_78, DSI_PKT_TYPE_WR, SEQ_NT36575_78 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_79, DSI_PKT_TYPE_WR, SEQ_NT36575_79 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_80, DSI_PKT_TYPE_WR, SEQ_NT36575_80 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_81, DSI_PKT_TYPE_WR, SEQ_NT36575_81 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_82, DSI_PKT_TYPE_WR, SEQ_NT36575_82 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_83, DSI_PKT_TYPE_WR, SEQ_NT36575_83 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_84, DSI_PKT_TYPE_WR, SEQ_NT36575_84 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_85, DSI_PKT_TYPE_WR, SEQ_NT36575_85 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_86, DSI_PKT_TYPE_WR, SEQ_NT36575_86 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_87, DSI_PKT_TYPE_WR, SEQ_NT36575_87 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_88, DSI_PKT_TYPE_WR, SEQ_NT36575_88 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_89, DSI_PKT_TYPE_WR, SEQ_NT36575_89 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_90, DSI_PKT_TYPE_WR, SEQ_NT36575_90 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_91, DSI_PKT_TYPE_WR, SEQ_NT36575_91 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_92, DSI_PKT_TYPE_WR, SEQ_NT36575_92 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_93, DSI_PKT_TYPE_WR, SEQ_NT36575_93 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_94, DSI_PKT_TYPE_WR, SEQ_NT36575_94 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_95, DSI_PKT_TYPE_WR, SEQ_NT36575_95 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_96, DSI_PKT_TYPE_WR, SEQ_NT36575_96 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_97, DSI_PKT_TYPE_WR, SEQ_NT36575_97 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_98, DSI_PKT_TYPE_WR, SEQ_NT36575_98 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_99, DSI_PKT_TYPE_WR, SEQ_NT36575_99 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_100, DSI_PKT_TYPE_WR, SEQ_NT36575_100 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_101, DSI_PKT_TYPE_WR, SEQ_NT36575_101 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_102, DSI_PKT_TYPE_WR, SEQ_NT36575_102 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_103, DSI_PKT_TYPE_WR, SEQ_NT36575_103 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_104, DSI_PKT_TYPE_WR, SEQ_NT36575_104 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_105, DSI_PKT_TYPE_WR, SEQ_NT36575_105 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_106, DSI_PKT_TYPE_WR, SEQ_NT36575_106 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_107, DSI_PKT_TYPE_WR, SEQ_NT36575_107 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_108, DSI_PKT_TYPE_WR, SEQ_NT36575_108 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_109, DSI_PKT_TYPE_WR, SEQ_NT36575_109 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_110, DSI_PKT_TYPE_WR, SEQ_NT36575_110 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_111, DSI_PKT_TYPE_WR, SEQ_NT36575_111 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_112, DSI_PKT_TYPE_WR, SEQ_NT36575_112 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_113, DSI_PKT_TYPE_WR, SEQ_NT36575_113 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_114, DSI_PKT_TYPE_WR, SEQ_NT36575_114 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_115, DSI_PKT_TYPE_WR, SEQ_NT36575_115 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_116, DSI_PKT_TYPE_WR, SEQ_NT36575_116 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_117, DSI_PKT_TYPE_WR, SEQ_NT36575_117 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_118, DSI_PKT_TYPE_WR, SEQ_NT36575_118 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_119, DSI_PKT_TYPE_WR, SEQ_NT36575_119 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_120, DSI_PKT_TYPE_WR, SEQ_NT36575_120 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_121, DSI_PKT_TYPE_WR, SEQ_NT36575_121 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_122, DSI_PKT_TYPE_WR, SEQ_NT36575_122 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_123, DSI_PKT_TYPE_WR, SEQ_NT36575_123 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_124, DSI_PKT_TYPE_WR, SEQ_NT36575_124 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_125, DSI_PKT_TYPE_WR, SEQ_NT36575_125 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_126, DSI_PKT_TYPE_WR, SEQ_NT36575_126 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_127, DSI_PKT_TYPE_WR, SEQ_NT36575_127 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_128, DSI_PKT_TYPE_WR, SEQ_NT36575_128 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_129, DSI_PKT_TYPE_WR, SEQ_NT36575_129 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_130, DSI_PKT_TYPE_WR, SEQ_NT36575_130 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_131, DSI_PKT_TYPE_WR, SEQ_NT36575_131 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_132, DSI_PKT_TYPE_WR, SEQ_NT36575_132 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_133, DSI_PKT_TYPE_WR, SEQ_NT36575_133 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_134, DSI_PKT_TYPE_WR, SEQ_NT36575_134 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_135, DSI_PKT_TYPE_WR, SEQ_NT36575_135 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_136, DSI_PKT_TYPE_WR, SEQ_NT36575_136 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_137, DSI_PKT_TYPE_WR, SEQ_NT36575_137 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_138, DSI_PKT_TYPE_WR, SEQ_NT36575_138 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_139, DSI_PKT_TYPE_WR, SEQ_NT36575_139 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_140, DSI_PKT_TYPE_WR, SEQ_NT36575_140 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_141, DSI_PKT_TYPE_WR, SEQ_NT36575_141 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_142, DSI_PKT_TYPE_WR, SEQ_NT36575_142 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_143, DSI_PKT_TYPE_WR, SEQ_NT36575_143 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_144, DSI_PKT_TYPE_WR, SEQ_NT36575_144 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_145, DSI_PKT_TYPE_WR, SEQ_NT36575_145 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_146, DSI_PKT_TYPE_WR, SEQ_NT36575_146 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_147, DSI_PKT_TYPE_WR, SEQ_NT36575_147 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_148, DSI_PKT_TYPE_WR, SEQ_NT36575_148 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_149, DSI_PKT_TYPE_WR, SEQ_NT36575_149 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_150, DSI_PKT_TYPE_WR, SEQ_NT36575_150 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_151, DSI_PKT_TYPE_WR, SEQ_NT36575_151 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_152, DSI_PKT_TYPE_WR, SEQ_NT36575_152 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_153, DSI_PKT_TYPE_WR, SEQ_NT36575_153 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_154, DSI_PKT_TYPE_WR, SEQ_NT36575_154 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_155, DSI_PKT_TYPE_WR, SEQ_NT36575_155 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_156, DSI_PKT_TYPE_WR, SEQ_NT36575_156 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_157, DSI_PKT_TYPE_WR, SEQ_NT36575_157 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_158, DSI_PKT_TYPE_WR, SEQ_NT36575_158 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_159, DSI_PKT_TYPE_WR, SEQ_NT36575_159 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_160, DSI_PKT_TYPE_WR, SEQ_NT36575_160 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_161, DSI_PKT_TYPE_WR, SEQ_NT36575_161 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_162, DSI_PKT_TYPE_WR, SEQ_NT36575_162 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_163, DSI_PKT_TYPE_WR, SEQ_NT36575_163 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_164, DSI_PKT_TYPE_WR, SEQ_NT36575_164 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_165, DSI_PKT_TYPE_WR, SEQ_NT36575_165 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_166, DSI_PKT_TYPE_WR, SEQ_NT36575_166 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_167, DSI_PKT_TYPE_WR, SEQ_NT36575_167 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_168, DSI_PKT_TYPE_WR, SEQ_NT36575_168 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_169, DSI_PKT_TYPE_WR, SEQ_NT36575_169 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_170, DSI_PKT_TYPE_WR, SEQ_NT36575_170 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_171, DSI_PKT_TYPE_WR, SEQ_NT36575_171 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_172, DSI_PKT_TYPE_WR, SEQ_NT36575_172 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_173, DSI_PKT_TYPE_WR, SEQ_NT36575_173 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_174, DSI_PKT_TYPE_WR, SEQ_NT36575_174 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_175, DSI_PKT_TYPE_WR, SEQ_NT36575_175 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_176, DSI_PKT_TYPE_WR, SEQ_NT36575_176 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_177, DSI_PKT_TYPE_WR, SEQ_NT36575_177 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_178, DSI_PKT_TYPE_WR, SEQ_NT36575_178 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_179, DSI_PKT_TYPE_WR, SEQ_NT36575_179 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_180, DSI_PKT_TYPE_WR, SEQ_NT36575_180 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_181, DSI_PKT_TYPE_WR, SEQ_NT36575_181 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_182, DSI_PKT_TYPE_WR, SEQ_NT36575_182 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_183, DSI_PKT_TYPE_WR, SEQ_NT36575_183 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_184, DSI_PKT_TYPE_WR, SEQ_NT36575_184 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_185, DSI_PKT_TYPE_WR, SEQ_NT36575_185 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_186, DSI_PKT_TYPE_WR, SEQ_NT36575_186 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_187, DSI_PKT_TYPE_WR, SEQ_NT36575_187 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_188, DSI_PKT_TYPE_WR, SEQ_NT36575_188 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_189, DSI_PKT_TYPE_WR, SEQ_NT36575_189 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_190, DSI_PKT_TYPE_WR, SEQ_NT36575_190 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_191, DSI_PKT_TYPE_WR, SEQ_NT36575_191 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_192, DSI_PKT_TYPE_WR, SEQ_NT36575_192 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_193, DSI_PKT_TYPE_WR, SEQ_NT36575_193 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_194, DSI_PKT_TYPE_WR, SEQ_NT36575_194 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_195, DSI_PKT_TYPE_WR, SEQ_NT36575_195 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_196, DSI_PKT_TYPE_WR, SEQ_NT36575_196 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_197, DSI_PKT_TYPE_WR, SEQ_NT36575_197 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_198, DSI_PKT_TYPE_WR, SEQ_NT36575_198 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_199, DSI_PKT_TYPE_WR, SEQ_NT36575_199 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_200, DSI_PKT_TYPE_WR, SEQ_NT36575_200 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_201, DSI_PKT_TYPE_WR, SEQ_NT36575_201 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_202, DSI_PKT_TYPE_WR, SEQ_NT36575_202 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_203, DSI_PKT_TYPE_WR, SEQ_NT36575_203 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_204, DSI_PKT_TYPE_WR, SEQ_NT36575_204 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_205, DSI_PKT_TYPE_WR, SEQ_NT36575_205 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_206, DSI_PKT_TYPE_WR, SEQ_NT36575_206 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_207, DSI_PKT_TYPE_WR, SEQ_NT36575_207 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_208, DSI_PKT_TYPE_WR, SEQ_NT36575_208 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_209, DSI_PKT_TYPE_WR, SEQ_NT36575_209 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_210, DSI_PKT_TYPE_WR, SEQ_NT36575_210 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_211, DSI_PKT_TYPE_WR, SEQ_NT36575_211 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_212, DSI_PKT_TYPE_WR, SEQ_NT36575_212 ,0);

static DEFINE_STATIC_PACKET(a21s_nt36575_213, DSI_PKT_TYPE_WR, SEQ_NT36575_213 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_214, DSI_PKT_TYPE_WR, SEQ_NT36575_214 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_215, DSI_PKT_TYPE_WR, SEQ_NT36575_215 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_216, DSI_PKT_TYPE_WR, SEQ_NT36575_216 ,0);
static DEFINE_STATIC_PACKET(a21s_nt36575_217, DSI_PKT_TYPE_WR, SEQ_NT36575_217 ,0);

static DEFINE_PANEL_MDELAY(a21s_wait_display_off, 1 * 17); /* 1 frame */
static DEFINE_PANEL_MDELAY(a21s_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(a21s_wait_10msec, 10);
static DEFINE_PANEL_MDELAY(a21s_wait_blic_off, 1);


static void *a21s_init_cmdtbl[] = {
	&nt36575_restbl[RES_ID],
	&PKTINFO(a21s_nt36575_01),
	&PKTINFO(a21s_nt36575_02),
	&PKTINFO(a21s_nt36575_03),
	&PKTINFO(a21s_nt36575_04),
	&PKTINFO(a21s_nt36575_05),
	&PKTINFO(a21s_nt36575_06),
	&PKTINFO(a21s_nt36575_07),
	&PKTINFO(a21s_nt36575_08),
	&PKTINFO(a21s_nt36575_09),
	&PKTINFO(a21s_nt36575_10),
	&PKTINFO(a21s_nt36575_11),
	&PKTINFO(a21s_nt36575_12),
	&PKTINFO(a21s_nt36575_13),
	&PKTINFO(a21s_nt36575_14),
	&PKTINFO(a21s_nt36575_15),
	&PKTINFO(a21s_nt36575_16),
	&PKTINFO(a21s_nt36575_17),
	&PKTINFO(a21s_nt36575_18),
	&PKTINFO(a21s_nt36575_19),
	&PKTINFO(a21s_nt36575_20),
	&PKTINFO(a21s_nt36575_21),
	&PKTINFO(a21s_nt36575_22),
	&PKTINFO(a21s_nt36575_23),
	&PKTINFO(a21s_nt36575_24),
	&PKTINFO(a21s_nt36575_25),
	&PKTINFO(a21s_nt36575_26),
	&PKTINFO(a21s_nt36575_27),
	&PKTINFO(a21s_nt36575_28),
	&PKTINFO(a21s_nt36575_29),
	&PKTINFO(a21s_nt36575_30),
	&PKTINFO(a21s_nt36575_31),
	&PKTINFO(a21s_nt36575_32),
	&PKTINFO(a21s_nt36575_33),
	&PKTINFO(a21s_nt36575_34),
	&PKTINFO(a21s_nt36575_35),
	&PKTINFO(a21s_nt36575_36),
	&PKTINFO(a21s_nt36575_37),
	&PKTINFO(a21s_nt36575_38),
	&PKTINFO(a21s_nt36575_39),
	&PKTINFO(a21s_nt36575_40),
	&PKTINFO(a21s_nt36575_41),
	&PKTINFO(a21s_nt36575_42),
	&PKTINFO(a21s_nt36575_43),
	&PKTINFO(a21s_nt36575_44),
	&PKTINFO(a21s_nt36575_45),
	&PKTINFO(a21s_nt36575_46),
	&PKTINFO(a21s_nt36575_47),
	&PKTINFO(a21s_nt36575_48),
	&PKTINFO(a21s_nt36575_49),
	&PKTINFO(a21s_nt36575_50),
	&PKTINFO(a21s_nt36575_51),
	&PKTINFO(a21s_nt36575_52),
	&PKTINFO(a21s_nt36575_53),
	&PKTINFO(a21s_nt36575_54),
	&PKTINFO(a21s_nt36575_55),
	&PKTINFO(a21s_nt36575_56),
	&PKTINFO(a21s_nt36575_57),
	&PKTINFO(a21s_nt36575_58),
	&PKTINFO(a21s_nt36575_59),
	&PKTINFO(a21s_nt36575_60),
	&PKTINFO(a21s_nt36575_61),
	&PKTINFO(a21s_nt36575_62),
	&PKTINFO(a21s_nt36575_63),
	&PKTINFO(a21s_nt36575_64),
	&PKTINFO(a21s_nt36575_65),
	&PKTINFO(a21s_nt36575_66),
	&PKTINFO(a21s_nt36575_67),
	&PKTINFO(a21s_nt36575_68),
	&PKTINFO(a21s_nt36575_69),
	&PKTINFO(a21s_nt36575_70),
	&PKTINFO(a21s_nt36575_71),
	&PKTINFO(a21s_nt36575_72),
	&PKTINFO(a21s_nt36575_73),
	&PKTINFO(a21s_nt36575_74),
	&PKTINFO(a21s_nt36575_75),
	&PKTINFO(a21s_nt36575_76),
	&PKTINFO(a21s_nt36575_77),
	&PKTINFO(a21s_nt36575_78),
	&PKTINFO(a21s_nt36575_79),
	&PKTINFO(a21s_nt36575_80),
	&PKTINFO(a21s_nt36575_81),
	&PKTINFO(a21s_nt36575_82),
	&PKTINFO(a21s_nt36575_83),
	&PKTINFO(a21s_nt36575_84),
	&PKTINFO(a21s_nt36575_85),
	&PKTINFO(a21s_nt36575_86),
	&PKTINFO(a21s_nt36575_87),
	&PKTINFO(a21s_nt36575_88),
	&PKTINFO(a21s_nt36575_89),
	&PKTINFO(a21s_nt36575_90),
	&PKTINFO(a21s_nt36575_91),
	&PKTINFO(a21s_nt36575_92),
	&PKTINFO(a21s_nt36575_93),
	&PKTINFO(a21s_nt36575_94),
	&PKTINFO(a21s_nt36575_95),
	&PKTINFO(a21s_nt36575_96),
	&PKTINFO(a21s_nt36575_97),
	&PKTINFO(a21s_nt36575_98),
	&PKTINFO(a21s_nt36575_99),
	&PKTINFO(a21s_nt36575_100),
	&PKTINFO(a21s_nt36575_101),
	&PKTINFO(a21s_nt36575_102),
	&PKTINFO(a21s_nt36575_103),
	&PKTINFO(a21s_nt36575_104),
	&PKTINFO(a21s_nt36575_105),
	&PKTINFO(a21s_nt36575_106),
	&PKTINFO(a21s_nt36575_107),
	&PKTINFO(a21s_nt36575_108),
	&PKTINFO(a21s_nt36575_109),
	&PKTINFO(a21s_nt36575_110),
	&PKTINFO(a21s_nt36575_111),
	&PKTINFO(a21s_nt36575_112),
	&PKTINFO(a21s_nt36575_113),
	&PKTINFO(a21s_nt36575_114),
	&PKTINFO(a21s_nt36575_115),
	&PKTINFO(a21s_nt36575_116),
	&PKTINFO(a21s_nt36575_117),
	&PKTINFO(a21s_nt36575_118),
	&PKTINFO(a21s_nt36575_119),
	&PKTINFO(a21s_nt36575_120),
	&PKTINFO(a21s_nt36575_121),
	&PKTINFO(a21s_nt36575_122),
	&PKTINFO(a21s_nt36575_123),
	&PKTINFO(a21s_nt36575_124),
	&PKTINFO(a21s_nt36575_125),
	&PKTINFO(a21s_nt36575_126),
	&PKTINFO(a21s_nt36575_127),
	&PKTINFO(a21s_nt36575_128),
	&PKTINFO(a21s_nt36575_129),
	&PKTINFO(a21s_nt36575_130),
	&PKTINFO(a21s_nt36575_131),
	&PKTINFO(a21s_nt36575_132),
	&PKTINFO(a21s_nt36575_133),
	&PKTINFO(a21s_nt36575_134),
	&PKTINFO(a21s_nt36575_135),
	&PKTINFO(a21s_nt36575_136),
	&PKTINFO(a21s_nt36575_137),
	&PKTINFO(a21s_nt36575_138),
	&PKTINFO(a21s_nt36575_139),
	&PKTINFO(a21s_nt36575_140),
	&PKTINFO(a21s_nt36575_141),
	&PKTINFO(a21s_nt36575_142),
	&PKTINFO(a21s_nt36575_143),
	&PKTINFO(a21s_nt36575_144),
	&PKTINFO(a21s_nt36575_145),
	&PKTINFO(a21s_nt36575_146),
	&PKTINFO(a21s_nt36575_147),
	&PKTINFO(a21s_nt36575_148),
	&PKTINFO(a21s_nt36575_149),
	&PKTINFO(a21s_nt36575_150),
	&PKTINFO(a21s_nt36575_151),
	&PKTINFO(a21s_nt36575_152),
	&PKTINFO(a21s_nt36575_153),
	&PKTINFO(a21s_nt36575_154),
	&PKTINFO(a21s_nt36575_155),
	&PKTINFO(a21s_nt36575_156),
	&PKTINFO(a21s_nt36575_157),
	&PKTINFO(a21s_nt36575_158),
	&PKTINFO(a21s_nt36575_159),
	&PKTINFO(a21s_nt36575_160),
	&PKTINFO(a21s_nt36575_161),
	&PKTINFO(a21s_nt36575_162),
	&PKTINFO(a21s_nt36575_163),
	&PKTINFO(a21s_nt36575_164),
	&PKTINFO(a21s_nt36575_165),
	&PKTINFO(a21s_nt36575_166),
	&PKTINFO(a21s_nt36575_167),
	&PKTINFO(a21s_nt36575_168),
	&PKTINFO(a21s_nt36575_169),
	&PKTINFO(a21s_nt36575_170),
	&PKTINFO(a21s_nt36575_171),
	&PKTINFO(a21s_nt36575_172),
	&PKTINFO(a21s_nt36575_173),
	&PKTINFO(a21s_nt36575_174),
	&PKTINFO(a21s_nt36575_175),
	&PKTINFO(a21s_nt36575_176),
	&PKTINFO(a21s_nt36575_177),
	&PKTINFO(a21s_nt36575_178),
	&PKTINFO(a21s_nt36575_179),
	&PKTINFO(a21s_nt36575_180),
	&PKTINFO(a21s_nt36575_181),
	&PKTINFO(a21s_nt36575_182),
	&PKTINFO(a21s_nt36575_183),
	&PKTINFO(a21s_nt36575_184),
	&PKTINFO(a21s_nt36575_185),
	&PKTINFO(a21s_nt36575_186),
	&PKTINFO(a21s_nt36575_187),
	&PKTINFO(a21s_nt36575_188),
	&PKTINFO(a21s_nt36575_189),
	&PKTINFO(a21s_nt36575_190),
	&PKTINFO(a21s_nt36575_191),
	&PKTINFO(a21s_nt36575_192),
	&PKTINFO(a21s_nt36575_193),
	&PKTINFO(a21s_nt36575_194),
	&PKTINFO(a21s_nt36575_195),
	&PKTINFO(a21s_nt36575_196),
	&PKTINFO(a21s_nt36575_197),
	&PKTINFO(a21s_nt36575_198),
	&PKTINFO(a21s_nt36575_199),
	&PKTINFO(a21s_nt36575_200),
	&PKTINFO(a21s_nt36575_201),
	&PKTINFO(a21s_nt36575_202),
	&PKTINFO(a21s_nt36575_203),
	&PKTINFO(a21s_nt36575_204),
	&PKTINFO(a21s_nt36575_205),
	&PKTINFO(a21s_nt36575_206),
	&PKTINFO(a21s_nt36575_207),
	&PKTINFO(a21s_nt36575_208),
	&PKTINFO(a21s_nt36575_209),
	&PKTINFO(a21s_nt36575_210),
	&PKTINFO(a21s_nt36575_211),
	&PKTINFO(a21s_nt36575_212),
	&PKTINFO(a21s_nt36575_213),
	&PKTINFO(a21s_nt36575_214),
	&PKTINFO(a21s_nt36575_215),
	&PKTINFO(a21s_nt36575_216),
	&PKTINFO(a21s_nt36575_217),

	/* back to cmd 0 page */
	&PKTINFO(a21s_nt36575_01),
	&PKTINFO(a21s_nt36575_02),

	&PKTINFO(a21s_sleep_out),
	&DLYINFO(a21s_wait_100msec),
	&PKTINFO(a21s_display_on),
	&PKTINFO(a21s_brightness_mode),
	&PKTINFO(a21s_brightness), //51h
};

static void *a21s_res_init_cmdtbl[] = {
	&nt36575_restbl[RES_ID],
};

static void *a21s_set_bl_cmdtbl[] = {
	&PKTINFO(a21s_brightness), //51h
};

static void *a21s_display_off_cmdtbl[] = {
	&PKTINFO(a21s_display_off),
	&DLYINFO(a21s_wait_display_off),
};

static void *a21s_exit_cmdtbl[] = {
	&PKTINFO(a21s_sleep_in),
};

/* ===================================================================================== */
/* ================================= [EA8076 I2C TABLE] ===========+++================= */
/* ===================================================================================== */
static u8 NT36575_A21S_I2C_INIT[] = {
	0x0C, 0x28,
	0x0D, 0x1E,
	0x0E, 0x1E,
	0x09, 0xBE,
	0x02, 0x69,
	0x03, 0x0D,
	0x11, 0x75,
	0x04, 0x05,
	0x05, 0xCC,
	0x10, 0x67,
	0x08, 0x13,
};

static u8 NT36575_A21S_I2C_EXIT_VSN[] = {
	0x09, 0xBC,
};

static u8 NT36575_A21S_I2C_EXIT_VSP[] = {
	0x09, 0xB8,
};

static DEFINE_STATIC_PACKET(nt36575_a21s_i2c_init, I2C_PKT_TYPE_WR, NT36575_A21S_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(nt36575_a21s_i2c_exit_vsn, I2C_PKT_TYPE_WR, NT36575_A21S_I2C_EXIT_VSN, 0);
static DEFINE_STATIC_PACKET(nt36575_a21s_i2c_exit_vsp, I2C_PKT_TYPE_WR, NT36575_A21S_I2C_EXIT_VSP, 0);
static DEFINE_STATIC_PACKET(nt36575_a21s_i2c_dump, I2C_PKT_TYPE_RD, NT36575_A21S_I2C_INIT, 0);

static void *nt36575_a21s_init_cmdtbl[] = {
	&PKTINFO(nt36575_a21s_i2c_init),
};

static void *nt36575_a21s_exit_cmdtbl[] = {
	&PKTINFO(nt36575_a21s_i2c_exit_vsn),
	&DLYINFO(a21s_wait_blic_off),
	&PKTINFO(nt36575_a21s_i2c_exit_vsp),
};

static void *nt36575_a21s_dump_cmdtbl[] = {
	&PKTINFO(nt36575_a21s_i2c_dump),
};


static struct seqinfo a21s_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", a21s_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", a21s_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", a21s_set_bl_cmdtbl),
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", a21s_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", a21s_exit_cmdtbl),
#ifdef CONFIG_SUPPORT_I2C
	[PANEL_I2C_INIT_SEQ] = SEQINFO_INIT("i2c-init-seq", nt36575_a21s_init_cmdtbl),
	[PANEL_I2C_EXIT_SEQ] = SEQINFO_INIT("i2c-exit-seq", nt36575_a21s_exit_cmdtbl),
	[PANEL_I2C_DUMP_SEQ] = SEQINFO_INIT("i2c-dump-seq", nt36575_a21s_dump_cmdtbl),
#endif

};

struct common_panel_info nt36575_a21s_default_panel_info = {
	.ldi_name = "nt36575",
	.name = "nt36575_a21s_default",
	.model = "CSOT_6_55_inch",
	.vendor = "CSO",
	.id = 0x0A7230,
	.rev = 0,
	.ddi_props = {
		.gpara = 0,
		.err_fg_recovery = false,
		.init_seq_by_lpdt = 1,
	},
	.mres = {
		.nr_resol = ARRAY_SIZE(nt36575_a21s_resol),
		.resol = nt36575_a21s_resol,
	},
	.maptbl = a21s_maptbl,
	.nr_maptbl = ARRAY_SIZE(a21s_maptbl),
	.seqtbl = a21s_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(a21s_seqtbl),
	.rditbl = nt36575_rditbl,
	.nr_rditbl = ARRAY_SIZE(nt36575_rditbl),
	.restbl = nt36575_restbl,
	.nr_restbl = ARRAY_SIZE(nt36575_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		&nt36575_a21s_panel_dimming_info,
	},
	.i2c_data = &nt36575_a21s_i2c_data,
};

static int __init nt36575_a21s_panel_init(void)
{
	register_common_panel(&nt36575_a21s_default_panel_info);

	return 0;
}
arch_initcall(nt36575_a21s_panel_init)
#endif /* __NT36575_A21S_PANEL_H__ */