/*
 * linux/drivers/video/fbdev/exynos/panel/hx83122a/hx83122a_gts9fep_boe_panel.h
 *
 * Header file for HX83122A Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HX83122A_GTS9FEP_BOE_PANEL_H__
#define __HX83122A_GTS9FEP_BOE_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "tft_common.h"
#include "tft_function.h"
#include "hx83122a_gts9fep_boe_resol.h"

#undef __pn_name__
#define __pn_name__	gts9fep_boe

#undef __PN_NAME__
#define __PN_NAME__ GTS9FEP_BOE

#define HX83122A_NR_STEP (256)
#define HX83122A_HBM_STEP 52
#define HX83122A_TOTAL_STEP (HX83122A_NR_STEP + HX83122A_HBM_STEP) /* 0 ~ 256 */

static unsigned int hx83122a_gts9fep_boe_brt_tbl[HX83122A_TOTAL_STEP] = {
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
	BRT(296), BRT(297), BRT(298), BRT(299), BRT(300), BRT(301), BRT(302), BRT(303), BRT(304), BRT(305),
	BRT(306), BRT(307)
};

static unsigned int hx83122a_gts9fep_boe_step_cnt_tbl[HX83122A_TOTAL_STEP] = {
	[0 ... 255] = 1,
	/* HBM */
	[256 ... 307] = 1,
};

struct brightness_table hx83122a_gts9fep_boe_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = hx83122a_gts9fep_boe_brt_tbl,
	.sz_brt = ARRAY_SIZE(hx83122a_gts9fep_boe_brt_tbl),
	.sz_ui_brt = HX83122A_NR_STEP,
	.sz_hbm_brt = HX83122A_HBM_STEP,
	.lum = hx83122a_gts9fep_boe_brt_tbl,
	.sz_lum = ARRAY_SIZE(hx83122a_gts9fep_boe_brt_tbl),
	.sz_ui_lum = HX83122A_NR_STEP,
	.sz_hbm_lum = HX83122A_HBM_STEP,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = hx83122a_gts9fep_boe_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(hx83122a_gts9fep_boe_step_cnt_tbl),
	.vtotal = 0,
};

static struct panel_dimming_info hx83122a_gts9fep_boe_panel_dimming_info = {
	.name = "hx83122a_gts9fep_boe",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = -1,
	.brt_tbl = &hx83122a_gts9fep_boe_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,	/* read dim flash when probe or not */
	.hbm_aor = NULL,
};

static u8 hx83122a_gts9fep_boe_brt_table[HX83122A_TOTAL_STEP][2] = {
	// platfom 0
	{ 0x00, 0x00 },
	// platfrom 1 ~ 255
	{ 0x00, 0x20 }, { 0x00, 0x20 }, { 0x00, 0x2D }, { 0x00, 0x35 }, { 0x00, 0x47 },
	{ 0x00, 0x54 }, { 0x00, 0x61 }, { 0x00, 0x6E }, { 0x00, 0x7B }, { 0x00, 0x88 },
	{ 0x00, 0x95 }, { 0x00, 0xA2 }, { 0x00, 0xAF }, { 0x00, 0xBC }, { 0x00, 0xC9 },
	{ 0x00, 0xD6 }, { 0x00, 0xE3 }, { 0x00, 0xF0 }, { 0x00, 0xFD }, { 0x01, 0x0B },
	{ 0x01, 0x18 }, { 0x01, 0x25 }, { 0x01, 0x32 }, { 0x01, 0x3F }, { 0x01, 0x4C },
	{ 0x01, 0x59 }, { 0x01, 0x66 }, { 0x01, 0x73 }, { 0x01, 0x80 }, { 0x01, 0x8D },
	{ 0x01, 0x9A }, { 0x01, 0xA7 }, { 0x01, 0xB4 }, { 0x01, 0xC1 }, { 0x01, 0xCE },
	{ 0x01, 0xDB }, { 0x01, 0xE8 }, { 0x01, 0xF5 }, { 0x02, 0x02 }, { 0x02, 0x0F },
	{ 0x02, 0x1C }, { 0x02, 0x29 }, { 0x02, 0x36 }, { 0x02, 0x43 }, { 0x02, 0x50 },
	{ 0x02, 0x5D }, { 0x02, 0x6A }, { 0x02, 0x77 }, { 0x02, 0x84 }, { 0x02, 0x91 },
	{ 0x02, 0x9E }, { 0x02, 0xAB }, { 0x02, 0xB8 }, { 0x02, 0xC5 }, { 0x02, 0xD2 },
	{ 0x02, 0xDF }, { 0x02, 0xEC }, { 0x02, 0xF9 }, { 0x03, 0x06 }, { 0x03, 0x13 },
	{ 0x03, 0x20 }, { 0x03, 0x2D }, { 0x03, 0x3A }, { 0x03, 0x47 }, { 0x03, 0x54 },
	{ 0x03, 0x61 }, { 0x03, 0x6E }, { 0x03, 0x7B }, { 0x03, 0x88 }, { 0x03, 0x95 },
	{ 0x03, 0xA2 }, { 0x03, 0xAF }, { 0x03, 0xBC }, { 0x03, 0xC9 }, { 0x03, 0xD6 },
	{ 0x03, 0xE3 }, { 0x03, 0xF0 }, { 0x03, 0xFD }, { 0x04, 0x0A }, { 0x04, 0x17 },
	{ 0x04, 0x24 }, { 0x04, 0x31 }, { 0x04, 0x3E }, { 0x04, 0x4B }, { 0x04, 0x58 },
	{ 0x04, 0x65 }, { 0x04, 0x72 }, { 0x04, 0x7F }, { 0x04, 0x8C }, { 0x04, 0x99 },
	{ 0x04, 0xA6 }, { 0x04, 0xB3 }, { 0x04, 0xC0 }, { 0x04, 0xCD }, { 0x04, 0xDB },
	{ 0x04, 0xE8 }, { 0x04, 0xF5 }, { 0x05, 0x02 }, { 0x05, 0x0F }, { 0x05, 0x1C },
	{ 0x05, 0x29 }, { 0x05, 0x36 }, { 0x05, 0x43 }, { 0x05, 0x50 }, { 0x05, 0x5D },
	{ 0x05, 0x6A }, { 0x05, 0x77 }, { 0x05, 0x84 }, { 0x05, 0x91 }, { 0x05, 0x9E },
	{ 0x05, 0xAB }, { 0x05, 0xB8 }, { 0x05, 0xC5 }, { 0x05, 0xD2 }, { 0x05, 0xDF },
	{ 0x05, 0xEC }, { 0x05, 0xF9 }, { 0x06, 0x06 }, { 0x06, 0x13 }, { 0x06, 0x20 },
	{ 0x06, 0x2D }, { 0x06, 0x3A }, { 0x06, 0x47 }, { 0x06, 0x54 }, { 0x06, 0x61 },
	{ 0x06, 0x6E }, { 0x06, 0x7B }, { 0x06, 0x88 }, { 0x06, 0x95 }, { 0x06, 0xA2 },
	{ 0x06, 0xAF }, { 0x06, 0xBC }, { 0x06, 0xC9 }, { 0x06, 0xD6 }, { 0x06, 0xE3 },
	{ 0x06, 0xF0 }, { 0x06, 0xFD }, { 0x07, 0x0A }, { 0x07, 0x17 }, { 0x07, 0x24 },
	{ 0x07, 0x31 }, { 0x07, 0x3E }, { 0x07, 0x4B }, { 0x07, 0x58 }, { 0x07, 0x65 },
	{ 0x07, 0x72 }, { 0x07, 0x7F }, { 0x07, 0x8C }, { 0x07, 0x99 }, { 0x07, 0xA6 },
	{ 0x07, 0xB3 }, { 0x07, 0xC0 }, { 0x07, 0xCD }, { 0x07, 0xDA }, { 0x07, 0xE7 },
	{ 0x07, 0xF4 }, { 0x08, 0x01 }, { 0x08, 0x0E }, { 0x08, 0x1B }, { 0x08, 0x28 },
	{ 0x08, 0x35 }, { 0x08, 0x42 }, { 0x08, 0x4F }, { 0x08, 0x5C }, { 0x08, 0x69 },
	{ 0x08, 0x76 }, { 0x08, 0x83 }, { 0x08, 0x90 }, { 0x08, 0x9D }, { 0x08, 0xAB },
	{ 0x08, 0xB8 }, { 0x08, 0xC5 }, { 0x08, 0xD2 }, { 0x08, 0xDF }, { 0x08, 0xEC },
	{ 0x08, 0xF9 }, { 0x09, 0x06 }, { 0x09, 0x13 }, { 0x09, 0x20 }, { 0x09, 0x2D },
	{ 0x09, 0x3A }, { 0x09, 0x47 }, { 0x09, 0x54 }, { 0x09, 0x61 }, { 0x09, 0x6E },
	{ 0x09, 0x7B }, { 0x09, 0x88 }, { 0x09, 0x95 }, { 0x09, 0xA2 }, { 0x09, 0xAF },
	{ 0x09, 0xBC }, { 0x09, 0xC9 }, { 0x09, 0xD6 }, { 0x09, 0xE3 }, { 0x09, 0xF0 },
	{ 0x09, 0xFD }, { 0x0A, 0x0A }, { 0x0A, 0x17 }, { 0x0A, 0x24 }, { 0x0A, 0x31 },
	{ 0x0A, 0x3E }, { 0x0A, 0x4B }, { 0x0A, 0x58 }, { 0x0A, 0x65 }, { 0x0A, 0x72 },
	{ 0x0A, 0x7F }, { 0x0A, 0x8C }, { 0x0A, 0x99 }, { 0x0A, 0xA6 }, { 0x0A, 0xB3 },
	{ 0x0A, 0xC0 }, { 0x0A, 0xCD }, { 0x0A, 0xDA }, { 0x0A, 0xE7 }, { 0x0A, 0xF4 },
	{ 0x0B, 0x01 }, { 0x0B, 0x0E }, { 0x0B, 0x1B }, { 0x0B, 0x28 }, { 0x0B, 0x35 },
	{ 0x0B, 0x42 }, { 0x0B, 0x4F }, { 0x0B, 0x5C }, { 0x0B, 0x69 }, { 0x0B, 0x76 },
	{ 0x0B, 0x83 }, { 0x0B, 0x90 }, { 0x0B, 0x9D }, { 0x0B, 0xAA }, { 0x0B, 0xB7 },
	{ 0x0B, 0xC4 }, { 0x0B, 0xD1 }, { 0x0B, 0xDE }, { 0x0B, 0xEB }, { 0x0B, 0xF8 },
	{ 0x0C, 0x05 }, { 0x0C, 0x12 }, { 0x0C, 0x1F }, { 0x0C, 0x2C }, { 0x0C, 0x39 },
	{ 0x0C, 0x46 }, { 0x0C, 0x53 }, { 0x0C, 0x60 }, { 0x0C, 0x6D }, { 0x0C, 0x7B },
	{ 0x0C, 0x88 }, { 0x0C, 0x95 }, { 0x0C, 0xA2 }, { 0x0C, 0xAF }, { 0x0C, 0xBC },
	{ 0x0C, 0xC9 }, { 0x0C, 0xD6 }, { 0x0C, 0xE3 }, { 0x0C, 0xF0 }, { 0x0C, 0xFD },
	// HBM(platfrom 256 ~ 307)
	{ 0x0D, 0x0A }, { 0x0D, 0x17 }, { 0x0D, 0x24 }, { 0x0D, 0x31 }, { 0x0D, 0x3E },
	{ 0x0D, 0x4B }, { 0x0D, 0x58 }, { 0x0D, 0x65 }, { 0x0D, 0x72 }, { 0x0D, 0x7F },
	{ 0x0D, 0x8C }, { 0x0D, 0x99 }, { 0x0D, 0xA6 }, { 0x0D, 0xB3 }, { 0x0D, 0xC0 },
	{ 0x0D, 0xCD }, { 0x0D, 0xDA }, { 0x0D, 0xE7 }, { 0x0D, 0xF4 }, { 0x0E, 0x01 },
	{ 0x0E, 0x0E }, { 0x0E, 0x1B }, { 0x0E, 0x28 }, { 0x0E, 0x35 }, { 0x0E, 0x42 },
	{ 0x0E, 0x4F }, { 0x0E, 0x5C }, { 0x0E, 0x69 }, { 0x0E, 0x76 }, { 0x0E, 0x83 },
	{ 0x0E, 0x90 }, { 0x0E, 0x9D }, { 0x0E, 0xAA }, { 0x0E, 0xB7 }, { 0x0E, 0xC4 },
	{ 0x0E, 0xD1 }, { 0x0E, 0xDE }, { 0x0E, 0xEB }, { 0x0E, 0xF8 }, { 0x0F, 0x05 },
	{ 0x0F, 0x12 }, { 0x0F, 0x1F }, { 0x0F, 0x2C }, { 0x0F, 0x39 }, { 0x0F, 0x46 },
	{ 0x0F, 0x53 }, { 0x0F, 0x60 }, { 0x0F, 0x6D }, { 0x0F, 0x7A }, { 0x0F, 0x87 },
	{ 0x0F, 0x94 }, { 0x0F, 0x99},
};

static struct maptbl hx83122a_gts9fep_boe_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(hx83122a_gts9fep_boe_brt_table,
			&TFT_FUNC(TFT_MAPTBL_INIT_BRT),
			&TFT_FUNC(TFT_MAPTBL_GETIDX_BRT),
			&TFT_FUNC(TFT_MAPTBL_COPY_DEFAULT)),
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_SLEEP_OUT[] = {
	0x11
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_SLEEP_IN[] = {
	0x10
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_DISPLAY_ON[] = {
	0x29
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_DISPLAY_OFF[] = {
	0x28
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_BRIGHTNESS[] = {
	0x51,
	0x64, 0x0B,
};

/* < CABC Mode control Function > */

static u8 SEQ_HX83122A_GTS9FEP_BOE_BRIGHTNESS_ON[] = {
	0x53,
	0x24,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_UI_MODE[] = {
	0x55,
	0x01,
};

/* Display config (1) */
static u8 SEQ_HX83122A_GTS9FEP_BOE_001[] = {
	0xB9,
	0x83, 0x12, 0x2A, 0x55, 0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_002[] = {
	0x51,
	0x00, 0x00,
};
/*
static u8 SEQ_HX83122A_GTS9FEP_BOE_003[] = {
	0x53,
	0x24,
};
*/
static u8 SEQ_HX83122A_GTS9FEP_BOE_004[] = {
	0xB1,
	0x10, 0x21, 0x21, 0x1D, 0xDD, 0x42, 0x07, 0x1B, 0x23, 0x23,
	0x00, 0x00, 0x15, 0x33, 0xE1, 0x40, 0xCD, 0x03, 0x1A, 0x05,
	0x15, 0x98, 0x00, 0x88, 0xC4, 0xCC, 0xCC, 0xCC, 0x0F, 0x88,
	0x12, 0x00, 0x00, 0xF0, 0x00, 0x00, 0xFF,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_005[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_006[] = {
	0xB1,
	0x01, 0xA3, 0x00, 0x30,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_007[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_008[] = {
	0xB2,
	0x00, 0x6A, 0x40, 0x01, 0x00, 0x14, 0x60, 0x00, 0x4E, 0x00,
	0x00, 0x11, 0x00, 0x00, 0x00, 0x15, 0x27,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_009[] = {
	0xB4,
	0x00, 0x67, 0x00, 0x67, 0x00, 0x67, 0x0A, 0x59, 0x0A, 0x59,
	0x0A, 0x59, 0x00, 0x5B, 0x00, 0x00, 0x00, 0x13, 0x00, 0x27,
	0x07, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x10, 0x00, 0x02,
	0x2B, 0x2B, 0x2B, 0x2B,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_010[] = {
	0xE9,
	0xCD,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_011[] = {
	0xBA,
	0x84, 0x01, 0x03,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_012[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_013[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_014[] = {
	0xE9,
	0xC5,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_015[] = {
	0xBA,
	0x6F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_016[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_017[] = {
	0xBD,
	0x02,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_018[] = {
	0xBA,
	0x70, 0x03, 0xA8, 0xB2,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_019[] = {
	0xBD,
	0x03,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_020[] = {
	0xBA,
	0x00, 0x00, 0x02, 0xC0, 0x6F, 0xEF,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_021[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_022[] = {
	0xBC,
	0x34, 0x02, 0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_023[] = {
	0xBF,
	0x00, 0x41, 0x80, 0x1C, 0x36, 0x00, 0x85,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_024[] = {
	0xBD,
	0x02,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_025[] = {
	0xBF,
	0x70,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_026[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_027[] = {
	0xC0,
	0x09, 0x09, 0x44, 0x22, 0x08, 0xD8,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_028[] = {
	0xE9,
	0xD2,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_029[] = {
	0xC0,
	0xFF,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_030[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_031[] = {
	0xCB,
	0x00, 0x13, 0x38, 0x00, 0x07, 0x62,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_032[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_033[] = {
	0xCB,
	0x13, 0x55, 0x03, 0x28, 0x1C, 0x08, 0x1E,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_034[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_035[] = {
	0xCC,
	0x02, 0x03, 0x4C,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_036[] = {
	0xCE,
	0x00, 0x8A,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_037[] = {
	0xD0,
	0x07, 0xC0, 0x18, 0x48, 0x11, 0x08,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_038[] = {
	0xD1,
	0x07, 0x03, 0x0C, 0xFD,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_039[] = {
	0xE9,
	0xC9,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_040[] = {
	0xD1,
	0x02,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_041[] = {
	0xE9,
	0xC9,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_042[] = {
	0xC7,
	0x80,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_043[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_044[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_045[] = {
	0xD2,
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x2F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_046[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_047[] = {
	0xD3,
	0x00, 0xC0, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x00, 0x00,
	0x03, 0x03, 0x03, 0x11, 0x11, 0x0C, 0x0C, 0x12, 0x12, 0x03,
	0x03, 0x32, 0x10, 0x13, 0x00, 0x13, 0x32, 0x10, 0x03, 0x00,
	0x25, 0x32, 0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0xFF, 0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_048[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_049[] = {
	0xE9,
	0xC9,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_050[] = {
	0xD3,
	0x04,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_051[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_052[] = {
	0xBD,
	0x02,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_053[] = {
	0xD3,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x01,
	0x11, 0x00, 0x00, 0x00, 0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_054[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_055[] = {
	0xD5,
	0x01, 0x01, 0x00, 0x00, 0x03, 0x03, 0x02, 0x02, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x18, 0x18, 0x41, 0x41, 0x41, 0x41,
	0x40, 0x40, 0x40, 0x40, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xBE, 0xBE,
	0x21, 0x21, 0x20, 0x20, 0x18, 0x18, 0x18, 0x18,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_056[] = {
	0xD6,
	0x40, 0x40, 0x18, 0x18, 0x05, 0x05, 0x04, 0x04, 0x03, 0x03,
	0x02, 0x02, 0x01, 0x01, 0x00, 0x00, 0x07, 0x07, 0x06, 0x06,
	0x18, 0x18, 0x19, 0x19, 0x18, 0x18, 0x18, 0x18, 0x20, 0x20,
	0x20, 0x20, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x40, 0x40, 0x18, 0x18, 0x41, 0x41, 0x41, 0x41,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_057[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_058[] = {
	0xD6,
	0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_059[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_060[] = {
	0xD8,
	0xFF, 0xFA, 0xAA, 0xAA, 0xAB, 0xAA, 0xFF, 0xFA, 0xAA, 0xAA,
	0xAB, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_061[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_062[] = {
	0xD8,
	0xFF, 0xFA, 0xFF, 0xAA, 0xFF, 0xFA, 0xFF, 0xFA, 0xFF, 0xAA,
	0xFF, 0xFA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_063[] = {
	0xBD,
	0x02,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_064[] = {
	0xD8,
	0xFF, 0xFE, 0xFF, 0xAA, 0xFF, 0xFA, 0xFF, 0xFE, 0xFF, 0xAA,
	0xFF, 0xFA,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_065[] = {
	0xBD,
	0x03,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_066[] = {
	0xD8,
	0xAA, 0xAE, 0xFF, 0xAA, 0xEA, 0xAA, 0xAA, 0xAE, 0xFF, 0xAA,
	0xEA, 0xAA, 0xFF, 0xFE, 0xFF, 0xAF, 0xFF, 0xFA, 0xFF, 0xFE,
	0xFF, 0xAF, 0xFF, 0xFA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_067[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_068[] = {
	0xE0,
	0x00, 0x09, 0x1B, 0x1A, 0x1D, 0x52, 0x61, 0x69, 0x6B, 0x6E,
	0x6E, 0x70, 0x70, 0x70, 0x74, 0x74, 0x75, 0x7C, 0x7F, 0x9A,
	0xAE, 0x47, 0x67, 0x00, 0x09, 0x1B, 0x1A, 0x1D, 0x52, 0x61,
	0x69, 0x6B, 0x6E, 0x6E, 0x70, 0x70, 0x70, 0x74, 0x74, 0x75,
	0x7C, 0x7F, 0x9A, 0xAE, 0x47, 0x67,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_069[] = {
	0xE1,
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0A, 0x00, 0x03, 0x20,
	0x00, 0x14, 0x03, 0x20, 0x03, 0x20, 0x02, 0x00, 0x02, 0x91,
	0x00, 0x20, 0x02, 0x47, 0x00, 0x0B, 0x00, 0x0C, 0x05, 0x0E,
	0x03, 0x68, 0x18, 0x00, 0x10, 0xE0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46, 0x54,
	0x62, 0x69, 0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01, 0x02,
	0x01, 0x00, 0x09,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_070[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_071[] = {
	0xE1,
	0x40, 0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8, 0x1A,
	0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6, 0x2B, 0x34, 0x2B,
	0x74, 0x3B, 0x74, 0x6B, 0xF4,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_072[] = {
	0xBD,
	0x03,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_073[] = {
	0xE1,
	0x01, 0x3F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_074[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_075[] = {
	0xE7,
	0x1E, 0x00, 0x00, 0x5E, 0x58, 0x32, 0x04, 0x21, 0x50, 0x02,
	0x02,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_076[] = {
	0xE9,
	0xD6,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_077[] = {
	0xE7,
	0x03, 0x88,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_078[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_079[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_080[] = {
	0xE7,
	0x02, 0x00, 0x10, 0x01, 0x48, 0x07, 0x54, 0x08, 0x15, 0x0A,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_081[] = {
	0xBD,
	0x02,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_082[] = {
	0xE7,
	0x01, 0x05, 0x01, 0x05, 0x01, 0x05, 0x04, 0x04, 0x04, 0x24,
	0x00, 0x24, 0x81, 0x02, 0x40, 0x10, 0x20, 0x5B, 0x03, 0x02,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_083[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_084[] = {
	0xBB,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_085[] = {
	0xE9,
	0xD0,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_086[] = {
	0xBB,
	0x38,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_087[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_088[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_089[] = {
	0xC1,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_090[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_091[] = {
	0xC1,
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24,
	0x27, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4B,
	0x54, 0x5B, 0x63, 0x6B, 0x73, 0x7B, 0x83, 0x8B, 0x93, 0x9B,
	0xA3, 0xAB, 0xB3, 0xBB, 0xC3, 0xCB, 0xD3, 0xDB, 0xE3, 0xEC,
	0xF5, 0xF9, 0xFB, 0xFB, 0xFD, 0xFF, 0x15, 0x5F, 0xD8, 0x5B,
	0xE7, 0x35, 0xEC, 0x84, 0x85, 0xA9, 0x02, 0x80,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_092[] = {
	0xBD,
	0x02,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_093[] = {
	0xC1,
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24,
	0x28, 0x2D, 0x31, 0x35, 0x39, 0x3D, 0x41, 0x45, 0x49, 0x4C,
	0x55, 0x5C, 0x64, 0x6D, 0x75, 0x7D, 0x84, 0x8C, 0x94, 0x9C,
	0xA4, 0xAC, 0xB4, 0xBC, 0xC4, 0xCC, 0xD4, 0xDC, 0xE4, 0xEC,
	0xF3, 0xF7, 0xF9, 0xFC, 0xFE, 0xFF, 0x05, 0x5A, 0xFC, 0x06,
	0x93, 0x3C, 0x1F, 0xB6, 0xC3, 0x99, 0xFD, 0x40,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_094[] = {
	0xBD,
	0x03,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_095[] = {
	0xC1,
	0x00, 0x04, 0x07, 0x0B, 0x0F, 0x13, 0x17, 0x1B, 0x20, 0x24,
	0x27, 0x2C, 0x30, 0x33, 0x38, 0x3C, 0x40, 0x44, 0x47, 0x4B,
	0x53, 0x5B, 0x62, 0x6A, 0x73, 0x7B, 0x82, 0x8A, 0x92, 0x9A,
	0xA2, 0xAA, 0xB2, 0xBA, 0xC2, 0xCA, 0xD2, 0xDA, 0xE2, 0xEB,
	0xF3, 0xF7, 0xF9, 0xFC, 0xFD, 0xFF, 0x0F, 0xAF, 0x0C, 0x31,
	0x4D, 0x8F, 0x0D, 0x50, 0x91, 0x9D, 0x9C, 0x80,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_096[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_097[] = {
	0x55,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_098[] = {
	0xBD,
	0x01,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_099[] = {
	0xE9,
	0xD7,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_100[] = {
	0xE4,
	0x18, 0x12, 0x0C, 0x08, 0x06, 0x03, 0x02, 0x01, 0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_101[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_102[] = {
	0xBD,
	0x03,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_103[] = {
	0xE4,
	0xCC, 0xF2, 0x18, 0x3F, 0x65, 0x8B, 0xB2, 0xD8, 0xFF, 0xFA,
	0xFF, 0x03,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_104[] = {
	0xBD,
	0x00,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_105[] = {
	0xE4,
	0x2D, 0x03, 0x81,
};

static u8 SEQ_HX83122A_GTS9FEP_BOE_106[] = {
	0xB9,
	0x00, 0x00, 0x00, 0x00, 0x00,
};



static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_sleep_out, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_sleep_in, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_display_on, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_display_off, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_brightness_on, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_BRIGHTNESS_ON, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_ui_mode, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_UI_MODE, 0);

static DEFINE_PKTUI(hx83122a_gts9fep_boe_brightness, &hx83122a_gts9fep_boe_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(hx83122a_gts9fep_boe_brightness, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_001, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_001, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_002, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_002, 0);
//static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_003, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_003, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_004, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_004, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_005, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_005, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_006, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_006, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_007, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_007, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_008, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_008, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_009, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_009, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_010, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_010, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_011, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_011, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_012, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_012, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_013, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_013, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_014, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_014, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_015, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_015, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_016, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_016, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_017, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_017, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_018, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_018, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_019, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_019, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_020, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_020, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_021, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_021, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_022, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_022, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_023, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_023, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_024, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_024, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_025, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_025, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_026, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_026, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_027, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_027, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_028, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_028, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_029, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_029, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_030, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_030, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_031, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_031, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_032, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_032, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_033, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_033, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_034, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_034, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_035, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_035, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_036, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_036, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_037, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_037, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_038, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_038, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_039, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_039, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_040, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_040, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_041, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_041, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_042, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_042, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_043, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_043, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_044, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_044, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_045, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_045, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_046, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_046, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_047, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_047, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_048, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_048, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_049, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_049, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_050, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_050, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_051, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_051, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_052, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_052, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_053, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_053, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_054, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_054, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_055, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_055, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_056, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_056, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_057, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_057, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_058, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_058, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_059, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_059, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_060, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_060, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_061, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_061, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_062, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_062, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_063, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_063, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_064, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_064, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_065, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_065, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_066, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_066, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_067, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_067, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_068, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_068, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_069, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_069, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_070, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_070, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_071, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_071, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_072, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_072, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_073, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_073, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_074, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_074, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_075, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_075, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_076, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_076, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_077, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_077, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_078, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_078, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_079, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_079, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_080, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_080, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_081, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_081, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_082, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_082, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_083, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_083, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_084, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_084, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_085, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_085, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_086, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_086, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_087, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_087, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_088, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_088, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_089, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_089, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_090, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_090, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_091, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_091, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_092, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_092, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_093, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_093, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_094, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_094, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_095, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_095, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_096, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_096, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_097, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_097, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_098, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_098, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_099, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_099, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_100, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_100, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_101, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_101, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_102, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_102, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_103, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_103, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_104, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_104, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_105, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_105, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_106, DSI_PKT_TYPE_WR, SEQ_HX83122A_GTS9FEP_BOE_106, 0);

static DEFINE_PANEL_MDELAY(hx83122a_gts9fep_boe_wait_20msec, 20); /* 1 frame */
static DEFINE_PANEL_MDELAY(hx83122a_gts9fep_boe_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(hx83122a_gts9fep_boe_wait_120msec, 120);

static DEFINE_PNOBJ_CONFIG(hx83122a_gts9fep_boe_set_wait_tx_done_property_on, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_ON);
static DEFINE_PNOBJ_CONFIG(hx83122a_gts9fep_boe_set_wait_tx_done_property_off, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
static DEFINE_PNOBJ_CONFIG(hx83122a_gts9fep_boe_set_wait_tx_done_property_auto, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

static u8 HX83122A_GTS9FEP_BOE_ID[TFT_COMMON_ID_LEN];
static DEFINE_RDINFO(hx83122a_gts9fep_boe_id1, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DA_OFS, TFT_COMMON_ID_DA_LEN);
static DEFINE_RDINFO(hx83122a_gts9fep_boe_id2, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DB_OFS, TFT_COMMON_ID_DB_LEN);
static DEFINE_RDINFO(hx83122a_gts9fep_boe_id3, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DC_OFS, TFT_COMMON_ID_DC_LEN);
static DECLARE_RESUI(hx83122a_gts9fep_boe_id) = {
	{ .rditbl = &RDINFO(hx83122a_gts9fep_boe_id1), .offset = 0 },
	{ .rditbl = &RDINFO(hx83122a_gts9fep_boe_id2), .offset = 1 },
	{ .rditbl = &RDINFO(hx83122a_gts9fep_boe_id3), .offset = 2 },
};
static DEFINE_RESOURCE(hx83122a_gts9fep_boe_id, HX83122A_GTS9FEP_BOE_ID, RESUI(hx83122a_gts9fep_boe_id));

static void *hx83122a_gts9fep_boe_init_cmdtbl[] = {
	&PNOBJ_CONFIG(hx83122a_gts9fep_boe_set_wait_tx_done_property_on),
	&PKTINFO(hx83122a_gts9fep_boe_001),
	&PKTINFO(hx83122a_gts9fep_boe_002),
//	&PKTINFO(hx83122a_gts9fep_boe_003),
	&PKTINFO(hx83122a_gts9fep_boe_004),
	&PKTINFO(hx83122a_gts9fep_boe_005),
	&PKTINFO(hx83122a_gts9fep_boe_006),
	&PKTINFO(hx83122a_gts9fep_boe_007),
	&PKTINFO(hx83122a_gts9fep_boe_008),
	&PKTINFO(hx83122a_gts9fep_boe_009),
	&PKTINFO(hx83122a_gts9fep_boe_010),
	&PKTINFO(hx83122a_gts9fep_boe_011),
	&PKTINFO(hx83122a_gts9fep_boe_012),
	&PKTINFO(hx83122a_gts9fep_boe_013),
	&PKTINFO(hx83122a_gts9fep_boe_014),
	&PKTINFO(hx83122a_gts9fep_boe_015),
	&PKTINFO(hx83122a_gts9fep_boe_016),
	&PKTINFO(hx83122a_gts9fep_boe_017),
	&PKTINFO(hx83122a_gts9fep_boe_018),
	&PKTINFO(hx83122a_gts9fep_boe_019),
	&PKTINFO(hx83122a_gts9fep_boe_020),
	&PKTINFO(hx83122a_gts9fep_boe_021),
	&PKTINFO(hx83122a_gts9fep_boe_022),
	&PKTINFO(hx83122a_gts9fep_boe_023),
	&PKTINFO(hx83122a_gts9fep_boe_024),
	&PKTINFO(hx83122a_gts9fep_boe_025),
	&PKTINFO(hx83122a_gts9fep_boe_026),
	&PKTINFO(hx83122a_gts9fep_boe_027),
	&PKTINFO(hx83122a_gts9fep_boe_028),
	&PKTINFO(hx83122a_gts9fep_boe_029),
	&PKTINFO(hx83122a_gts9fep_boe_030),
	&PKTINFO(hx83122a_gts9fep_boe_031),
	&PKTINFO(hx83122a_gts9fep_boe_032),
	&PKTINFO(hx83122a_gts9fep_boe_033),
	&PKTINFO(hx83122a_gts9fep_boe_034),
	&PKTINFO(hx83122a_gts9fep_boe_035),
	&PKTINFO(hx83122a_gts9fep_boe_036),
	&PKTINFO(hx83122a_gts9fep_boe_037),
	&PKTINFO(hx83122a_gts9fep_boe_038),
	&PKTINFO(hx83122a_gts9fep_boe_039),
	&PKTINFO(hx83122a_gts9fep_boe_040),
	&PKTINFO(hx83122a_gts9fep_boe_041),
	&PKTINFO(hx83122a_gts9fep_boe_042),
	&PKTINFO(hx83122a_gts9fep_boe_043),
	&PKTINFO(hx83122a_gts9fep_boe_044),
	&PKTINFO(hx83122a_gts9fep_boe_045),
	&PKTINFO(hx83122a_gts9fep_boe_046),
	&PKTINFO(hx83122a_gts9fep_boe_047),
	&PKTINFO(hx83122a_gts9fep_boe_048),
	&PKTINFO(hx83122a_gts9fep_boe_049),
	&PKTINFO(hx83122a_gts9fep_boe_050),
	&PKTINFO(hx83122a_gts9fep_boe_051),
	&PKTINFO(hx83122a_gts9fep_boe_052),
	&PKTINFO(hx83122a_gts9fep_boe_053),
	&PKTINFO(hx83122a_gts9fep_boe_054),
	&PKTINFO(hx83122a_gts9fep_boe_055),
	&PKTINFO(hx83122a_gts9fep_boe_056),
	&PKTINFO(hx83122a_gts9fep_boe_057),
	&PKTINFO(hx83122a_gts9fep_boe_058),
	&PKTINFO(hx83122a_gts9fep_boe_059),
	&PKTINFO(hx83122a_gts9fep_boe_060),
	&PKTINFO(hx83122a_gts9fep_boe_061),
	&PKTINFO(hx83122a_gts9fep_boe_062),
	&PKTINFO(hx83122a_gts9fep_boe_063),
	&PKTINFO(hx83122a_gts9fep_boe_064),
	&PKTINFO(hx83122a_gts9fep_boe_065),
	&PKTINFO(hx83122a_gts9fep_boe_066),
	&PKTINFO(hx83122a_gts9fep_boe_067),
	&PKTINFO(hx83122a_gts9fep_boe_068),
	&PKTINFO(hx83122a_gts9fep_boe_069),
	&PKTINFO(hx83122a_gts9fep_boe_070),
	&PKTINFO(hx83122a_gts9fep_boe_071),
	&PKTINFO(hx83122a_gts9fep_boe_072),
	&PKTINFO(hx83122a_gts9fep_boe_073),
	&PKTINFO(hx83122a_gts9fep_boe_074),
	&PKTINFO(hx83122a_gts9fep_boe_075),
	&PKTINFO(hx83122a_gts9fep_boe_076),
	&PKTINFO(hx83122a_gts9fep_boe_077),
	&PKTINFO(hx83122a_gts9fep_boe_078),
	&PKTINFO(hx83122a_gts9fep_boe_079),
	&PKTINFO(hx83122a_gts9fep_boe_080),
	&PKTINFO(hx83122a_gts9fep_boe_081),
	&PKTINFO(hx83122a_gts9fep_boe_082),
	&PKTINFO(hx83122a_gts9fep_boe_083),
	&PKTINFO(hx83122a_gts9fep_boe_084),
	&PKTINFO(hx83122a_gts9fep_boe_085),
	&PKTINFO(hx83122a_gts9fep_boe_086),
	&PKTINFO(hx83122a_gts9fep_boe_087),
	&PKTINFO(hx83122a_gts9fep_boe_088),
	&PKTINFO(hx83122a_gts9fep_boe_089),
	&PKTINFO(hx83122a_gts9fep_boe_090),
	&PKTINFO(hx83122a_gts9fep_boe_091),
	&PKTINFO(hx83122a_gts9fep_boe_092),
	&PKTINFO(hx83122a_gts9fep_boe_093),
	&PKTINFO(hx83122a_gts9fep_boe_094),
	&PKTINFO(hx83122a_gts9fep_boe_095),
	&PKTINFO(hx83122a_gts9fep_boe_096),
	&PKTINFO(hx83122a_gts9fep_boe_097),
	&PKTINFO(hx83122a_gts9fep_boe_098),
	&PKTINFO(hx83122a_gts9fep_boe_099),
	&PKTINFO(hx83122a_gts9fep_boe_100),
	&PKTINFO(hx83122a_gts9fep_boe_101),
	&PKTINFO(hx83122a_gts9fep_boe_102),
	&PKTINFO(hx83122a_gts9fep_boe_103),
	&PKTINFO(hx83122a_gts9fep_boe_104),
	&PKTINFO(hx83122a_gts9fep_boe_105),
	&PKTINFO(hx83122a_gts9fep_boe_106),
	&PKTINFO(hx83122a_gts9fep_boe_sleep_out),
	&DLYINFO(hx83122a_gts9fep_boe_wait_120msec),
	&PKTINFO(hx83122a_gts9fep_boe_display_on),
	&PNOBJ_CONFIG(hx83122a_gts9fep_boe_set_wait_tx_done_property_auto),
};

static void *hx83122a_gts9fep_boe_res_init_cmdtbl[] = {
	&RESINFO(hx83122a_gts9fep_boe_id),
};

static void *hx83122a_gts9fep_boe_set_bl_cmdtbl[] = {
	&PKTINFO(hx83122a_gts9fep_boe_brightness),
};

static void *hx83122a_gts9fep_boe_display_on_cmdtbl[] = {
	&DLYINFO(hx83122a_gts9fep_boe_wait_40msec),
	&PKTINFO(hx83122a_gts9fep_boe_brightness),
	&PKTINFO(hx83122a_gts9fep_boe_brightness_on),
};

static void *hx83122a_gts9fep_boe_display_off_cmdtbl[] = {
	&PKTINFO(hx83122a_gts9fep_boe_display_off),
};

static void *hx83122a_gts9fep_boe_exit_cmdtbl[] = {
	&PKTINFO(hx83122a_gts9fep_boe_sleep_in),
};

static void *hx83122a_gts9fep_boe_display_mode_cmdtbl[] = {
	&PNOBJ_CONFIG(hx83122a_gts9fep_boe_set_wait_tx_done_property_off),
	&PKTINFO(hx83122a_gts9fep_boe_brightness),
		/* Will flush on next VFP */
	&PNOBJ_CONFIG(hx83122a_gts9fep_boe_set_wait_tx_done_property_auto),
};

static struct seqinfo hx83122a_gts9fep_boe_seqtbl[] = {
	SEQINFO_INIT(PANEL_INIT_SEQ, hx83122a_gts9fep_boe_init_cmdtbl),
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, hx83122a_gts9fep_boe_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_SET_BL_SEQ, hx83122a_gts9fep_boe_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, hx83122a_gts9fep_boe_display_mode_cmdtbl), /* Dummy */
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, hx83122a_gts9fep_boe_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, hx83122a_gts9fep_boe_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, hx83122a_gts9fep_boe_exit_cmdtbl),
};

/* BLIC 1 SETTING START */
static u8 HX83122A_GTS9FEP_BOE_ISL98608_I2C_INIT[] = {
	0x06, 0x0D,
	0x08, 0x08,
	0x09, 0x08,
};

#ifdef DEBUG_I2C_READ
static u8 HX83122A_GTS9FEP_BOE_ISL98608_I2C_DUMP[] = {
	0x06, 0x00,
	0x08, 0x00,
	0x09, 0x00,
};
#endif

static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_isl98608_i2c_init, I2C_PKT_TYPE_WR, HX83122A_GTS9FEP_BOE_ISL98608_I2C_INIT, 0);
#ifdef DEBUG_I2C_READ
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_isl98608_i2c_dump, I2C_PKT_TYPE_RD, HX83122A_GTS9FEP_BOE_ISL98608_I2C_DUMP, 0);
#endif

static void *hx83122a_gts9fep_boe_isl98608_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83122a_gts9fep_boe_isl98608_i2c_dump),
#endif
	&PKTINFO(hx83122a_gts9fep_boe_isl98608_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83122a_gts9fep_boe_isl98608_i2c_dump),
#endif
};

static void *hx83122a_gts9fep_boe_isl98608_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83122a_gts9fep_boe_isl98608_i2c_dump),
#endif
};

static struct seqinfo hx83122a_gts9fep_boe_isl98608_seq_tbl[] = {
	SEQINFO_INIT(PANEL_BLIC_I2C_ON_SEQ, hx83122a_gts9fep_boe_isl98608_init_cmdtbl),
	SEQINFO_INIT(PANEL_BLIC_I2C_OFF_SEQ, hx83122a_gts9fep_boe_isl98608_exit_cmdtbl),
};

static struct blic_data hx83122a_gts9fep_boe_isl98608_blic_data = {
	.name = "isl98608",
	.seqtbl = hx83122a_gts9fep_boe_isl98608_seq_tbl,
	.nr_seqtbl = ARRAY_SIZE(hx83122a_gts9fep_boe_isl98608_seq_tbl),
};

/* BLIC 2 SETTING START */
static u8 HX83122A_GTS9FEP_BOE_MAX77816_I2C_INIT[] = {
	0x03, 0x30,
	0x02, 0x8E,
	0x03, 0x70,
	0x04, 0x78,
};

static u8 HX83122A_GTS9FEP_BOE_MAX77816_I2C_EXIT_BLEN[] = {
	0x03, 0x32,
};

#ifdef DEBUG_I2C_READ
static u8 HX83122A_GTS9FEP_BOE_MAX77816_I2C_DUMP[] = {
	0x04, 0x00,
	0x03, 0x00,
};
#endif

static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_max77816_i2c_init, I2C_PKT_TYPE_WR, HX83122A_GTS9FEP_BOE_MAX77816_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_max77816_i2c_exit_blen, I2C_PKT_TYPE_WR, HX83122A_GTS9FEP_BOE_MAX77816_I2C_EXIT_BLEN, 0);
#ifdef DEBUG_I2C_READ
static DEFINE_STATIC_PACKET(hx83122a_gts9fep_boe_max77816_i2c_dump, I2C_PKT_TYPE_RD, HX83122A_GTS9FEP_BOE_MAX77816_I2C_DUMP, 0);
#endif

static void *hx83122a_gts9fep_boe_max77816_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83122a_gts9fep_boe_max77816_i2c_dump),
#endif
	&PKTINFO(hx83122a_gts9fep_boe_max77816_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83122a_gts9fep_boe_max77816_i2c_dump),
#endif
};

static void *hx83122a_gts9fep_boe_max77816_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(hx83122a_gts9fep_boe_max77816_i2c_dump),
#endif
	&PKTINFO(hx83122a_gts9fep_boe_max77816_i2c_exit_blen),
};

static struct seqinfo hx83122a_gts9fep_boe_max77816_seq_tbl[] = {
	SEQINFO_INIT(PANEL_BLIC_I2C_ON_SEQ, hx83122a_gts9fep_boe_max77816_init_cmdtbl),
	SEQINFO_INIT(PANEL_BLIC_I2C_OFF_SEQ, hx83122a_gts9fep_boe_max77816_exit_cmdtbl),
};

static struct blic_data hx83122a_gts9fep_boe_max77816_blic_data = {
	.name = "max77816",
	.seqtbl = hx83122a_gts9fep_boe_max77816_seq_tbl,
	.nr_seqtbl = ARRAY_SIZE(hx83122a_gts9fep_boe_max77816_seq_tbl),
};

static struct blic_data *hx83122a_gts9fep_boe_blic_tbl[] = {
	&hx83122a_gts9fep_boe_isl98608_blic_data,
	&hx83122a_gts9fep_boe_max77816_blic_data,
};
/* BLIC SETTING END */


struct common_panel_info hx83122a_gts9fep_boe_panel_info = {
	.ldi_name = "hx83122a",
	.name = "hx83122a_gts9fep_boe",
	.model = "csot_12_4_inch",
	.vendor = "BOE",
	.id = 0x4BF240,
	.rev = 0,
	.ddi_props = {
		.err_fg_recovery = false,
		.support_vrr = true,
		.init_seq_by_lpdt = true,
	},
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.common_panel_modes = &hx83122a_gts9fep_boe_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(hx83122a_gts9fep_boe_default_resol),
		.resol = hx83122a_gts9fep_boe_default_resol,
	},
	.vrrtbl = hx83122a_gts9fep_boe_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(hx83122a_gts9fep_boe_default_vrrtbl),
	.maptbl = hx83122a_gts9fep_boe_maptbl,
	.nr_maptbl = ARRAY_SIZE(hx83122a_gts9fep_boe_maptbl),
	.seqtbl = hx83122a_gts9fep_boe_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(hx83122a_gts9fep_boe_seqtbl),
	.rditbl = NULL,
	.nr_rditbl = 0,
	.restbl = NULL,
	.nr_restbl = 0,
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &hx83122a_gts9fep_boe_panel_dimming_info,
	},
	.blic_data_tbl = hx83122a_gts9fep_boe_blic_tbl,
	.nr_blic_data_tbl = ARRAY_SIZE(hx83122a_gts9fep_boe_blic_tbl),
};
#endif /* __HX83122A_GTS9FEP_BOE_PANEL_H__ */
