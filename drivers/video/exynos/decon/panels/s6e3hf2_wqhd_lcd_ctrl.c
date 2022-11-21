/*
 * drivers/video/decon/panels/s6e3hf2_lcd_ctrl.c
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

#include <video/mipi_display.h>

#include "../dsim.h"

#include "panel_info.h"

unsigned int s6e3hf2_lcd_type = S6E3HF2_LCDTYPE_WQHD;

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "s6e3hf2_wqhd_aid_dimming.h"
#endif


#ifdef CONFIG_PANEL_AID_DIMMING
static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = {SEQ_HBM_OFF, SEQ_HBM_ON};
static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {S6E3HF2_SEQ_ACL_OFF, S6E3HF2_SEQ_ACL_ON};
static const unsigned char *ACL_OPR_TABLE[ACL_OPR_MAX] = {S6E3HF2_SEQ_ACL_OFF_OPR, S6E3HF2_SEQ_ACL_ON_OPR_8, S6E3HF2_SEQ_ACL_ON_OPR_15};

static const unsigned int br_tbl[EXTEND_BRIGHTNESS + 1] = {
	2, 2, 2, 3,	4, 5, 6, 7,	8,	9,	10,	11,	12,	13,	14,	15,		// 16
	16,	17,	18,	19,	20,	21,	22,	23,	25,	27,	29,	31,	33,	36,   	// 14
	39,	41,	41,	44,	44,	47,	47,	50,	50,	53,	53,	56,	56,	56,		// 14
	60,	60,	60,	64,	64,	64,	68,	68,	68,	72,	72,	72,	72,	77,		// 14
	77,	77,	82,	82,	82,	82,	87,	87,	87,	87,	93,	93,	93,	93,		// 14
	98,	98,	98,	98,	98,	105, 105, 105, 105,	111, 111, 111,		// 12
	111, 111, 111, 119, 119, 119, 119, 119, 126, 126, 126,		// 11
	126, 126, 126, 134, 134, 134, 134, 134,	134, 134, 143,
	143, 143, 143, 143, 143, 152, 152, 152, 152, 152, 152,
	152, 152, 162, 162,	162, 162, 162, 162,	162, 172, 172,
	172, 172, 172, 172,	172, 172, 183, 183, 183, 183, 183,
	183, 183, 183, 183, 195, 195, 195, 195, 195, 195, 195,
	195, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207,
	220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 234,
	234, 234, 234, 234,	234, 234, 234, 234,	234, 234, 249,
	249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249,
	265, 265, 265, 265, 265, 265, 265, 265, 265, 265, 265,
	265, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
	282, 282, 282, 300, 300, 300, 300, 300,	300, 300, 300,
	300, 300, 300, 300, 316, 316, 316, 316, 316, 316, 316,
	316, 316, 316, 316, 316, 333, 333, 333, 333, 333, 333,
	333, 333, 333, 333, 333, 333, 333,							//7
	[256 ... 262] = 333,
	[263 ... 278] = 360,
	[279 ... 296] = 382,
	[297 ... 315] = 407,
	[316 ... 336] = 433,
	[337 ... 357] = 461,
	[358 ... 376] = 490,
	[377 ... 397] = 516,
	[398 ... 438] = 544,
	[439 ... 439] = 600
};


static const unsigned gallery_br_tbl[EXTEND_BRIGHTNESS + 1] = {
	 2,  2,  2,  3,  5,  6,  8,  10,  11,  13,  15,  16,  18,  20,  21,  23,
	 25,  26,  28,  29,  32,  33,  34,  36,  38,  39,  41,  43,  44,  46,  48,  50,
	 51,  53,  55,  56,  57,  60,  61,  62,  64,  66,  68,  69,  71,  73,  74,  76,
	 78,  79,  81,  83,  84,  86,  88,  89,  91,  92,  94,  96,  97,  99,  101, 102,
	 104, 106, 107, 109, 111, 112, 114, 116, 117, 119, 120, 123, 124, 125, 127, 129,
	 130, 132, 134, 135, 137, 139, 141, 142, 144, 146, 147, 148, 151, 152, 153, 155,
	 157, 159, 160, 162, 164, 165, 167, 169, 170, 172, 174, 175, 177, 179, 180, 182,
	 183, 185, 187, 188, 190, 192, 193, 195, 197, 198, 200, 202, 203, 205, 207, 208,
	 210, 211, 214, 215, 216, 218, 220, 221, 223, 225, 226, 228, 230, 232, 233, 235,
	 237, 238, 239, 242, 243, 244, 246, 248, 250, 251, 253, 255, 256, 258, 260, 261,
	 263, 265, 266, 268, 270, 271, 273, 274, 276, 278, 279, 281, 283, 284, 286, 288,
	 289, 291, 293, 294, 296, 298, 299, 301, 302, 305, 306, 307, 309, 311, 312, 314,
	 316, 317, 319, 321, 323, 324, 326, 328, 329, 330, 333, 334, 335, 337, 339, 341,
	 342, 344, 346, 347, 349, 351, 352, 354, 356, 357, 359, 361, 362, 364, 365, 367,
	 369, 370, 372, 374, 375, 377, 379, 380, 382, 384, 385, 387, 389, 390, 392, 393,
	 396, 397, 398, 400, 402, 403, 405, 407, 408, 410, 412, 414, 415, 417, 419, 430,
	[256 ... 336] = 433,
	[337 ... 357] = 461,
	[358 ... 376] = 490,
	[377 ... 397] = 516,
	[398 ... 438] = 544,
	[439 ... 439] = 600
};

static const short center_gamma[NUM_VREF][CI_MAX] = {
	{0x000, 0x000, 0x000},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x100, 0x100, 0x100},
};

struct SmtDimInfo tulip_dimming_info[MAX_BR_INFO] = {				// add hbm array
	{.br = 2, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl2nit, .cTbl = ctbl2nit, .aid = aid9841, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 3, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl3nit, .cTbl = ctbl3nit, .aid = aid9779, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 4, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl4nit, .cTbl = ctbl4nit, .aid = aid9686, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 5, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl5nit, .cTbl = ctbl5nit, .aid = aid9639, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 6, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl6nit, .cTbl = ctbl6nit, .aid = aid9546, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 7, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl7nit, .cTbl = ctbl7nit, .aid = aid9464, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 8, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl8nit, .cTbl = ctbl8nit, .aid = aid9406, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 9, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl9nit, .cTbl = ctbl9nit, .aid = aid9309, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 10, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl10nit, .cTbl = ctbl10nit, .aid = aid9262, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 11, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl11nit, .cTbl = ctbl11nit, .aid = aid9181, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 12, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl12nit, .cTbl = ctbl12nit, .aid = aid9092, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 13, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl13nit, .cTbl = ctbl13nit, .aid = aid9030, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 14, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl14nit, .cTbl = ctbl14nit, .aid = aid8964, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 15, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl15nit, .cTbl = ctbl15nit, .aid = aid8886, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 16, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl16nit, .cTbl = ctbl16nit, .aid = aid8797, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 17, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl17nit, .cTbl = ctbl17nit, .aid = aid8719, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 19, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl19nit, .cTbl = ctbl19nit, .aid = aid8579, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 20, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl20nit, .cTbl = ctbl20nit, .aid = aid8482, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 21, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl21nit, .cTbl = ctbl21nit, .aid = aid8381, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 22, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl22nit, .cTbl = ctbl22nit, .aid = aid8342, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 24, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl24nit, .cTbl = ctbl24nit, .aid = aid8172, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 25, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl25nit, .cTbl = ctbl25nit, .aid = aid8075, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 27, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl27nit, .cTbl = ctbl27nit, .aid = aid7958, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 29, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl29nit, .cTbl = ctbl29nit, .aid = aid7795, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 30, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl30nit, .cTbl = ctbl30nit, .aid = aid7710, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 32, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl32nit, .cTbl = ctbl32nit, .aid = aid7539, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 34, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl34nit, .cTbl = ctbl34nit, .aid = aid7372, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 37, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl37nit, .cTbl = ctbl37nit, .aid = aid7139, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 39, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl39nit, .cTbl = ctbl39nit, .aid = aid7023, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 41, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl41nit, .cTbl = ctbl41nit, .aid = aid6828, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 44, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl44nit, .cTbl = ctbl44nit, .aid = aid6584, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 47, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl47nit, .cTbl = ctbl47nit, .aid = aid6351, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 50, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl50nit, .cTbl = ctbl50nit, .aid = aid6075, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 53, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl53nit, .cTbl = ctbl53nit, .aid = aid5881, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 56, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl56nit, .cTbl = ctbl56nit, .aid = aid5648, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 60, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl60nit, .cTbl = ctbl60nit, .aid = aid5299, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 64, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl64nit, .cTbl = ctbl64nit, .aid = aid4942, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 68, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl68nit, .cTbl = ctbl68nit, .aid = aid4620, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 72, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl72nit, .cTbl = ctbl72nit, .aid = aid4220, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 77, .refBr = 114, .cGma = gma2p15, .rTbl = rtbl77nit, .cTbl = ctbl77nit, .aid = aid3750, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 82, .refBr = 120, .cGma = gma2p15, .rTbl = rtbl82nit, .cTbl = ctbl82nit, .aid = aid3750, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 87, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl87nit, .cTbl = ctbl87nit, .aid = aid3750, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 93, .refBr = 137, .cGma = gma2p15, .rTbl = rtbl93nit, .cTbl = ctbl93nit, .aid = aid3750, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 98, .refBr = 143, .cGma = gma2p15, .rTbl = rtbl98nit, .cTbl = ctbl98nit, .aid = aid3750, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 105, .refBr = 153, .cGma = gma2p15, .rTbl = rtbl105nit, .cTbl = ctbl105nit, .aid = aid3750, .elvCaps = elv1, .elv = elv1, .way = W1},
	{.br = 110, .refBr = 162, .cGma = gma2p15, .rTbl = rtbl110nit, .cTbl = ctbl110nit, .aid = aid3750, .elvCaps = elv2, .elv = elv2, .way = W1},
	{.br = 119, .refBr = 171, .cGma = gma2p15, .rTbl = rtbl119nit, .cTbl = ctbl119nit, .aid = aid3750, .elvCaps = elv2, .elv = elv2, .way = W1},
	{.br = 126, .refBr = 179, .cGma = gma2p15, .rTbl = rtbl126nit, .cTbl = ctbl126nit, .aid = aid3750, .elvCaps = elv2, .elv = elv2, .way = W1},
	{.br = 134, .refBr = 190, .cGma = gma2p15, .rTbl = rtbl134nit, .cTbl = ctbl134nit, .aid = aid3750, .elvCaps = elv3, .elv = elv3, .way = W1},
	{.br = 143, .refBr = 202, .cGma = gma2p15, .rTbl = rtbl143nit, .cTbl = ctbl143nit, .aid = aid3750, .elvCaps = elv3, .elv = elv3, .way = W1},
	{.br = 152, .refBr = 213, .cGma = gma2p15, .rTbl = rtbl152nit, .cTbl = ctbl152nit, .aid = aid3750, .elvCaps = elv4, .elv = elv4, .way = W1},
	{.br = 162, .refBr = 228, .cGma = gma2p15, .rTbl = rtbl162nit, .cTbl = ctbl162nit, .aid = aid3750, .elvCaps = elv4, .elv = elv4, .way = W1},
	{.br = 172, .refBr = 238, .cGma = gma2p15, .rTbl = rtbl172nit, .cTbl = ctbl172nit, .aid = aid3750, .elvCaps = elv5, .elv = elv5, .way = W1},
	{.br = 183, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl183nit, .cTbl = ctbl183nit, .aid = aid3750, .elvCaps = elv6, .elv = elv6, .way = W1},
	{.br = 195, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl195nit, .cTbl = ctbl195nit, .aid = aid3269, .elvCaps = elv6, .elv = elv6, .way = W1},
	{.br = 207, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl207nit, .cTbl = ctbl207nit, .aid = aid2764, .elvCaps = elv6, .elv = elv6, .way = W1},
	{.br = 220, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl220nit, .cTbl = ctbl220nit, .aid = aid2220, .elvCaps = elv6, .elv = elv6, .way = W1},
	{.br = 234, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl234nit, .cTbl = ctbl234nit, .aid = aid1677, .elvCaps = elv6, .elv = elv6, .way = W1},
	{.br = 249, .refBr = 252, .cGma = gma2p15, .rTbl = rtbl249nit, .cTbl = ctbl249nit, .aid = aid1005, .elvCaps = elvCaps6, .elv = elvCaps6, .way = W1}, // 249 ~ 360 acl off
	{.br = 265, .refBr = 268, .cGma = gma2p15, .rTbl = rtbl265nit, .cTbl = ctbl265nit, .aid = aid1005, .elvCaps = elvCaps6, .elv = elvCaps6, .way = W1},
	{.br = 282, .refBr = 283, .cGma = gma2p15, .rTbl = rtbl282nit, .cTbl = ctbl282nit, .aid = aid1005, .elvCaps = elvCaps7, .elv = elvCaps7, .way = W1},
	{.br = 300, .refBr = 300, .cGma = gma2p15, .rTbl = rtbl300nit, .cTbl = ctbl300nit, .aid = aid1005, .elvCaps = elvCaps7, .elv = elvCaps7, .way = W1},
	{.br = 316, .refBr = 315, .cGma = gma2p15, .rTbl = rtbl316nit, .cTbl = ctbl316nit, .aid = aid1005, .elvCaps = elvCaps8, .elv = elvCaps8, .way = W1},
	{.br = 333, .refBr = 334, .cGma = gma2p15, .rTbl = rtbl333nit, .cTbl = ctbl333nit, .aid = aid1005, .elvCaps = elvCaps8, .elv = elvCaps8, .way = W1},
	{.br = 360, .refBr = 360, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid1005, .elvCaps = elvCaps8, .elv = elvCaps8, .way = W2},
/*hbm interpolation */
	{.br = 382, .refBr = 382, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid1005, .elvCaps = elv8, .elv = elv8, .way = W3},  // hbm is acl on
	{.br = 407, .refBr = 407, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid1005, .elvCaps = elv8, .elv = elv8, .way = W3},  // hbm is acl on
	{.br = 433, .refBr = 433, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid1005, .elvCaps = elv8, .elv = elv8, .way = W3},  // hbm is acl on
	{.br = 461, .refBr = 461, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid1005, .elvCaps = elv8, .elv = elv8, .way = W3},  // hbm is acl on
	{.br = 491, .refBr = 490, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid1005, .elvCaps = elv8, .elv = elv8, .way = W3},  // hbm is acl on
	{.br = 517, .refBr = 516, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid1005, .elvCaps = elv8, .elv = elv8, .way = W3},  // hbm is acl on
	{.br = 545, .refBr = 544, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid1005, .elvCaps = elv8, .elv = elv8, .way = W3},  // hbm is acl on
/* hbm */
	{.br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid1005, .elvCaps = elv8, .elv = elv8, .way = W4},  // hbm is acl on
};

// aid sheet in rev.C
struct SmtDimInfo daisy_dimming_info_RC[MAX_BR_INFO] = {				// add hbm array
	{.br = 2, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl2nit, .cTbl = RCdctbl2nit, .aid = aid9833, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 3, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl3nit, .cTbl = RCdctbl3nit, .aid = aid9736, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 4, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl4nit, .cTbl = RCdctbl4nit, .aid = aid9689, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 5, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl5nit, .cTbl = RCdctbl5nit, .aid = aid9592, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 6, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl6nit, .cTbl = RCdctbl6nit, .aid = aid9523, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 7, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl7nit, .cTbl = RCdctbl7nit, .aid = aid9453, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 8, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl8nit, .cTbl = RCdctbl8nit, .aid = aid9379, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 9, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl9nit, .cTbl = RCdctbl9nit, .aid = aid9278, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 10, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl10nit, .cTbl = RCdctbl10nit, .aid = aid9185, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 11, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl11nit, .cTbl = RCdctbl11nit, .aid = aid9142, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 12, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl12nit, .cTbl = RCdctbl12nit, .aid = aid9053, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 13, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl13nit, .cTbl = RCdctbl13nit, .aid = aid8967, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 14, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl14nit, .cTbl = RCdctbl14nit, .aid = aid8898, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 15, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl15nit, .cTbl = RCdctbl15nit, .aid = aid8832, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 16, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl16nit, .cTbl = RCdctbl16nit, .aid = aid8738, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 17, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl17nit, .cTbl = RCdctbl17nit, .aid = aid8657, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 19, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl19nit, .cTbl = RCdctbl19nit, .aid = aid8517, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 20, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl20nit, .cTbl = RCdctbl20nit, .aid = aid8432, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 21, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl21nit, .cTbl = RCdctbl21nit, .aid = aid8346, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 22, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl22nit, .cTbl = RCdctbl22nit, .aid = aid8276, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 24, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl24nit, .cTbl = RCdctbl24nit, .aid = aid8102, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 25, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl25nit, .cTbl = RCdctbl25nit, .aid = aid8032, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 27, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl27nit, .cTbl = RCdctbl27nit, .aid = aid7873, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 29, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl29nit, .cTbl = RCdctbl29nit, .aid = aid7717, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 30, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl30nit, .cTbl = RCdctbl30nit, .aid = aid7640, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 32, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl32nit, .cTbl = RCdctbl32nit, .aid = aid7469, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 34, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl34nit, .cTbl = RCdctbl34nit, .aid = aid7294, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 37, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl37nit, .cTbl = RCdctbl37nit, .aid = aid7050, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 39, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl39nit, .cTbl = RCdctbl39nit, .aid = aid6906, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 41, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl41nit, .cTbl = RCdctbl41nit, .aid = aid6727, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 44, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl44nit, .cTbl = RCdctbl44nit, .aid = aid6479, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 47, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl47nit, .cTbl = RCdctbl47nit, .aid = aid6231, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 50, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl50nit, .cTbl = RCdctbl50nit, .aid = aid5986, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 53, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl53nit, .cTbl = RCdctbl53nit, .aid = aid5734, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 56, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl56nit, .cTbl = RCdctbl56nit, .aid = aid5481, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 60, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl60nit, .cTbl = RCdctbl60nit, .aid = aid5148, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 64, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl64nit, .cTbl = RCdctbl64nit, .aid = aid4798, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 68, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl68nit, .cTbl = RCdctbl68nit, .aid = aid4472, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 72, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl72nit, .cTbl = RCdctbl72nit, .aid = aid4115, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 77, .refBr = 116, .cGma = gma2p15, .rTbl = RCdrtbl77nit, .cTbl = RCdctbl77nit, .aid = aid3680, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 82, .refBr = 124, .cGma = gma2p15, .rTbl = RCdrtbl82nit, .cTbl = RCdctbl82nit, .aid = aid3680, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 87, .refBr = 131, .cGma = gma2p15, .rTbl = RCdrtbl87nit, .cTbl = RCdctbl87nit, .aid = aid3680, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 93, .refBr = 140, .cGma = gma2p15, .rTbl = RCdrtbl93nit, .cTbl = RCdctbl93nit, .aid = aid3680, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 98, .refBr = 147, .cGma = gma2p15, .rTbl = RCdrtbl98nit, .cTbl = RCdctbl98nit, .aid = aid3680, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 105, .refBr = 157, .cGma = gma2p15, .rTbl = RCdrtbl105nit, .cTbl = RCdctbl105nit, .aid = aid3680, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 110, .refBr = 166, .cGma = gma2p15, .rTbl = RCdrtbl110nit, .cTbl = RCdctbl110nit, .aid = aid3680, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 119, .refBr = 175, .cGma = gma2p15, .rTbl = RCdrtbl119nit, .cTbl = RCdctbl119nit, .aid = aid3680, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 126, .refBr = 186, .cGma = gma2p15, .rTbl = RCdrtbl126nit, .cTbl = RCdctbl126nit, .aid = aid3680, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 134, .refBr = 196, .cGma = gma2p15, .rTbl = RCdrtbl134nit, .cTbl = RCdctbl134nit, .aid = aid3680, .elvCaps = delvCaps5, .elv = delv5, .way = W1},
	{.br = 143, .refBr = 208, .cGma = gma2p15, .rTbl = RCdrtbl143nit, .cTbl = RCdctbl143nit, .aid = aid3680, .elvCaps = delvCaps6, .elv = delv6, .way = W1},
	{.br = 152, .refBr = 220, .cGma = gma2p15, .rTbl = RCdrtbl152nit, .cTbl = RCdctbl152nit, .aid = aid3680, .elvCaps = delvCaps6, .elv = delv6, .way = W1},
	{.br = 162, .refBr = 232, .cGma = gma2p15, .rTbl = RCdrtbl162nit, .cTbl = RCdctbl162nit, .aid = aid3680, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 172, .refBr = 242, .cGma = gma2p15, .rTbl = RCdrtbl172nit, .cTbl = RCdctbl172nit, .aid = aid3680, .elvCaps = delvCaps8, .elv = delv8, .way = W1},
	{.br = 183, .refBr = 257, .cGma = gma2p15, .rTbl = RCdrtbl183nit, .cTbl = RCdctbl183nit, .aid = aid3680, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 195, .refBr = 257, .cGma = gma2p15, .rTbl = RCdrtbl195nit, .cTbl = RCdctbl195nit, .aid = aid3241, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 207, .refBr = 257, .cGma = gma2p15, .rTbl = RCdrtbl207nit, .cTbl = RCdctbl207nit, .aid = aid2752, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 220, .refBr = 257, .cGma = gma2p15, .rTbl = RCdrtbl220nit, .cTbl = RCdctbl220nit, .aid = aid2244, .elvCaps = delvCaps8, .elv = delv8, .way = W1},
	{.br = 234, .refBr = 257, .cGma = gma2p15, .rTbl = RCdrtbl234nit, .cTbl = RCdctbl234nit, .aid = aid1646, .elvCaps = delvCaps8, .elv = delv8, .way = W1},
	{.br = 249, .refBr = 257, .cGma = gma2p15, .rTbl = RCdrtbl249nit, .cTbl = RCdctbl249nit, .aid = aid1005, .elvCaps = delvCaps9, .elv = delv9, .way = W1},		// 249 ~ 360nit acl off
	{.br = 265, .refBr = 272, .cGma = gma2p15, .rTbl = RCdrtbl265nit, .cTbl = RCdctbl265nit, .aid = aid1005, .elvCaps = delvCaps9, .elv = delv9, .way = W1},
	{.br = 282, .refBr = 287, .cGma = gma2p15, .rTbl = RCdrtbl282nit, .cTbl = RCdctbl282nit, .aid = aid1005, .elvCaps = delvCaps10, .elv = delv10, .way = W1},
	{.br = 300, .refBr = 303, .cGma = gma2p15, .rTbl = RCdrtbl300nit, .cTbl = RCdctbl300nit, .aid = aid1005, .elvCaps = delvCaps10, .elv = delv10, .way = W1},
	{.br = 316, .refBr = 316, .cGma = gma2p15, .rTbl = RCdrtbl316nit, .cTbl = RCdctbl316nit, .aid = aid1005, .elvCaps = delvCaps11, .elv = delv11, .way = W1},
	{.br = 333, .refBr = 337, .cGma = gma2p15, .rTbl = RCdrtbl333nit, .cTbl = RCdctbl333nit, .aid = aid1005, .elvCaps = delvCaps11, .elv = delv11, .way = W1},
	{.br = 360, .refBr = 360, .cGma = gma2p20, .rTbl = RCdrtbl360nit, .cTbl = RCdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W2},
/*hbm interpolation */
	{.br = 382, .refBr = 382, .cGma = gma2p20, .rTbl = RCdrtbl360nit, .cTbl = RCdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 407, .refBr = 407, .cGma = gma2p20, .rTbl = RCdrtbl360nit, .cTbl = RCdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 433, .refBr = 433, .cGma = gma2p20, .rTbl = RCdrtbl360nit, .cTbl = RCdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 461, .refBr = 461, .cGma = gma2p20, .rTbl = RCdrtbl360nit, .cTbl = RCdctbl360nit, .aid = aid1005, .elvCaps = delvCaps13, .elv = delv13, .way = W3},  // hbm is acl on
	{.br = 491, .refBr = 490, .cGma = gma2p20, .rTbl = RCdrtbl360nit, .cTbl = RCdctbl360nit, .aid = aid1005, .elvCaps = delvCaps14, .elv = delv14, .way = W3},  // hbm is acl on
	{.br = 517, .refBr = 516, .cGma = gma2p20, .rTbl = RCdrtbl360nit, .cTbl = RCdctbl360nit, .aid = aid1005, .elvCaps = delvCaps15, .elv = delv15, .way = W3},  // hbm is acl on
	{.br = 545, .refBr = 544, .cGma = gma2p20, .rTbl = RCdrtbl360nit, .cTbl = RCdctbl360nit, .aid = aid1005, .elvCaps = delvCaps16, .elv = delv16, .way = W3},  // hbm is acl on
/* hbm */
	{.br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = RCdrtbl360nit, .cTbl = RCdctbl360nit, .aid = aid1005, .elvCaps = delvCaps17, .elv = delv17, .way = W4},
};


// aid sheet in rev.D
struct SmtDimInfo daisy_dimming_info_RD[MAX_BR_INFO] = {				// add hbm array
	{.br = 2, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl2nit, .cTbl = RDdctbl2nit, .aid = aid9798, .elvCaps = delvCaps5, .elv = delv5, .way = W1},
	{.br = 3, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl3nit, .cTbl = RDdctbl3nit, .aid = aid9720, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 4, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl4nit, .cTbl = RDdctbl4nit, .aid = aid9612, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 5, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl5nit, .cTbl = RDdctbl5nit, .aid = aid9530, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 6, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl6nit, .cTbl = RDdctbl6nit, .aid = aid9449, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 7, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl7nit, .cTbl = RDdctbl7nit, .aid = aid9359, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 8, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl8nit, .cTbl = RDdctbl8nit, .aid = aid9282, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 9, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl9nit, .cTbl = RDdctbl9nit, .aid = aid9208, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 10, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl10nit, .cTbl = RDdctbl10nit, .aid = aid9115, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 11, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl11nit, .cTbl = RDdctbl11nit, .aid = aid8998, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 12, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl12nit, .cTbl = RDdctbl12nit, .aid = aid8960, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 13, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl13nit, .cTbl = RDdctbl13nit, .aid = aid8894, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 14, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl14nit, .cTbl = RDdctbl14nit, .aid = aid8832, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 15, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl15nit, .cTbl = RDdctbl15nit, .aid = aid8723, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 16, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl16nit, .cTbl = RDdctbl16nit, .aid = aid8653, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 17, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl17nit, .cTbl = RDdctbl17nit, .aid = aid8575, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 19, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl19nit, .cTbl = RDdctbl19nit, .aid = aid8381, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 20, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl20nit, .cTbl = RDdctbl20nit, .aid = aid8292, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 21, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl21nit, .cTbl = RDdctbl21nit, .aid = aid8218, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 22, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl22nit, .cTbl = RDdctbl22nit, .aid = aid8148, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 24, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl24nit, .cTbl = RDdctbl24nit, .aid = aid7966, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 25, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl25nit, .cTbl = RDdctbl25nit, .aid = aid7865, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 27, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl27nit, .cTbl = RDdctbl27nit, .aid = aid7710, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 29, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl29nit, .cTbl = RDdctbl29nit, .aid = aid7512, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 30, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl30nit, .cTbl = RDdctbl30nit, .aid = aid7384, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 32, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl32nit, .cTbl = RDdctbl32nit, .aid = aid7267, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 34, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl34nit, .cTbl = RDdctbl34nit, .aid = aid7089, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 37, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl37nit, .cTbl = RDdctbl37nit, .aid = aid6813, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 39, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl39nit, .cTbl = RDdctbl39nit, .aid = aid6658, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 41, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl41nit, .cTbl = RDdctbl41nit, .aid = aid6467, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 44, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl44nit, .cTbl = RDdctbl44nit, .aid = aid6188, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 47, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl47nit, .cTbl = RDdctbl47nit, .aid = aid5920, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 50, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl50nit, .cTbl = RDdctbl50nit, .aid = aid5637, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 53, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl53nit, .cTbl = RDdctbl53nit, .aid = aid5365, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 56, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl56nit, .cTbl = RDdctbl56nit, .aid = aid5085, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 60, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl60nit, .cTbl = RDdctbl60nit, .aid = aid4732, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 64, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl64nit, .cTbl = RDdctbl64nit, .aid = aid4344, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 68, .refBr = 113, .cGma = gma2p15, .rTbl = RDdrtbl68nit, .cTbl = RDdctbl68nit, .aid = aid3956, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 72, .refBr = 112, .cGma = gma2p15, .rTbl = RDdrtbl72nit, .cTbl = RDdctbl72nit, .aid = aid3637, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 77, .refBr = 120, .cGma = gma2p15, .rTbl = RDdrtbl77nit, .cTbl = RDdctbl77nit, .aid = aid3637, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 82, .refBr = 127, .cGma = gma2p15, .rTbl = RDdrtbl82nit, .cTbl = RDdctbl82nit, .aid = aid3637, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 87, .refBr = 132, .cGma = gma2p15, .rTbl = RDdrtbl87nit, .cTbl = RDdctbl87nit, .aid = aid3637, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 93, .refBr = 142, .cGma = gma2p15, .rTbl = RDdrtbl93nit, .cTbl = RDdctbl93nit, .aid = aid3637, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 98, .refBr = 148, .cGma = gma2p15, .rTbl = RDdrtbl98nit, .cTbl = RDdctbl98nit, .aid = aid3637, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 105, .refBr = 158, .cGma = gma2p15, .rTbl = RDdrtbl105nit, .cTbl = RDdctbl105nit, .aid = aid3637, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 110, .refBr = 165, .cGma = gma2p15, .rTbl = RDdrtbl110nit, .cTbl = RDdctbl110nit, .aid = aid3637, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 119, .refBr = 176, .cGma = gma2p15, .rTbl = RDdrtbl119nit, .cTbl = RDdctbl119nit, .aid = aid3637, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 126, .refBr = 186, .cGma = gma2p15, .rTbl = RDdrtbl126nit, .cTbl = RDdctbl126nit, .aid = aid3637, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 134, .refBr = 196, .cGma = gma2p15, .rTbl = RDdrtbl134nit, .cTbl = RDdctbl134nit, .aid = aid3637, .elvCaps = delvCaps5, .elv = delv5, .way = W1},
	{.br = 143, .refBr = 205, .cGma = gma2p15, .rTbl = RDdrtbl143nit, .cTbl = RDdctbl143nit, .aid = aid3637, .elvCaps = delvCaps6, .elv = delv6, .way = W1},
	{.br = 152, .refBr = 217, .cGma = gma2p15, .rTbl = RDdrtbl152nit, .cTbl = RDdctbl152nit, .aid = aid3637, .elvCaps = delvCaps6, .elv = delv6, .way = W1},
	{.br = 162, .refBr = 228, .cGma = gma2p15, .rTbl = RDdrtbl162nit, .cTbl = RDdctbl162nit, .aid = aid3637, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 172, .refBr = 243, .cGma = gma2p15, .rTbl = RDdrtbl172nit, .cTbl = RDdctbl172nit, .aid = aid3637, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 183, .refBr = 257, .cGma = gma2p15, .rTbl = RCdrtbl183nit, .cTbl = RCdctbl183nit, .aid = aid3680, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 195, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl195nit, .cTbl = RDdctbl195nit, .aid = aid3168, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 207, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl207nit, .cTbl = RDdctbl207nit, .aid = aid2659, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 220, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl220nit, .cTbl = RDdctbl220nit, .aid = aid2186, .elvCaps = delvCaps8, .elv = delv8, .way = W1},
	{.br = 234, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl234nit, .cTbl = RDdctbl234nit, .aid = aid1580, .elvCaps = delvCaps8, .elv = delv8, .way = W1},
	{.br = 249, .refBr = 257, .cGma = gma2p15, .rTbl = RDdrtbl249nit, .cTbl = RDdctbl249nit, .aid = aid1005, .elvCaps = delvCaps9, .elv = delv9, .way = W1},		// 249 ~ 360nit acl off
	{.br = 265, .refBr = 274, .cGma = gma2p15, .rTbl = RDdrtbl265nit, .cTbl = RDdctbl265nit, .aid = aid1005, .elvCaps = delvCaps9, .elv = delv9, .way = W1},
	{.br = 282, .refBr = 286, .cGma = gma2p15, .rTbl = RDdrtbl282nit, .cTbl = RDdctbl282nit, .aid = aid1005, .elvCaps = delvCaps10, .elv = delv10, .way = W1},
	{.br = 300, .refBr = 307, .cGma = gma2p15, .rTbl = RDdrtbl300nit, .cTbl = RDdctbl300nit, .aid = aid1005, .elvCaps = delvCaps10, .elv = delv10, .way = W1},
	{.br = 316, .refBr = 320, .cGma = gma2p15, .rTbl = RDdrtbl316nit, .cTbl = RDdctbl316nit, .aid = aid1005, .elvCaps = delvCaps11, .elv = delv11, .way = W1},
	{.br = 333, .refBr = 339, .cGma = gma2p15, .rTbl = RDdrtbl333nit, .cTbl = RDdctbl333nit, .aid = aid1005, .elvCaps = delvCaps11, .elv = delv11, .way = W1},
	{.br = 360, .refBr = 360, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W2},
/*hbm interpolation */
	{.br = 382, .refBr = 382, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 407, .refBr = 407, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 433, .refBr = 433, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 461, .refBr = 461, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps13, .elv = delv13, .way = W3},  // hbm is acl on
	{.br = 491, .refBr = 490, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps14, .elv = delv14, .way = W3},  // hbm is acl on
	{.br = 517, .refBr = 516, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps15, .elv = delv15, .way = W3},  // hbm is acl on
	{.br = 545, .refBr = 544, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps16, .elv = delv16, .way = W3},  // hbm is acl on
/* hbm */
	{.br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = RDdrtbl360nit, .cTbl = RDdctbl360nit, .aid = aid1005, .elvCaps = delvCaps17, .elv = delv17, .way = W4},
};

struct SmtDimInfo a3_daisy_dimming_info[MAX_BR_INFO + 1] = {				// add hbm array
	{.br = 2, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl2nit, .cTbl = A3ctbl2nit, .aid = aid9821, .elvCaps = delvCaps5, .elv = delv5, .way = W1},
	{.br = 3, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl3nit, .cTbl = A3ctbl3nit, .aid = aid9717, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 4, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl4nit, .cTbl = A3ctbl4nit, .aid = aid9639, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 5, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl5nit, .cTbl = A3ctbl5nit, .aid = aid9526, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 6, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl6nit, .cTbl = A3ctbl6nit, .aid = aid9449, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 7, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl7nit, .cTbl = A3ctbl7nit, .aid = aid9344, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 8, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl8nit, .cTbl = A3ctbl8nit, .aid = aid9270, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 9, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl9nit, .cTbl = A3ctbl9nit, .aid = aid9200, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 10, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl10nit, .cTbl = A3ctbl10nit, .aid = aid9095, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 11, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl11nit, .cTbl = A3ctbl11nit, .aid = aid9026, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 12, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl12nit, .cTbl = A3ctbl12nit, .aid = aid8956, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 13, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl13nit, .cTbl = A3ctbl13nit, .aid = aid8886, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 14, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl14nit, .cTbl = A3ctbl14nit, .aid = aid8793, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 15, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl15nit, .cTbl = A3ctbl15nit, .aid = aid8715, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 16, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl16nit, .cTbl = A3ctbl16nit, .aid = aid8622, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 17, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl17nit, .cTbl = A3ctbl17nit, .aid = aid8536, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 19, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl19nit, .cTbl = A3ctbl19nit, .aid = aid8373, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 20, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl20nit, .cTbl = A3ctbl20nit, .aid = aid8280, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 21, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl21nit, .cTbl = A3ctbl21nit, .aid = aid8207, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 22, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl22nit, .cTbl = A3ctbl22nit, .aid = aid8102, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 24, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl24nit, .cTbl = A3ctbl24nit, .aid = aid7923, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 25, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl25nit, .cTbl = A3ctbl25nit, .aid = aid7842, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 27, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl27nit, .cTbl = A3ctbl27nit, .aid = aid7667, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 29, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl29nit, .cTbl = A3ctbl29nit, .aid = aid7488, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 30, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl30nit, .cTbl = A3ctbl30nit, .aid = aid7411, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 32, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl32nit, .cTbl = A3ctbl32nit, .aid = aid7224, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 34, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl34nit, .cTbl = A3ctbl34nit, .aid = aid7046, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 37, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl37nit, .cTbl = A3ctbl37nit, .aid = aid6786, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 39, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl39nit, .cTbl = A3ctbl39nit, .aid = aid6603, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 41, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl41nit, .cTbl = A3ctbl41nit, .aid = aid6413, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 44, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl44nit, .cTbl = A3ctbl44nit, .aid = aid6153, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 47, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl47nit, .cTbl = A3ctbl47nit, .aid = aid5862, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 50, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl50nit, .cTbl = A3ctbl50nit, .aid = aid5598, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 53, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl53nit, .cTbl = A3ctbl53nit, .aid = aid5303, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 56, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl56nit, .cTbl = A3ctbl56nit, .aid = aid5004, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 60, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl60nit, .cTbl = A3ctbl60nit, .aid = aid4655, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 64, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl64nit, .cTbl = A3ctbl64nit, .aid = aid4266, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 68, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl68nit, .cTbl = A3ctbl68nit, .aid = aid3874, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 72, .refBr = 112, .cGma = gma2p15, .rTbl = A3rtbl72nit, .cTbl = A3ctbl72nit, .aid = aid3587, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 77, .refBr = 121, .cGma = gma2p15, .rTbl = A3rtbl77nit, .cTbl = A3ctbl77nit, .aid = aid3587, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 82, .refBr = 129, .cGma = gma2p15, .rTbl = A3rtbl82nit, .cTbl = A3ctbl82nit, .aid = aid3587, .elvCaps = delvCaps1, .elv = delv1, .way = W1},
	{.br = 87, .refBr = 134, .cGma = gma2p15, .rTbl = A3rtbl87nit, .cTbl = A3ctbl87nit, .aid = aid3587, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 93, .refBr = 144, .cGma = gma2p15, .rTbl = A3rtbl93nit, .cTbl = A3ctbl93nit, .aid = aid3587, .elvCaps = delvCaps2, .elv = delv2, .way = W1},
	{.br = 98, .refBr = 150, .cGma = gma2p15, .rTbl = A3rtbl98nit, .cTbl = A3ctbl98nit, .aid = aid3587, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 105, .refBr = 162, .cGma = gma2p15, .rTbl = A3rtbl105nit, .cTbl = A3ctbl105nit, .aid = aid3587, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 110, .refBr = 169, .cGma = gma2p15, .rTbl = A3rtbl110nit, .cTbl = A3ctbl110nit, .aid = aid3587, .elvCaps = delvCaps3, .elv = delv3, .way = W1},
	{.br = 119, .refBr = 178, .cGma = gma2p15, .rTbl = A3rtbl119nit, .cTbl = A3ctbl119nit, .aid = aid3587, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 126, .refBr = 188, .cGma = gma2p15, .rTbl = A3rtbl126nit, .cTbl = A3ctbl126nit, .aid = aid3587, .elvCaps = delvCaps4, .elv = delv4, .way = W1},
	{.br = 134, .refBr = 198, .cGma = gma2p15, .rTbl = A3rtbl134nit, .cTbl = A3ctbl134nit, .aid = aid3587, .elvCaps = delvCaps5, .elv = delv5, .way = W1},
	{.br = 143, .refBr = 209, .cGma = gma2p15, .rTbl = A3rtbl143nit, .cTbl = A3ctbl143nit, .aid = aid3587, .elvCaps = delvCaps6, .elv = delv6, .way = W1},
	{.br = 152, .refBr = 220, .cGma = gma2p15, .rTbl = A3rtbl152nit, .cTbl = A3ctbl152nit, .aid = aid3587, .elvCaps = delvCaps6, .elv = delv6, .way = W1},
	{.br = 162, .refBr = 232, .cGma = gma2p15, .rTbl = A3rtbl162nit, .cTbl = A3ctbl162nit, .aid = aid3587, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 172, .refBr = 245, .cGma = gma2p15, .rTbl = A3rtbl172nit, .cTbl = A3ctbl172nit, .aid = aid3587, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 183, .refBr = 259, .cGma = gma2p15, .rTbl = A3rtbl183nit, .cTbl = A3ctbl183nit, .aid = aid3587, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 195, .refBr = 259, .cGma = gma2p15, .rTbl = A3rtbl195nit, .cTbl = A3ctbl195nit, .aid = aid3078, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 207, .refBr = 259, .cGma = gma2p15, .rTbl = A3rtbl207nit, .cTbl = A3ctbl207nit, .aid = aid2613, .elvCaps = delvCaps7, .elv = delv7, .way = W1},
	{.br = 220, .refBr = 259, .cGma = gma2p15, .rTbl = A3rtbl220nit, .cTbl = A3ctbl220nit, .aid = aid2108, .elvCaps = delvCaps8, .elv = delv8, .way = W1},
	{.br = 234, .refBr = 259, .cGma = gma2p15, .rTbl = A3rtbl234nit, .cTbl = A3ctbl234nit, .aid = aid1553, .elvCaps = delvCaps8, .elv = delv8, .way = W1},
	{.br = 249, .refBr = 259, .cGma = gma2p15, .rTbl = A3rtbl249nit, .cTbl = A3ctbl249nit, .aid = aid1005, .elvCaps = delvCaps9, .elv = delv9, .way = W1},		// 249 ~ 360nit acl off
	{.br = 265, .refBr = 275, .cGma = gma2p15, .rTbl = A3rtbl265nit, .cTbl = A3ctbl265nit, .aid = aid1005, .elvCaps = delvCaps9, .elv = delv9, .way = W1},
	{.br = 282, .refBr = 289, .cGma = gma2p15, .rTbl = A3rtbl282nit, .cTbl = A3ctbl282nit, .aid = aid1005, .elvCaps = delvCaps10, .elv = delv10, .way = W1},
	{.br = 300, .refBr = 308, .cGma = gma2p15, .rTbl = A3rtbl300nit, .cTbl = A3ctbl300nit, .aid = aid1005, .elvCaps = delvCaps10, .elv = delv10, .way = W1},
	{.br = 316, .refBr = 321, .cGma = gma2p15, .rTbl = A3rtbl316nit, .cTbl = A3ctbl316nit, .aid = aid1005, .elvCaps = delvCaps11, .elv = delv11, .way = W1},
	{.br = 333, .refBr = 338, .cGma = gma2p15, .rTbl = A3rtbl333nit, .cTbl = A3ctbl333nit, .aid = aid1005, .elvCaps = delvCaps11, .elv = delv11, .way = W1},
	{.br = 360, .refBr = 360, .cGma = gma2p20, .rTbl = A3rtbl360nit, .cTbl = A3ctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W2},
/*hbm interpolation */
	{.br = 382, .refBr = 382, .cGma = gma2p20, .rTbl = A3rtbl360nit, .cTbl = A3ctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 407, .refBr = 407, .cGma = gma2p20, .rTbl = A3rtbl360nit, .cTbl = A3ctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 433, .refBr = 433, .cGma = gma2p20, .rTbl = A3rtbl360nit, .cTbl = A3ctbl360nit, .aid = aid1005, .elvCaps = delvCaps12, .elv = delv12, .way = W3},  // hbm is acl on
	{.br = 461, .refBr = 461, .cGma = gma2p20, .rTbl = A3rtbl360nit, .cTbl = A3ctbl360nit, .aid = aid1005, .elvCaps = delvCaps13, .elv = delv13, .way = W3},  // hbm is acl on
	{.br = 491, .refBr = 490, .cGma = gma2p20, .rTbl = A3rtbl360nit, .cTbl = A3ctbl360nit, .aid = aid1005, .elvCaps = delvCaps14, .elv = delv14, .way = W3},  // hbm is acl on
	{.br = 517, .refBr = 516, .cGma = gma2p20, .rTbl = A3rtbl360nit, .cTbl = A3ctbl360nit, .aid = aid1005, .elvCaps = delvCaps15, .elv = delv15, .way = W3},  // hbm is acl on
	{.br = 545, .refBr = 544, .cGma = gma2p20, .rTbl = A3rtbl360nit, .cTbl = A3ctbl360nit, .aid = aid1005, .elvCaps = delvCaps16, .elv = delv16, .way = W3},  // hbm is acl on
/* hbm */
	{.br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = A3rtbl360nit, .cTbl = A3ctbl360nit, .aid = aid1005, .elvCaps = delvCaps17, .elv = delv17, .way = W4},
};

static int set_gamma_to_center(struct SmtDimInfo *brInfo)
{
	int i, j;
	int ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;

	result[index++] = OLED_CMD_GAMMA;

	for (i = V255; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] = (unsigned char)((center_gamma[i][j] >> 8) & 0x01);
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			} else {
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			}
		}
	}
	result[index++] = 0x00;
	result[index++] = 0x00;

	return ret;
}

static int gammaToVolt255(int gamma)
{
	int ret;

	if (gamma > vreg_element_max[V255]) {
		dsim_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V255];
	}
	if (gamma < 0) {
		dsim_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	ret = (int)v255_trans_volt[gamma];

	return ret;
}

static int voltToGamma(int hbm_volt_table[][3], int* vt, int tp, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	if(tp == V3)
	{
		t1 = DOUBLE_MULTIPLE_VREGOUT - hbm_volt_table[V3][color];
		t2 = DOUBLE_MULTIPLE_VREGOUT - hbm_volt_table[V11][color];
	}
	else
	{
		t1 = vt[color] - hbm_volt_table[tp][color];
		t2 = vt[color] - hbm_volt_table[tp + 1][color];
	}

	temp = ((unsigned long)t1 * (unsigned long)fix_const[tp].de) / (unsigned long)t2;
	ret = temp - fix_const[tp].nu;

	return ret;

}

static int set_gamma_to_hbm(struct SmtDimInfo *brInfo, struct dim_data *dimData, u8 *hbm)
{
	int ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;
	int i, j, idx;
	int temp = 0;
	int voltTableHbm[NUM_VREF][CI_MAX];

	memset(result, 0, OLED_CMD_GAMMA_CNT);

	result[index++] = OLED_CMD_GAMMA;

	memcpy(result+1, hbm, S6E3HF2_HBMGAMMA_LEN);


	memcpy(voltTableHbm[V0], dimData->volt[0], sizeof(voltTableHbm[V0]));
	memcpy(voltTableHbm[V3], dimData->volt[1], sizeof(voltTableHbm[V3]));
	memcpy(voltTableHbm[V11], dimData->volt[10], sizeof(voltTableHbm[V11]));
	memcpy(voltTableHbm[V23], dimData->volt[26], sizeof(voltTableHbm[V23]));
	memcpy(voltTableHbm[V35], dimData->volt[40], sizeof(voltTableHbm[V35]));
	memcpy(voltTableHbm[V51], dimData->volt[60], sizeof(voltTableHbm[V51]));
	memcpy(voltTableHbm[V87], dimData->volt[105], sizeof(voltTableHbm[V87]));
	memcpy(voltTableHbm[V151], dimData->volt[182], sizeof(voltTableHbm[V151]));
	memcpy(voltTableHbm[V203], dimData->volt[238], sizeof(voltTableHbm[V203]));


	idx = 0;
	for(i = CI_RED; i < CI_MAX; i++, idx += 2) {
		temp = (hbm[idx] << 8) | (hbm[idx + 1]);
		voltTableHbm[V255][i] = gammaToVolt255(temp + dimData->mtp[V255][i]);
	}

	idx = 1;
	for (i = V255; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				idx += 2;
			} else if(i == V0) {
				idx++;
			} else {
				temp = voltToGamma(voltTableHbm, dimData->volt_vt, i, j) - dimData->mtp[i][j];
				if(temp <= 0)
					temp = 0;
				result[idx] = temp;
				idx ++;
			}
		}
	}

	dsim_info("============ TUNE HBM GAMMA ========== : \n");
	for (i= 0; i < S6E3HF2_HBMGAMMA_LEN; i ++) {
		dsim_info("HBM GAMMA[%d] : %x\n", i, result[i]);
	}

	return ret;
}

/* gamma interpolaion table */
const unsigned int tbl_hbm_inter[7] = {
	94, 201, 311, 431, 559, 670, 789
};


static int interpolation_gamma_to_hbm(struct SmtDimInfo *dimInfo, int br_idx)
{
	int i, j;
	int ret = 0;
	int idx = 0;
	int tmp = 0;
	int hbmcnt, refcnt, gap = 0;
	int ref_idx = 0;
	int hbm_idx = 0;
	int rst = 0;
	int hbm_tmp, ref_tmp;
	unsigned char *result = dimInfo[br_idx].gamma;

	for (i = 0; i < MAX_BR_INFO; i++) {
		if (dimInfo[i].br == S6E3HF2_MAX_BRIGHTNESS)
			ref_idx = i;

		if (dimInfo[i].br == S6E3HF2_HBM_BRIGHTNESS)
			hbm_idx = i;
	}

	if ((ref_idx == 0) || (hbm_idx == 0)) {
		dsim_info("%s failed to get index ref index : %d, hbm index : %d\n",
					__func__, ref_idx, hbm_idx);
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
				hbm_tmp = (dimInfo[hbm_idx].gamma[hbmcnt] << 8) | (dimInfo[hbm_idx].gamma[hbmcnt+1]);
				ref_tmp = (dimInfo[ref_idx].gamma[refcnt] << 8) | (dimInfo[ref_idx].gamma[refcnt+1]);

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
			} else {
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

static int init_dimming(struct dsim_device *dsim, u8 *mtp, u8 *hbm)
{
	int i, j;
	int pos = 0;
	int ret = 0;
	short temp;
	int method = 0;
	unsigned char panelrev, paneltype, panelline;
	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *diminfo = NULL;
	int string_offset;
	char string_buf[1024];

	dimming = (struct dim_data *)kmalloc(sizeof(struct dim_data), GFP_KERNEL);
	if (!dimming) {
		dsim_err("failed to allocate memory for dim data\n");
		ret = -ENOMEM;
		goto error;
	}

	panelline = panel->id[0] & 0xc0;
	paneltype = panel->id[1] & 0x01;
	panelrev = panel->id[2] & 0x0F;
	dsim_info("%s : Panel Line : %x Panel type : %d Panel rev : %d\n",
			__func__, panelline, paneltype, panelrev);

	if (panelline == S6E3HF2_A3_LINE_ID) {
		dsim_info("%s init dimming info for A3 Line Daisy rev.D, E panel\n", __func__);
		diminfo = (void *)a3_daisy_dimming_info;
	} else {
		if (paneltype == 1) {
			switch(panelrev) {
			case 0: 			// rev.A
			case 1: 			// rev.B
			case 2: 			// rev.C
				dsim_info("%s init dimming info for A2 Line Daisy rev.A,B,C panel\n", __func__);
				diminfo = (void *)daisy_dimming_info_RC;
				break;
			default:			// rev.D ~
				dsim_info("%s init dimming info for A2 Line Daisy rev.D, E panel\n", __func__);
				diminfo = (void *)daisy_dimming_info_RD;
				break;
			}
		} else {
			dsim_info("%s init dimming info for A2 Line tulip panel\n", __func__);
			diminfo = tulip_dimming_info;
		}

	}

	panel->dim_data= (void *)dimming;
	panel->dim_info = (void *)diminfo;

	panel->br_tbl = (unsigned int *)br_tbl;
	panel->gallery_br_tbl = (unsigned int *)gallery_br_tbl;
	panel->hbm_tbl = (unsigned char **)HBM_TABLE;
	panel->acl_cutoff_tbl = (unsigned char **)ACL_CUTOFF_TABLE;
	panel->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos+1];
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
	temp = (mtp[pos+1]) << 8 | mtp[pos];

	for(i=0;i<CI_MAX;i++)
		dimming->vt_mtp[i] = (temp >> (i*4)) &0x0f;
#ifdef SMART_DIMMING_DEBUG
	dimm_info("Center Gamma Info : \n");
	for(i=0;i<VMAX;i++) {
		dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE],
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE] );
	}
#endif
	dimm_info("VT MTP : \n");
	dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE],
			dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE] );

	dimm_info("MTP Info : \n");
	for(i=0;i<VMAX;i++) {
		dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE],
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE] );
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
			ret = set_gamma_to_hbm(&diminfo[i], dimming, hbm);
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
		string_offset = sprintf(string_buf, "gamma[%3d] : ",diminfo[i].br);

		for(j = 0; j < GAMMA_CMD_CNT; j++)
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

struct SmtDimInfo a2_hmt_dimming_info[HMT_MAX_BR_INFO] = {
	{.br = 10, .refBr = 52, .cGma = gma2p15, .rTbl = HMTrtbl10nit, .cTbl = HMTctbl10nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 11, .refBr = 57, .cGma = gma2p15, .rTbl = HMTrtbl11nit, .cTbl = HMTctbl11nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 12, .refBr = 61, .cGma = gma2p15, .rTbl = HMTrtbl12nit, .cTbl = HMTctbl12nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 13, .refBr = 66, .cGma = gma2p15, .rTbl = HMTrtbl13nit, .cTbl = HMTctbl13nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 14, .refBr = 70, .cGma = gma2p15, .rTbl = HMTrtbl14nit, .cTbl = HMTctbl14nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 15, .refBr = 74, .cGma = gma2p15, .rTbl = HMTrtbl15nit, .cTbl = HMTctbl15nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 16, .refBr = 79, .cGma = gma2p15, .rTbl = HMTrtbl16nit, .cTbl = HMTctbl16nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 17, .refBr = 82, .cGma = gma2p15, .rTbl = HMTrtbl17nit, .cTbl = HMTctbl17nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 19, .refBr = 91, .cGma = gma2p15, .rTbl = HMTrtbl19nit, .cTbl = HMTctbl19nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 20, .refBr = 94, .cGma = gma2p15, .rTbl = HMTrtbl20nit, .cTbl = HMTctbl20nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 21, .refBr = 98, .cGma = gma2p15, .rTbl = HMTrtbl21nit, .cTbl = HMTctbl21nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 22, .refBr = 101, .cGma = gma2p15, .rTbl = HMTrtbl22nit, .cTbl = HMTctbl22nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 23, .refBr = 106, .cGma = gma2p15, .rTbl = HMTrtbl23nit, .cTbl = HMTctbl23nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 25, .refBr = 112, .cGma = gma2p15, .rTbl = HMTrtbl25nit, .cTbl = HMTctbl25nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 27, .refBr = 121, .cGma = gma2p15, .rTbl = HMTrtbl27nit, .cTbl = HMTctbl27nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 29, .refBr = 127, .cGma = gma2p15, .rTbl = HMTrtbl29nit, .cTbl = HMTctbl29nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 31, .refBr = 136, .cGma = gma2p15, .rTbl = HMTrtbl31nit, .cTbl = HMTctbl31nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 33, .refBr = 143, .cGma = gma2p15, .rTbl = HMTrtbl33nit, .cTbl = HMTctbl33nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 35, .refBr = 151, .cGma = gma2p15, .rTbl = HMTrtbl35nit, .cTbl = HMTctbl35nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 37, .refBr = 160, .cGma = gma2p15, .rTbl = HMTrtbl37nit, .cTbl = HMTctbl37nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 39, .refBr = 167, .cGma = gma2p15, .rTbl = HMTrtbl39nit, .cTbl = HMTctbl39nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 41, .refBr = 174, .cGma = gma2p15, .rTbl = HMTrtbl41nit, .cTbl = HMTctbl41nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 44, .refBr = 185, .cGma = gma2p15, .rTbl = HMTrtbl44nit, .cTbl = HMTctbl44nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 47, .refBr = 195, .cGma = gma2p15, .rTbl = HMTrtbl47nit, .cTbl = HMTctbl47nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 50, .refBr = 206, .cGma = gma2p15, .rTbl = HMTrtbl50nit, .cTbl = HMTctbl50nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 53, .refBr = 217, .cGma = gma2p15, .rTbl = HMTrtbl53nit, .cTbl = HMTctbl53nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 56, .refBr = 227, .cGma = gma2p15, .rTbl = HMTrtbl56nit, .cTbl = HMTctbl56nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 60, .refBr = 239, .cGma = gma2p15, .rTbl = HMTrtbl60nit, .cTbl = HMTctbl60nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 64, .refBr = 254, .cGma = gma2p15, .rTbl = HMTrtbl64nit, .cTbl = HMTctbl64nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 68, .refBr = 267, .cGma = gma2p15, .rTbl = HMTrtbl68nit, .cTbl = HMTctbl68nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 72, .refBr = 281, .cGma = gma2p15, .rTbl = HMTrtbl72nit, .cTbl = HMTctbl72nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 77, .refBr = 216, .cGma = gma2p15, .rTbl = HMTrtbl77nit, .cTbl = HMTctbl77nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 82, .refBr = 225, .cGma = gma2p15, .rTbl = HMTrtbl82nit, .cTbl = HMTctbl82nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 87, .refBr = 237, .cGma = gma2p15, .rTbl = HMTrtbl87nit, .cTbl = HMTctbl87nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 93, .refBr = 249, .cGma = gma2p15, .rTbl = HMTrtbl93nit, .cTbl = HMTctbl93nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 99, .refBr = 265, .cGma = gma2p15, .rTbl = HMTrtbl99nit, .cTbl = HMTctbl99nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 105, .refBr = 278, .cGma = gma2p15, .rTbl = HMTrtbl105nit, .cTbl = HMTctbl105nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
};

struct SmtDimInfo a3_hmt_dimming_info[HMT_MAX_BR_INFO] = {
	{.br = 10, .refBr = 41, .cGma = gma2p15, .rTbl = A3HMTrtbl10nit, .cTbl = A3HMTctbl10nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 11, .refBr = 45, .cGma = gma2p15, .rTbl = A3HMTrtbl11nit, .cTbl = A3HMTctbl11nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 12, .refBr = 49, .cGma = gma2p15, .rTbl = A3HMTrtbl12nit, .cTbl = A3HMTctbl12nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 13, .refBr = 53, .cGma = gma2p15, .rTbl = A3HMTrtbl13nit, .cTbl = A3HMTctbl13nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 14, .refBr = 58, .cGma = gma2p15, .rTbl = A3HMTrtbl14nit, .cTbl = A3HMTctbl14nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 15, .refBr = 60, .cGma = gma2p15, .rTbl = A3HMTrtbl15nit, .cTbl = A3HMTctbl15nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 16, .refBr = 65, .cGma = gma2p15, .rTbl = A3HMTrtbl16nit, .cTbl = A3HMTctbl16nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 17, .refBr = 70, .cGma = gma2p15, .rTbl = A3HMTrtbl17nit, .cTbl = A3HMTctbl17nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 19, .refBr = 79, .cGma = gma2p15, .rTbl = A3HMTrtbl19nit, .cTbl = A3HMTctbl19nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 20, .refBr = 82, .cGma = gma2p15, .rTbl = A3HMTrtbl20nit, .cTbl = A3HMTctbl20nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 21, .refBr = 87, .cGma = gma2p15, .rTbl = A3HMTrtbl21nit, .cTbl = A3HMTctbl21nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 22, .refBr = 91, .cGma = gma2p15, .rTbl = A3HMTrtbl22nit, .cTbl = A3HMTctbl22nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 23, .refBr = 93, .cGma = gma2p15, .rTbl = A3HMTrtbl23nit, .cTbl = A3HMTctbl23nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 25, .refBr = 99, .cGma = gma2p15, .rTbl = A3HMTrtbl25nit, .cTbl = A3HMTctbl25nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 27, .refBr = 107, .cGma = gma2p15, .rTbl = A3HMTrtbl27nit, .cTbl = A3HMTctbl27nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 29, .refBr = 113, .cGma = gma2p15, .rTbl = A3HMTrtbl29nit, .cTbl = A3HMTctbl29nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 31, .refBr = 121, .cGma = gma2p15, .rTbl = A3HMTrtbl31nit, .cTbl = A3HMTctbl31nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 33, .refBr = 130, .cGma = gma2p15, .rTbl = A3HMTrtbl33nit, .cTbl = A3HMTctbl33nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 35, .refBr = 136, .cGma = gma2p15, .rTbl = A3HMTrtbl35nit, .cTbl = A3HMTctbl35nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 37, .refBr = 142, .cGma = gma2p15, .rTbl = A3HMTrtbl37nit, .cTbl = A3HMTctbl37nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 39, .refBr = 152, .cGma = gma2p15, .rTbl = A3HMTrtbl39nit, .cTbl = A3HMTctbl39nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 41, .refBr = 158, .cGma = gma2p15, .rTbl = A3HMTrtbl41nit, .cTbl = A3HMTctbl41nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 44, .refBr = 169, .cGma = gma2p15, .rTbl = A3HMTrtbl44nit, .cTbl = A3HMTctbl44nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 47, .refBr = 177, .cGma = gma2p15, .rTbl = A3HMTrtbl47nit, .cTbl = A3HMTctbl47nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 50, .refBr = 190, .cGma = gma2p15, .rTbl = A3HMTrtbl50nit, .cTbl = A3HMTctbl50nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 53, .refBr = 199, .cGma = gma2p15, .rTbl = A3HMTrtbl53nit, .cTbl = A3HMTctbl53nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 56, .refBr = 210, .cGma = gma2p15, .rTbl = A3HMTrtbl56nit, .cTbl = A3HMTctbl56nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 60, .refBr = 222, .cGma = gma2p15, .rTbl = A3HMTrtbl60nit, .cTbl = A3HMTctbl60nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 64, .refBr = 234, .cGma = gma2p15, .rTbl = A3HMTrtbl64nit, .cTbl = A3HMTctbl64nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 68, .refBr = 251, .cGma = gma2p15, .rTbl = A3HMTrtbl68nit, .cTbl = A3HMTctbl68nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 72, .refBr = 263, .cGma = gma2p15, .rTbl = A3HMTrtbl72nit, .cTbl = A3HMTctbl72nit, .aid = HMTaid8001, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 77, .refBr = 197, .cGma = gma2p15, .rTbl = A3HMTrtbl77nit, .cTbl = A3HMTctbl77nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 82, .refBr = 208, .cGma = gma2p15, .rTbl = A3HMTrtbl82nit, .cTbl = A3HMTctbl82nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 87, .refBr = 217, .cGma = gma2p15, .rTbl = A3HMTrtbl87nit, .cTbl = A3HMTctbl87nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 93, .refBr = 233, .cGma = gma2p15, .rTbl = A3HMTrtbl93nit, .cTbl = A3HMTctbl93nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 99, .refBr = 247, .cGma = gma2p15, .rTbl = A3HMTrtbl99nit, .cTbl = A3HMTctbl99nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
	{.br = 105, .refBr = 260, .cGma = gma2p15, .rTbl = A3HMTrtbl105nit, .cTbl = A3HMTctbl105nit, .aid = HMTaid7003, .elvCaps = HMTelvCaps, .elv = HMTelv},
};


static int hmt_init_dimming(struct dsim_device *dsim, u8 *mtp)
{
	int i, j;
	int pos = 0;
	int ret = 0;
	short temp;
	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *diminfo = NULL;
	unsigned char panelline;

	dimming = (struct dim_data *)kmalloc(sizeof(struct dim_data), GFP_KERNEL);
	if (!dimming) {
		dsim_err("failed to allocate memory for dim data\n");
		ret = -ENOMEM;
		goto error;
	}

	panelline = panel->id[0] & 0xc0;

	if (panelline == S6E3HF2_A3_LINE_ID) {
		dsim_info("%s hmt init dimming info for A3 Line Daisy panel\n", __func__);
		diminfo = a3_hmt_dimming_info;
	} else {
		dsim_info("%s hmt init dimming info for A2 Line Daisy panel\n", __func__);
		diminfo = a2_hmt_dimming_info;
	}

	panel->hmt_dim_data= (void *)dimming;
	panel->hmt_dim_info = (void *)diminfo;

	panel->hmt_br_tbl = (unsigned int *)hmt_br_tbl;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos+1];
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
	temp = (mtp[pos+1]) << 8 | mtp[pos];

	for(i=0;i<CI_MAX;i++)
		dimming->vt_mtp[i] = (temp >> (i*4)) &0x0f;

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

static int s6e3hf2_read_init_info(struct dsim_device *dsim, unsigned char* mtp, unsigned char* hbm)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[S6E3HF2_COORDINATE_LEN] = {0,};
	unsigned char buf[S6E3HF2_MTP_DATE_SIZE] = {0, };
	unsigned char hbm_gamma[S6E3HF2_HBMGAMMA_LEN + 1] = {0, };


	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	// id
	ret = dsim_read_hl_data(dsim, S6E3HF2_ID_REG, S6E3HF2_ID_LEN, dsim->priv.id);
	if (ret != S6E3HF2_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
		panel->lcdConnected = PANEL_DISCONNECTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for(i = 0; i < S6E3HF2_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	// mtp
	ret = dsim_read_hl_data(dsim, S6E3HF2_MTP_ADDR, S6E3HF2_MTP_DATE_SIZE, buf);
	if (ret != S6E3HF2_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, S6E3HF2_MTP_SIZE);
	memcpy(dsim->priv.date, &buf[40], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3HF2_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E3HF2_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	// coordinate
	ret = dsim_read_hl_data(dsim, S6E3HF2_COORDINATE_REG, S6E3HF2_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3HF2_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];	/* Y */
	dsim_info("READ coordi : ");
	for(i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");


	// code
	ret = dsim_read_hl_data(dsim, S6E3HF2_CODE_REG, S6E3HF2_CODE_LEN, dsim->priv.code);
	if (ret != S6E3HF2_CODE_LEN) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ code : ");
	for(i = 0; i < S6E3HF2_CODE_LEN; i++)
		dsim_info("%x, ", dsim->priv.code[i]);
	dsim_info("\n");

	// tset
	ret = dsim_read_hl_data(dsim, TSET_REG, TSET_LEN - 1, dsim->priv.tset);
	if (ret < TSET_LEN - 1) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ tset : ");
	for(i = 0; i < TSET_LEN - 1; i++)
		dsim_info("%x, ", dsim->priv.tset[i]);
	dsim_info("\n");

	// elvss
	ret = dsim_read_hl_data(dsim, ELVSS_REG, ELVSS_LEN - 1, dsim->priv.elvss_set);
	if (ret < ELVSS_LEN - 1) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("READ elvss : ");
	for(i = 0; i < ELVSS_LEN - 1; i++)
		dsim_info("%x \n", dsim->priv.elvss_set[i]);

/* read hbm elvss for hbm interpolation */
	panel->hbm_elvss = dsim->priv.elvss_set[S6E3HF2_HBM_ELVSS_INDEX];

	ret = dsim_read_hl_data(dsim, S6E3HF2_HBMGAMMA_REG, S6E3HF2_HBMGAMMA_LEN + 1, hbm_gamma);
	if (ret != S6E3HF2_HBMGAMMA_LEN + 1) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("HBM Gamma : ");
	memcpy(hbm, hbm_gamma + 1, S6E3HF2_HBMGAMMA_LEN);

	for(i = 1; i < S6E3HF2_HBMGAMMA_LEN + 1; i++)
		dsim_info("hbm gamma[%d] : %x\n", i, hbm_gamma[i]);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto read_fail;
	}
read_exit:
	return 0;

read_fail:
	return -ENODEV;

}


static int s6e3hf2_read_reg_status(struct dsim_device *dsim, bool need_key_unlock )
{
	static int cnt = 0;
	static int err_cnt = 0;
	int ret = 0;
	int result = 0;

	unsigned char reg_buffer_dsi[S6E3HF2_REG_DSI_LEN+4] = { 0, };
	unsigned char reg_buffer_mic[S6E3HF2_REG_MIC_LEN+4] = { 0, };

	if( need_key_unlock ) {
		ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		}
	}

	ret = dsim_read_hl_data(dsim, S6E3HF2_REG_DSI_ADDR, S6E3HF2_REG_DSI_LEN, reg_buffer_dsi);
	ret = dsim_read_hl_data(dsim, S6E3HF2_REG_MIC_ADDR, S6E3HF2_REG_MIC_LEN, reg_buffer_mic);

	if( reg_buffer_dsi[0]!=SEQ_SINGLE_DSI_1[1] || reg_buffer_mic[0]!=SEQ_SINGLE_DSI_2[1] ) {
		dsim_err( "%s : register unMatch detected(%d,%d). DSI(%02x.%02x)->%02x.%02x, MIC(%02x.%02x)->%02x.%02x\n",
			__func__, cnt, ++err_cnt, SEQ_SINGLE_DSI_1[0], SEQ_SINGLE_DSI_1[1], S6E3HF2_REG_DSI_ADDR, reg_buffer_dsi[0],
			SEQ_SINGLE_DSI_2[0], SEQ_SINGLE_DSI_2[1], S6E3HF2_REG_MIC_ADDR, reg_buffer_mic[0] );
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


static int s6e3hf2_wqhd_dump(struct dsim_device *dsim)
{
	int ret = 0;
	int i;
	unsigned char id[S6E3HF2_ID_LEN];
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
		dsim_err("%s : can't read Panel's EA Reg\n",__func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's 0xEA Reg Value ===\n");
	dsim_info("* 0xEA : buf[0] = %x\n", err_buf[0]);
	dsim_info("* 0xEA : buf[1] = %x\n", err_buf[1]);

	ret = dsim_read_hl_data(dsim, S6E3HF2_RDDPM_ADDR, 3, rddpm);
	if (ret != 3) {
		dsim_err("%s : can't read RDDPM Reg\n",__func__);
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

	ret = dsim_read_hl_data(dsim, S6E3HF2_RDDSM_ADDR, 3, rddsm);
	if (ret != 3) {
		dsim_err("%s : can't read RDDSM Reg\n",__func__);
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

	ret = dsim_read_hl_data(dsim, S6E3HF2_ID_REG, S6E3HF2_ID_LEN, id);
	if (ret != S6E3HF2_ID_LEN) {
		dsim_err("%s : can't read panel id\n",__func__);
		goto dump_exit;
	}

	dsim_info("READ ID : ");
	for(i = 0; i < S6E3HF2_ID_LEN; i++)
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
	return ret;

}
static int s6e3hf2_wqhd_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3HF2_MTP_SIZE] = {0, };
	unsigned char hbm[S6E3HF2_HBMGAMMA_LEN] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;
#ifdef CONFIG_LCD_ALPM
	panel->alpm = 0;
	panel->current_alpm = 0;
	mutex_init(&panel->alpm_lock);
#endif

	ret = s6e3hf2_read_init_info(dsim, mtp, hbm);
	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}
#ifdef CONFIG_LCD_ALPM
	panel->mtpForALPM[0] = 0xC8;
	memcpy(&(panel->mtpForALPM[1]), mtp, ARRAY_SIZE(mtp));
	panel->prev_VT[0] = panel->mtpForALPM[34];
	panel->prev_VT[1] = panel->mtpForALPM[35];
#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	panel->mdnie_support = 1;
#endif

/* test */
/*
	panel->id[0] = 0x40;
	panel->id[1] = 0x11;
	panel->id[2] = 0x96;
*/

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(dsim, mtp, hbm);
	if (ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#ifdef CONFIG_LCD_HMT
	ret = hmt_init_dimming(dsim, mtp);
	if(ret) {
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
	}
#endif
#endif

probe_exit:
	return ret;

}


static int s6e3hf2_wqhd_displayon(struct dsim_device *dsim)
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


static int s6e3hf2_wqhd_exit(struct dsim_device *dsim)
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
		dsim_info( "%s : ALPM mode\n", __func__);
	} else {
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

static int s6e3hf2_wqhd_init(struct dsim_device *dsim)
{
	int ret = 0;
	int cnt;
	dsim_info("MDD : %s was called\n", __func__);

#ifdef CONFIG_LCD_ALPM
	if (dsim->priv.current_alpm) {
		dsim_info("%s : ALPM mode\n", __func__);

		return ret;
	}
#endif

	/* 7. Sleep Out(11h) */
	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(5);

	/* 9. Interface Setting */
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}

	cnt = 0;
	do {
		if( cnt>0 ) dsim_err( "%s : DSI/MIC cmd retry\n", __func__ );

		ret = dsim_write_hl_data(dsim, SEQ_SINGLE_DSI_1, ARRAY_SIZE(SEQ_SINGLE_DSI_1));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SINGLE_DSI_1\n", __func__);
			//goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_SINGLE_DSI_2, ARRAY_SIZE(SEQ_SINGLE_DSI_2));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SINGLE_DSI_2\n", __func__);
			//goto init_exit;
		}
	} while( s6e3hf2_read_reg_status(dsim, false ) && cnt++ <3 );
	// if( cnt >= 3 ) panic( "s6e3hf2_read_reg_status()" );

#ifdef CONFIG_LCD_HMT
	if(dsim->priv.hmt_on != HMT_ON)
#endif
		msleep(120);

	/* Common Setting */
	ret = dsim_write_hl_data(dsim, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TSP_TE, ARRAY_SIZE(SEQ_TSP_TE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PENTILE_SETTING, ARRAY_SIZE(SEQ_PENTILE_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PENTILE_SETTING\n", __func__);
		goto init_exit;
	}

	/* POC setting */
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_POC_SETTING1, ARRAY_SIZE(SEQ_POC_SETTING1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_POC_SETTING1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_POC_SETTING2, ARRAY_SIZE(SEQ_POC_SETTING2));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_POC_SETTING2\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}

	/* PCD setting */
	ret = dsim_write_hl_data(dsim, SEQ_PCD_SETTING, ARRAY_SIZE(SEQ_PCD_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PCD_SETTING\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ERR_FG_SETTING, ARRAY_SIZE(SEQ_ERR_FG_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ERR_FG_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TE_START_SETTING, ARRAY_SIZE(SEQ_TE_START_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_START_SETTING\n", __func__);
		goto init_exit;
	}

#ifndef CONFIG_PANEL_AID_DIMMING
	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_AOR_CONTROL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	/* elvss */
	ret = dsim_write_hl_data(dsim, SEQ_TSET_GLOBAL, ARRAY_SIZE(SEQ_TSET_GLOBAL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET_GLOBAL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_ELVSS_SET, ARRAY_SIZE(SEQ_ELVSS_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ELVSS_SET\n", __func__);
		goto init_exit;
	}
	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF_OPR_AVR, ARRAY_SIZE(SEQ_ACL_OFF_OPR_AVR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
		goto init_exit;
	}
#endif
	if (s6e3hf2_lcd_type == S6E3HF2_LCDTYPE_WQXGA) {
		ret = dsim_write_hl_data(dsim, SEQ_WQXGA_PARTIAL_UPDATE, ARRAY_SIZE(SEQ_WQXGA_PARTIAL_UPDATE));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_WQXGA_PARTIAL_UPDATE\n", __func__);
			goto init_exit;
		}
	}
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;
}


struct dsim_panel_ops s6e3hf2_panel_ops = {
	.early_probe = NULL,
	.probe		= s6e3hf2_wqhd_probe,
	.displayon	= s6e3hf2_wqhd_displayon,
	.exit		= s6e3hf2_wqhd_exit,
	.init		= s6e3hf2_wqhd_init,
	.dump 		= s6e3hf2_wqhd_dump,
};

struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
	return &s6e3hf2_panel_ops;
}

static int __init s6e3hf2_get_lcd_type(char *arg)
{
	unsigned int lcdtype;

	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

	if ((lcdtype == S6E3HF2_WQXGA_ID1) || (lcdtype == S6E3HF2_WQXGA_ID2)) {
		s6e3hf2_lcd_type = S6E3HF2_LCDTYPE_WQXGA;
		dsim_info("S6E3HF2 LCD TYPE : %d (WQXGA)\n", s6e3hf2_lcd_type);
	} else {
		s6e3hf2_lcd_type = S6E3HF2_LCDTYPE_WQHD;
		dsim_info("S6E3HF2 LCD TYPE : %d (WQHD)\n", s6e3hf2_lcd_type);
	}
	return 0;
}
early_param("lcdtype", s6e3hf2_get_lcd_type);

