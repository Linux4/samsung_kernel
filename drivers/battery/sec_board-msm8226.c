/*
 *  sec_board-msm8974.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charging_common.h>

extern int current_cable_type;
#if defined(CONFIG_FUELGAUGE_MAX17048)
static struct battery_data_t samsung_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{

		.RCOMP0 = 0x73,
		.RCOMP_charging = 0x8D,
		.temp_cohot = -1000,
		.temp_cocold = -4350,
		.is_using_model_data = true,
		.type_str = "SDI",
	}
};
#endif

#if defined(CONFIG_MACH_KLTE_EUR)|| defined(CONFIG_MACH_K3GDUOS_CTC) || defined(CONFIG_MACH_KLTE_CMCC)|| defined(CONFIG_MACH_KLTE_CTC) || defined(CONFIG_MACH_KACTIVELTE_EUR)
static sec_bat_adc_table_data_t temp_table[] = {
	{27281,	700},
	{27669,	650},
	{28178,	600},
	{28724,	550},
	{29342,	500},
	{30101,	450},
	{30912,	400},
	{31807,	350},
	{32823,	300},
	{33858,	250},
	{34950,	200},
	{36049,	150},
	{37054,	100},
	{38025,	50},
	{38219,	40},
	{38448,	30},
	{38626,	20},
	{38795,	10},
	{38989,	0},
	{39229,	-10},
	{39540,	-30},
	{39687,	-40},
	{39822,	-50},
	{40523,	-100},
	{41123,	-150},
	{41619,	-200},
};
#endif

static void sec_bat_adc_ap_init(struct platform_device *pdev,
			  struct sec_battery_info *battery)
{
}

static int sec_bat_adc_ap_read(struct sec_battery_info *battery, int channel)
{
	int data = -1;

	switch (channel)
	{
	case SEC_BAT_ADC_CHANNEL_TEMP :
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		data = 33000;
		break;
	default :
		break;
	}

	return data;
}

static void sec_bat_adc_ap_exit(void)
{
}

static void sec_bat_adc_none_init(struct platform_device *pdev,
			  struct sec_battery_info *battery)
{
}

static int sec_bat_adc_none_read(struct sec_battery_info *battery, int channel)
{
	return 0;
}

static void sec_bat_adc_none_exit(void)
{
}

static void sec_bat_adc_ic_init(struct platform_device *pdev,
			  struct sec_battery_info *battery)
{
}

static int sec_bat_adc_ic_read(struct sec_battery_info *battery, int channel)
{
	return 0;
}

static void sec_bat_adc_ic_exit(void)
{
}
static int adc_read_type(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		adc = sec_bat_adc_none_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		adc = sec_bat_adc_ap_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		adc = sec_bat_adc_ic_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
	pr_debug("[%s] ADC = %d\n", __func__, adc);
	return adc;
}

static void adc_init_type(struct platform_device *pdev,
			  struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

static void adc_exit_type(struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

int adc_read(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	adc = adc_read_type(battery, channel);

	pr_debug("[%s]adc = %d\n", __func__, adc);

	return adc;
}

void adc_exit(struct sec_battery_info *battery)
{
	adc_exit_type(battery);
}

bool sec_bat_check_jig_status(void)
{
	return false;
}
/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
bool sec_bat_check_callback(struct sec_battery_info *battery)
{
	return true;
}

bool sec_bat_check_cable_result_callback(
		int cable_type)
{
	return true;
}

int sec_bat_check_cable_callback(struct sec_battery_info *battery)
{
	union power_supply_propval value;
	msleep(750);

	if (battery->pdata->ta_irq_gpio == 0) {
		pr_err("%s: ta_int_gpio is 0 or not assigned yet(cable_type(%d))\n",
			__func__, current_cable_type);
	} else {
		if (current_cable_type == POWER_SUPPLY_TYPE_BATTERY &&
			!gpio_get_value_cansleep(battery->pdata->ta_irq_gpio)) {
			pr_info("%s : VBUS IN\n", __func__);
			psy_do_property("battery", set, POWER_SUPPLY_PROP_ONLINE, value);
			return POWER_SUPPLY_TYPE_UARTOFF;
		}

		if ((current_cable_type == POWER_SUPPLY_TYPE_UARTOFF ||
			current_cable_type == POWER_SUPPLY_TYPE_CARDOCK) &&
			gpio_get_value_cansleep(battery->pdata->ta_irq_gpio)) {
			pr_info("%s : VBUS OUT\n", __func__);
			psy_do_property("battery", set, POWER_SUPPLY_PROP_ONLINE, value);
			return POWER_SUPPLY_TYPE_BATTERY;
		}
	}

	return current_cable_type;
}

void board_battery_init(struct platform_device *pdev, struct sec_battery_info *battery)
{
	adc_init_type(pdev, battery);
}

void board_fuelgauge_init(struct sec_fuelgauge_info *fuelgauge)
{
	pr_info("%s\n", __func__);
}

void cable_initial_check(struct sec_battery_info *battery)
{
	pr_info("%s\n", __func__);
}
