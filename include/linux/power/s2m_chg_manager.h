/*
 * s2m_chg_manager.h
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __S2M_BATTERY_H
#define __S2M_BATTERY_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/wakelock.h>
#include <linux/alarmtimer.h>

#if defined(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#include <linux/ifconn/ifconn_manager.h>
#include <linux/muic/muic.h>
#endif

#define FAKE_BAT_LEVEL		50
#define DEFAULT_ALARM_INTERVAL	10
#define SLEEP_ALARM_INTERVAL	30
#define NORMAL_CURR		10

static char *bat_status_str[] = {
	"Unknown",
	"Charging",
	"Discharging",
	"Not-charging",
	"Full"
};

static char *health_str[] = {
	"Unknown",
	"Good",
	"Overheat",
	"Dead",
	"OverVoltage",
	"UnspecFailure",
	"Cold",
	"WatchdogTimerExpire",
	"SafetyTimerExpire",
	"UnderVoltage",
	"ColdLimit",
	"OverheatLimit",
};

static enum power_supply_property s2m_chg_manager_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_SOH,
};

static enum power_supply_property s2m_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *s2m_supplied_to[] = {
	"s2m-battery",
};

enum sec_battery_rp_curr {
	RP_CURRENT1 = 500,
	RP_CURRENT2 = 1500,
	RP_CURRENT3 = 3000,
};

enum s2m_battery_voltage_mode {
	S2M_BATTERY_VOLTAGE_AVERAGE = 0,
	S2M_BATTERY_VOLTAGE_OCV,
};

enum s2m_battery_current_mode {
	S2M_BATTERY_CURRENT_UA = 0,
	S2M_BATTERY_CURRENT_MA,
};

enum s2m_battery_charger_mode {
	S2M_BAT_CHG_MODE_CHARGING = 0,
	S2M_BAT_CHG_MODE_CHARGING_OFF,
	S2M_BAT_CHG_MODE_BUCK_OFF,
};

enum s2m_battery_factory_mode {
	S2M_BAT_FAC_MODE_VBAT = 0,
	S2M_BAT_FAC_MODE_VBUS,
	S2M_BAT_FAC_MODE_NORMAL,
};

struct s2m_charging_current {
#ifdef CONFIG_OF
	unsigned int input_current_limit;
	unsigned int fast_charging_current;
	unsigned int full_check_current;
#else
	int input_current_limit;
	int fast_charging_current;
	int full_check_current;
#endif
};

#define s2m_charging_current_t struct s2m_charging_current

typedef struct s2m_chg_manager_platform_data {
	s2m_charging_current_t *charging_current;

	char *charger_name;
	char *fuelgauge_name;

	int default_limit_current;
	int max_input_current;
	int max_charging_current;

	int chg_float_voltage;
	/* full check */
	unsigned int full_check_count;
	unsigned int chg_recharge_vcell;
	unsigned int chg_full_vcell;

	/* Initial maximum raw SOC */
	unsigned int max_rawsoc;
	unsigned int max_rawsoc_offset;

	/* battery */
	char *vendor;
	int technology;

#if defined(CONFIG_SMALL_CHARGER)
	char *smallcharger_name;
	int small_input_current;
	int small_charging_current;
	int small_limit_current;
#endif

	int temp_high;
	int temp_high_recovery;
	int temp_high_limit;
	int temp_high_limit_recovery;
	int temp_low;
	int temp_low_recovery;
	int temp_low_limit;
	int temp_low_limit_recovery;
	int temp_limit_float_voltage;

#if defined(CONFIG_USE_CCIC)
	int pdo_max_input_vol;
	int pdo_max_chg_power;
#endif
} s2m_chg_manager_platform_data_t;

struct s2m_chg_manager_info {
	s2m_chg_manager_platform_data_t *pdata;

	struct device *dev;
	struct power_supply *psy_battery;
	struct power_supply_desc psy_battery_desc;
	struct power_supply *psy_usb;
	struct power_supply_desc psy_usb_desc;
	struct power_supply *psy_ac;
	struct power_supply_desc psy_ac_desc;

	struct mutex iolock;
	struct mutex ifconn_lock;

	struct wake_lock monitor_wake_lock;
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work monitor_work;
	struct delayed_work soc_control;
	struct wake_lock vbus_wake_lock;

	struct alarm monitor_alarm;
	unsigned int monitor_alarm_interval;

	int default_limit_current;
	int input_current;
	int max_input_current;
	int charging_current;
	int max_charging_current;
	int topoff_current;
	int cable_type;
	unsigned int charging_mode;

	int vchg_current;
	int vchg_voltage;
	int full_check_cnt;

	/* charging */
	bool is_recharging;

	bool battery_valid;
	int status;
	int health;

	int voltage_now;
	int voltage_avg;
	int voltage_ocv;

	unsigned int capacity;
	unsigned int max_rawsoc;
	unsigned int max_rawsoc_offset;

	int soh;		/* State of Health (%) */

	int current_now;	/* current (mA) */
	int current_avg;	/* average current (mA) */
	int current_max;	/* input current limit (mA) */
	int current_chg;	/* charge current limit (mA) */

#if defined(CONFIG_IFCONN_NOTIFIER)
	struct notifier_block ifconn_nb;
	struct notifier_block ifconn_cc_nb;
#endif

#if defined(CONFIG_SMALL_CHARGER)
	int small_input;	/* input current limit (mA) */
	int small_chg;		/* charge current limit (mA) */
	int small_input_flag;
	int small_limit_current;
#endif

	/* temperature check */
#if defined(CONFIG_BAT_TEMP)
	bool is_temp_control;
#endif
	int temperature;    	/* battery temperature(0.1 Celsius)*/
	int temp_high;
	int temp_high_recovery;
	int temp_high_limit;
	int temp_high_limit_recovery;
	int temp_low;
	int temp_low_recovery;
	int temp_low_limit;
	int temp_low_limit_recovery;

#if defined(CONFIG_USE_CCIC)
	struct delayed_work select_pdo_work;
	int pdo_max_input_vol;
	int pdo_max_chg_power;

	int pdo_sel_num;
	int pdo_sel_vol;
	int pdo_sel_cur;

	int pd_input_current;
	bool pd_attach;
	bool rp_attach;
	int rp_input_current;
	int rp_charging_current;
#endif
	bool monitor_trigger;
};
#endif /* __S2M_BATTERY_H */
