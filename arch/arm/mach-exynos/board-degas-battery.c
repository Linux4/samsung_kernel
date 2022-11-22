/*
 * Copyright (C) 2014 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/platform_device.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/machine.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/gpio.h>

#include <asm/io.h>

#include <plat/devs.h>
#include <plat/iic.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-pmu.h>
#include <mach/irqs.h>

#include <linux/mfd/samsung/core.h>

#include "board-universal222ap.h"

#include "common.h"

#if defined(CONFIG_S3C_ADC)
#include <plat/adc.h>
#endif

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>

#ifdef CONFIG_SM5502_MUIC
#include <mach/sm5502-muic.h>
#endif

#define SEC_BATTERY_PMIC_NAME ""

#if defined(CONFIG_S3C_ADC)
static struct s3c_adc_client* adc_client;
#endif

#if 0
static unsigned int sec_bat_recovery_mode;
#endif

extern int current_cable_type;
bool is_jig_on = false;
bool is_wpc_cable_attached = false;
bool is_ovlo_state = false;

#define SEC_CHG_I2C_ID	13
#define SEC_FG_I2C_ID	15

/* For TEMP*/
#define	LOW_BAT_DETECT	EXYNOS4_GPX2(3)
#define	GPIO_FG_SCL     EXYNOS4_GPF1(5)
#define	GPIO_FG_SDA     EXYNOS4_GPF0(7)
#define GPIO_CHG_SCL	EXYNOS4_GPM4(6)
#define GPIO_CHG_SDA	EXYNOS4_GPM4(7)
#define GPIO_CHG_STAT	EXYNOS4_GPX2(1)
#define GPIO_CHG_EN		EXYNOS4_GPM1(0)

unsigned int lpcharge;
EXPORT_SYMBOL(lpcharge);

static sec_charging_current_t charging_current_table[] = {
	{950,	1000,	280,	2400},     /* POWER_SUPPLY_TYPE_UNKNOWN */
	{0,	0,	0,	0},     /* POWER_SUPPLY_TYPE_BATTERY */
	{0,	0,	0,	0},     /* POWER_SUPPLY_TYPE_UPS */
	{1000,	1200,	280,	2400},	  /* POWER_SUPPLY_TYPE_MAINS */
	{450,	450,	280,	2400},	  /* POWER_SUPPLY_TYPE_USB */
	{450,	450,	280,	2400},	  /* POWER_SUPPLY_TYPE_USB_DCP */
	{950,	1000,	280,	2400},	  /* POWER_SUPPLY_TYPE_USB_CDP */
	{450,	450,	280,	2400},	  /* POWER_SUPPLY_TYPE_USB_ACA */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_BMS*/
	{500,	500,	280,	2400},	 /* POWER_SUPPLY_TYPE_MISC */
	{0, 0,	0,	0},   /* POWER_SUPPLY_TYPE_WIRELESS */
	{950,	1000,	280,	2400},		/* POWER_SUPPLY_TYPE_CARDOCK */
	{950,	1200,	280,	2400},	  /* POWER_SUPPLY_TYPE_UARTOFF */
	{0,	0,	0,	0},	/* POWER_SUPPLY_TYPE_OTG */
	{500,	500,	280,	2400},/* POWER_SUPPLY_TYPE_LAN_HUB */
};

static bool sec_bat_adc_none_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_none_exit(void) {return true; }
static int sec_bat_adc_none_read(unsigned int channel) {return 0; }

static bool sec_bat_adc_ap_init(struct platform_device *pdev)
{
#if defined(CONFIG_S3C_ADC)
	if (!adc_client) {
		adc_client = s3c_adc_register(pdev, NULL, NULL, 0);
		if (IS_ERR(adc_client))
			pr_err("ADC READ ERROR");
		else
			pr_info("%s: sec_bat_adc_ap_init succeed\n", __func__);
	}
#endif
	return true;
}
static bool sec_bat_adc_ap_exit(void)
{
#if defined(CONFIG_S3C_ADC)
	s3c_adc_release(adc_client);
#endif
	return true;
}
static int sec_bat_adc_ap_read(unsigned int channel)
{
	int data = -1;

	switch (channel) {
	case SEC_BAT_ADC_CHANNEL_TEMP:
#if defined(CONFIG_S3C_ADC)
		data = s3c_adc_read(adc_client, 1);
#else
		data = 1500;
#endif
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		data = 1500;
		break;
	}

	return data;
}

static bool sec_bat_adc_ic_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_ic_exit(void) {return true; }
static int sec_bat_adc_ic_read(unsigned int channel) {return 0; }

static bool sec_bat_gpio_init(void)
{
	return true;
}

static bool sec_fg_gpio_init(void)
{
	s3c_gpio_cfgpin(LOW_BAT_DETECT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(LOW_BAT_DETECT, S3C_GPIO_PULL_NONE);

	return true;
}

static bool sec_chg_gpio_init(void)
{
	return true;
}

static int sec_bat_is_lpm_check(char *str)
{
	if (strncmp(str, "charger", 7) == 0)
		lpcharge = 1;

	pr_info("%s: Low power charging mode: %d\n", __func__, lpcharge);

	return lpcharge;
}
__setup("androidboot.mode=", sec_bat_is_lpm_check);

static bool sec_bat_is_lpm(void)
{
	pr_err("%s: lpcharge(%d)\n", __func__, lpcharge);
	return lpcharge;
}

int extended_cable_type;

static void sec_bat_initial_check(void)
{
	union power_supply_propval value;

	if (POWER_SUPPLY_TYPE_BATTERY < current_cable_type) {
		if (current_cable_type == POWER_SUPPLY_TYPE_POWER_SHARING) {
			value.intval = current_cable_type;
			psy_do_property("ps", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		} else {
			value.intval = current_cable_type;
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		}
	} else {
		psy_do_property("sec-charger", get,
				POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_WIRELESS) {
			value.intval = POWER_SUPPLY_TYPE_WIRELESS;
			psy_do_property("battery", set,
					POWER_SUPPLY_PROP_ONLINE, value);
		}
	}
}

static bool sec_bat_switch_to_check(void) {return true; }
static bool sec_bat_switch_to_normal(void) {return true; }

static int sec_bat_check_cable_callback(void)
{
	return current_cable_type;
}

#if defined(CONFIG_REGULATOR_S2MPU01A)
extern bool s2mpu01a_get_jig_status(void);
#endif

static bool sec_bat_check_jig_status(void)
{
#if defined(CONFIG_REGULATOR_S2MPU01A)
	is_jig_on = s2mpu01a_get_jig_status();
#endif
	pr_info("%s: is_jig_on(%d)\n", __func__, is_jig_on);
	return is_jig_on;
}

#if 0
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
			case ONLINE_POWER_TYPE_MHL_500:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 400;
				charge_current = 400;
				break;
			case ONLINE_POWER_TYPE_MHL_900:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 700;
				charge_current = 700;
				break;
			case ONLINE_POWER_TYPE_MHL_1500:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 1300;
				charge_current = 1300;
				break;
			case ONLINE_POWER_TYPE_USB:
				cable_type = POWER_SUPPLY_TYPE_USB;
				charge_current_max = 300;
				charge_current = 300;
				break;
			default:
				cable_type = cable_main;
			}
			break;
		case ONLINE_SUB_TYPE_SMART_OTG:
			cable_type = POWER_SUPPLY_TYPE_USB;
			charge_current_max = 1000;
			charge_current = 1000;
			break;
		case ONLINE_SUB_TYPE_SMART_NOTG:
			cable_type = POWER_SUPPLY_TYPE_MAINS;
			charge_current_max = 1900;
			charge_current = 1600;
			break;
		default:
			cable_type = cable_main;
			charge_current_max = 0;
			break;
		}
		break;
	default:
		cable_type = cable_main;
		break;
	}

#if 0
	if (cable_type == POWER_SUPPLY_TYPE_WIRELESS)
		is_wpc_cable_attached = true;
	else
		is_wpc_cable_attached = false;
#endif

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
#endif

static bool sec_bat_check_cable_result_callback(
				int cable_type)
{
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
		return false;
	}
	return true;
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
static bool sec_bat_check_callback(void)
{
#if 0/*No need for Tablet*/
	struct power_supply *psy;
	union power_supply_propval value;

	psy = get_power_supply_by_name(("sec-charger"));
	if (!psy) {
		pr_err("%s: Fail to get psy (%s)\n",
			__func__, "sec-charger");
		value.intval = 1;
	} else {
		int ret;
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &(value));
		if (ret < 0) {
			pr_err("%s: Fail to sec-charger get_property (%d=>%d)\n",
				__func__, POWER_SUPPLY_PROP_PRESENT, ret);
			value.intval = 1;
		}
	}

	return value.intval;
#endif
	return true;
}
static bool sec_bat_check_result_callback(void) {return true; }

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

static bool sec_bat_ovp_uvlo_result_callback(int health) {return true; }

/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val) {return true; }

static bool sec_fg_fuelalert_process(bool is_fuel_alerted) {return true; }

static sec_bat_adc_table_data_t temp_table[] = {
	{	108,	900 },
	{	123,	850 },
	{	140,	800 },
	{	159,	750 },
	{	175,	700 },
	{	212,	650 },
	{	243,	600 },
	{	282,	550 },
	{	327,	500 },
	{	367,	460 },
	{	377,	450 },
	{	435,	400 },
	{	480,	350 },
	{	549,	300 },
	{	626,	250 },
	{	711,	200 },
	{	803,	150 },
	{	903,	100 },
	{	1033,	50	},
	{	1144,	0	},
	{	1230,	-50 },
	{	1338,	-100	},
	{	1439,	-150	},
	{	1536,	-200	},
	{	1626,	-250	},
};

/* ADC region should be exclusive */
static sec_bat_adc_region_t cable_adc_value_table[] = {
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
};

static int polling_time_table[] = {
	10,	/* BASIC */
	30,	/* CHARGING */
	30,	/* DISCHARGING */
	30,	/* NOT_CHARGING */
	3600,	/* SLEEP */
};

int Temperature_fn(void)
{
	return 0;
}

static struct battery_data_t stc3117_battery_data[] = {
	{
		.Vmode= 0,		 /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
		.Alm_SOC = 1,		/* SOC alm level %*/
		.Alm_Vbat = 3400,	/* Vbat alm level mV*/
		.CC_cnf = 823,		/* nominal CC_cnf, coming from battery characterisation*/
		.VM_cnf = 632,		/* nominal VM cnf , coming from battery characterisation*/
		.Rint = 155,		/* nominal internal impedance*/
		.Cnom = 4000,		/* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,		/* sense resistor mOhms*/
		.RelaxCurrent = 150, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,	   /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

		.CapDerating[6] = 600,				/* capacity derating in 0.1%, for temp = -20C */
		.CapDerating[5] = 300,				/* capacity derating in 0.1%, for temp = -10C */
		.CapDerating[4] = 80,			   /* capacity derating in 0.1%, for temp = 0C */
		.CapDerating[3] = 20,			   /* capacity derating in 0.1%, for temp = 10C */
		.CapDerating[2] = 0,			  /* capacity derating in 0.1%, for temp = 25C */
		.CapDerating[1] = 0,			  /* capacity derating in 0.1%, for temp = 40C */
		.CapDerating[0] = 0,			  /* capacity derating in 0.1%, for temp = 60C */

		.OCVValue[15] = 4312,			 /* OCV curve adjustment */
		.OCVValue[14] = 4187,			 /* OCV curve adjustment */
		.OCVValue[13] = 4089,			 /* OCV curve adjustment */
		.OCVValue[12] = 3990,			 /* OCV curve adjustment */
		.OCVValue[11] = 3959,			 /* OCV curve adjustment */
		.OCVValue[10] = 3906,			 /* OCV curve adjustment */
		.OCVValue[9] = 3839,			 /* OCV curve adjustment */
		.OCVValue[8] = 3801,			 /* OCV curve adjustment */
		.OCVValue[7] = 3772,			 /* OCV curve adjustment */
		.OCVValue[6] = 3756,			 /* OCV curve adjustment */
		.OCVValue[5] = 3738,			 /* OCV curve adjustment */
		.OCVValue[4] = 3707,			 /* OCV curve adjustment */
		.OCVValue[3] = 3685,			 /* OCV curve adjustment */
		.OCVValue[2] = 3681,			 /* OCV curve adjustment */
		.OCVValue[1] = 3629,			 /* OCV curve adjustment */
		.OCVValue[0] = 3400,			 /* OCV curve adjustment */

		/*if the application temperature data is preferred than the STC3117 temperature*/
		.ExternalTemperature = Temperature_fn, /*External temperature fonction, return C*/
		.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3117 temperature */
	}
};

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
#if 0
	.get_cable_from_extended_cable_type =
		sec_bat_get_cable_from_extended_cable_type,
#endif
	.cable_switch_check = sec_bat_switch_to_check,
	.cable_switch_normal = sec_bat_switch_to_normal,
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

	.adc_check_count = 6,
	.adc_type = {
		SEC_BATTERY_ADC_TYPE_NONE,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_NONE,	/* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_NONE,	/* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_data = (void *)stc3117_battery_data,
	.bat_gpio_ta_nconnected = 0,
	.bat_polarity_ta_nconnected = 1,/* active HIGH */
	.bat_irq = 0,
	.bat_irq_attr = IRQF_TRIGGER_FALLING | IRQF_EARLY_RESUME,
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE |
		SEC_BATTERY_CABLE_CHECK_INT,
	.cable_source_type =
		SEC_BATTERY_CABLE_SOURCE_EXTERNAL,

	.event_check = true,
	.event_waiting_time = 600,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_NONE,
	.check_count = 0,
	/* Battery check by ADC */
	.check_adc_max = 0,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGINT,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	//.thermal_source = SEC_BATTERY_THERMAL_SOURCE_FG,
	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_adc_table = temp_table,
	.temp_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = temp_table,
	.temp_amb_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_check_count = 1,

#ifdef CONFIG_MACH_DEGAS_CHN_TW
	.temp_highlimit_threshold_event = 800,
	.temp_highlimit_recovery_event = 750,
	.temp_highlimit_threshold_normal = 800,
	.temp_highlimit_recovery_normal = 750,
	.temp_highlimit_threshold_lpm = 800,
	.temp_highlimit_recovery_lpm = 750,
	.temp_high_threshold_event = 540,
	.temp_high_recovery_event = 480,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 20,
	.temp_high_threshold_normal = 540,
	.temp_high_recovery_normal = 480,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 20,
	.temp_high_threshold_lpm = 540,
	.temp_high_recovery_lpm = 480,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 20,
#else
	.temp_highlimit_threshold_event = 800,
	.temp_highlimit_recovery_event = 750,
	.temp_highlimit_threshold_normal = 800,
	.temp_highlimit_recovery_normal = 750,
	.temp_highlimit_threshold_lpm = 800,
	.temp_highlimit_recovery_lpm = 750,
	.temp_high_threshold_event = 540,
	.temp_high_recovery_event = 480,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 540,
	.temp_high_recovery_normal = 480,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 540,
	.temp_high_recovery_lpm = 480,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 0,
#endif

	.full_check_type = SEC_BATTERY_FULLCHARGED_FG_CURRENT,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_TIME,
	.full_check_count = 2,
	.chg_gpio_full_check = 0,
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
		SEC_BATTERY_FULL_CONDITION_VCELL,
	.full_condition_soc = 100,
	.full_condition_vcell = 4300,

	.recharge_check_count = 2,
	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_VCELL,
	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4150,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time = 90 * 60,
	.charging_reset_time = 0,

	/* Fuel Gauge */
	.fg_irq = LOW_BAT_DETECT,
	.fg_irq_attr =
		IRQF_TRIGGER_FALLING,
	.fuel_alert_soc = 1,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_RAW |
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL,
		/*SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC, */
	.capacity_max = 1000,
	.capacity_max_margin = 30,
	.capacity_min = 0,

	/* Charger */
	.charger_name = "sec-charger",
	.chg_gpio_en = GPIO_CHG_EN,
	.chg_polarity_en = 0,
	.chg_gpio_status = 0,
	.chg_polarity_status = 0,
	.chg_irq = 0,
	.chg_irq_attr = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
	.chg_float_voltage = 4350,
};

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_battery_pdata,
};

static struct i2c_gpio_platform_data gpio_i2c_data_fg = {
	.sda_pin = GPIO_FG_SDA,
	.scl_pin = GPIO_FG_SCL,
};

static struct i2c_gpio_platform_data gpio_i2c_data_chg = {
	.sda_pin = GPIO_CHG_SDA,
	.scl_pin = GPIO_CHG_SCL,
};

struct platform_device sec_device_fg = {
	.name = "i2c-gpio",
	.id = SEC_FG_I2C_ID,
	.dev.platform_data = &gpio_i2c_data_fg,
};

struct platform_device sec_device_chg = {
	.name = "i2c-gpio",
	.id = SEC_CHG_I2C_ID,
	.dev.platform_data = &gpio_i2c_data_chg,
};

static struct i2c_board_info sec_brdinfo_fg[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
			SEC_FUELGAUGE_I2C_SLAVEADDR),
		.platform_data	= &sec_battery_pdata,
	},
};

static struct i2c_board_info sec_brdinfo_chg[] __initdata = {
	{
		I2C_BOARD_INFO("sec-charger",
			SEC_CHARGER_I2C_SLAVEADDR),
		.platform_data	= &sec_battery_pdata,
	},
};

static struct platform_device *universal3470_battery_devices[] __initdata = {
	&sec_device_chg,
	&sec_device_fg,
	&sec_device_battery,
};

static void charger_gpio_init(void)
{
	sec_battery_pdata.chg_irq = gpio_to_irq(GPIO_CHG_STAT);
	s3c_gpio_cfgpin(GPIO_CHG_STAT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_CHG_STAT, S3C_GPIO_PULL_NONE);

	s3c_gpio_cfgpin(GPIO_CHG_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_CHG_EN, S3C_GPIO_PULL_DOWN);

#if 0
	s3c_gpio_cfgpin(GPIO_CHG_STAT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_CHG_STAT, S3C_GPIO_PULL_NONE);
#endif
}

#if 0
static int __init sec_bat_current_boot_mode(char *mode)
{
	/*
	*	1 is recovery booting
	*	0 is normal booting
	*/

	if (strncmp(mode, "1", 1) == 0)
		sec_bat_recovery_mode = 1;
	else
		sec_bat_recovery_mode = 0;

	pr_info("%s : %s", __func__, sec_bat_recovery_mode == 1 ?
				"recovery" : "normal");

	return 1;
}
__setup("androidboot.batt_check_recovery=", sec_bat_current_boot_mode);
#endif

void __init exynos4_smdk4270_charger_init(void)
{
	/* board dependent changes in booting */
	charger_gpio_init();

	platform_add_devices(
		universal3470_battery_devices,
		ARRAY_SIZE(universal3470_battery_devices));

	i2c_register_board_info(
		SEC_CHG_I2C_ID,
		sec_brdinfo_chg,
		ARRAY_SIZE(sec_brdinfo_chg));

	i2c_register_board_info(
		SEC_FG_I2C_ID,
		sec_brdinfo_fg,
		ARRAY_SIZE(sec_brdinfo_fg));
}

#endif

