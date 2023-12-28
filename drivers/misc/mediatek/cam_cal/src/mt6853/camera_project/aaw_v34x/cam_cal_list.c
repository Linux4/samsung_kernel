/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define MAX_EEPROM_SIZE_16K 0x4000

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	{IMX582_SENSOR_ID,   0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX258_SENSOR_ID,   0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K4HAYX_SENSOR_ID, 0x5A, Common_read_otp_cal},
	{SR846D_SENSOR_ID,   0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{GC5035_SENSOR_ID,   0x7E, Common_read_otp_cal},
	{HI1339_SENSOR_ID,   0x40, Common_read_otp_cal},
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
