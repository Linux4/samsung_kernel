/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fab/s6e3fab_canvas1_a3_s0_panel_dimming.h
 *
 * Header file for S6E3FAB Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S6E3FAB_CANVAS1_A3_S0_PANEL_DIMMING_H___
#define __S6E3FAB_CANVAS1_A3_S0_PANEL_DIMMING_H___
#include "../dimming.h"
#include "../panel_dimming.h"

/*
 * PANEL INFORMATION
 * LDI : S6E3FAB
 * PANEL : CANVAS1_A3_S0
 */
#define S6E3FAB_NR_STEP (256)
#define S6E3FAB_HBM_STEP (85)
#define S6E3FAB_TOTAL_STEP (S6E3FAB_NR_STEP + S6E3FAB_HBM_STEP)

static unsigned int canvas1_a3_s0_brt_tbl[S6E3FAB_TOTAL_STEP] = {
	BRT(0), BRT(1), BRT(2), BRT(3), BRT(4), BRT(5), BRT(6), BRT(7), BRT(8), BRT(9), BRT(10),
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
	BRT(258), BRT(261), BRT(263), BRT(266), BRT(269), BRT(272), BRT(275), BRT(277), BRT(280), BRT(283),
	BRT(286), BRT(289), BRT(291), BRT(294), BRT(297), BRT(300), BRT(303), BRT(305), BRT(308), BRT(311),
	BRT(314), BRT(317), BRT(319), BRT(322), BRT(325), BRT(328), BRT(331), BRT(333), BRT(336), BRT(339),
	BRT(342), BRT(345), BRT(347), BRT(350), BRT(353), BRT(356), BRT(359), BRT(361), BRT(364), BRT(367),
	BRT(370), BRT(373), BRT(375), BRT(378), BRT(381), BRT(384), BRT(387), BRT(389), BRT(392), BRT(395),
	BRT(398), BRT(400), BRT(403), BRT(405), BRT(408), BRT(411), BRT(413), BRT(416), BRT(418), BRT(421),
	BRT(424), BRT(426), BRT(429), BRT(431), BRT(434), BRT(437), BRT(439), BRT(442), BRT(444), BRT(447),
	BRT(450), BRT(452), BRT(455), BRT(457), BRT(460), BRT(463), BRT(465), BRT(468), BRT(470), BRT(473),
	BRT(476), BRT(478), BRT(481), BRT(483), BRT(486),
};

static unsigned int canvas1_a3_s0_step_cnt_tbl[S6E3FAB_TOTAL_STEP] = {
	[0 ... S6E3FAB_TOTAL_STEP - 1] = 1,
};

struct brightness_table s6e3fab_canvas1_a3_s0_panel_brightness_table = {
	.control_type = BRIGHTNESS_CONTROL_TYPE_GAMMA_MODE2,
	.brt = canvas1_a3_s0_brt_tbl,
	.sz_brt = ARRAY_SIZE(canvas1_a3_s0_brt_tbl),
	.sz_ui_brt = S6E3FAB_NR_STEP,
	.sz_hbm_brt = S6E3FAB_HBM_STEP,
	.lum = NULL,
	.sz_lum = 0,
	.sz_ui_lum = 0,
	.sz_hbm_lum = 0,
	.sz_ext_hbm_lum = 0,
	.brt_to_step = NULL,
	.sz_brt_to_step = 0,
	.step_cnt = canvas1_a3_s0_step_cnt_tbl,
	.sz_step_cnt = ARRAY_SIZE(canvas1_a3_s0_step_cnt_tbl),
	.vtotal = 3232,
};

static struct panel_dimming_info s6e3fab_canvas1_a3_s0_panel_dimming_info = {
	.name = "s6e3fab_canvas1_a3_s0",
	.dim_init_info = {
		NULL,
	},
	.target_luminance = -1,
	.nr_luminance = 0,
	.hbm_target_luminance = -1,
	.nr_hbm_luminance = 0,
	.extend_hbm_target_luminance = -1,
	.nr_extend_hbm_luminance = 0,
	.brt_tbl = &s6e3fab_canvas1_a3_s0_panel_brightness_table,
	/* dimming parameters */
	.dimming_maptbl = NULL,
	.dim_flash_on = false,
	.hbm_aor = NULL,
};
#endif /* __S6E3FAB_CANVAS1_A3_S0_PANEL_DIMMING_H___ */
