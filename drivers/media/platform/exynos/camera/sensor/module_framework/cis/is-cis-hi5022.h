/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_HI5022_H
#define IS_CIS_HI5022_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_HI5022_MAX_WIDTH                   (8192 + 0)
#define SENSOR_HI5022_MAX_HEIGHT                  (6144 + 0)

#define USE_GROUP_PARAM_HOLD                      (0)
#define TOTAL_NUM_OF_MIPI_LANES                   (4)

/* TODO: Check below values are valid */
#define SENSOR_HI5022_FINE_INTEGRATION_TIME_MIN           (0x0) //Not supported
#define SENSOR_HI5022_FINE_INTEGRATION_TIME_MAX           (0x0) //Not supported
#define SENSOR_HI5022_COARSE_INTEGRATION_TIME_MIN         (0x08)
#define SENSOR_HI5022_COARSE_INTEGRATION_TIME_MAX_MARGIN  (0x08)

#define SENSOR_HI5022_FRAME_LENGTH_LINE_ADDR      (0x050C)
#define SENSOR_HI5022_LINE_LENGTH_PCK_ADDR        (0x03A0)
#define SENSOR_HI5022_GROUP_PARAM_HOLD_ADDR       (0x0208)
#define SENSOR_HI5022_COARSE_INTEG_TIME_ADDR      (0x0510)
#define SENSOR_HI5022_LONG_COARSE_INTEG_TIME_ADDR (0x0512)
#define SENSOR_HI5022_ANALOG_GAIN_ADDR            (0x050A) //8bit
#define SENSOR_HI5022_DIG_GAIN_ADDR               (0x0514) //Gr:0x0514 ,Gb:0x0516, R:0x0518, B:0x051A
#define SENSOR_HI5022_MODEL_ID_ADDR               (0x0716)
#define SENSOR_HI5022_STREAM_ONOFF_ADDR           (0x0B00)
#define SENSOR_HI5022_ISP_ENABLE_ADDR             (0x0B04) //pdaf, mipi, fmt, Hscaler, D gain, DPC, LSC
#define SENSOR_HI5022_MIPI_TX_OP_MODE_ADDR        (0x1002)
#define SENSOR_HI5022_ISP_FRAME_CNT_ADDR          (0x1056)
#define SENSOR_HI5022_ISP_PLL_ENABLE_ADDR         (0x0702)

#define SENSOR_HI5022_MIN_ANALOG_GAIN_SET_VALUE   (0)      //x1.0
#define SENSOR_HI5022_MAX_ANALOG_GAIN_SET_VALUE   (0x3F0)  //x64.0
#define SENSOR_HI5022_MIN_DIGITAL_GAIN_SET_VALUE  (0x0200) //x1.0
#define SENSOR_HI5022_MAX_DIGITAL_GAIN_SET_VALUE  (0x1FFB) //x15.99

//WB gain : Not Supported ??

struct setfile_info {
	const u32 *file;
	u32 file_size;
};

#define SET_SETFILE_INFO(s) {   \
	.file = s,                  \
	.file_size = ARRAY_SIZE(s), \
}

enum hi5022_fsync_enum {
	HI5022_FSYNC_NORMAL,
	HI5022_FSYNC_SLAVE,
	HI5022_FSYNC_MASTER_FULL,
	HI5022_FSYNC_MASTER_2_BINNING,
	HI5022_FSYNC_MASTER_4_BINNING,
};

enum sensor_hi556_mode_enum {
	SENSOR_HI5022_4SUM_4080x3060_30FPS = 0,
	SENSOR_HI5022_4SUM_4080x2296_30FPS,
	SENSOR_HI5022_A2A2_2032x1524_120FPS,
	SENSOR_HI5022_FULL_8160x6120_15FPS,
};

#define USE_HI5022_SETFILE

#endif


