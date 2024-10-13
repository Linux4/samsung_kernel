/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_SC201_H
#define IS_CIS_SC201_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_SC201_MAX_WIDTH          (1600)
#define SENSOR_SC201_MAX_HEIGHT         (1200)

#define SENSOR_SC201_FINE_INTEGRATION_TIME_MIN                (0x00)
#define SENSOR_SC201_FINE_INTEGRATION_TIME_MAX                (0x00)
#define SENSOR_SC201_COARSE_INTEGRATION_TIME_MIN              (0x01)
#define SENSOR_SC201_COARSE_INTEGRATION_TIME_MAX_MARGIN       (0x06)

#define USE_OTP_AWB_CAL_DATA            (0)

#define SENSOR_SC201_MIN_ANALOG_GAIN_SET_VALUE   (0x00)
#define SENSOR_SC201_MAX_ANALOG_GAIN_SET_VALUE   (0x0F)

#define SENSOR_SC201_MIN_DIGITAL_GAIN_SET_VALUE   (0x00)
#define SENSOR_SC201_MAX_DIGITAL_GAIN_SET_VALUE   (0x03)
#define SENSOR_SC201_MIN_DIGITAL_FINE_GAIN_SET_VALUE   (0x80)
#define SENSOR_SC201_MAX_DIGITAL_FINE_GAIN_SET_VALUE   (0xFF)

#define SENSOR_SC201_MAX_FRAME_LENGTH_LINE_VALUE  (0x7FFF)

#define MAX_GAIN_INDEX    4
#define CODE_GAIN_INDEX    0
#define PERMILE_GAIN_INDEX 1

const u32 sensor_sc201_analog_gain[][MAX_GAIN_INDEX + 1] = {
	{0, 1000},
	{1, 2000},
	{3, 4000},
	{7, 8000},
	{15, 16000},
};

const u32 sensor_sc201_otp_read[] = {
	0xfc, 0x01, 0x01,
	0xf4, 0x41, 0x01,
	0xf5, 0xc0, 0x01,
	0xf6, 0x44, 0x01,
	0xf8, 0x38, 0x01,
	0xf9, 0x82, 0x01,
	0xfa, 0x00, 0x01,
	0xfd, 0x80, 0x01,
	0xfc, 0x81, 0x01,
	0xfe, 0x03, 0x01,
	0x01, 0x0b, 0x01,
	0xf7, 0x01, 0x01,
	0xfc, 0x80, 0x01,
	0xfc, 0x80, 0x01,
	0xfc, 0x80, 0x01,
	0xfc, 0x8e, 0x01,
	0xfe, 0x00, 0x01,
	0x87, 0x09, 0x01,
	0xee, 0x72, 0x01,
	0xfe, 0x01, 0x01,
	0x8c, 0x90, 0x01,
	0xF3, 0x30, 0x01,
	0xFE, 0x02, 0x01,
};

#endif //IS_CIS_SC201_H
