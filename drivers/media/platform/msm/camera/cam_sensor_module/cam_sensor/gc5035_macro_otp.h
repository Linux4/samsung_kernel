/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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

#ifndef GC5035_MACRO_OTP_H
#define GC5035_MACRO_OTP_H

#define DD_AUTOLOAD_TOTAL_NUM_BAD_PIXEL (0x6c)

/*GC5035 OTP DD Autoload Write Register Addresses, Register Data, Delay and Data Mask*/

static struct cam_sensor_i2c_reg_array dd_autoload_step1_reg_arr[] = {

	{0xfa, 0x10, 0x00, 0x00},
	{0xfe, 0x02, 0x00, 0x00},
	{0x67, 0xc0, 0x00, 0x00}, //Enable OTP and OTP CLK
	{0x55, 0x80, 0x00, 0x00},
	{0x65, 0x80, 0x00, 0x00},
	{0x66, 0x03, 0x00, 0x00},

};

static struct cam_sensor_i2c_reg_array dd_autoload_step3_reg_arr[] = {

	{0xfe, 0x02, 0x00, 0x00},
	{0xbe, 0x00, 0x00, 0x00}, //Allow DD Write
	{0xa9, 0x01, 0x00, 0x00}, //Clear SRAM

};

static struct cam_sensor_i2c_reg_array dd_autoload_defect_qty1_reg_arr[] = {

	{0x69, 0x00, 0x00, 0x00},
	{0x6a, 0x70, 0x00, 0x00},
	{0xf3, 0x20, 0x00, 0x00},

};

static struct cam_sensor_i2c_reg_array dd_autoload_defect_qty2_reg_arr[] = {

	{0x6a, 0x78, 0x00, 0x00},
	{0xf3, 0x20, 0x00, 0x00},

};

static struct cam_sensor_i2c_reg_array dd_autoload_step4_reg_arr[] = {

	{0x03, 0x00, 0x00, 0x00},
	{0x04, 0x80, 0x00, 0x00}, //otp bad pixel start position

};

static struct cam_sensor_i2c_reg_array dd_autoload_step5_reg_arr[] = {

	{0x09, 0x33, 0x00, 0x00},
	{0xf3, 0x80, 0x00, 0x00}, // auto load start pulse

};

static struct cam_sensor_i2c_reg_array dd_autoload_step7_reg_arr[] = {

	{0xfe, 0x02, 0x00, 0x00},
	{0xbe, 0x01, 0x00, 0x00},
	{0x09, 0x00, 0x00, 0x00}, //close auto load
	{0x67, 0x00, 0x00, 0x00},
	{0xfe, 0x01, 0x00, 0x00},
	{0x80, 0x02, 0x00, 0x00}, /* [1]DD_EN */
	{0xfe, 0x00, 0x00, 0x00},

};

#endif /* GC5035_OTP_H */