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

#ifndef _CAM_SENSOR_RETENTION_H_
#define _CAM_SENSOR_RETENTION_H_

#if defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
struct cam_sensor_i2c_reg_array stream_on_setting[] = {
	{ 0x0100,	0x0103, 0x00,	0x00 },
};
#elif defined(CONFIG_SEC_B0Q_PROJECT)
struct cam_sensor_i2c_reg_array stream_on_setting[] = {
	{ 0x0100,	0x0100, 0x00,	0x00 },
};
#endif

struct cam_sensor_i2c_reg_setting stream_on_settings[] =  {
	{	stream_on_setting,
		ARRAY_SIZE(stream_on_setting),
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		0
	},
};

#if defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
struct cam_sensor_i2c_reg_array stream_off_setting[] = {
	{ 0x010E,	0x0100, 0x00,	0x00 },
	{ 0x0100,	0x0003, 0x00,	0x00 },
};

struct cam_sensor_i2c_reg_array aeb_off_setting[] = {
};
#elif defined(CONFIG_SEC_B0Q_PROJECT)
struct cam_sensor_i2c_reg_array stream_off_setting[] = {
	{ 0x0100,	0x0000, 0x00,	0x00 },
};

struct cam_sensor_i2c_reg_array aeb_off_setting[] = {
	{ 0x0b30,	0x0111, 0x00,	0x00 },
	{ 0x0e00,	0x0023, 0x00,	0x00 },
	{ 0x6028,	0x2000, 0x00,	0x00 },
	{ 0x602A,	0x1664, 0x00,	0x00 },
	{ 0x6F12,	0x0002, 0x00,	0x00 },
};
#endif

struct cam_sensor_i2c_reg_setting stream_off_settings[] =  {
	{	stream_off_setting,
		ARRAY_SIZE(stream_off_setting),
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		0
	},
};

struct cam_sensor_i2c_reg_setting aeb_off_settings[] = {
	{	aeb_off_setting,
		ARRAY_SIZE(aeb_off_setting),
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		0
	},
};


#if defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
struct cam_sensor_i2c_reg_array retention_enable_setting[] = {
	{ 0xFCFC,	0x4000, 0x00,	0x00 },
	{ 0x6000,	0x0005, 0x00,	0x00 },
	{ 0x010E,	0x0100, 0x00,	0x00 },
	{ 0x19C2,	0x0000, 0x00,	0x00 },
	{ 0x6000,	0x0085, 0x00,	0x00 },
};
#elif defined(CONFIG_SEC_B0Q_PROJECT)
struct cam_sensor_i2c_reg_array retention_enable_setting[] = {
	{ 0x010E,	0x0100, 0x00,	0x00 },
	{ 0x19C2,	0x0000, 0x00,	0x00 },
};
#endif

struct cam_sensor_i2c_reg_setting retention_enable_settings[] =  {
	{	retention_enable_setting,
		ARRAY_SIZE(retention_enable_setting),
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		0
	},
};

#if defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
struct cam_sensor_i2c_reg_array retention_prepare_setting[] = {
	{ 0xFCFC,	0x4000, 0x00,	0x00 },
	{ 0x6000,	0x0005, 0x00,	0x00 },
	{ 0x19C4,	0x0000, 0x00,	0x00 },
	{ 0x6000,	0x0085, 0x00,	0x00 },
};

struct cam_sensor_i2c_reg_setting retention_prepare_settings[] =  {
	{	retention_prepare_setting,
		ARRAY_SIZE(retention_prepare_setting),
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		0
	},
};
#endif

#endif /* _CAM_SENSOR_RETENTION_H_ */
