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

#ifndef IS_CIS_HI1337_H
#define IS_CIS_HI1337_H

#include "is-cis.h"

#define USE_GROUP_PARAM_HOLD                      (0)

#define SENSOR_HI1337_GROUP_PARAM_HOLD_ADDR       (0x0208)
#define SENSOR_HI1337_COARSE_INTEG_TIME_ADDR      (0x020A)

#define SENSOR_HI1337_ANALOG_GAIN_ADDR            (0x0213) //8bit
#define SENSOR_HI1337_DIG_GAIN_ADDR               (0x0214) //Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A
#define SENSOR_HI1337_MODEL_ID_ADDR               (0x0716)
#define SENSOR_HI1337_STREAM_ONOFF_ADDR           (0x0B00)
#define SENSOR_HI1337_ISP_ENABLE_ADDR             (0x0B04) //pdaf, mipi, fmt, Hscaler, D gain, DPC, LSC
#define SENSOR_HI1337_MIPI_TX_OP_MODE_ADDR        (0x1002)
#define SENSOR_HI1337_ISP_FRAME_CNT_ADDR          (0x1056)
#define SENSOR_HI1337_ISP_PLL_ENABLE_ADDR         (0x0702)

struct setfile_info {
	const u32 *file;
	u32 file_size;
};

#define SET_SETFILE_INFO(s) {   \
	.file = s,                  \
	.file_size = ARRAY_SIZE(s), \
}

enum hi1337_fsync_enum {
	HI1337_FSYNC_NORMAL,
	HI1337_FSYNC_SLAVE,
	HI1337_FSYNC_MASTER,
};

enum sensor_hi1337_mode_enum {
	SENSOR_HI1337_4000x3000_30fps = 0,
	SENSOR_HI1337_MODE_MAX,
};

struct sensor_hi1337_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_hi1337_reg_addr = {
	.fll = 0x020E,
	.fll_shifter = 0, /* not supported */
	.cit = 0x020A,
	.cit_shifter = 0, /* not supported */
	.again = 0x0213, /* 8bit */
	.dgain = 0x0214, /* Gr:0x214 ,Gb:0x216, R:0x218, B:0x21A */
};
#endif
