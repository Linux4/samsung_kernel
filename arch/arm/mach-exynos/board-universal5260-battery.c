/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
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
#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>

#include <linux/mfd/samsung/core.h>

#include "board-universal5260.h"

#include "common.h"


#if defined(CONFIG_S3C_ADC)
#include <plat/adc.h>
#endif

#if defined(CONFIG_BATTERY_SAMSUNG)
#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/battery/charger/max77803_charger.h>

#include <linux/regulator/consumer.h>

#define SEC_BATTERY_PMIC_NAME ""

static struct s3c_adc_client *adc_client;

/* static unsigned int sec_bat_recovery_mode; */

bool is_wpc_cable_attached;
extern int system_rev;
bool is_ovlo_state;

/* For TEMP*/
#define	GPIO_FUEL_ALERT			EXYNOS5260_GPX1(5)
#define	GPIO_FUEL_SCL_18V		EXYNOS5260_GPB5(3)
#define	GPIO_FUEL_SDA_18V		EXYNOS5260_GPB5(2)

unsigned int lpcharge;
EXPORT_SYMBOL(lpcharge);
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
static sec_charging_current_t charging_current_table[] = {
	{1500,	1640,	200,	40 * 60},
	{0, 0,	0,	0},
	{0, 0,	0,	0},
	{1500,	1640,	200,	40*60},
	{460,	460,	200,	40*60},
	{900,	1200,	200,	40*60},
	{1000,	1000,	200,	40*60},
	{460,	460,	200,	40*60},
	{1000,	1200,	200,	40*60},
	{0, 0,	0,	0},
	{650,	750,	200,	40*60},
	{1500,	1640,	200,	40*60},
	{0, 0,	0,	0},
	{0, 0,	0,	0},
	{0, 0,	0,	0},/*lan hub*/
	{460,	460,	200,	40*60},/*mhl usb*/
	{0,	0,	0,	0},/*power sharing*/
};
#else
static sec_charging_current_t charging_current_table[] = {
	{1800,	2100,	200,	40 * 60},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{1800,	2100,	200,	40*60},
	{460,	460,	200,	40*60},
	{900,	1200,	200,	40*60},
	{1000,	1000,	200,	40*60},
	{460,	460,	200,	40*60},
	{1000,	1200,	200,	40*60},
	{0,	0,	0,	0},
	{650,	750,	200,	40*60},
	{1900,	1600,	200,	40*60},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
	{0, 0,	0,	0},/*lan hub*/
	{460,	460,	200,	40*60},/*mhl_usb*/
	{0,	0,	0,	0},/*power sharing*/
};
#endif
static bool sec_bat_adc_none_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_none_exit(void) {return true; }
static int sec_bat_adc_none_read(unsigned int channel) {return 0; }

static bool sec_bat_adc_ap_init(struct platform_device *pdev)
{
	adc_client = s3c_adc_register(pdev, NULL, NULL, 0);
	return true;

}
static bool sec_bat_adc_ap_exit(void)
{
	s3c_adc_release(adc_client);
	return true;
}
static int sec_bat_adc_ap_read(unsigned int channel)
{
	int data = -1;

	switch (channel) {
	case SEC_BAT_ADC_CHANNEL_BAT_CHECK:
		data = s3c_adc_read(adc_client, 1);
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP:
		data = s3c_adc_read(adc_client, 4);
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		data = 1300;
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
	s3c_gpio_cfgpin(GPIO_FUEL_ALERT, S3C_GPIO_INPUT);

	return true;
}

#if 0
static bool sec_chg_gpio_init(void)
{
	s3c_gpio_cfgpin(GPIO_WPC_INT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_WPC_INT, S3C_GPIO_PULL_NONE);
	return true;
}
#endif

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
			value.intval = current_cable_type<<ONLINE_TYPE_MAIN_SHIFT;
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		}
	} else {
		psy_do_property("sec-charger", get,
				POWER_SUPPLY_PROP_ONLINE, value);
		if (value.intval == POWER_SUPPLY_TYPE_WPC) {
			value.intval =
				POWER_SUPPLY_TYPE_WPC<<ONLINE_TYPE_MAIN_SHIFT;
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

static bool sec_bat_check_jig_status(void)
{
	if (current_cable_type == POWER_SUPPLY_TYPE_UARTOFF)
		return true;
	else
		return false;

}


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
				cable_type = POWER_SUPPLY_TYPE_MHL_USB;
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

	if (cable_type == POWER_SUPPLY_TYPE_WPC)
		is_wpc_cable_attached = true;
	else
		is_wpc_cable_attached = false;

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

#define ADC_REG_NAME	"VCC_1.8V_VF"
static int sec_bat_set_adc_power(bool en)
{
	struct regulator *regulator;
	int ret = 0;
	pr_info("%s(%d)\n", __func__, en);

	regulator = regulator_get(NULL, ADC_REG_NAME);
	if (IS_ERR(regulator))
		return -ENODEV;

	if (en) {
		ret = regulator_enable(regulator);
		udelay(100);
	} else {
		ret = regulator_disable(regulator);
	}
	pr_info("%s: %s: en(%d) ret(%d)\n", __func__, ADC_REG_NAME, en, ret);

	regulator_put(regulator);

	return ret;
}

static bool sec_bat_check_cable_result_callback(
				int cable_type)
{
	current_cable_type = cable_type;

	if (sec_battery_pdata.battery_check_type == SEC_BATTERY_CHECK_ADC) {
		if (cable_type == POWER_SUPPLY_TYPE_BATTERY)
			sec_bat_set_adc_power(0);
		else
			sec_bat_set_adc_power(1);
	}

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
	struct power_supply *psy;
	union power_supply_propval value;

	psy = get_power_supply_by_name(("sec-charger"));
	if (!psy) {
		pr_err("%s: Fail to get psy (%s)\n",
			__func__, "sec_charger");
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

static bool sec_bat_ovp_uvlo_result_callback(int health) {
	union power_supply_propval value;

	psy_do_property("sec-charger", get,
				POWER_SUPPLY_PROP_HEALTH, value);

	if (value.intval  == POWER_SUPPLY_HEALTH_OVERVOLTAGE) {
		is_ovlo_state = true;
	} else {
		is_ovlo_state = false;
	}

	return true;
}

/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val) {return true; }
static bool sec_fg_fuelalert_process(bool is_fuel_alerted) {return true; }

#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
static const sec_bat_adc_table_data_t temp_table[] = {
	{	303,	900 },
	{	350,	850 },
	{	423,	800 },
	{	490,	750 },
	{	567,	700 },
	{	661,	650 },
	{	764,	600 },
	{	885,	550 },
	{	1032,	500 },
	{	1186,	450 },
	{	1367,	400 },
	{	1578,	350 },
	{	1784,	300 },
	{	2007,	250 },
	{	2238,	200 },
	{	2475,	150 },
	{	2693,	100 },
	{	2907,	50	},
	{	3104,	0	},
	{	3283,	-50 },
	{	3427,	-100	},
	{	3553,	-150	},
	{	3653,	-200	},
};
#else/*h-lite*/
static const sec_bat_adc_table_data_t temp_table[] = {
	{  497,	 700 },
	{  578,	 650 },
	{  622,	 620 },
	{  632,	 610 },
	{  651,	 600 },
	{  670,	 590 },
	{  688,	 580 },
	{  754,	 550 },
	{  839,	 500 },
	{  949,	 450 },
	{ 1005,	 420 },
	{ 1029,	 410 },
	{ 1050,	 400 },
	{ 1070,	 390 },
	{ 1094,	 380 },
	{ 1162,	 350 },
	{ 1269,	 300 },
	{ 1382,	 250 },
	{ 1476,	 200 },
	{ 1565,	 150 },
	{ 1644,	 100 },
	{ 1721,	 50 },
	{ 1762,	 20 },
	{ 1774,	 10 },
	{ 1785,	   0 },
	{ 1797,  -10 },
	{ 1809,  -20 },
	{ 1821,  -30 },
	{ 1830,  -40 },
	{ 1840,  -50 },
	{ 1850,  -60 },
	{ 1858,  -70 },
	{ 1883,  -100 },
	{ 1920,  -150 },
	{ 1945,  -200 },
};

/* for h/w rev 0.2 */
static const sec_bat_adc_table_data_t temp_table2[] = {
	{ 294, 900 },
	{ 304, 890 },
	{ 314, 880 },
	{ 325, 870 },
	{ 335, 860 },
	{ 345, 850 },
	{ 357, 840 },
	{ 369, 830 },
	{ 382, 820 },
	{ 391, 810 },
	{ 406, 800 },
	{ 420, 790 },
	{ 434, 780 },
	{ 449, 770 },
	{ 463, 760 },
	{ 477, 750 },
	{ 494, 740 },
	{ 511, 730 },
	{ 527, 720 },
	{ 544, 710 },
	{ 561, 700 },
	{ 580, 690 },
	{ 599, 680 },
	{ 619, 670 },
	{ 638, 660 },
	{ 657, 650 },
	{ 679, 640 },
	{ 700, 630 },
	{ 722, 620 },
	{ 743, 610 },
	{ 765, 600 },
	{ 791, 590 },
	{ 816, 580 },
	{ 842, 570 },
	{ 867, 560 },
	{ 893, 550 },
	{ 922, 540 },
	{ 951, 530 },
	{ 979, 520 },
	{ 1008, 510 },
	{ 1037, 500 },
	{ 1070, 490 },
	{ 1103, 480 },
	{ 1136, 470 },
	{ 1168, 460 },
	{ 1201, 450 },
	{ 1236, 440 },
	{ 1272, 430 },
	{ 1308, 420 },
	{ 1344, 410 },
	{ 1382, 400 },
	{ 1408, 390 },
	{ 1452, 380 },
	{ 1495, 370 },
	{ 1539, 360 },
	{ 1583, 350 },
	{ 1628, 340 },
	{ 1670, 330 },
	{ 1713, 320 },
	{ 1757, 310 },
	{ 1803, 300 },
	{ 1843, 290 },
	{ 1885, 280 },
	{ 1937, 270 },
	{ 1984, 260 },
	{ 2031, 250 },
	{ 2080, 240 },
	{ 2120, 230 },
	{ 2171, 220 },
	{ 2218, 210 },
	{ 2262, 200 },
	{ 2309, 190 },
	{ 2352, 180 },
	{ 2404, 170 },
	{ 2452, 160 },
	{ 2497, 150 },
	{ 2542, 140 },
	{ 2593, 130 },
	{ 2631, 120 },
	{ 2680, 110 },
	{ 2717, 100 },
	{ 2765, 90 },
	{ 2810, 80 },
	{ 2856, 70 },
	{ 2902, 60 },
	{ 2948, 50 },
	{ 2990, 40 },
	{ 3031, 30 },
	{ 3073, 20 },
	{ 3114, 10 },
	{ 3132, 0 },
	{ 3156, -10 },
	{ 3188, -20 },
	{ 3219, -30 },
	{ 3251, -40 },
	{ 3282, -50 },
	{ 3314, -60 },
	{ 3343, -70 },
	{ 3373, -80 },
	{ 3402, -90 },
	{ 3461, -100 },
	{ 3485, -110 },
	{ 3509, -120 },
	{ 3534, -130 },
	{ 3558, -140 },
	{ 3582, -150 },
	{ 3602, -160 },
	{ 3622, -170 },
	{ 3642, -180 },
	{ 3662, -190 },
	{ 3682, -200 },
};
#endif

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

#if defined(CONFIG_FUELGAUGE_MAX17048)
/* for MAX17048 */
#if defined(CONFIG_MACH_HL3G)
static struct battery_data_t adonis_battery_data[] = {
/* SDI battery data (High voltage 4.35V) */
	{
		.RCOMP0 = 0x83,
		.RCOMP_charging = 0x83,
		.temp_cohot = -1275,
		.temp_cocold = -6400,
		.is_using_model_data = true,
		.type_str = "SDI",
	}
};
#elif defined(CONFIG_MACH_HLLTE)
static struct battery_data_t adonis_battery_data[] = {
/* SDI battery data (High voltage 4.35V) */
	{
		.RCOMP0 = 0x80,
		.RCOMP_charging = 0x80,
		.temp_cohot = -1275,
		.temp_cocold = -6400,
		.is_using_model_data = true,
		.type_str = "SDI",
	}
};
#elif defined(CONFIG_MACH_M2ALTE)
static struct battery_data_t adonis_battery_data[] = {
/* SDI battery data (High voltage 4.35V) */
        {
                .RCOMP0 = 0x80,
                .RCOMP_charging = 0x80,
                .temp_cohot = -1150,
                .temp_cocold = -5950,
                .is_using_model_data = true,
                .type_str = "SDI",
        }
};
#elif defined(CONFIG_MACH_M2A3G)
static struct battery_data_t adonis_battery_data[] = {
/* SDI battery data (High voltage 4.35V) */
	{
		.RCOMP0 = 0x7D,
		.RCOMP_charging = 0x80,
		.temp_cohot = -1150,
		.temp_cocold = -5950,
		.is_using_model_data = true,
		.type_str = "SDI",
	}
};
#else
static struct battery_data_t adonis_battery_data[] = {
/* SDI battery data (High voltage 4.35V) */
	{
		.RCOMP0 = 0x70,
		.RCOMP_charging = 0x70,
		.temp_cohot = -1275,
		.temp_cocold = -6400,
		.is_using_model_data = true,
		.type_str = "SDI",
	}
};
#endif
#endif
sec_battery_platform_data_t sec_battery_pdata = {
	/* NO NEED TO BE CHANGED */
	.initial_check = sec_bat_initial_check,
	.bat_gpio_init = sec_bat_gpio_init,
	.fg_gpio_init = sec_fg_gpio_init,
//	.chg_gpio_init = sec_chg_gpio_init,

	.is_lpm = sec_bat_is_lpm,
	.check_jig_status = sec_bat_check_jig_status,
	.check_cable_callback =
		sec_bat_check_cable_callback,
	.get_cable_from_extended_cable_type =
		sec_bat_get_cable_from_extended_cable_type,
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
		SEC_BATTERY_ADC_TYPE_AP,	/* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_AP,	/* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_data = (void *)adonis_battery_data,
	.bat_gpio_ta_nconnected = 0,
	.bat_polarity_ta_nconnected = 0,
	.bat_irq = IRQ_BOARD_IFIC_START + MAX77803_CHG_IRQ_BATP_I,
	.bat_irq_attr = IRQF_TRIGGER_FALLING | IRQF_EARLY_RESUME,
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_PSY,
	.cable_source_type =
		SEC_BATTERY_CABLE_SOURCE_EXTERNAL |
		SEC_BATTERY_CABLE_SOURCE_EXTENDED,

	.event_check = false,
	.event_waiting_time = 600,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 0,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_INT,
	.check_count = 0,
	/* Battery check by ADC */
	.check_adc_max = 2000,//temporal range
	.check_adc_min = 100,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	.temp_adc_table = temp_table,
	.temp_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = temp_table,
	.temp_amb_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),

	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 1,

#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
	.temp_high_threshold_event = 700,
	.temp_high_recovery_event = 490,
	.temp_low_threshold_event = -30,
	.temp_low_recovery_event = 15,
	.temp_high_threshold_normal = 600,
	.temp_high_recovery_normal = 490,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 15,
	.temp_high_threshold_lpm = 600,
	.temp_high_recovery_lpm = 490,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 15,
#else
	.temp_high_threshold_event = 700,
	.temp_high_recovery_event = 460,
	.temp_low_threshold_event = -30,
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
	.chg_gpio_full_check = 0,
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
		SEC_BATTERY_FULL_CONDITION_VCELL,
	.full_condition_soc = 97,
	.full_condition_vcell = 4300,

	.recharge_check_count = 2,
	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_VCELL,
	.recharge_condition_soc = 98,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time =  90 * 60,
	.charging_reset_time = 0,

	/* Fuel Gauge */
	.fg_irq = GPIO_FUEL_ALERT,
	.fg_irq_attr =
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
	.fuel_alert_soc = 2,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_RAW |
		SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL,
		/* SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC, */
#if defined(CONFIG_MACH_M2ALTE_KOR_SKT) || defined(CONFIG_MACH_M2ALTE_KOR_KTT) || defined(CONFIG_MACH_M2ALTE_KOR_LGT)
	.capacity_max = 1000,
	.capacity_max_margin = 30,
	.capacity_min = 5,
#else
	.capacity_max = 1000,
	.capacity_max_margin = 30,
	.capacity_min = -7,
#endif
	/* .get_fg_current = false, */

	/* Charger */
	.charger_name = "sec-charger",
	.chg_gpio_en = 0,
	.chg_polarity_en = 0,
	.chg_gpio_status = 0,
	.chg_polarity_status = 0,
	.chg_irq = 0,
	.chg_irq_attr = IRQF_TRIGGER_FALLING,
	.chg_float_voltage = 4350,
};

#define SEC_FG_I2C_ID	9

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_battery_pdata,
};

static struct i2c_gpio_platform_data gpio_i2c_data_fg = {
	.sda_pin = GPIO_FUEL_SDA_18V,
	.scl_pin = GPIO_FUEL_SCL_18V,
};

struct platform_device sec_device_fg = {
	.name = "i2c-gpio",
	.id = SEC_FG_I2C_ID,
	.dev.platform_data = &gpio_i2c_data_fg,
};

static struct i2c_board_info sec_brdinfo_fg[] __initdata = {
#if !defined(CONFIG_MFD_MAX77803)
	{
		I2C_BOARD_INFO("sec-charger",
			SEC_CHARGER_I2C_SLAVEADDR),
		.platform_data	= &sec_battery_pdata,
	},
#endif
	{
		I2C_BOARD_INFO("sec-fuelgauge",
			SEC_FUELGAUGE_I2C_SLAVEADDR),
		.platform_data	= &sec_battery_pdata,
	},
};

static struct platform_device *universal5260_battery_devices[] __initdata = {
	&s3c_device_i2c5,
	&sec_device_battery,
};

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

void __init exynos5_universal5260_battery_init(void)
{
	/* board dependent changes in booting */
#if !defined(CONFIG_MACH_M2ALTE) && !defined(CONFIG_MACH_M2A3G)
	if (system_rev > 6) {
		sec_battery_pdata.temp_adc_table = temp_table2;
		sec_battery_pdata.temp_adc_table_size =
			sizeof(temp_table2)/sizeof(sec_bat_adc_table_data_t),
		sec_battery_pdata.temp_amb_adc_table = temp_table2;
		sec_battery_pdata.temp_amb_adc_table_size =
			sizeof(temp_table2)/sizeof(sec_bat_adc_table_data_t);
	}
#endif

#if defined(CONFIG_TARGET_LOCALE_USA) && defined(CONFIG_MACH_M2ALTE)
	sec_battery_pdata.battery_check_type = SEC_BATTERY_CHECK_ADC;
#endif

	s3c_i2c5_set_platdata(NULL);
	platform_add_devices(
		universal5260_battery_devices,
		ARRAY_SIZE(universal5260_battery_devices));

	i2c_register_board_info(
		SEC_FG_I2C_ID,
		sec_brdinfo_fg,
		ARRAY_SIZE(sec_brdinfo_fg));
}

#endif

