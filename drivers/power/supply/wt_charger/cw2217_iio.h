#ifndef __CW2217_IIO_H
#define __CW2217_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>
#include <linux/module.h>

struct cw2217_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

enum cw221x_iio_type {
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
};


static const char * const wtchg_iio_chan_name[] = {
	[WTCHG_UPDATE_WORK] = "wtchg_update_work",
};


#define CW2217_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define CW2217_CHAN_ENERGY(_name, _num)			\
	CW2217_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))
		
static const struct cw2217_iio_channels cw2217_iio_psy_channels[] = {
	CW2217_CHAN_ENERGY("present", PSY_IIO_PRESENT)
	CW2217_CHAN_ENERGY("status", PSY_IIO_STATUS)
	CW2217_CHAN_ENERGY("capacity", PSY_IIO_CAPACITY)
	CW2217_CHAN_ENERGY("current_now", PSY_IIO_CURRENT_NOW)
	CW2217_CHAN_ENERGY("voltage_now", PSY_IIO_VOLTAGE_NOW)
	//P86801AA1-3622,gudi.wt,20230705,battery protect func
	CW2217_CHAN_ENERGY("qg_battery_cycle", PSY_IIO_CYCLE_COUNT)
	CW2217_CHAN_ENERGY("voltage_max", PSY_IIO_VOLTAGE_MAX)
	CW2217_CHAN_ENERGY("charge_full", PSY_IIO_CHARGE_FULL)
	CW2217_CHAN_ENERGY("resistance_id", PSY_IIO_RESISTANCE_ID)
	CW2217_CHAN_ENERGY("temp", PSY_IIO_TEMP)
	CW2217_CHAN_ENERGY("cycle_count", PSY_IIO_CHARGE_COUNTER)
	CW2217_CHAN_ENERGY("charge_full_design", PSY_IIO_CHARGE_FULL_DESIGN)
	CW2217_CHAN_ENERGY("time_to_full_now", PSY_IIO_TIME_TO_FULL_NOW)
	CW2217_CHAN_ENERGY("therm_curr", PSY_IIO_MAIN_FCC_MAX)
	CW2217_CHAN_ENERGY("chip_ok", PSY_IIO_BMS_CHIP_OK)
	CW2217_CHAN_ENERGY("battery_auth", PSY_IIO_BATTERY_AUTH)
	CW2217_CHAN_ENERGY("soc_decimal", PSY_IIO_BMS_SOC_DECIMAL)
	CW2217_CHAN_ENERGY("soc_decimal_rate", PSY_IIO_BMS_SOC_DECIMAL_RATE)
	CW2217_CHAN_ENERGY("battery_id", PSY_IIO_BATTERY_ID)
};

MODULE_LICENSE("GPL v2");

#endif
