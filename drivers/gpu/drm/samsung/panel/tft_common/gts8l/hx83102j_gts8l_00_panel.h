/*
 * linux/drivers/video/fbdev/exynos/panel/hx83102j/hx83102j_gts8l_panel.h
 *
 * Header file for HX83102J Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HX83102J_GTS8L_00_PANEL_H__
#define __HX83102J_GTS8L_00_PANEL_H__

#include "../../panel.h"
#include "../../panel_drv.h"
#include "../tft_common.h"
#include "hx83102j_gts8l_00_resol.h"

#undef __pn_name__
#define __pn_name__	gts8l

#undef __PN_NAME__
#define __PN_NAME__

#define HX83102J_NR_STEP (256)
#define HX83102J_HBM_STEP (51)
#define HX83102J_TOTAL_STEP (HX83102J_NR_STEP + HX83102J_HBM_STEP) /* 0 ~ 306 */

static unsigned int hx83102j_gts8l_brt_tbl[HX83102J_TOTAL_STEP] = {
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
	BRT(256), BRT(257), BRT(258), BRT(259), BRT(260),
	BRT(261), BRT(262), BRT(263), BRT(264), BRT(265), BRT(266), BRT(267), BRT(268), BRT(269), BRT(270),
	BRT(271), BRT(272), BRT(273), BRT(274), BRT(275), BRT(276), BRT(277), BRT(278), BRT(279), BRT(280),
	BRT(281), BRT(282), BRT(283), BRT(284), BRT(285), BRT(286), BRT(287), BRT(288), BRT(289), BRT(290),
	BRT(291), BRT(292), BRT(293), BRT(294), BRT(295), BRT(296), BRT(297), BRT(298), BRT(299), BRT(300),
	BRT(301), BRT(302), BRT(303), BRT(304), BRT(305), BRT(306),
};

static unsigned int hx83102j_gts8l_step_cnt_tbl[HX83102J_TOTAL_STEP] = {
	[0 ... 255] = 1,
	/* HBM */
	[256 ... 306] = 1,
};

struct brightness_table hx83102j_gts8l_00_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = hx83102j_gts8l_brt_tbl,
	.sz_brt = ARRAY_SIZE(hx83102j_gts8l_brt_tbl),
	.sz_ui_brt = HX83102J_NR_STEP,
	.sz_hbm_brt = HX83102J_HBM_STEP,
	.lum = hx83102j_gts8l_brt_tbl,
	.sz_lum = ARRAY_SIZE(hx83102j_gts8l_brt_tbl),
	.sz_ui_lum = HX83102J_NR_STEP,
	.sz_hbm_lum = HX83102J_HBM_STEP,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = hx83102j_gts8l_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(hx83102j_gts8l_step_cnt_tbl),
	.vtotal = 0,
};

static struct panel_dimming_info hx83102j_gts8l_00_panel_dimming_info = {
	.name = "hx83102j_gts8l",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = -1,
	.brt_tbl = &hx83102j_gts8l_00_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,	/* read dim flash when probe or not */
	.hbm_aor = NULL,
};

static u8 brt_table[HX83102J_TOTAL_STEP][2] = {
	{0x00, 0x00},
	{0x00, 0x20},
	{0x00, 0x28},
	{0x00, 0x31},
	{0x00, 0x39},
	{0x00, 0x42},
	{0x00, 0x4A},
	{0x00, 0x53},
	{0x00, 0x5B},
	{0x00, 0x64},
	{0x00, 0x6D},
	{0x00, 0x75},
	{0x00, 0x7E},
	{0x00, 0x86},
	{0x00, 0x8F},
	{0x00, 0x97},
	{0x00, 0xA0},
	{0x00, 0xA9},
	{0x00, 0xB1},
	{0x00, 0xBA},
	{0x00, 0xC2},
	{0x00, 0xCB},
	{0x00, 0xD3},
	{0x00, 0xDC},
	{0x00, 0xE5},
	{0x00, 0xED},
	{0x00, 0xF6},
	{0x00, 0xFE},
	{0x01, 0x07},
	{0x01, 0x0F},
	{0x01, 0x18},
	{0x01, 0x21},
	{0x01, 0x29},
	{0x01, 0x32},
	{0x01, 0x3A},
	{0x01, 0x43},
	{0x01, 0x4B},
	{0x01, 0x54},
	{0x01, 0x5C},
	{0x01, 0x65},
	{0x01, 0x6E},
	{0x01, 0x76},
	{0x01, 0x7F},
	{0x01, 0x87},
	{0x01, 0x90},
	{0x01, 0x98},
	{0x01, 0xA1},
	{0x01, 0xAA},
	{0x01, 0xB2},
	{0x01, 0xBB},
	{0x01, 0xC3},
	{0x01, 0xCC},
	{0x01, 0xD4},
	{0x01, 0xDD},
	{0x01, 0xE6},
	{0x01, 0xEE},
	{0x01, 0xF7},
	{0x01, 0xFF},
	{0x02, 0x08},
	{0x02, 0x10},
	{0x02, 0x19},
	{0x02, 0x22},
	{0x02, 0x2A},
	{0x02, 0x33},
	{0x02, 0x3B},
	{0x02, 0x44},
	{0x02, 0x4C},
	{0x02, 0x55},
	{0x02, 0x5D},
	{0x02, 0x66},
	{0x02, 0x6F},
	{0x02, 0x77},
	{0x02, 0x80},
	{0x02, 0x88},
	{0x02, 0x91},
	{0x02, 0x99},
	{0x02, 0xA2},
	{0x02, 0xAB},
	{0x02, 0xB3},
	{0x02, 0xBC},
	{0x02, 0xC4},
	{0x02, 0xCD},
	{0x02, 0xD5},
	{0x02, 0xDE},
	{0x02, 0xE7},
	{0x02, 0xEF},
	{0x02, 0xF8},
	{0x03, 0x00},
	{0x03, 0x09},
	{0x03, 0x11},
	{0x03, 0x1A},
	{0x03, 0x23},
	{0x03, 0x2B},
	{0x03, 0x34},
	{0x03, 0x3C},
	{0x03, 0x45},
	{0x03, 0x4D},
	{0x03, 0x56},
	{0x03, 0x5E},
	{0x03, 0x67},
	{0x03, 0x70},
	{0x03, 0x78},
	{0x03, 0x81},
	{0x03, 0x89},
	{0x03, 0x92},
	{0x03, 0x9A},
	{0x03, 0xA3},
	{0x03, 0xAC},
	{0x03, 0xB4},
	{0x03, 0xBD},
	{0x03, 0xC5},
	{0x03, 0xCE},
	{0x03, 0xD6},
	{0x03, 0xDF},
	{0x03, 0xE8},
	{0x03, 0xF0},
	{0x03, 0xF9},
	{0x04, 0x01},
	{0x04, 0x0A},
	{0x04, 0x12},
	{0x04, 0x1B},
	{0x04, 0x24},
	{0x04, 0x2C},
	{0x04, 0x35},
	{0x04, 0x3D},
	{0x04, 0x46},
	{0x04, 0x4E},
	{0x04, 0x57},
	{0x04, 0x60},
	{0x04, 0x72},
	{0x04, 0x84},
	{0x04, 0x96},
	{0x04, 0xA8},
	{0x04, 0xBA},
	{0x04, 0xCC},
	{0x04, 0xDE},
	{0x04, 0xF1},
	{0x05, 0x03},
	{0x05, 0x15},
	{0x05, 0x27},
	{0x05, 0x39},
	{0x05, 0x4B},
	{0x05, 0x5D},
	{0x05, 0x70},
	{0x05, 0x82},
	{0x05, 0x94},
	{0x05, 0xA6},
	{0x05, 0xB8},
	{0x05, 0xCA},
	{0x05, 0xDC},
	{0x05, 0xEF},
	{0x06, 0x01},
	{0x06, 0x13},
	{0x06, 0x25},
	{0x06, 0x37},
	{0x06, 0x49},
	{0x06, 0x5B},
	{0x06, 0x6E},
	{0x06, 0x80},
	{0x06, 0x92},
	{0x06, 0xA4},
	{0x06, 0xB6},
	{0x06, 0xC8},
	{0x06, 0xDA},
	{0x06, 0xED},
	{0x06, 0xFF},
	{0x07, 0x11},
	{0x07, 0x23},
	{0x07, 0x35},
	{0x07, 0x47},
	{0x07, 0x59},
	{0x07, 0x6C},
	{0x07, 0x7E},
	{0x07, 0x90},
	{0x07, 0xA2},
	{0x07, 0xB4},
	{0x07, 0xC6},
	{0x07, 0xD8},
	{0x07, 0xEB},
	{0x07, 0xFD},
	{0x08, 0x0F},
	{0x08, 0x21},
	{0x08, 0x33},
	{0x08, 0x45},
	{0x08, 0x57},
	{0x08, 0x6A},
	{0x08, 0x7C},
	{0x08, 0x8E},
	{0x08, 0xA0},
	{0x08, 0xB2},
	{0x08, 0xC4},
	{0x08, 0xD6},
	{0x08, 0xE9},
	{0x08, 0xFB},
	{0x09, 0x0D},
	{0x09, 0x1F},
	{0x09, 0x31},
	{0x09, 0x43},
	{0x09, 0x55},
	{0x09, 0x68},
	{0x09, 0x7A},
	{0x09, 0x8C},
	{0x09, 0x9E},
	{0x09, 0xB0},
	{0x09, 0xC2},
	{0x09, 0xD4},
	{0x09, 0xE7},
	{0x09, 0xF9},
	{0x0A, 0x0B},
	{0x0A, 0x1D},
	{0x0A, 0x2F},
	{0x0A, 0x41},
	{0x0A, 0x53},
	{0x0A, 0x66},
	{0x0A, 0x78},
	{0x0A, 0x8A},
	{0x0A, 0x9C},
	{0x0A, 0xAE},
	{0x0A, 0xC0},
	{0x0A, 0xD2},
	{0x0A, 0xE5},
	{0x0A, 0xF7},
	{0x0B, 0x09},
	{0x0B, 0x1B},
	{0x0B, 0x2D},
	{0x0B, 0x3F},
	{0x0B, 0x51},
	{0x0B, 0x64},
	{0x0B, 0x76},
	{0x0B, 0x88},
	{0x0B, 0x9A},
	{0x0B, 0xAC},
	{0x0B, 0xBE},
	{0x0B, 0xD0},
	{0x0B, 0xE3},
	{0x0B, 0xF5},
	{0x0C, 0x07},
	{0x0C, 0x19},
	{0x0C, 0x2B},
	{0x0C, 0x3D},
	{0x0C, 0x4F},
	{0x0C, 0x62},
	{0x0C, 0x74},
	{0x0C, 0x86},
	{0x0C, 0x98},
	{0x0C, 0xAA},
	{0x0C, 0xBC},
	{0x0C, 0xCE},
	{0x0C, 0xE1},
	{0x0C, 0xF3},
	{0x0D, 0x05},
	{0x0D, 0x17},
	{0x0D, 0x29},
	{0x0D, 0x3B},
	{0x0D, 0x4D},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0D, 0x60},
	{0x0F, 0xFF},
};

static struct maptbl hx83102j_gts8l_00_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(brt_table, init_brt_table, getidx_brt_table, copy_common_maptbl),
};

static u8 SEQ_HX83102J_GTS8L_00_SLEEP_OUT[] = {
	0x11
};

static u8 SEQ_HX83102J_GTS8L_00_SLEEP_IN[] = {
	0x10
};

static u8 SEQ_HX83102J_GTS8L_00_DISPLAY_ON[] = {
	0x29
};

static u8 SEQ_HX83102J_GTS8L_00_DISPLAY_OFF[] = {
	0x28
};

static u8 SEQ_HX83102J_GTS8L_00_BRIGHTNESS[] = {
	0x51,
	0xFF, 0xFF
};

static u8 SEQ_HX83102J_GTS8L_00_BRIGHTNESS_ON[] = {
	0x53,
	0x2C,
};

static u8 SEQ_HX83102J_GTS8L_00_001[] = {
	0xB9,
	0x83, 0x10, 0x21, 0x55, 0x00,
};

static u8 SEQ_HX83102J_GTS8L_00_002[] = {
	0xB1,
	0x2C, 0x2F, 0x2F, 0x2F, 0xEF, 0x42, 0xE1, 0x4D, 0x36, 0x36,
	0x36, 0x36, 0x1A, 0x8B, 0x11, 0x65, 0x00, 0x88, 0xFA, 0xFF,
	0xFF, 0x8F, 0xFF, 0x8, 0x9A, 0x33,
};

static u8 SEQ_HX83102J_GTS8L_00_003[] = {
	0xB2,
	0x00, 0x59, 0xA0, 0x00, 0x00, 0x12, 0x5E, 0x3C, 0x84,
};

static u8 SEQ_HX83102J_GTS8L_00_004[] = {
	0xB4,
	0x7C, 0x70, 0x7C, 0x70, 0x7C, 0x70, 0x88, 0x70, 0x7C, 0x70,
	0x01, 0x8A,
};

static u8 SEQ_HX83102J_GTS8L_00_005[] = {
	0xBC,
	0x1B, 0x0,
};

static u8 SEQ_HX83102J_GTS8L_00_006[] = {
	0xBE,
	0x20,
};

static u8 SEQ_HX83102J_GTS8L_00_007[] = {
	0xBF,
	0xFC, 0x80, 0x80, 0x9C, 0x36, 0x0, 0x0C, 0x4,
};

static u8 SEQ_HX83102J_GTS8L_00_008[] = {
	0xC0,
	0x32, 0x32, 0x22, 0x0, 0x0, 0x60, 0x61, 0x8, 0xF4, 0x3,
};

static u8 SEQ_HX83102J_GTS8L_00_009[] = {
	0xC2,
	0x43, 0xFF, 0x0,
};

static u8 SEQ_HX83102J_GTS8L_00_010[] = {
	0xE9,
	0xD1,
};

static u8 SEQ_HX83102J_GTS8L_00_011[] = {
	0xC7,
	0x10, 0x80, 0x20, 0x40,
};

static u8 SEQ_HX83102J_GTS8L_00_012[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS8L_00_013[] = {
	0xE9,
	0xC6,
};

static u8 SEQ_HX83102J_GTS8L_00_014[] = {
	0xC8,
	0x97,
};

static u8 SEQ_HX83102J_GTS8L_00_015[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS8L_00_016[] = {
	0xE9,
	0xD0,
};

static u8 SEQ_HX83102J_GTS8L_00_017[] = {
	0xC8,
	0x1D,
};

static u8 SEQ_HX83102J_GTS8L_00_018[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS8L_00_019[] = {
	0xC9,
	0x00, 0x1E, 0x0F, 0xA0, 0x01,
};

static u8 SEQ_HX83102J_GTS8L_00_020[] = {
	0xCB,
	0x8, 0x13, 0x07, 0x00, 0x0C, 0xE9,
};

static u8 SEQ_HX83102J_GTS8L_00_021[] = {
	0xCC,
	0x2, 0x03, 0x4C,
};

static u8 SEQ_HX83102J_GTS8L_00_022[] = {
	0xD1,
	0x37, 0x6, 0x0, 0x0, 0x4, 0x0C, 0xFF,
};

static u8 SEQ_HX83102J_GTS8L_00_023[] = {
	0xD3,
	0x06, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08, 0x04, 0x08, 0x37,
	0x47, 0x64, 0x4B, 0x11, 0x11, 0x03, 0x03, 0x32, 0x10, 0x0D,
	0x00, 0x0D, 0x32, 0x10, 0x9, 0x00, 0x9, 0x32, 0x19, 0x17,
	0x09, 0x17, 0x00, 0x00,
};

static u8 SEQ_HX83102J_GTS8L_00_024[] = {
	0xD5,
	0x18, 0x18, 0x18, 0x18, 0x1F, 0x1F, 0x1F, 0x1F, 0x1E, 0x1E,
	0x1E, 0x1E, 0x24, 0x24, 0x24, 0x24, 0x07, 0x06, 0x07, 0x06,
	0x05, 0x04, 0x05, 0x04, 0x03, 0x02, 0x03, 0x02, 0x01, 0x00,
	0x01, 0x00, 0x21, 0x20, 0x21, 0x20, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18,
};

static u8 SEQ_HX83102J_GTS8L_00_025[] = {
	0xE1,
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x00, 0x02, 0xD0, /*1st para is version 0x12 -> 0x11 */
	0x00, 0x20, 0x02, 0xD0, 0x02, 0xD0, 0x02, 0x00, 0x02, 0x68,
	0x00, 0x20, 0x03, 0x87, 0x00, 0x0A, 0x00, 0x0C, 0x03, 0x19,
	0x02, 0x63, 0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
	0x06, 0x0B, 0x0B, 0x33, 0x0E,
};

static u8 SEQ_HX83102J_GTS8L_00_026[] = {
	0xE7,
	0x17, 0x0E, 0x0E, 0x2A, 0x22, 0x8B, 0x1, 0x21, 0x8B, 0x14,
	0x14, 0x00, 0x00, 0x00, 0x00, 0x12, 0x05, 0x02, 0x02, 0x0E,
};

static u8 SEQ_HX83102J_GTS8L_00_027[] = {
	0xE0,
	0x00, 0x05, 0x0E, 0x16, 0x1E, 0x33, 0x4C, 0x54, 0x5B, 0x57,
	0x71, 0x77, 0x7B, 0x89, 0x86, 0x8B, 0x93, 0xA5, 0xA4, 0x50,
	0x58, 0x61, 0x73, 0x00, 0x05, 0x0E, 0x16, 0x1E, 0x33, 0x4C,
	0x54, 0x5B, 0x57, 0x71, 0x77, 0x7B, 0x89, 0x86, 0x8B, 0x93,
	0xA5, 0xA4, 0x50, 0x58, 0x61, 0x73,
};

static u8 SEQ_HX83102J_GTS8L_00_028[] = {
	0xBD,
	0x1,
};

static u8 SEQ_HX83102J_GTS8L_00_029[] = {
	0xB1,
	0x01, 0x9B, 0x11,
};

static u8 SEQ_HX83102J_GTS8L_00_030[] = {
	0xD2,
	0xB4,
};

static u8 SEQ_HX83102J_GTS8L_00_031[] = {
	0xE9,
	0xC9,
};

static u8 SEQ_HX83102J_GTS8L_00_032[] = {
	0xD3,
	0x84,
};

static u8 SEQ_HX83102J_GTS8L_00_033[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS8L_00_034[] = {
	0xE7,
	0x02, 0x00, 0xA0, 0x01, 0xA6, 0x0D, 0xA4, 0x0E, 0xA0, 0x00,
	0x00,
};

static u8 SEQ_HX83102J_GTS8L_00_035[] = {
	0xBD,
	0x2,
};

static u8 SEQ_HX83102J_GTS8L_00_036[] = {
	0xBF,
	0xF2,
};

static u8 SEQ_HX83102J_GTS8L_00_037[] = {
	0xD8,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFA, 0xA0, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFA, 0xA0,
};

static u8 SEQ_HX83102J_GTS8L_00_038[] = {
	0xE7,
	0xFE, 0x01, 0xFE, 0x01, 0xFE, 0x01, 0x00, 0x00, 0x00, 0x23,
	0x00, 0x23, 0x81, 0x02, 0x40, 0x00, 0x20, 0x8A, 0x2, 0x1,
	0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
};

static u8 SEQ_HX83102J_GTS8L_00_039[] = {
	0xBD,
	0x3,
};

static u8 SEQ_HX83102J_GTS8L_00_040[] = {
	0xD8,
	0xAA, 0xAA, 0xAA, 0xAA, 0xA0, 0x00, 0xAA, 0xAA, 0xAA, 0xAA,
	0xA0, 0x00, 0x55, 0x55, 0x55, 0x55, 0x50, 0x00, 0x55, 0x55,
	0x55, 0x55, 0x50, 0x00,
};

static u8 SEQ_HX83102J_GTS8L_00_041[] = {
	0xE1,
	0x1,
};

static u8 SEQ_HX83102J_GTS8L_00_042[] = {
	0xBD,
	0x0,
};

static u8 SEQ_HX83102J_GTS8L_00_043[] = {
	0xE9,
	0xCE,
};

static u8 SEQ_HX83102J_GTS8L_00_044[] = {
	0xBA,
	0x1, 0x2,
};

static u8 SEQ_HX83102J_GTS8L_00_045[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS8L_00_046[] = {
	0xBD,
	0x1,
};

static u8 SEQ_HX83102J_GTS8L_00_047[] = {
	0xE9,
	0xC1,
};

static u8 SEQ_HX83102J_GTS8L_00_048[] = {
	0xBA,
	0x3F, 0x3F,
};

static u8 SEQ_HX83102J_GTS8L_00_049[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS8L_00_050[] = {
	0xBD,
	0x3,
};

static u8 SEQ_HX83102J_GTS8L_00_051[] = {
	0xE9,
	0xC5,
};

static u8 SEQ_HX83102J_GTS8L_00_052[] = {
	0xBA,
	0x4F, 0x48,
};

static u8 SEQ_HX83102J_GTS8L_00_053[] = {
	0xE9,
	0x3F,
};

static u8 SEQ_HX83102J_GTS8L_00_054[] = {
	0xBD,
	0x0,
};

static u8 SEQ_HX83102J_GTS8L_00_055[] = {
	0xB9,
	0x0, 0x0, 0x0,
};

/* Below cmd is same to SEQ_HX83102J_GTS8L_00_BRIGHTNESS_ON */

/*
static u8 SEQ_HX83102J_GTS8L_00_316[] = {
	0x53,
	0x2C,
};
*/

static DEFINE_STATIC_PACKET(hx83102j_gts8l_sleep_out, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_sleep_in, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_display_on, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_display_off, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_brightness_on, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_BRIGHTNESS_ON, 0);

static DEFINE_PKTUI(hx83102j_gts8l_brightness, &hx83102j_gts8l_00_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(hx83102j_gts8l_brightness, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_001, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_001, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_002, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_002, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_003, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_003, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_004, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_004, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_005, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_005, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_006, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_006, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_007, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_007, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_008, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_008, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_009, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_009, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_010, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_010, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_011, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_011, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_012, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_012, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_013, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_013, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_014, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_014, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_015, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_015, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_016, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_016, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_017, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_017, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_018, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_018, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_019, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_019, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_020, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_020, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_021, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_021, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_022, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_022, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_023, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_023, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_024, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_024, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_025, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_025, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_026, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_026, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_027, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_027, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_028, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_028, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_029, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_029, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_030, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_030, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_031, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_031, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_032, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_032, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_033, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_033, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_034, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_034, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_035, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_035, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_036, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_036, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_037, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_037, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_038, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_038, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_039, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_039, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_040, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_040, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_041, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_041, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_042, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_042, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_043, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_043, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_044, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_044, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_045, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_045, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_046, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_046, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_047, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_047, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_048, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_048, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_049, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_049, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_050, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_050, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_051, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_051, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_052, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_052, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_053, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_053, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_054, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_054, 0);
static DEFINE_STATIC_PACKET(hx83102j_gts8l_hx83102j_055, DSI_PKT_TYPE_WR, SEQ_HX83102J_GTS8L_00_055, 0);

static DEFINE_PANEL_MDELAY(hx83102j_gts8l_wait_20msec, 20); /* 1 frame */
static DEFINE_PANEL_MDELAY(hx83102j_gts8l_wait_60msec, 60); /* 4 frame */
static DEFINE_PANEL_MDELAY(hx83102j_gts8l_wait_100msec, 100);
static DEFINE_PANEL_MDELAY(hx83102j_gts8l_wait_1msec, 1);
static DEFINE_PANEL_MDELAY(hx83102j_gts8l_wait_2msec, 2);

static void *hx83102j_gts8l_00_init_cmdtbl[] = {
	&tft_common_restbl[RES_ID],
	&PKTINFO(hx83102j_gts8l_hx83102j_001),
	&PKTINFO(hx83102j_gts8l_hx83102j_002),
	&PKTINFO(hx83102j_gts8l_hx83102j_003),
	&PKTINFO(hx83102j_gts8l_hx83102j_004),
	&PKTINFO(hx83102j_gts8l_hx83102j_005),
	&PKTINFO(hx83102j_gts8l_hx83102j_006),
	&PKTINFO(hx83102j_gts8l_hx83102j_007),
	&PKTINFO(hx83102j_gts8l_hx83102j_008),
	&PKTINFO(hx83102j_gts8l_hx83102j_009),
	&PKTINFO(hx83102j_gts8l_hx83102j_010),
	&PKTINFO(hx83102j_gts8l_hx83102j_011),
	&PKTINFO(hx83102j_gts8l_hx83102j_012),
	&PKTINFO(hx83102j_gts8l_hx83102j_013),
	&PKTINFO(hx83102j_gts8l_hx83102j_014),
	&PKTINFO(hx83102j_gts8l_hx83102j_015),
	&PKTINFO(hx83102j_gts8l_hx83102j_016),
	&PKTINFO(hx83102j_gts8l_hx83102j_017),
	&PKTINFO(hx83102j_gts8l_hx83102j_018),
	&PKTINFO(hx83102j_gts8l_hx83102j_019),
	&PKTINFO(hx83102j_gts8l_hx83102j_020),
	&PKTINFO(hx83102j_gts8l_hx83102j_021),
	&PKTINFO(hx83102j_gts8l_hx83102j_022),
	&PKTINFO(hx83102j_gts8l_hx83102j_023),
	&PKTINFO(hx83102j_gts8l_hx83102j_024),
	&PKTINFO(hx83102j_gts8l_hx83102j_025),
	&PKTINFO(hx83102j_gts8l_hx83102j_026),
	&PKTINFO(hx83102j_gts8l_hx83102j_027),
	&PKTINFO(hx83102j_gts8l_hx83102j_028),
	&PKTINFO(hx83102j_gts8l_hx83102j_029),
	&PKTINFO(hx83102j_gts8l_hx83102j_030),
	&PKTINFO(hx83102j_gts8l_hx83102j_031),
	&PKTINFO(hx83102j_gts8l_hx83102j_032),
	&PKTINFO(hx83102j_gts8l_hx83102j_033),
	&PKTINFO(hx83102j_gts8l_hx83102j_034),
	&PKTINFO(hx83102j_gts8l_hx83102j_035),
	&PKTINFO(hx83102j_gts8l_hx83102j_036),
	&PKTINFO(hx83102j_gts8l_hx83102j_037),
	&PKTINFO(hx83102j_gts8l_hx83102j_038),
	&PKTINFO(hx83102j_gts8l_hx83102j_039),
	&PKTINFO(hx83102j_gts8l_hx83102j_040),
	&PKTINFO(hx83102j_gts8l_hx83102j_041),
	&PKTINFO(hx83102j_gts8l_hx83102j_042),
	&PKTINFO(hx83102j_gts8l_hx83102j_043),
	&PKTINFO(hx83102j_gts8l_hx83102j_044),
	&PKTINFO(hx83102j_gts8l_hx83102j_045),
	&PKTINFO(hx83102j_gts8l_hx83102j_046),
	&PKTINFO(hx83102j_gts8l_hx83102j_047),
	&PKTINFO(hx83102j_gts8l_hx83102j_048),
	&PKTINFO(hx83102j_gts8l_hx83102j_049),
	&PKTINFO(hx83102j_gts8l_hx83102j_050),
	&PKTINFO(hx83102j_gts8l_hx83102j_051),
	&PKTINFO(hx83102j_gts8l_hx83102j_052),
	&PKTINFO(hx83102j_gts8l_hx83102j_053),
	&PKTINFO(hx83102j_gts8l_hx83102j_054),
	&PKTINFO(hx83102j_gts8l_hx83102j_055),
	&PKTINFO(hx83102j_gts8l_sleep_out),
	&DLYINFO(hx83102j_gts8l_wait_100msec),
	&PKTINFO(hx83102j_gts8l_display_on),
};

static void *hx83102j_gts8l_00_res_init_cmdtbl[] = {
	&tft_common_restbl[RES_ID],
};

static void *hx83102j_gts8l_00_set_bl_cmdtbl[] = {
	&PKTINFO(hx83102j_gts8l_brightness),
	&PKTINFO(hx83102j_gts8l_brightness_on),
};

static void *hx83102j_gts8l_00_display_off_cmdtbl[] = {
	&PKTINFO(hx83102j_gts8l_display_off),
	&DLYINFO(hx83102j_gts8l_wait_20msec),
};

static void *hx83102j_gts8l_00_exit_cmdtbl[] = {
	&PKTINFO(hx83102j_gts8l_sleep_in),
	&DLYINFO(hx83102j_gts8l_wait_60msec),
};

static void *hx83102j_gts8l_00_dummy_cmdtbl[] = {
	NULL,
};

static struct seqinfo hx83102j_gts8l_00_seqtbl[MAX_PANEL_SEQ] = {
	[PANEL_INIT_SEQ] = SEQINFO_INIT("init-seq", hx83102j_gts8l_00_init_cmdtbl),
	[PANEL_RES_INIT_SEQ] = SEQINFO_INIT("resource-init-seq", hx83102j_gts8l_00_res_init_cmdtbl),
	[PANEL_SET_BL_SEQ] = SEQINFO_INIT("set-bl-seq", hx83102j_gts8l_00_set_bl_cmdtbl),
	[PANEL_DISPLAY_MODE_SEQ] = SEQINFO_INIT("set-bl-seq", hx83102j_gts8l_00_set_bl_cmdtbl), /* Dummy */
	[PANEL_DISPLAY_ON_SEQ] = SEQINFO_INIT("display-off-seq", hx83102j_gts8l_00_set_bl_cmdtbl), /* Dummy */
	[PANEL_DISPLAY_OFF_SEQ] = SEQINFO_INIT("display-off-seq", hx83102j_gts8l_00_display_off_cmdtbl),
	[PANEL_EXIT_SEQ] = SEQINFO_INIT("exit-seq", hx83102j_gts8l_00_exit_cmdtbl),
};


/* BLIC SETTING START */
static u8 _ISL98608_I2C_DUMP[] = {
	0x04, 0x00,
	0x03, 0x00,
};

static DEFINE_STATIC_PACKET(isl98608_i2c_dump, I2C_PKT_TYPE_RD, _ISL98608_I2C_DUMP, 0);

static void *isl98608_init_cmdtbl[] = {
	&PKTINFO(isl98608_i2c_dump),
};

static void *isl98608_exit_cmdtbl[] = {
	&PKTINFO(isl98608_i2c_dump),
};

static struct seqinfo isl98608_seq_tbl[MAX_PANEL_BLIC_SEQ] = {
	[PANEL_BLIC_I2C_ON_SEQ] = SEQINFO_INIT("i2c-init-seq", isl98608_init_cmdtbl),
	[PANEL_BLIC_I2C_OFF_SEQ] = SEQINFO_INIT("i2c-exit-seq", isl98608_exit_cmdtbl),
};

static struct blic_data isl98608_blic_data = {
	.name = "isl98608",
	.seqtbl = isl98608_seq_tbl,
};

/* BLIC SETTING START */
static u8 _MAX77816_I2C_INIT[] = {
	0x04, 0x78,
};

static u8 _MAX77816_I2C_EXIT[] = {
	0x03, 0x32,
};

static u8 _MAX77816_I2C_DUMP[] = {
	0x04, 0x00,
	0x03, 0x00,
};

static DEFINE_STATIC_PACKET(max77816_i2c_init, I2C_PKT_TYPE_WR, _MAX77816_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(max77816_i2c_exit, I2C_PKT_TYPE_WR, _MAX77816_I2C_EXIT, 0);
static DEFINE_STATIC_PACKET(max77816_i2c_dump, I2C_PKT_TYPE_RD, _MAX77816_I2C_DUMP, 0);

static DEFINE_PANEL_MDELAY(blic_wait_delay, 2);

static void *max77816_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(max77816_i2c_dump),
#endif
	&PKTINFO(max77816_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(max77816_i2c_dump),
#endif
};

static void *max77816_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(max77816_i2c_dump),
#endif
	&PKTINFO(max77816_i2c_exit),
};

static struct seqinfo max77816_seq_tbl[MAX_PANEL_BLIC_SEQ] = {
	[PANEL_BLIC_I2C_ON_SEQ] = SEQINFO_INIT("i2c-init-seq", max77816_init_cmdtbl),
	[PANEL_BLIC_I2C_OFF_SEQ] = SEQINFO_INIT("i2c-exit-seq", max77816_exit_cmdtbl),
};

static struct blic_data max77816_blic_data = {
	.name = "max77816",
	.seqtbl = max77816_seq_tbl,
};

static struct blic_data *hx83102j_gts8l_00_blic_tbl[] = {
	&isl98608_blic_data,
	&max77816_blic_data,
};
/* BLIC SETTING END */




struct common_panel_info hx83102j_gts8l_00_panel_info = {
	.ldi_name = "hx83102j",
	.name = "hx83102j_gts8l_00",
	.model = "BOE_10_91_inch",
	.vendor = "BOE",
	.id = 0x4CD230,
	.rev = 0,
	.ddi_props = {
		.err_fg_recovery = false,
		.support_vrr = true,
		.init_seq_by_lpdt = true,
	},
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.common_panel_modes = &hx83102j_gts8l_00_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(hx83102j_gts8l_00_default_resol),
		.resol = hx83102j_gts8l_00_default_resol,
	},
	.vrrtbl = hx83102j_gts8l_00_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(hx83102j_gts8l_00_default_vrrtbl),
	.maptbl = hx83102j_gts8l_00_maptbl,
	.nr_maptbl = ARRAY_SIZE(hx83102j_gts8l_00_maptbl),
	.seqtbl = hx83102j_gts8l_00_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(hx83102j_gts8l_00_seqtbl),
	.rditbl = tft_common_rditbl,
	.nr_rditbl = ARRAY_SIZE(tft_common_rditbl),
	.restbl = tft_common_restbl,
	.nr_restbl = ARRAY_SIZE(tft_common_restbl),
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &hx83102j_gts8l_00_panel_dimming_info,
	},
	.blic_data_tbl = hx83102j_gts8l_00_blic_tbl,
	.nr_blic_data_tbl = ARRAY_SIZE(hx83102j_gts8l_00_blic_tbl),
};
#endif /* __HX83102J_GTS8L_00_PANEL_H__ */
