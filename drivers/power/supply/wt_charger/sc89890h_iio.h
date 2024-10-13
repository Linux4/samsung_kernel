#ifndef __sc8989x_IIO_H
#define __sc8989x_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>
#include <linux/module.h>

struct sc8989x_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define sc8989x_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define sc8989x_CHAN_ENERGY(_name, _num)			\
	sc8989x_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct sc8989x_iio_channels sc8989x_iio_psy_channels[] = {
	sc8989x_CHAN_ENERGY("charge_done", PSY_IIO_CHARGE_DONE)
	sc8989x_CHAN_ENERGY("main_chager_hz", PSY_IIO_MAIN_CHAGER_HZ)
	sc8989x_CHAN_ENERGY("main_input_current_settled", PSY_IIO_MAIN_INPUT_CURRENT_SETTLED)
	sc8989x_CHAN_ENERGY("main_input_voltage_settled", PSY_IIO_MAIN_INPUT_VOLTAGE_SETTLED)
	sc8989x_CHAN_ENERGY("main_charge_current", PSY_IIO_MAIN_CHAGER_CURRENT)
	sc8989x_CHAN_ENERGY("charger_enable", PSY_IIO_CHARGING_ENABLED)
	sc8989x_CHAN_ENERGY("otg_enable", PSY_IIO_OTG_ENABLE)
	sc8989x_CHAN_ENERGY("main_charger_term", PSY_IIO_MAIN_CHAGER_TERM)
	sc8989x_CHAN_ENERGY("batt_voltage_term", PSY_IIO_BATTERY_VOLTAGE_TERM)
	sc8989x_CHAN_ENERGY("charger_status", PSY_IIO_CHARGER_STATUS)
	sc8989x_CHAN_ENERGY("charger_type", PSY_IIO_CHARGE_TYPE)
	sc8989x_CHAN_ENERGY("vbus_voltage", PSY_IIO_SC_BUS_VOLTAGE)
	sc8989x_CHAN_ENERGY("apsd_rerun", PSY_IIO_APSD_RERUN)
	sc8989x_CHAN_ENERGY("main_charge_vbus_online", PSY_IIO_MAIN_CHARGE_VBUS_ONLINE)
	sc8989x_CHAN_ENERGY("set_ship_mode", PSY_IIO_SET_SHIP_MODE)
	sc8989x_CHAN_ENERGY("set_dp_dm",PSY_IIO_DP_DM)
	sc8989x_CHAN_ENERGY("main_feed_wdt",PSY_IIO_MAIN_CHARGE_FEED_WDT)
};

enum sc8989x_iio_type {
	MAIN_CHG,
	SLAVE_CHG,
	BMS,
	WL,
	CP_MASTER,
	CP_SLAVE,
};

enum wtcharge_iio_channels {
	WTCHG_UPDATE_WORK,
};

static const char * const wtchg_iio_chan_name[] = {
	[WTCHG_UPDATE_WORK] = "wtchg_update_work",
};

enum pmic_iio_channels {
	VBUS_VOLTAGE,
};

static const char * const pmic_iio_chan_name[] = {
	[VBUS_VOLTAGE] = "vbus_pwr",
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