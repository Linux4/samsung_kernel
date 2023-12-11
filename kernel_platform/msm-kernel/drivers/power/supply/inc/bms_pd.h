
#ifndef __BMS_PD_H
#define __BMS_PD_H

#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/power_supply.h>
#include <linux/qti_power_supply.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

#define PROBE_CNT_MAX	50

struct bms_pd {
	struct device *dev;
	struct iio_dev	*indio_dev;
	struct iio_chan_spec	*iio_chan;
	struct iio_channel	*int_iio_chans;
	int pd_active;
	int pd_cur_max;
	int pd_vol_min;
	int pd_vol_max;
	int pd_in_hard_reset;
	int typec_cc_orientation;
	int typec_mode;
	int pd_usb_suspend_supported;
	int pd_apdo_volt_max;
	int pd_apdo_curr_max;
	int pd_usb_real_type;
	int typec_accessory_mode;
};

struct bms_pd_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define BMS_PD_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define BMS_PD_CHAN_ENERGY(_name, _num)			\
	BMS_PD_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct bms_pd_iio_channels bms_pd_iio_psy_channels[] = {
	BMS_PD_CHAN_ENERGY("pd_active", PSY_IIO_PD_ACTIVE)
	BMS_PD_CHAN_ENERGY("pd_usb_suspend_supported", PSY_IIO_PD_USB_SUSPEND_SUPPORTED)
	BMS_PD_CHAN_ENERGY("pd_in_hard_reset", PSY_IIO_PD_IN_HARD_RESET)
	BMS_PD_CHAN_ENERGY("pd_current_max", PSY_IIO_PD_CURRENT_MAX)
	BMS_PD_CHAN_ENERGY("pd_voltage_min", PSY_IIO_PD_VOLTAGE_MIN)
	BMS_PD_CHAN_ENERGY("pd_voltage_max", PSY_IIO_PD_VOLTAGE_MAX)
	BMS_PD_CHAN_ENERGY("typec_cc_orientation", PSY_IIO_TYPEC_CC_ORIENTATION)
	BMS_PD_CHAN_ENERGY("typec_mode", PSY_IIO_TYPEC_MODE)
	BMS_PD_CHAN_ENERGY("pd_usb_real_type", PSY_IIO_USB_REAL_TYPE)
};
#endif /*__BMS_PD_H*/
