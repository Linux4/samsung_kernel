/*
 * sec_charger.h
 * Samsung Mobile Charger Header
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

#ifndef __SEC_CHARGER_H
#define __SEC_CHARGER_H __FILE__

#if defined(CONFIG_CHARGER_MFD) || \
	defined(CONFIG_CHARGER_SPRD4SAMSUNG27X3) && \
	!defined(CONFIG_CHARGER_SM5414)
#define charger_variable charger
#define charger_variable_t struct sec_charger_info
#else
#define charger_variable (charger->client)
#define charger_variable_t struct i2c_client
#endif

#include <linux/battery/sec_charging_common.h>
#if defined(CONFIG_CHARGER_SPRD4SAMSUNG27X3)
#include <linux/battery/charger/sprd27x3_charger4samsung.h>
#endif

#if defined(CONFIG_CHARGER_DUMMY) || \
	defined(CONFIG_CHARGER_PM8917)
#include <linux/battery/charger/dummy_charger.h>
#elif defined(CONFIG_CHARGER_MAX8903)
#include <linux/battery/charger/max8903_charger.h>
#elif defined(CONFIG_CHARGER_SMB327)
#include <linux/battery/charger/smb327_charger.h>
#elif defined(CONFIG_CHARGER_SMB328)
#include <linux/battery/charger/smb328_charger.h>
#elif defined(CONFIG_CHARGER_SMB347)
#include <linux/battery/charger/smb347_charger.h>
#elif defined(CONFIG_CHARGER_SMB358)
#include <linux/battery/charger/smb358_charger.h>
#elif defined(CONFIG_CHARGER_BQ24157)
#include <linux/battery/charger/bq24157_charger.h>
#elif defined(CONFIG_CHARGER_BQ24190) || \
	defined(CONFIG_CHARGER_BQ24191)
#include <linux/battery/charger/bq24190_charger.h>
#elif defined(CONFIG_CHARGER_BQ24260)
#include <linux/battery/charger/bq24260_charger.h>
#elif defined(CONFIG_CHARGER_MAX77803)
#include <linux/battery/charger/max77803_charger.h>
#elif defined(CONFIG_CHARGER_NCP1851)
#include <linux/battery/charger/ncp1851_charger.h>
#elif defined(CONFIG_CHARGER_RT5033)
#include <linux/battery/charger/rt5033_charger.h>
#elif defined(CONFIG_CHARGER_RT9455)
#include <linux/battery/charger/rt9455_charger.h>
#elif defined(CONFIG_CHARGER_SM5414)
#include <linux/battery/charger/sm5414_charger.h>
#elif defined(CONFIG_CHARGER_SM5701)
#include <linux/battery/charger/sm5701_charger.h>
#endif

#if defined(CONFIG_CHARGER_SM5414)
extern int sec_vf_adc_check(void);
#endif

struct sec_charger_info {
	struct i2c_client		*client;
	sec_battery_platform_data_t *pdata;
	struct power_supply		psy_chg;
	struct delayed_work isr_work;

	struct workqueue_struct *wq;
	struct delayed_work delayed_work;

	int cable_type;
	int status;
	bool is_charging;

	/* HW-dedicated charger info structure
	 * used in individual charger file only
	 * (ex. dummy_charger.c)
	 */
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_CHARGER_BQ24157)
	struct sec_chg_info info;
#endif

#if defined(CONFIG_CHARGER_SM5414)
	struct sec_chg_info sm5414_chg_inf;
	bool is_fullcharged;
#endif

	/* charging current : + charging, - OTG */
	int charging_current;
	int input_current_limit;
	unsigned charging_current_max;

	/* register programming */
	int reg_addr;
	int reg_data;
	int irq_base;
	int siop_level;
};

#if defined(CONFIG_CHARGER_SPRD4SAMSUNG27X3) && \
	!defined(CONFIG_CHARGER_SM5414)
bool sec_hal_chg_init(charger_variable_t *);
bool sec_hal_chg_suspend(charger_variable_t *);
bool sec_hal_chg_resume(charger_variable_t *);
bool sec_hal_chg_get_property(charger_variable_t *,
				enum power_supply_property,
				union power_supply_propval *);
bool sec_hal_chg_set_property(charger_variable_t *,
				enum power_supply_property,
				const union power_supply_propval *);
#else
bool sec_hal_chg_init(struct i2c_client *);
bool sec_hal_chg_suspend(struct i2c_client *);
bool sec_hal_chg_resume(struct i2c_client *);
bool sec_hal_chg_shutdown(struct i2c_client *);
bool sec_hal_chg_get_property(struct i2c_client *,
		enum power_supply_property,
		union power_supply_propval *);
bool sec_hal_chg_set_property(struct i2c_client *,
		enum power_supply_property,
		const union power_supply_propval *);
#if defined(CONFIG_CHARGER_SM5414)
	int sec_get_charging_health(struct i2c_client *client);
#endif
#endif

ssize_t sec_hal_chg_show_attrs(struct device *dev,
		const ptrdiff_t offset, char *buf);

ssize_t sec_hal_chg_store_attrs(struct device *dev,
		const ptrdiff_t offset,
		const char *buf, size_t count);

ssize_t sec_chg_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);

ssize_t sec_chg_store_attrs(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count);

#define SEC_CHARGER_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sec_chg_show_attrs,			\
	.store = sec_chg_store_attrs,			\
}

/* never declare static varibles in header files
   static struct device_attribute sec_charger_attrs[] = {
   SEC_CHARGER_ATTR(reg),
   SEC_CHARGER_ATTR(data),
   SEC_CHARGER_ATTR(regs),
   };*/

enum {
	CHG_REG = 0,
	CHG_DATA,
	CHG_REGS,
};

#endif /* __SEC_CHARGER_H */
