/* arch/arm/mach-exynos/board-tab3-battery.c
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

#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/gpio_event.h>
#include <mach/mfp-pxa986-cocoa7.h>
#include <mach/fsa9480.h>

#include "board-cocoa7.h"

#define SEC_CHARGER_I2C_ID	2
#define SEC_FUELGAUGE_I2C_ID	6

#define SEC_BATTERY_PMIC_NAME ""

#define TA_ADC_LOW              800
#define TA_ADC_HIGH             2200

/* cable state */
bool is_cable_attached;
unsigned int lpcharge;

static struct s3c_adc_client *temp_adc_client;

static sec_bat_adc_table_data_t cocoa7_temp_table[] = {
        {404, 600},
        {548, 500},
        {696, 420},
        {725, 400},
        {927, 300},
        {1144, 200},
        {1377, 100},
        {1460, 50},
        {1582, 0},
        {1647, -50},
        {1738, -100},
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
	{2000,  2000,   200,    200},     /* POWER_SUPPLY_TYPE_MAINS */
	{500,   500,    200,    200},     /* POWER_SUPPLY_TYPE_USB */
	{500,   500,    200,    200},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{500,   500,    200,    200},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{500,   500,    200,    200},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_OTG */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_DOCK */
	{500,   500,    200,    200},     /* POWER_SUPPLY_TYPE_MISC */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_WIRELESS */
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

void sec_charger_cb(u8 attached)
{

	pr_info("%s: cable attached(%d)\n", __func__, attached);

	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	int ret;

	switch (attached) {
	case CABLE_TYPE_NONE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_TYPE1_USB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE1_TA_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE1_OTG_MUIC:
		goto skip;
	case CABLE_TYPE1_CARKIT_T1OR2_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
	case CABLE_TYPE2_JIG_UART_OFF_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_TYPE2_JIG_UART_OFF_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	case CABLE_TYPE2_JIG_USB_ON_MUIC:
	case CABLE_TYPE2_JIG_USB_OFF_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE2_DESKDOCK_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
	case CABLE_TYPE3_MHL_VB_MUIC:
	case CABLE_TYPE3_MHL_MUIC:
		goto skip;
	case CABLE_TYPE3_U200CHG_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_AUDIODOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
	case CABLE_TYPE_SMARTDOCK_USB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE_SMARTDOCK_TA_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, attached);
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
	return 0;
}

/*
int extended_cable_type;

static int sec_bat_get_cable_from_extended_cable_type(
	int input_extended_cable_type)
{
	int cable_main, cable_sub, cable_power;
	int cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
	union power_supply_propval value;
	int charge_current_max = 0, charge_current = 0;

	cable_main = GET_MAIN_CABLE_TYPE(input_extended_cable_type);
	if (cable_main != POWER_SUPPLY_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_MAIN_MASK) |
			(cable_main << ONLINE_TYPE_MAIN_SHIFT);
	cable_sub = GET_SUB_CABLE_TYPE(input_extended_cable_type);
	if (cable_sub != ONLINE_SUB_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_SUB_MASK) |
			(cable_sub << ONLINE_TYPE_SUB_SHIFT);
	cable_power = GET_POWER_CABLE_TYPE(input_extended_cable_type);
	if (cable_power != ONLINE_POWER_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_PWR_MASK) |
			(cable_power << ONLINE_TYPE_PWR_SHIFT);

	switch (cable_main) {
	case POWER_SUPPLY_TYPE_CARDOCK:
		switch (cable_power) {
		case ONLINE_POWER_TYPE_BATTERY:
			cable_type = POWER_SUPPLY_TYPE_BATTERY;
			break;
		case ONLINE_POWER_TYPE_TA:
			switch (cable_sub) {
			case ONLINE_SUB_TYPE_MHL:
				cable_type = POWER_SUPPLY_TYPE_USB;
				break;
			case ONLINE_SUB_TYPE_AUDIO:
			case ONLINE_SUB_TYPE_DESK:
			case ONLINE_SUB_TYPE_SMART_NOTG:
			case ONLINE_SUB_TYPE_KBD:
				cable_type = POWER_SUPPLY_TYPE_MAINS;
				break;
			case ONLINE_SUB_TYPE_SMART_OTG:
				cable_type = POWER_SUPPLY_TYPE_CARDOCK;
				break;
			}
			break;
		case ONLINE_POWER_TYPE_USB:
			cable_type = POWER_SUPPLY_TYPE_USB;
			break;
		default:
			cable_type = current_cable_type;
			break;
		}
		break;
	case POWER_SUPPLY_TYPE_MISC:
		switch (cable_sub) {
		case ONLINE_SUB_TYPE_MHL:
			switch (cable_power) {
			case ONLINE_POWER_TYPE_BATTERY:
				cable_type = POWER_SUPPLY_TYPE_BATTERY;
				break;
			case ONLINE_POWER_TYPE_TA:
				cable_type = POWER_SUPPLY_TYPE_MAINS;
				charge_current_max = 700;
				charge_current = 700;
				break;
			case ONLINE_POWER_TYPE_USB:
				cable_type = POWER_SUPPLY_TYPE_USB;
				charge_current_max = 300;
				charge_current = 300;
				break;
			}
			break;
		default:
			cable_type = cable_main;
			break;
		}
		break;
	default:
		cable_type = cable_main;
		break;
	}

	if (charge_current_max == 0) {
		charge_current_max =
			charging_current_table[cable_type].input_current_limit;
		charge_current =
			charging_current_table[cable_type].
			fast_charging_current;
	}
	value.intval = charge_current_max;
	psy_do_property(sec_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_MAX, value);
	value.intval = charge_current;
	psy_do_property(sec_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_AVG, value);
	return cable_type;
}
*/


static int sec_bat_check_cable_callback(void)
{
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
	}
}

#if 0 
static int sec_bat_check_cable_callback(void)
{
/*	struct usb_gadget *gadget =
 *			platform_get_drvdata(&s3c_device_usbgadget);
 */
	bool attach = true;
	int ta_nconnected;
	int adc = 0;

	if (!charger_supply) {
		charger_supply = power_supply_get_by_name("sec-charger");

		if (!charger_supply)
			pr_err("%s: failed to get power supplies\n", __func__);
	}

	/* delay 100ms */
	msleep(100);

//	usb_switch_lock();
//	usb_switch_set_path(USB_PATH_ADCCHECK);

//	adc = stmpe811_get_adc_data(6);

//	usb_switch_clr_path(USB_PATH_ADCCHECK);
//	usb_switch_unlock();

	ta_nconnected = gpio_get_value(mfp_to_gpio(GPIO005_GPIO_5));

	pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);

	/* temp : set cable type always TA */
	current_cable_type = !ta_nconnected ?
				POWER_SUPPLY_TYPE_BATTERY :
				POWER_SUPPLY_TYPE_MAINS;

	attach = ta_nconnected ? true : false;

	is_cable_attached = attach;

	/* temp code : only set vbus enable when usb attaced */
/*	if (gadget) {
		if (attach && current_cable_type == POWER_SUPPLY_TYPE_USB)
			usb_gadget_vbus_connect(gadget);
		else
			usb_gadget_vbus_disconnect(gadget);
	} else {
		pr_err("%s: usb gadget drive not exist\n", __func__);
	}
*/
	pr_info("%s: Cable type(%s), Attach(%d), Adc(%d)\n", __func__,
		current_cable_type == POWER_SUPPLY_TYPE_BATTERY ?
		"Battery" : current_cable_type == POWER_SUPPLY_TYPE_USB ?
		"USB" : "TA", attach, adc);

	return current_cable_type;
}

static void sec_bat_initial_check(void)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	struct power_supply *chg_psy = power_supply_get_by_name("sec-charger");
	union power_supply_propval value;
	int ta_nconnected = 0;
	int ret = 0;

	value.intval = sec_bat_check_cable_callback();
	pr_info("%s: %d\n", __func__, value.intval);
/*
	if (value.intval != POWER_SUPPLY_TYPE_BATTERY)
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	else
		ret = chg_psy->set_property(chg_psy, POWER_SUPPLY_PROP_ONLINE, &value);
*/

	ta_nconnected = gpio_get_value(mfp_to_gpio(GPIO005_GPIO_5));

	pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);

	if (ta_nconnected &&
			value.intval != POWER_SUPPLY_TYPE_BATTERY) {
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret)
			pr_err("%s: fail to set power_suppy ONLINE property\n",
					__func__);
	} else {
		if (chg_psy) {
			ret = chg_psy->set_property(chg_psy,
					POWER_SUPPLY_PROP_ONLINE, &value);
			if (ret)
				pr_err("%s: fail to set ONLINE property\n",
						__func__);
		}
	}

}
#endif

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

static struct battery_data_t stc3115_battery_data[] = {
	{
		.Vmode= 0,       /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
		.Alm_SOC = 10,      /* SOC alm level %*/
		.Alm_Vbat = 0,   /* Vbat alm level mV*/
		.CC_cnf = 807,      /* nominal CC_cnf, coming from battery characterisation*/
		.VM_cnf = 1127,      /* nominal VM cnf , coming from battery characterisation*/
		.Cnom = 4000,       /* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,       /* sense resistor mOhms*/
		.RelaxCurrent = 150, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

		/* Elentec Co Ltd Battery pack - 80 means 8% */
		.CapDerating[6] = 277,   /* capacity derating in 0.1%, for temp = -20C */
		.CapDerating[5] = 82,   /* capacity derating in 0.1%, for temp = -10C */
		.CapDerating[4] = 23,    /* capacity derating in 0.1%, for temp = 0C */
		.CapDerating[3] = 19,  /* capacity derating in 0.1%, for temp = 10C */
		.CapDerating[2] = 0,  /* capacity derating in 0.1%, for temp = 25C */
		.CapDerating[1] = -2,  /* capacity derating in 0.1%, for temp = 40C */
		.CapDerating[0] = -2,  /* capacity derating in 0.1%, for temp = 60C */

		.OCVOffset[15] = -22,    /* OCV curve adjustment */
		.OCVOffset[14] = -9,   /* OCV curve adjustment */
		.OCVOffset[13] = -15,    /* OCV curve adjustment */
		.OCVOffset[12] = -2,    /* OCV curve adjustment */
		.OCVOffset[11] = 0,    /* OCV curve adjustment */
		.OCVOffset[10] = -2,    /* OCV curve adjustment */
		.OCVOffset[9] = -26,     /* OCV curve adjustment */
		.OCVOffset[8] = -6,      /* OCV curve adjustment */
		.OCVOffset[7] = -7,      /* OCV curve adjustment */
		.OCVOffset[6] = -14,    /* OCV curve adjustment */
		.OCVOffset[5] = -23,    /* OCV curve adjustment */
		.OCVOffset[4] = -46,     /* OCV curve adjustment */
		.OCVOffset[3] = -27,    /* OCV curve adjustment */
		.OCVOffset[2] = -34,     /* OCV curve adjustment */
		.OCVOffset[1] = -125,    /* OCV curve adjustment */
		.OCVOffset[0] = -68,     /* OCV curve adjustment */

		.OCVOffset2[15] = -58,    /* OCV curve adjustment */
		.OCVOffset2[14] = -37,   /* OCV curve adjustment */
		.OCVOffset2[13] = -21,    /* OCV curve adjustment */
		.OCVOffset2[12] = -14,    /* OCV curve adjustment */
		.OCVOffset2[11] = -6,    /* OCV curve adjustment */
		.OCVOffset2[10] = -16,    /* OCV curve adjustment */
		.OCVOffset2[9] = -6,     /* OCV curve adjustment */
		.OCVOffset2[8] = 4,      /* OCV curve adjustment */
		.OCVOffset2[7] = 9,      /* OCV curve adjustment */
		.OCVOffset2[6] = 11,  /* OCV curve adjustment */
		.OCVOffset2[5] = 24,    /* OCV curve adjustment */
		.OCVOffset2[4] = 7,     /* OCV curve adjustment */
		.OCVOffset2[3] = 28,    /* OCV curve adjustment */
		.OCVOffset2[2] = 89,     /* OCV curve adjustment */
		.OCVOffset2[1] = 94,    /* OCV curve adjustment */
		.OCVOffset2[0] = 0,     /* OCV curve adjustment */

			/*if the application temperature data is preferred than the STC3115 temperature*/
		.ExternalTemperature = NULL, /*External temperature fonction, return C*/
		.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3115 temperature */
	}
};

static bool sec_bat_adc_none_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_none_exit(void) { return true; }
static int sec_bat_adc_none_read(unsigned int channel) { return 0; }

static bool sec_bat_adc_ap_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ap_exit(void) { return true; }
#if 0
static int sec_bat_adc_ap_read(unsigned int channel)
{
        int i;
        int ret = 0;

	ret = s3c_adc_read(temp_adc_client, 2);

	if (ret == -ETIMEDOUT) {
		for (i = 0; i < 5; i++) {
			msleep(20);
			ret = s3c_adc_read(temp_adc_client, 2);
			if (ret > 0)
				break;
		}

		if (i >= 5)
			pr_err("%s: Retry count exceeded\n", __func__);

	} else if (ret < 0) {
		pr_err("%s: Failed read adc value : %d\n",
		__func__, ret);
	}

out:
        pr_info("%s: temp acd : %d\n", __func__, ret);
        return ret;
}
#endif

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
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_data = (void *)stc3115_battery_data,
	.bat_polarity_ta_nconnected = 1,        /* active HIGH */
	.bat_irq_attr = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE |
		SEC_BATTERY_CABLE_CHECK_INT,
	.cable_source_type = //SEC_BATTERY_CABLE_SOURCE_CALLBACK,
	SEC_BATTERY_CABLE_SOURCE_EXTERNAL,
//	SEC_BATTERY_CABLE_SOURCE_EXTENDED,

	.event_check = false,
	.event_waiting_time = 60,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_NONE,
	.check_count = 3,

	/* Battery check by ADC */
	.check_adc_max = 0,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_FG,
	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,

        .temp_check_count = 1,
        .temp_high_threshold_event = 50000,
        .temp_high_recovery_event = 420,
        .temp_low_threshold_event = -50,
        .temp_low_recovery_event = 0,
        .temp_high_threshold_normal = 50000,
        .temp_high_recovery_normal = 420,
        .temp_low_threshold_normal = -50,
        .temp_low_recovery_normal = 0,
        .temp_high_threshold_lpm = 50000,
        .temp_high_recovery_lpm = 420,
        .temp_low_threshold_lpm = -50,
        .temp_low_recovery_lpm = 0,

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_NONE,
	.full_check_count = 1,
	/* .full_check_adc_1st = 26500, */
	/*.full_check_adc_2nd = 25800, */
	.chg_polarity_full_check = 1,
	.full_condition_type =
		SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_OCV,
	.full_condition_soc = 99,
	.full_condition_ocv = 4170,

	.recharge_condition_type =
                SEC_BATTERY_RECHARGE_CONDITION_VCELL,

	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4150,
	.recharge_condition_vcell = 4150,

	.charging_total_time = 10 * 60 * 60,
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
	.chg_irq_attr = IRQF_TRIGGER_RISING,

	.chg_float_voltage = 4200,
};

/* set STC3115 Fuel Gauge gpio i2c */
static struct i2c_gpio_platform_data cocoa7_gpio_i2c6_pdata = {
	.sda_pin = mfp_to_gpio(GPIO052_GPIO_52),
	.scl_pin = mfp_to_gpio(GPIO051_GPIO_51),
	.udelay = 3,
	.timeout = 100,
};

static struct platform_device cocoa7_gpio_i2c6_device = {
	.name = "i2c-gpio",
	.id = SEC_FUELGAUGE_I2C_ID,
	.dev = {
		.platform_data = &cocoa7_gpio_i2c6_pdata,
	},
};

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev = {
		.platform_data = &sec_battery_pdata,
	},
};

static struct i2c_board_info sec_brdinfo_charger[] __initdata = {
	{
		I2C_BOARD_INFO("sec-charger",
				SEC_CHARGER_I2C_SLAVEADDR),
		.platform_data = &sec_battery_pdata,
	},
};

static struct i2c_board_info sec_brdinfo_fuelgauge[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
				STC31xx_SLAVE_ADDRESS),
		.platform_data  = &sec_battery_pdata,
	},
};

static struct platform_device *sec_battery_devices[] __initdata = {
	&cocoa7_gpio_i2c6_device,
	&sec_device_battery,
};

static void charger_gpio_init(void)
{
	sec_battery_pdata.bat_irq = MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO005_GPIO_5));
}

void __init pxa986_cocoa7_charger_init(void)
{
	pr_info("%s: cocoa7 charger init\n", __func__);
	charger_gpio_init();

	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));

	i2c_register_board_info(SEC_CHARGER_I2C_ID, sec_brdinfo_charger,
			ARRAY_SIZE(sec_brdinfo_charger));

	i2c_register_board_info(SEC_FUELGAUGE_I2C_ID, sec_brdinfo_fuelgauge,
			ARRAY_SIZE(sec_brdinfo_fuelgauge));

//	temp_adc_client = s3c_adc_register(&sec_device_battery, NULL, NULL, 0);
}



