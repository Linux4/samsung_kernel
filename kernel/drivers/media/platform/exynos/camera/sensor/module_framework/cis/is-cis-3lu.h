/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_3LU_H
#define IS_CIS_3LU_H

#include "is-cis.h"

#define LATEST_3LU_REV	(0xA101)

#define AEB_3LU_LUT0	0x0E10
#define AEB_3LU_LUT1	0x0E20

#define AEB_3LU_OFFSET_CIT		0x0
#define AEB_3LU_OFFSET_AGAIN	0x2
#define AEB_3LU_OFFSET_DGAIN	0x4
#define AEB_3LU_OFFSET_FLL		0x6

enum sensor_3lu_mode_enum {
	SENSOR_3LU_4000X3000_60FPS_10BIT = 0, /* A-01-A */
	SENSOR_3LU_3392X2544_60FPS_10BIT = 1, /* A-01-B */
	SENSOR_3LU_3392X1908_60FPS_10BIT = 2, /* A-01-C */
	SENSOR_3LU_4000X3000_30FPS_ADC = 3, /* A-04 */
	SENSOR_3LU_4000X2252_60FPS_10BIT = 4, /* A-09 */
	SENSOR_3LU_4000X2252_60FPS_ADC = 5, /* A-12 */
	SENSOR_3LU_2000X1124_120FPS_10BIT = 6, /* B-01 */
	SENSOR_3LU_2000X1500_120FPS_FAST_AE = 7, /* C-02 */
	SENSOR_3LU_2000X1500_30FPS_10BIT = 8, /* C-04-A */
	SENSOR_3LU_1696X1272_30FPS_10BIT = 9, /* C-04-B */
	SENSOR_3LU_1696X954_30FPS_10BIT = 10, /* C-04-C */

	SENSOR_3LU_4000X3000_30FPS_DSG, /* A-03 */
	SENSOR_3LU_4000X3000_30FPS_12BIT_LN2, /* A-06 */
	SENSOR_3LU_4000X3000_60FPS_AEB, /* A-05 */
	SENSOR_3LU_4000X3000_15FPS_10BIT_LN4, /* A-07 */
	SENSOR_3LU_4000X2252_30FPS_DSG, /* A-11-A */
	SENSOR_3LU_4000X2252_30FPS_12BIT_LN2, /* A-14-A */
	SENSOR_3LU_4000X2252_15FPS_10BIT_LN4, /* A-16 */
	SENSOR_3LU_4000X2252_30FPS_10BIT_LN2, /* A-18 */
	SENSOR_3LU_4000X2252_60FPS_AEB, /* A-19 */

	SENSOR_3LU_MODE_MAX,
};

#define MODE_GROUP_NONE (-1)
enum sensor_3lu_mode_group_enum {
	SENSOR_3LU_MODE_DEFAULT,
	SENSOR_3LU_MODE_AEB,
	SENSOR_3LU_MODE_IDCG,
	SENSOR_3LU_MODE_LN2,
	SENSOR_3LU_MODE_LN4,
	SENSOR_3LU_MODE_MODE_GROUP_MAX
};
static u32 sensor_3lu_mode_groups[SENSOR_3LU_MODE_MODE_GROUP_MAX];

#define FCI_NONE_3LU (0x01FF)

struct sensor_3lu_specific_mode_fast_change_attr {
	u32 fast_change_idx;
};

struct sensor_3lu_private_data {
	const struct sensor_regs init;
	const struct sensor_regs global;
	const struct sensor_regs global_burst;
	const struct sensor_regs fcm_lut;
	const struct sensor_regs fcm_lut_burst;
	const struct sensor_regs secure;
	const struct sensor_regs disable_scramble;
	const struct sensor_regs aeb_on;
	const struct sensor_regs aeb_off;
};

struct sensor_3lu_private_runtime {
	bool is_long_term_mode;
};

static const struct sensor_reg_addr sensor_3lu_reg_addr = {
	.fll = 0x0340,
	.fll_aeb_long = AEB_3LU_LUT0 + AEB_3LU_OFFSET_FLL,
	.fll_aeb_short = AEB_3LU_LUT1 + AEB_3LU_OFFSET_FLL,
	.fll_shifter = 0x0702,
	.cit = 0x0202,
	.cit_aeb_long = AEB_3LU_LUT0 + AEB_3LU_OFFSET_CIT,
	.cit_aeb_short = AEB_3LU_LUT1 + AEB_3LU_OFFSET_CIT,
	.cit_shifter = 0x0704,
	.again = 0x0204,
	.again_aeb_long = AEB_3LU_LUT0 + AEB_3LU_OFFSET_AGAIN,
	.again_aeb_short = AEB_3LU_LUT1 + AEB_3LU_OFFSET_AGAIN,
	.dgain = 0x020E,
	.dgain_aeb_long = AEB_3LU_LUT0 + AEB_3LU_OFFSET_DGAIN,
	.dgain_aeb_short = AEB_3LU_LUT1 + AEB_3LU_OFFSET_DGAIN,
	.group_param_hold = 0x0104,
};

int sensor_3lu_cis_stream_on(struct v4l2_subdev *subdev);
int sensor_3lu_cis_stream_off(struct v4l2_subdev *subdev);
int sensor_3lu_cis_set_lownoise_mode_change(struct v4l2_subdev *subdev);
#endif
