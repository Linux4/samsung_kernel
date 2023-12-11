#ifndef __SD76030_CHG_IIO_H
#define __SD76030_CHG_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct sd76030_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define SD76030_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define SD76030_CHAN_ENERGY(_name, _num)			\
	SD76030_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct sd76030_iio_channels sd76030_iio_psy_channels[] = {
	SD76030_CHAN_ENERGY("bmt_charge_type", PSY_IIO_CHARGE_TYPE)
	SD76030_CHAN_ENERGY("bmt_charge_enabled", PSY_IIO_CHARGE_ENABLED)
	SD76030_CHAN_ENERGY("bmt_charge_done", PSY_IIO_CHARGE_DONE)
	SD76030_CHAN_ENERGY("bmt_charge_ic_type", PSY_IIO_CHARGE_IC_TYPE)
	SD76030_CHAN_ENERGY("bmt_charge_pd_active", PSY_IIO_PD_ACTIVE)
	SD76030_CHAN_ENERGY("charge_afc", PSY_IIO_CHARGE_AFC)
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
	NOPMI_CHG_FFC_DISABLE,
	NOPMI_CHG_CHARGER_AFC,
};


static const char * const nopmi_chg_ext_iio_chan_name[] = {
	[NOPMI_CHG_MTBF_CUR] = "bmt_mtbf_cur",
	[NOPMI_CHG_USB_REAL_TYPE] = "bmt_usb_real_type",
	[NOPMI_CHG_PD_ACTIVE] = "bmt_pd_active",
	[NOPMI_CHG_TYPEC_CC_ORIENTATION] = "bmt_typec_cc_orientation",
	[NOPMI_CHG_TYPEC_MODE] = "bmt_typec_mode",
	[NOPMI_CHG_FFC_DISABLE] = "bmt_ffc_disable",
	[NOPMI_CHG_CHARGER_AFC] = "bmt_charge_afc",
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
};

static const char * const main_iio_chan_name[] = {
	[MAIN_CHARGING_ENABLED] = "charge_enabled",
	[MAIN_CHARGER_TYPE] = "charge_type",
};

#endif
