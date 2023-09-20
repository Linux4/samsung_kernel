/*
 * Header file for TD4160 Driver
 *
 * Copyright (c) 2022 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __TD4160_A14X_00_PANEL_H__
#define __TD4160_A14X_00_PANEL_H__

#include "../panel.h"
#include "../panel_drv.h"
#include "tft_common.h"
#include "tft_function.h"
#include "td4160_a14x_00_resol.h"

#undef __pn_name__
#define __pn_name__	a14x

#undef __PN_NAME__
#define __PN_NAME__

#define TD4160_NR_STEP (256)
#define TD4160_HBM_STEP (51)
#define TD4160_TOTAL_STEP (TD4160_NR_STEP + TD4160_HBM_STEP) /* 0 ~ 306 */

#define IGNORE_PANEL_ON_OFF_CONTROL

static unsigned int td4160_a14x_00_brt_tbl[TD4160_TOTAL_STEP] = {
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

static unsigned int td4160_a14x_00_step_cnt_tbl[TD4160_TOTAL_STEP] = {
	[0 ... 255] = 1,
	/* HBM */
	[256 ... 306] = 1,
};

struct brightness_table td4160_a14x_00_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = td4160_a14x_00_brt_tbl,
	.sz_brt = ARRAY_SIZE(td4160_a14x_00_brt_tbl),
	.sz_ui_brt = TD4160_NR_STEP,
	.sz_hbm_brt = TD4160_HBM_STEP,
	.lum = td4160_a14x_00_brt_tbl,
	.sz_lum = ARRAY_SIZE(td4160_a14x_00_brt_tbl),
	.sz_ui_lum = TD4160_NR_STEP,
	.sz_hbm_lum = TD4160_HBM_STEP,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = td4160_a14x_00_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(td4160_a14x_00_step_cnt_tbl),
	.vtotal = 0,
};

static struct panel_dimming_info td4160_a14x_00_panel_dimming_info = {
	.name = "td4160_a14x",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = -1,
	.brt_tbl = &td4160_a14x_00_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,	/* read dim flash when probe or not */
	.hbm_aor = NULL,
};

static u8 td4160_a14x_00_brt_table[TD4160_TOTAL_STEP][1] = {
	{0},
	{1}, {1}, {2}, {2}, {3}, {3}, {4}, {4}, {5}, {5},
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
	{77}, {78}, {79}, {80}, {81}, {81}, {82}, {83}, {84}, {85},
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
	{208}, {209}, {210}, {211}, {212}, {213}, {214}, {215}, {215}, {216},
	{217}, {218}, {219}, {220}, {220}, {221}, {222}, {223}, {224}, {225},
	{225}, {226}, {227}, {228}, {229}, {230}, {231}, {231}, {232}, {233},
	{234}, {235}, {236}, {236}, {237}, {238}, {239}, {240}, {241}, {242},
	{242}, {243}, {244}, {245}, {246}, {247}, {247}, {248}, {249}, {250},
	{251}, {252}, {252}, {253}, {254}, {255},
};

static struct maptbl td4160_a14x_00_maptbl[MAX_MAPTBL] = {
	[BRT_MAPTBL] = DEFINE_2D_MAPTBL(td4160_a14x_00_brt_table,
			&TFT_FUNC(TFT_MAPTBL_INIT_BRT),
			&TFT_FUNC(TFT_MAPTBL_GETIDX_BRT),
			&TFT_FUNC(TFT_MAPTBL_COPY_DEFAULT)),
};

#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
static u8 SEQ_TD4160_A14X_00_SLEEP_OUT[] = {
	0x11
};

static u8 SEQ_TD4160_A14X_00_SLEEP_IN[] = {
	0x10
};

static u8 SEQ_TD4160_A14X_00_DISPLAY_ON[] = {
	0x29
};

static u8 SEQ_TD4160_A14X_00_DISPLAY_OFF[] = {
	0x28
};
#endif

static u8 SEQ_TD4160_A14X_00_BRIGHTNESS[] = {
	0x51,
	0xFF,
};

/* < CABC Mode control Function > */

#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
static u8 SEQ_TD4160_A14X_00_BRIGHTNESS_ON[] = {
	0x53,
	0x2C,
};

static unsigned char SEQ_TD4160_A14X_00_CABC_MODE[] = {
	0x55,
	0x03,
};

static unsigned char SEQ_TD4160_A14X_00_CABC_MIN[] = {
	0x5E,
	0x30,
};
#endif

/* Display config (1) */
#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
static unsigned char SEQ_TD4160_A14X_00_001[] = {
	0xB0,
	0x04,
};

static unsigned char SEQ_TD4160_A14X_00_002[] = {
	0xB6,
	0x30, 0x6A, 0x00, 0x06, 0xC3, 0x03,
};

static unsigned char SEQ_TD4160_A14X_00_003[] = {
	0xB7,
	0x31, 0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_004[] = {
	0xB8,
	0x00, 0x78, 0x64, 0x10, 0x64, 0xB4,
};

static unsigned char SEQ_TD4160_A14X_00_005[] = {
	0xB9,
	0x00, 0x78, 0x64, 0x10, 0x64, 0xB4,
};

static unsigned char SEQ_TD4160_A14X_00_006[] = {
	0xBA,
	0x03, 0x79, 0x46, 0x10, 0x1F, 0x33,
};

static unsigned char SEQ_TD4160_A14X_00_007[] = {
	0xBB,
	0x00, 0xB4, 0xA0,
};

static unsigned char SEQ_TD4160_A14X_00_008[] = {
	0xBC,
	0x00, 0xB4, 0xA0,
};

static unsigned char SEQ_TD4160_A14X_00_009[] = {
	0xBD,
	0x00, 0xB4, 0xA0,
};

static unsigned char SEQ_TD4160_A14X_00_010[] = {
	0xBE,
	0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_011[] = {
	0xC0,
	0x00, 0x60, 0x10, 0x06, 0x40, 0x00, 0x16, 0x06, 0x98, 0x00,
	0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_012[] = {
	0xC1,
	0x30, 0x11, 0x50, 0xFA, 0x01, 0x00, 0x00, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x40, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xF0, 0x07, 0x08, 0x07, 0xD0, 0x0A, 0xBE,
};

static unsigned char SEQ_TD4160_A14X_00_013[] = {
	0xC2,
	0x01, 0x20, 0x50, 0x14, 0x04, 0x10, 0x0B, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x05, 0xF0, 0x54, 0x08, 0x08, 0x05, 0x05, 0xC5,
	0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x11, 0x00, 0x80, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_014[] = {
	0xC3,
	0x01, 0x00, 0x55, 0x01, 0x00, 0x46, 0x54, 0x00, 0x00, 0x00,
	0x20, 0x01, 0x01, 0x00, 0x55, 0x01, 0x00, 0x46, 0x54, 0x00,
	0x00, 0x00, 0x20, 0x02, 0x01, 0x00, 0x55, 0x01, 0x00, 0x46,
	0x54, 0x00, 0x00, 0x00, 0x20, 0x00, 0x01, 0x00, 0x55, 0x01,
	0x00, 0x46, 0x54, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_015[] = {
	0xC4,
	0x00, 0x00, 0x5E, 0x64, 0x00, 0x03, 0x05, 0x13, 0x15, 0x17,
	0x19, 0x5D, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x5F, 0x5D, 0x1A, 0x18, 0x16, 0x14, 0x06,
	0x04, 0x00, 0x64, 0x5E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x57, 0x55, 0x55, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0x55, 0x55, 0xD5,
};

static unsigned char SEQ_TD4160_A14X_00_016[] = {
	0xC5,
	0x08, 0x00, 0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_017[] = {
	0xC6,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x22, 0x04, 0x22, 0x01, 0x00,
	0x61, 0x00, 0x00, 0x00, 0x01, 0x00, 0x61, 0x00, 0x01, 0x05,
	0x01, 0x0B, 0x01, 0x35, 0xFF, 0x8F, 0x06, 0x05, 0x01, 0xC0,
	0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x20, 0x00, 0x00, 0x00, 0xC0, 0x11,
	0x00, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00,
	0x50, 0x00, 0x33, 0x03, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_018[] = {
	0xC7,
	0x00, 0x00, 0x00, 0x6C, 0x00, 0xA6, 0x00, 0xB3, 0x00, 0xCA,
	0x00, 0xD8, 0x00, 0xEB, 0x00, 0xEF, 0x01, 0x0C, 0x00, 0xDE,
	0x01, 0x39, 0x01, 0x00, 0x01, 0x4C, 0x01, 0x0A, 0x01, 0x7F,
	0x01, 0x8B, 0x02, 0x19, 0x02, 0x93, 0x02, 0xA0, 0x00, 0x00,
	0x00, 0x6C, 0x00, 0xA6, 0x00, 0xB3, 0x00, 0xCA, 0x00, 0xD8,
	0x00, 0xEB, 0x00, 0xEF, 0x01, 0x0C, 0x00, 0xDE, 0x01, 0x39,
	0x01, 0x00, 0x01, 0x4C, 0x01, 0x0A, 0x01, 0x7F, 0x01, 0x8B,
	0x02, 0x19, 0x02, 0x93, 0x02, 0xA0,
};

static unsigned char SEQ_TD4160_A14X_00_019[] = {
	0xCB,
	0x02, 0xD0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x40, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
};

static unsigned char SEQ_TD4160_A14X_00_020[] = {
	0xCE,
	0x7D, 0x40, 0x48, 0x56, 0x67, 0x78, 0x88, 0x98, 0xA7, 0xB5,
	0xC3, 0xD1, 0xDE, 0xE9, 0xF2, 0xFA, 0xFF, 0x18, 0x40, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_021[] = {
	0xCF,
	0x00,
};

static unsigned char SEQ_TD4160_A14X_00_022[] = {
	0xD0,
	0xC1, 0x46, 0x81, 0x66, 0x09, 0x90, 0x00, 0xCC, 0xF2, 0xFF,
	0x11, 0x46, 0x06, 0x7E, 0x09, 0x08, 0xCC, 0x1B, 0xF0, 0x06,
};

static unsigned char SEQ_TD4160_A14X_00_023[] = {
	0xD1,
	0xCC, 0xD4, 0x1B, 0x33, 0x33, 0x17, 0x07, 0xBB, 0x33, 0x33,
	0x33, 0x33, 0x00, 0x3B, 0x77, 0x07, 0x3B, 0x30, 0x06, 0x72,
	0x33, 0x13, 0x00, 0xD7, 0x0C, 0x33, 0x02, 0x00, 0x18, 0x70,
	0x18, 0x77, 0x11, 0x11, 0x11, 0x20, 0x20,
};

static unsigned char SEQ_TD4160_A14X_00_024[] = {
	0xD2,
	0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_025[] = {
	0xD3,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF,
	0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7,
	0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF,
	0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF,
	0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7,
	0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF,
	0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF,
	0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7,
	0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF,
	0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF,
	0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7,
	0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF,
	0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF,
	0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7,
	0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xF7, 0xFF,
	0xFF, 0xF7, 0xFF,
};

static unsigned char SEQ_TD4160_A14X_00_026[] = {
	0xD4,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20,
	0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_027[] = {
	0xD6,
	0x00,
};

static unsigned char SEQ_TD4160_A14X_00_028[] = {
	0xD7,
	0x01, 0x00, 0x12, 0x12, 0x00, 0x62, 0x00, 0x16, 0x00, 0x62,
	0x00, 0x16, 0x03, 0x83, 0x80, 0x85, 0x85, 0x85, 0x87, 0x84,
	0x45, 0x86, 0x87, 0x80, 0x88, 0x86, 0x89, 0x83, 0x83, 0x87,
	0x84, 0x88, 0x8A, 0x0C, 0x0B, 0x0A, 0x0A, 0x0A, 0x07, 0x07,
	0x06, 0x06, 0x00, 0x08, 0x0A, 0x0A,
};

static unsigned char SEQ_TD4160_A14X_00_029[] = {
	0xD8,
	0x40, 0x99, 0x26, 0xED, 0x16, 0x6C, 0x16, 0x6C, 0x16, 0x6C,
	0x00, 0x14, 0x00, 0x14, 0x00, 0x14, 0x01, 0x0C, 0x00, 0x00,
	0x01, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_030[] = {
	0xD9,
	0x00, 0x02, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_031[] = {
	0xDD,
	0x30, 0x06, 0x23, 0x65,
};

static unsigned char SEQ_TD4160_A14X_00_032[] = {
	0xDE,
	0x00, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_033[] = {
	0xE5,
	0x03, 0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_034[] = {
	0xE6,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_035[] = {
	0xEA,
	0x02, 0x07, 0x07, 0x04, 0x80, 0x10, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x03, 0x23, 0x00, 0x07, 0x00, 0xED, 0x00, 0xED, 0x00,
	0xED, 0x01, 0x28, 0x01, 0x28, 0x00, 0x62, 0x00, 0x62, 0x00,
	0x62, 0x01, 0x0F, 0x01, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_036[] = {
	0xEB,
	0x07, 0x60, 0x76, 0x07, 0x61, 0x01, 0x08, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_037[] = {
	0xEC,
	0x02, 0xD0, 0x01, 0x70, 0x73, 0x10, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x02, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00,
};

static unsigned char SEQ_TD4160_A14X_00_038[] = {
	0xED,
	0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x65, 0xF6,
	0xE2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x41, 0x10, 0x01,
};

static unsigned char SEQ_TD4160_A14X_00_039[] = {
	0xEE,
	0x05, 0x00, 0x05, 0x00, 0xC0, 0x0B, 0x00, 0x00, 0x03, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0xC0, 0x03, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x55, 0x17, 0x04,
	0x65, 0x40, 0x00, 0x00, 0x55, 0x17, 0x04, 0x65, 0x40, 0x00,
	0x00, 0x55, 0x17, 0x04, 0x65, 0x40, 0x00, 0x00, 0x40, 0x17,
	0x04, 0x65, 0x40, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
	0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x40, 0x15, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02,
};

static unsigned char SEQ_TD4160_A14X_00_040[] = {
	0xB0,
	0x03,
};
#endif

#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
static DEFINE_STATIC_PACKET(td4160_a14x_00_sleep_out, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_SLEEP_OUT, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_sleep_in, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_SLEEP_IN, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_display_on, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_DISPLAY_ON, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_display_off, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_DISPLAY_OFF, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_brightness_on, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_BRIGHTNESS_ON, 0);
#endif

static DEFINE_PKTUI(td4160_a14x_00_brightness, &td4160_a14x_00_maptbl[BRT_MAPTBL], 1);
static DEFINE_VARIABLE_PACKET(td4160_a14x_00_brightness, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_BRIGHTNESS, 0);

#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
static DEFINE_STATIC_PACKET(td4160_a14x_00_001, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_001, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_002, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_002, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_003, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_003, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_004, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_004, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_005, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_005, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_006, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_006, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_007, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_007, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_008, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_008, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_009, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_009, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_010, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_010, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_011, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_011, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_012, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_012, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_013, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_013, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_014, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_014, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_015, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_015, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_016, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_016, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_017, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_017, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_018, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_018, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_019, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_019, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_020, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_020, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_021, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_021, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_022, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_022, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_023, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_023, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_024, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_024, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_025, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_025, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_026, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_026, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_027, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_027, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_028, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_028, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_029, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_029, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_030, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_030, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_031, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_031, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_032, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_032, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_033, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_033, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_034, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_034, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_035, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_035, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_036, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_036, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_037, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_037, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_038, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_038, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_039, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_039, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_040, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_040, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_cabc_mode, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_CABC_MODE, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_cabc_min, DSI_PKT_TYPE_WR, SEQ_TD4160_A14X_00_CABC_MIN, 0);
#endif

static DEFINE_PANEL_MDELAY(td4160_a14x_00_wait_20msec, 20); /* 1 frame */
static DEFINE_PANEL_MDELAY(td4160_a14x_00_wait_40msec, 40);
static DEFINE_PANEL_MDELAY(td4160_a14x_00_wait_100msec, 100);

//static DEFINE_PNOBJ_CONFIG(a14x_00_set_wait_tx_done_property_off, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
//static DEFINE_PNOBJ_CONFIG(a14x_00_set_wait_tx_done_property_auto, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

static u8 TD4160_A14X_00_ID[TFT_COMMON_ID_LEN];
static DEFINE_RDINFO(td4160_a14x_00_id1, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DA_OFS, TFT_COMMON_ID_DA_LEN);
static DEFINE_RDINFO(td4160_a14x_00_id2, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DB_OFS, TFT_COMMON_ID_DB_LEN);
static DEFINE_RDINFO(td4160_a14x_00_id3, DSI_PKT_TYPE_RD, TFT_COMMON_ID_DA_REG, TFT_COMMON_ID_DC_OFS, TFT_COMMON_ID_DC_LEN);
static DECLARE_RESUI(td4160_a14x_00_id) = {
	{ .rditbl = &RDINFO(td4160_a14x_00_id1), .offset = 0 },
	{ .rditbl = &RDINFO(td4160_a14x_00_id2), .offset = 1 },
	{ .rditbl = &RDINFO(td4160_a14x_00_id3), .offset = 2 },
};
static DEFINE_RESOURCE(td4160_a14x_00_id, TD4160_A14X_00_ID, RESUI(td4160_a14x_00_id));

#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
static void *td4160_a14x_00_init_cmdtbl[] = {
	&PKTINFO(td4160_a14x_00_001),
	&PKTINFO(td4160_a14x_00_002),
	&PKTINFO(td4160_a14x_00_003),
	&PKTINFO(td4160_a14x_00_004),
	&PKTINFO(td4160_a14x_00_005),
	&PKTINFO(td4160_a14x_00_006),
	&PKTINFO(td4160_a14x_00_007),
	&PKTINFO(td4160_a14x_00_008),
	&PKTINFO(td4160_a14x_00_009),
	&PKTINFO(td4160_a14x_00_010),
	&PKTINFO(td4160_a14x_00_011),
	&PKTINFO(td4160_a14x_00_012),
	&PKTINFO(td4160_a14x_00_013),
	&PKTINFO(td4160_a14x_00_014),
	&PKTINFO(td4160_a14x_00_015),
	&PKTINFO(td4160_a14x_00_016),
	&PKTINFO(td4160_a14x_00_017),
	&PKTINFO(td4160_a14x_00_018),
	&PKTINFO(td4160_a14x_00_019),
	&PKTINFO(td4160_a14x_00_020),
	&PKTINFO(td4160_a14x_00_021),
	&PKTINFO(td4160_a14x_00_022),
	&PKTINFO(td4160_a14x_00_023),
	&PKTINFO(td4160_a14x_00_024),
	&PKTINFO(td4160_a14x_00_025),
	&PKTINFO(td4160_a14x_00_026),
	&PKTINFO(td4160_a14x_00_027),
	&PKTINFO(td4160_a14x_00_028),
	&PKTINFO(td4160_a14x_00_029),
	&PKTINFO(td4160_a14x_00_030),
	&PKTINFO(td4160_a14x_00_031),
	&PKTINFO(td4160_a14x_00_032),
	&PKTINFO(td4160_a14x_00_033),
	&PKTINFO(td4160_a14x_00_034),
	&PKTINFO(td4160_a14x_00_035),
	&PKTINFO(td4160_a14x_00_036),
	&PKTINFO(td4160_a14x_00_037),
	&PKTINFO(td4160_a14x_00_038),
	&PKTINFO(td4160_a14x_00_039),
	&PKTINFO(td4160_a14x_00_040),
	&PKTINFO(td4160_a14x_00_cabc_mode),
	&PKTINFO(td4160_a14x_00_cabc_min),
	&PKTINFO(td4160_a14x_00_sleep_out),
	&DLYINFO(td4160_a14x_00_wait_100msec),
	&PKTINFO(td4160_a14x_00_display_on),
};
#endif

static void *td4160_a14x_00_res_init_cmdtbl[] = {
	&RESINFO(td4160_a14x_00_id),
};

static void *td4160_a14x_00_set_bl_cmdtbl[] = {
	&PKTINFO(td4160_a14x_00_brightness),
};

#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
static void *td4160_a14x_00_display_on_cmdtbl[] = {
	&DLYINFO(td4160_a14x_00_wait_40msec),
	&PKTINFO(td4160_a14x_00_brightness),
	&PKTINFO(td4160_a14x_00_brightness_on),
};

static void *td4160_a14x_00_display_off_cmdtbl[] = {
	&PKTINFO(td4160_a14x_00_display_off),
	&DLYINFO(td4160_a14x_00_wait_20msec),
};

static void *td4160_a14x_00_exit_cmdtbl[] = {
	&PKTINFO(td4160_a14x_00_sleep_in),
};
#endif

static void *td4160_a14x_00_display_mode_cmdtbl[] = {
//	&PNOBJ_CONFIG(a14x_00_set_wait_tx_done_property_off),
	&PKTINFO(td4160_a14x_00_brightness),
	/* Will flush on next VFP */
//	&PNOBJ_CONFIG(a14x_00_set_wait_tx_done_property_auto),
};

static struct seqinfo td4160_a14x_00_seqtbl[] = {
#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
	SEQINFO_INIT(PANEL_INIT_SEQ, td4160_a14x_00_init_cmdtbl),
#endif
	SEQINFO_INIT(PANEL_RES_INIT_SEQ, td4160_a14x_00_res_init_cmdtbl),
	SEQINFO_INIT(PANEL_SET_BL_SEQ, td4160_a14x_00_set_bl_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_MODE_SEQ, td4160_a14x_00_display_mode_cmdtbl), /* Dummy */
#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
	SEQINFO_INIT(PANEL_DISPLAY_ON_SEQ, td4160_a14x_00_display_on_cmdtbl),
	SEQINFO_INIT(PANEL_DISPLAY_OFF_SEQ, td4160_a14x_00_display_off_cmdtbl),
	SEQINFO_INIT(PANEL_EXIT_SEQ, td4160_a14x_00_exit_cmdtbl),
#endif
};

/* BLIC SETTING START */
static u8 TD4160_A14X_00_KTZ8864_I2C_INIT[] = {
	0x0C, 0x24,
	0x0D, 0x1E,
	0x0E, 0x1E,
	0x09, 0x99,
	0x02, 0x6B,
	0x03, 0x0D,
	0x11, 0x74,
	0x04, 0x03,
	0x05, 0xC2,
	0x10, 0x66,
	0x08, 0x13,
};

static u8 TD4160_A14X_00_KTZ8864_I2C_EXIT_BLEN[] = {
	0x08, 0x00,
};

#ifdef DEBUG_I2C_READ
static u8 TD4160_A14X_00_KTZ8864_I2C_DUMP[] = {
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

static DEFINE_STATIC_PACKET(td4160_a14x_00_ktz8864_i2c_init, I2C_PKT_TYPE_WR, TD4160_A14X_00_KTZ8864_I2C_INIT, 0);
static DEFINE_STATIC_PACKET(td4160_a14x_00_ktz8864_i2c_exit_blen, I2C_PKT_TYPE_WR, TD4160_A14X_00_KTZ8864_I2C_EXIT_BLEN, 0);
#ifdef DEBUG_I2C_READ
static DEFINE_STATIC_PACKET(td4160_a14x_00_ktz8864_i2c_dump, I2C_PKT_TYPE_RD, TD4160_A14X_00_KTZ8864_I2C_DUMP, 0);
#endif

static void *td4160_a14x_00_ktz8864_init_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(td4160_a14x_00_ktz8864_i2c_dump),
#endif
	&PKTINFO(td4160_a14x_00_ktz8864_i2c_init),
#ifdef DEBUG_I2C_READ
	&PKTINFO(td4160_a14x_00_ktz8864_i2c_dump),
#endif
};

static void *td4160_a14x_00_ktz8864_exit_cmdtbl[] = {
#ifdef DEBUG_I2C_READ
	&PKTINFO(td4160_a14x_00_ktz8864_i2c_dump),
#endif
	&PKTINFO(td4160_a14x_00_ktz8864_i2c_exit_blen),
};

static struct seqinfo td4160_a14x_00_ktz8864_seq_tbl[] = {
#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
	SEQINFO_INIT(PANEL_BLIC_I2C_ON_SEQ, td4160_a14x_00_ktz8864_init_cmdtbl),
	SEQINFO_INIT(PANEL_BLIC_I2C_OFF_SEQ, td4160_a14x_00_ktz8864_exit_cmdtbl),
#endif
};

static struct blic_data td4160_a14x_00_ktz8864_blic_data = {
	.name = "ktz8864",
	.seqtbl = td4160_a14x_00_ktz8864_seq_tbl,
	.nr_seqtbl = ARRAY_SIZE(td4160_a14x_00_ktz8864_seq_tbl),
};

static struct blic_data *td4160_a14x_00_blic_tbl[] = {
	&td4160_a14x_00_ktz8864_blic_data,
};
/* BLIC SETTING END */


struct common_panel_info td4160_a14x_00_panel_info = {
	.ldi_name = "td4160",
	.name = "td4160_a14x_00",
	.model = "tianma_6_58_inch",
	.vendor = "TMC",
	.id = 0x2A6220,
	.rev = 0,
	.ddi_props = {
		.err_fg_recovery = false,
		.support_vrr = true,
#if !defined(IGNORE_PANEL_ON_OFF_CONTROL)
		.init_seq_by_lpdt = true,
#else
		.init_seq_by_lpdt = false,
#endif
	},
#if defined(CONFIG_USDM_PANEL_DISPLAY_MODE)
	.common_panel_modes = &td4160_a14x_00_display_modes,
#endif
	.mres = {
		.nr_resol = ARRAY_SIZE(td4160_a14x_00_default_resol),
		.resol = td4160_a14x_00_default_resol,
	},
	.vrrtbl = td4160_a14x_00_default_vrrtbl,
	.nr_vrrtbl = ARRAY_SIZE(td4160_a14x_00_default_vrrtbl),
	.maptbl = td4160_a14x_00_maptbl,
	.nr_maptbl = ARRAY_SIZE(td4160_a14x_00_maptbl),
	.seqtbl = td4160_a14x_00_seqtbl,
	.nr_seqtbl = ARRAY_SIZE(td4160_a14x_00_seqtbl),
	.rditbl = NULL,
	.nr_rditbl = 0,
	.restbl = NULL,
	.nr_restbl = 0,
	.dumpinfo = NULL,
	.nr_dumpinfo = 0,
	.panel_dim_info = {
		[PANEL_BL_SUBDEV_TYPE_DISP] = &td4160_a14x_00_panel_dimming_info,
	},
	.blic_data_tbl = td4160_a14x_00_blic_tbl,
	.nr_blic_data_tbl = ARRAY_SIZE(td4160_a14x_00_blic_tbl),
};

#endif /* __TD4160_A14X_00_PANEL_H__ */
