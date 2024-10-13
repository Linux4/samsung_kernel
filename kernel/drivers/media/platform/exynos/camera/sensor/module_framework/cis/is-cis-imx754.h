/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_IMX754_H
#define IS_CIS_IMX754_H

#include "is-cis.h"

#define AEB_IMX754_LUT0	0x0E20
#define AEB_IMX754_LUT1	0x0E40

#define AEB_IMX754_OFFSET_CIT		0x0
#define AEB_IMX754_OFFSET_AGAIN		0x2
#define AEB_IMX754_OFFSET_DGAIN		0x4
#define AEB_IMX754_OFFSET_FLL		0xE

#define CROP_SHIFT_ADDR_X	0x029E
#define CROP_SHIFT_ADDR_Y	0x02A0
#define CROP_SHIFT_DELTA_ALIGN_X	2
#define CROP_SHIFT_DELTA_ALIGN_Y	4

enum sensor_imx754_mode_enum {
	SENSOR_IMX754_3648X2736_60FPS = 0,
	SENSOR_IMX754_3840X2160_60FPS = 1,
	SENSOR_IMX754_3648X2736_30FPS = 2,
	SENSOR_IMX754_3840X2160_30FPS = 3,
	SENSOR_IMX754_1920X1080_120FPS = 4,
	SENSOR_IMX754_1920X1080_240FPS = 5,
	SENSOR_IMX754_912X684_120FPS = 6,
	SENSOR_IMX754_3648X2052_60FPS = 7,
	SENSOR_IMX754_4000X2252_60FPS = 8,
	SENSOR_IMX754_4000X3000_30FPS = 9,
	SENSOR_IMX754_3648X2736_30FPS_LN4 = 10,
	SENSOR_IMX754_3840X2160_30FPS_LN4 = 11,
	SENSOR_IMX754_3648X2052_30FPS_LN4 = 12,
	SENSOR_IMX754_3648X2736_30FPS_NORMAL = 13,
	SENSOR_IMX754_3840X2160_30FPS_NORMAL = 14,
	SENSOR_IMX754_3648X2052_30FPS_NORMAL = 15,
	SENSOR_IMX754_MODE_MAX
};

struct sensor_imx754_private_data {
	const struct sensor_regs global;
	const struct sensor_regs init_aeb;
};

struct sensor_imx754_private_runtime {
	bool seamless_fast_transit;
};

static const struct sensor_reg_addr sensor_imx754_reg_addr = {
	.fll = 0x0340,
	.fll_aeb_long = AEB_IMX754_LUT0 + AEB_IMX754_OFFSET_FLL,
	.fll_aeb_short = AEB_IMX754_LUT1 + AEB_IMX754_OFFSET_FLL,
	.fll_shifter = 0x3151,
	.cit = 0x0202,
	.cit_aeb_long = AEB_IMX754_LUT0 + AEB_IMX754_OFFSET_CIT,
	.cit_aeb_short = AEB_IMX754_LUT1 + AEB_IMX754_OFFSET_CIT,
	.cit_shifter = 0x3150,
	.again = 0x0204,
	.again_aeb_long = AEB_IMX754_LUT0 + AEB_IMX754_OFFSET_AGAIN,
	.again_aeb_short = AEB_IMX754_LUT1 + AEB_IMX754_OFFSET_AGAIN,
	.dgain = 0x020E,
	.dgain_aeb_long = AEB_IMX754_LUT0 + AEB_IMX754_OFFSET_DGAIN,
	.dgain_aeb_short = AEB_IMX754_LUT1 + AEB_IMX754_OFFSET_DGAIN,
	.group_param_hold = 0x0104,
};

#define MODE_GROUP_NONE (-1)
enum sensor_imx754_mode_group_enum {
	SENSOR_IMX754_MODE_DEFAULT,
	SENSOR_IMX754_MODE_AEB,
	SENSOR_IMX754_MODE_LN2,
	SENSOR_IMX754_MODE_LN4,
	SENSOR_IMX754_MODE_MODE_GROUP_MAX
};
static u32 sensor_imx754_mode_groups[SENSOR_IMX754_MODE_MODE_GROUP_MAX];

struct is_start_pos_info {
	u16 x_start;
	u16 y_start;
} static start_pos_infos[SENSOR_IMX754_MODE_MAX];

/**
 * Register address & data
 */
#define DATA_IMX754_GPH_HOLD            (0x01)
#define DATA_IMX754_GPH_RELEASE         (0x00)

static const u16 sensor_imx754_cis_dual_master_settings[] = {
	0x3050,	0x01,	0x01,
	0x3030,	0x01,	0x01,
	0x3053,	0x00,	0x01,
	0x3051,	0x01,	0x01,
	0x3052,	0x07,	0x01,
};

#endif

