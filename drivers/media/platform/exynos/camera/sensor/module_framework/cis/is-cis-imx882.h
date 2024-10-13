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

#ifndef IS_CIS_IMX882_H
#define IS_CIS_IMX882_H

#include "is-cis.h"

#define IMX882_SETUP_MODE_SELECT_ADDR      (0x0100)
#define IMX882_GROUP_PARAM_HOLD_ADDR       (0x0104)
#define IMX882_EBD_CONTROL_ADDR            (0x3970)
#define IMX882_ABS_GAIN_GR_SET_ADDR        (0x0B8E)

#define CIS_QSC_CAL     1
#define CIS_SPC_CAL     1

#if IS_ENABLED(CIS_QSC_CAL)
#define IMX882_QSC_CAL_ADDR      (0x01A0)
#define IMX882_QSC_ADDR          (0xC000)
#define IMX882_QSC_SIZE          (3072)
#endif
#if IS_ENABLED(CIS_SPC_CAL)
#define IMX882_SPC_CAL_ADDR      (0x0DA0)
#define IMX882_SPC_CAL_SIZE      (384)
#define IMX882_SPC_1ST_ADDR      (0xD200)
#define IMX882_SPC_2ND_ADDR      (0xD300)
#endif

#define IMX882_REMOSAIC_ZOOM_RATIO_X_2	20	/* x2.0 = 20 */

/*
 * Sensor Modes
 *
 * - 2x2 BIN For Still Preview / Capture -
 *    [ 0 ] 2x2 Binning mode A 4624x3468 30fps       : Single Still Preview/Capture (4:3)
 *    [ 1 ] 2x2 Binning mode B 4624x2604 30fps       : Single Still Preview/Capture (16:9)
 *    [ 2 ] 2x2 Binning mode D 4000x3000 30fps       : Single Still Preview/Capture (4:3)
 *
 * - 2x2 BIN V2H2 For HighSpeed Recording/FastAE-
 *    [ 3 ] 2x2 Binning mode V2H2 2304X1728 120fps   : FAST AE (4:3)
 *
 * - 2x2 BIN V2H2 For HighSpeed Recording -
 *    [ 4 ] 2x2 Binning mode V2H2 2000X1128 240fps   : High Speed Recording (16:9)
 *
 * - 2x2 BIN FHD Recording
 *    [ 5 ] 2x2 Binning V2H2 4624x2604  60fps        : FHD Recording (16:9)
 *
 * - Remosaic For Single Still Remosaic Capture -
 *    [ 6 ] Remosaic Full 9248x6936 12fps            : Single Still Remosaic Capture (4:3)
 *    [ 7 ] Remosaic Crop 4624x3468 30fps            : Single Still Remosaic Capture (4:3)
 *    [ 8 ] Remosaic Crop 4624x2604 30fps            : Single Still Remosaic Capture (16:9)
 */

enum sensor_imx882_mode_enum {
	SENSOR_IMX882_VBIN_4080X3060_30FPS = 0,
	SENSOR_IMX882_VBIN_4080X2296_30FPS,
	SENSOR_IMX882_QBIN_4080X2296_60FPS,
	SENSOR_IMX882_FULL_8160X6120_15FPS,
	SENSOR_IMX882_FULL_8160X4592_15FPS,
	SENSOR_IMX882_VBIN_V2H2_2040X1532_30FPS,
	SENSOR_IMX882_VBIN_V2H2_2040X1532_120FPS,
	SENSOR_IMX882_VBIN_V2H2_2040X1148_120FPS,
	SENSOR_IMX882_VBIN_V2H2_2040X1148_240FPS,
	SENSOR_IMX882_FULL_CROP_4080X3060_30FPS,
	SENSOR_IMX882_FULL_CROP_4080X2296_30FPS,
	SENSOR_IMX882_VBIN_4080X3060_30FPS_R12,
	SENSOR_IMX882_VBIN_4080X2296_30FPS_R12,
	SENSOR_IMX882_MODE_MAX,
};

#define MODE_GROUP_NONE (-1)
enum sensor_imx882_mode_group_enum {
	SENSOR_IMX882_MODE_DEFAULT,
	SENSOR_IMX882_MODE_RMS_CROP,
	SENSOR_IMX882_MODE_MODE_GROUP_MAX
};
u32 sensor_imx882_mode_groups[SENSOR_IMX882_MODE_MODE_GROUP_MAX];

const u32 sensor_imx882_rms_binning_ratio[SENSOR_IMX882_MODE_MAX] = {
	[SENSOR_IMX882_FULL_CROP_4080X3060_30FPS] = 1000,
	[SENSOR_IMX882_FULL_CROP_4080X2296_30FPS] = 1000,
};

struct sensor_imx882_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_imx882_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0x3161,
	.cit = 0x0202,
	.cit_shifter = 0x3160,
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};

#endif
