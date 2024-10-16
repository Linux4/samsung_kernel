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

#define SENSOR_3L6_FRAME_COUNT_ADDR		(0x0005)
#define SENSOR_3L6_MODE_SELECT_ADDR		(0x0100)
#define SENSOR_3L6_PLL_POWER_CONTROL_ADDR	(0x3C1E)

/********* S5K3L6 OTP *********/
#define IS_READ_MAX_S5K3L6_OTP_CAL_SIZE		(380)
#define S5K3L6_OTP_PAGE_SIZE			(64)
#define S5K3L6_OTP_BANK_READ_PAGE		(0x34)
#define S5K3L6_OTP_PAGE_START_ADDR		(0x0A04)
#define S5K3L6_OTP_START_PAGE_BANK1		(0x34)
#define S5K3L6_OTP_START_PAGE_BANK2		(0x3A)
#define S5K3L6_OTP_READ_START_ADDR		(0x0A08)

#ifdef USE_S5K3L6_13MP_FULL_SIZE
enum sensor_mode_enum {
	SENSOR_3L6_MODE_4128x3096_30FPS,
	SENSOR_3L6_MODE_4128x2576_30FPS,
	SENSOR_3L6_MODE_4128x2324_30FPS,
	SENSOR_3L6_MODE_2064x1548_30FPS,
	SENSOR_3L6_MODE_2064x1160_30FPS,
	SENSOR_3L6_MODE_1028x772_120FPS,
	SENSOR_3L6_MODE_3408x2556_30FPS,
	SENSOR_3L6_MODE_1024x768_120FPS,
	SENSOR_3L6_MODE_2072x1552_30FPS,
	SENSOR_3L6_MODE_2072x1162_30FPS,
	SENSOR_3L6_MODE_3712x2556_30FPS,
	SENSOR_3L6_MODE_1696x1272_30FPS,
	SENSOR_3L6_MODE_1856x1044_30FPS,
	SENSOR_3L6_MODE_MAX
};
#else
enum sensor_mode_enum {
	SENSOR_3L6_MODE_4000x3000_30FPS,
	SENSOR_3L6_MODE_4000x2256_30FPS,
	SENSOR_3L6_MODE_2000x1500_30FPS,
	SENSOR_3L6_MODE_2000x1124_30FPS,
	SENSOR_3L6_MODE_992x744_120FPS,
	SENSOR_3L6_MODE_MAX
};
#endif

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
