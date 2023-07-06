/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fa9/s6e3fa9_c1_dynamic_freq.h
 *
 * Copyright (c) 2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef __S6E3FA9_C1_DYNAMIC_FREQ_TABLE__
#define __S6E3FA9_C1_DYNAMIC_FREQ_TABLE__

#include "../df/dynamic_freq.h"

struct dynamic_freq_range c1_freq_range_850[] = {
	DEFINE_FREQ_RANGE(0, 0, 1, 0),
};

struct dynamic_freq_range c1_freq_range_900[] = {
	DEFINE_FREQ_RANGE(0, 0, 2, 0),
};

struct dynamic_freq_range c1_freq_range_1800[] = {
	DEFINE_FREQ_RANGE(0, 0, 1, 0),
};

struct dynamic_freq_range c1_freq_range_1900[] = {
	DEFINE_FREQ_RANGE(0, 0,	1, 0),
};

struct dynamic_freq_range c1_freq_range_wb01[] = {
	DEFINE_FREQ_RANGE(10562, 10658, 0, 0),
	DEFINE_FREQ_RANGE(10659, 10733, 1, 0),
	DEFINE_FREQ_RANGE(10734, 10838, 0, 0),
};

struct dynamic_freq_range c1_freq_range_wb02[] = {
	DEFINE_FREQ_RANGE(9662, 9738, 1, 0),
	DEFINE_FREQ_RANGE(9739, 9912, 0, 0),
	DEFINE_FREQ_RANGE(9913, 9938, 1, 0),
};

struct dynamic_freq_range c1_freq_range_wb03[] = {
	DEFINE_FREQ_RANGE(1162, 1291, 0, 0),
	DEFINE_FREQ_RANGE(1292, 1366, 1, 0),
	DEFINE_FREQ_RANGE(1367, 1513, 0, 0),
};

struct dynamic_freq_range c1_freq_range_wb04[] = {
	DEFINE_FREQ_RANGE(1537, 1633, 0, 0),
	DEFINE_FREQ_RANGE(1634, 1708, 1, 0),
	DEFINE_FREQ_RANGE(1709, 1738, 0, 0),
};

struct dynamic_freq_range c1_freq_range_wb05[] = {
	DEFINE_FREQ_RANGE(4357, 4439, 0, 0),
	DEFINE_FREQ_RANGE(4440, 4458, 1, 0),
};

struct dynamic_freq_range c1_freq_range_wb07[] = {
	DEFINE_FREQ_RANGE(2237, 2271, 0, 0),
	DEFINE_FREQ_RANGE(2272, 2345, 2, 0),
	DEFINE_FREQ_RANGE(2346, 2519, 0, 0),
	DEFINE_FREQ_RANGE(2520, 2563, 2, 0),
};

struct dynamic_freq_range c1_freq_range_wb08[] = {
	DEFINE_FREQ_RANGE(2937, 2988, 0, 0),
	DEFINE_FREQ_RANGE(2989, 3056, 1, 0),
	DEFINE_FREQ_RANGE(3057, 3063, 2, 0),
	DEFINE_FREQ_RANGE(3064, 3088, 0, 0),
};


struct dynamic_freq_range c1_freq_range_td1[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dynamic_freq_range c1_freq_range_td2[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dynamic_freq_range c1_freq_range_td3[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dynamic_freq_range c1_freq_range_td4[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dynamic_freq_range c1_freq_range_td5[] = {
	DEFINE_FREQ_RANGE(0, 0, 2, 0),
};

struct dynamic_freq_range c1_freq_range_td6[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dynamic_freq_range c1_freq_range_bc0[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};

struct dynamic_freq_range c1_freq_range_bc1[] = {
	DEFINE_FREQ_RANGE(0, 0, 2, 0),
};

struct dynamic_freq_range c1_freq_range_bc10[] = {
	DEFINE_FREQ_RANGE(0, 0, 0, 0),
};


struct dynamic_freq_range c1_freq_range_lb01[] = {
	DEFINE_FREQ_RANGE(0, 217, 0, 0),
	DEFINE_FREQ_RANGE(218, 367, 1, 0),
	DEFINE_FREQ_RANGE(368, 599, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb02[] = {
	DEFINE_FREQ_RANGE(600, 627, 0, 0),
	DEFINE_FREQ_RANGE(628, 777, 1, 0),
	DEFINE_FREQ_RANGE(778, 1124, 0, 0),
	DEFINE_FREQ_RANGE(1125, 1199, 1, 0),
};

struct dynamic_freq_range c1_freq_range_lb03[] = {
	DEFINE_FREQ_RANGE(1200, 1482, 0, 0),
	DEFINE_FREQ_RANGE(1483, 1632, 1, 0),
	DEFINE_FREQ_RANGE(1633, 1949, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb04[] = {
	DEFINE_FREQ_RANGE(1950, 2167, 0, 0),
	DEFINE_FREQ_RANGE(2168, 2317, 1, 0),
	DEFINE_FREQ_RANGE(2318, 2399, 0, 0),
};


struct dynamic_freq_range c1_freq_range_lb05[] = {
	DEFINE_FREQ_RANGE(2400, 2589, 0, 0),
	DEFINE_FREQ_RANGE(2590, 2649, 1, 0),
};

struct dynamic_freq_range c1_freq_range_lb07[] = {
	DEFINE_FREQ_RANGE(2750, 2842, 0, 0),
	DEFINE_FREQ_RANGE(2843, 2990, 2, 0),
	DEFINE_FREQ_RANGE(2991, 3339, 0, 0),
	DEFINE_FREQ_RANGE(3340, 3449, 2, 0),
};

struct dynamic_freq_range c1_freq_range_lb08[] = {
	DEFINE_FREQ_RANGE(3450, 3577, 0, 0),
	DEFINE_FREQ_RANGE(3578, 3712, 1, 0),
	DEFINE_FREQ_RANGE(3713, 3727, 2, 0),
	DEFINE_FREQ_RANGE(3728, 3799, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb12[] = {
	DEFINE_FREQ_RANGE(5010, 5107, 0, 0),
	DEFINE_FREQ_RANGE(5108, 5179, 1, 0),
};

struct dynamic_freq_range c1_freq_range_lb13[] = {
	DEFINE_FREQ_RANGE(5180, 5257, 1, 0),
	DEFINE_FREQ_RANGE(5258, 5279, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb14[] = {
	DEFINE_FREQ_RANGE(5280, 5379, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb17[] = {
	DEFINE_FREQ_RANGE(5730, 5777, 0, 0),
	DEFINE_FREQ_RANGE(5778, 5849, 1, 0),
};

struct dynamic_freq_range c1_freq_range_lb18[] = {
	DEFINE_FREQ_RANGE(5850, 5999, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb19[] = {
	DEFINE_FREQ_RANGE(6000, 6129, 0, 0),
	DEFINE_FREQ_RANGE(6130, 6149, 1, 0),
};

struct dynamic_freq_range c1_freq_range_lb20[] = {
	DEFINE_FREQ_RANGE(6150, 6274, 1, 0),
	DEFINE_FREQ_RANGE(6275, 6449, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb21[] = {
	DEFINE_FREQ_RANGE(6450, 6490, 1, 0),
	DEFINE_FREQ_RANGE(6491, 6599, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb25[] = {
	DEFINE_FREQ_RANGE(8040, 8067, 0, 0),
	DEFINE_FREQ_RANGE(8068, 8217, 1, 0),
	DEFINE_FREQ_RANGE(8218, 8564, 0, 0),
	DEFINE_FREQ_RANGE(8565, 8689, 1, 0),
};

struct dynamic_freq_range c1_freq_range_lb26[] = {
	DEFINE_FREQ_RANGE(8690, 8979, 0, 0),
	DEFINE_FREQ_RANGE(8980, 9039, 1, 0),
};

struct dynamic_freq_range c1_freq_range_lb28[] = {
	DEFINE_FREQ_RANGE(9210, 9514, 0, 0),
	DEFINE_FREQ_RANGE(9515, 9659, 1, 0),
};

struct dynamic_freq_range c1_freq_range_lb29[] = {
	DEFINE_FREQ_RANGE(9660, 9769, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb30[] = {
	DEFINE_FREQ_RANGE(9770, 9869, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb32[] = {
	DEFINE_FREQ_RANGE(9920, 10249, 0, 0),
	DEFINE_FREQ_RANGE(10250, 10302, 2, 0),
	DEFINE_FREQ_RANGE(10303, 10359, 1, 0),
};

struct dynamic_freq_range c1_freq_range_lb34[] = {
	DEFINE_FREQ_RANGE(36200, 36349, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb38[] = {
	DEFINE_FREQ_RANGE(37750, 37844, 0, 0),
	DEFINE_FREQ_RANGE(37845, 37861, 1, 0),
	DEFINE_FREQ_RANGE(37862, 37994, 2, 0),
	DEFINE_FREQ_RANGE(37995, 38249, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb39[] = {
	DEFINE_FREQ_RANGE(38250, 38279, 0, 0),
	DEFINE_FREQ_RANGE(38280, 38429, 1, 0),
	DEFINE_FREQ_RANGE(38430, 38649, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb40[] = {
	DEFINE_FREQ_RANGE(38650, 38957, 0, 0),
	DEFINE_FREQ_RANGE(38958, 39064, 1, 0),
	DEFINE_FREQ_RANGE(39065, 39107, 2, 0),
	DEFINE_FREQ_RANGE(39108, 39454, 0, 0),
	DEFINE_FREQ_RANGE(39455, 39543, 1, 0),
	DEFINE_FREQ_RANGE(39544, 39604, 2, 0),
	DEFINE_FREQ_RANGE(39605, 39649, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb41[] = {
	DEFINE_FREQ_RANGE(39650, 39987, 0, 0),
	DEFINE_FREQ_RANGE(39988, 40021, 1, 0),
	DEFINE_FREQ_RANGE(40022, 40137, 2, 0),
	DEFINE_FREQ_RANGE(40138, 40484, 0, 0),
	DEFINE_FREQ_RANGE(40485, 40501, 1, 0),
	DEFINE_FREQ_RANGE(40502, 40634, 2, 0),
	DEFINE_FREQ_RANGE(40635, 40982, 0, 0),
	DEFINE_FREQ_RANGE(40983, 41130, 2, 0),
	DEFINE_FREQ_RANGE(41131, 41479, 0, 0),
	DEFINE_FREQ_RANGE(41480, 41589, 2, 0),
};

struct dynamic_freq_range c1_freq_range_lb42[] = {
	DEFINE_FREQ_RANGE(41590, 41842, 0, 0),
	DEFINE_FREQ_RANGE(41843, 41992, 1, 0),
	DEFINE_FREQ_RANGE(41993, 42340, 0, 0),
	DEFINE_FREQ_RANGE(42341, 42490, 1, 0),
	DEFINE_FREQ_RANGE(42491, 42837, 0, 0),
	DEFINE_FREQ_RANGE(42838, 42987, 1, 0),
	DEFINE_FREQ_RANGE(42988, 43335, 0, 0),
	DEFINE_FREQ_RANGE(43336, 43468, 1, 0),
	DEFINE_FREQ_RANGE(43469, 43485, 2, 0),
	DEFINE_FREQ_RANGE(43486, 43589, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb48[] = {
	DEFINE_FREQ_RANGE(55240, 55485, 0, 0),
	DEFINE_FREQ_RANGE(55486, 55618, 1, 0),
	DEFINE_FREQ_RANGE(55619, 55635, 2, 0),
	DEFINE_FREQ_RANGE(55636, 55982, 0, 0),
	DEFINE_FREQ_RANGE(55983, 56097, 1, 0),
	DEFINE_FREQ_RANGE(56098, 56132, 2, 0),
	DEFINE_FREQ_RANGE(56133, 56480, 0, 0),
	DEFINE_FREQ_RANGE(56481, 56576, 1, 0),
	DEFINE_FREQ_RANGE(56577, 56630, 2, 0),
	DEFINE_FREQ_RANGE(56631, 56739, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb66[] = {
	DEFINE_FREQ_RANGE(66436, 66653, 0, 0),
	DEFINE_FREQ_RANGE(66654, 66803, 1, 0),
	DEFINE_FREQ_RANGE(66804, 67150, 0, 0),
	DEFINE_FREQ_RANGE(67151, 67300, 1, 0),
	DEFINE_FREQ_RANGE(67301, 67335, 0, 0),
};

struct dynamic_freq_range c1_freq_range_lb71[] = {
	DEFINE_FREQ_RANGE(68586, 68808, 0, 0),
	DEFINE_FREQ_RANGE(68809, 68935, 1, 0),
};

struct dynamic_freq_range c1_freq_range_n005[] = {
	DEFINE_FREQ_RANGE(173800, 176680, 0, 0),
	DEFINE_FREQ_RANGE(176681, 178780, 1, 0),
};


struct dynamic_freq_range c1_freq_range_n008[] = {
	DEFINE_FREQ_RANGE(185000, 186640, 0, 0),
	DEFINE_FREQ_RANGE(186641, 189340, 1, 0),
	DEFINE_FREQ_RANGE(189341, 190920, 2, 0),
	DEFINE_FREQ_RANGE(190921, 191980, 0, 0),
};

struct dynamic_freq_range c1_freq_range_n028[] = {
	DEFINE_FREQ_RANGE(151600, 151640, 2, 0),
	DEFINE_FREQ_RANGE(151641, 156780, 0, 0),
	DEFINE_FREQ_RANGE(156781, 160580, 1, 0),
};

struct dynamic_freq_range c1_freq_range_n071[] = {
	DEFINE_FREQ_RANGE(123400, 126980, 0, 0),
	DEFINE_FREQ_RANGE(126981, 130380, 1, 0),
};

struct df_freq_tbl_info c1_dynamic_freq_set[FREQ_RANGE_MAX] = {
	[FREQ_RANGE_850] = DEFINE_FREQ_SET(c1_freq_range_850),
	[FREQ_RANGE_900] = DEFINE_FREQ_SET(c1_freq_range_900),
	[FREQ_RANGE_1800] = DEFINE_FREQ_SET(c1_freq_range_1800),
	[FREQ_RANGE_1900] = DEFINE_FREQ_SET(c1_freq_range_1900),
	[FREQ_RANGE_WB01] = DEFINE_FREQ_SET(c1_freq_range_wb01),
	[FREQ_RANGE_WB02] = DEFINE_FREQ_SET(c1_freq_range_wb02),
	[FREQ_RANGE_WB03] = DEFINE_FREQ_SET(c1_freq_range_wb03),
	[FREQ_RANGE_WB04] = DEFINE_FREQ_SET(c1_freq_range_wb04),
	[FREQ_RANGE_WB05] = DEFINE_FREQ_SET(c1_freq_range_wb05),
	[FREQ_RANGE_WB07] = DEFINE_FREQ_SET(c1_freq_range_wb07),
	[FREQ_RANGE_WB08] = DEFINE_FREQ_SET(c1_freq_range_wb08),
	[FREQ_RANGE_TD1] = DEFINE_FREQ_SET(c1_freq_range_td1),
	[FREQ_RANGE_TD2] = DEFINE_FREQ_SET(c1_freq_range_td2),
	[FREQ_RANGE_TD3] = DEFINE_FREQ_SET(c1_freq_range_td3),
	[FREQ_RANGE_TD4] = DEFINE_FREQ_SET(c1_freq_range_td4),
	[FREQ_RANGE_TD5] = DEFINE_FREQ_SET(c1_freq_range_td5),
	[FREQ_RANGE_TD6] = DEFINE_FREQ_SET(c1_freq_range_td6),
	[FREQ_RANGE_BC0] = DEFINE_FREQ_SET(c1_freq_range_bc0),
	[FREQ_RANGE_BC1] = DEFINE_FREQ_SET(c1_freq_range_bc1),
	[FREQ_RANGE_BC10] = DEFINE_FREQ_SET(c1_freq_range_bc10),
	[FREQ_RANGE_LB01] = DEFINE_FREQ_SET(c1_freq_range_lb01),
	[FREQ_RANGE_LB02] = DEFINE_FREQ_SET(c1_freq_range_lb02),
	[FREQ_RANGE_LB03] = DEFINE_FREQ_SET(c1_freq_range_lb03),
	[FREQ_RANGE_LB04] = DEFINE_FREQ_SET(c1_freq_range_lb04),
	[FREQ_RANGE_LB05] = DEFINE_FREQ_SET(c1_freq_range_lb05),
	[FREQ_RANGE_LB07] = DEFINE_FREQ_SET(c1_freq_range_lb07),
	[FREQ_RANGE_LB08] = DEFINE_FREQ_SET(c1_freq_range_lb08),
	[FREQ_RANGE_LB12] = DEFINE_FREQ_SET(c1_freq_range_lb12),
	[FREQ_RANGE_LB13] = DEFINE_FREQ_SET(c1_freq_range_lb13),
	[FREQ_RANGE_LB14] = DEFINE_FREQ_SET(c1_freq_range_lb14),
	[FREQ_RANGE_LB17] = DEFINE_FREQ_SET(c1_freq_range_lb17),
	[FREQ_RANGE_LB18] = DEFINE_FREQ_SET(c1_freq_range_lb18),
	[FREQ_RANGE_LB19] = DEFINE_FREQ_SET(c1_freq_range_lb19),
	[FREQ_RANGE_LB20] = DEFINE_FREQ_SET(c1_freq_range_lb20),
	[FREQ_RANGE_LB21] = DEFINE_FREQ_SET(c1_freq_range_lb21),
	[FREQ_RANGE_LB25] = DEFINE_FREQ_SET(c1_freq_range_lb25),
	[FREQ_RANGE_LB26] = DEFINE_FREQ_SET(c1_freq_range_lb26),
	[FREQ_RANGE_LB28] = DEFINE_FREQ_SET(c1_freq_range_lb28),
	[FREQ_RANGE_LB29] = DEFINE_FREQ_SET(c1_freq_range_lb29),
	[FREQ_RANGE_LB30] = DEFINE_FREQ_SET(c1_freq_range_lb30),
	[FREQ_RANGE_LB32] = DEFINE_FREQ_SET(c1_freq_range_lb32),
	[FREQ_RANGE_LB34] = DEFINE_FREQ_SET(c1_freq_range_lb34),
	[FREQ_RANGE_LB38] = DEFINE_FREQ_SET(c1_freq_range_lb38),
	[FREQ_RANGE_LB39] = DEFINE_FREQ_SET(c1_freq_range_lb39),
	[FREQ_RANGE_LB40] = DEFINE_FREQ_SET(c1_freq_range_lb40),
	[FREQ_RANGE_LB41] = DEFINE_FREQ_SET(c1_freq_range_lb41),
	[FREQ_RANGE_LB42] = DEFINE_FREQ_SET(c1_freq_range_lb42),
	[FREQ_RANGE_LB48] = DEFINE_FREQ_SET(c1_freq_range_lb48),
	[FREQ_RANGE_LB66] = DEFINE_FREQ_SET(c1_freq_range_lb66),
	[FREQ_RANGE_LB71] = DEFINE_FREQ_SET(c1_freq_range_lb71),
	[FREQ_RANGE_N005] = DEFINE_FREQ_SET(c1_freq_range_n005),
	[FREQ_RANGE_N008] = DEFINE_FREQ_SET(c1_freq_range_n008),
	[FREQ_RANGE_N028] = DEFINE_FREQ_SET(c1_freq_range_n028),
	[FREQ_RANGE_N071] = DEFINE_FREQ_SET(c1_freq_range_n071),
};

#endif
