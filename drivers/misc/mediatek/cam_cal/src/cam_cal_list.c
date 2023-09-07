/*
* Copyright (C) 2016 MediaTek Inc.
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
#include "kd_imgsensor.h"

/*Common EEPRom Driver*/
#ifdef TITAN_OTP_SUPPORT
#include "common/DW9807_eeprom/DW9807_eeprom.h"
#include "common/M24C64S_eeprom/M24C64S_eeprom.h"
#endif
#ifdef C10_OTP_SUPPORT
#include "common/GT24c128b/GT24c128b.h"
#include "common/M24C64S_eeprom/M24C64S_eeprom.h"
#endif
#ifdef JADE_OTP_SUPPORT
#include "common/CAT24s128c/CAT24s128c.h"
#include "common/M24C64S_eeprom/M24C64S_eeprom.h"
#endif

#define CAM_CAL_DEBUG
#ifdef CAM_CAL_DEBUG
/*#include <linux/log.h>*/
#include <linux/kern_levels.h>
#define PFX "cam_cal_list"

#define CAM_CALINF(format, args...)     pr_info(PFX "[%s] " format, __func__, ##args)
#define CAM_CALDB(format, args...)      pr_debug(PFX "[%s] " format, __func__, ##args)
#define CAM_CALERR(format, args...)     pr_info(format, ##args)
#else
#define CAM_CALINF(x, ...)
#define CAM_CALDB(x, ...)
#define CAM_CALERR(x, ...)
#endif


#define MTK_MAX_CID_NUM 6
unsigned int mtkCidList[MTK_MAX_CID_NUM] = {
#ifdef TITAN_OTP_SUPPORT
	0xc4000007,
#endif
	0x010b00ff,/*Single MTK Format*/
	0x020b00ff,/*Double MTK Format in One OTP/EEPRom - Legacy*/
	0x030b00ff,/*Double MTK Format in One OTP/EEPRom*/
	0x040b00ff /*Double MTK Format in One OTP/EEPRom V1.4*/
};

struct stCAM_CAL_FUNC_STRUCT g_camCalCMDFunc[] = {
#ifdef TITAN_OTP_SUPPORT
	{CMD_DW9807, dw9807_selective_read_region},
	{CMD_M24C64S, m24c64s_selective_read_region},
#endif
#ifdef C10_OTP_SUPPORT
	{CMD_GT24C128B, gt24c128b_selective_read_region},
	{CMD_M24C64S, m24c64s_selective_read_region},
#endif
#ifdef JADE_OTP_SUPPORT
	{CMD_CAT24S128C, cat24s128c_selective_read_region},
	{CMD_M24C64S, m24c64s_selective_read_region},
#endif

	/*      ADD before this line */
	{0, 0} /*end of list*/
};

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
#ifdef C10_OTP_SUPPORT
	{S5K2L2_SENSOR_ID, 0xA0, CMD_GT24C128B, cam_cal_check_mtk_cid},
	{S5K4H9_MONO_SENSOR_ID, 0xA0, CMD_MAIN, cam_cal_check_mtk_cid},
	{S5K3P8SN_SENSOR_ID, 0xA2, CMD_M24C64S, cam_cal_check_mtk_cid},
#endif
#ifdef JADE_OTP_SUPPORT
	{IMX258_SENSOR_ID, 0xA2, CMD_CAT24S128C, cam_cal_check_mtk_cid},
	{IMX241_MONO_SENSOR_ID, 0xA2, CMD_MAIN, cam_cal_check_mtk_cid},
	{S5K3P8SN_SENSOR_ID, 0xA2, CMD_M24C64S, cam_cal_check_mtk_cid},
#endif
#ifdef TITAN_OTP_SUPPORT
	{IMX258J7M_SENSOR_ID, 0xB0, CMD_DW9807, cam_cal_check_mtk_cid},
	{S5K3M3_SENSOR_ID, 0xA3, CMD_M24C64S, cam_cal_check_mtk_cid},
#endif
	{IMX338_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX318_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K2L7_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K2P8_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX258_MONO_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX386_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{OV8858_SENSOR_ID, 0xA8, CMD_AUTO, cam_cal_check_mtk_cid},
	/*  ADD before this line */
	{0, 0, CMD_NONE, 0} /*end of list*/
};


unsigned int cam_cal_get_sensor_list(struct stCAM_CAL_LIST_STRUCT **ppCamcalList)

{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


unsigned int cam_cal_get_func_list(struct stCAM_CAL_FUNC_STRUCT **ppCamcalFuncList)
{
	if (ppCamcalFuncList == NULL)
		return 1;

	*ppCamcalFuncList = &g_camCalCMDFunc[0];
	return 0;
}

unsigned int cam_cal_check_mtk_cid(struct i2c_client *client, cam_cal_cmd_func readCamCalData)
{
	unsigned int calibrationID = 0, ret = 0;
	int j = 0;

	if (readCamCalData != NULL) {
	#if defined(TITAN_OTP_SUPPORT) || defined(C10_OTP_SUPPORT)
		readCamCalData(client, 0x0201, (unsigned char *)&calibrationID, 4);
	#elif defined(JADE_OTP_SUPPORT)
		readCamCalData(client, 0x0201, (unsigned char *)&calibrationID, 4);
	#else
		readCamCalData(client, 1, (unsigned char *)&calibrationID, 4);
	#endif
		CAM_CALDB("calibrationID = %x\n", calibrationID);
	}

	if (calibrationID != 0)
		for (j = 0; j < MTK_MAX_CID_NUM; j++) {
			CAM_CALDB("mtkCidList[%d] == %x\n", j, calibrationID);
			if (mtkCidList[j] == calibrationID) {
				ret = 1;
				break;
			}
		}

	CAM_CALDB("ret =%d\n", ret);
	return ret;
}

unsigned int cam_cal_check_double_eeprom(struct i2c_client *client, cam_cal_cmd_func readCamCalData)
{
	unsigned int calibrationID = 0, ret = 0;

	CAM_CALDB("start cam_cal_check_double_eeprom !\n");
	if (readCamCalData != NULL) {
		CAM_CALDB("readCamCalData != NULL !\n");
		readCamCalData(client, 1, (unsigned char *)&calibrationID, 4);
		CAM_CALDB("calibrationID = %x\n", calibrationID);
	}

	if (calibrationID == 0x020b00ff || calibrationID == 0x030b00ff)
		ret = 1;


	CAM_CALDB("ret=%d\n", ret);
	return ret;
}



