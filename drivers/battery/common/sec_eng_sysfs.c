/*
 *  sec_eng_sysfs.c
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

static ssize_t sysfs_eng_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t sysfs_eng_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SYSFS_ENG_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sysfs_eng_show_attrs,					\
	.store = sysfs_eng_store_attrs,					\
}

static struct device_attribute sysfs_eng_attrs[] = {
#if defined(CONFIG_STEP_CHARGING)
	SYSFS_ENG_ATTR(test_step_condition),
#endif
	SYSFS_ENG_ATTR(test_charge_current),
	SYSFS_ENG_ATTR(batt_tune_float_voltage),
	SYSFS_ENG_ATTR(batt_tune_input_charge_current),
	SYSFS_ENG_ATTR(batt_tune_fast_charge_current),
	SYSFS_ENG_ATTR(batt_tune_ui_term_cur_1st),
	SYSFS_ENG_ATTR(batt_tune_ui_term_cur_2nd),
	SYSFS_ENG_ATTR(batt_tune_chg_temp_high),
	SYSFS_ENG_ATTR(batt_tune_chg_temp_rec),
	SYSFS_ENG_ATTR(batt_tune_chg_limit_cur),
	SYSFS_ENG_ATTR(batt_tune_coil_temp_high),
	SYSFS_ENG_ATTR(batt_tune_coil_temp_rec),
	SYSFS_ENG_ATTR(batt_tune_coil_limit_cur),
	SYSFS_ENG_ATTR(batt_tune_wpc_temp_high),
	SYSFS_ENG_ATTR(batt_tune_wpc_temp_high_rec),
#if defined(CONFIG_DIRECT_CHARGING)
	SYSFS_ENG_ATTR(batt_tune_dchg_temp_high),
	SYSFS_ENG_ATTR(batt_tune_dchg_temp_high_rec),
	SYSFS_ENG_ATTR(batt_tune_dchg_limit_input_cur),
	SYSFS_ENG_ATTR(batt_tune_dchg_limit_chg_cur),
#endif
	SYSFS_ENG_ATTR(batt_temp_test),
};

enum {
#if defined(CONFIG_STEP_CHARGING)
	TEST_STEP_CONDITION = 0,
#endif
	TEST_CHARGE_CURRENT,
	BATT_TUNE_FLOAT_VOLTAGE,
	BATT_TUNE_INPUT_CHARGE_CURRENT,
	BATT_TUNE_FAST_CHARGE_CURRENT,
	BATT_TUNE_UI_TERM_CURRENT_1ST,
	BATT_TUNE_UI_TERM_CURRENT_2ND,
	BATT_TUNE_CHG_TEMP_HIGH,
	BATT_TUNE_CHG_TEMP_REC,
	BATT_TUNE_CHG_LIMIT_CUR,
	BATT_TUNE_COIL_TEMP_HIGH,
	BATT_TUNE_COIL_TEMP_REC,
	BATT_TUNE_COIL_LIMIT_CUR,
	BATT_TUNE_WPC_TEMP_HIGH,
	BATT_TUNE_WPC_TEMP_HIGH_REC,
#if defined(CONFIG_DIRECT_CHARGING)
	BATT_TUNE_DCHG_TEMP_HIGH,
	BATT_TUNE_DCHG_TEMP_HIGH_REC,
	BATT_TUNE_DCHG_LIMIT_INPUT_CUR,
	BATT_TUNE_DCHG_LIMIT_CHG_CUR,
#endif
	BATT_TEMP_TEST,
};

ssize_t sysfs_eng_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sysfs_eng_attrs;
	union power_supply_propval value = {0, };
	int i = 0;
	int ret = 0;

	switch (offset) {
#if defined(CONFIG_STEP_CHARGING)
	case TEST_STEP_CONDITION:
		{
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					battery->test_step_condition);
		}
		break;
#endif
	case TEST_CHARGE_CURRENT:
		{
			psy_do_property(battery->pdata->charger_name, get,
				POWER_SUPPLY_PROP_CURRENT_NOW, value);
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
					value.intval);
		}
		break;
	case BATT_TUNE_FLOAT_VOLTAGE:
		ret = get_sec_vote_result(battery->fv_vote);
		pr_info("%s float voltage = %d mA", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_INPUT_CHARGE_CURRENT:
		ret = battery->pdata->charging_current[i].input_current_limit;
		pr_info("%s input charge current = %d mA", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_FAST_CHARGE_CURRENT:
		ret = battery->pdata->charging_current[i].fast_charging_current;
		pr_info("%s fast charge current = %d mA", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_UI_TERM_CURRENT_1ST:
		ret = battery->pdata->full_check_current_1st;
		pr_info("%s ui term current = %d mA", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_UI_TERM_CURRENT_2ND:
		ret = battery->pdata->full_check_current_2nd;
		pr_info("%s ui term current = %d mA", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_CHG_TEMP_HIGH:
		ret = battery->pdata->chg_high_temp;
		pr_info("%s chg_high_temp = %d ", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_CHG_TEMP_REC:
		ret = battery->pdata->chg_high_temp_recovery;
		pr_info("%s chg_high_temp_recovery = %d ", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_CHG_LIMIT_CUR:
		ret = battery->pdata->chg_charging_limit_current;
		pr_info("%s chg_charging_limit_current = %d ", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_COIL_TEMP_HIGH:
		break;
	case BATT_TUNE_COIL_TEMP_REC:
		break;
	case BATT_TUNE_COIL_LIMIT_CUR:
		break;
	case BATT_TUNE_WPC_TEMP_HIGH:
		ret = battery->pdata->wpc_high_temp;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_WPC_TEMP_HIGH_REC:
		ret = battery->pdata->wpc_high_temp_recovery;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
#if defined(CONFIG_DIRECT_CHARGING)
	case BATT_TUNE_DCHG_TEMP_HIGH:
		ret = battery->pdata->dchg_high_temp;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_DCHG_TEMP_HIGH_REC:
		ret = battery->pdata->dchg_high_temp_recovery;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_DCHG_LIMIT_INPUT_CUR:
		ret = battery->pdata->dchg_input_limit_current;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
	case BATT_TUNE_DCHG_LIMIT_CHG_CUR:
		ret = battery->pdata->dchg_charging_limit_current;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				ret);
		break;
#endif
#if defined(CONFIG_DUAL_BATTERY)
	case BATT_TEMP_TEST:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d %d %d %d %d\n",
			battery->temperature_test_battery,
			battery->temperature_test_usb,
			battery->temperature_test_wpc,
			battery->temperature_test_chg,
			battery->temperature_test_sub);
		break;
#else
	case BATT_TEMP_TEST:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d %d %d %d %d %d\n",
			battery->temperature_test_battery,
			battery->temperature_test_usb,
			battery->temperature_test_wpc,
			battery->temperature_test_chg,
			battery->temperature_test_dchg,
			battery->temperature_test_blkt);
		break;
#endif
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t sysfs_eng_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - sysfs_eng_attrs;
	int ret = -EINVAL;
	int x = 0;
	int i = 0;
	union power_supply_propval value = {0, };

	switch (offset) {
#if defined(CONFIG_STEP_CHARGING)
	case TEST_STEP_CONDITION:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x >= 0 && x <= 100) {
				dev_err(battery->dev,
					"%s: TEST_STEP_CONDITION(%d)\n", __func__, x);
				battery->test_step_condition = x;
			}
			ret = count;
		}
		break;
#endif
	case TEST_CHARGE_CURRENT:
		if (sscanf(buf, "%10d\n", &x) == 1) {
			if (x >= 0 && x <= 2000) {
				dev_err(battery->dev,
					"%s: BATT_TEST_CHARGE_CURRENT(%d)\n", __func__, x);
				battery->pdata->charging_current[
					SEC_BATTERY_CABLE_USB].input_current_limit = x;
				battery->pdata->charging_current[
					SEC_BATTERY_CABLE_USB].fast_charging_current = x;
				if (x > 500) {
					battery->eng_not_full_status = true;
					battery->pdata->temp_check_type =
						SEC_BATTERY_TEMP_CHECK_NONE;
				}
				if (battery->cable_type == SEC_BATTERY_CABLE_USB) {
					value.intval = x;
					psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_CURRENT_NOW,
						value);
				}
			}
			ret = count;
		}
		break;
	case BATT_TUNE_FLOAT_VOLTAGE:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s float voltage = %d mV", __func__, x);
		sec_vote(battery->fv_vote, VOTER_CABLE, true, x);
		break;
	case BATT_TUNE_INPUT_CHARGE_CURRENT:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s input charge current = %d mA", __func__, x);
		if (x >= 0 && x <= 4000) {
			battery->test_max_current = true;
			for (i = 0; i < SEC_BATTERY_CABLE_MAX; i++) {
				if (i != SEC_BATTERY_CABLE_USB)
					battery->pdata->charging_current[i].input_current_limit = x;
				pr_info("%s [%d] = %d mA", __func__, i, battery->pdata->charging_current[i].input_current_limit);
			}

			if (battery->cable_type != SEC_BATTERY_CABLE_USB) {
				value.intval = x;
				psy_do_property(battery->pdata->charger_name, set,
							POWER_SUPPLY_PROP_CURRENT_MAX, value);
			}
		}
		break;
	case BATT_TUNE_FAST_CHARGE_CURRENT:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s fast charge current = %d mA", __func__, x);
		if (x >= 0 && x <= 4000) {
			battery->test_charge_current = true;
			for (i = 0; i < SEC_BATTERY_CABLE_MAX; i++) {
				if (i != SEC_BATTERY_CABLE_USB)
					battery->pdata->charging_current[i].fast_charging_current = x;
				pr_info("%s [%d] = %d mA", __func__, i, battery->pdata->charging_current[i].fast_charging_current);
			}

			if (battery->cable_type != SEC_BATTERY_CABLE_USB) {
				value.intval = x;
					psy_do_property(battery->pdata->charger_name, set,
						POWER_SUPPLY_PROP_CURRENT_AVG, value);
			}
		}
		break;
	case BATT_TUNE_UI_TERM_CURRENT_1ST:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s ui term current = %d mA", __func__, x);

		if (x > 0 && x < 1000)
			battery->pdata->full_check_current_1st = x;
		break;
	case BATT_TUNE_UI_TERM_CURRENT_2ND:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s ui term current = %d mA", __func__, x);

		if (x > 0 && x < 1000)
			battery->pdata->full_check_current_2nd = x;
		break;
	case BATT_TUNE_CHG_TEMP_HIGH:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s chg_high_temp  = %d ", __func__, x);
		if (x < 1000 && x >= -200)
			battery->pdata->chg_high_temp = x;
		break;
	case BATT_TUNE_CHG_TEMP_REC:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s chg_high_temp_recovery = %d ", __func__, x);
		if (x < 1000 && x >= -200)
			battery->pdata->chg_high_temp_recovery = x;
		break;
	case BATT_TUNE_CHG_LIMIT_CUR:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s chg_charging_limit_current	= %d ", __func__, x);
		if (x < 3000 && x > 0) {
			battery->pdata->chg_charging_limit_current = x;
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_ERR].input_current_limit = x;
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_UNKNOWN].input_current_limit = x;
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_TA].input_current_limit = x;
		}
		break;
	case BATT_TUNE_COIL_TEMP_HIGH:
		break;
	case BATT_TUNE_COIL_TEMP_REC:
		break;
	case BATT_TUNE_COIL_LIMIT_CUR:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s wpc_input_limit_current = %d ", __func__, x);
		if (x < 3000 && x > 0) {
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_ERR].input_current_limit = x;
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_UNKNOWN].input_current_limit = x;
			battery->pdata->charging_current[SEC_BATTERY_CABLE_9V_TA].input_current_limit = x;
		}
		break;
	case BATT_TUNE_WPC_TEMP_HIGH:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s wpc_high_temp = %d ", __func__, x);
		battery->pdata->wpc_high_temp = x;
		break;
	case BATT_TUNE_WPC_TEMP_HIGH_REC:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s wpc_high_temp_recovery = %d ", __func__, x);
		battery->pdata->wpc_high_temp_recovery = x;
		break;
#if defined(CONFIG_DIRECT_CHARGING)
	case BATT_TUNE_DCHG_TEMP_HIGH:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s dchg_high_temp = %d", __func__, x);
		battery->pdata->dchg_high_temp = x;
		break;
	case BATT_TUNE_DCHG_TEMP_HIGH_REC:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s dchg_high_temp_recovery = %d", __func__, x);
		battery->pdata->dchg_high_temp_recovery = x;
		break;
	case BATT_TUNE_DCHG_LIMIT_INPUT_CUR:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s dchg_input_limit_current = %d", __func__, x);
		battery->pdata->dchg_input_limit_current = x;
		break;
	case BATT_TUNE_DCHG_LIMIT_CHG_CUR:
		sscanf(buf, "%10d\n", &x);
		pr_info("%s dchg_charging_limit_current = %d", __func__, x);
		battery->pdata->dchg_charging_limit_current = x;
		break;
#endif
	case BATT_TEMP_TEST:
	{
		char tc;

		if (sscanf(buf, "%c %10d\n", &tc, &x) == 2) {
			pr_info("%s : temperature t: %c, temp: %d\n", __func__, tc, x);
			if (tc == 'u') {
				if (x > 900)
					battery->pdata->usb_temp_check_type = 0;
				else
					battery->temperature_test_usb = x;
			} else if (tc == 'w') {
				if (x > 900)
					battery->pdata->wpc_temp_check_type = 0;
				else
					battery->temperature_test_wpc = x;
			} else if (tc == 'b') {
				if (x > 900)
					battery->pdata->temp_check_type = 0;
				else
					battery->temperature_test_battery = x;
			} else if (tc == 'c') {
				if (x > 900)
					battery->pdata->chg_temp_check_type = 0;
				else
					battery->temperature_test_chg = x;
#if defined(CONFIG_DUAL_BATTERY)
			} else if (tc == 's') {
				if (x > 900)
					battery->pdata->sub_bat_temp_check_type = 0;
				else
					battery->temperature_test_sub = x;
#endif
#if defined(CONFIG_DIRECT_CHARGING)
			} else if (tc == 'd') {
				if (x > 900)
					battery->pdata->dchg_temp_check_type = 0;
				else
					battery->temperature_test_dchg = x;
#endif
			} else if (tc == 'k') {
				battery->temperature_test_blkt = x;
			}
			ret = count;
		}
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct sec_sysfs sysfs_eng_list = {
	.attr = sysfs_eng_attrs,
	.size = ARRAY_SIZE(sysfs_eng_attrs),
};

static int __init sysfs_eng_init(void)
{
	add_sec_sysfs(&sysfs_eng_list.list);
	return 0;
}
late_initcall(sysfs_eng_init);
