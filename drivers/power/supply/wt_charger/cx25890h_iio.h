/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __CX25890H_IIO_H
#define __CX25890H_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>
#include <linux/module.h>

struct cx25890h_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define CX25890H_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define CX25890H_CHAN_ENERGY(_name, _num)			\
	CX25890H_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct cx25890h_iio_channels cx25890h_iio_psy_channels[] = {
	CX25890H_CHAN_ENERGY("charge_done", PSY_IIO_CHARGE_DONE)
	CX25890H_CHAN_ENERGY("main_chager_hz", PSY_IIO_MAIN_CHAGER_HZ)
	CX25890H_CHAN_ENERGY("main_input_current_settled", PSY_IIO_MAIN_INPUT_CURRENT_SETTLED)
	CX25890H_CHAN_ENERGY("main_input_voltage_settled", PSY_IIO_MAIN_INPUT_VOLTAGE_SETTLED)
	CX25890H_CHAN_ENERGY("main_charge_current", PSY_IIO_MAIN_CHAGER_CURRENT)
	CX25890H_CHAN_ENERGY("charger_enable", PSY_IIO_CHARGING_ENABLED)
	CX25890H_CHAN_ENERGY("otg_enable", PSY_IIO_OTG_ENABLE)
	CX25890H_CHAN_ENERGY("main_charger_term", PSY_IIO_MAIN_CHAGER_TERM)
	CX25890H_CHAN_ENERGY("batt_voltage_term", PSY_IIO_BATTERY_VOLTAGE_TERM)
	CX25890H_CHAN_ENERGY("charger_status", PSY_IIO_CHARGER_STATUS)
	CX25890H_CHAN_ENERGY("charger_type", PSY_IIO_CHARGE_TYPE)
	CX25890H_CHAN_ENERGY("vbus_voltage", PSY_IIO_SC_BUS_VOLTAGE)
	CX25890H_CHAN_ENERGY("apsd_rerun", PSY_IIO_APSD_RERUN)
	CX25890H_CHAN_ENERGY("main_charge_vbus_online", PSY_IIO_MAIN_CHARGE_VBUS_ONLINE)
	CX25890H_CHAN_ENERGY("set_ship_mode", PSY_IIO_SET_SHIP_MODE)
	CX25890H_CHAN_ENERGY("set_dp_dm",PSY_IIO_DP_DM)
	CX25890H_CHAN_ENERGY("main_feed_wdt",PSY_IIO_MAIN_CHARGE_FEED_WDT)
	CX25890H_CHAN_ENERGY("charge_pg_stat",PSY_IIO_CHARGE_PG_STAT)
};

enum cx25890h_iio_type {
	MAIN_CHG,
	SLAVE_CHG,
	BMS,
	WL,
	CP_MASTER,
	CP_SLAVE,
	AFC,
	WT_CHG,
};

enum wtcharge_iio_channels {
	WTCHG_UPDATE_WORK,
	WTCHG_VBUS_VAL_NOW,
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
	HV_DISABLE_DETECT,
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE

};

static const char * const wtchg_iio_chan_name[] = {
	[WTCHG_UPDATE_WORK] = "wtchg_update_work",
	[WTCHG_VBUS_VAL_NOW] = "wtchg_vbus_val_now",
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
	[HV_DISABLE_DETECT] = "wtchg_hv_disable_detect",
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE

};

enum pmic_iio_channels {
	VBUS_VOLTAGE,
};

static const char * const pmic_iio_chan_name[] = {
	[VBUS_VOLTAGE] = "vbus_dect",
};

enum afc_chg_iio_channels {
	AFC_DETECT,
	AFC_TYPE,
	AFC_DETECH,
};

static const char * const afc_chg_iio_chan_name[] = {
	[AFC_DETECT] = "afc_detect",
	[AFC_TYPE] = "afc_type",
	[AFC_DETECH] = "afc_detech",
};

enum batt_qg_exit_iio_channels {
	BATT_QG_CURRENT_NOW,
	BATT_QG_VOLTAGE_NOW,
};

static const char * const qg_ext_iio_chan_name[] = {
	[BATT_QG_CURRENT_NOW] = "qg_current_now",
	[BATT_QG_VOLTAGE_NOW] = "qg_voltage_now",
};

MODULE_LICENSE("GPL v2");

#endif

