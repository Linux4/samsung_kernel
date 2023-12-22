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

#define MAX_EEPROM_SIZE_16K 0x4000

extern unsigned int imgsensor_read_otp_cal(struct i2c_client *client, struct CAM_CAL_SENSOR_INFO sensor_info,
		unsigned int addr, unsigned char *data, unsigned int size);

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	{S5KGM2_SENSOR_ID, 0xB0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3L6_SENSOR_ID, 0x5A, imgsensor_read_otp_cal, MAX_EEPROM_SIZE_16K},
	{GC5035_SENSOR_ID, 0x7E, imgsensor_read_otp_cal, MAX_EEPROM_SIZE_16K},
	{SR846D_SENSOR_ID, 0x42, imgsensor_read_otp_cal, MAX_EEPROM_SIZE_16K},
	{GC02M1B_SENSOR_ID, 0x6E, imgsensor_read_otp_cal},
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


