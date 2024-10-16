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

#ifndef IS_CIS_GC13A0_H
#define IS_CIS_GC13A0_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_GC13A0_MAX_WIDTH          (4128)
#define SENSOR_GC13A0_MAX_HEIGHT         (3096)

#define SENSOR_GC13A0_FINE_INTEGRATION_TIME_MIN                (0x00)
#define SENSOR_GC13A0_FINE_INTEGRATION_TIME_MAX                (0x00)
#define SENSOR_GC13A0_COARSE_INTEGRATION_TIME_MIN              (0x04)
#define SENSOR_GC13A0_COARSE_INTEGRATION_TIME_MAX_MARGIN       (0x10)

#define USE_OTP_AWB_CAL_DATA            (0)

#define SENSOR_GC13A0_MIN_ANALOG_GAIN   (0x0400)
#define SENSOR_GC13A0_MAX_ANALOG_GAIN   (0x4000)

#define SENSOR_GC13A0_MIN_DIGITAL_GAIN   (0x0400)
#define SENSOR_GC13A0_MAX_DIGITAL_GAIN   (0x3FFF)

const u32 sensor_gc13a0_otp_read[] = {
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
	0xf3, 0x30, 0x01,
	0xfE, 0x02, 0x01,
};

enum sensor_gc13a0_mode_enum {
	SENSOR_GC13A0_4128x3096_30FPS = 0,
	SENSOR_GC13A0_4128x2324_30FPS,
	SENSOR_GC13A0_3408x2556_30FPS,
	SENSOR_GC13A0_3712x2556_30FPS,
	SENSOR_GC13A0_2064x1548_60FPS,
	SENSOR_GC13A0_2064x1512_60FPS,
	SENSOR_GC13A0_2064x1548_30FPS,
	SENSOR_GC13A0_2064x1160_30FPS,
	SENSOR_GC13A0_1696x1272_30FPS,
	SENSOR_GC13A0_1856x1044_30FPS,
};

#endif
