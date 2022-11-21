// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define MAX_EEPROM_SIZE_16K 0x4000

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	//+bug 621775 liuxiangyin.wt, add, 2021/3/5, n21 1st supply sub camera otp porting
	{N21_TXD_MAIN_S5K2P6_SENSOR_ID	,	0xA0, Common_read_region},
	{N21_SHINE_SUB_HI1336_SENSOR_ID ,	0xA0, Common_read_region},
	//-bug 621775 liuxiangyin.wt, add, 2021/3/5, n21 1st supply sub camera otp porting
	//+bug 621775 lintaicheng.wt, add, 2021/03/05, add for n21 otp bring up
	{N21_HLT_MAIN_OV16B10_SENSOR_ID	,	0xA0, Common_read_region},
	{N21_TXD_SUB_S5K3L6_SENSOR_ID ,		0xA0, Common_read_region},
	//-bug 621775 lintaicheng.wt, add, 2021/03/05, add for n21 otp bring up
	//+bug 621775 huangzheng1.wt, add, 2021/03/08, add for n21 otp bring up
	{N21_CXT_MICRO_GC2375H_SENSOR_ID,	0xA4, Common_read_region},
	{N21_HLT_MICRO_GC2375H_SENSOR_ID,	0xA4, Common_read_region},
	{N21_SHINE_WIDE_GC8034W_SENSOR_ID,	0xA2, Common_read_region},
	{N21_HLT_WIDE_GC8034W_SENSOR_ID,		0xA2, Common_read_region},
	//-bug 621775 huangzheng1.wt, add, 2021/03/08, add for n21 otp bring up
	//+bug 612420,huangguoyong.wt,add,2020/12/24,add for n6 camera bring up
	/*N6Q*/
    //+bug 612420,zhanghao2.wt,add,2020/12/30,add for n6 otp bring up
	{N8_HI1336_XL_SENSOR_ID, 0xA0, Common_read_region},
	{N8_S5K3L6_HLT_SENSOR_ID, 0xA0, Common_read_region},
	{N8_HI1336_TXD_JCT_SENSOR_ID,	        0xA0, Common_read_region},
    {N8_HI1336_XL_JCT_SENSOR_ID,           0xA0, Common_read_region},
	//-bug 612420,huangguoyong.wt,add,2020/12/30,add for n6 camera bring up

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


