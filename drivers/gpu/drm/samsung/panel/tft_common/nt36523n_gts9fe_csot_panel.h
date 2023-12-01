/*
 * linux/drivers/video/fbdev/exynos/panel/hx83122a/nt36523n_gts9fe_csot_panel.h
 *
 * Header file for NT36523N Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NT36523N_GTS9FE_CSOT_PANEL_H__
#define __NT36523N_GTS9FE_CSOT_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "tft_common.h"
#include "tft_function.h"
#include "nt36523n_gts9fe_csot_resol.h"

#undef __pn_name__
#define __pn_name__	gts9fe

#undef __PN_NAME__
#define __PN_NAME__

#define NT36523N_NR_STEP (256)
#define NT36523N_HBM_STEP 45
#define NT36523N_TOTAL_STEP (NT36523N_NR_STEP + NT36523N_HBM_STEP) /* 0 ~ 256 */

static unsigned int nt36523n_gts9fe_csot_brt_tbl[NT36523N_TOTAL_STEP] = {
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

static unsigned int nt36523n_gts9fe_csot_step_cnt_tbl[NT36523N_TOTAL_STEP] = {
	[0 ... 255] = 1,
	/* HBM */
	[256 ... 300] = 1,
};

struct brightness_table nt36523n_gts9fe_csot_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = nt36523n_gts9fe_csot_brt_tbl,
	.sz_brt = ARRAY_SIZE(nt36523n_gts9fe_csot_brt_tbl),
	.sz_ui_brt = NT36523N_NR_STEP,
	.sz_hbm_brt = NT36523N_HBM_STEP,
	.lum = nt36523n_gts9fe_csot_brt_tbl,
	.sz_lum = ARRAY_SIZE(nt36523n_gts9fe_csot_brt_tbl),
	.sz_ui_lum = NT36523N_NR_STEP,
	.sz_hbm_lum = NT36523N_HBM_STEP,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = nt36523n_gts9fe_csot_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(nt36523n_gts9fe_csot_step_cnt_tbl),
	.vtotal = 0,
};

static struct panel_dimming_info nt36523n_gts9fe_csot_panel_dimming_info = {
	.name = "nt36523n_gts9fe",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = -1,
	.brt_tbl = &nt36523n_gts9fe_csot_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,	/* read dim flash when probe or not */
	.hbm_aor = NULL,
};

static u8 nt36523n_gts9fe_csot_brt_table[NT36523N_TOTAL_STEP][2] = {
	// platfom 0
	{ 0x00, 0x00 },
	// platfrom 1 ~ 255
	{ 0x00, 0x18 }, { 0x00, 0x18 }, { 0x00, 0x22 }, { 0x00, 0x2E }, { 0x00, 0x3A },
	{ 0x00, 0x45 }, { 0x00, 0x51 }, { 0x00, 0x5C }, { 0x00, 0x68 }, { 0x00, 0x74 },
	{ 0x00, 0x7F }, { 0x00, 0x8B }, { 0x00, 0x97 }, { 0x00, 0xA2 }, { 0x00, 0xAE },
	{ 0x00, 0xB9 }, { 0x00, 0xC5 }, { 0x00, 0xD1 }, { 0x00, 0xDC }, { 0x00, 0xE8 },
	{ 0x00, 0xF4 }, { 0x00, 0xFF }, { 0x01, 0x0B }, { 0x01, 0x16 }, { 0x01, 0x22 },
	{ 0x01, 0x2E }, { 0x01, 0x39 }, { 0x01, 0x45 }, { 0x01, 0x51 }, { 0x01, 0x5C },
	{ 0x01, 0x68 }, { 0x01, 0x73 }, { 0x01, 0x7F }, { 0x01, 0x8B }, { 0x01, 0x96 },
	{ 0x01, 0xA2 }, { 0x01, 0xAD }, { 0x01, 0xB9 }, { 0x01, 0xC5 }, { 0x01, 0xD0 },
	{ 0x01, 0xDC }, { 0x01, 0xE8 }, { 0x01, 0xF3 }, { 0x01, 0xFF }, { 0x02, 0x0A },
	{ 0x02, 0x16 }, { 0x02, 0x22 }, { 0x02, 0x2D }, { 0x02, 0x39 }, { 0x02, 0x45 },
	{ 0x02, 0x50 }, { 0x02, 0x5C }, { 0x02, 0x67 }, { 0x02, 0x73 }, { 0x02, 0x7F },
	{ 0x02, 0x8A }, { 0x02, 0x96 }, { 0x02, 0xA2 }, { 0x02, 0xAD }, { 0x02, 0xB9 },
	{ 0x02, 0xC4 }, { 0x02, 0xD0 }, { 0x02, 0xDC }, { 0x02, 0xE7 }, { 0x02, 0xF3 },
	{ 0x02, 0xFF }, { 0x03, 0x0A }, { 0x03, 0x16 }, { 0x03, 0x21 }, { 0x03, 0x2D },
	{ 0x03, 0x39 }, { 0x03, 0x44 }, { 0x03, 0x50 }, { 0x03, 0x5B }, { 0x03, 0x67 },
	{ 0x03, 0x73 }, { 0x03, 0x7E }, { 0x03, 0x8A }, { 0x03, 0x96 }, { 0x03, 0xA1 },
	{ 0x03, 0xAD }, { 0x03, 0xB8 }, { 0x03, 0xC4 }, { 0x03, 0xD0 }, { 0x03, 0xDB },
	{ 0x03, 0xE7 }, { 0x03, 0xF3 }, { 0x03, 0xFE }, { 0x04, 0x0A }, { 0x04, 0x15 },
	{ 0x04, 0x21 }, { 0x04, 0x2D }, { 0x04, 0x38 }, { 0x04, 0x44 }, { 0x04, 0x50 },
	{ 0x04, 0x5B }, { 0x04, 0x67 }, { 0x04, 0x72 }, { 0x04, 0x7E }, { 0x04, 0x8A },
	{ 0x04, 0x95 }, { 0x04, 0xA1 }, { 0x04, 0xAD }, { 0x04, 0xB8 }, { 0x04, 0xC4 },
	{ 0x04, 0xCF }, { 0x04, 0xDB }, { 0x04, 0xE7 }, { 0x04, 0xF2 }, { 0x04, 0xFE },
	{ 0x05, 0x09 }, { 0x05, 0x15 }, { 0x05, 0x21 }, { 0x05, 0x2C }, { 0x05, 0x38 },
	{ 0x05, 0x44 }, { 0x05, 0x4F }, { 0x05, 0x5B }, { 0x05, 0x66 }, { 0x05, 0x72 },
	{ 0x05, 0x7E }, { 0x05, 0x89 }, { 0x05, 0x95 }, { 0x05, 0xA1 }, { 0x05, 0xAC },
	{ 0x05, 0xB8 }, { 0x05, 0xC3 }, { 0x05, 0xCF }, { 0x05, 0xDB }, { 0x05, 0xE6 },
	{ 0x05, 0xF2 }, { 0x05, 0xFE }, { 0x06, 0x09 }, { 0x06, 0x15 }, { 0x06, 0x20 },
	{ 0x06, 0x2C }, { 0x06, 0x38 }, { 0x06, 0x43 }, { 0x06, 0x4F }, { 0x06, 0x5B },
	{ 0x06, 0x66 }, { 0x06, 0x72 }, { 0x06, 0x7D }, { 0x06, 0x89 }, { 0x06, 0x95 },
	{ 0x06, 0xA0 }, { 0x06, 0xAC }, { 0x06, 0xB7 }, { 0x06, 0xC3 }, { 0x06, 0xCF },
	{ 0x06, 0xDA }, { 0x06, 0xE6 }, { 0x06, 0xF2 }, { 0x06, 0xFD }, { 0x07, 0x09 },
	{ 0x07, 0x14 }, { 0x07, 0x20 }, { 0x07, 0x2C }, { 0x07, 0x37 }, { 0x07, 0x43 },
	{ 0x07, 0x4F }, { 0x07, 0x5A }, { 0x07, 0x66 }, { 0x07, 0x71 }, { 0x07, 0x7D },
	{ 0x07, 0x89 }, { 0x07, 0x94 }, { 0x07, 0xA0 }, { 0x07, 0xAC }, { 0x07, 0xB7 },
	{ 0x07, 0xC3 }, { 0x07, 0xCE }, { 0x07, 0xDA }, { 0x07, 0xE6 }, { 0x07, 0xF1 },
	{ 0x07, 0xFD }, { 0x08, 0x09 }, { 0x08, 0x14 }, { 0x08, 0x20 }, { 0x08, 0x2B },
	{ 0x08, 0x37 }, { 0x08, 0x43 }, { 0x08, 0x4E }, { 0x08, 0x5A }, { 0x08, 0x65 },
	{ 0x08, 0x71 }, { 0x08, 0x7D }, { 0x08, 0x88 }, { 0x08, 0x94 }, { 0x08, 0xA0 },
	{ 0x08, 0xAB }, { 0x08, 0xB7 }, { 0x08, 0xC2 }, { 0x08, 0xCE }, { 0x08, 0xDA },
	{ 0x08, 0xE5 }, { 0x08, 0xF1 }, { 0x08, 0xFD }, { 0x09, 0x08 }, { 0x09, 0x14 },
	{ 0x09, 0x1F }, { 0x09, 0x2B }, { 0x09, 0x37 }, { 0x09, 0x42 }, { 0x09, 0x4E },
	{ 0x09, 0x5A }, { 0x09, 0x65 }, { 0x09, 0x71 }, { 0x09, 0x7C }, { 0x09, 0x88 },
	{ 0x09, 0x94 }, { 0x09, 0x9F }, { 0x09, 0xAB }, { 0x09, 0xB7 }, { 0x09, 0xC2 },
	{ 0x09, 0xCE }, { 0x09, 0xD9 }, { 0x09, 0xE5 }, { 0x09, 0xF1 }, { 0x09, 0xFC },
	{ 0x0A, 0x08 }, { 0x0A, 0x13 }, { 0x0A, 0x1F }, { 0x0A, 0x2B }, { 0x0A, 0x36 },
	{ 0x0A, 0x42 }, { 0x0A, 0x4E }, { 0x0A, 0x59 }, { 0x0A, 0x65 }, { 0x0A, 0x70 },
	{ 0x0A, 0x7C }, { 0x0A, 0x88 }, { 0x0A, 0x93 }, { 0x0A, 0x9F }, { 0x0A, 0xAB },
	{ 0x0A, 0xB6 }, { 0x0A, 0xC2 }, { 0x0A, 0xCD }, { 0x0A, 0xD9 }, { 0x0A, 0xE5 },
	{ 0x0A, 0xF0 }, { 0x0A, 0xFC }, { 0x0B, 0x08 }, { 0x0B, 0x13 }, { 0x0B, 0x1F },
	{ 0x0B, 0x2A }, { 0x0B, 0x36 }, { 0x0B, 0x42 }, { 0x0B, 0x4D }, { 0x0B, 0x59 },
	{ 0x0B, 0x65 }, { 0x0B, 0x70 }, { 0x0B, 0x7C }, { 0x0B, 0x87 }, { 0x0B, 0x93 },
	// HBM(platfrom 256)
	{ 0x0B, 0x9C }, { 0x0B, 0xA5 }, { 0x0B, 0xAF }, { 0x0B, 0xB8 }, { 0x0B, 0xC1 },
	{ 0x0B, 0xCA }, { 0x0B, 0xD3 }, { 0x0B, 0xDC }, { 0x0B, 0xE6 }, { 0x0B, 0xEF },
	{ 0x0B, 0xF8 }, { 0x0C, 0x01 }, { 0x0C, 0x0A }, { 0x0C, 0x14 }, { 0x0C, 0x1D },
	{ 0x0C, 0x26 }, { 0x0C, 0x2F }, { 0x0C, 0x38 }, { 0x0C, 0x41 }, { 0x0C, 0x4B },
	{ 0x0C, 0x54 }, { 0x0C, 0x5D }, { 0x0C, 0x66 }, { 0x0C, 0x6F }, { 0x0C, 0x78 },
	{ 0x0C, 0x82 }, { 0x0C, 0x8B }, { 0x0C, 0x94 }, { 0x0C, 0x9D }, { 0x0C, 0xA6 },
	{ 0x0C, 0xB0 }, { 0x0C, 0xB9 }, { 0x0C, 0xC2 }, { 0x0C, 0xCB }, { 0x0C, 0xD4 },
	{ 0x0C, 0xDD }, { 0x0C, 0xE7 }, { 0x0C, 0xF0 }, { 0x0C, 0xF9 }, { 0x0D, 0x02 },
	{ 0x0D, 0x0B }, { 0x0D, 0x15 }, { 0x0D, 0x1E }, { 0x0D, 0x27 }, { 0x0D, 0x30 },
};

static struct maptbl nt36523n_gts9fe_csot_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(nt36523n_gts9fe_csot_brt_table,
			&TFT_FUNC(TFT_MAPTBL_INIT_BRT),
			&TFT_FUNC(TFT_MAPTBL_GETIDX_BRT),
			&TFT_FUNC(TFT_MAPTBL_COPY_DEFAULT)),
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_SLEEP_OUT[] = {
	0x11
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_SLEEP_IN[] = {
	0x10
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_DISPLAY_ON[] = {
	0x29
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_PRE_DISPLAY_OFF_1[] = {
	0xFF,
	0x10
};
static u8 SEQ_NT36523N_GTS9FE_CSOT_PRE_DISPLAY_OFF_2[] = {
	0xFB,
	0x01
};
static u8 SEQ_NT36523N_GTS9FE_CSOT_DISPLAY_OFF[] = {
	0x28
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_BRIGHTNESS[] = {
	0x51,
	0x64, 0x0B,
};

/* < CABC Mode control Function > */

static u8 SEQ_NT36523N_GTS9FE_CSOT_BRIGHTNESS_ON[] = {
	0x53,
	0x24,
};

/* Display config (1) */
static u8 SEQ_NT36523N_GTS9FE_CSOT_001[] = {
	0xFF,
	0x20,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_002[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_003[] = {
	0x05,
	0x09,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_004[] = {
	0x06,
	0xC0,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_005[] = {
	0x07,
	0x3A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_006[] = {
	0x08,
	0x1E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_007[] = {
	0x0D,
	0x63,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_008[] = {
	0x0E,
	0x55,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_009[] = {
	0x0F,
	0x37,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_010[] = {
	0x30,
	0x10,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_011[] = {
	0x31,
	0x02,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_012[] = {
	0x32,
	0x42,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_013[] = {
	0x58,
	0x40,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_014[] = {
	0x60,
	0xBB,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_015[] = {
	0x62,
	0x36,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_016[] = {
	0x63,
	0xA6,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_017[] = {
	0x69,
	0xDD,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_018[] = {
	0x65,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_019[] = {
	0x6D,
	0x55,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_020[] = {
	0x6E,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_021[] = {
	0x75,
	0xC4,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_022[] = {
	0x77,
	0x3B,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_023[] = {
	0x7C,
	0x44,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_024[] = {
	0x88,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_025[] = {
	0x89,
	0x25,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_026[] = {
	0x8A,
	0x25,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_027[] = {
	0x94,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_028[] = {
	0x95,
	0xA5,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_029[] = {
	0x96,
	0xA5,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_030[] = {
	0xFF,
	0x24,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_031[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_032[] = {
	0x00,
	0x23,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_033[] = {
	0x01,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_034[] = {
	0x02,
	0x22,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_035[] = {
	0x03,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_036[] = {
	0x04,
	0x13,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_037[] = {
	0x05,
	0x12,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_038[] = {
	0x06,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_039[] = {
	0x07,
	0x10,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_040[] = {
	0x08,
	0x0F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_041[] = {
	0x09,
	0x0E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_042[] = {
	0x0A,
	0x0D,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_043[] = {
	0x0B,
	0x0C,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_044[] = {
	0x0C,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_045[] = {
	0x0D,
	0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_046[] = {
	0x0E,
	0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_047[] = {
	0x0F,
	0x28,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_048[] = {
	0x10,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_049[] = {
	0x11,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_050[] = {
	0x12,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_051[] = {
	0x13,
	0x26,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_052[] = {
	0x14,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_053[] = {
	0x15,
	0x27,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_054[] = {
	0x16,
	0x23,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_055[] = {
	0x17,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_056[] = {
	0x18,
	0x22,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_057[] = {
	0x19,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_058[] = {
	0x1A,
	0x13,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_059[] = {
	0x1B,
	0x12,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_060[] = {
	0x1C,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_061[] = {
	0x1D,
	0x10,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_062[] = {
	0x1E,
	0x0F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_063[] = {
	0x1F,
	0x0E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_064[] = {
	0x20,
	0x0D,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_065[] = {
	0x21,
	0x0C,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_066[] = {
	0x22,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_067[] = {
	0x23,
	0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_068[] = {
	0x24,
	0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_069[] = {
	0x25,
	0x28,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_070[] = {
	0x26,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_071[] = {
	0x27,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_072[] = {
	0x28,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_073[] = {
	0x29,
	0x26,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_074[] = {
	0x2A,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_075[] = {
	0x2B,
	0x27,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_076[] = {
	0x2F,
	0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_077[] = {
	0x30,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_078[] = {
	0x33,
	0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_079[] = {
	0x34,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_080[] = {
	0x37,
	0x44,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_081[] = {
	0x3A,
	0x48,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_082[] = {
	0x3B,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_083[] = {
	0x3D,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_084[] = {
	0x4D,
	0x43,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_085[] = {
	0x4E,
	0x65,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_086[] = {
	0x4F,
	0x87,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_087[] = {
	0x50,
	0x21,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_088[] = {
	0x51,
	0x12,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_089[] = {
	0x52,
	0x78,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_090[] = {
	0x53,
	0x56,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_091[] = {
	0x54,
	0x34,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_092[] = {
	0x55,
	0x04, 0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_093[] = {
	0x56,
	0x08,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_094[] = {
	0x58,
	0x10,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_095[] = {
	0x59,
	0x10,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_096[] = {
	0x5A,
	0x08,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_097[] = {
	0x5B,
	0x7F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_098[] = {
	0x5C,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_099[] = {
	0x5D,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_100[] = {
	0x5E,
	0x00, 0x06,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_101[] = {
	0x60,
	0xB4,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_102[] = {
	0x61,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_103[] = {
	0x63,
	0x90,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_104[] = {
	0x91,
	0x40,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_105[] = {
	0x92,
	0x89, 0x00, 0x89,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_106[] = {
	0x93,
	0x60, 0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_107[] = {
	0x94,
	0x14, 0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_108[] = {
	0x95,
	0x81,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_109[] = {
	0x96,
	0x62,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_110[] = {
	0x97,
	0xC2,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_111[] = {
	0x98,
	0x80,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_112[] = {
	0xAA,
	0x4D,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_113[] = {
	0xAB,
	0x44,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_114[] = {
	0xB6,
	0x05, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x05, 0x05, 0x05,
	0x00, 0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_115[] = {
	0xBB,
	0x80,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_116[] = {
	0xBC,
	0x00, 0x00, 0x43, 0x00, 0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_117[] = {
	0xC2,
	0xC6, 0x00, 0x08,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_118[] = {
	0xC3,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_119[] = {
	0xC4,
	0x2F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_120[] = {
	0xDB,
	0x71,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_121[] = {
	0xDC,
	0x8A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_122[] = {
	0xDF,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_123[] = {
	0xE0,
	0x8A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_124[] = {
	0xE1,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_125[] = {
	0xE2,
	0x8A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_126[] = {
	0xE5,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_127[] = {
	0xE6,
	0x8A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_128[] = {
	0xFF,
	0x25,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_129[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_130[] = {
	0x05,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_131[] = {
	0x13,
	0x02,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_132[] = {
	0x14,
	0x95,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_133[] = {
	0xDB,
	0x02,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_134[] = {
	0xDC,
	0x08,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_135[] = {
	0x19,
	0x07,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_136[] = {
	0x1B,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_137[] = {
	0x1E,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_138[] = {
	0x1F,
	0x08,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_139[] = {
	0x20,
	0x7F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_140[] = {
	0x25,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_141[] = {
	0x26,
	0x08,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_142[] = {
	0x27,
	0x7F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_143[] = {
	0x3F,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_144[] = {
	0x40,
	0x02,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_145[] = {
	0x43,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_146[] = {
	0x48,
	0x08,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_147[] = {
	0x49,
	0x7F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_148[] = {
	0x5B,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_149[] = {
	0xC0,
	0x0C,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_150[] = {
	0xC2,
	0x40,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_151[] = {
	0xC5,
	0x17,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_152[] = {
	0xC8,
	0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_153[] = {
	0xF1,
	0x10,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_154[] = {
	0xFF,
	0x26,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_155[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_156[] = {
	0x00,
	0x81,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_157[] = {
	0x06,
	0x80,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_158[] = {
	0x04,
	0x96,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_159[] = {
	0x0A,
	0xF5,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_160[] = {
	0x0C,
	0x05,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_161[] = {
	0x0D,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_162[] = {
	0x0F,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_163[] = {
	0x11,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_164[] = {
	0x12,
	0x51,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_165[] = {
	0x13,
	0x80,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_166[] = {
	0x14,
	0x80,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_167[] = {
	0x16,
	0x91,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_168[] = {
	0x19,
	0x26,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_169[] = {
	0x1A,
	0xAB,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_170[] = {
	0x1B,
	0x24,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_171[] = {
	0x1C,
	0x43,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_172[] = {
	0x1D,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_173[] = {
	0x1E,
	0x8B,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_174[] = {
	0x1F,
	0x89,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_175[] = {
	0x20,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_176[] = {
	0x21,
	0x07,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_177[] = {
	0x2A,
	0x26,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_178[] = {
	0x2B,
	0xAB,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_179[] = {
	0x2F,
	0x06,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_180[] = {
	0x30,
	0x89,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_181[] = {
	0x31,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_182[] = {
	0x32,
	0x89,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_183[] = {
	0x39,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_184[] = {
	0x33,
	0x67,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_185[] = {
	0x34,
	0x89,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_186[] = {
	0x35,
	0x67,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_187[] = {
	0x36,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_188[] = {
	0x8B,
	0x96,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_189[] = {
	0xC7,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_190[] = {
	0xC8,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_191[] = {
	0xD2,
	0x2F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_192[] = {
	0xFF,
	0x27,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_193[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_194[] = {
	0x13,
	0x06,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_195[] = {
	0x56,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_196[] = {
	0x58,
	0x80,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_197[] = {
	0x59,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_198[] = {
	0x5A,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_199[] = {
	0x5B,
	0x13,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_200[] = {
	0x5C,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_201[] = {
	0x5D,
	0x96,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_202[] = {
	0x5E,
	0x32,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_203[] = {
	0x5F,
	0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_204[] = {
	0x60,
	0x88,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_205[] = {
	0x61,
	0xFF,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_206[] = {
	0x62,
	0xFF,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_207[] = {
	0x63,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_208[] = {
	0x64,
	0x91,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_209[] = {
	0x65,
	0xFE,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_210[] = {
	0x66,
	0xFF,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_211[] = {
	0x67,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_212[] = {
	0x68,
	0x91,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_213[] = {
	0x75,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_214[] = {
	0x76,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_215[] = {
	0x77,
	0x08,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_216[] = {
	0xD1,
	0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_217[] = {
	0xFF,
	0x2A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_218[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_219[] = {
	0x22,
	0x1F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_220[] = {
	0x23,
	0x14,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_221[] = {
	0x24,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_222[] = {
	0x25,
	0x89,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_223[] = {
	0x27,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_224[] = {
	0x28,
	0x1E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_225[] = {
	0x29,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_226[] = {
	0x2A,
	0x1E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_227[] = {
	0x2B,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_228[] = {
	0x2D,
	0x60,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_229[] = {
	0x64,
	0x41,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_230[] = {
	0x6A,
	0x41,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_231[] = {
	0x79,
	0x41,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_232[] = {
	0x7C,
	0x41,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_233[] = {
	0x7F,
	0x41,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_234[] = {
	0x82,
	0x41,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_235[] = {
	0x85,
	0x41,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_236[] = {
	0x97,
	0x3C,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_237[] = {
	0x98,
	0x02,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_238[] = {
	0x99,
	0x95,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_239[] = {
	0x9A,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_240[] = {
	0x9B,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_241[] = {
	0x9C,
	0x0B,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_242[] = {
	0x9D,
	0x0A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_243[] = {
	0x9E,
	0x90,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_244[] = {
	0xF2,
	0x3C,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_245[] = {
	0xF3,
	0x02,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_246[] = {
	0xF4,
	0x95,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_247[] = {
	0xF5,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_248[] = {
	0xF6,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_249[] = {
	0xF7,
	0x0B,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_250[] = {
	0xF8,
	0x0A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_251[] = {
	0xF9,
	0x90,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_252[] = {
	0xA2,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_253[] = {
	0xA3,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_254[] = {
	0xA4,
	0x30,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_255[] = {
	0xE8,
	0x15,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_256[] = {
	0xFF,
	0x23,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_257[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_258[] = {
	0x00,
	0x80,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_259[] = {
	0x03,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_260[] = {
	0x07,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_261[] = {
	0x08,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_262[] = {
	0x09,
	0xC2,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_263[] = {
	0x10,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_264[] = {
	0x11,
	0x02,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_265[] = {
	0x12,
	0x06,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_266[] = {
	0x15,
	0x51,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_267[] = {
	0x16,
	0x14,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_268[] = {
	0x2A,
	0x3F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_269[] = {
	0x45,
	0xFF,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_270[] = {
	0x46,
	0xFF,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_271[] = {
	0x47,
	0xFF,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_272[] = {
	0x48,
	0xFF,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_273[] = {
	0x49,
	0xFE,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_274[] = {
	0x4A,
	0xFD,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_275[] = {
	0x4B,
	0xFC,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_276[] = {
	0x4C,
	0xFA,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_277[] = {
	0x4D,
	0xF8,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_278[] = {
	0x4E,
	0xF6,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_279[] = {
	0x4F,
	0xF2,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_280[] = {
	0x50,
	0xEC,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_281[] = {
	0x51,
	0xE4,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_282[] = {
	0x52,
	0xDA,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_283[] = {
	0x53,
	0xCE,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_284[] = {
	0x54,
	0xB4,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_285[] = {
	0x28,
	0x3F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_286[] = {
	0x27,
	0x3F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_287[] = {
	0x26,
	0x3A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_288[] = {
	0x25,
	0x2E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_289[] = {
	0x24,
	0x26,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_290[] = {
	0x23,
	0x1E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_291[] = {
	0x22,
	0x1A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_292[] = {
	0x21,
	0x18,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_293[] = {
	0x20,
	0x16,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_294[] = {
	0x1F,
	0x12,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_295[] = {
	0x1E,
	0x0E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_296[] = {
	0x1D,
	0x0A,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_297[] = {
	0x1C,
	0x08,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_298[] = {
	0x1B,
	0x06,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_299[] = {
	0x1A,
	0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_300[] = {
	0x19,
	0x02,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_301[] = {
	0xFF,
	0x20,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_302[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_303[] = {
	0xB0,
	0x00, 0x08, 0x00, 0x1C, 0x00, 0x30, 0x00, 0x5A, 0x00, 0x72,
	0x00, 0x8A, 0x00, 0x9E, 0x00, 0xB1,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_304[] = {
	0xB1,
	0x00, 0xC3, 0x00, 0xFF, 0x01, 0x2B, 0x01, 0x70, 0x01, 0xA3,
	0x01, 0xF5, 0x02, 0x34, 0x02, 0x36,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_305[] = {
	0xB2,
	0x02, 0x73, 0x02, 0xB8, 0x02, 0xE6, 0x03, 0x1E, 0x03, 0x44,
	0x03, 0x70, 0x03, 0x80, 0x03, 0x8F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_306[] = {
	0xB3,
	0x03, 0xA1, 0x03, 0xB3, 0x03, 0xD4, 0x03, 0xE0, 0x03, 0xFE,
	0x03, 0xFF,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_307[] = {
	0xB4,
	0x00, 0x08, 0x00, 0x1C, 0x00, 0x30, 0x00, 0x5A, 0x00, 0x72,
	0x00, 0x8A, 0x00, 0x9E, 0x00, 0xB1,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_308[] = {
	0xB5,
	0x00, 0xC3, 0x00, 0xFF, 0x01, 0x2B, 0x01, 0x70, 0x01, 0xA3,
	0x01, 0xF5, 0x02, 0x34, 0x02, 0x36,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_309[] = {
	0xB6,
	0x02, 0x73, 0x02, 0xB8, 0x02, 0xE6, 0x03, 0x1E, 0x03, 0x44,
	0x03, 0x70, 0x03, 0x80, 0x03, 0x8F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_310[] = {
	0xB7,
	0x03, 0xA1, 0x03, 0xB3, 0x03, 0xD4, 0x03, 0xE0, 0x03, 0xFE,
	0x03, 0xFF,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_311[] = {
	0xB8,
	0x00, 0x08, 0x00, 0x1C, 0x00, 0x30, 0x00, 0x5A, 0x00, 0x72,
	0x00, 0x8A, 0x00, 0x9E, 0x00, 0xB1,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_312[] = {
	0xB9,
	0x00, 0xC3, 0x00, 0xFF, 0x01, 0x2B, 0x01, 0x70, 0x01, 0xA3,
	0x01, 0xF5, 0x02, 0x34, 0x02, 0x36,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_313[] = {
	0xBA,
	0x02, 0x73, 0x02, 0xB8, 0x02, 0xE6, 0x03, 0x1E, 0x03, 0x44,
	0x03, 0x70, 0x03, 0x80, 0x03, 0x8F,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_314[] = {
	0xBB,
	0x03, 0xA1, 0x03, 0xB3, 0x03, 0xD4, 0x03, 0xE0, 0x03, 0xFE,
	0x03, 0xFF,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_315[] = {
	0xC6,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_316[] = {
	0xC7,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_317[] = {
	0xC8,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_318[] = {
	0xC9,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_319[] = {
	0xCA,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_320[] = {
	0xCB,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_321[] = {
	0xCC,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_322[] = {
	0xCD,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_323[] = {
	0xCE,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_324[] = {
	0xCF,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_325[] = {
	0xD0,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_326[] = {
	0xD1,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_327[] = {
	0xD2,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_328[] = {
	0xD3,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_329[] = {
	0xD4,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_330[] = {
	0xD5,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_331[] = {
	0xD6,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_332[] = {
	0xD7,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_333[] = {
	0xD8,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_334[] = {
	0xD9,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_335[] = {
	0xDA,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_336[] = {
	0xDB,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_337[] = {
	0xDC,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_338[] = {
	0xDD,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_339[] = {
	0xDE,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_340[] = {
	0xDF,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_341[] = {
	0xE0,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_342[] = {
	0xE1,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_343[] = {
	0xE2,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_344[] = {
	0xE3,
	0x11,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_345[] = {
	0xC0,
	0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E,
	0x0E, 0x0E, 0x0E, 0x0C, 0x0C, 0x0C,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_346[] = {
	0xC1,
	0x0C, 0x0C, 0x0D, 0x0E, 0x0E, 0x0E, 0x0D, 0x0C, 0x0A, 0x08,
	0x06, 0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_347[] = {
	0xC2,
	0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E,
	0x0E, 0x0E, 0x0E, 0x0C, 0x0C, 0x0C,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_348[] = {
	0xC3,
	0x0C, 0x0C, 0x0D, 0x0E, 0x0E, 0x0E, 0x0D, 0x0C, 0x0A, 0x08,
	0x06, 0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_349[] = {
	0xC4,
	0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E,
	0x0E, 0x0E, 0x0E, 0x0C, 0x0C, 0x0C,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_350[] = {
	0xC5,
	0x0C, 0x0C, 0x0D, 0x0E, 0x0E, 0x0E, 0x0D, 0x0C, 0x0A, 0x08,
	0x06, 0x04,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_351[] = {
	0xFF,
	0x21,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_352[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_353[] = {
	0xB0,
	0x00, 0x00, 0x00, 0x14, 0x00, 0x28, 0x00, 0x52, 0x00, 0x6A,
	0x00, 0x82, 0x00, 0x96, 0x00, 0xA9,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_354[] = {
	0xB1,
	0x00, 0xBB, 0x00, 0xF7, 0x01, 0x23, 0x01, 0x68, 0x01, 0x9B,
	0x01, 0xED, 0x02, 0x2C, 0x02, 0x2E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_355[] = {
	0xB2,
	0x02, 0x6B, 0x02, 0xB0, 0x02, 0xDE, 0x03, 0x16, 0x03, 0x3C,
	0x03, 0x68, 0x03, 0x78, 0x03, 0x87,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_356[] = {
	0xB3,
	0x03, 0x99, 0x03, 0xAB, 0x03, 0xCC, 0x03, 0xD8, 0x03, 0xF6,
	0x03, 0xF7,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_357[] = {
	0xB4,
	0x00, 0x00, 0x00, 0x14, 0x00, 0x28, 0x00, 0x52, 0x00, 0x6A,
	0x00, 0x82, 0x00, 0x96, 0x00, 0xA9,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_358[] = {
	0xB5,
	0x00, 0xBB, 0x00, 0xF7, 0x01, 0x23, 0x01, 0x68, 0x01, 0x9B,
	0x01, 0xED, 0x02, 0x2C, 0x02, 0x2E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_359[] = {
	0xB6,
	0x02, 0x6B, 0x02, 0xB0, 0x02, 0xDE, 0x03, 0x16, 0x03, 0x3C,
	0x03, 0x68, 0x03, 0x78, 0x03, 0x87,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_360[] = {
	0xB7,
	0x03, 0x99, 0x03, 0xAB, 0x03, 0xCC, 0x03, 0xD8, 0x03, 0xF6,
	0x03, 0xF7,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_361[] = {
	0xB8,
	0x00, 0x00, 0x00, 0x14, 0x00, 0x28, 0x00, 0x52, 0x00, 0x6A,
	0x00, 0x82, 0x00, 0x96, 0x00, 0xA9,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_362[] = {
	0xB9,
	0x00, 0xBB, 0x00, 0xF7, 0x01, 0x23, 0x01, 0x68, 0x01, 0x9B,
	0x01, 0xED, 0x02, 0x2C, 0x02, 0x2E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_363[] = {
	0xBA,
	0x02, 0x6B, 0x02, 0xB0, 0x02, 0xDE, 0x03, 0x16, 0x03, 0x3C,
	0x03, 0x68, 0x03, 0x78, 0x03, 0x87,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_364[] = {
	0xBB,
	0x03, 0x99, 0x03, 0xAB, 0x03, 0xCC, 0x03, 0xD8, 0x03, 0xF6,
	0x03, 0xF7,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_365[] = {
	0xFF,
	0x10,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_366[] = {
	0xFB,
	0x01,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_367[] = {
	0x35,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_368[] = {
	0x51,
	0x00, 0x00,
};
/*
static u8 SEQ_NT36523N_GTS9FE_CSOT_369[] = {
	0x53,
	0x24,
};
*/
static u8 SEQ_NT36523N_GTS9FE_CSOT_370[] = {
	0x55,
	0x02,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_371[] = {
	0x90,
	0x03,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_372[] = {
	0x91,
	0x89, 0x28, 0x00, 0x18, 0xC2, 0x00, 0x02, 0x68, 0x02, 0xA1,
	0x00, 0x0A, 0x04, 0x2D, 0x03, 0x2E,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_373[] = {
	0x92,
	0x10, 0xF0,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_374[] = {
	0xB2,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_375[] = {
	0xB3,
	0x00,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_376[] = {
	0xBB,
	0x13,
};

static u8 SEQ_NT36523N_GTS9FE_CSOT_377[] = {
	0x3B,
	0x03, 0x14, 0x60, 0x04, 0x04, 0x00,
};


static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_sleep_out, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_sleep_in, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_display_on, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_pre_display_off_1, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_PRE_DISPLAY_OFF_1, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_pre_display_off_2, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_PRE_DISPLAY_OFF_2, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_display_off, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_brightness_on, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_BRIGHTNESS_ON, 0);

static DEFINE_PKTUI(nt36523n_gts9fe_csot_brightness, &nt36523n_gts9fe_csot_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(nt36523n_gts9fe_csot_brightness, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_BRIGHTNESS, 0);

static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_001, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_001, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_002, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_002, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_003, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_003, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_004, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_004, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_005, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_005, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_006, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_006, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_007, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_007, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_008, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_008, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_009, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_009, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_010, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_010, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_011, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_011, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_012, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_012, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_013, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_013, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_014, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_014, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_015, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_015, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_016, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_016, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_017, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_017, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_018, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_018, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_019, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_019, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_020, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_020, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_021, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_021, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_022, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_022, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_023, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_023, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_024, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_024, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_025, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_025, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_026, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_026, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_027, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_027, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_028, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_028, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_029, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_029, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_030, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_030, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_031, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_031, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_032, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_032, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_033, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_033, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_034, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_034, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_035, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_035, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_036, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_036, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_037, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_037, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_038, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_038, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_039, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_039, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_040, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_040, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_041, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_041, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_042, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_042, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_043, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_043, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_044, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_044, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_045, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_045, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_046, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_046, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_047, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_047, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_048, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_048, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_049, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_049, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_050, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_050, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_051, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_051, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_052, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_052, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_053, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_053, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_054, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_054, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_055, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_055, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_056, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_056, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_057, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_057, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_058, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_058, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_059, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_059, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_060, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_060, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_061, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_061, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_062, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_062, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_063, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_063, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_064, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_064, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_065, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_065, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_066, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_066, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_067, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_067, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_068, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_068, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_069, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_069, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_070, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_070, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_071, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_071, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_072, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_072, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_073, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_073, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_074, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_074, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_075, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_075, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_076, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_076, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_077, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_077, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_078, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_078, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_079, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_079, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_080, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_080, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_081, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_081, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_082, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_082, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_083, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_083, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_084, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_084, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_085, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_085, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_086, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_086, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_087, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_087, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_088, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_088, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_089, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_089, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_090, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_090, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_091, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_091, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_092, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_092, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_093, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_093, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_094, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_094, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_095, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_095, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_096, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_096, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_097, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_097, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_098, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_098, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_099, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_099, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_100, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_100, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_101, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_101, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_102, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_102, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_103, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_103, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_104, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_104, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_105, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_105, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_106, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_106, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_107, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_107, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_108, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_108, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_109, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_109, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_110, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_110, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_111, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_111, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_112, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_112, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_113, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_113, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_114, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_114, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_115, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_115, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_116, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_116, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_117, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_117, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_118, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_118, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_119, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_119, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_120, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_120, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_121, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_121, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_122, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_122, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_123, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_123, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_124, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_124, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_125, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_125, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_126, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_126, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_127, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_127, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_128, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_128, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_129, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_129, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_130, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_130, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_131, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_131, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_132, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_132, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_133, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_133, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_134, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_134, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_135, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_135, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_136, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_136, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_137, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_137, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_138, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_138, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_139, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_139, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_140, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_140, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_141, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_141, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_142, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_142, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_143, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_143, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_144, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_144, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_145, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_145, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_146, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_146, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_147, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_147, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_148, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_148, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_149, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_149, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_150, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_150, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_151, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_151, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_152, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_152, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_153, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_153, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_154, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_154, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_155, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_155, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_156, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_156, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_157, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_157, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_158, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_158, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_159, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_159, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_160, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_160, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_161, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_161, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_162, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_162, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_163, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_163, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_164, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_164, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_165, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_165, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_166, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_166, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_167, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_167, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_168, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_168, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_169, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_169, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_170, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_170, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_171, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_171, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_172, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_172, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_173, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_173, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_174, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_174, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_175, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_175, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_176, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_176, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_177, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_177, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_178, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_178, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_179, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_179, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_180, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_180, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_181, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_181, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_182, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_182, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_183, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_183, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_184, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_184, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_185, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_185, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_186, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_186, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_187, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_187, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_188, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_188, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_189, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_189, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_190, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_190, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_191, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_191, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_192, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_192, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_193, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_193, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_194, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_194, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_195, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_195, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_196, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_196, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_197, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_197, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_198, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_198, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_199, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_199, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_200, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_200, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_201, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_201, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_202, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_202, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_203, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_203, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_204, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_204, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_205, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_205, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_206, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_206, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_207, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_207, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_208, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_208, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_209, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_209, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_210, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_210, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_211, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_211, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_212, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_212, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_213, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_213, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_214, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_214, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_215, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_215, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_216, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_216, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_217, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_217, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_218, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_218, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_219, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_219, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_220, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_220, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_221, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_221, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_222, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_222, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_223, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_223, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_224, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_224, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_225, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_225, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_226, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_226, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_227, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_227, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_228, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_228, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_229, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_229, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_230, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_230, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_231, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_231, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_232, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_232, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_233, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_233, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_234, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_234, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_235, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_235, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_236, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_236, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_237, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_237, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_238, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_238, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_239, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_239, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_240, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_240, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_241, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_241, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_242, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_242, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_243, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_243, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_244, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_244, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_245, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_245, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_246, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_246, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_247, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_247, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_248, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_248, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_249, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_249, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_250, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_250, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_251, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_251, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_252, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_252, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_253, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_253, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_254, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_254, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_255, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_255, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_256, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_256, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_257, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_257, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_258, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_258, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_259, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_259, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_260, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_260, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_261, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_261, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_262, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_262, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_263, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_263, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_264, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_264, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_265, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_265, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_266, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_266, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_267, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_267, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_268, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_268, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_269, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_269, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_270, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_270, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_271, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_271, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_272, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_272, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_273, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_273, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_274, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_274, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_275, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_275, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_276, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_276, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_277, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_277, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_278, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_278, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_279, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_279, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_280, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_280, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_281, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_281, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_282, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_282, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_283, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_283, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_284, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_284, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_285, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_285, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_286, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_286, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_287, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_287, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_288, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_288, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_289, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_289, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_290, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_290, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_291, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_291, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_292, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_292, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_293, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_293, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_294, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_294, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_295, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_295, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_296, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_296, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_297, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_297, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_298, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_298, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_299, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_299, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_300, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_300, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_301, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_301, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_302, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_302, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_303, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_303, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_304, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_304, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_305, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_305, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_306, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_306, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_307, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_307, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_308, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_308, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_309, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_309, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_310, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_310, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_311, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_311, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_312, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_312, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_313, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_313, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_314, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_314, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_315, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_315, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_316, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_316, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_317, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_317, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_318, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_318, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_319, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_319, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_320, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_320, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_321, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_321, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_322, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_322, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_323, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_323, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_324, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_324, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_325, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_325, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_326, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_326, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_327, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_327, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_328, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_328, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_329, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_329, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_330, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_330, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_331, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_331, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_332, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_332, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_333, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_333, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_334, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_334, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_335, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_335, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_336, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_336, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_337, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_337, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_338, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_338, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_339, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_339, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_340, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_340, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_341, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_341, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_342, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_342, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_343, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_343, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_344, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_344, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_345, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_345, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_346, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_346, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_347, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_347, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_348, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_348, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_349, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_349, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_350, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_350, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_351, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_351, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_352, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_352, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_353, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_353, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_354, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_354, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_355, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_355, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_356, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_356, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_357, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_357, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_358, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_358, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_359, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_359, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_360, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_360, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_361, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_361, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_362, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_362, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_363, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_363, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_364, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_364, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_365, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_365, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_366, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_366, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_367, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_367, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_368, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_368, 0);
//static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_369, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_369, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_370, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_370, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_371, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_371, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_372, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_372, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_373, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_373, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_374, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_374, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_375, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_375, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_376, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_376, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_377, DSI_PKT_TYPE_WR, SEQ_NT36523N_GTS9FE_CSOT_377, 0);

static DEFINE_PANEL_MDELAY(nt36523n_gts9fe_csot_wait_20msec, 20); /* 1 frame */
static DEFINE_PANEL_MDELAY(nt36523n_gts9fe_csot_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(nt36523n_gts9fe_csot_wait_105msec, 105);

static DEFINE_PNOBJ_CONFIG(nt36523n_gts9fe_csot_set_wait_tx_done_property_on, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_ON);
static DEFINE_PNOBJ_CONFIG(nt36523n_gts9fe_csot_set_wait_tx_done_property_off, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
static DEFINE_PNOBJ_CONFIG(nt36523n_gts9fe_csot_set_wait_tx_done_property_auto, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

static u8 NT36523N_GTS9FE_CSOT_ID[TFT_COMMON_ID_LEN];
static DEFINE_RDINFO(nt36523n_gts9fe_csot_id1, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DA_OFS, TFT_COMMON_ID_DA_LEN);
static DEFINE_RDINFO(nt36523n_gts9fe_csot_id2, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DB_OFS, TFT_COMMON_ID_DB_LEN);
static DEFINE_RDINFO(nt36523n_gts9fe_csot_id3, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DC_OFS, TFT_COMMON_ID_DC_LEN);
static DECLARE_RESUI(nt36523n_gts9fe_csot_id) = {
	{ .rditbl = &RDINFO(nt36523n_gts9fe_csot_id1), .offset = 0 },
	{ .rditbl = &RDINFO(nt36523n_gts9fe_csot_id2), .offset = 1 },
	{ .rditbl = &RDINFO(nt36523n_gts9fe_csot_id3), .offset = 2 },
};
static DEFINE_RESOURCE(nt36523n_gts9fe_csot_id, NT36523N_GTS9FE_CSOT_ID, RESUI(nt36523n_gts9fe_csot_id));

static void *nt36523n_gts9fe_csot_init_cmdtbl[] = {
	&PNOBJ_CONFIG(nt36523n_gts9fe_csot_set_wait_tx_done_property_on),
	&PKTINFO(nt36523n_gts9fe_csot_001),
	&PKTINFO(nt36523n_gts9fe_csot_002),
	&PKTINFO(nt36523n_gts9fe_csot_003),
	&PKTINFO(nt36523n_gts9fe_csot_004),
	&PKTINFO(nt36523n_gts9fe_csot_005),
	&PKTINFO(nt36523n_gts9fe_csot_006),
	&PKTINFO(nt36523n_gts9fe_csot_007),
	&PKTINFO(nt36523n_gts9fe_csot_008),
	&PKTINFO(nt36523n_gts9fe_csot_009),
	&PKTINFO(nt36523n_gts9fe_csot_010),
	&PKTINFO(nt36523n_gts9fe_csot_011),
	&PKTINFO(nt36523n_gts9fe_csot_012),
	&PKTINFO(nt36523n_gts9fe_csot_013),
	&PKTINFO(nt36523n_gts9fe_csot_014),
	&PKTINFO(nt36523n_gts9fe_csot_015),
	&PKTINFO(nt36523n_gts9fe_csot_016),
	&PKTINFO(nt36523n_gts9fe_csot_017),
	&PKTINFO(nt36523n_gts9fe_csot_018),
	&PKTINFO(nt36523n_gts9fe_csot_019),
	&PKTINFO(nt36523n_gts9fe_csot_020),
	&PKTINFO(nt36523n_gts9fe_csot_021),
	&PKTINFO(nt36523n_gts9fe_csot_022),
	&PKTINFO(nt36523n_gts9fe_csot_023),
	&PKTINFO(nt36523n_gts9fe_csot_024),
	&PKTINFO(nt36523n_gts9fe_csot_025),
	&PKTINFO(nt36523n_gts9fe_csot_026),
	&PKTINFO(nt36523n_gts9fe_csot_027),
	&PKTINFO(nt36523n_gts9fe_csot_028),
	&PKTINFO(nt36523n_gts9fe_csot_029),
	&PKTINFO(nt36523n_gts9fe_csot_030),
	&PKTINFO(nt36523n_gts9fe_csot_031),
	&PKTINFO(nt36523n_gts9fe_csot_032),
	&PKTINFO(nt36523n_gts9fe_csot_033),
	&PKTINFO(nt36523n_gts9fe_csot_034),
	&PKTINFO(nt36523n_gts9fe_csot_035),
	&PKTINFO(nt36523n_gts9fe_csot_036),
	&PKTINFO(nt36523n_gts9fe_csot_037),
	&PKTINFO(nt36523n_gts9fe_csot_038),
	&PKTINFO(nt36523n_gts9fe_csot_039),
	&PKTINFO(nt36523n_gts9fe_csot_040),
	&PKTINFO(nt36523n_gts9fe_csot_041),
	&PKTINFO(nt36523n_gts9fe_csot_042),
	&PKTINFO(nt36523n_gts9fe_csot_043),
	&PKTINFO(nt36523n_gts9fe_csot_044),
	&PKTINFO(nt36523n_gts9fe_csot_045),
	&PKTINFO(nt36523n_gts9fe_csot_046),
	&PKTINFO(nt36523n_gts9fe_csot_047),
	&PKTINFO(nt36523n_gts9fe_csot_048),
	&PKTINFO(nt36523n_gts9fe_csot_049),
	&PKTINFO(nt36523n_gts9fe_csot_050),
	&PKTINFO(nt36523n_gts9fe_csot_051),
	&PKTINFO(nt36523n_gts9fe_csot_052),
	&PKTINFO(nt36523n_gts9fe_csot_053),
	&PKTINFO(nt36523n_gts9fe_csot_054),
	&PKTINFO(nt36523n_gts9fe_csot_055),
	&PKTINFO(nt36523n_gts9fe_csot_056),
	&PKTINFO(nt36523n_gts9fe_csot_057),
	&PKTINFO(nt36523n_gts9fe_csot_058),
	&PKTINFO(nt36523n_gts9fe_csot_059),
	&PKTINFO(nt36523n_gts9fe_csot_060),
	&PKTINFO(nt36523n_gts9fe_csot_061),
	&PKTINFO(nt36523n_gts9fe_csot_062),
	&PKTINFO(nt36523n_gts9fe_csot_063),
	&PKTINFO(nt36523n_gts9fe_csot_064),
	&PKTINFO(nt36523n_gts9fe_csot_065),
	&PKTINFO(nt36523n_gts9fe_csot_066),
	&PKTINFO(nt36523n_gts9fe_csot_067),
	&PKTINFO(nt36523n_gts9fe_csot_068),
	&PKTINFO(nt36523n_gts9fe_csot_069),
	&PKTINFO(nt36523n_gts9fe_csot_070),
	&PKTINFO(nt36523n_gts9fe_csot_071),
	&PKTINFO(nt36523n_gts9fe_csot_072),
	&PKTINFO(nt36523n_gts9fe_csot_073),
	&PKTINFO(nt36523n_gts9fe_csot_074),
	&PKTINFO(nt36523n_gts9fe_csot_075),
	&PKTINFO(nt36523n_gts9fe_csot_076),
	&PKTINFO(nt36523n_gts9fe_csot_077),
	&PKTINFO(nt36523n_gts9fe_csot_078),
	&PKTINFO(nt36523n_gts9fe_csot_079),
	&PKTINFO(nt36523n_gts9fe_csot_080),
	&PKTINFO(nt36523n_gts9fe_csot_081),
	&PKTINFO(nt36523n_gts9fe_csot_082),
	&PKTINFO(nt36523n_gts9fe_csot_083),
	&PKTINFO(nt36523n_gts9fe_csot_084),
	&PKTINFO(nt36523n_gts9fe_csot_085),
	&PKTINFO(nt36523n_gts9fe_csot_086),
	&PKTINFO(nt36523n_gts9fe_csot_087),
	&PKTINFO(nt36523n_gts9fe_csot_088),
	&PKTINFO(nt36523n_gts9fe_csot_089),
	&PKTINFO(nt36523n_gts9fe_csot_090),
	&PKTINFO(nt36523n_gts9fe_csot_091),
	&PKTINFO(nt36523n_gts9fe_csot_092),
	&PKTINFO(nt36523n_gts9fe_csot_093),
	&PKTINFO(nt36523n_gts9fe_csot_094),
	&PKTINFO(nt36523n_gts9fe_csot_095),
	&PKTINFO(nt36523n_gts9fe_csot_096),
	&PKTINFO(nt36523n_gts9fe_csot_097),
	&PKTINFO(nt36523n_gts9fe_csot_098),
	&PKTINFO(nt36523n_gts9fe_csot_099),
	&PKTINFO(nt36523n_gts9fe_csot_100),
	&PKTINFO(nt36523n_gts9fe_csot_101),
	&PKTINFO(nt36523n_gts9fe_csot_102),
	&PKTINFO(nt36523n_gts9fe_csot_103),
	&PKTINFO(nt36523n_gts9fe_csot_104),
	&PKTINFO(nt36523n_gts9fe_csot_105),
	&PKTINFO(nt36523n_gts9fe_csot_106),
	&PKTINFO(nt36523n_gts9fe_csot_107),
	&PKTINFO(nt36523n_gts9fe_csot_108),
	&PKTINFO(nt36523n_gts9fe_csot_109),
	&PKTINFO(nt36523n_gts9fe_csot_110),
	&PKTINFO(nt36523n_gts9fe_csot_111),
	&PKTINFO(nt36523n_gts9fe_csot_112),
	&PKTINFO(nt36523n_gts9fe_csot_113),
	&PKTINFO(nt36523n_gts9fe_csot_114),
	&PKTINFO(nt36523n_gts9fe_csot_115),
	&PKTINFO(nt36523n_gts9fe_csot_116),
	&PKTINFO(nt36523n_gts9fe_csot_117),
	&PKTINFO(nt36523n_gts9fe_csot_118),
	&PKTINFO(nt36523n_gts9fe_csot_119),
	&PKTINFO(nt36523n_gts9fe_csot_120),
	&PKTINFO(nt36523n_gts9fe_csot_121),
	&PKTINFO(nt36523n_gts9fe_csot_122),
	&PKTINFO(nt36523n_gts9fe_csot_123),
	&PKTINFO(nt36523n_gts9fe_csot_124),
	&PKTINFO(nt36523n_gts9fe_csot_125),
	&PKTINFO(nt36523n_gts9fe_csot_126),
	&PKTINFO(nt36523n_gts9fe_csot_127),
	&PKTINFO(nt36523n_gts9fe_csot_128),
	&PKTINFO(nt36523n_gts9fe_csot_129),
	&PKTINFO(nt36523n_gts9fe_csot_130),
	&PKTINFO(nt36523n_gts9fe_csot_131),
	&PKTINFO(nt36523n_gts9fe_csot_132),
	&PKTINFO(nt36523n_gts9fe_csot_133),
	&PKTINFO(nt36523n_gts9fe_csot_134),
	&PKTINFO(nt36523n_gts9fe_csot_135),
	&PKTINFO(nt36523n_gts9fe_csot_136),
	&PKTINFO(nt36523n_gts9fe_csot_137),
	&PKTINFO(nt36523n_gts9fe_csot_138),
	&PKTINFO(nt36523n_gts9fe_csot_139),
	&PKTINFO(nt36523n_gts9fe_csot_140),
	&PKTINFO(nt36523n_gts9fe_csot_141),
	&PKTINFO(nt36523n_gts9fe_csot_142),
	&PKTINFO(nt36523n_gts9fe_csot_143),
	&PKTINFO(nt36523n_gts9fe_csot_144),
	&PKTINFO(nt36523n_gts9fe_csot_145),
	&PKTINFO(nt36523n_gts9fe_csot_146),
	&PKTINFO(nt36523n_gts9fe_csot_147),
	&PKTINFO(nt36523n_gts9fe_csot_148),
	&PKTINFO(nt36523n_gts9fe_csot_149),
	&PKTINFO(nt36523n_gts9fe_csot_150),
	&PKTINFO(nt36523n_gts9fe_csot_151),
	&PKTINFO(nt36523n_gts9fe_csot_152),
	&PKTINFO(nt36523n_gts9fe_csot_153),
	&PKTINFO(nt36523n_gts9fe_csot_154),
	&PKTINFO(nt36523n_gts9fe_csot_155),
	&PKTINFO(nt36523n_gts9fe_csot_156),
	&PKTINFO(nt36523n_gts9fe_csot_157),
	&PKTINFO(nt36523n_gts9fe_csot_158),
	&PKTINFO(nt36523n_gts9fe_csot_159),
	&PKTINFO(nt36523n_gts9fe_csot_160),
	&PKTINFO(nt36523n_gts9fe_csot_161),
	&PKTINFO(nt36523n_gts9fe_csot_162),
	&PKTINFO(nt36523n_gts9fe_csot_163),
	&PKTINFO(nt36523n_gts9fe_csot_164),
	&PKTINFO(nt36523n_gts9fe_csot_165),
	&PKTINFO(nt36523n_gts9fe_csot_166),
	&PKTINFO(nt36523n_gts9fe_csot_167),
	&PKTINFO(nt36523n_gts9fe_csot_168),
	&PKTINFO(nt36523n_gts9fe_csot_169),
	&PKTINFO(nt36523n_gts9fe_csot_170),
	&PKTINFO(nt36523n_gts9fe_csot_171),
	&PKTINFO(nt36523n_gts9fe_csot_172),
	&PKTINFO(nt36523n_gts9fe_csot_173),
	&PKTINFO(nt36523n_gts9fe_csot_174),
	&PKTINFO(nt36523n_gts9fe_csot_175),
	&PKTINFO(nt36523n_gts9fe_csot_176),
	&PKTINFO(nt36523n_gts9fe_csot_177),
	&PKTINFO(nt36523n_gts9fe_csot_178),
	&PKTINFO(nt36523n_gts9fe_csot_179),
	&PKTINFO(nt36523n_gts9fe_csot_180),
	&PKTINFO(nt36523n_gts9fe_csot_181),
	&PKTINFO(nt36523n_gts9fe_csot_182),
	&PKTINFO(nt36523n_gts9fe_csot_183),
	&PKTINFO(nt36523n_gts9fe_csot_184),
	&PKTINFO(nt36523n_gts9fe_csot_185),
	&PKTINFO(nt36523n_gts9fe_csot_186),
	&PKTINFO(nt36523n_gts9fe_csot_187),
	&PKTINFO(nt36523n_gts9fe_csot_188),
	&PKTINFO(nt36523n_gts9fe_csot_189),
	&PKTINFO(nt36523n_gts9fe_csot_190),
	&PKTINFO(nt36523n_gts9fe_csot_191),
	&PKTINFO(nt36523n_gts9fe_csot_192),
	&PKTINFO(nt36523n_gts9fe_csot_193),
	&PKTINFO(nt36523n_gts9fe_csot_194),
	&PKTINFO(nt36523n_gts9fe_csot_195),
	&PKTINFO(nt36523n_gts9fe_csot_196),
	&PKTINFO(nt36523n_gts9fe_csot_197),
	&PKTINFO(nt36523n_gts9fe_csot_198),
	&PKTINFO(nt36523n_gts9fe_csot_199),
	&PKTINFO(nt36523n_gts9fe_csot_200),
	&PKTINFO(nt36523n_gts9fe_csot_201),
	&PKTINFO(nt36523n_gts9fe_csot_202),
	&PKTINFO(nt36523n_gts9fe_csot_203),
	&PKTINFO(nt36523n_gts9fe_csot_204),
	&PKTINFO(nt36523n_gts9fe_csot_205),
	&PKTINFO(nt36523n_gts9fe_csot_206),
	&PKTINFO(nt36523n_gts9fe_csot_207),
	&PKTINFO(nt36523n_gts9fe_csot_208),
	&PKTINFO(nt36523n_gts9fe_csot_209),
	&PKTINFO(nt36523n_gts9fe_csot_210),
	&PKTINFO(nt36523n_gts9fe_csot_211),
	&PKTINFO(nt36523n_gts9fe_csot_212),
	&PKTINFO(nt36523n_gts9fe_csot_213),
	&PKTINFO(nt36523n_gts9fe_csot_214),
	&PKTINFO(nt36523n_gts9fe_csot_215),
	&PKTINFO(nt36523n_gts9fe_csot_216),
	&PKTINFO(nt36523n_gts9fe_csot_217),
	&PKTINFO(nt36523n_gts9fe_csot_218),
	&PKTINFO(nt36523n_gts9fe_csot_219),
	&PKTINFO(nt36523n_gts9fe_csot_220),
	&PKTINFO(nt36523n_gts9fe_csot_221),
	&PKTINFO(nt36523n_gts9fe_csot_222),
	&PKTINFO(nt36523n_gts9fe_csot_223),
	&PKTINFO(nt36523n_gts9fe_csot_224),
	&PKTINFO(nt36523n_gts9fe_csot_225),
	&PKTINFO(nt36523n_gts9fe_csot_226),
	&PKTINFO(nt36523n_gts9fe_csot_227),
	&PKTINFO(nt36523n_gts9fe_csot_228),
	&PKTINFO(nt36523n_gts9fe_csot_229),
	&PKTINFO(nt36523n_gts9fe_csot_230),
	&PKTINFO(nt36523n_gts9fe_csot_231),
	&PKTINFO(nt36523n_gts9fe_csot_232),
	&PKTINFO(nt36523n_gts9fe_csot_233),
	&PKTINFO(nt36523n_gts9fe_csot_234),
	&PKTINFO(nt36523n_gts9fe_csot_235),
	&PKTINFO(nt36523n_gts9fe_csot_236),
	&PKTINFO(nt36523n_gts9fe_csot_237),
	&PKTINFO(nt36523n_gts9fe_csot_238),
	&PKTINFO(nt36523n_gts9fe_csot_239),
	&PKTINFO(nt36523n_gts9fe_csot_240),
	&PKTINFO(nt36523n_gts9fe_csot_241),
	&PKTINFO(nt36523n_gts9fe_csot_242),
	&PKTINFO(nt36523n_gts9fe_csot_243),
	&PKTINFO(nt36523n_gts9fe_csot_244),
	&PKTINFO(nt36523n_gts9fe_csot_245),
	&PKTINFO(nt36523n_gts9fe_csot_246),
	&PKTINFO(nt36523n_gts9fe_csot_247),
	&PKTINFO(nt36523n_gts9fe_csot_248),
	&PKTINFO(nt36523n_gts9fe_csot_249),
	&PKTINFO(nt36523n_gts9fe_csot_250),
	&PKTINFO(nt36523n_gts9fe_csot_251),
	&PKTINFO(nt36523n_gts9fe_csot_252),
	&PKTINFO(nt36523n_gts9fe_csot_253),
	&PKTINFO(nt36523n_gts9fe_csot_254),
	&PKTINFO(nt36523n_gts9fe_csot_255),
	&PKTINFO(nt36523n_gts9fe_csot_256),
	&PKTINFO(nt36523n_gts9fe_csot_257),
	&PKTINFO(nt36523n_gts9fe_csot_258),
	&PKTINFO(nt36523n_gts9fe_csot_259),
	&PKTINFO(nt36523n_gts9fe_csot_260),
	&PKTINFO(nt36523n_gts9fe_csot_261),
	&PKTINFO(nt36523n_gts9fe_csot_262),
	&PKTINFO(nt36523n_gts9fe_csot_263),
	&PKTINFO(nt36523n_gts9fe_csot_264),
	&PKTINFO(nt36523n_gts9fe_csot_265),
	&PKTINFO(nt36523n_gts9fe_csot_266),
	&PKTINFO(nt36523n_gts9fe_csot_267),
	&PKTINFO(nt36523n_gts9fe_csot_268),
	&PKTINFO(nt36523n_gts9fe_csot_269),
	&PKTINFO(nt36523n_gts9fe_csot_270),
	&PKTINFO(nt36523n_gts9fe_csot_271),
	&PKTINFO(nt36523n_gts9fe_csot_272),
	&PKTINFO(nt36523n_gts9fe_csot_273),
	&PKTINFO(nt36523n_gts9fe_csot_274),
	&PKTINFO(nt36523n_gts9fe_csot_275),
	&PKTINFO(nt36523n_gts9fe_csot_276),
	&PKTINFO(nt36523n_gts9fe_csot_277),
	&PKTINFO(nt36523n_gts9fe_csot_278),
	&PKTINFO(nt36523n_gts9fe_csot_279),
	&PKTINFO(nt36523n_gts9fe_csot_280),
	&PKTINFO(nt36523n_gts9fe_csot_281),
	&PKTINFO(nt36523n_gts9fe_csot_282),
	&PKTINFO(nt36523n_gts9fe_csot_283),
	&PKTINFO(nt36523n_gts9fe_csot_284),
	&PKTINFO(nt36523n_gts9fe_csot_285),
	&PKTINFO(nt36523n_gts9fe_csot_286),
	&PKTINFO(nt36523n_gts9fe_csot_287),
	&PKTINFO(nt36523n_gts9fe_csot_288),
	&PKTINFO(nt36523n_gts9fe_csot_289),
	&PKTINFO(nt36523n_gts9fe_csot_290),
	&PKTINFO(nt36523n_gts9fe_csot_291),
	&PKTINFO(nt36523n_gts9fe_csot_292),
	&PKTINFO(nt36523n_gts9fe_csot_293),
	&PKTINFO(nt36523n_gts9fe_csot_294),
	&PKTINFO(nt36523n_gts9fe_csot_295),
	&PKTINFO(nt36523n_gts9fe_csot_296),
	&PKTINFO(nt36523n_gts9fe_csot_297),
	&PKTINFO(nt36523n_gts9fe_csot_298),
	&PKTINFO(nt36523n_gts9fe_csot_299),
	&PKTINFO(nt36523n_gts9fe_csot_300),
	&PKTINFO(nt36523n_gts9fe_csot_301),
	&PKTINFO(nt36523n_gts9fe_csot_302),
	&PKTINFO(nt36523n_gts9fe_csot_303),
	&PKTINFO(nt36523n_gts9fe_csot_304),
	&PKTINFO(nt36523n_gts9fe_csot_305),
	&PKTINFO(nt36523n_gts9fe_csot_306),
	&PKTINFO(nt36523n_gts9fe_csot_307),
	&PKTINFO(nt36523n_gts9fe_csot_308),
	&PKTINFO(nt36523n_gts9fe_csot_309),
	&PKTINFO(nt36523n_gts9fe_csot_310),
	&PKTINFO(nt36523n_gts9fe_csot_311),
	&PKTINFO(nt36523n_gts9fe_csot_312),
	&PKTINFO(nt36523n_gts9fe_csot_313),
	&PKTINFO(nt36523n_gts9fe_csot_314),
	&PKTINFO(nt36523n_gts9fe_csot_315),
	&PKTINFO(nt36523n_gts9fe_csot_316),
	&PKTINFO(nt36523n_gts9fe_csot_317),
	&PKTINFO(nt36523n_gts9fe_csot_318),
	&PKTINFO(nt36523n_gts9fe_csot_319),
	&PKTINFO(nt36523n_gts9fe_csot_320),
	&PKTINFO(nt36523n_gts9fe_csot_321),
	&PKTINFO(nt36523n_gts9fe_csot_322),
	&PKTINFO(nt36523n_gts9fe_csot_323),
	&PKTINFO(nt36523n_gts9fe_csot_324),
	&PKTINFO(nt36523n_gts9fe_csot_325),
	&PKTINFO(nt36523n_gts9fe_csot_326),
	&PKTINFO(nt36523n_gts9fe_csot_327),
	&PKTINFO(nt36523n_gts9fe_csot_328),
	&PKTINFO(nt36523n_gts9fe_csot_329),
	&PKTINFO(nt36523n_gts9fe_csot_330),
	&PKTINFO(nt36523n_gts9fe_csot_331),
	&PKTINFO(nt36523n_gts9fe_csot_332),
	&PKTINFO(nt36523n_gts9fe_csot_333),
	&PKTINFO(nt36523n_gts9fe_csot_334),
	&PKTINFO(nt36523n_gts9fe_csot_335),
	&PKTINFO(nt36523n_gts9fe_csot_336),
	&PKTINFO(nt36523n_gts9fe_csot_337),
	&PKTINFO(nt36523n_gts9fe_csot_338),
	&PKTINFO(nt36523n_gts9fe_csot_339),
	&PKTINFO(nt36523n_gts9fe_csot_340),
	&PKTINFO(nt36523n_gts9fe_csot_341),
	&PKTINFO(nt36523n_gts9fe_csot_342),
	&PKTINFO(nt36523n_gts9fe_csot_343),
	&PKTINFO(nt36523n_gts9fe_csot_344),
	&PKTINFO(nt36523n_gts9fe_csot_345),
	&PKTINFO(nt36523n_gts9fe_csot_346),
	&PKTINFO(nt36523n_gts9fe_csot_347),
	&PKTINFO(nt36523n_gts9fe_csot_348),
	&PKTINFO(nt36523n_gts9fe_csot_349),
	&PKTINFO(nt36523n_gts9fe_csot_350),
	&PKTINFO(nt36523n_gts9fe_csot_351),
	&PKTINFO(nt36523n_gts9fe_csot_352),
	&PKTINFO(nt36523n_gts9fe_csot_353),
	&PKTINFO(nt36523n_gts9fe_csot_354),
	&PKTINFO(nt36523n_gts9fe_csot_355),
	&PKTINFO(nt36523n_gts9fe_csot_356),
	&PKTINFO(nt36523n_gts9fe_csot_357),
	&PKTINFO(nt36523n_gts9fe_csot_358),
	&PKTINFO(nt36523n_gts9fe_csot_359),
	&PKTINFO(nt36523n_gts9fe_csot_360),
	&PKTINFO(nt36523n_gts9fe_csot_361),
	&PKTINFO(nt36523n_gts9fe_csot_362),
	&PKTINFO(nt36523n_gts9fe_csot_363),
	&PKTINFO(nt36523n_gts9fe_csot_364),
	&PKTINFO(nt36523n_gts9fe_csot_365),
	&PKTINFO(nt36523n_gts9fe_csot_366),
	&PKTINFO(nt36523n_gts9fe_csot_367),
	&PKTINFO(nt36523n_gts9fe_csot_368),
//	&PKTINFO(nt36523n_gts9fe_csot_369),
	&PKTINFO(nt36523n_gts9fe_csot_370),
	&PKTINFO(nt36523n_gts9fe_csot_371),
	&PKTINFO(nt36523n_gts9fe_csot_372),
	&PKTINFO(nt36523n_gts9fe_csot_373),
	&PKTINFO(nt36523n_gts9fe_csot_374),
	&PKTINFO(nt36523n_gts9fe_csot_375),
	&PKTINFO(nt36523n_gts9fe_csot_376),
	&PKTINFO(nt36523n_gts9fe_csot_377),
	&PKTINFO(nt36523n_gts9fe_csot_sleep_out),
	&DLYINFO(nt36523n_gts9fe_csot_wait_105msec),
	&PKTINFO(nt36523n_gts9fe_csot_display_on),
	&PNOBJ_CONFIG(nt36523n_gts9fe_csot_set_wait_tx_done_property_auto),
};

static void *nt36523n_gts9fe_csot_res_init_cmdtbl[] = {
	&RESINFO(nt36523n_gts9fe_csot_id),
};

static void *nt36523n_gts9fe_csot_set_bl_cmdtbl[] = {
	&PKTINFO(nt36523n_gts9fe_csot_brightness),
};

static void *nt36523n_gts9fe_csot_display_on_cmdtbl[] = {
	&DLYINFO(nt36523n_gts9fe_csot_wait_40msec),
	&PKTINFO(nt36523n_gts9fe_csot_brightness_on),
	&PKTINFO(nt36523n_gts9fe_csot_brightness),
};

static void *nt36523n_gts9fe_csot_display_off_cmdtbl[] = {
	&PKTINFO(nt36523n_gts9fe_csot_display_off),
	&DLYINFO(nt36523n_gts9fe_csot_wait_20msec),
};

static void *nt36523n_gts9fe_csot_exit_cmdtbl[] = {
	&PKTINFO(nt36523n_gts9fe_csot_sleep_in),
};

static void *nt36523n_gts9fe_csot_display_mode_cmdtbl[] = {
	&PNOBJ_CONFIG(nt36523n_gts9fe_csot_set_wait_tx_done_property_off),
	&PKTINFO(nt36523n_gts9fe_csot_brightness),
	/* Will flush on next VFP */
	&PNOBJ_CONFIG(nt36523n_gts9fe_csot_set_wait_tx_done_property_auto),
};

static struct seqinfo nt36523n_gts9fe_csot_seqtbl[] = {
	SEQINFO_INIT(PANEL_INIT_SEQ, nt36523n_gts9fe_csot_init_cmdtbl),
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, nt36523n_gts9fe_csot_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_SET_BL_SEQ, nt36523n_gts9fe_csot_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, nt36523n_gts9fe_csot_display_mode_cmdtbl), /* Dummy */
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, nt36523n_gts9fe_csot_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, nt36523n_gts9fe_csot_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, nt36523n_gts9fe_csot_exit_cmdtbl),
};


/* BLIC 1 SETTING START */

static u8 NT36523N_GTS9FE_CSOT_ISL98608_I2C_INIT[] = {
	0x06, 0x0D,
	0x08, 0x02,
	0x09, 0x02,
};

#ifdef DEBUG_I2C_READ
static u8 NT36523N_GTS9FE_CSOT_ISL98608_I2C_DUMP[] = {
	0x06, 0x00,
	0x08, 0x00,
	0x09, 0x00,
};
#endif

static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_isl98608_i2c_init, I2C_PKT_TYPE_WR, NT36523N_GTS9FE_CSOT_ISL98608_I2C_INIT, 0);
#ifdef DEBUG_I2C_READ
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_isl98608_i2c_dump, I2C_PKT_TYPE_RD, NT36523N_GTS9FE_CSOT_ISL98608_I2C_DUMP, 0);
#endif

static void *nt36523n_gts9fe_csot_isl98608_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36523n_gts9fe_csot_isl98608_i2c_dump),
#endif
	&PKTINFO(nt36523n_gts9fe_csot_isl98608_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36523n_gts9fe_csot_isl98608_i2c_dump),
#endif
};

static void *nt36523n_gts9fe_csot_isl98608_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36523n_gts9fe_csot_isl98608_i2c_dump),
#endif
};

static struct seqinfo nt36523n_gts9fe_csot_isl98608_seq_tbl[] = {
	SEQINFO_INIT(PANEL_BLIC_I2C_ON_SEQ, nt36523n_gts9fe_csot_isl98608_init_cmdtbl),
	SEQINFO_INIT(PANEL_BLIC_I2C_OFF_SEQ, nt36523n_gts9fe_csot_isl98608_exit_cmdtbl),
};

static struct blic_data nt36523n_gts9fe_csot_isl98608_blic_data = {
	.name = "isl98608",
	.seqtbl = nt36523n_gts9fe_csot_isl98608_seq_tbl,
	.nr_seqtbl = ARRAY_SIZE(nt36523n_gts9fe_csot_isl98608_seq_tbl),
};

/* BLIC 2 SETTING START */
static u8 NT36523N_GTS9FE_CSOT_MAX77816_I2C_INIT[] = {
	0x03, 0x30,
	0x02, 0x8E,
	0x03, 0x70,
	0x04, 0x78,
};

static u8 NT36523N_GTS9FE_CSOT_MAX77816_I2C_EXIT_BLEN[] = {
	0x03, 0x32,
};

#ifdef DEBUG_I2C_READ
static u8 NT36523N_GTS9FE_CSOT_MAX77816_I2C_DUMP[] = {
	0x04, 0x00,
	0x03, 0x00,
};
#endif

static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_max77816_i2c_init, I2C_PKT_TYPE_WR, NT36523N_GTS9FE_CSOT_MAX77816_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_max77816_i2c_exit_blen, I2C_PKT_TYPE_WR, NT36523N_GTS9FE_CSOT_MAX77816_I2C_EXIT_BLEN, 0);
#ifdef DEBUG_I2C_READ
static DEFINE_STATIC_PACKET(nt36523n_gts9fe_csot_max77816_i2c_dump, I2C_PKT_TYPE_RD, NT36523N_GTS9FE_CSOT_MAX77816_I2C_DUMP, 0);
#endif

static void *nt36523n_gts9fe_csot_max77816_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36523n_gts9fe_csot_max77816_i2c_dump),
#endif
	&PKTINFO(nt36523n_gts9fe_csot_max77816_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36523n_gts9fe_csot_max77816_i2c_dump),
#endif
};

static void *nt36523n_gts9fe_csot_max77816_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(nt36523n_gts9fe_csot_max77816_i2c_dump),
#endif
	&PKTINFO(nt36523n_gts9fe_csot_max77816_i2c_exit_blen),
};

static struct seqinfo nt36523n_gts9fe_csot_max77816_seq_tbl[] = {
	SEQINFO_INIT(PANEL_BLIC_I2C_ON_SEQ, nt36523n_gts9fe_csot_max77816_init_cmdtbl),
	SEQINFO_INIT(PANEL_BLIC_I2C_OFF_SEQ, nt36523n_gts9fe_csot_max77816_exit_cmdtbl),
};

static struct blic_data nt36523n_gts9fe_csot_max77816_blic_data = {
	.name = "max77816",
	.seqtbl = nt36523n_gts9fe_csot_max77816_seq_tbl,
	.nr_seqtbl = ARRAY_SIZE(nt36523n_gts9fe_csot_max77816_seq_tbl),
};

static struct blic_data *nt36523n_gts9fe_csot_blic_tbl[] = {
	&nt36523n_gts9fe_csot_isl98608_blic_data,
	&nt36523n_gts9fe_csot_max77816_blic_data,
};
/* BLIC SETTING END */


struct common_panel_info nt36523n_gts9fe_csot_panel_info = {
	.ldi_name = "nt36523n",
	.name = "nt36523n_gts9fe_csot",
	.model = "csot_10_9_inch",
	.vendor = "CSO",
	.id = 0x4CF240,
	.rev = 0,
	.ddi_props = {
		.err_fg_recovery = false,
		.support_vrr = true,
		.init_seq_by_lpdt = true,
	},
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.common_panel_modes = &nt36523n_gts9fe_csot_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(nt36523n_gts9fe_csot_default_resol),
		.resol = nt36523n_gts9fe_csot_default_resol,
	},
	.vrrtbl = nt36523n_gts9fe_csot_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(nt36523n_gts9fe_csot_default_vrrtbl),
	.maptbl = nt36523n_gts9fe_csot_maptbl,
	.nr_maptbl = ARRAY_SIZE(nt36523n_gts9fe_csot_maptbl),
	.seqtbl = nt36523n_gts9fe_csot_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(nt36523n_gts9fe_csot_seqtbl),
	.rditbl = NULL,
	.nr_rditbl = 0,
	.restbl = NULL,
	.nr_restbl = 0,
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &nt36523n_gts9fe_csot_panel_dimming_info,
	},
	.blic_data_tbl = nt36523n_gts9fe_csot_blic_tbl,
	.nr_blic_data_tbl = ARRAY_SIZE(nt36523n_gts9fe_csot_blic_tbl),
};
#endif /* __NT36523N_GTS9FE_CSOT_PANEL_H__ */
