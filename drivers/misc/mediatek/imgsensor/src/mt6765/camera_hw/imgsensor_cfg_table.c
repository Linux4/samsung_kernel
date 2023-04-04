// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "kd_imgsensor.h"

#include "regulator/regulator.h"
#include "gpio/gpio.h"
/*#include "mt6306/mt6306.h"*/
#include "mclk/mclk.h"



#include "imgsensor_cfg_table.h"

enum IMGSENSOR_RETURN
	(*hw_open[IMGSENSOR_HW_ID_MAX_NUM])(struct IMGSENSOR_HW_DEVICE **) = {
	imgsensor_hw_regulator_open,
	imgsensor_hw_gpio_open,
	/*imgsensor_hw_mt6306_open,*/
	imgsensor_hw_mclk_open
};
#ifdef CONFIG_MTK_96516_CAMERA
struct IMGSENSOR_HW_CFG imgsensor_custom_config[] = {
	{
		IMGSENSOR_SENSOR_IDX_MAIN,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			//{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN2,
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB2,
		IMGSENSOR_I2C_DEV_3,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN3,
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},

	{IMGSENSOR_SENSOR_IDX_NONE}
};
#else
#ifdef CONFIG_MTK_96717_CAMERA
//+bug 612420,huangguoyong.wt,add,2020/12/23,add for n6 camera bring up
struct IMGSENSOR_HW_CFG imgsensor_custom_config[] = {
	{
		IMGSENSOR_SENSOR_IDX_MAIN,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN2,
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB2,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN3,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},

	{IMGSENSOR_SENSOR_IDX_NONE}
};
//-bug 612420,huangguoyong.wt,add,2020/12/23,add for n6 camera bring up
#else
struct IMGSENSOR_HW_CFG imgsensor_custom_config[] = {
	{
		IMGSENSOR_SENSOR_IDX_MAIN,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AFVDD},
			//+bug727089liangyiyi.wt,MODIFY,2022/3/29,modify for fix front camera have loss power when open rear camera
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_DVDD_EN},
			//+bug720412,qinduilin.wt,ADD,2022/1/27,n26_hi5021q_rear_truly sensor bringup
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_DVDD_1V1},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_DVDD_1V2},
			//+bug720412,qinduilin.wt,ADD,2022/1/27,n26_hi5021q_rear_truly sensor bringup
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST_SUB},
			//-bug727089liangyiyi.wt,MODIFY,2022/3/29,modify for fix front camera have loss power when open rear camera
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB,
		IMGSENSOR_I2C_DEV_1,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN2,
		IMGSENSOR_I2C_DEV_2,
		{
			//+bug720412,lintaicheng.wt,modify,2022/1/27,n26_c2515_dep_cxt sensor bringup
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			//{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			//-bug720412,lintaicheng.wt,modify,2022/1/27,n26_c2515_dep_cxt sensor bringup
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_SUB2,
		IMGSENSOR_I2C_DEV_3,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			//{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			//{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},
	{
		IMGSENSOR_SENSOR_IDX_MAIN3,
		IMGSENSOR_I2C_DEV_2,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_PDN},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_RST},
			{IMGSENSOR_HW_ID_NONE, IMGSENSOR_HW_PIN_NONE},
		},
	},

	{IMGSENSOR_SENSOR_IDX_NONE}
};
#endif
#endif
struct IMGSENSOR_HW_POWER_SEQ platform_power_sequence[] = {
#ifdef MIPI_SWITCH
	{
		PLATFORM_POWER_SEQ_NAME,
		{
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0
			},
		},
		IMGSENSOR_SENSOR_IDX_SUB,
	},
	{
		PLATFORM_POWER_SEQ_NAME,
		{
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_EN,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_HIGH,
				0
			},
			{
				IMGSENSOR_HW_PIN_MIPI_SWITCH_SEL,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0,
				IMGSENSOR_HW_PIN_STATE_LEVEL_0,
				0
			},
		},
		IMGSENSOR_SENSOR_IDX_MAIN2,
	},
#endif

	{NULL}
};

/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {
//+bug682590,zhanghengyuan.wt,ADD,2021/8/12,n23_hi1336_rear_txd sensor bringup
#if defined(N23_HI1336_REAR_TXD_MIPI_RAW)
	{
		SENSOR_DRVNAME_N23_HI1336_REAR_TXD_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1100, 1},
			{SensorMCLK, Vol_High,5 },
			{PDN, Vol_High, 2},
			{RST, Vol_High, 5},
		},
	},
#endif
//-bug682590,zhanghengyuan.wt,ADD,2021/8/12,n23_hi1336_rear_txd sensor bringup

//+bug682590,zhoumin.wt, ADD, 2021/8/19, second supply back camera bringup
#if defined(N23_HI1336_REAR_ST_MIPI_RAW)
	{
		SENSOR_DRVNAME_N23_HI1336_REAR_ST_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1100, 1},
			{SensorMCLK, Vol_High,5 },
			{PDN, Vol_High, 2},
			{RST, Vol_High, 5},
		},
	},
#endif
//-bug682590,zhoumin.wt, ADD, 2021/8/19, second supply back camera bringup

//+bug682590, zhoumin.wt, ADD, 2021/8/16, first supply front camera bringup
#if defined(N23_SC500CS_FRONT_TXD_MIPI_RAW)
	{
		SENSOR_DRVNAME_N23_SC500CS_FRONT_TXD_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_High, 2},
			{RST, Vol_Low, 2},
			{RST, Vol_High, 5},
			{SensorMCLK, Vol_High, 5},
		},
	},
#endif
//-bug682590, zhoumin.wt, ADD, 2021/8/16, first supply front camera bringup

//+bug682590,liudijin.wt,ADD,2021/8/12,n23_gc5035_front_ly sensor bringup
#if defined(N23_GC5035_FRONT_LY_MIPI_RAW)
	{
		SENSOR_DRVNAME_N23_GC5035_FRONT_LY_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{RST, Vol_Low, 10},
			{RST, Vol_High, 1},
			//{PDN, Vol_Low, 0},
			//{PDN, Vol_High, 0},
			{SensorMCLK, Vol_High, 1},
		},
	},
#endif
//-bug682590,liudijin.wt,ADD,2021/8/12,n23_gc5035_front_ly sensor bringup

//+bug682590, huangzheng1.wt, ADD, 2021/8/13, n23_bf2253_dep_lh sensor bringup
#if defined(N23_BF2253_DEP_LH_MIPI_RAW)
	{
		SENSOR_DRVNAME_N23_BF2253_DEP_LH_MIPI_RAW,
		{
				{PDN, Vol_Low, 1,Vol_High,1},
				{DOVDD, Vol_1800, 1},
				//{AVDD, Vol_Low, 1},
				{SensorMCLK, Vol_High, 1},
				{AVDD, Vol_2800, 1},
				{SensorMCLK, Vol_High, 1,Vol_Low,2},
				{PDN, Vol_High, 1,Vol_High,1},
				{PDN, Vol_Low, 1,Vol_High,1},
		},
	},
#endif
//-bug682590, huangzheng1.wt, ADD, 2021/8/13, n23_bf2253_dep_lh sensor bringup

//+bug682590,zhanghengyuan.wt,ADD,2021/8/12,n23_sc201cs_dep_cxt sensor bringup
#if defined(N23_SC201CS_DEP_CXT_MIPI_MONO)
	{
		SENSOR_DRVNAME_N23_SC201CS_DEP_CXT_MIPI_MONO,
		{
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{PDN, Vol_High, 2},
			{SensorMCLK, Vol_High, 2},
		},
	},
#endif
//-bug682590,zhanghengyuan.wt,ADD,2021/8/12,n23_sc201cs_dep_cxt sensor bringup

//+bug682590, liuxiangyin.wt, ADD, 2021/8/14, n23_c2515_dep_sj sensor bringup
#if defined(N23_C2515_DEP_SJ_MIPI_MONO)
	{
			SENSOR_DRVNAME_N23_C2515_DEP_SJ_MIPI_MONO,
			{
				{PDN, Vol_Low, 1},
				{DOVDD, Vol_1800, 20},
				{AVDD, Vol_2800, 10},
				{PDN, Vol_High, 10},
				{SensorMCLK, Vol_High, 10},
			},
	},
#endif
//-bug682590, liuxiangyin.wt, ADD, 2021/8/14, n23_c2515_dep_sj sensor bringup
//+bug682590, zhanghao2.wt, ADD, 2021/8/12, n23_bf2253_micro_cxt sensor bringup
#if defined(N23_BF2253_MICRO_CXT_MIPI_RAW)
		{
			SENSOR_DRVNAME_N23_BF2253_MICRO_CXT_MIPI_RAW,
			{
				{PDN, Vol_Low, 1,Vol_High,1},
				{DOVDD, Vol_1800, 1},
				//{AVDD, Vol_Low, 1},
				{SensorMCLK, Vol_High, 1},
				{AVDD, Vol_2800, 1},
				{SensorMCLK, Vol_High, 1,Vol_Low,2},
				{PDN, Vol_High, 1,Vol_High,1},
				{PDN, Vol_Low, 1,Vol_High,1},

			},
		},
#endif
//-bug682590, zhanghao2.wt, ADD, 2021/8/12, n23_bf2253_micro_cxt sensor bringup

//+bug682590, zhoumin.wt, ADD, 2021/8/14, second supply micro camera bringup
#if defined(N23_SC201CS_MICRO_LHYX_MIPI_RAW)
	{
		SENSOR_DRVNAME_N23_SC201CS_MICRO_LHYX_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{PDN, Vol_High, 2},
			{SensorMCLK, Vol_High, 2},
		},
	},
#endif
//-bug682590, zhoumin.wt, ADD, 2021/8/14, second supply micro camera bringup
//-bug727089liangyiyi.wt,MODIFY,2022/3/29,modify for fix front camera have loss power when open rear camera
//+bug720412,qinduilin.wt,ADD,2022/1/27,n26_hi5021q_rear_truly sensor bringup
#if defined(N26_HI5021Q_REAR_TRULY_MIPI_RAW)
	{
		SENSOR_DRVNAME_N26_HI5021Q_REAR_TRULY_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{RST_SUB, Vol_High, 2},
			{AVDD, Vol_2800, 2},
			{RST_SUB, Vol_Low, 0},
			{DVDD, 0, 0},
			{DVDD_1V1, Vol_High, 1},
			{DVDD_1V2, Vol_Low, 1},
			{DVDD_EN, Vol_High, 1},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High,5 },
			{RST, Vol_Low, 2},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(N26_HI5021Q_REAR_ST_MIPI_RAW)
	{
		SENSOR_DRVNAME_N26_HI5021Q_REAR_ST_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{RST_SUB, Vol_High, 2},
			{AVDD, Vol_2800, 2},
			{RST_SUB, Vol_Low, 0},
			{DVDD, 0, 0},
			{DVDD_1V1, Vol_High, 1},
			{DVDD_1V2, Vol_Low, 1},
			{DVDD_EN, Vol_High, 1},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High,5 },
			{RST, Vol_Low, 2},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(N26_S5KJN1_REAR_TXD_MIPI_RAW)
	{
		SENSOR_DRVNAME_N26_S5KJN1_REAR_TXD_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{RST_SUB, Vol_High, 2},
			{AVDD, Vol_2800, 2},
			{RST_SUB, Vol_Low, 0},
			{DVDD, 0, 0},
			{DVDD_1V1, Vol_High, 1},
			{DVDD_1V2, Vol_Low, 1},
			{DVDD_EN, Vol_High, 1},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High,5 },
			{RST, Vol_Low, 2},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(N26_HI5021Q_REAR_DELTA_MIPI_RAW)
	{
		SENSOR_DRVNAME_N26_HI5021Q_REAR_DELTA_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{RST_SUB, Vol_High, 2},
			{AVDD, Vol_2800, 2},
			{RST_SUB, Vol_Low, 0},
			{DVDD, 0, 0},
			{DVDD_1V1, Vol_High, 1},
			{DVDD_1V2, Vol_Low, 1},
			{DVDD_EN, Vol_High, 1},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High,5 },
			{RST, Vol_Low, 2},
			{RST, Vol_High, 5},
		},
	},
#endif
//-bug720412,qinduilin.wt,ADD,2022/1/27,n26_hi5021q_rear_truly sensor bringup
//-bug727089liangyiyi.wt,MODIFY,2022/3/29,modify for fix front camera have loss power when open rear camera
//+bug682590,zhanghengyuan.wt,ADD,2022/1/24,n26 front camera bringup
#if defined(N26_SC501CS_FRONT_LY_MIPI_RAW)
	{
		SENSOR_DRVNAME_N26_SC501CS_FRONT_LY_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 4},
		},
	},
#endif
#if defined(N26_HI556_FRONT_XL_MIPI_RAW)
	{
		SENSOR_DRVNAME_N26_HI556_FRONT_XL_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},//bug727089, liangyiyi.wt, MODIFY, 2022/3/1, modify 2st front camera dvdd to 1.2V
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(N26_S5K5E9_FRONT_TXD_MIPI_RAW)
	{
		SENSOR_DRVNAME_N26_S5K5E9_FRONT_TXD_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 0},
			{AVDD, Vol_2800, 1},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1},
			{SensorMCLK, Vol_High, 4},
		},
	},
#endif
#if defined(N26_HI556_FRONT_DELTA_MIPI_RAW)
	{
		SENSOR_DRVNAME_N26_HI556_FRONT_DELTA_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},//bug727089, liangyiyi.wt, MODIFY, 2022/3/1, modify 4st front camera dvdd to 1.2V
			{SensorMCLK, Vol_High, 1},
			{RST, Vol_High, 5},
		},
	},
#endif
//-bug682590,zhanghengyuan.wt,ADD,2021/1/24,n26 front camera bringup
//+bug720412,lintaicheng.wt,ADD,2022/1/28,n26_c2515_dep_cxt sensor bringup
#if defined(N26_C2515_DEP_CXT_MIPI_MONO)
	{
		SENSOR_DRVNAME_N26_C2515_DEP_CXT_MIPI_MONO,
		{
			{PDN, Vol_Low, 1},
			{DOVDD, Vol_1800, 20},
			{AVDD, Vol_2800, 10},
			{PDN, Vol_High, 10},
			{SensorMCLK, Vol_High, 10},
		},
	},
#endif
#if defined(N26_SC201CS_DEP_LH_MIPI_MONO)
	{
		SENSOR_DRVNAME_N26_SC201CS_DEP_LH_MIPI_MONO,
		{
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{PDN, Vol_High, 2},
			{SensorMCLK, Vol_High, 2},
		},
	},
#endif
#if defined(N26_GC02M1_DEP_CXT_MIPI_RAW)
	{
		SENSOR_DRVNAME_N26_GC02M1_DEP_CXT_MIPI_RAW,
		{
			{PDN, Vol_Low, 15},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
		},
	},
#endif
#if defined(N26_C2519_DEP_DELTA_MIPI_MONO)
	{
		SENSOR_DRVNAME_N26_C2519_DEP_DELTA_MIPI_MONO,
		{
			{PDN, Vol_Low, 1},
			{DOVDD, Vol_1800, 20},
			{AVDD, Vol_2800, 10},
			{PDN, Vol_High, 10},
			{SensorMCLK, Vol_High, 10},
		},
	},
#endif
//-bug720412,lintaicheng.wt,ADD,2022/1/28,n26_c2515_dep_cxt sensor bringup
//+bug720367,liudijin.wt,ADD,2022/01/27,gc02m2 and sc201cs micro sensor bringup
#if defined(N26_GC02M2_MICRO_CXT_MIPI_RAW)
                {
                    SENSOR_DRVNAME_N26_GC02M2_MICRO_CXT_MIPI_RAW,
                    {
                            {PDN, Vol_Low, 5},
                            {DOVDD, Vol_1800, 5},
                            {AVDD, Vol_2800, 5},
                            {SensorMCLK, Vol_High, 10},
                            {PDN, Vol_High, 5},

                    },
                },
#endif
#if defined(N26_SC201CS_MICRO_LCE_MIPI_RAW)
        {
            SENSOR_DRVNAME_N26_SC201CS_MICRO_LCE_MIPI_RAW,
            {
                {PDN, Vol_Low, 5},
                {DOVDD, Vol_1800, 5},
                {AVDD, Vol_2800, 5},
                {PDN, Vol_High, 5},
                {SensorMCLK, Vol_High, 10},
            },
        },
#endif
#if defined(N26_C2599_MICRO_DELTA_MIPI_RAW)
	{
		SENSOR_DRVNAME_N26_C2599_MICRO_DELTA_MIPI_RAW,
		{
			{PDN, Vol_Low, 1},
			{DOVDD, Vol_1800, 20},
			{AVDD, Vol_2800, 10},
			{PDN, Vol_High, 10},
			{SensorMCLK, Vol_High, 10},
		},
	},
#endif
//-bug720367,liudijin.wt,ADD,2022/01/27,gc02m2 and sc201cs micro sensor bringup
#if 0
#if defined(IMX398_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX398_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(OV23850_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV23850_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(IMX386_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX386_MIPI_RAW,
		{
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(IMX386_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX386_MIPI_MONO,
		{
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif

#if defined(IMX338_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX338_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2500, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(S5K4E6_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K4E6_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2900, 0},
			{DVDD, Vol_1200, 2},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3P8SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P8SP_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K2T7SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2T7SP_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(IMX230_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX230_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2500, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 10}
		},
	},
#endif
#if defined(S5K3M2_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M2_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3P3SX_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P3SX_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K5E2YA_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K5E2YA_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K4ECGX_MIPI_YUV)
	{
		SENSOR_DRVNAME_S5K4ECGX_MIPI_YUV,
		{
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV16880_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV16880_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(S5K2P7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2P7_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1000, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K2P8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2P8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX258_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX258_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX258_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX258_MIPI_MONO,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX377_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX377_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV8858_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8858_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV8856_MIPI_RAW)
	{SENSOR_DRVNAME_OV8856_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(S5K2X8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2X8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX214_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX214_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX214_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX214_MIPI_MONO,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(S5K3L8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3L8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX362_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX362_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1200, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K2L7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2L7_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 3},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX318_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX318_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1200, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV8865_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8865_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX219_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX219_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1000, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3M3_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M3_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV5670_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV5670_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV5670_MIPI_RAW_2)
	{
		SENSOR_DRVNAME_OV5670_MIPI_RAW_2,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV20880_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV20880_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 1},
			{RST, Vol_High, 5}
		},
	},
#endif
#endif
	/* add new sensor before this line */
	{NULL,},
};

