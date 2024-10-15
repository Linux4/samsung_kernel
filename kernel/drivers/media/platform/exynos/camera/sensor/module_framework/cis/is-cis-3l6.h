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

#ifndef IS_CIS_3L6_H
#define IS_CIS_3L6_H

#include "is-cis.h"

/*Apply the same order as in is-cis-3l6-setX.h file*/
enum sensor_mode_enum {
	SENSOR_3L6_MODE_4000x3000_30FPS,
	SENSOR_3L6_MODE_4000x2256_30FPS,
	SENSOR_3L6_MODE_2000x1500_30FPS,
	SENSOR_3L6_MODE_2000x1124_30FPS,
	SENSOR_3L6_MODE_992x744_120FPS,
	SENSOR_3L6_MODE_2800x2100_30FPS,
	SENSOR_3L6_MODE_MAX
};

struct sensor_3l6_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_3l6_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0, /* not supported */
	.cit = 0x0202,
	.cit_shifter = 0, /* not supported */
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};

#endif
