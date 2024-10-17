/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_3J1_H
#define IS_CIS_3J1_H

#include "is-cis.h"

enum sensor_3j1_mode_enum {
	SENSOR_3J1_3648x2736_30FPS = 0,
	SENSOR_3J1_3968x2232_30FPS,
	SENSOR_3J1_3968x2232_60FPS,
	SENSOR_3J1_1984x1120_120FPS,
	SENSOR_3J1_1824x1368_30FPS,
	SENSOR_3J1_1984x1120_30FPS,
	SENSOR_3J1_912x684_120FPS,
	SENSOR_3J1_3280x2268_30FPS,
	SENSOR_3J1_1520x1136_30FPS,
	SENSOR_3J1_1640x924_30FPS,
	SENSOR_3J1_MODE_MAX
};

struct sensor_3j1_private_data {
	const struct sensor_regs global;
	const struct sensor_regs dual_slave;
	const struct sensor_regs dual_standalone;
};

static const struct sensor_reg_addr sensor_3j1_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0x0702,
	.cit = 0x0202,
	.cit_shifter = 0x0704,
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};

#endif
