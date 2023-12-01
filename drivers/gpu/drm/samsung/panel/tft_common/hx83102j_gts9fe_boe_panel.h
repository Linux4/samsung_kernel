/*
 * linux/drivers/video/fbdev/exynos/panel/hx83122a/hx83102j_gts9fe_boe_panel.h
 *
 * Header file for HX83102J Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HX83102J_GTS9FE_BOE_PANEL_H__
#define __HX83102J_GTS9FE_BOE_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "tft_common.h"
#include "tft_function.h"
#include "hx83102j_gts9fe_boe_resol.h"

#undef __pn_name__
#define __pn_name__	gts9fe

#undef __PN_NAME__
#define __PN_NAME__

#define HX83102J_NR_STEP (256)
#define HX83102J_HBM_STEP 45
#define HX83102J_TOTAL_STEP (HX83102J_NR_STEP + HX83102J_HBM_STEP) /* 0 ~ 256 */

static unsigned int hx83102j_gts9fe_boe_brt_tbl[HX83102J_TOTAL_STEP] = {
	BRT(0),
	BRT(1), BRT(2), BRT(3), BRT(4), BRT(5), BRT(6), BRT(7), BRT(8), BRT(9), BRT(10),
	BRT(11), BRT(12), BRT(13), BRT(14), BRT(15), BRT(16), BRT(17), BRT(18), BRT(19), BRT(20),
	BRT(21), BRT(22), BRT(23), BRT(24), BRT(25), BRT(26), BRT(27), BRT(28), BRT(29), BRT(30),
	BRT(31), BRT(32), BRT(33), BRT(34), BRT(35), BRT(36), BRT(37), BRT(38), BRT(39), BRT(40),
	BRT(41), BRT(42), BRT(43), BRT(44), BRT(45), BRT(46), BRT(47), BRT(48), BRT(49), BRT(50),
	BRT(51), BRT(52), BRT(53), BRT(54), BRT(55), BRT(56), BRT(57), BRT(58), BRT(59), BRT(60),
	BRT(61), BRT(62), BRT(63), BRT(64), BRT(65), BRT(66), BRT(67), BRT(68), BRT(69), BRT(70),
	BRT(71), BRT(72), BRT(73), BRT(74), BRT(75), BRT(76), BRT(77), BRT(78), BRT(79), BRT(80),
	BRT(81), BRT(82), BRT(83), BRT(84), BRT(85), BRT(86), BRT(87), BRT(88), BRT(89), BRT(90),
	BRT(91), BRT(92), BRT(93), BRT(94), BRT(95), BRT(96), BRT(97), BRT(98), BRT(99), BRT(100),
	BRT(101), BRT(102), BRT(103), BRT(104), BRT(105), BRT(106), BRT(107), BRT(108), BRT(109), BRT(110),
	BRT(111), BRT(112), BRT(113), BRT(114), BRT(115), BRT(116), BRT(117), BRT(118), BRT(119), BRT(120),
	BRT(121), BRT(122), BRT(123), BRT(124), BRT(125), BRT(126), BRT(127), BRT(128), BRT(129), BRT(130),
	BRT(131), BRT(132), BRT(133), BRT(134), BRT(135), BRT(136), BRT(137), BRT(138), BRT(139), BRT(140),
	BRT(141), BRT(142), BRT(143), BRT(144), BRT(145), BRT(146), BRT(147), BRT(148), BRT(149), BRT(150),
	BRT(151), BRT(152), BRT(153), BRT(154), BRT(155), BRT(156), BRT(157), BRT(158), BRT(159), BRT(160),
	BRT(161), BRT(162), BRT(163), BRT(164), BRT(165), BRT(166), BRT(167), BRT(168), BRT(169), BRT(170),
	BRT(171), BRT(172), BRT(173), BRT(174), BRT(175), BRT(176), BRT(177), BRT(178), BRT(179), BRT(180),
	BRT(181), BRT(182), BRT(183), BRT(184), BRT(185), BRT(186), BRT(187), BRT(188), BRT(189), BRT(190),
	BRT(191), BRT(192), BRT(193), BRT(194), BRT(195), BRT(196), BRT(197), BRT(198), BRT(199), BRT(200),
	BRT(201), BRT(202), BRT(203), BRT(204), BRT(205), BRT(206), BRT(207), BRT(208), BRT(209), BRT(210),
	BRT(211), BRT(212), BRT(213), BRT(214), BRT(215), BRT(216), BRT(217), BRT(218), BRT(219), BRT(220),
	BRT(221), BRT(222), BRT(223), BRT(224), BRT(225), BRT(226), BRT(227), BRT(228), BRT(229), BRT(230),
	BRT(231), BRT(232), BRT(233), BRT(234), BRT(235), BRT(236), BRT(237), BRT(238), BRT(239), BRT(240),
	BRT(241), BRT(242), BRT(243), BRT(244), BRT(245), BRT(246), BRT(247), BRT(248), BRT(249), BRT(250),
	BRT(251), BRT(252), BRT(253), BRT(254), BRT(255),
	/* HBM */
	BRT(256), BRT(257), BRT(258), BRT(259), BRT(260), BRT(261), BRT(262), BRT(263), BRT(264), BRT(265),
	BRT(266), BRT(267), BRT(268), BRT(269), BRT(270), BRT(271), BRT(272), BRT(273), BRT(274), BRT(275),
	BRT(276), BRT(277), BRT(278), BRT(279), BRT(280), BRT(281), BRT(282), BRT(283), BRT(284), BRT(285),
	BRT(286), BRT(287), BRT(288), BRT(289), BRT(290), BRT(291), BRT(292), BRT(293), BRT(294), BRT(295),
	BRT(296), BRT(297), BRT(298), BRT(299), BRT(300),
};

static unsigned int hx83102j_gts9fe_boe_step_cnt_tbl[HX83102J_TOTAL_STEP] = {
	[0 ... 255] = 1,
	/* HBM */
	[256 ... 300] = 1,
};

struct brightness_table hx83102j_gts9fe_boe_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = hx83102j_gts9fe_boe_brt_tbl,
	.sz_brt = ARRAY_SIZE(hx83102j_gts9fe_boe_brt_tbl),
	.sz_ui_brt = HX83102J_NR_STEP,
	.sz_hbm_brt = HX83102J_HBM_STEP,
	.lum = hx83102j_gts9fe_boe_brt_tbl,
	.sz_lum = ARRAY_SIZE(hx83102j_gts9fe_boe_brt_tbl),
	.sz_ui_lum = HX83102J_NR_STEP,
	.sz_hbm_lum = HX83102J_HBM_STEP,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = hx83102j_gts9fe_boe_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(hx83102j_gts9fe_boe_step_cnt_tbl),
	.vtotal = 0,
};

static struct panel_dimming_info hx83102j_gts9fe_boe_panel_dimming_info = {
	.name = "hx83122a_gts9fe",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = -1,
	.brt_tbl = &hx83102j_gts9fe_boe_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,	/* read dim flash when probe or not */
	.hbm_aor = NULL,
};

static u8 hx83102j_gts9fe_boe_brt_table[HX83102J_TOTAL_STEP][2] = {
	// platfom 0
	{ 0x00, 0x00 },
	// platfrom 1 ~ 255
	{ 0x00, 0x18 }, { 0x00, 0x18 }, { 0x00, 0x24 }, { 0x00, 0x30 }, { 0x00, 0x3C },
	{ 0x00, 0x48 }, { 0x00, 0x54 }, { 0x00, 0x60 }, { 0x00, 0x6D }, { 0x00, 0x79 },
	{ 0x00, 0x85 }, { 0x00, 0x91 }, { 0x00, 0x9D }, { 0x00, 0xA9 }, { 0x00, 0xB5 },
	{ 0x00, 0xC1 }, { 0x00, 0xCD }, { 0x00, 0xDA }, { 0x00, 0xE6 }, { 0x00, 0xF2 },
	{ 0x00, 0xFE }, { 0x01, 0x0A }, { 0x01, 0x16 }, { 0x01, 0x22 }, { 0x01, 0x2E },
	{ 0x01, 0x3A }, { 0x01, 0x47 }, { 0x01, 0x53 }, { 0x01, 0x5F }, { 0x01, 0x6B },
	{ 0x01, 0x77 }, { 0x01, 0x83 }, { 0x01, 0x8F }, { 0x01, 0x9B }, { 0x01, 0xA8 },
	{ 0x01, 0xB4 }, { 0x01, 0xC0 }, { 0x01, 0xCC }, { 0x01, 0xD8 }, { 0x01, 0xE4 },
	{ 0x01, 0xF0 }, { 0x01, 0xFC }, { 0x02, 0x08 }, { 0x02, 0x15 }, { 0x02, 0x21 },
	{ 0x02, 0x2D }, { 0x02, 0x39 }, { 0x02, 0x45 }, { 0x02, 0x51 }, { 0x02, 0x5D },
	{ 0x02, 0x69 }, { 0x02, 0x75 }, { 0x02, 0x82 }, { 0x02, 0x8E }, { 0x02, 0x9A },
	{ 0x02, 0xA6 }, { 0x02, 0xB2 }, { 0x02, 0xBE }, { 0x02, 0xCA }, { 0x02, 0xD6 },
	{ 0x02, 0xE2 }, { 0x02, 0xEF }, { 0x02, 0xFB }, { 0x03, 0x07 }, { 0x03, 0x13 },
	{ 0x03, 0x1F }, { 0x03, 0x2B }, { 0x03, 0x37 }, { 0x03, 0x43 }, { 0x03, 0x50 },
	{ 0x03, 0x5C }, { 0x03, 0x68 }, { 0x03, 0x74 }, { 0x03, 0x80 }, { 0x03, 0x8C },
	{ 0x03, 0x98 }, { 0x03, 0xA4 }, { 0x03, 0xB0 }, { 0x03, 0xBD }, { 0x03, 0xC9 },
	{ 0x03, 0xD5 }, { 0x03, 0xE1 }, { 0x03, 0xED }, { 0x03, 0xF9 }, { 0x04, 0x05 },
	{ 0x04, 0x11 }, { 0x04, 0x1D }, { 0x04, 0x2A }, { 0x04, 0x36 }, { 0x04, 0x42 },
	{ 0x04, 0x4E }, { 0x04, 0x5A }, { 0x04, 0x66 }, { 0x04, 0x72 }, { 0x04, 0x7E },
	{ 0x04, 0x8A }, { 0x04, 0x97 }, { 0x04, 0xA3 }, { 0x04, 0xAF }, { 0x04, 0xBB },
	{ 0x04, 0xC7 }, { 0x04, 0xD3 }, { 0x04, 0xDF }, { 0x04, 0xEB }, { 0x04, 0xF8 },
	{ 0x05, 0x04 }, { 0x05, 0x10 }, { 0x05, 0x1C }, { 0x05, 0x28 }, { 0x05, 0x34 },
	{ 0x05, 0x40 }, { 0x05, 0x4C }, { 0x05, 0x58 }, { 0x05, 0x65 }, { 0x05, 0x71 },
	{ 0x05, 0x7D }, { 0x05, 0x89 }, { 0x05, 0x95 }, { 0x05, 0xA1 }, { 0x05, 0xAD },
	{ 0x05, 0xB9 }, { 0x05, 0xC5 }, { 0x05, 0xD2 }, { 0x05, 0xDE }, { 0x05, 0xEA },
	{ 0x05, 0xF6 }, { 0x06, 0x02 }, { 0x06, 0x0E }, { 0x06, 0x1A }, { 0x06, 0x26 },
	{ 0x06, 0x33 }, { 0x06, 0x3F }, { 0x06, 0x4B }, { 0x06, 0x57 }, { 0x06, 0x63 },
	{ 0x06, 0x6F }, { 0x06, 0x7B }, { 0x06, 0x87 }, { 0x06, 0x93 }, { 0x06, 0xA0 },
	{ 0x06, 0xAC }, { 0x06, 0xB8 }, { 0x06, 0xC4 }, { 0x06, 0xD0 }, { 0x06, 0xDC },
	{ 0x06, 0xE8 }, { 0x06, 0xF4 }, { 0x07, 0x00 }, { 0x07, 0x0D }, { 0x07, 0x19 },
	{ 0x07, 0x25 }, { 0x07, 0x31 }, { 0x07, 0x3D }, { 0x07, 0x49 }, { 0x07, 0x55 },
	{ 0x07, 0x61 }, { 0x07, 0x6D }, { 0x07, 0x7A }, { 0x07, 0x86 }, { 0x07, 0x92 },
	{ 0x07, 0x9E }, { 0x07, 0xAA }, { 0x07, 0xB6 }, { 0x07, 0xC2 }, { 0x07, 0xCE },
	{ 0x07, 0xDB }, { 0x07, 0xE7 }, { 0x07, 0xF3 }, { 0x07, 0xFF }, { 0x08, 0x0B },
	{ 0x08, 0x17 }, { 0x08, 0x23 }, { 0x08, 0x2F }, { 0x08, 0x3B }, { 0x08, 0x48 },
	{ 0x08, 0x54 }, { 0x08, 0x60 }, { 0x08, 0x6C }, { 0x08, 0x78 }, { 0x08, 0x84 },
	{ 0x08, 0x90 }, { 0x08, 0x9C }, { 0x08, 0xA8 }, { 0x08, 0xB5 }, { 0x08, 0xC1 },
	{ 0x08, 0xCD }, { 0x08, 0xD9 }, { 0x08, 0xE5 }, { 0x08, 0xF1 }, { 0x08, 0xFD },
	{ 0x09, 0x09 }, { 0x09, 0x15 }, { 0x09, 0x22 }, { 0x09, 0x2E }, { 0x09, 0x3A },
	{ 0x09, 0x46 }, { 0x09, 0x52 }, { 0x09, 0x5E }, { 0x09, 0x6A }, { 0x09, 0x76 },
	{ 0x09, 0x83 }, { 0x09, 0x8F }, { 0x09, 0x9B }, { 0x09, 0xA7 }, { 0x09, 0xB3 },
	{ 0x09, 0xBF }, { 0x09, 0xCB }, { 0x09, 0xD7 }, { 0x09, 0xE3 }, { 0x09, 0xF0 },
	{ 0x09, 0xFC }, { 0x0A, 0x08 }, { 0x0A, 0x14 }, { 0x0A, 0x20 }, { 0x0A, 0x2C },
	{ 0x0A, 0x38 }, { 0x0A, 0x44 }, { 0x0A, 0x50 }, { 0x0A, 0x5D }, { 0x0A, 0x69 },
	{ 0x0A, 0x75 }, { 0x0A, 0x81 }, { 0x0A, 0x8D }, { 0x0A, 0x99 }, { 0x0A, 0xA5 },
	{ 0x0A, 0xB1 }, { 0x0A, 0xBE }, { 0x0A, 0xCA }, { 0x0A, 0xD6 }, { 0x0A, 0xE2 },
	{ 0x0A, 0xEE }, { 0x0A, 0xFA }, { 0x0B, 0x06 }, { 0x0B, 0x12 }, { 0x0B, 0x1E },
	{ 0x0B, 0x2B }, { 0x0B, 0x37 }, { 0x0B, 0x43 }, { 0x0B, 0x4F }, { 0x0B, 0x5B },
	{ 0x0B, 0x67 }, { 0x0B, 0x73 }, { 0x0B, 0x7F }, { 0x0B, 0x8B }, { 0x0B, 0x98 },
	{ 0x0B, 0xA4 }, { 0x0B, 0xB0 }, { 0x0B, 0xBC }, { 0x0B, 0xC8 }, { 0x0B, 0xD4 },
	{ 0x0B, 0xE0 }, { 0x0B, 0xEC }, { 0x0B, 0xF8 }, { 0x0C, 0x05 }, { 0x0C, 0x11 },
	// HBM(platfrom 256 ~ 300)
	{ 0x0C, 0x1E }, { 0x0C, 0x2C }, { 0x0C, 0x39 }, { 0x0C, 0x47 }, { 0x0C, 0x55 },
	{ 0x0C, 0x62 }, { 0x0C, 0x70 }, { 0x0C, 0x7D }, { 0x0C, 0x8B }, { 0x0C, 0x98 },
	{ 0x0C, 0xA6 }, { 0x0C, 0xB4 }, { 0x0C, 0xC1 }, { 0x0C, 0xCF }, { 0x0C, 0xDC },
	{ 0x0C, 0xEA }, { 0x0C, 0xF7 }, { 0x0D, 0x05 }, { 0x0D, 0x13 }, { 0x0D, 0x20 },
	{ 0x0D, 0x2E }, { 0x0D, 0x3B }, { 0x0D, 0x49 }, { 0x0D, 0x56 }, { 0x0D, 0x64 },
	{ 0x0D, 0x72 }, { 0x0D, 0x7F }, { 0x0D, 0x8D }, { 0x0D, 0x9A }, { 0x0D, 0xA8 },
	{ 0x0D, 0xB5 }, { 0x0D, 0xC3 }, { 0x0D, 0xD1 }, { 0x0D, 0xDE }, { 0x0D, 0xEC },
	{ 0x0D, 0xF9 }, { 0x0E, 0x07 }, { 0x0E, 0x14 }, { 0x0E, 0x22 }, { 0x0E, 0x30 },
	{ 0x0E, 0x3D }, { 0x0E, 0x4B }, { 0x0E, 0x58 }, { 0x0E, 0x66 }, { 0x0E, 0x73 },
};

static struct maptbl hx83102j_gts9fe_boe_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(hx83102j_gts9fe_boe_brt_table,
			&TFT_FUNC(TFT_MAPTBL_INIT_BRT),
			&TFT_FUNC(TFT_MAPTBL_GETIDX_BRT),
			&TFT_FUNC(TFT_MAPTBL_COPY_DEFAULT)),
};

static u8 SEQ_HX83102J_GTS9FE_BOE_SLEEP_OUT[] = {
	0x11
};

static u8 SEQ_HX83102J_GTS9FE_BOE_SLEEP_IN[] = {
	0x10
};

static u8 SEQ_HX83102J_GTS9FE_BOE_DISPLAY_ON[] = {
	0x29
};

static u8 SEQ_HX83102J_GTS9FE_BOE_DISPLAY_OFF[] = {
	0x28
};

static u8 SEQ_HX83102J_GTS9FE_BOE_BRIGHTNESS[] = {
	0x51,
	0x64, 0x0B,
};

/* < CABC Mode control Function > */

static u8 SEQ_HX83102J_GTS9FE_BOE_BRIGHTNESS_ON[] = {
	0x53,
	0x24,
};

/* Display config (1) */
static u8 SEQ_HX83102J_GTS9FE_BOE_001[] = {
	0xB9,
	0x83, 0x10, 0x21, 0x55, 0x00,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_002[] = {
	0xB1,
	0x2C, 0xB3, 0xB3, 0x2F, 0xEF, 0x42, 0xE1, 0x43, 0x36, 0x36,
	0x36, 0x36,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_003[] = {
	0xE9,
	0xD9,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_004[] = {
	0xB1,
	0x98, 0x33,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_005[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_006[] = {
	0xB2,
	0x00, 0x59, 0xA0, 0x00, 0x00, 0x12, 0x5E, 0x3C, 0x4B, 0x32,
	0x30, 0x0, 0x00, 0x88, 0xF5,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_007[] = {
	0xB4,
	0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44,
	0x01, 0x5C,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_008[] = {
	0xBC,
	0x1B, 0x0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_009[] = {
	0xBE,
	0x20,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_010[] = {
	0xBF,
	0xFC, 0xC4,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_011[] = {
	0xC0,
	0x38, 0x38, 0x22, 0x11, 0x22, 0xA0, 0x61, 0x8, 0xF5, 0x3,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_012[] = {
	0xE9,
	0xCC,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_013[] = {
	0xC7,
	0x80,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_014[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_015[] = {
	0xE9,
	0xC6,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_016[] = {
	0xC8,
	0x97,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_017[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_018[] = {
	0x51,
	0x00, 0x00,
};
/*
static u8 SEQ_HX83102J_GTS9FE_BOE_019[] = {
	0x53,
	0x24,
};
*/
static u8 SEQ_HX83102J_GTS9FE_BOE_020[] = {
	0xC9,
	0x00, 0x1E, 0x13, 0x88, 0x01,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_021[] = {
	0xE9,
	0xC3,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_022[] = {
	0xCE,
	0xE0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_023[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_024[] = {
	0xCB,
	0x8, 0x13, 0x07, 0x00, 0x08, 0x9B,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_025[] = {
	0xCC,
	0x2, 0x3, 0x4C,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_026[] = {
	0xD1,
	0x37, 0x6, 0x0, 0x2, 0x4, 0x0C, 0xFF,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_027[] = {
	0xD2,
	0x1F, 0x11, 0x1F,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_028[] = {
	0xD3,
	0x06, 0x00, 0x00, 0x00, 0x40, 0x04, 0x08, 0x04, 0x08, 0x27,
	0x47, 0x64, 0x4B, 0x12, 0x12, 0x03, 0x03, 0x32, 0x10, 0x0D,
	0x00, 0x0D, 0x32, 0x10, 0x7, 0x00, 0x7, 0x32, 0x19, 0x18,
	0x9, 0x18, 0x00, 0x00,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_029[] = {
	0xD5,
	0x24, 0x24, 0x24, 0x24, 0x07, 0x06, 0x07, 0x06, 0x05, 0x04,
	0x05, 0x04, 0x03, 0x02, 0x03, 0x02, 0x01, 0x00, 0x01, 0x00,
	0x21, 0x20, 0x21, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x1F, 0x1F, 0x1F, 0x1F, 0x1E, 0x1E, 0x1E, 0x1E,
	0x18, 0x18, 0x18, 0x18,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_030[] = {
	0xD8,
	0xAA, 0xAA, 0xAA, 0xAA, 0xFA, 0xA0, 0xAA, 0xAA, 0xAA, 0xAA,
	0xFA, 0xA0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_031[] = {
	0xE0,
	0x0, 0x5, 0x0D, 0x14, 0x1C, 0x30, 0x4C, 0x55, 0x60, 0x5E,
	0x7C, 0x84, 0x8B, 0x9B, 0x99, 0xA1, 0xA8, 0xB9, 0xB6, 0x59,
	0x5F, 0x69, 0x75, 0x0, 0x5, 0x0D, 0x14, 0x1C, 0x30, 0x4C,
	0x55, 0x60, 0x5E, 0x7C, 0x84, 0x8B, 0x9B, 0x99, 0xA1, 0xA8,
	0xB9, 0xB6, 0x59, 0x5F, 0x69, 0x75,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_032[] = {
	0xE1,
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x00, 0x02, 0xD0,
	0x00, 0x18, 0x02, 0xD0, 0x02, 0xD0, 0x02, 0x00, 0x02, 0x68,
	0x00, 0x20, 0x02, 0xA1, 0x00, 0x0A, 0x00, 0x0C, 0x04, 0x2D,
	0x03, 0x2E, 0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_033[] = {
	0xE7,
	0x3A, 0x1E, 0x1E, 0x4D, 0x50, 0x5C, 0x2, 0x37, 0x5C, 0x14,
	0x14, 0x00, 0x00, 0x00, 0x00, 0x12, 0x05, 0x02, 0x02, 0x0,
	0x33, 0x3, 0x84,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_034[] = {
	0xBD,
	0x1,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_035[] = {
	0xB1,
	0x1, 0xBF, 0x11,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_036[] = {
	0xE9,
	0xC5,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_037[] = {
	0xBA,
	0x4F,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_038[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_039[] = {
	0xD2,
	0xB4,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_040[] = {
	0xE9,
	0xC9,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_041[] = {
	0xD3,
	0x84,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_042[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_043[] = {
	0xD8,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xA0, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xA0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xA0, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xA0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_044[] = {
	0xE9,
	0xD1,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_045[] = {
	0xE1,
	0xF6, 0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_046[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_047[] = {
	0xE7,
	0x02, 0x00, 0x96, 0x01, 0xF0, 0x09, 0xF0, 0x0A, 0xA0, 0x00,
	0x00,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_048[] = {
	0xBD,
	0x2,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_049[] = {
	0xCB,
	0x3, 0x7, 0x00, 0x09, 0x53,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_050[] = {
	0xD8,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xA0, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xA0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_051[] = {
	0xE7,
	0xFF, 0x01, 0xFF, 0x01, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x22,
	0x00, 0x22, 0x81, 0x02, 0x40, 0x00, 0x20, 0x5C, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_052[] = {
	0xBD,
	0x3,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_053[] = {
	0xD8,
	0xFA, 0xAA, 0xAA, 0xAA, 0xFF, 0xA0, 0xFA, 0xAA, 0xAA, 0xAA,
	0xFF, 0xA0, 0xFA, 0xAA, 0xAA, 0xAA, 0xFF, 0xA0, 0xFA, 0xAA,
	0xAA, 0xAA, 0xFF, 0xA0, 0xFA, 0xAA, 0xAA, 0xAA, 0xFF, 0xA0,
	0xFA, 0xAA, 0xAA, 0xAA, 0xFF, 0xA0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_054[] = {
	0xE9,
	0xC6,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_055[] = {
	0xB4,
	0x3, 0xFF, 0xF8,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_056[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_057[] = {
	0xE1,
	0x1,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_058[] = {
	0xBD,
	0x0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_059[] = {
	0x55,
	0x1,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_060[] = {
	0xE4,
	0x2D, 0x03, 0x81, 0x2C, 0x1E, 0x1E, 0x1E, 0x1E, 0x00, 0x00,
	0x00, 0x01, 0x14,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_061[] = {
	0xBD,
	0x1,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_062[] = {
	0xE4,
	0xE1, 0xE1, 0xE1, 0xE1, 0xE1, 0xE1, 0xE1, 0xE1, 0xC7, 0xB2,
	0xA0, 0x90, 0x81, 0x75, 0x69, 0x5F, 0x55, 0x4C, 0x44, 0x3D,
	0x36, 0x2F, 0x27, 0x1C, 0x1B, 0x15, 0x0E, 0x8, 0x5, 0x4,
	0x2, 0x54, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_063[] = {
	0xBD,
	0x03,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_064[] = {
	0xE4,
	0xCC, 0xF2, 0x19, 0x40, 0x65, 0x8D, 0xB3, 0xD9, 0xFF, 0xFA,
	0xFF, 0x03,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_065[] = {
	0xBD,
	0x0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_066[] = {
	0xC1,
	0x1,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_067[] = {
	0xBD,
	0x1,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_068[] = {
	0xC1,
	0x0, 0x3, 0x7, 0x0B, 0x10, 0x13, 0x17, 0x1B, 0x1F, 0x23,
	0x27, 0x2B, 0x2F, 0x32, 0x37, 0x3B, 0x3F, 0x43, 0x47, 0x4A,
	0x52, 0x5A, 0x61, 0x69, 0x71, 0x79, 0x81, 0x89, 0x90, 0x99,
	0xA0, 0xA8, 0xB0, 0xB7, 0xBE, 0xC7, 0xCE, 0xD6, 0xDE, 0xE6,
	0xED, 0xF2, 0xF4, 0xF6, 0xF8, 0xF9, 0x3B, 0x2D, 0xFA, 0x71,
	0x92, 0x9F, 0xF8, 0xC8, 0x2C, 0xF5, 0xC0, 0x0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_069[] = {
	0xBD,
	0x2,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_070[] = {
	0xC1,
	0x0, 0x4, 0x7, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24,
	0x28, 0x2C, 0x30, 0x33, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C,
	0x54, 0x5C, 0x63, 0x6C, 0x74, 0x7C, 0x84, 0x8B, 0x93, 0x9C,
	0xA3, 0xAB, 0xB3, 0xBA, 0xC2, 0xCB, 0xD3, 0xDA, 0xE3, 0xEB,
	0xF3, 0xF7, 0xF9, 0xFB, 0xFD, 0xFE, 0x0C, 0x40, 0x55, 0x76,
	0xE4, 0x4C, 0x13, 0xC9, 0xFD, 0x30, 0x0, 0x0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_071[] = {
	0xBD,
	0x3,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_072[] = {
	0xC1,
	0x0, 0x4, 0x7, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24,
	0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x41, 0x44, 0x48, 0x4C,
	0x54, 0x5C, 0x64, 0x6C, 0x74, 0x7C, 0x84, 0x8C, 0x94, 0x9C,
	0xA4, 0xAC, 0xB4, 0xBB, 0xC3, 0xCC, 0xD3, 0xDB, 0xE4, 0xEC,
	0xF4, 0xF8, 0xFA, 0xFC, 0xFE, 0xFF, 0x0C, 0x45, 0xAA, 0x4B,
	0x39, 0xA5, 0xB9, 0xA0, 0x68, 0xF0, 0x0, 0x0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_073[] = {
	0xBD,
	0x0,
};

static u8 SEQ_HX83102J_GTS9FE_BOE_074[] = {
	0xB9,
	0x0, 0x0, 0x0,
};

static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_sleep_out, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_sleep_in, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_display_on, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_display_off, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_brightness_on, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_BRIGHTNESS_ON, 0);

static DEFINE_PKTUI(hx83102j_gts9fe_boe_brightness, &hx83102j_gts9fe_boe_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(hx83102j_gts9fe_boe_brightness, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_001, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_001, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_002, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_002, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_003, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_003, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_004, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_004, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_005, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_005, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_006, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_006, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_007, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_007, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_008, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_008, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_009, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_009, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_010, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_010, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_011, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_011, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_012, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_012, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_013, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_013, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_014, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_014, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_015, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_015, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_016, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_016, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_017, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_017, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_018, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_018, 0);
//static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_019, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_019, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_020, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_020, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_021, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_021, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_022, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_022, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_023, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_023, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_024, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_024, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_025, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_025, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_026, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_026, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_027, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_027, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_028, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_028, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_029, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_029, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_030, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_030, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_031, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_031, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_032, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_032, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_033, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_033, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_034, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_034, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_035, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_035, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_036, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_036, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_037, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_037, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_038, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_038, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_039, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_039, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_040, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_040, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_041, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_041, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_042, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_042, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_043, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_043, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_044, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_044, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_045, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_045, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_046, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_046, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_047, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_047, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_048, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_048, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_049, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_049, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_050, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_050, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_051, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_051, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_052, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_052, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_053, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_053, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_054, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_054, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_055, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_055, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_056, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_056, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_057, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_057, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_058, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_058, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_059, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_059, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_060, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_060, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_061, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_061, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_062, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_062, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_063, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_063, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_064, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_064, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_065, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_065, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_066, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_066, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_067, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_067, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_068, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_068, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_069, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_069, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_070, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_070, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_071, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_071, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_072, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_072, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_073, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_073, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_074, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS9FE_BOE_074, 0);

static DEFINE_PANEL_MDELAY(hx83102j_gts9fe_boe_wait_20msec, 20); /* 1 frame */
static DEFINE_PANEL_MDELAY(hx83102j_gts9fe_boe_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(hx83102j_gts9fe_boe_wait_105msec, 105);

static DEFINE_PNOBJ_CONFIG(hx83102j_gts9fe_boe_set_wait_tx_done_property_on, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_ON);
static DEFINE_PNOBJ_CONFIG(hx83102j_gts9fe_boe_set_wait_tx_done_property_off, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
static DEFINE_PNOBJ_CONFIG(hx83102j_gts9fe_boe_set_wait_tx_done_property_auto, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

static u8 HX83102J_GTS9FE_BOE_ID[TFT_COMMON_ID_LEN];
static DEFINE_RDINFO(hx83102j_gts9fe_boe_id1, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DA_OFS, TFT_COMMON_ID_DA_LEN);
static DEFINE_RDINFO(hx83102j_gts9fe_boe_id2, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DB_OFS, TFT_COMMON_ID_DB_LEN);
static DEFINE_RDINFO(hx83102j_gts9fe_boe_id3, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DC_OFS, TFT_COMMON_ID_DC_LEN);
static DECLARE_RESUI(hx83102j_gts9fe_boe_id) = {
	{ .rditbl = &RDINFO(hx83102j_gts9fe_boe_id1), .offset = 0 },
	{ .rditbl = &RDINFO(hx83102j_gts9fe_boe_id2), .offset = 1 },
	{ .rditbl = &RDINFO(hx83102j_gts9fe_boe_id3), .offset = 2 },
};
static DEFINE_RESOURCE(hx83102j_gts9fe_boe_id, HX83102J_GTS9FE_BOE_ID, RESUI(hx83102j_gts9fe_boe_id));

static void *hx83102j_gts9fe_boe_init_cmdtbl[] = {
	&PNOBJ_CONFIG(hx83102j_gts9fe_boe_set_wait_tx_done_property_on),
	&PKTINFO(hx83102j_gts9fe_boe_001),
	&PKTINFO(hx83102j_gts9fe_boe_002),
	&PKTINFO(hx83102j_gts9fe_boe_003),
	&PKTINFO(hx83102j_gts9fe_boe_004),
	&PKTINFO(hx83102j_gts9fe_boe_005),
	&PKTINFO(hx83102j_gts9fe_boe_006),
	&PKTINFO(hx83102j_gts9fe_boe_007),
	&PKTINFO(hx83102j_gts9fe_boe_008),
	&PKTINFO(hx83102j_gts9fe_boe_009),
	&PKTINFO(hx83102j_gts9fe_boe_010),
	&PKTINFO(hx83102j_gts9fe_boe_011),
	&PKTINFO(hx83102j_gts9fe_boe_012),
	&PKTINFO(hx83102j_gts9fe_boe_013),
	&PKTINFO(hx83102j_gts9fe_boe_014),
	&PKTINFO(hx83102j_gts9fe_boe_015),
	&PKTINFO(hx83102j_gts9fe_boe_016),
	&PKTINFO(hx83102j_gts9fe_boe_017),
	&PKTINFO(hx83102j_gts9fe_boe_018),
//	&PKTINFO(hx83102j_gts9fe_boe_019),
	&PKTINFO(hx83102j_gts9fe_boe_020),
	&PKTINFO(hx83102j_gts9fe_boe_021),
	&PKTINFO(hx83102j_gts9fe_boe_022),
	&PKTINFO(hx83102j_gts9fe_boe_023),
	&PKTINFO(hx83102j_gts9fe_boe_024),
	&PKTINFO(hx83102j_gts9fe_boe_025),
	&PKTINFO(hx83102j_gts9fe_boe_026),
	&PKTINFO(hx83102j_gts9fe_boe_027),
	&PKTINFO(hx83102j_gts9fe_boe_028),
	&PKTINFO(hx83102j_gts9fe_boe_029),
	&PKTINFO(hx83102j_gts9fe_boe_030),
	&PKTINFO(hx83102j_gts9fe_boe_031),
	&PKTINFO(hx83102j_gts9fe_boe_032),
	&PKTINFO(hx83102j_gts9fe_boe_033),
	&PKTINFO(hx83102j_gts9fe_boe_034),
	&PKTINFO(hx83102j_gts9fe_boe_035),
	&PKTINFO(hx83102j_gts9fe_boe_036),
	&PKTINFO(hx83102j_gts9fe_boe_037),
	&PKTINFO(hx83102j_gts9fe_boe_038),
	&PKTINFO(hx83102j_gts9fe_boe_039),
	&PKTINFO(hx83102j_gts9fe_boe_040),
	&PKTINFO(hx83102j_gts9fe_boe_041),
	&PKTINFO(hx83102j_gts9fe_boe_042),
	&PKTINFO(hx83102j_gts9fe_boe_043),
	&PKTINFO(hx83102j_gts9fe_boe_044),
	&PKTINFO(hx83102j_gts9fe_boe_045),
	&PKTINFO(hx83102j_gts9fe_boe_046),
	&PKTINFO(hx83102j_gts9fe_boe_047),
	&PKTINFO(hx83102j_gts9fe_boe_048),
	&PKTINFO(hx83102j_gts9fe_boe_049),
	&PKTINFO(hx83102j_gts9fe_boe_050),
	&PKTINFO(hx83102j_gts9fe_boe_051),
	&PKTINFO(hx83102j_gts9fe_boe_052),
	&PKTINFO(hx83102j_gts9fe_boe_053),
	&PKTINFO(hx83102j_gts9fe_boe_054),
	&PKTINFO(hx83102j_gts9fe_boe_055),
	&PKTINFO(hx83102j_gts9fe_boe_056),
	&PKTINFO(hx83102j_gts9fe_boe_057),
	&PKTINFO(hx83102j_gts9fe_boe_058),
	&PKTINFO(hx83102j_gts9fe_boe_059),
	&PKTINFO(hx83102j_gts9fe_boe_060),
	&PKTINFO(hx83102j_gts9fe_boe_061),
	&PKTINFO(hx83102j_gts9fe_boe_062),
	&PKTINFO(hx83102j_gts9fe_boe_063),
	&PKTINFO(hx83102j_gts9fe_boe_064),
	&PKTINFO(hx83102j_gts9fe_boe_065),
	&PKTINFO(hx83102j_gts9fe_boe_066),
	&PKTINFO(hx83102j_gts9fe_boe_067),
	&PKTINFO(hx83102j_gts9fe_boe_068),
	&PKTINFO(hx83102j_gts9fe_boe_069),
	&PKTINFO(hx83102j_gts9fe_boe_070),
	&PKTINFO(hx83102j_gts9fe_boe_071),
	&PKTINFO(hx83102j_gts9fe_boe_072),
	&PKTINFO(hx83102j_gts9fe_boe_073),
	&PKTINFO(hx83102j_gts9fe_boe_074),
	&PKTINFO(hx83102j_gts9fe_boe_sleep_out),
	&DLYINFO(hx83102j_gts9fe_boe_wait_105msec),
	&PKTINFO(hx83102j_gts9fe_boe_display_on),
	&PNOBJ_CONFIG(hx83102j_gts9fe_boe_set_wait_tx_done_property_auto),
};

static void *hx83102j_gts9fe_boe_res_init_cmdtbl[] = {
	&RESINFO(hx83102j_gts9fe_boe_id),
};

static void *hx83102j_gts9fe_boe_set_bl_cmdtbl[] = {
	&PKTINFO(hx83102j_gts9fe_boe_brightness),
};

static void *hx83102j_gts9fe_boe_display_on_cmdtbl[] = {
	&DLYINFO(hx83102j_gts9fe_boe_wait_40msec),
	&PKTINFO(hx83102j_gts9fe_boe_brightness),
	&PKTINFO(hx83102j_gts9fe_boe_brightness_on),
};

static void *hx83102j_gts9fe_boe_display_off_cmdtbl[] = {
	&PKTINFO(hx83102j_gts9fe_boe_display_off),
};

static void *hx83102j_gts9fe_boe_exit_cmdtbl[] = {
	&PKTINFO(hx83102j_gts9fe_boe_sleep_in),
};

static void *hx83102j_gts9fe_boe_display_mode_cmdtbl[] = {
	&PNOBJ_CONFIG(hx83102j_gts9fe_boe_set_wait_tx_done_property_off),
	&PKTINFO(hx83102j_gts9fe_boe_brightness),
	/* Will flush on next VFP */
	&PNOBJ_CONFIG(hx83102j_gts9fe_boe_set_wait_tx_done_property_auto),
};

static struct seqinfo hx83102j_gts9fe_boe_seqtbl[] = {
	SEQINFO_INIT(PANEL_INIT_SEQ, hx83102j_gts9fe_boe_init_cmdtbl),
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, hx83102j_gts9fe_boe_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_SET_BL_SEQ, hx83102j_gts9fe_boe_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, hx83102j_gts9fe_boe_display_mode_cmdtbl), /* Dummy */
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, hx83102j_gts9fe_boe_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, hx83102j_gts9fe_boe_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, hx83102j_gts9fe_boe_exit_cmdtbl),
};


/* BLIC 1 SETTING START */

static u8 HX83102J_GTS9FE_BOE_ISL98608_I2C_INIT[] = {
	0x06, 0x0D,
	0x08, 0x0A,
	0x09, 0x0A,
};

#ifdef DEBUG_I2C_READ
static u8 HX83102J_GTS9FE_BOE_ISL98608_I2C_DUMP[] = {
	0x06, 0x00,
	0x08, 0x00,
	0x09, 0x00,
};
#endif

static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_isl98608_i2c_init, I2C_PKT_TYPE_WR, HX83102J_GTS9FE_BOE_ISL98608_I2C_INIT, 0);
#ifdef DEBUG_I2C_READ
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_isl98608_i2c_dump, I2C_PKT_TYPE_RD, HX83102J_GTS9FE_BOE_ISL98608_I2C_DUMP, 0);
#endif

static void *hx83102j_gts9fe_boe_isl98608_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83102j_gts9fe_boe_isl98608_i2c_dump),
#endif
	&PKTINFO(hx83102j_gts9fe_boe_isl98608_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83102j_gts9fe_boe_isl98608_i2c_dump),
#endif
};

static void *hx83102j_gts9fe_boe_isl98608_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83102j_gts9fe_boe_isl98608_i2c_dump),
#endif
};

static struct seqinfo hx83102j_gts9fe_boe_isl98608_seq_tbl[] = {
	SEQINFO_INIT(PANEL_BLIC_I2C_ON_SEQ, hx83102j_gts9fe_boe_isl98608_init_cmdtbl),
	SEQINFO_INIT(PANEL_BLIC_I2C_OFF_SEQ, hx83102j_gts9fe_boe_isl98608_exit_cmdtbl),
};

static struct blic_data hx83102j_gts9fe_boe_isl98608_blic_data = {
	.name = "isl98608",
	.seqtbl = hx83102j_gts9fe_boe_isl98608_seq_tbl,
	.nr_seqtbl = ARRAY_SIZE(hx83102j_gts9fe_boe_isl98608_seq_tbl),
};

/* BLIC 2 SETTING START */
static u8 HX83102J_GTS9FE_BOE_MAX77816_I2C_INIT[] = {
	0x03, 0x30,
	0x02, 0x8E,
	0x03, 0x70,
	0x04, 0x78,
};

static u8 HX83102J_GTS9FE_BOE_MAX77816_I2C_EXIT_BLEN[] = {
	0x03, 0x32,
};

#ifdef DEBUG_I2C_READ
static u8 HX83102J_GTS9FE_BOE_MAX77816_I2C_DUMP[] = {
	0x04, 0x00,
	0x03, 0x00,
};
#endif

static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_max77816_i2c_init, I2C_PKT_TYPE_WR, HX83102J_GTS9FE_BOE_MAX77816_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_max77816_i2c_exit_blen, I2C_PKT_TYPE_WR, HX83102J_GTS9FE_BOE_MAX77816_I2C_EXIT_BLEN, 0);
#ifdef DEBUG_I2C_READ
static DEFINE_STATIC_PACKET(hx83102j_gts9fe_boe_max77816_i2c_dump, I2C_PKT_TYPE_RD, HX83102J_GTS9FE_BOE_MAX77816_I2C_DUMP, 0);
#endif

static void *hx83102j_gts9fe_boe_max77816_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83102j_gts9fe_boe_max77816_i2c_dump),
#endif
	&PKTINFO(hx83102j_gts9fe_boe_max77816_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83102j_gts9fe_boe_max77816_i2c_dump),
#endif
};

static void *hx83102j_gts9fe_boe_max77816_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83102j_gts9fe_boe_max77816_i2c_dump),
#endif
	&PKTINFO(hx83102j_gts9fe_boe_max77816_i2c_exit_blen),
};

static struct seqinfo hx83102j_gts9fe_boe_max77816_seq_tbl[] = {
	SEQINFO_INIT(PANEL_BLIC_I2C_ON_SEQ, hx83102j_gts9fe_boe_max77816_init_cmdtbl),
	SEQINFO_INIT(PANEL_BLIC_I2C_OFF_SEQ, hx83102j_gts9fe_boe_max77816_exit_cmdtbl),
};

static struct blic_data hx83102j_gts9fe_boe_max77816_blic_data = {
	.name = "max77816",
	.seqtbl = hx83102j_gts9fe_boe_max77816_seq_tbl,
	.nr_seqtbl = ARRAY_SIZE(hx83102j_gts9fe_boe_max77816_seq_tbl),
};

static struct blic_data *hx83102j_gts9fe_boe_blic_tbl[] = {
	&hx83102j_gts9fe_boe_isl98608_blic_data,
	&hx83102j_gts9fe_boe_max77816_blic_data,
};
/* BLIC SETTING END */


struct common_panel_info hx83102j_gts9fe_boe_panel_info = {
	.ldi_name = "hx83122a",
	.name = "hx83102j_gts9fe_boe",
	.model = "boe_10_9_inch",
	.vendor = "BOE",
	.id = 0x4CD230,
	.rev = 0,
	.ddi_props = {
		.err_fg_recovery = false,
		.support_vrr = true,
		.init_seq_by_lpdt = true,
	},
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.common_panel_modes = &hx83102j_gts9fe_boe_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(hx83102j_gts9fe_boe_default_resol),
		.resol = hx83102j_gts9fe_boe_default_resol,
	},
	.vrrtbl = hx83102j_gts9fe_boe_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(hx83102j_gts9fe_boe_default_vrrtbl),
	.maptbl = hx83102j_gts9fe_boe_maptbl,
	.nr_maptbl = ARRAY_SIZE(hx83102j_gts9fe_boe_maptbl),
	.seqtbl = hx83102j_gts9fe_boe_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(hx83102j_gts9fe_boe_seqtbl),
	.rditbl = NULL,
	.nr_rditbl = 0,
	.restbl = NULL,
	.nr_restbl = 0,
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &hx83102j_gts9fe_boe_panel_dimming_info,
	},
	.blic_data_tbl = hx83102j_gts9fe_boe_blic_tbl,
	.nr_blic_data_tbl = ARRAY_SIZE(hx83102j_gts9fe_boe_blic_tbl),
};
#endif /* __HX83102J_GTS9FE_BOE_PANEL_H__ */
