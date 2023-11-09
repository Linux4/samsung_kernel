// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"
/*HS04 code for DEVAL6398A-9 Universal macro adaptation by chenjun at 2022/7/2 start*/
#ifdef CONFIG_HQ_PROJECT_OT8
#include "eeprom_i2c_hi846_driver.h"
#include "eeprom_i2c_gc8054_driver.h"
#include "eeprom_i2c_gc8054_cxt_driver.h"
#include "eeprom_i2c_ov02b10_jk_driver.h"
#include "eeprom_i2c_gc02m1_sjc_driver.h"
#include "eeprom_i2c_gc02m1sub_cxt_driver.h"
/*  TabA7 Lite code for SR-AX3565-01-907 by chenjun at 2022/02/21 start */
#include "eeprom_i2c_s5k4h7_hlt_driver.h"
/*  TabA7 Lite code for SR-AX3565-01-907 by chenjun at 2022/02/21 end */
#endif
/*HS04 code for DEVAL6398A-9 Universal macro adaptation by chenjun at 2022/7/2 end*/
#ifdef CONFIG_HQ_PROJECT_HS03S
#include "eeprom_i2c_hi556_txd_driver.h"
#include "eeprom_i2c_gc5035_ly_driver.h"
#include "eeprom_i2c_gc02m1_cxt_driver.h"
#include "eeprom_i2c_hi556_ofilm_driver.h"
#include "eeprom_i2c_gc5035_dd_driver.h"
#include "eeprom_i2c_ov02b10_ly_driver.h"
/* A03s code for CAM-AL5625-01-247 by majunfeng at 2021/05/25 start */
#include "eeprom_i2c_gc5035_xl_driver.h"
/*A03s code for CAM-AL5625-01-247 by majunfeng at 2021/05/25 end */
/* A03s code for SR-AL5625-01-324 by wuwenjie at 2021/06/28 start */
#include "eeprom_i2c_gc02m1_jk_driver.h"
/* A03s code for SR-AL5625-01-324 by wuwenjie at 2021/06/28 end */
#include "eeprom_i2c_sc500cs_dd_driver.h"
#endif

/*hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/14 start*/
#ifdef CONFIG_HQ_PROJECT_HS04
#include "eeprom_i2c_o2101_hi556txd_front_driver.h"
#include "eeprom_i2c_o2103_sc520syx_front_driver.h"
#include "eeprom_i2c_o2104_hi556wtxd_front_driver.h"
#endif
/*hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/14 end*/

#define MAX_EEPROM_SIZE_16K 0x4000

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
/*HS04 code for DEVAL6398A-9 Universal macro adaptation by chenjun at 2022/7/2 start*/
#ifdef CONFIG_HQ_PROJECT_OT8
/*TabA7 Lite code for SR-AX3565-01-320 by liuchengfei at 20201127 start*/
	{HI846_SJC_SENSOR_ID, 0xB0, Common_read_region},
        {HI846_TXD_SENSOR_ID, 0x42, hi846_read_region},
/*TabA7 Lite code for SR-AX3565-01-320 by liuchengfei at 20201127 end*/
/*TabA7 Lite code for SR-AX3565-01-320 by lisizhou at 202011210 start*/
	{GC8054_HLT_SENSOR_ID, 0x62, gc8054_read_region},
/*TabA7 Lite code for SR-AX3565-01-320 by lisizhou at 202011210 end*/
/*TabA7 Lite code for SR-AX3565-01-320 by wangqi at 202011228 start*/
	{GC8054_CXT_SENSOR_ID, 0x62, gc8054_cxt_read_region},
/*TabA7 Lite code for SR-AX3565-01-320 by wangqi at 202011228 end*/
/*  TabA7 Lite code for SR-AX3565-01-875 by gaozhenyu at 2021/11/25 start */
        {SC800CS_LY_SENSOR_ID, 0xA0, Common_read_region},
/*  TabA7 Lite code for SR-AX3565-01-875 by gaozhenyu at 2021/11/25 end */
/*  TabA7 Lite code for SR-AX3565-01-907 by chenjun at 2022/02/21 start */
	{S5K4H7_HLT_SENSOR_ID, 0x20, s5k4h7_hlt_read_region},
/*  TabA7 Lite code for SR-AX3565-01-907 by chenjun at 2022/02/21 end */
/*TabA7 Lite code for SR-AX3565-01-320 by lisizhou at 20210104 start*/
	{OV02B10_JK_SENSOR_ID, 0x78, ov02b10_jk_read_region},
/*TabA7 Lite code for SR-AX3565-01-320 by lisizhou at 20210104 end*/
/*TabA7 Lite code for SR-AX3565-01-320 by wangqi at 20210107 start*/
	{GC02M1_SJC_SENSOR_ID, 0x6E, gc02m1_sjc_read_region},
/*TabA7 Lite code for SR-AX3565-01-320 by wangqi at 20210107 end*/
/*TabA7 Lite code for SR-AX3565-01-320 by wangqi at 20210107 start*/
	{GC02M1SUB_CXT_SENSOR_ID, 0x6E, gc02m1sub_cxt_read_region},
/*TabA7 Lite code for SR-AX3565-01-320 by wangqi at 20210107 end*/
#endif
#ifdef CONFIG_HQ_PROJECT_HS03S
	/*Below is commom sensor */
	/* A03s code for CAM-AL5625-01-247 by lisizhou at 2021/04/28 start */
	{HI1336_TXD_SENSOR_ID, 0xA0, Common_read_region},
	{HI1336_HLT_SENSOR_ID, 0xA0, Common_read_region},
	{GC13053_LY_SENSOR_ID, 0xA0, Common_read_region},
	/* A03s code for CAM-AL5625-01-247 by lisizhou at 2021/04/28 end */
	/* hs03s code for SR-AL5625-01-247 by majunfeng at 2021/06/07 start  */
	{S5K3L6_OFILM_SENSOR_ID, 0xA0, Common_read_region},
	{OV13B10_QT_SENSOR_ID, 0xA0, Common_read_region},
	{OV13B10_DD_SENSOR_ID, 0xA0, Common_read_region},
	{OV13B10_XL_SENSOR_ID, 0xA0, Common_read_region},
	/* hs03s code for SR-AL5625-01-247 by majunfeng at 2021/06/07 end  */
/* A03s code for SR-AL5625-01-332 by xuxianwei at 2021/05/02 start */
	{HI556_TXD_SENSOR_ID, 0x50, hi556_txd_read_region},
/* A03s code for SR-AL5625-01-332 by xuxianwei at 2021/05/07 start */
	{HI556_OFILM_SENSOR_ID, 0x40, hi556_ofilm_read_region},
    {GC5035_DD_SENSOR_ID, 0x6e, gc5035_dd_read_region},
    {GC5035_LY_SENSOR_ID, 0x6e, gc5035_ly_read_region},
/* A03s code for SR-AL5625-01-332 by xuxianwei at 2021/05/07 end */
/* A03s code for SR-AL5625-01-332 by xuxianwei at 2021/05/02 end */
/* A03s code for SR-AL5625-01-324 by wuwenjie at 2021/05/06 start */
	{GC02M1_CXT_SENSOR_ID, 0x6E, gc02m1_cxt_read_region},
	{OV02B10_LY_SENSOR_ID, 0x7A, ov02b10_ly_read_region},
	/* A03s code for SR-AL5625-01-324 by wuwenjie at 2021/06/28 start */
	{GC02M1_JK_SENSOR_ID, 0x6E, gc02m1_jk_read_region},
/* A03s code for SR-AL5625-01-324 by wuwenjie at 2021/06/28 end */
	{GC5035_XL_SENSOR_ID, 0x6e, gc5035_xl_read_region},
/* A03s code for SR-AL5625-01-324 by wuwenjie at 2021/04/06 end */
/*hs03s_NM code for SL6215DEV-4183 by liluling at 2022/4/15 start*/
	{SC500CS_DD_SENSOR_ID, 0x6c, sc500cs_dd_read_region},
/*hs03s code for DEVAL5625-2576 by majunfeng at 2022/04/08 start*/
	{SC1300CS_LY_SENSOR_ID, 0xA0, Common_read_region},
/*hs03s code for DEVAL5625-2576 by majunfeng at 2022/04/08 end*/
/*hs03s_NM code for SL6215DEV-4183 by liluling at 2022/4/15 end*/
#endif
/*hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/14 start*/
#ifdef CONFIG_HQ_PROJECT_HS04
	{O2101_SC1300CSLY_BACK_SENSOR_ID, 0xA0, Common_read_region},
	{O2102_HI1336TXD_BACK_SENSOR_ID, 0xA0, Common_read_region},
	{O2103_OV13B10HLT_BACK_SENSOR_ID, 0xB0, Common_read_region},
	{O2104_HI1336SJC_BACK_SENSOR_ID, 0xA0, Common_read_region},
	{O2101_HI556TXD_FRONT_SENSOR_ID, 0x50, o2101_hi556txd_front_read_region},
	{O2102_OV05A10HLT_FRONT_SENSOR_ID, 0xA0, Common_read_region},
	{O2103_SC520SYX_FRONT_SENSOR_ID, 0x6C, o2103_sc520syx_front_read_region},
	{O2104_HI556WTXD_FRONT_SENSOR_ID, 0x50, o2104_hi556wtxd_front_read_region},
#endif
/*hs04 code for DEVAL6398A-46 by renxinglin at  2022/10/14 end*/
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


