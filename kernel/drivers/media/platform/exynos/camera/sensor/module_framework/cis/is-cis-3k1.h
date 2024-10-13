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

#ifndef IS_CIS_3K1_H
#define IS_CIS_3K1_H

#include "is-cis.h"

#define AEB_3K1_LUT0	0x0E10
#define AEB_3K1_LUT1	0x0E18

#define AEB_3K1_OFFSET_CIT		0x0
#define AEB_3K1_OFFSET_AGAIN	0x2
#define AEB_3K1_OFFSET_DGAIN	0x4
#define AEB_3K1_OFFSET_FLL		0x6

#define MAX_MULTICAL_DATA_LENGTH	120
#define MULTICAL_FULL_SIZE_W	3840
#define MULTICAL_FULL_SIZE_H	2880
#define CROP_SHIFT_ADDR_X	0x029E
#define CROP_SHIFT_ADDR_Y	0x02A0
#define CROP_SHIFT_FLIP	0x18
#define CROP_SHIFT_UNIT_PIX_X	0x1C
#define CROP_SHIFT_UNIT_PIX_Y	0x20
#define CROP_SHIFT_DELTA_ALIGN_X	8
#define CROP_SHIFT_DELTA_ALIGN_Y	32
#define CROP_SHIFT_DELTA_MAX_X	88
#define CROP_SHIFT_DELTA_MIN_X	-88
#define CROP_SHIFT_DELTA_MAX_Y	64
#define CROP_SHIFT_DELTA_MIN_Y	-64

#define MODE_GROUP_NONE (-1)

#define FCI_NONE_3K1 (0x01FF)

enum sensor_3k1_mode_group_enum {
	SENSOR_3K1_MODE_DEFAULT,
	SENSOR_3K1_MODE_AEB,
	SENSOR_3K1_MODE_MODE_GROUP_MAX
};
static u32 sensor_3k1_mode_groups[SENSOR_3K1_MODE_MODE_GROUP_MAX];

enum is_efs_state {
	IS_EFS_STATE_READ,
};

struct is_efs_info {
	unsigned long	efs_state;
	s16 crop_shift_delta_x;
	s16 crop_shift_delta_y;
};

enum sensor_3k1_mode_enum {
	SENSOR_3K1_3648x2736_60FPS = 0,
	SENSOR_3K1_3648x2052_60FPS = 1,
	SENSOR_3K1_3328x1872_60FPS = 2,
	SENSOR_3K1_2800x2100_30FPS = 3,
	SENSOR_3K1_1824x1368_120FPS = 4,
	SENSOR_3K1_1824x1368_30FPS = 5,
	SENSOR_3K1_3840x2880_60FPS = 6,
	SENSOR_3K1_3840x2160_60FPS = 7,
	SENSOR_3K1_3648x2736_60FPS_12BIT = 8,
	SENSOR_3K1_MODE_MAX
};

struct is_start_pos_info {
	u16 x_start;
	u16 y_start;
} start_pos_infos[SENSOR_3K1_MODE_MAX];

struct sensor_3k1_private_data {
	const struct sensor_regs global;
	const struct sensor_regs init_aeb;
	const struct sensor_regs dual_master;
	const struct sensor_regs dual_slave;
	const struct sensor_regs dual_standalone;
};

struct sensor_3k1_private_runtime {
	struct crop_shift_info shift_info;
};

static const struct sensor_reg_addr sensor_3k1_reg_addr = {
	.fll = 0x0340,
	.fll_aeb_long = AEB_3K1_LUT0 + AEB_3K1_OFFSET_FLL,
	.fll_aeb_short = AEB_3K1_LUT1 + AEB_3K1_OFFSET_FLL,
	.fll_shifter = 0x0702,
	.cit = 0x0202,
	.cit_aeb_long = AEB_3K1_LUT0 + AEB_3K1_OFFSET_CIT,
	.cit_aeb_short = AEB_3K1_LUT1 + AEB_3K1_OFFSET_CIT,
	.cit_shifter = 0x0704,
	.again = 0x0204,
	.again_aeb_long = AEB_3K1_LUT0 + AEB_3K1_OFFSET_AGAIN,
	.again_aeb_short = AEB_3K1_LUT1 + AEB_3K1_OFFSET_AGAIN,
	.dgain = 0x020E,
	.dgain_aeb_long = AEB_3K1_LUT0 + AEB_3K1_OFFSET_DGAIN,
	.dgain_aeb_short = AEB_3K1_LUT1 + AEB_3K1_OFFSET_DGAIN,
	.group_param_hold = 0x0104,
};

#endif

