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

extern unsigned int imgsensor_read_otp_cal(struct i2c_client *client,
		struct stCAM_CAL_INFO_STRUCT *sensor_info, unsigned int addr, unsigned char *data, unsigned int size);

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	// Temp code for IMX582 without OIS
	//{IMX582_SENSOR_ID,  0xB0, Common_read_region, MAX_EEPROM_SIZE_16K},
	// IMX582 with OIS
	{IMX582_SENSOR_ID,  0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX258F_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX355_SENSOR_ID,  0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{GC02M1B_SENSOR_ID, 0x6E, imgsensor_read_otp_cal},
	{GC02M1_SENSOR_ID,  0xA4, Common_read_region, MAX_EEPROM_SIZE_16K},
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
