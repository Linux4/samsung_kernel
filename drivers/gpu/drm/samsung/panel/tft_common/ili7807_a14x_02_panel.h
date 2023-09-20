/*
 * linux/drivers/video/fbdev/exynos/panel/ili7807/ili7807_a14x_02_panel.h
 *
 * Header file for ILI7807 Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ILI7807_A14X_02_PANEL_H__
#define __ILI7807_A14X_02_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "tft_common.h"
#include "ili7807_a14x_02.h"
#include "tft_function.h"
#include "ili7807_a14x_02_resol.h"

#undef __pn_name__
#define __pn_name__	a14x

#undef __PN_NAME__
#define __PN_NAME__ A14X

#define ILI7807_NR_STEP (256)
#define ILI7807_HBM_STEP (51)
#define ILI7807_TOTAL_STEP (ILI7807_NR_STEP + ILI7807_HBM_STEP) /* 0 ~ 306 */

static unsigned int ili7807_a14x_02_brt_tbl[ILI7807_TOTAL_STEP] = {
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

static unsigned int ili7807_a14x_02_step_cnt_tbl[ILI7807_TOTAL_STEP] = {
	[0 ... 255] = 1,
	/* HBM */
	[256 ... 306] = 1,
};

struct brightness_table ili7807_a14x_02_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = ili7807_a14x_02_brt_tbl,
	.sz_brt = ARRAY_SIZE(ili7807_a14x_02_brt_tbl),
	.sz_ui_brt = ILI7807_NR_STEP,
	.sz_hbm_brt = ILI7807_HBM_STEP,
	.lum = ili7807_a14x_02_brt_tbl,
	.sz_lum = ARRAY_SIZE(ili7807_a14x_02_brt_tbl),
	.sz_ui_lum = ILI7807_NR_STEP,
	.sz_hbm_lum = ILI7807_HBM_STEP,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = ili7807_a14x_02_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(ili7807_a14x_02_step_cnt_tbl),
	.vtotal = 0,
};

static struct panel_dimming_info ili7807_a14x_02_panel_dimming_info = {
	.name = "ili7807_a14x",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = -1,
	.brt_tbl = &ili7807_a14x_02_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,	/* read dim flash when probe or not */
	.hbm_aor = NULL,
};

static u8 ili7807_a14x_02_brt_table[ILI7807_TOTAL_STEP][2] = {
	{0x00, 0x00},
	{0x00, 0x10}, {0x00, 0x10}, {0x00, 0x20}, {0x00, 0x20}, {0x00, 0x30}, {0x00, 0x30}, {0x00, 0x40}, {0x00, 0x40}, {0x00, 0x50}, {0x00, 0x50},
	{0x00, 0x60}, {0x00, 0x60}, {0x00, 0x70}, {0x00, 0x70}, {0x00, 0x80}, {0x00, 0x80}, {0x00, 0x90}, {0x00, 0x90}, {0x00, 0xA0}, {0x00, 0xA0},
	{0x00, 0xB0}, {0x00, 0xB0}, {0x00, 0xC0}, {0x00, 0xC0}, {0x00, 0xD0}, {0x00, 0xD0}, {0x00, 0xE0}, {0x00, 0xE0}, {0x00, 0xF0}, {0x00, 0xF0},
	{0x01, 0x00}, {0x01, 0x00}, {0x01, 0x10}, {0x01, 0x10}, {0x01, 0x20}, {0x01, 0x30}, {0x01, 0x30}, {0x01, 0x40}, {0x01, 0x40}, {0x01, 0x50},
	{0x01, 0x60}, {0x01, 0x60}, {0x01, 0x70}, {0x01, 0x70}, {0x01, 0x80}, {0x01, 0x90}, {0x01, 0x90}, {0x01, 0xA0}, {0x01, 0xA0}, {0x01, 0xB0},
	{0x01, 0xC0}, {0x01, 0xC0}, {0x01, 0xD0}, {0x01, 0xD0}, {0x01, 0xE0}, {0x01, 0xF0}, {0x01, 0xF0}, {0x02, 0x00}, {0x02, 0x00}, {0x02, 0x10},
	{0x02, 0x20}, {0x02, 0x20}, {0x02, 0x30}, {0x02, 0x30}, {0x02, 0x40}, {0x02, 0x50}, {0x02, 0x50}, {0x02, 0x60}, {0x02, 0x60}, {0x02, 0x70},
	{0x02, 0x80}, {0x02, 0x80}, {0x02, 0x90}, {0x02, 0x90}, {0x02, 0xA0}, {0x02, 0xB0}, {0x02, 0xB0}, {0x02, 0xC0}, {0x02, 0xC0}, {0x02, 0xD0},
	{0x02, 0xE0}, {0x02, 0xF0}, {0x02, 0xF0}, {0x03, 0x00}, {0x03, 0x10}, {0x03, 0x20}, {0x03, 0x30}, {0x03, 0x30}, {0x03, 0x40}, {0x03, 0x50},
	{0x03, 0x60}, {0x03, 0x70}, {0x03, 0x70}, {0x03, 0x80}, {0x03, 0x90}, {0x03, 0xA0}, {0x03, 0xA0}, {0x03, 0xB0}, {0x03, 0xC0}, {0x03, 0xD0},
	{0x03, 0xE0}, {0x03, 0xE0}, {0x03, 0xF0}, {0x04, 0x00}, {0x04, 0x10}, {0x04, 0x20}, {0x04, 0x20}, {0x04, 0x30}, {0x04, 0x40}, {0x04, 0x50},
	{0x04, 0x60}, {0x04, 0x60}, {0x04, 0x70}, {0x04, 0x80}, {0x04, 0x90}, {0x04, 0xA0}, {0x04, 0xA0}, {0x04, 0xB0}, {0x04, 0xC0}, {0x04, 0xD0},
	{0x04, 0xD0}, {0x04, 0xE0}, {0x04, 0xF0}, {0x05, 0x00}, {0x05, 0x10}, {0x05, 0x10}, {0x05, 0x20}, {0x05, 0x30}, {0x05, 0x40}, {0x05, 0x50},
	{0x05, 0x60}, {0x05, 0x70}, {0x05, 0x80}, {0x05, 0x90}, {0x05, 0xA0}, {0x05, 0xB0}, {0x05, 0xC0}, {0x05, 0xD0}, {0x05, 0xE0}, {0x05, 0xF0},
	{0x06, 0x00}, {0x06, 0x10}, {0x06, 0x20}, {0x06, 0x30}, {0x06, 0x40}, {0x06, 0x50}, {0x06, 0x60}, {0x06, 0x70}, {0x06, 0x80}, {0x06, 0x90},
	{0x06, 0xA0}, {0x06, 0xB0}, {0x06, 0xC0}, {0x06, 0xD0}, {0x06, 0xE0}, {0x06, 0xF0}, {0x07, 0x00}, {0x07, 0x10}, {0x07, 0x20}, {0x07, 0x40},
	{0x07, 0x50}, {0x07, 0x60}, {0x07, 0x70}, {0x07, 0x80}, {0x07, 0x90}, {0x07, 0xA0}, {0x07, 0xB0}, {0x07, 0xC0}, {0x07, 0xD0}, {0x07, 0xE0},
	{0x07, 0xF0}, {0x08, 0x00}, {0x08, 0x10}, {0x08, 0x20}, {0x08, 0x30}, {0x08, 0x40}, {0x08, 0x50}, {0x08, 0x60}, {0x08, 0x70}, {0x08, 0x80},
	{0x08, 0x90}, {0x08, 0xA0}, {0x08, 0xB0}, {0x08, 0xC0}, {0x08, 0xD0}, {0x08, 0xE0}, {0x08, 0xF0}, {0x09, 0x00}, {0x09, 0x10}, {0x09, 0x20},
	{0x09, 0x30}, {0x09, 0x40}, {0x09, 0x50}, {0x09, 0x60}, {0x09, 0x70}, {0x09, 0x80}, {0x09, 0x90}, {0x09, 0xA0}, {0x09, 0xB0}, {0x09, 0xC0},
	{0x09, 0xD0}, {0x09, 0xE0}, {0x09, 0xF0}, {0x0A, 0x00}, {0x0A, 0x10}, {0x0A, 0x20}, {0x0A, 0x30}, {0x0A, 0x40}, {0x0A, 0x50}, {0x0A, 0x60},
	{0x0A, 0x70}, {0x0A, 0x80}, {0x0A, 0x90}, {0x0A, 0xA0}, {0x0A, 0xB0}, {0x0A, 0xC0}, {0x0A, 0xD0}, {0x0A, 0xE0}, {0x0A, 0xF0}, {0x0B, 0x00},
	{0x0B, 0x10}, {0x0B, 0x20}, {0x0B, 0x30}, {0x0B, 0x50}, {0x0B, 0x60}, {0x0B, 0x70}, {0x0B, 0x80}, {0x0B, 0x90}, {0x0B, 0xA0}, {0x0B, 0xB0},
	{0x0B, 0xC0}, {0x0B, 0xD0}, {0x0B, 0xE0}, {0x0B, 0xF0}, {0x0C, 0x00}, {0x0C, 0x10}, {0x0C, 0x20}, {0x0C, 0x30}, {0x0C, 0x40}, {0x0C, 0x50},
	{0x0C, 0x60}, {0x0C, 0x70}, {0x0C, 0x80}, {0x0C, 0x90}, {0x0C, 0xA0}, {0x0C, 0xB0}, {0x0C, 0xC0}, {0x0C, 0xD0}, {0x0C, 0xE0}, {0x0C, 0xF0},
	{0x0D, 0x00}, {0x0D, 0x10}, {0x0D, 0x20}, {0x0D, 0x30}, {0x0D, 0x40}, {0x0D, 0x50}, {0x0D, 0x60}, {0x0D, 0x70}, {0x0D, 0x70}, {0x0D, 0x80},
	{0x0D, 0x90}, {0x0D, 0xA0}, {0x0D, 0xB0}, {0x0D, 0xC0}, {0x0D, 0xC0}, {0x0D, 0xD0}, {0x0D, 0xE0}, {0x0D, 0xF0}, {0x0E, 0x00}, {0x0E, 0x10},
	{0x0E, 0x10}, {0x0E, 0x20}, {0x0E, 0x30}, {0x0E, 0x40}, {0x0E, 0x50}, {0x0E, 0x60}, {0x0E, 0x70}, {0x0E, 0x70}, {0x0E, 0x80}, {0x0E, 0x90},
	{0x0E, 0xA0}, {0x0E, 0xB0}, {0x0E, 0xC0}, {0x0E, 0xC0}, {0x0E, 0xD0}, {0x0E, 0xE0}, {0x0E, 0xF0}, {0x0F, 0x00}, {0x0F, 0x10}, {0x0F, 0x20},
	{0x0F, 0x20}, {0x0F, 0x30}, {0x0F, 0x40}, {0x0F, 0x50}, {0x0F, 0x60}, {0x0F, 0x70}, {0x0F, 0x70}, {0x0F, 0x80}, {0x0F, 0x90}, {0x0F, 0xA0},
	{0x0F, 0xB0}, {0x0F, 0xC0}, {0x0F, 0xC0}, {0x0F, 0xD0}, {0x0F, 0xE0}, {0x0F, 0xF0},

};

static struct maptbl ili7807_a14x_02_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(ili7807_a14x_02_brt_table,
			&TFT_FUNC(TFT_MAPTBL_INIT_BRT),
			&TFT_FUNC(TFT_MAPTBL_GETIDX_BRT),
			&TFT_FUNC(TFT_MAPTBL_COPY_DEFAULT)),
};

static u8 SEQ_ILI7807_A14X_02_SLEEP_OUT[] = {
	0x11
};

static u8 SEQ_ILI7807_A14X_02_SLEEP_IN[] = {
	0x10
};

static u8 SEQ_ILI7807_A14X_02_DISPLAY_ON[] = {
	0x29
};

static u8 SEQ_ILI7807_A14X_02_DISPLAY_OFF[] = {
	0x28
};

static u8 SEQ_ILI7807_A14X_02_BRIGHTNESS[] = {
	0x51,
	0x0F, 0xF0,
};

/* < CABC Mode control Function > */

static u8 SEQ_ILI7807_A14X_02_BRIGHTNESS_ON[] = {
	0x53,
	0x24,
};

/* Display config (1) */
static u8 SEQ_ILI7807_A14X_02_001[] = {
	0xFF,
	0x78, 0x07, 0x01,
};

static u8 SEQ_ILI7807_A14X_02_002[] = {
	0x00,
	0x62,
};

static u8 SEQ_ILI7807_A14X_02_003[] = {
	0x01,
	0x11,
};

static u8 SEQ_ILI7807_A14X_02_004[] = {
	0x02,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_005[] = {
	0x03,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_006[] = {
	0x04,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_007[] = {
	0x05,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_008[] = {
	0x06,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_009[] = {
	0x07,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_010[] = {
	0x08,
	0xA9,
};

static u8 SEQ_ILI7807_A14X_02_011[] = {
	0x09,
	0x0A,
};

static u8 SEQ_ILI7807_A14X_02_012[] = {
	0x0A,
	0x30,
};

static u8 SEQ_ILI7807_A14X_02_013[] = {
	0x0B,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_014[] = {
	0x0C,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_015[] = {
	0x0D,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_016[] = {
	0x0E,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_017[] = {
	0x31,
	0x07,
};

static u8 SEQ_ILI7807_A14X_02_018[] = {
	0x32,
	0x02,
};

static u8 SEQ_ILI7807_A14X_02_019[] = {
	0x33,
	0x41,
};

static u8 SEQ_ILI7807_A14X_02_020[] = {
	0x34,
	0x41,
};

static u8 SEQ_ILI7807_A14X_02_021[] = {
	0x35,
	0x28,
};

static u8 SEQ_ILI7807_A14X_02_022[] = {
	0x36,
	0x28,
};

static u8 SEQ_ILI7807_A14X_02_023[] = {
	0x37,
	0x30,
};

static u8 SEQ_ILI7807_A14X_02_024[] = {
	0x38,
	0x2F,
};

static u8 SEQ_ILI7807_A14X_02_025[] = {
	0x39,
	0x2E,
};

static u8 SEQ_ILI7807_A14X_02_026[] = {
	0x3A,
	0x30,
};

static u8 SEQ_ILI7807_A14X_02_027[] = {
	0x3B,
	0x2F,
};

static u8 SEQ_ILI7807_A14X_02_028[] = {
	0x3C,
	0x2E,
};

static u8 SEQ_ILI7807_A14X_02_029[] = {
	0x3D,
	0x25,
};

static u8 SEQ_ILI7807_A14X_02_030[] = {
	0x3E,
	0x11,
};

static u8 SEQ_ILI7807_A14X_02_031[] = {
	0x3F,
	0x10,
};

static u8 SEQ_ILI7807_A14X_02_032[] = {
	0x40,
	0x13,
};

static u8 SEQ_ILI7807_A14X_02_033[] = {
	0x41,
	0x12,
};

static u8 SEQ_ILI7807_A14X_02_034[] = {
	0x42,
	0x2C,
};

static u8 SEQ_ILI7807_A14X_02_035[] = {
	0x43,
	0x40,
};

static u8 SEQ_ILI7807_A14X_02_036[] = {
	0x44,
	0x40,
};

static u8 SEQ_ILI7807_A14X_02_037[] = {
	0x45,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_038[] = {
	0x46,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_039[] = {
	0x47,
	0x09,
};

static u8 SEQ_ILI7807_A14X_02_040[] = {
	0x48,
	0x08,
};

static u8 SEQ_ILI7807_A14X_02_041[] = {
	0x49,
	0x07,
};

static u8 SEQ_ILI7807_A14X_02_042[] = {
	0x4A,
	0x02,
};

static u8 SEQ_ILI7807_A14X_02_043[] = {
	0x4B,
	0x41,
};

static u8 SEQ_ILI7807_A14X_02_044[] = {
	0x4C,
	0x41,
};

static u8 SEQ_ILI7807_A14X_02_045[] = {
	0x4D,
	0x28,
};

static u8 SEQ_ILI7807_A14X_02_046[] = {
	0x4E,
	0x28,
};

static u8 SEQ_ILI7807_A14X_02_047[] = {
	0x4F,
	0x30,
};

static u8 SEQ_ILI7807_A14X_02_048[] = {
	0x50,
	0x2F,
};

static u8 SEQ_ILI7807_A14X_02_049[] = {
	0x51,
	0x2E,
};

static u8 SEQ_ILI7807_A14X_02_050[] = {
	0x52,
	0x30,
};

static u8 SEQ_ILI7807_A14X_02_051[] = {
	0x53,
	0x2F,
};

static u8 SEQ_ILI7807_A14X_02_052[] = {
	0x54,
	0x2E,
};

static u8 SEQ_ILI7807_A14X_02_053[] = {
	0x55,
	0x25,
};

static u8 SEQ_ILI7807_A14X_02_054[] = {
	0x56,
	0x11,
};

static u8 SEQ_ILI7807_A14X_02_055[] = {
	0x57,
	0x10,
};

static u8 SEQ_ILI7807_A14X_02_056[] = {
	0x58,
	0x13,
};

static u8 SEQ_ILI7807_A14X_02_057[] = {
	0x59,
	0x12,
};

static u8 SEQ_ILI7807_A14X_02_058[] = {
	0x5A,
	0x2C,
};

static u8 SEQ_ILI7807_A14X_02_059[] = {
	0x5B,
	0x40,
};

static u8 SEQ_ILI7807_A14X_02_060[] = {
	0x5C,
	0x40,
};

static u8 SEQ_ILI7807_A14X_02_061[] = {
	0x5D,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_062[] = {
	0x5E,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_063[] = {
	0x5F,
	0x09,
};

static u8 SEQ_ILI7807_A14X_02_064[] = {
	0x60,
	0x08,
};

static u8 SEQ_ILI7807_A14X_02_065[] = {
	0x61,
	0x07,
};

static u8 SEQ_ILI7807_A14X_02_066[] = {
	0x62,
	0x02,
};

static u8 SEQ_ILI7807_A14X_02_067[] = {
	0x63,
	0x41,
};

static u8 SEQ_ILI7807_A14X_02_068[] = {
	0x64,
	0x41,
};

static u8 SEQ_ILI7807_A14X_02_069[] = {
	0x65,
	0x28,
};

static u8 SEQ_ILI7807_A14X_02_070[] = {
	0x66,
	0x28,
};

static u8 SEQ_ILI7807_A14X_02_071[] = {
	0x67,
	0x30,
};

static u8 SEQ_ILI7807_A14X_02_072[] = {
	0x68,
	0x2F,
};

static u8 SEQ_ILI7807_A14X_02_073[] = {
	0x69,
	0x2E,
};

static u8 SEQ_ILI7807_A14X_02_074[] = {
	0x6A,
	0x30,
};

static u8 SEQ_ILI7807_A14X_02_075[] = {
	0x6B,
	0x2F,
};

static u8 SEQ_ILI7807_A14X_02_076[] = {
	0x6C,
	0x2E,
};

static u8 SEQ_ILI7807_A14X_02_077[] = {
	0x6D,
	0x25,
};

static u8 SEQ_ILI7807_A14X_02_078[] = {
	0x6E,
	0x12,
};

static u8 SEQ_ILI7807_A14X_02_079[] = {
	0x6F,
	0x13,
};

static u8 SEQ_ILI7807_A14X_02_080[] = {
	0x70,
	0x10,
};

static u8 SEQ_ILI7807_A14X_02_081[] = {
	0x71,
	0x11,
};

static u8 SEQ_ILI7807_A14X_02_082[] = {
	0x72,
	0x2C,
};

static u8 SEQ_ILI7807_A14X_02_083[] = {
	0x73,
	0x40,
};

static u8 SEQ_ILI7807_A14X_02_084[] = {
	0x74,
	0x40,
};

static u8 SEQ_ILI7807_A14X_02_085[] = {
	0x75,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_086[] = {
	0x76,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_087[] = {
	0x77,
	0x08,
};

static u8 SEQ_ILI7807_A14X_02_088[] = {
	0x78,
	0x09,
};

static u8 SEQ_ILI7807_A14X_02_089[] = {
	0x79,
	0x07,
};

static u8 SEQ_ILI7807_A14X_02_090[] = {
	0x7A,
	0x02,
};

static u8 SEQ_ILI7807_A14X_02_091[] = {
	0x7B,
	0x41,
};

static u8 SEQ_ILI7807_A14X_02_092[] = {
	0x7C,
	0x41,
};

static u8 SEQ_ILI7807_A14X_02_093[] = {
	0x7D,
	0x28,
};

static u8 SEQ_ILI7807_A14X_02_094[] = {
	0x7E,
	0x28,
};

static u8 SEQ_ILI7807_A14X_02_095[] = {
	0x7F,
	0x30,
};

static u8 SEQ_ILI7807_A14X_02_096[] = {
	0x80,
	0x2F,
};

static u8 SEQ_ILI7807_A14X_02_097[] = {
	0x81,
	0x2E,
};

static u8 SEQ_ILI7807_A14X_02_098[] = {
	0x82,
	0x30,
};

static u8 SEQ_ILI7807_A14X_02_099[] = {
	0x83,
	0x2F,
};

static u8 SEQ_ILI7807_A14X_02_100[] = {
	0x84,
	0x2E,
};

static u8 SEQ_ILI7807_A14X_02_101[] = {
	0x85,
	0x25,
};

static u8 SEQ_ILI7807_A14X_02_102[] = {
	0x86,
	0x12,
};

static u8 SEQ_ILI7807_A14X_02_103[] = {
	0x87,
	0x13,
};

static u8 SEQ_ILI7807_A14X_02_104[] = {
	0x88,
	0x10,
};

static u8 SEQ_ILI7807_A14X_02_105[] = {
	0x89,
	0x11,
};

static u8 SEQ_ILI7807_A14X_02_106[] = {
	0x8A,
	0x2C,
};

static u8 SEQ_ILI7807_A14X_02_107[] = {
	0x8B,
	0x40,
};

static u8 SEQ_ILI7807_A14X_02_108[] = {
	0x8C,
	0x40,
};

static u8 SEQ_ILI7807_A14X_02_109[] = {
	0x8D,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_110[] = {
	0x8E,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_111[] = {
	0x8F,
	0x08,
};

static u8 SEQ_ILI7807_A14X_02_112[] = {
	0x90,
	0x09,
};

static u8 SEQ_ILI7807_A14X_02_113[] = {
	0xA0,
	0x4C,
};

static u8 SEQ_ILI7807_A14X_02_114[] = {
	0xA1,
	0x4A,
};

static u8 SEQ_ILI7807_A14X_02_115[] = {
	0xA2,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_116[] = {
	0xA3,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_117[] = {
	0xA7,
	0x10,
};

static u8 SEQ_ILI7807_A14X_02_118[] = {
	0xAA,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_119[] = {
	0xAB,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_120[] = {
	0xAC,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_121[] = {
	0xAE,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_122[] = {
	0xB0,
	0x20,
};

static u8 SEQ_ILI7807_A14X_02_123[] = {
	0xB1,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_124[] = {
	0xB2,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_125[] = {
	0xB3,
	0x04,
};

static u8 SEQ_ILI7807_A14X_02_126[] = {
	0xB4,
	0x05,
};

static u8 SEQ_ILI7807_A14X_02_127[] = {
	0xB5,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_128[] = {
	0xB6,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_129[] = {
	0xB7,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_130[] = {
	0xB8,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_131[] = {
	0xC0,
	0x0C,
};

static u8 SEQ_ILI7807_A14X_02_132[] = {
	0xC1,
	0x05,
};

static u8 SEQ_ILI7807_A14X_02_133[] = {
	0xC2,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_134[] = {
	0xC3,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_135[] = {
	0xC4,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_136[] = {
	0xC5,
	0x2B,
};

static u8 SEQ_ILI7807_A14X_02_137[] = {
	0xC6,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_138[] = {
	0xC7,
	0x28,
};

static u8 SEQ_ILI7807_A14X_02_139[] = {
	0xC8,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_140[] = {
	0xC9,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_141[] = {
	0xCA,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_142[] = {
	0xD0,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_143[] = {
	0xD1,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_144[] = {
	0xD2,
	0x10,
};

static u8 SEQ_ILI7807_A14X_02_145[] = {
	0xD3,
	0x41,
};

static u8 SEQ_ILI7807_A14X_02_146[] = {
	0xD4,
	0x89,
};

static u8 SEQ_ILI7807_A14X_02_147[] = {
	0xD5,
	0x16,
};

static u8 SEQ_ILI7807_A14X_02_148[] = {
	0xD6,
	0x49,
};

static u8 SEQ_ILI7807_A14X_02_149[] = {
	0xD7,
	0x40,
};

static u8 SEQ_ILI7807_A14X_02_150[] = {
	0xD8,
	0x09,
};

static u8 SEQ_ILI7807_A14X_02_151[] = {
	0xD9,
	0x96,
};

static u8 SEQ_ILI7807_A14X_02_152[] = {
	0xDA,
	0xAA,
};

static u8 SEQ_ILI7807_A14X_02_153[] = {
	0xDB,
	0xAA,
};

static u8 SEQ_ILI7807_A14X_02_154[] = {
	0xDC,
	0x8A,
};

static u8 SEQ_ILI7807_A14X_02_155[] = {
	0xDD,
	0xA8,
};

static u8 SEQ_ILI7807_A14X_02_156[] = {
	0xDE,
	0x05,
};

static u8 SEQ_ILI7807_A14X_02_157[] = {
	0xDF,
	0x42,
};

static u8 SEQ_ILI7807_A14X_02_158[] = {
	0xE0,
	0x1E,
};

static u8 SEQ_ILI7807_A14X_02_159[] = {
	0xE1,
	0x68,
};

static u8 SEQ_ILI7807_A14X_02_160[] = {
	0xE2,
	0x07,
};

static u8 SEQ_ILI7807_A14X_02_161[] = {
	0xE3,
	0x11,
};

static u8 SEQ_ILI7807_A14X_02_162[] = {
	0xE4,
	0x42,
};

static u8 SEQ_ILI7807_A14X_02_163[] = {
	0xE5,
	0x4F,
};

static u8 SEQ_ILI7807_A14X_02_164[] = {
	0xE6,
	0x2A,
};

static u8 SEQ_ILI7807_A14X_02_165[] = {
	0xE7,
	0x0C,
};

static u8 SEQ_ILI7807_A14X_02_166[] = {
	0xE8,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_167[] = {
	0xE9,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_168[] = {
	0xEA,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_169[] = {
	0xEB,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_170[] = {
	0xEC,
	0x80,
};

static u8 SEQ_ILI7807_A14X_02_171[] = {
	0xED,
	0x56,
};

static u8 SEQ_ILI7807_A14X_02_172[] = {
	0xEE,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_173[] = {
	0xEF,
	0x32,
};

static u8 SEQ_ILI7807_A14X_02_174[] = {
	0xF0,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_175[] = {
	0xF1,
	0xC0,
};

static u8 SEQ_ILI7807_A14X_02_176[] = {
	0xF2,
	0xFF,
};

static u8 SEQ_ILI7807_A14X_02_177[] = {
	0xF3,
	0xFF,
};

static u8 SEQ_ILI7807_A14X_02_178[] = {
	0xF4,
	0x54,
};

static u8 SEQ_ILI7807_A14X_02_179[] = {
	0xFF,
	0x78, 0x07, 0x03,
};

static u8 SEQ_ILI7807_A14X_02_180[] = {
	0x80,
	0x36,
};

static u8 SEQ_ILI7807_A14X_02_181[] = {
	0x83,
	0x60,
};

static u8 SEQ_ILI7807_A14X_02_182[] = {
	0x84,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_183[] = {
	0x88,
	0xE6,
};

static u8 SEQ_ILI7807_A14X_02_184[] = {
	0x89,
	0xEE,
};

static u8 SEQ_ILI7807_A14X_02_185[] = {
	0x8A,
	0xF6,
};

static u8 SEQ_ILI7807_A14X_02_186[] = {
	0x8B,
	0xF6,
};

static u8 SEQ_ILI7807_A14X_02_187[] = {
	0xAC,
	0xFF,
};

static u8 SEQ_ILI7807_A14X_02_188[] = {
	0xC6,
	0x14,
};

static u8 SEQ_ILI7807_A14X_02_189[] = {
	0xFF,
	0x78, 0x07, 0x11,
};

static u8 SEQ_ILI7807_A14X_02_190[] = {
	0x38,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_191[] = {
	0x39,
	0x04,
};

static u8 SEQ_ILI7807_A14X_02_192[] = {
	0x50,
	0x15,
};

static u8 SEQ_ILI7807_A14X_02_193[] = {
	0x51,
	0x05,
};

static u8 SEQ_ILI7807_A14X_02_194[] = {
	0xFF,
	0x78, 0x07, 0x02,
};

static u8 SEQ_ILI7807_A14X_02_195[] = {
	0x1B,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_196[] = {
	0x46,
	0x21,
};

static u8 SEQ_ILI7807_A14X_02_197[] = {
	0x47,
	0x03,
};

static u8 SEQ_ILI7807_A14X_02_198[] = {
	0x4F,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_199[] = {
	0x4C,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_200[] = {
	0x5B,
	0x50,
};

static u8 SEQ_ILI7807_A14X_02_201[] = {
	0xFF,
	0x78, 0x07, 0x12,
};

static u8 SEQ_ILI7807_A14X_02_202[] = {
	0x48,
	0x04,
};

static u8 SEQ_ILI7807_A14X_02_203[] = {
	0x49,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_204[] = {
	0x4A,
	0x06,
};

static u8 SEQ_ILI7807_A14X_02_205[] = {
	0x4B,
	0x20,
};

static u8 SEQ_ILI7807_A14X_02_206[] = {
	0x4E,
	0x05,
};

static u8 SEQ_ILI7807_A14X_02_207[] = {
	0x52,
	0x1F,
};

static u8 SEQ_ILI7807_A14X_02_208[] = {
	0x53,
	0x25,
};

static u8 SEQ_ILI7807_A14X_02_209[] = {
	0xC8,
	0x47,
};

static u8 SEQ_ILI7807_A14X_02_210[] = {
	0xC9,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_211[] = {
	0xCA,
	0x2C,
};

static u8 SEQ_ILI7807_A14X_02_212[] = {
	0xCB,
	0x28,
};

static u8 SEQ_ILI7807_A14X_02_213[] = {
	0xFF,
	0x78, 0x07, 0x04,
};

static u8 SEQ_ILI7807_A14X_02_214[] = {
	0xBD,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_215[] = {
	0xFF,
	0x78, 0x07, 0x05,
};

static u8 SEQ_ILI7807_A14X_02_216[] = {
	0x1D,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_217[] = {
	0x1E,
	0x87,
};

static u8 SEQ_ILI7807_A14X_02_218[] = {
	0x72,
	0x56,
};

static u8 SEQ_ILI7807_A14X_02_219[] = {
	0x74,
	0x56,
};

static u8 SEQ_ILI7807_A14X_02_220[] = {
	0x76,
	0x51,
};

static u8 SEQ_ILI7807_A14X_02_221[] = {
	0x7A,
	0x51,
};

static u8 SEQ_ILI7807_A14X_02_222[] = {
	0x7B,
	0x88,
};

static u8 SEQ_ILI7807_A14X_02_223[] = {
	0x7C,
	0x74,
};

static u8 SEQ_ILI7807_A14X_02_224[] = {
	0x46,
	0x5E,
};

static u8 SEQ_ILI7807_A14X_02_225[] = {
	0x47,
	0x5E,
};

static u8 SEQ_ILI7807_A14X_02_226[] = {
	0xB5,
	0x55,
};

static u8 SEQ_ILI7807_A14X_02_227[] = {
	0xB7,
	0x55,
};

static u8 SEQ_ILI7807_A14X_02_228[] = {
	0xC6,
	0x1B,
};

static u8 SEQ_ILI7807_A14X_02_229[] = {
	0x56,
	0xFF,
};

static u8 SEQ_ILI7807_A14X_02_230[] = {
	0x3E,
	0x50,
};

static u8 SEQ_ILI7807_A14X_02_231[] = {
	0xFF,
	0x78, 0x07, 0x06,
};

static u8 SEQ_ILI7807_A14X_02_232[] = {
	0xC0,
	0x68,
};

static u8 SEQ_ILI7807_A14X_02_233[] = {
	0xC1,
	0x19,
};

static u8 SEQ_ILI7807_A14X_02_234[] = {
	0xC3,
	0x06,
};

static u8 SEQ_ILI7807_A14X_02_235[] = {
	0x13,
	0x13,
};

static u8 SEQ_ILI7807_A14X_02_236[] = {
	0xFF,
	0x78, 0x07, 0x07,
};

static u8 SEQ_ILI7807_A14X_02_237[] = {
	0x29,
	0xCF,
};

static u8 SEQ_ILI7807_A14X_02_238[] = {
	0xFF,
	0x78, 0x07, 0x17,
};

static u8 SEQ_ILI7807_A14X_02_239[] = {
	0x20,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x89, 0x30,
	0x80, 0x09, 0x68, 0x04, 0x38, 0x00, 0x08, 0x02, 0x1C, 0x02,
	0x1C, 0x00, 0xAA, 0x02, 0x0E, 0x00, 0x20, 0x00, 0x2B, 0x00,
	0x07, 0x00, 0x0C, 0x0D, 0xB7, 0x0C, 0xB7, 0x18, 0x00, 0x1B,
	0xA0, 0x03, 0x0C, 0x20, 0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E,
	0x1C, 0x2A, 0x38, 0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79,
	0x7B, 0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40, 0x09,
	0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8, 0x1A, 0x38, 0x1A,
	0x78, 0x1A, 0xB6, 0x2A, 0xF6, 0x2B, 0x34, 0x2B, 0x74, 0x3B,
	0x74, 0x6B, 0xF4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static u8 SEQ_ILI7807_A14X_02_240[] = {
	0xFF,
	0x78, 0x07, 0x08,
};

static u8 SEQ_ILI7807_A14X_02_241[] = {
	0xE0,
	0x00, 0x00, 0x1A, 0x43, 0x00, 0x7A, 0xA6, 0xCB, 0x15, 0x04,
	0x2F, 0x72, 0x25, 0xA2, 0xEE, 0x28, 0x2A, 0x61, 0xA2, 0xCA,
	0x3E, 0xFC, 0x23, 0x4D, 0x3F, 0x66, 0x8B, 0xBE, 0x0F, 0xD3,
	0xD7,
};

static u8 SEQ_ILI7807_A14X_02_242[] = {
	0xE1,
	0x00, 0x00, 0x1A, 0x43, 0x00, 0x7A, 0xA6, 0xCB, 0x15, 0x04,
	0x2F, 0x72, 0x25, 0xA2, 0xEE, 0x28, 0x2A, 0x61, 0xA2, 0xCA,
	0x3E, 0xFC, 0x23, 0x4D, 0x3F, 0x66, 0x8B, 0xBE, 0x0F, 0xD3,
	0xD7,
};

static u8 SEQ_ILI7807_A14X_02_243[] = {
	0xFF,
	0x78, 0x07, 0x0B,
};

static u8 SEQ_ILI7807_A14X_02_244[] = {
	0xC6,
	0x85,
};

static u8 SEQ_ILI7807_A14X_02_245[] = {
	0xC7,
	0x6C,
};

static u8 SEQ_ILI7807_A14X_02_246[] = {
	0xC8,
	0x04,
};

static u8 SEQ_ILI7807_A14X_02_247[] = {
	0xC9,
	0x04,
};

static u8 SEQ_ILI7807_A14X_02_248[] = {
	0xCA,
	0x87,
};

static u8 SEQ_ILI7807_A14X_02_249[] = {
	0xCB,
	0x87,
};

static u8 SEQ_ILI7807_A14X_02_250[] = {
	0xD8,
	0x08,
};

static u8 SEQ_ILI7807_A14X_02_251[] = {
	0xD9,
	0xEA,
};

static u8 SEQ_ILI7807_A14X_02_252[] = {
	0xDA,
	0x06,
};

static u8 SEQ_ILI7807_A14X_02_253[] = {
	0xDB,
	0x06,
};

static u8 SEQ_ILI7807_A14X_02_254[] = {
	0xDC,
	0xDE,
};

static u8 SEQ_ILI7807_A14X_02_255[] = {
	0xDD,
	0xDE,
};

static u8 SEQ_ILI7807_A14X_02_256[] = {
	0xAA,
	0x12,
};

static u8 SEQ_ILI7807_A14X_02_257[] = {
	0xAB,
	0xE0,
};

static u8 SEQ_ILI7807_A14X_02_258[] = {
	0xFF,
	0x78, 0x07, 0x0C,
};

static u8 SEQ_ILI7807_A14X_02_259[] = {
	0x40,
	0x3F,
};

static u8 SEQ_ILI7807_A14X_02_260[] = {
	0x41,
	0x98,
};

static u8 SEQ_ILI7807_A14X_02_261[] = {
	0x42,
	0x3F,
};

static u8 SEQ_ILI7807_A14X_02_262[] = {
	0x43,
	0x9A,
};

static u8 SEQ_ILI7807_A14X_02_263[] = {
	0x44,
	0x3F,
};

static u8 SEQ_ILI7807_A14X_02_264[] = {
	0x45,
	0xA0,
};

static u8 SEQ_ILI7807_A14X_02_265[] = {
	0x46,
	0x3F,
};

static u8 SEQ_ILI7807_A14X_02_266[] = {
	0x47,
	0x96,
};

static u8 SEQ_ILI7807_A14X_02_267[] = {
	0x48,
	0x3F,
};

static u8 SEQ_ILI7807_A14X_02_268[] = {
	0x49,
	0x9C,
};

static u8 SEQ_ILI7807_A14X_02_269[] = {
	0x4A,
	0x3F,
};

static u8 SEQ_ILI7807_A14X_02_270[] = {
	0x4B,
	0x9E,
};

static u8 SEQ_ILI7807_A14X_02_271[] = {
	0xFF,
	0x78, 0x07, 0x0E,
};

static u8 SEQ_ILI7807_A14X_02_272[] = {
	0x00,
	0xA3,
};

static u8 SEQ_ILI7807_A14X_02_273[] = {
	0x02,
	0x0F,
};

static u8 SEQ_ILI7807_A14X_02_274[] = {
	0x04,
	0x06,
};

static u8 SEQ_ILI7807_A14X_02_275[] = {
	0x0B,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_276[] = {
	0x13,
	0x04,
};

static u8 SEQ_ILI7807_A14X_02_277[] = {
	0xB0,
	0x21,
};

static u8 SEQ_ILI7807_A14X_02_278[] = {
	0xC0,
	0x12,
};

static u8 SEQ_ILI7807_A14X_02_279[] = {
	0x05,
	0x20,
};

static u8 SEQ_ILI7807_A14X_02_280[] = {
	0xFF,
	0x78, 0x07, 0x1E,
};

static u8 SEQ_ILI7807_A14X_02_281[] = {
	0x20,
	0x04,
};

static u8 SEQ_ILI7807_A14X_02_282[] = {
	0x21,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_283[] = {
	0x22,
	0x06,
};

static u8 SEQ_ILI7807_A14X_02_284[] = {
	0x23,
	0x20,
};

static u8 SEQ_ILI7807_A14X_02_285[] = {
	0x24,
	0x05,
};

static u8 SEQ_ILI7807_A14X_02_286[] = {
	0xBD,
	0x01,
};

static u8 SEQ_ILI7807_A14X_02_287[] = {
	0xB1,
	0x1F,
};

static u8 SEQ_ILI7807_A14X_02_288[] = {
	0x61,
	0x1E,
};

static u8 SEQ_ILI7807_A14X_02_289[] = {
	0x62,
	0x03,
};

static u8 SEQ_ILI7807_A14X_02_290[] = {
	0x63,
	0x1E,
};

static u8 SEQ_ILI7807_A14X_02_291[] = {
	0x64,
	0x83,
};

static u8 SEQ_ILI7807_A14X_02_292[] = {
	0x60,
	0x07,
};

static u8 SEQ_ILI7807_A14X_02_293[] = {
	0x65,
	0x0A,
};

static u8 SEQ_ILI7807_A14X_02_294[] = {
	0x66,
	0xCF,
};

static u8 SEQ_ILI7807_A14X_02_295[] = {
	0x67,
	0x10,
};

static u8 SEQ_ILI7807_A14X_02_296[] = {
	0x69,
	0x2D,
};

static u8 SEQ_ILI7807_A14X_02_297[] = {
	0x16,
	0x3E,
};

static u8 SEQ_ILI7807_A14X_02_298[] = {
	0x1E,
	0x3E,
};

static u8 SEQ_ILI7807_A14X_02_299[] = {
	0x1F,
	0x3E,
};

static u8 SEQ_ILI7807_A14X_02_300[] = {
	0x6D,
	0x7A,
};

static u8 SEQ_ILI7807_A14X_02_301[] = {
	0x70,
	0x00,
};

static u8 SEQ_ILI7807_A14X_02_302[] = {
	0x6B,
	0x05,
};

static u8 SEQ_ILI7807_A14X_02_303[] = {
	0xFF,
	0x78, 0x07, 0x00,
};

static u8 SEQ_ILI7807_A14X_02_304[] = {
	0x51,
	0x0F, 0xF0,
};

static u8 SEQ_ILI7807_A14X_02_305[] = {
	0x53,
	0x2C,
};

static u8 SEQ_ILI7807_A14X_02_306[] = {
	0x55,
	0x01,
};

static DEFINE_STATIC_PACKET(ili7807_a14x_02_sleep_out, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_sleep_in, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_display_on, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_display_off, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_brightness_on, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_BRIGHTNESS_ON, 0);

static DEFINE_PKTUI(ili7807_a14x_02_brightness, &ili7807_a14x_02_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(ili7807_a14x_02_brightness, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(ili7807_a14x_02_001, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_001, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_002, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_002, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_003, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_003, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_004, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_004, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_005, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_005, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_006, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_006, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_007, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_007, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_008, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_008, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_009, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_009, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_010, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_010, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_011, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_011, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_012, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_012, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_013, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_013, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_014, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_014, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_015, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_015, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_016, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_016, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_017, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_017, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_018, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_018, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_019, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_019, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_020, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_020, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_021, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_021, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_022, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_022, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_023, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_023, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_024, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_024, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_025, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_025, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_026, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_026, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_027, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_027, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_028, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_028, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_029, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_029, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_030, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_030, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_031, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_031, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_032, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_032, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_033, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_033, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_034, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_034, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_035, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_035, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_036, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_036, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_037, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_037, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_038, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_038, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_039, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_039, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_040, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_040, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_041, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_041, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_042, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_042, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_043, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_043, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_044, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_044, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_045, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_045, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_046, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_046, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_047, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_047, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_048, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_048, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_049, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_049, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_050, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_050, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_051, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_051, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_052, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_052, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_053, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_053, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_054, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_054, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_055, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_055, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_056, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_056, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_057, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_057, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_058, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_058, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_059, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_059, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_060, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_060, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_061, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_061, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_062, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_062, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_063, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_063, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_064, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_064, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_065, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_065, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_066, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_066, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_067, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_067, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_068, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_068, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_069, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_069, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_070, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_070, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_071, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_071, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_072, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_072, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_073, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_073, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_074, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_074, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_075, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_075, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_076, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_076, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_077, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_077, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_078, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_078, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_079, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_079, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_080, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_080, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_081, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_081, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_082, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_082, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_083, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_083, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_084, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_084, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_085, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_085, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_086, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_086, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_087, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_087, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_088, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_088, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_089, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_089, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_090, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_090, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_091, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_091, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_092, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_092, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_093, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_093, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_094, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_094, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_095, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_095, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_096, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_096, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_097, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_097, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_098, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_098, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_099, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_099, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_100, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_100, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_101, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_101, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_102, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_102, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_103, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_103, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_104, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_104, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_105, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_105, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_106, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_106, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_107, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_107, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_108, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_108, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_109, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_109, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_110, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_110, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_111, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_111, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_112, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_112, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_113, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_113, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_114, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_114, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_115, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_115, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_116, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_116, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_117, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_117, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_118, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_118, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_119, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_119, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_120, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_120, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_121, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_121, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_122, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_122, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_123, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_123, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_124, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_124, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_125, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_125, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_126, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_126, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_127, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_127, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_128, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_128, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_129, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_129, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_130, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_130, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_131, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_131, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_132, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_132, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_133, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_133, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_134, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_134, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_135, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_135, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_136, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_136, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_137, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_137, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_138, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_138, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_139, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_139, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_140, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_140, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_141, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_141, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_142, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_142, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_143, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_143, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_144, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_144, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_145, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_145, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_146, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_146, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_147, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_147, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_148, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_148, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_149, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_149, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_150, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_150, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_151, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_151, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_152, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_152, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_153, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_153, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_154, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_154, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_155, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_155, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_156, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_156, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_157, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_157, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_158, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_158, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_159, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_159, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_160, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_160, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_161, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_161, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_162, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_162, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_163, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_163, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_164, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_164, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_165, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_165, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_166, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_166, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_167, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_167, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_168, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_168, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_169, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_169, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_170, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_170, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_171, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_171, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_172, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_172, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_173, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_173, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_174, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_174, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_175, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_175, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_176, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_176, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_177, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_177, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_178, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_178, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_179, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_179, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_180, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_180, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_181, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_181, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_182, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_182, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_183, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_183, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_184, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_184, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_185, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_185, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_186, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_186, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_187, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_187, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_188, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_188, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_189, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_189, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_190, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_190, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_191, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_191, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_192, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_192, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_193, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_193, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_194, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_194, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_195, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_195, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_196, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_196, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_197, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_197, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_198, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_198, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_199, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_199, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_200, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_200, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_201, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_201, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_202, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_202, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_203, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_203, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_204, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_204, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_205, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_205, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_206, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_206, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_207, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_207, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_208, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_208, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_209, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_209, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_210, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_210, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_211, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_211, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_212, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_212, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_213, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_213, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_214, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_214, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_215, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_215, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_216, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_216, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_217, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_217, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_218, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_218, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_219, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_219, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_220, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_220, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_221, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_221, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_222, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_222, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_223, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_223, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_224, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_224, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_225, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_225, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_226, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_226, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_227, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_227, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_228, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_228, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_229, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_229, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_230, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_230, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_231, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_231, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_232, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_232, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_233, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_233, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_234, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_234, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_235, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_235, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_236, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_236, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_237, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_237, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_238, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_238, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_239, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_239, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_240, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_240, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_241, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_241, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_242, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_242, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_243, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_243, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_244, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_244, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_245, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_245, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_246, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_246, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_247, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_247, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_248, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_248, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_249, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_249, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_250, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_250, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_251, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_251, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_252, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_252, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_253, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_253, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_254, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_254, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_255, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_255, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_256, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_256, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_257, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_257, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_258, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_258, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_259, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_259, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_260, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_260, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_261, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_261, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_262, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_262, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_263, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_263, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_264, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_264, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_265, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_265, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_266, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_266, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_267, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_267, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_268, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_268, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_269, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_269, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_270, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_270, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_271, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_271, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_272, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_272, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_273, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_273, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_274, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_274, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_275, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_275, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_276, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_276, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_277, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_277, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_278, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_278, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_279, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_279, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_280, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_280, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_281, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_281, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_282, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_282, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_283, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_283, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_284, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_284, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_285, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_285, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_286, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_286, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_287, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_287, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_288, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_288, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_289, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_289, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_290, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_290, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_291, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_291, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_292, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_292, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_293, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_293, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_294, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_294, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_295, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_295, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_296, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_296, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_297, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_297, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_298, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_298, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_299, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_299, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_300, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_300, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_301, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_301, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_302, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_302, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_303, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_303, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_304, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_304, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_305, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_305, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_306, DSI_PKT_TYPE_WR, SEQ_ILI7807_A14X_02_306, 0);

#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
static u8 ILI7807_A14X_02_VCOM_TRIM_ACCESS_1[] = {
	0xFF,
	0x78, 0x07, 0x05,
};
static DEFINE_STATIC_PACKET(ili7807_a14x_02_vcom_trim_access_1, DSI_PKT_TYPE_WR, ILI7807_A14X_02_VCOM_TRIM_ACCESS_1, 0);

static u8 ILI7807_A14X_02_VCOM_TRIM_ACCESS_2[] = {
	0xFF,
	0x78, 0x07, 0x07,
};
static DEFINE_STATIC_PACKET(ili7807_a14x_02_vcom_trim_access_2, DSI_PKT_TYPE_WR, ILI7807_A14X_02_VCOM_TRIM_ACCESS_2, 0);

static u8 ILI7807_A14X_02_VCOM_TRIM_ACCESS_END[] = {
	0xFF,
	0x78, 0x07, 0x00,
};
static DEFINE_STATIC_PACKET(ili7807_a14x_02_vcom_trim_access_end, DSI_PKT_TYPE_WR, ILI7807_A14X_02_VCOM_TRIM_ACCESS_END, 0);

static u8 ILI7807_A14X_02_VCOM_TRIM[ILI7807_A14X_02_VCOM_TRIM_LEN];
static DEFINE_RDINFO(ili7807_a14x_02_vcom_trim, DSI_PKT_TYPE_RD,
	ILI7807_A14X_02_VCOM_TRIM_REG, ILI7807_A14X_02_VCOM_TRIM_OFS, ILI7807_A14X_02_VCOM_TRIM_LEN);
static DECLARE_RESUI(ili7807_a14x_02_vcom_trim) = {
	{ .rditbl = &RDINFO(ili7807_a14x_02_vcom_trim), .offset = 0 },
};
static DEFINE_RESOURCE(ili7807_a14x_02_vcom_trim, ILI7807_A14X_02_VCOM_TRIM, RESUI(ili7807_a14x_02_vcom_trim));

static u8 ILI7807_A14X_02_VCOM_MARK1[ILI7807_A14X_02_VCOM_MARK1_LEN];
static DEFINE_RDINFO(ili7807_a14x_02_vcom_mark1, DSI_PKT_TYPE_RD,
	ILI7807_A14X_02_VCOM_MARK1_REG, ILI7807_A14X_02_VCOM_MARK1_OFS, ILI7807_A14X_02_VCOM_MARK1_LEN);
static DECLARE_RESUI(ili7807_a14x_02_vcom_mark1) = {
	{ .rditbl = &RDINFO(ili7807_a14x_02_vcom_mark1), .offset = 0 },
};
static DEFINE_RESOURCE(ili7807_a14x_02_vcom_mark1, ILI7807_A14X_02_VCOM_MARK1, RESUI(ili7807_a14x_02_vcom_mark1));

static u8 ILI7807_A14X_02_VCOM_MARK2[ILI7807_A14X_02_VCOM_MARK2_LEN];
static DEFINE_RDINFO(ili7807_a14x_02_vcom_mark2, DSI_PKT_TYPE_RD,
	ILI7807_A14X_02_VCOM_MARK2_REG, ILI7807_A14X_02_VCOM_MARK2_OFS, ILI7807_A14X_02_VCOM_MARK2_LEN);
static DECLARE_RESUI(ili7807_a14x_02_vcom_mark2) = {
	{ .rditbl = &RDINFO(ili7807_a14x_02_vcom_mark2), .offset = 0 },
};
static DEFINE_RESOURCE(ili7807_a14x_02_vcom_mark2, ILI7807_A14X_02_VCOM_MARK2, RESUI(ili7807_a14x_02_vcom_mark2));
#endif
static DEFINE_PANEL_MDELAY(ili7807_a14x_02_wait_20msec, 20); /* 1 frame */
static DEFINE_PANEL_MDELAY(ili7807_a14x_02_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(ili7807_a14x_02_wait_100msec, 100);

static DEFINE_RULE_BASED_COND(ili7807_a14x_02_cond_is_fps_90hz,
		PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 90);

static u8 ILI7807_A14X_02_ID[TFT_COMMON_ID_LEN];
static DEFINE_RDINFO(ili7807_a14x_02_id1, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DA_OFS, TFT_COMMON_ID_DA_LEN);
static DEFINE_RDINFO(ili7807_a14x_02_id2, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DB_OFS, TFT_COMMON_ID_DB_LEN);
static DEFINE_RDINFO(ili7807_a14x_02_id3, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DC_OFS, TFT_COMMON_ID_DC_LEN);
static DECLARE_RESUI(ili7807_a14x_02_id) = {
	{ .rditbl = &RDINFO(ili7807_a14x_02_id1), .offset = 0 },
	{ .rditbl = &RDINFO(ili7807_a14x_02_id2), .offset = 1 },
	{ .rditbl = &RDINFO(ili7807_a14x_02_id3), .offset = 2 },
};
static DEFINE_RESOURCE(ili7807_a14x_02_id, ILI7807_A14X_02_ID, RESUI(ili7807_a14x_02_id));

static void *ili7807_a14x_02_init_cmdtbl[] = {
	&PKTINFO(ili7807_a14x_02_001),
	&PKTINFO(ili7807_a14x_02_001),
	&PKTINFO(ili7807_a14x_02_002),
	&PKTINFO(ili7807_a14x_02_003),
	&PKTINFO(ili7807_a14x_02_004),
	&PKTINFO(ili7807_a14x_02_005),
	&PKTINFO(ili7807_a14x_02_006),
	&PKTINFO(ili7807_a14x_02_007),
	&PKTINFO(ili7807_a14x_02_008),
	&PKTINFO(ili7807_a14x_02_009),
	&PKTINFO(ili7807_a14x_02_010),
	&PKTINFO(ili7807_a14x_02_011),
	&PKTINFO(ili7807_a14x_02_012),
	&PKTINFO(ili7807_a14x_02_013),
	&PKTINFO(ili7807_a14x_02_014),
	&PKTINFO(ili7807_a14x_02_015),
	&PKTINFO(ili7807_a14x_02_016),
	&PKTINFO(ili7807_a14x_02_017),
	&PKTINFO(ili7807_a14x_02_018),
	&PKTINFO(ili7807_a14x_02_019),
	&PKTINFO(ili7807_a14x_02_020),
	&PKTINFO(ili7807_a14x_02_021),
	&PKTINFO(ili7807_a14x_02_022),
	&PKTINFO(ili7807_a14x_02_023),
	&PKTINFO(ili7807_a14x_02_024),
	&PKTINFO(ili7807_a14x_02_025),
	&PKTINFO(ili7807_a14x_02_026),
	&PKTINFO(ili7807_a14x_02_027),
	&PKTINFO(ili7807_a14x_02_028),
	&PKTINFO(ili7807_a14x_02_029),
	&PKTINFO(ili7807_a14x_02_030),
	&PKTINFO(ili7807_a14x_02_031),
	&PKTINFO(ili7807_a14x_02_032),
	&PKTINFO(ili7807_a14x_02_033),
	&PKTINFO(ili7807_a14x_02_034),
	&PKTINFO(ili7807_a14x_02_035),
	&PKTINFO(ili7807_a14x_02_036),
	&PKTINFO(ili7807_a14x_02_037),
	&PKTINFO(ili7807_a14x_02_038),
	&PKTINFO(ili7807_a14x_02_039),
	&PKTINFO(ili7807_a14x_02_040),
	&PKTINFO(ili7807_a14x_02_041),
	&PKTINFO(ili7807_a14x_02_042),
	&PKTINFO(ili7807_a14x_02_043),
	&PKTINFO(ili7807_a14x_02_044),
	&PKTINFO(ili7807_a14x_02_045),
	&PKTINFO(ili7807_a14x_02_046),
	&PKTINFO(ili7807_a14x_02_047),
	&PKTINFO(ili7807_a14x_02_048),
	&PKTINFO(ili7807_a14x_02_049),
	&PKTINFO(ili7807_a14x_02_050),
	&PKTINFO(ili7807_a14x_02_051),
	&PKTINFO(ili7807_a14x_02_052),
	&PKTINFO(ili7807_a14x_02_053),
	&PKTINFO(ili7807_a14x_02_054),
	&PKTINFO(ili7807_a14x_02_055),
	&PKTINFO(ili7807_a14x_02_056),
	&PKTINFO(ili7807_a14x_02_057),
	&PKTINFO(ili7807_a14x_02_058),
	&PKTINFO(ili7807_a14x_02_059),
	&PKTINFO(ili7807_a14x_02_060),
	&PKTINFO(ili7807_a14x_02_061),
	&PKTINFO(ili7807_a14x_02_062),
	&PKTINFO(ili7807_a14x_02_063),
	&PKTINFO(ili7807_a14x_02_064),
	&PKTINFO(ili7807_a14x_02_065),
	&PKTINFO(ili7807_a14x_02_066),
	&PKTINFO(ili7807_a14x_02_067),
	&PKTINFO(ili7807_a14x_02_068),
	&PKTINFO(ili7807_a14x_02_069),
	&PKTINFO(ili7807_a14x_02_070),
	&PKTINFO(ili7807_a14x_02_071),
	&PKTINFO(ili7807_a14x_02_072),
	&PKTINFO(ili7807_a14x_02_073),
	&PKTINFO(ili7807_a14x_02_074),
	&PKTINFO(ili7807_a14x_02_075),
	&PKTINFO(ili7807_a14x_02_076),
	&PKTINFO(ili7807_a14x_02_077),
	&PKTINFO(ili7807_a14x_02_078),
	&PKTINFO(ili7807_a14x_02_079),
	&PKTINFO(ili7807_a14x_02_080),
	&PKTINFO(ili7807_a14x_02_081),
	&PKTINFO(ili7807_a14x_02_082),
	&PKTINFO(ili7807_a14x_02_083),
	&PKTINFO(ili7807_a14x_02_084),
	&PKTINFO(ili7807_a14x_02_085),
	&PKTINFO(ili7807_a14x_02_086),
	&PKTINFO(ili7807_a14x_02_087),
	&PKTINFO(ili7807_a14x_02_088),
	&PKTINFO(ili7807_a14x_02_089),
	&PKTINFO(ili7807_a14x_02_090),
	&PKTINFO(ili7807_a14x_02_091),
	&PKTINFO(ili7807_a14x_02_092),
	&PKTINFO(ili7807_a14x_02_093),
	&PKTINFO(ili7807_a14x_02_094),
	&PKTINFO(ili7807_a14x_02_095),
	&PKTINFO(ili7807_a14x_02_096),
	&PKTINFO(ili7807_a14x_02_097),
	&PKTINFO(ili7807_a14x_02_098),
	&PKTINFO(ili7807_a14x_02_099),
	&PKTINFO(ili7807_a14x_02_100),
	&PKTINFO(ili7807_a14x_02_101),
	&PKTINFO(ili7807_a14x_02_102),
	&PKTINFO(ili7807_a14x_02_103),
	&PKTINFO(ili7807_a14x_02_104),
	&PKTINFO(ili7807_a14x_02_105),
	&PKTINFO(ili7807_a14x_02_106),
	&PKTINFO(ili7807_a14x_02_107),
	&PKTINFO(ili7807_a14x_02_108),
	&PKTINFO(ili7807_a14x_02_109),
	&PKTINFO(ili7807_a14x_02_110),
	&PKTINFO(ili7807_a14x_02_111),
	&PKTINFO(ili7807_a14x_02_112),
	&PKTINFO(ili7807_a14x_02_113),
	&PKTINFO(ili7807_a14x_02_114),
	&PKTINFO(ili7807_a14x_02_115),
	&PKTINFO(ili7807_a14x_02_116),
	&PKTINFO(ili7807_a14x_02_117),
	&PKTINFO(ili7807_a14x_02_118),
	&PKTINFO(ili7807_a14x_02_119),
	&PKTINFO(ili7807_a14x_02_120),
	&PKTINFO(ili7807_a14x_02_121),
	&PKTINFO(ili7807_a14x_02_122),
	&PKTINFO(ili7807_a14x_02_123),
	&PKTINFO(ili7807_a14x_02_124),
	&PKTINFO(ili7807_a14x_02_125),
	&PKTINFO(ili7807_a14x_02_126),
	&PKTINFO(ili7807_a14x_02_127),
	&PKTINFO(ili7807_a14x_02_128),
	&PKTINFO(ili7807_a14x_02_129),
	&PKTINFO(ili7807_a14x_02_130),
	&PKTINFO(ili7807_a14x_02_131),
	&PKTINFO(ili7807_a14x_02_132),
	&PKTINFO(ili7807_a14x_02_133),
	&PKTINFO(ili7807_a14x_02_134),
	&PKTINFO(ili7807_a14x_02_135),
	&PKTINFO(ili7807_a14x_02_136),
	&PKTINFO(ili7807_a14x_02_137),
	&PKTINFO(ili7807_a14x_02_138),
	&PKTINFO(ili7807_a14x_02_139),
	&PKTINFO(ili7807_a14x_02_140),
	&PKTINFO(ili7807_a14x_02_141),
	&PKTINFO(ili7807_a14x_02_142),
	&PKTINFO(ili7807_a14x_02_143),
	&PKTINFO(ili7807_a14x_02_144),
	&PKTINFO(ili7807_a14x_02_145),
	&PKTINFO(ili7807_a14x_02_146),
	&PKTINFO(ili7807_a14x_02_147),
	&PKTINFO(ili7807_a14x_02_148),
	&PKTINFO(ili7807_a14x_02_149),
	&PKTINFO(ili7807_a14x_02_150),
	&PKTINFO(ili7807_a14x_02_151),
	&PKTINFO(ili7807_a14x_02_152),
	&PKTINFO(ili7807_a14x_02_153),
	&PKTINFO(ili7807_a14x_02_154),
	&PKTINFO(ili7807_a14x_02_155),
	&PKTINFO(ili7807_a14x_02_156),
	&PKTINFO(ili7807_a14x_02_157),
	&PKTINFO(ili7807_a14x_02_158),
	&PKTINFO(ili7807_a14x_02_159),
	&PKTINFO(ili7807_a14x_02_160),
	&PKTINFO(ili7807_a14x_02_161),
	&PKTINFO(ili7807_a14x_02_162),
	&PKTINFO(ili7807_a14x_02_163),
	&PKTINFO(ili7807_a14x_02_164),
	&PKTINFO(ili7807_a14x_02_165),
	&PKTINFO(ili7807_a14x_02_166),
	&PKTINFO(ili7807_a14x_02_167),
	&PKTINFO(ili7807_a14x_02_168),
	&PKTINFO(ili7807_a14x_02_169),
	&PKTINFO(ili7807_a14x_02_170),
	&PKTINFO(ili7807_a14x_02_171),
	&PKTINFO(ili7807_a14x_02_172),
	&PKTINFO(ili7807_a14x_02_173),
	&PKTINFO(ili7807_a14x_02_174),
	&PKTINFO(ili7807_a14x_02_175),
	&PKTINFO(ili7807_a14x_02_176),
	&PKTINFO(ili7807_a14x_02_177),
	&PKTINFO(ili7807_a14x_02_178),
	&PKTINFO(ili7807_a14x_02_179),
	&PKTINFO(ili7807_a14x_02_180),
	&PKTINFO(ili7807_a14x_02_181),
	&PKTINFO(ili7807_a14x_02_182),
	&PKTINFO(ili7807_a14x_02_183),
	&PKTINFO(ili7807_a14x_02_184),
	&PKTINFO(ili7807_a14x_02_185),
	&PKTINFO(ili7807_a14x_02_186),
	&PKTINFO(ili7807_a14x_02_187),
	&PKTINFO(ili7807_a14x_02_188),
	&PKTINFO(ili7807_a14x_02_189),
	&PKTINFO(ili7807_a14x_02_190),
	&PKTINFO(ili7807_a14x_02_191),
	&PKTINFO(ili7807_a14x_02_192),
	&PKTINFO(ili7807_a14x_02_193),
	&PKTINFO(ili7807_a14x_02_194),
	&PKTINFO(ili7807_a14x_02_195),
	&PKTINFO(ili7807_a14x_02_196),
	&PKTINFO(ili7807_a14x_02_197),
	&PKTINFO(ili7807_a14x_02_198),
	&PKTINFO(ili7807_a14x_02_199),
	&PKTINFO(ili7807_a14x_02_200),
	&PKTINFO(ili7807_a14x_02_201),
	&PKTINFO(ili7807_a14x_02_202),
	&PKTINFO(ili7807_a14x_02_203),
	&PKTINFO(ili7807_a14x_02_204),
	&PKTINFO(ili7807_a14x_02_205),
	&PKTINFO(ili7807_a14x_02_206),
	&PKTINFO(ili7807_a14x_02_207),
	&PKTINFO(ili7807_a14x_02_208),
	&PKTINFO(ili7807_a14x_02_209),
	&PKTINFO(ili7807_a14x_02_210),
	&PKTINFO(ili7807_a14x_02_211),
	&PKTINFO(ili7807_a14x_02_212),
	&PKTINFO(ili7807_a14x_02_213),
	&PKTINFO(ili7807_a14x_02_214),
	&PKTINFO(ili7807_a14x_02_215),
	&PKTINFO(ili7807_a14x_02_216),
	&PKTINFO(ili7807_a14x_02_217),
	&PKTINFO(ili7807_a14x_02_218),
	&PKTINFO(ili7807_a14x_02_219),
	&PKTINFO(ili7807_a14x_02_220),
	&PKTINFO(ili7807_a14x_02_221),
	&PKTINFO(ili7807_a14x_02_222),
	&PKTINFO(ili7807_a14x_02_223),
	&PKTINFO(ili7807_a14x_02_224),
	&PKTINFO(ili7807_a14x_02_225),
	&PKTINFO(ili7807_a14x_02_226),
	&PKTINFO(ili7807_a14x_02_227),
	&PKTINFO(ili7807_a14x_02_228),
	&PKTINFO(ili7807_a14x_02_229),
	&PKTINFO(ili7807_a14x_02_230),
	&PKTINFO(ili7807_a14x_02_231),
	&PKTINFO(ili7807_a14x_02_232),
	&PKTINFO(ili7807_a14x_02_233),
	&PKTINFO(ili7807_a14x_02_234),
	&PKTINFO(ili7807_a14x_02_235),
	&PKTINFO(ili7807_a14x_02_236),
	&PKTINFO(ili7807_a14x_02_237),
	&PKTINFO(ili7807_a14x_02_238),
	&PKTINFO(ili7807_a14x_02_239),
	&PKTINFO(ili7807_a14x_02_240),
	&PKTINFO(ili7807_a14x_02_241),
	&PKTINFO(ili7807_a14x_02_242),
	&PKTINFO(ili7807_a14x_02_243),
	&PKTINFO(ili7807_a14x_02_244),
	&PKTINFO(ili7807_a14x_02_245),
	&PKTINFO(ili7807_a14x_02_246),
	&PKTINFO(ili7807_a14x_02_247),
	&PKTINFO(ili7807_a14x_02_248),
	&PKTINFO(ili7807_a14x_02_249),
	&PKTINFO(ili7807_a14x_02_250),
	&PKTINFO(ili7807_a14x_02_251),
	&PKTINFO(ili7807_a14x_02_252),
	&PKTINFO(ili7807_a14x_02_253),
	&PKTINFO(ili7807_a14x_02_254),
	&PKTINFO(ili7807_a14x_02_255),
	&PKTINFO(ili7807_a14x_02_256),
	&PKTINFO(ili7807_a14x_02_257),
	&PKTINFO(ili7807_a14x_02_258),
	&PKTINFO(ili7807_a14x_02_259),
	&PKTINFO(ili7807_a14x_02_260),
	&PKTINFO(ili7807_a14x_02_261),
	&PKTINFO(ili7807_a14x_02_262),
	&PKTINFO(ili7807_a14x_02_263),
	&PKTINFO(ili7807_a14x_02_264),
	&PKTINFO(ili7807_a14x_02_265),
	&PKTINFO(ili7807_a14x_02_266),
	&PKTINFO(ili7807_a14x_02_267),
	&PKTINFO(ili7807_a14x_02_268),
	&PKTINFO(ili7807_a14x_02_269),
	&PKTINFO(ili7807_a14x_02_270),
	&PKTINFO(ili7807_a14x_02_271),
	&PKTINFO(ili7807_a14x_02_272),
	&PKTINFO(ili7807_a14x_02_273),
	&PKTINFO(ili7807_a14x_02_274),
	&PKTINFO(ili7807_a14x_02_275),
	&PKTINFO(ili7807_a14x_02_276),
	&PKTINFO(ili7807_a14x_02_277),
	&PKTINFO(ili7807_a14x_02_278),
	&PKTINFO(ili7807_a14x_02_279),
	&PKTINFO(ili7807_a14x_02_280),
	&PKTINFO(ili7807_a14x_02_281),
	&PKTINFO(ili7807_a14x_02_282),
	&PKTINFO(ili7807_a14x_02_283),
	&PKTINFO(ili7807_a14x_02_284),
	&PKTINFO(ili7807_a14x_02_285),
	&PKTINFO(ili7807_a14x_02_286),
	&PKTINFO(ili7807_a14x_02_287),
	&PKTINFO(ili7807_a14x_02_288),
	&PKTINFO(ili7807_a14x_02_289),
	&PKTINFO(ili7807_a14x_02_290),
	&PKTINFO(ili7807_a14x_02_291),
	&PKTINFO(ili7807_a14x_02_292),
	&PKTINFO(ili7807_a14x_02_293),
	&PKTINFO(ili7807_a14x_02_294),
	&PKTINFO(ili7807_a14x_02_295),
	&PKTINFO(ili7807_a14x_02_296),
	&PKTINFO(ili7807_a14x_02_297),
	&PKTINFO(ili7807_a14x_02_298),
	&PKTINFO(ili7807_a14x_02_299),
	&PKTINFO(ili7807_a14x_02_300),
	&PKTINFO(ili7807_a14x_02_301),
	&PKTINFO(ili7807_a14x_02_302),
	&PKTINFO(ili7807_a14x_02_303),
	&PKTINFO(ili7807_a14x_02_304),
	&PKTINFO(ili7807_a14x_02_305),
	&PKTINFO(ili7807_a14x_02_306),

	&PKTINFO(ili7807_a14x_02_sleep_out),
	&DLYINFO(ili7807_a14x_02_wait_100msec),
	&PKTINFO(ili7807_a14x_02_display_on),
};

static void *ili7807_a14x_02_res_init_cmdtbl[] = {
	&RESINFO(ili7807_a14x_02_id),
};

static void *ili7807_a14x_02_set_bl_cmdtbl[] = {
	&PKTINFO(ili7807_a14x_02_brightness),
};

static void *ili7807_a14x_02_display_on_cmdtbl[] = {
	&DLYINFO(ili7807_a14x_02_wait_40msec),
	&PKTINFO(ili7807_a14x_02_brightness),
	&PKTINFO(ili7807_a14x_02_brightness_on),
};

static void *ili7807_a14x_02_display_off_cmdtbl[] = {
	&PKTINFO(ili7807_a14x_02_display_off),
	&DLYINFO(ili7807_a14x_02_wait_20msec),
};

static void *ili7807_a14x_02_exit_cmdtbl[] = {
	&PKTINFO(ili7807_a14x_02_sleep_in),
};

#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
static void *ili7807_a14x_02_vcom_trim_test_cmdtbl[] = {
	&PKTINFO(ili7807_a14x_02_vcom_trim_access_1),
	&RESINFO(ili7807_a14x_02_vcom_trim),
	&DLYINFO(ili7807_a14x_02_wait_20msec),
	&PKTINFO(ili7807_a14x_02_vcom_trim_access_2),
	&RESINFO(ili7807_a14x_02_vcom_mark1),
	&DLYINFO(ili7807_a14x_02_wait_20msec),
	&RESINFO(ili7807_a14x_02_vcom_mark2),
	&DLYINFO(ili7807_a14x_02_wait_20msec),
	&PKTINFO(ili7807_a14x_02_vcom_trim_access_end),
};
#endif

static void *ili7807_a14x_02_display_mode_cmdtbl[] = {
	/* dummy cmd */
	&CONDINFO_IF(ili7807_a14x_02_cond_is_fps_90hz),
	&CONDINFO_EL(ili7807_a14x_02_cond_is_fps_90hz),
	&CONDINFO_FI(ili7807_a14x_02_cond_is_fps_90hz),
};

static struct seqinfo ili7807_a14x_02_seqtbl[] = {
	SEQINFO_INIT(PANEL_INIT_SEQ, ili7807_a14x_02_init_cmdtbl),
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, ili7807_a14x_02_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_SET_BL_SEQ, ili7807_a14x_02_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, ili7807_a14x_02_display_mode_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, ili7807_a14x_02_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, ili7807_a14x_02_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, ili7807_a14x_02_exit_cmdtbl),
#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
	SEQINFO_INIT(PANEL_VCOM_TRIM_TEST_SEQ, ili7807_a14x_02_vcom_trim_test_cmdtbl),
#endif
};

/* BLIC SETTING START */
static u8 ILI7807_A14X_02_KTZ8864_I2C_INIT[] = {
	0x0C, 0x24,
	0x0D, 0x1E,
	0x0E, 0x1E,
	0x09, 0x99,
	0x02, 0x6B,
	0x03, 0x0D,
	0x11, 0x74,
	0x04, 0x07,
	0x05, 0xAD,
	0x10, 0x66,
	0x08, 0x13,
};

static u8 ILI7807_A14X_02_KTZ8864_I2C_EXIT_BLEN[] = {
	0x08, 0x00,
};

#ifdef DEBUG_I2C_READ
static u8 ILI7807_A14X_02_KTZ8864_I2C_DUMP[] = {
	0x0C, 0x00,
	0x0D, 0x00,
	0x0E, 0x00,
	0x09, 0x00,
	0x09, 0x00,
	0x02, 0x00,
	0x03, 0x00,
	0x11, 0x00,
	0x04, 0x00,
	0x05, 0x00,
	0x10, 0x00,
	0x08, 0x00,
};
#endif

static DEFINE_STATIC_PACKET(ili7807_a14x_02_ktz8864_i2c_init, I2C_PKT_TYPE_WR, ILI7807_A14X_02_KTZ8864_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(ili7807_a14x_02_ktz8864_i2c_exit_blen, I2C_PKT_TYPE_WR, ILI7807_A14X_02_KTZ8864_I2C_EXIT_BLEN, 0);
#ifdef DEBUG_I2C_READ
static DEFINE_STATIC_PACKET(ili7807_a14x_02_ktz8864_i2c_dump, I2C_PKT_TYPE_RD, ILI7807_A14X_02_KTZ8864_I2C_DUMP, 0);
#endif

static void *ili7807_a14x_02_ktz8864_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(ili7807_a14x_02_ktz8864_i2c_dump),
#endif
	&PKTINFO(ili7807_a14x_02_ktz8864_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(ili7807_a14x_02_ktz8864_i2c_dump),
#endif
};

static void *ili7807_a14x_02_ktz8864_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(ili7807_a14x_02_ktz8864_i2c_dump),
#endif
	&PKTINFO(ili7807_a14x_02_ktz8864_i2c_exit_blen),
};

static struct seqinfo ili7807_a14x_02_ktz8864_seq_tbl[] = {
	SEQINFO_INIT(PANEL_BLIC_I2C_ON_SEQ, ili7807_a14x_02_ktz8864_init_cmdtbl),
	SEQINFO_INIT(PANEL_BLIC_I2C_OFF_SEQ, ili7807_a14x_02_ktz8864_exit_cmdtbl),
};

static struct blic_data ili7807_a14x_02_ktz8864_blic_data = {
	.name = "ktz8864",
	.seqtbl = ili7807_a14x_02_ktz8864_seq_tbl,
	.nr_seqtbl = ARRAY_SIZE(ili7807_a14x_02_ktz8864_seq_tbl),
};

static struct blic_data *ili7807_a14x_02_blic_tbl[] = {
	&ili7807_a14x_02_ktz8864_blic_data,
};
/* BLIC SETTING END */


struct common_panel_info ili7807_a14x_02_panel_info = {
	.ldi_name = "ili7807",
	.name = "ili7807_a14x_02",
	.model = "boe_6_58_inch",
	.vendor = "BOE",
	.id = 0x6B6250,
	.rev = 0,
	.ddi_props = {
		.err_fg_recovery = false,
		.support_vrr = true,
		.init_seq_by_lpdt = true,
	},
#if defined(CONFIG_USDM_PANEL_VCOM_TRIM_TEST)
	.ddi_ops = {
		.vcom_trim_test = ili7807_a14x_02_vcom_trim_test,
	},
#endif
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.common_panel_modes = &ili7807_a14x_02_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(ili7807_a14x_02_default_resol),
		.resol = ili7807_a14x_02_default_resol,
	},
	.vrrtbl = ili7807_a14x_02_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(ili7807_a14x_02_default_vrrtbl),
	.maptbl = ili7807_a14x_02_maptbl,
	.nr_maptbl = ARRAY_SIZE(ili7807_a14x_02_maptbl),
	.seqtbl = ili7807_a14x_02_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(ili7807_a14x_02_seqtbl),
	.rditbl = NULL,
	.nr_rditbl = 0,
	.restbl = NULL,
	.nr_restbl = 0,
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &ili7807_a14x_02_panel_dimming_info,
	},
	.blic_data_tbl = ili7807_a14x_02_blic_tbl,
	.nr_blic_data_tbl = ARRAY_SIZE(ili7807_a14x_02_blic_tbl),
};

#endif /* __ILI7807_A14X_02_PANEL_H__ */

