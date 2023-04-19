/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_3L6_H
#define IS_CIS_3L6_H

#include "is-cis.h"

#define SENSOR_3L6_FRAME_COUNT_ADDR		(0x0005)
#define SENSOR_3L6_MODE_SELECT_ADDR		(0x0100)
#define SENSOR_3L6_GROUP_PARAM_HOLD_ADDR	(0x0104)
#define SENSOR_3L6_PLL_POWER_CONTROL_ADDR	(0x3C1E)

/*Apply the same order as in is-cis-3l6-setX.h file*/
enum sensor_mode_enum {
	SENSOR_3L6_MODE_4128x3096_30FPS,
	SENSOR_3L6_MODE_4128x2324_30FPS,
	SENSOR_3L6_MODE_3408x2556_30FPS,
	SENSOR_3L6_MODE_3408x1916_30FPS,
	SENSOR_3L6_MODE_1028x772_120FPS,
	SENSOR_3L6_MODE_2064x1548_30FPS,
	SENSOR_3L6_MODE_2064x1160_30FPS,
	SENSOR_3L6_MODE_1696x1272_30FPS,
	SENSOR_3L6_MODE_1856x1044_30FPS,
	SENSOR_3L6_MODE_1024x768_120FPS,
	SENSOR_3L6_MODE_3712x2556_30FPS,
	SENSOR_3L6_MODE_MAX,
};

static const struct sensor_reg_addr sensor_3l6_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0, /* not supported */
	.cit = 0x0202,
	.cit_shifter = 0, /* not supported */
	.again = 0x0204,
	.dgain = 0x020E, /* 4byte */
};

#endif
