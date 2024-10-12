/*
 *  sec_battery_sysfs.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2018 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "sec_battery.h"
#include "sec_battery_sysfs.h"
#if defined(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

ssize_t sysfs_battery_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);
ssize_t sysfs_battery_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);


#define SYSFS_BATTERY_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sysfs_battery_show_attrs,					\
	.store = sysfs_battery_store_attrs,					\
}

static struct device_attribute sysfs_battery_attrs[] = {
	SYSFS_BATTERY_ATTR(batt_type),
	SYSFS_BATTERY_ATTR(batt_vfocv),
	SYSFS_BATTERY_ATTR(batt_temp),
	SYSFS_BATTERY_ATTR(batt_temp_adc),
	SYSFS_BATTERY_ATTR(batt_temp_aver),
	SYSFS_BATTERY_ATTR(batt_temp_adc_aver),
	SYSFS_BATTERY_ATTR(usb_temp),
	SYSFS_BATTERY_ATTR(usb_temp_adc),
	SYSFS_BATTERY_ATTR(chg_temp),
	SYSFS_BATTERY_ATTR(chg_temp_adc),
	SYSFS_BATTERY_ATTR(sub_bat_temp),
	SYSFS_BATTERY_ATTR(sub_bat_temp_adc),
	SYSFS_BATTERY_ATTR(slave_chg_temp),
	SYSFS_BATTERY_ATTR(slave_chg_temp_adc),
#if defined(CONFIG_DIRECT_CHARGING)
	SYSFS_BATTERY_ATTR(dchg_temp),
	SYSFS_BATTERY_ATTR(dchg_temp_adc),
#endif
	SYSFS_BATTERY_ATTR(blkt_temp),
	SYSFS_BATTERY_ATTR(blkt_temp_adc),
	SYSFS_BATTERY_ATTR(batt_wpc_temp),
	SYSFS_BATTERY_ATTR(batt_wpc_temp_adc),
	SYSFS_BATTERY_ATTR(batt_charging_source),
	SYSFS_BATTERY_ATTR(chg_current_adc),
	SYSFS_BATTERY_ATTR(hv_charger_status),
	SYSFS_BATTERY_ATTR(hv_wc_charger_status),
	SYSFS_BATTERY_ATTR(hv_charger_set),
	SYSFS_BATTERY_ATTR(update),
	SYSFS_BATTERY_ATTR(test_mode),
	SYSFS_BATTERY_ATTR(batt_high_current_usb),
	SYSFS_BATTERY_ATTR(set_stability_test),
	SYSFS_BATTERY_ATTR(hmt_ta_connected),
	SYSFS_BATTERY_ATTR(hmt_ta_charge),
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	SYSFS_BATTERY_ATTR(battery_cycle),
#if defined(CONFIG_BATTERY_AGE_FORECAST_DETACHABLE)
	SYSFS_BATTERY_ATTR(batt_after_manufactured),
#endif
	SYSFS_BATTERY_ATTR(battery_cycle_test),
#endif
	SYSFS_BATTERY_ATTR(check_ps_ready),
	SYSFS_BATTERY_ATTR(safety_timer_set),
	SYSFS_BATTERY_ATTR(batt_swelling_control),
	SYSFS_BATTERY_ATTR(batt_battery_id),
	SYSFS_BATTERY_ATTR(batt_temp_control_test),
	SYSFS_BATTERY_ATTR(safety_timer_info),
	SYSFS_BATTERY_ATTR(batt_misc_test),
#if defined(CONFIG_BATTERY_SAMSUNG_MHS)
	SYSFS_BATTERY_ATTR(batt_charging_port),
#endif
	SYSFS_BATTERY_ATTR(usb_conf_test),
	SYSFS_BATTERY_ATTR(voter_status),
	SYSFS_BATTERY_ATTR(batt_full_capacity),
};

enum {
	BATT_TYPE = 0,
	BATT_VFOCV,
	BATT_TEMP,
	BATT_TEMP_ADC,
	BATT_TEMP_AVER,
	BATT_TEMP_ADC_AVER,
	USB_TEMP,
	USB_TEMP_ADC,
	BATT_CHG_TEMP,
	BATT_CHG_TEMP_ADC,
	SUB_BAT_TEMP,
	SUB_BAT_TEMP_ADC,
	SLAVE_CHG_TEMP,
	SLAVE_CHG_TEMP_ADC,
#if defined(CONFIG_DIRECT_CHARGING)
	DCHG_TEMP,
	DCHG_TEMP_ADC,
#endif
	BLKT_TEMP,
	BLKT_TEMP_ADC,
	BATT_WPC_TEMP,
	BATT_WPC_TEMP_ADC,
	BATT_CHARGING_SOURCE,
	CHG_CURRENT_ADC,
	HV_CHARGER_STATUS,
	HV_WC_CHARGER_STATUS,
	HV_CHARGER_SET,
	UPDATE,
	TEST_MODE,
	BATT_HIGH_CURRENT_USB,
	SET_STABILITY_TEST,
	HMT_TA_CONNECTED,
	HMT_TA_CHARGE,
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	BATTERY_CYCLE,
#if defined(CONFIG_BATTERY_AGE_FORECAST_DETACHABLE)
	BATT_AFTER_MANUFACTURED,
#endif
	BATTERY_CYCLE_TEST,
#endif
	CHECK_PS_READY,
	SAFETY_TIMER_SET,
	BATT_SWELLING_CONTROL,
	BATT_BATTERY_ID,
	BATT_TEMP_CONTROL_TEST,
	SAFETY_TIMER_INFO,
	BATT_MISC_TEST,
#if defined(CONFIG_BATTERY_SAMSUNG_MHS)
	BATT_CHARGING_PORT,
#endif
	USB_CONF,
	VOTER_STATUS,
	BATT_FULL_CAPACITY,
};

ssize_t sysfs_battery_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sysfs_battery_attrs;
	union power_supply_propval value = {0, };
	int i = 0;

	switch (offset) {
	case BATT_TYPE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			battery->batt_type);
		break;
	case BATT_VFOCV:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->voltage_ocv);
		break;
	case BATT_TEMP:
		switch (battery->pdata->thermal_source) {
		case SEC_BATTERY_THERMAL_SOURCE_FG:
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_TEMP, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
			break;
		case SEC_BATTERY_THERMAL_SOURCE_CALLBACK:
			if (battery->pdata->get_temperature_callback) {
			battery->pdata->get_temperature_callback(
				POWER_SUPPLY_PROP_TEMP, &value);
			}
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
			break;
		case SEC_BATTERY_THERMAL_SOURCE_ADC:
			if (sec_bat_get_value_by_adc(battery,
					SEC_BAT_ADC_CHANNEL_TEMP, &value, battery->pdata->temp_check_type)) {
				i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					value.intval);
			} else {
				i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
			}
			break;
		case SEC_BATTERY_THERMAL_SOURCE_FG_ADC:
			{
				int temp = 0;
				psy_do_property(battery->pdata->fuelgauge_name, get,
					POWER_SUPPLY_PROP_TEMP, value);

				temp = sec_bat_get_fg_temp_adc(battery, value.intval);
				if (battery->pdata->temp_check_type ==
							SEC_BATTERY_TEMP_CHECK_FAKE)
					temp = 300;
				i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					temp);
			}
			break;
		default:
			break;
		}
		break;
	case BATT_TEMP_ADC:
		/*
			If F/G is used for reading the temperature and
			compensation table is used,
			the raw value that isn't compensated can be read by
			POWER_SUPPLY_PROP_TEMP_AMBIENT
		 */
		switch (battery->pdata->thermal_source) {
		case SEC_BATTERY_THERMAL_SOURCE_FG_ADC:
		case SEC_BATTERY_THERMAL_SOURCE_FG:
			psy_do_property(battery->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_TEMP_AMBIENT, value);
			battery->temp_adc = value.intval;
			break;
		default:
			break;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->temp_adc);
		break;
	case BATT_TEMP_AVER:
		break;
	case BATT_TEMP_ADC_AVER:
		break;
	case USB_TEMP:
		if (battery->pdata->usb_thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
			sec_bat_get_value_by_adc(battery,
				SEC_BAT_ADC_CHANNEL_USB_TEMP, &value, battery->pdata->usb_temp_check_type);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case USB_TEMP_ADC:
		if (battery->pdata->usb_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       battery->usb_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case BATT_CHG_TEMP:
		if (battery->pdata->chg_thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
			sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_CHG_TEMP, &value, battery->pdata->chg_temp_check_type);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       value.intval);
		} else if (battery->pdata->chg_thermal_source == SEC_BATTERY_THERMAL_SOURCE_FG) {
			psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_TEMP, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case BATT_CHG_TEMP_ADC:
		if (battery->pdata->chg_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       battery->chg_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case SUB_BAT_TEMP:
		if (battery->pdata->sub_bat_thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
			sec_bat_get_value_by_adc(battery,
				SEC_BAT_ADC_CHANNEL_SUB_BAT_TEMP, &value, battery->pdata->sub_bat_temp_check_type);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case SUB_BAT_TEMP_ADC:
		if (battery->pdata->sub_bat_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       battery->sub_bat_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case SLAVE_CHG_TEMP:
		if (battery->pdata->slave_thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
			sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_SLAVE_CHG_TEMP, &value , battery->pdata->slave_chg_temp_check_type);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					   value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					   0);
		}
		break;
	case SLAVE_CHG_TEMP_ADC:
		if (battery->pdata->slave_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					   battery->slave_chg_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					   0);
		}
		break;
#if defined(CONFIG_DIRECT_CHARGING)
	case DCHG_TEMP:
		{
			battery->dchg_temp = sec_bat_get_dctp_info(battery);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->dchg_temp);
		}
		break;
	case DCHG_TEMP_ADC:
		{
			switch (battery->pdata->dchg_thermal_source) {
				case SEC_BATTERY_THERMAL_SOURCE_CHG_ADC:
					psy_do_property(battery->pdata->charger_name, get,
						POWER_SUPPLY_PROP_TEMP, value);
					break;
				default:
					value.intval = -1;
					break;
			}
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		}
		break;
#endif
	case BLKT_TEMP:
		if (battery->pdata->blkt_thermal_source) {
			sec_bat_get_value_by_adc(battery,
					       SEC_BAT_ADC_CHANNEL_BLKT_TEMP, &value, battery->pdata->blkt_temp_check_type);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case BLKT_TEMP_ADC:
		if (battery->pdata->blkt_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       battery->blkt_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       0);
		}
		break;
	case BATT_WPC_TEMP:
		if (battery->pdata->wpc_thermal_source == SEC_BATTERY_THERMAL_SOURCE_ADC) {
			sec_bat_get_value_by_adc(battery,
			SEC_BAT_ADC_CHANNEL_WPC_TEMP, &value, battery->pdata->wpc_temp_check_type);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				0);
		}
		break;
	case BATT_WPC_TEMP_ADC:
		if (battery->pdata->wpc_thermal_source) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				battery->wpc_temp_adc);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				0);
		}
		break;
	case BATT_CHARGING_SOURCE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->cable_type);
		break;
	case CHG_CURRENT_ADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->current_adc);
		break;
	case HV_CHARGER_STATUS:
		{
			int check_val = 0;
			if (is_wireless_type(battery->cable_type)) {
				check_val = 0;
			} else {
				if (is_pd_wire_type(battery->cable_type) &&
					battery->pd_max_charge_power >= HV_CHARGER_STATUS_STANDARD4)
					check_val = SFC_45W;
				else if (is_pd_wire_type(battery->cable_type) &&
					battery->pd_max_charge_power >= HV_CHARGER_STATUS_STANDARD3)
					check_val = SFC_25W;
				else if (is_hv_wire_12v_type(battery->cable_type) ||
					battery->max_charge_power >= HV_CHARGER_STATUS_STANDARD2) /* 20000mW */
					check_val = AFC_12V_OR_20W;
				else if (is_hv_wire_type(battery->cable_type) ||
#if defined(CONFIG_PDIC_NOTIFIER)
					(is_pd_wire_type(battery->cable_type) &&
					battery->pd_max_charge_power >= HV_CHARGER_STATUS_STANDARD1 &&
					battery->pdic_info.sink_status.available_pdo_num > 1) ||
#endif
					battery->wire_status == SEC_BATTERY_CABLE_PREPARE_TA ||
					battery->max_charge_power >= HV_CHARGER_STATUS_STANDARD1) /* 12000mW */
					check_val = AFC_9V_OR_15W;
			}
			pr_info("%s : HV_CHARGER_STATUS(%d), max_charger_power(%d), pd_max charge power(%d)\n",
				__func__, check_val, battery->max_charge_power, battery->pd_max_charge_power);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", check_val);
		}
		break;
	case HV_WC_CHARGER_STATUS:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			is_hv_wireless_type(battery->cable_type) ? 1 : 0);
		break;
	case HV_CHARGER_SET:
		break;
	case UPDATE:
		break;
	case TEST_MODE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->test_mode);
		break;
	case BATT_HIGH_CURRENT_USB:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->is_hc_usb);
		break;
	case SET_STABILITY_TEST:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->stability_test);
		break;
	case HMT_TA_CONNECTED:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->cable_type == SEC_BATTERY_CABLE_HMT_CONNECTED) ? 1 : 0);
		break;
	case HMT_TA_CHARGE:
#if defined(CONFIG_PDIC_NOTIFIER)
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->current_event & SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE) ? 0 : 1);
#else
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(battery->cable_type == SEC_BATTERY_CABLE_HMT_CHARGE) ? 1 : 0);
#endif
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
#if defined(CONFIG_BATTERY_AGE_FORECAST_DETACHABLE)
	case BATT_AFTER_MANUFACTURED:
#if defined(CONFIG_ENG_BATTERY_CONCEPT) || defined(CONFIG_SEC_FACTORY)
	case BATTERY_CYCLE:
#endif
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", battery->batt_cycle);
		break;
#else
	case BATTERY_CYCLE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", battery->batt_cycle);
		break;
#endif
	case BATTERY_CYCLE_TEST:
		break;
#endif
	case CHECK_PS_READY:
#if defined(CONFIG_PDIC_NOTIFIER)
#if defined(CONFIG_BATTERY_SAMSUNG_MHS)
		value.intval = battery->sub->pdic_ps_rdy;
#else
		value.intval = battery->pdic_ps_rdy;
#endif
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		pr_info("%s : CHECK_PS_READY=%d\n",__func__,value.intval);
#endif
		break;
	case SAFETY_TIMER_SET:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       battery->safety_timer_set);
		break;
	case BATT_SWELLING_CONTROL:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       battery->skip_swelling);
		break;
	case BATT_BATTERY_ID:
		value.intval = 0;
		psy_do_property(battery->pdata->fuelgauge_name, get,
			POWER_SUPPLY_EXT_PROP_BATTERY_ID, value);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				value.intval);
		break;
	case BATT_TEMP_CONTROL_TEST:
		{
			int temp_ctrl_t = 0;

			if (battery->current_event & SEC_BAT_CURRENT_EVENT_TEMP_CTRL_TEST)
				temp_ctrl_t = 1;
			else
				temp_ctrl_t = 0;

			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				temp_ctrl_t);
		}
		break;
	case SAFETY_TIMER_INFO:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%ld\n",
			       battery->cal_safety_time);
		break;
	case BATT_MISC_TEST:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				   battery->display_test);
		break;
#if defined(CONFIG_BATTERY_SAMSUNG_MHS)
	case BATT_CHARGING_PORT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			battery->charging_port);
		break;
#endif
	case USB_CONF:
		break;
	case VOTER_STATUS:
		i = show_sec_vote_status(buf, PAGE_SIZE);
		break;
	case BATT_FULL_CAPACITY:
		pr_info("%s: BATT_FULL_CAPACITY = %d\n", __func__, battery->batt_full_capacity);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", battery->batt_full_capacity);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sysfs_battery_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sysfs_battery_attrs;
	int ret = -EINVAL;
	int x = 0;
	union power_supply_propval value = {0, };

	switch (offset) {
	case BATT_TYPE:
		strncpy(battery->batt_type, buf, sizeof(battery->batt_type) - 1);
		battery->batt_type[sizeof(battery->batt_type)-1] = '\0';
		ret = count;
		break;
	case BATT_VFOCV:
		break;
	case BATT_TEMP:
		break;
	case BATT_TEMP_ADC:
	case BATT_TEMP_AVER:
	case BATT_TEMP_ADC_AVER:
	case USB_TEMP:
	case USB_TEMP_ADC:
	case BATT_CHG_TEMP:
	case BATT_CHG_TEMP_ADC:
	case SUB_BAT_TEMP:
	case SUB_BAT_TEMP_ADC:
	case SLAVE_CHG_TEMP:
	case SLAVE_CHG_TEMP_ADC:
		break;
#if defined(CONFIG_DIRECT_CHARGING)
	case DCHG_TEMP:
	case DCHG_TEMP_ADC:
		break;
#endif
	case BLKT_TEMP:
		break;
	case BLKT_TEMP_ADC:
		break;
	case BATT_WPC_TEMP:
	case BATT_WPC_TEMP_ADC:
		break;
	case BATT_CHARGING_SOURCE:
		break;
	case CHG_CURRENT_ADC:
		break;
	case HV_CHARGER_STATUS:
		break;
	case HV_WC_CHARGER_STATUS:
		break;
	case HV_CHARGER_SET:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev,
				"%s: HV_CHARGER_SET(%d)\n", __func__, x);
			if (x == 1) {
				battery->wire_status = SEC_BATTERY_CABLE_9V_TA;
				__pm_stay_awake(battery->cable_ws);
				queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
			} else {
				battery->wire_status = SEC_BATTERY_CABLE_NONE;
				__pm_stay_awake(battery->cable_ws);
				queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
			}
			ret = count;
		}
		break;
	case UPDATE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			/* update battery info */
			sec_bat_get_battery_info(battery);
			ret = count;
		}
		break;
	case TEST_MODE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			battery->test_mode = x;
			__pm_stay_awake(battery->monitor_ws);
			queue_delayed_work(battery->monitor_wqueue,
				&battery->monitor_work, 0);
			ret = count;
		}
		break;
	case BATT_HIGH_CURRENT_USB:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			battery->is_hc_usb = x ? true : false;
			pr_info("%s: is_hc_usb (%d)\n", __func__, battery->is_hc_usb);
			ret = count;
		}
		break;
	case SET_STABILITY_TEST:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_err(battery->dev,
				"%s: BATT_STABILITY_TEST(%d)\n", __func__, x);
			if (x) {
				battery->stability_test = true;
				battery->eng_not_full_status = true;
			}
			else {
				battery->stability_test = false;
				battery->eng_not_full_status = false;
			}
			ret = count;
		}
		break;
	case HMT_TA_CONNECTED:
		if (sscanf(buf, "%10d\n", &x) == 1) {
#if !defined(CONFIG_PDIC_NOTIFIER)
			dev_info(battery->dev,
					"%s: HMT_TA_CONNECTED(%d)\n", __func__, x);
			if (x) {
				value.intval = false;
				psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
						value);
				dev_info(battery->dev,
						"%s: changed to OTG cable detached\n", __func__);

				battery->wire_status = SEC_BATTERY_CABLE_HMT_CONNECTED;
				__pm_stay_awake(battery->cable_ws);
				queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
			} else {
				value.intval = true;
				psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
						value);
				dev_info(battery->dev,
						"%s: changed to OTG cable attached\n", __func__);

				battery->wire_status = SEC_BATTERY_CABLE_OTG;
				__pm_stay_awake(battery->cable_ws);
				queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
			}
#endif
			ret = count;
		}
		break;
	case HMT_TA_CHARGE:
		if (sscanf(buf, "%10d\n", &x) == 1) {
#if defined(CONFIG_PDIC_NOTIFIER)
			dev_info(battery->dev,
					"%s: HMT_TA_CHARGE(%d)\n", __func__, x);

			/* do not charge off without cable type, since wdt could be expired */
			if (x) {
				sec_bat_set_current_event(battery, 0, SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE);
				sec_vote(battery->chgen_vote, VOTER_HMT, false, 0);
			} else if (!x && !is_nocharge_type(battery->cable_type)) {
				sec_bat_set_current_event(battery, SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE,
						SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE);
				sec_vote(battery->chgen_vote, VOTER_HMT, true, SEC_BAT_CHG_MODE_CHARGING_OFF);
			} else
				dev_info(battery->dev, "%s: Wrong HMT control\n", __func__);

			ret = count;
#else
			dev_info(battery->dev,
					"%s: HMT_TA_CHARGE(%d)\n", __func__, x);
			psy_do_property(battery->pdata->charger_name, get,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
			if (value.intval) {
				dev_info(battery->dev,
					"%s: ignore HMT_TA_CHARGE(%d)\n", __func__, x);
			} else {
				if (x) {
					value.intval = false;
					psy_do_property(battery->pdata->charger_name, set,
							POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
							value);
					dev_info(battery->dev,
						"%s: changed to OTG cable detached\n", __func__);
					battery->wire_status = SEC_BATTERY_CABLE_HMT_CHARGE;
					__pm_stay_awake(battery->cable_ws);
					queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
				} else {
					value.intval = false;
					psy_do_property(battery->pdata->charger_name, set,
							POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
							value);
					dev_info(battery->dev,
							"%s: changed to OTG cable detached\n", __func__);
					battery->wire_status = SEC_BATTERY_CABLE_HMT_CONNECTED;
					__pm_stay_awake(battery->cable_ws);
					queue_delayed_work(battery->monitor_wqueue, &battery->cable_work, 0);
				}
			}
			ret = count;
#endif
		}
		break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
#if defined(CONFIG_BATTERY_AGE_FORECAST_DETACHABLE)
	case BATT_AFTER_MANUFACTURED:
#else
	case BATTERY_CYCLE:
#endif
		if (sscanf(buf, "%10d\n", &x) == 1) {
			dev_info(battery->dev, "%s: %s(%d)\n", __func__,
				(offset == BATTERY_CYCLE) ?
				"BATTERY_CYCLE" : "BATTERY_CYCLE(W)", x);
			if (x >= 0) {
				int prev_battery_cycle = battery->batt_cycle;
				battery->batt_cycle = x;
#if defined(CONFIG_BATTERY_CISD)
				battery->cisd.data[CISD_DATA_CYCLE] = x;
#endif
				if (prev_battery_cycle < 0) {
					sec_bat_aging_check(battery);
				}
				sec_bat_check_battery_health(battery);

				if ((prev_battery_cycle - battery->batt_cycle) >= 9000) {
					value.intval = 0;
					psy_do_property(battery->pdata->fuelgauge_name, set,
									POWER_SUPPLY_PROP_ENERGY_NOW, value);
					dev_info(battery->dev, "%s: change the concept of battery protection mode.\n", __func__);
				}
			}
			ret = count;
		}
		break;
	case BATTERY_CYCLE_TEST:
		sec_bat_aging_check(battery);
		break;
#endif
	case CHECK_PS_READY:
		break;
	case SAFETY_TIMER_SET:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x) {
				battery->safety_timer_set = true;
			} else {
				battery->safety_timer_set = false;
			}
			ret = count;
		}
		break;
	case BATT_SWELLING_CONTROL:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x) {
				pr_info("%s : 15TEST START!! SWELLING MODE DISABLE\n", __func__);
				battery->skip_swelling = true;
			} else {
				pr_info("%s : 15TEST END!! SWELLING MODE END\n", __func__);
				battery->skip_swelling = false;
			}
			ret = count;
		}
		break;
	case BATT_BATTERY_ID:
		break;
	case BATT_TEMP_CONTROL_TEST:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x)
				sec_bat_set_temp_control_test(battery, true);
			else
				sec_bat_set_temp_control_test(battery, false);

			ret = count;
		}
		break;
	case SAFETY_TIMER_INFO:
		break;
	case BATT_MISC_TEST:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			pr_info("%s batt_misc_test %d\n", __func__, x);
			switch (x) {
			case MISC_TEST_RESET:
				pr_info("%s RESET MISC_TEST command\n", __func__);
				battery->display_test = false;
				battery->store_mode = false;
				battery->skip_swelling = false;
				battery->pdata->temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP;
				battery->pdata->usb_temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP;
				battery->pdata->chg_temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP;
				battery->pdata->wpc_temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP;
				battery->pdata->sub_bat_temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP;
				battery->pdata->blkt_temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
				battery->pdata->dchg_temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP;
#endif
				break;
				break;
			case MISC_TEST_DISPLAY:
				pr_info("%s START DISPLAY_TEST command\n", __func__);
				battery->display_test = true;	/* block for display test */
				battery->store_mode = true;	/* enter store mode for prevent 100% full charge */
				battery->skip_swelling = true;	/* restore thermal_zone to NORMAL */
				battery->pdata->temp_check_type = SEC_BATTERY_TEMP_CHECK_NONE;
				battery->pdata->usb_temp_check_type = SEC_BATTERY_TEMP_CHECK_NONE;
				battery->pdata->chg_temp_check_type = SEC_BATTERY_TEMP_CHECK_NONE;
				battery->pdata->wpc_temp_check_type = SEC_BATTERY_TEMP_CHECK_NONE;
				battery->pdata->sub_bat_temp_check_type = SEC_BATTERY_TEMP_CHECK_NONE;
				battery->pdata->blkt_temp_check_type = SEC_BATTERY_TEMP_CHECK_NONE;
#if IS_ENABLED(CONFIG_DIRECT_CHARGING)
				battery->pdata->dchg_temp_check_type = SEC_BATTERY_TEMP_CHECK_NONE;
#endif
				break;
			case MISC_TEST_MAX:
			default:
				pr_info("%s Wrong MISC_TEST command\n", __func__);
				break;
			}
			ret = count;
		}
		break;
#if defined(CONFIG_BATTERY_SAMSUNG_MHS)
	case BATT_CHARGING_PORT:
		break;
#endif
	case USB_CONF:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			value.intval = x;
			psy_do_property("battery", set,
				POWER_SUPPLY_EXT_PROP_USB_CONFIGURE, value);
			ret = count;
		}
		break;
	case VOTER_STATUS:
		break;
	case BATT_FULL_CAPACITY:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x >= 0 && x <= 100) {
				pr_info("%s: update BATT_FULL_CAPACITY(%d)\n", __func__, x);
				battery->batt_full_capacity = x;
				__pm_stay_awake(battery->monitor_ws);
				queue_delayed_work(battery->monitor_wqueue,
					&battery->monitor_work, 0);
			} else {
				pr_info("%s: out of range(%d)\n", __func__, x);
			}
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct sec_sysfs sysfs_battery_list = {
	.attr = sysfs_battery_attrs,
	.size = ARRAY_SIZE(sysfs_battery_attrs),
};

static int __init sysfs_battery_init(void)
{
	add_sec_sysfs(&sysfs_battery_list.list);
	return 0;
}
late_initcall(sysfs_battery_init);
