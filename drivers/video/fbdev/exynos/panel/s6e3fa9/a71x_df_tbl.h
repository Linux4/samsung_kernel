/*
 * linux/drivers/video/fbdev/exynos/panel/s6e3fa9/a71x_dynamic_freq.h
 *
 * Copyright (c) 2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __A71X_DYNAMIC_FREQ__
#define __A71X_DYNAMIC_FREQ__

#include "../df/dynamic_freq.h"

struct dynamic_freq_range a71x_freq_range_850[] = {
	DEFINE_FREQ_RANGE(0, 0, 2),
};

struct dynamic_freq_range a71x_freq_range_900[] = {
	DEFINE_FREQ_RANGE(0, 0,	3),
};

struct dynamic_freq_range a71x_freq_range_1800[] = {
	DEFINE_FREQ_RANGE(0, 0, 0),
};

struct dynamic_freq_range a71x_freq_range_1900[] = {
	DEFINE_FREQ_RANGE(0, 0, 2),
};

struct dynamic_freq_range a71x_freq_range_wb01[] = {
	DEFINE_FREQ_RANGE(10562, 10576, 1),
	DEFINE_FREQ_RANGE(10577, 10747, 0),
	DEFINE_FREQ_RANGE(10748, 10822, 1),
	DEFINE_FREQ_RANGE(10823, 10838, 0),
};

struct dynamic_freq_range a71x_freq_range_wb02[] = {
	DEFINE_FREQ_RANGE(9662, 9766, 0),
	DEFINE_FREQ_RANGE(9767, 9841, 1),
	DEFINE_FREQ_RANGE(9842, 9938, 0),
};

struct dynamic_freq_range a71x_freq_range_wb03[] = {
	DEFINE_FREQ_RANGE(1162, 1231, 1),
	DEFINE_FREQ_RANGE(1232, 1401, 0),
	DEFINE_FREQ_RANGE(1402, 1476, 1),
	DEFINE_FREQ_RANGE(1477, 1513, 0),
};

struct dynamic_freq_range a71x_freq_range_wb04[] = {
	DEFINE_FREQ_RANGE(1537, 1551, 1),
	DEFINE_FREQ_RANGE(1552, 1722, 0),
	DEFINE_FREQ_RANGE(1723, 1738, 1),
};

struct dynamic_freq_range a71x_freq_range_wb05[] = {
	DEFINE_FREQ_RANGE(4357, 4374, 0),
	DEFINE_FREQ_RANGE(4375, 4445, 1),
	DEFINE_FREQ_RANGE(4446, 4449, 2),
	DEFINE_FREQ_RANGE(4450, 4458, 0),
};

struct dynamic_freq_range a71x_freq_range_wb07[] = {
	DEFINE_FREQ_RANGE(2237, 2323, 0),
	DEFINE_FREQ_RANGE(2324, 2345, 3),
	DEFINE_FREQ_RANGE(2346, 2362, 2),
	DEFINE_FREQ_RANGE(2363, 2398, 1),
	DEFINE_FREQ_RANGE(2399, 2563, 0),
};

struct dynamic_freq_range a71x_freq_range_wb08[] = {
	DEFINE_FREQ_RANGE(2937, 2994, 1),
	DEFINE_FREQ_RANGE(2995, 3088, 0),
};

struct dynamic_freq_range a71x_freq_range_td1[] = {
	DEFINE_FREQ_RANGE(0, 0, 1),
};

struct dynamic_freq_range a71x_freq_range_td2[] = {
	DEFINE_FREQ_RANGE(0, 0, 0),
};

struct dynamic_freq_range a71x_freq_range_td3[] = {
	DEFINE_FREQ_RANGE(0, 0, 1),
};

struct dynamic_freq_range a71x_freq_range_td4[] = {
	DEFINE_FREQ_RANGE(0, 0, 0),
};

struct dynamic_freq_range a71x_freq_range_td5[] = {
	DEFINE_FREQ_RANGE(0, 0, 3),
};

struct dynamic_freq_range a71x_freq_range_td6[] = {
	DEFINE_FREQ_RANGE(0, 0, 1),
};

struct dynamic_freq_range a71x_freq_range_bc0[] = {
	DEFINE_FREQ_RANGE(0, 0, 1),
};

struct dynamic_freq_range a71x_freq_range_bc1[] = {
	DEFINE_FREQ_RANGE(0, 0, 0),
};

struct dynamic_freq_range a71x_freq_range_bc10[] = {
	DEFINE_FREQ_RANGE(0, 0, 0),
};

struct dynamic_freq_range a71x_freq_range_lb01[] = {
	DEFINE_FREQ_RANGE(0, 53, 1),
	DEFINE_FREQ_RANGE(54, 394, 0),
	DEFINE_FREQ_RANGE(395, 544, 1),
	DEFINE_FREQ_RANGE(545, 599, 0),
};

struct dynamic_freq_range a71x_freq_range_lb02[] = {
	DEFINE_FREQ_RANGE(600, 833, 0),
	DEFINE_FREQ_RANGE(834, 983, 1),
	DEFINE_FREQ_RANGE(984, 1199, 0),
};

struct dynamic_freq_range a71x_freq_range_lb03[] = {
	DEFINE_FREQ_RANGE(1200, 1212, 0),
	DEFINE_FREQ_RANGE(1213, 1362, 1),
	DEFINE_FREQ_RANGE(1363, 1702, 0),
	DEFINE_FREQ_RANGE(1703, 1852, 1),
	DEFINE_FREQ_RANGE(1853, 1949, 0),
};

struct dynamic_freq_range a71x_freq_range_lb04[] = {
	DEFINE_FREQ_RANGE(1950, 2003, 1),
	DEFINE_FREQ_RANGE(2004, 2344, 0),
	DEFINE_FREQ_RANGE(2345, 2399, 1),
};


struct dynamic_freq_range a71x_freq_range_lb05[] = {
	DEFINE_FREQ_RANGE(2400, 2458, 0),
	DEFINE_FREQ_RANGE(2459, 2601, 1),
	DEFINE_FREQ_RANGE(2602, 2608, 2),
	DEFINE_FREQ_RANGE(2609, 2649, 0),
};

struct dynamic_freq_range a71x_freq_range_lb07[] = {
	DEFINE_FREQ_RANGE(2750, 2946, 0),
	DEFINE_FREQ_RANGE(2947, 2990, 3),
	DEFINE_FREQ_RANGE(2991, 3025, 2),
	DEFINE_FREQ_RANGE(3026, 3096, 1),
	DEFINE_FREQ_RANGE(3097, 3436, 0),
	DEFINE_FREQ_RANGE(3437, 3449, 3),
};

struct dynamic_freq_range a71x_freq_range_lb08[] = {
	DEFINE_FREQ_RANGE(3450, 3588, 1),
	DEFINE_FREQ_RANGE(3589, 3799, 0),
};

struct dynamic_freq_range a71x_freq_range_lb12[] = {
	DEFINE_FREQ_RANGE(5010, 5116, 1),
	DEFINE_FREQ_RANGE(5117, 5148, 2),
	DEFINE_FREQ_RANGE(5149, 5179, 0),
};

struct dynamic_freq_range a71x_freq_range_lb13[] = {
	DEFINE_FREQ_RANGE(5180, 5279, 0),
};

struct dynamic_freq_range a71x_freq_range_lb14[] = {
	DEFINE_FREQ_RANGE(5280, 5379, 0),
};

struct dynamic_freq_range a71x_freq_range_lb17[] = {
	DEFINE_FREQ_RANGE(5730, 5786, 1),
	DEFINE_FREQ_RANGE(5787, 5818, 2),
	DEFINE_FREQ_RANGE(5819, 5849, 0),
};

struct dynamic_freq_range a71x_freq_range_lb18[] = {
	DEFINE_FREQ_RANGE(5850, 5999, 0),
};

struct dynamic_freq_range a71x_freq_range_lb19[] = {
	DEFINE_FREQ_RANGE(6000, 6141, 1),
	DEFINE_FREQ_RANGE(6142, 6149, 2),
};

struct dynamic_freq_range a71x_freq_range_lb20[] = {
	DEFINE_FREQ_RANGE(6150, 6158, 2),
	DEFINE_FREQ_RANGE(6159, 6449, 0),
};

struct dynamic_freq_range a71x_freq_range_lb21[] = {
	DEFINE_FREQ_RANGE(6450, 6599, 0),
};

struct dynamic_freq_range a71x_freq_range_lb25[] = {
	DEFINE_FREQ_RANGE(8040, 8273, 0),
	DEFINE_FREQ_RANGE(8274, 8423, 1),
	DEFINE_FREQ_RANGE(8424, 8689, 0),
};

struct dynamic_freq_range a71x_freq_range_lb26[] = {
	DEFINE_FREQ_RANGE(8690, 8848, 0),
	DEFINE_FREQ_RANGE(8849, 8991, 1),
	DEFINE_FREQ_RANGE(8992, 8998, 2),
	DEFINE_FREQ_RANGE(8999, 9039, 0),
};

struct dynamic_freq_range a71x_freq_range_lb28[] = {
	DEFINE_FREQ_RANGE(9210, 9398, 0),
	DEFINE_FREQ_RANGE(9399, 9524, 1),
	DEFINE_FREQ_RANGE(9525, 9548, 2),
	DEFINE_FREQ_RANGE(9549, 9659, 0),
};

struct dynamic_freq_range a71x_freq_range_lb29[] = {
	DEFINE_FREQ_RANGE(9660, 9769, 0),
};

struct dynamic_freq_range a71x_freq_range_lb30[] = {
	DEFINE_FREQ_RANGE(9770, 9869, 1),
};

struct dynamic_freq_range a71x_freq_range_lb32[] = {
	DEFINE_FREQ_RANGE(9920, 10031, 0),
	DEFINE_FREQ_RANGE(10032, 10181, 1),
	DEFINE_FREQ_RANGE(10182, 10359, 0),
};

struct dynamic_freq_range a71x_freq_range_lb34[] = {
	DEFINE_FREQ_RANGE(36200, 36273, 1),
	DEFINE_FREQ_RANGE(36274, 36349, 0),
};

struct dynamic_freq_range a71x_freq_range_lb38[] = {
	DEFINE_FREQ_RANGE(37750, 37956, 0),
	DEFINE_FREQ_RANGE(37957, 38009, 3),
	DEFINE_FREQ_RANGE(39010, 38027, 2),
	DEFINE_FREQ_RANGE(38028, 38106, 1),
	DEFINE_FREQ_RANGE(38107, 38249, 0),
};

struct dynamic_freq_range a71x_freq_range_lb39[] = {
	DEFINE_FREQ_RANGE(38250, 38493, 0),
	DEFINE_FREQ_RANGE(38494, 38643, 1),
	DEFINE_FREQ_RANGE(38644, 38649, 0),
};

struct dynamic_freq_range a71x_freq_range_lb40[] = {
	DEFINE_FREQ_RANGE(38650, 38764, 1),
	DEFINE_FREQ_RANGE(38765, 39104, 0),
	DEFINE_FREQ_RANGE(39105, 39136, 3),
	DEFINE_FREQ_RANGE(39137, 39254, 1),
	DEFINE_FREQ_RANGE(39255, 39595, 0),
	DEFINE_FREQ_RANGE(39596, 39634, 3),
	DEFINE_FREQ_RANGE(39635, 39649, 1),
};

struct dynamic_freq_range a71x_freq_range_lb41[] = {
	DEFINE_FREQ_RANGE(39650, 39671, 3),
	DEFINE_FREQ_RANGE(39672, 39765, 1),
	DEFINE_FREQ_RANGE(39766, 40105, 0),
	DEFINE_FREQ_RANGE(40106, 40167, 3),
	DEFINE_FREQ_RANGE(40168, 40255, 1),
	DEFINE_FREQ_RANGE(40256, 40596, 0),
	DEFINE_FREQ_RANGE(40597, 40648, 3),
	DEFINE_FREQ_RANGE(40649, 40667, 2),
	DEFINE_FREQ_RANGE(40668, 40746, 1),
	DEFINE_FREQ_RANGE(40747, 41086, 0),
	DEFINE_FREQ_RANGE(41087, 41130, 3),
	DEFINE_FREQ_RANGE(41131, 41165, 2),
	DEFINE_FREQ_RANGE(41166, 41236, 1),
	DEFINE_FREQ_RANGE(41237, 41576, 0),
	DEFINE_FREQ_RANGE(41577, 41589, 3),
};

struct dynamic_freq_range a71x_freq_range_lb42[] = {
	DEFINE_FREQ_RANGE(41590, 41829, 0),
	DEFINE_FREQ_RANGE(41830, 41885, 1),
	DEFINE_FREQ_RANGE(41886, 41979, 2),
	DEFINE_FREQ_RANGE(41980, 42319, 0),
	DEFINE_FREQ_RANGE(42320, 42383, 1),
	DEFINE_FREQ_RANGE(42384, 42469, 2),
	DEFINE_FREQ_RANGE(42470, 42810, 0),
	DEFINE_FREQ_RANGE(42811, 42881, 1),
	DEFINE_FREQ_RANGE(42882, 42960, 2),
	DEFINE_FREQ_RANGE(42961, 43300, 0),
	DEFINE_FREQ_RANGE(43301, 43380, 1),
	DEFINE_FREQ_RANGE(43381, 43450, 2),
	DEFINE_FREQ_RANGE(43451, 43589, 0),
};

struct dynamic_freq_range a71x_freq_range_lb48[] = {
	DEFINE_FREQ_RANGE(55240, 55450, 0),
	DEFINE_FREQ_RANGE(55451, 55530, 1),
	DEFINE_FREQ_RANGE(55531, 55600, 2),
	DEFINE_FREQ_RANGE(55601, 55940, 0),
	DEFINE_FREQ_RANGE(55941, 56028, 1),
	DEFINE_FREQ_RANGE(56029, 56090, 2),
	DEFINE_FREQ_RANGE(56091, 56430, 0),
	DEFINE_FREQ_RANGE(56431, 56526, 1),
	DEFINE_FREQ_RANGE(56527, 56576, 2),
	DEFINE_FREQ_RANGE(56577, 56580, 3),
	DEFINE_FREQ_RANGE(56581, 56739, 0),
};

struct dynamic_freq_range a71x_freq_range_lb66[] = {
	DEFINE_FREQ_RANGE(66436, 66489, 1),
	DEFINE_FREQ_RANGE(66490, 66830, 0),
	DEFINE_FREQ_RANGE(66831, 66980, 1),
	DEFINE_FREQ_RANGE(66981, 67320, 0),
	DEFINE_FREQ_RANGE(67321, 67328, 3),
	DEFINE_FREQ_RANGE(67329, 67335, 1),
};

struct dynamic_freq_range a71x_freq_range_lb71[] = {
	DEFINE_FREQ_RANGE(68586, 68713, 0),
	DEFINE_FREQ_RANGE(68714, 68816, 1),
	DEFINE_FREQ_RANGE(68817, 68863, 2),
	DEFINE_FREQ_RANGE(68864, 68935, 0),
};

struct df_freq_tbl_info a71x_dynamic_freq_set[FREQ_RANGE_MAX] = {
	[FREQ_RANGE_850] = DEFINE_FREQ_SET(a71x_freq_range_850),
	[FREQ_RANGE_900] = DEFINE_FREQ_SET(a71x_freq_range_900),
	[FREQ_RANGE_1800] = DEFINE_FREQ_SET(a71x_freq_range_1800),
	[FREQ_RANGE_1900] = DEFINE_FREQ_SET(a71x_freq_range_1900),
	[FREQ_RANGE_WB01] = DEFINE_FREQ_SET(a71x_freq_range_wb01),
	[FREQ_RANGE_WB02] = DEFINE_FREQ_SET(a71x_freq_range_wb02),
	[FREQ_RANGE_WB03] = DEFINE_FREQ_SET(a71x_freq_range_wb03),
	[FREQ_RANGE_WB04] = DEFINE_FREQ_SET(a71x_freq_range_wb04),
	[FREQ_RANGE_WB05] = DEFINE_FREQ_SET(a71x_freq_range_wb05),
	[FREQ_RANGE_WB07] = DEFINE_FREQ_SET(a71x_freq_range_wb07),
	[FREQ_RANGE_WB08] = DEFINE_FREQ_SET(a71x_freq_range_wb08),
	[FREQ_RANGE_TD1] = DEFINE_FREQ_SET(a71x_freq_range_td1),
	[FREQ_RANGE_TD2] = DEFINE_FREQ_SET(a71x_freq_range_td2),
	[FREQ_RANGE_TD3] = DEFINE_FREQ_SET(a71x_freq_range_td3),
	[FREQ_RANGE_TD4] = DEFINE_FREQ_SET(a71x_freq_range_td4),
	[FREQ_RANGE_TD5] = DEFINE_FREQ_SET(a71x_freq_range_td5),
	[FREQ_RANGE_TD6] = DEFINE_FREQ_SET(a71x_freq_range_td6),
	[FREQ_RANGE_BC0] = DEFINE_FREQ_SET(a71x_freq_range_bc0),
	[FREQ_RANGE_BC1] = DEFINE_FREQ_SET(a71x_freq_range_bc1),
	[FREQ_RANGE_BC10] = DEFINE_FREQ_SET(a71x_freq_range_bc10),
	[FREQ_RANGE_LB01] = DEFINE_FREQ_SET(a71x_freq_range_lb01),
	[FREQ_RANGE_LB02] = DEFINE_FREQ_SET(a71x_freq_range_lb02),
	[FREQ_RANGE_LB03] = DEFINE_FREQ_SET(a71x_freq_range_lb03),
	[FREQ_RANGE_LB04] = DEFINE_FREQ_SET(a71x_freq_range_lb04),
	[FREQ_RANGE_LB05] = DEFINE_FREQ_SET(a71x_freq_range_lb05),
	[FREQ_RANGE_LB07] = DEFINE_FREQ_SET(a71x_freq_range_lb07),
	[FREQ_RANGE_LB08] = DEFINE_FREQ_SET(a71x_freq_range_lb08),
	[FREQ_RANGE_LB12] = DEFINE_FREQ_SET(a71x_freq_range_lb12),
	[FREQ_RANGE_LB13] = DEFINE_FREQ_SET(a71x_freq_range_lb13),
	[FREQ_RANGE_LB14] = DEFINE_FREQ_SET(a71x_freq_range_lb14),
	[FREQ_RANGE_LB17] = DEFINE_FREQ_SET(a71x_freq_range_lb17),
	[FREQ_RANGE_LB18] = DEFINE_FREQ_SET(a71x_freq_range_lb18),
	[FREQ_RANGE_LB19] = DEFINE_FREQ_SET(a71x_freq_range_lb19),
	[FREQ_RANGE_LB20] = DEFINE_FREQ_SET(a71x_freq_range_lb20),
	[FREQ_RANGE_LB21] = DEFINE_FREQ_SET(a71x_freq_range_lb21),
	[FREQ_RANGE_LB25] = DEFINE_FREQ_SET(a71x_freq_range_lb25),
	[FREQ_RANGE_LB26] = DEFINE_FREQ_SET(a71x_freq_range_lb26),
	[FREQ_RANGE_LB28] = DEFINE_FREQ_SET(a71x_freq_range_lb28),
	[FREQ_RANGE_LB29] = DEFINE_FREQ_SET(a71x_freq_range_lb29),
	[FREQ_RANGE_LB30] = DEFINE_FREQ_SET(a71x_freq_range_lb30),
	[FREQ_RANGE_LB32] = DEFINE_FREQ_SET(a71x_freq_range_lb32),
	[FREQ_RANGE_LB34] = DEFINE_FREQ_SET(a71x_freq_range_lb34),
	[FREQ_RANGE_LB38] = DEFINE_FREQ_SET(a71x_freq_range_lb38),
	[FREQ_RANGE_LB39] = DEFINE_FREQ_SET(a71x_freq_range_lb39),
	[FREQ_RANGE_LB40] = DEFINE_FREQ_SET(a71x_freq_range_lb40),
	[FREQ_RANGE_LB41] = DEFINE_FREQ_SET(a71x_freq_range_lb41),
	[FREQ_RANGE_LB42] = DEFINE_FREQ_SET(a71x_freq_range_lb42),
	[FREQ_RANGE_LB48] = DEFINE_FREQ_SET(a71x_freq_range_lb48),
	[FREQ_RANGE_LB66] = DEFINE_FREQ_SET(a71x_freq_range_lb66),
	[FREQ_RANGE_LB71] = DEFINE_FREQ_SET(a71x_freq_range_lb71),
};
#endif //__A71X_DYNAMIC_FREQ_TBL__
