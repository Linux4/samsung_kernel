/*
 * sec_charging_common.h
 * Samsung Mobile Charging Common Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SEC_CHARGING_COMMON_H
#define __SEC_CHARGING_COMMON_H __FILE__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/wakelock.h>

/* definitions */
#define SEC_BATTERY_CABLE_HV_WIRELESS_ETX	100

#define WC_AUTH_MSG		"@WC_AUTH "
#define WC_TX_MSG		"@Tx_Mode "

#define MFC_LDO_ON		1
#define MFC_LDO_OFF		0

enum power_supply_ext_property {
	POWER_SUPPLY_EXT_PROP_CHECK_SLAVE_I2C = POWER_SUPPLY_PROP_MAX,
	POWER_SUPPLY_EXT_PROP_MULTI_CHARGER_MODE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_OP_FREQ,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TRX_CMD,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TRX_VAL,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ID,
	POWER_SUPPLY_EXT_PROP_WIRELESS_ERR,
	POWER_SUPPLY_EXT_PROP_WIRELESS_SWITCH,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ENABLE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VOUT,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_IOUT,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_UNO_VIN,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_UNO_IIN,
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONNECTED,
	POWER_SUPPLY_EXT_PROP_WIRELESS_DUO_RX_POWER,
	POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_STATUS,	
	POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_DATA,
	POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_SIZE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_TYPE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR,
	POWER_SUPPLY_EXT_PROP_WIRELESS_MIN_DUTY,
	POWER_SUPPLY_EXT_PROP_WIRELESS_SEND_FSK,
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_VOUT,
	POWER_SUPPLY_EXT_PROP_AICL_CURRENT,
	POWER_SUPPLY_EXT_PROP_CHECK_MULTI_CHARGE,
	POWER_SUPPLY_EXT_PROP_CHIP_ID,
	POWER_SUPPLY_EXT_PROP_ERROR_CAUSE,
	POWER_SUPPLY_EXT_PROP_SYSOVLO,
	POWER_SUPPLY_EXT_PROP_VBAT_OVP,
	POWER_SUPPLY_EXT_PROP_USB_CONFIGURE,
	POWER_SUPPLY_EXT_PROP_WATER_DETECT,
	POWER_SUPPLY_EXT_PROP_SURGE,
	POWER_SUPPLY_EXT_PROP_WDT_STATUS,
	POWER_SUPPLY_EXT_PROP_HV_DISABLE,
	POWER_SUPPLY_EXT_PROP_SUB_PBA_TEMP_REC,
	POWER_SUPPLY_EXT_PROP_OVERHEAT_NOTIFY,
	POWER_SUPPLY_EXT_PROP_CHARGE_POWER,
	POWER_SUPPLY_EXT_PROP_MEASURE_SYS,
	POWER_SUPPLY_EXT_PROP_MEASURE_INPUT,
	POWER_SUPPLY_EXT_PROP_WC_CONTROL,
	POWER_SUPPLY_EXT_PROP_CHGINSEL,
	POWER_SUPPLY_EXT_PROP_JIG_GPIO,
	POWER_SUPPLY_EXT_PROP_OVERHEAT_HICCUP,
	POWER_SUPPLY_EXT_PROP_MONITOR_WORK,
	POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST,
	POWER_SUPPLY_EXT_PROP_AUTO_SHIPMODE_CONTROL,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TIMER_ON,
	POWER_SUPPLY_EXT_PROP_CALL_EVENT,
#if defined(CONFIG_DUAL_BATTERY)
	POWER_SUPPLY_EXT_PROP_CHGIN_OK,
	POWER_SUPPLY_EXT_PROP_SUPLLEMENT_MODE,
	POWER_SUPPLY_EXT_PROP_RECHG_ON,
	POWER_SUPPLY_EXT_PROP_EOC_ON,
	POWER_SUPPLY_EXT_PROP_DISCHG_MODE,
	POWER_SUPPLY_EXT_PROP_CHG_MODE,
	POWER_SUPPLY_EXT_PROP_CHG_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_BAT_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_CHG_CURRENT,
	POWER_SUPPLY_EXT_PROP_DISCHG_CURRENT,
	POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT,
	POWER_SUPPLY_EXT_PROP_TRICKLECHG_LIMIT_CURRENT,
	POWER_SUPPLY_EXT_PROP_DISCHG_LIMIT_CURRENT,
	POWER_SUPPLY_EXT_PROP_RECHG_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_EOC_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_EOC_CURRENT,
	POWER_SUPPLY_EXT_PROP_POWERMETER_ENABLE,
	POWER_SUPPLY_EXT_PROP_TSD_ENABLE,
	POWER_SUPPLY_EXT_PROP_DUAL_BAT_DET,
#endif
#if defined(CONFIG_BATTERY_SAMSUNG_MHS)
	POWER_SUPPLY_EXT_PROP_CHARGE_PORT,
#endif
#if defined(CONFIG_WIRELESS_TX_MODE)
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_AVG_CURR,
#endif
	POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE,
#if defined(CONFIG_DIRECT_CHARGING)
	POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE,
	POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC,
	POWER_SUPPLY_EXT_PROP_DIRECT_DONE,
	POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO,
	POWER_SUPPLY_EXT_PROP_DIRECT_INIT_INFO,
	POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL,
#endif
	POWER_SUPPLY_EXT_PROP_INBAT_VOLTAGE_FGSRC_SWITCHING,
	POWER_SUPPLY_EXT_PROP_FUELGAUGE_RESET,
	POWER_SUPPLY_EXT_PROP_FACTORY_VOLTAGE_REGULATION,
	POWER_SUPPLY_EXT_PROP_ANDIG_IVR_SWITCH,
	POWER_SUPPLY_EXT_PROP_FUELGAUGE_FACTORY,
	POWER_SUPPLY_EXT_PROP_WPC_DET_STATUS,
};

enum rx_device_type {
	NO_DEV = 0,
	SS_GEAR,
	SS_PHONE,
	OTHER_DEV,
};

enum sec_battery_usb_conf {
	USB_CURRENT_UNCONFIGURED = 100,
	USB_CURRENT_HIGH_SPEED = 500,
	USB_CURRENT_SUPER_SPEED = 850,
};

enum power_supply_ext_health {
	POWER_SUPPLY_HEALTH_VSYS_OVP = POWER_SUPPLY_HEALTH_MAX,
	POWER_SUPPLY_HEALTH_VBAT_OVP,
};

enum sec_battery_cable {
	SEC_BATTERY_CABLE_UNKNOWN = 0,
	SEC_BATTERY_CABLE_NONE,               	/* 1 */
	SEC_BATTERY_CABLE_PREPARE_TA,         	/* 2 */
	SEC_BATTERY_CABLE_TA,                 	/* 3 */
	SEC_BATTERY_CABLE_USB,                	/* 4 */
	SEC_BATTERY_CABLE_USB_CDP,            	/* 5 */
	SEC_BATTERY_CABLE_9V_TA,              	/* 6 */
	SEC_BATTERY_CABLE_9V_ERR,             	/* 7 */
	SEC_BATTERY_CABLE_9V_UNKNOWN,         	/* 8 */
	SEC_BATTERY_CABLE_12V_TA,             	/* 9 */
	SEC_BATTERY_CABLE_WIRELESS,           	/* 10 */
	SEC_BATTERY_CABLE_HV_WIRELESS,		/* 11 */
	SEC_BATTERY_CABLE_PMA_WIRELESS,       	/* 12 */
	SEC_BATTERY_CABLE_WIRELESS_PACK,      	/* 13 */
	SEC_BATTERY_CABLE_WIRELESS_HV_PACK,   	/* 14 */
	SEC_BATTERY_CABLE_WIRELESS_STAND,     	/* 15 */
	SEC_BATTERY_CABLE_WIRELESS_HV_STAND,  	/* 16 */
	SEC_BATTERY_CABLE_QC20,               	/* 17 */
	SEC_BATTERY_CABLE_QC30,               	/* 18 */
	SEC_BATTERY_CABLE_PDIC,                	/* 19 */
	SEC_BATTERY_CABLE_UARTOFF,            	/* 20 */
	SEC_BATTERY_CABLE_OTG,                	/* 21 */
	SEC_BATTERY_CABLE_LAN_HUB,            	/* 22 */
	SEC_BATTERY_CABLE_POWER_SHARING,      	/* 23 */
	SEC_BATTERY_CABLE_HMT_CONNECTED,      	/* 24 */
	SEC_BATTERY_CABLE_HMT_CHARGE,         	/* 25 */
	SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT,    	/* 26 */
	SEC_BATTERY_CABLE_WIRELESS_VEHICLE,	/* 27 */
	SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE,	/* 28 */
	SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV,	/* 29 */
	SEC_BATTERY_CABLE_TIMEOUT,	        /* 30 */
	SEC_BATTERY_CABLE_SMART_OTG,            /* 31 */
	SEC_BATTERY_CABLE_SMART_NOTG,           /* 32 */
	SEC_BATTERY_CABLE_WIRELESS_TX,			/* 33 */
	SEC_BATTERY_CABLE_HV_WIRELESS_20,		/* 34 */
	SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT,	/* 35 */
	SEC_BATTERY_CABLE_WIRELESS_FAKE, /* 36 */
	SEC_BATTERY_CABLE_PREPARE_WIRELESS_20,	/* 37 */
	SEC_BATTERY_CABLE_PDIC_APDO,		/* 38 */
	SEC_BATTERY_CABLE_MAX,			/* 39 */
};

enum sec_battery_voltage_mode {
	/* average voltage */
	SEC_BATTERY_VOLTAGE_AVERAGE = 0,
	/* open circuit voltage */
	SEC_BATTERY_VOLTAGE_OCV,
};

enum sec_battery_current_type {
	/* uA */
	SEC_BATTERY_CURRENT_UA = 0,
	/* mA */
	SEC_BATTERY_CURRENT_MA,
};

enum sec_battery_voltage_type {
	/* uA */
	SEC_BATTERY_VOLTAGE_UV = 0,
	/* mA */
	SEC_BATTERY_VOLTAGE_MV,
};

#if defined(CONFIG_DUAL_BATTERY)
enum sec_battery_dual_mode { 
	SEC_DUAL_BATTERY_MAIN = 0,
	SEC_DUAL_BATTERY_SUB,
};
#endif

enum sec_battery_capacity_mode {
	/* designed capacity */
	SEC_BATTERY_CAPACITY_DESIGNED = 0,
	/* absolute capacity by fuel gauge */
	SEC_BATTERY_CAPACITY_ABSOLUTE,
	/* temperary capacity in the time */
	SEC_BATTERY_CAPACITY_TEMPERARY,
	/* current capacity now */
	SEC_BATTERY_CAPACITY_CURRENT,
	/* cell aging information */
	SEC_BATTERY_CAPACITY_AGEDCELL,
	/* charge count */
	SEC_BATTERY_CAPACITY_CYCLE,
	/* full capacity rep */
	SEC_BATTERY_CAPACITY_FULL,
	/* QH capacity */
	SEC_BATTERY_CAPACITY_QH,
	/* vfsoc */
	SEC_BATTERY_CAPACITY_VFSOC,
};

enum sec_wireless_info_mode {
	SEC_WIRELESS_OTP_FIRM_RESULT = 0,
	SEC_WIRELESS_IC_REVISION,
	SEC_WIRELESS_IC_GRADE,
	SEC_WIRELESS_OTP_FIRM_VER_BIN,
	SEC_WIRELESS_OTP_FIRM_VER,
	SEC_WIRELESS_TX_FIRM_RESULT,
	SEC_WIRELESS_TX_FIRM_VER,
	SEC_TX_FIRMWARE,
	SEC_WIRELESS_OTP_FIRM_VERIFY,
	SEC_WIRELESS_MST_SWITCH_VERIFY,
};

enum sec_wireless_firm_update_mode {
	SEC_WIRELESS_RX_SDCARD_MODE = 0,
	SEC_WIRELESS_RX_BUILT_IN_MODE,
	SEC_WIRELESS_TX_ON_MODE,
	SEC_WIRELESS_TX_OFF_MODE,
	SEC_WIRELESS_RX_INIT,
};

enum sec_tx_sharing_mode {
	SEC_TX_OFF = 0,
	SEC_TX_STANDBY,
	SEC_TX_POWER_TRANSFER,
	SEC_TX_ERROR,
};

enum sec_wireless_auth_mode {
	WIRELESS_AUTH_WAIT = 0,
	WIRELESS_AUTH_START,
	WIRELESS_AUTH_SENT,
	WIRELESS_AUTH_RECEIVED,
	WIRELESS_AUTH_FAIL,
	WIRELESS_AUTH_PASS,
};

enum sec_wireless_control_mode {
	WIRELESS_VOUT_OFF = 0,
	WIRELESS_VOUT_NORMAL_VOLTAGE,	/* 5V , reserved by factory */
	WIRELESS_VOUT_RESERVED,			/* 6V */
	WIRELESS_VOUT_HIGH_VOLTAGE,		/* 9V , reserved by factory */
	WIRELESS_VOUT_CC_CV_VOUT,
	WIRELESS_VOUT_CALL,
	WIRELESS_VOUT_5V,
	WIRELESS_VOUT_9V,
	WIRELESS_VOUT_10V,
	WIRELESS_VOUT_5V_STEP,
	WIRELESS_VOUT_5_5V_STEP,
	WIRELESS_VOUT_9V_STEP,
	WIRELESS_VOUT_10V_STEP,
	WIRELESS_PAD_FAN_OFF,
	WIRELESS_PAD_FAN_ON,
	WIRELESS_PAD_LED_OFF,
	WIRELESS_PAD_LED_ON,
	WIRELESS_VRECT_ADJ_ON,
	WIRELESS_VRECT_ADJ_OFF,
	WIRELESS_VRECT_ADJ_ROOM_0,
	WIRELESS_VRECT_ADJ_ROOM_1,
	WIRELESS_VRECT_ADJ_ROOM_2,
	WIRELESS_VRECT_ADJ_ROOM_3,
	WIRELESS_VRECT_ADJ_ROOM_4,
	WIRELESS_VRECT_ADJ_ROOM_5,
	WIRELESS_CLAMP_ENABLE,
};

enum sec_wireless_tx_vout {
	WC_TX_VOUT_5_0V = 0,
	WC_TX_VOUT_5_5V,
	WC_TX_VOUT_6_0V,
	WC_TX_VOUT_6_5V,
	WC_TX_VOUT_7_0V,
	WC_TX_VOUT_7_5V,
	WC_TX_VOUT_8_0V,
	WC_TX_VOUT_8_5V,
	WC_TX_VOUT_9_0V,
	WC_TX_VOUT_OFF=100,
};

enum sec_wireless_pad_mode {
	SEC_WIRELESS_PAD_NONE = 0,
	SEC_WIRELESS_PAD_WPC,
	SEC_WIRELESS_PAD_WPC_HV,
	SEC_WIRELESS_PAD_WPC_PACK,
	SEC_WIRELESS_PAD_WPC_PACK_HV,
	SEC_WIRELESS_PAD_WPC_STAND,
	SEC_WIRELESS_PAD_WPC_STAND_HV,
	SEC_WIRELESS_PAD_PMA,
	SEC_WIRELESS_PAD_VEHICLE,
	SEC_WIRELESS_PAD_VEHICLE_HV,
	SEC_WIRELESS_PAD_PREPARE_HV,
	SEC_WIRELESS_PAD_TX,
	SEC_WIRELESS_PAD_WPC_PREPARE_DUO_HV_20,
	SEC_WIRELESS_PAD_WPC_DUO_HV_20,
	SEC_WIRELESS_PAD_WPC_DUO_HV_20_LIMIT,
	SEC_WIRELESS_PAD_FAKE,
};

enum sec_wireless_pad_id {
	WC_PAD_ID_UNKNOWN	= 0x00,
	/* 0x01~1F : Single Port */
	WC_PAD_ID_SNGL_NOBLE = 0x10,
	WC_PAD_ID_SNGL_VEHICLE,
	WC_PAD_ID_SNGL_MINI,
	WC_PAD_ID_SNGL_ZERO,
	WC_PAD_ID_SNGL_DREAM,
	/* 0x20~2F : Multi Port */
	/* 0x30~3F : Stand Type */
	WC_PAD_ID_STAND_HERO = 0x30,
	WC_PAD_ID_STAND_DREAM,
	/* 0x40~4F : External Battery Pack */
	WC_PAD_ID_EXT_BATT_PACK = 0x40,
	WC_PAD_ID_EXT_BATT_PACK_TA,
	/* 0x50~6F : Reserved */
	WC_PAD_ID_MAX = 0x6F,
};

enum sec_battery_temp_control_source {
	TEMP_CONTROL_SOURCE_NONE = 0,
	TEMP_CONTROL_SOURCE_BAT_THM,
	TEMP_CONTROL_SOURCE_CHG_THM,
	TEMP_CONTROL_SOURCE_WPC_THM,
	TEMP_CONTROL_SOURCE_USB_THM,
};

/* ADC type */
enum sec_battery_adc_type {
	/* NOT using this ADC channel */
	SEC_BATTERY_ADC_TYPE_NONE = 0,
	/* ADC in AP */
	SEC_BATTERY_ADC_TYPE_AP,
	/* ADC by additional IC */
	SEC_BATTERY_ADC_TYPE_IC,
	SEC_BATTERY_ADC_TYPE_NUM
};

enum sec_battery_adc_channel {
	SEC_BAT_ADC_CHANNEL_CABLE_CHECK = 0,
	SEC_BAT_ADC_CHANNEL_BAT_CHECK,
	SEC_BAT_ADC_CHANNEL_TEMP,
	SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT,
	SEC_BAT_ADC_CHANNEL_FULL_CHECK,
	SEC_BAT_ADC_CHANNEL_VOLTAGE_NOW,
	SEC_BAT_ADC_CHANNEL_CHG_TEMP,
	SEC_BAT_ADC_CHANNEL_INBAT_VOLTAGE,
	SEC_BAT_ADC_CHANNEL_DISCHARGING_CHECK,
	SEC_BAT_ADC_CHANNEL_DISCHARGING_NTC,
	SEC_BAT_ADC_CHANNEL_WPC_TEMP,
	SEC_BAT_ADC_CHANNEL_SLAVE_CHG_TEMP,
	SEC_BAT_ADC_CHANNEL_USB_TEMP,
	SEC_BAT_ADC_CHANNEL_SUB_BAT_TEMP,
	SEC_BAT_ADC_CHANNEL_NUM,
};

enum sec_battery_charge_mode {
	SEC_BAT_CHG_MODE_CHARGING = 0, /* buck, chg on */
	SEC_BAT_CHG_MODE_CHARGING_OFF,
	SEC_BAT_CHG_MODE_BUCK_OFF, /* buck, chg off */
//	SEC_BAT_CHG_MODE_BUCK_ON,
	SEC_BAT_CHG_MODE_OTG_ON,
	SEC_BAT_CHG_MODE_OTG_OFF,
	SEC_BAT_CHG_MODE_UNO_ON,
	SEC_BAT_CHG_MODE_UNO_OFF,
	SEC_BAT_CHG_MODE_UNO_ONLY,
	SEC_BAT_CHG_MODE_MAX,
};

/* charging mode */
enum sec_battery_charging_mode {
	/* no charging */
	SEC_BATTERY_CHARGING_NONE = 0,
	/* 1st charging */
	SEC_BATTERY_CHARGING_1ST,
	/* 2nd charging */
	SEC_BATTERY_CHARGING_2ND,
	/* recharging */
	SEC_BATTERY_CHARGING_RECHARGING,
};

/* POWER_SUPPLY_EXT_PROP_MEASURE_SYS */
enum sec_battery_measure_sys {
	SEC_BATTERY_ISYS_MA = 0,
	SEC_BATTERY_ISYS_UA,
	SEC_BATTERY_ISYS_AVG_MA,
	SEC_BATTERY_ISYS_AVG_UA,
	SEC_BATTERY_VSYS,
};

/* POWER_SUPPLY_EXT_PROP_MEASURE_INPUT */
enum sec_battery_measure_input {
	SEC_BATTERY_IIN_MA = 0,
	SEC_BATTERY_IIN_UA,
	SEC_BATTERY_VBYP,
	SEC_BATTERY_VIN_MA,
	SEC_BATTERY_VIN_UA,
};

/* tx_event */
#define BATT_TX_EVENT_WIRELESS_TX_STATUS		0x00000001
#define BATT_TX_EVENT_WIRELESS_RX_CONNECT		0x00000002
#define BATT_TX_EVENT_WIRELESS_TX_FOD			0x00000004
#define BATT_TX_EVENT_WIRELESS_TX_HIGH_TEMP		0x00000008
#define BATT_TX_EVENT_WIRELESS_RX_UNSAFE_TEMP	0x00000010
#define BATT_TX_EVENT_WIRELESS_RX_CHG_SWITCH	0x00000020
#define BATT_TX_EVENT_WIRELESS_RX_CS100			0x00000040
#define BATT_TX_EVENT_WIRELESS_TX_OTG_ON		0x00000080
#define BATT_TX_EVENT_WIRELESS_TX_LOW_TEMP		0x00000100
#define BATT_TX_EVENT_WIRELESS_TX_SOC_DRAIN		0x00000200
#define BATT_TX_EVENT_WIRELESS_TX_CRITICAL_EOC	0x00000400
#define BATT_TX_EVENT_WIRELESS_TX_CAMERA_ON		0x00000800
#define BATT_TX_EVENT_WIRELESS_TX_OCP			0x00001000
#define BATT_TX_EVENT_WIRELESS_TX_MISALIGN      0x00002000
#define BATT_TX_EVENT_WIRELESS_TX_ETC			0x00004000
#define BATT_TX_EVENT_WIRELESS_TX_RETRY			0x00008000
#define BATT_TX_EVENT_WIRELESS_ALL_MASK			0x0000ffff

#define SEC_BAT_ERROR_CAUSE_NONE		0x0000
#define SEC_BAT_ERROR_CAUSE_FG_INIT_FAIL	0x0001
#define SEC_BAT_ERROR_CAUSE_I2C_FAIL		0xFFFFFFFF

#define SEC_BAT_TX_RETRY_NONE			0x0000
#define SEC_BAT_TX_RETRY_MISALIGN		0x0001
#define SEC_BAT_TX_RETRY_CAMERA			0x0002
#define SEC_BAT_TX_RETRY_CALL			0x0004
#define SEC_BAT_TX_RETRY_MIX_TEMP		0x0008
#define SEC_BAT_TX_RETRY_HIGH_TEMP		0x0010
#define SEC_BAT_TX_RETRY_LOW_TEMP		0x0020

/* ext_event */
#define BATT_EXT_EVENT_NONE			0x00000000
#define BATT_EXT_EVENT_CAMERA		0x00000001
#define BATT_EXT_EVENT_DEX			0x00000002
#define BATT_EXT_EVENT_CALL			0x00000004

/* monitor activation */
enum sec_battery_polling_time_type {
	/* same order with power supply status */
	SEC_BATTERY_POLLING_TIME_BASIC = 0,
	SEC_BATTERY_POLLING_TIME_CHARGING,
	SEC_BATTERY_POLLING_TIME_DISCHARGING,
	SEC_BATTERY_POLLING_TIME_NOT_CHARGING,
	SEC_BATTERY_POLLING_TIME_SLEEP,
};

enum sec_battery_monitor_polling {
	/* polling work queue */
	SEC_BATTERY_MONITOR_WORKQUEUE,
	/* alarm polling */
	SEC_BATTERY_MONITOR_ALARM,
	/* timer polling (NOT USE) */
	SEC_BATTERY_MONITOR_TIMER,
};
#define sec_battery_monitor_polling_t \
	enum sec_battery_monitor_polling

/* full charged check : POWER_SUPPLY_PROP_STATUS */
enum sec_battery_full_charged {
	SEC_BATTERY_FULLCHARGED_NONE = 0,
	/* current check by ADC */
	SEC_BATTERY_FULLCHARGED_ADC,
	/* fuel gauge current check */
	SEC_BATTERY_FULLCHARGED_FG_CURRENT,
	/* time check */
	SEC_BATTERY_FULLCHARGED_TIME,
	/* SOC check */
	SEC_BATTERY_FULLCHARGED_SOC,
	/* charger GPIO, NO additional full condition */
	SEC_BATTERY_FULLCHARGED_CHGGPIO,
	/* charger interrupt, NO additional full condition */
	SEC_BATTERY_FULLCHARGED_CHGINT,
	/* charger power supply property, NO additional full condition */
	SEC_BATTERY_FULLCHARGED_CHGPSY,
	/* Limiter power supply property, NO additional full condition */
	SEC_BATTERY_FULLCHARGED_LIMITER,
};

#if defined(CONFIG_BATTERY_SAMSUNG_MHS)
enum charging_port {
	PORT_NONE = 0,
	MAIN_PORT,
	SUB_PORT,
};
#endif

#define sec_battery_full_charged_t \
	enum sec_battery_full_charged

/* full check condition type (can be used overlapped) */
#define sec_battery_full_condition_t unsigned int
/* SEC_BATTERY_FULL_CONDITION_NOTIMEFULL
  * full-charged by absolute-timer only in high voltage
  */
#define SEC_BATTERY_FULL_CONDITION_NOTIMEFULL	1
/* SEC_BATTERY_FULL_CONDITION_NOSLEEPINFULL
  * do not set polling time as sleep polling time in full-charged
  */
#define SEC_BATTERY_FULL_CONDITION_NOSLEEPINFULL	2
/* SEC_BATTERY_FULL_CONDITION_SOC
  * use capacity for full-charged check
  */
#define SEC_BATTERY_FULL_CONDITION_SOC		4
/* SEC_BATTERY_FULL_CONDITION_VCELL
  * use VCELL for full-charged check
  */
#define SEC_BATTERY_FULL_CONDITION_VCELL	8
/* SEC_BATTERY_FULL_CONDITION_AVGVCELL
  * use average VCELL for full-charged check
  */
#define SEC_BATTERY_FULL_CONDITION_AVGVCELL	16
/* SEC_BATTERY_FULL_CONDITION_OCV
  * use OCV for full-charged check
  */
#define SEC_BATTERY_FULL_CONDITION_OCV		32

/* recharge check condition type (can be used overlapped) */
#define sec_battery_recharge_condition_t unsigned int
/* SEC_BATTERY_RECHARGE_CONDITION_SOC
  * use capacity for recharging check
  */
#define SEC_BATTERY_RECHARGE_CONDITION_SOC		1
/* SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL
  * use average VCELL for recharging check
  */
#define SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL		2
/* SEC_BATTERY_RECHARGE_CONDITION_VCELL
  * use VCELL for recharging check
  */
#define SEC_BATTERY_RECHARGE_CONDITION_VCELL		4

/* SEC_BATTERY_RECHARGE_CONDITION_LIMITER
 * use VCELL of LIMITER for recharging check
 */
#define SEC_BATTERY_RECHARGE_CONDITION_LIMITER		8


/* battery check : POWER_SUPPLY_PROP_PRESENT */
enum sec_battery_check {
	/* No Check for internal battery */
	SEC_BATTERY_CHECK_NONE,
	/* by ADC */
	SEC_BATTERY_CHECK_ADC,
	/* by callback function (battery certification by 1 wired)*/
	SEC_BATTERY_CHECK_CALLBACK,
	/* by PMIC */
	SEC_BATTERY_CHECK_PMIC,
	/* by fuel gauge */
	SEC_BATTERY_CHECK_FUELGAUGE,
	/* by charger */
	SEC_BATTERY_CHECK_CHARGER,
	/* by interrupt (use check_battery_callback() to check battery) */
	SEC_BATTERY_CHECK_INT,
#if defined(CONFIG_DUAL_BATTERY)
	/* by dual battery */
	SEC_BATTERY_CHECK_DUAL_BAT_GPIO,
#endif	
};
#define sec_battery_check_t \
	enum sec_battery_check

/* OVP, UVLO check : POWER_SUPPLY_PROP_HEALTH */
enum sec_battery_ovp_uvlo {
	/* by callback function */
	SEC_BATTERY_OVP_UVLO_CALLBACK,
	/* by PMIC polling */
	SEC_BATTERY_OVP_UVLO_PMICPOLLING,
	/* by PMIC interrupt */
	SEC_BATTERY_OVP_UVLO_PMICINT,
	/* by charger polling */
	SEC_BATTERY_OVP_UVLO_CHGPOLLING,
	/* by charger interrupt */
	SEC_BATTERY_OVP_UVLO_CHGINT,
};
#define sec_battery_ovp_uvlo_t \
	enum sec_battery_ovp_uvlo

/* thermal source */
enum sec_battery_thermal_source {
	/* by fuel gauge */
	SEC_BATTERY_THERMAL_SOURCE_FG,
	/* by external source */
	SEC_BATTERY_THERMAL_SOURCE_CALLBACK,
	/* by ADC */
	SEC_BATTERY_THERMAL_SOURCE_ADC,
	/* by charger */
	SEC_BATTERY_THERMAL_SOURCE_CHG_ADC,
	/* none */
	SEC_BATTERY_THERMAL_SOURCE_NONE,
};
#define sec_battery_thermal_source_t \
	enum sec_battery_thermal_source

/* temperature check type */
enum sec_battery_temp_check {
	SEC_BATTERY_TEMP_CHECK_NONE = 0,	/* no temperature check */
	SEC_BATTERY_TEMP_CHECK_ADC, 		/* by ADC value */
	SEC_BATTERY_TEMP_CHECK_TEMP,		/* by temperature */
	SEC_BATTERY_TEMP_CHECK_FAKE,		/* by a fake temperature */
};
#define sec_battery_temp_check_t \
	enum sec_battery_temp_check

/* cable check (can be used overlapped) */
#define sec_battery_cable_check_t unsigned int
/* SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE
  * for USB cable in tablet model,
  * status is stuck into discharging,
  * but internal charging logic is working
  */
#define SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE		1
/* SEC_BATTERY_CABLE_CHECK_NOINCOMPATIBLECHARGE
  * for incompatible charger
  * (Not compliant to USB specification,
  *  cable type is SEC_BATTERY_CABLE_UNKNOWN),
  * do NOT charge and show message to user
  * (only for VZW)
  */
#define SEC_BATTERY_CABLE_CHECK_NOINCOMPATIBLECHARGE	2
/* SEC_BATTERY_CABLE_CHECK_PSY
  * check cable by power supply set_property
  */
#define SEC_BATTERY_CABLE_CHECK_PSY			4
/* SEC_BATTERY_CABLE_CHECK_INT
  * check cable by interrupt
  */
#define SEC_BATTERY_CABLE_CHECK_INT			8
/* SEC_BATTERY_CABLE_CHECK_CHGINT
  * check cable by charger interrupt
  */
#define SEC_BATTERY_CABLE_CHECK_CHGINT			16
/* SEC_BATTERY_CABLE_CHECK_POLLING
  * check cable by GPIO polling
  */
#define SEC_BATTERY_CABLE_CHECK_POLLING			32

/* check cable source (can be used overlapped) */
#define sec_battery_cable_source_t unsigned int
/* SEC_BATTERY_CABLE_SOURCE_EXTERNAL
 * already given by external argument
 */
#define	SEC_BATTERY_CABLE_SOURCE_EXTERNAL	1
/* SEC_BATTERY_CABLE_SOURCE_CALLBACK
 * by callback (MUIC, USB switch)
 */
#define	SEC_BATTERY_CABLE_SOURCE_CALLBACK	2
/* SEC_BATTERY_CABLE_SOURCE_ADC
 * by ADC
 */
#define	SEC_BATTERY_CABLE_SOURCE_ADC		4

/* capacity calculation type (can be used overlapped) */
#define sec_fuelgauge_capacity_type_t int
/* SEC_FUELGAUGE_CAPACITY_TYPE_RESET
  * use capacity information to reset fuel gauge
  * (only for driver algorithm, can NOT be set by user)
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_RESET	(-1)
/* SEC_FUELGAUGE_CAPACITY_TYPE_RAW
  * use capacity information from fuel gauge directly
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_RAW		1
/* SEC_FUELGAUGE_CAPACITY_TYPE_SCALE
  * rescale capacity by scaling, need min and max value for scaling
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_SCALE	2
/* SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE
  * change only maximum capacity dynamically
  * to keep time for every SOC unit
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE	4
/* SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC
  * change capacity value by only -1 or +1
  * no sudden change of capacity
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC	8
/* SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL
  * skip current capacity value
  * if it is abnormal value
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL	16

#define SEC_FUELGAUGE_CAPACITY_TYPE_CAPACITY_POINT	32

/* charger function settings (can be used overlapped) */
#define sec_charger_functions_t unsigned int
/* SEC_CHARGER_NO_GRADUAL_CHARGING_CURRENT
 * disable gradual charging current setting
 * SUMMIT:AICL, MAXIM:regulation loop
 */
#define SEC_CHARGER_NO_GRADUAL_CHARGING_CURRENT		1

/* SEC_CHARGER_MINIMUM_SIOP_CHARGING_CURRENT
 * charging current should be over than USB charging current
 */
#define SEC_CHARGER_MINIMUM_SIOP_CHARGING_CURRENT	2

#if defined(CONFIG_BATTERY_AGE_FORECAST)
typedef struct sec_age_data {
	unsigned int cycle;
	unsigned int float_voltage;
	unsigned int recharge_condition_vcell;
	unsigned int full_condition_vcell;
	unsigned int full_condition_soc;
#if defined(CONFIG_STEP_CHARGING)
	unsigned int step_charging_condition;
#endif
} sec_age_data_t;
#endif

static inline struct power_supply *get_power_supply_by_name(char *name)
{
	if (!name)
		return (struct power_supply *)NULL;
	else
		return power_supply_get_by_name(name);
}

#define psy_do_property(name, function, property, value) \
({	\
	struct power_supply *psy;	\
	int ret = 0;	\
	psy = get_power_supply_by_name((name));	\
	if (!psy) {	\
		pr_err("%s: Fail to "#function" psy (%s)\n",	\
			__func__, (name));	\
		value.intval = 0;	\
		ret = -ENOENT;	\
	} else {	\
		if (psy->desc->function##_property != NULL) { \
			ret = psy->desc->function##_property(psy, \
				(enum power_supply_property) (property), &(value)); \
			if (ret < 0) {	\
				pr_err("%s: Fail to %s "#function" (%d=>%d)\n", \
						__func__, name, (property), ret);	\
				value.intval = 0;	\
			}	\
		} else {	\
			ret = -ENOSYS;	\
		}	\
		power_supply_put(psy);		\
	}					\
	ret;	\
})

#define is_hv_wireless_pad_type(cable_type) ( \
	cable_type == SEC_WIRELESS_PAD_WPC_HV || \
	cable_type == SEC_WIRELESS_PAD_WPC_PACK_HV || \
	cable_type == SEC_WIRELESS_PAD_WPC_STAND_HV || \
	cable_type == SEC_WIRELESS_PAD_VEHICLE_HV || \
	cable_type == SEC_WIRELESS_PAD_WPC_DUO_HV_20 || \
	cable_type == SEC_WIRELESS_PAD_WPC_DUO_HV_20_LIMIT)

#define is_nv_wireless_pad_type(cable_type) ( \
	cable_type == SEC_WIRELESS_PAD_WPC || \
	cable_type == SEC_WIRELESS_PAD_WPC_PACK || \
	cable_type == SEC_WIRELESS_PAD_WPC_STAND || \
	cable_type == SEC_WIRELESS_PAD_PMA || \
	cable_type == SEC_WIRELESS_PAD_VEHICLE || \
	cable_type == SEC_WIRELESS_PAD_WPC_PREPARE_DUO_HV_20 || \
	cable_type == SEC_WIRELESS_PAD_PREPARE_HV)

#define is_wireless_pad_type(cable_type) \
	(is_hv_wireless_pad_type(cable_type) || is_nv_wireless_pad_type(cable_type))

#define is_hv_wireless_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS || \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_ETX || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_STAND || \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20 || \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_PACK)

#define is_nv_wireless_type(cable_type)	( \
	cable_type == SEC_BATTERY_CABLE_WIRELESS || \
	cable_type == SEC_BATTERY_CABLE_PMA_WIRELESS || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_VEHICLE || \
	cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20 || \
	cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV || \
	cable_type == SEC_WIRELESS_PAD_WPC_PREPARE_DUO_HV_20 || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_TX)

#define is_wireless_type(cable_type) \
	(is_hv_wireless_type(cable_type) || is_nv_wireless_type(cable_type))

#define is_not_wireless_type(cable_type) ( \
	cable_type != SEC_BATTERY_CABLE_WIRELESS && \
	cable_type != SEC_BATTERY_CABLE_PMA_WIRELESS && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_PACK && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_STAND && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS_ETX && \
	cable_type != SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_STAND && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_VEHICLE && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_TX && \
	cable_type != SEC_BATTERY_CABLE_PREPARE_WIRELESS_20 && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS_20 && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_PACK)

#define is_wired_type(cable_type) \
	(is_not_wireless_type(cable_type) && (cable_type != SEC_BATTERY_CABLE_NONE) && \
	(cable_type != SEC_BATTERY_CABLE_OTG))

#define is_hv_qc_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_QC20 || \
	cable_type == SEC_BATTERY_CABLE_QC30)

#define is_hv_afc_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_9V_ERR || \
	cable_type == SEC_BATTERY_CABLE_9V_TA || \
	cable_type == SEC_BATTERY_CABLE_9V_UNKNOWN || \
	cable_type == SEC_BATTERY_CABLE_12V_TA)

#define is_hv_wire_9v_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_9V_ERR || \
	cable_type == SEC_BATTERY_CABLE_9V_TA || \
	cable_type == SEC_BATTERY_CABLE_9V_UNKNOWN || \
	cable_type == SEC_BATTERY_CABLE_QC20)

#define is_hv_wire_12v_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_12V_TA || \
	cable_type == SEC_BATTERY_CABLE_QC30)

#define is_hv_wire_type(cable_type) ( \
	is_hv_afc_wire_type(cable_type) || is_hv_qc_wire_type(cable_type))

#define is_nocharge_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_NONE || \
	cable_type == SEC_BATTERY_CABLE_OTG || \
	cable_type == SEC_BATTERY_CABLE_POWER_SHARING)

#define is_slate_mode(battery) ((battery->current_event & SEC_BAT_CURRENT_EVENT_SLATE) \
		== SEC_BAT_CURRENT_EVENT_SLATE)

#if defined(CONFIG_PDIC_PD30)
#define is_pd_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_PDIC || \
	cable_type == SEC_BATTERY_CABLE_PDIC_APDO)

#define is_pd_apdo_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_PDIC_APDO)
#else
#define is_pd_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_PDIC)
#endif
#endif /* __SEC_CHARGING_COMMON_H */
