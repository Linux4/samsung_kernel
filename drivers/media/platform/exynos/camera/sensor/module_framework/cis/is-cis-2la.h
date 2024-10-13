/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_2LA_H
#define IS_CIS_2LA_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_2LA_MAX_WIDTH		(4032 + 0)
#define SENSOR_2LA_MAX_HEIGHT		(3024 + 0)

#define SENSOR_2LA_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_2LA_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_2LA_COARSE_INTEGRATION_TIME_MIN              0x10
#define SENSOR_2LA_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x30

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_2la_mode_enum {
	SENSOR_2LA_4032X3024_30FPS = 0,
	SENSOR_2LA_4032X2268_60FPS = 1,
	SENSOR_2LA_4032X2268_30FPS = 2,
	SENSOR_2LA_2016X1134_240FPS = 3,
	SENSOR_2LA_2016X1134_120FPS = 4,
	SENSOR_2LA_MODE_MAX,
};

static bool sensor_2la_support_wdr[] = {
	false, //SENSOR_2LA_4032X3024_30FPS = 0,
	false, //SENSOR_2LA_4032X2268_60FPS = 1,
	false, //SENSOR_2LA_4032X2268_30FPS = 2,
	false, //SENSOR_2LA_2016X1134_240FPS = 3,
	false, //SENSOR_2LA_2016X1134_120FPS = 4,
};

enum sensor_2la_load_sram_mode {
	SENSOR_2LA_4032x3024_30FPS_LOAD_SRAM = 0,
	SENSOR_2LA_4032x2268_30FPS_LOAD_SRAM,
	SENSOR_2LA_4032x3024_24FPS_LOAD_SRAM,
	SENSOR_2LA_4032x2268_24FPS_LOAD_SRAM,
	SENSOR_2LA_4032x2268_60FPS_LOAD_SRAM,
	SENSOR_2LA_1008x756_120FPS_LOAD_SRAM,
};

int sensor_2la_cis_stream_on(struct v4l2_subdev *subdev);
int sensor_2la_cis_stream_off(struct v4l2_subdev *subdev);
#ifdef CONFIG_SENSOR_RETENTION_USE
int sensor_2la_cis_retention_crc_check(struct v4l2_subdev *subdev);
int sensor_2la_cis_retention_prepare(struct v4l2_subdev *subdev);
#endif
int sensor_2la_cis_set_lownoise_mode_change(struct v4l2_subdev *subdev);
#endif
