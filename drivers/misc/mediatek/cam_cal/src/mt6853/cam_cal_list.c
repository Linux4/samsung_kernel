/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define MAX_OTP_SIZE_4K 0x1000
#define MAX_EEPROM_SIZE_16K 0x4000

extern unsigned int imgsensor_read_otp_cal(struct i2c_client *client, struct CAM_CAL_SENSOR_INFO sensor_info,
		unsigned int addr, unsigned char *data, unsigned int size);

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	{IMX258_SENSOR_ID, 0xB0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{GC5035_SENSOR_ID, 0x6E, Otp_read_region_GC5035, MAX_OTP_SIZE_4K},
	/*Below is commom sensor */
	{IMX586_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K,
		BL24SA64_write_region},
	{IMX576_SENSOR_ID, 0xA2, Common_read_region},
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{IMX319_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3M5SX_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K,
		BL24SA64_write_region},
	{IMX686_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI846_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5KGD1SP_SENSOR_ID, 0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV16A10_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3P9SP_SENSOR_ID, 0xA8, Common_read_region},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	{S5KJD1_SENSOR_ID, 0xB0, Common_read_region, DEFAULT_MAX_EEPROM_SIZE_8K,
		DW9763_write_region},
	{IMX499_SENSOR_ID, 0xA0, Common_read_region},
	{S5KGM2_SENSOR_ID, 0xB0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K4HAYX_SENSOR_ID, 0x5A, Otp_read_region_S5K4HAYX,
		MAX_EEPROM_SIZE_16K},
	{GC02M1_SENSOR_ID, 0xA4, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3L6_SENSOR_ID, 0x5A, Otp_read_region_S5K3L6,
		MAX_EEPROM_SIZE_16K},
#if 0
	{SR846D_SENSOR_ID, 0x42, Otp_read_region_SR846,
		MAX_EEPROM_SIZE_16K},
	{GC5035_SENSOR_ID, 0x7E, Otp_read_region_GC5035,
		MAX_EEPROM_SIZE_16K},
#endif
	{IMX481_SENSOR_ID, 0xA4, Common_read_region, DEFAULT_MAX_EEPROM_SIZE_8K,
		BL24SA64_write_region},
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


