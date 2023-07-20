/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_GW2_H
#define IS_CIS_GW2_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_GW2_MAX_WIDTH		(4880)
#define SENSOR_GW2_MAX_HEIGHT		(3660)

/* TODO: Check below values are valid */
#define SENSOR_GW2_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_GW2_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_GW2_COARSE_INTEGRATION_TIME_MIN              0x04
#define SENSOR_GW2_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x04

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_gw2_mode_enum {
	SENSOR_GW2_9280X6944_21FPS = 0,
	SENSOR_GW2_7680X4320_30FPS,
	SENSOR_GW2_4880X3660_30FPS,
	SENSOR_GW2_4880X3660_22FPS,
	SENSOR_GW2_4880X2736_30FPS,
	SENSOR_GW2_4880X2736_22FPS,
	SENSOR_GW2_4640X3472_30FPS,
	SENSOR_GW2_4640X2610_30FPS,
	SENSOR_GW2_1920X1080_120FPS,
	SENSOR_GW2_1920X1080_240FPS,
};

#endif

