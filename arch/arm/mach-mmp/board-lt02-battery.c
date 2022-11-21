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
#include <mach/mfp-pxa986-lt02.h>
#include <mach/fsa9480.h>

#include "board-lt02.h"

#if defined(CONFIG_MFD_88PM800)
#include <linux/mfd/88pm80x.h>
#endif

#define SEC_CHARGER_I2C_ID	2
#define SEC_FUELGAUGE_I2C_ID	6

#define SEC_BATTERY_PMIC_NAME ""

#define TA_ADC_LOW              800
#define TA_ADC_HIGH             2200

/* cable state */
bool is_cable_attached;
unsigned int lpcharge;
EXPORT_SYMBOL(lpcharge);

static struct s3c_adc_client *temp_adc_client;

static sec_bat_adc_table_data_t lt02_temp_table[] = {
        {64,  700},
        {69,  670},
        {82,  640},
        {87,  620},
        {93,  600},
        {97,  580},
        {108, 550},
        {123, 500},
        {146, 440},
        {157, 420},
        {169, 400},
        {233, 300},
        {343, 200},
        {498, 100},
        {692, 20},
        {753, 0},
        {819, -20},
        {875, -40},
        {930, -50},
        {1010, -70},
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
	{1800,  2000,   200,    40 * 60},     /* POWER_SUPPLY_TYPE_MAINS */
	{500,   500,    200,    40 * 60},     /* POWER_SUPPLY_TYPE_USB */
	{500,   500,    200,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{500,   500,    200,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{500,   500,    200,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{1800,  2000,   200,    40 * 60},     /* POWER_SUPPLY_TYPE_MISC */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_CARDOCK */
	{500,   500,    200,    40 * 60},     /* POWER_SUPPLY_TYPE_WPC */
	{1800,  2000,   200,    40 * 60},     /* POWER_SUPPLY_TYPE_UARTOFF */
};

/* unit: seconds */
static int polling_time_table[] = {
	10,     /* BASIC */
	30,     /* CHARGING */
	60,     /* DISCHARGING */
	30,     /* NOT_CHARGING */
	1800,    /* SLEEP */
};

static struct power_supply *charger_supply;
static bool is_jig_on;
int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
EXPORT_SYMBOL(current_cable_type);
u8 attached_cable;

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
	if (strstr(saved_command_line, "androidboot.mode=charger")) {
		lpcharge = 1;
	}
	else
		lpcharge = 0;
	pr_info("%s: Low power charging mode: %d\n", __func__, lpcharge);

	return lpcharge;
}
__setup("androidboot.mode=", battery_get_lpm_state);

static bool sec_bat_is_lpm(void)
{
	return lpcharge == 1 ? true : false;
}

static bool sec_bat_check_jig_status(void)
{
	if ((attached_cable == CABLE_TYPE2_JIG_UART_OFF_VB_MUIC) ||
	    (attached_cable == CABLE_TYPE2_JIG_UART_OFF_MUIC) ||
	    (attached_cable == CABLE_TYPE2_JIG_USB_OFF_MUIC) ||
	    (attached_cable == CABLE_TYPE2_JIG_USB_ON_MUIC)) {
		pr_info("%s: JIG On so reset fuel gauge capacity\n", __func__);
		is_jig_on = true;
	} else {
		is_jig_on = false;
	}

	return is_jig_on;
}

void sec_charger_cb(u8 attached)
{

	pr_info("%s: cable attached(%d)\n", __func__, attached);

	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	int ret;

	attached_cable = attached;

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
	case CABLE_TYPE2_JIG_USB_ON_MUIC:
	case CABLE_TYPE2_JIG_USB_OFF_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE2_DESKDOCK_MUIC:
	case CABLE_TYPE3_MHL_VB_MUIC:
	case CABLE_TYPE3_MHL_MUIC:
	case CABLE_TYPE3_DESKDOCK_VB_MUIC:
		goto skip;
	case CABLE_TYPE3_U200CHG_MUIC:
	case CABLE_TYPE3_NONSTD_SDP_MUIC:
	case CABLE_TYPE3_APPLECHG_MUIC:
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
	int ta_nconnected;
	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");
	int ret;
#if defined(CONFIG_MACH_LT02LGT)
	int retry_cnt = 0;
#endif

	ta_nconnected = gpio_get_value(mfp_to_gpio(GPIO005_GPIO_5));

	pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);

#if defined(CONFIG_MACH_LT02LGT)
	do {
		msleep(300);

		ta_nconnected = gpio_get_value(mfp_to_gpio(GPIO005_GPIO_5));
		if (!(ta_nconnected && !attached_cable))
			break;

		pr_info("%s : ta_nconnected : %d (%d)\n",
			__func__, ta_nconnected, retry_cnt);
	} while(retry_cnt++ < 13);
#else
	msleep(300);
#endif

	if((attached_cable == CABLE_TYPE2_DESKDOCK_MUIC) ||
	   (attached_cable == CABLE_TYPE3_DESKDOCK_VB_MUIC)) {
		if (!ta_nconnected)
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		else
			current_cable_type = POWER_SUPPLY_TYPE_MISC;
	} else if((attached_cable == CABLE_TYPE2_JIG_UART_OFF_MUIC) ||
		  (attached_cable == CABLE_TYPE2_JIG_UART_OFF_VB_MUIC)) {
		if (!ta_nconnected)
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		else
			current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
	} else
		return current_cable_type;

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
	int ta_nconnected;

	ta_nconnected = gpio_get_value(mfp_to_gpio(GPIO005_GPIO_5));

	if (ta_nconnected == 1)
		sec_bat_check_cable_callback();

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
		.Alm_SOC = 1,      /* SOC alm level %*/
		.Alm_Vbat = 3400,   /* Vbat alm level mV*/
		.CC_cnf = 870,      /* nominal CC_cnf, coming from battery characterisation*/
		.VM_cnf = 422,      /* nominal VM cnf , coming from battery characterisation*/
		.Cnom = 4000,       /* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,       /* sense resistor mOhms*/
		.RelaxCurrent = 150, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

		/* Elentec Co Ltd Battery pack - 80 means 8% */
		.CapDerating[6] = 80,   /* capacity derating in 0.1%, for temp = -20C */
		.CapDerating[5] = 50,   /* capacity derating in 0.1%, for temp = -10C */
		.CapDerating[4] = 20,    /* capacity derating in 0.1%, for temp = 0C */
		.CapDerating[3] = 10,  /* capacity derating in 0.1%, for temp = 10C */
		.CapDerating[2] = 0,  /* capacity derating in 0.1%, for temp = 25C */
		.CapDerating[1] = 0,  /* capacity derating in 0.1%, for temp = 40C */
		.CapDerating[0] = 0,  /* capacity derating in 0.1%, for temp = 60C */

		.OCVOffset[15] = -110,    /* OCV curve adjustment */
		.OCVOffset[14] = -10,   /* OCV curve adjustment */
		.OCVOffset[13] = -6,    /* OCV curve adjustment */
		.OCVOffset[12] = -6,    /* OCV curve adjustment */
		.OCVOffset[11] = 0,    /* OCV curve adjustment */
		.OCVOffset[10] = -6,    /* OCV curve adjustment */
		.OCVOffset[9] = 2,     /* OCV curve adjustment */
		.OCVOffset[8] = -9,      /* OCV curve adjustment */
		.OCVOffset[7] = -1,      /* OCV curve adjustment */
		.OCVOffset[6] = 11,    /* OCV curve adjustment */
		.OCVOffset[5] = 9,    /* OCV curve adjustment */
		.OCVOffset[4] = 16,     /* OCV curve adjustment */
		.OCVOffset[3] = 37,    /* OCV curve adjustment */
		.OCVOffset[2] = 26,     /* OCV curve adjustment */
		.OCVOffset[1] = -44,    /* OCV curve adjustment */
		.OCVOffset[0] = -66,     /* OCV curve adjustment */

		.OCVOffset2[15] = -25,    /* OCV curve adjustment */
		.OCVOffset2[14] = -19,   /* OCV curve adjustment */
		.OCVOffset2[13] = -6,    /* OCV curve adjustment */
		.OCVOffset2[12] = -6,    /* OCV curve adjustment */
		.OCVOffset2[11] = -2,    /* OCV curve adjustment */
		.OCVOffset2[10] = 2,    /* OCV curve adjustment */
		.OCVOffset2[9] = -1,     /* OCV curve adjustment */
		.OCVOffset2[8] = 1,      /* OCV curve adjustment */
		.OCVOffset2[7] = 1,      /* OCV curve adjustment */
		.OCVOffset2[6] = -1,  /* OCV curve adjustment */
		.OCVOffset2[5] = 10,    /* OCV curve adjustment */
		.OCVOffset2[4] = -18,     /* OCV curve adjustment */
		.OCVOffset2[3] = 21,    /* OCV curve adjustment */
		.OCVOffset2[2] = 66,     /* OCV curve adjustment */
		.OCVOffset2[1] = 2,    /* OCV curve adjustment */
		.OCVOffset2[0] = 0,     /* OCV curve adjustment */

			/*if the application temperature data is preferred than the STC3115 temperature*/
		.ExternalTemperature = NULL, /*External temperature fonction, return C*/
		.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3115 temperature */
	},
	{
		.Vmode= 0,       /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
		.Alm_SOC = 1,      /* SOC alm level %*/
		.Alm_Vbat = 3400,   /* Vbat alm level mV*/
		.CC_cnf = 872,      /* nominal CC_cnf, coming from battery characterisation*/
		.VM_cnf = 412,      /* nominal VM cnf , coming from battery characterisation*/
		.Cnom = 4000,       /* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,       /* sense resistor mOhms*/
		.RelaxCurrent = 150, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

		/* Elentec Co Ltd Battery pack - 80 means 8% */
		.CapDerating[6] = 80,   /* capacity derating in 0.1%, for temp = -20C */
		.CapDerating[5] = 50,   /* capacity derating in 0.1%, for temp = -10C */
		.CapDerating[4] = 20,    /* capacity derating in 0.1%, for temp = 0C */
		.CapDerating[3] = 10,  /* capacity derating in 0.1%, for temp = 10C */
		.CapDerating[2] = 0,  /* capacity derating in 0.1%, for temp = 25C */
		.CapDerating[1] = 0,  /* capacity derating in 0.1%, for temp = 40C */
		.CapDerating[0] = 0,  /* capacity derating in 0.1%, for temp = 60C */

		.OCVOffset[15] = -110,    /* OCV curve adjustment */
		.OCVOffset[14] = -10,   /* OCV curve adjustment */
		.OCVOffset[13] = -6,    /* OCV curve adjustment */
		.OCVOffset[12] = -6,    /* OCV curve adjustment */
		.OCVOffset[11] = 0,    /* OCV curve adjustment */
		.OCVOffset[10] = -6,    /* OCV curve adjustment */
		.OCVOffset[9] = 2,     /* OCV curve adjustment */
		.OCVOffset[8] = -9,      /* OCV curve adjustment */
		.OCVOffset[7] = -1,      /* OCV curve adjustment */
		.OCVOffset[6] = 11,    /* OCV curve adjustment */
		.OCVOffset[5] = 9,    /* OCV curve adjustment */
		.OCVOffset[4] = 16,     /* OCV curve adjustment */
		.OCVOffset[3] = 37,    /* OCV curve adjustment */
		.OCVOffset[2] = 26,     /* OCV curve adjustment */
		.OCVOffset[1] = -44,    /* OCV curve adjustment */
		.OCVOffset[0] = -66,     /* OCV curve adjustment */

		.OCVOffset2[15] = -38,    /* OCV curve adjustment */
		.OCVOffset2[14] = -38,   /* OCV curve adjustment */
		.OCVOffset2[13] = -24,    /* OCV curve adjustment */
		.OCVOffset2[12] = -26,    /* OCV curve adjustment */
		.OCVOffset2[11] = -25,    /* OCV curve adjustment */
		.OCVOffset2[10] = -31,    /* OCV curve adjustment */
		.OCVOffset2[9] = -5,     /* OCV curve adjustment */
		.OCVOffset2[8] = -3,      /* OCV curve adjustment */
		.OCVOffset2[7] = -3,      /* OCV curve adjustment */
		.OCVOffset2[6] = -17,  /* OCV curve adjustment */
		.OCVOffset2[5] = -3,    /* OCV curve adjustment */
		.OCVOffset2[4] = -5,     /* OCV curve adjustment */
		.OCVOffset2[3] = 3,    /* OCV curve adjustment */
		.OCVOffset2[2] = 48,     /* OCV curve adjustment */
		.OCVOffset2[1] = 17,    /* OCV curve adjustment */
		.OCVOffset2[0] = 0,     /* OCV curve adjustment */

			/*if the application temperature data is preferred than the STC3115 temperature*/
		.ExternalTemperature = NULL, /*External temperature fonction, return C*/
		.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3115 temperature */
	}
};

static struct battery_data_t *battery_data_sample;

static void sec_bat_check_vf_callback(void)
{
        int vfbat;
	int min = 1 << 11, max = -1;
	int total = 0;
	int vf_avg = 0;
	int count;
	int ret;

	for (count = 0; count < 5; count++)
	{
		ret = pm80x_read_vf(&vfbat);
		if (vfbat > max)
			max = vfbat;
	        if (vfbat < min)
			min = vfbat;
		total += vfbat;
	}

	vf_avg = (total - min - max) / 3;

	if ((vf_avg > 0) && (vf_avg < 120)) {
		battery_data_sample = &stc3115_battery_data[0];
		sec_battery_pdata.vendor = "SDI SDI";
		sec_battery_pdata.battery_data = (void *)battery_data_sample;

		pr_info("%s : ADC = %d[%s]\n", __func__,
			vf_avg, sec_battery_pdata.vendor);

	} else if((vf_avg > 290) && (vf_avg < 390)) {
		battery_data_sample = &stc3115_battery_data[1];
		sec_battery_pdata.vendor = "ATL ATL";
		sec_battery_pdata.battery_data = (void *)battery_data_sample;

		pr_info("%s : ADC = %d[%s]\n", __func__,
			vf_avg, sec_battery_pdata.vendor);

	} else
		pr_info("%s : ADC = %d[VF ADC ERROR]\n",
			__func__, vf_avg);

	sec_battery_pdata.vf_adc = vf_avg;
}

static bool sec_bat_adc_none_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_none_exit(void) { return true; }
static int sec_bat_adc_none_read(unsigned int channel) { return 0; }

static bool sec_bat_adc_ap_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ap_exit(void) { return true; }
static int sec_bat_adc_ap_read(unsigned int channel)
{
        int data;
        int ret = 0;

	ret = pm80x_read_temperature(&data);

	return data;
}
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
	.check_vf_callback =
	        sec_bat_check_vf_callback,

	.adc_api[SEC_BATTERY_ADC_TYPE_NONE] = {
		.init = sec_bat_adc_none_init,
		.exit = sec_bat_adc_none_exit,
		.read = sec_bat_adc_none_read
	},
	.adc_api[SEC_BATTERY_ADC_TYPE_AP] = {
		.init = sec_bat_adc_ap_init,
		.exit = sec_bat_adc_ap_exit,
		.read = sec_bat_adc_ap_read
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
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP */
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
	.cable_source_type = SEC_BATTERY_CABLE_SOURCE_CALLBACK |
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

	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_adc_table = lt02_temp_table,
	.temp_adc_table_size =
	sizeof(lt02_temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = lt02_temp_table,
	.temp_amb_adc_table_size =
		sizeof(lt02_temp_table)/sizeof(sec_bat_adc_table_data_t),

        .temp_check_count = 1,
#if defined(CONFIG_MACH_LT02LGT)
        .temp_high_threshold_event = 640,
        .temp_high_recovery_event = 420,
        .temp_low_threshold_event = -70,
        .temp_low_recovery_event = 0,
        .temp_high_threshold_normal = 640,
        .temp_high_recovery_normal = 420,
        .temp_low_threshold_normal = -70,
        .temp_low_recovery_normal = 0,
        .temp_high_threshold_lpm = 640,
        .temp_high_recovery_lpm = 420,
        .temp_low_threshold_lpm = -70,
        .temp_low_recovery_lpm = 0,
#elif defined(CONFIG_MACH_LT02_BBY)
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 420,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 480,
	.temp_high_recovery_normal = 420,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 480,
	.temp_high_recovery_lpm = 420,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 0,
#else
        .temp_high_threshold_event = 600,
        .temp_high_recovery_event = 420,
        .temp_low_threshold_event = -50,
        .temp_low_recovery_event = 0,
        .temp_high_threshold_normal = 600,
        .temp_high_recovery_normal = 420,
        .temp_low_threshold_normal = -50,
        .temp_low_recovery_normal = 0,
        .temp_high_threshold_lpm = 600,
        .temp_high_recovery_lpm = 420,
        .temp_low_threshold_lpm = -50,
        .temp_low_recovery_lpm = 0,
#endif

	.full_check_type = SEC_BATTERY_FULLCHARGED_FG_CURRENT,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_TIME,
	.full_check_count = 2,
	/* .full_check_adc_1st = 26500, */
	/*.full_check_adc_2nd = 25800, */
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
		SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_OCV,
	.full_condition_soc = 95,
	.full_condition_ocv = 4150,

	.recharge_condition_type =
                SEC_BATTERY_RECHARGE_CONDITION_VCELL,

	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4150,
	.recharge_condition_vcell = 4150,

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
	.chg_irq_attr = IRQF_TRIGGER_RISING,

#if defined(CONFIG_MACH_LT02LGT)
	.chg_float_voltage = 4180,
#else
	.chg_float_voltage = 4200,
#endif
	.siop_activated = 0,
	.siop_level = 0,
};

/* set STC3115 Fuel Gauge gpio i2c */
static struct i2c_gpio_platform_data lt02_gpio_i2c6_pdata = {
	.sda_pin = mfp_to_gpio(GPIO052_GPIO_52),
	.scl_pin = mfp_to_gpio(GPIO051_GPIO_51),
	.udelay = 3,
	.timeout = 100,
};

static struct platform_device lt02_gpio_i2c6_device = {
	.name = "i2c-gpio",
	.id = SEC_FUELGAUGE_I2C_ID,
	.dev = {
		.platform_data = &lt02_gpio_i2c6_pdata,
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
	&lt02_gpio_i2c6_device,
	&sec_device_battery,
};

static void charger_gpio_init(void)
{
	sec_battery_pdata.bat_irq = MMP_GPIO_TO_IRQ(mfp_to_gpio(GPIO005_GPIO_5));
}

void __init pxa986_lt02_charger_init(void)
{
	pr_info("%s: lt02 charger init\n", __func__);
	charger_gpio_init();

	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));

	i2c_register_board_info(SEC_CHARGER_I2C_ID, sec_brdinfo_charger,
			ARRAY_SIZE(sec_brdinfo_charger));

	i2c_register_board_info(SEC_FUELGAUGE_I2C_ID, sec_brdinfo_fuelgauge,
			ARRAY_SIZE(sec_brdinfo_fuelgauge));

}



