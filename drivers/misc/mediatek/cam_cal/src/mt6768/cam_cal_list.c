/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"
/* a06 code for SR-AL7160A-01-502 by hanhaoran at 20240320 start */
/*hs14 code for SR-AL5628-01-161 Universal macro adaptation by lisizhou at 2022/9/23 start*/
/* A06 code for SR-AL7160A-01-501 by zhongbin at 20240415 start */
#if defined(CONFIG_HQ_PROJECT_O22)
#include "eeprom_i2c_a1402widesc520cstxd_driver.h"
#include "eeprom_i2c_a1401widehi556wly_driver.h"
#endif
#if defined(CONFIG_HQ_PROJECT_O8)
#include "eeprom_i2c_a0601ddfrontsc820cs_driver.h"
#include "eeprom_i2c_a0602cxtfrontgc08a8_driver.h"
#include "eeprom_i2c_a0603ddfrontmt811_driver.h"
#include "eeprom_i2c_a0604xlfrontmt811_driver.h"
/* A06 code for SR-AL7160A-01-502 by xuyunhui at 20240605 start */
#include "eeprom_i2c_a0605ddfrontsc820cs_driver.h"
/* A06 code for SR-AL7160A-01-502 by xuyunhui at 20240605 end */
/* A06 code for SR-AL7160A-01-502 by xuyunhui at 20240619 start */
#include "eeprom_i2c_a0606ddfrontmt811_driver.h"
/* A06 code for SR-AL7160A-01-502 by jiangwenhan at 20240626 start */
#include "eeprom_i2c_a0607xlfrontmt811_driver.h"
#endif
/*hs14 code for SR-AL5628-01-161 Universal macro adaptation by lisizhou at 2022/9/23 end*/
struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
/*hs14 code for AL6528ADEU-2675 by pengxutao at 2022/11/18 start*/
#if defined(CONFIG_HQ_PROJECT_O22)
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
#if defined(CONFIG_HQ_PROJECT_O8)
    {A0601LYBACKS5KJN1_SENSOR_ID,       0xA0, Common_read_region},
    {A0601DDFRONTSC820CS_SENSOR_ID,	0x20, a0601ddfrontsc820cs_read_region},
    {A0602XLBACKS5KJN1_SENSOR_ID,       0xA0, Common_read_region},
    {A0602CXTFRONTGC08A8_SENSOR_ID,     0x62, a0602cxtfrontgc08a8_read_region},
    {A0603TXDBACKS5KJN1_SENSOR_ID,      0xA0, Common_read_region},
    {A0603DDFRONTMT811_SENSOR_ID,       0x40, a0603ddfrontmt811_read_region},
    {A0604XLBACKS5KJN1_SENSOR_ID,       0xA0, Common_read_region},
    {A0604XLFRONTMT811_SENSOR_ID,       0x40, a0604xlfrontmt811_read_region},
	/* A06 code for SR-AL7160A-01-502 by xuyunhui at 20240605 start */
	/* A06 code for SR-AL7160A-01-502 by jiangwenhna at 20240611 start */
	{A0605OFBACKS5KJN1_SENSOR_ID,       0xA0, Common_read_region},
    {A0605DDFRONTSC820CS_SENSOR_ID,	0x20, a0605ddfrontsc820cs_read_region},
	/* A06 code for SR-AL7160A-01-502 by xuyunhui at 20240605 end */
	/* A06 code for SR-AL7160A-01-502 by jiangwenhan at 20240611 end */
	{A0606DDFRONTMT811_SENSOR_ID,       0x40, a0606ddfrontmt811_read_region},
	/* A06 code for SR-AL7160A-01-502 by xuyunhui at 20240619 end */
	{A0607XLFRONTMT811_SENSOR_ID,       0x40, a0607xlfrontmt811_read_region},
	/* A06 code for SR-AL7160A-01-502 by jiangwenhan at 20240626 end */	
#endif
/* A06 code for SR-AL7160A-01-501 by zhongbin at 20240415 end */
/*hs14 code for AL6528ADEU-2675 by pengxutao at 2022/11/18 end*/
/* a06 code for SR-AL7160A-01-502 by hanhaoran at 20240320 end */
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


