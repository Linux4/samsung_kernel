/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_GC08A3_H
#define IS_CIS_GC08A3_H

#include "is-cis.h"

enum sensor_gc08a3_mode_enum {
	SENSOR_GC08A3_3264X2448_30FPS = 0,
	SENSOR_GC08A3_3264X1836_30FPS,
	SENSOR_GC08A3_2880X1980_30FPS,
	SENSOR_GC08A3_2640X1980_30FPS,
	SENSOR_GC08A3_2640X1488_30FPS,
	SENSOR_GC08A3_1632X1224_30FPS,
	SENSOR_GC08A3_1632X916_30FPS,
	SENSOR_GC08A3_1632X1224_60FPS,
	SENSOR_GC08A3_1280X720_90FPS,
	SENSOR_GC08A3_MODE_MAX,
};

struct sensor_gc08a3_private_data {
	const struct sensor_regs global;
	const struct sensor_regs otprom_init;
};

static const struct sensor_reg_addr sensor_gc08a3_reg_addr = {
	.fll = 0x0340,
	.cit = 0x0202,
	.again = 0x0204,
	.dgain = 0x020E,
};
#endif
