/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc/board-huskytd/board-teddy3g-battery.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/mfd/rt8973.h>
#include <linux/i2c-gpio.h>
#include <mach/board.h>

static struct platform_device sec_device_battery;
static struct platform_device fuelgauge_gpio_i2c_device;
static struct platform_device rt5033_mfd_device;
static struct platform_device rt8973_mfd_device_i2cadaptor;

#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/battery/fuelgauge/rt5033_fuelgauge.h>
#include <linux/battery/fuelgauge/rt5033_fuelgauge_impl.h>
#include <linux/mfd/rt5033.h>
#include <linux/leds/rt5033_fled.h>

/* ---- battery ---- */
#define SEC_BATTERY_PMIC_NAME "rt5xx"
extern struct battery_data_t rt5033_comp_data[];
unsigned int lpcharge;
EXPORT_SYMBOL(lpcharge);

static sec_bat_adc_region_t cable_adc_value_table[] = {
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_UNKNOWN */
	{ 0,    500 },  /* POWER_SUPPLY_TYPE_BATTERY */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_UPS */
	{ 1000, 1500 }, /* POWER_SUPPLY_TYPE_MAINS */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_USB */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_OTG */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_DOCK */
	{ 0,    0 },    /* POWER_SUPPLY_TYPE_MISC */
};

static gain_table_prop rt5033_battery_param1[] = {
	{3300, 105, 5, 5, 235},
	{3400, 105, 5, 5, 235},
	{3500, 105, 5, 5, 235},
	{3600, 105, 5, 5, 235},
	{3700, 105, 5, 5, 235},
	{3800, 105, 5, 5, 235},
	{3900, 105, 5, 5, 235},
	{4000, 105, 5, 5, 235},
	{4100, 105, 5, 5, 235},
	{4200, 105, 5, 5, 235},
	{4300, 105, 5, 5, 235},
};

static gain_table_prop rt5033_battery_param2[] = {
	{3300, 35, 23, 5, 235},
	{3400, 35, 23, 5, 235},
	{3500, 35, 23, 5, 235},
	{3600, 35, 23, 5, 235},
	{3700, 35, 23, 5, 235},
	{3800, 35, 23, 5, 235},
	{3900, 35, 23, 5, 235},
	{4000, 35, 23, 5, 235},
	{4100, 35, 23, 5, 235},
	{4200, 35, 23, 5, 235},
	{4300, 35, 23, 5, 235},
};

static gain_table_prop rt5033_battery_param3[] = {
	{3300, 25, 33, 45, 215},
	{3400, 25, 33, 45, 215},
	{3500, 25, 33, 45, 215},
	{3600, 25, 33, 45, 215},
	{3700, 25, 33, 45, 215},
	{3800, 25, 33, 45, 215},
	{3900, 25, 33, 45, 215},
	{4000, 25, 33, 45, 215},
	{4100, 25, 33, 45, 215},
	{4200, 25, 33, 45, 215},
	{4300, 25, 33, 45, 215},
};

static gain_table_prop rt5033_battery_param4[] = {
	{3300, 85, 60, 95, 245},
	{3400, 85, 60, 95, 245},
	{3500, 85, 60, 95, 245},
	{3600, 85, 60, 95, 245},
	{3700, 85, 60, 95, 245},
	{3800, 85, 60, 95, 245},
	{3900, 85, 60, 95, 245},
	{4000, 85, 60, 95, 245},
	{4100, 85, 60, 95, 245},
	{4200, 85, 60, 95, 245},
	{4300, 85, 60, 95, 245},
};

/* DSG -20oC */
static offset_table_prop rt5033_battery_offset1[] = {
	{3310, -5},
	{3320, -5},
	{3330, -5},
	{3340, -5},
	{3350, -5},
	{3360, -5},
	{3370, -5},
	{3380, 5},
	{3390, 5},
	{3400, 5},
	{3410, 10},
	{3420, 10},
	{3430, 10},
	{3440, 20},
	{3450, 20},
	{3460, 25},
	{3470, 25},
	{3480, 30},
	{3490, 30},
	{3500, 30},
	{3510, 35},
	{3520, 35},
	{3530, 35},
	{3540, 35},
	{3550, 35},
	{3560, 35},
	{3570, 35},
	{3580, 35},
	{3590, 35},
	{3600, 35},
	{3610, 30},
	{3620, 25},
	{3630, 20},
	{3640, 20},
	{3650, 20},
	{3660, 20},
	{3670, 5},
	{3680, 5},
	{3690, 5},
	{3700, 5},
	{3710, 0},
	{3720, 0},
	{3730, 0},
	{3740, 0},
	{3750, 0},
	{3760, 0},
	{3770, 0},
	{3780, 0},
	{3790, 0},
	{3800, 0},
	{3810, 0},
	{3820, 0},
	{3830, 0},
	{3840, 0},
	{3850, 0},
	{3860, 0},
	{3870, 0},
	{3880, 0},
	{3890, 0},
	{3900, 0},
	{3910, 0},
	{3920, 0},
	{3930, 0},
	{3940, 0},
	{3950, 0},
	{3960, 0},
	{3970, 0},
	{3980, 0},
	{3990, 0},
	{4000, 0},
	{4010, 0},
	{4020, 0},
	{4030, 0},
	{4040, 0},
	{4050, 0},
	{4060, 0},
	{4070, 0},
	{4080, 0},
	{4090, 0},
	{4100, 0},
	{4110, 0},
	{4120, 0},
	{4130, 0},
	{4140, 0},
	{4150, 0},
	{4160, 0},
	{4170, 0},
	{4180, 0},
	{4190, 0},
	{4200, 0},
	{4210, 0},
	{4220, 0},
	{4230, 0},
	{4240, 0},
	{4250, 0},
	{4260, 0},
	{4270, 0},
	{4280, 0},
	{4290, 0},
	{4300, 0},
};

/* CHG 25oC */
static offset_table_prop rt5033_battery_offset2[] = {
	{3310, -4},
	{3320, -4},
	{3330, -4},
	{3340, -4},
	{3350, -4},
	{3360, -4},
	{3370, -4},
	{3380, -4},
	{3390, -4},
	{3400, -4},
	{3410, -4},
	{3420, -4},
	{3430, -4},
	{3440, -4},
	{3450, -4},
	{3460, -4},
	{3470, -4},
	{3480, -4},
	{3490, -4},
	{3500, -4},
	{3510, -4},
	{3520, -4},
	{3530, -4},
	{3540, -4},
	{3550, -4},
	{3560, -4},
	{3570, -4},
	{3580, -4},
	{3590, -4},
	{3600, -4},
	{3610, -4},
	{3620, -4},
	{3630, -4},
	{3640, -4},
	{3650, -4},
	{3660, -4},
	{3670, -4},
	{3680, -4},
	{3690, -4},
	{3700, -4},
	{3710, -4},
	{3720, -4},
	{3730, -4},
	{3740, -4},
	{3750, -4},
	{3760, 5},
	{3770, 5},
	{3780, 5},
	{3790, -5},
	{3800, -5},
	{3810, -5},
	{3820, -5},
	{3830, -5},
	{3840, -15},
	{3850, -15},
	{3860, -15},
	{3870, -10},
	{3880, -5},
	{3890, -20},
	{3900, -20},
	{3910, -15},
	{3920, -15},
	{3930, -15},
	{3940, -20},
	{3950, -20},
	{3960, -20},
	{3970, -20},
	{3980, -20},
	{3990, -20},
	{4000, -20},
	{4010, -20},
	{4020, -15},
	{4030, -15},
	{4040, -15},
	{4050, -15},
	{4060, -10},
	{4070, -10},
	{4080, -10},
	{4090, -10},
	{4100, -10},
	{4110, 0},
	{4120, 0},
	{4130, 0},
	{4140, 0},
	{4150, 0},
	{4160, 10},
	{4170, 15},
	{4180, 15},
	{4190, 15},
	{4200, 15},
	{4210, 15},
	{4220, 15},
	{4230, 15},
	{4240, 15},
	{4250, 15},
	{4260, 15},
	{4270, 15},
	{4280, 15},
	{4290, 15},
	{4300, 15},
};

/* DHG 25oC */
static offset_table_prop rt5033_battery_offset3[] = {
	{3310, -10},
	{3320, -10},
	{3330, -10},
	{3340, -10},
	{3350, -10},
	{3360, -10},
	{3370, -10},
	{3380, -10},
	{3390, -10},
	{3400, -10},
	{3410, -10},
	{3420, -10},
	{3430, -10},
	{3440, -10},
	{3450, -10},
	{3460, -10},
	{3470, -10},
	{3480, -10},
	{3490, -10},
	{3500, -10},
	{3510, -10},
	{3520, -10},
	{3530, -10},
	{3540, -10},
	{3550, -10},
	{3560, -10},
	{3570, -10},
	{3580, -10},
	{3590, -10},
	{3600, -10},
	{3610, -20},
	{3620, -20},
	{3630, -20},
	{3640, -20},
	{3650, -20},
	{3660, -20},
	{3670, -20},
	{3680, -20},
	{3690, -20},
	{3700, -20},
	{3710, -5},
	{3720, -5},
	{3730, -5},
	{3740, -5},
	{3750, -5},
	{3760, -5},
	{3770, -5},
	{3780, -5},
	{3790, -5},
	{3800, 5},
	{3810, 5},
	{3820, 5},
	{3830, 5},
	{3840, 5},
	{3850, 5},
	{3860, 5},
	{3870, 5},
	{3880, 5},
	{3890, 5},
	{3900, 5},
	{3910, 10},
	{3920, 10},
	{3930, 10},
	{3940, 10},
	{3950, 10},
	{3960, 10},
	{3970, 10},
	{3980, 5},
	{3990, 5},
	{4000, 5},
	{4010, -5},
	{4020, -5},
	{4030, -5},
	{4040, -5},
	{4050, -5},
	{4060, -5},
	{4070, -5},
	{4080, -10},
	{4090, -10},
	{4100, -10},
	{4110, -10},
	{4120, -10},
	{4130, -10},
	{4140, -10},
	{4150, -10},
	{4160, -10},
	{4170, -10},
	{4180, -10},
	{4190, -10},
	{4200, -10},
	{4210, -10},
	{4220, -10},
	{4230, -10},
	{4240, -10},
	{4250, -10},
	{4260, -10},
	{4270, -10},
	{4280, -10},
	{4290, -10},
	{4300, -10},
};

/* Special Case */
static offset_table_prop rt5033_battery_offset4[] = {
	{3310, 0},
	{3320, 0},
	{3330, 0},
	{3340, 0},
	{3350, 0},
	{3360, 0},
	{3370, 0},
	{3380, 0},
	{3390, 0},
	{3400, 0},
	{3410, 0},
	{3420, 0},
	{3430, 0},
	{3440, 0},
	{3450, 0},
	{3460, 0},
	{3470, 0},
	{3480, 0},
	{3490, 0},
	{3500, 0},
	{3510, 0},
	{3520, 0},
	{3530, 0},
	{3540, 0},
	{3550, 0},
	{3560, 0},
	{3570, 0},
	{3580, 0},
	{3590, 0},
	{3600, 0},
	{3610, 0},
	{3620, 0},
	{3630, 0},
	{3640, 0},
	{3650, 0},
	{3660, 0},
	{3670, 0},
	{3680, 0},
	{3690, 0},
	{3700, 0},
	{3710, 0},
	{3720, 0},
	{3730, 0},
	{3740, 0},
	{3750, 0},
	{3760, 0},
	{3770, 0},
	{3780, 0},
	{3790, 0},
	{3800, 0},
	{3810, 0},
	{3820, 0},
	{3830, 0},
	{3840, 0},
	{3850, 0},
	{3860, 0},
	{3870, 0},
	{3880, 0},
	{3890, 0},
	{3900, 0},
	{3910, 0},
	{3920, 0},
	{3930, 0},
	{3940, 0},
	{3950, 0},
	{3960, 0},
	{3970, 0},
	{3980, 0},
	{3990, 0},
	{4000, 0},
	{4010, 0},
	{4020, 0},
	{4030, 0},
	{4040, 0},
	{4050, 0},
	{4060, 0},
	{4070, 0},
	{4080, 0},
	{4090, 0},
	{4100, 0},
	{4110, 0},
	{4120, 0},
	{4130, 0},
	{4140, 0},
	{4150, 0},
	{4160, 0},
	{4170, 0},
	{4180, 0},
	{4190, 0},
	{4200, 0},
	{4210, 0},
	{4220, 0},
	{4230, 0},
	{4240, 0},
	{4250, 0},
	{4260, 0},
	{4270, 0},
	{4280, 0},
	{4290, 0},
	{4300, 0},
};

struct battery_data_t rt5033_comp_data[] = {
	{
		.param1 = rt5033_battery_param1,
		.param1_size = sizeof(rt5033_battery_param1)/
				sizeof(gain_table_prop),
		.param2 = rt5033_battery_param2,
		.param2_size = sizeof(rt5033_battery_param2)/
				sizeof(gain_table_prop),
		.param3 = rt5033_battery_param3,
		.param3_size = sizeof(rt5033_battery_param3)/
				sizeof(gain_table_prop),
		.param4 = rt5033_battery_param4,
		.param4_size = sizeof(rt5033_battery_param4)/
				sizeof(gain_table_prop),
		.offset1 = rt5033_battery_offset1,
		.offset1_size = sizeof(rt5033_battery_offset1)/
				sizeof(offset_table_prop),
		.offset2 = rt5033_battery_offset2,
		.offset2_size = sizeof(rt5033_battery_offset2)/
				sizeof(offset_table_prop),
		.offset3 = rt5033_battery_offset3,
		.offset3_size = sizeof(rt5033_battery_offset3)/
				sizeof(offset_table_prop),
		.offset4 = rt5033_battery_offset4,
		.offset4_size = sizeof(rt5033_battery_offset4)/
				sizeof(offset_table_prop),
	},
};

/* assume that system's average current consumption is 150mA */
/* We will suggest use 250mA as EOC setting */
/* AICR of RT5033 is 100, 500, 700, 900, 1000, 1500, 2000,
 * we suggest to use 500mA for USB */
static sec_charging_current_t charging_current_table[] = {
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_UNKNOWN */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_BATTERY */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_UPS */
	{1000,  1000,   250,    1800},     /* POWER_SUPPLY_TYPE_MAINS */
	{500,   700,    250,    1800},     /* POWER_SUPPLY_TYPE_USB */
	{500,   700,    250,    1800},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{500,   700,    250,    1800},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{500,   700,    250,    1800},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{500,   700,    250,   1800},     /* POWER_SUPPLY_TYPE_MISC */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_CARDOCK */
	{550,   500,    250,    1800},     /* POWER_SUPPLY_TYPE_WPC */
	{750,  750,   250,    1800},     /* POWER_SUPPLY_TYPE_UARTOFF */
};

/* unit: seconds */
static int polling_time_table[] = {
	2,     /* BASIC */
	2,     /* CHARGING */
	10,     /* DISCHARGING */
	10,     /* NOT_CHARGING */
	300,    /* SLEEP */
};

static bool sec_bat_gpio_init(void)
{
	return true;
}

static bool sec_fg_gpio_init(void)
{
	return true;
}

static bool sec_chg_gpio_init(void)
{
	int ret;

	ret = gpio_request(GPIO_IF_PMIC_IRQ, "charger-irq");
	if (ret)
		return false;

	gpio_direction_input(GPIO_IF_PMIC_IRQ);

	return true;
}

/* Get LP charging mode state */

static int battery_get_lpm_state(char *str)
{
	get_option(&str, &lpcharge);
	pr_info("%s: Low power charging mode: %d\n", __func__, lpcharge);

	return lpcharge;
}
__setup("lpcharge=", battery_get_lpm_state);

static bool sec_bat_is_lpm(void)
{
	return lpcharge == 1 ? true : false;
}

static bool sec_bat_check_jig_status(void) { return 0; }

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
EXPORT_SYMBOL(current_cable_type);

static int sec_bat_check_cable_callback(void)
{
	int ta_nconnected;
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	ta_nconnected = gpio_get_value(GPIO_IF_PMIC_IRQ);

	pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);

	if (ta_nconnected)
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
	else
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;

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

	sec_bat_check_cable_callback();

/*	current_cable_type = POWER_SUPPLY_TYPE_MAINS; */

	
		psy_do_property("sec-charger", get,
				POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_WPC) {
			value.intval =
				POWER_SUPPLY_TYPE_WPC;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		} else {
			value.intval =
				POWER_SUPPLY_TYPE_BATTERY;
				psy_do_property("sec-charger", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
	
}

static bool sec_bat_check_cable_result_callback(int cable_type)
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
	/* omap4_tab3_tsp_ta_detect(cable_type); */

	return ret;
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
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

static bool sec_bat_ovp_uvlo_result_callback(int health) { return true; }

/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val) { return true; }

static bool sec_fg_fuelalert_process(bool is_fuel_alerted) { return true; }

static bool sec_bat_adc_none_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_none_exit(void) { return true; }
static int sec_bat_adc_none_read(unsigned int channel) { return 0; }

static bool sec_bat_adc_ap_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ap_exit(void) { return true; }
/* static int sec_bat_adc_ap_read(unsigned int channel)
{ channel = channel; return 0; } */

static bool sec_bat_adc_ic_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ic_exit(void) { return true; }
static int sec_bat_adc_ic_read(unsigned int channel) { return 0; }

sec_battery_platform_data_t sec_battery_pdata = {
	/* NO NEED TO BE CHANGED */
	.initial_check = sec_bat_initial_check,
	.bat_gpio_init = sec_bat_gpio_init,
	.fg_gpio_init = sec_fg_gpio_init,
	.chg_gpio_init = sec_chg_gpio_init,

	.is_lpm = sec_bat_is_lpm,
	.check_jig_status = sec_bat_check_jig_status,
	.check_cable_callback =
		sec_bat_check_cable_callback,
/*	.get_cable_from_extended_cable_type =
		sec_bat_get_cable_from_extended_cable_type,*/
	.check_cable_result_callback =
		sec_bat_check_cable_result_callback,
	.check_battery_callback =
		sec_bat_check_callback,
	.check_battery_result_callback =
		sec_bat_check_result_callback,
	.ovp_uvlo_callback = sec_bat_ovp_uvlo_callback,
	.ovp_uvlo_result_callback =
		sec_bat_ovp_uvlo_result_callback,
	.fuelalert_process = sec_fg_fuelalert_process,
	.get_temperature_callback =
		sec_bat_get_temperature_callback,

	.adc_api[SEC_BATTERY_ADC_TYPE_NONE] = {
		.init = sec_bat_adc_none_init,
		.exit = sec_bat_adc_none_exit,
		.read = sec_bat_adc_none_read
	},
	.adc_api[SEC_BATTERY_ADC_TYPE_AP] = {
		.init = sec_bat_adc_ap_init,
		.exit = sec_bat_adc_ap_exit,
/*		.read = sec_bat_adc_ap_read*/
	},
	.adc_api[SEC_BATTERY_ADC_TYPE_IC] = {
		.init = sec_bat_adc_ic_init,
		.exit = sec_bat_adc_ic_exit,
		.read = sec_bat_adc_ic_read
	},
	.cable_adc_value = cable_adc_value_table,
	.charging_current = charging_current_table,
	.polling_time = polling_time_table,
	/* NO NEED TO BE CHANGED */

	.pmic_name = SEC_BATTERY_PMIC_NAME,

	.adc_check_count = 5,

	.adc_type = {
		SEC_BATTERY_ADC_TYPE_NONE,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_NONE,      /* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_NONE,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_NONE,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_NONE,      /* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.battery_data = (void *)rt5033_comp_data,
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.bat_polarity_ta_nconnected = 1,        /* active HIGH */
	.bat_irq_attr = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.cable_check_type = SEC_BATTERY_CABLE_CHECK_INT,
	.cable_source_type = SEC_BATTERY_CABLE_SOURCE_EXTERNAL,

	.event_check = false,
	.event_waiting_time = 60,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_FUELGAUGE,
	.check_count = 1,

	/* Battery check by ADC */
	.check_adc_max = 600,
	.check_adc_min = 50,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_FG,
	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,

	.temp_check_count = 1,
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 400,
	.temp_low_threshold_event = -80,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 600,
	.temp_high_recovery_normal = 400,
	.temp_low_threshold_normal = -80,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 600,
	.temp_high_recovery_lpm = 400,
	.temp_low_threshold_lpm = -80,
	.temp_low_recovery_lpm = 0,

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_TIME,
	.full_check_count = 1,
	/* .full_check_adc_1st = 26500, */
	/*.full_check_adc_2nd = 25800, */
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_SOC,
	.full_condition_soc = 100,
	.full_condition_ocv = 4300,

	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL,

	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4280,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time = 90 * 60,
	.charging_reset_time = 10 * 60,

	/* Fuel Gauge */
	.fg_irq_attr = IRQF_TRIGGER_FALLING,
	.fuel_alert_soc = 1,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE,
	.capacity_max = 1000,
	.capacity_min = 0,
	.capacity_max_margin = 30,

	/* Charger */
	.chg_polarity_en = 0,   /* active LOW charge enable */
	.chg_polarity_status = 0,
	.chg_irq_attr = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.chg_float_voltage = 4350,
};

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev = {
		.platform_data = &sec_battery_pdata,
	},
};

static struct i2c_gpio_platform_data fuelgauge_gpio_i2c_pdata = {
	.sda_pin = GPIO_FUELGAUGE_SDA,
	.scl_pin = GPIO_FUELGAUGE_SCL,
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device fuelgauge_gpio_i2c_device = {
	.name = "i2c-gpio",
	.id = 6,
	.dev = {
		.platform_data = &fuelgauge_gpio_i2c_pdata,
	},
};

static struct i2c_board_info sec_brdinfo_fuelgauge[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
				RT5033_SLAVE_ADDRESS),
		.platform_data = &sec_battery_pdata,
	}
};


/* ---- mfd ---- */
static struct regulator_consumer_supply rt5033_safe_ldo_consumers[] = {
	REGULATOR_SUPPLY("rt5033_safe_ldo", NULL),
};
static struct regulator_consumer_supply rt5033_ldo_consumers[] = {
	REGULATOR_SUPPLY("rt5033_ldo", NULL),
};
static struct regulator_consumer_supply rt5033_buck_consumers[] = {
	REGULATOR_SUPPLY("rt5033_buck", NULL),
};

static struct regulator_init_data rt5033_safe_ldo_data = {
	.constraints = {
		.min_uV = 3300000,
		.max_uV = 4950000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.boot_on = 1,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_safe_ldo_consumers),
	.consumer_supplies = rt5033_safe_ldo_consumers,
};
static struct regulator_init_data rt5033_ldo_data = {
	.constraints = {
		.min_uV = 1200000,
		.max_uV = 3000000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_ldo_consumers),
	.consumer_supplies = rt5033_ldo_consumers,
};
static struct regulator_init_data rt5033_buck_data = {
	.constraints = {
		.min_uV = 1200000,
		.max_uV = 1200000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(rt5033_buck_consumers),
	.consumer_supplies = rt5033_buck_consumers,
};

const static struct rt5033_regulator_platform_data rv_pdata = {
	.regulator = {
		[RT5033_ID_LDO_SAFE] = &rt5033_safe_ldo_data,
		[RT5033_ID_LDO1] = &rt5033_ldo_data,
		[RT5033_ID_DCDC1] = &rt5033_buck_data,
	},
};

#define RT5033FG_SLAVE_ADDR_MSB (0x00)

#define RT5033_SLAVE_ADDR   (0x34|RT5033FG_SLAVE_ADDR_MSB)

/* add regulator data */

/* add fled platform data */
static rt5033_fled_platform_data_t fled_pdata = {
	.fled1_en = 1,
	.fled2_en = 1,
	.fled_mid_track_alive = 0,
	.fled_mid_auto_track_en = 1,
	.fled_timeout_current_level = RT5033_TIMEOUT_LEVEL(50),
	.fled_strobe_current = RT5033_STROBE_CURRENT(750),
	.fled_strobe_timeout = RT5033_STROBE_TIMEOUT(544),
	.fled_torch_current = RT5033_TORCH_CURRENT(38),
	.fled_lv_protection = RT5033_LV_PROTECTION(3200),
	.fled_mid_level = RT5033_MID_REGULATION_LEVEL(4800),
};

/* Define mfd driver platform data*/
struct rt5033_mfd_platform_data sc88xx_rt5033_info = {
	.irq_gpio = GPIO_IF_PMIC_IRQ,
	.irq_base = __NR_IRQS,
	.charger_data = &sec_battery_pdata,
	.fled_platform_data = &fled_pdata,
	.regulator_platform_data = &rv_pdata,
};

static struct i2c_board_info __initdata rt5033_i2c_devices[] = {
	{
		I2C_BOARD_INFO("rt5033-mfd", RT5033_SLAVE_ADDR),
		.platform_data	= &sc88xx_rt5033_info,
	}
};

int epmic_event_handler(int level);
u8 attached_cable;

void sec_charger_cb(u8 cable_type)
{
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	pr_info("%s: cable type (0x%02x)\n", __func__, cable_type);
	attached_cable = cable_type;

	switch (cable_type) {
	case MUIC_RT8973_CABLE_TYPE_NONE:
	case MUIC_RT8973_CABLE_TYPE_UNKNOWN:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case MUIC_RT8973_CABLE_TYPE_USB:
	case MUIC_RT8973_CABLE_TYPE_CDP:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case MUIC_RT8973_CABLE_TYPE_REGULAR_TA:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case MUIC_RT8973_CABLE_TYPE_OTG:
		goto skip;
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF:
	/*
		if (!gpio_get_value(mfp_to_gpio(GPIO008_GPIO_8))) {
			pr_info("%s cable type POWER_SUPPLY_TYPE_UARTOFF\n",
				__func__);
			current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		}
		else {
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		}*/
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

		break;
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_ON:
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_OFF:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case MUIC_RT8973_CABLE_TYPE_0x1A:
	case MUIC_RT8973_CABLE_TYPE_TYPE1_CHARGER:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case MUIC_RT8973_CABLE_TYPE_0x15:
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

	if (current_cable_type == POWER_SUPPLY_TYPE_USB)
		epmic_event_handler(1);

skip:
	return;
}
EXPORT_SYMBOL(sec_charger_cb);

void teddy3g_battery_init(void)
{
	i2c_register_board_info(8, rt5033_i2c_devices,
				ARRAY_SIZE(rt5033_i2c_devices));
	i2c_register_board_info(6, sec_brdinfo_fuelgauge,
				ARRAY_SIZE(sec_brdinfo_fuelgauge));
	platform_device_register(&fuelgauge_gpio_i2c_device);
	platform_device_register(&sec_device_battery);
}
