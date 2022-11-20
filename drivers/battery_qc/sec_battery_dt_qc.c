/*
 *  sec_battery_dt_qc.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2018 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "include/sec_battery_qc.h"
#include "include/sec_battery_dt_qc.h"


#ifdef CONFIG_OF
int sec_bat_parse_dt(struct device *dev,
		struct sec_battery_info *battery)
{
	struct device_node *np;
	int ret = 0, len = 0;
	unsigned int i = 0;
	const u32 *p;
	u32 temp = 0;

	np = of_find_node_by_name(NULL, "cable-info");
	if (!np) {
		pr_err ("%s : np NULL\n", __func__);
	} else {
		struct device_node *child;
		u32 input_current = 0, charging_current = 0;

		ret = of_property_read_u32(np, "default_input_current", &input_current);
		ret = of_property_read_u32(np, "default_charging_current", &charging_current);

		battery->default_input_current = input_current;
		battery->default_charging_current = charging_current;

		battery->charging_current =
			kzalloc(sizeof(sec_charging_current_t) * POWER_SUPPLY_TYPE_MAX,
				GFP_KERNEL);

		for (i = 0; i < POWER_SUPPLY_TYPE_MAX; i++) {
			battery->charging_current[i].input_current_limit = (unsigned int)input_current;
			battery->charging_current[i].fast_charging_current = (unsigned int)charging_current;
		}

		for_each_child_of_node(np, child) {
			ret = of_property_read_u32(child, "input_current", &input_current);
			ret = of_property_read_u32(child, "charging_current", &charging_current);

			p = of_get_property(child, "cable_number", &len);
			if (!p)
				return 1;

			len = len / sizeof(u32);

			for (i = 0; i <= len; i++) {
				ret = of_property_read_u32_index(child, "cable_number", i, &temp);
				battery->charging_current[temp].input_current_limit = (unsigned int)input_current;
				battery->charging_current[temp].fast_charging_current = (unsigned int)charging_current;
			}

		}
	}

	for (i = 0; i < POWER_SUPPLY_TYPE_MAX; i++) {
		pr_info("%s : CABLE_NUM(%d) INPUT(%d) CHARGING(%d)\n",
			__func__, i,
			battery->charging_current[i].input_current_limit,
			battery->charging_current[i].fast_charging_current);
	}

#ifdef CONFIG_SEC_FACTORY
	battery->default_charging_current = 1500;
	battery->charging_current[POWER_SUPPLY_TYPE_USB_DCP].fast_charging_current = 1500;
#if defined(CONFIG_SEC_A70Q_PROJECT) || defined(CONFIG_SEC_A70S_PROJECT)
	battery->charging_current[POWER_SUPPLY_TYPE_AFC].fast_charging_current = 2400;
#endif
#endif

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_info("%s: np NULL\n", __func__);
		return 1;
	}

	ret = of_property_read_u32(np, "battery,battery_full_capacity",
		&battery->battery_full_capacity);

	if (ret) {
		pr_info("%s : battery_full_capacity is Empty\n", __func__);
	}
#if defined(CONFIG_BATTERY_CISD)
 	else {
		pr_info("%s : battery_full_capacity : %d\n", __func__, battery->battery_full_capacity);
		battery->cisd_cap_high_thr = battery->battery_full_capacity + 1000; /* battery_full_capacity + 1000 */
		battery->cisd_cap_low_thr = battery->battery_full_capacity + 500; /* battery_full_capacity + 500 */
		battery->cisd_cap_limit = (battery->battery_full_capacity * 11) / 10; /* battery_full_capacity + 10% */
	}

	ret = of_property_read_u32(np, "battery,cisd_max_voltage_thr",
		&battery->max_voltage_thr);
	if (ret) {
		pr_info("%s : cisd_max_voltage_thr is Empty\n", __func__);
		battery->max_voltage_thr = 4400;
	}

	ret = of_property_read_u32(np, "battery,cisd_alg_index",
			&battery->cisd_alg_index);
	if (ret) {
		pr_info("%s : cisd_alg_index is Empty. Defalut set to six\n", __func__);
		battery->cisd_alg_index = 6;
	} else {
		pr_info("%s : set cisd_alg_index : %d\n", __func__, battery->cisd_alg_index);
	}
#endif
	ret = of_property_read_u32(np, "battery,store_mode_polling_time",
			&battery->store_mode_polling_time);

	if (ret) {
		pr_info("%s : store_mode_polling_time is Empty\n", __func__);
		battery->store_mode_polling_time = 30;
	}

	battery->gpio_vbat_sense = of_get_named_gpio(np,"battery,gpio_vbat_sense", 0);
	if (battery->gpio_vbat_sense < 0) {
		pr_info("%s : gpio_vbat_sense is Empty\n", __func__);
		battery->gpio_vbat_sense = 0;
	}
	else {
		gpio_request(battery->gpio_vbat_sense, "battery,gpio_vbat_sense");
	}

	battery->gpio_sub_pcb_detect = of_get_named_gpio(np,"battery,gpio_sub_pcb_detect", 0);
	if (battery->gpio_sub_pcb_detect < 0) {
		pr_info("%s : gpio_sub_pcb_detect is Empty\n", __func__);
		battery->gpio_sub_pcb_detect = 0;
	}
	else {
		gpio_request(battery->gpio_sub_pcb_detect, "battery,gpio_sub_pcb_detect");
	}

	ret = of_property_read_u32(np, "battery,full_check_current_1st",
			&battery->full_check_current_1st);
	if (ret) {
		pr_info("%s : full_check_current_1st is Empty\n", __func__);
		battery->full_check_current_1st = 400;
	}

	battery->use_temp_adc_table = of_property_read_bool(np,
					"battery,use_temp_adc_table");
	battery->use_chg_temp_adc_table = of_property_read_bool(np,
					"battery,use_chg_temp_adc_table");

	if (battery->use_temp_adc_table) {
		p = of_get_property(np, "battery,temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		battery->temp_adc_table_size = len;

		battery->temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				battery->temp_adc_table_size, GFP_KERNEL);

		for(i = 0; i < battery->temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
					 "battery,temp_table_adc", i, &temp);
			battery->temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : Temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,temp_table_data", i, &temp);
			battery->temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : Temp_adc_table(data) is Empty\n",
					__func__);
		}
	}

	if (battery->use_chg_temp_adc_table) {
		p = of_get_property(np, "battery,chg_temp_table_adc", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		battery->chg_temp_adc_table_size = len;

		battery->chg_temp_adc_table =
			kzalloc(sizeof(sec_bat_adc_table_data_t) *
				battery->chg_temp_adc_table_size, GFP_KERNEL);

		for(i = 0; i < battery->chg_temp_adc_table_size; i++) {
			ret = of_property_read_u32_index(np,
							 "battery,chg_temp_table_adc", i, &temp);
			battery->chg_temp_adc_table[i].adc = (int)temp;
			if (ret)
				pr_info("%s : CHG_Temp_adc_table(adc) is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
							 "battery,chg_temp_table_data", i, &temp);
			battery->chg_temp_adc_table[i].data = (int)temp;
			if (ret)
				pr_info("%s : CHG_Temp_adc_table(data) is Empty\n",
					__func__);
		}
	}

	ret = of_property_read_u32(np, "battery,mix_temp_recovery",
			&battery->mix_temp_recovery);

	if (ret) {
		pr_info("%s : mix_temp_recovery is Empty\n", __func__);
		battery->mix_temp_recovery = 390;
	}

	ret = of_property_read_u32(np, "battery,mix_temp_batt_threshold",
			&battery->mix_temp_batt_threshold);

	if (ret) {
		pr_info("%s : mix_temp_batt_threshold is Empty\n", __func__);
		battery->mix_temp_batt_threshold = 420;
	}

	ret = of_property_read_u32(np, "battery,mix_temp_die_threshold",
			&battery->mix_temp_die_threshold);

	if (ret) {
		pr_info("%s : mix_temp_die_threshold is Empty\n", __func__);
		battery->mix_temp_die_threshold = 500;
	}

	ret = of_property_read_u32(np, "battery,chg_high_temp",
			&battery->chg_high_temp);
	if (ret) {
		pr_info("%s : chg_high_temp is Empty\n", __func__);
		battery->chg_high_temp = 490;
	}

	ret = of_property_read_u32(np, "battery,chg_high_temp_recovery",
			&battery->chg_high_temp_recovery);
	if (ret) {
		pr_info("%s : chg_temp_recovery is Empty\n", __func__);
		battery->chg_high_temp_recovery = 470;
	}

	ret = of_property_read_u32(np, "battery,chg_charging_limit_current",
			&battery->chg_charging_limit_current);
	if (ret) {
		pr_info("%s : chg_charging_limit_current is Empty\n", __func__);
		battery->chg_charging_limit_current = 1500;
	}

	ret = of_property_read_u32(np, "battery,chg_input_limit_current",
			&battery->chg_input_limit_current);
	if (ret) {
		pr_info("%s : chg_input_limit_current is Empty\n", __func__);
		battery->chg_input_limit_current = 1000;
	}

#if defined(CONFIG_DIRECT_CHARGING)
	battery->check_dc_chg_lmit = of_property_read_bool(np,
					"battery,check_dc_chg_lmit");

	if (battery->check_dc_chg_lmit) {
		ret = of_property_read_u32(np, "battery,dc_chg_high_temp",
				&battery->dc_chg_high_temp);
		if (ret) {
			pr_info("%s : dc_chg_high_temp is Empty\n", __func__);
			battery->dc_chg_high_temp = 560;
		}

		ret = of_property_read_u32(np, "battery,dc_chg_high_temp_recovery",
				&battery->dc_chg_high_temp_recovery);
		if (ret) {
			pr_info("%s : dc_chg_high_temp_recovery is Empty\n", __func__);
			battery->dc_chg_high_temp_recovery = 540;
		}

		ret = of_property_read_u32(np, "battery,dc_chg_input_limit_current",
				&battery->dc_chg_input_limit_current);
		if (ret) {
			pr_info("%s : dc_chg_input_limit_current is Empty\n", __func__);
			battery->dc_chg_input_limit_current = 1200;
		}
	}
#endif

	ret = of_property_read_u32(np, "battery,safety_timer_polling_time",
			&battery->safety_timer_polling_time);
	if (ret) {
		pr_info("%s : safet_timer_polling_time is Empty\n", __func__);
		battery->safety_timer_polling_time = 30;
	}

	ret = of_property_read_u32(np, "battery,expired_time", &battery->pdata_expired_time);
	if (ret) {
		pr_info("expired time is empty\n");
		battery->pdata_expired_time = 3 * 60 * 60;
	}
	battery->pdata_expired_time *= 1000;
	battery->expired_time = battery->pdata_expired_time;

	ret = of_property_read_u32(np, "battery,recharging_expired_time", &battery->recharging_expired_time);
	if (ret) {
		pr_info("expired time is empty\n");
		battery->recharging_expired_time = 90 * 60;
	}
	battery->recharging_expired_time *= 1000;

	ret = of_property_read_u32(np, "battery,standard_curr", &battery->standard_curr);
	if (ret) {
		pr_info("standard_curr is empty\n");
		battery->standard_curr = 2820000;
	}

	ret = of_property_read_u32(np, "battery,siop_input_limit_current",
			&battery->siop_input_limit_current);
	if (ret)
		battery->siop_input_limit_current = SIOP_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_charging_limit_current",
			&battery->siop_charging_limit_current);
	if (ret)
		battery->siop_charging_limit_current = SIOP_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_12v_input_limit_current",
			&battery->siop_hv_12v_input_limit_current);
	if (ret)
		battery->siop_hv_12v_input_limit_current = SIOP_HV_12V_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_12v_charging_limit_current",
			&battery->siop_hv_12v_charging_limit_current);
	if (ret)
		battery->siop_hv_12v_charging_limit_current = SIOP_HV_12V_CHARGING_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_input_limit_current",
			&battery->siop_hv_input_limit_current);
	if (ret)
		battery->siop_hv_input_limit_current = SIOP_HV_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_hv_charging_limit_current",
			&battery->siop_hv_charging_limit_current);
	if (ret)
		battery->siop_hv_charging_limit_current = SIOP_HV_CHARGING_LIMIT_CURRENT;
	ret = of_property_read_u32(np, "battery,siop_dc_input_limit_current",
			&battery->siop_dc_input_limit_current);
	if (ret)
		battery->siop_dc_input_limit_current = SIOP_DC_INPUT_LIMIT_CURRENT;

	ret = of_property_read_u32(np, "battery,siop_dc_charging_limit_current",
			&battery->siop_dc_charging_limit_current);
	if (ret)
		battery->siop_dc_charging_limit_current = SIOP_DC_CHARGING_LIMIT_CURRENT;
	
	ret = of_property_read_u32(np, "battery,minimum_hv_input_current_by_siop_10",
			&battery->minimum_hv_input_current_by_siop_10);
	if (ret)
		battery->minimum_hv_input_current_by_siop_10 = 0;
	
	return 0;
}
#endif
