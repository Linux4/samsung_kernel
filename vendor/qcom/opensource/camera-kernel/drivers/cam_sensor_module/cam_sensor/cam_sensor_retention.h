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

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
struct cam_sensor_i2c_reg_array stream_on_setting[] = {
	{ 0x0100,	0x0103, 0x00,	0x00 },
};
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
struct cam_sensor_i2c_reg_array stream_on_setting[] = {
	{ 0x0100,	0x0103, 0x00,	0x00 },
};
#elif defined(CONFIG_SEC_B5Q_PROJECT)
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

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
struct cam_sensor_i2c_reg_array stream_off_setting[] = {
	{ 0x010E,	0x0100, 0x00,	0x00 },
	{ 0x0100,	0x0003, 0x00,	0x00 },
};
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
struct cam_sensor_i2c_reg_array stream_off_setting[] = {
	{ 0xFCFC,	0x4000, 0x00,	0x00 },
	{ 0x0100,	0x0003, 0x00,	0x00 },
};
#elif defined(CONFIG_SEC_B5Q_PROJECT)
struct cam_sensor_i2c_reg_array stream_off_setting[] = {
	{ 0x0100,	0x0000, 0x00,	0x00 },
};

struct cam_sensor_i2c_reg_array aeb_off_setting[] = {
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


#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
struct cam_sensor_i2c_reg_array retention_enable_setting[] = {
	{ 0xFCFC,	0x4000, 0x00,	0x00 },
	{ 0x6000,	0x0005, 0x00,	0x00 },
	{ 0x010E,	0x0100, 0x00,	0x00 },
	{ 0x19C2,	0x0000, 0x00,	0x00 },
	{ 0x6000,	0x0085, 0x00,	0x00 },
};
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
struct cam_sensor_i2c_reg_array retention_enable_setting[] = {
	{ 0xFCFC,	0x4000, 0x00,	0x00 },
	{ 0x0B30,	0x01FF, 0x00,	0x00 },
};
#elif defined(CONFIG_SEC_B5Q_PROJECT)
struct cam_sensor_i2c_reg_array retention_enable_setting[] = {
	{ 0x6028,	0x3000, 0x00,	0x00 },
	{ 0x602A,	0x0484, 0x00,	0x00 },
	{ 0x6F12,	0x0100, 0x00,	0x00 },
	{ 0x010E,	0x0100, 0x00,	0x00 },
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

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
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
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
struct cam_sensor_i2c_reg_array retention_prepare1_setting[] = {
	{ 0xFCFC,	0x4000, 0x00,	0x00 },
	{ 0x6018,	0x0001, 0x01,	0x00 },
};

struct cam_sensor_i2c_reg_array retention_prepare2_setting[] = {
	{ 0x652A,	0x0001, 0x00,	0x00 },
	{ 0x7096,	0x0001, 0x00,	0x00 },
	{ 0x7002,	0x0008, 0x00,	0x00 },
	{ 0x706E,	0x0D13, 0x00,	0x00 },
	{ 0x6028,	0x1001, 0x00,	0x00 },
	{ 0x602A,	0xC990, 0x00,	0x00 },
	{ 0x6F12,	0x1002, 0x00,	0x00 },
	{ 0x6F12,	0xF601, 0x00,	0x00 },
	{ 0x6028,	0x1002, 0x00,	0x00 },
	{ 0x602A,	0xC8C0, 0x00,	0x00 },
	{ 0x6F12,	0xCAFE, 0x00,	0x00 },
	{ 0x6F12,	0x1234, 0x00,	0x00 },
	{ 0x6F12,	0xABBA, 0x00,	0x00 },
	{ 0x6F12,	0x0345, 0x00,	0x00 },
	{ 0x6014,	0x0001, 0x00,	0x00 },
};

struct cam_sensor_i2c_reg_setting retention_prepare_settings[] =  {
	{	retention_prepare1_setting,
		ARRAY_SIZE(retention_prepare1_setting),
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		5
	},
	{	retention_prepare2_setting,
		ARRAY_SIZE(retention_prepare2_setting),
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		10
	},
};

struct cam_sensor_i2c_reg_array retention_hw_init_setting[] = {
	{ 0xFCFC,	0x4000, 0x00,	0x00 },
	{ 0x6214,	0xE949, 0x00,	0x00 },
	{ 0x6218,	0xE940, 0x00,	0x00 },
	{ 0x6222,	0x0000, 0x00,	0x00 },
	{ 0x621E,	0x00F0, 0x00,	0x00 },
};

struct cam_sensor_i2c_reg_setting retention_hw_init_settings[] =  {
	{	retention_hw_init_setting,
		ARRAY_SIZE(retention_hw_init_setting),
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		0
	},
};

struct cam_sensor_i2c_reg_array retention_checksum_setting[] = {
	{ 0x602C,	0x1002, 0x01,	0x00 },
	{ 0x602E,	0xF36C, 0x00,	0x00 },
};

struct cam_sensor_i2c_reg_setting retention_checksum_settings[] =  {
	{	retention_checksum_setting,
		ARRAY_SIZE(retention_checksum_setting),
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		0
	},
};

struct cam_sensor_i2c_reg_array retention_ready_setting[] = {
	{ 0x602C,	0x1002, 0x00,	0x00 },
	{ 0x602E,	0xF36E, 0x00,	0x00 },
};

struct cam_sensor_i2c_reg_setting retention_ready_settings[] =  {
	{	retention_ready_setting,
		ARRAY_SIZE(retention_ready_setting),
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		0
	},
};
#elif defined(CONFIG_SEC_B5Q_PROJECT)
struct cam_sensor_i2c_reg_array retention_prepare_setting[] = {
	{ 0x6028,	0x3000, 0x00,	0x00 },
	{ 0x602A,	0x0484, 0x00,	0x00 },
	{ 0x6F12,	0x0100, 0x00,	0x00 },
	{ 0x010E,	0x0100, 0x00,	0x00 },
	{ 0x0BCC,	0x0000, 0x00,	0x00 },
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
