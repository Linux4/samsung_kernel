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

#ifndef IS_CIS_IMX766_H
#define IS_CIS_IMX766_H

#include "is-cis.h"

#define IMX766_SETUP_MODE_SELECT_ADDR      (0x0100)
#define IMX766_GROUP_PARAM_HOLD_ADDR       (0x0104)
#define IMX766_EBD_CONTROL_ADDR            (0x3960)
#define IMX766_ABS_GAIN_GR_SET_ADDR        (0x0B8E)

/* Related Function Option */
#define SENSOR_IMX766_WRITE_SENSOR_CAL            (1)

/* Related EEPROM CAL */
#define IMX766_QSC_BASE_REAR     (0x01D0)
#define IMX766_QSC_SIZE          (3072)
#define IMX766_QSC_ADDR          (0xC800)

#define IMX766_REMOSAIC_ZOOM_RATIO_X_2	20	/* x2.0 = 20 */

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

enum sensor_imx766_mode_enum {
	SENSOR_IMX766_4080X3060_30FPS,
	SENSOR_IMX766_4080X2296_30FPS,
	SENSOR_IMX766_2032X1524_120FPS,
	SENSOR_IMX766_3840x2160_60FPS,
	SENSOR_IMX766_1920x1080_240FPS,
	/* Remosaic */
	SENSOR_IMX766_REMOSAIC_FULL_8160x6120_10FPS,

	SENSOR_IMX766_4080X3060_30FPS_FCM,
	SENSOR_IMX766_4080X2296_30FPS_FCM,
	SENSOR_IMX766_REMOSAIC_12MPCROP_4080x3060_30FPS_FCM,
	SENSOR_IMX766_REMOSAIC_8MPCROP_4080x2296_30FPS_FCM,
	SENSOR_IMX766_MODE_MAX,
};

#define MODE_GROUP_NONE (-1)
enum sensor_imx766_mode_group_enum {
	SENSOR_IMX766_MODE_DEFAULT,
	SENSOR_IMX766_MODE_RMS_CROP,
	SENSOR_IMX766_MODE_MODE_GROUP_MAX
};
u32 sensor_imx766_mode_groups[SENSOR_IMX766_MODE_MODE_GROUP_MAX];

const u32 sensor_imx766_rms_binning_ratio[SENSOR_IMX766_MODE_MAX] = {
	[SENSOR_IMX766_REMOSAIC_12MPCROP_4080x3060_30FPS_FCM] = 1000,
	[SENSOR_IMX766_REMOSAIC_8MPCROP_4080x2296_30FPS_FCM] = 1000,
};

struct sensor_imx766_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_imx766_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0, /* not supported */
	.cit = 0x0202,
	.cit_shifter = 0x3128,
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};

#endif
