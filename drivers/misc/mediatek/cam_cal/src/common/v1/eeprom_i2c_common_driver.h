/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __EEPROM_I2C_COMMON_DRIVER_H
#define __EEPROM_I2C_COMMON_DRIVER_H
#include <linux/i2c.h>

unsigned int Common_read_region(struct i2c_client *client,
				struct stCAM_CAL_INFO_STRUCT *sensor_info,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);
unsigned int Otp_read_region_SR846D(struct i2c_client *client,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);
void set_global_i2c_client(struct i2c_client *client);
#endif				/* __CAM_CAL_LIST_H */
