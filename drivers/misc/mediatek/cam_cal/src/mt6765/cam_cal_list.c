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
#ifdef CONFIG_MTK_96516_CAMERA
	{N23_HI1336_REAR_TXD_SENSOR_ID, 0xA0, Common_read_region},   //+bug682590,zhanghengyuan.wt,ADD,2021/8/25,add n23_hi1336_rear_txd OTP
	{N23_HI1336_REAR_ST_SENSOR_ID, 0xA0, Common_read_region},   //+bug682590,zhanghengyuan.wt,ADD,2021/8/27,add n23_hi1336_rear_st OTP
	{N23_BF2253_MICRO_CXT_SENSOR_ID, 0xA4, Common_read_region},
	//+ Bug 682590, zhoumin.wt, add, 2021.08.24, bring-up OTP for front and micro camera
	{N23_SC500CS_FRONT_TXD_SENSOR_ID, 0xA0, Common_read_region},
	{N23_SC201CS_MICRO_LHYX_SENSOR_ID, 0xA4, Common_read_region},
	//- Bug 682590, zhoumin.wt, add, 2021.08.24, bring-up OTP for front and micro camera
	//+bug682590 liudijin.wt, add, 2021/8/26, gc5035 otp porting
	{N23_GC5035_FRONT_LY_SENSOR_ID, 0xA0, Common_read_region},
	//+bug682590 liudijin.wt, add, 2021/8/26, gc5035 otp porting
#else
	/*Below is commom sensor */
#ifdef CONFIG_MTK_96717_CAMERA
	{N21_TXD_MAIN_S5K2P6_SENSOR_ID	,	0xA0, Common_read_region},
	{N21_HLT_MAIN_OV16B10_SENSOR_ID	,	0xA0, Common_read_region},
	{N21_SHINE_SUB_HI1336_SENSOR_ID ,	0xA0, Common_read_region},
	{N21_TXD_SUB_S5K3L6_SENSOR_ID ,		0xA0, Common_read_region},
	{N21_CXT_MICRO_GC2375H_SENSOR_ID,	0xA4, Common_read_region},
	{N21_HLT_MICRO_GC2375H_SENSOR_ID,	0xA4, Common_read_region},
	//+bug 621775 huangzheng1.wt, add, 2021/03/08, add for n21 otp bring up
	{N21_HLT_WIDE_GC8034W_SENSOR_ID,		0xA2, Common_read_region},
	{N21_SHINE_WIDE_GC8034W_SENSOR_ID,	0xA2, Common_read_region},
	//-bug 621775 huangzheng1.wt, add, 2021/03/08, add for n21 otp bring up
#else
	{N26_HI5021Q_REAR_TRULY_SENSOR_ID,	0xA0, Common_read_region},
	{N26_HI5021Q_REAR_ST_SENSOR_ID, 0xA0, Common_read_region},
	{N26_S5KJN1_REAR_TXD_SENSOR_ID, 0xA0, Common_read_region},
	{N26_HI5021Q_REAR_DELTA_SENSOR_ID, 0xA0, Common_read_region},
	{N26_S5K5E9_FRONT_TXD_SENSOR_ID, 0xA0, Common_read_region},
        //+Bug 720367, liudijin.wt, add, 2022.03.02, bring-up OTP for n26 micro camera
        {N26_GC02M2_MICRO_CXT_SENSOR_ID, 0xA4, Common_read_region},
        {N26_SC201CS_MICRO_LCE_SENSOR_ID, 0xA4, Common_read_region},
        {N26_C2599_MICRO_DELTA_SENSOR_ID, 0xA4, Common_read_region},
        //-Bug 720367, liudijin.wt, add, 2022.03.02, bring-up OTP for n26 micro camera
#endif
#endif
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


