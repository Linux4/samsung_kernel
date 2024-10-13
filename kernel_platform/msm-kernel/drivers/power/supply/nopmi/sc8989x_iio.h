#ifndef __SC8989X_CHG_IIO_H
#define __SC8989X_CHG_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

const char sc89890_driver_name[] = "sc89890";

struct sc8989x_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define SC8989X_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define SC8989X_CHAN_ENERGY(_name, _num)			\
	SC8989X_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct sc8989x_iio_channels sc8989x_iio_psy_channels[] = {
	SC8989X_CHAN_ENERGY("second_charge_type", PSY_IIO_CHARGE_TYPE)
	SC8989X_CHAN_ENERGY("second_charge_enabled", PSY_IIO_CHARGE_ENABLED)
	SC8989X_CHAN_ENERGY("second_charge_disable", PSY_IIO_CHARGE_DISABLE)
	SC8989X_CHAN_ENERGY("second_charge_done", PSY_IIO_CHARGE_DONE)
	SC8989X_CHAN_ENERGY("second_charge_ic_type", PSY_IIO_CHARGE_IC_TYPE)
	SC8989X_CHAN_ENERGY("second_charge_pd_active", PSY_IIO_PD_ACTIVE)
	SC8989X_CHAN_ENERGY("second_charge_afc", PSY_IIO_CHARGE_AFC)
	SC8989X_CHAN_ENERGY("second_charge_afc_disable", PSY_IIO_CHARGE_AFC_DISABLE)
};

enum fg_ext_iio_channels {
	FG_FASTCHARGE_MODE,
	FG_MONITOR_WORK,
};

static const char * const fg_ext_iio_chan_name[] = {
	[FG_FASTCHARGE_MODE] = "fastcharge_mode",
	[FG_MONITOR_WORK] = "fg_monitor_work",
};

enum nopmi_chg_ext_iio_channels {
	NOPMI_CHG_MTBF_CUR,
	NOPMI_CHG_USB_REAL_TYPE,
	NOPMI_CHG_PD_ACTIVE,
	NOPMI_CHG_TYPEC_CC_ORIENTATION,
	NOPMI_CHG_TYPEC_MODE,
	NOPMI_CHG_INPUT_SUSPEND,
	NOPMI_CHG_FFC_DISABLE,
	NOPMI_CHG_CHARGER_AFC,
};


static const char * const nopmi_chg_ext_iio_chan_name[] = {
	[NOPMI_CHG_MTBF_CUR] = "mtbf_cur",
	[NOPMI_CHG_USB_REAL_TYPE] = "usb_real_type",
	[NOPMI_CHG_PD_ACTIVE] = "pd_active",
	[NOPMI_CHG_TYPEC_CC_ORIENTATION] = "typec_cc_orientation",
	[NOPMI_CHG_TYPEC_MODE] = "typec_mode",
	[NOPMI_CHG_INPUT_SUSPEND] = "input_suspend",
	[NOPMI_CHG_FFC_DISABLE] = "ffc_disable",
	[NOPMI_CHG_CHARGER_AFC] = "charge_afc",
};

enum ds_ext_iio_channels {
	DS_CHIP_OK,
};

static const char * const ds_ext_iio_chan_name[] = {
	[DS_CHIP_OK] = "ds_chip_ok",
};

enum main_iio_channels {
	MAIN_CHARGING_ENABLED,
	MAIN_CHARGER_TYPE,
	MAIN_CHARGING_DISABLE,
};

static const char * const main_iio_chan_name[] = {
	[MAIN_CHARGING_ENABLED] = "second_charge_enabled",
	[MAIN_CHARGER_TYPE] = "second_charge_type",
	[MAIN_CHARGING_DISABLE] = "second_charge_disable",
};

#endif
