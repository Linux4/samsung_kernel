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

#define SENSOR_JN1_MAX_WIDTH          (8160 + 32)
#define SENSOR_JN1_MAX_HEIGHT         (6144 + 32)

#define SENSOR_JN1_BURST_WRITE        (0)

/* Related Sensor Parameter */
#define USE_GROUP_PARAM_HOLD                      (0)
#define TOTAL_NUM_OF_IVTPX_CHANNEL                (1)

#define SENSOR_JN1_FINE_INTEGRATION_TIME                    (0x0100)    /* FINE_INTEG_TIME is a fixed value (0x0200: 16bit - defailt value is 0x0100) */
#define SENSOR_JN1_COARSE_INTEGRATION_TIME_MIN_FULL         (5) /* FULL Mode */
#define SENSOR_JN1_COARSE_INTEGRATION_TIME_MIN_2SUM         (3) /* 2 Binning Mode */
#define SENSOR_JN1_COARSE_INTEGRATION_TIME_MIN_4SUM         (3) /* 4 Binning Mode 120 fps */
#define SENSOR_JN1_COARSE_INTEGRATION_TIME_MIN_4SUM_240     (4) /* 4 Binning Mode 240 fps */
#define SENSOR_JN1_COARSE_INTEGRATION_TIME_MAX_MARGIN       (5)

#define SENSOR_JN1_FRAME_LENGTH_LINE_ADDR      (0x0340)
#define SENSOR_JN1_LINE_LENGTH_PCK_ADDR        (0x0342)
#define SENSOR_JN1_FINE_INTEG_TIME_ADDR        (0x0200)
#define SENSOR_JN1_COARSE_INTEG_TIME_ADDR      (0x0202)
#define SENSOR_JN1_LONG_COARSE_INTEG_TIME_ADDR (0x021E)

#define SENSOR_JN1_ANALOG_GAIN_ADDR            (0x0204)
#define SENSOR_JN1_DIG_GAIN_ADDR               (0x020E)
#define SENSOR_JN1_MIN_ANALOG_GAIN_ADDR        (0x0084)
#define SENSOR_JN1_MAX_ANALOG_GAIN_ADDR        (0x0086)
#define SENSOR_JN1_MIN_DIG_GAIN_ADDR           (0x1084)
#define SENSOR_JN1_MAX_DIG_GAIN_ADDR           (0x1086)

#define SENSOR_JN1_OTP_PAGE_SETUP_ADDR         (0x0A02)
#define SENSOR_JN1_OTP_READ_TRANSFER_MODE_ADDR (0x0A00)
#define SENSOR_JN1_OTP_STATUS_REGISTER_ADDR    (0x0A01)
#define SENSOR_JN1_OTP_CHIP_REVISION_ADDR      (0x0002)

/* Related EEPROM CAL : WRITE_SENSOR_CAL_FOR_GGC */
#define SENSOR_JN1_GGC_CAL_BASE_REAR           (0x2A36)
#define SENSOR_JN1_GGC_CAL_SIZE                (92)
#define SENSOR_JN1_GGC_REG_ADDR                (0x39DA)

/* Related Function Option */
#define WRITE_SENSOR_CAL_FOR_GGC                (0)
#define SENSOR_JN1_CAL_DEBUG                    (0)
#define SENSOR_JN1_DEBUG_INFO                   (0)

#define SENSOR_JN1_GGC_DUMP_NAME                "/data/vendor/camera/JN1_GGC_DUMP.bin"

/*
 * [Mode Information]
 *
 * Reference File : S5KJN1SP_EVT0_Setfile_Ver0.6_210809.xlsx
 * Update Data    : 2021-10-13
 * Author         : g.lakshay
 *
 * - Remosaic Full For Single Still Remosaic Capture -
 *    [  0 ] FULL : Remosaic Full 8000x6000 10fps       : Single Still Remosaic Capture (4:3)
 *
 * - 2x2 BIN For Still Preview / Capture -
 *    [  1 ] 2SUM2BIN : 2 Binning Full 4000x3000 30fps    : Single Still Preview/Capture (4:3)
 *    [  2 ] 2SUM2BIN : 2 Binning Crop 4000X2256 30fps    : Single Still Preview/Capture (16:9)
 *    [  3 ] 2SUM2BIN : 2 Binning Crop 4000X1844 30fps    : Single Still Preview/Capture (19.5:9)
 *    [  4 ] 2SUM2BIN : 2 Binning Crop 3680X3000 30fps    : MMS (11:9)
 *    [  5 ] 2SUM2BIN : 2 Binning Crop 3000X3000 30fps    : Single Still Preview/Capture (1:1)
 *    [  6 ] 4SUM4BIN : 4 Binning Full 2000X1500 94fps    : Fast-AE (2000x1500@94fps)
 *    [  7 ] 4SUM4BIN : 4 Binning Crop 2000X1128 120fps   : SlowMotion (1280x720@120fps)
 *    [  8 ] 4SUM4BIN : 4 Binning Crop 1920X1080 240fps   : SlowMotion (1280x720@240fps)
 *
 */

enum sensor_jn1_mode_enum {
	/* 2SUM2BIN Binnig 30Fps */
	SENSOR_JN1_2X2BIN_FULL_4000X3000_30FPS = 0,
	SENSOR_JN1_2X2BIN_CROP_4000X2256_30FPS,
	SENSOR_JN1_2X2BIN_CROP_4000X1844_30FPS,
	SENSOR_JN1_2X2BIN_CROP_3664X3000_30FPS,
	SENSOR_JN1_2X2BIN_CROP_3000X3000_30FPS,

	/* 4SUM4BIN Binnig */
	SENSOR_JN1_4X4BIN_FULL_4080X3060_30FPS,
	SENSOR_JN1_4X4BIN_CROP_2000X1500_94FPS,
	SENSOR_JN1_4X4BIN_CROP_2000X1128_120FPS,
	SENSOR_JN1_4X4BIN_CROP_1920X1080_240FPS,

	/* Remosaic */
	JN1_MODE_REMOSAIC_START,
	SENSOR_JN1_REMOSAIC_FULL_9248x6932_12FPS = JN1_MODE_REMOSAIC_START,
	SENSOR_JN1_REMOSAIC_CROP_4624x3468_30FPS,
	SENSOR_JN1_REMOSAIC_CROP_4624x2604_30FPS,
	JN1_MODE_REMOSAIC_END = SENSOR_JN1_REMOSAIC_CROP_4624x2604_30FPS,
};

#undef DISABLE_JN1_PDAF_TAIL_MODE

#endif

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
