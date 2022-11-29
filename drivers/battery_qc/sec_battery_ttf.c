/*
 * sec_battery_ttf.c
 * Samsung Mobile Battery Driver
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "include/sec_battery_qc.h"
#include "include/sec_battery_ttf.h"

#if IS_ENABLED(CONFIG_CALC_TIME_TO_FULL)
int sec_calc_ttf(struct sec_battery_info *battery, unsigned int ttf_curr)
{
	struct sec_cv_slope *cv_data = battery->ttf_d->cv_data;
	int i, cc_time = 0, cv_time = 0;
	int rc = 0;
	int soc = battery->soc; // battery->capacity
	int charge_current = ttf_curr;
	int design_cap = battery->ttf_d->ttf_capacity;
	union power_supply_propval val = {0, };

	if (!battery->ttf_d)
		return -ENODEV;

	rc = power_supply_get_property(battery->psy_bms,
				(enum power_supply_property)POWER_SUPPLY_EXT_PROP_RAW_CAP, &val);

	soc = val.intval;

	if (!cv_data || (ttf_curr <= 0)) {
		pr_info("%s: no cv_data or val: %d\n", __func__, ttf_curr);
		return -1;
	}
	for (i = 0; i < battery->ttf_d->cv_data_length; i++) {
		if (charge_current >= cv_data[i].fg_current)
			break;
	}
	i = i >= battery->ttf_d->cv_data_length ? battery->ttf_d->cv_data_length - 1 : i;
	if (cv_data[i].soc < soc) {
		for (i = 0; i < battery->ttf_d->cv_data_length; i++) {
			if (soc <= cv_data[i].soc)
				break;
		}
		cv_time =
		    ((cv_data[i - 1].time - cv_data[i].time) * (cv_data[i].soc - soc)
		     / (cv_data[i].soc - cv_data[i - 1].soc)) + cv_data[i].time;
	} else {		/* CC mode || NONE */
		cv_time = cv_data[i].time;
		cc_time =
			design_cap * (cv_data[i].soc - soc) / ttf_curr * 3600 / 1000;
		pr_debug("%s: cc_time: %d\n", __func__, cc_time);
		if (cc_time < 0)
			cc_time = 0;
	}

	pr_info("%s: cap: %d, soc: %4d, T: %6d, avg: %4d, cv soc: %4d, i: %4d, val: %d\n",
	     __func__, design_cap, soc, cv_time + cc_time,
	     battery->i_now, cv_data[i].soc, i, ttf_curr); //battery->current_avg

	if (cv_time + cc_time >= 0)
		return cv_time + cc_time + 60;
	else
		return 60;	/* minimum 1minutes */
}

void sec_bat_calc_time_to_full(struct sec_battery_info *battery)
{
	int pd_enable, pd_max_charge_power;
	union power_supply_propval val = {0, };
	int rc = 0;
	int input_voltage = 0;
	int max_icl = 0, settled_icl = 0;

	if (!battery->ttf_d)
		return;

	pd_enable = get_pd_active(battery);
	pd_max_charge_power = get_pd_max_power();

	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get POWER_SUPPLY_PROP_INPUT_CURRENT_SETTLED. rc=%d\n", __func__, rc);
		settled_icl = 500;    /* in mA */
	} else {
		settled_icl = val.intval / 1000; /* uA -> mA */
	}
	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get input current. rc=%d\n", __func__, rc);
		max_icl = 500;    /* in mA */
	} else {
		max_icl = val.intval / 1000; /* uA -> mA */
	}
	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get input voltage. rc=%d\n", __func__, rc);
		input_voltage = 5;    /* in V */
	} else {
		input_voltage = val.intval / 1000000; /* in V */
	}

	if (max_icl > settled_icl)
		battery->ttf_d->max_charge_power = settled_icl * input_voltage;
	else
		battery->ttf_d->max_charge_power =
			battery->charging_current[battery->cable_real_type].input_current_limit * input_voltage;

	if (delayed_work_pending(&battery->ttf_d->timetofull_work)) {
		pr_info("%s: keep time_to_full(%5d sec)\n", __func__, battery->ttf_d->timetofull);
	} else if ((battery->status == POWER_SUPPLY_STATUS_CHARGING ||
		(battery->status == POWER_SUPPLY_STATUS_FULL && battery->soc != 100))) { //battery->capacity
		int charge = 0;

		if (battery->cable_real_type ==  POWER_SUPPLY_TYPE_USB_HVDCP || //hv_wire_type_case
		battery->cable_real_type == POWER_SUPPLY_TYPE_AFC) {
			charge = battery->ttf_d->ttf_hv_charge_current;
		} else if (battery->cable_real_type == POWER_SUPPLY_TYPE_USB_HVDCP_3 ||
			(battery->cable_real_type == POWER_SUPPLY_TYPE_USB_PD && pd_enable)) {
			if (pd_max_charge_power > HV_CHARGER_STATUS_STANDARD4) {
				charge = battery->ttf_d->ttf_dc45_charge_current;
			} else if (pd_max_charge_power > HV_CHARGER_STATUS_STANDARD3) {
				charge = battery->ttf_d->ttf_dc25_charge_current;
			} else if (pd_max_charge_power <= battery->ttf_d->pd_charging_charge_power &&
				battery->charging_current[battery->cable_real_type].fast_charging_current >=
				battery->ttf_d->max_charging_current) { //same PD power with AFC
				charge = battery->ttf_d->ttf_hv_charge_current;
			} else { //other PD charging
				charge = (pd_max_charge_power / 5) > battery->charging_current[battery->cable_real_type].fast_charging_current ?
					battery->charging_current[battery->cable_real_type].fast_charging_current : (pd_max_charge_power / 5);
			}
		} else {
			charge = (battery->ttf_d->max_charge_power / 5) > battery->charging_current[battery->cable_real_type].fast_charging_current ?
					battery->charging_current[battery->cable_real_type].fast_charging_current : (battery->ttf_d->max_charge_power / 5);
		}
		battery->ttf_d->timetofull = sec_calc_ttf(battery, charge);
		dev_info(battery->dev, "%s: T: %5d sec, current: %d\n",
				__func__, battery->ttf_d->timetofull, charge); //passed_time
	} else {
		battery->ttf_d->timetofull = -1;
	}
}

#ifdef CONFIG_OF
int sec_ttf_parse_dt(struct sec_battery_info *battery)
{
	struct device_node *np;
	struct sec_ttf_data *pdata = battery->ttf_d;
	int ret = 0, len = 0;
	const u32 *p;

	if (!battery->ttf_d)
		return -ENODEV;

	pdata->pdev = battery;
	np = of_find_node_by_name(NULL, "battery");

		if (!np) {
			pr_info("%s: np NULL\n", __func__);
			return 1;
	}

	ret = of_property_read_u32(np, "battery,ttf_hv_12v_charge_current",
					&pdata->ttf_hv_12v_charge_current);
	if (ret) {
		pdata->ttf_hv_12v_charge_current =
			battery->charging_current[POWER_SUPPLY_TYPE_USB_HVDCP_3].fast_charging_current;
		pr_info("%s: ttf_hv_12v_charge_current is Empty, Default value %d\n",
			__func__, pdata->ttf_hv_12v_charge_current);
	}
	ret = of_property_read_u32(np, "battery,ttf_hv_charge_current",
					&pdata->ttf_hv_charge_current);
	if (ret) {
		pdata->ttf_hv_charge_current =
			battery->charging_current[POWER_SUPPLY_TYPE_AFC].fast_charging_current;
		pr_info("%s: ttf_hv_charge_current is Empty, Default value %d\n",
			__func__, pdata->ttf_hv_charge_current);
	}

	ret = of_property_read_u32(np, "battery,ttf_dc25_charge_current",
					&pdata->ttf_dc25_charge_current);
	if (ret) {
		pr_info("%s: ttf_dc25_charge_current is Empty, Default value 0\n", __func__);
		pdata->ttf_dc25_charge_current =
			battery->charging_current[POWER_SUPPLY_TYPE_AFC].fast_charging_current;
	}

	ret = of_property_read_u32(np, "battery,ttf_dc45_charge_current",
					&pdata->ttf_dc45_charge_current);
	if (ret) {
		pr_info("%s: ttf_dc45_charge_current is Empty, Default value 0\n", __func__);
		pdata->ttf_dc45_charge_current = pdata->ttf_dc25_charge_current;
	}

	ret = of_property_read_u32(np, "battery,max_charging_current",
					&pdata->max_charging_current);
	if (ret < 0) {
		pr_err("%s error reading max_charging_current %d\n", __func__, ret);
		pdata->max_charging_current =
			battery->charging_current[POWER_SUPPLY_TYPE_AFC].fast_charging_current;
	}

	ret = of_property_read_u32(np, "battery,pd_charging_charge_power",
					&pdata->pd_charging_charge_power);
	if (ret < 0) {
		pr_err("%s error reading pd_charging_charge_power %d\n", __func__, ret);
		pdata->pd_charging_charge_power = 15000;
	}
	ret = of_property_read_u32(np, "battery,ttf_capacity",
					   &pdata->ttf_capacity);
	if (ret < 0) {
		pr_err("%s error reading capacity_calculation_type %d\n", __func__, ret);
		pdata->ttf_capacity = battery->battery_full_capacity;
	}

	p = of_get_property(np, "battery,cv_data", &len);
	if (p) {
		pdata->cv_data = kzalloc(len, GFP_KERNEL);
		pdata->cv_data_length = len / sizeof(struct sec_cv_slope);
		pr_err("%s: len= %ld, length= %d, %d\n", __func__,
		       sizeof(int) * len, len, pdata->cv_data_length);
		ret = of_property_read_u32_array(np, "battery,cv_data",
				(u32 *)pdata->cv_data, len / sizeof(u32));
		if (ret) {
			pr_err("%s: failed to read battery->cv_data: %d\n",
				__func__, ret);
			kfree(pdata->cv_data);
			pdata->cv_data = NULL;
		}
	} else {
		pr_err("%s: there is not cv_data\n", __func__);
	}
	return 0;
}
#endif

void sec_bat_time_to_full_work(struct work_struct *work)
{
	struct sec_ttf_data *dev = container_of(work,
				struct sec_ttf_data, timetofull_work.work);
	struct sec_battery_info *battery = dev->pdev;
	union power_supply_propval val = {0, };
	int rc = 0;

	rc = power_supply_get_property(battery->psy_bat,
			POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get current now prop. rc=%d\n", __func__, rc);
		battery->i_now = 0;
	} else {
		battery->i_now = val.intval / 1000; /* uA -> mA */
	}

	rc = power_supply_get_property(battery->psy_usb,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_SW_CURRENT_MAX, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get sw current max prop. rc=%d\n", __func__, rc);
		battery->i_max = 500;
	} else {
		battery->i_max = val.intval / 1000; /* uA -> mA */
	}

	rc = power_supply_get_property(battery->psy_usb,
			POWER_SUPPLY_PROP_CURRENT_MAX, &val);
	if (rc < 0) {
		dev_err(battery->dev, "%s: Fail to get hw current max prop. rc=%d\n", __func__, rc);
		battery->hw_max = 500;
	} else {
		battery->hw_max = val.intval / 1000; /* uA -> mA */
	}

	sec_bat_calc_time_to_full(battery);
	dev_info(battery->dev, "%s:\n", __func__);
	if (battery->voltage_now > 0)
		battery->voltage_now--;

	power_supply_changed(battery->psy_bat);
}

void ttf_work_start(struct sec_battery_info *battery)
{
	if (!battery->ttf_d)
		return;

	if (lpcharge) {
		cancel_delayed_work(&battery->ttf_d->timetofull_work);
			queue_delayed_work(battery->monitor_wqueue,
				&battery->ttf_d->timetofull_work, msecs_to_jiffies(1500));
	}
}

int ttf_display(struct sec_battery_info *battery)
{
	if (battery->soc == 100 || !battery->ttf_d) //battery->capacity
		return 0;

	if (((battery->status == POWER_SUPPLY_STATUS_CHARGING) ||
		(battery->status == POWER_SUPPLY_STATUS_FULL && battery->soc != 100)) && //battery->capacity
		!(battery->current_event & SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING))
		return battery->ttf_d->timetofull;
	else
		return 0;
}

void ttf_init(struct sec_battery_info *battery)
{

	battery->ttf_d = kzalloc(sizeof(struct sec_ttf_data),
		GFP_KERNEL);
	if (!battery->ttf_d) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		return;
	}

	sec_ttf_parse_dt(battery);
	battery->ttf_d->timetofull = -1;

	INIT_DELAYED_WORK(&battery->ttf_d->timetofull_work, sec_bat_time_to_full_work);
}
#else
int sec_calc_ttf(struct sec_battery_info *battery, unsigned int ttf_curr) { return -ENODEV; }
void sec_bat_calc_time_to_full(struct sec_battery_info *battery) { }
void sec_bat_time_to_full_work(struct work_struct *work) { }
void ttf_init(struct sec_battery_info *battery) { }
void ttf_work_start(struct sec_battery_info *battery) { }
int ttf_display(struct sec_battery_info *battery) { return 0; }
#ifdef CONFIG_OF
int sec_ttf_parse_dt(struct sec_battery_info *battery) { return -ENODEV; }
#endif
#endif
