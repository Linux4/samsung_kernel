/*
 * drivers/video/decon/panels/S6E3HA0_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "s6e3ha3_wqhd_aid_dimming.h"
#endif
#include "s6e3ha3_s6e3ha2_wqhd_param.h"
#include "../dsim.h"
#include <video/mipi_display.h>

extern unsigned dynamic_lcd_type;
static unsigned int hw_rev = 0; // for dcdc set


unsigned char ELVSS_LEN;
unsigned char ELVSS_REG;
unsigned char ELVSS_CMD_CNT;
unsigned char AID_CMD_CNT;
unsigned char AID_REG_OFFSET;
unsigned char TSET_LEN;
unsigned char TSET_REG;
unsigned char TSET_MINUS_OFFSET;
unsigned char VINT_REG2;

#if defined(CONFIG_LCD_RES) && defined(CONFIG_FB_DSU)
#error cannot use both of CONFIG_LCD_RES and CONFIG_FB_DSU
#endif

#ifdef CONFIG_ALWAYS_RELOAD_MTP_FACTORY_BUILD
void update_mdnie_coordinate( u16 coordinate0, u16 coordinate1 );
static int lcd_reload_mtp(int lcd_type, struct dsim_device *dsim);
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = { SEQ_HBM_OFF, SEQ_HBM_ON };
static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = { S6E3HA3_SEQ_ACL_OFF, S6E3HA3_SEQ_ACL_ON };
static const unsigned char *ACL_OPR_TABLE_HA2[ACL_OPR_MAX] = { S6E3HA2_SEQ_ACL_OFF_OPR, S6E3HA2_SEQ_ACL_ON_OPR_8, S6E3HA2_SEQ_ACL_ON_OPR_15 };
static const unsigned char *ACL_OPR_TABLE_HA3[ACL_OPR_MAX] = { S6E3HA3_SEQ_ACL_OFF_OPR, S6E3HA3_SEQ_ACL_ON_OPR_8, S6E3HA3_SEQ_ACL_ON_OPR_15 };
static const unsigned char *ACL_OPR_TABLE_HF3[ACL_OPR_MAX] = { S6E3HF3_SEQ_ACL_OFF_OPR, S6E3HF3_SEQ_ACL_ON_OPR_8, S6E3HF3_SEQ_ACL_ON_OPR_15 };

static const unsigned int br_tbl[EXTEND_BRIGHTNESS + 1] = {
	2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,   // 16
	16, 17, 18, 19, 20, 21, 22, 23, 25, 27, 29, 31, 33, 36, // 14
	39, 41, 41, 44, 44, 47, 47, 50, 50, 53, 53, 56, 56, 56, // 14
	60, 60, 60, 64, 64, 64, 68, 68, 68, 72, 72, 72, 72, 77, // 14
	77, 77, 82, 82, 82, 82, 87, 87, 87, 87, 93, 93, 93, 93, // 14
	98, 98, 98, 98, 98, 105, 105, 105, 105, 111, 111, 111,  // 12
	111, 111, 111, 119, 119, 119, 119, 119, 126, 126, 126,  // 11
	126, 126, 126, 134, 134, 134, 134, 134, 134, 134, 143,
	143, 143, 143, 143, 143, 152, 152, 152, 152, 152, 152,
	152, 152, 162, 162, 162, 162, 162, 162, 162, 172, 172,
	172, 172, 172, 172, 172, 172, 183, 183, 183, 183, 183,
	183, 183, 183, 183, 195, 195, 195, 195, 195, 195, 195,
	195, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
	220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 234,
	234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 249,
	249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
	265, 265, 265, 265, 265, 265, 265, 265, 265, 265, 265,
	265, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
	282, 282, 282, 300, 300, 300, 300, 300, 300, 300, 300,
	300, 300, 300, 300, 316, 316, 316, 316, 316, 316, 316,
	316, 316, 316, 316, 316, 333, 333, 333, 333, 333, 333,
	333, 333, 333, 333, 333, 333, 360,       //7
	[256 ... 281] = 420,
	[282 ... 295] = 465,
	[296 ... 309] = 488,
	[310 ... 323] = 510,
	[324 ... 336] = 533,
	[337 ... 350] = 555,
	[351 ... 364] = 578,
	[365 ... 365] = 600
};
static const unsigned int br_tbl_420[EXTEND_BRIGHTNESS + 1] = {
	2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 19, 20, 21, 22, 24, 25, 25, 27, 29, 30, 32, 34, 37, 39,
	41, 44, 47, 47, 50, 50, 53, 56, 56, 60, 60, 64, 64, 68, 68, 68,
	72, 72, 72, 77, 77, 77, 82, 82, 82, 87, 87, 87, 87, 93, 93, 93,
	98, 98, 98, 98, 105, 105, 105, 105, 111, 111, 111, 111, 119, 119, 119, 119,
	119, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 134, 134, 134, 134, 134,
	143, 143, 143, 143, 143, 143, 152, 152, 152, 152, 152, 152, 152, 162, 162, 162,
	162, 162, 162, 172, 172, 172, 172, 172, 172, 172, 172, 183, 183, 183, 183, 183,
	183, 183, 195, 195, 195, 195, 195, 195, 195, 195, 207, 207, 207, 207, 207, 207,
	207, 207, 207, 220, 220, 220, 220, 220, 220, 220, 234, 234, 234, 234, 234, 234,
	234, 234, 234, 249, 249, 249, 249, 249, 249, 249, 249, 249, 265, 265, 265, 265,
	265, 265, 265, 265, 265, 265, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
	300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 316, 316, 316, 316, 316, 316,
	316, 316, 316, 316, 316, 316, 333, 333, 333, 333, 350, 350, 350, 350, 350, 350,
	350, 350, 350, 357, 357, 357, 357, 357, 372, 372, 372, 372, 372, 372, 380, 380,
	380, 380, 387, 387, 387, 395, 395, 395, 395, 395, 403, 403, 403, 412, 412, 420,
	[256 ... 281] = 420,
	[282 ... 295] = 465,
	[296 ... 309] = 488,
	[310 ... 323] = 510,
	[324 ... 336] = 533,
	[337 ... 350] = 555,
	[351 ... 364] = 578,
	[365 ... 365] = 600
};

static unsigned int nb_br_tbl_420[EXTEND_BRIGHTNESS + 1] = {
        2,        3,        4,        4,        5,        6,        7,        8,        9,        10,        11,        12,
        13,        14,        15,        16,        17,        19,        19,        20,        21,        22,        24,
        25,        27,        27,        29,        30,        32,        32,        34,        34,        37,        37,
        39,        41,        41,        44,        44,        47,        47,        47,        50,        50,        53,
        53,        56,        56,        60,        60,        60,        64,        64,        64,        68,        68,
        68,        72,        72,        77,        77,        77,        77,        82,        82,        82,        87,
        87,        87,        87,        93,        93,        93,        93,        98,        98,        98,        105,
        105,        105,        105,        111,        111,        111,        111,        119,        119,        119,
        119,        119,        126,        126,        126,        126,        126,        134,        134,        134,
        134,        134,        143,        143,        143,        143,        143,        152,        152,        152,
        152,        152,        152,        162,        162,        162,        162,        162,        162,        172,
        172,        172,        172,        172,        183,        183,        183,        183,        183,        183,
        183,        195,        195,        195,        195,        195,        195,        195,        207,        207,
        207,        207,        207,        207,        207,        220,        220,        220,        220,        220,
        220,        220,        234,        234,        234,        234,        234,        234,        234,        234,
        249,        249,        249,        249,        249,        249,        249,        249,        265,        265,
        265,        265,        265,        265,        265,        265,        265,        282,        282,        282,
        282,        282,        282,        282,        282,        282,        300,        300,        300,        300,
        300,        300,        300,        300,        300,        300,        316,        316,        316,        316,
        316,        316,        316,        316,        333,        333,        333,        333,        333,        333,
        333,        333,        333,        350,        350,        350,        350,        350,        350,        350,
        350,        350,        357,        357,        357,        357,        365,        365,        365,        365,
        372,        372,        372,        380,        380,        380,        380,        387,        387,        387,
        387,        395,        395,        395,        395,        403,        403,        403,        403,        412,
        412,        412,        412,        420,        420,        420,        420,        420,
	[256 ... 281] = 420,
	[282 ... 295] = 465,
	[296 ... 309] = 488,
	[310 ... 323] = 510,
	[324 ... 336] = 533,
	[337 ... 350] = 555,
	[351 ... 364] = 578,
	[365 ... 365] = 600
};

static unsigned int zen_br_tbl_420[EXTEND_BRIGHTNESS + 1] ={
        2,        3,        4,        4,        5,        6,        7,        8,        9,        10,        11,        12,
        13,        14,        15,        16,        19,        19,        19,        20,        21,        22,        24,
        25,        27,        27,        29,        30,        32,        32,        34,        34,        37,        37,
        41,        41,        41,        44,        44,        47,        47,        47,        50,        50,        53,
        53,        56,        56,        60,        60,        60,        64,        64,        64,        68,        68,
        68,        72,        72,        77,        77,        77,        77,        82,        82,        82,        87,
        87,        87,        87,        93,        93,        93,        93,        98,        98,        98,        105,
        105,        105,        105,        111,        111,        111,        111,        119,        119,        119,
        119,        119,        126,        126,        126,        126,        126,        134,        134,        134,
        134,        134,        143,        143,        143,        143,        143,        152,        152,        152,
        152,        152,        152,        162,        162,        162,        162,        162,        162,        172,
        172,        172,        172,        172,        183,        183,        183,        183,        183,        183,
        183,        195,        195,        195,        195,        195,        195,        195,        207,        207,
        207,        207,        207,        207,        207,        220,        220,        220,        220,        220,
        220,        220,        234,        234,        234,        234,        234,        234,        234,        234,
        249,        249,        249,        249,        249,        249,        249,        249,        265,        265,
        265,        265,        265,        265,        265,        265,        265,        282,        282,        282,
        282,        282,        282,        282,        282,        282,        300,        300,        300,        300,
        300,        300,        300,        300,        300,        300,        316,        316,        316,        316,
        316,        316,        316,        316,        333,        333,        333,        333,        333,        333,
        333,        333,        333,        350,        350,        350,        350,        350,        350,        350,
        350,        350,        357,        357,        357,        357,        365,        365,        365,        365,
        372,        372,        372,        380,        380,        380,        380,        387,        387,        387,
        387,        395,        395,        395,        395,        403,        403,        403,        403,        412,
        412,        412,        412,        420,        420,        420,        420,        420,
	[256 ... 281] = 420,
	[282 ... 295] = 465,
	[296 ... 309] = 488,
	[310 ... 323] = 510,
	[324 ... 336] = 533,
	[337 ... 350] = 555,
	[351 ... 364] = 578,
	[365 ... 365] = 600
};


static unsigned int zen_br_tbl_420_a3[EXTEND_BRIGHTNESS + 1] ={
	2,	3,	4,	4,	5,	6,	7,	8,
	9,	10,  11,  12,  13,	14,  15,  16,
	17,  19,  19,  20,	21,  22,  24,  25,
	27,  27,  29,  30,	32,  32,  34,  34,
	37,  37,  41,  41,	41,  44,  44,  47,
	47,  47,  50,  50,	53,  53,  56,  56,
	60,  60,  60,  64,	64,  64,  68,  68,
	68,  72,  72,  77,	77,  77,  77,  82,
	82,  82,  87,  87,	87,  87,  93,  93,
	93,  93,  98,  98,	98,  105,  105,  105,
	105,  111,	111,  111,	111,  119,	119,  119,
	119,  119,	126,  126,	126,  126,	126,  134,
	134,  134,	134,  134,	143,  143,	143,  143,
	143,  152,	152,  152,	152,  152,	152,  162,
	162,  162,	162,  162,	162,  172,	172,  172,
	172,  172,	183,  183,	183,  183,	183,  183,
	183,  195,	195,  195,	195,  195,	195,  195,
	207,  207,	207,  207,	207,  207,	207,  220,
	220,  220,	220,  220,	220,  220,	234,  234,
	234,  234,	234,  234,	234,  234,	249,  249,
	249,  249,	249,  249,	249,  249,	265,  265,
	265,  265,	265,  265,	265,  265,	265,  282,
	282,  282,	282,  282,	282,  282,	282,  282,
	300,  300,	300,  300,	300,  300,	300,  300,
	300,  300,	316,  316,	316,  316,	316,  316,
	316,  316,	333,  333,	333,  333,	333,  333,
	333,  333,	333,  350,	350,  350,	350,  350,
	350,  350,	350,  350,	357,  357,	357,  357,
	365,  365,	365,  365,	372,  372,	372,  380,
	380,  380,	380,  387,	387,  387,	387,  395,
	395,  395,	395,  403,	403,  403,	403,  412,
	412,  412,	412,  420,	420,  420,	420,  420,
	[256 ... 281] = 420,
	[282 ... 295] = 465,
	[296 ... 309] = 488,
	[310 ... 323] = 510,
	[324 ... 336] = 533,
	[337 ... 350] = 555,
	[351 ... 364] = 578,
	[365 ... 365] = 600
};

static unsigned char nb_inter_aor_tbl [512] = {
        0x90, 0xE5,        0x90, 0xD1,        0x90, 0xC6,        0x90, 0xBB,        0x90, 0xA0,
        0x90, 0x82,        0x90, 0x72,        0x90, 0x5C,        0x90, 0x42,        0x90, 0x32,
        0x90, 0x21,        0x90, 0x11,        0x80, 0xF6,        0x80, 0xE5,        0x80, 0xD5,
        0x80, 0xC1,        0x80, 0xAB,        0x80, 0x96,        0x80, 0x80,        0x80, 0x68,
        0x80, 0x52,        0x80, 0x3F,        0x80, 0x12,        0x70, 0xFC,        0x70, 0xE6,
        0x70, 0xD1,        0x70, 0xAB,        0x70, 0x94,        0x70, 0x80,        0x70, 0x6C,
        0x70, 0x56,        0x70, 0x41,        0x70, 0x1F,        0x60, 0xFD,        0x60, 0xD1,
        0x60, 0xBA,        0x60, 0xA2,        0x60, 0x81,        0x60, 0x61,        0x60, 0x4B,
        0x60, 0x34,        0x60, 0x1E,        0x50, 0xFA,        0x50, 0xD6,        0x50, 0xB5,
        0x50, 0x93,        0x50, 0x70,        0x50, 0x4D,        0x50, 0x2F,        0x50, 0x10,
        0x40, 0xF2,        0x40, 0xD3,        0x40, 0xB3,        0x40, 0x94,        0x40, 0x75,
        0x40, 0x55,        0x40, 0x36,        0x40, 0x05,        0x30, 0xD5,        0x40, 0x28,
        0x30, 0xFE,        0x30, 0xEA,        0x30, 0xD5,        0x40, 0x06,        0x30, 0xF2,
        0x30, 0xCB,        0x40, 0x15,        0x30, 0xF0,        0x30, 0xDE,        0x30, 0xCB,
        0x40, 0x24,        0x40, 0x13,        0x30, 0xF1,        0x30, 0xE0,        0x30, 0xFE,
        0x30, 0xEE,        0x30, 0xCD,        0x40, 0x22,        0x40, 0x04,        0x30, 0xF4,
        0x30, 0xD6,        0x40, 0x19,        0x40, 0x0B,        0x30, 0xEE,        0x30, 0xE0,
        0x40, 0x30,        0x40, 0x23,        0x40, 0x08,        0x30, 0xFB,        0x30, 0xE0,
        0x40, 0x18,        0x30, 0xFE,        0x30, 0xF1,        0x30, 0xD8,        0x30, 0xCB,
        0x40, 0x18,        0x40, 0x0C,        0x30, 0xF4,        0x30, 0xE8,        0x30, 0xD0,
        0x40, 0x1A,        0x40, 0x03,        0x30, 0xF8,        0x30, 0xE2,        0x30, 0xCB,
        0x40, 0x19,        0x40, 0x0E,        0x30, 0xF9,        0x30, 0xEF,        0x30, 0xDA,
        0x30, 0xCF,        0x40, 0x25,        0x40, 0x1B,        0x40, 0x07,        0x30, 0xF4,
        0x30, 0xEA,        0x30, 0xD6,        0x40, 0x2A,        0x40, 0x17,        0x40, 0x05,
        0x30, 0xF2,        0x30, 0xE0,        0x40, 0x1F,        0x40, 0x16,        0x40, 0x05,
        0x30, 0xFC,        0x30, 0xEA,        0x30, 0xE2,        0x30, 0xD0,        0x30, 0xBE,
        0x30, 0xAC,        0x30, 0x9A,        0x30, 0x88,        0x30, 0x76,        0x30, 0x64,
        0x30, 0x52,        0x30, 0x43,        0x30, 0x33,        0x30, 0x24,        0x30, 0x14,
        0x30, 0x05,        0x20, 0xF5,        0x20, 0xE6,        0x20, 0xD1,        0x20, 0xBC,
        0x20, 0xA7,        0x20, 0x91,        0x20, 0x7C,        0x20, 0x67,        0x20, 0x52,
        0x20, 0x40,        0x20, 0x2E,        0x20, 0x1B,        0x20, 0x09,        0x10, 0xF7,
        0x10, 0xE5,        0x10, 0xD2,        0x10, 0xC0,        0x10, 0xAB,        0x10, 0x96,
        0x10, 0x80,        0x10, 0x6C,        0x10, 0x56,        0x10, 0x41,        0x10, 0x2C,
        0x10, 0x17,        0x10, 0x7E,        0x10, 0x6C,        0x10, 0x63,        0x10, 0x52,
        0x10, 0x40,        0x10, 0x2F,        0x10, 0x26,        0x10, 0x15,        0x10, 0x03,
        0x10, 0x7E,        0x10, 0x6E,        0x10, 0x5E,        0x10, 0x4D,        0x10, 0x45,
        0x10, 0x34,        0x10, 0x24,        0x10, 0x13,        0x10, 0x03,        0x10, 0x7F,
        0x10, 0x6F,        0x10, 0x68,        0x10, 0x58,        0x10, 0x49,        0x10, 0x39,
        0x10, 0x2A,        0x10, 0x22,        0x10, 0x13,        0x10, 0x03,        0x10, 0x6A,
        0x10, 0x5B,        0x10, 0x4C,        0x10, 0x3E,        0x10, 0x2F,        0x10, 0x20,
        0x10, 0x12,        0x10, 0x03,        0x10, 0x6C,        0x10, 0x5E,        0x10, 0x50,
        0x10, 0x42,        0x10, 0x3B,        0x10, 0x2D,        0x10, 0x1F,        0x10, 0x11,
        0x10, 0x03,        0x10, 0x67,        0x10, 0x59,        0x10, 0x4C,        0x10, 0x3F,
        0x10, 0x38,        0x10, 0x2B,        0x10, 0x1E,        0x10, 0x10,        0x10, 0x03,
        0x10, 0x24,        0x10, 0x17,        0x10, 0x10,        0x10, 0x03,        0x10, 0x29,
        0x10, 0x1C,        0x10, 0x10,        0x10, 0x03,        0x00, 0xED,        0x00, 0xD6,
        0x00, 0xC0,        0x00, 0xB5,        0x00, 0xAA,        0x00, 0x9F,        0x00, 0x94,
        0x00, 0x89,        0x00, 0x7E,        0x00, 0x72,        0x00, 0x67,        0x00, 0x56,
        0x00, 0x45,        0x00, 0x33,        0x00, 0x22,        0x00, 0x30,        0x00, 0x24,
        0x00, 0x17,        0x00, 0x0A,        0x00, 0x36,        0x00, 0x23,        0x00, 0x17,
        0x00, 0x0A,        0x00, 0x2F,        0x00, 0x29,        0x00, 0x1C,        0x00, 0x0A,
        0x00, 0x0A,
};


static unsigned char zen_inter_aor_tbl [512] = {
        0x90,0xDC,        0x90,0xC1,        0x90,0xB1,        0x90,0xA2,        0x90,0x85,
        0x90,0x6B,        0x90,0x52,        0x90,0x3B,        0x90,0x22,        0x90,0x0B,
        0x80,0xFA,        0x80,0xE2,        0x80,0xD0,        0x80,0xBC,        0x80,0xA4,
        0x80,0x8B,        0x80,0x73,        0x80,0x5C,        0x80,0x44,        0x80,0x2F,
        0x80,0x12,        0x70,0xFF,        0x70,0xCB,        0x70,0xB1,        0x70,0x99,
        0x70,0x80,        0x70,0x4C,        0x70,0x33,        0x70,0x1B,        0x70,0x04,
        0x60,0xEA,        0x60,0xD0,        0x60,0xAB,        0x60,0x85,        0x60,0x4F,
        0x60,0x37,        0x60,0x1F,        0x50,0xF8,        0x50,0xD1,        0x50,0xB7,
        0x50,0x9D,        0x50,0x83,        0x50,0x6D,        0x50,0x56,        0x50,0x1E,
        0x40,0xE6,        0x40,0xC3,        0x40,0x9F,        0x40,0x88,        0x40,0x72,
        0x40,0x5B,        0x40,0x26,        0x30,0xF1,        0x30,0xBB,        0x40,0x03,
        0x30,0xD3,        0x30,0xBB,        0x30,0xE8,        0x30,0xBB,        0x40,0x10,
        0x30,0xE5,        0x30,0xD0,        0x30,0xBB,        0x30,0xF7,        0x30,0xE3,
        0x30,0xBB,        0x40,0x06,        0x30,0xE0,        0x30,0xCE,        0x30,0xBB,
        0x40,0x01,        0x30,0xF0,        0x30,0xCD,        0x30,0xBB,        0x30,0xED,
        0x30,0xDC,        0x30,0xBB,        0x40,0x08,        0x30,0xEA,        0x30,0xDA,
        0x30,0xBB,        0x30,0xF6,        0x30,0xE7,        0x30,0xCA,        0x30,0xBB,
        0x40,0x0D,        0x30,0xFF,        0x30,0xE4,        0x30,0xD6,        0x30,0xBB,
        0x40,0x08,        0x30,0xEF,        0x30,0xE2,        0x30,0xC8,        0x30,0xBB,
        0x40,0x04,        0x30,0xF8,        0x30,0xDF,        0x30,0xD3,        0x30,0xBB,
        0x40,0x0B,        0x30,0xF4,        0x30,0xE9,        0x30,0xD2,        0x30,0xBB,
        0x40,0x06,        0x30,0xFB,        0x30,0xE6,        0x30,0xDB,        0x30,0xC6,
        0x30,0xBB,        0x40,0x0B,        0x40,0x01,        0x30,0xED,        0x30,0xD9,
        0x30,0xCF,        0x30,0xBB,        0x40,0x07,        0x30,0xF4,        0x30,0xE1,
        0x30,0xCE,        0x30,0xBB,        0x40,0x0B,        0x40,0x02,        0x30,0xF0,
        0x30,0xE8,        0x30,0xD6,        0x30,0xCD,        0x30,0xBB,        0x30,0xA2,
        0x30,0x89,        0x30,0x6F,        0x30,0x56,        0x30,0x3D,        0x30,0x23,
        0x30,0x0A,        0x20,0xFB,        0x20,0xEB,        0x20,0xDC,        0x20,0xCD,
        0x20,0xBE,        0x20,0xAE,        0x20,0x9F,        0x20,0x8A,        0x20,0x74,
        0x20,0x5F,        0x20,0x4A,        0x20,0x35,        0x20,0x1F,        0x20,0x0A,
        0x10,0xF6,        0x10,0xE2,        0x10,0xCE,        0x10,0xBA,        0x10,0xA6,
        0x10,0x92,        0x10,0x7E,        0x10,0x6A,        0x10,0x5D,        0x10,0x50,
        0x10,0x43,        0x10,0x37,        0x10,0x2A,        0x10,0x1D,        0x10,0x10,
        0x10,0x03,        0x10,0x7E,        0x10,0x6C,        0x10,0x63,        0x10,0x52,
        0x10,0x40,        0x10,0x2F,        0x10,0x26,        0x10,0x15,        0x10,0x03,
        0x10,0x7E,        0x10,0x6E,        0x10,0x5E,        0x10,0x4D,        0x10,0x45,
        0x10,0x34,        0x10,0x24,        0x10,0x13,        0x10,0x03,        0x10,0x7F,
        0x10,0x6F,        0x10,0x68,        0x10,0x58,        0x10,0x49,        0x10,0x39,
        0x10,0x2A,        0x10,0x22,        0x10,0x13,        0x10,0x03,        0x10,0x6A,
        0x10,0x5B,        0x10,0x4C,        0x10,0x3E,        0x10,0x2F,        0x10,0x20,
        0x10,0x12,        0x10,0x03,        0x10,0x6C,        0x10,0x5E,        0x10,0x50,
        0x10,0x42,        0x10,0x3B,        0x10,0x2D,        0x10,0x1F,        0x10,0x11,
        0x10,0x03,        0x10,0x67,        0x10,0x59,        0x10,0x4C,        0x10,0x3F,
        0x10,0x38,        0x10,0x2B,        0x10,0x1E,        0x10,0x10,        0x10,0x03,
        0x10,0x24,        0x10,0x17,        0x10,0x10,        0x10,0x03,        0x10,0x29,
        0x10,0x1C,        0x10,0x10,        0x10,0x03,        0x00,0xEE,        0x00,0xDB,
        0x00,0xCE,        0x00,0xC1,        0x00,0xB5,        0x00,0xA8,        0x00,0x9B,
        0x00,0x87,        0x00,0x7A,        0x00,0x74,        0x00,0x67,        0x00,0x5B,
        0x00,0x4E,        0x00,0x41,        0x00,0x34,        0x00,0x30,        0x00,0x24,
        0x00,0x17,        0x00,0x0A,        0x00,0x36,        0x00,0x23,        0x00,0x17,
        0x00,0x0A,        0x00,0x2F,        0x00,0x29,        0x00,0x1C,        0x00,0x0A,
        0x00,0x0A,
};

static unsigned char zen_inter_aor_tbl_revd[512] = {
	0x90,0xDE, 0x90,0xBF, 0x90,0xAF, 0x90,0x9F, 0x90,0x80, 0x90,0x66, 0x90,0x4F, 0x90,0x35, 0x90,0x20, 0x90,0x0B,
	0x80,0xF4, 0x80,0xE1, 0x80,0xCF, 0x80,0xBC, 0x80,0xA4, 0x80,0x8F, 0x80,0x75, 0x80,0x5B, 0x80,0x41, 0x80,0x2B,
	0x80,0x11, 0x70,0xF5, 0x70,0xC7, 0x70,0xAB, 0x70,0x93, 0x70,0x7B, 0x70,0x45, 0x70,0x30, 0x70,0x19, 0x70,0x01,
	0x60,0xE7, 0x60,0xCC, 0x60,0xA4, 0x60,0x7C, 0x60,0x50, 0x60,0x36, 0x60,0x1B, 0x50,0xF4, 0x50,0xCC, 0x50,0xB1,
	0x50,0x96, 0x50,0x7B, 0x50,0x56, 0x50,0x30, 0x50,0x09, 0x40,0xE2, 0x40,0xB7, 0x40,0x8B, 0x40,0x64, 0x40,0x3C,
	0x40,0x15, 0x40,0x1C, 0x30,0xEA, 0x30,0xD1, 0x40,0x19, 0x30,0xEA, 0x30,0xD3, 0x30,0xFF, 0x30,0xD3, 0x40,0x26,
	0x30,0xFC, 0x30,0xE8, 0x30,0xD3, 0x40,0x0C, 0x30,0xF8, 0x30,0xD1, 0x40,0x1B, 0x30,0xF6, 0x30,0xE3, 0x30,0xD1,
	0x40,0x16, 0x40,0x05, 0x30,0xE2, 0x30,0xD1, 0x40,0x02, 0x30,0xF2, 0x30,0xD1, 0x40,0x1D, 0x30,0xFF, 0x30,0xF0,
	0x30,0xD1, 0x40,0x0B, 0x30,0xFC, 0x30,0xDF, 0x30,0xD1, 0x40,0x22, 0x40,0x14, 0x30,0xF9, 0x30,0xEC, 0x30,0xD1,
	0x40,0x1D, 0x40,0x04, 0x30,0xF7, 0x30,0xDE, 0x30,0xD1, 0x40,0x19, 0x40,0x0D, 0x30,0xF5, 0x30,0xE9, 0x30,0xD1,
	0x40,0x1F, 0x40,0x09, 0x30,0xFE, 0x30,0xE7, 0x30,0xD1, 0x40,0x1B, 0x40,0x10, 0x30,0xFB, 0x30,0xF1, 0x30,0xDC,
	0x30,0xD1, 0x40,0x20, 0x40,0x16, 0x40,0x02, 0x30,0xEF, 0x30,0xE5, 0x30,0xD1, 0x40,0x05, 0x30,0xF2, 0x30,0xDF,
	0x30,0xCC, 0x30,0xBA, 0x40,0x20, 0x40,0x17, 0x40,0x06, 0x30,0xFD, 0x30,0xEB, 0x30,0xE3, 0x30,0xD1, 0x30,0xC0,
	0x30,0xAF, 0x30,0x9F, 0x30,0x8E, 0x30,0x7D, 0x30,0x6C, 0x30,0x5B, 0x30,0x49, 0x30,0x38, 0x30,0x26, 0x30,0x14,
	0x30,0x02, 0x20,0xF1, 0x20,0xDF, 0x20,0xCB, 0x20,0xB7, 0x20,0xA3, 0x20,0x8E, 0x20,0x7A, 0x20,0x66, 0x20,0x52,
	0x20,0x3F, 0x20,0x2D, 0x20,0x1A, 0x20,0x08, 0x10,0xF5, 0x10,0xE2, 0x10,0xCF, 0x10,0xBD, 0x10,0xAB, 0x10,0x9A,
	0x10,0x88, 0x10,0x77, 0x10,0x66, 0x10,0x54, 0x10,0x43, 0x10,0x31, 0x10,0x7D, 0x10,0x6C, 0x10,0x63, 0x10,0x52,
	0x10,0x40, 0x10,0x2F, 0x10,0x26, 0x10,0x14, 0x10,0x03, 0x10,0x7E, 0x10,0x6E, 0x10,0x5D, 0x10,0x4D, 0x10,0x45,
	0x10,0x34, 0x10,0x24, 0x10,0x13, 0x10,0x03, 0x10,0x7E, 0x10,0x6F, 0x10,0x67, 0x10,0x58, 0x10,0x48, 0x10,0x39,
	0x10,0x2A, 0x10,0x22, 0x10,0x12, 0x10,0x03, 0x10,0x6A, 0x10,0x5B, 0x10,0x4C, 0x10,0x3E, 0x10,0x2F, 0x10,0x20,
	0x10,0x12, 0x10,0x03, 0x10,0x6B, 0x10,0x5D, 0x10,0x4F, 0x10,0x42, 0x10,0x3B, 0x10,0x2D, 0x10,0x1F, 0x10,0x11,
	0x10,0x03, 0x10,0x66, 0x10,0x59, 0x10,0x4C, 0x10,0x3E, 0x10,0x38, 0x10,0x2B, 0x10,0x1D, 0x10,0x10, 0x10,0x03,
	0x10,0x23, 0x10,0x16, 0x10,0x10, 0x10,0x03, 0x10,0x29, 0x10,0x1C, 0x10,0x10, 0x10,0x03, 0x00,0xF1, 0x00,0xE0,
	0x00,0xCE, 0x00,0xC2, 0x00,0xB5, 0x00,0xA9, 0x00,0x9C, 0x00,0x8E, 0x00,0x81, 0x00,0x73, 0x00,0x65, 0x00,0x53,
	0x00,0x41, 0x00,0x2F, 0x00,0x1D, 0x00,0x30, 0x00,0x24, 0x00,0x17, 0x00,0x0A, 0x00,0x36, 0x00,0x23, 0x00,0x17,
	0x00,0x0A, 0x00,0x2F, 0x00,0x29, 0x00,0x1C, 0x00,0x0A, 0x00,0x0A
};

static unsigned char zen_inter_aor_tbl_a3[512] = {
	0x90, 0xE1,   0x90, 0xCB,	0x90, 0xBE,   0x90, 0xB0,	0x90, 0x9A,   0x90, 0x80,	0x90, 0x64,   0x90, 0x4E,	0x90, 0x34,   0x90, 0x1C,	0x90, 0x07,   0x80, 0xF3,	0x80, 0xE1,   0x80, 0xCF,	0x80, 0xB5,   0x80, 0x9B,
	0x80, 0x87,   0x80, 0x6C,	0x80, 0x51,   0x80, 0x41,	0x80, 0x27,   0x80, 0x0F,	0x70, 0xD7,   0x70, 0xC5,	0x70, 0xAD,   0x70, 0x95,	0x70, 0x65,   0x70, 0x53,	0x70, 0x3A,   0x70, 0x21,	0x70, 0x09,   0x60, 0xF1,
	0x60, 0xCC,   0x60, 0xA7,	0x60, 0x7B,   0x60, 0x5F,	0x60, 0x43,   0x60, 0x1D,	0x50, 0xF7,   0x50, 0xE0,	0x50, 0xC8,   0x50, 0xB1,	0x50, 0x87,   0x50, 0x5D,	0x50, 0x38,   0x50, 0x13,	0x40, 0xED,   0x40, 0xC7,
	0x40, 0xA5,   0x40, 0x83,	0x40, 0x61,   0x40, 0x3D,	0x40, 0x18,   0x30, 0xF4,	0x40, 0x31,   0x40, 0x13,	0x30, 0xDC,   0x40, 0x08,	0x30, 0xDC,   0x40, 0x29,	0x40, 0x10,   0x30, 0xF6,	0x30, 0xDC,   0x40, 0x1D,
	0x30, 0xFC,   0x30, 0xDC,	0x40, 0x21,   0x40, 0x0A,	0x30, 0xF3,   0x30, 0xDC,	0x40, 0x29,   0x40, 0x0F,	0x30, 0xF6,   0x30, 0xDC,	0x40, 0x12,   0x30, 0xF7,	0x30, 0xDC,   0x40, 0x2B,	0x40, 0x11,   0x30, 0xF6,
	0x30, 0xDC,   0x40, 0x1C,	0x40, 0x07,   0x30, 0xF1,	0x30, 0xDC,   0x40, 0x32,	0x40, 0x1C,   0x40, 0x07,	0x30, 0xF1,   0x30, 0xDC,	0x40, 0x23,   0x40, 0x11,	0x30, 0xFF,   0x30, 0xEE,	0x30, 0xDC,   0x40, 0x28,
	0x40, 0x15,   0x40, 0x02,	0x30, 0xEF,   0x30, 0xDC,	0x40, 0x2C,   0x40, 0x18,	0x40, 0x04,   0x30, 0xF0,	0x30, 0xDC,   0x40, 0x2A,	0x40, 0x1B,   0x40, 0x0B,	0x30, 0xFB,   0x30, 0xEC,	0x30, 0xDC,   0x40, 0x2E,
	0x40, 0x1D,   0x40, 0x0D,	0x30, 0xFD,   0x30, 0xEC,	0x30, 0xDC,   0x40, 0x26,	0x40, 0x13,   0x40, 0x01,	0x30, 0xEE,   0x30, 0xDC,	0x40, 0x2E,   0x40, 0x20,	0x40, 0x13,   0x40, 0x05,	0x30, 0xF7,   0x30, 0xEA,
	0x30, 0xDC,   0x30, 0xCB,	0x30, 0xBA,   0x30, 0xA9,	0x30, 0x98,   0x30, 0x87,	0x30, 0x76,   0x30, 0x65,	0x30, 0x55,   0x30, 0x46,	0x30, 0x36,   0x30, 0x27,	0x30, 0x17,   0x30, 0x08,	0x20, 0xF8,   0x20, 0xE4,
	0x20, 0xCF,   0x20, 0xBB,	0x20, 0xA6,   0x20, 0x92,	0x20, 0x7D,   0x20, 0x69,	0x20, 0x56,   0x20, 0x44,	0x20, 0x31,   0x20, 0x1F,	0x20, 0x0D,   0x10, 0xFA,	0x10, 0xE8,   0x10, 0xD5,	0x10, 0xC3,   0x10, 0xB1,
	0x10, 0x9F,   0x10, 0x8D,	0x10, 0x7B,   0x10, 0x69,	0x10, 0x57,   0x10, 0x45,	0x10, 0xBE,   0x10, 0xAF,	0x10, 0xA0,   0x10, 0x91,	0x10, 0x82,   0x10, 0x72,	0x10, 0x63,   0x10, 0x54,	0x10, 0x27,   0x10, 0xA1,
	0x10, 0x92,   0x10, 0x83,	0x10, 0x73,   0x10, 0x64,	0x10, 0x55,   0x10, 0x46,	0x10, 0x36,   0x10, 0x27,	0x10, 0xA2,   0x10, 0x95,	0x10, 0x87,   0x10, 0x79,	0x10, 0x6B,   0x10, 0x5E,	0x10, 0x50,   0x10, 0x42,
	0x10, 0x35,   0x10, 0x27,	0x10, 0x8C,   0x10, 0x7E,	0x10, 0x6F,   0x10, 0x61,	0x10, 0x52,   0x10, 0x44,	0x10, 0x35,   0x10, 0x27,	0x10, 0x8F,   0x10, 0x82,	0x10, 0x75,   0x10, 0x68,	0x10, 0x5B,   0x10, 0x4E,
	0x10, 0x41,   0x10, 0x34,	0x10, 0x27,   0x10, 0x8A,	0x10, 0x7D,   0x10, 0x71,	0x10, 0x65,   0x10, 0x58,	0x10, 0x4C,   0x10, 0x40,	0x10, 0x33,   0x10, 0x27,	0x10, 0x48,   0x10, 0x3D,	0x10, 0x32,   0x10, 0x27,
	0x10, 0x4C,   0x10, 0x40,	0x10, 0x33,   0x10, 0x27,	0x10, 0x15,   0x10, 0x04,	0x00, 0xF2,   0x00, 0xE7,	0x00, 0xDD,   0x00, 0xD2,	0x00, 0xC7,   0x00, 0xBA,	0x00, 0xAD,   0x00, 0xA0,	0x00, 0x93,   0x00, 0x83,
	0x00, 0x73,   0x00, 0x62,	0x00, 0x52,   0x00, 0x40,	0x00, 0x2E,   0x00, 0x1C,	0x00, 0x0A,   0x00, 0x34,	0x00, 0x26,   0x00, 0x18,	0x00, 0x0A,   0x00, 0x2F,	0x00, 0x23,   0x00, 0x16,	0x00, 0x0A,   0x00, 0x0A,
};


static const short center_gamma[NUM_VREF][CI_MAX] = {
	{ 0x000, 0x000, 0x000 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x080, 0x080, 0x080 },
	{ 0x100, 0x100, 0x100 },
};

// aid sheet in rev.d opmanual
struct SmtDimInfo dimming_info_HA3[MAX_BR_INFO] = { // add hbm array
	{ .br = 2, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl2nit, .cTbl = HA3_ctbl2nit, .aid = HA3_aid9818, .elvCaps = HA3_elvCaps5, .elv = HA3_elv5, .way = W1 },
	{ .br = 3, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl3nit, .cTbl = HA3_ctbl3nit, .aid = HA3_aid9740, .elvCaps = HA3_elvCaps4, .elv = HA3_elv4, .way = W1 },
	{ .br = 4, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl4nit, .cTbl = HA3_ctbl4nit, .aid = HA3_aid9655, .elvCaps = HA3_elvCaps3, .elv = HA3_elv3, .way = W1 },
	{ .br = 5, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl5nit, .cTbl = HA3_ctbl5nit, .aid = HA3_aid9550, .elvCaps = HA3_elvCaps2, .elv = HA3_elv2, .way = W1 },
	{ .br = 6, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl6nit, .cTbl = HA3_ctbl6nit, .aid = HA3_aid9434, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 7, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl7nit, .cTbl = HA3_ctbl7nit, .aid = HA3_aid9372, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 8, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl8nit, .cTbl = HA3_ctbl8nit, .aid = HA3_aid9287, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 9, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl9nit, .cTbl = HA3_ctbl9nit, .aid = HA3_aid9186, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 10, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl10nit, .cTbl = HA3_ctbl10nit, .aid = HA3_aid9124, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 11, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl11nit, .cTbl = HA3_ctbl11nit, .aid = HA3_aid9058, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 12, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl12nit, .cTbl = HA3_ctbl12nit, .aid = HA3_aid8996, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 13, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl13nit, .cTbl = HA3_ctbl13nit, .aid = HA3_aid8891, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 14, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl14nit, .cTbl = HA3_ctbl14nit, .aid = HA3_aid8826, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 15, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl15nit, .cTbl = HA3_ctbl15nit, .aid = HA3_aid8764, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 16, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl16nit, .cTbl = HA3_ctbl16nit, .aid = HA3_aid8686, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 17, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl17nit, .cTbl = HA3_ctbl17nit, .aid = HA3_aid8601, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 19, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl19nit, .cTbl = HA3_ctbl19nit, .aid = HA3_aid8434, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 20, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl20nit, .cTbl = HA3_ctbl20nit, .aid = HA3_aid8341, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 21, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl21nit, .cTbl = HA3_ctbl21nit, .aid = HA3_aid8256, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 22, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl22nit, .cTbl = HA3_ctbl22nit, .aid = HA3_aid8182, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 24, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl24nit, .cTbl = HA3_ctbl24nit, .aid = HA3_aid8008, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 25, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl25nit, .cTbl = HA3_ctbl25nit, .aid = HA3_aid7922, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 27, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl27nit, .cTbl = HA3_ctbl27nit, .aid = HA3_aid7756, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 29, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl29nit, .cTbl = HA3_ctbl29nit, .aid = HA3_aid7609, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 30, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl30nit, .cTbl = HA3_ctbl30nit, .aid = HA3_aid7519, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 32, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl32nit, .cTbl = HA3_ctbl32nit, .aid = HA3_aid7364, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 34, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl34nit, .cTbl = HA3_ctbl34nit, .aid = HA3_aid7198, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 37, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl37nit, .cTbl = HA3_ctbl37nit, .aid = HA3_aid6934, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 39, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl39nit, .cTbl = HA3_ctbl39nit, .aid = HA3_aid6764, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 41, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl41nit, .cTbl = HA3_ctbl41nit, .aid = HA3_aid6581, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 44, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl44nit, .cTbl = HA3_ctbl44nit, .aid = HA3_aid6329, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 47, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl47nit, .cTbl = HA3_ctbl47nit, .aid = HA3_aid6070, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 50, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl50nit, .cTbl = HA3_ctbl50nit, .aid = HA3_aid5791, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 53, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl53nit, .cTbl = HA3_ctbl53nit, .aid = HA3_aid5531, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 56, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl56nit, .cTbl = HA3_ctbl56nit, .aid = HA3_aid5260, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 60, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl60nit, .cTbl = HA3_ctbl60nit, .aid = HA3_aid4907, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 64, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl64nit, .cTbl = HA3_ctbl64nit, .aid = HA3_aid4543, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 68, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl68nit, .cTbl = HA3_ctbl68nit, .aid = HA3_aid4178, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 72, .refBr = 125, .cGma = gma2p15, .rTbl = HA3_rtbl72nit, .cTbl = HA3_ctbl72nit, .aid = HA3_aid3802, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 77, .refBr = 132, .cGma = gma2p15, .rTbl = HA3_rtbl77nit, .cTbl = HA3_ctbl77nit, .aid = HA3_aid3802, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 82, .refBr = 141, .cGma = gma2p15, .rTbl = HA3_rtbl82nit, .cTbl = HA3_ctbl82nit, .aid = HA3_aid3764, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 87, .refBr = 148, .cGma = gma2p15, .rTbl = HA3_rtbl87nit, .cTbl = HA3_ctbl87nit, .aid = HA3_aid3764, .elvCaps = HA3_elvCaps1, .elv = HA3_elv1, .way = W1 },
	{ .br = 93, .refBr = 158, .cGma = gma2p15, .rTbl = HA3_rtbl93nit, .cTbl = HA3_ctbl93nit, .aid = HA3_aid3845, .elvCaps = HA3_elvCaps2, .elv = HA3_elv2, .way = W1 },
	{ .br = 98, .refBr = 167, .cGma = gma2p15, .rTbl = HA3_rtbl98nit, .cTbl = HA3_ctbl98nit, .aid = HA3_aid3771, .elvCaps = HA3_elvCaps2, .elv = HA3_elv2, .way = W1 },
	{ .br = 105, .refBr = 176, .cGma = gma2p15, .rTbl = HA3_rtbl105nit, .cTbl = HA3_ctbl105nit, .aid = HA3_aid3806, .elvCaps = HA3_elvCaps2, .elv = HA3_elv2, .way = W1 },
	{ .br = 111, .refBr = 185, .cGma = gma2p15, .rTbl = HA3_rtbl111nit, .cTbl = HA3_ctbl111nit, .aid = HA3_aid3845, .elvCaps = HA3_elvCaps3, .elv = HA3_elv3, .way = W1 },
	{ .br = 119, .refBr = 198, .cGma = gma2p15, .rTbl = HA3_rtbl119nit, .cTbl = HA3_ctbl119nit, .aid = HA3_aid3845, .elvCaps = HA3_elvCaps3, .elv = HA3_elv3, .way = W1 },
	{ .br = 126, .refBr = 206, .cGma = gma2p15, .rTbl = HA3_rtbl126nit, .cTbl = HA3_ctbl126nit, .aid = HA3_aid3764, .elvCaps = HA3_elvCaps3, .elv = HA3_elv3, .way = W1 },
	{ .br = 134, .refBr = 217, .cGma = gma2p15, .rTbl = HA3_rtbl134nit, .cTbl = HA3_ctbl134nit, .aid = HA3_aid3783, .elvCaps = HA3_elvCaps3, .elv = HA3_elv3, .way = W1 },
	{ .br = 143, .refBr = 229, .cGma = gma2p15, .rTbl = HA3_rtbl143nit, .cTbl = HA3_ctbl143nit, .aid = HA3_aid3764, .elvCaps = HA3_elvCaps4, .elv = HA3_elv4, .way = W1 },
	{ .br = 152, .refBr = 243, .cGma = gma2p15, .rTbl = HA3_rtbl152nit, .cTbl = HA3_ctbl152nit, .aid = HA3_aid3779, .elvCaps = HA3_elvCaps4, .elv = HA3_elv4, .way = W1 },
	{ .br = 162, .refBr = 256, .cGma = gma2p15, .rTbl = HA3_rtbl162nit, .cTbl = HA3_ctbl162nit, .aid = HA3_aid3806, .elvCaps = HA3_elvCaps4, .elv = HA3_elv4, .way = W1 },
	{ .br = 172, .refBr = 273, .cGma = gma2p15, .rTbl = HA3_rtbl172nit, .cTbl = HA3_ctbl172nit, .aid = HA3_aid3845, .elvCaps = HA3_elvCaps4, .elv = HA3_elv4, .way = W1 },
	{ .br = 183, .refBr = 285, .cGma = gma2p15, .rTbl = HA3_rtbl183nit, .cTbl = HA3_ctbl183nit, .aid = HA3_aid3783, .elvCaps = HA3_elvCaps4, .elv = HA3_elv4, .way = W1 },
	{ .br = 195, .refBr = 285, .cGma = gma2p15, .rTbl = HA3_rtbl195nit, .cTbl = HA3_ctbl195nit, .aid = HA3_aid3295, .elvCaps = HA3_elvCaps4, .elv = HA3_elv4, .way = W1 },
	{ .br = 207, .refBr = 285, .cGma = gma2p15, .rTbl = HA3_rtbl207nit, .cTbl = HA3_ctbl207nit, .aid = HA3_aid2876, .elvCaps = HA3_elvCaps5, .elv = HA3_elv5, .way = W1 },
	{ .br = 220, .refBr = 285, .cGma = gma2p15, .rTbl = HA3_rtbl220nit, .cTbl = HA3_ctbl220nit, .aid = HA3_aid2302, .elvCaps = HA3_elvCaps5, .elv = HA3_elv5, .way = W1 },
	{ .br = 234, .refBr = 285, .cGma = gma2p15, .rTbl = HA3_rtbl234nit, .cTbl = HA3_ctbl234nit, .aid = HA3_aid1736, .elvCaps = HA3_elvCaps5, .elv = HA3_elv5, .way = W1 },
	{ .br = 249, .refBr = 285, .cGma = gma2p15, .rTbl = HA3_rtbl249nit, .cTbl = HA3_ctbl249nit, .aid = HA3_aid1081, .elvCaps = HA3_elvCaps6, .elv = HA3_elv6, .way = W1 },    // 249 ~ 360 acl off
	{ .br = 265, .refBr = 298, .cGma = gma2p15, .rTbl = HA3_rtbl265nit, .cTbl = HA3_ctbl265nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps7, .elv = HA3_elv7, .way = W1 },
	{ .br = 282, .refBr = 317, .cGma = gma2p15, .rTbl = HA3_rtbl282nit, .cTbl = HA3_ctbl282nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps8, .elv = HA3_elv8, .way = W1 },
	{ .br = 300, .refBr = 336, .cGma = gma2p15, .rTbl = HA3_rtbl300nit, .cTbl = HA3_ctbl300nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps9, .elv = HA3_elv9, .way = W1 },
	{ .br = 316, .refBr = 352, .cGma = gma2p15, .rTbl = HA3_rtbl316nit, .cTbl = HA3_ctbl316nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps10, .elv = HA3_elv10, .way = W1 },
	{ .br = 333, .refBr = 370, .cGma = gma2p15, .rTbl = HA3_rtbl333nit, .cTbl = HA3_ctbl333nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps11, .elv = HA3_elv11, .way = W1 },
	{ .br = 350, .refBr = 383, .cGma = gma2p15, .rTbl = HA3_rtbl350nit, .cTbl = HA3_ctbl350nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps12, .elv = HA3_elv12, .way = W1 },
	{ .br = 357, .refBr = 390, .cGma = gma2p15, .rTbl = HA3_rtbl357nit, .cTbl = HA3_ctbl357nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps12, .elv = HA3_elv12, .way = W1 },
	{ .br = 365, .refBr = 400, .cGma = gma2p15, .rTbl = HA3_rtbl365nit, .cTbl = HA3_ctbl365nit, .aid = HA3_aid1004, .elvCaps = HA3_elvCaps12, .elv = HA3_elv12, .way = W1 },
	{ .br = 372, .refBr = 400, .cGma = gma2p15, .rTbl = HA3_rtbl372nit, .cTbl = HA3_ctbl372nit, .aid = HA3_aid744, .elvCaps = HA3_elvCaps12, .elv = HA3_elv12, .way = W1 },
	{ .br = 380, .refBr = 400, .cGma = gma2p15, .rTbl = HA3_rtbl380nit, .cTbl = HA3_ctbl380nit, .aid = HA3_aid574, .elvCaps = HA3_elvCaps13, .elv = HA3_elv13, .way = W1 },
	{ .br = 387, .refBr = 400, .cGma = gma2p15, .rTbl = HA3_rtbl387nit, .cTbl = HA3_ctbl387nit, .aid = HA3_aid399, .elvCaps = HA3_elvCaps13, .elv = HA3_elv13, .way = W1 },
	{ .br = 395, .refBr = 400, .cGma = gma2p15, .rTbl = HA3_rtbl395nit, .cTbl = HA3_ctbl395nit, .aid = HA3_aid132, .elvCaps = HA3_elvCaps13, .elv = HA3_elv13, .way = W1 },
	{ .br = 403, .refBr = 404, .cGma = gma2p15, .rTbl = HA3_rtbl403nit, .cTbl = HA3_ctbl403nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps13, .elv = HA3_elv13, .way = W1 },
	{ .br = 412, .refBr = 412, .cGma = gma2p15, .rTbl = HA3_rtbl412nit, .cTbl = HA3_ctbl412nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps14, .elv = HA3_elv14, .way = W1 },
	{ .br = 420, .refBr = 420, .cGma = gma2p20, .rTbl = HA3_rtbl420nit, .cTbl = HA3_ctbl420nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps14, .elv = HA3_elv14, .way = W2 },
/*hbm interpolation */
	{ .br = 443, .refBr = 443, .cGma = gma2p20, .rTbl = HA3_rtbl420nit, .cTbl = HA3_ctbl420nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps14, .elv = HA3_elv14, .way = W3},	// hbm is acl on
	{ .br = 465, .refBr = 465, .cGma = gma2p20, .rTbl = HA3_rtbl420nit, .cTbl = HA3_ctbl420nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps14, .elv = HA3_elv14, .way = W3},	// hbm is acl on
	{ .br = 488, .refBr = 488, .cGma = gma2p20, .rTbl = HA3_rtbl420nit, .cTbl = HA3_ctbl420nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps14, .elv = HA3_elv14, .way = W3},	// hbm is acl on
	{ .br = 510, .refBr = 510, .cGma = gma2p20, .rTbl = HA3_rtbl420nit, .cTbl = HA3_ctbl420nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps14, .elv = HA3_elv14, .way = W3},	// hbm is acl on
	{ .br = 533, .refBr = 533, .cGma = gma2p20, .rTbl = HA3_rtbl420nit, .cTbl = HA3_ctbl420nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps15, .elv = HA3_elv15, .way = W3},	// hbm is acl on
	{ .br = 555, .refBr = 555, .cGma = gma2p20, .rTbl = HA3_rtbl420nit, .cTbl = HA3_ctbl420nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps16, .elv = HA3_elv16, .way = W3},	// hbm is acl on
	{ .br = 578, .refBr = 578, .cGma = gma2p20, .rTbl = HA3_rtbl420nit, .cTbl = HA3_ctbl420nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps17, .elv = HA3_elv17, .way = W3},	// hbm is acl on
/* hbm */
	{ .br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = HA3_rtbl420nit, .cTbl = HA3_ctbl420nit, .aid = HA3_aid39, .elvCaps = HA3_elvCaps18, .elv = HA3_elv18, .way = W4 },
};

// aid sheet in rev.C
struct SmtDimInfo dimming_info_HF3[MAX_BR_INFO] = { // add hbm array
	{ .br = 2, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl2nit, .cTbl = HF3_ctbl2nit, .aid = HF3_aid9783, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 3, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl3nit, .cTbl = HF3_ctbl3nit, .aid = HF3_aid9678, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 4, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl4nit, .cTbl = HF3_ctbl4nit, .aid = HF3_aid9558, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 5, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl5nit, .cTbl = HF3_ctbl5nit, .aid = HF3_aid9446, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 6, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl6nit, .cTbl = HF3_ctbl6nit, .aid = HF3_aid9345, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 7, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl7nit, .cTbl = HF3_ctbl7nit, .aid = HF3_aid9248, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 8, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl8nit, .cTbl = HF3_ctbl8nit, .aid = HF3_aid9159, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 9, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl9nit, .cTbl = HF3_ctbl9nit, .aid = HF3_aid9062, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 10, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl10nit, .cTbl = HF3_ctbl10nit, .aid = HF3_aid8973, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 11, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl11nit, .cTbl = HF3_ctbl11nit, .aid = HF3_aid8907, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 12, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl12nit, .cTbl = HF3_ctbl12nit, .aid = HF3_aid8814, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 13, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl13nit, .cTbl = HF3_ctbl13nit, .aid = HF3_aid8744, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 14, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl14nit, .cTbl = HF3_ctbl14nit, .aid = HF3_aid8667, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 15, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl15nit, .cTbl = HF3_ctbl15nit, .aid = HF3_aid8574, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 16, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl16nit, .cTbl = HF3_ctbl16nit, .aid = HF3_aid8477, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 17, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl17nit, .cTbl = HF3_ctbl17nit, .aid = HF3_aid8380, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 19, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl19nit, .cTbl = HF3_ctbl19nit, .aid = HF3_aid8202, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 20, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl20nit, .cTbl = HF3_ctbl20nit, .aid = HF3_aid8120, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 21, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl21nit, .cTbl = HF3_ctbl21nit, .aid = HF3_aid8008, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 22, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl22nit, .cTbl = HF3_ctbl22nit, .aid = HF3_aid7934, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 24, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl24nit, .cTbl = HF3_ctbl24nit, .aid = HF3_aid7733, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 25, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl25nit, .cTbl = HF3_ctbl25nit, .aid = HF3_aid7632, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 27, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl27nit, .cTbl = HF3_ctbl27nit, .aid = HF3_aid7442, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 29, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl29nit, .cTbl = HF3_ctbl29nit, .aid = HF3_aid7240, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 30, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl30nit, .cTbl = HF3_ctbl30nit, .aid = HF3_aid7143, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 32, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl32nit, .cTbl = HF3_ctbl32nit, .aid = HF3_aid6961, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 34, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl34nit, .cTbl = HF3_ctbl34nit, .aid = HF3_aid6760, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 37, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl37nit, .cTbl = HF3_ctbl37nit, .aid = HF3_aid6469, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 39, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl39nit, .cTbl = HF3_ctbl39nit, .aid = HF3_aid6260, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 41, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl41nit, .cTbl = HF3_ctbl41nit, .aid = HF3_aid6074, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 44, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl44nit, .cTbl = HF3_ctbl44nit, .aid = HF3_aid5771, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 47, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl47nit, .cTbl = HF3_ctbl47nit, .aid = HF3_aid5469, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 50, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl50nit, .cTbl = HF3_ctbl50nit, .aid = HF3_aid5295, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 53, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl53nit, .cTbl = HF3_ctbl53nit, .aid = HF3_aid4860, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 56, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl56nit, .cTbl = HF3_ctbl56nit, .aid = HF3_aid4585, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 60, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl60nit, .cTbl = HF3_ctbl60nit, .aid = HF3_aid4322, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 64, .refBr = 111, .cGma = gma2p15, .rTbl = HF3_rtbl64nit, .cTbl = HF3_ctbl64nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 68, .refBr = 117, .cGma = gma2p15, .rTbl = HF3_rtbl68nit, .cTbl = HF3_ctbl68nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 72, .refBr = 123, .cGma = gma2p15, .rTbl = HF3_rtbl72nit, .cTbl = HF3_ctbl72nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 77, .refBr = 131, .cGma = gma2p15, .rTbl = HF3_rtbl77nit, .cTbl = HF3_ctbl77nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 82, .refBr = 139, .cGma = gma2p15, .rTbl = HF3_rtbl82nit, .cTbl = HF3_ctbl82nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 87, .refBr = 147, .cGma = gma2p15, .rTbl = HF3_rtbl87nit, .cTbl = HF3_ctbl87nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 93, .refBr = 154, .cGma = gma2p15, .rTbl = HF3_rtbl93nit, .cTbl = HF3_ctbl93nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 98, .refBr = 162, .cGma = gma2p15, .rTbl = HF3_rtbl98nit, .cTbl = HF3_ctbl98nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 105, .refBr = 172, .cGma = gma2p15, .rTbl = HF3_rtbl105nit, .cTbl = HF3_ctbl105nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 111, .refBr = 180, .cGma = gma2p15, .rTbl = HF3_rtbl111nit, .cTbl = HF3_ctbl111nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 119, .refBr = 190, .cGma = gma2p15, .rTbl = HF3_rtbl119nit, .cTbl = HF3_ctbl119nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 126, .refBr = 203, .cGma = gma2p15, .rTbl = HF3_rtbl126nit, .cTbl = HF3_ctbl126nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 134, .refBr = 214, .cGma = gma2p15, .rTbl = HF3_rtbl134nit, .cTbl = HF3_ctbl134nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 143, .refBr = 226, .cGma = gma2p15, .rTbl = HF3_rtbl143nit, .cTbl = HF3_ctbl143nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 152, .refBr = 242, .cGma = gma2p15, .rTbl = HF3_rtbl152nit, .cTbl = HF3_ctbl152nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 162, .refBr = 252, .cGma = gma2p15, .rTbl = HF3_rtbl162nit, .cTbl = HF3_ctbl162nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps6, .elv = HF3_elv6, .way = W1 },
	{ .br = 172, .refBr = 264, .cGma = gma2p15, .rTbl = HF3_rtbl172nit, .cTbl = HF3_ctbl172nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps6, .elv = HF3_elv6, .way = W1 },
	{ .br = 183, .refBr = 271, .cGma = gma2p15, .rTbl = HF3_rtbl183nit, .cTbl = HF3_ctbl183nit, .aid = HF3_aid3702, .elvCaps = HF3_elvCaps7, .elv = HF3_elv7, .way = W1 },
	{ .br = 195, .refBr = 271, .cGma = gma2p15, .rTbl = HF3_rtbl195nit, .cTbl = HF3_ctbl195nit, .aid = HF3_aid3016, .elvCaps = HF3_elvCaps7, .elv = HF3_elv7, .way = W1 },
	{ .br = 207, .refBr = 271, .cGma = gma2p15, .rTbl = HF3_rtbl207nit, .cTbl = HF3_ctbl207nit, .aid = HF3_aid2601, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1 },
	{ .br = 220, .refBr = 271, .cGma = gma2p15, .rTbl = HF3_rtbl220nit, .cTbl = HF3_ctbl220nit, .aid = HF3_aid2023, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1 },
	{ .br = 234, .refBr = 271, .cGma = gma2p15, .rTbl = HF3_rtbl234nit, .cTbl = HF3_ctbl234nit, .aid = HF3_aid1403, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1 },
	{ .br = 249, .refBr = 276, .cGma = gma2p15, .rTbl = HF3_rtbl249nit, .cTbl = HF3_ctbl249nit, .aid = HF3_aid1004, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1 },
	{ .br = 265, .refBr = 294, .cGma = gma2p15, .rTbl = HF3_rtbl265nit, .cTbl = HF3_ctbl265nit, .aid = HF3_aid1004, .elvCaps = HF3_elvCaps9, .elv = HF3_elv9, .way = W1 },
	{ .br = 282, .refBr = 312, .cGma = gma2p15, .rTbl = HF3_rtbl282nit, .cTbl = HF3_ctbl282nit, .aid = HF3_aid1004, .elvCaps = HF3_elvCaps9, .elv = HF3_elv9, .way = W1 },
	{ .br = 300, .refBr = 333, .cGma = gma2p15, .rTbl = HF3_rtbl300nit, .cTbl = HF3_ctbl300nit, .aid = HF3_aid1004, .elvCaps = HF3_elvCaps10, .elv = HF3_elv10, .way = W1 },
	{ .br = 316, .refBr = 350, .cGma = gma2p15, .rTbl = HF3_rtbl316nit, .cTbl = HF3_ctbl316nit, .aid = HF3_aid1004, .elvCaps = HF3_elvCaps10, .elv = HF3_elv10, .way = W1 },
	{ .br = 333, .refBr = 370, .cGma = gma2p15, .rTbl = HF3_rtbl333nit, .cTbl = HF3_ctbl333nit, .aid = HF3_aid1004, .elvCaps = HF3_elvCaps11, .elv = HF3_elv11, .way = W1 },
	{ .br = 350, .refBr = 385, .cGma = gma2p15, .rTbl = HF3_rtbl350nit, .cTbl = HF3_ctbl350nit, .aid = HF3_aid1004, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 357, .refBr = 394, .cGma = gma2p15, .rTbl = HF3_rtbl357nit, .cTbl = HF3_ctbl357nit, .aid = HF3_aid1004, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 365, .refBr = 404, .cGma = gma2p15, .rTbl = HF3_rtbl365nit, .cTbl = HF3_ctbl365nit, .aid = HF3_aid1004, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 372, .refBr = 404, .cGma = gma2p15, .rTbl = HF3_rtbl372nit, .cTbl = HF3_ctbl372nit, .aid = HF3_aid798, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 380, .refBr = 404, .cGma = gma2p15, .rTbl = HF3_rtbl380nit, .cTbl = HF3_ctbl380nit, .aid = HF3_aid601, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 387, .refBr = 404, .cGma = gma2p15, .rTbl = HF3_rtbl387nit, .cTbl = HF3_ctbl387nit, .aid = HF3_aid399, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 395, .refBr = 404, .cGma = gma2p15, .rTbl = HF3_rtbl395nit, .cTbl = HF3_ctbl395nit, .aid = HF3_aid202, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 403, .refBr = 403, .cGma = gma2p15, .rTbl = HF3_rtbl403nit, .cTbl = HF3_ctbl403nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 412, .refBr = 415, .cGma = gma2p15, .rTbl = HF3_rtbl412nit, .cTbl = HF3_ctbl412nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W1 },
	{ .br = 420, .refBr = 420, .cGma = gma2p20, .rTbl = HF3_rtbl420nit, .cTbl = HF3_ctbl420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W2 },
/*hbm interpolation */
	{ .br = 443, .refBr = 443, .cGma = gma2p20, .rTbl = HF3_rtbl420nit, .cTbl = HF3_ctbl420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 465, .refBr = 465, .cGma = gma2p20, .rTbl = HF3_rtbl420nit, .cTbl = HF3_ctbl420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 488, .refBr = 488, .cGma = gma2p20, .rTbl = HF3_rtbl420nit, .cTbl = HF3_ctbl420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 510, .refBr = 510, .cGma = gma2p20, .rTbl = HF3_rtbl420nit, .cTbl = HF3_ctbl420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 533, .refBr = 533, .cGma = gma2p20, .rTbl = HF3_rtbl420nit, .cTbl = HF3_ctbl420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps15, .elv = HF3_elv15, .way = W3},	// hbm is acl on
	{ .br = 555, .refBr = 555, .cGma = gma2p20, .rTbl = HF3_rtbl420nit, .cTbl = HF3_ctbl420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps16, .elv = HF3_elv16, .way = W3},	// hbm is acl on
	{ .br = 578, .refBr = 578, .cGma = gma2p20, .rTbl = HF3_rtbl420nit, .cTbl = HF3_ctbl420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps17, .elv = HF3_elv17, .way = W3},	// hbm is acl on
/* hbm */
	{ .br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = HF3_rtbl420nit, .cTbl = HF3_ctbl420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps18, .elv = HF3_elv18, .way = W4 }
};


// aid sheet in rev.D
struct SmtDimInfo dimming_info_HF3_REVD[MAX_BR_INFO] = { // add hbm array
	{ .br = 2, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_2nit, .cTbl = HF3_ctbl_revd_2nit, .aid = HF3_aid9806, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 3, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_3nit, .cTbl = HF3_ctbl_revd_3nit, .aid = HF3_aid9686, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 4, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_4nit, .cTbl = HF3_ctbl_revd_4nit, .aid = HF3_aid9561, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 5, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_5nit, .cTbl = HF3_ctbl_revd_5nit, .aid = HF3_aid9441, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 6, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_6nit, .cTbl = HF3_ctbl_revd_6nit, .aid = HF3_aid9340, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 7, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_7nit, .cTbl = HF3_ctbl_revd_7nit, .aid = HF3_aid9251, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 8, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_8nit, .cTbl = HF3_ctbl_revd_8nit, .aid = HF3_aid9150, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 9, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_9nit, .cTbl = HF3_ctbl_revd_9nit, .aid = HF3_aid9068, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 10, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_10nit, .cTbl = HF3_ctbl_revd_10nit, .aid = HF3_aid8987, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 11, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_11nit, .cTbl = HF3_ctbl_revd_11nit, .aid = HF3_aid8898, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 12, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_12nit, .cTbl = HF3_ctbl_revd_12nit, .aid = HF3_aid8824, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 13, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_13nit, .cTbl = HF3_ctbl_revd_13nit, .aid = HF3_aid8754, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 14, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_14nit, .cTbl = HF3_ctbl_revd_14nit, .aid = HF3_aid8680, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 15, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_15nit, .cTbl = HF3_ctbl_revd_15nit, .aid = HF3_aid8587, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 16, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_16nit, .cTbl = HF3_ctbl_revd_16nit, .aid = HF3_aid8505, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 17, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_17nit, .cTbl = HF3_ctbl_revd_17nit, .aid = HF3_aid8405, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 19, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_19nit, .cTbl = HF3_ctbl_revd_19nit, .aid = HF3_aid8203, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 20, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_20nit, .cTbl = HF3_ctbl_revd_20nit, .aid = HF3_aid8117, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 21, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_21nit, .cTbl = HF3_ctbl_revd_21nit, .aid = HF3_aid8016, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 22, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_22nit, .cTbl = HF3_ctbl_revd_22nit, .aid = HF3_aid7908, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 24, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_24nit, .cTbl = HF3_ctbl_revd_24nit, .aid = HF3_aid7729, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 25, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_25nit, .cTbl = HF3_ctbl_revd_25nit, .aid = HF3_aid7620, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 27, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_27nit, .cTbl = HF3_ctbl_revd_27nit, .aid = HF3_aid7434, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 29, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_29nit, .cTbl = HF3_ctbl_revd_29nit, .aid = HF3_aid7224, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 30, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_30nit, .cTbl = HF3_ctbl_revd_30nit, .aid = HF3_aid7143, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 32, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_32nit, .cTbl = HF3_ctbl_revd_32nit, .aid = HF3_aid6960, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 34, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_34nit, .cTbl = HF3_ctbl_revd_34nit, .aid = HF3_aid6755, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 37, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_37nit, .cTbl = HF3_ctbl_revd_37nit, .aid = HF3_aid6444, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 39, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_39nit, .cTbl = HF3_ctbl_revd_39nit, .aid = HF3_aid6273, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 41, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_41nit, .cTbl = HF3_ctbl_revd_41nit, .aid = HF3_aid6068, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 44, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_44nit, .cTbl = HF3_ctbl_revd_44nit, .aid = HF3_aid5761, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 47, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_47nit, .cTbl = HF3_ctbl_revd_47nit, .aid = HF3_aid5446, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 50, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_50nit, .cTbl = HF3_ctbl_revd_50nit, .aid = HF3_aid5155, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 53, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_53nit, .cTbl = HF3_ctbl_revd_53nit, .aid = HF3_aid4852, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 56, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_56nit, .cTbl = HF3_ctbl_revd_56nit, .aid = HF3_aid4515, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 60, .refBr = 112, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_60nit, .cTbl = HF3_ctbl_revd_60nit, .aid = HF3_aid4057, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 64, .refBr = 117, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_64nit, .cTbl = HF3_ctbl_revd_64nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 68, .refBr = 122, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_68nit, .cTbl = HF3_ctbl_revd_68nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 72, .refBr = 127, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_72nit, .cTbl = HF3_ctbl_revd_72nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 77, .refBr = 135, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_77nit, .cTbl = HF3_ctbl_revd_77nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 82, .refBr = 143, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_82nit, .cTbl = HF3_ctbl_revd_82nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 87, .refBr = 151, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_87nit, .cTbl = HF3_ctbl_revd_87nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 93, .refBr = 160, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_93nit, .cTbl = HF3_ctbl_revd_93nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 98, .refBr = 168, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_98nit, .cTbl = HF3_ctbl_revd_98nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 105, .refBr = 180, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_105nit, .cTbl = HF3_ctbl_revd_105nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 111, .refBr = 189, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_111nit, .cTbl = HF3_ctbl_revd_111nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 119, .refBr = 201, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_119nit, .cTbl = HF3_ctbl_revd_119nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 126, .refBr = 211, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_126nit, .cTbl = HF3_ctbl_revd_126nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 134, .refBr = 222, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_134nit, .cTbl = HF3_ctbl_revd_134nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 143, .refBr = 236, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_143nit, .cTbl = HF3_ctbl_revd_143nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 152, .refBr = 250, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_152nit, .cTbl = HF3_ctbl_revd_152nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 162, .refBr = 262, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_162nit, .cTbl = HF3_ctbl_revd_162nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 172, .refBr = 278, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_172nit, .cTbl = HF3_ctbl_revd_172nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 183, .refBr = 294, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_183nit, .cTbl = HF3_ctbl_revd_183nit, .aid = HF3_aid3793, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 195, .refBr = 294, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_195nit, .cTbl = HF3_ctbl_revd_195nit, .aid = HF3_aid3335, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 207, .refBr = 294, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_207nit, .cTbl = HF3_ctbl_revd_207nit, .aid = HF3_aid2853, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 220, .refBr = 294, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_220nit, .cTbl = HF3_ctbl_revd_220nit, .aid = HF3_aid2306, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 234, .refBr = 294, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_234nit, .cTbl = HF3_ctbl_revd_234nit, .aid = HF3_aid1727, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 249, .refBr = 294, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_249nit, .cTbl = HF3_ctbl_revd_249nit, .aid = HF3_aid1184, .elvCaps = HF3_elvCaps6, .elv = HF3_elv6, .way = W1 },
	{ .br = 265, .refBr = 308, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_265nit, .cTbl = HF3_ctbl_revd_265nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps7, .elv = HF3_elv7, .way = W1 },
	{ .br = 282, .refBr = 326, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_282nit, .cTbl = HF3_ctbl_revd_282nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1 },
	{ .br = 300, .refBr = 342, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_300nit, .cTbl = HF3_ctbl_revd_300nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps9, .elv = HF3_elv9, .way = W1 },
	{ .br = 316, .refBr = 359, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_316nit, .cTbl = HF3_ctbl_revd_316nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps10, .elv = HF3_elv10, .way = W1 },
	{ .br = 333, .refBr = 372, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_333nit, .cTbl = HF3_ctbl_revd_333nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps11, .elv = HF3_elv11, .way = W1 },
	{ .br = 350, .refBr = 387, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_350nit, .cTbl = HF3_ctbl_revd_350nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 357, .refBr = 393, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_357nit, .cTbl = HF3_ctbl_revd_357nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 365, .refBr = 403, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_365nit, .cTbl = HF3_ctbl_revd_365nit, .aid = HF3_aid1005, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 372, .refBr = 403, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_372nit, .cTbl = HF3_ctbl_revd_372nit, .aid = HF3_aid800, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 380, .refBr = 403, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_380nit, .cTbl = HF3_ctbl_revd_380nit, .aid = HF3_aid606, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 387, .refBr = 403, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_387nit, .cTbl = HF3_ctbl_revd_387nit, .aid = HF3_aid392, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 395, .refBr = 403, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_395nit, .cTbl = HF3_ctbl_revd_395nit, .aid = HF3_aid113, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 403, .refBr = 407, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_403nit, .cTbl = HF3_ctbl_revd_403nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 412, .refBr = 415, .cGma = gma2p15, .rTbl = HF3_rtbl_revd_412nit, .cTbl = HF3_ctbl_revd_412nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W1 },
	{ .br = 420, .refBr = 420, .cGma = gma2p20, .rTbl = HF3_rtbl_revd_420nit, .cTbl = HF3_ctbl_revd_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W2 },
/*hbm interpolation */
	{ .br = 443, .refBr = 443, .cGma = gma2p20, .rTbl = HF3_rtbl_revd_420nit, .cTbl = HF3_ctbl_revd_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 465, .refBr = 465, .cGma = gma2p20, .rTbl = HF3_rtbl_revd_420nit, .cTbl = HF3_ctbl_revd_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 488, .refBr = 488, .cGma = gma2p20, .rTbl = HF3_rtbl_revd_420nit, .cTbl = HF3_ctbl_revd_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 510, .refBr = 510, .cGma = gma2p20, .rTbl = HF3_rtbl_revd_420nit, .cTbl = HF3_ctbl_revd_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 533, .refBr = 533, .cGma = gma2p20, .rTbl = HF3_rtbl_revd_420nit, .cTbl = HF3_ctbl_revd_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps15, .elv = HF3_elv15, .way = W3},	// hbm is acl on
	{ .br = 555, .refBr = 555, .cGma = gma2p20, .rTbl = HF3_rtbl_revd_420nit, .cTbl = HF3_ctbl_revd_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps16, .elv = HF3_elv16, .way = W3},	// hbm is acl on
	{ .br = 578, .refBr = 578, .cGma = gma2p20, .rTbl = HF3_rtbl_revd_420nit, .cTbl = HF3_ctbl_revd_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps17, .elv = HF3_elv17, .way = W3},	// hbm is acl on
/* hbm */
	{ .br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = HF3_rtbl_revd_420nit, .cTbl = HF3_ctbl_revd_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps18, .elv = HF3_elv18, .way = W4 }
};


// aid sheet in A3
struct SmtDimInfo dimming_info_HF3_A3[MAX_BR_INFO] = { // add hbm array
	{ .br = 2, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_2nit, .cTbl = HF3_ctbl_a3_2nit, .aid = HF3_aid9802, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 3, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_3nit, .cTbl = HF3_ctbl_a3_3nit, .aid = HF3_aid9717, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 4, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_4nit, .cTbl = HF3_ctbl_a3_4nit, .aid = HF3_aid9612, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 5, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_5nit, .cTbl = HF3_ctbl_a3_5nit, .aid = HF3_aid9527, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 6, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_6nit, .cTbl = HF3_ctbl_a3_6nit, .aid = HF3_aid9426, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 7, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_7nit, .cTbl = HF3_ctbl_a3_7nit, .aid = HF3_aid9318, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 8, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_8nit, .cTbl = HF3_ctbl_a3_8nit, .aid = HF3_aid9233, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 9, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_9nit, .cTbl = HF3_ctbl_a3_9nit, .aid = HF3_aid9132, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 10, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_10nit, .cTbl = HF3_ctbl_a3_10nit, .aid = HF3_aid9039, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 11, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_11nit, .cTbl = HF3_ctbl_a3_11nit, .aid = HF3_aid8957, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 12, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_12nit, .cTbl = HF3_ctbl_a3_12nit, .aid = HF3_aid8880, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 13, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_13nit, .cTbl = HF3_ctbl_a3_13nit, .aid = HF3_aid8810, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 14, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_14nit, .cTbl = HF3_ctbl_a3_14nit, .aid = HF3_aid8740, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 15, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_15nit, .cTbl = HF3_ctbl_a3_15nit, .aid = HF3_aid8640, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 16, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_16nit, .cTbl = HF3_ctbl_a3_16nit, .aid = HF3_aid8539, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 17, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_17nit, .cTbl = HF3_ctbl_a3_17nit, .aid = HF3_aid8461, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 19, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_19nit, .cTbl = HF3_ctbl_a3_19nit, .aid = HF3_aid8252, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 20, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_20nit, .cTbl = HF3_ctbl_a3_20nit, .aid = HF3_aid8190, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 21, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_21nit, .cTbl = HF3_ctbl_a3_21nit, .aid = HF3_aid8089, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 22, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_22nit, .cTbl = HF3_ctbl_a3_22nit, .aid = HF3_aid7996, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 24, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_24nit, .cTbl = HF3_ctbl_a3_24nit, .aid = HF3_aid7779, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 25, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_25nit, .cTbl = HF3_ctbl_a3_25nit, .aid = HF3_aid7709, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 27, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_27nit, .cTbl = HF3_ctbl_a3_27nit, .aid = HF3_aid7523, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 29, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_29nit, .cTbl = HF3_ctbl_a3_29nit, .aid = HF3_aid7337, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 30, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_30nit, .cTbl = HF3_ctbl_a3_30nit, .aid = HF3_aid7267, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 32, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_32nit, .cTbl = HF3_ctbl_a3_32nit, .aid = HF3_aid7074, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 34, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_34nit, .cTbl = HF3_ctbl_a3_34nit, .aid = HF3_aid6888, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 37, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_37nit, .cTbl = HF3_ctbl_a3_37nit, .aid = HF3_aid6601, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 39, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_39nit, .cTbl = HF3_ctbl_a3_39nit, .aid = HF3_aid6430, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 41, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_41nit, .cTbl = HF3_ctbl_a3_41nit, .aid = HF3_aid6213, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 44, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_44nit, .cTbl = HF3_ctbl_a3_44nit, .aid = HF3_aid5919, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 47, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_47nit, .cTbl = HF3_ctbl_a3_47nit, .aid = HF3_aid5647, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 50, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_50nit, .cTbl = HF3_ctbl_a3_50nit, .aid = HF3_aid5322, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 53, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_53nit, .cTbl = HF3_ctbl_a3_53nit, .aid = HF3_aid5035, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 56, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_56nit, .cTbl = HF3_ctbl_a3_56nit, .aid = HF3_aid4740, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 60, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_60nit, .cTbl = HF3_ctbl_a3_60nit, .aid = HF3_aid4345, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 64, .refBr = 116, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_64nit, .cTbl = HF3_ctbl_a3_64nit, .aid = HF3_aid3922, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 68, .refBr = 121, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_68nit, .cTbl = HF3_ctbl_a3_68nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 72, .refBr = 127, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_72nit, .cTbl = HF3_ctbl_a3_72nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 77, .refBr = 137, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_77nit, .cTbl = HF3_ctbl_a3_77nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 82, .refBr = 144, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_82nit, .cTbl = HF3_ctbl_a3_82nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 87, .refBr = 153, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_87nit, .cTbl = HF3_ctbl_a3_87nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps1, .elv = HF3_elv1, .way = W1 },
	{ .br = 93, .refBr = 161, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_93nit, .cTbl = HF3_ctbl_a3_93nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 98, .refBr = 168, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_98nit, .cTbl = HF3_ctbl_a3_98nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 105, .refBr = 179, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_105nit, .cTbl = HF3_ctbl_a3_105nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 111, .refBr = 188, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_111nit, .cTbl = HF3_ctbl_a3_111nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps2, .elv = HF3_elv2, .way = W1 },
	{ .br = 119, .refBr = 200, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_119nit, .cTbl = HF3_ctbl_a3_119nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 126, .refBr = 212, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_126nit, .cTbl = HF3_ctbl_a3_126nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 134, .refBr = 222, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_134nit, .cTbl = HF3_ctbl_a3_134nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps3, .elv = HF3_elv3, .way = W1 },
	{ .br = 143, .refBr = 235, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_143nit, .cTbl = HF3_ctbl_a3_143nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 152, .refBr = 249, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_152nit, .cTbl = HF3_ctbl_a3_152nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 162, .refBr = 265, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_162nit, .cTbl = HF3_ctbl_a3_162nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 172, .refBr = 277, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_172nit, .cTbl = HF3_ctbl_a3_172nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 183, .refBr = 292, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_183nit, .cTbl = HF3_ctbl_a3_183nit, .aid = HF3_aid3829, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 195, .refBr = 292, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_195nit, .cTbl = HF3_ctbl_a3_195nit, .aid = HF3_aid3368, .elvCaps = HF3_elvCaps4, .elv = HF3_elv4, .way = W1 },
	{ .br = 207, .refBr = 292, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_207nit, .cTbl = HF3_ctbl_a3_207nit, .aid = HF3_aid2946, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 220, .refBr = 292, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_220nit, .cTbl = HF3_ctbl_a3_220nit, .aid = HF3_aid2391, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 234, .refBr = 292, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_234nit, .cTbl = HF3_ctbl_a3_234nit, .aid = HF3_aid1818, .elvCaps = HF3_elvCaps5, .elv = HF3_elv5, .way = W1 },
	{ .br = 249, .refBr = 292, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_249nit, .cTbl = HF3_ctbl_a3_249nit, .aid = HF3_aid1260, .elvCaps = HF3_elvCaps6, .elv = HF3_elv6, .way = W1 },
	{ .br = 265, .refBr = 307, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_265nit, .cTbl = HF3_ctbl_a3_265nit, .aid = HF3_aid1143, .elvCaps = HF3_elvCaps7, .elv = HF3_elv7, .way = W1 },
	{ .br = 282, .refBr = 325, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_282nit, .cTbl = HF3_ctbl_a3_282nit, .aid = HF3_aid1143, .elvCaps = HF3_elvCaps8, .elv = HF3_elv8, .way = W1 },
	{ .br = 300, .refBr = 344, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_300nit, .cTbl = HF3_ctbl_a3_300nit, .aid = HF3_aid1143, .elvCaps = HF3_elvCaps9, .elv = HF3_elv9, .way = W1 },
	{ .br = 316, .refBr = 361, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_316nit, .cTbl = HF3_ctbl_a3_316nit, .aid = HF3_aid1143, .elvCaps = HF3_elvCaps10, .elv = HF3_elv10, .way = W1 },
	{ .br = 333, .refBr = 374, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_333nit, .cTbl = HF3_ctbl_a3_333nit, .aid = HF3_aid1143, .elvCaps = HF3_elvCaps11, .elv = HF3_elv11, .way = W1 },
	{ .br = 350, .refBr = 388, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_350nit, .cTbl = HF3_ctbl_a3_350nit, .aid = HF3_aid1143, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 357, .refBr = 395, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_357nit, .cTbl = HF3_ctbl_a3_357nit, .aid = HF3_aid1143, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 365, .refBr = 402, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_365nit, .cTbl = HF3_ctbl_a3_365nit, .aid = HF3_aid1143, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 372, .refBr = 402, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_372nit, .cTbl = HF3_ctbl_a3_372nit, .aid = HF3_aid938, .elvCaps = HF3_elvCaps12, .elv = HF3_elv12, .way = W1 },
	{ .br = 380, .refBr = 402, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_380nit, .cTbl = HF3_ctbl_a3_380nit, .aid = HF3_aid771, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 387, .refBr = 402, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_387nit, .cTbl = HF3_ctbl_a3_387nit, .aid = HF3_aid570, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 395, .refBr = 402, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_395nit, .cTbl = HF3_ctbl_a3_395nit, .aid = HF3_aid318, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 403, .refBr = 402, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_403nit, .cTbl = HF3_ctbl_a3_403nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps13, .elv = HF3_elv13, .way = W1 },
	{ .br = 412, .refBr = 413, .cGma = gma2p15, .rTbl = HF3_rtbl_a3_412nit, .cTbl = HF3_ctbl_a3_412nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W1 },
	{ .br = 420, .refBr = 420, .cGma = gma2p20, .rTbl = HF3_rtbl_a3_420nit, .cTbl = HF3_ctbl_a3_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W2 },
/*hbm interpolation */
	{ .br = 443, .refBr = 443, .cGma = gma2p20, .rTbl = HF3_rtbl_a3_420nit, .cTbl = HF3_ctbl_a3_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 465, .refBr = 465, .cGma = gma2p20, .rTbl = HF3_rtbl_a3_420nit, .cTbl = HF3_ctbl_a3_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 488, .refBr = 488, .cGma = gma2p20, .rTbl = HF3_rtbl_a3_420nit, .cTbl = HF3_ctbl_a3_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 510, .refBr = 510, .cGma = gma2p20, .rTbl = HF3_rtbl_a3_420nit, .cTbl = HF3_ctbl_a3_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps14, .elv = HF3_elv14, .way = W3},	// hbm is acl on
	{ .br = 533, .refBr = 533, .cGma = gma2p20, .rTbl = HF3_rtbl_a3_420nit, .cTbl = HF3_ctbl_a3_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps15, .elv = HF3_elv15, .way = W3},	// hbm is acl on
	{ .br = 555, .refBr = 555, .cGma = gma2p20, .rTbl = HF3_rtbl_a3_420nit, .cTbl = HF3_ctbl_a3_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps16, .elv = HF3_elv16, .way = W3},	// hbm is acl on
	{ .br = 578, .refBr = 578, .cGma = gma2p20, .rTbl = HF3_rtbl_a3_420nit, .cTbl = HF3_ctbl_a3_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps17, .elv = HF3_elv17, .way = W3},	// hbm is acl on
/* hbm */
	{ .br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = HF3_rtbl_a3_420nit, .cTbl = HF3_ctbl_a3_420nit, .aid = HF3_aid39, .elvCaps = HF3_elvCaps18, .elv = HF3_elv18, .way = W4 }
};



static int set_gamma_to_center(struct SmtDimInfo *brInfo)
{
	int     i, j;
	int     ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;

	result[index++] = OLED_CMD_GAMMA;

	for (i = V255; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] = (unsigned char)((center_gamma[i][j] >> 8) & 0x01);
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			}
			else {
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			}
		}
	}
	result[index++] = 0x00;
	result[index++] = 0x00;

	return ret;
}


static int set_gamma_to_hbm(struct SmtDimInfo *brInfo, u8 * hbm)
{
	int     ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;

	memset(result, 0, OLED_CMD_GAMMA_CNT);

	result[index++] = OLED_CMD_GAMMA;
	result[index++] = ((hbm[0] >> 2) & 0x1);
	result[index++] = hbm[1];
	result[index++] = ((hbm[0] >> 1) & 0x1);
	result[index++] = hbm[2];
	result[index++] = (hbm[0] & 0x1);
	result[index++] = hbm[3];
	memcpy(result + 7, hbm + 4, S6E3HF3_HBMGAMMA_LEN - 4);

	return ret;
}

/* gamma interpolaion table */
const unsigned int tbl_hbm_inter[7] = {
	131, 256, 387, 512, 643, 768, 899
};

static int interpolation_gamma_to_hbm(struct SmtDimInfo *dimInfo, int br_idx)
{
	int     i, j;
	int     ret = 0;
	int     idx = 0;
	int     tmp = 0;
	int     hbmcnt, refcnt, gap = 0;
	int     ref_idx = 0;
	int     hbm_idx = 0;
	int     rst = 0;
	int     hbm_tmp, ref_tmp;
	unsigned char *result = dimInfo[br_idx].gamma;

	for (i = 0; i < MAX_BR_INFO; i++) {
		if (dimInfo[i].br == S6E3HF3_MAX_BRIGHTNESS)
			ref_idx = i;

		if (dimInfo[i].br == S6E3HF3_HBM_BRIGHTNESS)
			hbm_idx = i;
	}

	if ((ref_idx == 0) || (hbm_idx == 0)) {
		dsim_info("%s failed to get index ref index : %d, hbm index : %d\n", __func__, ref_idx, hbm_idx);
		ret = -EINVAL;
		goto exit;
	}

	result[idx++] = OLED_CMD_GAMMA;
	tmp = (br_idx - ref_idx) - 1;

	hbmcnt = 1;
	refcnt = 1;

	for (i = V255; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				hbm_tmp = (dimInfo[hbm_idx].gamma[hbmcnt] << 8) | (dimInfo[hbm_idx].gamma[hbmcnt + 1]);
				ref_tmp = (dimInfo[ref_idx].gamma[refcnt] << 8) | (dimInfo[ref_idx].gamma[refcnt + 1]);

				if (hbm_tmp > ref_tmp) {
					gap = hbm_tmp - ref_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst += ref_tmp;
				}
				else {
					gap = ref_tmp - hbm_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst = ref_tmp - rst;
				}
				result[idx++] = (unsigned char)((rst >> 8) & 0x01);
				result[idx++] = (unsigned char)rst & 0xff;
				hbmcnt += 2;
				refcnt += 2;
			}
			else {
				hbm_tmp = dimInfo[hbm_idx].gamma[hbmcnt++];
				ref_tmp = dimInfo[ref_idx].gamma[refcnt++];

				if (hbm_tmp > ref_tmp) {
					gap = hbm_tmp - ref_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst += ref_tmp;
				}
				else {
					gap = ref_tmp - hbm_tmp;
					rst = (gap * tbl_hbm_inter[tmp]) >> 10;
					rst = ref_tmp - rst;
				}
				result[idx++] = (unsigned char)rst & 0xff;
			}
		}
	}

	dsim_info("tmp index : %d\n", tmp);

exit:
	return ret;
}


static int init_dimming(struct dsim_device *dsim, u8 * mtp, u8 * hbm)
{
	int     i, j;
	int     pos = 0;
	int     ret = 0;
	short   temp;
	int     method = 0;
	static struct dim_data *dimming = NULL;
	unsigned char panel_rev = 0x00;
	unsigned char panel_line = 0x00;

	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *diminfo = NULL;
	int     string_offset;
	char    string_buf[1024];

	if( dimming == NULL ) {
        dimming = (struct dim_data *) kmalloc(sizeof(struct dim_data), GFP_KERNEL);
        if (!dimming) {
	        dsim_err("failed to allocate memory for dim data\n");
	        ret = -ENOMEM;
	        goto error;
        }
	}
	panel_line = panel->id[0] & 0xF0;
	panel_rev = panel->id[2] & 0x0F;

	dsim_info("%s panel line : %x,  Panel rev : %x\n",__func__, panel_line, panel_rev);

	switch (dynamic_lcd_type) {
	case LCD_TYPE_S6E3HA2_WQHD:
		dsim_info("%s init dimming info for daisy HA2 rev.E panel\n", __func__);
		diminfo = (void *)dimming_info_HA3;
		panel->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE_HA2;
		memcpy(aid_dimming_dynamic.vint_dim_offset, VINT_TABLE_HA2, sizeof(aid_dimming_dynamic.vint_dim_offset));
		memcpy(aid_dimming_dynamic.elvss_minus_offset, ELVSS_OFFSET_HA2, sizeof(aid_dimming_dynamic.elvss_minus_offset));
		panel->br_tbl = (unsigned int *)br_tbl;
		panel->inter_aor_tbl = (unsigned char *)nb_inter_aor_tbl;
		break;
	case LCD_TYPE_S6E3HA3_WQHD:
		dsim_info("%s init dimming info for daisy HA3 pre panel\n", __func__);
		diminfo = (void *)dimming_info_HA3;
		panel->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE_HA3;
		memcpy(aid_dimming_dynamic.vint_dim_offset, VINT_TABLE_HA3, sizeof(aid_dimming_dynamic.vint_dim_offset));
		memcpy(aid_dimming_dynamic.elvss_minus_offset, ELVSS_OFFSET_HA3, sizeof(aid_dimming_dynamic.elvss_minus_offset));
		panel->inter_aor_tbl = (unsigned char *)nb_inter_aor_tbl;
		panel->br_tbl = (unsigned int *)nb_br_tbl_420;
		break;
	case LCD_TYPE_S6E3HF3_WQHD:
		switch(panel_line) {
		// code
		case S6E3HF3_A2_INIT_ID:
		case S6E3HF3_A2_REV01_ID:
			panel->br_tbl = (unsigned int *)zen_br_tbl_420;
			if(panel_rev <= 3) {
				dsim_info("%s init dimming info for daisy HF3 panel under C\n", __func__);
				diminfo = (void *)dimming_info_HF3;
				panel->inter_aor_tbl = (unsigned char *)zen_inter_aor_tbl;
			} else {
				dsim_info("%s init dimming info for daisy HF3 panel over D\n", __func__);
				diminfo = (void *)dimming_info_HF3_REVD;
				panel->inter_aor_tbl = (unsigned char *)zen_inter_aor_tbl_revd;
			}
			break;
		case S6E3HF3_A3_REV01_ID:
			diminfo = (void *)dimming_info_HF3_A3;
			panel->br_tbl = (unsigned int *)zen_br_tbl_420_a3;
			panel->inter_aor_tbl = (unsigned char*)zen_inter_aor_tbl_a3;
			break;
		default:
			panel->br_tbl = (unsigned int *)zen_br_tbl_420;
			dsim_info("%s init dimming info for invalid line id\n", __func__);
			diminfo = (void *)dimming_info_HF3_REVD;
			panel->inter_aor_tbl = (unsigned char *)zen_inter_aor_tbl_revd;
			break;
		}

		panel->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE_HF3;
		memcpy(aid_dimming_dynamic.vint_dim_offset, VINT_TABLE_HF3, sizeof(aid_dimming_dynamic.vint_dim_offset));
		memcpy(aid_dimming_dynamic.elvss_minus_offset, ELVSS_OFFSET_HF3, sizeof(aid_dimming_dynamic.elvss_minus_offset));
		break;
	default:
		dsim_info("%s init dimming info for daisy (UNKNOWN) HA2 panel\n", __func__);
		diminfo = (void *)dimming_info_HA3;
		panel->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE_HA2;
		memcpy(aid_dimming_dynamic.vint_dim_offset, VINT_TABLE_HA2, sizeof(aid_dimming_dynamic.vint_dim_offset));
		memcpy(aid_dimming_dynamic.elvss_minus_offset, ELVSS_OFFSET_HA2, sizeof(aid_dimming_dynamic.elvss_minus_offset));
		panel->br_tbl = (unsigned int *)br_tbl;
		panel->inter_aor_tbl = (unsigned char *)nb_inter_aor_tbl;
		break;
	}


	panel->dim_data = (void *)dimming;
	panel->dim_info = (void *)diminfo;

	panel->hbm_tbl = (unsigned char **)HBM_TABLE;
	panel->acl_cutoff_tbl = (unsigned char **)ACL_CUTOFF_TABLE;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos + 1];
		dimming->t_gamma[V255][j] = (int)center_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}

	for (i = V203; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
			dimming->t_gamma[i][j] = (int)center_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}
	/* for vt */
	temp = (mtp[pos + 1]) << 8 | mtp[pos];

	for (i = 0; i < CI_MAX; i++)
		dimming->vt_mtp[i] = (temp >> (i * 4)) & 0x0f;
#ifdef SMART_DIMMING_DEBUG
	dimm_info("Center Gamma Info : \n");
	for (i = 0; i < VMAX; i++) {
		dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE],
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE]);
	}
#endif
	dimm_info("VT MTP : \n");
	dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
		dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE],
		dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE]);

	dimm_info("MTP Info : \n");
	for (i = 0; i < VMAX; i++) {
		dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE],
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE]);
	}

	ret = generate_volt_table(dimming);
	if (ret) {
		dimm_err("[ERR:%s] failed to generate volt table\n", __func__);
		goto error;
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		method = diminfo[i].way;

		if (method == DIMMING_METHOD_FILL_CENTER) {
			ret = set_gamma_to_center(&diminfo[i]);
			if (ret) {
				dsim_err("%s : failed to get center gamma\n", __func__);
				goto error;
			}
		}
		else if (method == DIMMING_METHOD_FILL_HBM) {
			ret = set_gamma_to_hbm(&diminfo[i], hbm);
			if (ret) {
				dsim_err("%s : failed to get hbm gamma\n", __func__);
				goto error;
			}
		}
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		method = diminfo[i].way;
		if (method == DIMMING_METHOD_AID) {
			ret = cal_gamma_from_index(dimming, &diminfo[i]);
			if (ret) {
				dsim_err("%s : failed to calculate gamma : index : %d\n", __func__, i);
				goto error;
			}
		}
		if (method == DIMMING_METHOD_INTERPOLATION) {
			ret = interpolation_gamma_to_hbm(diminfo, i);
			if (ret) {
				dsim_err("%s : failed to calculate gamma : index : %d\n", __func__, i);
				goto error;
			}
		}
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		memset(string_buf, 0, sizeof(string_buf));
		string_offset = sprintf(string_buf, "gamma[%3d] : ", diminfo[i].br);

		for (j = 0; j < GAMMA_CMD_CNT; j++)
			string_offset += sprintf(string_buf + string_offset, "%02x ", diminfo[i].gamma[j]);

		dsim_info("%s\n", string_buf);
	}
error:
	return ret;

}


#ifdef CONFIG_LCD_HMT
static const unsigned int hmt_br_tbl[EXTEND_BRIGHTNESS + 1] = {
	10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 12, 12,
	13, 13, 14, 14, 14, 15, 15, 16, 16, 16, 17, 17, 17, 17, 17, 19,
	19, 20, 20, 21, 21, 21, 22, 22, 23, 23, 23, 23, 23, 25, 25, 25,
	25, 25, 27, 27, 27, 27, 27, 29, 29, 29, 29, 29, 31, 31, 31, 31,
	31, 33, 33, 33, 33, 35, 35, 35, 35, 35, 37, 37, 37, 37, 37, 39,
	39, 39, 39, 39, 41, 41, 41, 41, 41, 41, 41, 44, 44, 44, 44, 44,
	44, 44, 44, 47, 47, 47, 47, 47, 47, 47, 50, 50, 50, 50, 50, 50,
	50, 53, 53, 53, 53, 53, 53, 53, 56, 56, 56, 56, 56, 56, 56, 56,
	56, 56, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 68, 68, 68, 68, 68, 68, 68, 68, 68, 72,
	72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 77, 77, 77, 77, 77,
	77, 77, 77, 77, 77, 77, 77, 77, 82, 82, 82, 82, 82, 82, 82, 82,
	82, 82, 82, 82, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87, 87,
	87, 87, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93, 93,
	93, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 105,
	[UI_MAX_BRIGHTNESS + 1 ... EXTEND_BRIGHTNESS] = 105
};

struct SmtDimInfo hmt_dimming_info_HA3[HMT_MAX_BR_INFO] = {
	{.br = 10, .refBr = 44, .cGma = gma2p15, .rTbl = HA3_HMTrtbl10nit, .cTbl = HA3_HMTctbl10nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 11, .refBr = 48, .cGma = gma2p15, .rTbl = HA3_HMTrtbl11nit, .cTbl = HA3_HMTctbl11nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 12, .refBr = 57, .cGma = gma2p15, .rTbl = HA3_HMTrtbl12nit, .cTbl = HA3_HMTctbl12nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 13, .refBr = 61, .cGma = gma2p15, .rTbl = HA3_HMTrtbl13nit, .cTbl = HA3_HMTctbl13nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 14, .refBr = 65, .cGma = gma2p15, .rTbl = HA3_HMTrtbl14nit, .cTbl = HA3_HMTctbl14nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 15, .refBr = 69, .cGma = gma2p15, .rTbl = HA3_HMTrtbl15nit, .cTbl = HA3_HMTctbl15nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 16, .refBr = 73, .cGma = gma2p15, .rTbl = HA3_HMTrtbl16nit, .cTbl = HA3_HMTctbl16nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 17, .refBr = 75, .cGma = gma2p15, .rTbl = HA3_HMTrtbl17nit, .cTbl = HA3_HMTctbl17nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 19, .refBr = 84, .cGma = gma2p15, .rTbl = HA3_HMTrtbl19nit, .cTbl = HA3_HMTctbl19nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 20, .refBr = 87, .cGma = gma2p15, .rTbl = HA3_HMTrtbl20nit, .cTbl = HA3_HMTctbl20nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 21, .refBr = 89, .cGma = gma2p15, .rTbl = HA3_HMTrtbl21nit, .cTbl = HA3_HMTctbl21nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 22, .refBr = 96, .cGma = gma2p15, .rTbl = HA3_HMTrtbl22nit, .cTbl = HA3_HMTctbl22nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 23, .refBr = 101, .cGma = gma2p15, .rTbl = HA3_HMTrtbl23nit, .cTbl = HA3_HMTctbl23nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 25, .refBr = 107, .cGma = gma2p15, .rTbl = HA3_HMTrtbl25nit, .cTbl = HA3_HMTctbl25nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 27, .refBr = 114, .cGma = gma2p15, .rTbl = HA3_HMTrtbl27nit, .cTbl = HA3_HMTctbl27nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 29, .refBr = 121, .cGma = gma2p15, .rTbl = HA3_HMTrtbl29nit, .cTbl = HA3_HMTctbl29nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 31, .refBr = 128, .cGma = gma2p15, .rTbl = HA3_HMTrtbl31nit, .cTbl = HA3_HMTctbl31nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 33, .refBr = 136, .cGma = gma2p15, .rTbl = HA3_HMTrtbl33nit, .cTbl = HA3_HMTctbl33nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 35, .refBr = 142, .cGma = gma2p15, .rTbl = HA3_HMTrtbl35nit, .cTbl = HA3_HMTctbl35nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 37, .refBr = 150, .cGma = gma2p15, .rTbl = HA3_HMTrtbl37nit, .cTbl = HA3_HMTctbl37nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 39, .refBr = 156, .cGma = gma2p15, .rTbl = HA3_HMTrtbl39nit, .cTbl = HA3_HMTctbl39nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 41, .refBr = 165, .cGma = gma2p15, .rTbl = HA3_HMTrtbl41nit, .cTbl = HA3_HMTctbl41nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 44, .refBr = 179, .cGma = gma2p15, .rTbl = HA3_HMTrtbl44nit, .cTbl = HA3_HMTctbl44nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 47, .refBr = 188, .cGma = gma2p15, .rTbl = HA3_HMTrtbl47nit, .cTbl = HA3_HMTctbl47nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 50, .refBr = 199, .cGma = gma2p15, .rTbl = HA3_HMTrtbl50nit, .cTbl = HA3_HMTctbl50nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 53, .refBr = 211, .cGma = gma2p15, .rTbl = HA3_HMTrtbl53nit, .cTbl = HA3_HMTctbl53nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 56, .refBr = 220, .cGma = gma2p15, .rTbl = HA3_HMTrtbl56nit, .cTbl = HA3_HMTctbl56nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 60, .refBr = 233, .cGma = gma2p15, .rTbl = HA3_HMTrtbl60nit, .cTbl = HA3_HMTctbl60nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 64, .refBr = 246, .cGma = gma2p15, .rTbl = HA3_HMTrtbl64nit, .cTbl = HA3_HMTctbl64nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 68, .refBr = 260, .cGma = gma2p15, .rTbl = HA3_HMTrtbl68nit, .cTbl = HA3_HMTctbl68nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 72, .refBr = 274, .cGma = gma2p15, .rTbl = HA3_HMTrtbl72nit, .cTbl = HA3_HMTctbl72nit, .aid = HA3_HMTaid8004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 77, .refBr = 211, .cGma = gma2p15, .rTbl = HA3_HMTrtbl77nit, .cTbl = HA3_HMTctbl77nit, .aid = HA3_HMTaid7004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 82, .refBr = 222, .cGma = gma2p15, .rTbl = HA3_HMTrtbl82nit, .cTbl = HA3_HMTctbl82nit, .aid = HA3_HMTaid7004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 87, .refBr = 235, .cGma = gma2p15, .rTbl = HA3_HMTrtbl87nit, .cTbl = HA3_HMTctbl87nit, .aid = HA3_HMTaid7004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 93, .refBr = 248, .cGma = gma2p15, .rTbl = HA3_HMTrtbl93nit, .cTbl = HA3_HMTctbl93nit, .aid = HA3_HMTaid7004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 99, .refBr = 263, .cGma = gma2p15, .rTbl = HA3_HMTrtbl99nit, .cTbl = HA3_HMTctbl99nit, .aid = HA3_HMTaid7004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
	{.br = 105, .refBr = 274, .cGma = gma2p15, .rTbl = HA3_HMTrtbl105nit, .cTbl = HA3_HMTctbl105nit, .aid = HA3_HMTaid7004, .elvCaps = HA3_HMTelvCaps, .elv = HA3_HMTelv},
};

struct SmtDimInfo hmt_dimming_info_HF3[HMT_MAX_BR_INFO] = {
	{.br = 10, .refBr = 39, .cGma = gma2p15, .rTbl = HF3_HMTrtbl10nit, .cTbl = HF3_HMTctbl10nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 11, .refBr = 44, .cGma = gma2p15, .rTbl = HF3_HMTrtbl11nit, .cTbl = HF3_HMTctbl11nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 12, .refBr = 48, .cGma = gma2p15, .rTbl = HF3_HMTrtbl12nit, .cTbl = HF3_HMTctbl12nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 13, .refBr = 52, .cGma = gma2p15, .rTbl = HF3_HMTrtbl13nit, .cTbl = HF3_HMTctbl13nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 14, .refBr = 56, .cGma = gma2p15, .rTbl = HF3_HMTrtbl14nit, .cTbl = HF3_HMTctbl14nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 15, .refBr = 60, .cGma = gma2p15, .rTbl = HF3_HMTrtbl15nit, .cTbl = HF3_HMTctbl15nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 16, .refBr = 63, .cGma = gma2p15, .rTbl = HF3_HMTrtbl16nit, .cTbl = HF3_HMTctbl16nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 17, .refBr = 66, .cGma = gma2p15, .rTbl = HF3_HMTrtbl17nit, .cTbl = HF3_HMTctbl17nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 19, .refBr = 74, .cGma = gma2p15, .rTbl = HF3_HMTrtbl19nit, .cTbl = HF3_HMTctbl19nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 20, .refBr = 77, .cGma = gma2p15, .rTbl = HF3_HMTrtbl20nit, .cTbl = HF3_HMTctbl20nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 21, .refBr = 82, .cGma = gma2p15, .rTbl = HF3_HMTrtbl21nit, .cTbl = HF3_HMTctbl21nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 22, .refBr = 85, .cGma = gma2p15, .rTbl = HF3_HMTrtbl22nit, .cTbl = HF3_HMTctbl22nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 23, .refBr = 87, .cGma = gma2p15, .rTbl = HF3_HMTrtbl23nit, .cTbl = HF3_HMTctbl23nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 25, .refBr = 95, .cGma = gma2p15, .rTbl = HF3_HMTrtbl25nit, .cTbl = HF3_HMTctbl25nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 27, .refBr = 102, .cGma = gma2p15, .rTbl = HF3_HMTrtbl27nit, .cTbl = HF3_HMTctbl27nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 29, .refBr = 107, .cGma = gma2p15, .rTbl = HF3_HMTrtbl29nit, .cTbl = HF3_HMTctbl29nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 31, .refBr = 114, .cGma = gma2p15, .rTbl = HF3_HMTrtbl31nit, .cTbl = HF3_HMTctbl31nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 33, .refBr = 120, .cGma = gma2p15, .rTbl = HF3_HMTrtbl33nit, .cTbl = HF3_HMTctbl33nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 35, .refBr = 126, .cGma = gma2p15, .rTbl = HF3_HMTrtbl35nit, .cTbl = HF3_HMTctbl35nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 37, .refBr = 131, .cGma = gma2p15, .rTbl = HF3_HMTrtbl37nit, .cTbl = HF3_HMTctbl37nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 39, .refBr = 138, .cGma = gma2p15, .rTbl = HF3_HMTrtbl39nit, .cTbl = HF3_HMTctbl39nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 41, .refBr = 147, .cGma = gma2p15, .rTbl = HF3_HMTrtbl41nit, .cTbl = HF3_HMTctbl41nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 44, .refBr = 157, .cGma = gma2p15, .rTbl = HF3_HMTrtbl44nit, .cTbl = HF3_HMTctbl44nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 47, .refBr = 165, .cGma = gma2p15, .rTbl = HF3_HMTrtbl47nit, .cTbl = HF3_HMTctbl47nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 50, .refBr = 175, .cGma = gma2p15, .rTbl = HF3_HMTrtbl50nit, .cTbl = HF3_HMTctbl50nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 53, .refBr = 184, .cGma = gma2p15, .rTbl = HF3_HMTrtbl53nit, .cTbl = HF3_HMTctbl53nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 56, .refBr = 192, .cGma = gma2p15, .rTbl = HF3_HMTrtbl56nit, .cTbl = HF3_HMTctbl56nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 60, .refBr = 204, .cGma = gma2p15, .rTbl = HF3_HMTrtbl60nit, .cTbl = HF3_HMTctbl60nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 64, .refBr = 218, .cGma = gma2p15, .rTbl = HF3_HMTrtbl64nit, .cTbl = HF3_HMTctbl64nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 68, .refBr = 230, .cGma = gma2p15, .rTbl = HF3_HMTrtbl68nit, .cTbl = HF3_HMTctbl68nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 72, .refBr = 241, .cGma = gma2p15, .rTbl = HF3_HMTrtbl72nit, .cTbl = HF3_HMTctbl72nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 77, .refBr = 186, .cGma = gma2p15, .rTbl = HF3_HMTrtbl77nit, .cTbl = HF3_HMTctbl77nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 82, .refBr = 199, .cGma = gma2p15, .rTbl = HF3_HMTrtbl82nit, .cTbl = HF3_HMTctbl82nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 87, .refBr = 209, .cGma = gma2p15, .rTbl = HF3_HMTrtbl87nit, .cTbl = HF3_HMTctbl87nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 93, .refBr = 223, .cGma = gma2p15, .rTbl = HF3_HMTrtbl93nit, .cTbl = HF3_HMTctbl93nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 99, .refBr = 234, .cGma = gma2p15, .rTbl = HF3_HMTrtbl99nit, .cTbl = HF3_HMTctbl99nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 105, .refBr = 247, .cGma = gma2p15, .rTbl = HF3_HMTrtbl105nit, .cTbl = HF3_HMTctbl105nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
};

struct SmtDimInfo hmt_dimming_info_HF3_A3_REV1[HMT_MAX_BR_INFO] = {
	{.br = 10, .refBr = 40, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl10nit, .cTbl = HF3_A3_HMTctbl10nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 11, .refBr = 44, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl11nit, .cTbl = HF3_A3_HMTctbl11nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 12, .refBr = 47, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl12nit, .cTbl = HF3_A3_HMTctbl12nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 13, .refBr = 52, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl13nit, .cTbl = HF3_A3_HMTctbl13nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 14, .refBr = 56, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl14nit, .cTbl = HF3_A3_HMTctbl14nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 15, .refBr = 60, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl15nit, .cTbl = HF3_A3_HMTctbl15nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 16, .refBr = 63, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl16nit, .cTbl = HF3_A3_HMTctbl16nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 17, .refBr = 67, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl17nit, .cTbl = HF3_A3_HMTctbl17nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 19, .refBr = 76, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl19nit, .cTbl = HF3_A3_HMTctbl19nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 20, .refBr = 79, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl20nit, .cTbl = HF3_A3_HMTctbl20nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 21, .refBr = 83, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl21nit, .cTbl = HF3_A3_HMTctbl21nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 22, .refBr = 88, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl22nit, .cTbl = HF3_A3_HMTctbl22nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 23, .refBr = 91, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl23nit, .cTbl = HF3_A3_HMTctbl23nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 25, .refBr = 97, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl25nit, .cTbl = HF3_A3_HMTctbl25nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 27, .refBr = 103, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl27nit, .cTbl = HF3_A3_HMTctbl27nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 29, .refBr = 113, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl29nit, .cTbl = HF3_A3_HMTctbl29nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 31, .refBr = 117, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl31nit, .cTbl = HF3_A3_HMTctbl31nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 33, .refBr = 123, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl33nit, .cTbl = HF3_A3_HMTctbl33nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 35, .refBr = 130, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl35nit, .cTbl = HF3_A3_HMTctbl35nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 37, .refBr = 137, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl37nit, .cTbl = HF3_A3_HMTctbl37nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 39, .refBr = 144, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl39nit, .cTbl = HF3_A3_HMTctbl39nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 41, .refBr = 152, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl41nit, .cTbl = HF3_A3_HMTctbl41nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 44, .refBr = 164, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl44nit, .cTbl = HF3_A3_HMTctbl44nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 47, .refBr = 174, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl47nit, .cTbl = HF3_A3_HMTctbl47nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 50, .refBr = 183, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl50nit, .cTbl = HF3_A3_HMTctbl50nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 53, .refBr = 195, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl53nit, .cTbl = HF3_A3_HMTctbl53nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 56, .refBr = 205, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl56nit, .cTbl = HF3_A3_HMTctbl56nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 60, .refBr = 218, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl60nit, .cTbl = HF3_A3_HMTctbl60nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 64, .refBr = 229, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl64nit, .cTbl = HF3_A3_HMTctbl64nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 68, .refBr = 243, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl68nit, .cTbl = HF3_A3_HMTctbl68nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 72, .refBr = 256, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl72nit, .cTbl = HF3_A3_HMTctbl72nit, .aid = HF3_HMTaid8001, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 77, .refBr = 197, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl77nit, .cTbl = HF3_A3_HMTctbl77nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 82, .refBr = 207, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl82nit, .cTbl = HF3_A3_HMTctbl82nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 87, .refBr = 222, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl87nit, .cTbl = HF3_A3_HMTctbl87nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 93, .refBr = 236, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl93nit, .cTbl = HF3_A3_HMTctbl93nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 99, .refBr = 249, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl99nit, .cTbl = HF3_A3_HMTctbl99nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
	{.br = 105, .refBr = 262, .cGma = gma2p15, .rTbl = HF3_A3_HMTrtbl105nit, .cTbl = HF3_A3_HMTctbl105nit, .aid = HF3_HMTaid6999, .elvCaps = HF3_HMTelvCaps, .elv = HF3_HMTelv},
};

static int hmt_init_dimming(struct dsim_device *dsim, u8 * mtp)
{
	int     i, j;
	int     pos = 0;
	int     ret = 0;
	short   temp;
	static struct dim_data *dimming = NULL;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *diminfo = NULL;
	unsigned char panel_line = panel->id[0] & 0xF0;

	if( dimming == NULL ) {
		dimming = (struct dim_data *) kmalloc(sizeof(struct dim_data), GFP_KERNEL);
		if (!dimming) {
			dsim_err("failed to allocate memory for dim data\n");
			ret = -ENOMEM;
			goto error;
		}
	}
	switch (dynamic_lcd_type) {
	case LCD_TYPE_S6E3HA3_WQHD:
		dsim_info("%s init HMT dimming info for HA3 panel\n", __func__);
		diminfo = (void *) hmt_dimming_info_HA3;
		break;
	case LCD_TYPE_S6E3HF3_WQHD:
		switch(panel_line) {
				// code
		case S6E3HF3_A2_INIT_ID:
		case S6E3HF3_A2_REV01_ID:
			dsim_info("%s init HMT dimming info for HF3 A2 init panel\n", __func__);
			diminfo = (void *) hmt_dimming_info_HF3;
			break;
		case S6E3HF3_A3_REV01_ID:
			dsim_info("%s init HMT dimming info for HF3 A3 rev1 init panel\n", __func__);
			diminfo = (void *) hmt_dimming_info_HF3_A3_REV1;
			break;
		default:
			dsim_info("%s init HMT dimming info for (UNKNOWN) HF3 panel\n", __func__);
			diminfo = (void *) hmt_dimming_info_HF3;
			break;
		}
		break;
	default:
		dsim_info("%s init HMT dimming info for (UNKNOWN) HA3 panel\n", __func__);
		diminfo = (void *) hmt_dimming_info_HA3;
		break;
	}
	panel->hmt_dim_data = (void *) dimming;
	panel->hmt_dim_info = (void *) diminfo;
	panel->hmt_br_tbl = (unsigned int *) hmt_br_tbl;
	for (j = 0; j < CI_MAX; j++) {
		temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos + 1];
		dimming->t_gamma[V255][j] = (int) center_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}
	for (i = V203; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
			dimming->t_gamma[i][j] = (int) center_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
			}
	}
        /* for vt */
	temp = (mtp[pos + 1]) << 8 | mtp[pos];
	for (i = 0; i < CI_MAX; i++)
		dimming->vt_mtp[i] = (temp >> (i * 4)) & 0x0f;
	ret = generate_volt_table(dimming);
	if (ret) {
		dimm_err("[ERR:%s] failed to generate volt table\n", __func__);
		goto error;
	}
	for (i = 0; i < HMT_MAX_BR_INFO; i++) {
		ret = cal_gamma_from_index(dimming, &diminfo[i]);
		if (ret) {
			dsim_err("failed to calculate gamma : index : %d\n", i);
			goto error;
		}
	}
error:
	return ret;

}

#endif

#endif


/************************************ HA2 *****************************************/


static int s6e3ha2_read_init_info(struct dsim_device *dsim, unsigned char *mtp, unsigned char *hbm)
{
        int     i = 0;
        int     ret = 0;
        struct panel_private *panel = &dsim->priv;
        unsigned char buf[S6E3HA2_MTP_DATE_SIZE] = { 0, };
        unsigned char bufForCoordi[S6E3HA2_COORDINATE_LEN] = { 0, };
        unsigned char hbm_gamma[S6E3HA2_HBMGAMMA_LEN + 1] = { 0, };
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
        }

        ret = dsim_read_hl_data(dsim, S6E3HA2_ID_REG, S6E3HA2_ID_LEN, dsim->priv.id);
        if (ret != S6E3HA2_ID_LEN) {
                dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
                panel->lcdConnected = PANEL_DISCONNEDTED;
                goto read_exit;
        }

        dsim_info("READ ID : ");
        for (i = 0; i < S6E3HA2_ID_LEN; i++)
                dsim_info("%02x, ", dsim->priv.id[i]);
        dsim_info("\n");

        ret = dsim_read_hl_data(dsim, S6E3HA2_MTP_ADDR, S6E3HA2_MTP_DATE_SIZE, buf);
        if (ret != S6E3HA2_MTP_DATE_SIZE) {
                dsim_err("failed to read mtp, check panel connection\n");
                goto read_fail;
        }
        memcpy(mtp, buf, S6E3HA2_MTP_SIZE);
        memcpy(dsim->priv.date, &buf[40], ARRAY_SIZE(dsim->priv.date));
        dsim_info("READ MTP SIZE : %d\n", S6E3HA2_MTP_SIZE);
        dsim_info("=========== MTP INFO =========== \n");
        for (i = 0; i < S6E3HA2_MTP_SIZE; i++)
                dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

        // coordinate
        ret = dsim_read_hl_data(dsim, S6E3HA2_COORDINATE_REG, S6E3HA2_COORDINATE_LEN, bufForCoordi);
        if (ret != S6E3HA2_COORDINATE_LEN) {
                dsim_err("fail to read coordinate on command.\n");
                goto read_fail;
        }
        dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];      /* X */
        dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];      /* Y */
        dsim_info("READ coordi : ");
        for (i = 0; i < 2; i++)
                dsim_info("%d, ", dsim->priv.coordinate[i]);
        dsim_info("\n");

        // code
        ret = dsim_read_hl_data(dsim, S6E3HA2_CODE_REG, S6E3HA2_CODE_LEN, dsim->priv.code);
        if (ret != S6E3HA2_CODE_LEN) {
                dsim_err("fail to read code on command.\n");
                goto read_fail;
        }
        dsim_info("READ code : ");
        for (i = 0; i < S6E3HA2_CODE_LEN; i++)
                dsim_info("%x, ", dsim->priv.code[i]);
        dsim_info("\n");

        // tset
        ret = dsim_read_hl_data(dsim, S6E3HA2_TSET_REG, S6E3HA2_TSET_LEN - 1, dsim->priv.tset);
        if (ret < TSET_LEN - 1) {
                dsim_err("fail to read code on command.\n");
                goto read_fail;
        }
        dsim_info("READ tset : ");
        for (i = 0; i < TSET_LEN - 1; i++)
                dsim_info("%x, ", dsim->priv.tset[i]);
        dsim_info("\n");


        // elvss
        ret = dsim_read_hl_data(dsim, S6E3HA2_ELVSS_REG, S6E3HA2_ELVSS_LEN - 1, dsim->priv.elvss_set);
        if (ret < ELVSS_LEN - 1) {
                dsim_err("fail to read elvss on command.\n");
                goto read_fail;
        }
        dsim_info("READ elvss : ");
        for (i = 0; i < ELVSS_LEN - 1; i++)
                dsim_info("%x \n", dsim->priv.elvss_set[i]);

/* read hbm elvss for hbm interpolation */
        panel->hbm_elvss = dsim->priv.elvss_set[S6E3HA2_HBM_ELVSS_INDEX];

        ret = dsim_read_hl_data(dsim, S6E3HA2_HBMGAMMA_REG, S6E3HA2_HBMGAMMA_LEN + 1, hbm_gamma);
        if (ret != S6E3HA2_HBMGAMMA_LEN + 1) {
                dsim_err("fail to read elvss on command.\n");
                goto read_fail;
        }
        dsim_info("HBM Gamma : ");
        memcpy(hbm, hbm_gamma + 1, S6E3HA2_HBMGAMMA_LEN);

        for (i = 1; i < S6E3HA2_HBMGAMMA_LEN + 1; i++)
                dsim_info("hbm gamma[%d] : %x\n", i, hbm_gamma[i]);
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
                goto read_exit;
        }
        ret = 0;
      read_exit:
        return 0;

      read_fail:
        return -ENODEV;
}
static int s6e3ha2_wqhd_dump(struct dsim_device *dsim)
{
        int     ret = 0;
        int     i;
        unsigned char id[S6E3HA2_ID_LEN];
        unsigned char rddpm[4];
        unsigned char rddsm[4];
        unsigned char err_buf[4];

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
        }

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
        }

        ret = dsim_read_hl_data(dsim, 0xEA, 3, err_buf);
        if (ret != 3) {
                dsim_err("%s : can't read Panel's EA Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's 0xEA Reg Value ===\n");
        dsim_info("* 0xEA : buf[0] = %x\n", err_buf[0]);
        dsim_info("* 0xEA : buf[1] = %x\n", err_buf[1]);

        ret = dsim_read_hl_data(dsim, S6E3HA2_RDDPM_ADDR, 3, rddpm);
        if (ret != 3) {
                dsim_err("%s : can't read RDDPM Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's RDDPM Reg Value : %x ===\n", rddpm[0]);

        if (rddpm[0] & 0x80)
                dsim_info("* Booster Voltage Status : ON\n");
        else
                dsim_info("* Booster Voltage Status : OFF\n");

        if (rddpm[0] & 0x40)
                dsim_info("* Idle Mode : On\n");
        else
                dsim_info("* Idle Mode : OFF\n");

        if (rddpm[0] & 0x20)
                dsim_info("* Partial Mode : On\n");
        else
                dsim_info("* Partial Mode : OFF\n");

        if (rddpm[0] & 0x10)
                dsim_info("* Sleep OUT and Working Ok\n");
        else
                dsim_info("* Sleep IN\n");

        if (rddpm[0] & 0x08)
                dsim_info("* Normal Mode On and Working Ok\n");
        else
                dsim_info("* Sleep IN\n");

        if (rddpm[0] & 0x04)
                dsim_info("* Display On and Working Ok\n");
        else
                dsim_info("* Display Off\n");

        ret = dsim_read_hl_data(dsim, S6E3HA2_RDDSM_ADDR, 3, rddsm);
        if (ret != 3) {
                dsim_err("%s : can't read RDDSM Reg\n", __func__);
                goto dump_exit;
        }

        dsim_info("=== Panel's RDDSM Reg Value : %x ===\n", rddsm[0]);

        if (rddsm[0] & 0x80)
                dsim_info("* TE On\n");
        else
                dsim_info("* TE OFF\n");

        if (rddsm[0] & 0x02)
                dsim_info("* S_DSI_ERR : Found\n");

        if (rddsm[0] & 0x01)
                dsim_info("* DSI_ERR : Found\n");

        // id
        ret = dsim_read_hl_data(dsim, S6E3HA2_ID_REG, S6E3HA2_ID_LEN, id);
        if (ret != S6E3HA2_ID_LEN) {
                dsim_err("%s : can't read panel id\n", __func__);
                goto dump_exit;
        }

        dsim_info("READ ID : ");
        for (i = 0; i < S6E3HA2_ID_LEN; i++)
                dsim_info("%02x, ", id[i]);
        dsim_info("\n");

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
        }

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
        }
      dump_exit:
        dsim_info(" - %s\n", __func__);
        return ret;

}
static int s6e3ha2_wqhd_probe(struct dsim_device *dsim)
{
        int     ret = 0;
        struct panel_private *panel = &dsim->priv;
        unsigned char mtp[S6E3HA2_MTP_SIZE] = { 0, };
        unsigned char hbm[S6E3HA2_HBMGAMMA_LEN] = { 0, };
        panel->dim_data = (void *) NULL;
        panel->lcdConnected = PANEL_CONNECTED;

        dsim_info(" +  : %s\n", __func__);
#ifdef CONFIG_LCD_ALPM
		panel->alpm = 0;
		panel->current_alpm = 0;
		mutex_init(&panel->alpm_lock);
		panel->alpm_support = 0;
#endif

        ret = s6e3ha2_read_init_info(dsim, mtp, hbm);
        if (panel->lcdConnected == PANEL_DISCONNEDTED) {
                dsim_err("dsim : %s lcd was not connected\n", __func__);
                goto probe_exit;
        }

	dsim->priv.esd_disable = 0;

#ifdef CONFIG_PANEL_AID_DIMMING
        ret = init_dimming(dsim, mtp, hbm);
        if (ret) {
                dsim_err("%s : failed to generate gamma tablen\n", __func__);
        }
#endif
#ifdef CONFIG_LCD_HMT
        ret = hmt_init_dimming(dsim, mtp);
        if (ret) {
                dsim_err("%s : failed to generate gamma tablen\n", __func__);
        }
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
        panel->mdnie_support = 0;
#endif

probe_exit:
        return ret;

}


static int s6e3ha2_wqhd_displayon(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
                goto displayon_err;
        }

      displayon_err:
        return ret;

}

static int s6e3ha2_wqhd_exit(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
                goto exit_err;
        }

        ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
                goto exit_err;
        }

        msleep(120);

      exit_err:
        return ret;
}

#if defined(CONFIG_FB_DSU)
static int _s6e3ha2_wqhd_dsu_command(struct dsim_device *dsim, int xres, int yres)
{
	int ret = 0;

	switch( xres ) {
	case 1080:
		ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_DDI_SCALER_FHD_00, ARRAY_SIZE(S6E3HA2_SEQ_DDI_SCALER_FHD_00));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA2_SEQ_DDI_SCALER_FHD_00\n", __func__);
		}
	break;
	case 1440:
		ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_DDI_SCALER_WQHD_00, ARRAY_SIZE(S6E3HA2_SEQ_DDI_SCALER_WQHD_00));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA2_SEQ_DDI_SCALER_WQHD_00\n", __func__);
		}
	break;
	default:
		dsim_err("%s : xres=%d, yres=%d, Unknown\n", __func__, xres, yres );
	break;
	}

	dsim_info("%s : xres=%d, yres=%d\n", __func__, xres, yres );
	return ret;
}

static int s6e3ha2_wqhd_dsu_command(struct dsim_device *dsim)
{
	int ret = 0;

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = _s6e3ha2_wqhd_dsu_command( dsim, dsim->dsu_xres, dsim->dsu_yres );

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
	}

	dsim_info("%s : xres=%d, yres=%d\n", __func__, dsim->dsu_xres, dsim->dsu_yres);
	return ret;
}
#endif

static int s6e3ha2_wqhd_init(struct dsim_device *dsim)
{
        int     ret = 0;

        dsim_info("MDD : %s was called\n", __func__);

        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
                goto init_exit;
        }

        /* 7. Sleep Out(11h) */
        ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
                goto init_exit;
        }
        msleep(5);

        /* 9. Interface Setting */

        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_SINGLE_DSI_1, ARRAY_SIZE(S6E3HA2_SEQ_SINGLE_DSI_1));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_SINGLE_DSI_1\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_SINGLE_DSI_2, ARRAY_SIZE(S6E3HA2_SEQ_SINGLE_DSI_2));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_SINGLE_DSI_2\n", __func__);
                goto init_exit;
        }

#ifdef CONFIG_FB_DSU
	ret = _s6e3ha2_wqhd_dsu_command( dsim, dsim->dsu_xres, dsim->dsu_yres );
#endif

#ifdef CONFIG_LCD_HMT
        if (dsim->priv.hmt_on != HMT_ON)
#endif
                msleep(120);

#ifdef CONFIG_ALWAYS_RELOAD_MTP_FACTORY_BUILD
	ret = lcd_reload_mtp(dynamic_lcd_type, dsim);
#endif

        /* Common Setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_TE_ON, ARRAY_SIZE(S6E3HA2_SEQ_TE_ON));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_TSP_TE, ARRAY_SIZE(S6E3HA2_SEQ_TSP_TE));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_PENTILE_SETTING, ARRAY_SIZE(S6E3HA2_SEQ_PENTILE_SETTING));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_PENTILE_SETTING\n", __func__);
                goto init_exit;
        }

        /* POC setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_POC_SETTING1, ARRAY_SIZE(S6E3HA2_SEQ_POC_SETTING1));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_POC_SETTING1\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_POC_SETTING2, ARRAY_SIZE(S6E3HA2_SEQ_POC_SETTING2));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_POC_SETTING2\n", __func__);
                goto init_exit;
        }

        /* OSC setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_OSC_SETTING1, ARRAY_SIZE(S6E3HA2_SEQ_OSC_SETTING1));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HA2_SEQ_OSC1\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_OSC_SETTING2, ARRAY_SIZE(S6E3HA2_SEQ_OSC_SETTING2));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HA2_SEQ_OSC2\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_OSC_SETTING3, ARRAY_SIZE(S6E3HA2_SEQ_OSC_SETTING3));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : S6E3HA2_SEQ_OSC3\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
                goto init_exit;
        }

        /* PCD setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_PCD_SETTING, ARRAY_SIZE(S6E3HA2_SEQ_PCD_SETTING));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_PCD_SETTING\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_ERR_FG_SETTING, ARRAY_SIZE(S6E3HA2_SEQ_ERR_FG_SETTING));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_ERR_FG_SETTING\n", __func__);
                goto init_exit;
        }

#ifndef CONFIG_PANEL_AID_DIMMING
        /* Brightness Setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(S6E3HA2_SEQ_GAMMA_CONDITION_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_AOR_CONTROL, ARRAY_SIZE(S6E3HA2_SEQ_AOR_CONTROL));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_AOR_CONTROL\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_ELVSS_SET, ARRAY_SIZE(S6E3HA2_SEQ_ELVSS_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ELVSS_SET\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_VINT_SET, ARRAY_SIZE(S6E3HA2_SEQ_VINT_SET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ELVSS_SET\n", __func__);
                goto init_exit;
        }

        ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
                goto init_exit;
        }

        /* ACL Setting */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HA2_SEQ_ACL_OFF));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_ACL_OFF_OPR, ARRAY_SIZE(S6E3HA2_SEQ_ACL_OFF_OPR));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
                goto init_exit;
        }
        /* elvss */
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_TSET_GLOBAL, ARRAY_SIZE(S6E3HA2_SEQ_TSET_GLOBAL));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_TSET_GLOBAL\n", __func__);
                goto init_exit;
        }
        ret = dsim_write_hl_data(dsim, S6E3HA2_SEQ_TSET, ARRAY_SIZE(S6E3HA2_SEQ_TSET));
        if (ret < 0) {
                dsim_err(":%s fail to write CMD : SEQ_TSET\n", __func__);
                goto init_exit;
        }
#endif
        ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
        if (ret < 0) {
                dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
                goto init_exit;
        }

      init_exit:
        return ret;
}



struct dsim_panel_ops s6e3ha2_panel_ops = {
        .probe = s6e3ha2_wqhd_probe,
        .displayon = s6e3ha2_wqhd_displayon,
        .exit = s6e3ha2_wqhd_exit,
        .init = s6e3ha2_wqhd_init,
        .dump = s6e3ha2_wqhd_dump,
#ifdef CONFIG_FB_DSU
	.dsu_cmd = s6e3ha2_wqhd_dsu_command,
#endif
};

/************************************ HA3 *****************************************/

// convert NEW SPEC to OLD SPEC
static int s6e3ha3_VT_RGB2GRB( unsigned char *VT )
{
	int ret = 0;
	int r, g, b;

	// new SPEC = RG0B
	r = (VT[0] & 0xF0) >>4;
	g = (VT[0] & 0x0F);
	b = (VT[1] & 0x0F);

	// old spec = GR0B
	VT[0] = g<<4 | r;
	VT[1] = b;

	return ret;
}


static int s6e3ha3_read_reg_status(struct dsim_device *dsim, bool need_key_unlock )
{
	static int cnt = 0;
	static int err_cnt = 0;
	int ret = 0;
	int result = 0;

	unsigned char reg_buffer_mic[S6E3HA3_REG_MIC_LEN + 4] = { 0, };

	if( need_key_unlock ) {
		ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		}
	}

	ret = dsim_read_hl_data(dsim, S6E3HA3_REG_MIC_ADDR, S6E3HA3_REG_MIC_LEN, reg_buffer_mic);

	if(reg_buffer_mic[0] != S6E3HA3_SEQ_MIC[1] ) {
		dsim_err( "%s : register unMatch detected(%d,%d). MIC(%02x.%02x)->%02x.%02x\n",
			__func__, cnt, ++err_cnt, S6E3HA3_SEQ_MIC[0], S6E3HA3_SEQ_MIC[1], S6E3HA3_REG_MIC_ADDR, reg_buffer_mic[0] );
		result = 1;
	} else {
		dsim_info( "%s : register matched(%d,%d).\n", __func__, ++cnt, err_cnt );
	}

	if( need_key_unlock ) {
		ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		}
	}
	ret = 0;

	return result;
}


static int s6e3ha3_read_init_info(struct dsim_device *dsim, unsigned char *mtp, unsigned char *hbm)
{
	int     i = 0;
	int     ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char buf[S6E3HA3_MTP_DATE_SIZE] = { 0, };
	unsigned char bufForCoordi[S6E3HA3_COORDINATE_LEN] = { 0, };
	unsigned char hbm_gamma[S6E3HA3_HBMGAMMA_LEN] = { 0, };
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = dsim_read_hl_data(dsim, S6E3HA3_ID_REG, S6E3HA3_ID_LEN, dsim->priv.id);
	if (ret != S6E3HA3_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3HA3_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	ret = dsim_read_hl_data(dsim, S6E3HA3_MTP_ADDR, S6E3HA3_MTP_DATE_SIZE, buf);
	if (ret != S6E3HA3_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	s6e3ha3_VT_RGB2GRB(buf + S6E3HA3_MTP_VT_ADDR);
	memcpy(mtp, buf, S6E3HA3_MTP_SIZE);
	memcpy(dsim->priv.date, &buf[40], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3HA3_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E3HA3_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	// coordinate
	ret = dsim_read_hl_data(dsim, S6E3HA3_COORDINATE_REG, S6E3HA3_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3HA3_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];      /* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];      /* Y */
	dsim_info("READ coordi : ");
	for (i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");

	// code
	ret = dsim_read_hl_data(dsim, S6E3HA3_CODE_REG, S6E3HA3_CODE_LEN, dsim->priv.code);
	if (ret != S6E3HA3_CODE_LEN) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ code : ");
	for (i = 0; i < S6E3HA3_CODE_LEN; i++)
		dsim_info("%x, ", dsim->priv.code[i]);
	dsim_info("\n");

	// aid

	memcpy(dsim->priv.aid, &S6E3HA3_SEQ_AOR[1], ARRAY_SIZE(S6E3HA3_SEQ_AOR) - 1);
	dsim_info("READ aid : ");
	for (i = 0; i < S6E3HA3_AID_CMD_CNT - 1; i++)
		dsim_info("%x, ", dsim->priv.aid[i]);
	dsim_info("\n");

	// tset
	ret = dsim_read_hl_data(dsim, S6E3HA3_TSET_REG, S6E3HA3_TSET_LEN - 1, dsim->priv.tset);
	if (ret < S6E3HA3_TSET_LEN - 1) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ tset : ");
	for (i = 0; i < S6E3HA3_TSET_LEN - 1; i++)
		dsim_info("%x, ", dsim->priv.tset[i]);
	dsim_info("\n");

	// elvss
	ret = dsim_read_hl_data(dsim, S6E3HA3_ELVSS_REG, S6E3HA3_ELVSS_LEN - 1, dsim->priv.elvss_set);
	if (ret < S6E3HA3_ELVSS_LEN - 1) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("READ elvss : ");
	for (i = 0; i < S6E3HA3_ELVSS_LEN - 1; i++)
		dsim_info("%x \n", dsim->priv.elvss_set[i]);

	/* read hbm elvss for hbm interpolation */
	panel->hbm_elvss = dsim->priv.elvss_set[S6E3HA3_HBM_ELVSS_INDEX];

	ret = dsim_read_hl_data(dsim, S6E3HA3_HBMGAMMA_REG, S6E3HA3_HBMGAMMA_LEN, hbm_gamma);
	if (ret != S6E3HA3_HBMGAMMA_LEN) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("HBM Gamma : ");
	memcpy(hbm, hbm_gamma, S6E3HA3_HBMGAMMA_LEN);

	for (i = 0; i < S6E3HA3_HBMGAMMA_LEN; i++)
		dsim_info("hbm gamma[%d] : %x\n", i, hbm_gamma[i]);
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto read_exit;
	}
	ret = 0;

read_exit:
	return 0;

read_fail:
	return -ENODEV;
}
static int s6e3ha3_wqhd_dump(struct dsim_device *dsim)
{
	int     ret = 0;
	int     i;
	unsigned char id[S6E3HA3_ID_LEN];
	unsigned char rddpm[4];
	unsigned char rddsm[4];
	unsigned char err_buf[4];

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
	}

	ret = dsim_read_hl_data(dsim, 0xEA, 3, err_buf);
	if (ret != 3) {
		dsim_err("%s : can't read Panel's EA Reg\n", __func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's 0xEA Reg Value ===\n");
	dsim_info("* 0xEA : buf[0] = %x\n", err_buf[0]);
	dsim_info("* 0xEA : buf[1] = %x\n", err_buf[1]);

	ret = dsim_read_hl_data(dsim, S6E3HA3_RDDPM_ADDR, 3, rddpm);
	if (ret != 3) {
		dsim_err("%s : can't read RDDPM Reg\n", __func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's RDDPM Reg Value : %x ===\n", rddpm[0]);

	if (rddpm[0] & 0x80)
		dsim_info("* Booster Voltage Status : ON\n");
	else
		dsim_info("* Booster Voltage Status : OFF\n");

	if (rddpm[0] & 0x40)
		dsim_info("* Idle Mode : On\n");
	else
		dsim_info("* Idle Mode : OFF\n");

	if (rddpm[0] & 0x20)
		dsim_info("* Partial Mode : On\n");
	else
		dsim_info("* Partial Mode : OFF\n");

	if (rddpm[0] & 0x10)
		dsim_info("* Sleep OUT and Working Ok\n");
	else
		dsim_info("* Sleep IN\n");

	if (rddpm[0] & 0x08)
		dsim_info("* Normal Mode On and Working Ok\n");
	else
		dsim_info("* Sleep IN\n");

	if (rddpm[0] & 0x04)
		dsim_info("* Display On and Working Ok\n");
	else
		dsim_info("* Display Off\n");

	ret = dsim_read_hl_data(dsim, S6E3HA3_RDDSM_ADDR, 3, rddsm);
	if (ret != 3) {
		dsim_err("%s : can't read RDDSM Reg\n", __func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's RDDSM Reg Value : %x ===\n", rddsm[0]);

	if (rddsm[0] & 0x80)
		dsim_info("* TE On\n");
	else
		dsim_info("* TE OFF\n");

	if (rddsm[0] & 0x02)
		dsim_info("* S_DSI_ERR : Found\n");

	if (rddsm[0] & 0x01)
		dsim_info("* DSI_ERR : Found\n");

	// id
	ret = dsim_read_hl_data(dsim, S6E3HA3_ID_REG, S6E3HA3_ID_LEN, id);
	if (ret != S6E3HA3_ID_LEN) {
		dsim_err("%s : can't read panel id\n", __func__);
		goto dump_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3HA3_ID_LEN; i++)
		dsim_info("%02x, ", id[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
	}
dump_exit:
	dsim_info(" - %s\n", __func__);
	return ret;

}
static int s6e3ha3_wqhd_probe(struct dsim_device *dsim)
{
	int     ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3HA3_MTP_SIZE] = { 0, };
	unsigned char hbm[S6E3HA3_HBMGAMMA_LEN] = { 0, };
	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
#ifdef CONFIG_LCD_ALPM
	panel->alpm = 0;
	panel->current_alpm = 0;
	mutex_init(&panel->alpm_lock);
	panel->alpm_support = 0;
#endif

	dsim_info(" +  : %s\n", __func__);

	ret = s6e3ha3_read_init_info(dsim, mtp, hbm);
	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

	if ((panel->id[2] & 0x0F) < 2)	// under rev.B
		dsim->priv.esd_disable = 1;
	else dsim->priv.esd_disable = 0;

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(dsim, mtp, hbm);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif
#ifdef CONFIG_LCD_HMT
	ret = hmt_init_dimming(dsim, mtp);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	panel->mdnie_support = 1;
#endif

probe_exit:
	return ret;

}


static int s6e3ha3_wqhd_displayon(struct dsim_device *dsim)
{
	int     ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

displayon_err:
	return ret;

}

static int s6e3ha3_wqhd_exit(struct dsim_device *dsim)
{
	int     ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(120);

exit_err:
	return ret;
}

#if defined(CONFIG_FB_DSU)
#undef CONFIG_HA3_CASET_PASET_CHECK
static int _s6e3ha3_wqhd_dsu_command(struct dsim_device *dsim, int xres, int yres )
{
	int ret = 0;
	unsigned char read_reg[3] = {0xFF,};
#ifdef CONFIG_HA3_CASET_PASET_CHECK
	const unsigned char SEQ_HA3_CASET_PASET_GPARAM[] = { 0xB0, 0x13 };
	const unsigned char REG_HA3_CASET_PASET = 0xFB;
	const unsigned char size_ha3_caset_paset = 8;
	char	buffer_caset_paset[size_ha3_caset_paset+4];
	u16 *pint16;
	int i;
#endif

	switch( xres ) {
	case 720:
		dsim_err("%s : xres=%d, yres=%d : HD\n", __func__, xres, yres );
		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_HD_00, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_HD_00));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DDI_SCALER_HD_00\n", __func__);
		}

		ret = dsim_read_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_HD_00[0], ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_HD_00) - 1 , read_reg);

		if(read_reg[0] != S6E3HA3_SEQ_DDI_SCALER_HD_00[1]) {
			dsim_err("%s : mis-match BA register %x %x\n", __func__, read_reg[0], S6E3HA3_SEQ_DDI_SCALER_HD_00[1]);
			ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
			ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_HD_00, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_HD_00));
		} else {
			dsim_info("%s : read value %x\n", __func__, read_reg[0]);
		}

		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_HD_01, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_HD_01));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DDI_SCALER_HD_01\n", __func__);
		}
		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_HD_02, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_HD_02));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DDI_SCALER_HD_02\n", __func__);
		}
	break;
	case 1080:
		dsim_err("%s : xres=%d, yres=%d : FHD\n", __func__, xres, yres );
		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_FHD_00, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_FHD_00));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DDI_SCALER_FHD_00\n", __func__);
		}

		ret = dsim_read_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_FHD_00[0], ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_FHD_00) - 1 , read_reg);

		if(read_reg[0] != S6E3HA3_SEQ_DDI_SCALER_FHD_00[1]) {
			dsim_err("%s : mis-match BA register %x %x\n", __func__, read_reg[0], S6E3HA3_SEQ_DDI_SCALER_FHD_00[1]);
			ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
			ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_FHD_00, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_FHD_00));
		} else {
			dsim_info("%s : read value %x\n", __func__, read_reg[0]);
		}
		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_FHD_01, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_FHD_01));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DDI_SCALER_FHD_01\n", __func__);
		}
		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_FHD_02, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_FHD_02));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DDI_SCALER_FHD_02\n", __func__);
		}
	break;
	case 1440:
		dsim_err("%s : xres=%d, yres=%d : WQHD\n", __func__, xres, yres );
		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_WQHD_00, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_WQHD_00));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DDI_SCALER_WQHD_00\n", __func__);
		}
		ret = dsim_read_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_WQHD_00[0], ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_WQHD_00) - 1 , read_reg);

		if(read_reg[0] != S6E3HA3_SEQ_DDI_SCALER_WQHD_00[1]) {
			dsim_err("%s : mis-match BA register %x %x\n", __func__, read_reg[0], S6E3HA3_SEQ_DDI_SCALER_WQHD_00[1]);
			ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
			ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_WQHD_00, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_WQHD_00));
		} else {
			dsim_info("%s : read value %x\n", __func__, read_reg[0]);
		}
	break;
	default:
		dsim_err("%s : xres=%d, yres=%d : default\n", __func__, xres, yres );
	break;
	}


#ifdef CONFIG_HA3_CASET_PASET_CHECK
	ret = dsim_write_hl_data(dsim, SEQ_HA3_CASET_PASET_GPARAM, ARRAY_SIZE(SEQ_HA3_CASET_PASET_GPARAM));
	ret = dsim_read_hl_data(dsim, REG_HA3_CASET_PASET, size_ha3_caset_paset, buffer_caset_paset);
	pint16 = (u16*) buffer_caset_paset;
	for( i = 0; i < size_ha3_caset_paset/sizeof(pint16[0]); i++ ) {
		pint16[i] = ((pint16[i] & 0xF0) >> 4) + ((pint16[i]&0x0F)<<4);
	}
	dsim_info( "%s.%d (dsu) caset paset(%d) : %d, %d, %d, %d\n", __func__, __LINE__, ret, pint16[0], pint16[1], pint16[2], pint16[3] );
#endif

	return ret;
}
#endif

#ifdef CONFIG_FB_DSU
static int s6e3ha3_wqhd_dsu_command(struct dsim_device *dsim)
{
	int ret = 0;

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = _s6e3ha3_wqhd_dsu_command( dsim, dsim->dsu_xres, dsim->dsu_yres );

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
	}

	dsim_info("%s : xres=%d, yres=%d\n", __func__, dsim->dsu_xres, dsim->dsu_yres);
	return ret;
}
#endif

static int s6e3ha3_wqhd_init(struct dsim_device *dsim)
{
	int ret = 0;
	int cnt;
	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}
	msleep(5);

	/* 7. Sleep Out(11h) */
	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(5);

	/* 9. Interface Setting */
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DISPLAY_TIMING1, ARRAY_SIZE(S6E3HA3_SEQ_DISPLAY_TIMING1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DISPLAY_TIMING1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DISPLAY_TIMING2, ARRAY_SIZE(S6E3HA3_SEQ_DISPLAY_TIMING2));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DISPLAY_TIMING2\n", __func__);
		goto init_exit;
	}


	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_FREQ_CALIBRATION, ARRAY_SIZE(S6E3HA3_SEQ_FREQ_CALIBRATION));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_MIC\n", __func__);
		goto init_exit;
	}

	/* 9. Interface Setting */
	cnt = 0;
	do {
		if( cnt>0 ) dsim_err( "%s : DSI/MIC cmd retry\n", __func__ );

		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_MIC, ARRAY_SIZE(S6E3HA3_SEQ_MIC));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_MIC\n", __func__);
		}
	} while( s6e3ha3_read_reg_status(dsim, false ) && cnt++ <3 );

	if (hw_rev < 3) {
		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DCDC_GLOBAL, ARRAY_SIZE(S6E3HA3_SEQ_DCDC_GLOBAL));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DCDC_GLOBAL\n", __func__);
			goto init_exit;
		}
		ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DCDC, ARRAY_SIZE(S6E3HA3_SEQ_DCDC));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DCDC\n", __func__);
			goto init_exit;
		}
		dsim_info("%s max77838 system rev : %d\n", __func__, hw_rev);
	}

#ifdef CONFIG_FB_DSU
	ret = _s6e3ha3_wqhd_dsu_command( dsim, dsim->dsu_xres, dsim->dsu_yres );
#endif

#ifdef CONFIG_LCD_HMT
		if(dsim->priv.hmt_on != HMT_ON)
#endif
	msleep(120);

#ifdef CONFIG_ALWAYS_RELOAD_MTP_FACTORY_BUILD
	ret = lcd_reload_mtp(dynamic_lcd_type, dsim);
#endif

	/* Common Setting */
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_TE_RISING_TIMING, ARRAY_SIZE(S6E3HA3_SEQ_TE_RISING_TIMING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_TE_ON, ARRAY_SIZE(S6E3HA3_SEQ_TE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_TSP_TE, ARRAY_SIZE(S6E3HA3_SEQ_TSP_TE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_PCD, ARRAY_SIZE(S6E3HA3_SEQ_PCD));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_PCD\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_ERR_FG, ARRAY_SIZE(S6E3HA3_SEQ_ERR_FG));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_PCD\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_PENTILE_SETTING, ARRAY_SIZE(S6E3HA3_SEQ_PENTILE_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_PCD\n", __func__);
		goto init_exit;
	}

#ifndef CONFIG_PANEL_AID_DIMMING
	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(S6E3HA3_SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_AOR, ARRAY_SIZE(S6E3HA3_SEQ_AOR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : S6E3HA3_SEQ_AOR_0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_MPS_ELVSS_SET, ARRAY_SIZE(S6E3HA3_SEQ_MPS_ELVSS_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : S6E3HA3_SEQ_MPS_ELVSS_SET\n", __func__);
		goto init_exit;
	}

	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HA3_SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_ACL_OFF_OPR, ARRAY_SIZE(S6E3HA3_SEQ_ACL_OFF_OPR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	/* elvss */
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_ELVSS_GLOBAL, ARRAY_SIZE(S6E3HA3_SEQ_ELVSS_GLOBAL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : S6E3HA3_SEQ_ELVSS_GLOBAL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_ELVSS, ARRAY_SIZE(S6E3HA3_SEQ_ELVSS));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : S6E3HA3_SEQ_ELVSS\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_TSET_GLOBAL, ARRAY_SIZE(S6E3HA3_SEQ_TSET_GLOBAL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET_GLOBAL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_TSET, ARRAY_SIZE(S6E3HA3_SEQ_TSET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_VINT_SET, ARRAY_SIZE(S6E3HA3_SEQ_VINT_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : S6E3HA3_SEQ_VINT_SET\n", __func__);
		goto init_exit;
	}
#endif
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;
}



struct dsim_panel_ops s6e3ha3_panel_ops = {
	.probe = s6e3ha3_wqhd_probe,
	.displayon = s6e3ha3_wqhd_displayon,
	.exit = s6e3ha3_wqhd_exit,
	.init = s6e3ha3_wqhd_init,
	.dump = s6e3ha3_wqhd_dump,
#ifdef CONFIG_FB_DSU
	.dsu_cmd = s6e3ha3_wqhd_dsu_command,
#endif
};

/******************** HF3 ********************/


static int s6e3hf3_read_init_info(struct dsim_device *dsim, unsigned char *mtp, unsigned char *hbm)
{
	int     i = 0;
	int     ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char buf[S6E3HF3_MTP_DATE_SIZE] = { 0, };
	unsigned char bufForCoordi[S6E3HF3_COORDINATE_LEN] = { 0, };
	unsigned char hbm_gamma[S6E3HF3_HBMGAMMA_LEN] = { 0, };
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = dsim_read_hl_data(dsim, S6E3HF3_ID_REG, S6E3HF3_ID_LEN, dsim->priv.id);
	if (ret != S6E3HF3_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3HF3_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	ret = dsim_read_hl_data(dsim, S6E3HF3_MTP_ADDR, S6E3HF3_MTP_DATE_SIZE, buf);
	if (ret != S6E3HF3_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	s6e3ha3_VT_RGB2GRB(buf + S6E3HF3_MTP_VT_ADDR);
	memcpy(mtp, buf, S6E3HF3_MTP_SIZE);
	memcpy(dsim->priv.date, &buf[40], min((int)(S6E3HF3_MTP_DATE_SIZE - 40), (int)ARRAY_SIZE(dsim->priv.date)));
	dsim_info("READ MTP SIZE : %d\n", S6E3HF3_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E3HF3_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	// coordinate
	ret = dsim_read_hl_data(dsim, S6E3HF3_COORDINATE_REG, S6E3HF3_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3HF3_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];      /* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];      /* Y */
	dsim_info("READ coordi : ");
	for (i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");

	// code
	ret = dsim_read_hl_data(dsim, S6E3HF3_CODE_REG, S6E3HF3_CODE_LEN, dsim->priv.code);
	if (ret != S6E3HF3_CODE_LEN) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ code : ");
	for (i = 0; i < S6E3HF3_CODE_LEN; i++)
		dsim_info("%x, ", dsim->priv.code[i]);
	dsim_info("\n");

	// aid
	memcpy(dsim->priv.aid, &S6E3HF3_SEQ_AOR[1], ARRAY_SIZE(S6E3HF3_SEQ_AOR) - 1);
	dsim_info("READ aid : ");
	for (i = 0; i < S6E3HF3_AID_CMD_CNT - 1; i++)
		dsim_info("%x, ", dsim->priv.aid[i]);
	dsim_info("\n");

	// tset
	ret = dsim_read_hl_data(dsim, S6E3HF3_TSET_REG, S6E3HF3_TSET_LEN - 1, dsim->priv.tset);
	if (ret < S6E3HF3_TSET_LEN - 1) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ tset : ");
	for (i = 0; i < S6E3HF3_TSET_LEN - 1; i++)
		dsim_info("%x, ", dsim->priv.tset[i]);
	dsim_info("\n");

	// elvss
	ret = dsim_read_hl_data(dsim, S6E3HF3_ELVSS_REG, S6E3HF3_ELVSS_LEN - 1, dsim->priv.elvss_set);
	if (ret < S6E3HF3_ELVSS_LEN - 1) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("READ elvss : ");
	for (i = 0; i < S6E3HF3_ELVSS_LEN - 1; i++)
		dsim_info("%x \n", dsim->priv.elvss_set[i]);

	/* read hbm elvss for hbm interpolation */
	panel->hbm_elvss = dsim->priv.elvss_set[S6E3HF3_HBM_ELVSS_INDEX];

	ret = dsim_read_hl_data(dsim, S6E3HF3_HBMGAMMA_REG, S6E3HF3_HBMGAMMA_LEN, hbm_gamma);
	if (ret != S6E3HF3_HBMGAMMA_LEN) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("HBM Gamma : ");
	memcpy(hbm, hbm_gamma, S6E3HF3_HBMGAMMA_LEN);

	for (i = 0; i < S6E3HF3_HBMGAMMA_LEN; i++)
		dsim_info("hbm gamma[%d] : %x\n", i, hbm_gamma[i]);
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto read_exit;
	}
	ret = 0;

read_exit:
	return 0;

read_fail:
	return -ENODEV;
}

static int s6e3hf3_wqhd_dump(struct dsim_device *dsim)
{
	int     ret = 0;
	int     i;
	unsigned char id[S6E3HF3_ID_LEN];
	unsigned char rddpm[4];
	unsigned char rddsm[4];
	unsigned char err_buf[4];

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
	}

	ret = dsim_read_hl_data(dsim, 0xEA, 3, err_buf);
	if (ret != 3) {
		dsim_err("%s : can't read Panel's EA Reg\n", __func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's 0xEA Reg Value ===\n");
	dsim_info("* 0xEA : buf[0] = %x\n", err_buf[0]);
	dsim_info("* 0xEA : buf[1] = %x\n", err_buf[1]);

	ret = dsim_read_hl_data(dsim, S6E3HF3_RDDPM_ADDR, 3, rddpm);
	if (ret != 3) {
		dsim_err("%s : can't read RDDPM Reg\n", __func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's RDDPM Reg Value : %x ===\n", rddpm[0]);

	if (rddpm[0] & 0x80)
		dsim_info("* Booster Voltage Status : ON\n");
	else
		dsim_info("* Booster Voltage Status : OFF\n");

	if (rddpm[0] & 0x40)
		dsim_info("* Idle Mode : On\n");
	else
		dsim_info("* Idle Mode : OFF\n");

	if (rddpm[0] & 0x20)
		dsim_info("* Partial Mode : On\n");
	else
		dsim_info("* Partial Mode : OFF\n");

	if (rddpm[0] & 0x10)
		dsim_info("* Sleep OUT and Working Ok\n");
	else
		dsim_info("* Sleep IN\n");

	if (rddpm[0] & 0x08)
		dsim_info("* Normal Mode On and Working Ok\n");
	else
		dsim_info("* Sleep IN\n");

	if (rddpm[0] & 0x04)
		dsim_info("* Display On and Working Ok\n");
	else
		dsim_info("* Display Off\n");

	ret = dsim_read_hl_data(dsim, S6E3HF3_RDDSM_ADDR, 3, rddsm);
	if (ret != 3) {
		dsim_err("%s : can't read RDDSM Reg\n", __func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's RDDSM Reg Value : %x ===\n", rddsm[0]);

	if (rddsm[0] & 0x80)
		dsim_info("* TE On\n");
	else
		dsim_info("* TE OFF\n");

	if (rddsm[0] & 0x02)
		dsim_info("* S_DSI_ERR : Found\n");

	if (rddsm[0] & 0x01)
		dsim_info("* DSI_ERR : Found\n");

	// id
	ret = dsim_read_hl_data(dsim, S6E3HF3_ID_REG, S6E3HF3_ID_LEN, id);
	if (ret != S6E3HF3_ID_LEN) {
		dsim_err("%s : can't read panel id\n", __func__);
		goto dump_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3HF3_ID_LEN; i++)
		dsim_info("%02x, ", id[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
	}
dump_exit:
	dsim_info(" - %s\n", __func__);
	return ret;

}

static int s6e3hf3_wqhd_probe(struct dsim_device *dsim)
{
	int     ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3HF3_MTP_SIZE] = { 0, };
	unsigned char hbm[S6E3HF3_HBMGAMMA_LEN] = { 0, };
	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	dsim->glide_display_size = 80;	// framebuffer of LCD is 1600x2560, display area is 1440x2560, glidesize = (1600-1440)/2
#ifdef CONFIG_LCD_ALPM
	panel->alpm = 0;
	panel->current_alpm = 0;
	mutex_init(&panel->alpm_lock);
	panel->alpm_support = 1;
#endif

	dsim_info(" +  : %s\n", __func__);

	ret = s6e3hf3_read_init_info(dsim, mtp, hbm);
	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

	if((panel->id[2] & 0x0F) < 5)	// under rev.E
		dsim->priv.esd_disable = 1;
	else
		dsim->priv.esd_disable = 0;



	/* test */
/*
	panel->id[0] = 0x17;
	panel->id[1] = 0x20;
	panel->id[2] = 0x83;
*/
#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(dsim, mtp, hbm);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif
#ifdef CONFIG_LCD_HMT
	ret = hmt_init_dimming(dsim, mtp);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	panel->mdnie_support = 1;
#endif

probe_exit:
	return ret;

}


static int s6e3hf3_wqhd_displayon(struct dsim_device *dsim)
{
	int ret = 0;
#ifdef CONFIG_LCD_ALPM
	struct panel_private *panel = &dsim->priv;
#endif

	dsim_info("MDD : %s was called\n", __func__);

#ifdef CONFIG_LCD_ALPM
	if (panel->current_alpm && panel->alpm) {
		 dsim_info("%s : ALPM mode\n", __func__);
	} else if (panel->current_alpm) {
		ret = alpm_set_mode(dsim, ALPM_OFF);
		if (ret) {
			dsim_err("failed to exit alpm.\n");
			goto displayon_err;
		}
	} else {
		if (panel->alpm) {
			ret = alpm_set_mode(dsim, ALPM_ON);
			if (ret) {
				dsim_err("failed to initialize alpm.\n");
				goto displayon_err;
			}
		} else {
			ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
			if (ret < 0) {
				dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
				goto displayon_err;
			}
		}
	}
#else
	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}
#endif

displayon_err:
	return ret;

}

static int s6e3hf3_wqhd_exit(struct dsim_device *dsim)
{
	int ret = 0;
#ifdef CONFIG_LCD_ALPM
	struct panel_private *panel = &dsim->priv;
#endif
	dsim_info("MDD : %s was called\n", __func__);
#ifdef CONFIG_LCD_ALPM
	mutex_lock(&panel->alpm_lock);
	if (panel->current_alpm && panel->alpm) {
		dsim->alpm = 1;
		dsim_info("%s : ALPM mode\n", __func__);
	}
	else {
		ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
			goto exit_err;
		}

		ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
			goto exit_err;
		}
		msleep(120);
	}

	dsim_info("MDD : %s was called unlock\n", __func__);
#else
	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(120);
#endif

exit_err:
#ifdef CONFIG_LCD_ALPM
	mutex_unlock(&panel->alpm_lock);
#endif
	return ret;
}


static int s6e3hf3_read_reg_status(struct dsim_device *dsim, bool need_key_unlock )
{
	static int cnt = 0;
	static int err_cnt = 0;
	int ret = 0;
	int result = 0;

	unsigned char reg_buffer_mic[S6E3HF3_REG_MIC_LEN + 4] = { 0, };

	if( need_key_unlock ) {
		ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		}
	}

	ret = dsim_read_hl_data(dsim, S6E3HF3_REG_MIC_ADDR, S6E3HF3_REG_MIC_LEN, reg_buffer_mic);

	if(reg_buffer_mic[0] != S6E3HF3_SEQ_MIC[1] ) {
		dsim_err( "%s : register unMatch detected(%d,%d). MIC(%02x.%02x)->%02x.%02x\n",
			__func__, cnt, ++err_cnt, S6E3HF3_SEQ_MIC[0], S6E3HF3_SEQ_MIC[1], S6E3HF3_REG_MIC_ADDR, reg_buffer_mic[0] );
		result = 1;
	} else {
		dsim_info( "%s : register matched(%d,%d).\n", __func__, ++cnt, err_cnt );
	}

	if( need_key_unlock ) {
		ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		}
	}
	ret = 0;

	return result;
}


#if defined(CONFIG_FB_DSU) || defined(CONFIG_LCD_RES)
#undef CONFIG_HF3_CASET_PASET_CHECK
static int _s6e3hf3_wqhd_dsu_command(struct dsim_device *dsim, int xres, int yres )
{
	int ret = 0;
	unsigned char read_reg[3] = {0xFF, };
#ifdef CONFIG_HF3_CASET_PASET_CHECK
	const unsigned char SEQ_HF3_CASET_PASET_GPARAM[] = { 0xB0, 0x13 };
	const unsigned char REG_HF3_CASET_PASET = 0xFB;
	const unsigned char size_hf3_caset_paset = 8;
	char	buffer_caset_paset[size_hf3_caset_paset+4];
	u16 *pint16;
	int i;
#endif

	switch( xres ) {
	case 720:
		dsim->glide_display_size = 40;
		dsim_err("%s : xres=%d, yres=%d : HD\n", __func__, xres, yres );

		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_HD_00, ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_HD_00));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_DDI_SCALER_HD_00\n", __func__);
		}

		ret = dsim_read_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_HD_00[0], ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_HD_00) - 1 , read_reg);

		if(read_reg[0] != S6E3HF3_SEQ_DDI_SCALER_HD_00[1]) {
			dsim_err("%s : mis-match BA register %x %x\n", __func__, read_reg[0], S6E3HF3_SEQ_DDI_SCALER_HD_00[1]);
			ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
			ret = dsim_write_hl_data(dsim, S6E3HA3_SEQ_DDI_SCALER_HD_00, ARRAY_SIZE(S6E3HA3_SEQ_DDI_SCALER_HD_00));
		} else {
			dsim_info("%s : read value %x\n", __func__, read_reg[0]);
		}

		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_HD_01, ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_HD_01));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_DDI_SCALER_HD_01\n", __func__);
		}
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_HD_02, ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_HD_02));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_DDI_SCALER_HD_02\n", __func__);
		}
	break;
	case 1080:
		dsim->glide_display_size = 60;
		dsim_err("%s : xres=%d, yres=%d : FHD\n", __func__, xres, yres );

		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_FHD_00, ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_FHD_00));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_DDI_SCALER_FHD_00\n", __func__);
		}

		ret = dsim_read_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_FHD_00[0], ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_FHD_00) - 1 , read_reg);

		if(read_reg[0] != S6E3HF3_SEQ_DDI_SCALER_FHD_00[1]) {
			dsim_err("%s : mis-match BA register %x %x\n", __func__, read_reg[0], S6E3HF3_SEQ_DDI_SCALER_FHD_00[1]);
			ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
			ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_FHD_00, ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_FHD_00));
		} else {
			dsim_info("%s : read value %x\n", __func__, read_reg[0]);
		}

		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_FHD_01, ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_FHD_01));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_DDI_SCALER_FHD_01\n", __func__);
		}
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_FHD_02, ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_FHD_02));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_DDI_SCALER_FHD_02\n", __func__);
		}
	break;
	case 1440:
		dsim->glide_display_size = 80;	// framebuffer of LCD is 1600x2560, display area is 1440x2560, glidesize = (1600-1440)/2
		dsim_err("%s : xres=%d, yres=%d : WQHD\n", __func__, xres, yres );

		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_WQHD_00, ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_WQHD_00));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_DDI_SCALER_WQHD_00\n", __func__);
		}

		ret = dsim_read_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_WQHD_00[0], ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_WQHD_00) - 1 , read_reg);

		if(read_reg[0] != S6E3HF3_SEQ_DDI_SCALER_WQHD_00[1]) {
			dsim_err("%s : mis-match BA register %x %x\n", __func__, read_reg[0], S6E3HF3_SEQ_DDI_SCALER_WQHD_00[1]);
			ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
			ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DDI_SCALER_WQHD_00, ARRAY_SIZE(S6E3HF3_SEQ_DDI_SCALER_WQHD_00));
		} else {
			dsim_info("%s : read value %x\n", __func__, read_reg[0]);
		}

	break;
	default:
		dsim->glide_display_size = 80;	// framebuffer of LCD is 1600x2560, display area is 1440x2560, glidesize = (1600-1440)/2
		dsim_err("%s : xres=%d, yres=%d : default\n", __func__, xres, yres );
	break;
	}


#ifdef CONFIG_HF3_CASET_PASET_CHECK
	ret = dsim_write_hl_data(dsim, SEQ_HF3_CASET_PASET_GPARAM, ARRAY_SIZE(SEQ_HF3_CASET_PASET_GPARAM));
	ret = dsim_read_hl_data(dsim, REG_HF3_CASET_PASET, size_hf3_caset_paset, buffer_caset_paset);
	pint16 = (u16*) buffer_caset_paset;
	for( i = 0; i < size_hf3_caset_paset/sizeof(pint16[0]); i++ ) {
		pint16[i] = ((pint16[i] & 0xF0) >> 4) + ((pint16[i]&0x0F)<<4);
	}
	dsim_info( "%s.%d (dsu) caset paset(%d) : %d, %d, %d, %d\n", __func__, __LINE__, ret, pint16[0], pint16[1], pint16[2], pint16[3] );
#endif

	return ret;
}
#endif

#ifdef CONFIG_FB_DSU
static int s6e3hf3_wqhd_dsu_command(struct dsim_device *dsim)
{
	int ret = 0;

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	ret = _s6e3hf3_wqhd_dsu_command( dsim, dsim->dsu_xres, dsim->dsu_yres );

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
	}

	dsim_info("%s : xres=%d, yres=%d\n", __func__, dsim->dsu_xres, dsim->dsu_yres);
	return ret;
}
#endif

static int s6e3hf3_wqhd_init(struct dsim_device *dsim)
{
	int ret = 0;
	int cnt = 0;
	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}

	/* 7. Sleep Out(11h) */
	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(5);

	/* 9. Interface Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DISPLAY_TIMING1, ARRAY_SIZE(S6E3HF3_SEQ_DISPLAY_TIMING1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DISPLAY_TIMING1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DISPLAY_TIMING2, ARRAY_SIZE(S6E3HF3_SEQ_DISPLAY_TIMING2));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HA3_SEQ_DISPLAY_TIMING2\n", __func__);
		goto init_exit;
	}

	/* 9. Interface Setting */
	cnt = 0;
	do {
		if( cnt>0 ) dsim_err( "%s : DSI/MIC cmd retry\n", __func__ );

		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_MIC, ARRAY_SIZE(S6E3HF3_SEQ_MIC));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_MIC\n", __func__);
		}
	} while( s6e3hf3_read_reg_status(dsim, false ) && cnt++ <3 );

	if (hw_rev < 3) {
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DCDC_GLOBAL, ARRAY_SIZE(S6E3HF3_SEQ_DCDC_GLOBAL));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_DCDC_GLOBAL\n", __func__);
			goto init_exit;
		}
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DCDC, ARRAY_SIZE(S6E3HF3_SEQ_DCDC));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_DCDC\n", __func__);
			goto init_exit;
		}
		dsim_info("%s max77838 system rev : %d\n", __func__, hw_rev);
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_CASET_SET, ARRAY_SIZE(S6E3HF3_SEQ_CASET_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_MIC\n", __func__);
		goto init_exit;
	}

#ifdef CONFIG_FB_DSU
	ret = _s6e3hf3_wqhd_dsu_command( dsim, dsim->dsu_xres, dsim->dsu_yres );
#endif

#ifdef CONFIG_LCD_HMT
		if(dsim->priv.hmt_on != HMT_ON)
#endif
	msleep(120);

#ifdef CONFIG_ALWAYS_RELOAD_MTP_FACTORY_BUILD
	ret = lcd_reload_mtp(dynamic_lcd_type, dsim);
#endif

	/* Common Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TE_RISING_TIMING, ARRAY_SIZE(S6E3HF3_SEQ_TE_RISING_TIMING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TE_ON, ARRAY_SIZE(S6E3HF3_SEQ_TE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TSP_TE, ARRAY_SIZE(S6E3HF3_SEQ_TSP_TE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_PCD, ARRAY_SIZE(S6E3HF3_SEQ_PCD));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_PCD\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ERR_FG, ARRAY_SIZE(S6E3HF3_SEQ_ERR_FG));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_PCD\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_PENTILE_SETTING, ARRAY_SIZE(S6E3HF3_SEQ_PENTILE_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_PCD\n", __func__);
		goto init_exit;
	}

#ifndef CONFIG_PANEL_AID_DIMMING
	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(S6E3HF3_SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_AOR, ARRAY_SIZE(S6E3HF3_SEQ_AOR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : S6E3HF3_SEQ_AOR_0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_MPS_ELVSS_SET, ARRAY_SIZE(S6E3HF3_SEQ_MPS_ELVSS_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : S6E3HF3_SEQ_MPS_ELVSS_SET\n", __func__);
		goto init_exit;
	}

	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HF3_SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ACL_OFF_OPR, ARRAY_SIZE(S6E3HF3_SEQ_ACL_OFF_OPR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	/* elvss */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ELVSS_GLOBAL, ARRAY_SIZE(S6E3HF3_SEQ_ELVSS_GLOBAL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : S6E3HF3_SEQ_ELVSS_GLOBAL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ELVSS, ARRAY_SIZE(S6E3HF3_SEQ_ELVSS));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : S6E3HF3_SEQ_ELVSS\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TSET_GLOBAL, ARRAY_SIZE(S6E3HF3_SEQ_TSET_GLOBAL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET_GLOBAL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TSET, ARRAY_SIZE(S6E3HF3_SEQ_TSET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_VINT_SET, ARRAY_SIZE(S6E3HF3_SEQ_VINT_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : S6E3HF3_SEQ_VINT_SET\n", __func__);
		goto init_exit;
	}
#endif
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;
}




struct dsim_panel_ops s6e3hf3_panel_ops = {
	.probe = s6e3hf3_wqhd_probe,
	.displayon = s6e3hf3_wqhd_displayon,
	.exit = s6e3hf3_wqhd_exit,
	.init = s6e3hf3_wqhd_init,
	.dump = s6e3hf3_wqhd_dump,
#ifdef CONFIG_FB_DSU
	.dsu_cmd = s6e3hf3_wqhd_dsu_command,
#endif
};



/******************** COMMON ********************/

#ifdef CONFIG_ALWAYS_RELOAD_MTP_FACTORY_BUILD
static int lcd_reload_mtp(int lcd_type, struct dsim_device *dsim)
{
	int i, ret;
	unsigned char mtp[S6E3HA2_MTP_SIZE] = { 0, };
	unsigned char hbm[S6E3HA2_HBMGAMMA_LEN] = { 0, };

	// retry 3 times
	for( i=0; i <3; i++ ) {
		switch (lcd_type) {
			case LCD_TYPE_S6E3HA2_WQHD:
				ret = s6e3ha2_read_init_info(dsim, mtp, hbm);
				dsim_info( "%s : load MTP of s6e3ha2\n", __func__ );
				break;
			case LCD_TYPE_S6E3HA3_WQHD:
				ret = s6e3ha3_read_init_info(dsim, mtp, hbm);
				dsim_info( "%s : load MTP of s6e3ha3\n", __func__ );
				break;
			case LCD_TYPE_S6E3HF3_WQHD:
				ret = s6e3hf3_read_init_info(dsim, mtp, hbm);
				dsim_info( "%s : load MTP of s6e3hf3\n", __func__ );
				break;
			default:
				ret = s6e3ha3_read_init_info(dsim, mtp, hbm);
				dsim_info( "%s : load MTP of s6e3ha3(default)\n", __func__ );
				break;
		}
		if( ret == 0 ) break;
	}
	if( ret != 0 ) return -EIO;

	update_mdnie_coordinate( dsim->priv.coordinate[0], dsim->priv.coordinate[1] );

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(dsim, mtp, hbm);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif

#ifdef CONFIG_LCD_HMT
	ret = hmt_init_dimming(dsim, mtp);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif

	return 0;	//success
}
#endif	// CONFIG_ALWAYS_RELOAD_MTP_FACTORY_BUILD


struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
	switch (dynamic_lcd_type) {
	case LCD_TYPE_S6E3HA2_WQHD:
		return &s6e3ha2_panel_ops;
	case LCD_TYPE_S6E3HA3_WQHD:
		return &s6e3ha3_panel_ops;
	case LCD_TYPE_S6E3HF3_WQHD:
		return &s6e3hf3_panel_ops;
	default:
		return &s6e3ha2_panel_ops;
	}
}


static int __init get_lcd_type(char *arg)
{
	unsigned int lcdtype;

	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

#if 1	// kyNam_150430_ HF3-bare LCD
	if (lcdtype == 0x000010) lcdtype = LCD_TYPE_ID_HF3;
#endif

	switch (lcdtype & LCD_TYPE_MASK) {
	case LCD_TYPE_ID_HA2:
		dynamic_lcd_type = LCD_TYPE_S6E3HA2_WQHD;
		dsim_info("LCD TYPE : S6E3HA2 (WQHD) : %d\n", dynamic_lcd_type);
		aid_dimming_dynamic.elvss_len = S6E3HA2_ELVSS_LEN;
		aid_dimming_dynamic.elvss_reg = S6E3HA2_ELVSS_REG;
		aid_dimming_dynamic.elvss_cmd_cnt = S6E3HA2_ELVSS_CMD_CNT;
		aid_dimming_dynamic.aid_cmd_cnt = S6E3HA2_AID_CMD_CNT;
		aid_dimming_dynamic.aid_reg_offset = S6E3HA2_AID_REG_OFFSET;
		aid_dimming_dynamic.tset_len = S6E3HA2_TSET_LEN;
		aid_dimming_dynamic.tset_reg = S6E3HA2_TSET_REG;
		aid_dimming_dynamic.tset_minus_offset = S6E3HA2_TSET_MINUS_OFFSET;
		aid_dimming_dynamic.vint_reg2 = S6E3HA2_VINT_REG2;
		break;
	case LCD_TYPE_ID_HA3:
		dynamic_lcd_type = LCD_TYPE_S6E3HA3_WQHD;
		dsim_info("LCD TYPE : S6E3HA3 (WQHD) : %d\n", dynamic_lcd_type);
		aid_dimming_dynamic.elvss_len = S6E3HA3_ELVSS_LEN;
		aid_dimming_dynamic.elvss_reg = S6E3HA3_ELVSS_REG;
		aid_dimming_dynamic.elvss_cmd_cnt = S6E3HA3_ELVSS_CMD_CNT;
		aid_dimming_dynamic.aid_cmd_cnt = S6E3HA3_AID_CMD_CNT;
		aid_dimming_dynamic.aid_reg_offset = S6E3HA3_AID_REG_OFFSET;
		aid_dimming_dynamic.tset_len = S6E3HA3_TSET_LEN;
		aid_dimming_dynamic.tset_reg = S6E3HA3_TSET_REG;
		aid_dimming_dynamic.tset_minus_offset = S6E3HA3_TSET_MINUS_OFFSET;
		aid_dimming_dynamic.vint_reg2 = S6E3HA3_VINT_REG2;
		break;
	case LCD_TYPE_ID_HF3:
		dynamic_lcd_type = LCD_TYPE_S6E3HF3_WQHD;
		dsim_info("LCD TYPE : S6E3HF3 (WQHD) : %d\n", dynamic_lcd_type);
		aid_dimming_dynamic.elvss_len = S6E3HF3_ELVSS_LEN;
		aid_dimming_dynamic.elvss_reg = S6E3HF3_ELVSS_REG;
		aid_dimming_dynamic.elvss_cmd_cnt = S6E3HF3_ELVSS_CMD_CNT;
		aid_dimming_dynamic.aid_cmd_cnt = S6E3HF3_AID_CMD_CNT;
		aid_dimming_dynamic.aid_reg_offset = S6E3HF3_AID_REG_OFFSET;
		aid_dimming_dynamic.tset_len = S6E3HF3_TSET_LEN;
		aid_dimming_dynamic.tset_reg = S6E3HF3_TSET_REG;
		aid_dimming_dynamic.tset_minus_offset = S6E3HF3_TSET_MINUS_OFFSET;
		aid_dimming_dynamic.vint_reg2 = S6E3HF3_VINT_REG2;
		break;
	default:
		dynamic_lcd_type = LCD_TYPE_S6E3HA2_WQHD;
		dsim_info("LCD TYPE : [UNKNOWN] -> S6E3HA2 (WQHD) : %d\n", dynamic_lcd_type);
		aid_dimming_dynamic.elvss_len = S6E3HA2_ELVSS_LEN;
		aid_dimming_dynamic.elvss_reg = S6E3HA2_ELVSS_REG;
		aid_dimming_dynamic.elvss_cmd_cnt = S6E3HA2_ELVSS_CMD_CNT;
		aid_dimming_dynamic.aid_cmd_cnt = S6E3HA2_AID_CMD_CNT;
		aid_dimming_dynamic.aid_reg_offset = S6E3HA2_AID_REG_OFFSET;
		aid_dimming_dynamic.tset_len = S6E3HA2_TSET_LEN;
		aid_dimming_dynamic.tset_reg = S6E3HA2_TSET_REG;
		aid_dimming_dynamic.tset_minus_offset = S6E3HA2_TSET_MINUS_OFFSET;
		aid_dimming_dynamic.vint_reg2 = S6E3HA2_VINT_REG2;
		break;
	}
	return 0;
}
early_param("lcdtype", get_lcd_type);


static int __init get_hw_rev(char *arg)
{
	get_option(&arg, &hw_rev);
	dsim_info("hw_rev : %d\n", hw_rev);

	return 0;
}

early_param("androidboot.hw_rev", get_hw_rev);



