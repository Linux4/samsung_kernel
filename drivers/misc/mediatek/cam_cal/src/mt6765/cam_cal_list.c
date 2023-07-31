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
	#if 0
	/*Below is commom sensor */
	{IMX230_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX338_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4E6_SENSOR_ID, 0xA8, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3M3_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX318_SENSOR_ID, 0xA0, Common_read_region},
	{OV8858_SENSOR_ID, 0xA8, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	/*B+B*/
	{S5K2P7_SENSOR_ID, 0xA0, Common_read_region},
	{OV8856_SENSOR_ID, 0xA0, Common_read_region},
	/*61*/
	{IMX499_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3L8_SENSOR_ID, 0xA0, Common_read_region},
	{S5K5E8YX_SENSOR_ID, 0xA2, Common_read_region},
	/*99*/
	{IMX258_SENSOR_ID, 0xA0, Common_read_region},
	{IMX258_MONO_SENSOR_ID, 0xA0, Common_read_region},
	/*97*/
	{OV23850_SENSOR_ID, 0xA0, Common_read_region},
	{OV23850_SENSOR_ID, 0xA8, Common_read_region},
	{S5K3M2_SENSOR_ID, 0xA0, Common_read_region},
	/*55*/
	{S5K2P8_SENSOR_ID, 0xA2, Common_read_region},
	{S5K2P8_SENSOR_ID, 0xA0, Common_read_region},
	{OV8858_SENSOR_ID, 0xA2, Common_read_region},
	/* Others */
	{S5K2X8_SENSOR_ID, 0xA0, Common_read_region},
	{IMX377_SENSOR_ID, 0xA0, Common_read_region},
	{IMX214_SENSOR_ID, 0xA0, Common_read_region},
	{IMX214_MONO_SENSOR_ID, 0xA0, Common_read_region},
	{IMX486_SENSOR_ID, 0xA8, Common_read_region},
	{OV12A10_SENSOR_ID, 0xA8, Common_read_region},
	{OV13855_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3L8_SENSOR_ID, 0xA0, Common_read_region},
	{HI556_SENSOR_ID, 0x51, Common_read_region},
	{S5K5E8YX_SENSOR_ID, 0x5a, Common_read_region},
	{S5K5E8YXREAR2_SENSOR_ID, 0x5a, Common_read_region},
	{S5KGM2_SENSOR_ID, 0xB0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K2P6_SENSOR_ID, 0xB0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{GC5035_SENSOR_ID, 0x7E, Otp_read_region_GC5035,
		DEFAULT_MAX_EEPROM_SIZE_8K},
	{GC02M1_SENSOR_ID, 0xA4, Common_read_region},
	{GC02M1_SENSOR_ID1, 0x6E, Otp_read_region_GC02M1B,
		DEFAULT_MAX_EEPROM_SIZE_8K},
	{SR846_SENSOR_ID, 0x40, Otp_read_region_SR846,
		DEFAULT_MAX_EEPROM_SIZE_8K},
	{SR846D_SENSOR_ID, 0x40, Otp_read_region_SR846D,
		DEFAULT_MAX_EEPROM_SIZE_8K},
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


