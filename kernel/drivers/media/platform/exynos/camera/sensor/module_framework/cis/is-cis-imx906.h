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

#ifndef IS_CIS_IMX906_H
#define IS_CIS_IMX906_H

#include "is-cis.h"

/* Register address */
#define IMX906_SETUP_MODE_SELECT_ADDR      (0x0100)
#define IMX906_ABS_GAIN_GR_SET_ADDR        (0x0B8E)

#define IMX906_REMOSAIC_ZOOM_RATIO_X_2	20	/* x2.0 = 20 */

/* Related Function Option */
#define CIS_CALIBRATION	1

#if IS_ENABLED(CIS_CALIBRATION)
#define IMX906_QSC_ADDR		(0x1000)
#endif

enum sensor_imx906_mode_enum {
	SENSOR_IMX906_4080X3060_30FPS_R12,
	SENSOR_IMX906_4080X2296_30FPS_R12,
	SENSOR_IMX906_4080X2296_60FPS_R12,
	SENSOR_IMX906_2040X1532_30FPS_R10,
	SENSOR_IMX906_2040X1532_120FPS_R10,
	SENSOR_IMX906_2040X1148_240FPS_R10,

	/* Remosaic */
	SENSOR_IMX906_REMOSAIC_FULL_8160x6120_30FPS,
	SENSOR_IMX906_REMOSAIC_FULL_8160x4592_30FPS,

	/* Default Video DCG on mode */
	SENSOR_IMX906_4080X3060_30FPS_DSG_R12, // 8
	SENSOR_IMX906_4080X2296_30FPS_DSG_R12,

	/* seamless mode */
	SENSOR_IMX906_4080X3060_30FPS_REMOSAIC_CROP_R12, // 10
	SENSOR_IMX906_4080X2296_30FPS_REMOSAIC_CROP_R12,
	SENSOR_IMX906_4080X3060_24FPS_LN4_R12,
	SENSOR_IMX906_4080X2296_30FPS_LN4_R12,
	SENSOR_IMX906_MODE_MAX,
};

#define MODE_GROUP_NONE (-1)
enum sensor_imx906_mode_group_enum {
	SENSOR_IMX906_MODE_NORMAL,
	SENSOR_IMX906_MODE_RMS_CROP,
	SENSOR_IMX906_MODE_DSG,
	SENSOR_IMX906_MODE_LN4,
	SENSOR_IMX906_MODE_MODE_GROUP_MAX
};
u32 sensor_imx906_mode_groups[SENSOR_IMX906_MODE_MODE_GROUP_MAX];

const u32 sensor_imx906_rms_binning_ratio[SENSOR_IMX906_MODE_MAX] = {
	[SENSOR_IMX906_4080X3060_30FPS_REMOSAIC_CROP_R12] = 1000,
	[SENSOR_IMX906_4080X2296_30FPS_REMOSAIC_CROP_R12] = 1000,
};

struct sensor_imx906_private_data {
	const struct sensor_regs global;
};

struct sensor_imx906_private_runtime {
	unsigned int lte_vsync;
};

static const struct sensor_reg_addr sensor_imx906_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0x3161,
	.cit = 0x0202,
	.cit_shifter = 0x3160,
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};

#endif
