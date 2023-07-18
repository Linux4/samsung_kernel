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

#ifndef IS_CIS_HI556_H
#define IS_CIS_HI556_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_HI556_MAX_WIDTH                   (2576 + 0)
#define SENSOR_HI556_MAX_HEIGHT                  (1932 + 0)

#define USE_GROUP_PARAM_HOLD                      (0)
#define TOTAL_NUM_OF_MIPI_LANES                   (2)

/* TODO: Check below values are valid */
#define SENSOR_HI556_FINE_INTEGRATION_TIME_MIN           (0x0) //Not supported
#define SENSOR_HI556_FINE_INTEGRATION_TIME_MAX           (0x0) //Not supported
#define SENSOR_HI556_COARSE_INTEGRATION_TIME_MIN         (0x02)
#define SENSOR_HI556_COARSE_INTEGRATION_TIME_MAX_MARGIN  (0x02)

#define SENSOR_HI556_FRAME_LENGTH_LINE_ADDR      (0x0006)
#define SENSOR_HI556_LINE_LENGTH_PCK_ADDR        (0x0008)
#define SENSOR_HI556_GROUP_PARAM_HOLD_ADDR       (0x0046)
#define SENSOR_HI556_COARSE_INTEG_TIME_ADDR      (0x0074)
#define SENSOR_HI556_ANALOG_GAIN_ADDR            (0x0077) //8bit
#define SENSOR_HI556_DIG_GAIN_ADDR               (0x0078) //Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A
#define SENSOR_HI556_MODEL_ID_ADDR               (0x0F16)
#define SENSOR_HI556_STREAM_ONOFF_ADDR           (0x0114)
#define SENSOR_HI556_ISP_ENABLE_ADDR             (0x0A04) //pdaf, mipi, fmt, Hscaler, D gain, DPC, LSC
#define SENSOR_HI556_MIPI_TX_OP_MODE_ADDR        (0x0902)
//#define SENSOR_HI556_ISP_FRAME_CNT_ADDR          (0x1056)
#define SENSOR_HI556_ISP_PLL_ENABLE_ADDR         (0x0F02)

#define SENSOR_HI556_MIN_ANALOG_GAIN_SET_VALUE   (0)      //x1.0
#define SENSOR_HI556_MAX_ANALOG_GAIN_SET_VALUE   (0xF0)   //x16.0
#define SENSOR_HI556_MIN_DIGITAL_GAIN_SET_VALUE  (0x0100) //x1.0
#define SENSOR_HI556_MAX_DIGITAL_GAIN_SET_VALUE  (0x07FD) //x7.99

//WB gain : Not Supported ??

struct setfile_info {
	const u32 *file;
	u32 file_size;
};

#define SET_SETFILE_INFO(s) {   \
	.file = s,                  \
	.file_size = ARRAY_SIZE(s), \
}

enum hi1336_fsync_enum {
	HI556_FSYNC_NORMAL,
	HI556_FSYNC_SLAVE,
	HI556_FSYNC_MASTER_FULL,
	HI556_FSYNC_MASTER_2_BINNING,
	HI556_FSYNC_MASTER_4_BINNING,
};

enum sensor_hi556_mode_enum {
	SENSOR_HI556_2576x1932_30FPS = 0,
	SENSOR_HI556_2560x1440_30FPS,
	SENSOR_HI556_1280x960_30FPS,
	SENSOR_HI556_1280x720_30FPS,
	SENSOR_HI556_640x480_112FPS,
	SENSOR_HI556_1280x960_58FPS,
};

#define USE_HI556_SETFILE

#endif

