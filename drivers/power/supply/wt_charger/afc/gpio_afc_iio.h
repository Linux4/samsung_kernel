/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __GPIO_AFC_IIO_H
#define __GPIO_AFC_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/module.h>



enum afc_iio_type {
	MAIN_CHG,
	SLAVE_CHG,
	BMS,
	WL,
	CP_MASTER,
	CP_SLAVE,
};

struct gafc_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define AFC_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define AFC_CHAN_ENERGY(_name, _num)			\
	AFC_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct gafc_iio_channels gafc_iio_psy_channels[] = {
	AFC_CHAN_ENERGY("afc_set_vol", PSY_IIO_AFC_SET_VOL)
	AFC_CHAN_ENERGY("afc_detect", PSY_IIO_AFC_DETECT)
	AFC_CHAN_ENERGY("afc_type", PSY_IIO_AFC_TYPE)
	AFC_CHAN_ENERGY("afc_detech", PSY_IIO_AFC_DETECH)
};

enum wtcharge_iio_channels {
	WTCHG_UPDATE_WORK,
	WTCHG_VBUS_VAL_NOW,
};

static const char * const wtchg_iio_chan_name[] = {
	[WTCHG_UPDATE_WORK] = "wtchg_update_work",
	[WTCHG_VBUS_VAL_NOW] = "wtchg_vbus_val_now",
};

MODULE_LICENSE("GPL v2");

#endif