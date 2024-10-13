/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __SY65153_WL_CHARGER_IIO_H
#define __SY65153_WL_CHARGER_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>
#include <linux/module.h>

struct sy65153_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define SY65153_IIO_CHAN(_name, _num, _type, _mask)		\
{						\
	.datasheet_name = _name,		\
	.channel_num = _num,			\
	.type = _type,				\
	.info_mask = _mask,			\
},

#define SY65153_CHAN_ENERGY(_name, _num)			\
	SY65153_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct sy65153_iio_channels sy65153_iio_psy_channels[] = {
	SY65153_CHAN_ENERGY("charger_enable", PSY_IIO_WL_CHARGING_ENABLED)
	SY65153_CHAN_ENERGY("charger_status", PSY_IIO_CHARGER_STATUS)
	SY65153_CHAN_ENERGY("charge_type", PSY_IIO_CHARGE_TYPE)
	SY65153_CHAN_ENERGY("vbus_voltage", PSY_IIO_SC_BUS_VOLTAGE)
	SY65153_CHAN_ENERGY("vbus_present", PSY_IIO_SC_VBUS_PRESENT)
	SY65153_CHAN_ENERGY("vbus_online", PSY_IIO_ONLINE)
	SY65153_CHAN_ENERGY("wireless_fod", PSY_IIO_WL_CHARGING_FOD_UPDATE)
};

enum sy65153_iio_type {
	MAIN_CHG,
	SLAVE_CHG,
	BMS,
	WL,
	CP_MASTER,
	CP_SLAVE,
};

enum wtcharge_iio_channels {
	WTCHG_UPDATE_WORK,
	WTCHG_OTG_ENABLE,
	LOGIC_IC_RESET,
};

static const char * const wtchg_iio_chan_name[] = {
	[WTCHG_UPDATE_WORK] = "wtchg_update_work",
	[WTCHG_OTG_ENABLE] = "wt_otg_enable",
	[LOGIC_IC_RESET] = "chip_reset",
};

MODULE_LICENSE("GPL v2");

#endif /* __SY65153_WL_CHARGER_IIO_H */