/*
 * sec_fuelgauge.h
 * Samsung Mobile Fuel Gauge Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SEC_FUELGAUGE_H
#define __SEC_FUELGAUGE_H __FILE__

#if defined(CONFIG_FUELGAUGE_MFD)
#define fuelgauge_variable fuelgauge
#define fuelgauge_variable_t struct sec_fuelgauge_info
#else
#define fuelgauge_variable (fuelgauge->client)
#define fuelgauge_variable_t struct i2c_client
#endif

#include <linux/battery/sec_charging_common.h>

#if defined(CONFIG_FUELGAUGE_DUMMY) || \
	defined(CONFIG_FUELGAUGE_PM8917)
#include <linux/battery/fuelgauge/dummy_fuelgauge.h>
#elif defined(CONFIG_FUELGAUGE_ADC)
#include <linux/battery/fuelgauge/adc_fuelgauge.h>
#elif defined(CONFIG_FUELGAUGE_MAX17042)
#include <linux/battery/fuelgauge/max17042_fuelgauge.h>
#elif defined(CONFIG_FUELGAUGE_MAX17048)
#include <linux/battery/fuelgauge/max17048_fuelgauge.h>
#elif defined(CONFIG_FUELGAUGE_MAX17050)
#include <linux/battery/fuelgauge/max17050_fuelgauge.h>
#elif defined(CONFIG_FUELGAUGE_STC3115)
#include <linux/battery/fuelgauge/stc3115_fuelgauge.h>
#elif defined(CONFIG_FUELGAUGE_D2199)
#include <linux/battery/fuelgauge/d2199_fuelgauge.h>
#elif defined(CONFIG_FUELGAUGE_RT5033)
#include <linux/battery/fuelgauge/rt5033_fuelgauge.h>
#endif

struct sec_fuelgauge_reg_data {
	u8 reg_addr;
	u8 reg_data1;
	u8 reg_data2;
};

struct sec_fuelgauge_info {
	struct i2c_client		*client;
	sec_battery_platform_data_t *pdata;
	struct power_supply		psy_fg;
	struct delayed_work isr_work;

	int cable_type;
	bool is_charging;

	/* HW-dedicated fuel guage info structure
	 * used in individual fuel gauge file only
	 * (ex. dummy_fuelgauge.c)
	 */

#if defined(CONFIG_BATTERY_SAMSUNG)
	struct sec_fg_info	info;
#endif

	bool is_fuel_alerted;
	struct wake_lock fuel_alert_wake_lock;

	unsigned int capacity_old;	/* only for atomic calculation */
	unsigned int capacity_max;	/* only for dynamic calculation */

	bool initial_update_of_soc;
	struct mutex fg_lock;

	/* register programming */
	int reg_addr;
	u8 reg_data[2];

};

bool sec_hal_fg_init(fuelgauge_variable_t *);
bool sec_hal_fg_suspend(fuelgauge_variable_t *);
bool sec_hal_fg_resume(fuelgauge_variable_t *);
bool sec_hal_fg_fuelalert_init(fuelgauge_variable_t *, int);
bool sec_hal_fg_is_fuelalerted(fuelgauge_variable_t *);
bool sec_hal_fg_fuelalert_process(void *, bool);
bool sec_hal_fg_full_charged(fuelgauge_variable_t *);
bool sec_hal_fg_reset(fuelgauge_variable_t *);
bool sec_hal_fg_get_property(fuelgauge_variable_t *,
				enum power_supply_property,
				union power_supply_propval *);
bool sec_hal_fg_set_property(fuelgauge_variable_t *,
				enum power_supply_property,
				const union power_supply_propval *);

ssize_t sec_hal_fg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf);

ssize_t sec_hal_fg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count);

ssize_t sec_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sec_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SEC_FG_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sec_fg_show_attrs,			\
	.store = sec_fg_store_attrs,			\
}

#if defined(CONFIG_FUELGAUGE_D2199)
enum {
	FG_REG = 0,
	FG_DATA,
	FG_REGS,
};
#else
enum {
	FG_CURR_UA = 0,
	FG_REG,
	FG_DATA,
	FG_REGS,
};
#endif

#endif /* __SEC_FUELGAUGE_H */
