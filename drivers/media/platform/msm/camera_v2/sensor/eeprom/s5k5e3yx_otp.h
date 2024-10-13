/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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

#ifndef S5K5E3YX_OTP_H
#define S5K5E3YX_OTP_H

static struct msm_camera_i2c_reg_array load_s5k5e3yx_otp_setfile_reg[] = {
	{0x0A00, 0x00},
};

static struct msm_camera_i2c_reg_array init_write_s5k5e3yx_otp_reg[] = {
	{0x3b42, 0x68},
	{0x3b41, 0x01},
	{0x3b40, 0x00},
	{0x3b45, 0x02},
	{0x0A00, 0x04},
	{0x0A00, 0x03},
	{0x3b42, 0x00},
	{0x0A02, 0x02},
	{0x0A00, 0x03},
};

static struct msm_camera_i2c_reg_array finish_write_s5k5e3yx_otp_reg[] = {
	{0x0A00, 0x04},
	{0x0A00, 0x00},
	{0x3b40, 0x01},
};

static struct msm_camera_i2c_reg_array init_read_s5k5e3yx_otp_reg[] = {
	{0x0A00, 0x04},
	{0x0A02, 0x02},
	{0x0A00, 0x01},
};

static struct msm_camera_i2c_reg_array finish_read_s5k5e3yx_otp_reg[] = {
	{0x0A00, 0x04},
	{0x0A00, 0x00},
};

#endif /* S5K5E3YX_OTP_H */
