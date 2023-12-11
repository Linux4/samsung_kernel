
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __SP2130_IIO_H
#define __SP2130_IIO_H

#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/qti_power_supply.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct sp2130_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define SP2130_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define SP2130_CHAN_VOLT(_name, _num)			\
	SP2130_IIO_CHAN(_name, _num, IIO_VOLTAGE,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define SP2130_CHAN_CUR(_name, _num)			\
	SP2130_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define SP2130_CHAN_TEMP(_name, _num)			\
	SP2130_IIO_CHAN(_name, _num, IIO_TEMP,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define SP2130_CHAN_POW(_name, _num)			\
	SP2130_IIO_CHAN(_name, _num, IIO_POWER,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define SP2130_CHAN_ENERGY(_name, _num)			\
	SP2130_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define SP2130_CHAN_COUNT(_name, _num)			\
	SP2130_IIO_CHAN(_name, _num, IIO_COUNT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct sp2130_iio_channels sp2130_iio_psy_channels[] = {
	SP2130_CHAN_ENERGY("present", PSY_IIO_PRESENT)
	SP2130_CHAN_ENERGY("charging_enabled", PSY_IIO_CHARGING_ENABLED)
	SP2130_CHAN_ENERGY("status", PSY_IIO_STATUS)
	SP2130_CHAN_ENERGY("sp2130_battery_present", PSY_IIO_SP2130_BATTERY_PRESENT)
	SP2130_CHAN_ENERGY("sp2130_vbus_present", PSY_IIO_SP2130_VBUS_PRESENT)
	SP2130_CHAN_VOLT("sp2130_battery_voltage", PSY_IIO_SP2130_BATTERY_VOLTAGE)
	SP2130_CHAN_CUR("sp2130_battery_current", PSY_IIO_SP2130_BATTERY_CURRENT)
	SP2130_CHAN_TEMP("sp2130_battery_temperature", PSY_IIO_SP2130_BATTERY_TEMPERATURE)
	SP2130_CHAN_VOLT("sp2130_bus_voltage", PSY_IIO_SP2130_BUS_VOLTAGE)
	SP2130_CHAN_CUR("sp2130_bus_current", PSY_IIO_SP2130_BUS_CURRENT)
	SP2130_CHAN_TEMP("sp2130_bus_temperature", PSY_IIO_SP2130_BUS_TEMPERATURE)
	SP2130_CHAN_TEMP("sp2130_die_temperature", PSY_IIO_SP2130_DIE_TEMPERATURE)
	SP2130_CHAN_ENERGY("sp2130_alarm_status", PSY_IIO_SP2130_ALARM_STATUS)
	SP2130_CHAN_ENERGY("sp2130_fault_status", PSY_IIO_SP2130_FAULT_STATUS)
	SP2130_CHAN_ENERGY("sp2130_vbus_error_status", PSY_IIO_SP2130_VBUS_ERROR_STATUS)
//	SP2130_CHAN_ENERGY("sp2130_enable_adc", PSY_IIO_SP2130_ENABLE_ADC)
//	SP2130_CHAN_ENERGY("sp2130_enable_acdrv1", PSY_IIO_SP2130_ACDRV1_ENABLED)
//	SP2130_CHAN_ENERGY("sp2130_enable_otg", PSY_IIO_SP2130_ENABLE_OTG)
};

static const struct sp2130_iio_channels sp2130_slave_iio_psy_channels[] = {
	SP2130_CHAN_ENERGY("sp2130_present_slave", PSY_IIO_PRESENT)
	SP2130_CHAN_ENERGY("sp2130_charging_enabled_slave", PSY_IIO_CHARGING_ENABLED)
	SP2130_CHAN_ENERGY("sp2130_status_slave", PSY_IIO_STATUS)
	SP2130_CHAN_ENERGY("sp2130_battery_present_slave", PSY_IIO_SP2130_BATTERY_PRESENT)
	SP2130_CHAN_ENERGY("sp2130_vbus_present_slave", PSY_IIO_SP2130_VBUS_PRESENT)
	SP2130_CHAN_VOLT("sp2130_battery_voltage_slave", PSY_IIO_SP2130_BATTERY_VOLTAGE)
	SP2130_CHAN_CUR("sp2130_battery_current_slave", PSY_IIO_SP2130_BATTERY_CURRENT)
	SP2130_CHAN_TEMP("sp2130_battery_temperature_slave", PSY_IIO_SP2130_BATTERY_TEMPERATURE)
	SP2130_CHAN_VOLT("sp2130_bus_voltage_slave", PSY_IIO_SP2130_BUS_VOLTAGE)
	SP2130_CHAN_CUR("sp2130_bus_current_slave", PSY_IIO_SP2130_BUS_CURRENT)
	SP2130_CHAN_TEMP("sp2130_bus_temperature_slave", PSY_IIO_SP2130_BUS_TEMPERATURE)
	SP2130_CHAN_TEMP("sp2130_die_temperature_slave", PSY_IIO_SP2130_DIE_TEMPERATURE)
	SP2130_CHAN_ENERGY("sp2130_alarm_status_slave", PSY_IIO_SP2130_ALARM_STATUS)
	SP2130_CHAN_ENERGY("sp2130_fault_status_slave", PSY_IIO_SP2130_FAULT_STATUS)
	SP2130_CHAN_ENERGY("sp2130_vbus_error_status_slave", PSY_IIO_SP2130_VBUS_ERROR_STATUS)
	//SP2130_CHAN_ENERGY("sp2130_enable_adc_slave", PSY_IIO_SP2130_ENABLE_ADC)
	//SP2130_CHAN_ENERGY("sp2130_enable_acdrv1_slave", PSY_IIO_SP2130_ACDRV1_ENABLED)
	//SP2130_CHAN_ENERGY("sp2130_enable_otg_slave", PSY_IIO_SP2130_ENABLE_OTG)
};

int sp2130_init_iio_psy(struct sp2130 *chip);

#endif /* __SP2130_IIO_H */
