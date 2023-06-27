/* arch/arm/mach-mmp/board-wilcox-battery.c
 *
 * Copyright (C) 2012 Samsung Electronics Co, Ltd.
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

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/switch.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/machine.h>
#include <linux/platform_device.h>
#include <mach/mfp-pxa986-golden.h>

#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/gpio_event.h>
#if defined(CONFIG_RT8973)
#include <linux/platform_data/rtmusc.h>
#endif
#if defined(CONFIG_FUELGAUGE_RT5033)
#include <linux/battery/fuelgauge/rt5033_fuelgauge.h>
#include <linux/battery/fuelgauge/rt5033_fuelgauge_impl.h>
#include <linux/mfd/rt5033.h>
#endif
#if defined(CONFIG_SM5502_MUIC)
#include <mach/sm5502-muic.h>
#endif

#include <linux/mfd/88pm822.h>
#if defined(CONFIG_FUELGAUGE_88PM822)
#include <linux/battery/fuelgauge/88pm80x_fg.h>
#endif

#include "board-golden.h"
#include "common.h"

#if defined(CONFIG_BATTERY_SAMSUNG)

#define SEC_CHARGER_I2C_ID	6
#define SEC_FUELGAUGE_I2C_ID	6

#define SEC_BATTERY_PMIC_NAME ""

#define TA_ADC_LOW              800
#define TA_ADC_HIGH             2200

/* cable state */
bool is_cable_attached;
unsigned int lpcharge;

static sec_bat_adc_table_data_t golden_temp_table[] = {
	{601, 600},
	{621, 590},
	{641, 580},
	{661, 570},
	{683, 560},
	{705, 550},
	{728, 540},
	{753, 530},
	{777, 520},
	{803, 510},
	{830, 500},
	{858, 490},
	{887, 480},
	{917, 470},
	{949, 460},
	{981, 450},
	{1015, 440},
	{1050, 430},
	{1087, 420},
	{1125, 410},
	{1164, 400},
	{1205, 390},
	{1248, 380},
	{1292, 370},
	{1338, 360},
	{1386, 350},
	{1436, 340},
	{1489, 330},
	{1543, 320},
	{1600, 310},
	{1659, 300},
	{1721, 290},
	{1785, 280},
	{1852, 270},
	{1922, 260},
	{1995, 250},
	{2071, 240},
	{2151, 230},
	{2234, 220},
	{2320, 210},
	{2410, 200},
	{2505, 190},
	{2604, 180},
	{2707, 170},
	{2815, 160},
	{2928, 150},
	{3046, 140},
	{3170, 130},
	{3299, 120},
	{3435, 110},
	{3577, 100},
	{3725, 90},
	{3881, 80},
	{4044, 70},
	{4215, 60},
	{4394, 50},
	{4582, 40},
	{4779, 30},
	{4986, 20},
	{5203, 10},
	{5431, 0},
	{5671, -10},
	{5924, -20},
	{6190, -30},
	{6469, -40},
	{6763, -50},
	{7072, -60},
	{7397, -70},
	{7740, -80},
	{8101, -90},
	{8481, -100},
	{8880, -110},
	{9301, -120},
	{9744, -130},
	{10212, -140},
	{10705, -150},
	{11225, -160},
	{11775, -170},
	{12355, -180},
	{12968, -190},
	{13616, -200},
};

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

static sec_charging_current_t charging_current_table[] = {
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_UNKNOWN */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_BATTERY */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_UPS */
	{1000,  650,   130,    2400},     /* POWER_SUPPLY_TYPE_MAINS */
	{550,   400,    130,    2400},     /* POWER_SUPPLY_TYPE_USB */
	{550,   400,    130,    2400},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{550,   400,    130,    2400},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{550,   400,    130,    2400},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{550,   400,    130,    2400},     /* POWER_SUPPLY_TYPE_MISC */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_CARDOCK */
	{550,   400,    130,    2400},     /* POWER_SUPPLY_TYPE_WPC */
	{750,   650,	130,    2400},     /* POWER_SUPPLY_TYPE_UARTOFF */
};

/* unit: seconds */
static int polling_time_table[] = {
	10,     /* BASIC */
	30,     /* CHARGING */
	30,     /* DISCHARGING */
	30,     /* NOT_CHARGING */
	1800,    /* SLEEP */
};

static struct power_supply *charger_supply;
static bool is_jig_on;

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

	ret = gpio_request(mfp_to_gpio(GPIO098_GPIO_98), "bq24157_CD");
	if(ret)
		return false;

	ret = gpio_request(mfp_to_gpio(GPIO008_GPIO_8), "charger-irq");
	if(ret)
		return false;

	gpio_direction_input(mfp_to_gpio(GPIO008_GPIO_8));

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

void check_jig_status(int status)
{
	if (status) {
		pr_info("%s: JIG On so reset fuel gauge capacity\n", __func__);
		is_jig_on = true;
	}

}

static bool sec_bat_check_jig_status(void)
{
	return is_jig_on;
}

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
u8 attached_cable;

void sec_charger_cb(u8 cable_type)
{

	pr_info("%s: cable type (0x%02x)\n", __func__, cable_type);

	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	attached_cable = cable_type;
	is_jig_on = false;

	switch (cable_type) {
	case CABLE_TYPE_NONE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_TYPE1_USB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE1_TA_MUIC:
	case CABLE_TYPE3_U200CHG_MUIC:
	case CABLE_TYPE3_NONSTD_SDP_MUIC:
		pr_info("%s: VBUS Online\n", __func__);
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE2_JIG_UART_OFF_VB_MUIC:
	case CABLE_TYPE2_JIG_UART_ON_VB_MUIC:
		is_jig_on = true;
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE1_OTG_MUIC:
		goto skip;
	case CABLE_TYPE2_JIG_UART_OFF_MUIC:
		is_jig_on = true;
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_TYPE2_JIG_USB_ON_MUIC:
	case CABLE_TYPE2_JIG_USB_OFF_MUIC:
                is_jig_on = true;
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE1_CARKIT_T1OR2_MUIC:
	case CABLE_TYPE3_DESKDOCK_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
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

skip:
	return;
}

static int sec_bat_check_cable_callback(void)
{
	int ta_nconnected;
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	ta_nconnected = gpio_get_value(mfp_to_gpio(GPIO008_GPIO_8));

	pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);

	if (ta_nconnected)
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
	else
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
#if 0
	if(attached_cable == MUIC_RT8973_CABLE_TYPE_0x1A) {
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
		if (value.intval == POWER_SUPPLY_TYPE_WPC) {
			value.intval =
				POWER_SUPPLY_TYPE_WPC;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
/*
		 else {
			value.intval =
				POWER_SUPPLY_TYPE_BATTERY;
				psy_do_property("sec-charger", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
*/
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

#if defined(CONFIG_FUELGAUGE_RT5033)
static gain_table_prop rt5033_battery_param1[] =
{
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

static gain_table_prop rt5033_battery_param2[] =
{
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

static gain_table_prop rt5033_battery_param3[] =
{
	{3300, 15, 65, 45, 225},
	{3400, 15, 65, 45, 225},
	{3500, 15, 65, 45, 225},
	{3600, 15, 65, 45, 225},
	{3700, 15, 65, 45, 225},
	{3800, 15, 65, 45, 225},
	{3900, 15, 65, 45, 225},
	{4000, 15, 65, 45, 225},
	{4100, 15, 65, 45, 225},
	{4200, 15, 65, 45, 225},
	{4300, 15, 65, 45, 225},
};

static gain_table_prop rt5033_battery_param4[] =
{
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

// DSG -20oC
static offset_table_prop rt5033_battery_offset1[] =
{
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

// CHG 25oC
static offset_table_prop rt5033_battery_offset2[] =
{
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

// DHG 25oC
static offset_table_prop rt5033_battery_offset3[] =
{
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

// Special Case
static offset_table_prop rt5033_battery_offset4[] =
{
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

static struct battery_data_t rt5033_comp_data[] =
{
	{
		.param1 = rt5033_battery_param1,
		.param1_size = sizeof(rt5033_battery_param1)/sizeof(gain_table_prop),
		.param2 = rt5033_battery_param2,
		.param2_size = sizeof(rt5033_battery_param2)/sizeof(gain_table_prop),
		.param3 = rt5033_battery_param3,
		.param3_size = sizeof(rt5033_battery_param3)/sizeof(gain_table_prop),
		.param4 = rt5033_battery_param4,
		.param4_size = sizeof(rt5033_battery_param4)/sizeof(gain_table_prop),
		.offset1 = rt5033_battery_offset1,
		.offset1_size = sizeof(rt5033_battery_offset1)/sizeof(offset_table_prop),
		.offset2 = rt5033_battery_offset2,
		.offset2_size = sizeof(rt5033_battery_offset2)/sizeof(offset_table_prop),
		.offset3 = rt5033_battery_offset3,
		.offset3_size = sizeof(rt5033_battery_offset3)/sizeof(offset_table_prop),
		.offset4 = rt5033_battery_offset4,
		.offset4_size = sizeof(rt5033_battery_offset4)/sizeof(offset_table_prop),
	},
};
#endif

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
static int sec_bat_adc_none_read(unsigned int channel)
{
	int adc_value = 0;
#if 0
	pm822_read_gpadc(&adc_value, PM822_GPADC2_MEAS1);
	pr_info("%s: Battery temp adc : %d\n", __func__, adc_value);
#endif
	return adc_value;
}

static bool sec_bat_adc_ap_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ap_exit(void) { return true; }
static int sec_bat_adc_ap_read(unsigned int channel) { return 0; }

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
//	.get_cable_from_extended_cable_type =
//	        sec_bat_get_cable_from_extended_cable_type,
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
//		.read = sec_bat_adc_ap_read
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
#if defined(CONFIG_FUELGAUGE_RT5033)
	.battery_data = (void *)rt5033_comp_data,
#endif
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.bat_polarity_ta_nconnected = 1,        /* active HIGH */
	.bat_irq_attr = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.cable_check_type = SEC_BATTERY_CABLE_CHECK_INT,
	.cable_source_type = //SEC_BATTERY_CABLE_SOURCE_CALLBACK,
	SEC_BATTERY_CABLE_SOURCE_EXTERNAL,
//	SEC_BATTERY_CABLE_SOURCE_EXTENDED,

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

#if defined(CONFIG_FUELGAUGE_88PM822)
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_FG,
	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 1,
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 460,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 600,
	.temp_high_recovery_normal = 460,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 600,
	.temp_high_recovery_lpm = 460,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 0,
#else
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	.temp_adc_table = golden_temp_table,
	.temp_adc_table_size =
		sizeof(golden_temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = golden_temp_table,
	.temp_amb_adc_table_size =
		sizeof(golden_temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,

    .temp_check_count = 1,
    .temp_high_threshold_event = 600,
    .temp_high_recovery_event = 460,
    .temp_low_threshold_event = -50,
    .temp_low_recovery_event = 0,
    .temp_high_threshold_normal = 600,
    .temp_high_recovery_normal = 460,
    .temp_low_threshold_normal = -50,
    .temp_low_recovery_normal = 0,
    .temp_high_threshold_lpm = 600,
    .temp_high_recovery_lpm = 460,
    .temp_low_threshold_lpm = -50,
    .temp_low_recovery_lpm = 0,
#endif

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_TIME,
	.full_check_count = 1,
	/* .full_check_adc_1st = 26500, */
	/*.full_check_adc_2nd = 25800, */
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
		SEC_BATTERY_FULL_CONDITION_VCELL,
	.full_condition_soc = 95,
	.full_condition_ocv = 4300,
	.full_condition_vcell = 4300,

	.recharge_condition_type =
                SEC_BATTERY_RECHARGE_CONDITION_VCELL,

	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4280,
	.recharge_condition_vcell = 4300,
	.recharge_check_count = 1,

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

	.chg_float_voltage = 4330,
};

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev = {
		.platform_data = &sec_battery_pdata,
	},
};

#if defined(CONFIG_FUELGAUGE_RT5033)
static struct i2c_gpio_platform_data fuelgauge_gpio_i2c_pdata = {
	.sda_pin = mfp_to_gpio(GPIO012_GPIO_12),
	.scl_pin = mfp_to_gpio(GPIO011_GPIO_11),
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device fuelgauge_gpio_i2c_device = {
	.name = "i2c-gpio",
	.id = SEC_FUELGAUGE_I2C_ID,
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
#endif

#if defined(CONFIG_CHARGER_RT9455)
static struct i2c_gpio_platform_data charger_gpio_i2c_pdata = {
        .sda_pin = mfp_to_gpio(GPIO017_GPIO_17),
        .scl_pin = mfp_to_gpio(GPIO016_GPIO_16),
        .udelay = 10,
        .timeout = 0,
};

static struct platform_device charger_gpio_i2c_device = {
        .name = "i2c-gpio",
        .id = SEC_CHARGER_I2C_ID,
        .dev = {
                .platform_data = &charger_gpio_i2c_pdata,
        },
};

static struct i2c_board_info sec_brdinfo_charger[] __initdata = {
        {
                I2C_BOARD_INFO("sec-charger",
                                SEC_CHARGER_I2C_SLAVEADDR),
                .platform_data = &sec_battery_pdata,
        }
};
#endif

static struct platform_device *sec_battery_devices[] __initdata = {
        &sec_device_battery,
#if defined(CONFIG_FUELGAUGE_RT5033)
        &fuelgauge_gpio_i2c_device,
#endif
#if defined(CONFIG_CHARGER_RT9455)
	&charger_gpio_i2c_device,
#endif
};

/*
static void charger_gpio_init(void)
{
	pxa_init_i2c_gpio_irq(ARRAY_AND_SIZE(sec_charger_i2c_gpio),
			ARRAY_AND_SIZE(sec_brdinfo_charger));

	sec_battery_pdata.chg_irq = gpio_to_irq(mfp_to_gpio(GPIO008_GPIO_8));
}
*/
void __init pxa986_golden_battery_init(void)
{
	pr_info("%s: golden charger init\n", __func__);

	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));

#if defined(CONFIG_FUELGAUGE_RT5033)
	i2c_register_board_info(6, ARRAY_AND_SIZE(sec_brdinfo_fuelgauge));
#endif
#if defined(CONFIG_CHARGER_RT9455)
	i2c_register_board_info(SEC_CHARGER_I2C_ID,
		ARRAY_AND_SIZE(sec_brdinfo_charger));
#endif
}
#endif
