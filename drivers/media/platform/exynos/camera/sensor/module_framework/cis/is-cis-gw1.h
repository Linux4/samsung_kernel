/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_CIS_GW1_H
#define IS_CIS_GW1_H

#include "is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_GW1_MAX_WIDTH		(9216)
#define SENSOR_GW1_MAX_HEIGHT		(6912)

/* TODO: Check below values are valid */
#define SENSOR_GW1_FINE_INTEGRATION_TIME_MIN                0x0
#define SENSOR_GW1_FINE_INTEGRATION_TIME_MAX                0x0 /* Not used */
#define SENSOR_GW1_COARSE_INTEGRATION_TIME_MIN              0x2 /* TODO */
#define SENSOR_GW1_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x6 /* TODO */

#define USE_GROUP_PARAM_HOLD	(0)

static const u32 sensor_gw1_cis_dual_master_settings[] = {
	0x0A70, 0x0001, 2,
	0x0A72, 0x0100, 2,
	0x0A7A, 0x0001, 2,
	0x6028, 0x2000, 2,
	0x602A, 0x1434, 2,
	0x6F12, 0x0300, 2,
	0x6028, 0x2001, 2,
	0x602A, 0xF146, 2,
	0x6F12, 0x0100, 2,
	0x6F12, 0x0100, 2,
};

static const u32 sensor_gw1_cis_dual_master_settings_size =
	ARRAY_SIZE(sensor_gw1_cis_dual_master_settings);

static const u32 sensor_gw1_cis_dual_slave_settings[] = {
	0x6028, 0x4000, 2,
	0x0A70, 0x0001, 2,
	0x0A72, 0x0000, 2,
	0x0A72, 0x0001, 2,
	0x0A76, 0x0001, 2,
	0x0A80, 0x0018, 2,
	0x6028, 0x2000, 2,
	0x602A, 0x13A8, 2,
	0x6F12, 0x1408, 2,
	0x602A, 0x13AA, 2,
	0x6F12, 0x0101, 2,
	0x602A, 0x13B0, 2,
	0x6F12, 0x0108, 2,
	0x602A, 0x1432, 2,
	0x6F12, 0x0305, 2,
	0x6028, 0x2001, 2,
	0x602A, 0xF146, 2,
	0x6F12, 0x0101, 2,
};

static const u32 sensor_gw1_cis_dual_slave_settings_size =
	ARRAY_SIZE(sensor_gw1_cis_dual_slave_settings);

static const u32 sensor_gw1_cis_dual_standalone_settings[] = {
	0x6028, 0x4000, 2,
	0x0A70, 0x0000, 2,
};

static const u32 sensor_gw1_cis_dual_standalone_settings_size =
	ARRAY_SIZE(sensor_gw1_cis_dual_standalone_settings);

#endif
