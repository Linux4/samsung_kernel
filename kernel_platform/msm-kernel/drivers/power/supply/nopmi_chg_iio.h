/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __NOPMI_CHG_IIO_H
#define __NOPMI_CHG_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct nopmi_chg_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define UPM6720_IIO_CHANNEL_OFFSET 4

#define NOPMI_CHG_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define NOPMI_CHG_CHAN_ENERGY(_name, _num)			\
	NOPMI_CHG_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct nopmi_chg_iio_channels  nopmi_chg_iio_psy_channels[] = {
	NOPMI_CHG_CHAN_ENERGY("battery_charging_enabled", PSY_IIO_CHARGING_ENABLED)
	//NOPMI_CHG_CHAN_ENERGY("pd_active", PSY_IIO_PD_ACTIVE)
	//NOPMI_CHG_CHAN_ENERGY("pd_usb_suspend_supported", PSY_IIO_PD_USB_SUSPEND_SUPPORTED)
	//NOPMI_CHG_CHAN_ENERGY("pd_in_hard_reset", PSY_IIO_PD_IN_HARD_RESET)
	//NOPMI_CHG_CHAN_ENERGY("pd_current_max", PSY_IIO_PD_CURRENT_MAX)
	//NOPMI_CHG_CHAN_ENERGY("pd_voltage_min", PSY_IIO_PD_VOLTAGE_MIN)
	//NOPMI_CHG_CHAN_ENERGY("pd_voltage_max", PSY_IIO_PD_VOLTAGE_MAX)
	NOPMI_CHG_CHAN_ENERGY("usb_real_type", PSY_IIO_USB_REAL_TYPE)
	NOPMI_CHG_CHAN_ENERGY("typec_cc_orientation", PSY_IIO_TYPEC_CC_ORIENTATION)
	NOPMI_CHG_CHAN_ENERGY("typec_mode", PSY_IIO_TYPEC_MODE)
	NOPMI_CHG_CHAN_ENERGY("input_suspend", PSY_IIO_INPUT_SUSPEND)
	NOPMI_CHG_CHAN_ENERGY("mtbf_cur", PSY_IIO_MTBF_CUR)
	//NOPMI_CHG_CHAN_ENERGY("apdo_volt", PSY_IIO_APDO_VOLT)
	//NOPMI_CHG_CHAN_ENERGY("apdo_curr", PSY_IIO_APDO_CURR)
	NOPMI_CHG_CHAN_ENERGY("nopmi_charge_ic", PSY_IIO_CHARGE_IC_TYPE)
	NOPMI_CHG_CHAN_ENERGY("ffc_disable", PSY_IIO_FFC_DISABLE)
	NOPMI_CHG_CHAN_ENERGY("quick_charge_disable", PSY_IIO_QUICK_CHARGE_DISABLE)
	NOPMI_CHG_CHAN_ENERGY("charge_afc", PSY_IIO_CHARGE_AFC)
	NOPMI_CHG_CHAN_ENERGY("pps_charge_disable", PSY_IIO_PPS_CHARGE_DISABLE)
};

enum fg_ext_iio_channels {
	FG_RESISTANCE_ID,
	FG_FASTCHARGE_MODE,
	FG_SHUTDOWN_DELAY,
	FG_SOC_DECIMAL,
	FG_SOC_RATE_DECIMAL,
	FG_BATT_ID,
};

static const char * const fg_ext_iio_chan_name[] = {
	[FG_RESISTANCE_ID]	= "resistance_id",
	[FG_FASTCHARGE_MODE]	= "fastcharge_mode",
	[FG_SHUTDOWN_DELAY]	= "shutdown_delay",
	[FG_SOC_DECIMAL]	= "soc_decimal",
	[FG_SOC_RATE_DECIMAL]	= "soc_decimal_rate",
	[FG_BATT_ID]		= "fg_batt_id",
};

static const char * const second_fg_ext_iio_chan_name[] = {
	[FG_RESISTANCE_ID]	= "second_resistance_id",
	[FG_FASTCHARGE_MODE]	= "second_fastcharge_mode",
	[FG_SHUTDOWN_DELAY]	= "second_shutdown_delay",
	[FG_SOC_DECIMAL]	= "second_soc_decimal",
	[FG_SOC_RATE_DECIMAL]	= "second_soc_decimal_rate",
	[FG_BATT_ID]		= "second_fg_batt_id",
};

static const char * const *ptr_fg_ext_iio_chan_name[] = {
	fg_ext_iio_chan_name,
	second_fg_ext_iio_chan_name,
};

enum cc_ext_iio_channels {
	CC_CHIP_ID,
};

static const char * const cc_ext_iio_chan_name[] = {
	[CC_CHIP_ID] = "cc_chip_id",
};

enum cp_ext_iio_channels {
	CP_CHARGE_PUMP_CHARGING_ENABLED,
	//CHARGE_PUMP_CHIP_ID,
	CHARGE_PUMP_SP_BUS_CURRENT,
	CHARGE_PUMP_SP_BUS_VOLTAGE,
	CHARGE_PUMP_SP_BATTERY_VOLTAGE,
	CHARGE_PUMP_UP_CHARGING_ENABLED = UPM6720_IIO_CHANNEL_OFFSET,
	//CHARGE_PUMP_LN_CHIP_ID,
	CHARGE_PUMP_UP_BUS_CURRENT,
	CHARGE_PUMP_UP_BUS_VOLTAGE,
	CHARGE_PUMP_UP_BATTERY_VOLTAGE,

};

static const char * const cp_ext_iio_chan_name[] = {
	[CP_CHARGE_PUMP_CHARGING_ENABLED] = "charging_enabled",
	//[CHARGE_PUMP_CHIP_ID] = "sc_chip_id",
	[CHARGE_PUMP_SP_BUS_CURRENT] = "sp2130_bus_current", //sp2130 ibus
	[CHARGE_PUMP_SP_BUS_VOLTAGE] = "sp2130_bus_voltage",     //sp2130 vbus
	[CHARGE_PUMP_SP_BATTERY_VOLTAGE] = "sp2130_battery_voltage", //sp2130 voltage
	[UPM6720_IIO_CHANNEL_OFFSET] = "up_charging_enabled",
	//[CHARGE_PUMP_LN_CHIP_ID] = "ln_chip_id",
	[CHARGE_PUMP_UP_BUS_CURRENT] = "up_bus_current", //upm6720 ibus
	[CHARGE_PUMP_UP_BUS_VOLTAGE] = "up_bus_voltage",     //upm6720 vbus
	[CHARGE_PUMP_UP_BATTERY_VOLTAGE] = "up_battery_voltage", //upm6720 voltage

};

enum main_chg_ext_iio_channels {
	MAIN_CHARGE_TYPE,
	MAIN_CHARGE_ENABLED,
	MAIN_CHARGE_DISABLE,
	MAIN_CHARGE_IC_TYPE,
	MAIN_CHARGE_USB_MA,
	MAIN_CHARGE_AFC,
	MAIN_CHARGE_AFC_DISABLE,
};

static const char * const main_chg_ext_iio_chan_name[] = {
	[MAIN_CHARGE_TYPE] = "charge_type",
	[MAIN_CHARGE_ENABLED] = "charge_enabled",
	[MAIN_CHARGE_DISABLE] = "charge_disable",
	[MAIN_CHARGE_IC_TYPE] = "charge_ic_type",
	[MAIN_CHARGE_USB_MA] = "charge_usb_ma",
	[MAIN_CHARGE_AFC] = "charge_afc",
	[MAIN_CHARGE_AFC_DISABLE] = "charge_afc_disable",
};

static const char * const second_main_chg_ext_iio_chan_name[] = {
	[MAIN_CHARGE_TYPE] = "second_charge_type",
	[MAIN_CHARGE_ENABLED] = "second_charge_enabled",
	[MAIN_CHARGE_DISABLE] = "second_charge_disable",
	[MAIN_CHARGE_IC_TYPE] = "second_charge_ic_type",
	[MAIN_CHARGE_USB_MA] = "second_charge_usb_ma",
	[MAIN_CHARGE_AFC] = "second_charge_afc",
	[MAIN_CHARGE_AFC_DISABLE] = "second_charge_afc_disable",
};

static const char * const *ptr_main_chg_ext_iio_chan_name[] = {
	main_chg_ext_iio_chan_name,
	second_main_chg_ext_iio_chan_name,
};

#endif
