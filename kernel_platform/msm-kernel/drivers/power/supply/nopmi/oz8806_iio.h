#ifndef __OZ8806_IIO_H
#define __OZ8806_IIO_H

#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/qti_power_supply.h>
const char oz8806_driver_name[] = "oz8806";

struct oz8806_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define OZ8806_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define OZ8806_CHAN_CURRENT(_name, _num)			\
	OZ8806_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct oz8806_iio_channels oz8806_iio_psy_channels[] = {
	OZ8806_CHAN_CURRENT("second_shutdown_delay", PSY_IIO_SHUTDOWN_DELAY)
	OZ8806_CHAN_CURRENT("second_resistance", PSY_IIO_RESISTANCE)
	OZ8806_CHAN_CURRENT("second_resistance_id", PSY_IIO_RESISTANCE_ID)
	OZ8806_CHAN_CURRENT("second_soc_decimal", PSY_IIO_SOC_DECIMAL)
	OZ8806_CHAN_CURRENT("second_soc_decimal_rate", PSY_IIO_SOC_DECIMAL_RATE)
	OZ8806_CHAN_CURRENT("second_fastcharger_mode", PSY_IIO_FASTCHARGE_MODE)
	OZ8806_CHAN_CURRENT("second_battery_type", PSY_IIO_BATTERY_TYPE)
	OZ8806_CHAN_CURRENT("second_soh", PSY_IIO_SOH)
	OZ8806_CHAN_CURRENT("second_fg_monitor_work", PSY_IIO_FG_MONITOR_WORK)
	OZ8806_CHAN_CURRENT("second_fg_batt_id", PSY_IIO_BATT_ID)
};

#endif
