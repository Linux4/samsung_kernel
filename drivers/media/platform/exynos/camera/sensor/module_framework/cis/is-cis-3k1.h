/* SPDX-License-Identifier: GPL-2.0-or-later */
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

#ifndef IS_CIS_3K1_H
#define IS_CIS_3K1_H

#include "is-cis.h"

#define EXT_CLK_Khz (19200)

/* FIXME */
#define SENSOR_3K1_MAX_WIDTH		(3648 + 0)
#define SENSOR_3K1_MAX_HEIGHT		(2736 + 0)

#define SENSOR_3K1_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_3K1_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_3K1_COARSE_INTEGRATION_TIME_MIN              0x6
#define SENSOR_3K1_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x13

#define USE_GROUP_PARAM_HOLD	(0)

#define CAMERA_3K1_MIRROR_FLIP

#define CAMERA_REAR2_SENSOR_SHIFT_CROP	1

#define CROP_SHIFT_ADDR_X	0x029E
#define CROP_SHIFT_ADDR_Y	0x02A0
#define CROP_SHIFT_DELTA_ALIGN_X	8
#define CROP_SHIFT_DELTA_ALIGN_Y	32
#define CROP_SHIFT_DELTA_MAX_X	88
#define CROP_SHIFT_DELTA_MIN_X	-88
#define CROP_SHIFT_DELTA_MAX_Y	64
#define CROP_SHIFT_DELTA_MIN_Y	-64

enum is_efs_state {
	IS_EFS_STATE_READ,
};

struct is_efs_info {
	unsigned long	efs_state;
	s16 crop_shift_delta_x;
	s16 crop_shift_delta_y;
};

enum sensor_3k1_mode_enum {
	SENSOR_3K1_3648x2736_60FPS = 0,
	SENSOR_3K1_3648x2052_60FPS = 1,
	SENSOR_3K1_3328x1872_60FPS = 2,
	SENSOR_3K1_2800x2100_31FPS = 3,
	SENSOR_3K1_1824x1368_120FPS = 4,
	SENSOR_3K1_1824x1368_30FPS = 5,
	SENSOR_3K1_3840x2880_60FPS = 6,
	SENSOR_3K1_MODE_MAX,
};

struct is_start_pos_info {
	u16 x_start;
	u16 y_start;
} start_pos_infos[SENSOR_3K1_MODE_MAX];

#endif
