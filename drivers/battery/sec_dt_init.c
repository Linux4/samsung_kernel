/*
 *  sec_dt_init.c
 *  Samsung battery data parsing from device tree
 *
 *  Copyright (C) 2014 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/battery/sec_battery.h>

#ifdef CONFIG_MFD_RT8973
#include <linux/mfd/rt8973.h>
#endif

static int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
static u8 attached_cable;
#if !defined(CONFIG_EXTCON)
static bool is_jig_on;
#endif
unsigned int lpcharge;
EXPORT_SYMBOL(lpcharge);
static unsigned chg_irq;

#if defined(CONFIG_INPUT_TOUCHSCREEN)
void (*tsp_charger_status_cb)(int);
EXPORT_SYMBOL(tsp_charger_status_cb);
#endif

#if defined(CONFIG_MACH_DEGASVELTE) || defined(CONFIG_MACH_XCOVER3LTE)
extern int otg_enable_flag;
#endif

static bool sec_fg_gpio_init(void)
{
	return true;
}

static bool sec_fg_fuelalert_process(bool is_fuel_alerted) { return true; }

static bool sec_bat_adc_none_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_none_exit(void) { return true; }
static int sec_bat_adc_none_read(unsigned int channel) { return 0; }

static bool sec_bat_adc_ap_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ap_exit(void) { return true; }

static bool sec_bat_adc_ic_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ic_exit(void) { return true; }
static int sec_bat_adc_ic_read(unsigned int channel) { return 0; }

static sec_bat_adc_api_t adc_api[SEC_BATTERY_ADC_TYPE_NUM] = {
	{
		.init = sec_bat_adc_none_init,
		.exit = sec_bat_adc_none_exit,
		.read = sec_bat_adc_none_read
	},
	{
		.init = sec_bat_adc_ap_init,
		.exit = sec_bat_adc_ap_exit,
	},
	{
		.init = sec_bat_adc_ic_init,
		.exit = sec_bat_adc_ic_exit,
		.read = sec_bat_adc_ic_read
	}
};

bool sec_bat_check_cable_result_callback(int cable_type)
{
	bool ret = true;
	current_cable_type = cable_type;

	switch (cable_type) {
	case POWER_SUPPLY_TYPE_USB:
		pr_info("%s set vbus applied\n",
				__func__);
		break;
	case POWER_SUPPLY_TYPE_BATTERY:
		pr_info("%s set vbus cut\n",
				__func__);
		break;
	case POWER_SUPPLY_TYPE_MAINS:
		break;
	default:
		pr_err("%s cable type (%d)\n",
				__func__, cable_type);
		ret = false;
		break;
	}

	return ret;
}

static bool sec_bat_ovp_uvlo_result_callback(int health)
{
	return true;
}

/* Get LP charging mode state */
static int sec_bat_is_lpm_check(char *str)
{
        if (strncmp(str, "charger", 7) == 0)
                lpcharge = 1;

        pr_info("%s: Low power charging mode: %d\n", __func__, lpcharge);

        return lpcharge;
}
__setup("androidboot.mode=", sec_bat_is_lpm_check);

bool sec_bat_is_lpm(void)
{
	return lpcharge == 1 ? true : false;
}
/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	return true;
}

#if !defined(CONFIG_EXTCON)
void check_jig_status(int status)
{
	if (status) {
		pr_info("%s: JIG On so reset fuel gauge capacity\n", __func__);
		is_jig_on = true;
	}

}

bool sec_bat_check_jig_status(void)
{
	return is_jig_on;
}
EXPORT_SYMBOL(sec_bat_check_jig_status);
#endif

static int sec_bat_check_cable_callback(void)
{
	int ta_nconnected;
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	ta_nconnected = gpio_get_value(chg_irq);

	pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);

	if (ta_nconnected)
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
	else
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;

#ifdef CONFIG_MFD_RT8973
	if (attached_cable == MUIC_RT8973_CABLE_TYPE_0x1A) {
		if (ta_nconnected)
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		else
			current_cable_type = POWER_SUPPLY_TYPE_MISC;
	} else
		return current_cable_type;
#endif
	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}

	return current_cable_type;
}

static void sec_bat_initial_check(void)
{
	union power_supply_propval value;

	if (POWER_SUPPLY_TYPE_BATTERY < current_cable_type) {
		value.intval = current_cable_type;
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_ONLINE, value);
	} else {
		psy_do_property("sec-charger", get,
				POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_WIRELESS) {
			value.intval = POWER_SUPPLY_TYPE_WIRELESS;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
#if 0
		else {
			value.intval = POWER_SUPPLY_TYPE_BATTERY;
				psy_do_property("sec-charger", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
#endif
	}
}

static bool sec_bat_gpio_init(void)
{
	return true;
}

static bool sec_bat_check_callback(void) { return true; }
static bool sec_bat_check_result_callback(void) { return true; }

/* callback for OVP/UVLO check
 * return : int
 * battery health
 */
static int sec_bat_ovp_uvlo_callback(void)
{
	int health;
	health = POWER_SUPPLY_HEALTH_GOOD;

	return health;
}

#if defined(CONFIG_SENSORS_SX9500) && defined(CONFIG_MACH_DEGASVELTE)
extern void charger_status_cb(int status);
#endif

#ifdef CONFIG_MFD_RT8973
void sec_charger_cb(u8 cable_type)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	pr_info("%s: cable type (0x%02x)\n", __func__, cable_type);

	attached_cable = cable_type;

	switch (cable_type) {
	case MUIC_RT8973_CABLE_TYPE_NONE:
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case MUIC_RT8973_CABLE_TYPE_USB:
	case MUIC_RT8973_CABLE_TYPE_CDP:
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_ON:
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_OFF:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case MUIC_RT8973_CABLE_TYPE_REGULAR_TA:
	case MUIC_RT8973_CABLE_TYPE_ATT_TA:
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS:
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_ON_WITH_VBUS:
	case MUIC_RT8973_CABLE_TYPE_TYPE1_CHARGER:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case MUIC_RT8973_CABLE_TYPE_OTG:
		goto skip;
	case MUIC_RT8973_CABLE_TYPE_0x15:
	case MUIC_RT8973_CABLE_TYPE_0x1A:
	case MUIC_RT8973_CABLE_TYPE_0x1A_VBUS:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, cable_type);
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
		goto skip;
	}

	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}

#if defined(CONFIG_INPUT_TOUCHSCREEN)
	/* for changing of tsp operation mode */
	if (tsp_charger_status_cb)
		tsp_charger_status_cb(current_cable_type);
#endif

skip:
	return 0;
}
#endif

void sec_charger_cb(u8 cable_type)
{
#if !defined(CONFIG_EXTCON)
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	struct power_supply *psy_p = power_supply_get_by_name("ps");
	static bool ps_is_on = false;

	pr_info("%s: cable type (0x%02x)\n", __func__, cable_type);

	attached_cable = cable_type;
	is_jig_on = false;

#if defined(CONFIG_MACH_DEGASVELTE) || defined(CONFIG_MACH_XCOVER3LTE)
		pr_info("%s: otg_enable_flag (%d)\n", __func__, otg_enable_flag);
		if (otg_enable_flag &&
			((cable_type == CABLE_JIG_UART_OFF_VB) ||
			(cable_type == CABLE_JIG_UART_OFF))) {
			pr_info("%s: otg attached, skip jig uart off cable\n", __func__);
			goto skip;
		}
#endif

	switch (cable_type) {
	case CABLE_NONE:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_JIG_UART_ON:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		is_jig_on = true;
		break;
	case CABLE_USB:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TA:
	case CABLE_DESKDOCK_VB:
		pr_info("%s: VBUS Online\n", __func__);
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_JIG_UART_OFF_VB:
		is_jig_on = true;
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_OTG:
#if defined(CONFIG_MACH_DEGASVELTE) || defined(CONFIG_MACH_XCOVER3LTE)
		current_cable_type = POWER_SUPPLY_TYPE_OTG;
#else
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
#endif
		break;
	case CABLE_CDP:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_DCP:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_JIG_UART_OFF:
		is_jig_on = true;
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_JIG_USB_ON:
	case CABLE_JIG_USB_OFF:
		is_jig_on = true;
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_POWER_SHARING_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_POWER_SHARING;
		break;
	case CABLE_UNKNOWN:
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, cable_type);
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
		goto skip;
	}

	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);

#if defined(CONFIG_INPUT_TOUCHSCREEN)
	/* for changing of tsp operation mode */
		if (tsp_charger_status_cb)
			tsp_charger_status_cb(current_cable_type);
#endif
	}
#if defined(CONFIG_SENSORS_SX9500) && defined(CONFIG_MACH_DEGASVELTE)
	/* for grip sensor threshold change at TA/USB status - sx9500.c */
	charger_status_cb(current_cable_type);
#endif


skip:
	return;
#endif
}

/* MUIC check battery connected function */
int is_battery_connected(void)
{
	union power_supply_propval value;

	psy_do_property("sec-fuelgauge", get,
		POWER_SUPPLY_PROP_PRESENT, value);
	if (value.intval == 1)
		pr_info("%s Battery present \n", __func__);
	else if (value.intval == 0)
		pr_info("%s Battery NOT present \n", __func__);
	else {
		pr_info("%s Battery state Unknown \n", __func__);
		return -1;
	}

	return value.intval;
}

int sec_bat_dt_init(struct device_node *np,
			 struct device *dev,
			 sec_battery_platform_data_t *pdata)
{
#if !defined(CONFIG_EXTCON)
	int ret, i, len;
	unsigned int bat_irq_attr;
        const u32 *p;

	ret = of_property_read_string(np,
		"vendor", (char const **)&pdata->vendor);
	if (ret)
		pr_info("%s: Vendor is Empty\n", __func__);

	ret = of_property_read_string(np,
		"charger_name", (char const **)&pdata->charger_name);
	if (ret)
		pr_info("%s: charger_name is Empty\n", __func__);

	ret = of_property_read_string(np,
		"fuelgauge_name", (char const **)&pdata->fuelgauge_name);
	if (ret)
		pr_info("%s: fuelgauge_name is Empty\n", __func__);

	ret = of_property_read_u32(np, "technology",
		&pdata->technology);
	if (ret)
		pr_info("%s : technology is Empty\n", __func__);

	p = of_get_property(np, "polling_time", &len);
	if (!p)
		return 1;

	len = len / sizeof(u32);
	pdata->polling_time = kzalloc(sizeof(*pdata->polling_time) * len, GFP_KERNEL);
	ret = of_property_read_u32_array(np, "polling_time",
			pdata->polling_time, len);

	p = of_get_property(np, "temp_table_adc", &len);
	if (!p)
		return 1;

	len = len / sizeof(u32);

	pdata->temp_adc_table_size = len;
	pdata->temp_amb_adc_table_size = len;

	pdata->temp_adc_table =
		kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->temp_adc_table_size, GFP_KERNEL);
	pdata->temp_amb_adc_table =
		kzalloc(sizeof(sec_bat_adc_table_data_t) *
				pdata->temp_adc_table_size, GFP_KERNEL);

	for(i = 0; i < pdata->temp_adc_table_size; i++) {
		ret = of_property_read_u32_index(np,
				"temp_table_adc", i,
				&pdata->temp_adc_table[i].adc);
		if (ret)
			pr_info("%s : Temp_adc_table(adc) is Empty\n",
					__func__);

		ret = of_property_read_u32_index(np,
				"temp_table_data", i,
				&pdata->temp_adc_table[i].data);
		if (ret)
			pr_info("%s : Temp_adc_table(data) is Empty\n",
					__func__);

		ret = of_property_read_u32_index(np,
				"temp_table_adc", i,
				&pdata->temp_amb_adc_table[i].adc);
		if (ret)
			pr_info("%s : Temp_amb_adc_table(adc) is Empty\n",
					__func__);

		ret = of_property_read_u32_index(np,
				"temp_table_data", i,
				&pdata->temp_amb_adc_table[i].data);
		if (ret)
			pr_info("%s : Temp_amb_adc_table(data) is Empty\n",
					__func__);
	}

	p = of_get_property(np, "input_current_limit", &len);
	if (!p)
		return 1;

	len = len / sizeof(u32);

	pdata->charging_current =
		kzalloc(sizeof(sec_charging_current_t) * len,
				GFP_KERNEL);

	for(i = 0; i < len; i++) {
		ret = of_property_read_u32_index(np,
				"input_current_limit", i,
				&pdata->charging_current[i].input_current_limit);
		if (ret)
			pr_info("%s : Input_current_limit is Empty\n",
					__func__);

		ret = of_property_read_u32_index(np,
				"fast_charging_current", i,
				&pdata->charging_current[i].fast_charging_current);
		if (ret)
			pr_info("%s : Fast charging current is Empty\n",
					__func__);

		ret = of_property_read_u32_index(np,
				"full_check_current_1st", i,
				&pdata->charging_current[i].full_check_current_1st);
		if (ret)
			pr_info("%s : Full check current 1st is Empty\n",
					__func__);

		ret = of_property_read_u32_index(np,
				"full_check_current_2nd", i,
				&pdata->charging_current[i].full_check_current_2nd);
		if (ret)
			pr_info("%s : Full check current 2nd is Empty\n",
					__func__);
	}

#if defined(CONFIG_BATTERY_SWELLING)
	ret = of_property_read_u32(np, "swelling_chg_current",
			&pdata->swelling_chg_current);
	if (ret)
		pr_info("%s: swelling low temp chg current is Empty\n", __func__);
#endif

	ret = of_property_read_u32(np, "adc_check_count",
			&pdata->adc_check_count);
	if (ret)
		pr_info("%s : Adc check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "cable_check_type",
			&pdata->cable_check_type);
	if (ret)
		pr_info("%s : Cable check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "cable_source_type",
			&pdata->cable_source_type);
	if (ret)
		pr_info("%s : Cable source type is Empty\n", __func__);

	ret = of_property_read_u32(np, "event_waiting_time",
			&pdata->event_waiting_time);
	if (ret)
		pr_info("%s : Event waiting time is Empty\n", __func__);

	ret = of_property_read_u32(np, "polling_type",
			&pdata->polling_type);
	if (ret)
		pr_info("%s : Polling type is Empty\n", __func__);

	ret = of_property_read_u32(np, "monitor_initial_count",
			&pdata->monitor_initial_count);
	if (ret)
		pr_info("%s : Monitor initial count is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery_check_type",
			&pdata->battery_check_type);
	if (ret)
		pr_info("%s : Battery check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "check_count",
			&pdata->check_count);

	if (ret)
		pr_info("%s : Check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "check_adc_max",
			&pdata->check_adc_max);
	if (ret)
		pr_info("%s : Check adc max is Empty\n", __func__);

	ret = of_property_read_u32(np, "check_adc_min",
			&pdata->check_adc_min);
	if (ret)
		pr_info("%s : Check adc min is Empty\n", __func__);

	ret = of_property_read_u32(np, "ovp_uvlo_check_type",
			&pdata->ovp_uvlo_check_type);
	if (ret)
		pr_info("%s : Ovp Uvlo check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "thermal_source",
			&pdata->thermal_source);
	if (ret)
		pr_info("%s : Thermal source is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_check_type",
			&pdata->temp_check_type);
	if (ret)
		pr_info("%s : Temp check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_check_count",
			&pdata->temp_check_count);
	if (ret)
		pr_info("%s : Temp check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_high_recovery_event",
			&pdata->temp_high_recovery_event);
	if (ret)
		pr_info("%s : Temp high recovery event is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_highlimit_threshold_event",
		&pdata->temp_highlimit_threshold_event);
	if (ret)
		pr_info("%s : Temp highlimit threshold event is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_highlimit_recovery_event",
		&pdata->temp_highlimit_recovery_event);
	if (ret)
		pr_info("%s : Temp highlimit threshold event recovery is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_highlimit_threshold_normal",
		&pdata->temp_highlimit_threshold_normal);
	if (ret)
		pr_info("%s : Temp highlimit threshold normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_highlimit_recovery_normal",
		&pdata->temp_highlimit_recovery_normal);
	if (ret)
		pr_info("%s : Temp highlimit threshold normal recovery is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_highlimit_threshold_lpm",
		&pdata->temp_highlimit_threshold_lpm);
	if (ret)
		pr_info("%s : Temp highlimit threshold lpm is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_highlimit_recovery_lpm",
		&pdata->temp_highlimit_recovery_lpm);
	if (ret)
		pr_info("%s : Temp highlimit threshold lpm recovery is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_high_threshold_event",
		&pdata->temp_high_threshold_event);
	if (ret)
		pr_info("%s : Temp high threshold event is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_low_threshold_event",
			&pdata->temp_low_threshold_event);
	if (ret)
		pr_info("%s : Temp low threshold event is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_low_recovery_event",
			&pdata->temp_low_recovery_event);
	if (ret)
		pr_info("%s : Temp low recovery event is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_high_threshold_normal",
			&pdata->temp_high_threshold_normal);
	if (ret)
		pr_info("%s : Temp high threshold normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_high_recovery_normal",
			&pdata->temp_high_recovery_normal);
	if (ret)
		pr_info("%s : Temp high recovery normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_low_threshold_normal",
			&pdata->temp_low_threshold_normal);
	if (ret)
		pr_info("%s : Temp low threshold normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_low_recovery_normal",
			&pdata->temp_low_recovery_normal);

	if (ret)
		pr_info("%s : Temp low recovery normal is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_high_threshold_lpm",
			&pdata->temp_high_threshold_lpm);
	if (ret)
		pr_info("%s : Temp high threshold lpm is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_high_recovery_lpm",
			&pdata->temp_high_recovery_lpm);
	if (ret)
		pr_info("%s : Temp high recovery lpm is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_low_threshold_lpm",
			&pdata->temp_low_threshold_lpm);
	if (ret)
		pr_info("%s : Temp low threshold lpm is Empty\n", __func__);

	ret = of_property_read_u32(np, "temp_low_recovery_lpm",
			&pdata->temp_low_recovery_lpm);
	if (ret)
		pr_info("%s : Temp low recovery lpm is Empty\n", __func__);

	ret = of_property_read_u32(np, "full_check_type",
			&pdata->full_check_type);
	if (ret)
		pr_info("%s : Full check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "full_check_type_2nd",
			&pdata->full_check_type_2nd);
	if (ret)
		pr_info("%s : Full check type 2nd is Empty\n", __func__);

	ret = of_property_read_u32(np, "full_check_count",
			&pdata->full_check_count);
	if (ret)
		pr_info("%s : Full check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "chg_gpio_full_check",
			&pdata->chg_gpio_full_check);
	if (ret)
		pr_info("%s : Chg gpio full check is Empty\n", __func__);

	ret = of_property_read_u32(np, "chg_polarity_full_check",
			&pdata->chg_polarity_full_check);
	if (ret)
		pr_info("%s : Chg polarity full check is Empty\n", __func__);

	ret = of_property_read_u32(np, "full_condition_type",
			&pdata->full_condition_type);
	if (ret)
		pr_info("%s : Full condition type is Empty\n", __func__);

	ret = of_property_read_u32(np, "full_condition_soc",
			&pdata->full_condition_soc);
	if (ret)
		pr_info("%s : Full condition soc is Empty\n", __func__);

	ret = of_property_read_u32(np, "full_condition_vcell",
			&pdata->full_condition_vcell);
	if (ret)
		pr_info("%s : Full condition vcell is Empty\n", __func__);

	ret = of_property_read_u32(np, "recharge_check_count",
			&pdata->recharge_check_count);
	if (ret)
		pr_info("%s : Recharge check count is Empty\n", __func__);

	ret = of_property_read_u32(np, "recharge_condition_type",
			&pdata->recharge_condition_type);
	if (ret)
		pr_info("%s : Recharge condition type is Empty\n", __func__);

	ret = of_property_read_u32(np, "recharge_condition_soc",
			&pdata->recharge_condition_soc);
	if (ret)
		pr_info("%s : Recharge condition soc is Empty\n", __func__);

	ret = of_property_read_u32(np, "recharge_condition_vcell",
			&pdata->recharge_condition_vcell);
	if (ret)
		pr_info("%s : Recharge condition vcell is Empty\n", __func__);

	ret = of_property_read_u32(np, "recharge_condition_avgvcell",
			&pdata->recharge_condition_avgvcell);
	if (ret)
		pr_info("%s : Recharge condition avgvcell is Empty\n", __func__);

	ret = of_property_read_u32(np, "charging_total_time",
			(unsigned int *)&pdata->charging_total_time);
	if (ret)
		pr_info("%s : Charging total time is Empty\n", __func__);

	ret = of_property_read_u32(np, "recharging_total_time",
			(unsigned int *)&pdata->recharging_total_time);
	if (ret)
		pr_info("%s : Recharging total time is Empty\n", __func__);

	ret = of_property_read_u32(np, "charging_reset_time",
			(unsigned int *)&pdata->charging_reset_time);
	if (ret)
		pr_info("%s : Charging reset time is Empty\n", __func__);

	ret = of_property_read_u32(np, "charging_reset_time",
			(unsigned int *)&pdata->charging_reset_time);
	if (ret)
		pr_info("%s : Charging reset time is Empty\n", __func__);

	ret = of_property_read_u32(np, "bat-irq-attr", &bat_irq_attr);
	if (ret)
		pr_info("%s : Battery irq is Empty\n", __func__);

	pdata->bat_gpio_init = sec_bat_gpio_init;
	pdata->check_battery_callback =	sec_bat_check_callback;
	pdata->check_battery_result_callback = sec_bat_check_result_callback;
	pdata->check_cable_callback = sec_bat_check_cable_callback;
	pdata->check_cable_result_callback =
		sec_bat_check_cable_result_callback;
	pdata->get_temperature_callback = sec_bat_get_temperature_callback;
	pdata->initial_check = sec_bat_initial_check;
	pdata->is_lpm = sec_bat_is_lpm;
	pdata->ovp_uvlo_callback = sec_bat_ovp_uvlo_callback;
	pdata->ovp_uvlo_result_callback = sec_bat_ovp_uvlo_result_callback;
	pdata->check_jig_status = sec_bat_check_jig_status;

	for (i = 0; i < SEC_BATTERY_ADC_TYPE_NUM; i++)
		pdata->adc_api[i] = adc_api[i];
#endif
	return 0;
}

int sec_chg_dt_init(struct device_node *np,
			 struct device *dev,
			 sec_battery_platform_data_t *pdata)
{
#if !defined(CONFIG_EXTCON)
	int ret = 0, len = 0;
	unsigned int chg_irq_attr = 0;
	int chg_gpio_en = 0;

	if (!np)
		return -EINVAL;
/*
	chg_gpio_en = of_get_named_gpio(np, "chgen-gpio", 0);
	if (chg_gpio_en < 0) {
		pr_err("%s: of_get_named_gpio failed: %d\n", __func__,
							chg_gpio_en);
		return chg_gpio_en;
	}

	ret = gpio_request(chg_gpio_en, "chgen-gpio");
	if (ret) {
		pr_err("%s gpio_request failed: %d\n", __func__, chg_gpio_en);
		return ret;
	}

	chg_irq = of_get_named_gpio(np, "chgirq-gpio", 0);
	if (chg_irq < 0) {
		pr_err("%s: of_get_named_gpio failed: %d\n", __func__,
								chg_irq);
		return chg_irq;
	}

	ret = gpio_request(chg_irq, "chgirq-gpio");
	if (ret) {
		pr_err("%s: gpio_request failed: chg_irq %d\n", __func__,
								chg_irq);
		return ret;
	}

	ret = of_property_read_u32(np, "chg-irq-attr", &chg_irq_attr);
	if (ret)
		return ret;
*/

	np = of_find_node_by_name(NULL, "sec_battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		int i = 0;
		const u32 *p;
		p = of_get_property(np, "input_current_limit", &len);
		if (!p) {
			pr_info("%s : RETURN ONE\n", __func__);
			return 1;
		}

		len = len / sizeof(u32);

		pdata->charging_current = kzalloc(sizeof(sec_charging_current_t) * len,
				GFP_KERNEL);

		for(i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
				"input_current_limit", i,
				&pdata->charging_current[i].input_current_limit);
			if (ret)
				pr_info("%s : Input_current_limit is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
				"fast_charging_current", i,
				&pdata->charging_current[i].fast_charging_current);
//			pr_info("%s : fast_charging_current %d\n", __func__, pdata->charging_current[i].fast_charging_current);
			if (ret)
				pr_info("%s : Fast charging current is Empty\n",
						__func__);

			ret = of_property_read_u32_index(np,
				"full_check_current_1st", i,
				&pdata->charging_current[i].full_check_current_1st);
			if (ret)
				pr_info("%s : Full check current 1st is Empty\n",
						__func__);

			ret = of_property_read_u32_index(np,
				"full_check_current_2nd", i,
				&pdata->charging_current[i].full_check_current_2nd);
			if (ret)
				pr_info("%s : Full check current 2nd is Empty\n",
						__func__);
		}
	}

	ret = of_property_read_u32(np, "chg-float-voltage",
					&pdata->chg_float_voltage);
	if (ret)
		pr_info("%s : chg-float-voltage is Empty\n", __func__);

	ret = of_property_read_u32(np, "ovp_uvlo_check_type",
			&pdata->ovp_uvlo_check_type);
	if (ret)
		pr_info("%s : Ovp Uvlo check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "full_check_type",
			&pdata->full_check_type);
	if (ret)
		pr_info("%s : Full check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "full_check_type_2nd",
			&pdata->full_check_type_2nd);
	if (ret)
		pr_info("%s : Full check type 2nd is Empty\n", __func__);

	ret = of_property_read_u32(np, "full_condition_vcell",
			&pdata->full_condition_vcell);
	if (ret)
		pr_info("%s : Full condition vcell is Empty\n", __func__);

	pdata->chg_irq_attr = chg_irq_attr;
	pdata->chg_irq = gpio_to_irq(chg_irq);
	pdata->chg_gpio_en = chg_gpio_en;
	pdata->check_cable_result_callback =
		sec_bat_check_cable_result_callback;
#endif
	return 0;
}

int sec_fg_dt_init(struct device_node *np,
			 struct device *dev,
			 sec_battery_platform_data_t *pdata)
{
	int ret;
	unsigned int fg_irq_attr;

	ret = of_property_read_u32(np, "capacity-max", &pdata->capacity_max);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "capacity-max-margin",
			&pdata->capacity_max_margin);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "capacity-min", &pdata->capacity_min);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "fg-irq-attr", &fg_irq_attr);
	if (ret)
		return ret;
	ret = of_property_read_u32(np, "fuel-alert-soc",
			&pdata->fuel_alert_soc);
	if (ret)
		return ret;
	if (of_find_property(np, "repeated-fuelalert", NULL))
		pdata->repeated_fuelalert = true;

	pdata->fg_irq_attr = fg_irq_attr;
	pdata->fuelalert_process = sec_fg_fuelalert_process;
	pdata->fg_gpio_init = sec_fg_gpio_init;
	pdata->capacity_calculation_type =
			SEC_FUELGAUGE_CAPACITY_TYPE_RAW;
#if !defined(CONFIG_EXTCON)
	pdata->check_jig_status = sec_bat_check_jig_status;
#endif

	return 0;
}
