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

#define EXT_CLK_Mhz (26)

#define SENSOR_GC08A3_MAX_WIDTH		(3264 + 0)
#define SENSOR_GC08A3_MAX_HEIGHT		(2448 + 0)

/* TODO: Check below values are valid */
#define SENSOR_GC08A3_FINE_INTEGRATION_TIME_MIN					0x0
#define SENSOR_GC08A3_FINE_INTEGRATION_TIME_MAX					0x0	
#define SENSOR_GC08A3_COARSE_INTEGRATION_TIME_MIN				0x10
#define SENSOR_GC08A3_COARSE_INTEGRATION_TIME_MAX_MARGIN		0x10

#define SENSOR_GC08A3_EXPOSURE_TIME_MAX						125000 /* 125ms */

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_gc08a3_mode_enum {
	SENSOR_GC08A3_3264X2448_30FPS = 0,
	SENSOR_GC08A3_3264X1836_30FPS,
	SENSOR_GC08A3_2880X1980_30FPS,
	SENSOR_GC08A3_2640X1980_30FPS,
	SENSOR_GC08A3_2640X1488_30FPS,
	SENSOR_GC08A3_1632X1224_60FPS,
	SENSOR_GC08A3_1632X1224_30FPS,
	SENSOR_GC08A3_1632X916_30FPS,
};
#endif
