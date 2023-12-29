/*
 * mtk_charger.h - Header of MTK Charger Driver
 *
 * Copyright (C) 2020 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef MTK_CHARGER_H
#define MTK_CHARGER_H
#include <linux/version.h>
#include <linux/sec_batt.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
#include <mt-plat/v1/charger_class.h>
#include <mt-plat/v1/charger_type.h>
#else
#include <mt-plat/charger_class.h>
#include <mt-plat/charger_type.h>
#endif
#if defined(CONFIG_AFC_CHARGER)
#include <mt-plat/afc_charger.h>
#endif
#include <linux/of_gpio.h>

#include "../../common/sec_charging_common.h"
#include "../../common/sec_battery.h"

#define is_usb_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_USB || \
	cable_type == SEC_BATTERY_CABLE_USB_CDP)

#define ENABLE_FLASH_HV_CTRL 1

/* define function if need */
#define ENABLE_MIVR 0

/* define IRQ function if need */
#define EN_BAT_DET_IRQ 1
#define EN_CHG1_IRQ_CHGIN 0

/* Test debug log enable */
#define EN_TEST_READ 1

#define HEALTH_DEBOUNCE_CNT 1

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)

#define chr_err(fmt, args...)					\
do {								\
		pr_notice(fmt, ##args);				\
} while (0)

#define chr_info(fmt, args...)					\
do {								\
		pr_notice_ratelimited(fmt, ##args);		\
} while (0)

#define chr_debug(fmt, args...)					\
do {								\
		pr_notice(fmt, ##args);				\
} while (0)

#define ENABLE 1
#define DISABLE 0

#define FAKE_BAT_LEVEL          50

#define REDUCE_CURRENT_STEP         25
#define MINIMUM_INPUT_CURRENT           300

ssize_t mtk_charger_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);

ssize_t mtk_charger_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define MTK_CHARGER_ATTR(_name)				\
{							                    \
	.attr = {.name = #_name, .mode = 0664},	    \
	.show = mtk_charger_show_attrs,			    \
	.store = mtk_charger_store_attrs,			\
}

enum {
	MTK_TOPOFF_TIMER_0m	= 0x0,
	MTK_TOPOFF_TIMER_30m	= 0x1,
	MTK_TOPOFF_TIMER_45m	= 0x2,
	MTK_TOPOFF_TIMER_60m	= 0x3,
};

typedef struct mtk_charger_platform_data {
	int chg_float_voltage;
	char *charger_name;
	char *fuelgauge_name;
	int slow_charging_current;
	int vbus_min_charger_voltage;
	int vbus_normal_mivr_voltage;
	int gpio_ilim;
	int gpio_chgenb;
	int max_icl;
	int ib_fcc;
	int chgenb_en;
	int chgilm_en;
	bool boosting_voltage_aicl;
} mtk_charger_platform_data_t;


struct mtk_charger_data {
	struct device *dev;
	struct power_supply *psy_chg;
	struct power_supply_desc psy_chg_desc;
	struct power_supply *psy_otg;
	struct power_supply_desc psy_otg_desc;
	struct power_supply *psy_bat;
	struct power_supply *psy_fg;
#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	struct power_supply *psy_bc12;
	struct power_supply_desc psy_bc12_desc;
#endif
	struct charger_device *chg_dev;

	mtk_charger_platform_data_t *pdata;
	int dev_id;
	int input_current;
	int charging_current;
	int topoff_current;
	int cable_type;
	bool is_charging;
	int buck_state;
	unsigned int charge_mode;
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	unsigned int f_mode;
#endif

	struct mutex charger_mutex;

	bool ovp;
	bool otg_on;

	int unhealth_cnt;
	int status;
	int health;

	int irq_ivr_enabled;
	int ivr_on;
	bool slow_charging;

	u8 read_reg;
};
#endif /*MTK_CHARGER_H*/
