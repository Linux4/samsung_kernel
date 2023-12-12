// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/* Tab A9 code for SR-AX6739A-01-762 by liluling at 20230703 start */
#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"
#include "eeprom_i2c_a0902backhi846st_driver.h"
#include "eeprom_i2c_a0904backsc820csst_driver.h"
#define MAX_EEPROM_SIZE_32K 0x8000
#define MAX_EEPROM_SIZE_16K 0x4000

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	{A0902_BACK_HI846ST_SENSOR_ID,0x40,a0902backhi846st_read_region},
	{A0901_BACK_SC800CSLY_SENSOR_ID, 0xA0, Common_read_region},
	{A0903_BACK_C8490XSJ_SENSOR_ID, 0xA0, Common_read_region},
	{A0904_BACK_SC820CSST_SENSOR_ID, 0x6c, a0904backsc820csst_read_region},
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

/* Tab A9 code for SR-AX6739A-01-762 by liluling at 20230703 end */
unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


