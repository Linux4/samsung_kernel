/*
 * sec_charging_common_qc.h
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

enum power_supply_ext_property {
	POWER_SUPPLY_EXT_PROP_BATT_FET_CONTROL = POWER_SUPPLY_PROP_MAX,
	POWER_SUPPLY_EXT_PROP_BATT_CHG_TEMP,
	POWER_SUPPLY_EXT_PROP_BATT_TEMP_ADC,
	POWER_SUPPLY_EXT_PROP_MULTI_CHARGER_MODE,
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	POWER_SUPPLY_EXT_PROP_BATT_TEMP_TEST,
	POWER_SUPPLY_EXT_PROP_CHG_TEMP_TEST,
#endif
	POWER_SUPPLY_EXT_PROP_DIRECT_CHARGING_STEP,
	POWER_SUPPLY_EXT_PROP_DIRECT_CHARGING_VOL,
	POWER_SUPPLY_EXT_PROP_AFC_RESULT,
//	POWER_SUPPLY_EXT_PROP_VBAT_OVP, // need to check cisd
	POWER_SUPPLY_EXT_PROP_CHIP_ID,
	POWER_SUPPLY_EXT_PROP_DCHG_ENABLE,
	POWER_SUPPLY_EXT_PROP_CP_DIE_TEMP_ADC,
	POWER_SUPPLY_EXT_PROP_CHG_SWELLING_STATE,
	POWER_SUPPLY_EXT_PROP_CHARGE_CURRENT,
	POWER_SUPPLY_EXT_PROP_PON_OCV,
	POWER_SUPPLY_EXT_FIXED_RECHARGE_VBAT,
	POWER_SUPPLY_EXT_PROP_BATT_CYCLE,
	POWER_SUPPLY_EXT_PROP_SW_CURRENT_MAX,
	POWER_SUPPLY_EXT_PROP_SOC_RESCALE,
	POWER_SUPPLY_EXT_PROP_HV_DISABLE,
	POWER_SUPPLY_EXT_PROP_CHARGE_FULL,
	POWER_SUPPLY_EXT_PROP_FULL_CONDITION_SOC,
	POWER_SUPPLY_EXT_PROP_RECHARGE_VBAT_AGING,
	POWER_SUPPLY_EXT_PROP_RAW_CAP,
};

enum sec_battery_usb_conf {
	USB_CURRENT_UNCONFIGURED = 100,
	USB_CURRENT_HIGH_SPEED = 500,
	USB_CURRENT_SUPER_SPEED = 900,
};

enum sec_battery_voltage_mode {
	/* average voltage */
	SEC_BATTERY_VOLTAGE_AVERAGE = 0,
	/* open circuit voltage */
	SEC_BATTERY_VOLTAGE_OCV,
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

struct sec_bat_adc_table_data {
	int adc;
	int data;
};
#define sec_bat_adc_table_data_t \
	struct sec_bat_adc_table_data

struct sec_charging_current {
	unsigned int input_current_limit;
	unsigned int fast_charging_current;
};

#define sec_charging_current_t \
	struct sec_charging_current

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

extern unsigned int get_pd_max_power(void); /* Maximum PD power calculated from PDOs having max voltage less than equal to 11V */
extern int get_pd_cap_count(void); /* Number of PDOs having max voltage less than equal to 11V */
extern bool get_ps_ready_status(void); /*ps ready status*/

#ifndef CONFIG_OF
#define adc_init(pdev, pdata, channel)	\
	(((pdata)->adc_api)[((((pdata)->adc_type[(channel)]) <	\
	SEC_BATTERY_ADC_TYPE_NUM) ? ((pdata)->adc_type[(channel)]) :	\
	SEC_BATTERY_ADC_TYPE_NONE)].init((pdev)))

#define adc_exit(pdata, channel)	\
	(((pdata)->adc_api)[((pdata)->adc_type[(channel)])].exit())

#define adc_read(pdata, channel)	\
	(((pdata)->adc_api)[((pdata)->adc_type[(channel)])].read((channel)))
#endif

#define get_battery_data(driver)	\
	(((struct battery_data_t *)(driver)->pdata->battery_data)	\
	[(driver)->pdata->battery_type])

#define is_nocharge_type(cable_type) ( \
	cable_type <= POWER_SUPPLY_TYPE_BATTERY || \
	cable_type == POWER_SUPPLY_TYPE_OTG || \
	cable_type == POWER_SUPPLY_TYPE_POWER_SHARING)

#define is_slate_mode(battery) ((battery->current_event & SEC_BAT_CURRENT_EVENT_SLATE) \
		== SEC_BAT_CURRENT_EVENT_SLATE)
#endif /* __SEC_CHARGING_COMMON_H */
