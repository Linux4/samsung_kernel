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

#ifndef IS_CIS_GN5_H
#define IS_CIS_GN5_H

#include "is-cis.h"

/* Related EEPROM CAL */
#define CIS_CALIBRATION 1
#if IS_ENABLED(CIS_CALIBRATION)
#define GN5_XTC_ADDR				(0x8320)
#define GN5_BURST_WRITE

enum gn5_endian {
	GN5_LITTLE_ENDIAN = 0,
	GN5_BIG_ENDIAN = 1,
};
#define GN5_ENDIAN(a, b, endian)  ((endian == GN5_BIG_ENDIAN) ? ((a << 8)|(b)) : ((a)|(b << 8)))
#endif

#define GN5_REMOSAIC_ZOOM_RATIO_X_2	20	/* x2.0 = 20 */

enum sensor_gn5_mode_enum {
	SENSOR_GN5_4080X3060_30FPS_R12,
	SENSOR_GN5_4080X2296_30FPS_R12,
	SENSOR_GN5_4080X2296_60FPS_R12,
	SENSOR_GN5_2040X1532_30FPS_R10,
	SENSOR_GN5_2040X1532_120FPS_R10,
	SENSOR_GN5_2040X1148_240FPS_R10,

	/* Remosaic */
	SENSOR_GN5_REMOSAIC_FULL_8160x6120_24FPS,
	SENSOR_GN5_REMOSAIC_FULL_8160x4592_24FPS,

	/* Default Video DCG on mode */
	SENSOR_GN5_4080X3060_30FPS_IDCG_R12,
	SENSOR_GN5_4080X2296_30FPS_IDCG_R12,

	/* seamless mode */
	SENSOR_GN5_4080X3060_30FPS_REMOSAIC_CROP_R12,
	SENSOR_GN5_4080X2296_30FPS_REMOSAIC_CROP_R12,
	SENSOR_GN5_4080X3060_20FPS_LN4_R12,
	SENSOR_GN5_4080X2296_30FPS_LN4_R12,
	SENSOR_GN5_MODE_MAX,
};

#define MODE_GROUP_NONE (-1)
enum sensor_gn5_mode_group_enum {
	SENSOR_GN5_MODE_NORMAL,
	SENSOR_GN5_MODE_RMS_CROP,
	SENSOR_GN5_MODE_LN4,
	SENSOR_GN5_MODE_IDCG,
	SENSOR_GN5_MODE_MODE_GROUP_MAX
};
u32 sensor_gn5_mode_groups[SENSOR_GN5_MODE_MODE_GROUP_MAX];

struct sensor_gn5_private_data {
	const struct sensor_regs global;
	const struct sensor_regs xtc_prefix;
	const struct sensor_regs xtc_postfix;
	const struct sensor_regs prepare_fcm;
	const struct sensor_regs *load_sram;
	u32 max_load_sram_num;
};

const u32 sensor_gn5_rms_binning_ratio[SENSOR_GN5_MODE_MAX] = {
	[SENSOR_GN5_4080X3060_30FPS_REMOSAIC_CROP_R12] = 1000,
	[SENSOR_GN5_4080X2296_30FPS_REMOSAIC_CROP_R12] = 1000,
};

static const struct sensor_reg_addr sensor_gn5_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0x0702,
	.cit = 0x0202,
	.cit_shifter = 0x0704,
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};

#endif
