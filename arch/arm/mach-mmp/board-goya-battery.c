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
#include <mach/mfp-pxa986-goya.h>

#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/gpio_event.h>
#if defined(CONFIG_MFD_RT8973)
#include <linux/mfd/rt8973.h>
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

#include "board-goya.h"
#include "common.h"

#if defined(CONFIG_BATTERY_SAMSUNG)

#define SEC_STBC_I2C_ID		6
#define SEC_CHARGER_I2C_ID	6
#define SEC_FUELGAUGE_I2C_ID	6

#define SEC_STBC_I2C_SLAVEADDR		0x71
#define SEC_STBC_CHG_EN			mfp_to_gpio(GPIO008_GPIO_8)
#define STBC_LOW_BATT				mfp_to_gpio(GPIO009_GPIO_9)

#define SEC_BATTERY_PMIC_NAME ""

#define TA_ADC_LOW              800
#define TA_ADC_HIGH             2200

/* cable state */
bool is_cable_attached;
unsigned int lpcharge;
extern unsigned int system_rev;

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
	{1000,  1550,   200,    2400},     /* POWER_SUPPLY_TYPE_MAINS */
	{550,   500,    200,    2400},     /* POWER_SUPPLY_TYPE_USB */
	{550,   500,    200,    2400},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{550,   500,    200,    2400},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{550,   500,    200,    2400},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{550,   500,    200,    2400},     /* POWER_SUPPLY_TYPE_MISC */
	{0,     0,      0,      0},     /* POWER_SUPPLY_TYPE_CARDOCK */
	{550,   500,    200,    2400},     /* POWER_SUPPLY_TYPE_WPC */
	{750,   750,   	200,    2400},     /* POWER_SUPPLY_TYPE_UARTOFF */
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

#if defined(CONFIG_MFD_RT8973)
#if defined(CONFIG_SENSORS_GRIP_SX9500) && defined(CONFIG_MACH_GOYA)
extern void charger_status_cb(int status);
#endif
void sec_charger_cb(u8 cable_type)
{
	pr_info("%s: cable type (0x%02x)\n", __func__, cable_type);

	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	attached_cable = cable_type;
	is_jig_on = false;

	switch (cable_type) {
	case MUIC_RT8973_CABLE_TYPE_NONE:
	case MUIC_RT8973_CABLE_TYPE_0x1A:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case MUIC_RT8973_CABLE_TYPE_USB:
	case MUIC_RT8973_CABLE_TYPE_CDP:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF_WITH_VBUS:
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_ON_WITH_VBUS:
		is_jig_on = true;
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case MUIC_RT8973_CABLE_TYPE_REGULAR_TA:
	case MUIC_RT8973_CABLE_TYPE_ATT_TA:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case MUIC_RT8973_CABLE_TYPE_OTG:
		goto skip;
	case MUIC_RT8973_CABLE_TYPE_JIG_UART_OFF:
		is_jig_on = true;
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_ON:
	case MUIC_RT8973_CABLE_TYPE_JIG_USB_OFF:
		is_jig_on = true;
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case MUIC_RT8973_CABLE_TYPE_L_SPEC_USB:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case MUIC_RT8973_CABLE_TYPE_TYPE1_CHARGER:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case MUIC_RT8973_CABLE_TYPE_0x15:
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
#if defined(CONFIG_SENSORS_GRIP_SX9500) && defined(CONFIG_MACH_GOYA)
	/* for grip sensor threshold change at TA/USB status - sx9500.c */
	charger_status_cb(current_cable_type);
#endif
skip:
	return 0;
}
#else
void sec_charger_cb(u8 cable_type)
{

	pr_info("%s: cable type (0x%02x)\n", __func__, cable_type);

	union power_supply_propval value;
	struct power_supply *psy = power_supply_get_by_name("battery");

	attached_cable = cable_type;

	switch (cable_type) {
	case CABLE_TYPE_NONE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_TYPE1_USB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE1_TA_MUIC:
	case CABLE_TYPE3_U200CHG_MUIC:
	case CABLE_TYPE2_JIG_UART_OFF_VB_MUIC:
	case CABLE_TYPE2_JIG_UART_ON_VB_MUIC:
		pr_info("%s: VBUS Online\n", __func__);
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE1_OTG_MUIC:
		goto skip;
	case CABLE_TYPE2_JIG_UART_OFF_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_TYPE2_JIG_USB_ON_MUIC:
	case CABLE_TYPE2_JIG_USB_OFF_MUIC:
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
#endif


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

#if defined(CONFIG_STBC_SAMSUNG)
static struct battery_data_t stbcfg01_battery_data[] = {
	{
		.Alm_en = 1,	         /* SOC and VBAT alarms enable, 0=disabled, 1=enabled */
		.Alm_SOC = 1,         /* SOC alarm level % */
		.Alm_Vbat = 3350,      /* Vbat alarm level mV */
		.VM_cnf = 191,         /* nominal VM_cnf for discharging, coming from battery characterisation */
		.VM_cnf_chg = 585,     /* nominal VM_cnf for charging, coming from battery characterisation */
		.Cnom = 1500,          /* nominal capacity in mAh, coming from battery characterisation */

		.CapDerating[6] = 0,   /* capacity derating in 0.1%, for temp = -20deg */
		.CapDerating[5] = 0,   /* capacity derating in 0.1%, for temp = -10deg */
		.CapDerating[4] = 0,   /* capacity derating in 0.1%, for temp = 0deg */
		.CapDerating[3] = 0,   /* capacity derating in 0.1%, for temp = 10deg */
		.CapDerating[2] = 0,   /* capacity derating in 0.1%, for temp = 25deg */
		.CapDerating[1] = 0,   /* capacity derating in 0.1%, for temp = 40deg */
		.CapDerating[0] = 0,   /* capacity derating in 0.1%, for temp = 60deg */

		.VM_cnf_comp[6] = 1400, /* VM_cnf temperature compensation multiplicator in %, for temp = -20deg */
		.VM_cnf_comp[5] = 1100, /* VM_cnf temperature compensation multiplicator in %, for temp = -10deg */
		.VM_cnf_comp[4] = 620, /* VM_cnf temperature compensation multiplicator in %, for temp = 0deg */
		.VM_cnf_comp[3] = 420, /* VM_cnf temperature compensation multiplicator in %, for temp = 10deg */
		.VM_cnf_comp[2] = 120, /* VM_cnf temperature compensation multiplicator in %, for temp = 25deg */
		.VM_cnf_comp[1] = 95,  /* VM_cnf temperature compensation multiplicator in %, for temp = 40deg */
		.VM_cnf_comp[0] = 88,  /* VM_cnf temperature compensation multiplicator in %, for temp = 60deg */

		.OCVOffset[15] = 20,    /* OCV curve adjustment */
		.OCVOffset[14] = -18,  /* OCV curve adjustment */
		.OCVOffset[13] = -23,  /* OCV curve adjustment */
		.OCVOffset[12] = -12,  /* OCV curve adjustment */
		.OCVOffset[11] = -20,  /* OCV curve adjustment */
		.OCVOffset[10] = -23,  /* OCV curve adjustment */
		.OCVOffset[9] = -15,   /* OCV curve adjustment */
		.OCVOffset[8] = -10,   /* OCV curve adjustment */
		.OCVOffset[7] = -8,    /* OCV curve adjustment */
		.OCVOffset[6] = -7,    /* OCV curve adjustment */
		.OCVOffset[5] = -22,   /* OCV curve adjustment */
		.OCVOffset[4] = -35,   /* OCV curve adjustment */
		.OCVOffset[3] = -46,   /* OCV curve adjustment */
		.OCVOffset[2] = -82,   /* OCV curve adjustment */
		.OCVOffset[1] = -103,  /* OCV curve adjustment */
		.OCVOffset[0] = 83,     /* OCV curve adjustment */

		.SysCutoffVolt = 3000, /* system cut-off voltage, system does not work under this voltage (platform UVLO) in mV */
		.EarlyEmptyCmp = 0,	   /* difference vs SysCutoffVolt to start early empty compensation in mV, typically 200, 0 for disable */

		.IFast = 0,            /* fast charge current, range 550-1250mA, value = IFast*100+550 mA */
		.IFast_usb = 0,        /* fast charge current (another conf. for USB), range 550-1250mA, value = IFast*100+550 mA */
		.IPre = 0,             /* pre-charge current, 0=450 mA, 1=100 mA */
		.ITerm = 4,            /* termination current, range 50-300 mA, value = ITerm*25+50 mA */
		.VFloat = 41,          /* floating voltage, range 3.52-4.78 V, value = VFloat*0.02+3.52 V */
		.ARChg = 0,            /* automatic recharge, 0=disabled, 1=enabled */
		.Iin_lim = 1,          /* input current limit, 0=100mA 1=500mA 2=800mA 3=1.2A 4=no limit */
		.DICL_en = 1,          /* dynamic input current limit, 0=disabled, 1=enabled */
		.VDICL = 2,            /* dynamic input curr lim thres, range 4.00-4.75V, value = VDICL*0.25+4.00 V */
		.DIChg_adj = 1,        /* dynamic charging current adjust enable, 0=disabled, 1=enabled */
		.TPre = 0,             /* pre-charge timer, 0=disabled, 1=enabled */
		.TFast = 0,            /* fast charge timer, 0=disabled, 1=enabled */
		.PreB_en = 1,          /* pre-bias function, 0=disabled, 1=enabled */
		.LDO_UVLO_th = 0,      /* LDO UVLO threshold, 0=3.5V 1=4.65V */
		.LDO_en = 1,           /* LDO output, 0=disabled, 1=enabled */
		.IBat_lim = 2,         /* OTG average battery current limit, 0=350mA 1=450mA 2=550mA 3=950mA */
		.WD = 0,               /* watchdog timer function, 0=disabled, 1=enabled */
		.FSW = 0,              /* switching frequency selection, 0=2MHz, 1=3MHz */

		.GPIO_cd = SEC_STBC_CHG_EN, /* charger disable GPIO */
		.GPIO_shdn = 0,                          /* all charger circuitry shutdown GPIO or 0=not used */

		.ExternalTemperature = 250,   /* external temperature function, return in 0.1deg */
		.power_supply_register = NULL,           /* custom power supply structure registration function */
		.power_supply_unregister = NULL,         /* custom power supply structure unregistration function */
	}
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
static int sec_bat_adc_none_read(unsigned int channel) { return 0; }

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
#if defined(CONFIG_STBC_SAMSUNG)
	.battery_data = (void *)stbcfg01_battery_data,
#endif
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.bat_polarity_ta_nconnected = 1,        /* active HIGH */
	.bat_irq_attr = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.cable_check_type = SEC_BATTERY_CABLE_CHECK_INT |
			SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE,
	.cable_source_type = //SEC_BATTERY_CABLE_SOURCE_CALLBACK,
		SEC_BATTERY_CABLE_SOURCE_EXTERNAL,
//	SEC_BATTERY_CABLE_SOURCE_EXTENDED,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_FUELGAUGE,
	.check_count = 3,

	/* Battery check by ADC */
	.check_adc_max = 600,
	.check_adc_min = 50,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

#if defined(CONFIG_FUELGAUGE_88PM822)
#if defined(CONFIG_MACH_GOYA_USA)
	.event_check = true,
	.event_waiting_time = 60,
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_FG,
	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 1,
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 460,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 560,
	.temp_high_recovery_normal = 460,
	.temp_low_threshold_normal = 10,
	.temp_low_recovery_normal = 40,
	.temp_high_threshold_lpm = 560,
	.temp_high_recovery_lpm = 460,
	.temp_low_threshold_lpm = 10,
	.temp_low_recovery_lpm = 40,
#else
	.event_check = false,
	.event_waiting_time = 60,
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
#endif
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
	.full_condition_soc = 100,
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
		/* SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE, */
		SEC_FUELGAUGE_CAPACITY_TYPE_RAW,
	.capacity_max = 970,
	.capacity_min = 0,
	.capacity_max_margin = 30,

	/* Charger */
	.chg_polarity_en = 0,   /* active LOW charge enable */
	.chg_polarity_status = 0,
	.chg_irq_attr = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,

#if defined(CONFIG_GOYA_WIFI)
	.chg_float_voltage = 4330,
#else
	.chg_float_voltage = 4340,
#endif

};

#if defined(CONFIG_STBC_SAMSUNG)
static struct stbcfg01_platform_data stbc_battery_pdata = {
	.charger_data = &sec_battery_pdata,
	.fuelgauge_data = &sec_battery_pdata,
};
#endif

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev = {
		.platform_data = &sec_battery_pdata,
	},
};

#if defined(CONFIG_STBC_SAMSUNG)
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

static struct i2c_board_info sec_brdinfo_stbc[] __initdata = {
	{
		I2C_BOARD_INFO("sec-stbc",
				SEC_STBC_I2C_SLAVEADDR),
		.platform_data = &stbc_battery_pdata,
	},
};
#endif

#if defined(CONFIG_CHARGER_RT9455) || defined(CONFIG_CHARGER_SMB328)
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
#if defined(CONFIG_STBC_SAMSUNG)
	&fuelgauge_gpio_i2c_device,
#endif
#if defined(CONFIG_CHARGER_RT9455) || defined(CONFIG_CHARGER_SMB328)
	&charger_gpio_i2c_device,
#endif
};



void __init pxa986_goya_battery_init(void)
{
	pr_info("%s: goya charger init\n", __func__);

#if defined(CONFIG_STBC_SAMSUNG)
	i2c_register_board_info(SEC_STBC_I2C_ID, sec_brdinfo_stbc,
		ARRAY_SIZE(sec_brdinfo_stbc));
#endif

#if defined(CONFIG_CHARGER_RT9455) || defined(CONFIG_CHARGER_SMB328)
	i2c_register_board_info(SEC_CHARGER_I2C_ID,
		ARRAY_AND_SIZE(sec_brdinfo_charger));
#endif
	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));
}
#endif
