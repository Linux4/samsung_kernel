/*
 * s2mu00x_battery.c - Example battery driver for S2MU00x series
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
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
 */

#include <linux/version.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/power/s2mu00x_battery.h>
#include <linux/alarmtimer.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/s2mu004-muic-notifier.h>
#include <linux/muic/muic.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#include <linux/ifconn/ifconn_manager.h>
#include <linux/muic/muic_core.h>
#endif

#define FAKE_BAT_LEVEL	50
#define DEFAULT_ALARM_INTERVAL	10
#define SLEEP_ALARM_INTERVAL	30

#define CHARGER_S2MU106

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
};

static enum power_supply_property s2mu00x_battery_props[] = {
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
	POWER_SUPPLY_PROP_CALIBRATE,
};

static enum power_supply_property s2mu00x_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

typedef struct s2mu00x_battery_platform_data {
	s2mu00x_charging_current_t *charging_current;
	char *charger_name;
#if defined(CONFIG_SMALL_CHARGER)
	char *smallcharger_name;
#endif
	char *fuelgauge_name;

	int max_input_current;
	int max_charging_current;

#if defined(CONFIG_SMALL_CHARGER)
	int small_input_current;
	int small_charging_current;
#endif

#if defined(CONFIG_USE_CCIC)
	int pdo_max_input_vol;
	int pdo_max_chg_power;
#endif

	int temp_high;
	int temp_high_recovery;
	int temp_low;
	int temp_low_recovery;

	/* full check */
	unsigned int full_check_count;
	unsigned int chg_recharge_vcell;
	unsigned int chg_full_vcell;

	/* Initial maximum raw SOC */
	unsigned int max_rawsoc;

	/* battery */
	char *vendor;
	int technology;
	int battery_type;
	void *battery_data;
} s2mu00x_battery_platform_data_t;

struct s2mu00x_battery_info {
	struct device *dev;
	s2mu00x_battery_platform_data_t *pdata;

	struct power_supply *psy_battery;
	struct power_supply_desc psy_battery_desc;
	struct power_supply *psy_usb;
	struct power_supply_desc psy_usb_desc;
	struct power_supply *psy_ac;
	struct power_supply_desc psy_ac_desc;

	struct mutex iolock;

	struct wake_lock monitor_wake_lock;
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work monitor_work;
	struct delayed_work soc_control;
	struct wake_lock vbus_wake_lock;

	struct alarm monitor_alarm;
	unsigned int monitor_alarm_interval;

	int input_current;
	int max_input_current;
	int charging_current;
	int max_charging_current;

#if defined(CONFIG_USE_CCIC)
	int pdo_max_input_vol;
	int pdo_max_chg_power;

	int pdo_sel_num;
	int pdo_sel_vol;
	int pdo_sel_cur;
#endif

	int topoff_current;
	int cable_type;
	unsigned int charging_mode;

#if defined(CONFIG_IFCONN_NOTIFIER)
	struct notifier_block ifconn_nb;
#elif defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block batt_nb;
#endif

	int full_check_cnt;

	/* charging */
	bool is_recharging;

	bool is_factory;	/* factory image support mode */

	bool battery_valid;
	int status;
	int health;

	int voltage_now;
	int voltage_avg;
	int voltage_ocv;

	unsigned int capacity;
	unsigned int max_rawsoc;

	int current_now;	/* current (mA) */
	int current_avg;	/* average current (mA) */
	int current_max;	/* input current limit (mA) */
	int current_chg;	/* charge current limit (mA */

#if defined(CONFIG_SMALL_CHARGER)
	int small_input;	/* input current limit (mA) */
	int small_chg;	/* charge current limit (mA */
#endif

#if defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block cable_check;
#endif

	/* temperature check */
	int temperature;    /* battery temperature(0.1 Celsius)*/
	int temp_high;
	int temp_high_recovery;
	int temp_low;
	int temp_low_recovery;
	int vchg_voltage;
};

static char *s2mu00x_supplied_to[] = {
	"s2mu00x-battery",
};
#if defined(CHARGER_S2MU106)
static void get_charging_current(struct s2mu00x_battery_info *battery,
		int *input_current, int *charging_current)
{
	int max_input_current = battery->max_input_current;
	int max_charging_current = battery->max_charging_current;

	if (*input_current > max_input_current) {
		*input_current = max_input_current;
		pr_info("%s: limit input current. (%d)\n", __func__, *input_current);
	}
	if (*charging_current > max_charging_current) {
		*charging_current = max_charging_current;
		pr_info("%s: limit charging current. (%d)\n", __func__, *charging_current);
	}
}

static int set_charging_current(struct s2mu00x_battery_info *battery)
{
	union power_supply_propval value;
	int input_current =
			battery->pdata->charging_current[battery->cable_type].input_current_limit,
		charging_current =
			battery->pdata->charging_current[battery->cable_type].fast_charging_current,
		topoff_current =
			battery->pdata->charging_current[battery->cable_type].full_check_current;
	struct power_supply *psy;
	int ret;

	pr_info("%s: cable_type(%d), current(%d, %d, %d)\n", __func__,
			battery->cable_type, input_current, charging_current, topoff_current);
	mutex_lock(&battery->iolock);

	/*Limit input & charging current according to the max current*/
	get_charging_current(battery, &input_current, &charging_current);\

	/* set input current limit */
	if (battery->input_current != input_current) {
		value.intval = input_current;

		psy = power_supply_get_by_name(battery->pdata->charger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		battery->input_current = input_current;
	}
	/* set fast charging current */
	if (battery->charging_current != charging_current) {
		value.intval = charging_current;

		psy = power_supply_get_by_name(battery->pdata->charger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		battery->charging_current = charging_current;
	}
	/* set topoff current */
	if (battery->topoff_current != topoff_current) {
		value.intval = topoff_current;

		psy = power_supply_get_by_name(battery->pdata->charger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CURRENT_FULL, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		battery->topoff_current = topoff_current;
	}
#if defined(CONFIG_SMALL_CHARGER)
	if (battery->cable_type == POWER_SUPPLY_TYPE_PREPARE_TA ||
		battery->cable_type == POWER_SUPPLY_TYPE_USB_PD) {
		value.intval = battery->pdata->small_input_current;
		psy = power_supply_get_by_name(battery->pdata->smallcharger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		value.intval = battery->pdata->small_charging_current;
		psy = power_supply_get_by_name(battery->pdata->smallcharger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
	}
#endif
	mutex_unlock(&battery->iolock);
	return 0;
}


/*
 * set_charger_mode(): charger_mode must have one of following values.
 * 1. S2MU00X_BAT_CHG_MODE_CHARGING
 *	Charger on.
 *	Supply power to system & battery both.
 * 2. S2MU00X_BAT_CHG_MODE_CHARGING_OFF
 *	Buck mode. Stop battery charging.
 *	But charger supplies system power.
 * 3. S2MU00X_BAT_CHG_MODE_BUCK_OFF
 *	All off. Charger is completely off.
 *	Do not supply power to battery & system both.
 */

static int set_charger_mode(
		struct s2mu00x_battery_info *battery,
		int charger_mode)
{
	union power_supply_propval val;
	struct power_supply *psy;
		int ret;

	if (charger_mode != S2MU00X_BAT_CHG_MODE_CHARGING)
		battery->full_check_cnt = 0;

	val.intval = charger_mode;

	psy = power_supply_get_by_name(battery->pdata->charger_name);
	if (!psy)
		return -EINVAL;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

#if defined(CONFIG_SMALL_CHARGER)
	if (charger_mode == S2MU00X_BAT_CHG_MODE_CHARGING &&
			(battery->cable_type == POWER_SUPPLY_TYPE_PREPARE_TA ||
			 battery->cable_type == POWER_SUPPLY_TYPE_USB_PD)) {
		psy = power_supply_get_by_name(battery->pdata->smallcharger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
	} else if (charger_mode != S2MU00X_BAT_CHG_MODE_CHARGING) {
		psy = power_supply_get_by_name(battery->pdata->smallcharger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
	}
#endif

	return 0;
}

#endif
static int set_battery_status(struct s2mu00x_battery_info *battery,
		int status)
{
	union power_supply_propval value;
#if defined(CHARGER_S2MU106)
	struct power_supply *psy;
	int ret;
#endif
	pr_info("%s: current status = %d, new status = %d\n", __func__, battery->status, status);
#if 0
	if (battery->status == status)
		return 0;
#endif

#if defined(CHARGER_S2MU106)
	switch (status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		/* notify charger cable type */
		value.intval = battery->cable_type;

		psy = power_supply_get_by_name(battery->pdata->charger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

#if defined(CONFIG_SMALL_CHARGER)
		if (battery->cable_type == POWER_SUPPLY_TYPE_PREPARE_TA ||
			battery->cable_type == POWER_SUPPLY_TYPE_USB_PD) {
			psy = power_supply_get_by_name(battery->pdata->smallcharger_name);
			if (!psy)
				return -EINVAL;
			ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
		}
#endif

		/* charger on */
		set_charger_mode(battery, S2MU00X_BAT_CHG_MODE_CHARGING);
		set_charging_current(battery);
		break;

	case POWER_SUPPLY_STATUS_DISCHARGING:
		set_charging_current(battery);

		/* notify charger cable type */
		value.intval = battery->cable_type;

#if defined(CONFIG_SMALL_CHARGER)
		if (battery->cable_type == POWER_SUPPLY_TYPE_PREPARE_TA ||
			battery->cable_type == POWER_SUPPLY_TYPE_USB_PD) {
			psy = power_supply_get_by_name(battery->pdata->smallcharger_name);
			if (!psy)
				return -EINVAL;
			ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
		}
#endif

		psy = power_supply_get_by_name(battery->pdata->charger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		set_charger_mode(battery, S2MU00X_BAT_CHG_MODE_CHARGING_OFF);
		break;

	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		set_charger_mode(battery, S2MU00X_BAT_CHG_MODE_BUCK_OFF);

		/* to recover charger configuration when heath is recovered */
		battery->input_current = 0;
		battery->charging_current = 0;
		battery->topoff_current = 0;
		break;

	case POWER_SUPPLY_STATUS_FULL:
		set_charger_mode(battery, S2MU00X_BAT_CHG_MODE_CHARGING_OFF);
		break;
	}
#endif
	/* battery status update */
	battery->status = status;
	value.intval = battery->status;
#if defined(CHARGER_S2MU106)
	psy = power_supply_get_by_name(battery->pdata->charger_name);
	if (!psy)
		return -EINVAL;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
#endif
	return 0;
}

static void set_bat_status_by_cable(struct s2mu00x_battery_info *battery)
{
	if (battery->is_factory) {
		pr_info("%s: factory image support mode. Skip!\n", __func__);
		return;
	}

#if defined(CHARGER_S2MU106)
	if (battery->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
		battery->cable_type == POWER_SUPPLY_TYPE_UNKNOWN ||
		battery->cable_type == POWER_SUPPLY_TYPE_OTG) {
		battery->is_recharging = false;
#if defined(CONFIG_USE_CCIC)
		battery->pdo_sel_num = 0;
		battery->pdo_sel_vol = 0;
		battery->pdo_sel_cur = 0;
#endif
		set_battery_status(battery, POWER_SUPPLY_STATUS_DISCHARGING);
		return;
	}
	if (battery->status != POWER_SUPPLY_STATUS_FULL) {
		set_battery_status(battery, POWER_SUPPLY_STATUS_CHARGING);
		return;
	}

	dev_info(battery->dev, "%s: abnormal cable_type or status", __func__);
#endif
}

static int s2mu00x_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct s2mu00x_battery_info *battery =  power_supply_get_drvdata(psy);
	int ret = 0;

	dev_dbg(battery->dev, "prop: %d\n", psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = battery->status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = battery->health;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = battery->cable_type;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = battery->battery_valid;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (!battery->battery_valid)
			val->intval = FAKE_BAT_LEVEL;
		else
			val->intval = battery->voltage_now * 1000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = battery->voltage_avg * 1000;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = battery->temperature;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = battery->charging_mode;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (!battery->battery_valid)
			val->intval = FAKE_BAT_LEVEL;
		else {
			if (battery->status == POWER_SUPPLY_STATUS_FULL)
				val->intval = 100;
			else
				val->intval = battery->capacity;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = battery->current_now;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = battery->current_avg;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = battery->full_check_cnt + 1;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = 100;
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		val->intval = battery->is_factory;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
		val->intval = battery->vchg_voltage;
		break;
	default:
		ret = -ENODATA;
	}
	return ret;
}

static int s2mu00x_battery_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mu00x_battery_info *battery = power_supply_get_drvdata(psy);
	int ret = 0;
	union power_supply_propval value;
	struct power_supply *psy_dest;


	dev_dbg(battery->dev, "prop: %d\n", psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		set_battery_status(battery, val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		battery->health = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		battery->cable_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (val->intval == S2MU00X_BAT_FAC_MODE_VBAT) {
			battery->is_factory = true;
			pr_info("%s: VBat factory image support mode\n", __func__);

			value.intval = S2MU00X_BAT_FAC_MODE_VBAT;
			psy_dest = power_supply_get_by_name(battery->pdata->charger_name);
			if (!psy_dest)
				return -EINVAL;
			ret = power_supply_set_property(psy_dest, POWER_SUPPLY_PROP_CALIBRATE, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
		} else if (val->intval == S2MU00X_BAT_FAC_MODE_VBUS) {
			battery->is_factory = true;
			pr_info("%s: VBUS charging factory image support mode\n", __func__);

			value.intval = S2MU00X_BAT_FAC_MODE_VBUS;
			psy_dest = power_supply_get_by_name(battery->pdata->charger_name);
			if (!psy_dest)
				return -EINVAL;
			ret = power_supply_set_property(psy_dest, POWER_SUPPLY_PROP_CALIBRATE, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
		} else {
			battery->is_factory = false;
			pr_info("%s: disable factory image support mode\n", __func__);

			/* Reset current setting for recovering */
			battery->input_current = 0;
			battery->charging_current = 0;
			battery->topoff_current = 0;

			set_bat_status_by_cable(battery);
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int s2mu00x_battery_property_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	int ret;
	switch (psp) {
		case POWER_SUPPLY_PROP_CALIBRATE:
			ret = 1;
			break;
		default:
			ret = 0;
	}

	return ret;
}

static int s2mu00x_usb_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu00x_battery_info *battery =  power_supply_get_drvdata(psy);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	switch (battery->cable_type) {
	case POWER_SUPPLY_TYPE_USB:
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB_CDP:
	case POWER_SUPPLY_TYPE_USB_ACA:
		val->intval = 1;
		break;
	default:
		val->intval = 0;
		break;
	}

	return 0;
}

/*
 * AC charger operations
 */
static int s2mu00x_ac_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu00x_battery_info *battery =  power_supply_get_drvdata(psy);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	switch (battery->cable_type) {
	case POWER_SUPPLY_TYPE_MAINS:
	case POWER_SUPPLY_TYPE_UNKNOWN:
	case POWER_SUPPLY_TYPE_PREPARE_TA:
	case POWER_SUPPLY_TYPE_HV_MAINS:
	case POWER_SUPPLY_TYPE_USB_PD:
		val->intval = 1;
		break;
	default:
		val->intval = 0;
		break;
	}

	return 0;
}

#if defined(CONFIG_MUIC_NOTIFIER) || defined(CONFIG_IFCONN_NOTIFIER)
static int s2mu00x_bat_cable_check(struct s2mu00x_battery_info *battery,
		muic_attached_dev_t attached_dev)
{
	int current_cable_type = -1;

	pr_info("[%s]ATTACHED(%d)\n", __func__, attached_dev);

	switch (attached_dev) {
	case ATTACHED_DEV_SMARTDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_HMT_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_OTG;
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_HV_MAINS;
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
#if defined(CONFIG_IFCONN_NOTIFIER)
	case ATTACHED_DEV_UNDEFINED_RANGE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_PREPARE_TA;
		break;
	case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_HV_MAINS;
		break;
	case ATTACHED_DEV_VZW_INCOMPATIBLE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
#endif
	default:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		pr_err("%s: invalid type for charger:%d\n",
				__func__, attached_dev);
	}

	return current_cable_type;
}
#endif

#if defined(CONFIG_MUIC_NOTIFIER)
static int s2mu00x_battery_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
	const char *cmd;
	int cable_type;
	union power_supply_propval value;
	struct s2mu00x_battery_info *battery =
		container_of(nb, struct s2mu00x_battery_info, batt_nb);
	struct power_supply *psy;
	int ret;

	if (attached_dev == ATTACHED_DEV_MHL_MUIC)
		return 0;

	switch (action) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
		cmd = "DETACH";
		cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
		cmd = "ATTACH";
		cable_type = s2mu00x_bat_cable_check(battery, attached_dev);
		break;
	default:
		cmd = "ERROR";
		cable_type = -1;
		break;
	}

	pr_info("%s: current_cable(%d) former cable_type(%d) battery_valid(%d)\n",
			__func__, cable_type, battery->cable_type,
			battery->battery_valid);
	if (battery->battery_valid == false)
		pr_info("%s: Battery is disconnected\n", __func__);

	battery->cable_type = cable_type;
	pr_info("%s: CMD=%s, attached_dev=%d battery_cable=%d\n",
			__func__, cmd, attached_dev, battery->cable_type);

#if defined(CHARGER_S2MU106)
	if (attached_dev == ATTACHED_DEV_OTG_MUIC) {
		if (!strcmp(cmd, "ATTACH")) {
			value.intval = true;

			psy = power_supply_get_by_name(battery->pdata->charger_name);
			if (!psy)
				return -EINVAL;
			ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);

			pr_info("%s: OTG cable attached\n", __func__);
		} else {
			value.intval = false;

			psy = power_supply_get_by_name(battery->pdata->charger_name);
			if (!psy)
				return -EINVAL;
			ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);

			pr_info("%s: OTG cable detached\n", __func__);
		}
	}

	if (battery->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
			battery->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) {
		battery->is_recharging = false;
		set_battery_status(battery, POWER_SUPPLY_STATUS_DISCHARGING);
	} else {
		if (battery->cable_type == POWER_SUPPLY_TYPE_OTG) {
			set_battery_status(battery, POWER_SUPPLY_STATUS_DISCHARGING);
		} else {
			if (battery->status != POWER_SUPPLY_STATUS_FULL)
				set_battery_status(battery, POWER_SUPPLY_STATUS_CHARGING);
		}
	}
#endif
	pr_info(
			"%s: Status(%s), Health(%s), Cable(%d), Recharging(%d))"
			"\n", __func__,
			bat_status_str[battery->status],
			health_str[battery->health],
			battery->cable_type,
			battery->is_recharging
		  );

	power_supply_changed(battery->psy_battery);
	alarm_cancel(&battery->monitor_alarm);
	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	return 0;
}
#endif
#if defined(CONFIG_IFCONN_NOTIFIER)
#if defined(CONFIG_USE_CCIC)
static int s2mu00x_bat_set_pdo(struct s2mu00x_battery_info *battery,
		ifconn_pd_sink_status_t *pdo_data)
{
	int ret = -1;
	int pdo_num = battery->pdo_sel_num;

	if (pdo_num > pdo_data->available_pdo_num + 1 || pdo_num < 1) {
		dev_info(battery->dev, "%s: wrong pdo number. Stop pdo select.\n",
				__func__);
		return ret;
	}

	ret = ifconn_notifier_notify(IFCONN_NOTIFY_BATTERY,
			IFCONN_NOTIFY_MANAGER,
			IFCONN_NOTIFY_ID_SELECT_PDO,
			pdo_num,
			IFCONN_NOTIFY_PARAM_DATA,
			NULL);
	if (ret < 0) {
		pr_err("%s: Fail to send noti\n", __func__);
		return ret;
	}

	ret = POWER_SUPPLY_TYPE_PREPARE_TA;

	return ret;
}

static int s2mu00x_bat_pdo_check(struct s2mu00x_battery_info *battery,
		struct ifconn_notifier_template *pdo_info)
{
	int current_cable = -1;
	int i;
	int pd_input_current_limit =
		battery->pdata->charging_current[POWER_SUPPLY_TYPE_USB_PD].input_current_limit;
	ifconn_pd_sink_status_t *pdo_data = pdo_info->data;

	dev_info(battery->dev, "%s: available_pdo_num:%d, selected_pdo_num:%d,"
		"current_pdo_num:%d\n",
		__func__, pdo_data->available_pdo_num, pdo_data->selected_pdo_num,
		pdo_data->current_pdo_num);

	dev_info(battery->dev, "%s: pdo_max_input_vol:%d, pdo_max_chg_power:%d, "
			"pdo_sel_num:%d\n",
			__func__, battery->pdo_max_input_vol, battery->pdo_max_chg_power,
			battery->pdo_sel_num);

	if (pdo_data->available_pdo_num < 0)
		return current_cable;

	if (battery->pdo_sel_num == pdo_data->selected_pdo_num) {
		dev_info(battery->dev, "%s: Already done. Finish pdo check.\n",
				__func__);
		current_cable = POWER_SUPPLY_TYPE_USB_PD;
		goto end_pdo_check;
	}

	for (i = 1; i <= pdo_data->available_pdo_num + 1; i++) {
		dev_info(battery->dev, "%s: pdo_num:%d, max_voltage:%d, max_current:%d\n",
				__func__, i, pdo_data->power_list[i].max_voltage,
				pdo_data->power_list[i].max_current);

		if (pdo_data->power_list[i].max_voltage > battery->pdo_max_input_vol)
			continue;

		pd_input_current_limit = (pd_input_current_limit > pdo_data->power_list[i].max_current)?
			pdo_data->power_list[i].max_current:pd_input_current_limit;

		if (((pdo_data->power_list[i].max_voltage/1000) * pd_input_current_limit) <=
				battery->pdo_max_chg_power) {
				battery->pdo_sel_num = i;
				battery->pdo_sel_vol = pdo_data->power_list[i].max_voltage;
				battery->pdo_sel_cur = pdo_data->power_list[i].max_current;
				dev_info(battery->dev, "%s: new pdo_sel_num:%d\n",
						__func__, battery->pdo_sel_num);
		}
	}

	if (battery->pdo_sel_num == 0) {
		dev_info(battery->dev, "%s: There is no proper pdo. Do normal TA setting\n", __func__);
		current_cable = POWER_SUPPLY_TYPE_MAINS;
	} else
		current_cable = s2mu00x_bat_set_pdo(battery, pdo_data);

end_pdo_check:
	return current_cable;
}
#endif
static int s2mu00x_ifconn_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct s2mu00x_battery_info *battery =
			container_of(nb, struct s2mu00x_battery_info, ifconn_nb);
	struct ifconn_notifier_template *ifconn_info = (struct ifconn_notifier_template *)data;
	muic_attached_dev_t attached_dev = (muic_attached_dev_t)ifconn_info->event;
	const char *cmd;
	int cable_type;
	union power_supply_propval value;
	struct power_supply *psy;
	int ret;

	dev_info(battery->dev, "%s: action (%ld) dump(0x%01x, 0x%01x, 0x%02x, 0x%04x, 0x%04x, 0x%04x, 0x%04x)\n",
		__func__, action, ifconn_info->src, ifconn_info->dest, ifconn_info->id,
		ifconn_info->attach, ifconn_info->rprd, ifconn_info->cable_type, ifconn_info->event);
	ifconn_info->cable_type = (muic_attached_dev_t)ifconn_info->event;

	action = ifconn_info->id;

	if (attached_dev == ATTACHED_DEV_MHL_MUIC)
		return 0;

	switch (action) {
	case IFCONN_NOTIFY_ID_DETACH:
		cmd = "DETACH";
		cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case IFCONN_NOTIFY_ID_ATTACH:
#if defined(CONFIG_USE_CCIC)
		if (battery->cable_type == POWER_SUPPLY_TYPE_USB_PD) {
			pr_info("%s: PD TA is attached. Skip cable check\n", __func__);
			cable_type =  POWER_SUPPLY_TYPE_USB_PD;
			cmd = "PD ATTACH";
			break;
		}
#endif
		cmd = "ATTACH";
		cable_type = s2mu00x_bat_cable_check(battery, attached_dev);
		break;
#if defined(CONFIG_USE_CCIC)
	case IFCONN_NOTIFY_ID_POWER_STATUS:
		cable_type = s2mu00x_bat_pdo_check(battery, ifconn_info);
		switch (cable_type) {
			case POWER_SUPPLY_TYPE_USB_PD:
				cmd = "PD ATTACH";
				attached_dev = ATTACHED_DEV_TYPE3_CHARGER_MUIC;
				break;
			case POWER_SUPPLY_TYPE_PREPARE_TA:
				cmd = "PD PREPARE";
				attached_dev = ATTACHED_DEV_TYPE3_CHARGER_MUIC;
				break;
			default:
				cmd = "PD FAIL";
				break;
		}
		break;
#endif
	default:
		cmd = "ERROR";
		cable_type = -1;
		break;
	}

	pr_info("%s: CMD[%s] attached_dev(%d) current_cable(%d) former cable_type(%d) battery_valid(%d)\n",
			__func__, cmd,  attached_dev, cable_type,
			battery->cable_type, battery->battery_valid);

	if (battery->battery_valid == false)
		pr_info("%s: Battery is disconnected\n", __func__);

	battery->cable_type = cable_type;

#if 0 // defined(CONFIG_USE_CCIC)
	if (cable_type == POWER_SUPPLY_TYPE_PREPARE_TA)
		goto end_ifconn_handle;
#endif

	if (attached_dev == ATTACHED_DEV_OTG_MUIC) {
		if (!strcmp(cmd, "ATTACH")) {
			value.intval = true;

			psy = power_supply_get_by_name(battery->pdata->charger_name);
			if (!psy)
				return -EINVAL;
			ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);

			pr_info("%s: OTG cable attached\n", __func__);
		} else {
			value.intval = false;

			psy = power_supply_get_by_name(battery->pdata->charger_name);
			if (!psy)
				return -EINVAL;
			ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);

			pr_info("%s: OTG cable detached\n", __func__);
		}
	}
	set_bat_status_by_cable(battery);

#if 0 //defined(CONFIG_USE_CCIC)
end_ifconn_handle:
#endif
	pr_info("%s: Status(%s), Health(%s), Cable(%d), Recharging(%d)\n",
			__func__, bat_status_str[battery->status], health_str[battery->health],
			battery->cable_type, battery->is_recharging);

	power_supply_changed(battery->psy_battery);
	alarm_cancel(&battery->monitor_alarm);
	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
	return 0;
}
#endif

static void get_battery_capacity(struct s2mu00x_battery_info *battery)
{

	union power_supply_propval value;
	struct power_supply *psy;
	int ret;
	unsigned int raw_soc = 0;

	psy = power_supply_get_by_name(battery->pdata->fuelgauge_name);
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	raw_soc = value.intval;

	if (battery->status == POWER_SUPPLY_STATUS_FULL)
		battery->max_rawsoc = raw_soc;

	battery->capacity = (raw_soc*100)/battery->max_rawsoc;
	if (battery->capacity > 100)
		battery->capacity = 100;

	dev_info(battery->dev, "%s: SOC(%u), rawsoc(%d), max_rawsoc(%u).\n",
		__func__, battery->capacity, raw_soc, battery->max_rawsoc);
}

static int get_battery_info(struct s2mu00x_battery_info *battery)
{
	union power_supply_propval value;
	struct power_supply *psy;
	int ret;

	/*Get fuelgauge psy*/
	psy = power_supply_get_by_name(battery->pdata->fuelgauge_name);
	if (!psy)
		return -EINVAL;

	/* Get voltage and current value */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	battery->voltage_now = value.intval;

	value.intval = S2MU00X_BATTERY_VOLTAGE_AVERAGE;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_AVG, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	battery->voltage_avg = value.intval;

	value.intval = S2MU00X_BATTERY_CURRENT_MA;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	battery->current_now = value.intval;

	value.intval = S2MU00X_BATTERY_CURRENT_MA;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_AVG, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	battery->current_avg = value.intval;

	/* Get temperature info */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	battery->temperature = value.intval;

	get_battery_capacity(battery);
#if defined(CHARGER_S2MU106)
	/*Get charger psy*/
	psy = power_supply_get_by_name(battery->pdata->charger_name);
	if (!psy)
		return -EINVAL;

	/* Get input current limit */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	battery->current_max = value.intval;

	/* Get charge current limit */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	battery->current_chg = value.intval;

	/* Get charger status*/
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	if (battery->status != value.intval)
		pr_err("%s: battery status = %d, charger status = %d\n",
				__func__, battery->status, value.intval);
#endif
	//battery->vchg_voltage = s2mu106_powermeter_get_vchg_voltage();

	dev_info(battery->dev,
			"%s:Vnow(%dmV),Inow(%dmA),Imax(%dmA),Ichg(%dmA),SOC(%d%%),Tbat(%d)"
			"\n", __func__,
			battery->voltage_now, battery->current_now,
			battery->current_max, battery->current_chg, battery->capacity,
			battery->temperature
			);
	dev_dbg(battery->dev,
			"%s,Vavg(%dmV),Vocv(%dmV),Iavg(%dmA)\n",
			battery->battery_valid ? "Connected" : "Disconnected",
			battery->voltage_avg, battery->voltage_ocv, battery->current_avg);

#if defined(CONFIG_SMALL_CHARGER)
	psy = power_supply_get_by_name(battery->pdata->smallcharger_name);
	if (!psy)
		return -EINVAL;

	/* Get input current limit */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	battery->small_input = value.intval;

	/* Get charge current limit */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	battery->small_chg = value.intval;

	dev_info(battery->dev,
			"%s: small Imax(%dmA), Ichg(%dmA)\n", __func__,
			battery->small_input, battery->small_chg);
#endif
	return 0;
}

static int get_battery_health(struct s2mu00x_battery_info *battery)
{
	int health = POWER_SUPPLY_HEALTH_UNKNOWN;
#if defined(CHARGER_S2MU106)
	struct power_supply *psy;
	union power_supply_propval value;
	int ret;
	/* Get health status from charger */
	psy = power_supply_get_by_name(battery->pdata->charger_name);
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	health = value.intval;
#else
	health = POWER_SUPPLY_HEALTH_GOOD;
#endif

	return health;
}

static int get_temperature_health(struct s2mu00x_battery_info *battery)
{
	int health = POWER_SUPPLY_HEALTH_UNKNOWN;

	switch (battery->health) {
	case POWER_SUPPLY_HEALTH_OVERHEAT:
		if (battery->temperature < battery->temp_high_recovery)
			health = POWER_SUPPLY_HEALTH_GOOD;
		else
			health = POWER_SUPPLY_HEALTH_OVERHEAT;
		break;
	case POWER_SUPPLY_HEALTH_COLD:
		if (battery->temperature > battery->temp_low_recovery)
			health = POWER_SUPPLY_HEALTH_GOOD;
		else
			health = POWER_SUPPLY_HEALTH_COLD;
		break;
	case POWER_SUPPLY_HEALTH_GOOD:
	default:
		if (battery->temperature > battery->temp_high)
			health = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (battery->temperature < battery->temp_low)
			health = POWER_SUPPLY_HEALTH_COLD;
		else
			health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	}

	/* For test, Temperature health is always good*/
	health = POWER_SUPPLY_HEALTH_GOOD;

	return health;
}

static void check_health(struct s2mu00x_battery_info *battery)
{
	int battery_health = 0;
	int temperature_health = 0;

	battery_health = get_battery_health(battery);
	temperature_health = get_temperature_health(battery);

	pr_info("%s: T = %d, bat_health(%s), T_health(%s), Charging(%s)\n",
		__func__, battery->temperature, health_str[battery_health],
		health_str[temperature_health], bat_status_str[battery->status]);

	/* If battery & temperature both are normal,			 *
	 *	set battery->health GOOD and recover battery->status */
	if (battery_health == POWER_SUPPLY_HEALTH_GOOD &&
		temperature_health == POWER_SUPPLY_HEALTH_GOOD) {
		battery->health = POWER_SUPPLY_HEALTH_GOOD;
		if (battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING)
			set_bat_status_by_cable(battery);
		return;
	}

	switch (battery_health) {
	case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
	case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
	case POWER_SUPPLY_HEALTH_UNKNOWN:
		battery->health = battery_health;
		goto abnormal_health;
	default:
		break;
	}
	switch (temperature_health) {
	case POWER_SUPPLY_HEALTH_OVERHEAT:
	case POWER_SUPPLY_HEALTH_COLD:
	case POWER_SUPPLY_HEALTH_UNKNOWN:
		battery->health = temperature_health;
		goto abnormal_health;
	default:
		break;
	}

	pr_err("%s: Abnormal case of temperature & battery health.\n", __func__);
	return;

abnormal_health:
	if (battery->status != POWER_SUPPLY_STATUS_NOT_CHARGING) {
		battery->is_recharging = false;
		/* Take the wakelock during 10 seconds	*
		 * when not_charging status is detected */
		wake_lock_timeout(&battery->vbus_wake_lock, HZ * 10);
		set_battery_status(battery, POWER_SUPPLY_STATUS_NOT_CHARGING);
	}
}

static void check_charging_full(
		struct s2mu00x_battery_info *battery)
{
	pr_info("%s Start\n", __func__);

	if ((battery->status == POWER_SUPPLY_STATUS_DISCHARGING) ||
			(battery->status == POWER_SUPPLY_STATUS_NOT_CHARGING)) {
		dev_dbg(battery->dev,
				"%s: No Need to Check Full-Charged\n", __func__);
		return;
	}

	/* 1. Recharging check */
	if (battery->status == POWER_SUPPLY_STATUS_FULL &&
			battery->voltage_now < battery->pdata->chg_recharge_vcell &&
			!battery->is_recharging) {
		pr_info("%s: Recharging start\n", __func__);
		set_battery_status(battery, POWER_SUPPLY_STATUS_CHARGING);
		battery->is_recharging = true;
	}

	/* 2. Full charged check */
	if ((battery->current_now > 0 && battery->current_now <
				battery->pdata->charging_current[
				battery->cable_type].full_check_current) &&
			(battery->voltage_avg > battery->pdata->chg_full_vcell)) {
		battery->full_check_cnt++;
		pr_info("%s: Full Check Cnt (%d)\n", __func__, battery->full_check_cnt);
	} else if (battery->full_check_cnt != 0) {
	/* Reset full check cnt when it is out of full condition */
		battery->full_check_cnt = 0;
		pr_info("%s: Reset Full Check Cnt\n", __func__);
	}

	/* 3. If full charged, turn off charging. */
	if (battery->full_check_cnt >= battery->pdata->full_check_count) {
		battery->full_check_cnt = 0;
		battery->is_recharging = false;
		set_battery_status(battery, POWER_SUPPLY_STATUS_FULL);
		pr_info("%s: Full charged, charger off\n", __func__);
	}
}

static void bat_monitor_work(struct work_struct *work)
{
	struct s2mu00x_battery_info *battery =
		container_of(work, struct s2mu00x_battery_info, monitor_work.work);
#if defined(CHARGER_S2MU106)
	union power_supply_propval value;
	struct power_supply *psy;
	int ret;
#endif
	pr_info("%s: start monitoring\n", __func__);
#if defined(CHARGER_S2MU106)
	psy = power_supply_get_by_name(battery->pdata->charger_name);
	if (!psy)
		return;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	if (!value.intval) {
		battery->battery_valid = false;
		pr_info("%s: There is no battery, skip monitoring.\n", __func__);
		goto continue_monitor;
	} else
		battery->battery_valid = true;
#else
	battery->battery_valid = true;
#endif
	get_battery_info(battery);

	check_health(battery);

	check_charging_full(battery);

	power_supply_changed(battery->psy_battery);
#if defined(CHARGER_S2MU106)
continue_monitor:
#endif
	pr_err(
		 "%s: Status(%s), Health(%s), Cable(%d), Recharging(%d))"
		 "\n", __func__,
		 bat_status_str[battery->status],
		 health_str[battery->health],
		 battery->cable_type,
		 battery->is_recharging
		 );

	alarm_cancel(&battery->monitor_alarm);
	alarm_start_relative(&battery->monitor_alarm, ktime_set(battery->monitor_alarm_interval, 0));
	wake_unlock(&battery->monitor_wake_lock);
}

#ifdef CONFIG_OF
static int s2mu00x_battery_parse_dt(struct device *dev,
		struct s2mu00x_battery_info *battery)
{
	struct device_node *np = of_find_node_by_name(NULL, "battery");
	s2mu00x_battery_platform_data_t *pdata = battery->pdata;
	int ret = 0, len;
	unsigned int i;
	const u32 *p;
	u32 temp;
	u32 default_input_current, default_charging_current, default_full_check_current;

	if (!np) {
		pr_info("%s np NULL(battery)\n", __func__);
		return -1;
	}
	ret = of_property_read_string(np,
			"battery,vendor", (char const **)&pdata->vendor);
	if (ret)
		pr_info("%s: Vendor is empty\n", __func__);

	ret = of_property_read_string(np,
			"battery,charger_name", (char const **)&pdata->charger_name);
	if (ret)
		pr_info("%s: Charger name is empty\n", __func__);

#if defined(CONFIG_SMALL_CHARGER)
	ret = of_property_read_string(np,
			"battery,smallcharger_name", (char const **)&pdata->smallcharger_name);
	if (ret)
		pr_info("%s: Small charger name is empty\n", __func__);
#endif

	ret = of_property_read_string(np,
			"battery,fuelgauge_name", (char const **)&pdata->fuelgauge_name);
	if (ret)
		pr_info("%s: Fuelgauge name is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,technology",
			&pdata->technology);
	if (ret)
		pr_info("%s : technology is empty\n", __func__);

	p = of_get_property(np, "battery,input_current_limit", &len);
	if (!p)
		return 1;

	len = len / sizeof(u32);

	if (len < POWER_SUPPLY_TYPE_END)
		len = POWER_SUPPLY_TYPE_END;

	pdata->charging_current = kzalloc(sizeof(s2mu00x_charging_current_t) * len,
			GFP_KERNEL);

	ret = of_property_read_u32(np, "battery,default_input_current",
			&default_input_current);
	if (ret)
		pr_info("%s : default_input_current is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,default_charging_current",
			&default_charging_current);
	if (ret)
		pr_info("%s : default_charging_current is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,default_full_check_current",
			&default_full_check_current);
	if (ret)
		pr_info("%s : default_full_check_current is empty\n", __func__);

	for (i = 0; i < len; i++) {
		ret = of_property_read_u32_index(np,
				"battery,input_current_limit", i,
				&pdata->charging_current[i].input_current_limit);
		if (ret) {
			pr_info("%s : Input_current_limit is empty\n",
					__func__);
			pdata->charging_current[i].input_current_limit = default_input_current;
		}

		ret = of_property_read_u32_index(np,
				"battery,fast_charging_current", i,
				&pdata->charging_current[i].fast_charging_current);
		if (ret) {
			pr_info("%s : Fast charging current is empty\n",
					__func__);
			pdata->charging_current[i].fast_charging_current = default_charging_current;
		}

		ret = of_property_read_u32_index(np,
				"battery,full_check_current", i,
				&pdata->charging_current[i].full_check_current);
		if (ret) {
			pr_info("%s : Full check current is empty\n",
					__func__);
			pdata->charging_current[i].full_check_current = default_full_check_current;
		}
	}

	ret = of_property_read_u32(np, "battery,max_input_current",
			&pdata->max_input_current);
	if (ret)
		pr_info("%s : max_input_current is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,max_charging_current",
			&pdata->max_charging_current);
	if (ret)
		pr_info("%s : max_charging_current is empty\n", __func__);

#if defined(CONFIG_SMALL_CHARGER)
	ret = of_property_read_u32(np, "battery,small_input_current",
			&pdata->small_input_current);
	if (ret) {
		pr_info("%s : small_input_current is empty\n", __func__);
		pdata->small_input_current = 500;
	}

	ret = of_property_read_u32(np, "battery,small_charging_current",
			&pdata->small_charging_current);
	if (ret) {
		pr_info("%s : small_charging_current is empty\n", __func__);
		pdata->small_charging_current = 800;
	}
#endif

#if defined(CONFIG_USE_CCIC)
	ret = of_property_read_u32(np, "battery,pdo_max_chg_power",
			&pdata->pdo_max_chg_power);
	if (ret)
		pr_info("%s : pdo_max_chg_power is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,pdo_max_input_vol",
			&pdata->pdo_max_input_vol);
	if (ret)
		pr_info("%s : pdo_max_input_vol is empty\n", __func__);
#endif

	ret = of_property_read_u32(np, "battery,temp_high", &temp);
	if (ret) {
		pr_info("%s : temp_high is empty\n", __func__);
		pdata->temp_high = 500;
	} else
		pdata->temp_high = (int)temp;

	ret = of_property_read_u32(np, "battery,temp_high_recovery", &temp);
	if (ret) {
		pr_info("%s : temp_high_recovery is empty\n", __func__);
		pdata->temp_high_recovery = pdata->temp_high - 50;
	} else
		pdata->temp_high_recovery = (int)temp;

	ret = of_property_read_u32(np, "battery,temp_low", &temp);
	if (ret) {
		pr_info("%s : temp_low is empty\n", __func__);
		pdata->temp_low = 100;
	} else
		pdata->temp_low = (int)temp;

	ret = of_property_read_u32(np, "battery,temp_low_recovery", &temp);
	if (ret) {
		pr_info("%s : temp_low_recovery is empty\n", __func__);
		pdata->temp_low_recovery = pdata->temp_low + 50;
	} else
		pdata->temp_low_recovery = (int)temp;

	pr_info("%s : temp_high(%d), temp_high_recovery(%d), temp_low(%d), temp_low_recovery(%d)\n",
			__func__,
			pdata->temp_high, pdata->temp_high_recovery,
			pdata->temp_low, pdata->temp_low_recovery);

	ret = of_property_read_u32(np, "battery,full_check_count",
			&pdata->full_check_count);
	if (ret)
		pr_info("%s : full_check_count is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,chg_full_vcell",
			&pdata->chg_full_vcell);
	if (ret)
		pr_info("%s : chg_full_vcell is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,chg_recharge_vcell",
			&pdata->chg_recharge_vcell);
	if (ret)
		pr_info("%s : chg_recharge_vcell is empty\n", __func__);

	ret = of_property_read_u32(np, "battery,max_rawsoc",
			&pdata->max_rawsoc);
	if (ret)
		pr_info("%s : max_rawsoc is empty\n", __func__);

	pr_info("%s:DT parsing is done, vendor : %s, technology : %d\n",
			__func__, pdata->vendor, pdata->technology);
	return ret;
}
#else
static int s2mu00x_battery_parse_dt(struct device *dev,
		struct s2mu00x_battery_platform_data *pdata)
{
	return pdev->dev.platform_data;
}
#endif

static const struct of_device_id s2mu00x_battery_match_table[] = {
	{ .compatible = "samsung,s2mu00x-battery",},
	{},
};

static enum alarmtimer_restart bat_monitor_alarm(
	struct alarm *alarm, ktime_t now)
{
	struct s2mu00x_battery_info *battery = container_of(alarm,
				struct s2mu00x_battery_info, monitor_alarm);

	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);

	return ALARMTIMER_NORESTART;
}

static int s2mu00x_battery_probe(struct platform_device *pdev)
{
	struct s2mu00x_battery_info *battery;
	struct power_supply_config psy_cfg = {};
	union power_supply_propval value;
	int ret = 0, temp = 0;
	struct power_supply *psy;
#ifndef CONFIG_OF
	int i;
#endif

	pr_info("%s: S2MU00x battery driver loading\n", __func__);

	/* Allocate necessary device data structures */
	battery = kzalloc(sizeof(*battery), GFP_KERNEL);
	if (!battery)
		return -ENOMEM;

	pr_info("%s: battery is allocated\n", __func__);

	battery->pdata = devm_kzalloc(&pdev->dev, sizeof(*(battery->pdata)),
			GFP_KERNEL);
	if (!battery->pdata) {
		ret = -ENOMEM;
		goto err_bat_free;
	}

	pr_info("%s: pdata is allocated\n", __func__);

	/* Get device/board dependent configuration data from DT */
	temp = s2mu00x_battery_parse_dt(&pdev->dev, battery);
	if (temp) {
		pr_info("%s: s2mu00x_battery_parse_dt(&pdev->dev, battery) == %d\n", __func__, temp);
		dev_err(&pdev->dev, "%s: Failed to get battery dt\n", __func__);
		ret = -EINVAL;
		goto err_parse_dt_nomem;
	}

	pr_info("%s: DT parsing is done\n", __func__);

	/* Set driver data */
	platform_set_drvdata(pdev, battery);
	battery->dev = &pdev->dev;

	mutex_init(&battery->iolock);

	wake_lock_init(&battery->monitor_wake_lock, WAKE_LOCK_SUSPEND,
			"sec-battery-monitor");
	wake_lock_init(&battery->vbus_wake_lock, WAKE_LOCK_SUSPEND,
			"sec-battery-vbus");

	/* Inintialization of battery information */
	battery->status = POWER_SUPPLY_STATUS_DISCHARGING;
	battery->health = POWER_SUPPLY_HEALTH_GOOD;

	battery->input_current = 0;
	battery->charging_current = 0;
	battery->topoff_current = 0;

	battery->max_input_current = battery->pdata->max_input_current;
	battery->max_charging_current = battery->pdata->max_charging_current;
#if defined(CONFIG_USE_CCIC)
	battery->pdo_max_input_vol = battery->pdata->pdo_max_input_vol;
	battery->pdo_max_chg_power = battery->pdata->pdo_max_chg_power;
#endif
	battery->temp_high = battery->pdata->temp_high;
	battery->temp_high_recovery = battery->pdata->temp_high_recovery;
	battery->temp_low = battery->pdata->temp_low;
	battery->temp_low_recovery = battery->pdata->temp_low_recovery;

	battery->max_rawsoc = battery->pdata->max_rawsoc;

	battery->is_factory = false;

	battery->is_recharging = false;
	battery->cable_type = POWER_SUPPLY_TYPE_BATTERY;
#if defined(CHARGER_S2MU106)
	psy = power_supply_get_by_name(battery->pdata->charger_name);
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	if (!value.intval)
		battery->battery_valid = false;
	else
		battery->battery_valid = true;
#else
	battery->battery_valid = true;
#endif
	/* Register battery as "POWER_SUPPLY_TYPE_BATTERY" */
	battery->psy_battery_desc.name = "battery";
	battery->psy_battery_desc.type = POWER_SUPPLY_TYPE_BATTERY;
	battery->psy_battery_desc.get_property =  s2mu00x_battery_get_property;
	battery->psy_battery_desc.set_property =  s2mu00x_battery_set_property;
	battery->psy_battery_desc.property_is_writeable =  s2mu00x_battery_property_is_writeable;
	battery->psy_battery_desc.properties = s2mu00x_battery_props;
	battery->psy_battery_desc.num_properties =  ARRAY_SIZE(s2mu00x_battery_props);

	battery->psy_usb_desc.name = "usb";
	battery->psy_usb_desc.type = POWER_SUPPLY_TYPE_USB;
	battery->psy_usb_desc.get_property = s2mu00x_usb_get_property;
	battery->psy_usb_desc.properties = s2mu00x_power_props;
	battery->psy_usb_desc.num_properties = ARRAY_SIZE(s2mu00x_power_props);

	battery->psy_ac_desc.name = "ac";
	battery->psy_ac_desc.type = POWER_SUPPLY_TYPE_MAINS;
	battery->psy_ac_desc.properties = s2mu00x_power_props;
	battery->psy_ac_desc.num_properties = ARRAY_SIZE(s2mu00x_power_props);
	battery->psy_ac_desc.get_property = s2mu00x_ac_get_property;

	/* Initialize work queue for periodic polling thread */
	battery->monitor_wqueue =
		create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!battery->monitor_wqueue) {
		dev_err(battery->dev,
				"%s: Fail to Create Workqueue\n", __func__);
		goto err_irr;
	}

	/* Init work & alarm for monitoring */
	INIT_DELAYED_WORK(&battery->monitor_work, bat_monitor_work);
	alarm_init(&battery->monitor_alarm, ALARM_BOOTTIME, bat_monitor_alarm);
	battery->monitor_alarm_interval = DEFAULT_ALARM_INTERVAL;

	/* Register power supply to framework */
	psy_cfg.drv_data = battery;
	psy_cfg.supplied_to = s2mu00x_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(s2mu00x_supplied_to);

	battery->psy_battery = power_supply_register(&pdev->dev, &battery->psy_battery_desc, &psy_cfg);
	if (IS_ERR(battery->psy_battery)) {
		pr_err("%s: Failed to Register psy_battery\n", __func__);
		ret = PTR_ERR(battery->psy_battery);
		goto err_workqueue;
	}
	pr_info("%s: Registered battery as power supply\n", __func__);

	battery->psy_usb = power_supply_register(&pdev->dev, &battery->psy_usb_desc, &psy_cfg);
	if (IS_ERR(battery->psy_usb)) {
		pr_err("%s: Failed to Register psy_usb\n", __func__);
		ret = PTR_ERR(battery->psy_usb);
		goto err_unreg_battery;
	}
	pr_info("%s: Registered USB as power supply\n", __func__);

	battery->psy_ac = power_supply_register(&pdev->dev, &battery->psy_ac_desc, &psy_cfg);
	if (IS_ERR(battery->psy_ac)) {
		pr_err("%s: Failed to Register psy_ac\n", __func__);
		ret = PTR_ERR(battery->psy_ac);
		goto err_unreg_usb;
	}
	pr_info("%s: Registered AC as power supply\n", __func__);

	/* Initialize battery level*/
	value.intval = 0;

	psy = power_supply_get_by_name(battery->pdata->fuelgauge_name);
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	battery->capacity = value.intval;

#if defined(CONFIG_IFCONN_NOTIFIER)
	ifconn_notifier_register(&battery->ifconn_nb,
			s2mu00x_ifconn_handle_notification,
			IFCONN_NOTIFY_BATTERY,
			IFCONN_NOTIFY_MANAGER);
#elif defined(CONFIG_MUIC_NOTIFIER)
	pr_info("%s: Register MUIC notifier\n", __func__);
	muic_notifier_register(&battery->batt_nb, s2mu00x_battery_handle_notification,
			MUIC_NOTIFY_DEV_CHARGER);
#endif

	/* Kick off monitoring thread */
	pr_info("%s: start battery monitoring work\n", __func__);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 5*HZ);

	dev_info(battery->dev, "%s: Battery driver is loaded\n", __func__);
	return 0;

err_unreg_usb:
	power_supply_unregister(battery->psy_usb);
err_unreg_battery:
	power_supply_unregister(battery->psy_battery);
err_workqueue:
	destroy_workqueue(battery->monitor_wqueue);
err_irr:
	wake_lock_destroy(&battery->monitor_wake_lock);
	wake_lock_destroy(&battery->vbus_wake_lock);
	mutex_destroy(&battery->iolock);
err_parse_dt_nomem:
	kfree(battery->pdata);
err_bat_free:
	kfree(battery);

	return ret;
}

static int s2mu00x_battery_remove(struct platform_device *pdev)
{
	return 0;
}

#if defined CONFIG_PM
static int s2mu00x_battery_prepare(struct device *dev)
{
	struct s2mu00x_battery_info *battery = dev_get_drvdata(dev);

	alarm_cancel(&battery->monitor_alarm);
	wake_unlock(&battery->monitor_wake_lock);
	/* If charger is connected, monitoring is required*/
	if (battery->cable_type != POWER_SUPPLY_TYPE_BATTERY) {
		battery->monitor_alarm_interval = SLEEP_ALARM_INTERVAL;
		pr_info("%s: Increase battery monitoring interval -> %d\n",
				__func__, battery->monitor_alarm_interval);
		alarm_start_relative(&battery->monitor_alarm,
				ktime_set(battery->monitor_alarm_interval, 0));
	}
	return 0;
}

static int s2mu00x_battery_suspend(struct device *dev)
{
	return 0;
}

static int s2mu00x_battery_resume(struct device *dev)
{
	return 0;
}

static void s2mu00x_battery_complete(struct device *dev)
{
	struct s2mu00x_battery_info *battery = dev_get_drvdata(dev);

	if (battery->monitor_alarm_interval != DEFAULT_ALARM_INTERVAL) {
		battery->monitor_alarm_interval = DEFAULT_ALARM_INTERVAL;
		pr_info("%s: Recover battery monitoring interval -> %d\n",
			__func__, battery->monitor_alarm_interval);
	}
	alarm_cancel(&battery->monitor_alarm);
	wake_lock(&battery->monitor_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
}

#else
#define s2mu00x_battery_prepare NULL
#define s2mu00x_battery_suspend NULL
#define s2mu00x_battery_resume NULL
#define s2mu00x_battery_complete NULL
#endif

static const struct dev_pm_ops s2mu00x_battery_pm_ops = {
	.prepare = s2mu00x_battery_prepare,
	.suspend = s2mu00x_battery_suspend,
	.resume = s2mu00x_battery_resume,
	.complete = s2mu00x_battery_complete,
};

static struct platform_driver s2mu00x_battery_driver = {
	.driver         = {
		.name   = "s2mu00x-battery",
		.owner  = THIS_MODULE,
		.pm     = &s2mu00x_battery_pm_ops,
		.of_match_table = s2mu00x_battery_match_table,
	},
	.probe          = s2mu00x_battery_probe,
	.remove     = s2mu00x_battery_remove,
};

static int __init s2mu00x_battery_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mu00x_battery_driver);
	return ret;
}
late_initcall(s2mu00x_battery_init);

static void __exit s2mu00x_battery_exit(void)
{
	platform_driver_unregister(&s2mu00x_battery_driver);
}
module_exit(s2mu00x_battery_exit);

static int __init is_poweroff_charging_mode(char *str)
{
	return 0;
} early_param("androidboot.mode", is_poweroff_charging_mode);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Battery driver for S2MU00x");
