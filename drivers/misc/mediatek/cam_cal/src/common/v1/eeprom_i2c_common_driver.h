/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __EEPROM_I2C_COMMON_DRIVER_H
#define __EEPROM_I2C_COMMON_DRIVER_H
#include <linux/i2c.h>

unsigned int Common_read_region(struct i2c_client *client,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);
unsigned int Otp_read_region_GC5035_A01(struct i2c_client *client,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);

unsigned int Otp_read_region_SR846(struct i2c_client *client,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);
unsigned int Otp_read_region_SR846D(struct i2c_client *client,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);
unsigned int Otp_read_region_GC5035(struct i2c_client *client,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);
unsigned int Otp_read_region_GC02M1B(struct i2c_client *client,
				unsigned int addr,
				unsigned char *data,
				unsigned int size);
#endif				/* __CAM_CAL_LIST_H */
