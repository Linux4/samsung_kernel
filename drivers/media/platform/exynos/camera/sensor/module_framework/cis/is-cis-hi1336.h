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

#ifndef IS_CIS_HI1336_H
#define IS_CIS_HI1336_H

#include "is-cis.h"

#define USE_GROUP_PARAM_HOLD                      (0)

#define SENSOR_HI1336_GROUP_PARAM_HOLD_ADDR       (0x0208)
#define SENSOR_HI1336_COARSE_INTEG_TIME_ADDR      (0x020A)

#define SENSOR_HI1336_ANALOG_GAIN_ADDR            (0x0213) //8bit
#define SENSOR_HI1336_DIG_GAIN_ADDR               (0x0214) //Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A
#define SENSOR_HI1336_MODEL_ID_ADDR               (0x0716)
#define SENSOR_HI1336_STREAM_ONOFF_ADDR           (0x0808)
#define SENSOR_HI1336_ISP_ENABLE_ADDR             (0x0B04) //pdaf, mipi, fmt, Hscaler, D gain, DPC, LSC
#define SENSOR_HI1336_MIPI_TX_OP_MODE_ADDR        (0x1002)
#define SENSOR_HI1336_ISP_FRAME_CNT_ADDR          (0x1056)
#define SENSOR_HI1336_ISP_PLL_ENABLE_ADDR         (0x0702)

struct setfile_info {
	const u32 *file;
	u32 file_size;
};

#define SET_SETFILE_INFO(s) {   \
	.file = s,                  \
	.file_size = ARRAY_SIZE(s), \
}

enum hi1336_fsync_enum {
	HI1336_FSYNC_NORMAL,
	HI1336_FSYNC_SLAVE,
	HI1336_FSYNC_MASTER_FULL,
	HI1336_FSYNC_MASTER_2_BINNING,
	HI1336_FSYNC_MASTER_4_BINNING,
};

enum sensor_hi1336_mode_enum {
	SENSOR_HI1336_4128x3096_30fps = 0,
	SENSOR_HI1336_4128x2324_30fps,
	SENSOR_HI1336_4128x1908_30fps,
	SENSOR_HI1336_3088x3088_30fps,
	SENSOR_HI1336_3408x2556_30fps,
	SENSOR_HI1336_1024x768_120fps,
	SENSOR_HI1336_2064x1548_30fps,
	SENSOR_HI1336_2064x1160_30fps,
	SENSOR_HI1336_1696x1272_30fps,
	SENSOR_HI1336_3712x2556_30fps,
	SENSOR_HI1336_MODE_MAX,
};

static const struct sensor_reg_addr sensor_hi1336_reg_addr = {
	.fll = 0x020E,
	.fll_shifter = 0, /* not supported */
	.cit = 0x020A,
	.cit_shifter = 0, /* not supported */
	.again = 0x0213, /* 8bit */
	.dgain = 0x0214, /* Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A */
};
#endif
