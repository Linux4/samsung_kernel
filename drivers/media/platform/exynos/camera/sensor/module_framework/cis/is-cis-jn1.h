/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_JN1_H
#define IS_CIS_JN1_H

#include "is-cis.h"

/* Related Function Option */
//#define SENSOR_JN1_WRITE_SENSOR_CAL            (1)

/* Related EEPROM CAL */
//#define SENSOR_JN1_UXTC_SENS_CAL_ADDR		(0x01A0)
//#define SENSOR_JN1_UXTC_SENS_CAL_SIZE		(7128)
//#define JN1_BURST_WRITE

#define WRITE_SENSOR_CAL_FOR_HW_GGC             (1)
#define SENSOR_JN1_CAL_DEBUG                    (0)

#if WRITE_SENSOR_CAL_FOR_HW_GGC
#define SENSOR_JN1_HW_GGC_CAL_BASE_REAR        (0x13BA)
#define SENSOR_JN1_HW_GGC_CAL_SIZE             (346)
#if SENSOR_JN1_CAL_DEBUG
#define SENSOR_JN1_GGC_DUMP_NAME                "/data/vendor/camera/JN1_GGC_DUMP.bin"
#endif
#endif

#define JN1_REMOSAIC_ZOOM_RATIO_X_2	20	/* x2.0 = 20 */

/*
 * Sensor Modes
 *
 *    [ 0 ] 2x2 Binning mode A 4080x3060 30fps       : Single Still Preview/Capture (4:3)
 *    [ 1 ] 2x2 Binning mode B 4080x2296 30fps       : Single Still Preview/Capture (16:9)
 *    [ 2 ] 2x2 Binning mode D 2032x1524 120fps       : FastAe (4:3)
 *
 */

enum sensor_jn1_mode_enum {
	SENSOR_JN1_4SUM_4080x3060_30FPS = 0,
	SENSOR_JN1_4SUM_4080x2296_30FPS,
	SENSOR_JN1_RMSC_8160x4592_10FPS,
	SENSOR_JN1_RMSC_8160x6120_10FPS,
	SENSOR_JN1_4SUM_3200x1800_60FPS,
	SENSOR_JN1_4SUM_2032x1148_120FPS,
	SENSOR_JN1_4SUM_1920x1080_240FPS,
	SENSOR_JN1_4SUM_2032x1524_120FPS,
	SENSOR_JN1_4SUM_2032x1524_30FPS,
	SENSOR_JN1_4SUM_2032x1148_30FPS,
	SENSOR_JN1_MODE_MAX,
};

#define MODE_GROUP_NONE (-1)
enum sensor_jn1_mode_group_enum {
	SENSOR_JN1_MODE_DEFAULT,
	SENSOR_JN1_MODE_RMS_CROP,
	SENSOR_JN1_MODE_MODE_GROUP_MAX
};
u32 sensor_jn1_mode_groups[SENSOR_JN1_MODE_MODE_GROUP_MAX];

struct sensor_jn1_private_data {
	const struct sensor_regs global;
};

static const struct sensor_reg_addr sensor_jn1_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0x0702,
	.cit = 0x0202,
	.cit_shifter = 0x0704,
	.again = 0x0204,
	.dgain = 0x020E,
	.group_param_hold = 0x0104,
};

#endif
