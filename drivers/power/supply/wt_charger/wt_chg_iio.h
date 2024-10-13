/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __BATTERY_IIO_H
#define __BATTERY_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/module.h>

enum wtchg_iio_type {
	MAIN_CHG,
	SLAVE_CHG,
	BMS,
	WL,
	CP_MASTER,
	CP_SLAVE,
	AFC,
};

struct wtchg_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define WTCHG_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define WTCHG_CHAN_ENERGY(_name, _num)			\
	WTCHG_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct wtchg_iio_channels wtchg_iio_psy_channels[] = {
	WTCHG_CHAN_ENERGY("pd_active", PSY_IIO_PD_ACTIVE)
	WTCHG_CHAN_ENERGY("pd_usb_suspend_supported", PSY_IIO_PD_USB_SUSPEND_SUPPORTED)
	WTCHG_CHAN_ENERGY("pd_in_hard_reset", PSY_IIO_PD_IN_HARD_RESET)
	WTCHG_CHAN_ENERGY("pd_current_max", PSY_IIO_PD_CURRENT_MAX)
	WTCHG_CHAN_ENERGY("pd_voltage_min", PSY_IIO_PD_VOLTAGE_MIN)
	WTCHG_CHAN_ENERGY("pd_voltage_max", PSY_IIO_PD_VOLTAGE_MAX)
	WTCHG_CHAN_ENERGY("real_type", PSY_IIO_USB_REAL_TYPE)
	WTCHG_CHAN_ENERGY("otg_enable", PSY_IIO_OTG_ENABLE)
	WTCHG_CHAN_ENERGY("typec_cc_orientation", PSY_IIO_TYPEC_CC_ORIENTATION)
	WTCHG_CHAN_ENERGY("apdo_max_volt", PSY_IIO_APDO_MAX_VOLT)
	WTCHG_CHAN_ENERGY("apdo_max_curr", PSY_IIO_APDO_MAX_CURR)
	WTCHG_CHAN_ENERGY("typec_mode", PSY_IIO_TYPEC_MODE)
	WTCHG_CHAN_ENERGY("wtchg_update_work", PSY_IIO_WTCHG_UPDATE_WORK)
	WTCHG_CHAN_ENERGY("wtchg_vbus_val_now", PSY_IIO_VBUS_VAL_NOW)
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
	WTCHG_CHAN_ENERGY("wtchg_hv_disable_detect", PSY_IIO_HV_DISABLE_DETECT)
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
};

enum batt_qg_exit_iio_channels {
	BATT_QG_PRESENT,
	BATT_QG_STATUS,
	BATT_QG_CAPACITY,
	BATT_QG_CURRENT_NOW,
	BATT_QG_VOLTAGE_NOW,
	//P86801AA1-3622,gudi.wt,20230705,battery protect func
	BATT_QG_BATTERY_CYCLE,
	BATT_QG_VOLTAGE_MAX,
	BATT_QG_CHARGE_FULL,
	BATT_QG_RESISTANCE_ID,
	BATT_QG_TEMP,
	BATT_QG_CYCLE_COUNT,
	BATT_QG_CHARGE_FULL_DESIGN,
	BATT_QG_TIME_TO_FULL_NOW,
	BATT_QG_FCC_MAX,
	BATT_QG_CHIP_OK,
	BATT_QG_BATTERY_AUTH,
	BATT_QG_SOC_DECIMAL,
	BATT_QG_SOC_DECIMAL_RATE,
	BATT_QG_BATTERY_ID,
};

//+bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function
static const char * const qg_ext_iio_chan_name[] = {
	[BATT_QG_PRESENT] = "qg_present",
	[BATT_QG_STATUS] = "qg_status",
	[BATT_QG_CAPACITY] = "qg_capacity",
	[BATT_QG_CURRENT_NOW] = "qg_current_now",
	[BATT_QG_VOLTAGE_NOW] = "qg_voltage_now",
	//P86801AA1-3622,gudi.wt,20230705,battery protect func
	[BATT_QG_BATTERY_CYCLE] = "qg_battery_cycle",
	[BATT_QG_VOLTAGE_MAX] = "qg_voltage_max",
	[BATT_QG_CHARGE_FULL] = "qg_charge_full",
	[BATT_QG_RESISTANCE_ID] = "qg_resistance_id",
	[BATT_QG_TEMP] = "qg_temp",
	[BATT_QG_CYCLE_COUNT] = "qg_cycle_count",
	[BATT_QG_CHARGE_FULL_DESIGN] = "qg_charge_full_design",
	[BATT_QG_TIME_TO_FULL_NOW] = "qg_time_to_full_now",
	[BATT_QG_FCC_MAX] = "qg_therm_curr",
	[BATT_QG_CHIP_OK] = "qg_chip_ok",
	[BATT_QG_BATTERY_AUTH] = "qg_battery_auth",
	[BATT_QG_SOC_DECIMAL] = "qg_soc_decimal",
	[BATT_QG_SOC_DECIMAL_RATE] = "qg_soc_decimal_rate",
	[BATT_QG_BATTERY_ID] = "qg_battery_id",
};
//-bug 761884, liyiying.wt, mod, 2022/7/5, add cw2217 iio function

enum main_iio_channels {
	MAIN_CHARGER_DONE,
	MAIN_CHARGER_HZ,
	MAIN_INPUT_CURRENT_SETTLED,
	MAIN_INPUT_VOLTAGE_SETTLED,
	MAIN_CHAGER_CURRENT,
	MAIN_CHARGING_ENABLED,
	MAIN_OTG_ENABLE,
	MAIN_CHAGER_TERM,
	MAIN_CHARGER_VOLTAGE_TERM,
	MAIN_CHARGER_STATUS,
	MAIN_CHARGER_TYPE,
	MAIN_BUS_VOLTAGE,
	MAIN_CHARGE_APSD_RERUN,
	MAIN_CHARGE_VBUS_ONLINE,
	MAIN_CHARGE_SET_SHIP_MODE,
	MAIN_CHARGE_SET_DP_DM,
	MAIN_CHARGE_FEED_WDT,
};

static const char * const main_iio_chan_name[] = {
	[MAIN_CHARGER_DONE] = "main_charge_done",
	[MAIN_CHARGER_HZ] = "main_chager_hz",
	[MAIN_INPUT_CURRENT_SETTLED] = "main_input_current_settled",
	[MAIN_INPUT_VOLTAGE_SETTLED] = "main_input_voltage_settled",
	[MAIN_CHAGER_CURRENT] = "main_charge_current",
	[MAIN_CHARGING_ENABLED] = "main_charger_enable",
	[MAIN_OTG_ENABLE] = "main_otg_enable",
	[MAIN_CHAGER_TERM] = "main_charger_term",
	[MAIN_CHARGER_VOLTAGE_TERM] = "main_batt_voltage_term",
	[MAIN_CHARGER_STATUS] = "main_charger_status",
	[MAIN_CHARGER_TYPE] = "main_charger_type",
	[MAIN_BUS_VOLTAGE] = "main_vbus_voltage",
	[MAIN_CHARGE_APSD_RERUN] = "main_apsd_rerun",
	[MAIN_CHARGE_VBUS_ONLINE] = "main_charge_vbus_online",
	[MAIN_CHARGE_SET_SHIP_MODE] = "main_set_ship_mode",
	[MAIN_CHARGE_SET_DP_DM] = "main_set_dp_dp",
	[MAIN_CHARGE_FEED_WDT] = "main_feed_wdt",
};

enum wl_chg_iio_channels {
	WL_CHARGING_ENABLED,
	WL_CHARGE_VBUS_ONLINE,
	WL_CHARGE_TYPE,
	WL_CHARGE_VBUS_VOLT,
	WL_CHARGE_FOD_UPDATE,
};

static const char * const wl_chg_iio_chan_name[] = {
	[WL_CHARGING_ENABLED] = "wl_charger_enable",
	[WL_CHARGE_VBUS_ONLINE] = "wl_vbus_online",
	[WL_CHARGE_TYPE] = "wl_charge_type",
	[WL_CHARGE_VBUS_VOLT] = "wl_charge_vbus_volt",
	[WL_CHARGE_FOD_UPDATE] = "wl_charge_fod_update",
};



enum afc_chg_iio_channels {
	AFC_DETECT,
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
	AFC_SET_VOL,
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
};

static const char * const afc_chg_iio_chan_name[] = {
	[AFC_DETECT] = "afc_detect",
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
	[AFC_SET_VOL] = "afc_set_vol",
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
};

struct quick_charge {
	enum power_supply_type adap_type;
	enum power_supply_quick_charge_type adap_cap;
};

MODULE_LICENSE("GPL v2");

#endif
