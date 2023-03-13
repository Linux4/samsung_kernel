/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

/*hs14 code for SR-AL5628-01-161 Universal macro adaptation by lisizhou at 2022/9/23 start*/
#ifdef CONFIG_HQ_PROJECT_O22
#include "eeprom_i2c_a1402widesc520cstxd_driver.h"
#include "eeprom_i2c_a1401widehi556wly_driver.h"
#endif
/*hs14 code for SR-AL5628-01-161 Universal macro adaptation by lisizhou at 2022/9/23 end*/

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
/*hs14 code for AL6528ADEU-2675 by pengxutao at 2022/11/18 start*/
#ifdef CONFIG_HQ_PROJECT_O22
    {A1401BACKS5KJN1HLT_SENSOR_ID,      0xA0, Common_read_region},
    {A1401FRONTS5K3L6LY_SENSOR_ID,      0xA2, Common_read_region},
    {A1401WIDEHI556WLY_SENSOR_ID,       0x50, a1401widehi556wly_read_region},
    {A1402BACKHI5022QXL_SENSOR_ID,      0xB0, Common_read_region},
    {A1402FRONTGC13A0XL_SENSOR_ID,      0xA2, Common_read_region},
    {A1402WIDESC520CSTXD_SENSOR_ID,     0x6c, a1402widesc520cstxd_read_region},
    {A1403BACKHI5022QLY_SENSOR_ID,      0xA0, Common_read_region},
    {A1403FRONTSC1301CSTXD_SENSOR_ID,   0xA2, Common_read_region},
    {A1403WIDEOV05AHLT_SENSOR_ID,       0xA4, Common_read_region},
    {A1404BACKS5KJN1TXD_SENSOR_ID,      0xA0, Common_read_region},
#endif
/*hs14 code for AL6528ADEU-2675 by pengxutao at 2022/11/18 end*/
	/*Below is commom sensor */
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX338_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4E6_SENSOR_ID, 0xA8, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3M3_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX318_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	/*B+B. No Cal data for main2 OV8856*/
	{S5K2P7_SENSOR_ID, 0xA0, Common_read_region},
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


