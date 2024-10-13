/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CAM_SENSOR_ADAPTIVE_MIPI_FRONT_TOP_2_0_H_
#define _CAM_SENSOR_ADAPTIVE_MIPI_FRONT_TOP_2_0_H_

#include "cam_sensor_dev.h"

int num_front_top_mipi_setting = 4;

struct cam_sensor_i2c_reg_array MIPI_FRONT_TOP_DUMMY_MHZ_REG_ARRAY[] = {
};

static const struct cam_sensor_i2c_reg_setting sensor_front_top_setfile_dummy_mhz[] = {
    { MIPI_FRONT_TOP_DUMMY_MHZ_REG_ARRAY, ARRAY_SIZE(MIPI_FRONT_TOP_DUMMY_MHZ_REG_ARRAY),
	  CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_WORD, 0 }
};

static const struct cam_mipi_setting sensor_front_top_setfile_dummy_mipi_setting[] = {
	{ "DUMMY Mhz", 0,
	  sensor_front_top_setfile_dummy_mhz, ARRAY_SIZE(sensor_front_top_setfile_dummy_mhz) },
};

static const struct cam_mipi_cell_ratings sensor_front_top_setfile_dummy_channel[] = {
	{ CAM_RAT_BAND(CAM_RAT_1_GSM, CAM_BAND_001_GSM_GSM850), 0, 0, {0} },
};

static const struct cam_mipi_sensor_mode sensor_front_top_mipi_A_mode[] = {
	{
		sensor_front_top_setfile_dummy_channel,	ARRAY_SIZE(sensor_front_top_setfile_dummy_channel),
		sensor_front_top_setfile_dummy_mipi_setting,	ARRAY_SIZE(sensor_front_top_setfile_dummy_mipi_setting)
	},
};

static const struct cam_mipi_sensor_mode sensor_front_top_mipi_B_mode[] = {
	{
		sensor_front_top_setfile_dummy_channel,	ARRAY_SIZE(sensor_front_top_setfile_dummy_channel),
		sensor_front_top_setfile_dummy_mipi_setting,	ARRAY_SIZE(sensor_front_top_setfile_dummy_mipi_setting)
	},
};
static const struct cam_mipi_sensor_mode sensor_front_top_mipi_C_mode[] = {
	{
		sensor_front_top_setfile_dummy_channel,	ARRAY_SIZE(sensor_front_top_setfile_dummy_channel),
		sensor_front_top_setfile_dummy_mipi_setting,	ARRAY_SIZE(sensor_front_top_setfile_dummy_mipi_setting)
	},
};

static const struct cam_mipi_sensor_mode sensor_front_top_mipi_D_mode[] = {
	{
		sensor_front_top_setfile_dummy_channel,	ARRAY_SIZE(sensor_front_top_setfile_dummy_channel),
		sensor_front_top_setfile_dummy_mipi_setting,	ARRAY_SIZE(sensor_front_top_setfile_dummy_mipi_setting)
	},
};
#endif /* _CAM_SENSOR_ADAPTIVE_MIPI_FRONT_TOP_2_0_H_ */
