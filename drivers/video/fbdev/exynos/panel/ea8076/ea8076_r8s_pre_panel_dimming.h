/*
 * linux/drivers/video/fbdev/exynos/panel/ea8076/ea8076_r8s_pre_panel_dimming.h
 *
 * Header file for EA8076 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EA8076_PRE_PANEL_DIMMING_H___
#define __EA8076_PRE_PANEL_DIMMING_H___
#include "../dimming.h"
#include "../panel_dimming.h"

/*
 * PANEL INFORMATION
 * LDI : EA8076
 * PANEL : PRE
 */
#define EA8076_NR_STEP (256)
#define EA8076_HBM_STEP (110)
#define EA8076_TOTAL_STEP (EA8076_NR_STEP + EA8076_HBM_STEP)

static unsigned int r8s_pre_brt_tbl[EA8076_TOTAL_STEP] = {
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
	BRT(301), BRT(302), BRT(303), BRT(304), BRT(305), BRT(306), BRT(307), BRT(308), BRT(309), BRT(310),
	BRT(311), BRT(312), BRT(313), BRT(314), BRT(315), BRT(316), BRT(317), BRT(318), BRT(319), BRT(320),
	BRT(321), BRT(322), BRT(323), BRT(324), BRT(325), BRT(326), BRT(327), BRT(328), BRT(329), BRT(330),
	BRT(331), BRT(332), BRT(333), BRT(334), BRT(335), BRT(336), BRT(337), BRT(338), BRT(339), BRT(340),
	BRT(341), BRT(342), BRT(343), BRT(344), BRT(345), BRT(346), BRT(347), BRT(348), BRT(349), BRT(350),
	BRT(351), BRT(352), BRT(353), BRT(354), BRT(355), BRT(356), BRT(357), BRT(358), BRT(359), BRT(360),
	BRT(361), BRT(362), BRT(363), BRT(364), BRT(365),
};

static unsigned int r8s_pre_lum_tbl[EA8076_R8S_TOTAL_NR_LUMINANCE] = {
	/* normal 8x32 */
	1, 2, 3, 4, 5, 6, 8, 9,
	10, 11, 12, 14, 15, 16, 17, 18,
	20, 21, 23, 24, 26, 27, 28, 30,
	31, 33, 34, 36, 37, 38, 40, 42,
	43, 44, 46, 47, 49, 50, 52, 53,
	54, 56, 57, 59, 60, 62, 63, 64,
	66, 67, 69, 70, 72, 73, 74, 76,
	78, 79, 80, 82, 83, 85, 86, 88,
	89, 91, 92, 93, 95, 96, 98, 99,
	101, 102, 103, 105, 107, 108, 109, 111,
	112, 114, 115, 117, 118, 119, 121, 122,
	124, 125, 127, 128, 129, 131, 133, 134,
	135, 137, 138, 140, 141, 143, 144, 145,
	147, 148, 150, 151, 153, 154, 155, 157,
	158, 160, 161, 163, 164, 165, 167, 169,
	170, 171, 173, 174, 176, 177, 179, 180,
	183, 185, 187, 188, 191, 192, 194, 196,
	198, 200, 202, 203, 206, 207, 209, 211,
	213, 215, 216, 218, 220, 222, 224, 226,
	228, 230, 231, 233, 235, 237, 239, 241,
	243, 245, 246, 249, 250, 252, 254, 256,
	258, 259, 261, 263, 265, 267, 269, 271,
	273, 274, 276, 278, 280, 282, 284, 286,
	287, 289, 291, 293, 295, 297, 299, 301,
	302, 304, 306, 308, 310, 312, 314, 316,
	317, 319, 321, 323, 325, 327, 329, 330,
	332, 334, 336, 338, 340, 342, 344, 345,
	347, 349, 351, 353, 354, 357, 358, 360,
	362, 364, 366, 368, 370, 372, 373, 375,
	377, 379, 381, 383, 385, 386, 388, 390,
	392, 394, 396, 397, 400, 401, 403, 405,
	407, 409, 411, 412, 415, 416, 418, 420,
	/* hbm 8x13+6 */
	422, 423, 425, 427, 428, 430, 432, 433,
	435, 436, 438, 440, 441, 443, 444, 446,
	448, 449, 451, 452, 454, 456, 457, 459,
	461, 462, 464, 466, 468, 469, 471, 473,
	474, 476, 477, 479, 481, 482, 484, 485,
	487, 489, 490, 492, 493, 495, 497, 498,
	500, 502, 503, 505, 507, 508, 510, 512,
	513, 515, 517, 518, 520, 522, 523, 525,
	526, 528, 530, 531, 533, 534, 536, 538,
	539, 541, 543, 544, 546, 547, 549, 551,
	552, 554, 556, 558, 559, 561, 563, 564,
	566, 567, 569, 571, 572, 574, 575, 577,
	579, 580, 582, 584, 585, 587, 588, 590,
	592, 593, 595, 597, 598, 600,
};

static unsigned int r8s_pre_step_cnt_tbl[EA8076_TOTAL_STEP] = {
	[0 ... EA8076_TOTAL_STEP - 1] = 1,
};

struct brightness_table ea8076_r8s_pre_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = r8s_pre_brt_tbl,
	.sz_brt = ARRAY_SIZE(r8s_pre_brt_tbl),
	.sz_ui_brt = EA8076_NR_STEP,
	.sz_hbm_brt = EA8076_HBM_STEP,
	.lum = r8s_pre_lum_tbl,
	.sz_lum = EA8076_R8S_TOTAL_NR_LUMINANCE,
	.sz_ui_lum = EA8076_R8S_NR_LUMINANCE,
	.sz_hbm_lum = EA8076_R8S_NR_HBM_LUMINANCE,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = r8s_pre_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(r8s_pre_step_cnt_tbl),
	.vtotal = 0,
};

static struct panel_dimming_info ea8076_r8s_pre_panel_dimming_info = {
	.name = "ea8076_r8s_pre",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = 0,
	.brt_tbl = &ea8076_r8s_pre_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,
	.hbm_aor = NULL,
};
#endif /* __EA8076_PRE_PANEL_DIMMING_H___ */
