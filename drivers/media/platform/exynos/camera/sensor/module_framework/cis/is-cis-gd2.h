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

#ifndef IS_CIS_GD2_H
#define IS_CIS_GD2_H

#include "is-cis.h"

#define SENSOR_GD2_BURST_WRITE        (1)

/* OPT Chip ID Version Check */
#define SENSOR_GD2_OTP_PAGE_SETUP_ADDR         (0x0A02)
#define SENSOR_GD2_OTP_READ_TRANSFER_MODE_ADDR (0x0A00)
#define SENSOR_GD2_OTP_STATUS_REGISTER_ADDR    (0x0A01)
#define SENSOR_GD2_OTP_CHIP_REVISION_ADDR      (0x0002)

/* Related EEPROM CAL : WRITE_SENSOR_CAL_FOR_GGC */
#define SENSOR_GD2_GGC_CAL_BASE_REAR           (0x2A36)
#define SENSOR_GD2_GGC_CAL_SIZE                (92)
#define SENSOR_GD2_GGC_REG_ADDR                (0x39DA)

/* Related Function Option */
#define WRITE_SENSOR_CAL_FOR_GGC                (1)

enum sensor_gd2_mode_enum {
	/* 2SUM */
	SENSOR_GD2_2SUM_FULL_3264X2448_30FPS,
	SENSOR_GD2_2SUM_CROP_3264X1836_30FPS,
	SENSOR_GD2_2SUM_CROP_2880X1980_30FPS,
	SENSOR_GD2_2SUM_CROP_2640X1980_30FPS,
	SENSOR_GD2_2SUM_CROP_1632X1224_120FPS,
	SENSOR_GD2_2SUM_CROP_3264X1836_60FPS,
	/* 4SUM */
	SENSOR_GD2_4SUM_CROP_3264X1836_120FPS,
	/* FULL Remosaic */
	SENSOR_GD2_REMOSAIC_FULL_6528x4896_24FPS,
	SENSOR_GD2_MODE_MAX
};

struct sensor_gd2_private_data {
	const struct sensor_regs initial;
	const struct sensor_regs tnp;
	const struct sensor_regs global;
	const struct sensor_regs global_calibration;
};

static const struct sensor_reg_addr sensor_gd2_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0x0702,
	.cit = 0x0202,
	.cit_shifter = 0x0704,
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};

#undef DISABLE_GD2_PDAF_TAIL_MODE

#endif
