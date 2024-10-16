/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_GN3_H
#define IS_CIS_GN3_H

#include "is-cis.h"

#define SENSOR_GN3_MAX_WIDTH		(8160 + 0)
#define SENSOR_GN3_MAX_HEIGHT		(6120 + 0)

#define SENSOR_GN3_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_GN3_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_GN3_COARSE_INTEGRATION_TIME_MIN              0x6
#define SENSOR_GN3_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x24

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_gn3_mode_enum {
	SENSOR_GN3_8160X6120_27FPS = 0,
	SENSOR_GN3_4896X3672_30FPS_RAW12 = 1,
	SENSOR_GN3_7680X4320_30FPS = 2,
	SENSOR_GN3_7680X4320_30FPS_MPC = 3,
	SENSOR_GN3_4080X3060_60FPS_RAW12_FRO = 4,
	SENSOR_GN3_4080X3060_60FPS_RAW12 = 5,
	SENSOR_GN3_3328X1872_120FPS = 6,
	SENSOR_GN3_4080X2296_60FPS_RAW12 = 7,
	SENSOR_GN3_2040X1148_240FPS = 8,
	SENSOR_GN3_2040X1148_480FPS_MODE2 = 9,
	SENSOR_GN3_2040X1532_120FPS = 10,
	SENSOR_GN3_2800X2100_60FPS = 11,
	SENSOR_GN3_4080X2720_60FPS_RAW12 = 12,
	SENSOR_GN3_MODE_MAX,
};

/* { max_analog_gain, max_digital_gain, min_coarse_integration_time, max_margin_coarse_integration_time, }, */
struct sensor_common_mode_attr sensor_gn3_common_mode_attr[SENSOR_GN3_MODE_MAX] = {
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_8160X6120_27FPS = 0, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_4896X3672_30FPS_RAW12 = 1, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_7680X4320_30FPS = 2, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_7680X4320_30FPS_MPC = 3, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_4080X3060_60FPS_RAW12_FRO = 4, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_4080X3060_60FPS_RAW12 = 5, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_3328X1872_120FPS = 6, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_4080X2296_60FPS_RAW12 = 7, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_2040X1148_240FPS = 8, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_2040X1148_480FPS_MODE2 = 9, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_2040X1532_120FPS = 10, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_2800X2100_60FPS = 11, */
	{0x200, 0x1000, 0xC, 0xD, }, /* SENSOR_GN3_4080X2720_60FPS_RAW12 = 12, */
};

struct sensor_gn3_specific_mode_attr {
	bool wb_gain_support;
	bool ln_support;
	bool highres_capture_mode;
	enum is_sensor_12bit_state status_12bit;
};

struct sensor_gn3_specific_mode_attr sensor_gn3_spec_mode_attr[SENSOR_GN3_MODE_MAX] = {
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_8160X6120_27FPS = 0, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_4896X3672_30FPS_RAW12 = 1, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_7680X4320_30FPS = 2, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_7680X4320_30FPS_MPC = 3, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_4080X3060_60FPS_RAW12_FRO = 4, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_4080X3060_60FPS_RAW12 = 5, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_3328X1872_120FPS = 6, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_4080X2296_60FPS_RAW12 = 7, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_2040X1148_240FPS = 8, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_2040X1148_480FPS_MODE2 = 9, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_2040X1532_120FPS = 10, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_2800X2100_60FPS = 11, */
	{false, false, false, SENSOR_12BIT_STATE_OFF, }, /* SENSOR_GN3_4080X2720_60FPS_RAW12 = 12, */
};

int sensor_gn3_cis_stream_on(struct v4l2_subdev *subdev);
int sensor_gn3_cis_stream_off(struct v4l2_subdev *subdev);
int sensor_gn3_cis_set_lownoise_mode_change(struct v4l2_subdev *subdev);
#if IS_ENABLED(USE_CAMERA_SENSOR_RETENTION)
int sensor_gn3_cis_retention_crc_check(struct v4l2_subdev *subdev);
int sensor_gn3_cis_retention_prepare(struct v4l2_subdev *subdev);
#endif
#endif
