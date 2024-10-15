/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_OV08A10_H
#define IS_CIS_OV08A10_H

#include "is-cis.h"

#define MAX_GAIN_INDEX 63
#define CODE_GAIN_INDEX 0
#define PERMILE_GAIN_INDEX 1

const u32 sensor_ov08a10_analog_gain[][MAX_GAIN_INDEX + 1] = {
	{0x100, 1000},
	{0x110, 1063},
	{0x120, 1125},
	{0x130, 1188},
	{0x140, 1250},
	{0x150, 1313},
	{0x160, 1375},
	{0x170, 1438},
	{0x180, 1500},
	{0x190, 1563},
	{0x1A0, 1625},
	{0x1B0, 1688},
	{0x1C0, 1750},
	{0x1D0, 1813},
	{0x1E0, 1875},
	{0x1F0, 1938},
	{0x200, 2000},
	{0x220, 2125},
	{0x240, 2250},
	{0x260, 2375},
	{0x280, 2500},
	{0x2A0, 2625},
	{0x2B0, 2750},
	{0x2C0, 2875},
	{0x300, 3000},
	{0x320, 3125},
	{0x340, 3250},
	{0x360, 3375},
	{0x380, 3500},
	{0x3A0, 3625},
	{0x3C0, 3750},
	{0x3E0, 3875},
	{0x400, 4000},
	{0x440, 4250},
	{0x480, 4500},
	{0x4C0, 4750},
	{0x500, 5000},
	{0x540, 5250},
	{0x580, 5500},
	{0x5C0, 5750},
	{0x600, 6000},
	{0x640, 6250},
	{0x680, 6500},
	{0x6C0, 6750},
	{0x700, 7000},
	{0x740, 7250},
	{0x780, 7500},
	{0x7C0, 7750},
	{0x800, 8000},
	{0x880, 8500},
	{0x900, 9000},
	{0x980, 9500},
	{0xA00, 10000},
	{0xA80, 10500},
	{0xB00, 11000},
	{0xB80, 11500},
	{0xC00, 12000},
	{0xC80, 12500},
	{0xD00, 13000},
	{0xD80, 13500},
	{0xE00, 14000},
	{0xE80, 14500},
	{0xF00, 15000},
	{0xF80, 15500},
};

enum sensor_ov08a10_mode_enum {
	SENSOR_OV08A10_3264x2448_30FPS = 0,
	SENSOR_OV08A10_3264x1836_30FPS,
	SENSOR_OV08A10_1632x1224_60FPS,
	SENSOR_OV08A10_MODE_MAX,
};

struct sensor_ov08a10_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_ov08a10_reg_addr = {
	.fll = 0x380E,
	.fll_shifter = 0, /* not supported */
	.cit = 0x3501,
	.cit_shifter = 0, /* not supported */
	.again = 0x3508,
	.dgain = 0x350A,
	.group_param_hold = 0x3208,
};

#endif
