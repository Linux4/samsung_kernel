// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 */

#include <linux/i2c.h>

//read functions of OTP cal
int hi847_read_otp_cal(struct i2c_client *client, unsigned char *data, unsigned int size);
int hi1337fu_read_otp_cal(struct i2c_client *client, unsigned char *data, unsigned int ssize);
