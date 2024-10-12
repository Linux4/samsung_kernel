/* SPDX-License-Identifier: GPL-2.0 */
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

struct IMGSENSOR_HW_CFG imgsensor_custom_config[] = {
	{
		IMGSENSOR_SENSOR_IDX_MAIN,
		IMGSENSOR_I2C_DEV_0,
		{
			{IMGSENSOR_HW_ID_MCLK, IMGSENSOR_HW_PIN_MCLK},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DOVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AFVDD},
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_DVDD},
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD_2V8},
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
			{IMGSENSOR_HW_ID_GPIO, IMGSENSOR_HW_PIN_AVDD_2V8},
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
			{IMGSENSOR_HW_ID_REGULATOR, IMGSENSOR_HW_PIN_AVDD},
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
//+S96818AA1-1936,liudijin.wt,ADD,2023/04/11,hi5022 main sensor bringup
#if defined(N28HI5021QREARTRULY_MIPI_RAW)
	{
		SENSOR_DRVNAME_N28HI5021QREARTRULY_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 1},
			{AVDD, Vol_2800, 2},
			{AVDD_2V8, Vol_High, 2},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High,5 },
			{RST, Vol_Low, 2},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(N28HI5021QREARDC_MIPI_RAW)
	{
		SENSOR_DRVNAME_N28HI5021QREARDC_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 1},
			{AVDD, Vol_2800, 2},
			{AVDD_2V8, Vol_High, 2},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High,5 },
			{RST, Vol_Low, 2},
			{RST, Vol_High, 5},
		},
	},
#endif
//-S96818AA1-1936,liudijin.wt,ADD,2023/04/11,hi5022 main sensor bringup
//+S96818AA1-1936,yangjiayuan.wt,ADD,2023/12/09,gc50e0 main sensor bringup
#if defined(N28GC50E0REARGKW_MIPI_RAW)
	{
		SENSOR_DRVNAME_N28GC50E0REARGKW_MIPI_RAW,
		{
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 2},
			{AVDD_2V8, Vol_High, 2},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High,5 },
			{RST, Vol_Low, 2},
			{RST, Vol_High, 5},
		},
	},
#endif
//-S96818AA1-1936,yangjiayuan.wt,ADD,2023/12/09,gc50e0 main sensor bringup
//+S96818AA1-1936,guomin2.wt,ADD,2024/06/11,s5kjn1 main sensor bringup
#if defined(N28S5KJN1REARTRULY_MIPI_RAW)
	{
		SENSOR_DRVNAME_N28S5KJN1REARTRULY_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1050, 1},
			{AVDD, Vol_2800, 1},
			{AVDD_2V8, Vol_High, 2},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High,5 },
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(N28S5KJN1REARDC_MIPI_RAW)
	{
		SENSOR_DRVNAME_N28S5KJN1REARDC_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1050, 1},
			{AVDD, Vol_2800, 1},
			{AVDD_2V8, Vol_High, 2},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High,5 },
			{RST, Vol_High, 5},
		},
	},
#endif
//-S96818AA1-1936,guomin2.wt,ADD,2024/06/11,s5kjn1 main sensor bringup
//+S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/12,hi846 sub sensor bringup
#if defined(N28HI846FRONTTRULY_MIPI_RAW)
        {
            SENSOR_DRVNAME_N28HI846FRONTTRULY_MIPI_RAW,
            {
				 {RST, Vol_Low, 0},
				 {DOVDD, Vol_1800, 0},
				 {AVDD, Vol_2800, 0},
				 {AVDD_2V8, Vol_High, 1},
				 {DVDD, Vol_1200, 2},
				 {SensorMCLK, Vol_High, 1},
				 {RST, Vol_High, 1}
            },
        },
#endif
//-S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/12,hi846 sub sensor bringup
//+S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/12,gc08a3 sub sensor bringup
#if defined(N28GC08A3FRONTCXT_MIPI_RAW)
        {
            SENSOR_DRVNAME_N28GC08A3FRONTCXT_MIPI_RAW,
            {
				 {RST, Vol_Low, 0},
				 {DOVDD, Vol_1800, 1},
				 {DVDD, Vol_1200, 1},
				 {AVDD, Vol_2800, 1},
				 {AVDD_2V8, Vol_High, 1},
				 {SensorMCLK, Vol_High, 1},
				 {RST, Vol_High, 5}
            },
        },
#endif
//-S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/12,gc08a3 sub sensor bringup
#if defined(N28SC800CSFRONTDC_MIPI_RAW)
        {
            SENSOR_DRVNAME_N28SC800CSFRONTDC_MIPI_RAW,
            {
                {RST, Vol_Low, 0},
                {SensorMCLK, Vol_High, 0},
                {DOVDD, Vol_1800, 1},
                {DVDD, Vol_1200, 1},
                {AVDD, Vol_2800, 1},
                {AVDD_2V8, Vol_High, 1},
                {RST, Vol_High, 2},
                {RST, Vol_Low, 2},
                {RST, Vol_High, 2}
            },
        },
#endif
//+S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/25,c8496 sub sensor bringup
#if defined(N28C8496FRONTDC_MIPI_RAW)
        {
            SENSOR_DRVNAME_N28C8496FRONTDC_MIPI_RAW,
            {
                {RST, Vol_Low, 0},
                {DOVDD, Vol_1800, 1},
                {AVDD, Vol_2800, 1},
                {AVDD_2V8, Vol_High, 1},
                {DVDD, Vol_1200, 1},
                {RST, Vol_High, 5},
                {SensorMCLK, Vol_High, 5},
            },
        },
#endif
//-S96818AA1-1936,wuwenhao2.wt,ADD,2023/04/25,c8496 sub sensor bringup
//+S96818AA1-1936,zhujianjia.wt,ADD,2023/08/21,sc800csa sub sensor bringup
#if defined(N28SC800CSAFRONTDC_MIPI_RAW)
        {
            SENSOR_DRVNAME_N28SC800CSAFRONTDC_MIPI_RAW,
            {
                {RST, Vol_Low, 0},
                {SensorMCLK, Vol_High, 0},
                {DOVDD, Vol_1800, 1},
                {DVDD, Vol_1200, 1},
                {AVDD, Vol_2800, 1},
                {AVDD_2V8, Vol_High, 1},
                {RST, Vol_High, 2}
            },
        },
#endif
//-S96818AA1-1936,zhujianjia.wt,ADD,2023/08/21,sc800csa sub sensor bringup
//+S96818AA1-1936,chenming.wt,ADD,2023/04/11,c2519 depth sensor bringup
#if defined(N28C2519DEPCXT_MIPI_MONO)
      {
              SENSOR_DRVNAME_N28C2519DEPCXT_MIPI_MONO,
              {
                      {PDN, Vol_Low, 1},
                      {DOVDD, Vol_1800, 20},
                      {AVDD, Vol_2800, 10},
                      {PDN, Vol_High, 10},
                      {SensorMCLK, Vol_High, 10},
              },
      },
#endif
#if defined(N28C2519DEPCXT2_MIPI_MONO)
      {
              SENSOR_DRVNAME_N28C2519DEPCXT2_MIPI_MONO,
              {
                      {PDN, Vol_Low, 1},
                      {DOVDD, Vol_1800, 20},
                      {AVDD, Vol_2800, 10},
                      {PDN, Vol_High, 10},
                      {SensorMCLK, Vol_High, 10},
              },
      },
#endif
#if defined(N28C2519DEPCXT3_MIPI_MONO)
      {
              SENSOR_DRVNAME_N28C2519DEPCXT3_MIPI_MONO,
              {
                      {PDN, Vol_Low, 1},
                      {DOVDD, Vol_1800, 20},
                      {AVDD, Vol_2800, 10},
                      {PDN, Vol_High, 10},
                      {SensorMCLK, Vol_High, 10},
              },
      },
#endif
#if defined(N28C2519DEPCXT4_MIPI_MONO)
      {
              SENSOR_DRVNAME_N28C2519DEPCXT4_MIPI_MONO,
              {
                      {PDN, Vol_Low, 1},
                      {DOVDD, Vol_1800, 20},
                      {AVDD, Vol_2800, 10},
                      {PDN, Vol_High, 10},
                      {SensorMCLK, Vol_High, 10},
              },
      },
#endif
#if defined(N28C2519DEPCXT5_MIPI_MONO)
      {
              SENSOR_DRVNAME_N28C2519DEPCXT5_MIPI_MONO,
              {
                      {PDN, Vol_Low, 1},
                      {DOVDD, Vol_1800, 20},
                      {AVDD, Vol_2800, 10},
                      {PDN, Vol_High, 10},
                      {SensorMCLK, Vol_High, 10},
              },
      },
#endif
//-S96818AA1-1936,chenming.wt,ADD,2023/04/11,c2519 depth sensor bringup
//+S96818AA1-1936,chenming.wt,ADD,2023/04/11,c2519 depth sensor bringup
#if defined(N28GC2375HDEPLH_MIPI_RAW)
      {
              SENSOR_DRVNAME_N28GC2375HDEPLH_MIPI_RAW,
              {
                      {DOVDD, Vol_1800, 20},
                      {DVDD, Vol_1800, 1},
                      {AVDD, Vol_2800, 10},
                      {SensorMCLK, Vol_High, 10},
                      {PDN, Vol_Low, 1},
              },
      },
#endif
#if defined(N28GC2375HDEPLH2_MIPI_RAW)
      {
              SENSOR_DRVNAME_N28GC2375HDEPLH2_MIPI_RAW,
              {
                      {DOVDD, Vol_1800, 20},
                      {DVDD, Vol_1800, 1},
                      {AVDD, Vol_2800, 10},
                      {SensorMCLK, Vol_High, 10},
                      {PDN, Vol_Low, 1},
              },
      },
#endif
#if defined(N28GC2375HDEPLH3_MIPI_RAW)
      {
              SENSOR_DRVNAME_N28GC2375HDEPLH3_MIPI_RAW,
              {
                      {DOVDD, Vol_1800, 20},
                      {DVDD, Vol_1800, 1},
                      {AVDD, Vol_2800, 10},
                      {SensorMCLK, Vol_High, 10},
                      {PDN, Vol_Low, 1},
              },
      },
#endif
#if defined(N28GC2375HDEPLH4_MIPI_RAW)
      {
              SENSOR_DRVNAME_N28GC2375HDEPLH4_MIPI_RAW,
              {
                      {DOVDD, Vol_1800, 20},
                      {DVDD, Vol_1800, 1},
                      {AVDD, Vol_2800, 10},
                      {SensorMCLK, Vol_High, 10},
                      {PDN, Vol_Low, 1},
              },
      },
#endif
#if defined(N28GC2375HDEPLH5_MIPI_RAW)
      {
              SENSOR_DRVNAME_N28GC2375HDEPLH5_MIPI_RAW,
              {
                      {DOVDD, Vol_1800, 20},
                      {DVDD, Vol_1800, 1},
                      {AVDD, Vol_2800, 10},
                      {SensorMCLK, Vol_High, 10},
                      {PDN, Vol_Low, 1},
              },
      },
#endif
//-S96818AA1-1936,chenming.wt,ADD,2023/04/11,c2519 depth sensor bringup

#if 0
#if defined(IMX519_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX519_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{AFVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{DOVDD, Vol_1800, 1},
			{SensorMCLK, Vol_High, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 8}
		},
	},
#endif
#if defined(IMX398_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX398_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(IMX350_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX350_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1200, 5},
			{SensorMCLK, Vol_High, 5},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
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
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2},

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
			{PDN, Vol_Low, 1},
			{RST, Vol_Low, 1},
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1000, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
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
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_High, 0},
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
#if defined(S5KGW3_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5KGW3_MIPI_RAW,
		{
			{RST, Vol_Low, 0},
			{DVDD, Vol_1100, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 1},
			{RST, Vol_High, 5},
			{SensorMCLK, Vol_High, 5}
		},
	},
#endif
#endif
	/* add new sensor before this line */
	{NULL,},
};

