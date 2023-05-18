/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

unsigned int Common_read_otp_cal(struct i2c_client *client,
				struct CAM_CAL_SENSOR_INFO sensor_info,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);
#endif				/* __CAM_CAL_LIST_H */
