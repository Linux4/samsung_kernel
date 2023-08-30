/*
 *  sec_event_sysfs.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2018 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#if defined(CONFIG_SEC_COMMON)
#include <linux/sec_common.h>
#endif
#include "sec_battery.h"
#include "sec_battery_sysfs.h"

static ssize_t sysfs_event_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t sysfs_event_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SYSFS_EVENT_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sysfs_event_show_attrs,					\
	.store = sysfs_event_store_attrs,					\
}

static struct device_attribute sysfs_event_attrs[] = {
	SYSFS_EVENT_ATTR(batt_slate_mode),
	SYSFS_EVENT_ATTR(batt_lp_charging),
	SYSFS_EVENT_ATTR(siop_level),
	SYSFS_EVENT_ATTR(siop_event),
	SYSFS_EVENT_ATTR(factory_mode),
	SYSFS_EVENT_ATTR(store_mode),
	SYSFS_EVENT_ATTR(call),
	SYSFS_EVENT_ATTR(2g_call),
	SYSFS_EVENT_ATTR(talk_gsm),
	SYSFS_EVENT_ATTR(3g_call),
	SYSFS_EVENT_ATTR(talk_wcdma),
	SYSFS_EVENT_ATTR(music),
	SYSFS_EVENT_ATTR(video),
	SYSFS_EVENT_ATTR(browser),
	SYSFS_EVENT_ATTR(hotspot),
	SYSFS_EVENT_ATTR(camera),
	SYSFS_EVENT_ATTR(camcorder),
	SYSFS_EVENT_ATTR(data_call),
	SYSFS_EVENT_ATTR(wifi),
	SYSFS_EVENT_ATTR(wibro),
	SYSFS_EVENT_ATTR(lte),
	SYSFS_EVENT_ATTR(lcd),
#if defined(CONFIG_ISDB_CHARGING_CONTROL)
	SYSFS_EVENT_ATTR(batt_event_isdb),
#endif
	SYSFS_EVENT_ATTR(gps),
	SYSFS_EVENT_ATTR(event),
	SYSFS_EVENT_ATTR(batt_misc_event),
	SYSFS_EVENT_ATTR(batt_tx_event),
	SYSFS_EVENT_ATTR(batt_current_event),
	SYSFS_EVENT_ATTR(ext_event),
};

enum {
	BATT_SLATE_MODE = 0,
	BATT_LP_CHARGING,
	SIOP_LEVEL,
	SIOP_EVENT,
	FACTORY_MODE,
	STORE_MODE,
	BATT_EVENT_CALL,
	BATT_EVENT_2G_CALL,
	BATT_EVENT_TALK_GSM,
	BATT_EVENT_3G_CALL,
	BATT_EVENT_TALK_WCDMA,
	BATT_EVENT_MUSIC,
	BATT_EVENT_VIDEO,
	BATT_EVENT_BROWSER,
	BATT_EVENT_HOTSPOT,
	BATT_EVENT_CAMERA,
	BATT_EVENT_CAMCORDER,
	BATT_EVENT_DATA_CALL,
	BATT_EVENT_WIFI,
	BATT_EVENT_WIBRO,
	BATT_EVENT_LTE,
	BATT_EVENT_LCD,
#if defined(CONFIG_ISDB_CHARGING_CONTROL)
	BATT_EVENT_ISDB,
#endif
	BATT_EVENT_GPS,
	BATT_EVENT,
	BATT_MISC_EVENT,
	BATT_TX_EVENT,
	BATT_CURRENT_EVENT,
	EXT_EVENT,
};

ssize_t sysfs_event_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sysfs_event_attrs;
	int i = 0;

	switch (offset) {
	case BATT_SLATE_MODE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			is_slate_mode(battery));
		break;

	case BATT_LP_CHARGING:
		if (lpcharge) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       lpcharge ? 1 : 0);
		}
		break;
	case SIOP_LEVEL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->siop_level);
		break;
	case SIOP_EVENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			0);
		break;
	case FACTORY_MODE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->factory_mode);
		break;
	case STORE_MODE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->store_mode);
		break;
	case BATT_EVENT_CALL:
	case BATT_EVENT_2G_CALL:
	case BATT_EVENT_TALK_GSM:
		break;
	case BATT_EVENT_3G_CALL:
	case BATT_EVENT_TALK_WCDMA:
		break;
	case BATT_EVENT_MUSIC:
		break;
	case BATT_EVENT_VIDEO:
		break;
	case BATT_EVENT_BROWSER:
		break;
	case BATT_EVENT_HOTSPOT:
		break;
	case BATT_EVENT_CAMERA:
		break;
	case BATT_EVENT_CAMCORDER:
		break;
	case BATT_EVENT_DATA_CALL:
		break;
	case BATT_EVENT_WIFI:
		break;
	case BATT_EVENT_WIBRO:
		break;
	case BATT_EVENT_LTE:
		break;
	case BATT_EVENT_LCD:
		break;
#if defined(CONFIG_ISDB_CHARGING_CONTROL)
	case BATT_EVENT_ISDB:
		break;
#endif
	case BATT_EVENT_GPS:
		break;
	case BATT_EVENT:
		break;
	case BATT_MISC_EVENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->misc_event);
		break;
	case BATT_TX_EVENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->tx_event);
		break;
	case BATT_CURRENT_EVENT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->current_event);
		break;
	case EXT_EVENT:
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sysfs_event_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sysfs_event_attrs;
	int ret = -EINVAL;
	int x = 0;
#if defined(CONFIG_DIRECT_CHARGING) && !defined(CONFIG_SEC_FACTORY)
	char direct_charging_source_status[2] = {0, };
	union power_supply_propval value = {0, };
#elif defined(CONFIG_ISDB_CHARGING_CONTROL)
	union power_supply_propval value = {0, };
#endif

	switch (offset) {
	case BATT_SLATE_MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x == 2) {
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SLATE, SEC_BAT_CURRENT_EVENT_SLATE);
				sec_vote(battery->chgen_vote, VOTER_SMART_SLATE, true, SEC_BAT_CHG_MODE_BUCK_OFF);
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE) && defined(CONFIG_SEC_FACTORY)
				battery->usb_factory_slate_mode = true;
#endif
				dev_info(battery->dev,
					"%s: enable smart switch slate mode : %d\n", __func__, x);
			} else if (x == 1) {
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SLATE,
						SEC_BAT_CURRENT_EVENT_SLATE);
				sec_vote(battery->chgen_vote, VOTER_SLATE, true, SEC_BAT_CHG_MODE_BUCK_OFF);
				dev_info(battery->dev,
					"%s: enable slate mode : %d\n", __func__, x);
			} else if (x == 0) {
				sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SLATE);
				sec_vote(battery->chgen_vote, VOTER_SLATE, false, 0);
				sec_vote(battery->chgen_vote, VOTER_SMART_SLATE, false, 0);
				dev_info(battery->dev,
					"%s: disable slate mode : %d\n", __func__, x);
			} else {
				dev_info(battery->dev,
					"%s: SLATE MODE unknown command\n", __func__);
				return -EINVAL;
			}
			__pm_stay_awake(battery->cable_ws);
			queue_delayed_work(battery->monitor_wqueue,
					   &battery->cable_work, 0);
			ret = count;
		}
		break;
	case BATT_LP_CHARGING:
		break;
	case SIOP_LEVEL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev,
					"%s: siop level: %d\n", __func__, x);

			battery->wc_heating_start_time = 0;
			if (x == battery->siop_level) {
				dev_info(battery->dev,
					"%s: skip same siop level: %d\n", __func__, x);
				return count;
			} else if (x >= 0 && x <= 100 && battery->pdata->temp_check_type) {
				battery->siop_level = x;
				if (battery->siop_level == 0)
					sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_SIOP_LIMIT,
						SEC_BAT_CURRENT_EVENT_SIOP_LIMIT);
				else
					sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_SIOP_LIMIT);
			} else {
				battery->siop_level = 100;
			}

			__pm_stay_awake(battery->siop_level_ws);
			queue_delayed_work(battery->monitor_wqueue, &battery->siop_level_work, 0);

			ret = count;
		}
		break;
	case SIOP_EVENT:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case FACTORY_MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			battery->factory_mode = x ? true : false;
			ret = count;
		}
		break;
	case STORE_MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
#if !defined(CONFIG_SEC_FACTORY)
			if (x) {
				battery->store_mode = true;
				__pm_stay_awake(battery->parse_mode_dt_ws);
				queue_delayed_work(battery->monitor_wqueue,
					&battery->parse_mode_dt_work, 0);
#if defined(CONFIG_DIRECT_CHARGING)
				direct_charging_source_status[0] = SEC_STORE_MODE;
				direct_charging_source_status[1] = SEC_DIRECT_CHG_CHARGING_SOURCE_SWITCHING;
				value.strval = direct_charging_source_status;
				psy_do_property(battery->pdata->charger_name, set,
					POWER_SUPPLY_EXT_PROP_CHANGE_CHARGING_SOURCE, value);
#endif
			}
#endif
			ret = count;
		}
		break;
	case BATT_EVENT_CALL:
	case BATT_EVENT_2G_CALL:
	case BATT_EVENT_TALK_GSM:
	case BATT_EVENT_3G_CALL:
	case BATT_EVENT_TALK_WCDMA:
		if (sscanf(buf, "%10d\n", &x) == 1) {
#if defined(CONFIG_LIMIT_CHARGING_DURING_CALL)
			if (x)
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_CALL, SEC_BAT_CURRENT_EVENT_CALL);
			else
				sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_CALL);
			__pm_stay_awake(battery->monitor_ws);
			queue_delayed_work(battery->monitor_wqueue, &battery->monitor_work, 0);
#endif
			ret = count;
		}
		break;
	case BATT_EVENT_MUSIC:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT_VIDEO:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT_BROWSER:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT_HOTSPOT:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT_CAMERA:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT_CAMCORDER:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT_DATA_CALL:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT_WIFI:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT_WIBRO:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT_LTE:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT_LCD:
		if (sscanf(buf, "%10d\n", &x) == 1) {
#if !defined(CONFIG_SEC_FACTORY)
			struct timespec ts;

			get_monotonic_boottime(&ts);
			if (x) {
				battery->lcd_status = true;
			} else {
				battery->lcd_status = false;
#if defined(CONFIG_SUPPORT_HV_CTRL)
				sec_bat_change_vbus(battery);
#endif
			}
			pr_info("%s : lcd_status (%d)\n", __func__, battery->lcd_status);

			if (battery->wc_tx_enable) {
				__pm_stay_awake(battery->monitor_ws);
				queue_delayed_work(battery->monitor_wqueue,
					&battery->monitor_work, 0);
			}
#endif
			ret = count;
		}
		break;
#if defined(CONFIG_ISDB_CHARGING_CONTROL)
	case BATT_EVENT_ISDB:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev,
				"%s: ISDB EVENT %d\n", __func__, x);
			if (x) {
				pr_info("%s: ISDB ON\n", __func__);
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_ISDB,
					SEC_BAT_CURRENT_EVENT_ISDB);
				if (is_hv_wireless_type(battery->cable_type)) {
					pr_info("%s: set vout 5.5V with ISDB\n", __func__);
					value.intval = WIRELESS_VOUT_5_5V_STEP; // 5.5V
					psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
					sec_bat_set_charging_current(battery);
				} else if (is_hv_wire_type(battery->cable_type) ||
					(is_pd_wire_type(battery->cable_type) &&
					battery->pd_max_charge_power >= HV_CHARGER_STATUS_STANDARD1 &&
					battery->pdic_info.sink_status.available_pdo_num > 1) ||
					battery->max_charge_power >= HV_CHARGER_STATUS_STANDARD1)
					sec_bat_set_charging_current(battery);
			} else {
				pr_info("%s: ISDB OFF\n", __func__);
				sec_bat_set_current_event(battery, 0,
					SEC_BAT_CURRENT_EVENT_ISDB);
				if (is_hv_wireless_type(battery->cable_type)) {
					pr_info("%s: recover vout 10V with ISDB\n", __func__);
					value.intval = WIRELESS_VOUT_10V; // 10V
					psy_do_property(battery->pdata->wireless_charger_name, set,
					POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
					sec_bat_set_charging_current(battery);
				} else if (is_hv_wire_type(battery->cable_type))
					sec_bat_set_charging_current(battery);
			}
			ret = count;
		}
		break;
#endif
	case BATT_EVENT_GPS:
		if (sscanf(buf, "%10d\n", &x) == 1)
			ret = count;
		break;
	case BATT_EVENT:
		break;
	case BATT_MISC_EVENT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s: PMS sevice hiccup read done : %d ", __func__, x);
			if (!battery->hiccup_status &&
				(battery->misc_event & (BATT_MISC_EVENT_HICCUP_TYPE | BATT_MISC_EVENT_TEMP_HICCUP_TYPE))) {
				sec_bat_set_misc_event(battery,
					0, (BATT_MISC_EVENT_HICCUP_TYPE | BATT_MISC_EVENT_TEMP_HICCUP_TYPE));
			}
		}
		ret = count;
		break;
	case BATT_TX_EVENT:
	case BATT_CURRENT_EVENT:
		break;
#if !defined(CONFIG_BATTERY_SAMSUNG_MHS)
	case EXT_EVENT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev,
				"%s: ext event 0x%x\n", __func__, x);
			battery->ext_event = x;
			__pm_stay_awake(battery->ext_event_ws);
			queue_delayed_work(battery->monitor_wqueue, &battery->ext_event_work, 0);
			ret = count;
		}
		break;
#else
	case EXT_EVENT:
		break;
#endif
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct sec_sysfs sysfs_event_list = {
	.attr = sysfs_event_attrs,
	.size = ARRAY_SIZE(sysfs_event_attrs),
};

static int __init sysfs_event_init(void)
{
	add_sec_sysfs(&sysfs_event_list.list);
	return 0;
}
late_initcall(sysfs_event_init);
