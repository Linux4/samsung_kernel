/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * SFR access functions for Samsung EXYNOS SoC MIPI-DSI Master driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "../dsim.h"
#include "regs-decon.h"

/* dsim version */
#define DSIM_VER_EVT0			0x02050000

/* These definitions are need to guide from AP team */
#define DSIM_STOP_STATE_CNT		0xA
#define DSIM_BTA_TIMEOUT		0xff
#define DSIM_LP_RX_TIMEOUT		0xffff
#define DSIM_MULTI_PACKET_CNT		0xffff
#define DSIM_PLL_STABLE_TIME		0x682A
#define DSIM_PH_FIFOCTRL_THRESHOLD	32 /* 1 ~ 32 */

#define PLL_SLEEP_CNT_MULT		450
#define PLL_SLEEP_CNT_MARGIN_RATIO	0	/* 10% ~ 20% */
#define PLL_SLEEP_CNT_MARGIN	(PLL_SLEEP_CNT_MULT * PLL_SLEEP_CNT_MARGIN_RATIO / 100)

/* If below values depend on panel. These values wil be move to panel file.
 * And these values are valid in case of video mode only.
 */
#define DSIM_CMD_ALLOW_VALUE		4
#define DSIM_STABLE_VFP_VALUE		2
#define TE_MARGIN					5 /* 5% */

#define PLL_LOCK_CNT_MULT		500
#define PLL_LOCK_CNT_MARGIN_RATIO	0	/* 10% ~ 20% */
#define PLL_LOCK_CNT_MARGIN	(PLL_LOCK_CNT_MULT * PLL_LOCK_CNT_MARGIN_RATIO / 100)

u32 DSIM_PHY_PLL_CTRL_VAL[] = {
	0x00000000,
	0x00000000,
	0x000000A1,
	0x00000027,
	0x00000062,
	0x00000009,
	0x0000005A,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
};

u32 DSIM_PHY_BIAS_VAL[] = {
	0x00000010,
	0x00000000,
	0x00000005,
	0x00000000,
	0x00000068,
	0x0000004C,
};

u32 DSIM_PHY_DTB_VAL[] = {
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
};

u32 DSIM_PHY_ACTRL_MC_VAL[] = {
	0x00000088,
	0x00000000,
	0x00000020,
	0x00000000,
};

u32 DSIM_PHY_ACTRL_MD0_VAL[] = {
	0x00000088,
	0x00000000,
	0x00000020,
	0x00000000,
	0x000000C0,
	0x00000000,
};

u32 DSIM_PHY_ACTRL_MD1_VAL[] = {
	0x00000088,
	0x00000000,
	0x00000020,
};

u32 DSIM_PHY_ACTRL_MD2_VAL[] = {
	0x00000088,
	0x00000000,
	0x00000020,
};

u32 DSIM_PHY_ACTRL_MD3_VAL[] = {
	0x00000088,
	0x00000000,
	0x00000020,
};

/* DPHY timing table */
const u32 dphy_timing[][10] = {
	/* bps, clk_prepare, clk_zero, clk_post, clk_trail,
	 * hs_prepare, hs_zero, hs_trail, lpx, hs_exit
	 */
	{2500, 12, 42, 15, 10, 11, 19, 9, 9, 16},
	{2490, 12, 42, 15, 10, 11, 19, 9, 9, 16},
	{2480, 12, 41, 15, 10, 11, 19, 9, 9, 16},
	{2470, 12, 41, 15, 10, 11, 19, 9, 9, 15},
	{2460, 12, 41, 15, 10, 11, 19, 9, 9, 15},
	{2450, 12, 41, 15, 10, 11, 19, 9, 9, 15},
	{2440, 12, 41, 15, 10, 11, 18, 9, 9, 15},
	{2430, 11, 41, 15, 10, 11, 18, 9, 9, 15},
	{2420, 11, 41, 15, 10, 11, 18, 9, 9, 15},
	{2500, 12, 42, 15, 10, 11, 19, 9, 9, 16},
	{2490, 12, 42, 15, 10, 11, 19, 9, 9, 16},
	{2480, 12, 41, 15, 10, 11, 19, 9, 9, 16},
	{2470, 12, 41, 15, 10, 11, 19, 9, 9, 15},
	{2460, 12, 41, 15, 10, 11, 19, 9, 9, 15},
	{2450, 12, 41, 15, 10, 11, 19, 9, 9, 15},
	{2440, 12, 41, 15, 10, 11, 18, 9, 9, 15},
	{2430, 11, 41, 15, 10, 11, 18, 9, 9, 15},
	{2420, 11, 41, 15, 10, 11, 18, 9, 9, 15},
	{2410, 11, 41, 15, 10, 11, 18, 9, 9, 15},
	{2400, 11, 40, 15, 10, 11, 18, 9, 8, 15},
	{2390, 11, 40, 15, 10, 11, 18, 9, 8, 15},
	{2380, 11, 40, 15, 10, 11, 18, 9, 8, 15},
	{2370, 11, 40, 15, 10, 11, 17, 9, 8, 15},
	{2360, 11, 40, 15, 10, 11, 17, 9, 8, 15},
	{2350, 11, 39, 15, 9, 11, 17, 9, 8, 15},
	{2340, 11, 39, 15, 9, 11, 17, 9, 8, 15},
	{2330, 11, 39, 15, 9, 11, 17, 9, 8, 15},
	{2320, 11, 39, 15, 9, 11, 17, 8, 8, 14},
	{2310, 11, 38, 15, 9, 11, 17, 8, 8, 14},
	{2300, 11, 38, 15, 9, 11, 17, 8, 8, 14},
	{2290, 11, 38, 15, 9, 10, 17, 8, 8, 14},
	{2280, 11, 38, 15, 9, 10, 17, 8, 8, 14},
	{2270, 11, 38, 15, 9, 10, 17, 8, 8, 14},
	{2260, 11, 38, 15, 9, 10, 17, 8, 8, 14},
	{2250, 11, 38, 15, 9, 10, 17, 8, 8, 14},
	{2240, 10, 38, 15, 9, 10, 17, 8, 8, 14},
	{2230, 10, 37, 15, 9, 10, 17, 8, 8, 14},
	{2220, 10, 37, 15, 9, 10, 16, 8, 8, 14},
	{2210, 10, 37, 15, 9, 10, 16, 8, 8, 14},
	{2200, 10, 37, 15, 9, 10, 16, 8, 8, 14},
	{2190, 10, 37, 15, 9, 10, 16, 8, 8, 14},
	{2180, 10, 36, 15, 9, 10, 16, 8, 8, 13},
	{2170, 10, 36, 15, 9, 10, 16, 8, 8, 13},
	{2160, 10, 36, 15, 9, 10, 16, 8, 8, 13},
	{2150, 10, 36, 15, 9, 10, 15, 8, 8, 13},
	{2140, 10, 35, 15, 9, 10, 15, 8, 8, 13},
	{2130, 10, 35, 15, 9, 10, 15, 8, 7, 13},
	{2120, 10, 35, 15, 8, 10, 15, 8, 7, 13},
	{2110, 10, 35, 15, 8, 10, 15, 8, 7, 13},
	{2100, 10, 34, 15, 8, 10, 15, 8, 7, 13},
	{2090, 10, 34, 15, 8, 10, 15, 7, 7, 13},
	{2080, 10, 34, 15, 8, 9, 15, 7, 7, 13},
	{2070, 10, 34, 15, 8, 9, 15, 7, 7, 13},
	{2060, 10, 34, 15, 8, 9, 15, 7, 7, 13},
	{2050, 10, 34, 15, 8, 9, 15, 7, 7, 13},
	{2040, 9, 34, 15, 8, 9, 15, 7, 7, 13},
	{2030, 9, 34, 15, 8, 9, 15, 7, 7, 12},
	{2020, 9, 34, 15, 8, 9, 15, 7, 7, 12},
	{2010, 9, 33, 15, 8, 9, 15, 7, 7, 12},
	{2000, 9, 33, 15, 8, 9, 14, 7, 7, 12},
	{1990, 9, 33, 15, 8, 9, 14, 7, 7, 12},
	{1980, 9, 33, 15, 8, 9, 14, 7, 7, 12},
	{1970, 9, 32, 15, 8, 9, 14, 7, 7, 12},
	{1960, 9, 32, 15, 8, 9, 14, 7, 7, 12},
	{1950, 9, 32, 15, 8, 9, 14, 7, 7, 12},
	{1940, 9, 32, 15, 8, 9, 14, 7, 7, 12},
	{1930, 9, 31, 15, 8, 9, 14, 7, 7, 12},
	{1920, 9, 31, 15, 8, 9, 14, 7, 7, 12},
	{1910, 9, 31, 15, 8, 9, 14, 7, 7, 12},
	{1900, 9, 31, 15, 7, 9, 14, 7, 7, 12},
	{1890, 9, 31, 15, 7, 9, 14, 7, 7, 11},
	{1880, 9, 31, 15, 7, 8, 14, 7, 7, 11},
	{1870, 9, 31, 15, 7, 8, 14, 6, 7, 11},
	{1860, 9, 31, 15, 7, 8, 13, 6, 6, 11},
	{1850, 8, 31, 15, 7, 8, 13, 6, 6, 11},
	{1840, 8, 30, 15, 7, 8, 13, 6, 6, 11},
	{1830, 8, 30, 15, 7, 8, 13, 6, 6, 11},
	{1820, 8, 30, 15, 7, 8, 13, 6, 6, 11},
	{1810, 8, 30, 15, 7, 8, 13, 6, 6, 11},
	{1800, 8, 29, 15, 7, 8, 13, 6, 6, 11},
	{1790, 8, 29, 15, 7, 8, 13, 6, 6, 11},
	{1780, 8, 29, 15, 7, 8, 12, 6, 6, 11},
	{1770, 8, 29, 15, 7, 8, 12, 6, 6, 11},
	{1760, 8, 29, 15, 7, 8, 12, 6, 6, 11},
	{1750, 8, 28, 15, 7, 8, 12, 6, 6, 11},
	{1740, 8, 28, 15, 7, 8, 12, 6, 6, 10},
	{1730, 8, 28, 15, 7, 8, 12, 6, 6, 10},
	{1720, 8, 28, 15, 7, 8, 12, 6, 6, 10},
	{1710, 8, 27, 15, 7, 8, 12, 6, 6, 10},
	{1700, 8, 27, 15, 7, 8, 12, 6, 6, 10},
	{1690, 8, 27, 15, 7, 8, 12, 6, 6, 10},
	{1680, 8, 27, 15, 7, 8, 12, 6, 6, 10},
	{1670, 8, 27, 15, 6, 7, 12, 6, 6, 10},
	{1660, 8, 27, 15, 6, 7, 12, 6, 6, 10},
	{1650, 7, 27, 15, 6, 7, 12, 6, 6, 10},
	{1640, 7, 27, 15, 6, 7, 11, 5, 6, 10},
	{1630, 7, 26, 15, 6, 7, 11, 5, 6, 10},
	{1620, 7, 26, 15, 6, 7, 11, 5, 6, 10},
	{1610, 7, 26, 15, 6, 7, 11, 5, 6, 10},
	{1600, 7, 26, 15, 6, 7, 11, 5, 5, 9},
	{1590, 7, 26, 15, 6, 7, 11, 5, 5, 9},
	{1580, 7, 25, 15, 6, 7, 11, 5, 5, 9},
	{1570, 7, 25, 15, 6, 7, 11, 5, 5, 9},
	{1560, 7, 25, 15, 6, 7, 10, 5, 5, 9},
	{1550, 7, 25, 15, 6, 7, 10, 5, 5, 9},
	{1540, 7, 24, 15, 6, 7, 10, 5, 5, 9},
	{1530, 7, 24, 15, 6, 7, 10, 5, 5, 9},
	{1520, 7, 24, 15, 6, 7, 10, 5, 5, 9},
	{1510, 7, 24, 15, 6, 7, 10, 5, 5, 9},
	{1500, 7, 24, 15, 6, 7, 10, 5, 5, 9},
	{1490, 60, 24, 15, 70, 60, 10, 63, 44, 9},
	{1480, 60, 23, 15, 70, 59, 10, 63, 44, 9},
	{1470, 59, 23, 15, 69, 59, 10, 62, 44, 9},
	{1460, 59, 23, 15, 69, 58, 10, 62, 43, 9},
	{1450, 58, 23, 15, 69, 58, 10, 61, 43, 8},
	{1440, 58, 23, 15, 68, 58, 9, 61, 43, 8},
	{1430, 58, 22, 15, 68, 57, 9, 61, 42, 8},
	{1420, 57, 22, 15, 67, 57, 9, 60, 42, 8},
	{1410, 57, 22, 15, 67, 56, 9, 60, 42, 8},
	{1400, 56, 22, 15, 67, 56, 9, 60, 41, 8},
	{1390, 56, 22, 15, 66, 56, 9, 59, 41, 8},
	{1380, 56, 22, 15, 66, 55, 9, 59, 41, 8},
	{1370, 55, 21, 15, 66, 55, 9, 59, 41, 8},
	{1360, 55, 21, 15, 65, 55, 9, 58, 40, 8},
	{1350, 54, 21, 15, 65, 54, 9, 58, 40, 8},
	{1340, 54, 21, 15, 65, 54, 9, 58, 40, 8},
	{1330, 54, 21, 15, 64, 53, 8, 57, 39, 8},
	{1320, 53, 21, 15, 64, 53, 8, 57, 39, 8},
	{1310, 53, 20, 15, 64, 53, 8, 57, 39, 8},
	{1300, 52, 20, 15, 63, 52, 8, 56, 38, 7},
	{1290, 52, 20, 15, 63, 52, 8, 56, 38, 7},
	{1280, 51, 20, 15, 62, 51, 8, 55, 38, 7},
	{1270, 51, 20, 15, 62, 51, 8, 55, 38, 7},
	{1260, 51, 19, 15, 62, 51, 8, 55, 37, 7},
	{1250, 50, 19, 15, 61, 50, 8, 54, 37, 7},
	{1240, 50, 19, 15, 61, 50, 8, 54, 37, 7},
	{1230, 49, 19, 15, 61, 49, 8, 54, 36, 7},
	{1220, 49, 19, 15, 60, 49, 7, 53, 36, 7},
	{1210, 49, 18, 15, 60, 49, 7, 53, 36, 7},
	{1200, 48, 18, 15, 60, 48, 7, 53, 35, 7},
	{1190, 48, 18, 15, 59, 48, 7, 52, 35, 7},
	{1180, 47, 18, 15, 59, 48, 7, 52, 35, 7},
	{1170, 47, 18, 15, 59, 47, 7, 52, 35, 7},
	{1160, 47, 18, 15, 58, 47, 7, 51, 34, 6},
	{1150, 46, 17, 15, 58, 46, 7, 51, 34, 6},
	{1140, 46, 17, 15, 57, 46, 7, 50, 34, 6},
	{1130, 45, 17, 15, 57, 46, 7, 50, 33, 6},
	{1120, 45, 17, 15, 57, 45, 7, 50, 33, 6},
	{1110, 45, 17, 15, 56, 45, 6, 49, 33, 6},
	{1100, 44, 17, 15, 56, 44, 6, 49, 32, 6},
	{1090, 44, 16, 15, 56, 44, 6, 49, 32, 6},
	{1080, 43, 16, 15, 55, 44, 6, 48, 32, 6},
	{1070, 43, 16, 15, 55, 43, 6, 48, 32, 6},
	{1060, 42, 16, 15, 55, 43, 6, 48, 31, 6},
	{1050, 42, 16, 15, 54, 42, 6, 47, 31, 6},
	{1040, 42, 15, 15, 54, 42, 6, 47, 31, 6},
	{1030, 41, 15, 15, 54, 34, 6, 47, 25, 6},
	{1020, 41, 15, 15, 53, 41, 6, 46, 30, 6},
	{1010, 40, 15, 15, 53, 41, 6, 46, 30, 5},
	{1000, 40, 15, 15, 52, 40, 6, 45, 29, 5},
	{990, 40, 14, 15, 52, 40, 5, 45, 29, 5},
	{980, 39, 14, 15, 52, 40, 5, 45, 29, 5},
	{970, 39, 14, 15, 51, 39, 5, 44, 29, 5},
	{960, 38, 14, 15, 51, 39, 5, 44, 28, 5},
	{950, 38, 14, 15, 51, 39, 5, 44, 28, 5},
	{940, 38, 14, 15, 50, 38, 5, 43, 28, 5},
	{930, 37, 13, 15, 50, 38, 5, 43, 27, 5},
	{920, 37, 13, 15, 50, 37, 5, 43, 27, 5},
	{910, 36, 13, 15, 49, 37, 5, 42, 27, 5},
	{900, 36, 13, 15, 49, 37, 5, 42, 26, 5},
	{890, 35, 13, 15, 49, 36, 5, 42, 26, 5},
	{880, 35, 13, 15, 48, 36, 5, 41, 26, 5},
	{870, 35, 12, 15, 48, 35, 4, 41, 26, 4},
	{860, 34, 12, 15, 48, 35, 4, 41, 25, 4},
	{850, 34, 12, 15, 47, 35, 4, 40, 25, 4},
	{840, 33, 12, 15, 47, 34, 4, 40, 25, 4},
	{830, 33, 12, 15, 46, 34, 4, 39, 24, 4},
	{820, 33, 11, 15, 46, 33, 4, 39, 24, 4},
	{810, 32, 11, 15, 46, 33, 4, 39, 24, 4},
	{800, 32, 11, 15, 45, 33, 4, 38, 23, 4},
	{790, 31, 11, 15, 45, 32, 4, 38, 23, 4},
	{780, 31, 11, 15, 45, 32, 4, 38, 23, 4},
	{770, 31, 10, 15, 44, 32, 3, 37, 23, 4},
	{760, 30, 10, 15, 44, 31, 3, 37, 22, 4},
	{750, 30, 10, 15, 44, 31, 3, 37, 22, 4},
	{740, 29, 10, 15, 43, 30, 3, 36, 22, 4},
	{730, 29, 10, 15, 43, 30, 3, 36, 21, 4},
	{720, 29, 10, 15, 43, 30, 3, 36, 21, 3},
	{710, 28, 9, 15, 42, 29, 3, 35, 21, 3},
	{700, 28, 9, 15, 42, 29, 3, 35, 20, 3},
	{690, 27, 9, 15, 41, 28, 3, 34, 20, 3},
	{680, 27, 9, 15, 41, 28, 3, 34, 20, 3},
	{670, 26, 9, 15, 41, 28, 3, 34, 20, 3},
	{660, 26, 9, 15, 40, 27, 3, 33, 19, 3},
	{650, 26, 8, 15, 40, 27, 2, 33, 19, 3},
	{640, 25, 8, 15, 40, 26, 2, 33, 19, 3},
	{630, 25, 8, 15, 39, 26, 2, 32, 18, 3},
	{620, 24, 8, 15, 39, 26, 2, 32, 18, 3},
	{610, 24, 8, 15, 39, 25, 2, 32, 18, 3},
	{600, 24, 7, 15, 38, 25, 2, 31, 17, 3},
	{590, 23, 7, 15, 38, 25, 2, 31, 17, 3},
	{580, 23, 7, 15, 38, 24, 2, 31, 17, 2},
	{570, 22, 7, 15, 37, 24, 2, 30, 17, 2},
	{560, 22, 7, 15, 37, 23, 2, 30, 16, 2},
	{550, 22, 6, 15, 37, 23, 2, 30, 16, 2},
	{540, 21, 6, 15, 36, 23, 1, 29, 16, 2},
	{530, 21, 6, 15, 36, 22, 1, 29, 15, 2},
	{520, 20, 6, 15, 35, 22, 1, 28, 15, 2},
	{510, 20, 6, 15, 35, 21, 1, 28, 15, 2},
	{500, 19, 6, 15, 35, 21, 1, 28, 14, 2},
};

const u32 b_dphyctl[14] = {
	0x0af, 0x0c8, 0x0e1, 0x0fa,			/* esc 7 ~ 10 */
	0x113, 0x12c, 0x145, 0x15e, 0x177,	/* esc 11 ~ 15 */
	0x190, 0x1a9, 0x1c2, 0x1db, 0x1f4	/* esc 16 ~ 20 */
};

/***************************** DPHY CAL functions *******************************/
#if defined(CONFIG_EXYNOS_DSIM_DITHER)
static void dsim_reg_set_dphy_dither_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_phy_write_mask(id, DSIM_PHY_PLL_CTRL_0A, val, DSIM_PHY_DITHER_EN);
}
#endif

/* DPHY setting */
static void dsim_reg_set_pll_freq(u32 id, u32 p, u32 m, u32 s, u32 k)
{
	u32 val, mask;

	/* K value */
	val = DSIM_PHY_PMS_K_15_8(k);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_04, val);

	/* K value */
	val = DSIM_PHY_PMS_K_7_0(k);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_05, val);

	/* P & M value */
	val = DSIM_PHY_PMS_P_5_0(p) | DSIM_PHY_PMS_M_9_8(m);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_06, val);

	/* M value */
	val = DSIM_PHY_PMS_M_7_0(m);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_07, val);

	/* S value */
	val = DSIM_PHY_PMS_S_2_0(s);
	mask = DSIM_PHY_PMS_S_2_0_MASK;
	dsim_phy_write_mask(id, DSIM_PHY_PLL_CTRL_08, val, mask);

	/* set other pll_ctrl registers */
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_01, DSIM_PHY_PLL_CTRL_VAL[0]);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_02, DSIM_PHY_PLL_CTRL_VAL[1]);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_03, DSIM_PHY_PLL_CTRL_VAL[2]);
#if !defined(CONFIG_EXYNOS_DSIM_DITHER)
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_09, DSIM_PHY_PLL_CTRL_VAL[8]);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_0A, DSIM_PHY_PLL_CTRL_VAL[9]);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_0B, DSIM_PHY_PLL_CTRL_VAL[10]);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_0C, DSIM_PHY_PLL_CTRL_VAL[11]);
#endif
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_0D, DSIM_PHY_PLL_CTRL_VAL[12]);
}

static void dsim_reg_set_dphy_timing_values(u32 id,
		struct dphy_timing_value *t, u32 hsmode)
{
	u32 val, mask;
	u32 hs_en;
	/* u32  skewcal_en; */

	/* HS mode setting */
	if (hsmode) {
		/* skewcal_en = DSIM_PHY_SKEWCAL_EN; */
		hs_en = DSIM_PHY_HS_MODE_SEL;
	} else {
		/* skewcal_en = 0; */
		hs_en = 0;
	}

	/* clock lane setting */
	val = DSIM_PHY_ULPS_EXIT_CNT_7_0(t->b_dphyctl);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MC_04, val);

	val = DSIM_PHY_ULPS_EXIT_CNT_9_8(t->b_dphyctl) | hs_en;
	mask = DSIM_PHY_ULPS_EXIT_CNT_9_8_MASK | DSIM_PHY_HS_MODE_SEL;
	dsim_phy_write_mask(id, DSIM_PHY_DCTRL_MC_05, val, mask);

	/* skew cal implementation : disable */
	val = 0;
	mask = DSIM_PHY_SKEWCAL_EN;
	dsim_phy_write_mask(id, DSIM_PHY_DCTRL_MC_06, val, mask);

	val = DSIM_PHY_TLPXCTRL(t->lpx);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MC_08, val);

	val = DSIM_PHY_THSEXITCTL(t->hs_exit);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MC_09, val);

	val = DSIM_PHY_TCLKPRPRCTL(t->clk_prepare);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MC_0B, val);

	val = DSIM_PHY_TCLKZEROCTL(t->clk_zero);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MC_0C, val);

	val = DSIM_PHY_TCLKPOSTCTL(t->clk_post);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MC_0D, val);

	val = DSIM_PHY_TCLKTRAILCTL(t->clk_trail);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MC_0E, val);
	/* set other clock lane setting */
	/* DCTRL_MC_X */

	/* data lane setting */
	/* D0 */
	val = DSIM_PHY_ULPS_EXIT_CNT_7_0(t->b_dphyctl);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD0_04, val);

	val = DSIM_PHY_ULPS_EXIT_CNT_9_8(t->b_dphyctl) | hs_en;
	mask = DSIM_PHY_ULPS_EXIT_CNT_9_8_MASK | DSIM_PHY_HS_MODE_SEL;
	dsim_phy_write_mask(id, DSIM_PHY_DCTRL_MD0_05, val, mask);

	/* skew cal implementation later */

	val = DSIM_PHY_THSPRPRCTL(t->hs_prepare);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD0_07, val);

	val = DSIM_PHY_TLPXCTRL(t->lpx);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD0_08, val);

	val = DSIM_PHY_THSEXITCTL(t->hs_exit);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD0_09, val);

	val = DSIM_PHY_THSZEROCTL(t->hs_zero);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD0_0B, val);

	val = DSIM_PHY_TTRAILCTL(t->hs_trail);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD0_0C, val);
	/* set other data lane setting */
	/* DCTRL_MC_X */

	/* D1 */
	val = DSIM_PHY_ULPS_EXIT_CNT_7_0(t->b_dphyctl);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD1_04, val);

	val = DSIM_PHY_ULPS_EXIT_CNT_9_8(t->b_dphyctl) | hs_en;
	mask = DSIM_PHY_ULPS_EXIT_CNT_9_8_MASK | DSIM_PHY_HS_MODE_SEL;
	dsim_phy_write_mask(id, DSIM_PHY_DCTRL_MD1_05, val, mask);


	/* skew cal implementation later */

	val = DSIM_PHY_THSPRPRCTL(t->hs_prepare);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD1_07, val);

	val = DSIM_PHY_TLPXCTRL(t->lpx);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD1_08, val);

	val = DSIM_PHY_THSEXITCTL(t->hs_exit);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD1_09, val);

	val = DSIM_PHY_THSZEROCTL(t->hs_zero);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD1_0B, val);

	val = DSIM_PHY_TTRAILCTL(t->hs_trail);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD1_0C, val);
	/* set other data lane setting */
	/* DCTRL_MC_X */

	/* D2 */
	val = DSIM_PHY_ULPS_EXIT_CNT_7_0(t->b_dphyctl);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD2_04, val);

	val = DSIM_PHY_ULPS_EXIT_CNT_9_8(t->b_dphyctl) | hs_en;
	mask = DSIM_PHY_ULPS_EXIT_CNT_9_8_MASK | DSIM_PHY_HS_MODE_SEL;
	dsim_phy_write_mask(id, DSIM_PHY_DCTRL_MD2_05, val, mask);

	/* skew cal implementation later */

	val = DSIM_PHY_THSPRPRCTL(t->hs_prepare);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD2_07, val);

	val = DSIM_PHY_TLPXCTRL(t->lpx);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD2_08, val);

	val = DSIM_PHY_THSEXITCTL(t->hs_exit);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD2_09, val);

	val = DSIM_PHY_THSZEROCTL(t->hs_zero);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD2_0B, val);

	val = DSIM_PHY_TTRAILCTL(t->hs_trail);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD2_0C, val);
	/* set other data lane setting */
	/* DCTRL_MC_X */

	/* D3 */
	val = DSIM_PHY_ULPS_EXIT_CNT_7_0(t->b_dphyctl);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD3_04, val);

	val = DSIM_PHY_ULPS_EXIT_CNT_9_8(t->b_dphyctl) | hs_en;
	mask = DSIM_PHY_ULPS_EXIT_CNT_9_8_MASK | DSIM_PHY_HS_MODE_SEL;
	dsim_phy_write_mask(id, DSIM_PHY_DCTRL_MD3_05, val, mask);

	/* skew cal implementation later */

	val = DSIM_PHY_THSPRPRCTL(t->hs_prepare);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD3_07, val);

	val = DSIM_PHY_TLPXCTRL(t->lpx);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD3_08, val);

	val = DSIM_PHY_THSEXITCTL(t->hs_exit);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD3_09, val);

	val = DSIM_PHY_THSZEROCTL(t->hs_zero);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD3_0B, val);

	val = DSIM_PHY_TTRAILCTL(t->hs_trail);
	dsim_phy_write(id, DSIM_PHY_DCTRL_MD3_0C, val);
	/* set other data lane setting */
	/* DCTRL_MC_X */
}

#if defined(CONFIG_EXYNOS_DSIM_DITHER)
static void dsim_reg_set_dphy_param_dither(u32 id, struct stdphy_pms *dphy_pms)
{
	u32 val, mask;

	/* MFR */
	val = DSIM_PHY_DITHER_MFR(dphy_pms->mfr);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_0B, val);

	/* MRR & SEL_PF */
	val = DSIM_PHY_DITHER_MRR(dphy_pms->mrr) | DSIM_PHY_DITHER_SEL_PF(dphy_pms->sel_pf);
	dsim_phy_write(id, DSIM_PHY_PLL_CTRL_0C, val);

	/* ICP & FSEL & RSEL */
	val = DSIM_PHY_DITHER_ICP(dphy_pms->icp) |
		DSIM_PHY_DITHER_FSEL(dphy_pms->fsel) |
		DSIM_PHY_DITHER_RSEL(dphy_pms->rsel);
	mask = DSIM_PHY_DITHER_ICP_MASK |
		DSIM_PHY_DITHER_FSEL_MASK |
		DSIM_PHY_DITHER_RSEL_MASK;
	dsim_phy_write_mask(id, DSIM_PHY_PLL_CTRL_09, val, mask);

	/* EXTAFC */
	val = DSIM_PHY_DITHER_EXTAFC(dphy_pms->extafc);
	mask = DSIM_PHY_DITHER_EXTAFC_MASK;
	dsim_phy_write_mask(id, DSIM_PHY_PLL_CTRL_08, val, mask);

	/* AFC_ENB & FEED_EN & FOUT_MASK */
	val = DSIM_PHY_DITHER_AFC_ENB(dphy_pms->afc_enb) |
		DSIM_PHY_DITHER_FEED_EN(dphy_pms->feed_en) |
		DSIM_PHY_DITHER_FOUT(dphy_pms->fout_mask);
	mask = DSIM_PHY_DITHER_AFC_ENB_MASK |
		DSIM_PHY_DITHER_FEED_EN_MASK |
		DSIM_PHY_DITHER_FOUT_MASK;
	dsim_phy_write_mask(id, DSIM_PHY_PLL_CTRL_0A, val, mask);
}
#endif

/* BIAS Block Control Register */
static void dsim_reg_set_bias_ctrl(u32 id, u32 *blk_ctl)
{
	u32 i;

	for (i = 0 ; i < 6 ; i++)
		dsim_phy_write(id, DSIM_PHY_BIAS_CTRL(i+1), blk_ctl[i]);
}

/* DTB Block Control Register */
static void dsim_reg_set_dtb_ctrl(u32 id, u32 *blk_ctl)
{
	u32 i;

	for (i = 0 ; i < 6 ; i++)
		dsim_phy_write(id, DSIM_PHY_DTB_CTRL(i+1), blk_ctl[i]);

}

/* Master Clock Lane D-PHY Analog Control Register */
static void dsim_reg_set_actrl_mc(u32 id, u32 *blk_ctl)
{
	u32 i;

	for (i = 0 ; i < 4 ; i++)
		dsim_phy_write(id, DSIM_PHY_ACTRL_MC(i+1), blk_ctl[i]);

}

/* Master Data0 Lane D-PHY Analog Control Register */
static void dsim_reg_set_actrl_md0(u32 id, u32 *blk_ctl)
{
	u32 i;

	for (i = 0 ; i < 6 ; i++)
		dsim_phy_write(id, DSIM_PHY_ACTRL_MD0(i+1), blk_ctl[i]);

}

/* Master Data1 Lane D-PHY Analog Control Register */
static void dsim_reg_set_actrl_md1(u32 id, u32 *blk_ctl)
{
	u32 i;

	for (i = 0 ; i < 3 ; i++)
		dsim_phy_write(id, DSIM_PHY_ACTRL_MD1(i+1), blk_ctl[i]);

}

/* Master Data2 Lane D-PHY Analog Control Register */
static void dsim_reg_set_actrl_md2(u32 id, u32 *blk_ctl)
{
	u32 i;

	for (i = 0 ; i < 3 ; i++)
		dsim_phy_write(id, DSIM_PHY_ACTRL_MD2(i+1), blk_ctl[i]);

}

/* Master Data3 Lane D-PHY Analog Control Register */
static void dsim_reg_set_actrl_md3(u32 id, u32 *blk_ctl)
{
	u32 i;

	for (i = 0 ; i < 3 ; i++)
		dsim_phy_write(id, DSIM_PHY_ACTRL_MD3(i+1), blk_ctl[i]);

}

/******************* DSIM CAL functions *************************/
static void dsim_reg_sw_reset(u32 id)
{
	u32 cnt = 1000;
	u32 state;

	dsim_write_mask(id, DSIM_SWRST, ~0, DSIM_SWRST_RESET);

	do {
		state = dsim_read(id, DSIM_SWRST) & DSIM_SWRST_RESET;
		cnt--;
		udelay(10);
	} while (state && cnt);

	if (!cnt)
		dsim_err("%s is timeout.\n", __func__);
}

#if 0
/* this function may be used for later use */
static void dsim_reg_dphy_resetn(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_SWRST, val, DSIM_DPHY_RST); /* reset high */
}
#endif

static void dsim_reg_set_config_options(u32 id, u32 lane,
		u32 eotp_en, u32 per_frame_read, u32 pix_format, u32 vc_id)
{
	u32 val, mask;

	val = DSIM_CONFIG_NUM_OF_DATA_LANE(lane) | DSIM_CONFIG_EOTP_EN(eotp_en)
		| DSIM_CONFIG_PER_FRAME_READ_EN(per_frame_read)
		| DSIM_CONFIG_PIXEL_FORMAT(pix_format)
		| DSIM_CONFIG_VC_ID(vc_id);

	mask = DSIM_CONFIG_NUM_OF_DATA_LANE_MASK | DSIM_CONFIG_EOTP_EN_MASK
		| DSIM_CONFIG_PER_FRAME_READ_EN_MASK
		| DSIM_CONFIG_PIXEL_FORMAT_MASK
		| DSIM_CONFIG_VC_ID_MASK;

	dsim_write_mask(id, DSIM_CONFIG, val, mask);
}

static void dsim_reg_enable_lane(u32 id, u32 lane, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_LANES_EN(lane));
}

static void dsim_reg_pll_stable_time(u32 id, u32 lock_cnt)
{
	dsim_write(id, DSIM_PLLTMR, lock_cnt);
}

static void dsim_reg_set_pll(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_PLLCTRL, val, DSIM_PLLCTRL_PLL_EN);
}

static u32 dsim_reg_is_pll_stable(u32 id)
{
	u32 val;

    val = dsim_read(id, DSIM_LINK_STATUS3);
	    if (val & DSIM_LINK_STATUS3_PLL_STABLE)
			        return 1;

	return 0;
}

static int dsim_reg_enable_pll(u32 id, u32 en)
{

	u32 cnt = 1000;

	if (en) {
		dsim_reg_clear_int(id, DSIM_INTSRC_PLL_STABLE);

		dsim_reg_set_pll(id, 1);
		while (1) {
			cnt--;
			if (dsim_reg_is_pll_stable(id))
				return 0;
			if (cnt == 0)
				return -EBUSY;
			udelay(10);
		}
	} else {
		dsim_reg_set_pll(id, 0);
		while (1) {
			cnt--;
			if (!dsim_reg_is_pll_stable(id))
				return 0;
			if (cnt == 0)
				return -EBUSY;
			udelay(10);
		}
	}

	return 0;
}

static void dsim_reg_set_esc_clk_en(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val, DSIM_CLK_CTRL_ESCCLK_EN);
}

static void dsim_reg_set_esc_clk_prescaler(u32 id, u32 en, u32 p)
{
	u32 val = en ? DSIM_CLK_CTRL_ESCCLK_EN : 0;
	u32 mask = DSIM_CLK_CTRL_ESCCLK_EN | DSIM_CLK_CTRL_ESC_PRESCALER_MASK;

	val |= DSIM_CLK_CTRL_ESC_PRESCALER(p);
	dsim_write_mask(id, DSIM_CLK_CTRL, val, mask);
}

static void dsim_reg_set_esc_clk_on_lane(u32 id, u32 en, u32 lane)
{
	u32 val;

	lane = (lane >> 1) | (1 << 4);

	val = en ? DSIM_CLK_CTRL_LANE_ESCCLK_EN(lane) : 0;
	dsim_write_mask(id, DSIM_CLK_CTRL, val,
				DSIM_CLK_CTRL_LANE_ESCCLK_EN_MASK);
}

static void dsim_reg_set_stop_state_cnt(u32 id)
{
	u32 val = DSIM_ESCMODE_STOP_STATE_CNT(DSIM_STOP_STATE_CNT);

	dsim_write_mask(id, DSIM_ESCMODE, val,
				DSIM_ESCMODE_STOP_STATE_CNT_MASK);
}

static void dsim_reg_set_timeout(u32 id)
{
	u32 val = DSIM_TIMEOUT_BTA_TOUT(DSIM_BTA_TIMEOUT)
		| DSIM_TIMEOUT_LPDR_TOUT(DSIM_LP_RX_TIMEOUT);

	dsim_write(id, DSIM_TIMEOUT, val);
}

static void dsim_reg_disable_hsa(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_HSA_DISABLE);
}

static void dsim_reg_disable_hbp(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_HBP_DISABLE);
}

static void dsim_reg_disable_hfp(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_HFP_DISABLE);
}

static void dsim_reg_disable_hse(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_HSE_DISABLE);
}

static void dsim_reg_set_burst_mode(u32 id, u32 burst)
{
	u32 val = burst ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_BURST_MODE);
}

static void dsim_reg_set_sync_inform(u32 id, u32 inform)
{
	u32 val = inform ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_SYNC_INFORM);
}

#if 0
static void dsim_reg_set_pll_clk_gate_enable(u32 id, u32 en)
{
	u32 mask, reg_id;
	u32 val = en ? ~0 : 0;

	reg_id = DSIM_CONFIG;
	mask = DSIM_CONFIG_PLL_CLOCK_GATING;
	dsim_write_mask(id, reg_id, val, mask);
}
#endif
/* This function is available from EVT1 */
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
static void dsim_reg_set_pll_sleep_enable(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_PLL_SLEEP);
}
#endif

/* 0=D-PHY, 1=C-PHY */
void dsim_reg_set_phy_selection(u32 id, u32 sel)
{
	u32 val = sel ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_PHY_SELECTION);
}

static void dsim_reg_set_vporch(u32 id, u32 vbp, u32 vfp)
{
	u32 val = DSIM_VPORCH_VFP_TOTAL(vfp) | DSIM_VPORCH_VBP(vbp);
	dsim_write(id, DSIM_VPORCH, val);
}

static void dsim_reg_set_vfp_detail(u32 id, u32 cmd_allow, u32 stable_vfp)
{
	u32 val = DSIM_VPORCH_VFP_CMD_ALLOW(cmd_allow)
		| DSIM_VPORCH_STABLE_VFP(stable_vfp);
	dsim_write(id, DSIM_VFP_DETAIL, val);
}

static void dsim_reg_set_hporch(u32 id, u32 hbp, u32 hfp)
{
	u32 val = DSIM_HPORCH_HFP(hfp) | DSIM_HPORCH_HBP(hbp);
	dsim_write(id, DSIM_HPORCH, val);
}

static void dsim_reg_set_sync_area(u32 id, u32 vsa, u32 hsa)
{
	u32 val = DSIM_SYNC_VSA(vsa) | DSIM_SYNC_HSA(hsa);

	dsim_write(id, DSIM_SYNC, val);
}

static void dsim_reg_set_resol(u32 id, struct exynos_panel_info *lcd_info)
{
	u32 height, width, val;

	width = lcd_info->xres;
	height = lcd_info->yres;

	val = DSIM_RESOL_VRESOL(height) | DSIM_RESOL_HRESOL(width);

	dsim_write(id, DSIM_RESOL, val);
}

#if 0 // unused function
static void dsim_reg_set_vresol(u32 id, u32 vresol)
{
	u32 val = DSIM_RESOL_VRESOL(vresol);

	dsim_write_mask(id, DSIM_RESOL, val, DSIM_RESOL_VRESOL_MASK);
}

static void dsim_reg_set_hresol(u32 id, u32 hresol, struct exynos_panel_info *lcd)
{
	u32 width, val;

	if (lcd->dsc.en)
		width = lcd->dsc.enc_sw * lcd->dsc.slice_num;
	else
		width = hresol;

	val = DSIM_RESOL_HRESOL(width);

	dsim_write_mask(id, DSIM_RESOL, val, DSIM_RESOL_HRESOL_MASK);
}
#endif

static void dsim_reg_set_porch(u32 id, struct exynos_panel_info *lcd)
{
	if (lcd->mode == DECON_VIDEO_MODE) {
		dsim_reg_set_vporch(id, lcd->vbp, lcd->vfp);
		dsim_reg_set_vfp_detail(id,
				DSIM_CMD_ALLOW_VALUE,
				DSIM_STABLE_VFP_VALUE);
		dsim_reg_set_hporch(id, lcd->hbp, lcd->hfp);
		dsim_reg_set_sync_area(id, lcd->vsa, lcd->hsa);
	}
}

static void dsim_reg_set_hact_period(u32 id, u32 hact_period)
{
	u32 val = DSIM_VT_HTIMING0_HACT_PERIOD(hact_period);

	dsim_write_mask(id, DSIM_VT_HTIMING0, val,
			DSIM_VT_HTIMING0_HACT_PERIOD_MASK);
}

static void dsim_reg_set_hsa_period(u32 id, u32 hsa_period)
{
	u32 val = DSIM_VT_HTIMING0_HSA_PERIOD(hsa_period);

	dsim_write_mask(id, DSIM_VT_HTIMING0, val,
			DSIM_VT_HTIMING0_HSA_PERIOD_MASK);
}

static void dsim_reg_set_hbp_period(u32 id, u32 hbp_period)
{
	u32 val = DSIM_VT_HTIMING1_HBP_PERIOD(hbp_period);

	dsim_write_mask(id, DSIM_VT_HTIMING1, val,
			DSIM_VT_HTIMING1_HBP_PERIOD_MASK);
}

static void dsim_reg_set_hfp_period(u32 id, u32 hfp_period)
{
	u32 val = DSIM_VT_HTIMING1_HFP_PERIOD(hfp_period);

	dsim_write_mask(id, DSIM_VT_HTIMING1, val,
			DSIM_VT_HTIMING1_HFP_PERIOD_MASK);
}

static void dsim_reg_set_hperiod(u32 id, struct exynos_panel_info *lcd)
{
	u32 vclk,  wclk;
	u32 hblank, vblank;
	u32 width, height;
	u32 hact_period, hsa_period, hbp_period, hfp_period;

	if (lcd->dsc.en)
		width = lcd->xres / 3;
	else
		width = lcd->xres;

	height = lcd->yres;

	if (lcd->mode == DECON_VIDEO_MODE) {
		hblank = lcd->hsa + lcd->hbp + lcd->hfp;
		vblank = lcd->vsa + lcd->vbp + lcd->vfp;
		vclk = DIV_ROUND_CLOSEST((width + hblank) * (height + vblank) * lcd->fps, 1000); /* khz */
		wclk = DIV_ROUND_CLOSEST(lcd->hs_clk * 1000, 16); /* khz */

		/* round calculation to reduce fps error */
		hact_period = DIV_ROUND_CLOSEST(width * wclk, vclk);
		hsa_period = DIV_ROUND_CLOSEST(lcd->hsa * wclk, vclk);
		hbp_period = DIV_ROUND_CLOSEST(lcd->hbp * wclk, vclk);
		hfp_period = DIV_ROUND_CLOSEST(lcd->hfp * wclk, vclk);

		dsim_reg_set_hact_period(id, hact_period);
		dsim_reg_set_hsa_period(id, hsa_period);
		dsim_reg_set_hbp_period(id, hbp_period);
		dsim_reg_set_hfp_period(id, hfp_period);
	}
}

void dsim_reg_update_vfp(u32 id, u32 vfp)
{
	dsim_write_mask(id, DSIM_VPORCH, DSIM_VPORCH_VFP_TOTAL(vfp),
		DSIM_VPORCH_VFP_TOTAL_MASK);
	dsim_info("Applied new VFP!(%d)\n", vfp);
}

static void dsim_reg_set_video_mode(u32 id, u32 mode)
{
	u32 val = mode ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_VIDEO_MODE);
}

static int dsim_reg_wait_idle_status(u32 id, u32 is_vm)
{
	u32 cnt = 1000;
	u32 reg_id, val, status;

	if (is_vm) {
		reg_id = DSIM_LINK_STATUS0;

		do {
			val = dsim_read(id, reg_id);
			status = DSIM_LINK_STATUS0_VIDEO_MODE_STATUS_GET(val);
			if (status == DSIM_STATUS_IDLE)
				break;
			cnt--;
			udelay(10);
		} while (cnt);

		if (!cnt) {
			dsim_err("dsim%d wait timeout idle status(%u)\n", id, status);
			return -EBUSY;
		}
	}

	return 0;
}

/* 0 = command, 1 = video mode */
static u32 dsim_reg_get_display_mode(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_CONFIG);
	return DSIM_CONFIG_DISPLAY_MODE_GET(val);
}

enum dsim_datalane_status dsim_reg_get_datalane_status(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_LINK_STATUS2);
	return DSIM_LINK_STATUS2_DATALANE_STATUS_GET(val);
}

static void dsim_reg_enable_dsc(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CONFIG, val, DSIM_CONFIG_CPRS_EN);
}

static void dsim_reg_set_num_of_slice(u32 id, u32 num_of_slice)
{
	u32 val = DSIM_CPRS_CTRL_NUM_OF_SLICE(num_of_slice);

	dsim_write_mask(id, DSIM_CPRS_CTRL, val,
				DSIM_CPRS_CTRL_NUM_OF_SLICE_MASK);
}

static void dsim_reg_get_num_of_slice(u32 id, u32 *num_of_slice)
{
	u32 val = dsim_read(id, DSIM_CPRS_CTRL);

	*num_of_slice = DSIM_CPRS_CTRL_NUM_OF_SLICE_GET(val);
}

static void dsim_reg_set_multi_slice(u32 id, struct exynos_panel_info *lcd_info)
{
	u32 multi_slice = 1;
	u32 val;

	/* if multi-slice(2~4 slices) DSC compression is used in video mode
	 * MULTI_SLICE_PACKET configuration must be matched
	 * to DDI's configuration
	 */
	if (lcd_info->mode == DECON_MIPI_COMMAND_MODE)
		multi_slice = 1;
	else if (lcd_info->mode == DECON_VIDEO_MODE)
		multi_slice = lcd_info->dsc.slice_num > 1 ? 1 : 0;

	/* if MULTI_SLICE_PACKET is enabled,
	 * only one packet header is transferred
	 * for multi slice
	 */
	val = multi_slice ? ~0 : 0;
	dsim_write_mask(id, DSIM_CPRS_CTRL, val,
				DSIM_CPRS_CTRL_MULI_SLICE_PACKET);
}

static void dsim_reg_set_size_of_slice(u32 id, struct exynos_panel_info *lcd_info)
{
	u32 slice_w = lcd_info->dsc.dec_sw;
	u32 val_01 = 0, mask_01 = 0;
	u32 val_23 = 0, mask_23 = 0;

	if (lcd_info->dsc.slice_num == 4) {
		val_01 = DSIM_SLICE01_SIZE_OF_SLICE1(slice_w) |
			DSIM_SLICE01_SIZE_OF_SLICE0(slice_w);
		mask_01 = DSIM_SLICE01_SIZE_OF_SLICE1_MASK |
			DSIM_SLICE01_SIZE_OF_SLICE0_MASK;
		val_23 = DSIM_SLICE23_SIZE_OF_SLICE3(slice_w) |
			DSIM_SLICE23_SIZE_OF_SLICE2(slice_w);
		mask_23 = DSIM_SLICE23_SIZE_OF_SLICE3_MASK |
			DSIM_SLICE23_SIZE_OF_SLICE2_MASK;

		dsim_write_mask(id, DSIM_SLICE01, val_01, mask_01);
		dsim_write_mask(id, DSIM_SLICE23, val_23, mask_23);
	} else if (lcd_info->dsc.slice_num == 2) {
		val_01 = DSIM_SLICE01_SIZE_OF_SLICE1(slice_w) |
			DSIM_SLICE01_SIZE_OF_SLICE0(slice_w);
		mask_01 = DSIM_SLICE01_SIZE_OF_SLICE1_MASK |
			DSIM_SLICE01_SIZE_OF_SLICE0_MASK;

		dsim_write_mask(id, DSIM_SLICE01, val_01, mask_01);
	} else if (lcd_info->dsc.slice_num == 1) {
		val_01 = DSIM_SLICE01_SIZE_OF_SLICE0(slice_w);
		mask_01 = DSIM_SLICE01_SIZE_OF_SLICE0_MASK;

		dsim_write_mask(id, DSIM_SLICE01, val_01, mask_01);
	} else {
		dsim_err("not supported slice mode. dsc(%d), slice(%d)\n",
				lcd_info->dsc.cnt, lcd_info->dsc.slice_num);
	}
}

static void dsim_reg_print_size_of_slice(u32 id)
{
	u32 val;
	u32 slice0_w, slice1_w, slice2_w, slice3_w;

	val = dsim_read(id, DSIM_SLICE01);
	slice0_w = DSIM_SLICE01_SIZE_OF_SLICE0_GET(val);
	slice1_w = DSIM_SLICE01_SIZE_OF_SLICE1_GET(val);

	val = dsim_read(id, DSIM_SLICE23);
	slice2_w = DSIM_SLICE23_SIZE_OF_SLICE2_GET(val);
	slice3_w = DSIM_SLICE23_SIZE_OF_SLICE3_GET(val);

	dsim_dbg("dsim%d: slice0 w(%d), slice1 w(%d), slice2 w(%d), slice3(%d)\n",
			id, slice0_w, slice1_w, slice2_w, slice3_w);
}

static void dsim_reg_set_multi_packet_count(u32 id, u32 multipacketcnt)
{
	u32 val = DSIM_CMD_CONFIG_MULTI_PKT_CNT(multipacketcnt);

	dsim_write_mask(id, DSIM_CMD_CONFIG, val,
				DSIM_CMD_CONFIG_MULTI_PKT_CNT_MASK);
}

static void dsim_reg_set_cmd_te_ctrl0(u32 id, u32 stablevfp)
{
	u32 val = DSIM_CMD_TE_CTRL0_TIME_STABLE_VFP(stablevfp);

	dsim_write(id, DSIM_CMD_TE_CTRL0, val);
}

static void dsim_reg_set_cmd_te_ctrl1(u32 id, u32 teprotecton, u32 tetout)
{
	u32 val = DSIM_CMD_TE_CTRL1_TIME_TE_PROTECT_ON(teprotecton)
		| DSIM_CMD_TE_CTRL1_TIME_TE_TOUT(tetout);

	dsim_write(id, DSIM_CMD_TE_CTRL1, val);
}

static void dsim_reg_get_cmd_timer(unsigned int fps, unsigned int *te_protect,
		unsigned int *te_timeout, u32 hs_clk)
{
	*te_protect = hs_clk * (100 - TE_MARGIN) * 100 / fps / 16;
	*te_timeout = hs_clk * (100 + TE_MARGIN * 2) * 100 / fps / 16;
}

static void dsim_reg_set_cmd_ctrl(u32 id, struct exynos_panel_info *lcd_info,
						struct dsim_clks *clks)
{
	unsigned int time_stable_vfp;
	unsigned int time_te_protect_on;
	unsigned int time_te_tout;

	time_stable_vfp = lcd_info->xres * DSIM_STABLE_VFP_VALUE * 3 / 100;
	dsim_reg_get_cmd_timer(lcd_info->fps, &time_te_protect_on,
			&time_te_tout, clks->hs_clk);
	dsim_reg_set_cmd_te_ctrl0(id, time_stable_vfp);
	dsim_reg_set_cmd_te_ctrl1(id, time_te_protect_on, time_te_tout);
}

static void dsim_reg_enable_noncont_clock(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val,
				DSIM_CLK_CTRL_NONCONT_CLOCK_LANE);
}

static void dsim_reg_enable_clocklane(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val,
				DSIM_CLK_CTRL_CLKLANE_ONOFF);
}

void dsim_reg_enable_packetgo(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CMD_CONFIG, val,
				DSIM_CMD_CONFIG_PKT_GO_EN);
}

void dsim_reg_set_packetgo_ready(u32 id)
{
	dsim_write_mask(id, DSIM_CMD_CONFIG, DSIM_CMD_CONFIG_PKT_GO_RDY,
			DSIM_CMD_CONFIG_PKT_GO_RDY);
}

static void dsim_reg_enable_multi_cmd_packet(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CMD_CONFIG, val,
				DSIM_CMD_CONFIG_MULTI_CMD_PKT_EN);
}

static void dsim_reg_enable_shadow(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_SFR_CTRL, val,
				DSIM_SFR_CTRL_SHADOW_EN);
}

static void dsim_reg_set_link_clock(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val, DSIM_CLK_CTRL_CLOCK_SEL);
}

static int dsim_reg_get_link_clock(u32 id)
{
	int val = 0;

	val = dsim_read_mask(id, DSIM_CLK_CTRL, DSIM_CLK_CTRL_CLOCK_SEL);

	return val;
}

static void dsim_reg_enable_hs_clock(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val, DSIM_CLK_CTRL_TX_REQUEST_HSCLK);
}

static void dsim_reg_enable_word_clock(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_CLK_CTRL, val, DSIM_CLK_CTRL_WORDCLK_EN);
}

u32 dsim_reg_is_hs_clk_ready(u32 id)
{
	if (dsim_read(id, DSIM_DPHY_STATUS) & DSIM_DPHY_STATUS_TX_READY_HS_CLK)
		return 1;

	return 0;
}

u32 dsim_reg_is_clk_stop(u32 id)
{
	if (dsim_read(id, DSIM_DPHY_STATUS) & DSIM_DPHY_STATUS_STOP_STATE_CLK)
		return 1;

	return 0;
}

int dsim_reg_wait_hs_clk_ready(u32 id)
{
	u32 state;
	u32 cnt = 1000;

	do {
		state = dsim_reg_is_hs_clk_ready(id);
		cnt--;
		udelay(10);
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("DSI Master is not HS state.\n");
		return -EBUSY;
	}

	return 0;
}

int dsim_reg_wait_hs_clk_disable(u32 id)
{
	u32 state;
	u32 cnt = 1000;

	do {
		state = dsim_reg_is_clk_stop(id);
		cnt--;
		udelay(10);
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("DSI Master clock isn't disabled.\n");
		return -EBUSY;
	}

	return 0;
}

void dsim_reg_force_dphy_stop_state(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_ESCMODE, val, DSIM_ESCMODE_FORCE_STOP_STATE);
}

void dsim_reg_enter_ulps(u32 id, u32 enter)
{
	u32 val = enter ? ~0 : 0;
	u32 mask = DSIM_ESCMODE_TX_ULPS_CLK | DSIM_ESCMODE_TX_ULPS_DATA;

	dsim_write_mask(id, DSIM_ESCMODE, val, mask);
}

void dsim_reg_exit_ulps(u32 id, u32 exit)
{
	u32 val = exit ? ~0 : 0;
	u32 mask = DSIM_ESCMODE_TX_ULPS_CLK_EXIT |
			DSIM_ESCMODE_TX_ULPS_DATA_EXIT;

	dsim_write_mask(id, DSIM_ESCMODE, val, mask);
}

void dsim_reg_set_num_of_transfer(u32 id, u32 num_of_transfer)
{
	u32 val = DSIM_NUM_OF_TRANSFER_PER_FRAME(num_of_transfer);

	dsim_write(id, DSIM_NUM_OF_TRANSFER, val);

	dsim_dbg("%s, write value : 0x%x, read value : 0x%x\n", __func__,
			val, dsim_read(id, DSIM_NUM_OF_TRANSFER));
}

static u32 dsim_reg_is_ulps_state(u32 id, u32 lanes)
{
	u32 val = dsim_read(id, DSIM_DPHY_STATUS);
	u32 data_lane = lanes >> DSIM_LANE_CLOCK;

	if ((DSIM_DPHY_STATUS_ULPS_DATA_LANE_GET(val) == data_lane)
			&& (val & DSIM_DPHY_STATUS_ULPS_CLK))
		return 1;

	return 0;
}

static void dsim_reg_set_deskew_hw(u32 id, u32 interval, u32 position)
{
	u32 val = DSIM_DESKEW_CTRL_HW_INTERVAL(interval)
		| DSIM_DESKEW_CTRL_HW_POSITION(position);
	u32 mask = DSIM_DESKEW_CTRL_HW_INTERVAL_MASK
		| DSIM_DESKEW_CTRL_HW_POSITION_MASK;

	dsim_write_mask(id, DSIM_DESKEW_CTRL, val, mask);
}

static void dsim_reg_enable_deskew_hw_enable(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_DESKEW_CTRL, val, DSIM_DESKEW_CTRL_HW_EN);
}

static void dsim_reg_set_cm_underrun_lp_ref(u32 id, u32 lp_ref)
{
	u32 val = DSIM_UNDERRUN_CTRL_CM_UNDERRUN_LP_REF(lp_ref);

	dsim_write(id, DSIM_UNDERRUN_CTRL, val);
}

static void dsim_reg_set_threshold(u32 id, u32 threshold)
{
	u32 val = DSIM_THRESHOLD_LEVEL(threshold);

	dsim_write(id, DSIM_THRESHOLD, val);
}

static void dsim_reg_set_vstatus_int(u32 id, u32 vstatus)
{
	u32 val = DSIM_VIDEO_TIMER_VSTATUS_INTR_SEL(vstatus);

	dsim_write_mask(id, DSIM_VIDEO_TIMER, val,
			DSIM_VIDEO_TIMER_VSTATUS_INTR_SEL_MASK);
}

static void dsim_reg_set_bist_te_interval(u32 id, u32 interval)
{
	u32 val = DSIM_BIST_CTRL0_BIST_TE_INTERVAL(interval);

	dsim_write_mask(id, DSIM_BIST_CTRL0, val,
			DSIM_BIST_CTRL0_BIST_TE_INTERVAL_MASK);
}

static void dsim_reg_set_bist_mode(u32 id, u32 bist_mode)
{
	u32 val = DSIM_BIST_CTRL0_BIST_PTRN_MODE(bist_mode);

	dsim_write_mask(id, DSIM_BIST_CTRL0, val,
			DSIM_BIST_CTRL0_BIST_PTRN_MODE_MASK);
}

static void dsim_reg_enable_bist_pattern_move(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_BIST_CTRL0, val,
			DSIM_BIST_CTRL0_BIST_PTRN_MOVE_EN);
}

static void dsim_reg_enable_bist(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_BIST_CTRL0, val, DSIM_BIST_CTRL0_BIST_EN);
}

static void dsim_set_hw_deskew(u32 id, u32 en)
{
	u32 hw_interval = 1;

	if (en) {
		/* 0 : VBP first line, 1 : VFP last line*/
		dsim_reg_set_deskew_hw(id, hw_interval, 0);
		dsim_reg_enable_deskew_hw_enable(id, en);
	} else {
		dsim_reg_enable_deskew_hw_enable(id, en);
	}
}

static int dsim_reg_wait_enter_ulps_state(u32 id, u32 lanes)
{
	u32 state;
	u32 cnt = 1000;

	do {
		state = dsim_reg_is_ulps_state(id, lanes);
		cnt--;
		udelay(10);
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("DSI Master is not ULPS state.\n");
		return -EBUSY;
	}

	dsim_dbg("DSI Master is ULPS state.\n");

	return 0;
}

static u32 dsim_reg_is_not_ulps_state(u32 id)
{
	u32 val = dsim_read(id, DSIM_DPHY_STATUS);

	if (!(DSIM_DPHY_STATUS_ULPS_DATA_LANE_GET(val))
			&& !(val & DSIM_DPHY_STATUS_ULPS_CLK))
		return 1;

	return 0;
}

static int dsim_reg_wait_exit_ulps_state(u32 id)
{
	u32 state;
	u32 cnt = 1000;

	do {
		state = dsim_reg_is_not_ulps_state(id);
		cnt--;
		udelay(10);
	} while (!state && cnt);

	if (!cnt) {
		dsim_err("DSI Master is not stop state.\n");
		return -EBUSY;
	}

	dsim_dbg("DSI Master is stop state.\n");

	return 0;
}

static int dsim_reg_get_dphy_timing(u32 hs_clk, u32 esc_clk,
		struct dphy_timing_value *t)
{
	int i;

	i = ARRAY_SIZE(dphy_timing) - 1;

	for (; i >= 0; i--) {
		if (dphy_timing[i][0] < hs_clk) {
			continue;
		} else {
			t->bps = hs_clk;
			t->clk_prepare = dphy_timing[i][1];
			t->clk_zero = dphy_timing[i][2];
			t->clk_post = dphy_timing[i][3];
			t->clk_trail = dphy_timing[i][4];
			t->hs_prepare = dphy_timing[i][5];
			t->hs_zero = dphy_timing[i][6];
			t->hs_trail = dphy_timing[i][7];
			t->lpx = dphy_timing[i][8];
			t->hs_exit = dphy_timing[i][9];
			break;
		}
	}

	if (i < 0) {
		dsim_err("%u Mhz hs clock can't find proper dphy timing values\n",
				hs_clk);
		return -EINVAL;
	}

	dsim_dbg("%s: bps(%u) clk_prepare(%u) clk_zero(%u) clk_post(%u)\n",
			__func__, t->bps, t->clk_prepare, t->clk_zero,
			t->clk_post);
	dsim_dbg("clk_trail(%u) hs_prepare(%u) hs_zero(%u) hs_trail(%u)\n",
			t->clk_trail, t->hs_prepare, t->hs_zero, t->hs_trail);
	dsim_dbg("lpx(%u) hs_exit(%u)\n", t->lpx, t->hs_exit);

	if ((esc_clk > 20) || (esc_clk < 7)) {
		dsim_err("%u Mhz cann't be used as escape clock\n", esc_clk);
		return -EINVAL;
	}

	t->b_dphyctl = b_dphyctl[esc_clk - 7];
	dsim_dbg("b_dphyctl(%u)\n", t->b_dphyctl);

	return 0;
}

static void dsim_reg_set_config(u32 id, struct exynos_panel_info *lcd_info,
		struct dsim_clks *clks)
{
	u32 threshold;
	u32 num_of_slice;
	u32 num_of_transfer;
	int idx;

	/* shadow read disable */
	dsim_reg_enable_shadow_read(id, 1);
	/* dsim_reg_enable_shadow(id, 1); */

	if (lcd_info->mode == DECON_VIDEO_MODE)
		dsim_reg_enable_clocklane(id, 0);
	else
		dsim_reg_enable_noncont_clock(id, 1);

	dsim_set_hw_deskew(id, 0); /* second param is to control enable bit */

	/* set bta & lpdr timeout vlaues */
	dsim_reg_set_timeout(id);

	dsim_reg_set_cmd_transfer_mode(id, 0);
	dsim_reg_set_stop_state_cnt(id);

	if (lcd_info->mode == DECON_MIPI_COMMAND_MODE) {
		idx = lcd_info->mres_mode;
		dsim_reg_set_cm_underrun_lp_ref(id, lcd_info->cmd_underrun_cnt[idx]);
	}

	if (lcd_info->dsc.en)
		threshold = lcd_info->dsc.enc_sw * lcd_info->dsc.slice_num;
	else
		threshold = lcd_info->xres;

	dsim_reg_set_threshold(id, threshold);

	dsim_reg_set_resol(id, lcd_info);
	dsim_reg_set_porch(id, lcd_info);

	if (lcd_info->mode == DECON_MIPI_COMMAND_MODE) {
		if (lcd_info->dsc.en)
			/* use 1-line transfer only */
			num_of_transfer = lcd_info->yres;
		else
			num_of_transfer = lcd_info->xres * lcd_info->yres
							/ threshold;

		dsim_reg_set_num_of_transfer(id, num_of_transfer);
	} else {
		num_of_transfer = lcd_info->xres * lcd_info->yres / threshold;
		dsim_reg_set_num_of_transfer(id, num_of_transfer);
	}

	/* set number of lanes, eotp enable, per_frame_read, pixformat, vc_id */
	dsim_reg_set_config_options(id, lcd_info->data_lane -1,
			(lcd_info->eotp_disabled ? 0 : 1),
			0, DSIM_PIXEL_FORMAT_RGB24, 0);

	/* CPSR & VIDEO MODE & HPERIOD can be set only when shadow enable on */
	/* shadow enable */
	dsim_reg_enable_shadow(id, 1);
	if (lcd_info->mode == DECON_VIDEO_MODE) {
		dsim_reg_set_video_mode(id, DSIM_CONFIG_VIDEO_MODE);
		dsim_dbg("%s: video mode set\n", __func__);
	} else {
		dsim_reg_set_video_mode(id, 0);
		dsim_dbg("%s: command mode set\n", __func__);
	}

	dsim_reg_enable_dsc(id, lcd_info->dsc.en);

	/* shadow disable */
	if (lcd_info->mode == DECON_MIPI_COMMAND_MODE)
		dsim_reg_enable_shadow(id, 0);

	if (lcd_info->mode == DECON_VIDEO_MODE) {
		dsim_reg_disable_hsa(id, 0);
		dsim_reg_disable_hbp(id, 0);
		dsim_reg_disable_hfp(id, 1);
		dsim_reg_disable_hse(id, 0);
		dsim_reg_set_burst_mode(id, 1);
		dsim_reg_set_sync_inform(id, 0);
		dsim_reg_enable_clocklane(id, 0);
	}

	if (lcd_info->dsc.en) {
		dsim_dbg("%s: dsc configuration is set\n", __func__);
		dsim_reg_set_num_of_slice(id, lcd_info->dsc.slice_num);
		dsim_reg_set_multi_slice(id, lcd_info); /* multi slice */
		dsim_reg_set_size_of_slice(id, lcd_info);

		dsim_reg_get_num_of_slice(id, &num_of_slice);
		dsim_dbg("dsim%d: number of DSC slice(%d)\n", id, num_of_slice);
		dsim_reg_print_size_of_slice(id);
	}

	if (lcd_info->mode == DECON_VIDEO_MODE) {
		dsim_reg_set_multi_packet_count(id, DSIM_PH_FIFOCTRL_THRESHOLD);
		dsim_reg_enable_multi_cmd_packet(id, 0);
	}
	dsim_reg_enable_packetgo(id, 0);

	if (lcd_info->mode == DECON_MIPI_COMMAND_MODE) {
		dsim_reg_set_cmd_ctrl(id, lcd_info, clks);
	} else if (lcd_info->mode == DECON_VIDEO_MODE) {
		dsim_reg_set_hperiod(id, lcd_info);
		dsim_reg_set_vstatus_int(id, DSIM_VFP);
	}

	/* dsim_reg_enable_shadow_read(id, 1); */
	/* dsim_reg_enable_shadow(id, 1); */

}

/*
 * configure and set DPHY PLL, byte clock, escape clock and hs clock
 *	- PMS value have to be optained by using PMS Gen.
 *      tool (MSC_PLL_WIZARD2_00.exe)
 *	- PLL out is source clock of HS clock
 *	- byte clock = HS clock / 16
 *	- calculate divider of escape clock using requested escape clock
 *	  from driver
 *	- DPHY PLL, byte clock, escape clock are enabled.
 *	- HS clock will be enabled another function.
 *
 * Parameters
 *	- hs_clk : in/out parameter.
 *		in :  requested hs clock. out : calculated hs clock
 *	- esc_clk : in/out paramter.
 *		in : requested escape clock. out : calculated escape clock
 *	- word_clk : out parameter. byte clock = hs clock / 16
 */
static int dsim_reg_set_clocks(u32 id, struct dsim_clks *clks,
		struct stdphy_pms *dphy_pms, u32 en)
{
	unsigned int esc_div;
	struct dsim_pll_param pll;
	struct dphy_timing_value t;
	u32 pll_lock_cnt;
	int ret = 0;
	u32 hsmode = 0;

	if (en) {
		/*
		 * Do not need to set clocks related with PLL,
		 * if DPHY_PLL is already stabled because of LCD_ON_UBOOT.
		 */
		if (dsim_reg_is_pll_stable(id)) {
			dsim_info("dsim%d DPHY PLL is already stable\n", id);
			return -EBUSY;
		}

		/*
		 * set p, m, s to DPHY PLL
		 * PMS value has to be optained by PMS calculation tool
		 * released to customer
		 */
		pll.p = dphy_pms->p;
		pll.m = dphy_pms->m;
		pll.s = dphy_pms->s;
		pll.k = dphy_pms->k;

		/* get word clock */
		/* clks ->hs_clk is from DT */
		clks->word_clk = clks->hs_clk / 16;
		dsim_dbg("word clock is %u MHz\n", clks->word_clk);

		/* requeseted escape clock */
		dsim_dbg("requested escape clock %u MHz\n", clks->esc_clk);

		/* escape clock divider */
		esc_div = clks->word_clk / clks->esc_clk;

		/* adjust escape clock */
		if ((clks->word_clk / esc_div) > clks->esc_clk)
			esc_div += 1;

		/* adjusted escape clock */
		clks->esc_clk = clks->word_clk / esc_div;
		dsim_dbg("escape clock divider is 0x%x\n", esc_div);
		dsim_dbg("escape clock is %u MHz\n", clks->esc_clk);

		if (clks->hs_clk < 1500)
			hsmode = 1;

		dsim_reg_set_esc_clk_prescaler(id, 1, esc_div);
		/* get DPHY timing values using hs clock and escape clock */
		dsim_reg_get_dphy_timing(clks->hs_clk, clks->esc_clk, &t);
		dsim_reg_set_dphy_timing_values(id, &t, hsmode);
#if defined(CONFIG_EXYNOS_DSIM_DITHER)
		/* check dither sequence */
		dsim_reg_set_dphy_param_dither(id, dphy_pms);
		dsim_reg_set_dphy_dither_en(id, 1);
#endif

		/* set BIAS ctrl */
		/* DSI & CSI need this setting */
		dsim_reg_set_bias_ctrl(id, DSIM_PHY_BIAS_VAL);

		/* set DTB ctrl */
		/* DSI & CSI need this setting */
		dsim_reg_set_dtb_ctrl(id, DSIM_PHY_DTB_VAL);

		/* set clock lane analog ctrl */
		dsim_reg_set_actrl_mc(id, DSIM_PHY_ACTRL_MC_VAL);

		/* set data lane analog ctrl */
		dsim_reg_set_actrl_md0(id, DSIM_PHY_ACTRL_MD0_VAL);
		dsim_reg_set_actrl_md1(id, DSIM_PHY_ACTRL_MD1_VAL);
		dsim_reg_set_actrl_md2(id, DSIM_PHY_ACTRL_MD2_VAL);
		dsim_reg_set_actrl_md3(id, DSIM_PHY_ACTRL_MD3_VAL);

		/* set pll pmsk */
		dsim_reg_set_pll_freq(id, pll.p, pll.m, pll.s, pll.k);

		/* set PLL's lock time (lock_cnt)
		 * PLL lock cnt setting guide
		 * PLL_LOCK_CNT_MULT = 500
		 * PLL_LOCK_CNT_MARGIN = 10 (10%)
		 * PLL lock time = PLL_LOCK_CNT_MULT * Tp
		 * Tp = 1 / (OSC clk / pll_p)
		 * PLL lock cnt = PLL lock time * OSC clk
		 */
		pll_lock_cnt = (PLL_LOCK_CNT_MULT + PLL_LOCK_CNT_MARGIN) * pll.p;
		dsim_reg_pll_stable_time(id, pll_lock_cnt);

		/* enable PLL */
		ret = dsim_reg_enable_pll(id, 1);
	} else {
		/* check disable PHY timing */
		/* TBD */
		dsim_reg_set_esc_clk_prescaler(id, 0, 0xff);
#if defined(CONFIG_EXYNOS_DSIM_DITHER)
		/* check dither sequence */
		dsim_reg_set_dphy_dither_en(id, 0);
#endif
		dsim_reg_enable_pll(id, 0);
	}

	return ret;
}

static int dsim_reg_set_lanes(u32 id, u32 lanes, u32 en)
{
	/* LINK lanes */
	dsim_reg_enable_lane(id, lanes, en);
	udelay(400);

	return 0;
}

static u32 dsim_reg_is_noncont_clk_enabled(u32 id)
{
	int ret;

	ret = dsim_read_mask(id, DSIM_CLK_CTRL,
					DSIM_CLK_CTRL_NONCONT_CLOCK_LANE);
	return ret;
}

static int dsim_reg_set_hs_clock(u32 id, u32 en)
{
	int reg = 0;
	int is_noncont = dsim_reg_is_noncont_clk_enabled(id);

	if (en) {
		dsim_reg_enable_hs_clock(id, 1);
		if (!is_noncont)
			reg = dsim_reg_wait_hs_clk_ready(id);
	} else {
		dsim_reg_enable_hs_clock(id, 0);
		reg = dsim_reg_wait_hs_clk_disable(id);
	}
	return reg;
}

static void dsim_reg_set_int(u32 id, u32 en)
{
	u32 val = en ? 0 : ~0;
	u32 mask;

	/*
	 * TODO: underrun irq will be unmasked in the future.
	 * underrun irq(dsim_reg_set_config) is ignored in zebu emulator.
	 * it's not meaningful
	 */
	mask = DSIM_INTMSK_SW_RST_RELEASE | DSIM_INTMSK_SFR_PL_FIFO_EMPTY |
		DSIM_INTMSK_SFR_PH_FIFO_EMPTY |
		DSIM_INTMSK_FRAME_DONE | DSIM_INTMSK_INVALID_SFR_VALUE |
		DSIM_INTMSK_UNDER_RUN | DSIM_INTMSK_RX_DATA_DONE |
		DSIM_INTMSK_ERR_RX_ECC | DSIM_INTMSK_VT_STATUS;

	dsim_write_mask(id, DSIM_INTMSK, val, mask);
}

/*
 * enter or exit ulps mode
 *
 * Parameter
 *	1 : enter ULPS mode
 *	0 : exit ULPS mode
 */
static int dsim_reg_set_ulps(u32 id, u32 en, u32 lanes)
{
	int ret = 0;

	if (en) {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(id, 1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(id, lanes);
		if (ret)
			return ret;

	} else {
		/* Exit ULPS clock and data lane */
		dsim_reg_exit_ulps(id, 1);

		ret = dsim_reg_wait_exit_ulps_state(id);
		if (ret)
			return ret;

		/* wait at least 1ms : Twakeup time for MARK1 state  */
		udelay(1000);

		/* Clear ULPS exit request */
		dsim_reg_exit_ulps(id, 0);

		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(id, 0);
	}

	return ret;
}

/*
 * enter or exit ulps mode for LSI DDI
 *
 * Parameter
 *	1 : enter ULPS mode
 *	0 : exit ULPS mode
 * assume that disp block power is off after ulps mode enter
 */
static int dsim_reg_set_smddi_ulps(u32 id, u32 en, u32 lanes)
{
	int ret = 0;

	if (en) {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(id, 1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(id, lanes);
		if (ret)
			return ret;
		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(id, 0);
	} else {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(id, 1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(id, lanes);
		if (ret)
			return ret;

		/* Exit ULPS clock and data lane */
		dsim_reg_exit_ulps(id, 1);

		ret = dsim_reg_wait_exit_ulps_state(id);
		if (ret)
			return ret;

		/* wait at least 1ms : Twakeup time for MARK1 state */
		udelay(100);

		/* Clear ULPS enter request */
		dsim_reg_enter_ulps(id, 0);

		/* Clear ULPS exit request */
		dsim_reg_exit_ulps(id, 0);
	}

	return ret;
}

/*
 * enter or exit ulps mode
 *
 * Parameter
 *	1 : enter ULPS mode
 *	0 : not used
 */
int dsim_reg_set_magnaddi_ulps(u32 id, u32 en, u32 lanes)
{
	int ret = 0;

	if (en) {
		/* Enable ULPS clock and data lane */
		dsim_reg_enter_ulps(id, 1);

		/* Check ULPS request for data lane */
		ret = dsim_reg_wait_enter_ulps_state(id, lanes);
		if (ret)
			return ret;
	}

	return ret;
}

static int dsim_reg_set_ulps_by_ddi(u32 id, u32 ddi_type, u32 lanes, u32 en)
{
	int ret;

	switch (ddi_type) {
	case TYPE_OF_SM_DDI:
		ret = dsim_reg_set_smddi_ulps(id, en, lanes);
		break;
	case TYPE_OF_MAGNA_DDI:
		ret = dsim_reg_set_magnaddi_ulps(id, en, lanes);
		break;
	case TYPE_OF_NORMAL_DDI:
	default:
		ret = dsim_reg_set_ulps(id, en, lanes);
		break;
	}

	if (ret < 0)
		dsim_err("%s: failed to %s ULPS", __func__,
				en ? "enter" : "exit");

	return ret;
}

/******************** EXPORTED DSIM CAL APIs ********************/

void dpu_sysreg_select_dphy_rst_control(void __iomem *sysreg, u32 dsim_id, u32 sel)
{
	u32 phy_num = dsim_id;
	u32 old = readl(sysreg + DISP_DPU_MIPI_PHY_CON);
	u32 val = sel ? ~0 : 0;
	u32 mask = SEL_RESET_DPHY_MASK(phy_num);

	/* exynos3830 only can support dsim id 0 */
	BUG_ON(dsim_id);

	val = (val & mask) | (old & ~mask);
	writel(val, sysreg + DISP_DPU_MIPI_PHY_CON);
	dsim_dbg("%s: phy_con_sel val=0x%x", __func__, val);
}

void dpu_sysreg_dphy_reset(void __iomem *sysreg, u32 dsim_id, u32 rst)
{
	u32 old = readl(sysreg + DISP_DPU_MIPI_PHY_CON);
	u32 val = rst ? ~0 : 0;
	u32 mask;

	/* exynos3830 only can support dsim id 0 */
	BUG_ON(dsim_id);

	mask = M_RESETN_M4S4_TOP_MASK;

	val = (val & mask) | (old & ~mask);
	writel(val, sysreg + DISP_DPU_MIPI_PHY_CON);
	dsim_dbg("%s: phy_con_rst val=0x%x", __func__, val);
}

static u32 dsim_reg_translate_lanecnt_to_lanes(int lanecnt)
{
	u32 lanes, i;

	lanes = DSIM_LANE_CLOCK;
	for (i = 1; i <= lanecnt; ++i)
		lanes |= 1 << i;

	return lanes;
}

/* This function is just for dsim basic initialization for reading panel id */
void dsim_reg_preinit(u32 id)
{
	u32 lanes;
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct dsim_clks clks;
	struct exynos_panel_info lcd_info;

	/* default configuration just for reading panel id */
#if 0 // for univarsal 3830 device
	memset(&clks, 0, sizeof(struct dsim_clks));
	clks.hs_clk = 830;
	clks.esc_clk = 16;
	memset(&lcd_info, 0, sizeof(struct exynos_panel_info));
	lcd_info.vfp = 24;
	lcd_info.vbp = 4;
	lcd_info.vsa = 2;
	lcd_info.hfp = 508;
	lcd_info.hbp = 80;
	lcd_info.hsa = 2;
	lcd_info.fps = 60;
	lcd_info.hs_clk = 830;
	lcd_info.mode = DECON_VIDEO_MODE;
	lcd_info.xres = 720;
	lcd_info.yres = 1600;
	lcd_info.dphy_pms.p = 2;
	lcd_info.dphy_pms.m = 255;
	lcd_info.dphy_pms.s = 2;
	lcd_info.dphy_pms.k = 25206;
	lcd_info.data_lane = 4;
	lcd_info.cmd_underrun_cnt[0] = 1695;
#else // configuration for erd3830 device
	memset(&clks, 0, sizeof(struct dsim_clks));
	clks.hs_clk = 898;
	clks.esc_clk = 20;
	memset(&lcd_info, 0, sizeof(struct exynos_panel_info));
	lcd_info.vfp = 20;
	lcd_info.vbp = 2;
	lcd_info.vsa = 2;
	lcd_info.hfp = 20;
	lcd_info.hbp = 20;
	lcd_info.hsa = 20;
	lcd_info.fps = 60;
	lcd_info.hs_clk = 898;
	lcd_info.mode = DECON_MIPI_COMMAND_MODE;
	lcd_info.xres = 1080;
	lcd_info.yres = 1920;
	lcd_info.dphy_pms.p = 2;
	lcd_info.dphy_pms.m = 276;
	lcd_info.dphy_pms.s = 2;
	lcd_info.dphy_pms.k = 20165; //0x4ec5
	lcd_info.data_lane = 4;
	lcd_info.cmd_underrun_cnt[0] = 583;
#endif

	/* DPHY reset control from SYSREG(0) */
	dpu_sysreg_select_dphy_rst_control(dsim->res.ss_regs, dsim->id, 0);

	lanes = dsim_reg_translate_lanecnt_to_lanes(lcd_info.data_lane);

	/* choose OSC_CLK */
	dsim_reg_set_link_clock(id, 0);

	dsim_reg_sw_reset(id);
	dsim_reg_set_lanes(id, lanes, 1);
	dsim_reg_set_esc_clk_on_lane(id, 1, lanes);
	dsim_reg_enable_word_clock(id, 1);

	/* Enable DPHY reset : DPHY reset start */
	dpu_sysreg_dphy_reset(dsim->res.ss_regs, id, 0);

	/* panel power on and reset are mandatory for reading panel id */
	dsim_set_panel_power(dsim, 1);

	dsim_reg_set_clocks(id, &clks, &lcd_info.dphy_pms, 1);
	dpu_sysreg_dphy_reset(dsim->res.ss_regs, id, 1); /* Release DPHY reset */

	dsim_reg_set_link_clock(id, 1);	/* Selection to word clock */

	dsim_reg_set_config(id, &lcd_info, &clks);

	/* PHY pll clock gate disable */
//	dsim_reg_set_pll_clk_gate_enable(id, DPHY_PLL_CLK_GATE_EN);
	dsim_reset_panel(dsim);
}

int dsim_reg_init(u32 id, struct exynos_panel_info *lcd_info, struct dsim_clks *clks,
		bool panel_ctrl)
{
	int ret = 0;
	u32 lanes;
#if !defined(CONFIG_EXYNOS_LCD_ON_UBOOT)
	struct dsim_device *dsim = get_dsim_drvdata(id);
#endif

	if (dsim->state == DSIM_STATE_INIT) {
		if (dsim_reg_get_link_clock(dsim->id)) {
			dsim_info("dsim%d is already enabled in bootloader\n", dsim->id);
			/* to prevent irq storm that may occur in the OFF STATE */
			dsim_reg_clear_int(id, 0xffffffff);
			return 0;
		}
	}

#if defined(CONFIG_EXYNOS_LCD_ON_UBOOT)
	/* TODO: This code will be implemented as uboot style */
#else
	/* Panel power on */
	if (panel_ctrl)
		ret = dsim_set_panel_power(dsim, 1);
#endif

	/* DPHY reset control from SYSREG(0) */
	dpu_sysreg_select_dphy_rst_control(dsim->res.ss_regs, dsim->id, 0);

	lanes = dsim_reg_translate_lanecnt_to_lanes(lcd_info->data_lane);

	/* choose OSC_CLK */
	dsim_reg_set_link_clock(id, 0);

	dsim_reg_sw_reset(id);

	dsim_reg_set_lanes(id, lanes, 1);

	dsim_reg_set_esc_clk_on_lane(id, 1, lanes);

	dsim_reg_enable_word_clock(id, 1);

	/* Enable DPHY reset : DPHY reset start */
	dpu_sysreg_dphy_reset(dsim->res.ss_regs, id, 0);


	dsim_reg_set_clocks(id, clks, &lcd_info->dphy_pms, 1);

	dpu_sysreg_dphy_reset(dsim->res.ss_regs, id, 1); /* Release DPHY reset */

	dsim_reg_set_link_clock(id, 1);	/* Selection to word clock */

	dsim_reg_set_config(id, lcd_info, clks);

//	dsim_reg_set_pll_clk_gate_enable(id, DPHY_PLL_CLK_GATE_EN); /* PHY pll clock gate disable */
#if defined(CONFIG_EXYNOS_PLL_SLEEP)
	dsim_reg_set_pll_sleep_enable(id, true);	/* PHY pll sleep enable */
#endif

#if defined(CONFIG_EXYNOS_LCD_ON_UBOOT)
	/* TODO: This code will be implemented as uboot style */
#else
	if (!ret && panel_ctrl)
		ret = dsim_reset_panel(dsim);
#endif

	return ret;
}

/* Set clocks and lanes and HS ready */
void dsim_reg_start(u32 id)
{
	dsim_reg_set_hs_clock(id, 1);
	dsim_reg_set_int(id, 1);
}

/* Unset clocks and lanes and stop_state */
int dsim_reg_stop(u32 id, u32 lanes)
{
	int err = 0;
	u32 is_vm;

	struct dsim_device *dsim = get_dsim_drvdata(id);

	dsim_reg_clear_int(id, 0xffffffff);
	/* disable interrupts */
	dsim_reg_set_int(id, 0);

	/* first disable HS clock */
	if (dsim_reg_set_hs_clock(id, 0) < 0)
		dsim_err("The CLK lane doesn't be switched to LP mode\n");

	/* 0. wait the IDLE status */
	is_vm = dsim_reg_get_display_mode(id);
	err = dsim_reg_wait_idle_status(id, is_vm);
	if (err < 0)
		dsim_err("DSIM status is not IDLE!\n");

	/* 1. clock selection : OSC */
	dsim_reg_set_link_clock(id, 0);

	/* 2. master resetn */
	dpu_sysreg_dphy_reset(dsim->res.ss_regs, id, 0);
	/* 3. disable lane */
	dsim_reg_set_lanes(id, lanes, 0);
	/* 4. turn off WORDCLK and ESCCLK */
	dsim_reg_set_esc_clk_on_lane(id, 0, lanes);
	dsim_reg_set_esc_clk_en(id, 0);
	/* 5. disable PLL */
	dsim_reg_set_clocks(id, NULL, NULL, 0);

	if (err == 0)
		dsim_reg_sw_reset(id);

	return err;
}

void dsim_reg_recovery_process(struct dsim_device *dsim)
{
	dsim_info("%s +\n", __func__);

	dsim_reg_clear_int(dsim->id, 0xffffffff);

	/* 0. disable HS clock */
	if (dsim_reg_set_hs_clock(dsim->id, 0) < 0)
		dsim_err("The CLK lane doesn't be switched to LP mode\n");

	/* 1. clock selection : OSC */
	dsim_reg_set_link_clock(dsim->id, 0);

	/* 2. reset & release */
	dpu_sysreg_dphy_reset(dsim->res.ss_regs, dsim->id, 0);
	dsim_reg_function_reset(dsim->id);
	dpu_sysreg_dphy_reset(dsim->res.ss_regs, dsim->id, 1);

	/* 3. clock selection : PLL */
	dsim_reg_set_link_clock(dsim->id, 1);

	/* 4. enable HS clock */
	dsim_reg_set_hs_clock(dsim->id, 1);

	dsim_info("%s -\n", __func__);
}

/* Exit ULPS mode and set clocks and lanes */
int dsim_reg_exit_ulps_and_start(u32 id, u32 ddi_type, u32 lanes)
{
	int ret = 0;

	#if 0
	/*
	 * Guarantee 1.2v signal level for data lane(positive) when exit ULPS.
	 * DSIM Should be set standby. If not, lane goes to 600mv sometimes.
	 */
	dsim_reg_set_hs_clock(id, 1);
	dsim_reg_set_hs_clock(id, 0);
	#endif

	/* try to exit ULPS mode. The sequence is depends on DDI type */
	ret = dsim_reg_set_ulps_by_ddi(id, ddi_type, lanes, 0);
	dsim_reg_start(id);
	return ret;
}

/* Unset clocks and lanes and enter ULPS mode */
int dsim_reg_stop_and_enter_ulps(u32 id, u32 ddi_type, u32 lanes)
{
	int ret = 0;
	u32 is_vm;
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct dsim_regs regs;

	dsim_reg_clear_int(id, 0xffffffff);
	/* disable interrupts */
	dsim_reg_set_int(id, 0);

	/* 1. turn off clk lane & wait for stopstate_clk */
	ret = dsim_reg_set_hs_clock(id, 0);
	if (ret < 0)
		dsim_err("The CLK lane doesn't be switched to LP mode\n");

	/* 2. enter to ULPS & wait for ulps state of clk and data */
	dsim_reg_set_ulps_by_ddi(id, ddi_type, lanes, 1);

	/* 3. sequence for BLK_DPU off */
	/* 3.1 wait idle */
	is_vm = dsim_reg_get_display_mode(id);
	ret = dsim_reg_wait_idle_status(id, is_vm);
	if (ret < 0)
		dsim_err("%s : DSIM_status is not IDLE!\n", __func__);
	/* 3.2 OSC clock */
	dsim_reg_set_link_clock(id, 0);
	/* 3.3 off DPHY */
	dsim_reg_set_lanes(id, lanes, 0);
	dsim_reg_set_esc_clk_on_lane(id, 0, lanes);
	dsim_reg_set_clocks(id, NULL, NULL, 0);

	if (ret < 0) {
		dsim_to_regs_param(dsim, &regs);
		__dsim_dump(id, &regs);
	}

	/* 3.4 sw reset */
	dsim_reg_sw_reset(id);

	return ret;
}

int dsim_reg_get_int_and_clear(u32 id)
{
	u32 val;

	val = dsim_read(id, DSIM_INTSRC);
	dsim_reg_clear_int(id, val);

	return val;
}

void dsim_reg_clear_int(u32 id, u32 int_src)
{
	dsim_write(id, DSIM_INTSRC, int_src);
}

void dsim_reg_wr_tx_header(u32 id, u32 d_id, unsigned long d0, u32 d1, u32 bta)
{
	u32 val = DSIM_PKTHDR_BTA_TYPE(bta) | DSIM_PKTHDR_ID(d_id) |
		DSIM_PKTHDR_DATA0(d0) | DSIM_PKTHDR_DATA1(d1);

	dsim_write_mask(id, DSIM_PKTHDR, val, DSIM_PKTHDR_DATA);
}

void dsim_reg_wr_tx_payload(u32 id, u32 payload)
{
	dsim_write(id, DSIM_PAYLOAD, payload);
}

u32 dsim_reg_header_fifo_is_empty(u32 id)
{
	return dsim_read_mask(id, DSIM_FIFOCTRL, DSIM_FIFOCTRL_EMPTY_PH_SFR);
}

u32 dsim_reg_payload_fifo_is_empty(u32 id)
{

	return dsim_read_mask(id, DSIM_FIFOCTRL, DSIM_FIFOCTRL_EMPTY_PL_SFR);
}

bool dsim_reg_is_writable_ph_fifo_state(u32 id, u32 cmd_cnt)
{
	u32 val = dsim_read(id, DSIM_FIFOCTRL);

	val = DSIM_FIFOCTRL_NUMBER_OF_PH_SFR_GET(val);
	val += cmd_cnt;

	if (val < DSIM_PH_FIFOCTRL_THRESHOLD)
		return true;
	else
		return false;
}

u32 dsim_reg_get_rx_fifo(u32 id)
{
	return dsim_read(id, DSIM_RXFIFO);
}

u32 dsim_reg_rx_fifo_is_empty(u32 id)
{
	return dsim_read_mask(id, DSIM_FIFOCTRL, DSIM_FIFOCTRL_EMPTY_RX);
}

int dsim_reg_get_linecount(u32 id, u32 mode)
{
	u32 val = 0;

	if (mode == DECON_VIDEO_MODE) {
		val = dsim_read(id, DSIM_LINK_STATUS0);
		return DSIM_LINK_STATUS0_VM_LINE_CNT_GET(val);
	} else if (mode == DECON_MIPI_COMMAND_MODE) {
		val = dsim_read(id, DSIM_LINK_STATUS1);
		return DSIM_LINK_STATUS1_CMD_TRANSF_CNT_GET(val);
	}

	return -EINVAL;
}

int dsim_reg_rx_err_handler(u32 id, u32 rx_fifo)
{
	int ret = 0;
	u32 err_bit = rx_fifo >> 8; /* Error_Range [23:8] */

	if ((err_bit & MIPI_DSI_ERR_BIT_MASK) == 0) {
		dsim_dbg("dsim%d, Non error reporting format (rx_fifo=0x%x)\n",
				id, rx_fifo);
		return ret;
	}

	/* Parse error report bit*/
	if (err_bit & MIPI_DSI_ERR_SOT)
		dsim_err("SoT error!\n");
	if (err_bit & MIPI_DSI_ERR_SOT_SYNC)
		dsim_err("SoT sync error!\n");
	if (err_bit & MIPI_DSI_ERR_EOT_SYNC)
		dsim_err("EoT error!\n");
	if (err_bit & MIPI_DSI_ERR_ESCAPE_MODE_ENTRY_CMD)
		dsim_err("Escape mode entry command error!\n");
	if (err_bit & MIPI_DSI_ERR_LOW_POWER_TRANSMIT_SYNC)
		dsim_err("Low-power transmit sync error!\n");
	if (err_bit & MIPI_DSI_ERR_HS_RECEIVE_TIMEOUT)
		dsim_err("HS receive timeout error!\n");
	if (err_bit & MIPI_DSI_ERR_FALSE_CONTROL)
		dsim_err("False control error!\n");
	if (err_bit & MIPI_DSI_ERR_ECC_SINGLE_BIT)
		dsim_err("ECC error, single-bit(detected and corrected)!\n");
	if (err_bit & MIPI_DSI_ERR_ECC_MULTI_BIT)
		dsim_err("ECC error, multi-bit(detected, not corrected)!\n");
	if (err_bit & MIPI_DSI_ERR_CHECKSUM)
		dsim_err("Checksum error(long packet only)!\n");
	if (err_bit & MIPI_DSI_ERR_DATA_TYPE_NOT_RECOGNIZED)
		dsim_err("DSI data type not recognized!\n");
	if (err_bit & MIPI_DSI_ERR_VCHANNEL_ID_INVALID)
		dsim_err("DSI VC ID invalid!\n");
	if (err_bit & MIPI_DSI_ERR_INVALID_TRANSMIT_LENGTH)
		dsim_err("Invalid transmission length!\n");

	dsim_err("dsim%d, (rx_fifo=0x%x) Check DPHY values about HS clk.\n",
			id, rx_fifo);
	return -EINVAL;
}

void dsim_reg_enable_shadow_read(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	dsim_write_mask(id, DSIM_SFR_CTRL, val,
				DSIM_SFR_CTRL_SHADOW_REG_READ_EN);
}

void dsim_reg_function_reset(u32 id)
{
	u32 cnt = 1000;
	u32 state;

	dsim_write_mask(id, DSIM_SWRST, ~0,  DSIM_SWRST_FUNCRST);

	do {
		state = dsim_read(id, DSIM_SWRST) & DSIM_SWRST_FUNCRST;
		cnt--;
		udelay(10);
	} while (state && cnt);

	if (!cnt)
		dsim_err("%s is timeout.\n", __func__);

}

/* Set porch and resolution to support Partial update */
void dsim_reg_set_partial_update(u32 id, struct exynos_panel_info *lcd_info)
{
	dsim_reg_set_resol(id, lcd_info);
	dsim_reg_set_threshold(id, lcd_info->xres);
	dsim_reg_set_porch(id, lcd_info);
	dsim_reg_set_num_of_transfer(id, lcd_info->yres);
}

void dsim_reg_set_mres(u32 id, struct exynos_panel_info *lcd_info)
{
	u32 threshold;
	u32 num_of_slice;
	u32 num_of_transfer;
	int idx;

	if (lcd_info->mode != DECON_MIPI_COMMAND_MODE) {
		dsim_info("%s: mode[%d] doesn't support multi resolution\n",
				__func__, lcd_info->mode);
		return;
	}

	idx = lcd_info->mres_mode;
	dsim_reg_set_cm_underrun_lp_ref(id, lcd_info->cmd_underrun_cnt[idx]);

	if (lcd_info->dsc.en) {
		threshold = lcd_info->dsc.enc_sw * lcd_info->dsc.slice_num;
		/* use 1-line transfer only */
		num_of_transfer = lcd_info->yres;
	} else {
		threshold = lcd_info->xres;
		num_of_transfer = lcd_info->xres * lcd_info->yres / threshold;
	}

	dsim_reg_set_threshold(id, threshold);
	dsim_reg_set_resol(id, lcd_info);
	dsim_reg_set_porch(id, lcd_info);
	dsim_reg_set_num_of_transfer(id, num_of_transfer);

	dsim_reg_enable_dsc(id, lcd_info->dsc.en);
	if (lcd_info->dsc.en) {
		dsim_dbg("%s: dsc configuration is set\n", __func__);
		dsim_reg_set_num_of_slice(id, lcd_info->dsc.slice_num);
		dsim_reg_set_multi_slice(id, lcd_info); /* multi slice */
		dsim_reg_set_size_of_slice(id, lcd_info);

		dsim_reg_get_num_of_slice(id, &num_of_slice);
		dsim_dbg("dsim%d: number of DSC slice(%d)\n", id, num_of_slice);
		dsim_reg_print_size_of_slice(id);
	}
}

void dsim_reg_set_bist(u32 id, u32 en)
{
	if (en) {
		dsim_reg_set_bist_te_interval(id, 4505);
		dsim_reg_set_bist_mode(id, DSIM_GRAY_GRADATION);
		dsim_reg_enable_bist_pattern_move(id, true);
		dsim_reg_enable_bist(id, en);
	}
}

void dsim_reg_set_cmd_transfer_mode(u32 id, u32 lp)
{
	u32 val = lp ? ~0 : 0;

	dsim_write_mask(id, DSIM_ESCMODE, val, DSIM_ESCMODE_CMD_LPDT);
}

/* Only for command mode */
void dsim_reg_set_dphy_freq_hopping(u32 id, u32 p, u32 m, u32 k, u32 en)
{
	/* This is not supported in exynos3830 */
}

#define PREFIX_LEN	40
#define ROW_LEN		32
static void dsim_print_hex_dump(void __iomem *regs, const void *buf, size_t len)
{
	char prefix_buf[PREFIX_LEN];
	unsigned long p;
	size_t i, row;

	for (i = 0; i < len; i += ROW_LEN) {
		p = buf - regs + i;

		if (len - i < ROW_LEN)
			row = len - i;
		else
			row = ROW_LEN;

		snprintf(prefix_buf, sizeof(prefix_buf), "[%08lX] ", p);
		print_hex_dump(KERN_NOTICE, prefix_buf, DUMP_PREFIX_NONE,
				32, 4, buf + i, row, false);
	}
}

void __dsim_dump(u32 id, struct dsim_regs *regs)
{
	/* change to updated register read mode (meaning: SHADOW in DECON) */
	dsim_info("=== DSIM %d LINK SFR DUMP ===\n", id);
	dsim_reg_enable_shadow_read(id, 0);
	dsim_print_hex_dump(regs->regs, regs->regs + 0x0000, 0x114);

	dsim_info("=== DSIM %d DPHY SFR DUMP ===\n", id);

	/* DPHY dump */
	dsim_info("-[BIAS_CTRL : offset + 0x0400]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x0400, 0x20);

	dsim_info("-[DTB_CTRL : offset + 0x0800]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x0800, 0x20);

	dsim_info("-[PLL_CTRL : offset + 0x0C00]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x0C00, 0x40);

	dsim_info("-[DPHY_ACTRL_MC : offset + 0x1000]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x1000, 0x10);

	dsim_info("-[DPHY_DCTRL_MC : offset + 0x1080]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x1080, 0x40);

	dsim_info("-[DPHY_ACTRL_MD0 : offset + 0x1400]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x1400, 0x20);

	dsim_info("-[DPHY_DCTRL_MD0 : offset + 0x1480]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x1480, 0x30);

	dsim_info("-[DPHY_ACTRL_MD1 : offset + 0x1800]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x1800, 0x10);

	dsim_info("-[DPHY_DCTRL_MD1 : offset + 0x1880]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x1880, 0x30);

	dsim_info("-[DPHY_ACTRL_MD2 : offset + 0x1C00]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x1C00, 0x10);

	dsim_info("-[DPHY_DCTRL_MD2 : offset + 0x1C80]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x1C80, 0x30);

	dsim_info("-[DPHY_ACTRL_MD3 : offset + 0x2000]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x2000, 0x10);

	dsim_info("-[DPHY_DCTRL_MD3 : offset + 0x2080]-\n");
	dsim_print_hex_dump(regs->phy_regs, regs->phy_regs + 0x2080, 0x30);

	/* restore to avoid size mismatch (possible config error at DECON) */
	dsim_reg_enable_shadow_read(id, 1);
}
