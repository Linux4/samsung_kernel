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

#ifndef IS_CIS_IMX355_H
#define IS_CIS_IMX355_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define USE_GROUP_PARAM_HOLD	(0)
#define SENSOR_IMX355_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL    65524
#define SENSOR_IMX355_MAX_CIT_LSHIFT_VALUE                     (0x7)

enum sensor_imx355_mode_enum{
	SENSOR_IMX355_3264X2448_30FPS_702MBPS = 0,
	SENSOR_IMX355_3264X2448_30FPS_715MBPS,
	SENSOR_IMX355_3264X1836_30FPS_702MBPS,
	SENSOR_IMX355_3264X1836_30FPS_689MBPS,
	SENSOR_IMX355_800X600_115FPS,
	SENSOR_IMX355_2880X1980_30FPS_702MBPS,
	SENSOR_IMX355_2640X1980_30FPS_702MBPS,
	SENSOR_IMX355_1632X1224_30FPS_702MBPS,
	SENSOR_IMX355_1632X916_30FPS_702MBPS,
	SENSOR_IMX355_736X552_120FPS,
	SENSOR_IMX355_MODE_MAX,
};

struct sensor_imx355_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_imx355_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0, /* Not Supported */
	.cit = 0x0202,
	.cit_shifter = 0x3060,
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};

#endif

