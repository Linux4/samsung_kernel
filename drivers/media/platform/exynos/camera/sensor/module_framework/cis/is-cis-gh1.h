/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_GH1_H
#define IS_CIS_GH1_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_GH1_MAX_WIDTH		(7296)
#define SENSOR_GH1_MAX_HEIGHT		(5472)

#define SENSOR_GH1_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_GH1_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_GH1_COARSE_INTEGRATION_TIME_MIN              0x4
#define SENSOR_GH1_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x5
#define SENSOR_GH1_POST_INIT_SETTING_MAX	30

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_gh1_mode_enum {
		SENSOR_GH1_7296X5472_15FPS = 0,
		SENSOR_GH1_3648X2736_30FPS,
		SENSOR_GH1_3968X2232_30FPS,
		SENSOR_GH1_3968X2232_60FPS,
		SENSOR_GH1_1984X1116_240FPS,
		SENSOR_GH1_1824X1368_30FPS,
		SENSOR_GH1_1984X1116_30FPS,
		SENSOR_GH1_912X684_120FPS,
		SENSOR_GH1_2944x2208_30FPS,
		SENSOR_GH1_3216x1808_30FPS,
};
#endif
