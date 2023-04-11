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

#ifndef IS_CIS_2L3_H
#define IS_CIS_2L3_H

#include "is-cis.h"

#ifdef CONFIG_SENSOR_RETENTION_USE
#undef CONFIG_SENSOR_RETENTION_USE
#endif

#define EXT_CLK_Mhz (26)

#define SENSOR_2L3_MAX_WIDTH		(4000 + 0)
#define SENSOR_2L3_MAX_HEIGHT		(3000 + 0)

#define SENSOR_2L3_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_2L3_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_2L3_COARSE_INTEGRATION_TIME_MIN              0x02
#define SENSOR_2L3_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x10

#ifdef USE_CAMERA_FACTORY_DRAM_TEST
#define SENSOR_2L3_DRAMTEST_SECTION2_FCOUNT	(8)
#endif

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_2l3_mode_enum {
	/* MODE 3 */
	SENSOR_2L3_4000X3000_30FPS = 0,
	SENSOR_2L3_4000X2252_30FPS = 1,
	SENSOR_2L3_4000X2252_60FPS = 2,
	SENSOR_2L3_1984X1488_30FPS = 3,
	SENSOR_2L3_1008X756_120FPS = 4,
	SENSOR_2L3_1280X720_240FPS = 5,
	SENSOR_2L3_1280X720_480FPS = 6,
	SENSOR_2L3_MODE_MAX,
};

static bool sensor_2l3_support_wdr[] = {
	/* MODE 3 */
	false, //SENSOR_2L3_4000X3000_30FPS = 0,
	false, //SENSOR_2L3_4000X2250_30FPS = 1,
	false, //SENSOR_2L3_4000X2250_60FPS = 2,
	false, //SENSOR_2L3_1984X1488_30FPS = 3,
	false, //SENSOR_2L3_1008X756_120FPS = 4,
	false, //SENSOR_2L3_1280X720_240FPS = 5,
	false, //SENSOR_2L3_1280X720_480FPS = 6,
};

/* dual sync - indirect */
static const u32 sensor_2l3_cis_dual_master_settings[] = {
	0xFCFC, 0x4000, 0x02,
	0x6000, 0x0005, 0x02,
	0x30C8, 0x0300, 0x02,
	0x30e8, 0x0010, 0x02,
	0x30e4, 0x0002, 0x02,
	0x30CC, 0x0000, 0x02,
	0x30D8, 0x0000, 0x02,
	0x30CE, 0x0000, 0x02,
	0x30D0, 0x0000, 0x02,
	0x30D2, 0x0000, 0x02,
	0xFCFC, 0x2000, 0x02,
	0xA568, 0x0000, 0x02,
	0xA540, 0x0000, 0x02,
	0xFCFC, 0x4000, 0x02,
	0x6000, 0x0085, 0x02,
};

static const u32 sensor_2l3_cis_dual_master_settings_size =
	ARRAY_SIZE(sensor_2l3_cis_dual_master_settings);

static const u32 sensor_2l3_cis_dual_slave_settings[] = {
	0xFCFC, 0x4000, 0x02,
	0x6000, 0x0005, 0x02,
	0x30C8, 0x0000, 0x02,
	0x30e8, 0x0000, 0x02,
	0x30e4, 0x0000, 0x02,
	0x30CC, 0x0100, 0x02,
	0x30D8, 0x0001, 0x02,
	0x30CE, 0x0100, 0x02,
	0x30D0, 0x0020, 0x02,
	0x30D2, 0x0020, 0x02,
	0xFCFC, 0x2000, 0x02,
	0xA568, 0x0200, 0x02,
	0xA540, 0x0000, 0x02,
	0xFCFC, 0x4000, 0x02,
	0x6000, 0x0085, 0x02,
};

static const u32 sensor_2l3_cis_dual_slave_settings_size =
	ARRAY_SIZE(sensor_2l3_cis_dual_slave_settings);

static const u32 sensor_2l3_cis_dual_single_settings[] = {
	0xFCFC, 0x4000, 0x02,
	0x6000, 0x0005, 0x02,
	0x30C8, 0x0000, 0x02,
	0x30E8, 0x0000, 0x02,
	0x30E4, 0x0000, 0x02,
	0x30CC, 0x0000, 0x02,
	0x30D8, 0x0000, 0x02,
	0x30CE, 0x0000, 0x02,
	0x30D0, 0x0000, 0x02,
	0x30D2, 0x0000, 0x02,
	0xFCFC, 0x2000, 0x02,
	0xA568, 0x0000, 0x02,
	0xA540, 0x0000, 0x02,
	0xFCFC, 0x4000, 0x02,
	0x6000, 0x0085, 0x02,
};

static const u32 sensor_2l3_cis_dual_single_settings_size =
	ARRAY_SIZE(sensor_2l3_cis_dual_single_settings);

int sensor_2l3_cis_stream_on(struct v4l2_subdev *subdev);
int sensor_2l3_cis_stream_off(struct v4l2_subdev *subdev);
int sensor_2l3_cis_set_lownoise_mode_change(struct v4l2_subdev *subdev);
#endif
