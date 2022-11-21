/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __EEPROM_I2C_COMMON_DRIVER_H
#define __EEPROM_I2C_COMMON_DRIVER_H
#include <linux/i2c.h>

unsigned int Common_read_region(struct i2c_client *client,
				struct CAM_CAL_SENSOR_INFO sensor_info,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);

unsigned int Common_write_region(struct i2c_client *client,
				struct CAM_CAL_SENSOR_INFO sensor_info,
				 unsigned int addr,
				 unsigned char *data,
				 unsigned int size);

unsigned int DW9763_write_region(struct i2c_client *client,
				 struct CAM_CAL_SENSOR_INFO sensor_info,
				 unsigned int addr,
				 unsigned char *data,
				 unsigned int size);

unsigned int Otp_read_region_S5K4HAYX(struct i2c_client *client,
				struct CAM_CAL_SENSOR_INFO sensor_info,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);

unsigned int Otp_read_region_S5K3L6(struct i2c_client *client,
				struct CAM_CAL_SENSOR_INFO sensor_info,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);

unsigned int Otp_read_region_SR846(struct i2c_client *client,
				struct CAM_CAL_SENSOR_INFO sensor_info,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);

unsigned int Otp_read_region_GC5035(struct i2c_client *client,
				struct CAM_CAL_SENSOR_INFO sensor_info,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);

unsigned int Otp_read_region_GC02M1B(struct i2c_client *client,
				struct CAM_CAL_SENSOR_INFO sensor_info,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);

unsigned int BL24SA64_write_region(struct i2c_client *client,
				 struct CAM_CAL_SENSOR_INFO sensor_info,
				 unsigned int addr,
				 unsigned char *data,
				 unsigned int size);

#endif				/* __CAM_CAL_LIST_H */
