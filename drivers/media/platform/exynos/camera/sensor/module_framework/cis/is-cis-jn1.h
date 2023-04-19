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

#ifndef IS_CIS_JN1_H
#define IS_CIS_JN1_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

/* Related Sensor Parameter */
#define SENSOR_JN1_BURST_WRITE        (0)
#define USE_GROUP_PARAM_HOLD          (0)

/* Related Function Option */
#define SENSOR_JN1_CAL_DEBUG                    (0)
#define SENSOR_JN1_DEBUG_INFO                   (0)
#define WRITE_SENSOR_CAL_FOR_HW_GGC             (1)

/* Related EEPROM CAL : WRITE_SENSOR_CAL_FOR_HW_GGC */
#define SENSOR_JN1_HW_GGC_CAL_BASE_REAR        (0x1352)
#define SENSOR_JN1_HW_GGC_CAL_SIZE             (346)

#define SENSOR_JN1_GGC_DUMP_NAME                "/data/vendor/camera/JN1_GGC_DUMP.bin"

/* HACK CODE : To to check */
#define SENSOR_JN1_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL       (65503)
#define SENSOR_JN1_MAX_CIT_LSHIFT_VALUE                        (11)
#define SENSOR_JN1_MIN_FRAME_DURATION                          (100000)

enum sensor_jn1_mode_enum {
	/* 4SUM Binning 30Fps */
	SENSOR_JN1_4SUM_4080x3060_30FPS = 0,
	SENSOR_JN1_4SUM_4080x2296_30FPS,
	SENSOR_JN1_A2A2_2032x1524_120FPS,
	SENSOR_JN1_A2A2_2032x1148_120FPS,

	/* Remosaic */
	JN1_MODE_REMOSAIC_START,
	SENSOR_JN1_FULL_8160x6120_10FPS = JN1_MODE_REMOSAIC_START,
	JN1_MODE_REMOSAIC_END = SENSOR_JN1_FULL_8160x6120_10FPS,
	SENSOR_JN1_A2A2_1920x1080_240FPS,
	SENSOR_JN1_4SUM_3200x1800_60FPS,
	SENSOR_JN1_MODE_MAX,
};

/* IS_REMOSAIC_MODE(struct is_cis *cis) */
#define IS_REMOSAIC(mode) ({						\
	typecheck(u32, mode) && (mode >= JN1_MODE_REMOSAIC_START) &&	\
	(mode <= JN1_MODE_REMOSAIC_END);				\
})

#define IS_REMOSAIC_MODE(cis) ({					\
	u32 m;								\
	typecheck(struct is_cis *, cis);				\
	m = cis->cis_data->sens_config_index_cur;			\
	(m >= JN1_MODE_REMOSAIC_START) && (m <= JN1_MODE_REMOSAIC_END); \
})

#define IS_SEAMLESS_MODE_CHANGE(cis) ({					\
	u32 m;								\
	typecheck(struct is_cis *, cis);				\
	m = cis->cis_data->sens_config_index_cur;			\
	(m == SENSOR_JN1_REMOSAIC_CROP_4624x3468_30FPS		\
	|| m == SENSOR_JN1_REMOSAIC_CROP_4624x2604_30FPS);		\
})

static const struct sensor_reg_addr sensor_jn1_reg_addr = {
	.fll = 0x0340,
	.fll_shifter = 0x0702,
	.cit = 0x0202,
	.cit_shifter = 0x0704,
	.again = 0x0204,
	.dgain = 0x020E,
};
#endif
