// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "kd_imgsensor.h"
#include "imgsensor_sensor_list.h"

/* Add Sensor Init function here
 * Note:
 * 1. Add by the resolution from ""large to small"", due to large sensor
 *    will be possible to be main sensor.
 *    This can avoid I2C error during searching sensor.
 * 2. This should be the same as
 *     mediatek\custom\common\hal\imgsensor\src\sensorlist.cpp
 */
struct IMGSENSOR_INIT_FUNC_LIST kdSensorList[MAX_NUM_OF_SUPPORT_SENSOR] = {

//+bug 612420,zhanghao2.wt,add,2020/12/24,add for n6 camera bring up
#if defined(N8_HI1336_XL_MIPI_RAW)
	{N8_HI1336_XL_SENSOR_ID,
	SENSOR_DRVNAME_N8_HI1336_XL_MIPI_RAW,
	N8_HI1336_XL_MIPI_RAW_SensorInit},
#endif
#if defined(N8_HI1336_TXD_MIPI_RAW)
    {N8_HI1336_TXD_SENSOR_ID,
    SENSOR_DRVNAME_N8_HI1336_TXD_MIPI_RAW,
    N8_HI1336_TXD_MIPI_RAW_SensorInit},
#endif
//-bug 612420,zhanghao2.wt,add,2020/12/24,add for n6 camera bring up
//+bug 612420,huangguoyong.wt,add,2021/01/15,add for n6 camera bring up
#if defined(N8_HI1336_TXD_JCT_MIPI_RAW)
        {N8_HI1336_TXD_JCT_SENSOR_ID,
        SENSOR_DRVNAME_N8_HI1336_TXD_JCT_MIPI_RAW,
        N8_HI1336_TXD_JCT_MIPI_RAW_SensorInit},
#endif
//+bug 612420,zhanghao2.wt,add,2020/12/25,add for n6 camera bring up
#if defined(N8_HI1336_XL_JCT_MIPI_RAW)
        {N8_HI1336_XL_JCT_SENSOR_ID,
        SENSOR_DRVNAME_N8_HI1336_XL_JCT_MIPI_RAW,
        N8_HI1336_XL_JCT_MIPI_RAW_SensorInit},
#endif
//-bug 612420,zhanghao2.wt,add,2020/12/25,add for n6 camera bring up
#if defined(N8_S5K3L6_HLT_MIPI_RAW)
	{N8_S5K3L6_HLT_SENSOR_ID,
	SENSOR_DRVNAME_N8_S5K3L6_HLT_MIPI_RAW,
	N8_S5K3L6_HLT_MIPI_RAW_SensorInit},
#endif
#if defined(N8_HI846_SHT_MIPI_RAW)
	{N8_HI846_SHT_SENSOR_ID,
	SENSOR_DRVNAME_N8_HI846_SHT_MIPI_RAW,
	N8_HI846_SHT_MIPI_RAW_SensorInit},
#endif
#if defined(N8_HI846_LY_MIPI_RAW)
    {N8_HI846_LY_SENSOR_ID,
    SENSOR_DRVNAME_N8_HI846_LY_MIPI_RAW,
    N8_HI846_LY_MIPI_RAW_SensorInit},
#endif
#if defined(N8_GC2375H_HLT_MIPI_RAW)
	{N8_GC2375H_HLT_MIPI_SENSOR_ID,
	SENSOR_DRVNAME_N8_GC2375H_HLT_MIPI_RAW,
	N8_GC2375H_HLT_MIPI_RAW_SensorInit},
#endif
//-bug 612420,zhanghao2.wt,add,2020/12/24,add for n6 camera bring up
#if defined(N8_GC2375A_QH_MIPI_RAW)
	{N8_GC2375A_QH_MIPI_SENSOR_ID,
	SENSOR_DRVNAME_N8_GC2375A_QH_MIPI_RAW,
	N8_GC2375A_QH_MIPI_RAW_SensorInit},
#endif
#if defined(N8_GC8034_TXD_MIPI_RAW)
	{N8_GC8034_TXD_MIPI_SENSOR_ID,
	SENSOR_DRVNAME_N8_GC8034_TXD_MIPI_RAW,
	N8_GC8034_TXD_MIPI_RAW_SensorInit},
#endif
#if defined(N8_BF2253_QH_MIPI_RAW)
	{N8_BF2253_QH_MIPI_SENSOR_ID,
	SENSOR_DRVNAME_N8_BF2253_QH_MIPI_RAW,
	N8_BF2253_QH_MIPI_RAW_SensorInit},
#endif
#if defined(N8_BF2253_QH_6_MIPI_RAW)
        {N8_BF2253_QH_6_MIPI_SENSOR_ID,
        SENSOR_DRVNAME_N8_BF2253_QH_6_MIPI_RAW,
        N8_BF2253_QH_6_MIPI_RAW_SensorInit},
#endif
#if defined(N8_BF2253_QH_7_MIPI_RAW)
        {N8_BF2253_QH_7_MIPI_SENSOR_ID,
        SENSOR_DRVNAME_N8_BF2253_QH_7_MIPI_RAW,
        N8_BF2253_QH_7_MIPI_RAW_SensorInit},
#endif
//-bug 612420,huangguoyong.wt,add,2021/01/15,add for n6 camera bring up

//+bug 621775,lintaicheng.wt, add, 20210207, add for n21 camera bring up
#if defined(N21_HLT_MAIN_OV16B10_MIPI_RAW)
	{N21_HLT_MAIN_OV16B10_SENSOR_ID,
	SENSOR_DRVNAME_N21_HLT_MAIN_OV16B10_MIPI_RAW,
	N21_HLT_MAIN_OV16B10_MIPI_RAW_SensorInit},
#endif
#if defined(N21_TXD_SUB_S5K3L6_MIPI_RAW)
	{N21_TXD_SUB_S5K3L6_SENSOR_ID,
	SENSOR_DRVNAME_N21_TXD_SUB_S5K3L6_MIPI_RAW,
	N21_TXD_SUB_S5K3L6_MIPI_RAW_SensorInit},
#endif
#if defined(N21_CXT_DEPTH_GC2375H_MIPI_RAW)
	{N21_CXT_DEPTH_GC2375H_SENSOR_ID,
	SENSOR_DRVNAME_N21_CXT_DEPTH_GC2375H_MIPI_RAW,
	N21_CXT_DEPTH_GC2375H_MIPI_RAW_SensorInit},
#endif
//-bug 621775,lintaicheng.wt, add, 20210207, add for n21 camera bring up
//+bug 621775,liuxiangyin, mod, 20210207, for n21 n21_cxt_micro_gc2375h camera bringup
#if defined(N21_TXD_MAIN_S5K2P6_MIPI_RAW)
	{N21_TXD_MAIN_S5K2P6_SENSOR_ID,
	SENSOR_DRVNAME_N21_TXD_MAIN_S5K2P6_MIPI_RAW,
	N21_TXD_MAIN_S5K2P6_MIPI_RAW_SensorInit},
#endif
#if defined(N21_SHINE_SUB_HI1336_MIPI_RAW)
	{N21_SHINE_SUB_HI1336_SENSOR_ID,
	SENSOR_DRVNAME_N21_SHINE_SUB_HI1336_MIPI_RAW,
	N21_SHINE_SUB_HI1336_MIPI_RAW_SensorInit},
#endif
#if defined(N21_CXT_DEPTH_GC02M1B_MIPI_RAW)
        {N21_CXT_DEPTH_GC02M1B_SENSOR_ID,
        SENSOR_DRVNAME_N21_CXT_DEPTH_GC02M1B_MIPI_RAW,
        N21_CXT_DEPTH_GC02M1B_MIPI_RAW_SensorInit},
#endif
#if defined(N21_HLT_DEPTH_GC2375H_MIPI_RAW)
	{N21_HLT_DEPTH_GC2375H_SENSOR_ID,
	SENSOR_DRVNAME_N21_HLT_DEPTH_GC2375H_MIPI_RAW,
	N21_HLT_DEPTH_GC2375H_MIPI_RAW_SensorInit},
#endif
#if defined(N21_HLT_MICRO_GC2375H_MIPI_RAW)
	{N21_HLT_MICRO_GC2375H_SENSOR_ID,
	SENSOR_DRVNAME_N21_HLT_MICRO_GC2375H_MIPI_RAW,
	N21_HLT_MICRO_GC2375H_MIPI_RAW_SensorInit},
#endif
#if defined(N21_CXT_MICRO_GC2375H_MIPI_RAW)
	{N21_CXT_MICRO_GC2375H_SENSOR_ID,
	SENSOR_DRVNAME_N21_CXT_MICRO_GC2375H_MIPI_RAW,
	N21_CXT_MICRO_GC2375H_MIPI_RAW_SensorInit},
#endif
//-bug 621775,liuxiangyin, mod, 20210207, for n21 n21_cxt_micro_gc2375h camera bringup
//+bug 621775,huangzheng1, add, 20210205, add for n21 camera bring up
#if defined(N21_SHINE_WIDE_GC8034W_MIPI_RAW)
	{N21_SHINE_WIDE_GC8034W_SENSOR_ID,
	SENSOR_DRVNAME_N21_SHINE_WIDE_GC8034W_MIPI_RAW,
	N21_SHINE_WIDE_GC8034WMIPI_RAW_SensorInit},
#endif
#if defined(N21_HLT_WIDE_GC8034W_MIPI_RAW)
	{N21_HLT_WIDE_GC8034W_SENSOR_ID,
	SENSOR_DRVNAME_N21_HLT_WIDE_GC8034W_MIPI_RAW,
	N21_HLT_WIDE_GC8034WMIPI_RAW_SensorInit},
#endif
//-bug 621775,huangzheng1, add, 20210205, add for n21 camera bring up
	/*  ADD sensor driver before this line */
	{0, {0}, NULL}, /* end of list */
};
/* e_add new sensor driver here */

