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

#ifndef IS_CIS_IMX258_H
#define IS_CIS_IMX258_H

#include "is-cis.h"

enum sensor_imx258_mode_enum {
	SENSOR_IMX258_MODE_4000x3000_30FPS = 0,
	SENSOR_IMX258_MODE_4000x2256_30FPS,
	SENSOR_IMX258_MODE_1000x750_120FPS,
	SENSOR_IMX258_MODE_2000x1500_30FPS,
	SENSOR_IMX258_MODE_2000x1128_30FPS,
	SENSOR_IMX258_MODE_MAX,
};

struct sensor_imx258_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_imx258_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0,
	.cit = 0x0202,
	.cit_shifter = 0, /* not supported */
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};
#endif
