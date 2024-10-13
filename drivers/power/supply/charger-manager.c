/*
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This driver enables to monitor battery health and control charger
 * during suspend-to-mem.
 * Charger manager depends on other devices. register this later than
 * the depending devices.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
**/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/usb/tcpm.h>
#include <linux/usb/pd.h>

/*
 * Default termperature threshold for charging.
 * Every temperature units are in tenth of centigrade.
 */
#define CM_DEFAULT_RECHARGE_TEMP_DIFF		50
#define CM_DEFAULT_CHARGE_TEMP_MAX		500
#define CM_CAP_CYCLE_TRACK_TIME			15
#define CM_UVLO_OFFSET				50000
#define CM_FORCE_SET_FUEL_CAP_FULL		1000
#define CM_LOW_TEMP_REGION			100
#define CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD	3250000
#define CM_UVLO_CALIBRATION_CNT_THRESHOLD	5
#define CM_LOW_TEMP_SHUTDOWN_VALTAGE		3200000

#define CM_CAP_ONE_PERCENT			10
#define CM_HCAP_DECREASE_STEP			8
#define CM_HCAP_THRESHOLD			995
#define CM_CAP_FULL_PERCENT			1000
#define CM_MAGIC_NUM				0x5A5AA5A5
#define CM_CAPACITY_LEVEL_CRITICAL		0
#define CM_CAPACITY_LEVEL_LOW			15
#define CM_CAPACITY_LEVEL_NORMAL		85
#define CM_CAPACITY_LEVEL_FULL			100
#define CM_CAPACITY_LEVEL_CRITICAL_VOLTAGE	3300000
#define CM_FAST_CHARGE_ENABLE_BATTERY_VOLTAGE	3400000
#define CM_FAST_CHARGE_ENABLE_CURRENT		1200000
#define CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT	1000000
#define CM_FAST_CHARGE_DISABLE_BATTERY_VOLTAGE	3400000
#define CM_FAST_CHARGE_DISABLE_CURRENT		1000000
#define CM_FAST_CHARGE_CURRENT_2A		2000000
#define CM_FAST_CHARGE_VOLTAGE_9V		9000000
#define CM_FAST_CHARGE_VOLTAGE_5V		5000000
#define CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD	3520000
#define CM_FAST_CHARGE_START_VOLTAGE_HTHRESHOLD	4200000
#define CM_FAST_CHARGE_DISABLE_COUNT		2

#define CM_CP_VSTEP				20000
#define CM_CP_ISTEP				50000
#define CM_CP_PRIMARY_CHARGER_DIS_TIMEOUT	20
#define CM_CP_IBAT_UCP_THRESHOLD		8
#define CM_CP_ADJUST_VOLTAGE_THRESHOLD		(5 * 1000 / CM_CP_WORK_TIME_MS)
#define CM_CP_ACC_VBAT_HTHRESHOLD		3850000
#define CM_CP_VBAT_STEP1			300000
#define CM_CP_VBAT_STEP2			150000
#define CM_CP_VBAT_STEP3			50000
#define CM_CP_IBAT_STEP1			2000000
#define CM_CP_IBAT_STEP2			1000000
#define CM_CP_IBAT_STEP3			100000
#define CM_CP_VBUS_STEP1			2000000
#define CM_CP_VBUS_STEP2			1000000
#define CM_CP_VBUS_STEP3			50000
#define CM_CP_IBUS_STEP1			1000000
#define CM_CP_IBUS_STEP2			500000
#define CM_CP_IBUS_STEP3			100000

#define CM_IR_COMPENSATION_TIME			3

#define CM_CP_WORK_TIME_MS			500

static const char * const jeita_type_names[] = {
	[CM_JEITA_UNKNOWN] = "cm-unknown-jeita-temp-table",
	[CM_JEITA_SDP] = "cm-sdp-jeita-temp-table",
	[CM_JEITA_CDP] = "cm-cdp-jeita-temp-table",
	[CM_JEITA_DCP] = "cm-dcp-jeita-temp-table",
	[CM_JEITA_FCHG] = "cm-fchg-jeita-temp-table",
	[CM_JEITA_FLASH] = "cm-flash-jeita-temp-table",
	[CM_JEITA_WL_BPP] = "cm-wl-bpp-jeita-temp-table",
	[CM_JEITA_WL_EPP] = "cm-wl-epp-jeita-temp-table",
};

static const char * const cm_cp_state_names[] = {
	[CM_CP_STATE_UNKNOWN] = "Charge pump state: UNKNOWN",
	[CM_CP_STATE_RECOVERY] = "Charge pump state: RECOVERY",
	[CM_CP_STATE_ENTRY] = "Charge pump state: ENTRY",
	[CM_CP_STATE_CHECK_VBUS] = "Charge pump state: CHECK VBUS",
	[CM_CP_STATE_TUNE] = "Charge pump state: TUNE",
	[CM_CP_STATE_EXIT] = "Charge pump state: EXIT",
};

static char *charger_manager_supplied_to[] = {
	"audio-ldo",
};

/*
 * Regard CM_JIFFIES_SMALL jiffies is small enough to ignore for
 * delayed works so that we can run delayed works with CM_JIFFIES_SMALL
 * without any delays.
 */
#define	CM_JIFFIES_SMALL	(2)

/* If y is valid (> 0) and smaller than x, do x = y */
#define CM_MIN_VALID(x, y)	x = (((y > 0) && ((x) > (y))) ? (y) : (x))

/*
 * Regard CM_RTC_SMALL (sec) is small enough to ignore error in invoking
 * rtc alarm. It should be 2 or larger
 */
#define CM_RTC_SMALL		(2)

struct charger_type {
	int type;
	enum power_supply_charger_type adap_type;
};

static struct charger_type charger_type[20] = {
	{POWER_SUPPLY_USB_TYPE_SDP, POWER_SUPPLY_USB_CHARGER_TYPE_SDP},
	{POWER_SUPPLY_USB_TYPE_DCP, POWER_SUPPLY_USB_CHARGER_TYPE_DCP},
	{POWER_SUPPLY_USB_TYPE_CDP, POWER_SUPPLY_USB_CHARGER_TYPE_CDP},
	{POWER_SUPPLY_USB_TYPE_ACA, POWER_SUPPLY_USB_CHARGER_TYPE_ACA},
	{POWER_SUPPLY_USB_TYPE_C, POWER_SUPPLY_USB_CHARGER_TYPE_C},
	{POWER_SUPPLY_USB_TYPE_PD, POWER_SUPPLY_USB_CHARGER_TYPE_PD},
	{POWER_SUPPLY_USB_TYPE_PD_DRP, POWER_SUPPLY_USB_CHARGER_TYPE_PD_DRP},
	{POWER_SUPPLY_USB_TYPE_PD_PPS, POWER_SUPPLY_USB_CHARGER_TYPE_PD_PPS},
	{POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID, POWER_SUPPLY_USB_CHARGER_TYPE_APPLE_BRICK_ID},
	{POWER_SUPPLY_USB_TYPE_SFCP_1P0, POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_1P0},
	{POWER_SUPPLY_USB_TYPE_SFCP_2P0, POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_2P0},
	{POWER_SUPPLY_WIRELESS_TYPE_UNKNOWN, POWER_SUPPLY_CHARGER_TYPE_UNKNOWN},
	{POWER_SUPPLY_WIRELESS_TYPE_BPP, POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP},
	{POWER_SUPPLY_WIRELESS_TYPE_EPP, POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP},
	{0, 0},
};

struct charger_otg_match {
	const char *charger_power_supply_name;
	const char *otg_name;
};

static LIST_HEAD(cm_list);
static DEFINE_MUTEX(cm_list_mtx);

/* About in-suspend (suspend-again) monitoring */
static struct alarm *cm_timer;

static bool cm_suspended;
static bool cm_timer_set;
static unsigned long cm_suspend_duration_ms;
static enum cm_event_types cm_event_type;
static char *cm_event_msg;
static struct charger_manager *global_cm;

/* About normal (not suspended) monitoring */
static unsigned long polling_jiffy = ULONG_MAX; /* ULONG_MAX: no polling */
static unsigned long next_polling; /* Next appointed polling time */
static struct workqueue_struct *cm_wq; /* init at driver add */
static struct delayed_work cm_monitor_work; /* init at driver add */

static bool allow_charger_enable;
static bool is_charger_mode;
static void cm_notify_type_handle(struct charger_manager *cm, enum cm_event_types type, char *msg);
static bool cm_manager_adjust_current(struct charger_manager *cm, int jeita_status);
static void cm_update_charger_type_status(struct charger_manager *cm);
static int cm_manager_get_jeita_status(struct charger_manager *cm, int cur_temp);
static bool cm_charger_is_support_fchg(struct charger_manager *cm);
static int batt_slate_mode_value = 0;
const char *chg_ic;
EXPORT_SYMBOL(chg_ic);
static int __init chg_ic_get(char *str)
{
	if (str != NULL)
		chg_ic = str;
	//printk("chg_ic from uboot: %s\n", chg_ic);
	return 0;
}
__setup("chg_ic=", chg_ic_get);

static int __init boot_calibration_mode(char *str)
{
	if (!str)
		return 0;

	if (!strncmp(str, "cali", strlen("cali")) ||
	    !strncmp(str, "autotest", strlen("autotest")))
		allow_charger_enable = true;
	else if (!strncmp(str, "charger", strlen("charger")))
		is_charger_mode =  true;

	return 0;
}
__setup("androidboot.mode=", boot_calibration_mode);

static void cm_cap_remap_init_boundary(struct charger_desc *desc, int index, struct device *dev)
{

	if (index == 0) {
		desc->cap_remap_table[index].lb = (desc->cap_remap_table[index].lcap) * 1000;
		desc->cap_remap_total_cnt = desc->cap_remap_table[index].lcap;
	} else {
		desc->cap_remap_table[index].lb = desc->cap_remap_table[index - 1].hb +
			(desc->cap_remap_table[index].lcap -
			 desc->cap_remap_table[index - 1].hcap) * 1000;
		desc->cap_remap_total_cnt += (desc->cap_remap_table[index].lcap -
					      desc->cap_remap_table[index - 1].hcap);
	}

	desc->cap_remap_table[index].hb = desc->cap_remap_table[index].lb +
		(desc->cap_remap_table[index].hcap - desc->cap_remap_table[index].lcap) *
		desc->cap_remap_table[index].cnt * 1000;

	desc->cap_remap_total_cnt +=
		(desc->cap_remap_table[index].hcap - desc->cap_remap_table[index].lcap) *
		desc->cap_remap_table[index].cnt;

	dev_info(dev, "%s, cap_remap_table[%d].lb =%d,cap_remap_table[%d].hb = %d\n",
		 __func__, index, desc->cap_remap_table[index].lb, index,
		 desc->cap_remap_table[index].hb);
}

/*
 * cm_capacity_remap - remap fuel_cap
 * @ fuel_cap: cap from fuel gauge
 * Return the remapped cap
 */
static int cm_capacity_remap(struct charger_manager *cm, int fuel_cap)
{
	int i, temp, cap = 0;

	if (cm->desc->cap_remap_full_percent) {
		fuel_cap = fuel_cap * 100 / cm->desc->cap_remap_full_percent;
		if (fuel_cap > CM_CAP_FULL_PERCENT)
			fuel_cap  = CM_CAP_FULL_PERCENT;
	}

	if (!cm->desc->cap_remap_table)
		return fuel_cap;

	if (fuel_cap < 0) {
		fuel_cap = 0;
		return 0;
	} else if (fuel_cap >  CM_CAP_FULL_PERCENT) {
		fuel_cap  = CM_CAP_FULL_PERCENT;
		return fuel_cap;
	}

	temp = fuel_cap * cm->desc->cap_remap_total_cnt;

	for (i = 0; i < cm->desc->cap_remap_table_len; i++) {
		if (temp <= cm->desc->cap_remap_table[i].lb) {
			if (i == 0)
				cap = DIV_ROUND_CLOSEST(temp, 100);
			else
				cap = DIV_ROUND_CLOSEST((temp -
					cm->desc->cap_remap_table[i - 1].hb), 100) +
					cm->desc->cap_remap_table[i - 1].hcap * 10;
			break;
		} else if (temp <= cm->desc->cap_remap_table[i].hb) {
			cap = DIV_ROUND_CLOSEST((temp - cm->desc->cap_remap_table[i].lb),
						cm->desc->cap_remap_table[i].cnt * 100)
				+ cm->desc->cap_remap_table[i].lcap * 10;
			break;
		}

		if (i == cm->desc->cap_remap_table_len - 1 && temp > cm->desc->cap_remap_table[i].hb)
			cap = DIV_ROUND_CLOSEST((temp - cm->desc->cap_remap_table[i].hb), 100)
				+ cm->desc->cap_remap_table[i].hcap;

	}

	return cap;
}

static int cm_init_cap_remap_table(struct charger_desc *desc, struct device *dev)
{

	struct device_node *np = dev->of_node;
	const __be32 *list;
	int i, size;

	list = of_get_property(np, "cm-cap-remap-table", &size);
	if (!list || !size) {
		dev_err(dev, "%s  get cm-cap-remap-table fail\n", __func__);
		return 0;
	}
	desc->cap_remap_table_len = (u32)size / (3 * sizeof(__be32));
	desc->cap_remap_table = devm_kzalloc(dev, sizeof(struct cap_remap_table) *
				(desc->cap_remap_table_len + 1), GFP_KERNEL);
	if (!desc->cap_remap_table) {
		dev_err(dev, "%s, get cap_remap_table fail\n", __func__);
		return -ENOMEM;
	}
	for (i = 0; i < desc->cap_remap_table_len; i++) {
		desc->cap_remap_table[i].lcap = be32_to_cpu(*list++);
		desc->cap_remap_table[i].hcap = be32_to_cpu(*list++);
		desc->cap_remap_table[i].cnt = be32_to_cpu(*list++);

		cm_cap_remap_init_boundary(desc, i, dev);

		dev_info(dev, " %s,cap_remap_table[%d].lcap= %d,cap_remap_table[%d].hcap = %d,"
			 "cap_remap_table[%d].cnt= %d\n",
		       __func__, i, desc->cap_remap_table[i].lcap,
			i, desc->cap_remap_table[i].hcap, i, desc->cap_remap_table[i].cnt);
	}

	if (desc->cap_remap_table[desc->cap_remap_table_len - 1].hcap != 100)
		desc->cap_remap_total_cnt +=
			(100 - desc->cap_remap_table[desc->cap_remap_table_len - 1].hcap);

	dev_info(dev, "%s, cap_remap_total_cnt =%d, cap_remap_table_len = %d\n",
	       __func__, desc->cap_remap_total_cnt, desc->cap_remap_table_len);

	return 0;
}

/**
 * is_batt_present - See if the battery presents in place.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_batt_present(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool present = false;
	int i, ret;

	switch (cm->desc->battery_present) {
	case CM_BATTERY_PRESENT:
		present = true;
		break;
	case CM_NO_BATTERY:
		break;
	case CM_FUEL_GAUGE:
		psy = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!psy)
			break;

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
		if (ret == 0 && val.intval)
			present = true;
		power_supply_put(psy);
		break;
	case CM_CHARGER_STAT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
			power_supply_put(psy);
			if (ret == 0 && val.intval) {
				present = true;
				break;
			}
		}
		break;
	}

	return present;
}

static bool is_ext_wl_pwr_online(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool online = false;
	int i, ret;

	if (!cm->desc->psy_wl_charger_stat)
		return online;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_wl_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		power_supply_put(psy);
		if (ret == 0 && val.intval) {
			online = true;
			break;
		}
	}

	return online;
}

/**
 * is_ext_usb_pwr_online - See if an external power source is attached to charge
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if at least one of the chargers of the battery has an external
 * power source attached to charge the battery regardless of whether it is
 * actually charging or not.
 */
static bool is_ext_usb_pwr_online(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	bool online = false;
	int i, ret;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		power_supply_put(psy);
		if (ret == 0 && val.intval) {
			online = true;
			break;
		}
	}

	return online;
}

static bool is_ext_pwr_online(struct charger_manager *cm)
{
	bool online = false;

	if (is_ext_usb_pwr_online(cm) || is_ext_wl_pwr_online(cm))
		online = true;

	return online;
}
/**
 * get_cp_ibat_uA - Get the charge current of the battery from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uA: the current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_ibat_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm || !cm->desc || !cm->desc->psy_cp_stat)
		return ret;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0)
			*uA += val.intval;
	}

	return ret;
}

/**
 * get_cp_vbat_uV - Get the voltage level of the battery from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_vbat_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm || !cm->desc || !cm->desc->psy_cp_stat)
		return ret;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		power_supply_put(cp_psy);
		if (ret == 0) {
			*uV = val.intval;
			break;
		}
	}

	return ret;
}

/**
 * get_cp_vbus_uV - Get the voltage level of the bus from charge pump
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_cp_vbus_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm || !cm->desc || !cm->desc->psy_cp_stat)
		return ret;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
						&val);
		power_supply_put(cp_psy);
		if (ret == 0) {
			*uV = val.intval;
			break;
		}
	}

	return ret;
}

 /**
  * get_cp_ibus_uA - Get the current level of the charge pump
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_cp_ibus_uA(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm->desc->psy_cp_stat)
		return 0;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		ret = power_supply_get_property(cp_psy,
						POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
						&val);
		power_supply_put(cp_psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

static int get_cp_ibat_uA_by_id(struct charger_manager *cm, int *cur, int id)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret = -ENODEV;

	*cur = 0;

	if (!cm->desc->psy_cp_stat || !cm->desc->psy_cp_stat[id])
		return 0;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[id]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
			cm->desc->psy_cp_stat[id]);
		return ret;
	}

	ret = power_supply_get_property(cp_psy,
					POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(cp_psy);
	if (ret == 0)
		*cur = val.intval;

	return ret;
}

 /**
  * get_ibat_avg_uA - Get the current level of the battery
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_ibat_avg_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_AVG, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uA = val.intval;
	return 0;
}

 /**
  * get_ibat_now_uA - Get the current level of the battery
  * @cm: the Charger Manager representing the battery.
  * @uA: the current level returned.
  *
  * Returns 0 if there is no error.
  * Returns a negative value on error.
  */
static int get_ibat_now_uA(struct charger_manager *cm, int *uA)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uA = val.intval;
	return 0;
}

/**
 *
 * get_vbat_avg_uV - Get the voltage level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_vbat_avg_uV(struct charger_manager *cm, int *uV)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_AVG, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*uV = val.intval;
	return 0;
}

/*
 * get_batt_ocv - Get the battery ocv
 * level of the battery.
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_ocv(struct charger_manager *cm, int *ocv)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_OCV, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*ocv = val.intval;
	return 0;
}

/*
 * get_batt_now - Get the battery voltage now
 * level of the battery.
 * @cm: the Charger Manager representing the battery.
 * @uV: the voltage level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_vbat_now_uV(struct charger_manager *cm, int *ocv)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*ocv = val.intval;
	return 0;
}

/**
 * get_batt_cap - Get the cap level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the cap level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_cap(struct charger_manager *cm, int *cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = 0;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*cap = val.intval;
	return 0;
}

/**
 * get_batt_total_cap - Get the total capacity level of the battery
 * @cm: the Charger Manager representing the battery.
 * @uV: the total_cap level returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_batt_total_cap(struct charger_manager *cm, u32 *total_cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*total_cap = val.intval;

	return 0;
}

/*
 * get_boot_cap - Get the battery boot capacity
 * of the battery.
 * @cm: the Charger Manager representing the battery.
 * @cap: the battery capacity returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_boot_cap(struct charger_manager *cm, int *cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = CM_BOOT_CAPACITY;
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	*cap = val.intval;
	return 0;
}

static void cm_get_charger_type(struct charger_manager *cm, u32 *type)
{
	int i = 0;

	while (charger_type[i].type != 0) {
		if (*type == charger_type[i].type) {
			*type = charger_type[i].adap_type;
			return;
		}

		i++;
	}
}

/**
 * get_usb_charger_type - Get the charger type
 * @cm: the Charger Manager representing the battery.
 * @type: the charger type returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_usb_charger_type(struct charger_manager *cm, u32 *type)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = -EINVAL, i;

	mutex_lock(&cm->desc->charger_type_mtx);
	if (cm->desc->is_fast_charge) {
		mutex_unlock(&cm->desc->charger_type_mtx);
		return 0;
	}

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_USB_TYPE, &val);
		power_supply_put(psy);
		if (ret == 0) {
			*type = val.intval;
			break;
		}
	}

	cm_get_charger_type(cm, type);

	mutex_unlock(&cm->desc->charger_type_mtx);
	return ret;
}

/**
 * get_wireless_charger_type - Get the wireless_charger type
 * @cm: the Charger Manager representing the battery.
 * @type: the wireless charger type returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_wireless_charger_type(struct charger_manager *cm, u32 *type)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = -EINVAL, i;

	if (!cm->desc->psy_wl_charger_stat)
		return 0;

	mutex_lock(&cm->desc->charger_type_mtx);
	for (i = 0; cm->desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_wl_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_WIRELESS_TYPE, &val);
		power_supply_put(psy);
		if (ret == 0) {
			*type = val.intval;
			break;
		}
	}

	cm_get_charger_type(cm, type);
	mutex_unlock(&cm->desc->charger_type_mtx);

	return ret;
}

/**
 * set_batt_cap - Set the cap level of the battery
 * @cm: the Charger Manager representing the battery.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int set_batt_cap(struct charger_manager *cm, int cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "can not find fuel gauge device\n");
		return -ENODEV;
	}

	val.intval = cap;
	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to save current battery capacity\n");

	return ret;
}
/**
 * get_charger_voltage - Get the charging voltage from fgu
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging input voltage returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_charger_voltage(struct charger_manager *cm, int *vol)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret = -ENODEV;

	if (!is_ext_pwr_online(cm))
		return 0;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(cm->dev, "Cannot find power supply  %s\n",
			cm->desc->psy_fuel_gauge);
		return	ret;
	}

	ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(fuel_gauge);
	if (ret == 0)
		*vol = val.intval;

	return ret;
}

/**
 * adjust_fuel_cap - Adjust the fuel cap level
 * @cm: the Charger Manager representing the battery.
 * @cap: the adjust fuel cap level.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int adjust_fuel_cap(struct charger_manager *cm, int cap)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	val.intval = cap;
	ret = power_supply_set_property(fuel_gauge, POWER_SUPPLY_PROP_CALIBRATE, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		dev_err(cm->dev, "failed to adjust fuel cap\n");

	return ret;
}

/**
 * get_charger_current - Get the charging current from charging ic
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_charger_current(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
						&val);
		power_supply_put(psy);
		if (ret == 0) {
			*cur += val.intval;
		}
	}

	return ret;
}

/**
 * get_charger_limit_current - Get the charging limit current from charging ic
 * @cm: the Charger Manager representing the battery.
 * @cur: the charging input limit current returned.
 *
 * Returns 0 if there is no error.
 * Returns a negative value on error.
 */
static int get_charger_limit_current(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
						&val);
		power_supply_put(psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

static int get_charger_input_current(struct charger_manager *cm, int *cur)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret = -ENODEV;

	/* If at least one of them has one, it's yes. */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
						&val);
		power_supply_put(psy);
		if (ret == 0)
			*cur += val.intval;
	}

	return ret;
}

/**
 * is_charging - Returns true if the battery is being charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_charging(struct charger_manager *cm)
{
	bool charging = false, wl_online = false;
	struct power_supply *psy;
	union power_supply_propval val;
	int i, ret;

	/* If there is no battery, it cannot be charged */
	if (!is_batt_present(cm))
		return false;

	if (is_ext_wl_pwr_online(cm))
		wl_online = true;

	/* If at least one of the charger is charging, return yes */
	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		/* 1. The charger sholuld not be DISABLED */
		if (cm->emergency_stop)
			continue;
		if (!cm->charger_enabled)
			continue;

		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
			continue;
		}

		/* 2. The charger should be online (ext-power) */
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
		if (ret) {
			dev_warn(cm->dev, "Cannot read ONLINE value from %s\n",
				 cm->desc->psy_charger_stat[i]);
			power_supply_put(psy);
			continue;
		}

		if (val.intval == 0 && !wl_online) {
			power_supply_put(psy);
			continue;
		}

		/*
		 * 3. The charger should not be FULL, DISCHARGING,
		 * or NOT_CHARGING.
		 */
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (ret) {
			dev_warn(cm->dev, "Cannot read STATUS value from %s\n",
				 cm->desc->psy_charger_stat[i]);
			continue;
		}
		if (val.intval == POWER_SUPPLY_STATUS_FULL ||
		    val.intval == POWER_SUPPLY_STATUS_DISCHARGING ||
		    val.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)
			continue;

		/* Then, this is charging. */
		charging = true;
		break;
	}

	return charging;
}

static bool cm_primary_charger_enable(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret;

	if (!cm->desc->psy_cp_stat)
		return false;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	val.intval = enable;
	pr_err("%s:val=%d\n",__func__,val.intval);
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev, "failed to %s primary charger, ret = %d\n",
			enable ? "enable" : "disable", ret);
		return false;
	}

	return true;
}

/**
 * is_full_charged - Returns true if the battery is fully charged.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_full_charged(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	bool is_full = false;
	int ret = 0;
	int uV, uA;

	/* If there is no battery, it cannot be charged */
	if (!is_batt_present(cm))
		return false;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return false;

	if (desc->fullbatt_full_capacity > 0) {
		val.intval = 0;

		/* Not full if capacity of fuel gauge isn't full */
		ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CHARGE_FULL, &val);
		if (!ret && val.intval > desc->fullbatt_full_capacity) {
			is_full = true;
			goto out;
		}
	}

	/* Full, if it's over the fullbatt voltage */
	if (desc->fullbatt_uV > 0 && desc->fullbatt_uA > 0) {
		ret = get_vbat_now_uV(cm, &uV);
		if (ret)
			goto out;

		ret = get_ibat_now_uA(cm, &uA);
		if (ret)
			goto out;

		/* Battery is already full, checks voltage drop. */
		if (cm->battery_status == POWER_SUPPLY_STATUS_FULL && desc->fullbatt_vchkdrop_uV) {
			int batt_ocv;

			ret = get_batt_ocv(cm, &batt_ocv);
			if (ret)
				goto out;

			if (batt_ocv > (cm->desc->fullbatt_uV - cm->desc->fullbatt_vchkdrop_uV))
				is_full = true;
			goto out;
		}

		if (desc->first_fullbatt_uA > 0 && uV >= desc->fullbatt_uV &&
		    uA > desc->fullbatt_uA && uA <= desc->first_fullbatt_uA && uA >= 0) {
			if (++desc->first_trigger_cnt > 1)
				cm->desc->force_set_full = true;
		} else {
			desc->first_trigger_cnt = 0;
		}

		if (uV >= desc->fullbatt_uV && uA <= desc->fullbatt_uA && uA >= 0) {
			if (++desc->trigger_cnt > 1) {
				if (cm->desc->cap >= CM_CAP_FULL_PERCENT) {
					if (desc->trigger_cnt == 2)
						adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
					is_full = true;
				} else {
					is_full = false;
					adjust_fuel_cap(cm, CM_FORCE_SET_FUEL_CAP_FULL);
					if (desc->trigger_cnt == 2)
						cm_primary_charger_enable(cm, false);
				}
				cm->desc->force_set_full = true;
			} else {
				is_full = false;
			}
			goto out;
		} else {
			is_full = false;
			desc->trigger_cnt = 0;
			goto out;
		}
	}

	/* Full, if the capacity is more than fullbatt_soc */
	if (desc->fullbatt_soc > 0) {
		val.intval = 0;

		ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CAPACITY, &val);
		if (!ret && val.intval >= desc->fullbatt_soc) {
			is_full = true;
			goto out;
		}
	}

out:
	power_supply_put(fuel_gauge);
	return is_full;
}

/**
 * is_polling_required - Return true if need to continue polling for this CM.
 * @cm: the Charger Manager representing the battery.
 */
static bool is_polling_required(struct charger_manager *cm)
{
	switch (cm->desc->polling_mode) {
	case CM_POLL_DISABLE:
		return false;
	case CM_POLL_ALWAYS:
		return true;
	case CM_POLL_EXTERNAL_POWER_ONLY:
		return is_ext_pwr_online(cm);
	case CM_POLL_CHARGING_ONLY:
		return is_charging(cm);
	default:
		dev_warn(cm->dev, "Incorrect polling_mode (%d)\n",
			 cm->desc->polling_mode);
	}

	return false;
}

static void cm_update_current_jeita_status(struct charger_manager *cm)
{
	int cur_jeita_status;

	/* Note that it need to vote for ibat befor the caller of this function
	 * if does not define jeita table*/
	if (cm->desc->jeita_tab_size && !cm->charging_status) {
		cur_jeita_status = cm_manager_get_jeita_status(cm, cm->desc->temperature);
		if (cm->desc->jeita_disabled)
			cur_jeita_status = cm->desc->force_jeita_status;
		cm_manager_adjust_current(cm, cur_jeita_status);
	}
}

static void cm_update_charge_info(struct charger_manager *cm, int cmd)
{
	struct charger_desc *desc = cm->desc;
	struct cm_thermal_info *thm_info = &cm->desc->thm_info;

	switch (desc->charger_type) {
	case POWER_SUPPLY_USB_CHARGER_TYPE_DCP:
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_tab_size)
			desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_DCP];
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_USB_CHARGER_TYPE_SDP:
		desc->charge_limit_cur = desc->cur.sdp_cur;
		desc->input_limit_cur = desc->cur.sdp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_tab_size)
			desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_SDP];
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_USB_CHARGER_TYPE_CDP:
		desc->charge_limit_cur = desc->cur.cdp_cur;
		desc->input_limit_cur = desc->cur.cdp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_tab_size)
			desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_CDP];
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_USB_CHARGER_TYPE_PD:
	case POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_1P0:
		if (desc->enable_fast_charge) {
			desc->charge_limit_cur = desc->cur.fchg_cur;
			desc->input_limit_cur = desc->cur.fchg_limit;
			thm_info->adapter_default_charge_vol = 9;
			if (desc->jeita_tab_size)
				desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_FCHG];
			if (desc->fast_charge_voltage_max)
				desc->charge_voltage_max = desc->fast_charge_voltage_max;
			if (desc->fast_charge_voltage_drop)
				desc->charge_voltage_drop = desc->fast_charge_voltage_drop;
			break;
		}
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_tab_size)
			desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_DCP];
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_USB_CHARGER_TYPE_PD_PPS:
	case POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_2P0:
		if (desc->cp.cp_running && !desc->cp.recovery) {
			desc->charge_limit_cur = desc->cur.flash_cur;
			desc->input_limit_cur = desc->cur.flash_limit;
			thm_info->adapter_default_charge_vol = 11;
			if (desc->jeita_tab_size)
				desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_FLASH];
			if (desc->flash_charge_voltage_max)
				desc->charge_voltage_max = desc->flash_charge_voltage_max;
			if (desc->flash_charge_voltage_drop)
				desc->charge_voltage_drop = desc->flash_charge_voltage_drop;
			break;
		}
		desc->charge_limit_cur = desc->cur.dcp_cur;
		desc->input_limit_cur = desc->cur.dcp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_tab_size)
			desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_DCP];
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP:
		desc->charge_limit_cur = desc->cur.wl_bpp_cur;
		desc->input_limit_cur = desc->cur.wl_bpp_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_tab_size)
			desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_WL_BPP];
		if (desc->wireless_normal_charge_voltage_max)
			desc->charge_voltage_max = desc->wireless_normal_charge_voltage_max;
		if (desc->wireless_normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->wireless_normal_charge_voltage_drop;
		break;
	case POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP:
		desc->charge_limit_cur = desc->cur.wl_epp_cur;
		desc->input_limit_cur = desc->cur.wl_epp_limit;
		thm_info->adapter_default_charge_vol = 11;
		if (desc->jeita_tab_size)
			desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_WL_EPP];
		if (desc->wireless_fast_charge_voltage_max)
			desc->charge_voltage_max = desc->wireless_fast_charge_voltage_max;
		if (desc->wireless_fast_charge_voltage_drop)
			desc->charge_voltage_drop = desc->wireless_fast_charge_voltage_drop;
		break;
	default:
		desc->charge_limit_cur = desc->cur.unknown_cur;
		desc->input_limit_cur = desc->cur.unknown_limit;
		thm_info->adapter_default_charge_vol = 5;
		if (desc->jeita_tab_size)
			desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_UNKNOWN];
		if (desc->normal_charge_voltage_max)
			desc->charge_voltage_max = desc->normal_charge_voltage_max;
		if (desc->normal_charge_voltage_drop)
			desc->charge_voltage_drop = desc->normal_charge_voltage_drop;
		break;
	}

	if (thm_info->thm_pwr && thm_info->adapter_default_charge_vol)
		thm_info->thm_adjust_cur = (int)(thm_info->thm_pwr /
			thm_info->adapter_default_charge_vol) * 1000;

	dev_info(cm->dev, "%s, chgr type = %d, fchg_en = %d, cp_running = %d, cp_recovery = %d"
		 " max chg_lmt_cur = %duA, max inpt_lmt_cur = %duA, max chg_volt = %duV,"
		 " chg_volt_drop = %d, adapter_chg_volt = %dmV, thm_cur = %d, chg_info_cmd = 0x%x\n",
		 __func__, desc->charger_type, desc->enable_fast_charge, desc->cp.cp_running,
		 desc->cp.recovery, desc->charge_limit_cur, desc->input_limit_cur,
		 desc->charge_voltage_max, desc->charge_voltage_drop,
		 thm_info->adapter_default_charge_vol * 1000, thm_info->thm_adjust_cur, cmd);

	if (!cm->cm_charge_vote || !cm->cm_charge_vote->vote) {
		dev_err(cm->dev, "%s: cm_charge_vote is null\n", __func__);
		return;
	}

	if (cmd & CM_CHARGE_INFO_CHARGE_LIMIT)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CHARGER_TYPE,
					 SPRD_VOTE_CMD_MIN, desc->charge_limit_cur, cm);
	if (cmd & CM_CHARGE_INFO_INPUT_LIMIT)
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBUS,
					 SPRD_VOTE_TYPE_IBUS_ID_CHARGER_TYPE,
					 SPRD_VOTE_CMD_MIN, desc->input_limit_cur, cm);
	if (cmd & CM_CHARGE_INFO_THERMAL_LIMIT && thm_info->thm_adjust_cur > 0) {
		/* The ChargerIC with linear charging cannot set Ibus, only Ibat. */
		if (cm->desc->thm_info.need_calib_charge_lmt)
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CHARGE_CONTROL_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 cm->desc->thm_info.thm_adjust_cur, cm);
		else
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
						 SPRD_VOTE_TYPE_IBUS,
						 SPRD_VOTE_TYPE_IBUS_ID_CHARGE_CONTROL_LIMIT,
						 SPRD_VOTE_CMD_MIN,
						 cm->desc->thm_info.thm_adjust_cur, cm);
	}
	if (cmd & CM_CHARGE_INFO_JEITA_LIMIT)
		cm_update_current_jeita_status(cm);
}

static void cm_vote_property(struct charger_manager *cm, int target_val,
			     const char **name, enum power_supply_property psp)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int i, ret;

	if (!name) {
		dev_err(cm->dev, "psy name is null!!!\n");
		return;
	}

	for (i = 0; name[i]; i++) {
		psy = power_supply_get_by_name(name[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n", name[i]);
			continue;
		}

		val.intval = target_val;
		ret = power_supply_set_property(psy, psp, &val);
		power_supply_put(psy);
		if (ret)
			dev_err(cm->dev, "failed to %s set power_supply_property[%d], ret = %d\n",
				name[i], psp, ret);
	}
}

static int cm_check_parallel_charger(struct charger_manager *cm, int cur)
{
	if (cm->desc->enable_fast_charge && cm->desc->psy_charger_stat[1])
		cur /= 2;

	return cur;
}

static void cm_sprd_vote_callback(struct sprd_vote *vote_gov, int vote_type,
				  int value, void *data)
{
	struct charger_manager *cm = (struct charger_manager *)data;
	const char **psy_charger_name;

	dev_info(cm->dev, "%s, %s[%d]\n", __func__, vote_type_names[vote_type], value);
	switch (vote_type) {
	case SPRD_VOTE_TYPE_IBAT:
		psy_charger_name = cm->desc->psy_charger_stat;
		value = cm_check_parallel_charger(cm, value);
		cm_vote_property(cm, value, psy_charger_name,
				 POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT);
		break;
	case SPRD_VOTE_TYPE_IBUS:
		psy_charger_name = cm->desc->psy_charger_stat;
		value = cm_check_parallel_charger(cm, value);
		cm_vote_property(cm, value, psy_charger_name,
				 POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT);
		break;
	case SPRD_VOTE_TYPE_CCCV:
		psy_charger_name = cm->desc->psy_charger_stat;
		if (cm->desc->cp.cp_running)
			psy_charger_name = cm->desc->psy_cp_stat;
		cm_vote_property(cm, value, psy_charger_name,
				 POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX);
		break;
	default:
		dev_err(cm->dev, "vote_gov: vote_type[%d] error!!!\n", vote_type);
		break;
	}
}

static int cm_get_adapter_max_voltage(struct charger_manager *cm, int *max_vol)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	*max_vol = 0;
	psy = power_supply_get_by_name(desc->psy_fast_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fast_charger_stat[0]);
		return -ENODEV;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to get max voltage\n");
		return ret;
	}

	*max_vol = val.intval;

	return 0;
}

static int cm_get_adapter_max_current(struct charger_manager *cm, int *max_cur)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	*max_cur = 0;
	psy = power_supply_get_by_name(desc->psy_fast_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fast_charger_stat[0]);
		return -ENODEV;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to get max current\n");
		return ret;
	}

	*max_cur = val.intval;

	return 0;
}

static int cm_set_main_charger_current(struct charger_manager *cm, int cmd)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!desc->psy_charger_stat[0])
		return -ENODEV;

	/*
	 * make the psy_charger_stat[0] to be main charger,
	 * set the main charger charge current and limit current
	 * in 9V/5V fast charge status.
	 */

	psy = power_supply_get_by_name(desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_charger_stat[0]);
		power_supply_put(psy);
		return -ENODEV;
	}

	val.intval = cmd;
	pr_err("%s:val=%d\n",__func__,val.intval);
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to set main charger current cmd = %d\n", cmd);
		return ret;
	}

	return 0;
}

static int cm_set_second_charger_current(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!desc->psy_charger_stat[1])
		return 0;

	/*
	 * if psy_charger_stat[1] defined,
	 * make the psy_charger_stat[1] to be second charger,
	 * set the second charger current.
	 */
	psy = power_supply_get_by_name(desc->psy_charger_stat[1]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_charger_stat[1]);
		power_supply_put(psy);
		return -ENODEV;
	}

	/*
	 * set the second charger charge current and limit current
	 * in 9V fast charge status.
	 */
	val.intval = CM_FAST_CHARGE_ENABLE_CMD;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to set second charger current"
			"in 9V fast charge status\n");
		return ret;
	}

	return 0;
}

static int cm_enable_second_charger(struct charger_manager *cm, bool enable)
{

	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!desc->psy_charger_stat[1])
		return 0;

	psy = power_supply_get_by_name(desc->psy_charger_stat[1]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_charger_stat[1]);
		power_supply_put(psy);
		return -ENODEV;
	}

	/*
	 * enable/disable the second charger to start/stop charge
	 */
	val.intval = enable;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to %s second charger \n", enable ? "enable" : "disable");
		return ret;
	}

	return 0;
}

static int cm_adjust_fast_charge_voltage(struct charger_manager *cm, int vol)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(desc->psy_fast_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fast_charger_stat[0]);
		return -ENODEV;
	}

	val.intval = vol;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to adjust fast charger voltage vol = %d\n", vol);
		return ret;
	}

	return 0;
}

static bool cm_is_reach_fchg_threshold(struct charger_manager *cm)
{
	int batt_ocv, batt_uA, fchg_ocv_threshold, thm_cur;
	int cur_jeita_status, target_cur;

	if (get_batt_ocv(cm, &batt_ocv)) {
		dev_err(cm->dev, "get_batt_ocv error.\n");
		return false;
	}

	if (get_ibat_now_uA(cm, &batt_uA)) {
		dev_err(cm->dev, "get_ibat_now_uA error.\n");
		return false;
	}

	target_cur = batt_uA;
	if (cm->desc->jeita_tab_size) {
		cur_jeita_status = cm_manager_get_jeita_status(cm, cm->desc->temperature);
		if (cm->desc->jeita_disabled)
			cur_jeita_status = cm->desc->force_jeita_status;

		target_cur = 0;
		if (cur_jeita_status != cm->desc->jeita_tab_size)
			target_cur = cm->desc->jeita_tab[cur_jeita_status].current_ua;
	}

	fchg_ocv_threshold = CM_FAST_CHARGE_START_VOLTAGE_HTHRESHOLD;
	if (cm->desc->fchg_ocv_threshold)
		fchg_ocv_threshold = cm->desc->fchg_ocv_threshold;

	thm_cur = CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT;
	if (cm->desc->thm_info.thm_adjust_cur > 0)
		thm_cur = cm->desc->thm_info.thm_adjust_cur;

	if (target_cur >= CM_FAST_CHARGE_ENABLE_CURRENT &&
		 thm_cur >= CM_FAST_CHARGE_ENABLE_THERMAL_CURRENT &&
		 batt_ocv > 0 && batt_ocv < fchg_ocv_threshold &&
		 batt_ocv >= CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD)
		return true;
	else if (batt_ocv > 0 && batt_ocv >= CM_FAST_CHARGE_START_VOLTAGE_LTHRESHOLD &&
		 batt_uA > 0 && batt_uA >= CM_FAST_CHARGE_ENABLE_CURRENT)
		return true;

	return false;
}

static int cm_fast_charge_enable_check(struct charger_manager *cm)
{
	int ret, adapter_max_vbus;

	/*
	 * if it occurs emergency event, don't enable fast charge.
	 */
	if (cm->emergency_stop)
		return -EAGAIN;

	if (!cm->desc) {
		dev_err(cm->dev, "cm->desc is a null pointer!!!\n");
		return 0;
	}

	/*
	 * if it don't define cm-fast-chargers in dts,
	 * we think that it don't plan to use fast charge.
	 */
	if (!cm->desc->psy_fast_charger_stat || !cm->desc->psy_fast_charger_stat[0])
		return 0;

	if (!cm->desc->is_fast_charge || cm->desc->enable_fast_charge)
		return 0;

	if (!cm_is_reach_fchg_threshold(cm))
		return 0;

	ret = cm_get_adapter_max_voltage(cm, &adapter_max_vbus);
	if (ret) {
		dev_err(cm->dev, "failed to obtain the adapter max voltage\n");
		return ret;
	}

	if (adapter_max_vbus <= CM_FAST_CHARGE_VOLTAGE_5V) {
		dev_info(cm->dev, "the adapter max vbus : %d\n", adapter_max_vbus);
		return 0;
	}

	ret = cm_set_main_charger_current(cm, CM_FAST_CHARGE_ENABLE_CMD);
	if (ret) {
		/*
		 * if it failed to set fast charge current, reset to DCP setting
		 * first so that the charging current can reach the condition again.
		 */
		cm_set_main_charger_current(cm, CM_FAST_CHARGE_DISABLE_CMD);
		dev_err(cm->dev, "failed to set main charger current\n");
		return ret;
	}

	ret = cm_set_second_charger_current(cm);
	if (ret) {
		cm_set_main_charger_current(cm, CM_FAST_CHARGE_DISABLE_CMD);
		dev_err(cm->dev, "failed to set second charger current\n");
		return ret;
	}

	/*
	 * adjust fast charger output voltage from 5V to 9V
	 */

	if (adapter_max_vbus > CM_FAST_CHARGE_VOLTAGE_9V)
		adapter_max_vbus = CM_FAST_CHARGE_VOLTAGE_9V;

	ret = cm_adjust_fast_charge_voltage(cm, adapter_max_vbus);
	if (ret) {
		cm_set_main_charger_current(cm, CM_FAST_CHARGE_DISABLE_CMD);
		dev_err(cm->dev, "failed to adjust fast charger voltage\n");
		return ret;
	}

	ret = cm_enable_second_charger(cm, true);
	if (ret) {
		cm_set_main_charger_current(cm, CM_FAST_CHARGE_DISABLE_CMD);
		dev_err(cm->dev, "failed to enable second charger\n");
		return ret;
	}

	cm->desc->enable_fast_charge = true;
	/*
	 * adjust over voltage protection in 9V
	 */
	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	return 0;
}

static int cm_fast_charge_disable(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int ret;

	if (!desc->enable_fast_charge)
		return 0;

	/*
	 * if defined psy_charger_stat[1], then disable the second
	 * charger first.
	 */
	ret = cm_enable_second_charger(cm, false);
	if (ret) {
		dev_err(cm->dev, "failed to disable second charger\n");
		return ret;
	}

	/*
	 * adjust fast charger output voltage from 9V to 5V
	 */
	ret = cm_adjust_fast_charge_voltage(cm, CM_FAST_CHARGE_VOLTAGE_5V);
	if (ret) {
		dev_err(cm->dev,
				"failed to adjust 5V fast charger voltage\n");
		return ret;
	}

	ret = cm_set_main_charger_current(cm, CM_FAST_CHARGE_DISABLE_CMD);
	if (ret) {
		dev_err(cm->dev, "failed to set DCP current\n");
		return ret;
	}

	desc->enable_fast_charge = false;
	/*
	 * adjust over voltage protection in 5V
	 */
	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	return 0;
}

static int cm_fast_charge_disable_check(struct charger_manager *cm)
{
	int batt_uV, batt_uA, ret;

	if (!cm->desc->enable_fast_charge)
		return 0;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "failed to get batt uV\n");
		return ret;
	}

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "failed to get batt uA\n");
		return ret;
	}

	if (batt_uV < CM_FAST_CHARGE_DISABLE_BATTERY_VOLTAGE ||
	    batt_uA < CM_FAST_CHARGE_DISABLE_CURRENT)
		cm->desc->fast_charge_disable_count++;
	else
		cm->desc->fast_charge_disable_count = 0;

	if (cm->desc->fast_charge_disable_count < CM_FAST_CHARGE_DISABLE_COUNT)
		return 0;

	cm->desc->fast_charge_disable_count = 0;
	ret = cm_fast_charge_disable(cm);
	if (ret) {
		dev_err(cm->dev, "failed to disable fast charge\n");
		return ret;
	}

	return 0;
}

static int try_fast_charger_enable(struct charger_manager *cm, bool enable)
{
	int err = 0;

	if (cm->desc->fast_charger_type != POWER_SUPPLY_USB_CHARGER_TYPE_PD &&
	    cm->desc->fast_charger_type != POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_1P0)
		return 0;

	if (enable) {
		err = cm_fast_charge_enable_check(cm);
		if (err) {
			dev_err(cm->dev,
				"failed to check fast charge enable\n");
			return err;
		}

		err = cm_fast_charge_disable_check(cm);
		if (err) {
			dev_err(cm->dev,
				"failed to check fast charge disable\n");
			return err;
		}
	} else {
		err = cm_fast_charge_disable(cm);
		if (err) {
			dev_err(cm->dev,
				"failed to disable fast charge\n");
			return err;
		}
	}

	return 0;
}

static int cm_get_ibat_avg(struct charger_manager *cm, int *ibat)
{
	int ret, batt_uA, min, max, i, sum = 0;
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "get bat_uA error.\n");
		return ret;
	}

	if (ir_sts->ibat_index >= CM_IBAT_BUFF_CNT)
		ir_sts->ibat_index = 0;
	ir_sts->ibat_buf[ir_sts->ibat_index++] = batt_uA;

	if (ir_sts->ibat_buf[CM_IBAT_BUFF_CNT - 1] == CM_MAGIC_NUM)
		return -EINVAL;

	min = max = ir_sts->ibat_buf[0];
	for (i = 0; i < CM_IBAT_BUFF_CNT; i++) {
		if (max < ir_sts->ibat_buf[i])
			max = ir_sts->ibat_buf[i];
		if (min > ir_sts->ibat_buf[i])
			min = ir_sts->ibat_buf[i];
		sum += ir_sts->ibat_buf[i];
	}

	sum  = sum - min - max;

	*ibat = DIV_ROUND_CLOSEST(sum, (CM_IBAT_BUFF_CNT - 2));

	if (*ibat < 0)
		*ibat = 0;

	return ret;
}

static void cm_ir_compensation_exit(struct charger_manager *cm)
{
	cm->desc->ir_comp.ibat_buf[CM_IBAT_BUFF_CNT - 1] = CM_MAGIC_NUM;
	cm->desc->ir_comp.ibat_index = 0;
	cm->desc->ir_comp.last_target_cccv = 0;
}

static void cm_ir_compensation_enable(struct charger_manager *cm, bool enable)
{
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;

	if (enable) {
		if (ir_sts->rc && !ir_sts->ir_compensation_en) {
			dev_info(cm->dev, "%s enable ir compensation\n", __func__);
			ir_sts->last_target_cccv = ir_sts->us;
			ir_sts->ir_compensation_en = true;
			queue_delayed_work(system_power_efficient_wq,
					   &cm->ir_compensation_work,
					   CM_IR_COMPENSATION_TIME * HZ);
		}
	} else {
		if (ir_sts->ir_compensation_en) {
			dev_info(cm->dev, "%s stop ir compensation\n", __func__);
			cancel_delayed_work_sync(&cm->ir_compensation_work);
			ir_sts->ir_compensation_en = false;
			cm_ir_compensation_exit(cm);
		}
	}
}

static void cm_ir_compensation(struct charger_manager *cm, enum cm_ir_comp_state state, int *target)
{
	struct cm_ir_compensation *ir_sts = &cm->desc->ir_comp;
	int ibat_avg, target_cccv;

	if (!ir_sts->rc)
		return;

	if (cm_get_ibat_avg(cm, &ibat_avg))
		return;

	target_cccv = ir_sts->us + (ibat_avg / 1000)  * ir_sts->rc;

	if (target_cccv < ir_sts->us_lower_limit)
		target_cccv = ir_sts->us_lower_limit;
	else if (target_cccv > ir_sts->us_upper_limit)
		target_cccv = ir_sts->us_upper_limit;

	*target = target_cccv;

	if ((*target / 1000) == (ir_sts->last_target_cccv / 1000))
		return;

	dev_info(cm->dev, "%s, us = %d, rc = %d, upper_limit = %d, lower_limit = %d, "
		 "target_cccv = %d, ibat_avg = %d, offset = %d\n",
		 __func__, ir_sts->us, ir_sts->rc, ir_sts->us_upper_limit,
		 ir_sts->us_lower_limit, target_cccv, ibat_avg,
		 ir_sts->cp_upper_limit_offset);

	ir_sts->last_target_cccv = *target;
	switch (state) {
	case CM_IR_COMP_STATE_CP:
		target_cccv = min(ir_sts->us_upper_limit,
				  (*target + ir_sts->cp_upper_limit_offset));
	case CM_IR_COMP_STATE_NORMAL:
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					SPRD_VOTE_TYPE_CCCV,
					SPRD_VOTE_TYPE_CCCV_ID_IR,
					SPRD_VOTE_CMD_MAX,
					target_cccv, cm);
		break;
	default:
		break;
	}
}

static void cm_ir_compensation_works(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  ir_compensation_work);

	int target_cccv;

	cm_ir_compensation(cm, CM_IR_COMP_STATE_NORMAL, &target_cccv);
	queue_delayed_work(system_power_efficient_wq,
			   &cm->ir_compensation_work,
			   CM_IR_COMPENSATION_TIME * HZ);
}

static void cm_cp_state_change(struct charger_manager *cm, int state)
{
	cm->desc->cp.cp_state = state;
	dev_dbg(cm->dev, "%s, current cp_state = %d\n", __func__, state);
}

static  bool cm_cp_master_charger_enable(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret;

	if (!cm->desc->psy_cp_stat)
		return false;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[0]);
		return false;
	}

	val.intval = enable;
	pr_err("%s:val=%d\n",__func__,val.intval);
	ret = power_supply_set_property(cp_psy, POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
	power_supply_put(cp_psy);
	if (ret) {
		dev_err(cm->dev, "failed to %s master charge pump, ret = %d\n",
			enable ? "enabel" : "disable", ret);
		return false;
	}

	return true;
}

static void cm_init_cp(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int i, ret = -ENODEV;

	if (!cm->desc->psy_cp_stat)
		return;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!cp_psy) {
			dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = CM_USB_PRESENT_CMD;
		ret = power_supply_set_property(cp_psy,
						POWER_SUPPLY_PROP_PRESENT,
						&val);
		power_supply_put(cp_psy);
		if (ret) {
			dev_err(cm->dev, "fail to init cp[%d], ret = %d\n", i, ret);
			break;
		}
	}
}

static int cm_adjust_fast_charge_current(struct charger_manager *cm, int cur)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(desc->psy_fast_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fast_charger_stat[0]);
		return -ENODEV;
	}

	val.intval = cur;
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to adjust fast ibus = %d\n", cur);
		return ret;
	}

	return 0;
}

static int cm_fast_enable_pps(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	dev_dbg(cm->dev, "%s, pps %s\n", __func__, enable ? "enable" : "disable");
	psy = power_supply_get_by_name(desc->psy_fast_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fast_charger_stat[0]);
		return -ENODEV;
	}

	if (enable)
		val.intval = CM_PPS_CHARGE_ENABLE_CMD;
	else
		val.intval = CM_PPS_CHARGE_DISABLE_CMD;

	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ONLINE, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(cm->dev,
			"failed to disable pps\n");
		return ret;
	}

	return 0;
}

static bool cm_check_primary_charger_enabled(struct charger_manager *cm)
{
	int ret;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *psy;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find primary power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
	power_supply_put(psy);
	if (!ret) {
		if (val.intval)
			enabled = true;
	}

	dev_dbg(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");
	return enabled;
}

static bool cm_check_cp_charger_enabled(struct charger_manager *cm)
{
	int ret;
	bool enabled = false;
	union power_supply_propval val = {0,};
	struct power_supply *cp_psy;

	if (!cm->desc->psy_cp_stat)
		return false;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_cp_stat[0]);
		return false;
	}

	ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
	power_supply_put(cp_psy);
	if (!ret)
		enabled = !!val.intval;

	dev_dbg(cm->dev, "%s: %s\n", __func__, enabled ? "enabled" : "disabled");

	return enabled;
}

static void cm_cp_clear_fault_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "%s\n", __func__);
	cp->cp_fault_event = false;
	cp->flt.bat_ovp_fault = false;
	cp->flt.bat_ocp_fault = false;
	cp->flt.bus_ovp_fault = false;
	cp->flt.bus_ocp_fault = false;
	cp->flt.bat_therm_fault = false;
	cp->flt.bus_therm_fault = false;
	cp->flt.die_therm_fault = false;

	cp->alm.bat_ovp_alarm = false;
	cp->alm.bat_ocp_alarm = false;
	cp->alm.bus_ovp_alarm = false;
	cp->alm.bus_ocp_alarm = false;
	cp->alm.bat_therm_alarm = false;
	cp->alm.bus_therm_alarm = false;
	cp->alm.die_therm_alarm = false;
	cp->alm.bat_ucp_alarm = false;

}

static void cm_check_cp_fault_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (!cm->desc->psy_cp_stat || !cm->desc->cm_check_int)
		return;

	dev_info(cm->dev, "%s\n", __func__);

	cm->desc->cm_check_int = false;
	cp->cp_fault_event = true;

	psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_cp_stat[0]);
		return;
	}

	val.intval = CM_FAULT_HEALTH_CMD;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
	if (!ret) {
		cp->flt.bat_ovp_fault = !!(val.intval & CM_CHARGER_BAT_OVP_FAULT_MASK);
		cp->flt.bat_ocp_fault = !!(val.intval & CM_CHARGER_BAT_OCP_FAULT_MASK);
		cp->flt.bus_ovp_fault = !!(val.intval & CM_CHARGER_BUS_OVP_FAULT_MASK);
		cp->flt.bus_ocp_fault = !!(val.intval & CM_CHARGER_BUS_OCP_FAULT_MASK);
		cp->flt.bat_therm_fault = !!(val.intval & CM_CHARGER_BAT_THERM_FAULT_MASK);
		cp->flt.bus_therm_fault = !!(val.intval & CM_CHARGER_BUS_THERM_FAULT_MASK);
		cp->flt.die_therm_fault = !!(val.intval & CM_CHARGER_DIE_THERM_FAULT_MASK);
		cp->alm.bat_ovp_alarm = !!(val.intval & CM_CHARGER_BAT_OVP_ALARM_MASK);
		cp->alm.bat_ocp_alarm = !!(val.intval & CM_CHARGER_BAT_OCP_ALARM_MASK);
		cp->alm.bus_ovp_alarm = !!(val.intval & CM_CHARGER_BUS_OVP_ALARM_MASK);
		cp->alm.bus_ocp_alarm = !!(val.intval & CM_CHARGER_BUS_OCP_ALARM_MASK);
		cp->alm.bat_therm_alarm = !!(val.intval & CM_CHARGER_BAT_THERM_ALARM_MASK);
		cp->alm.bus_therm_alarm = !!(val.intval & CM_CHARGER_BUS_THERM_ALARM_MASK);
		cp->alm.die_therm_alarm = !!(val.intval & CM_CHARGER_DIE_THERM_ALARM_MASK);
		cp->alm.bat_ucp_alarm = !!(val.intval & CM_CHARGER_BAT_UCP_ALARM_MASK);
	}
}

static void cm_update_cp_charger_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	cp->ibus_uA = 0;
	cp->vbat_uV = 0;
	cp->vbus_uV = 0;
	cp->ibat_uA = 0;

	if (cp->cp_running) {
		if (get_cp_ibus_uA(cm, &cp->ibus_uA)) {
			cp->ibus_uA = 0;
			dev_err(cm->dev, "get ibus current error.\n");
		}

		if (get_cp_vbat_uV(cm, &cp->vbat_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

		if (get_cp_vbus_uV(cm, &cp->vbus_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbus error.\n");
		}

		if (get_cp_ibat_uA(cm, &cp->ibat_uA)) {
			cp->ibat_uA = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

	} else {
		if (get_charger_input_current(cm, &cp->ibus_uA)) {
			cp->ibus_uA = 0;
			dev_err(cm->dev, "get ibus current error.\n");
		}

		if (get_vbat_now_uV(cm, &cp->vbat_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}

		if (get_charger_voltage(cm, &cp->vbus_uV)) {
			cp->vbat_uV = 0;
			dev_err(cm->dev, "get vbus error.\n");
		}


		if (get_ibat_now_uA(cm, &cp->ibat_uA)) {
			cp->ibat_uA = 0;
			dev_err(cm->dev, "get vbatt error.\n");
		}
	}

	dev_dbg(cm->dev, " %s,  %s, batt_uV = %duV, vbus_uV = %duV, batt_uA = %duA, ibus_uA = %duA\n",
	       __func__, (cp->cp_running ? "charge pump" : "Primary charger"),
	       cp->vbat_uV, cp->vbus_uV, cp->ibat_uA, cp->ibus_uA);
}

static void cm_cp_check_vbus_status(struct charger_manager *cm)
{
	struct cm_fault_status *fault = &cm->desc->cp.flt;
	union power_supply_propval val;
	struct power_supply *cp_psy;
	int ret;

	fault->vbus_error_lo = false;
	fault->vbus_error_hi = false;

	if (!cm->desc->psy_cp_stat || !cm->desc->cp.cp_running)
		return;

	cp_psy = power_supply_get_by_name(cm->desc->psy_cp_stat[0]);
	if (!cp_psy) {
		dev_err(cm->dev, "Cannot find charge pump power supply \"%s\"\n",
				cm->desc->psy_cp_stat[0]);
		return;
	}

	val.intval = CM_BUS_ERR_HEALTH_CMD;
	ret = power_supply_get_property(cp_psy, POWER_SUPPLY_PROP_HEALTH, &val);
	power_supply_put(cp_psy);
	if (ret) {
		dev_err(cm->dev, "failed to get vbus status, ret = %d\n", ret);
		return;
	}

	fault->vbus_error_lo = !!(val.intval & CM_CHARGER_BUS_ERR_LO_MASK);
	fault->vbus_error_hi = !!(val.intval & CM_CHARGER_BUS_ERR_HI_MASK);
}

static void cm_check_target_ibus(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int target_ibus;

	target_ibus = cp->cp_max_ibus;

	if (cp->adapter_max_ibus > 0)
		target_ibus = min(target_ibus, cp->adapter_max_ibus);

	if (cm->desc->thm_info.thm_adjust_cur > 0)
		target_ibus = min(target_ibus, cm->desc->thm_info.thm_adjust_cur);

	cp->cp_target_ibus = target_ibus;

	dev_dbg(cm->dev, "%s, adp_max_ibus = %d, cp_max_ibus = %d, thm_cur = %d, target_ibus = %d\n",
	       __func__, cp->adapter_max_ibus, cp->cp_max_ibus,
	       cm->desc->thm_info.thm_adjust_cur, cp->cp_target_ibus);
}

static void cm_check_target_vbus(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	if (cp->adapter_max_vbus > 0)
		cp->cp_target_vbus = min(cp->cp_target_vbus, cp->adapter_max_vbus);

	dev_dbg(cm->dev, "%s, adp_max_vbus = %d, target_vbus = %d\n",
	       __func__, cp->adapter_max_vbus, cp->cp_target_vbus);
}

static int cm_cp_vbat_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int vbat_step = 0, delta_vbat_uV;

	delta_vbat_uV = cp->cp_target_vbat - cp->vbat_uV;

	if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP1)
		vbat_step = CM_CP_VSTEP * 3;
	else if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP2)
		vbat_step = CM_CP_VSTEP * 2;
	else if (cp->vbat_uV > 0 && delta_vbat_uV > CM_CP_VBAT_STEP3)
		vbat_step = CM_CP_VSTEP;
	else if (cp->vbat_uV > 0 && delta_vbat_uV < 0)
		vbat_step = -CM_CP_VSTEP * 2;

	return vbat_step;
}

static int cm_cp_ibat_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int ibat_step = 0, delta_ibat_uA;

	delta_ibat_uA = cp->cp_target_ibat - cp->ibat_uA;

	if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP1)
		ibat_step = CM_CP_VSTEP * 3;
	else if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP2)
		ibat_step = CM_CP_VSTEP * 2;
	else if (cp->ibat_uA > 0 && delta_ibat_uA > CM_CP_IBAT_STEP3)
		ibat_step = CM_CP_VSTEP;
	else if (cp->ibat_uA > 0 && delta_ibat_uA < 0)
		ibat_step = -CM_CP_VSTEP * 2;

	return ibat_step;
}

static int cm_cp_vbus_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int vbus_step = 0, delta_vbus_uV;

	delta_vbus_uV = cp->adapter_max_vbus - cp->vbus_uV;

	if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP1)
		vbus_step = CM_CP_VSTEP * 3;
	else if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP2)
		vbus_step = CM_CP_VSTEP * 2;
	else if (cp->vbus_uV > 0 && delta_vbus_uV > CM_CP_VBUS_STEP3)
		vbus_step = CM_CP_VSTEP;
	else if (cp->vbus_uV > 0 && delta_vbus_uV < 0)
		vbus_step = -CM_CP_VSTEP * 2;

	return vbus_step;
}

static int cm_cp_ibus_step_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int ibus_step = 0, delta_ibus_uA;

	delta_ibus_uA = cp->cp_target_ibus - cp->ibus_uA;

	if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP1)
		ibus_step = CM_CP_VSTEP * 3;
	else if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP2)
		ibus_step = CM_CP_VSTEP * 2;
	else if (cp->ibus_uA > 0 && delta_ibus_uA > CM_CP_IBUS_STEP3)
		ibus_step = CM_CP_VSTEP;
	else if (cp->ibus_uA > 0 && delta_ibus_uA < 0)
		ibus_step = -CM_CP_VSTEP * 2;

	return ibus_step;
}

static bool cm_cp_tune_algo(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	int vbat_step = 0;
	int ibat_step = 0;
	int vbus_step = 0;
	int ibus_step = 0;
	int alarm_step = 0;
	bool is_taper_done = false;

	/* check taper done*/
	if (cp->vbat_uV >= cp->cp_target_vbat - 50000) {
		if (cp->ibat_uA < cp->cp_taper_current) {
			if (cp->cp_taper_trigger_cnt++ > 5) {
				is_taper_done = true;
				return is_taper_done;
			}
		} else {
			cp->cp_taper_trigger_cnt = 0;
		}
	}

	/* check battery voltage*/
	vbat_step = cm_cp_vbat_step_algo(cm);

	/* check battery current*/
	ibat_step = cm_cp_ibat_step_algo(cm);

	/* check bus voltage*/
	vbus_step = cm_cp_vbus_step_algo(cm);

	/* check bus current*/
	cm_check_target_ibus(cm);
	ibus_step = cm_cp_ibus_step_algo(cm);

	/* check alarm status*/
	if (cp->alm.bat_ovp_alarm || cp->alm.bat_ocp_alarm ||
	    cp->alm.bus_ovp_alarm || cp->alm.bus_ocp_alarm ||
	    cp->alm.bat_therm_alarm || cp->alm.bus_therm_alarm ||
	    cp->alm.die_therm_alarm) {
		dev_warn(cm->dev, "%s, bat_ovp_alarm = %d, bat_ocp_alarm = %d, bus_ovp_alarm = %d, "
		       "bus_ocp_alarm = %d, bat_therm_alarm = %d, bus_therm_alarm = %d, "
		       "die_therm_alarm = %d\n", __func__, cp->alm.bat_ovp_alarm,
		       cp->alm.bat_ocp_alarm, cp->alm.bus_ovp_alarm,
		       cp->alm.bus_ocp_alarm, cp->alm.bat_therm_alarm,
		       cp->alm.bus_therm_alarm, cp->alm.die_therm_alarm);
		alarm_step = -CM_CP_VSTEP * 2;
	} else {
		alarm_step = CM_CP_VSTEP * 3;
	}

	cp->cp_target_vbus += min(min(min(min(vbat_step, ibat_step),
					  vbus_step), ibus_step), alarm_step);
	cm_check_target_vbus(cm);

	dev_info(cm->dev, "%s vbatt = %duV, ibatt = %duA, vbus = %duV, ibus = %duA, "
	       "cp_target_vbat = %duV, cp_target_ibat = %duA, cp_target_vbus = %duV, "
	       "cp_target_ibus = %duA, cp_taper_current = %duA, taper_cnt = %d, "
	       "vbat_step = %d, ibat_step = %d, vbus_step = %d, ibus_step = %d, alarm_step = %d, "
	       "adapter_max_vbus = %duV, adapter_max_ibus = %duA, ucp_cnt = %d\n",
	       __func__, cp->vbat_uV, cp->ibat_uA, cp->vbus_uV,
	       cp->ibus_uA, cp->cp_target_vbat, cp->cp_target_ibat,
	       cp->cp_target_vbus, cp->cp_target_ibus, cp->cp_taper_current,
	       cp->cp_taper_trigger_cnt, vbat_step, ibat_step, vbus_step,
	       ibus_step, alarm_step, cp->adapter_max_vbus, cp->adapter_max_ibus,
	       cp->cp_ibat_ucp_cnt);

	return is_taper_done;
}

static bool cm_cp_check_ibat_ucp_status(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	bool status = false;
	bool ibat_ucp_flag = false;

	if (cp->alm.bat_ucp_alarm) {
		dev_warn(cm->dev, "%s, bat_ucp_alarm = %d\n", __func__, cp->alm.bat_ucp_alarm);
		cp->cp_ibat_ucp_cnt++;
		ibat_ucp_flag = true;
	}

	if (!cp->cp_ibat_ucp_cnt)
		return status;

	if (cp->vbat_uV >= cp->cp_target_vbat - 50000) {
		cp->cp_ibat_ucp_cnt = 0;
		return status;
	}

	if (cp->ibat_uA < cp->cp_taper_current && !(ibat_ucp_flag))
		cp->cp_ibat_ucp_cnt++;
	else if (cp->ibat_uA >= cp->cp_taper_current)
		cp->cp_ibat_ucp_cnt = 0;

	if (cp->cp_ibat_ucp_cnt > CM_CP_IBAT_UCP_THRESHOLD)
		status = true;

	return status;
}

static void cm_cp_state_recovery(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	if (is_ext_pwr_online(cm) && cm_is_reach_fchg_threshold(cm)) {
		cm_cp_state_change(cm, CM_CP_STATE_ENTRY);
	} else {
		cm->desc->cp.recovery = false;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	}
}

static void cm_cp_state_entry(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	static int primary_charger_dis_retry;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	cm->desc->cm_check_fault = false;
	cm_fast_enable_pps(cm, false);
	if (cm_fast_enable_pps(cm, true)) {
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		dev_err(cm->dev, "fail to enable pps\n");
		return;
	}

	cm_adjust_fast_charge_voltage(cm, CM_FAST_CHARGE_VOLTAGE_5V);
	cm_cp_master_charger_enable(cm, false);
	cm_primary_charger_enable(cm, false);
	cm_ir_compensation_enable(cm, false);

	if (cm_check_primary_charger_enabled(cm)) {
		if (primary_charger_dis_retry++ > CM_CP_PRIMARY_CHARGER_DIS_TIMEOUT) {
			cm_cp_state_change(cm, CM_CP_STATE_EXIT);
			primary_charger_dis_retry = 0;
		}
		return;
	}

	cm_get_adapter_max_current(cm, &cp->adapter_max_ibus);
	cm_get_adapter_max_voltage(cm, &cp->adapter_max_vbus);

	cm_init_cp(cm);

	cp->recovery = false;
	cm->desc->enable_fast_charge = true;

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	cm_cp_master_charger_enable(cm, true);

	cm->desc->cp.tune_vbus_retry = 0;
	primary_charger_dis_retry = 0;
	cp->cp_ibat_ucp_cnt = 0;

	cp->cp_target_ibus = cp->cp_max_ibus;

	if (cp->vbat_uV <= CM_CP_ACC_VBAT_HTHRESHOLD)
		cp->cp_target_vbus = cp->vbat_uV * 205 / 100 + 10 * CM_CP_VSTEP;
	else
		cp->cp_target_vbus = cp->vbat_uV * 205 / 100 + 2 * CM_CP_VSTEP;


	dev_dbg(cm->dev, "%s, target_ibat = %d, cp_target_vbus = %d\n",
	       __func__, cp->cp_target_ibat, cp->cp_target_vbus);

	cm_check_target_vbus(cm);
	cm_adjust_fast_charge_voltage(cm, cp->cp_target_vbus);
	cp->cp_last_target_vbus = cp->cp_target_vbus;

	cm_check_target_ibus(cm);
	cm_adjust_fast_charge_current(cm, cp->cp_target_ibus);
	cm_cp_state_change(cm, CM_CP_STATE_CHECK_VBUS);
}

static void cm_cp_state_check_vbus(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	if (cp->flt.vbus_error_lo && cp->vbus_uV < cp->vbat_uV * 219 / 100) {
		cp->tune_vbus_retry++;
		cp->cp_target_vbus += CM_CP_VSTEP;
		cm_check_target_vbus(cm);

		if (cm_adjust_fast_charge_voltage(cm, cp->cp_target_vbus))
			cp->cp_target_vbus -= CM_CP_VSTEP;

	} else if (cp->flt.vbus_error_hi && cp->vbus_uV > cp->vbat_uV * 205 / 100) {
		cp->tune_vbus_retry++;
		cp->cp_target_vbus -= CM_CP_VSTEP;
		if (cm_adjust_fast_charge_voltage(cm, cp->cp_target_vbus))
			dev_err(cm->dev, "fail to adjust pps voltage = %duV\n",
				cp->cp_target_vbus);
	} else {
		dev_info(cm->dev, "adapter volt tune ok, retry %d times\n",
			 cp->tune_vbus_retry);
		cm_cp_state_change(cm, CM_CP_STATE_TUNE);

		if (!cm_check_cp_charger_enabled(cm))
			cm_cp_master_charger_enable(cm, true);

		cm->desc->cm_check_fault = true;
		return;
	}

	dev_info(cm->dev, " %s, target_ibat = %duA, cp_target_vbus = %duV, vbus_err_lo = %d, "
	       "vbus_err_hi = %d, retry_time = %d",
	       __func__, cp->cp_target_ibat, cp->cp_target_vbus,
	       cp->flt.vbus_error_lo, cp->flt.vbus_error_hi, cp->tune_vbus_retry);

	if (cp->tune_vbus_retry >= 50) {
		dev_info(cm->dev, "Failed to tune adapter volt into valid range,move to CM_CP_STATE_EXIT\n");
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	}
}

static void cm_cp_state_tune(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	int target_vbat = 0;

	if (!cp->cp_state_tune_log) {
		dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
			 cp->cp_state, cm_cp_state_names[cp->cp_state]);
		cp->cp_state_tune_log = true;
	}

	cm_ir_compensation(cm, CM_IR_COMP_STATE_CP, &target_vbat);
	if (target_vbat > 0)
		cp->cp_target_vbat = target_vbat;

	if (cp->flt.bat_therm_fault || cp->flt.die_therm_fault ||
	    cp->flt.bus_therm_fault) {
		dev_err(cm->dev, "bat_therm_fault = %d, die_therm_fault = %d, exit cp\n",
			 cp->flt.bat_therm_fault, cp->flt.die_therm_fault);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (cp->flt.bat_ocp_fault || cp->flt.bat_ovp_fault ||
		cp->flt.bus_ocp_fault || cp->flt.bus_ovp_fault) {
		dev_err(cm->dev, "bat_ocp_fault = %d, bat_ovp_fault = %d, "
			 "bus_ocp_fault = %d, bus_ovp_fault = %d, exit cp\n",
			 cp->flt.bat_ocp_fault, cp->flt.bat_ovp_fault,
			 cp->flt.bus_ocp_fault, cp->flt.bus_ovp_fault);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (!cm_check_cp_charger_enabled(cm) &&
		   (cp->flt.vbus_error_hi || cp->flt.vbus_error_lo)) {
		dev_err(cm->dev, " %s some error happen, need recovery\n", __func__);
		cp->recovery = true;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);

	} else if (!cm_check_cp_charger_enabled(cm)) {
		dev_err(cm->dev, "%s cp charger is disabled, exit cp\n", __func__);
		cp->recovery = true;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	} else if (cm_cp_check_ibat_ucp_status(cm)) {
		dev_err(cm->dev, "cp_ibat_ucp_cnt =%d, exit cp!\n", cp->cp_ibat_ucp_cnt);
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
	} else {
		dev_info(cm->dev, "cp is ok, fine tune\n");
		if (cm_cp_tune_algo(cm)) {
			dev_info(cm->dev, "taper done, exit cp machine\n");
			cm_cp_state_change(cm, CM_CP_STATE_EXIT);
			cp->recovery = false;
		} else {
			if (cp->cp_last_target_vbus != cp->cp_target_vbus) {
				cm_adjust_fast_charge_voltage(cm, cp->cp_target_vbus);
				cp->cp_last_target_vbus = cp->cp_target_vbus;
				cp->cp_adjust_cnt = 0;
			} else if (cp->cp_adjust_cnt++ > CM_CP_ADJUST_VOLTAGE_THRESHOLD) {
				cm_adjust_fast_charge_voltage(cm, cp->cp_target_vbus);
				cp->cp_adjust_cnt = 0;
			}
		}
	}

	if (cp->cp_fault_event)
		cm_cp_clear_fault_status(cm);
}

static void cm_cp_state_exit(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_info(cm->dev, "cm_cp_state_machine: state %d, %s\n",
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	if (!cm_cp_master_charger_enable(cm, false))
		return;

	/* Hardreset will request 5V/2A or 5V/3A default.
	 * Disbale pps will request sink-pdos PDO_FIXED value.
	 * And PDO_FIXED defined in dts is 5V/2A or 5V/3A, so
	 * we does not need requeset 5V/2A or 5V/3A when exit cp
	 */
	cm_fast_enable_pps(cm, false);

	if (!cp->recovery)
		cp->cp_running = false;

	cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
				   CM_CHARGE_INFO_INPUT_LIMIT |
				   CM_CHARGE_INFO_THERMAL_LIMIT |
				   CM_CHARGE_INFO_JEITA_LIMIT));

	if (!cm->charging_status && !cm->emergency_stop) {
		cm_primary_charger_enable(cm, true);
		cm_ir_compensation_enable(cm, true);
	}

	if (cp->recovery)
		cm_cp_state_change(cm, CM_CP_STATE_RECOVERY);

	cm->desc->cm_check_fault = false;
	cm->desc->enable_fast_charge = false;
	cp->cp_fault_event = false;
	cp->cp_ibat_ucp_cnt = 0;
	cp->cp_state_tune_log = false;

	cm_ir_compensation_exit(cm);
}

static int cm_cp_state_machine(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_dbg(cm->dev, "%s, state %d, %s\n", __func__,
	       cp->cp_state, cm_cp_state_names[cp->cp_state]);

	switch (cp->cp_state) {
	case CM_CP_STATE_RECOVERY:
		cm_cp_state_recovery(cm);
		break;
	case CM_CP_STATE_ENTRY:
		cm_cp_state_entry(cm);
		break;
	case CM_CP_STATE_CHECK_VBUS:
		cm_cp_state_check_vbus(cm);
		break;
	case CM_CP_STATE_TUNE:
		cm_cp_state_tune(cm);
		break;
	case CM_CP_STATE_EXIT:
		cm_cp_state_exit(cm);
		break;
	case CM_CP_STATE_UNKNOWN:
	default:
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		break;
	}

	return 0;
}

static void cm_cp_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
						  struct charger_manager,
						  cp_work);

	cm_update_cp_charger_status(cm);
	cm_cp_check_vbus_status(cm);

	if (cm->desc->cm_check_int && cm->desc->cm_check_fault)
		cm_check_cp_fault_status(cm);

	if (cm->desc->cp.cp_running && !cm_cp_state_machine(cm))
		schedule_delayed_work(&cm->cp_work, msecs_to_jiffies(CM_CP_WORK_TIME_MS));
}

static void cm_check_cp_start_condition(struct charger_manager *cm, bool enable)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	dev_dbg(cm->dev, "%s enable = %d start\n", __func__, enable);

	if (!cm->desc->psy_cp_stat)
		return;

	if (enable) {
		cp->check_cp_threshold = enable;
	} else {
		cp->check_cp_threshold = enable;
		cp->recovery = false;
		cm_cp_state_change(cm, CM_CP_STATE_EXIT);
		if (cp->cp_running) {
			cancel_delayed_work_sync(&cm->cp_work);
			cm_cp_state_machine(cm);
		}
		__pm_relax(&cm->charge_ws);
	}
}

static bool cm_is_need_start_cp(struct charger_manager *cm)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;
	bool need = false;

	if (!cm->desc->psy_cp_stat)
		return false;

	cm_charger_is_support_fchg(cm);
	dev_info(cm->dev, "%s, check_cp_threshold = %d, pps_running = %d, fast_charger_type = %d\n",
		 __func__, cp->check_cp_threshold, cp->cp_running, cm->desc->fast_charger_type);
	if (cp->check_cp_threshold && !cp->cp_running &&
	   cm_is_reach_fchg_threshold(cm) && cm->charger_enabled &&
	   cm->desc->fast_charger_type == POWER_SUPPLY_USB_CHARGER_TYPE_PD_PPS)
		need = true;

	return need;
}

static void cm_start_cp_state_machine(struct charger_manager *cm, bool start)
{
	struct cm_charge_pump_status *cp = &cm->desc->cp;

	if (!cp->cp_running && start) {
		dev_info(cm->dev, "%s, reach pps threshold\n", __func__);
		cp->cp_running = start;
		cm->desc->cm_check_fault = false;
		__pm_stay_awake(&cm->charge_ws);
		cm_cp_state_change(cm, CM_CP_STATE_ENTRY);
		schedule_delayed_work(&cm->cp_work, 0);
	}
}

static int try_charger_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		pr_err("%s:val=%d\n",__func__,val.intval);
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
		power_supply_put(psy);
		if (err)
			return err;
		if (desc->psy_charger_stat[1])
			break;
	}

	return 0;
}

static int try_wireless_charger_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	if (!cm->desc->psy_wl_charger_stat)
		return 0;

	for (i = 0; desc->psy_wl_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_wl_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_wl_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

static int try_wireless_cp_converter_enable_by_psy(struct charger_manager *cm, bool enable)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	struct power_supply *psy;
	int i, err;

	if (!cm->desc->psy_cp_converter_stat)
		return 0;

	for (i = 0; desc->psy_cp_converter_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_cp_converter_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		val.intval = enable;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGE_ENABLED, &val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

static int cm_set_primary_charge_wirless_type(struct charger_manager *cm, bool enable)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int ret = 0;

	psy = power_supply_get_by_name(cm->desc->psy_charger_stat[0]);
	if (!psy) {
		dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
			cm->desc->psy_charger_stat[0]);
		return false;
	}

	if (enable)
		val.intval = cm->desc->charger_type;
	else
		val.intval = 0;

	dev_info(cm->dev, "set wirless type = %d\n", val.intval);
	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_WIRELESS_TYPE, &val);
	power_supply_put(psy);

	return ret;
}

static void try_wireless_charger_enable(struct charger_manager *cm, bool enable)
{
	int ret = 0;

	ret = cm_set_primary_charge_wirless_type(cm, enable);
	if (ret) {
		dev_err(cm->dev, "set wl type to primary charge fail, ret = %d\n", ret);
		return;
	}

	ret = try_wireless_charger_enable_by_psy(cm, enable);
	if (ret) {
		dev_err(cm->dev, "enable wl charger fail, ret = %d\n", ret);
		return;
	}

	ret = try_wireless_cp_converter_enable_by_psy(cm, enable);
	if (ret)
		dev_err(cm->dev, "enable wl charger fail, ret = %d\n", ret);
}
/**
 * try_charger_enable - Enable/Disable chargers altogether
 * @cm: the Charger Manager representing the battery.
 * @enable: true: enable / false: disable
 *
 * Note that Charger Manager keeps the charger enabled regardless whether
 * the charger is charging or not (because battery is full or no external
 * power source exists) except when CM needs to disable chargers forcibly
 * bacause of emergency causes; when the battery is overheated or too cold.
 */
static int try_charger_enable(struct charger_manager *cm, bool enable)
{
	int err = 0;
	pr_err("%s:%d\n",__func__,enable);
	try_fast_charger_enable(cm, enable);

	/* Ignore if it's redundant command */
	if (enable == cm->charger_enabled)
		return 0;

	if (enable) {
		if (cm->emergency_stop)
			return -EAGAIN;

		/*
		 * Enable charge is permitted in calibration mode
		 * even if use fake battery.
		 * So it will not return in calibration mode.
		 */
		if (!is_batt_present(cm) && !allow_charger_enable)
			return 0;
		/*
		 * Save start time of charging to limit
		 * maximum possible charging time.
		 */
		cm->charging_start_time = ktime_to_ms(ktime_get());
		cm->charging_end_time = 0;

		err = try_charger_enable_by_psy(cm, enable);
		cm_ir_compensation_enable(cm, enable);
		cm_check_cp_start_condition(cm, enable);
	} else {
		/*
		 * Save end time of charging to maintain fully charged state
		 * of battery after full-batt.
		 */
		cm->charging_start_time = 0;
		cm->charging_end_time = ktime_to_ms(ktime_get());
		cm_check_cp_start_condition(cm, enable);
		cm_ir_compensation_enable(cm, enable);
		err = try_charger_enable_by_psy(cm, enable);
	}

	if (!err) {
		cm->charger_enabled = enable;
		power_supply_changed(cm->charger_psy);
	}
	return err;
}

/**
 * try_charger_restart - Restart charging.
 * @cm: the Charger Manager representing the battery.
 *
 * Restart charging by turning off and on the charger.
 */
static int try_charger_restart(struct charger_manager *cm)
{
	int err;

	if (cm->emergency_stop)
		return -EAGAIN;

	err = try_charger_enable(cm, false);
	if (err)
		return err;

	return try_charger_enable(cm, true);
}

/**
 * fullbatt_vchk - Check voltage drop some times after "FULL" event.
 * @work: the work_struct appointing the function
 *
 * If a user has designated "fullbatt_vchkdrop_ms/uV" values with
 * charger_desc, Charger Manager checks voltage drop after the battery
 * "FULL" event. It checks whether the voltage has dropped more than
 * fullbatt_vchkdrop_uV by calling this function after fullbatt_vchkrop_ms.
 */
static void fullbatt_vchk(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
			struct charger_manager, fullbatt_vchk_work);
	struct charger_desc *desc = cm->desc;
	int batt_ocv, err, diff;

	/* remove the appointment for fullbatt_vchk */
	cm->fullbatt_vchk_jiffies_at = 0;

	if (!desc->fullbatt_vchkdrop_uV || !desc->fullbatt_vchkdrop_ms)
		return;

	err = get_batt_ocv(cm, &batt_ocv);
	if (err) {
		dev_err(cm->dev, "%s: get_batt_ocV error(%d)\n", __func__, err);
		return;
	}

	diff = desc->fullbatt_uV - batt_ocv;
	if (diff < 0)
		return;

	dev_info(cm->dev, "VBATT dropped %duV after full-batt\n", diff);

	if (diff >= desc->fullbatt_vchkdrop_uV)
		try_charger_restart(cm);

}

/**
 * check_charging_duration - Monitor charging/discharging duration
 * @cm: the Charger Manager representing the battery.
 *
 * If whole charging duration exceed 'charging_max_duration_ms',
 * cm stop charging to prevent overcharge/overheat. If discharging
 * duration exceed 'discharging _max_duration_ms', charger cable is
 * attached, after full-batt, cm start charging to maintain fully
 * charged state for battery.
 */
static void check_charging_duration(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	u64 curr = ktime_to_ms(ktime_get());
	u64 duration;
	int ret = false;

	if (!desc->charging_max_duration_ms &&
			!desc->discharging_max_duration_ms)
		return;

	if (cm->charging_status != 0 &&
	    !(cm->charging_status & CM_CHARGE_DURATION_ABNORMAL))
		return;

	if (cm->charger_enabled) {
		int batt_ocv, diff;

		ret = get_batt_ocv(cm, &batt_ocv);
		if (ret) {
			dev_err(cm->dev, "failed to get battery OCV\n");
			return;
		}

		diff = desc->fullbatt_uV - batt_ocv;
		duration = curr - cm->charging_start_time;

		if (duration > desc->charging_max_duration_ms &&
		    diff < desc->fullbatt_vchkdrop_uV) {
			dev_info(cm->dev, "Charging duration exceed %ums\n",
				 desc->charging_max_duration_ms);
			cm->charging_status |= CM_CHARGE_DURATION_ABNORMAL;
			try_charger_enable(cm, false);
		}
	} else if (!cm->charger_enabled  && (cm->charging_status & CM_CHARGE_DURATION_ABNORMAL)) {
		duration = curr - cm->charging_end_time;

		if (duration > desc->discharging_max_duration_ms) {
			dev_info(cm->dev, "Discharging duration exceed %ums\n",
				 desc->discharging_max_duration_ms);
			cm->charging_status &= ~CM_CHARGE_DURATION_ABNORMAL;
		}
	}

	return;
}

static int cm_get_battery_temperature_by_psy(struct charger_manager *cm, int *temp)
{
	struct power_supply *fuel_gauge;
	int ret;
	int64_t temp_val;

	fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_TEMP,
					(union power_supply_propval *)&temp_val);
	power_supply_put(fuel_gauge);

	*temp = (int)temp_val;
	return ret;
}

static int cm_get_battery_temperature(struct charger_manager *cm, int *temp)
{
	int ret = 0;

	if (!cm->desc->measure_battery_temp)
		return -ENODEV;

#ifdef CONFIG_THERMAL
	if (cm->tzd_batt) {
		ret = thermal_zone_get_temp(cm->tzd_batt, temp);
		if (!ret)
			/* Calibrate temperature unit */
			*temp /= 100;
	} else
#endif
	{
		/* if-else continued from CONFIG_THERMAL */
		*temp = cm->desc->temperature;
	}

	return ret;
}

static int cm_check_thermal_status(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	int temp, upper_limit, lower_limit;
	int ret = 0;

	ret = cm_get_battery_temperature(cm, &temp);
	if (ret) {
		/* FIXME:
		 * No information of battery temperature might
		 * occur hazadous result. We have to handle it
		 * depending on battery type.
		 */
		dev_err(cm->dev, "Failed to get battery temperature\n");
		return 0;
	}

	upper_limit = desc->temp_max;
	lower_limit = desc->temp_min;

	if (cm->emergency_stop) {
		upper_limit -= desc->temp_diff;
		lower_limit += desc->temp_diff;
	}

	if (temp > upper_limit)
		ret = CM_EVENT_BATT_OVERHEAT;
	else if (temp < lower_limit)
		ret = CM_EVENT_BATT_COLD;

	cm->emergency_stop = ret;

	return ret;
}

static void cm_check_charge_voltage(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *fuel_gauge;
	union power_supply_propval val;
	int ret, charge_vol;

	if (!desc->charge_voltage_max || !desc->charge_voltage_drop)
		return;

	if (cm->charging_status != 0 &&
	    !(cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL))
		return;

	fuel_gauge = power_supply_get_by_name(desc->psy_fuel_gauge);
	if (!fuel_gauge)
		return;

	ret = power_supply_get_property(fuel_gauge,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(fuel_gauge);
	if (ret)
		return;

	charge_vol = val.intval;

	if (cm->charger_enabled && charge_vol > desc->charge_voltage_max) {
		dev_info(cm->dev, "Charging voltage is larger than %d\n",
			 desc->charge_voltage_max);
		cm->charging_status |= CM_CHARGE_VOLTAGE_ABNORMAL;
		try_charger_enable(cm, false);
	} else if (!cm->charger_enabled &&
		   charge_vol <= (desc->charge_voltage_max - desc->charge_voltage_drop) &&
		   (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL)) {
		dev_info(cm->dev, "Charging voltage is less than %d, recharging\n",
			 desc->charge_voltage_max - desc->charge_voltage_drop);
		cm->charging_status &= ~CM_CHARGE_VOLTAGE_ABNORMAL;
	}
}

static void cm_check_charge_health(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int health = POWER_SUPPLY_HEALTH_UNKNOWN;
	int ret, i;

	if (cm->charging_status != 0 &&
	    !(cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL))
		return;

	for (i = 0; desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
		power_supply_put(psy);
		if (ret)
			return;
		health = val.intval;
	}

	if (health == POWER_SUPPLY_HEALTH_UNKNOWN)
		return;

	if (cm->charger_enabled && health != POWER_SUPPLY_HEALTH_GOOD) {
		dev_info(cm->dev, "Charging health is not good\n");
		cm->charging_status |= CM_CHARGE_HEALTH_ABNORMAL;
		try_charger_enable(cm, false);
	} else if (!cm->charger_enabled && health == POWER_SUPPLY_HEALTH_GOOD &&
		   (cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL)) {
		dev_info(cm->dev, "Charging health is recover good\n");
		cm->charging_status &= ~CM_CHARGE_HEALTH_ABNORMAL;
	}
}

static int cm_feed_watchdog(struct charger_manager *cm)
{
	union power_supply_propval val;
	struct power_supply *psy;
	int err, i;

	if (!cm->desc->wdt_interval)
		return 0;

	for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_charger_stat[i]);

			continue;
		}
		val.intval = cm->desc->wdt_interval;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_FEED_WATCHDOG, &val);
		power_supply_put(psy);
		if (err)
			return err;

	}

	if (!cm->desc->cp.cp_running)
		return 0;

	for (i = 0; cm->desc->psy_cp_stat[i]; i++) {
		psy = power_supply_get_by_name(cm->desc->psy_cp_stat[i]);
		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				cm->desc->psy_cp_stat[i]);
			continue;
		}

		val.intval = cm->desc->wdt_interval;
		err = power_supply_set_property(psy, POWER_SUPPLY_PROP_FEED_WATCHDOG, &val);
		power_supply_put(psy);
		if (err)
			return err;
	}

	return 0;
}

static bool cm_manager_adjust_current(struct charger_manager *cm, int jeita_status)
{
	struct charger_desc *desc = cm->desc;
	int term_volt, target_cur;

	if (cm->charging_status != 0 &&
	    !(cm->charging_status & (CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD)))
		return true;

	if (jeita_status > desc->jeita_tab_size)
		jeita_status = desc->jeita_tab_size;

	if (jeita_status == 0 || jeita_status == desc->jeita_tab_size) {
		dev_warn(cm->dev,
			 "stop charging due to battery overheat or cold\n");
		try_charger_enable(cm, false);

		if (jeita_status == 0) {
			cm->charging_status &= ~CM_CHARGE_TEMP_OVERHEAT;
			cm->charging_status |= CM_CHARGE_TEMP_COLD;
		} else {
			cm->charging_status &= ~CM_CHARGE_TEMP_COLD;
			cm->charging_status |= CM_CHARGE_TEMP_OVERHEAT;
		}
		return false;
	}

	term_volt = desc->jeita_tab[jeita_status].term_volt;
	target_cur = desc->jeita_tab[jeita_status].current_ua;

	cm->desc->ir_comp.us = term_volt;
	cm->desc->ir_comp.us_lower_limit = term_volt;

	if (cm->desc->cp.cp_running && !cm_check_primary_charger_enabled(cm)) {
		dev_info(cm->dev, "cp target terminate voltage = %d, target current = %d\n",
			 term_volt, target_cur);
		cm->desc->cp.cp_target_ibat = target_cur;
		goto exit;
	}

	dev_info(cm->dev, "target terminate voltage = %d, target current = %d\n",
		 term_volt, target_cur);

	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_IBAT,
				 SPRD_VOTE_TYPE_IBAT_ID_JEITA,
				 SPRD_VOTE_CMD_MIN,
				 target_cur, cm);
	cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
				 SPRD_VOTE_TYPE_CCCV,
				 SPRD_VOTE_TYPE_CCCV_ID_JEITA,
				 SPRD_VOTE_CMD_MIN,
				 term_volt, cm);

exit:
	cm->charging_status &= ~(CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD);
	return true;
}

static void cm_jeita_temp_goes_down(struct charger_desc *desc, int status,
				    int recovery_status, int *jeita_status)
{
	if (recovery_status == desc->jeita_tab_size) {
		if (*jeita_status >= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (desc->jeita_tab[recovery_status].temp > desc->jeita_tab[recovery_status].recovery_temp) {
		if (*jeita_status >= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (*jeita_status >= status)
		*jeita_status = status;
}

static void cm_jeita_temp_goes_up(struct charger_desc *desc, int status,
				  int recovery_status, int *jeita_status)
{
	if (recovery_status == desc->jeita_tab_size) {
		if (*jeita_status <= status)
			*jeita_status = status;
		return;
	}

	if (desc->jeita_tab[recovery_status].temp < desc->jeita_tab[recovery_status].recovery_temp) {
		if (*jeita_status <= recovery_status)
			*jeita_status = recovery_status;
		return;
	}

	if (*jeita_status <= status)
		*jeita_status = status;
}

static int cm_manager_get_jeita_status(struct charger_manager *cm, int cur_temp)
{
	struct charger_desc *desc = cm->desc;
	static int jeita_status, last_temp = -200;
	int i, temp_status, recovery_temp_status = -1;

	for (i = desc->jeita_tab_size - 1; i >= 0; i--) {
		if ((cur_temp >= desc->jeita_tab[i].temp && i > 0) ||
		    (cur_temp > desc->jeita_tab[i].temp && i == 0)) {
			break;
		}
	}

	temp_status = i + 1;

	if (temp_status == desc->jeita_tab_size) {
		jeita_status = desc->jeita_tab_size;
		recovery_temp_status = desc->jeita_tab_size;
		goto out;
	} else if (temp_status == 0) {
		jeita_status = 0;
		recovery_temp_status = 0;
		goto out;
	}

	for (i = desc->jeita_tab_size - 1; i >= 0; i--) {
		if ((cur_temp >= desc->jeita_tab[i].recovery_temp && i > 0) ||
		    (cur_temp > desc->jeita_tab[i].recovery_temp && i == 0)) {
			break;
		}
	}

	recovery_temp_status = i + 1;

	/* temperature goes down */
	if (last_temp > cur_temp)
		cm_jeita_temp_goes_down(desc, temp_status, recovery_temp_status, &jeita_status);
	/* temperature goes up */
	else
		cm_jeita_temp_goes_up(desc, temp_status, recovery_temp_status, &jeita_status);

out:
	last_temp = cur_temp;
	dev_info(cm->dev, "%s: jeita status:(%d) %d %d, temperature:%d, jeita_size:%d\n",
		 __func__, jeita_status, temp_status, recovery_temp_status,
		 cur_temp, desc->jeita_tab_size);

	return jeita_status;
}

static int cm_manager_jeita_current_monitor(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	static int last_jeita_status = -1, temp_up_trigger, temp_down_trigger;
	int cur_jeita_status;
	static bool is_normal = true;

	if (!desc->jeita_tab_size)
		return 0;

	if (!is_ext_pwr_online(cm)) {
		if (last_jeita_status != -1)
			last_jeita_status = -1;

		return 0;
	}

	if (desc->jeita_disabled) {
		if (last_jeita_status != cm->desc->force_jeita_status) {
			dev_info(cm->dev, "Disable jeita and force jeita state to force_jeita_status\n");
			last_jeita_status = cm->desc->force_jeita_status;
			desc->thm_info.thm_adjust_cur = -EINVAL;
			cm_manager_adjust_current(cm, last_jeita_status);
		}

		return 0;
	}

	cur_jeita_status = cm_manager_get_jeita_status(cm, desc->temperature);

	dev_info(cm->dev, "current-last jeita status: %d-%d, current temperature: %d\n",
		 cur_jeita_status, last_jeita_status, desc->temperature);

	/*
	 * We should give a initial jeita status with adjusting the charging
	 * current when pluging in the cabel.
	 */
	if (last_jeita_status == -1) {
		is_normal = cm_manager_adjust_current(cm, cur_jeita_status);
		last_jeita_status = cur_jeita_status;
		goto out;
	}

	if (cur_jeita_status > last_jeita_status) {
		temp_down_trigger = 0;

		if (++temp_up_trigger > 2) {
			is_normal = cm_manager_adjust_current(cm,
							      cur_jeita_status);
			last_jeita_status = cur_jeita_status;
		}
	} else if (cur_jeita_status < last_jeita_status) {
		temp_up_trigger = 0;

		if (++temp_down_trigger > 2) {
			is_normal = cm_manager_adjust_current(cm,
							      cur_jeita_status);
			last_jeita_status = cur_jeita_status;
		}
	} else {
		temp_up_trigger = 0;
		temp_down_trigger = 0;
	}

out:
	if (!is_normal)
		return -EAGAIN;

	return 0;
}

/**
 * cm_use_typec_charger_type_polling - Polling charger type if use typec extcon.
 * @cm: the Charger Manager representing the battery.
 */
static int cm_use_typec_charger_type_polling(struct charger_manager *cm)
{
	int ret;
	u32 type;

	if (cm->desc->is_fast_charge)
		return 0;

	if (cm->desc->charger_type != POWER_SUPPLY_USB_TYPE_UNKNOWN)
		return 0;

	ret = get_usb_charger_type(cm, &type);
	if (!ret && type != POWER_SUPPLY_USB_TYPE_UNKNOWN)
		cm->desc->charger_type_cnt++;

	if (cm->desc->charger_type_cnt > 1) {
		dev_info(cm->dev, "%s: update charger type:%d\n", __func__, type);
		cm->desc->charger_type = type;
		cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT |
					   CM_CHARGE_INFO_JEITA_LIMIT));
	}

	return ret;
}

/**
 * cm_get_target_status - Check current status and get next target status.
 * @cm: the Charger Manager representing the battery.
 */
static int cm_get_target_status(struct charger_manager *cm)
{
	int ret;

	/*
	 * Adjust the charging current according to current battery
	 * temperature jeita table.
	 */
	ret = cm_manager_jeita_current_monitor(cm);
	if (ret)
		dev_warn(cm->dev, "Errors orrurs when adjusting charging current\n");

	if (!is_ext_pwr_online(cm) || (!is_batt_present(cm) && !allow_charger_enable))
		return POWER_SUPPLY_STATUS_DISCHARGING;

	if (cm_check_thermal_status(cm))
		return POWER_SUPPLY_STATUS_NOT_CHARGING;

	if (cm->charging_status & (CM_CHARGE_TEMP_OVERHEAT | CM_CHARGE_TEMP_COLD)) {
		dev_warn(cm->dev, "battery overheat or cold is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	cm_check_charge_health(cm);
	if (cm->charging_status & CM_CHARGE_HEALTH_ABNORMAL) {
		dev_warn(cm->dev, "Charging health is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	cm_check_charge_voltage(cm);
	if (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL) {
		dev_warn(cm->dev, "Charging voltage is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	check_charging_duration(cm);
	if (cm->charging_status & CM_CHARGE_DURATION_ABNORMAL) {
		dev_warn(cm->dev, "Charging duration is still abnormal\n");
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	if (is_full_charged(cm))
		return POWER_SUPPLY_STATUS_FULL;
	/* Charging is allowed. */
	return POWER_SUPPLY_STATUS_CHARGING;
}

/**
 * _cm_monitor - Monitor the temperature and return true for exceptions.
 * @cm: the Charger Manager representing the battery.
 *
 * Returns true if there is an event to notify for the battery.
 * (True if the status of "emergency_stop" changes)
 */
static bool _cm_monitor(struct charger_manager *cm)
{
	int ret, i;
	static int last_target = -1;

	/* Feed the charger watchdog if necessary */
	ret = cm_feed_watchdog(cm);
	if (ret) {
		dev_warn(cm->dev, "Failed to feed charger watchdog\n");
		last_target = -1;
		return false;
	}

	for (i = 0; i < cm->desc->num_charger_regulators; i++) {
		if (cm->desc->charger_regulators[i].externally_control) {
			dev_info(cm->dev, "Charger has been controlled externally, so no need monitoring\n");
			last_target = -1;
			return false;
		}
	}

	cm->battery_status = cm_get_target_status(cm);

	if (cm->battery_status == POWER_SUPPLY_STATUS_CHARGING) {
		cm->emergency_stop = 0;
		cm->charging_status = 0;
		try_charger_enable(cm, true);
		cm_use_typec_charger_type_polling(cm);

		if (!cm->desc->cp.cp_running && !cm_check_primary_charger_enabled(cm)
		    && !cm->desc->force_set_full) {
			dev_info(cm->dev, "%s, primary charger does not enable,enable it\n", __func__);
			cm_primary_charger_enable(cm, true);
		}

		if (cm_is_need_start_cp(cm)) {
			dev_info(cm->dev, "%s, reach pps threshold\n", __func__);
			cm_start_cp_state_machine(cm, true);
		}
	} else {
		try_charger_enable(cm, false);
	}

	if (last_target != cm->battery_status) {
		last_target = cm->battery_status;
		power_supply_changed(cm->charger_psy);
	}

	dev_info(cm->dev, "target %d, charging_status %d\n", cm->battery_status, cm->charging_status);
	return (cm->battery_status == POWER_SUPPLY_STATUS_NOT_CHARGING);
}

/**
 * cm_monitor - Monitor every battery.
 *
 * Returns true if there is an event to notify from any of the batteries.
 * (True if the status of "emergency_stop" changes)
 */
static bool cm_monitor(void)
{
	bool stop = false;
	struct charger_manager *cm;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (_cm_monitor(cm))
			stop = true;
	}

	mutex_unlock(&cm_list_mtx);

	return stop;
}

/**
 * _setup_polling - Setup the next instance of polling.
 * @work: work_struct of the function _setup_polling.
 */
static void _setup_polling(struct work_struct *work)
{
	unsigned long min = ULONG_MAX;
	struct charger_manager *cm;
	bool keep_polling = false;
	unsigned long _next_polling;

	mutex_lock(&cm_list_mtx);

	list_for_each_entry(cm, &cm_list, entry) {
		if (is_polling_required(cm) && cm->desc->polling_interval_ms) {
			keep_polling = true;

			if (min > cm->desc->polling_interval_ms)
				min = cm->desc->polling_interval_ms;
		}
	}

	polling_jiffy = msecs_to_jiffies(min);
	if (polling_jiffy <= CM_JIFFIES_SMALL)
		polling_jiffy = CM_JIFFIES_SMALL + 1;

	if (!keep_polling)
		polling_jiffy = ULONG_MAX;
	if (polling_jiffy == ULONG_MAX)
		goto out;

	WARN(cm_wq == NULL, "charger-manager: workqueue not initialized"
			    ". try it later. %s\n", __func__);

	/*
	 * Use mod_delayed_work() iff the next polling interval should
	 * occur before the currently scheduled one.  If @cm_monitor_work
	 * isn't active, the end result is the same, so no need to worry
	 * about stale @next_polling.
	 */
	_next_polling = jiffies + polling_jiffy;

	if (time_before(_next_polling, next_polling)) {
		mod_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy);
		next_polling = _next_polling;
	} else {
		if (queue_delayed_work(cm_wq, &cm_monitor_work, polling_jiffy))
			next_polling = _next_polling;
	}
out:
	mutex_unlock(&cm_list_mtx);
}
static DECLARE_WORK(setup_polling, _setup_polling);

/**
 * cm_monitor_poller - The Monitor / Poller.
 * @work: work_struct of the function cm_monitor_poller
 *
 * During non-suspended state, cm_monitor_poller is used to poll and monitor
 * the batteries.
 */
static void cm_monitor_poller(struct work_struct *work)
{
	cm_monitor();
	schedule_work(&setup_polling);
}

/**
 * fullbatt_handler - Event handler for CM_EVENT_BATT_FULL
 * @cm: the Charger Manager representing the battery.
 */
static void fullbatt_handler(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;

	if (!desc->fullbatt_vchkdrop_uV || !desc->fullbatt_vchkdrop_ms)
		goto out;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	mod_delayed_work(cm_wq, &cm->fullbatt_vchk_work,
			 msecs_to_jiffies(desc->fullbatt_vchkdrop_ms));
	cm->fullbatt_vchk_jiffies_at = jiffies + msecs_to_jiffies(
				       desc->fullbatt_vchkdrop_ms);

	if (cm->fullbatt_vchk_jiffies_at == 0)
		cm->fullbatt_vchk_jiffies_at = 1;

out:
	dev_info(cm->dev, "EVENT_HANDLE: Battery Fully Charged\n");
}

/**
 * battout_handler - Event handler for CM_EVENT_BATT_OUT
 * @cm: the Charger Manager representing the battery.
 */
static void battout_handler(struct charger_manager *cm)
{
	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	if (!is_batt_present(cm)) {
		dev_emerg(cm->dev, "Battery Pulled Out!\n");
		try_charger_enable(cm, false);
	} else {
		dev_emerg(cm->dev, "Battery Pulled in!\n");

		if (cm->charging_status) {
			dev_emerg(cm->dev, "Charger status abnormal, stop charge!\n");
			try_charger_enable(cm, false);
		} else {
			try_charger_enable(cm, true);
		}
	}
}

static bool cm_charger_is_support_fchg(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, i;

	if (!desc->psy_fast_charger_stat)
		return false;

	for (i = 0; desc->psy_fast_charger_stat[i]; i++) {
		psy = power_supply_get_by_name(desc->psy_fast_charger_stat[i]);

		if (!psy) {
			dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
				desc->psy_fast_charger_stat[i]);
			continue;
		}

		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_USB_TYPE, &val);
		power_supply_put(psy);
		if (!ret) {
			if (val.intval == POWER_SUPPLY_USB_TYPE_SFCP_1P0 ||
			    val.intval == POWER_SUPPLY_USB_TYPE_PD ||
			    val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS) {
				mutex_lock(&cm->desc->charger_type_mtx);
				desc->is_fast_charge = true;
				if (!desc->psy_cp_stat &&
				    val.intval == POWER_SUPPLY_USB_TYPE_PD_PPS)
					val.intval = POWER_SUPPLY_USB_TYPE_PD;
				cm_get_charger_type(cm, &val.intval);
				desc->fast_charger_type = val.intval;
				desc->charger_type = val.intval;
				mutex_unlock(&cm->desc->charger_type_mtx);
				return true;
			} else {
				return false;
			}
		}
	}

	return false;
}

static void cm_charger_int_handler(struct charger_manager *cm)
{
	dev_info(cm->dev, "%s\n", __func__);
	cm->desc->cm_check_int = true;
}


/**
 * fast_charge_handler - Event handler for CM_EVENT_FAST_CHARGE
 * @cm: the Charger Manager representing the battery.
 */
static void fast_charge_handler(struct charger_manager *cm)
{
	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	cm_charger_is_support_fchg(cm);

	dev_info(cm->dev, "%s fast_charger_type = %d, cp_running = %d, charger_enabled = %d \n",
		 __func__, cm->desc->fast_charger_type,
		 cm->desc->cp.cp_running, cm->charger_enabled);

	if (!is_ext_pwr_online(cm))
		return;

	if (cm->desc->is_fast_charge && !cm->desc->enable_fast_charge)
		cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT));

	/*
	 * Once the fast charge is identified, it is necessary to open
	 * the charge in the first time to avoid the fast charge to boost
	 * the voltage in the next charging cycle, especially the SFCP
	 * fast charge.
	 */
	if (cm->desc->fast_charger_type == POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_1P0 &&
	    cm->charger_enabled)
		_cm_monitor(cm);

	if (cm->desc->fast_charger_type == POWER_SUPPLY_USB_CHARGER_TYPE_PD_PPS &&
	    !cm->desc->cp.cp_running && cm->charger_enabled) {
		cm_check_cp_start_condition(cm, true);
		schedule_delayed_work(&cm_monitor_work, 0);
	}
}

/**
 * misc_event_handler - Handler for other evnets
 * @cm: the Charger Manager representing the battery.
 * @type: the Charger Manager representing the battery.
 */
static void misc_event_handler(struct charger_manager *cm, enum cm_event_types type)
{
	int ret;

	if (cm_suspended)
		device_set_wakeup_capable(cm->dev, true);

	if (is_ext_pwr_online(cm)) {
		if (is_ext_usb_pwr_online(cm) && type == CM_EVENT_WL_CHG_START_STOP) {
			dev_warn(cm->dev, "usb charging, does not need start wl charge\n");
			return;
		} else if (is_ext_usb_pwr_online(cm)) {
			if (cm->desc->wl_charge_en) {
				try_wireless_charger_enable(cm, false);
				try_charger_enable(cm, false);
				cm->desc->wl_charge_en = false;
			}

			if (!cm->desc->is_fast_charge) {
				ret = get_usb_charger_type(cm, &cm->desc->charger_type);
				if (ret)
					dev_warn(cm->dev, "Fail to get usb charger type, ret = %d", ret);
			}

			cm->desc->usb_charge_en = true;
		} else {
			if (cm->desc->usb_charge_en) {
				try_charger_enable(cm, false);
				cm->desc->is_fast_charge = false;
				cm->desc->enable_fast_charge = false;
				cm->desc->fast_charge_disable_count = 0;
				cm->desc->cp.cp_running = false;
				cm->desc->fast_charger_type = 0;
				cm->desc->cp.cp_target_vbus = 0;
				cm->desc->usb_charge_en = false;
				cm->desc->charger_type = 0;
			}

			ret = get_wireless_charger_type(cm, &cm->desc->charger_type);
			if (ret)
				dev_warn(cm->dev, "Fail to get wl charger type, ret = %d\n", ret);

			try_wireless_charger_enable(cm, true);
			cm->desc->wl_charge_en = true;
		}

		cm_update_charge_info(cm, (CM_CHARGE_INFO_CHARGE_LIMIT |
					   CM_CHARGE_INFO_INPUT_LIMIT |
					   CM_CHARGE_INFO_JEITA_LIMIT));

	} else {
		try_wireless_charger_enable(cm, false);
		try_charger_enable(cm, false);
		cancel_delayed_work_sync(&cm_monitor_work);
		cancel_delayed_work_sync(&cm->cp_work);
		_cm_monitor(cm);

		cm->desc->is_fast_charge = false;
		cm->desc->ir_comp.ir_compensation_en = false;
		cm->desc->enable_fast_charge = false;
		cm->desc->fast_charge_disable_count = 0;
		cm->desc->cp.cp_running = false;
		cm->desc->cm_check_int = false;
		cm->desc->fast_charger_type = 0;
		cm->desc->charger_type = 0;
		cm->desc->cp.cp_target_vbus = 0;
		cm->desc->force_set_full = false;
		cm->emergency_stop = 0;
		cm->charging_status = 0;
		cm->desc->thm_info.thm_adjust_cur = -EINVAL;
		cm->desc->thm_info.thm_pwr = 0;
		cm->desc->thm_info.adapter_default_charge_vol = 5;
		cm->desc->wl_charge_en = 0;
		cm->desc->usb_charge_en = 0;
		cm->desc->pd_port_partner = 0;
		cm->desc->charger_type_cnt = 0;
		cm->cm_charge_vote->vote(cm->cm_charge_vote, false,
					 SPRD_VOTE_TYPE_ALL, 0, 0, 0, cm);
	}

	cm_update_charger_type_status(cm);

	if (is_polling_required(cm) && cm->desc->polling_interval_ms) {
		_cm_monitor(cm);
		schedule_work(&setup_polling);
	}

	power_supply_changed(cm->charger_psy);
}

static int cm_get_capacity_level(struct charger_manager *cm)
{
	int level = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	int uisoc, ocv_uv = 0;

	if (!is_batt_present(cm)) {
		/* There is no battery. Assume 100% */
		level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		return level;
	}

	uisoc = DIV_ROUND_CLOSEST(cm->desc->cap, 10);

	if (uisoc >= CM_CAPACITY_LEVEL_FULL)
		level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (uisoc > CM_CAPACITY_LEVEL_NORMAL)
		level = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (uisoc > CM_CAPACITY_LEVEL_LOW)
		level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (uisoc > CM_CAPACITY_LEVEL_CRITICAL)
		level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else
		level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;


	if (get_batt_ocv(cm, &ocv_uv)) {
		dev_err(cm->dev, "%s, get_batt_ocV error.\n", __func__);
		return level;
	}

	if (level == POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL && is_charging(cm) &&
	    ocv_uv > CM_CAPACITY_LEVEL_CRITICAL_VOLTAGE)
		level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;

	return level;
}

static int wireless_get_property(struct power_supply *psy, enum power_supply_property
				 psp, union power_supply_propval *val)
{
	int ret = 0;
	struct wireless_data *data = container_of(psy->desc, struct  wireless_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->WIRELESS_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ac_get_property(struct power_supply *psy, enum power_supply_property psp,
			   union power_supply_propval *val)
{
	int ret = 0;
	struct ac_data *data = container_of(psy->desc, struct ac_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->AC_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int usb_get_property(struct power_supply *psy, enum power_supply_property psp,
			    union power_supply_propval *val)
{
	int ret = 0;
	struct usb_data *data = container_of(psy->desc, struct usb_data, psd);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = data->USB_ONLINE;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static bool cm_add_battery_psy_property(struct charger_manager *cm, enum power_supply_property psp)
{
	u32 i;

	for (i = 0; i < cm->charger_psy_desc.num_properties; i++)
		if (cm->charger_psy_desc.properties[i] == psp)
			break;

	if (i == cm->charger_psy_desc.num_properties) {
		cm->charger_psy_desc.properties[cm->charger_psy_desc.num_properties++] = psp;
		return true;
	}
	return false;
}

static int charger_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	struct power_supply *fuel_gauge = NULL;
	unsigned int total_cap = 0;
	int chg_cur = 0;
	int ret = 0;
	int i;

	if (!cm)
		return -ENOMEM;
	if (!is_ext_pwr_online(cm) && (POWER_SUPPLY_PROP_BATT_SLATE_MODE == psp)){
		val->intval = batt_slate_mode_value;
		pr_err("%s: unplug charger,then return 0\n",__func__);
		return 0;
	}
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (is_charging(cm)) {
			cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
		} else if (is_ext_pwr_online(cm)) {
			if (is_full_charged(cm))
				cm->battery_status = POWER_SUPPLY_STATUS_FULL;
			else
				cm->battery_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else {
			cm->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;
		}

		val->intval = cm->battery_status;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (cm->emergency_stop == CM_EVENT_BATT_OVERHEAT ||
			(cm->charging_status & CM_CHARGE_TEMP_OVERHEAT))
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else if (cm->emergency_stop == CM_EVENT_BATT_COLD ||
			(cm->charging_status & CM_CHARGE_TEMP_COLD))
			val->intval = POWER_SUPPLY_HEALTH_COLD;
		else if (cm->charging_status & CM_CHARGE_VOLTAGE_ABNORMAL)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (is_batt_present(cm))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = get_vbat_avg_uV(cm, &val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = get_vbat_now_uV(cm, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = get_ibat_avg_uA(cm, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_BATT_CURRENT_UA_NOW:
		ret = get_ibat_now_uA(cm, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_UNKNOWN;
		fuel_gauge = power_supply_get_by_name(cm->desc->psy_fuel_gauge);
		if (!fuel_gauge) {
			ret = -ENODEV;
			break;
		}
		ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_TECHNOLOGY, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = cm->desc->temperature;
		break;
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		return cm_get_battery_temperature(cm, &val->intval);
	case POWER_SUPPLY_PROP_CAPACITY:
		if (!is_batt_present(cm)) {
			/* There is no battery. Assume 100% */
			val->intval = 100;
			break;
		}
		val->intval = DIV_ROUND_CLOSEST(cm->desc->cap, 10);
		if (val->intval > 100)
			val->intval = 100;
		else if (val->intval < 0)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = cm_get_capacity_level(cm);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		if (is_batt_present(cm))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		fuel_gauge = power_supply_get_by_name(
					cm->desc->psy_fuel_gauge);
		if (!fuel_gauge) {
			ret = -ENODEV;
			break;
		}

		ret = power_supply_get_property(fuel_gauge,
						POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
						val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		if (is_charging(cm)) {
			fuel_gauge = power_supply_get_by_name(
					cm->desc->psy_fuel_gauge);
			if (!fuel_gauge) {
				ret = -ENODEV;
				break;
			}

			ret = power_supply_get_property(fuel_gauge,
							POWER_SUPPLY_PROP_CHARGE_NOW,
							val);
			if (ret) {
				val->intval = 1;
				ret = 0;
			} else {
				/* If CHARGE_NOW is supplied, use it */
				val->intval = (val->intval > 0) ?
						val->intval : 1;
			}
		} else {
			val->intval = 0;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy,
							POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
							val);
			power_supply_put(psy);
			if (ret) {
				dev_err(cm->dev, "get charge current failed\n");
				continue;
			}
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy,
							POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
							val);
			power_supply_put(psy);
			if (ret) {
				dev_err(cm->dev, "set charge limit current failed\n");
				continue;
			}
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		fuel_gauge = power_supply_get_by_name(
					cm->desc->psy_fuel_gauge);
		if (!fuel_gauge) {
			ret = -ENODEV;
			break;
		}

		ret = power_supply_get_property(fuel_gauge,
						POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
						val);
		if (ret) {
			val->intval = 1;
			ret = 0;
		} else {
			/* If CHARGE_COUNTER is supplied, use it */
			val->intval = val->intval > 0 ? (val->intval / 1000) : 1;
			val->intval = (cm->desc->cap * val->intval);
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_get_property(psy,
							POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
							val);
			if (!ret) {
				power_supply_put(psy);
				if (cm->desc->enable_fast_charge &&
				    cm->desc->psy_charger_stat[1])
					val->intval *= 2;
				break;
			}

			ret = power_supply_get_property(psy,
							POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
							val);
			if (!ret) {
				power_supply_put(psy);
				break;
			}
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		fuel_gauge = power_supply_get_by_name(
					cm->desc->psy_fuel_gauge);
		if (!fuel_gauge) {
			ret = -ENODEV;
			break;
		}

		ret = power_supply_get_property(fuel_gauge,
						POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
						val);
		break;

	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = get_charger_current(cm, &chg_cur);
		if (ret) {
			dev_err(cm->dev, "get chg_cur error.\n");
			break;
		}
		chg_cur = chg_cur / 1000;

		ret = get_batt_total_cap(cm, &total_cap);
		if (ret) {
			dev_err(cm->dev, "failed to get total cap.\n");
			break;
		}
		total_cap = total_cap / 1000;

		val->intval =
			((1000 - cm->desc->cap) * total_cap / 1000) * 3600 / chg_cur;

		break;

	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}
			ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_CHARGE_ENABLED, val);
			power_supply_put(psy);
			if (ret) {
				dev_err(cm->dev, "get charge enabled failed\n");
				continue;
			}
		}
		if(val->intval == 1)
			val->intval = 0;
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}
			ret = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_CHARGE_ENABLED, val);
			power_supply_put(psy);
			if (ret) {
				dev_err(cm->dev, "get charge enabled failed\n");
				continue;
			}
		}
		break;
	case POWER_SUPPLY_PROP_BATT_HV_CHARGER_STATUS:
		val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	if (fuel_gauge)
		power_supply_put(fuel_gauge);
	return ret;
}

static int
charger_set_property(struct power_supply *psy,
		     enum power_supply_property psp,
		     const union power_supply_propval *val)
{
	struct charger_manager *cm = power_supply_get_drvdata(psy);
	int ret = 0;
	int i;
	union power_supply_propval charging_val;

	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!is_ext_pwr_online(cm) && (POWER_SUPPLY_PROP_BATT_SLATE_MODE != psp))
		return -ENODEV;
	else if (!is_ext_pwr_online(cm) && (POWER_SUPPLY_PROP_BATT_SLATE_MODE == psp)){
		batt_slate_mode_value = val->intval;
		pr_err("%s: unplug charger,then return 0\n",__func__);
		return 0;
	}
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_CONSTANT_CHARGE_CURRENT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		/* The ChargerIC with linear charging cannot set Ibus, only Ibat. */
		if (cm->desc->thm_info.need_calib_charge_lmt) {
			cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBAT,
					 SPRD_VOTE_TYPE_IBAT_ID_INPUT_CURRENT_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
			break;
		}

		cm->cm_charge_vote->vote(cm->cm_charge_vote, true,
					 SPRD_VOTE_TYPE_IBUS,
					 SPRD_VOTE_TYPE_IBUS_ID_INPUT_CURRENT_LIMIT,
					 SPRD_VOTE_CMD_MIN,
					 val->intval, cm);
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		dev_info(cm->dev, "thermal set charge power limit, thm_pwr = %dmW, is_charger_mode=%d\n", val->intval,is_charger_mode);
		if(!is_charger_mode){
			cm->desc->thm_info.thm_pwr = val->intval;
			cm_update_charge_info(cm, CM_CHARGE_INFO_THERMAL_LIMIT);

			if (cm->desc->cp.cp_running)
				cm_check_target_ibus(cm);
		}
		break;

	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
		if(val->intval == 0)
			charging_val.intval= 1;
		else
			charging_val.intval = 0;

		pr_err("%s:val=%d\n",__func__,charging_val.intval);

		if(charging_val.intval == 0){
			cm->desc->force_set_full = false;
		}
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_set_property(psy,
				POWER_SUPPLY_PROP_CHARGE_ENABLED, &charging_val);
			power_supply_put(psy);
			if (ret) {
				dev_err(cm->dev, "set charge enabled failed\n");
				continue;
			}
		}
		cm_notify_event(psy, CM_EVENT_CHG_START_STOP, NULL);
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		pr_err("%s:val=%d\n",__func__,val->intval);
		if(val->intval == 0){
			cm->desc->force_set_full = false;
		}
		for (i = 0; cm->desc->psy_charger_stat[i]; i++) {
			psy = power_supply_get_by_name(
					cm->desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(cm->dev, "Cannot find power supply \"%s\"\n",
					cm->desc->psy_charger_stat[i]);
				continue;
			}

			ret = power_supply_set_property(psy,
				POWER_SUPPLY_PROP_CHARGE_ENABLED, val);
			power_supply_put(psy);
			if (ret) {
				dev_err(cm->dev, "set charge enabled failed\n");
				continue;
			}
		}
		cm_notify_event(psy, CM_EVENT_CHG_START_STOP, NULL);
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int charger_property_is_writeable(struct power_supply *psy, enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
	case POWER_SUPPLY_PROP_BATT_SLATE_MODE:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}
#define NUM_CHARGER_PSY_OPTIONAL	(4)

static enum power_supply_property wireless_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property default_charger_props[] = {
	/* Guaranteed to provide */
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_BATT_CURRENT_UA_NOW,
	POWER_SUPPLY_PROP_BATT_HV_CHARGER_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_BATT_SLATE_MODE,
	/*
	 * Optional properties are:
	 * POWER_SUPPLY_PROP_CHARGE_NOW,
	 * POWER_SUPPLY_PROP_CURRENT_NOW,
	 * POWER_SUPPLY_PROP_TEMP, and
	 * POWER_SUPPLY_PROP_TEMP_AMBIENT,
	 */
};

/* wireless_data initialization */
static struct wireless_data wireless_main = {
	.psd = {
		.name = "wireless",
		.type =	POWER_SUPPLY_TYPE_WIRELESS,
		.properties = wireless_props,
		.num_properties = ARRAY_SIZE(wireless_props),
		.get_property = wireless_get_property,
	},
	.WIRELESS_ONLINE = 0,
};

/* ac_data initialization */
static struct ac_data ac_main = {
	.psd = {
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.properties = ac_props,
		.num_properties = ARRAY_SIZE(ac_props),
		.get_property = ac_get_property,
	},
	.AC_ONLINE = 0,
};

/* usb_data initialization */
static struct usb_data usb_main = {
	.psd = {
		.name = "usb",
		.type = POWER_SUPPLY_TYPE_USB,
		.properties = usb_props,
		.num_properties = ARRAY_SIZE(usb_props),
		.get_property = usb_get_property,
	},
	.USB_ONLINE = 0,
};

static const struct power_supply_desc psy_default = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = default_charger_props,
	.num_properties = ARRAY_SIZE(default_charger_props),
	.get_property = charger_get_property,
	.set_property = charger_set_property,
	.property_is_writeable	= charger_property_is_writeable,
	.no_thermal = true,
};

/*
 * support multi charger adapt
 */

static struct charger_otg_match psy_otg_table[PSY_OTG_NUM_MAX] = {
	{"bq2560x_charger", "bq2560x_otg_vbus"},
	{"fan54015_charger", "fan54015_otg_vbus"},
	{"bq25890_charger", "bq25890_otg_vbus"},
	{"eta6937_charger", "eta6937_otg_vbus"},
	{"sgm41511_charger", "sgm41511_otg_vbus"},
	{"rt9471_charger", "rt9471_otg_vbus"},
	{NULL, NULL},
};

static const char *cm_charger_find_otg(struct charger_manager *cm)
{
	int i;
	const char *psy_name;
	const char *otg_name = NULL;
	const char *psy_charger_name = cm->desc->psy_charger_stat[0];

	for (i = 0; psy_otg_table[i].charger_power_supply_name; i++) {
		psy_name = psy_otg_table[i].charger_power_supply_name;
		if (!strcmp(psy_name, psy_charger_name)) {
			otg_name = psy_otg_table[i].otg_name;
			break;
		}
	}

	return otg_name;
}

static int cm_charger_enable_otg(struct regulator_dev *dev)
{
	struct charger_manager *cm = rdev_get_drvdata(dev);
	int ret = 0;
	const char *otg_name = NULL;
	struct regulator *charger_otg;

	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	otg_name = cm_charger_find_otg(cm);
	dev_info(cm->dev, "%s:line%d otg_name:%s\n",
		 __func__, __LINE__, otg_name);

	if (otg_name) {
		charger_otg = regulator_get(cm->dev, otg_name);
		if (IS_ERR(charger_otg)) {
			ret = PTR_ERR(charger_otg);
			dev_err(cm->dev, "failed to get charger "
				"otg vbus supply:%s, ret = %d\n",
				otg_name, ret);
			return ret;
		}

		ret = regulator_enable(charger_otg);
		if (ret) {
			dev_err(cm->dev, "cannot enable %s "
				"vbus regulator, ret=%d\n",
				otg_name, ret);
			return ret;
		}
	}

	return ret;
}

static int cm_charger_disable_otg(struct regulator_dev *dev)
{
	struct charger_manager *cm = rdev_get_drvdata(dev);
	int ret = 0;
	const char *otg_name = NULL;
	struct regulator *charger_otg;

	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	otg_name = cm_charger_find_otg(cm);
	dev_info(cm->dev, "%s:line%d otg_name:%s\n",
		 __func__, __LINE__, otg_name);

	if (otg_name) {
		charger_otg = regulator_get(cm->dev, otg_name);
		if (IS_ERR(charger_otg)) {
			ret = PTR_ERR(charger_otg);
			dev_err(cm->dev, "failed to get charger "
				"otg vbus supply:%s, ret = %d\n",
				otg_name, ret);
			return ret;
		}

		ret = regulator_disable(charger_otg);
		if (ret) {
			dev_err(cm->dev, "cannot disable %s "
				"vbus regulator, ret=%d\n",
				otg_name, ret);
			return ret;
		}
	}
	return ret;
}

static int cm_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct charger_manager *cm = rdev_get_drvdata(dev);
	int ret = -EINVAL;
	const char *otg_name = NULL;
	struct regulator *charger_otg;

	if (!cm) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	otg_name = cm_charger_find_otg(cm);
	dev_info(cm->dev, "%s:line%d otg_name:%s\n",
		 __func__, __LINE__, otg_name);

	if (otg_name) {
		charger_otg = regulator_get(cm->dev, otg_name);
		if (IS_ERR(charger_otg)) {
			ret = PTR_ERR(charger_otg);
			dev_err(cm->dev, "failed to get charger "
				"otg vbus supply:%s, ret = %d\n",
				otg_name, ret);
			return ret;
		}

		ret = regulator_is_enabled(charger_otg);
	}

	return ret;
}

static const struct regulator_ops cm_charger_vbus_ops = {
	.enable = cm_charger_enable_otg,
	.disable = cm_charger_disable_otg,
	.is_enabled = cm_charger_vbus_is_enabled,
};

static const struct regulator_desc cm_charger_vbus_desc = {
	.name = "cm_otg_vbus",
	.of_match = "cm_otg_vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &cm_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
cm_charger_register_vbus_regulator(struct charger_manager *cm)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	if (cm->desc->psy_nums <= 0 || !cm->desc->enable_multi_charger_adapt) {
		dev_info(cm->dev, "psy nums:%d, enable_multi_charger_adapt:%d\n",
			cm->desc->psy_nums, cm->desc->enable_multi_charger_adapt);
		return 0;
	}

	cfg.dev = cm->dev;
	cfg.driver_data = cm;
	reg = devm_regulator_register(cm->dev, &cm_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(cm->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#ifdef CONFIG_TYPEC_TCPM
static bool cm_pd_is_ac_online(struct charger_manager *cm)
{
	struct adapter_power_cap pd_source_cap;
	struct tcpm_port *port;
	struct power_supply *psy;
	int i, index = 0;

	psy = power_supply_get_by_name("tcpm-source-psy-sc27xx-pd");
	if (!psy) {
		dev_err(cm->dev, "failed to get tcpm psy!\n");
		return true;
	}

	port = power_supply_get_drvdata(psy);
	if (!port) {
		dev_err(cm->dev, "failed to get tcpm port!\n");
		return true;
	}

	tcpm_get_source_capabilities(port, &pd_source_cap);

	for (i = 0; i < pd_source_cap.nr_source_caps; i++) {
		if ((pd_source_cap.max_mv[i] <= 5000) &&
		    (pd_source_cap.type[i] == PDO_TYPE_FIXED))
			index++;
	}

	if (pd_source_cap.nr_source_caps == index) {
		dev_info(cm->dev, "pd type ac online is false\n");
		return false;
	}

	return true;
}
#else
static bool cm_pd_is_ac_online(struct charger_manager *cm)
{
	return true;
}
#endif

static void cm_update_charger_type_status(struct charger_manager *cm)
{

	if (is_ext_usb_pwr_online(cm)) {
		switch (cm->desc->charger_type) {
		case POWER_SUPPLY_USB_CHARGER_TYPE_DCP:
		case POWER_SUPPLY_USB_CHARGER_TYPE_PD:
		case POWER_SUPPLY_USB_CHARGER_TYPE_PD_PPS:
		case POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_1P0:
		case POWER_SUPPLY_USB_CHARGER_TYPE_SFCP_2P0:
			if (cm->desc->charger_type == POWER_SUPPLY_USB_CHARGER_TYPE_PD &&
				!cm_pd_is_ac_online(cm) && !cm->desc->pd_port_partner) {
				wireless_main.WIRELESS_ONLINE = 0;
				ac_main.AC_ONLINE = 0;
				usb_main.USB_ONLINE = 1;
				dev_info(cm->dev, "usb online--pd type 5V\n");
			} else if (cm->desc->pd_port_partner) {
				wireless_main.WIRELESS_ONLINE = 0;
				usb_main.USB_ONLINE = 0;
				ac_main.AC_ONLINE = 1;
				cm->desc->pd_port_partner = 0;
				dev_info(cm->dev, "pd_port_partner ac online\n");
			} else {
				wireless_main.WIRELESS_ONLINE = 0;
				usb_main.USB_ONLINE = 0;
				ac_main.AC_ONLINE = 1;
				dev_info(cm->dev, "ac online\n");
			}
			break;
		default:
			wireless_main.WIRELESS_ONLINE = 0;
			ac_main.AC_ONLINE = 0;
			usb_main.USB_ONLINE = 1;
			break;
		}
	} else if (is_ext_wl_pwr_online(cm)) {
		wireless_main.WIRELESS_ONLINE = 1;
		ac_main.AC_ONLINE = 0;
		usb_main.USB_ONLINE = 0;
	} else {
		wireless_main.WIRELESS_ONLINE = 0;
		ac_main.AC_ONLINE = 0;
		usb_main.USB_ONLINE = 0;
	}
}

void cm_check_pd_port_partner(bool is_pd_hub)
{
	struct charger_manager *cm = global_cm;

	if (!cm) {
		pr_err("%s:line%d NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	dev_info(cm->dev, "%s is_pd_hub = %d\n", __func__, is_pd_hub);

	if (!is_ext_usb_pwr_online(cm)) {
		dev_info(cm->dev, "%s offline = %d\n", __func__);
	}

	dev_info(cm->dev, "%s is_pd_hub = %d\n", __func__, is_pd_hub);

	cm->desc->pd_port_partner = is_pd_hub;

	if (usb_main.USB_ONLINE && cm->desc->pd_port_partner) {
		wireless_main.WIRELESS_ONLINE = 0;
		usb_main.USB_ONLINE = 0;
		ac_main.AC_ONLINE = 1;
		cm->desc->pd_port_partner = 0;
		power_supply_changed(cm->charger_psy);
		dev_info(cm->dev, "%s ac online\n", __func__);
	}
}

/**
 * cm_setup_timer - For in-suspend monitoring setup wakeup alarm
 *		    for suspend_again.
 *
 * Returns true if the alarm is set for Charger Manager to use.
 * Returns false if
 *	cm_setup_timer fails to set an alarm,
 *	cm_setup_timer does not need to set an alarm for Charger Manager,
 *	or an alarm previously configured is to be used.
 */
static bool cm_setup_timer(void)
{
	struct charger_manager *cm;
	unsigned int wakeup_ms = UINT_MAX;
	int timer_req = 0;

	if (time_after(next_polling, jiffies))
		CM_MIN_VALID(wakeup_ms,
			jiffies_to_msecs(next_polling - jiffies));

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		unsigned int fbchk_ms = 0;

		/* fullbatt_vchk is required. setup timer for that */
		if (cm->fullbatt_vchk_jiffies_at) {
			fbchk_ms = jiffies_to_msecs(cm->fullbatt_vchk_jiffies_at
						    - jiffies);
			if (time_is_before_eq_jiffies(
				cm->fullbatt_vchk_jiffies_at) ||
				msecs_to_jiffies(fbchk_ms) < CM_JIFFIES_SMALL) {
				fullbatt_vchk(&cm->fullbatt_vchk_work.work);
				fbchk_ms = 0;
			}
		}
		CM_MIN_VALID(wakeup_ms, fbchk_ms);

		/* Skip if polling is not required for this CM */
		if (!is_polling_required(cm) && !cm->emergency_stop)
			continue;
		timer_req++;
		if (cm->desc->polling_interval_ms == 0)
			continue;
		if (cm->desc->ir_comp.ir_compensation_en)
			CM_MIN_VALID(wakeup_ms, CM_IR_COMPENSATION_TIME * 1000);
		else
			CM_MIN_VALID(wakeup_ms, cm->desc->polling_interval_ms);
	}
	mutex_unlock(&cm_list_mtx);

	if (timer_req && cm_timer) {
		ktime_t now, add;

		/*
		 * Set alarm with the polling interval (wakeup_ms)
		 * The alarm time should be NOW + CM_RTC_SMALL or later.
		 */
		if (wakeup_ms == UINT_MAX ||
			wakeup_ms < CM_RTC_SMALL * MSEC_PER_SEC)
			wakeup_ms = 2 * CM_RTC_SMALL * MSEC_PER_SEC;

		pr_info("Charger Manager wakeup timer: %u ms\n", wakeup_ms);

		now = ktime_get_boottime();
		add = ktime_set(wakeup_ms / MSEC_PER_SEC,
				(wakeup_ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
		alarm_start(cm_timer, ktime_add(now, add));

		cm_suspend_duration_ms = wakeup_ms;

		return true;
	}
	return false;
}

/**
 * charger_extcon_notifier - receive the state of charger cable
 *			when registered cable is attached or detached.
 *
 * @self: the notifier block of the charger_extcon_notifier.
 * @event: the cable state.
 * @ptr: the data pointer of notifier block.
 */
static int charger_extcon_notifier(struct notifier_block *self, unsigned long event, void *ptr)
{
	struct charger_cable *cable =
		container_of(self, struct charger_cable, nb);

	/*
	 * The newly state of charger cable.
	 * If cable is attached, cable->attached is true.
	 */
	cable->attached = event;

	/*
	 * Setup monitoring to check battery state
	 * when charger cable is attached.
	 */
	if (cable->attached && is_polling_required(cable->cm)) {
		cancel_work_sync(&setup_polling);
		schedule_work(&setup_polling);
	}

	return NOTIFY_DONE;
}

/**
 * charger_extcon_init - register external connector to use it
 *			as the charger cable
 *
 * @cm: the Charger Manager representing the battery.
 * @cable: the Charger cable representing the external connector.
 */
static int charger_extcon_init(struct charger_manager *cm, struct charger_cable *cable)
{
	int ret;

	/*
	 * Charger manager use Extcon framework to identify
	 * the charger cable among various external connector
	 * cable (e.g., TA, USB, MHL, Dock).
	 */
	cable->nb.notifier_call = charger_extcon_notifier;
	ret = devm_extcon_register_notifier(cm->dev, cable->extcon_dev,
					    EXTCON_USB, &cable->nb);
	if (ret < 0)
		dev_err(cm->dev, "Cannot register extcon_dev for (cable: %s)\n",
			cable->name);

	return ret;
}

/**
 * charger_manager_register_extcon - Register extcon device to recevie state
 *				     of charger cable.
 * @cm: the Charger Manager representing the battery.
 *
 * This function support EXTCON(External Connector) subsystem to detect the
 * state of charger cables for enabling or disabling charger(regulator) and
 * select the charger cable for charging among a number of external cable
 * according to policy of H/W board.
 */
static int charger_manager_register_extcon(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct charger_regulator *charger;
	int ret;
	int i;
	int j;

	for (i = 0; i < desc->num_charger_regulators; i++) {
		charger = &desc->charger_regulators[i];

		charger->consumer = regulator_get(cm->dev,
					charger->regulator_name);
		if (IS_ERR(charger->consumer)) {
			dev_err(cm->dev, "Cannot find charger(%s)\n",
				charger->regulator_name);
			return PTR_ERR(charger->consumer);
		}
		charger->cm = cm;

		for (j = 0; j < charger->num_cables; j++) {
			struct charger_cable *cable = &charger->cables[j];

			ret = charger_extcon_init(cm, cable);
			if (ret < 0) {
				dev_err(cm->dev, "Cannot initialize charger(%s)\n",
					charger->regulator_name);
				return ret;
			}
			cable->charger = charger;
			cable->cm = cm;
		}
	}

	return 0;
}

/* help function of sysfs node to control charger(regulator) */
static ssize_t charger_name_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator, attr_name);

	return sprintf(buf, "%s\n", charger->regulator_name);
}

static ssize_t charger_state_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator, attr_state);
	int state = 0;

	if (!charger->externally_control)
		state = regulator_is_enabled(charger->consumer);

	return sprintf(buf, "%s\n", state ? "enabled" : "disabled");
}

static ssize_t jeita_control_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator,
			     attr_jeita_control);
	struct charger_desc *desc = charger->cm->desc;

	return sprintf(buf, "%d\n", !desc->jeita_disabled);
}

static ssize_t jeita_control_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator,
			     attr_jeita_control);
	struct charger_desc *desc = charger->cm->desc;
	bool enabled;

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	desc->jeita_disabled = !enabled;

	return count;
}

static ssize_t charge_pump_present_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator,
			     attr_charge_pump_present);
	struct charger_manager *cm = charger->cm;
	bool status = false;

	if (cm_check_cp_charger_enabled(cm))
		status = true;

	return sprintf(buf, "%d\n", status);
}

static ssize_t charge_pump_present_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator,
			     attr_charge_pump_present);
	struct charger_manager *cm = charger->cm;
	bool enabled;

	if (!cm || !cm->desc || !cm->desc->psy_cp_stat)
		return count;

	ret =  kstrtobool(buf, &enabled);
	if (ret)
		return ret;

	if (enabled) {
		cm_init_cp(cm);
		cm_primary_charger_enable(cm, false);
		if (cm_check_primary_charger_enabled(cm)) {
			dev_err(cm->dev, "Fail to disable primary charger\n");
			return -EINVAL;
		}

		cm_cp_master_charger_enable(cm, true);
		if (!cm_check_cp_charger_enabled(cm))
			dev_err(cm->dev, "Fail to enable charge pump\n");
	} else {
		if (!cm_cp_master_charger_enable(cm, false))
			dev_err(cm->dev, "Fail to disable master charge pump\n");
	}

	return count;
}

static ssize_t charge_pump_current_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator,
			     attr_charge_pump_current);
	struct charger_manager *cm = charger->cm;
	int cur, ret;

	if (charger->cp_id < 0) {
		dev_err(cm->dev, "charge pump id is error!!!!!!\n");
		cur = 0;
		return sprintf(buf, "%d\n", cur);
	}

	ret = get_cp_ibat_uA_by_id(cm, &cur, charger->cp_id);
	if (ret)
		cur = 0;

	return sprintf(buf, "%d\n", cur);
}

static ssize_t charge_pump_current_id_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	struct charger_regulator *charger =
		container_of(attr, struct charger_regulator,
			     attr_charge_pump_current);
	struct charger_manager *cm = charger->cm;
	int cp_id;

	ret =  kstrtoint(buf, 10, &cp_id);
	if (ret)
		return ret;

	if (cp_id < 0) {
		dev_err(cm->dev, "charge pump id is error!!!!!!\n");
		cp_id = 0;
	}
	charger->cp_id = cp_id;

	return count;
}
static ssize_t charger_stop_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
			       attr_stop_charge);
	bool stop_charge;

	stop_charge = is_charging(charger->cm);

	return sprintf(buf, "%d\n", !stop_charge);
}

static ssize_t charger_stop_store(struct device *dev,
				  struct device_attribute *attr, const char *buf,
				  size_t count)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
					attr_stop_charge);
	struct charger_manager *cm = charger->cm;
	int stop_charge, ret;

	ret = sscanf(buf, "%d", &stop_charge);
	if (!ret)
		return -EINVAL;

	if (!is_ext_pwr_online(cm))
		return -EINVAL;

	if (!stop_charge) {
		ret = try_charger_enable(cm, true);
		if (ret) {
			dev_err(cm->dev, "failed to start charger.\n");
			return ret;
		}
		charger->externally_control = false;
	} else {
		ret = try_charger_enable(cm, false);
		if (ret) {
			dev_err(cm->dev, "failed to stop charger.\n");
			return ret;
		}
		charger->externally_control = true;
	}

	power_supply_changed(cm->charger_psy);

	return count;
}

static ssize_t charger_externally_control_show(struct device *dev,
					       struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger = container_of(attr,
			struct charger_regulator, attr_externally_control);

	return sprintf(buf, "%d\n", charger->externally_control);
}

static ssize_t charger_externally_control_store(struct device *dev,
						struct device_attribute *attr, const char *buf,
						size_t count)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator,
					attr_externally_control);
	struct charger_manager *cm = charger->cm;
	struct charger_desc *desc = cm->desc;
	int i;
	int ret;
	int externally_control;
	int chargers_externally_control = 1;

	ret = sscanf(buf, "%d", &externally_control);
	if (ret == 0) {
		ret = -EINVAL;
		return ret;
	}

	if (!externally_control) {
		charger->externally_control = 0;
		return count;
	}

	for (i = 0; i < desc->num_charger_regulators; i++) {
		if (&desc->charger_regulators[i] != charger &&
			!desc->charger_regulators[i].externally_control) {
			/*
			 * At least, one charger is controlled by
			 * charger-manager
			 */
			chargers_externally_control = 0;
			break;
		}
	}

	if (!chargers_externally_control) {
		if (cm->charger_enabled) {
			try_charger_enable(charger->cm, false);
			charger->externally_control = externally_control;
			try_charger_enable(charger->cm, true);
		} else {
			charger->externally_control = externally_control;
		}
	} else {
		dev_warn(cm->dev,
			 "'%s' regulator should be controlled in charger-manager because charger-manager must need at least one charger for charging\n",
			 charger->regulator_name);
	}

	return count;
}

static ssize_t cp_num_show(struct device *dev,
		       struct device_attribute *attr, char *buf)
{
	struct charger_regulator *charger
		= container_of(attr, struct charger_regulator, attr_cp_num);
	struct charger_manager *cm = charger->cm;
	int cp_num = 0;

	cp_num = cm->desc->cp_nums;
	return sprintf(buf, "%d\n", cp_num);
}

/**
 * charger_manager_register_sysfs - Register sysfs entry for each charger
 * @cm: the Charger Manager representing the battery.
 *
 * This function add sysfs entry for charger(regulator) to control charger from
 * user-space. If some development board use one more chargers for charging
 * but only need one charger on specific case which is dependent on user
 * scenario or hardware restrictions, the user enter 1 or 0(zero) to '/sys/
 * class/power_supply/battery/charger.[index]/externally_control'. For example,
 * if user enter 1 to 'sys/class/power_supply/battery/charger.[index]/
 * externally_control, this charger isn't controlled from charger-manager and
 * always stay off state of regulator.
 */
static int charger_manager_register_sysfs(struct charger_manager *cm)
{
	struct charger_desc *desc = cm->desc;
	struct charger_regulator *charger;
	int chargers_externally_control = 1;
	char buf[11];
	char *str;
	int ret;
	int i;

	/* Create sysfs entry to control charger(regulator) */
	for (i = 0; i < desc->num_charger_regulators; i++) {
		charger = &desc->charger_regulators[i];

		snprintf(buf, 10, "charger.%d", i);
		str = devm_kzalloc(cm->dev,
				sizeof(char) * (strlen(buf) + 1), GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		strcpy(str, buf);

		charger->attrs[0] = &charger->attr_name.attr;
		charger->attrs[1] = &charger->attr_state.attr;
		charger->attrs[2] = &charger->attr_externally_control.attr;
		charger->attrs[3] = &charger->attr_stop_charge.attr;
		charger->attrs[4] = &charger->attr_jeita_control.attr;
		charger->attrs[5] = &charger->attr_cp_num.attr;
		charger->attrs[6] = &charger->attr_charge_pump_present.attr;
		charger->attrs[7] = &charger->attr_charge_pump_current.attr;
		charger->attrs[8] = NULL;

		charger->attr_g.name = str;
		charger->attr_g.attrs = charger->attrs;

		sysfs_attr_init(&charger->attr_name.attr);
		charger->attr_name.attr.name = "name";
		charger->attr_name.attr.mode = 0444;
		charger->attr_name.show = charger_name_show;

		sysfs_attr_init(&charger->attr_state.attr);
		charger->attr_state.attr.name = "state";
		charger->attr_state.attr.mode = 0444;
		charger->attr_state.show = charger_state_show;

		sysfs_attr_init(&charger->attr_stop_charge.attr);
		charger->attr_stop_charge.attr.name = "stop_charge";
		charger->attr_stop_charge.attr.mode = 0644;
		charger->attr_stop_charge.show = charger_stop_show;
		charger->attr_stop_charge.store = charger_stop_store;

		sysfs_attr_init(&charger->attr_jeita_control.attr);
		charger->attr_jeita_control.attr.name = "jeita_control";
		charger->attr_jeita_control.attr.mode = 0644;
		charger->attr_jeita_control.show = jeita_control_show;
		charger->attr_jeita_control.store = jeita_control_store;

		sysfs_attr_init(&charger->attr_cp_num.attr);
		charger->attr_cp_num.attr.name = "cp_num";
		charger->attr_cp_num.attr.mode = 0444;
		charger->attr_cp_num.show = cp_num_show;

		sysfs_attr_init(&charger->attr_charge_pump_present.attr);
		charger->attr_charge_pump_present.attr.name = "charge_pump_present";
		charger->attr_charge_pump_present.attr.mode = 0644;
		charger->attr_charge_pump_present.show = charge_pump_present_show;
		charger->attr_charge_pump_present.store = charge_pump_present_store;

		sysfs_attr_init(&charger->attr_charge_pump_current.attr);
		charger->attr_charge_pump_current.attr.name = "charge_pump_current";
		charger->attr_charge_pump_current.attr.mode = 0644;
		charger->attr_charge_pump_current.show = charge_pump_current_show;
		charger->attr_charge_pump_current.store = charge_pump_current_id_store;

		sysfs_attr_init(&charger->attr_externally_control.attr);
		charger->attr_externally_control.attr.name
				= "externally_control";
		charger->attr_externally_control.attr.mode = 0644;
		charger->attr_externally_control.show
				= charger_externally_control_show;
		charger->attr_externally_control.store
				= charger_externally_control_store;

		if (!desc->charger_regulators[i].externally_control ||
				!chargers_externally_control)
			chargers_externally_control = 0;

		dev_info(cm->dev, "'%s' regulator's externally_control is %d\n",
			 charger->regulator_name, charger->externally_control);

		ret = sysfs_create_group(&cm->charger_psy->dev.kobj,
					&charger->attr_g);
		if (ret < 0) {
			dev_err(cm->dev, "Cannot create sysfs entry of %s regulator\n",
				charger->regulator_name);
			return ret;
		}
	}

	if (chargers_externally_control) {
		dev_err(cm->dev, "Cannot register regulator because charger-manager must need at least one charger for charging battery\n");
		return -EINVAL;
	}

	return 0;
}

static int cm_init_thermal_data(struct charger_manager *cm, struct power_supply *fuel_gauge)
{
	struct charger_desc *desc = cm->desc;
	union power_supply_propval val;
	int ret;

	/* Verify whether fuel gauge provides battery temperature */
	ret = power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_TEMP, &val);

	if (!ret) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_TEMP))
			dev_warn(cm->dev, "POWER_SUPPLY_PROP_TEMP is present\n");
		cm->desc->measure_battery_temp = true;
	}
#ifdef CONFIG_THERMAL
	if (desc->thermal_zone) {
		cm->tzd_batt =
			thermal_zone_get_zone_by_name(desc->thermal_zone);
		if (IS_ERR(cm->tzd_batt))
			return PTR_ERR(cm->tzd_batt);

		/* Use external thermometer */
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_TEMP_AMBIENT))
			dev_warn(cm->dev, "POWER_SUPPLY_PROP_TEMP_AMBIENT is present\n");
		cm->desc->measure_battery_temp = true;
		ret = 0;
	}
#endif
	if (cm->desc->measure_battery_temp) {
		/* NOTICE : Default allowable minimum charge temperature is 0 */
		if (!desc->temp_max)
			desc->temp_max = CM_DEFAULT_CHARGE_TEMP_MAX;
		if (!desc->temp_diff)
			desc->temp_diff = CM_DEFAULT_RECHARGE_TEMP_DIFF;
	}

	return ret;
}

static int cm_parse_jeita_table(struct charger_desc *desc,
				struct device *dev,
				const char *np_name,
				struct charger_jeita_table **cur_table)
{
	struct device_node *np = dev->of_node;
	struct charger_jeita_table *table;
	const __be32 *list;
	int i, size;

	list = of_get_property(np, np_name, &size);
	if (!list || !size)
		return 0;

	desc->jeita_tab_size = size / (sizeof(struct charger_jeita_table) /
				       sizeof(int) * sizeof(__be32));

	table = devm_kzalloc(dev, sizeof(struct charger_jeita_table) *
				(desc->jeita_tab_size + 1), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	for (i = 0; i < desc->jeita_tab_size; i++) {
		table[i].temp = be32_to_cpu(*list++) - 1000;
		table[i].recovery_temp = be32_to_cpu(*list++) - 1000;
		table[i].current_ua = be32_to_cpu(*list++);
		table[i].term_volt = be32_to_cpu(*list++);
	}
	*cur_table = table;

	return 0;
}

static int cm_init_jeita_table(struct charger_desc *desc, struct device *dev)
{
	int ret, i;

	for (i = CM_JEITA_DCP; i < CM_JEITA_MAX; i++) {
		ret = cm_parse_jeita_table(desc,
					   dev,
					   jeita_type_names[i],
					   &desc->jeita_tab_array[i]);
		if (ret)
			return ret;
	}

	desc->jeita_tab = desc->jeita_tab_array[CM_JEITA_UNKNOWN];

	return 0;
}

static const struct of_device_id charger_manager_match[] = {
	{
		.compatible = "charger-manager",
	},
	{},
};

static struct charger_desc *of_cm_parse_desc(struct device *dev)
{
	struct charger_desc *desc;
	struct device_node *np = dev->of_node;
	u32 poll_mode = CM_POLL_DISABLE;
	u32 battery_stat = CM_NO_BATTERY;
	int ret, i = 0, num_chgs = 0;
	int num_psys = 0;

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return ERR_PTR(-ENOMEM);

	of_property_read_string(np, "cm-name", &desc->psy_name);

	of_property_read_u32(np, "cm-poll-mode", &poll_mode);
	desc->polling_mode = poll_mode;

	desc->uvlo_shutdown_mode = CM_SHUTDOWN_MODE_ANDROID;
	of_property_read_u32(np, "cm-uvlo-shutdown-mode", &desc->uvlo_shutdown_mode);

	of_property_read_u32(np, "cm-poll-interval",
				&desc->polling_interval_ms);

	of_property_read_u32(np, "cm-fullbatt-vchkdrop-ms",
					&desc->fullbatt_vchkdrop_ms);
	of_property_read_u32(np, "cm-fullbatt-vchkdrop-volt",
					&desc->fullbatt_vchkdrop_uV);
	of_property_read_u32(np, "cm-fullbatt-voltage", &desc->fullbatt_uV);
	of_property_read_u32(np, "cm-fullbatt-current", &desc->fullbatt_uA);
	of_property_read_u32(np, "cm-first-fullbatt-current", &desc->first_fullbatt_uA);
	of_property_read_u32(np, "cm-fullbatt-soc", &desc->fullbatt_soc);
	of_property_read_u32(np, "cm-fullbatt-capacity",
					&desc->fullbatt_full_capacity);
	of_property_read_u32(np, "cm-shutdown-voltage", &desc->shutdown_voltage);
	of_property_read_u32(np, "cm-tickle-time-out", &desc->trickle_time_out);
	of_property_read_u32(np, "cm-one-cap-time", &desc->cap_one_time);
	of_property_read_u32(np, "cm-wdt-interval", &desc->wdt_interval);

	of_property_read_u32(np, "cm-battery-stat", &battery_stat);
	desc->battery_present = battery_stat;

	/* power supply */
	num_psys = of_property_count_strings(np, "cm-power-supplys");
	dev_info(dev, "%s num_psys = %d\n", __func__, num_psys);
	desc->enable_multi_charger_adapt =
		device_property_read_bool(dev, "cm-multi-charger-adapt-enable");
	if (num_psys > 0) {
		desc->psy_nums = num_psys;
		/* Allocate empty bin at the tail of array */
		desc->psy_stat = devm_kzalloc(dev, sizeof(char *)
						* (num_psys + 1), GFP_KERNEL);
		if (desc->psy_stat) {
			for (i = 0; i < num_psys; i++)
				of_property_read_string_index(np, "cm-power-supplys",
						i, &desc->psy_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	/* chargers */
	num_chgs = of_property_count_strings(np, "cm-chargers");
	if (num_chgs > 0) {
		desc->buck_nums = num_chgs;
		/* Allocate empty bin at the tail of array */
		desc->psy_charger_stat = devm_kzalloc(dev, sizeof(char *)
						* (num_chgs + 1), GFP_KERNEL);
		if (desc->psy_charger_stat) {
			for (i = 0; i < num_chgs; i++)
				of_property_read_string_index(np, "cm-chargers",
						i, &desc->psy_charger_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	/* fast chargers */
	num_chgs = of_property_count_strings(np, "cm-fast-chargers");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_fast_charger_stat = devm_kzalloc(dev, sizeof(char *)
						* (num_chgs + 1), GFP_KERNEL);
		if (desc->psy_fast_charger_stat) {
			for (i = 0; i < num_chgs; i++)
				of_property_read_string_index(np, "cm-fast-chargers",
						i, &desc->psy_fast_charger_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	/* charge pumps */
	num_chgs = of_property_count_strings(np, "cm-charge-pumps");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->cp_nums = num_chgs;
		desc->psy_cp_stat = devm_kzalloc(dev, sizeof(char *)
						* (num_chgs + 1), GFP_KERNEL);
		if (desc->psy_cp_stat) {
			for (i = 0; i < num_chgs; i++)
				of_property_read_string_index(np, "cm-charge-pumps",
						i, &desc->psy_cp_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	/* wireless chargers */
	num_chgs = of_property_count_strings(np, "cm-wireless-chargers");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_wl_charger_stat = devm_kzalloc(dev,
							 sizeof(char *) * (num_chgs + 1),
							 GFP_KERNEL);
		if (desc->psy_wl_charger_stat) {
			for (i = 0; i < num_chgs; i++)
				of_property_read_string_index(np, "cm-wireless-chargers",
						i, &desc->psy_wl_charger_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	/* wireless charge pump converters */
	num_chgs = of_property_count_strings(np, "cm-wireless-charge-pump-converters");
	if (num_chgs > 0) {
		/* Allocate empty bin at the tail of array */
		desc->psy_cp_converter_stat = devm_kzalloc(dev,
							   sizeof(char *) * (num_chgs + 1),
							   GFP_KERNEL);
		if (desc->psy_cp_converter_stat) {
			for (i = 0; i < num_chgs; i++)
				of_property_read_string_index(np, "cm-wireless-charge-pump-converters",
						i, &desc->psy_cp_converter_stat[i]);
		} else {
			return ERR_PTR(-ENOMEM);
		}
	}

	of_property_read_string(np, "cm-fuel-gauge", &desc->psy_fuel_gauge);

	of_property_read_string(np, "cm-thermal-zone", &desc->thermal_zone);

	of_property_read_u32(np, "cm-battery-cold", &desc->temp_min);
	if (of_get_property(np, "cm-battery-cold-in-minus", NULL))
		desc->temp_min *= -1;
	of_property_read_u32(np, "cm-battery-hot", &desc->temp_max);
	of_property_read_u32(np, "cm-battery-temp-diff", &desc->temp_diff);

	of_property_read_u32(np, "cm-charging-max",
				&desc->charging_max_duration_ms);
	of_property_read_u32(np, "cm-discharging-max",
				&desc->discharging_max_duration_ms);
	of_property_read_u32(np, "cm-charge-voltage-max",
			     &desc->normal_charge_voltage_max);
	of_property_read_u32(np, "cm-charge-voltage-drop",
			     &desc->normal_charge_voltage_drop);
	of_property_read_u32(np, "cm-fast-charge-voltage-max",
			     &desc->fast_charge_voltage_max);
	of_property_read_u32(np, "cm-fast-charge-voltage-drop",
			     &desc->fast_charge_voltage_drop);
	of_property_read_u32(np, "cm-flash-charge-voltage-max",
			     &desc->flash_charge_voltage_max);
	of_property_read_u32(np, "cm-flash-charge-voltage-drop",
			     &desc->flash_charge_voltage_drop);
	of_property_read_u32(np, "cm-wireless-charge-voltage-max",
			     &desc->wireless_normal_charge_voltage_max);
	of_property_read_u32(np, "cm-wireless-charge-voltage-drop",
			     &desc->wireless_normal_charge_voltage_drop);
	of_property_read_u32(np, "cm-wireless-fast-charge-voltage-max",
			     &desc->wireless_fast_charge_voltage_max);
	of_property_read_u32(np, "cm-wireless-fast-charge-voltage-drop",
			     &desc->wireless_fast_charge_voltage_drop);
	of_property_read_u32(np, "cm-cp-taper-current",
			     &desc->cp.cp_taper_current);
	of_property_read_u32(np, "cm-ir-rc", &desc->ir_comp.rc);
	of_property_read_u32(np, "cm-ir-us-upper-limit-microvolt",
			     &desc->ir_comp.us_upper_limit);
	of_property_read_u32(np, "cm-ir-cv-offset-microvolt",
			     &desc->ir_comp.cp_upper_limit_offset);
	of_property_read_u32(np, "cm-force-jeita-status",
			     &desc->force_jeita_status);
	of_property_read_u32(np, "cm-cap-full-advance-percent",
			     &desc->cap_remap_full_percent);

	/* Initialize the jeita temperature table. */
	ret = cm_init_jeita_table(desc, dev);
	if (ret)
		return ERR_PTR(ret);

	if (!desc->force_jeita_status)
		desc->force_jeita_status = (desc->jeita_tab_size + 1) / 2;

	ret = cm_init_cap_remap_table(desc, dev);
	if (ret)
		dev_info(dev, "%s init cap remap table fail\n", __func__);

	/* battery charger regualtors */
	desc->num_charger_regulators = of_get_child_count(np);
	dev_info(dev, "num_charger_regulators = %d\n", desc->num_charger_regulators);
	if (desc->enable_multi_charger_adapt && (desc->num_charger_regulators >= 1))
		desc->num_charger_regulators -= 1;
	if (desc->num_charger_regulators) {
		struct charger_regulator *chg_regs;
		struct device_node *child;

		chg_regs = devm_kzalloc(dev, sizeof(*chg_regs)
					* desc->num_charger_regulators,
					GFP_KERNEL);
		if (!chg_regs)
			return ERR_PTR(-ENOMEM);

		desc->charger_regulators = chg_regs;

		for_each_child_of_node(np, child) {
			struct charger_cable *cables;
			struct device_node *_child;

			if (desc->enable_multi_charger_adapt) {
				of_property_read_string(child, "regulator-name",
						&chg_regs->regulator_name);
				if (!strcmp(chg_regs->regulator_name, "vddvbus"))
					continue;
			}

			of_property_read_string(child, "cm-regulator-name",
					&chg_regs->regulator_name);

			/* charger cables */
			chg_regs->num_cables = of_get_child_count(child);
			if (chg_regs->num_cables) {
				cables = devm_kzalloc(dev, sizeof(*cables)
						* chg_regs->num_cables,
						GFP_KERNEL);
				if (!cables) {
					of_node_put(child);
					return ERR_PTR(-ENOMEM);
				}

				chg_regs->cables = cables;

				for_each_child_of_node(child, _child) {
					of_property_read_string(_child,
					"cm-cable-name", &cables->name);
					of_property_read_u32(_child,
					"cm-cable-min",
					&cables->min_uA);
					of_property_read_u32(_child,
					"cm-cable-max",
					&cables->max_uA);

					if (of_property_read_bool(_child, "extcon")) {
						struct device_node *np;

						np = of_parse_phandle(_child, "extcon", 0);
						if (!np)
							return ERR_PTR(-ENODEV);

						cables->extcon_dev = extcon_find_edev_by_node(np);
						of_node_put(np);
						if (IS_ERR(cables->extcon_dev))
							return ERR_PTR(PTR_ERR(cables->extcon_dev));
					}
					cables++;
				}
			}
			chg_regs++;
		}
	}
	return desc;
}

static inline struct charger_desc *cm_get_drv_data(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		return of_cm_parse_desc(&pdev->dev);
	return dev_get_platdata(&pdev->dev);
}

static int cm_get_bat_info(struct charger_manager *cm)
{
	struct power_supply_battery_info info = { };
	struct power_supply_battery_ocv_table *table;
	int ret;

	ret = power_supply_get_battery_info(cm->charger_psy, &info, 0);
	if (ret) {
		dev_err(cm->dev, "failed to get battery information\n");
		return ret;
	}

	cm->desc->internal_resist = info.factory_internal_resistance_uohm / 1000;
	cm->desc->ir_comp.us = info.constant_charge_voltage_max_uv;
	cm->desc->fchg_ocv_threshold = info.fchg_ocv_threshold;
	cm->desc->cp.cp_target_vbat = info.constant_charge_voltage_max_uv;
	cm->desc->cp.cp_max_ibat = info.cur.flash_cur;
	cm->desc->cp.cp_target_ibat = info.cur.flash_cur;
	cm->desc->cp.cp_max_ibus = info.cur.flash_limit;
	cm->desc->cur.sdp_limit = info.cur.sdp_limit;
	cm->desc->cur.sdp_cur = info.cur.sdp_cur;
	cm->desc->cur.dcp_limit = info.cur.dcp_limit;
	cm->desc->cur.dcp_cur = info.cur.dcp_cur;
	cm->desc->cur.cdp_limit = info.cur.cdp_limit;
	cm->desc->cur.cdp_cur = info.cur.cdp_cur;
	cm->desc->cur.aca_limit = info.cur.aca_limit;
	cm->desc->cur.aca_cur = info.cur.aca_cur;
	cm->desc->cur.unknown_limit = info.cur.unknown_limit;
	cm->desc->cur.unknown_cur = info.cur.unknown_cur;
	cm->desc->cur.fchg_limit = info.cur.fchg_limit;
	cm->desc->cur.fchg_cur = info.cur.fchg_cur;
	cm->desc->cur.flash_limit = info.cur.flash_limit;
	cm->desc->cur.flash_cur = info.cur.flash_cur;
	cm->desc->cur.wl_bpp_limit = info.cur.wl_bpp_limit;
	cm->desc->cur.wl_bpp_cur = info.cur.wl_bpp_cur;
	cm->desc->cur.wl_epp_limit = info.cur.wl_epp_limit;
	cm->desc->cur.wl_epp_cur = info.cur.wl_epp_cur;

	/*
	 * For CHARGER MANAGER device, we only use one ocv-capacity
	 * table in normal temperature 20 Celsius.
	 */
	table = power_supply_find_ocv2cap_table(&info, 20, &cm->desc->cap_table_len);
	if (!table)
		return -EINVAL;

	cm->desc->cap_table = devm_kmemdup(cm->dev, table,
				     cm->desc->cap_table_len * sizeof(*table),
				     GFP_KERNEL);
	if (!cm->desc->cap_table) {
		power_supply_put_battery_info(cm->charger_psy, &info);
		return -ENOMEM;
	}

	power_supply_put_battery_info(cm->charger_psy, &info);

	return 0;
}

static void cm_uvlo_check_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, uvlo_work);
	int batt_uV, ret;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "get_vbat_now_uV error.\n");
		return;
	}

	if (batt_uV < cm->desc->shutdown_voltage)
		cm->desc->uvlo_trigger_cnt++;
	else
		cm->desc->uvlo_trigger_cnt = 0;

	if (cm->desc->uvlo_trigger_cnt >= CM_UVLO_CALIBRATION_CNT_THRESHOLD) {
		dev_err(cm->dev, "WARN: batt_uV less than uvlo, will shutdown\n");
		set_batt_cap(cm, 0);
		switch (cm->desc->uvlo_shutdown_mode) {
		case CM_SHUTDOWN_MODE_ORDERLY:
			orderly_poweroff(true);
			break;

		case CM_SHUTDOWN_MODE_KERNEL:
			kernel_power_off();
			break;

		case CM_SHUTDOWN_MODE_ANDROID:
			cancel_delayed_work_sync(&cm->cap_update_work);
			cm->desc->cap = 0;
			power_supply_changed(cm->charger_psy);
			break;

		default:
			dev_warn(cm->dev, "Incorrect uvlo_shutdown_mode (%d)\n",
				 cm->desc->uvlo_shutdown_mode);
		}
	}

	if (batt_uV < CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD)
		schedule_delayed_work(&cm->uvlo_work, msecs_to_jiffies(800));
}

static void cm_batt_works(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct charger_manager *cm = container_of(dwork,
				struct charger_manager, cap_update_work);
	struct timespec64 cur_time;
	int batt_uV, batt_ocV, batt_uA, fuel_cap, ret;
	int period_time, flush_time, cur_temp, board_temp = 0;
	int chg_cur = 0, chg_limit_cur = 0, input_cur = 0;
	int chg_vol = 0, vbat_avg = 0, ibat_avg = 0, recharge_uv = 0;
	static int last_fuel_cap = CM_MAGIC_NUM;

	ret = get_vbat_now_uV(cm, &batt_uV);
	if (ret) {
		dev_err(cm->dev, "get_vbat_now_uV error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_vbat_avg_uV(cm, &vbat_avg);
	if (ret)
		dev_err(cm->dev, "get_vbat_avg_uV error.\n");

	ret = get_batt_ocv(cm, &batt_ocV);
	if (ret) {
		dev_err(cm->dev, "get_batt_ocV error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_ibat_now_uA(cm, &batt_uA);
	if (ret) {
		dev_err(cm->dev, "get batt_uA error.\n");
		goto schedule_cap_update_work;
	}

	ret = get_ibat_avg_uA(cm, &ibat_avg);
	if (ret)
		dev_err(cm->dev, "get ibat_avg_uA error.\n");

	ret = get_batt_cap(cm, &fuel_cap);
	if (ret) {
		dev_err(cm->dev, "get fuel_cap error.\n");
		goto schedule_cap_update_work;
	}
	fuel_cap = cm_capacity_remap(cm, fuel_cap);

	ret = get_charger_current(cm, &chg_cur);
	if (ret)
		dev_warn(cm->dev, "get chg_cur error.\n");

	ret = get_charger_limit_current(cm, &chg_limit_cur);
	if (ret)
		dev_warn(cm->dev, "get chg_limit_cur error.\n");

	if (cm->desc->cp.cp_running)
		ret = get_cp_ibus_uA(cm, &input_cur);
	else
		ret = get_charger_input_current(cm, &input_cur);
	if (ret)
		dev_warn(cm->dev, "cant not get input_cur.\n");

	ret = get_charger_voltage(cm, &chg_vol);
	if (ret)
		dev_warn(cm->dev, "get chg_vol error.\n");

	ret = cm_get_battery_temperature_by_psy(cm, &cur_temp);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		goto schedule_cap_update_work;
	}

	cm->desc->temperature = cur_temp;

	ret = cm_get_battery_temperature(cm, &board_temp);
	if (ret)
		dev_warn(cm->dev, "failed to get board temperature\n");

	if (cur_temp <= CM_LOW_TEMP_REGION &&
	    batt_uV <= CM_LOW_TEMP_SHUTDOWN_VALTAGE) {
		if (cm->desc->low_temp_trigger_cnt++ > 1)
			fuel_cap = 0;
	} else if (cm->desc->low_temp_trigger_cnt != 0) {
		cm->desc->low_temp_trigger_cnt = 0;
	}

	if (fuel_cap > CM_CAP_FULL_PERCENT)
		fuel_cap = CM_CAP_FULL_PERCENT;
	else if (fuel_cap < 0)
		fuel_cap = 0;

	if (last_fuel_cap == CM_MAGIC_NUM)
		last_fuel_cap = fuel_cap;

	cur_time = ktime_to_timespec64(ktime_get_boottime());

	if (is_full_charged(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_FULL;
	else if (is_charging(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_CHARGING;
	else if (is_ext_pwr_online(cm))
		cm->battery_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	else
		cm->battery_status = POWER_SUPPLY_STATUS_DISCHARGING;

	/*
	 * Record the charging time when battery
	 * capacity is larger than 99%.
	 */
	if (cm->battery_status == POWER_SUPPLY_STATUS_CHARGING) {
		if (cm->desc->cap >= 986) {
			cm->desc->trickle_time =
				cur_time.tv_sec - cm->desc->trickle_start_time;
		} else {
			cm->desc->trickle_start_time = cur_time.tv_sec;
			cm->desc->trickle_time = 0;
		}
	} else {
		cm->desc->trickle_start_time = cur_time.tv_sec;
		cm->desc->trickle_time = cm->desc->trickle_time_out +
				cm->desc->cap_one_time;
	}

	flush_time = cur_time.tv_sec - cm->desc->update_capacity_time;
	period_time = cur_time.tv_sec - cm->desc->last_query_time;
	cm->desc->last_query_time = cur_time.tv_sec;

	if (cm->desc->force_set_full && is_ext_pwr_online(cm))
		cm->desc->charger_status = POWER_SUPPLY_STATUS_FULL;
	else
		cm->desc->charger_status = cm->battery_status;

	dev_info(cm->dev, "vbat: %d, vbat_avg: %d, OCV: %d, ibat: %d, ibat_avg: %d, ibus: %d,"
		 " vbus: %d, msoc: %d, chg_sts: %d, frce_full: %d, chg_lmt_cur: %d,"
		 " inpt_lmt_cur: %d, chgr_type: %d, Tboard: %d, Tbatt: %d, thm_cur: %d,"
		 " thm_pwr: %d, is_fchg: %d, fchg_en: %d, tflush: %d, tperiod: %d\n",
		 batt_uV, vbat_avg, batt_ocV, batt_uA, ibat_avg, input_cur, chg_vol, fuel_cap,
		 cm->desc->charger_status, cm->desc->force_set_full, chg_cur, chg_limit_cur,
		 cm->desc->charger_type, board_temp, cur_temp,
		 cm->desc->thm_info.thm_adjust_cur, cm->desc->thm_info.thm_pwr,
		 cm->desc->is_fast_charge, cm->desc->enable_fast_charge, flush_time, period_time);

	switch (cm->desc->charger_status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		last_fuel_cap = fuel_cap;
		if (fuel_cap < cm->desc->cap) {
			if (batt_uA >= 0) {
				fuel_cap = cm->desc->cap;
			} else {
				if (period_time < cm->desc->cap_one_time) {
					/*
					 * The percentage of electricity is not
					 * allowed to change by 1% in cm->desc->cap_one_time.
					 */
					if ((cm->desc->cap - fuel_cap) >= 5)
						fuel_cap = cm->desc->cap - 5;
					if (flush_time < cm->desc->cap_one_time &&
					    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
					    DIV_ROUND_CLOSEST(cm->desc->cap, 10))
						fuel_cap = cm->desc->cap;
				} else {
					/*
					 * If wake up from long sleep mode,
					 * will make a percentage compensation based on time.
					 */
					if ((cm->desc->cap - fuel_cap) >=
					    (period_time / cm->desc->cap_one_time) * 10)
						fuel_cap = cm->desc->cap -
							(period_time / cm->desc->cap_one_time) * 10;
				}
			}
		} else if (fuel_cap > cm->desc->cap) {
			if (period_time < cm->desc->cap_one_time) {
				if ((fuel_cap - cm->desc->cap) >= 5)
					fuel_cap = cm->desc->cap + 5;
				if (flush_time < cm->desc->cap_one_time &&
				    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
				    DIV_ROUND_CLOSEST(cm->desc->cap, 10))
					fuel_cap = cm->desc->cap;
			} else {
				/*
				 * If wake up from long sleep mode,
				 * will make a percentage compensation based on time.
				 */
				if ((fuel_cap - cm->desc->cap) >=
				    (period_time / cm->desc->cap_one_time) * 10)
					fuel_cap = cm->desc->cap +
						(period_time / cm->desc->cap_one_time) * 10;
			}
		}

		if (cm->desc->cap >= 985 && cm->desc->cap <= 994 &&
		    fuel_cap >= CM_CAP_FULL_PERCENT)
			fuel_cap = 994;
		/*
		 * Record 99% of the charging time.
		 * if it is greater than 1500s,
		 * it will be mandatory to display 100%,
		 * but the background is still charging.
		 */
		if (cm->desc->cap >= 995 &&
		    cm->desc->trickle_time >= cm->desc->trickle_time_out &&
		    cm->desc->trickle_time_out > 0 &&
		    batt_uA > 0)
			cm->desc->force_set_full = true;

		break;

	case POWER_SUPPLY_STATUS_NOT_CHARGING:
	case POWER_SUPPLY_STATUS_DISCHARGING:
		/*
		 * In not charging status,
		 * the cap is not allowed to increase.
		 */
		if (fuel_cap >= cm->desc->cap) {
			last_fuel_cap = fuel_cap;
			fuel_cap = cm->desc->cap;
		} else if (cm->desc->cap >= CM_HCAP_THRESHOLD) {
			if (last_fuel_cap - fuel_cap >= CM_HCAP_DECREASE_STEP) {
				if (cm->desc->cap - fuel_cap >= CM_CAP_ONE_PERCENT)
					fuel_cap = cm->desc->cap - CM_CAP_ONE_PERCENT;
				else
					fuel_cap = cm->desc->cap - CM_HCAP_DECREASE_STEP;

				last_fuel_cap -= CM_HCAP_DECREASE_STEP;
			} else {
				fuel_cap = cm->desc->cap;
			}
		} else {
			if (period_time < cm->desc->cap_one_time) {
				if ((cm->desc->cap - fuel_cap) >= 5)
					fuel_cap = cm->desc->cap - 5;
				if (flush_time < cm->desc->cap_one_time &&
				    DIV_ROUND_CLOSEST(fuel_cap, 10) !=
				    DIV_ROUND_CLOSEST(cm->desc->cap, 10))
					fuel_cap = cm->desc->cap;
			} else {
				/*
				 * If wake up from long sleep mode,
				 * will make a percentage compensation based on time.
				 */
				if ((cm->desc->cap - fuel_cap) >=
				    (period_time / cm->desc->cap_one_time) * 10)
					fuel_cap = cm->desc->cap -
						(period_time / cm->desc->cap_one_time) * 10;
			}
		}
		break;

	case POWER_SUPPLY_STATUS_FULL:
		last_fuel_cap = fuel_cap;
		cm->desc->update_capacity_time = cur_time.tv_sec;
		recharge_uv = cm->desc->fullbatt_uV - cm->desc->fullbatt_vchkdrop_uV - 50000;
		if ((batt_ocV < recharge_uv) && (batt_uA < 0)) {
			cm->desc->force_set_full = false;
			dev_info(cm->dev, "recharge_uv = %d\n", recharge_uv);
		}

		if (is_ext_pwr_online(cm)) {
			if (fuel_cap != CM_CAP_FULL_PERCENT)
				fuel_cap = CM_CAP_FULL_PERCENT;

			if (fuel_cap > cm->desc->cap)
				fuel_cap = cm->desc->cap + 1;
		}

		break;
	default:
		break;
	}

	if (batt_uV < CM_UVLO_CALIBRATION_VOLTAGE_THRESHOLD) {
		dev_info(cm->dev, "batt_uV is less than UVLO calib volt\n");
		schedule_delayed_work(&cm->uvlo_work, msecs_to_jiffies(100));
	}

	dev_info(cm->dev, "new_uisoc = %d, old_uisoc = %d\n", fuel_cap, cm->desc->cap);

	if (fuel_cap != cm->desc->cap) {
		if (DIV_ROUND_CLOSEST(fuel_cap, 10) != DIV_ROUND_CLOSEST(cm->desc->cap, 10)) {
			cm->desc->cap = fuel_cap;
			cm->desc->update_capacity_time = cur_time.tv_sec;
			power_supply_changed(cm->charger_psy);
		}

		cm->desc->cap = fuel_cap;
		if (cm->desc->uvlo_trigger_cnt < CM_UVLO_CALIBRATION_CNT_THRESHOLD)
			set_batt_cap(cm, cm->desc->cap);
	}

schedule_cap_update_work:
	queue_delayed_work(system_power_efficient_wq,
			   &cm->cap_update_work,
			   CM_CAP_CYCLE_TRACK_TIME * HZ);
}

static int charger_manager_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct charger_desc *desc = cm_get_drv_data(pdev);
	struct charger_manager *cm;
	int ret, i = 0;
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	struct power_supply_config psy_cfg = {};
	struct timespec64 cur_time;
	int chg_num = 0;

	if (IS_ERR(desc)) {
		dev_err(&pdev->dev, "No platform data (desc) found\n");
		return PTR_ERR(desc);
	}

	cm = devm_kzalloc(&pdev->dev, sizeof(*cm), GFP_KERNEL);
	if (!cm)
		return -ENOMEM;

	/* Basic Values. Unspecified are Null or 0 */
	cm->dev = &pdev->dev;
	cm->desc = desc;
	psy_cfg.drv_data = cm;

	/* Initialize alarm timer */
	if (alarmtimer_get_rtcdev()) {
		cm_timer = devm_kzalloc(cm->dev, sizeof(*cm_timer), GFP_KERNEL);
		if (!cm_timer)
			return -ENOMEM;
		alarm_init(cm_timer, ALARM_BOOTTIME, NULL);
	}

	/*
	 * Some of the following do not need to be errors.
	 * Users may intentionally ignore those features.
	 */
	if (desc->fullbatt_uV == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery voltage threshold as it is not supplied\n");
	}

	if (desc->fullbatt_uA == 0)
		dev_info(&pdev->dev, "Ignoring full-battery current threshold as it is not supplied\n");

	if (!desc->fullbatt_vchkdrop_ms || !desc->fullbatt_vchkdrop_uV) {
		dev_info(&pdev->dev, "Disabling full-battery voltage drop checking mechanism as it is not supplied\n");
		desc->fullbatt_vchkdrop_ms = 0;
		desc->fullbatt_vchkdrop_uV = 0;
	}
	if (desc->fullbatt_soc == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery soc(state of charge) threshold as it is not supplied\n");
	}
	if (desc->fullbatt_full_capacity == 0) {
		dev_info(&pdev->dev, "Ignoring full-battery full capacity threshold as it is not supplied\n");
	}

	if (!desc->charger_regulators || desc->num_charger_regulators < 1) {
		dev_err(&pdev->dev, "charger_regulators undefined\n");
		return -EINVAL;
	}

	if (!desc->psy_charger_stat || !desc->psy_charger_stat[0]) {
		dev_err(&pdev->dev, "No power supply defined\n");
		return -EINVAL;
	}

	if (desc->enable_multi_charger_adapt) {
		if (!desc->psy_stat || !desc->psy_stat[0]) {
			dev_err(&pdev->dev, "No power supply defined in dts\n");
			return -EINVAL;
		}
	}

	if (!desc->psy_fuel_gauge) {
		dev_err(&pdev->dev, "No fuel gauge power supply defined\n");
		return -EINVAL;
	}

	/* Check if charger's supplies are present at probe */
	if ((desc->psy_nums > 0) && desc->enable_multi_charger_adapt) {
		for (i = 0; desc->psy_stat[i]; i++) {
			struct power_supply *psy;

			psy = power_supply_get_by_name(desc->psy_stat[i]);
			if (!psy) {
				dev_warn(&pdev->dev, "Cannot find power supply \"%s\"\n",
					desc->psy_stat[i]);
			} else {
				chg_num++;
				dev_info(&pdev->dev, "find power supply \"%s\"\n",
					desc->psy_stat[i]);
				power_supply_put(psy);
			}

			dev_info(&pdev->dev, "i = %d, chg_num = %d\n", i, chg_num);

			if (chg_num >= desc->buck_nums)
				break;
		}

		if (i == desc->psy_nums) {
			dev_err(&pdev->dev, "Cannot find all power supply\n");
			return -EPROBE_DEFER;
		}

		if (desc->psy_stat[i])
			desc->psy_charger_stat[0] = desc->psy_stat[i];
	} else {
		for (i = 0; desc->psy_charger_stat[i]; i++) {
			struct power_supply *psy;

			psy = power_supply_get_by_name(desc->psy_charger_stat[i]);
			if (!psy) {
				dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
					desc->psy_charger_stat[i]);
				return -EPROBE_DEFER;
			}
			power_supply_put(psy);
		}
	}

	if (desc->polling_interval_ms == 0 ||
	    msecs_to_jiffies(desc->polling_interval_ms) <= CM_JIFFIES_SMALL) {
		dev_err(&pdev->dev, "polling_interval_ms is too small\n");
		return -EINVAL;
	}

	if (!desc->charging_max_duration_ms ||
			!desc->discharging_max_duration_ms) {
		dev_info(&pdev->dev, "Cannot limit charging duration checking mechanism to prevent overcharge/overheat and control discharging duration\n");
		desc->charging_max_duration_ms = 0;
		desc->discharging_max_duration_ms = 0;
	}

	if (!desc->charge_voltage_max || !desc->charge_voltage_drop) {
		dev_info(&pdev->dev, "Cannot validate charge voltage\n");
		desc->charge_voltage_max = 0;
		desc->charge_voltage_drop = 0;
	}

	platform_set_drvdata(pdev, cm);

	memcpy(&cm->charger_psy_desc, &psy_default, sizeof(psy_default));

	if (!desc->psy_name)
		strncpy(cm->psy_name_buf, psy_default.name, PSY_NAME_MAX);
	else
		strncpy(cm->psy_name_buf, desc->psy_name, PSY_NAME_MAX);
	cm->charger_psy_desc.name = cm->psy_name_buf;

	/* Allocate for psy properties because they may vary */
	cm->charger_psy_desc.properties = devm_kzalloc(&pdev->dev,
				sizeof(enum power_supply_property)
				* (ARRAY_SIZE(default_charger_props) +
				NUM_CHARGER_PSY_OPTIONAL), GFP_KERNEL);
	if (!cm->charger_psy_desc.properties)
		return -ENOMEM;

	memcpy(cm->charger_psy_desc.properties, default_charger_props,
		sizeof(enum power_supply_property) *
		ARRAY_SIZE(default_charger_props));
	cm->charger_psy_desc.num_properties = psy_default.num_properties;

	/* Find which optional psy-properties are available */
	fuel_gauge = power_supply_get_by_name(desc->psy_fuel_gauge);
	if (!fuel_gauge) {
		dev_err(&pdev->dev, "Cannot find power supply \"%s\"\n",
			desc->psy_fuel_gauge);
		return -EPROBE_DEFER;
	}
	if (!power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CHARGE_NOW, &val)) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_CHARGE_NOW))
			dev_warn(&pdev->dev, "POWER_SUPPLY_PROP_CHARGE_NOW is present\n");
	}

	if (!power_supply_get_property(fuel_gauge, POWER_SUPPLY_PROP_CURRENT_NOW, &val)) {
		if (!cm_add_battery_psy_property(cm, POWER_SUPPLY_PROP_CURRENT_NOW))
			dev_warn(&pdev->dev, "POWER_SUPPLY_PROP_CURRENT_NOW is present\n");
	}

	ret = get_boot_cap(cm, &cm->desc->cap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get initial battery capacity\n");
		return ret;
	}

	cm->desc->thm_info.thm_adjust_cur = -EINVAL;
	cm->desc->ir_comp.ibat_buf[CM_IBAT_BUFF_CNT - 1] = CM_MAGIC_NUM;
	cm->desc->ir_comp.us_lower_limit = cm->desc->ir_comp.us;

	if (device_property_read_bool(&pdev->dev, "cm-support-linear-charge"))
		cm->desc->thm_info.need_calib_charge_lmt = true;

	ret = cm_get_battery_temperature_by_psy(cm, &cm->desc->temperature);
	if (ret) {
		dev_err(cm->dev, "failed to get battery temperature\n");
		return ret;
	}

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cm->desc->update_capacity_time = cur_time.tv_sec;
	cm->desc->last_query_time = cur_time.tv_sec;

	ret = cm_init_thermal_data(cm, fuel_gauge);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize thermal data\n");
		cm->desc->measure_battery_temp = false;
	}
	power_supply_put(fuel_gauge);

	INIT_DELAYED_WORK(&cm->fullbatt_vchk_work, fullbatt_vchk);
	INIT_DELAYED_WORK(&cm->cap_update_work, cm_batt_works);
	INIT_DELAYED_WORK(&cm->cp_work, cm_cp_work);
	INIT_DELAYED_WORK(&cm->ir_compensation_work, cm_ir_compensation_works);

	psy_cfg.of_node = np;
	cm->charger_psy = power_supply_register(&pdev->dev,
						&cm->charger_psy_desc,
						&psy_cfg);
	if (IS_ERR(cm->charger_psy)) {
		dev_err(&pdev->dev, "Cannot register charger-manager with name \"%s\"\n",
			cm->charger_psy_desc.name);
		return PTR_ERR(cm->charger_psy);
	}
	cm->charger_psy->supplied_to = charger_manager_supplied_to;
	cm->charger_psy->num_supplicants =
		ARRAY_SIZE(charger_manager_supplied_to);

	wireless_main.psy = power_supply_register(&pdev->dev, &wireless_main.psd, NULL);
	if (IS_ERR(wireless_main.psy)) {
		dev_err(&pdev->dev, "Cannot register wireless_main.psy with name \"%s\"\n",
			wireless_main.psd.name);
		return PTR_ERR(wireless_main.psy);

	}

	ac_main.psy = power_supply_register(&pdev->dev, &ac_main.psd, NULL);
	if (IS_ERR(ac_main.psy)) {
		dev_err(&pdev->dev, "Cannot register usb_main.psy with name \"%s\"\n",
			ac_main.psd.name);
		return PTR_ERR(ac_main.psy);

	}

	usb_main.psy = power_supply_register(&pdev->dev, &usb_main.psd, NULL);
	if (IS_ERR(usb_main.psy)) {
		dev_err(&pdev->dev, "Cannot register usb_main.psy with name \"%s\"\n",
			usb_main.psd.name);
		return PTR_ERR(usb_main.psy);

	}

	/* Register extcon device for charger cable */
	ret = charger_manager_register_extcon(cm);
	if (ret < 0) {
		dev_err(&pdev->dev, "Cannot initialize extcon device\n");
		goto err_reg_extcon;
	}

	/* Register sysfs entry for charger(regulator) */
	ret = charger_manager_register_sysfs(cm);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Cannot initialize sysfs entry of regulator\n");
		goto err_reg_sysfs;
	}

	/* Add to the list */
	mutex_lock(&cm_list_mtx);
	list_add(&cm->entry, &cm_list);
	mutex_unlock(&cm_list_mtx);

	/*
	 * Charger-manager is capable of waking up the system from sleep
	 * when event is happened through cm_notify_event()
	 */
	device_init_wakeup(&pdev->dev, true);
	device_set_wakeup_capable(&pdev->dev, false);
	wakeup_source_init(&cm->charge_ws, "charger_manager_wakelock");
	mutex_init(&cm->desc->charger_type_mtx);

	ret = cm_get_bat_info(cm);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get battery information\n");
		goto err_reg_sysfs;
	}

	cm->cm_charge_vote = sprd_charge_vote_register("cm_charge_vote",
						       cm_sprd_vote_callback,
						       cm,
						       &cm->charger_psy->dev);
	if (IS_ERR(cm->cm_charge_vote)) {
		dev_err(&pdev->dev, "Failed to register charge vote, ret = %d\n", ret);
		goto err_reg_sysfs;
	}

	ret = cm_charger_register_vbus_regulator(cm);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register cm otg vbus, ret = %d\n", ret);
		goto err_reg_sysfs;
	}

	global_cm = cm;
	if (cm_event_type)
		cm_notify_type_handle(cm, cm_event_type, cm_event_msg);

	/*
	 * Charger-manager have to check the charging state right after
	 * tialization of charger-manager and then update current charging
	 * state.
	 */
	cm_monitor();

	schedule_work(&setup_polling);

	queue_delayed_work(system_power_efficient_wq, &cm->cap_update_work, CM_CAP_CYCLE_TRACK_TIME * HZ);
	INIT_DELAYED_WORK(&cm->uvlo_work, cm_uvlo_check_work);

	return 0;

err_reg_sysfs:
	for (i = 0; i < desc->num_charger_regulators; i++) {
		struct charger_regulator *charger;

		charger = &desc->charger_regulators[i];
		sysfs_remove_group(&cm->charger_psy->dev.kobj,
				&charger->attr_g);
	}
err_reg_extcon:
	for (i = 0; i < desc->num_charger_regulators; i++)
		regulator_put(desc->charger_regulators[i].consumer);

	power_supply_unregister(cm->charger_psy);

	return ret;
}

static int charger_manager_remove(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);
	struct charger_desc *desc = cm->desc;
	int i = 0;

	/* Remove from the list */
	mutex_lock(&cm_list_mtx);
	list_del(&cm->entry);
	mutex_unlock(&cm_list_mtx);

	cancel_work_sync(&setup_polling);
	cancel_delayed_work_sync(&cm_monitor_work);
	cancel_delayed_work_sync(&cm->cap_update_work);
	cancel_delayed_work_sync(&cm->uvlo_work);

	for (i = 0 ; i < desc->num_charger_regulators ; i++)
		regulator_put(desc->charger_regulators[i].consumer);

	power_supply_unregister(cm->charger_psy);

	try_charger_enable(cm, false);
	wakeup_source_trash(&cm->charge_ws);

	return 0;
}

static void charger_manager_shutdown(struct platform_device *pdev)
{
	struct charger_manager *cm = platform_get_drvdata(pdev);

	if (cm->desc->uvlo_trigger_cnt < CM_UVLO_CALIBRATION_CNT_THRESHOLD)
		set_batt_cap(cm, cm->desc->cap);
}

static const struct platform_device_id charger_manager_id[] = {
	{ "charger-manager", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, charger_manager_id);

static int cm_suspend_noirq(struct device *dev)
{
	if (device_may_wakeup(dev)) {
		device_set_wakeup_capable(dev, false);
		return -EAGAIN;
	}

	return 0;
}

static int cm_suspend_prepare(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (!cm_suspended)
		cm_suspended = true;

	/*
	 * In some situation, the system is not sleep between
	 * the charger polling interval - 15s, it maybe occur
	 * that charger manager will feed watchdog, but the
	 * system has no work to do to suspend, and charger
	 * manager also suspend. In this function, it will
	 * cancel cm_monito_work, it cause that this time can't
	 * feed watchdog until the next polling time, this means
	 * that charger manager feed watchdog per 15s usually,
	 * but this time need 30s, and the charger IC(fan54015)
	 * watchdog timeout to reset.
	 */
	if (is_ext_pwr_online(cm))
		cm_feed_watchdog(cm);
	cm_timer_set = cm_setup_timer();

	if (cm_timer_set) {
		cancel_work_sync(&setup_polling);
		cancel_delayed_work_sync(&cm_monitor_work);
		cancel_delayed_work(&cm->fullbatt_vchk_work);
		cancel_delayed_work_sync(&cm->cap_update_work);
		cancel_delayed_work_sync(&cm->uvlo_work);
	}

	return 0;
}

static void cm_suspend_complete(struct device *dev)
{
	struct charger_manager *cm = dev_get_drvdata(dev);

	if (cm_suspended)
		cm_suspended = false;

	if (cm_timer_set) {
		ktime_t remain;

		alarm_cancel(cm_timer);
		cm_timer_set = false;
		remain = alarm_expires_remaining(cm_timer);
		if (remain > 0)
			cm_suspend_duration_ms -= ktime_to_ms(remain);
		schedule_work(&setup_polling);
	}

	_cm_monitor(cm);
	cm_batt_works(&cm->cap_update_work.work);

	/* Re-enqueue delayed work (fullbatt_vchk_work) */
	if (cm->fullbatt_vchk_jiffies_at) {
		unsigned long delay = 0;
		unsigned long now = jiffies + CM_JIFFIES_SMALL;

		if (time_after_eq(now, cm->fullbatt_vchk_jiffies_at)) {
			delay = (unsigned long)((long)now
				- (long)(cm->fullbatt_vchk_jiffies_at));
			delay = jiffies_to_msecs(delay);
		} else {
			delay = 0;
		}

		/*
		 * Account for cm_suspend_duration_ms with assuming that
		 * timer stops in suspend.
		 */
		if (delay > cm_suspend_duration_ms)
			delay -= cm_suspend_duration_ms;
		else
			delay = 0;

		queue_delayed_work(cm_wq, &cm->fullbatt_vchk_work,
				   msecs_to_jiffies(delay));
	}
	device_set_wakeup_capable(cm->dev, false);
}

static const struct dev_pm_ops charger_manager_pm = {
	.prepare	= cm_suspend_prepare,
	.suspend_noirq	= cm_suspend_noirq,
	.complete	= cm_suspend_complete,
};

static struct platform_driver charger_manager_driver = {
	.driver = {
		.name = "charger-manager",
		.pm = &charger_manager_pm,
		.of_match_table = charger_manager_match,
	},
	.probe = charger_manager_probe,
	.remove = charger_manager_remove,
	.shutdown = charger_manager_shutdown,
	.id_table = charger_manager_id,
};

static int __init charger_manager_init(void)
{
	cm_wq = create_freezable_workqueue("charger_manager");
	INIT_DELAYED_WORK(&cm_monitor_work, cm_monitor_poller);

	return platform_driver_register(&charger_manager_driver);
}
late_initcall(charger_manager_init);

static void __exit charger_manager_cleanup(void)
{
	destroy_workqueue(cm_wq);
	cm_wq = NULL;

	platform_driver_unregister(&charger_manager_driver);
}
module_exit(charger_manager_cleanup);

/**
 * cm_notify_type_handle - charger driver handle charger event
 * @cm: the Charger Manager representing the battery
 * @type: type of charger event
 * @msg: optional message passed to uevent_notify function
 */
static void cm_notify_type_handle(struct charger_manager *cm, enum cm_event_types type, char *msg)
{
	switch (type) {
	case CM_EVENT_BATT_FULL:
		fullbatt_handler(cm);
		break;
	case CM_EVENT_BATT_IN:
	case CM_EVENT_BATT_OUT:
		battout_handler(cm);
		break;
	case CM_EVENT_WL_CHG_START_STOP:
	case CM_EVENT_EXT_PWR_IN_OUT ... CM_EVENT_CHG_START_STOP:
		misc_event_handler(cm, type);
		break;
	case CM_EVENT_FAST_CHARGE:
		fast_charge_handler(cm);
		break;
	case CM_EVENT_INT:
		cm_charger_int_handler(cm);
		break;
	case CM_EVENT_UNKNOWN:
	case CM_EVENT_OTHERS:
	default:
		dev_err(cm->dev, "%s: type not specified\n", __func__);
		break;
	}

	power_supply_changed(cm->charger_psy);

}

/**
 * cm_notify_event - charger driver notify Charger Manager of charger event
 * @psy: pointer to instance of charger's power_supply
 * @type: type of charger event
 * @msg: optional message passed to uevent_notify function
 */
void cm_notify_event(struct power_supply *psy, enum cm_event_types type,
		     char *msg)
{
	struct charger_manager *cm;
	bool found_power_supply = false;

	if (psy == NULL)
		return;

	mutex_lock(&cm_list_mtx);
	list_for_each_entry(cm, &cm_list, entry) {
		if (cm->desc->psy_charger_stat) {
			if (match_string(cm->desc->psy_charger_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_fast_charger_stat) {
			if (match_string(cm->desc->psy_fast_charger_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_fuel_gauge) {
			if (match_string(&cm->desc->psy_fuel_gauge, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_cp_stat) {
			if (match_string(cm->desc->psy_cp_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}

		if (cm->desc->psy_wl_charger_stat) {
			if (match_string(cm->desc->psy_wl_charger_stat, -1,
					 psy->desc->name) >= 0) {
				found_power_supply = true;
				break;
			}
		}
	}
	mutex_unlock(&cm_list_mtx);

	if (!found_power_supply || !cm->cm_charge_vote) {
		cm_event_msg = msg;
		cm_event_type = type;
		return;
	}

	cm_notify_type_handle(cm, type, msg);
}
EXPORT_SYMBOL_GPL(cm_notify_event);

MODULE_AUTHOR("MyungJoo Ham <myungjoo.ham@samsung.com>");
MODULE_DESCRIPTION("Charger Manager");
MODULE_LICENSE("GPL");
