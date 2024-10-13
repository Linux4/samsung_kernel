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

#ifndef IS_CIS_SC501_H
#define IS_CIS_SC501_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_SC501_MAX_WIDTH          (2592)
#define SENSOR_SC501_MAX_HEIGHT         (1944)

#define SENSOR_SC501_FINE_INTEGRATION_TIME_MIN                (0x00)
#define SENSOR_SC501_FINE_INTEGRATION_TIME_MAX                (0x00)
#define SENSOR_SC501_COARSE_INTEGRATION_TIME_MIN              (0x03)
#define SENSOR_SC501_COARSE_INTEGRATION_TIME_MAX_MARGIN       (0x0A)

#define USE_OTP_AWB_CAL_DATA            (0)

#define SENSOR_SC501_MAX_FRAME_LENGTH_LINE_VALUE  (0xFFFF)

#define SENSOR_SC501_MIN_ANALOG_GAIN_SET_VALUE   (0x00)
#define SENSOR_SC501_MAX_ANALOG_GAIN_SET_VALUE   (0x0F)

#define SENSOR_SC501_MIN_DIGITAL_GAIN_SET_VALUE   (0x00)
#define SENSOR_SC501_MAX_DIGITAL_GAIN_SET_VALUE   (0x0F)
#define SENSOR_SC501_MIN_DIGITAL_FINE_GAIN_SET_VALUE   (0x80)
#define SENSOR_SC501_MAX_DIGITAL_FINE_GAIN_SET_VALUE   (0xFF)

//#define SENSOR_SC501_CHIP_ID_WC1XA    (0xA0)
//#define SENSOR_SC501_CHIP_ID_WC1XB    (0xA1)

#define MAX_GAIN_INDEX    4
#define CODE_GAIN_INDEX    0
#define PERMILE_GAIN_INDEX 1

const u32 sensor_sc501_analog_gain[][MAX_GAIN_INDEX] = {
	{0, 1000},
	{8, 2000},
	{9, 4000},
	{11, 8000},
	{15, 16000},
};

#endif //IS_CIS_SC501_H
