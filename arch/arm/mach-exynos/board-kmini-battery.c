/*
 * Copyright (C) 2013 Samsung Electronics Co, Ltd.
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
#include <plat/adc.h>
#include <linux/gpio_event.h>
#include <plat/gpio-cfg.h>
#include "board-universal222ap.h"
#include <asm/system_info.h>

#ifdef CONFIG_BATTERY_SAMSUNG
#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#endif

#if defined(CONFIG_CHARGER_RT5033)
#include <linux/battery/charger/rt5033_charger.h>
#endif

#if defined(CONFIG_FUELGAUGE_RT5033)
#include <linux/battery/fuelgauge/rt5033_fuelgauge.h>
#include <linux/battery/fuelgauge/rt5033_fuelgauge_impl.h>
#include <linux/mfd/rt5033.h>
#endif

#if defined(CONFIG_SM5502_MUIC)
#include <mach/sm5502-muic.h>
#endif

#define SEC_BATTERY_PMIC_NAME ""

#define FG_ID           15
#define GPIO_FG_SCL     EXYNOS4_GPF1(5)
#define GPIO_FG_SDA     EXYNOS4_GPF0(7)

#ifdef CONFIG_RT5033_SADDR
#define RT5033FG_SLAVE_ADDR_MSB (0x40)
#else
#define RT5033FG_SLAVE_ADDR_MSB (0x00)
#endif

#define RT5033FG_SLAVE_ADDR (0x35|RT5033FG_SLAVE_ADDR_MSB)

#define GPIO_FUEL_ALERT		EXYNOS4_GPX2(3)
#define GPIO_IF_PMIC_INT	EXYNOS4_GPX0(3)

/* cable state */
unsigned int lpcharge;
static struct s3c_adc_client *temp_adc_client;
extern int current_cable_type;
extern bool is_cable_attached;

static sec_bat_adc_table_data_t kmini_temp_table_01[] = {
	{145,	800},
	{149,	790},
	{153,	780},
	{157,	770},
	{162,	760},
	{166,	750},
	{171,	740},
	{176,	730},
	{181,	720},
	{186,	710},
	{191,	700},
	{197,	690},
	{202,	680},
	{208,	670},
	{214,	660},
	{220,	650},
	{227,	640},
	{233,	630},
	{240,	620},
	{247,	610},
	{255,	600},
	{262,	590},
	{270,	580},
	{278,	570},
	{287,	560},
	{295,	550},
	{304,	540},
	{314,	530},
	{323,	520},
	{333,	510},
	{343,	500},
	{354,	490},
	{365,	480},
	{376,	470},
	{388,	460},
	{400,	450},
	{412,	440},
	{425,	430},
	{438,	420},
	{452,	410},
	{466,	400},
	{480,	390},
	{495,	380},
	{511,	370},
	{527,	360},
	{543,	350},
	{560,	340},
	{577,	330},
	{595,	320},
	{614,	310},
	{633,	300},
	{653,	290},
	{673,	280},
	{695,	270},
	{716,	260},
	{738,	250},
	{761,	240},
	{785,	230},
	{809,	220},
	{834,	210},
	{860,	200},
	{886,	190},
	{913,	180},
	{941,	170},
	{970,	160},
	{999,	150},
	{1030,	140},
	{1061,	130},
	{1092,	120},
	{1125,	110},
	{1158,	100},
	{1192,	90},
	{1227,	80},
	{1263,	70},
	{1299,	60},
	{1336,	50},
	{1374,	40},
	{1413,	30},
	{1453,	20},
	{1493,	10},
	{1534,	0},
	{1575,	-10},
	{1618,	-20},
	{1661,	-30},
	{1705,	-40},
	{1749,	-50},
	{1794,	-60},
	{1840,	-70},
	{1886,	-80},
	{1932,	-90},
	{1979,	-100},
	{2026,	-110},
	{2073,	-120},
	{2121,	-130},
	{2169,	-140},
	{2217,	-150},
	{2265,	-160},
	{2313,	-170},
	{2361,	-180},
	{2410,	-190},
	{2458,	-200},
	{2506,	-210},
	{2554,	-220},
	{2601,	-230},
	{2649,	-240},
	{2696,	-250},
};

static sec_bat_adc_table_data_t kmini_temp_table_02[] = {
	{483,	750},
	{799,	600},
	{1200,	460},
	{2035,	250},
	{2863,   50},
	{3045,	  0},
	{3201,  -50},
	{3447,	-150},
	{3494,	-190},
};

//static struct power_supply *charger_supply;
extern bool is_jig_on;

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
//TEMP_TEST - check if this code has to be entered or not
/*	int ret;

	ret = gpio_request(mfp_to_gpio(GPIO098_GPIO_98), "bq24157_CD");
	if(ret)
		return false;

	ret = gpio_request(mfp_to_gpio(GPIO008_GPIO_8), "charger-irq");
	if(ret)
		return false;

	gpio_direction_input(mfp_to_gpio(GPIO008_GPIO_8));
*/
	return true;
}

/* Get LP charging mode state */
unsigned int lpcharge;
EXPORT_SYMBOL(lpcharge);

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

static int sec_bat_check_cable_callback(void)
{
	int ta_nconnected = 0;
	union power_supply_propval value;
	struct power_supply *psy_charger =
				power_supply_get_by_name("sec-charger");
	int cable_type = POWER_SUPPLY_TYPE_BATTERY;

	msleep(300);
	if (psy_charger) {
		psy_charger->get_property(psy_charger,
				POWER_SUPPLY_PROP_CHARGE_NOW, &value);
		ta_nconnected = value.intval;

		pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);
	}

	if (current_cable_type == POWER_SUPPLY_TYPE_MISC) {
		cable_type = !ta_nconnected ?
				POWER_SUPPLY_TYPE_BATTERY : POWER_SUPPLY_TYPE_MISC;
		pr_info("%s: current cable : MISC, type : %s\n", __func__,
				!ta_nconnected ? "BATTERY" : "MISC");
	} else if (current_cable_type == POWER_SUPPLY_TYPE_UARTOFF) {
		cable_type = !ta_nconnected ?
				POWER_SUPPLY_TYPE_BATTERY : POWER_SUPPLY_TYPE_UARTOFF;
		pr_info("%s: current cable : UARTOFF, type : %s\n", __func__,
				!ta_nconnected ? "BATTERY" : "UARTOFF");
	} else {
		cable_type = current_cable_type;
	}

	return cable_type;
}

static void sec_bat_initial_check(void)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	pr_info("%s: init cable type : %d\n", __func__, current_cable_type);
	if (!psy || !psy->set_property)
		pr_err("%s: fail to get battery psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}
}

static bool sec_bat_check_cable_result_callback(int cable_type)
{
	bool ret = true;

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

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
static bool sec_bat_check_callback(void) { return true; }
static bool sec_bat_check_result_callback(void) { return true; }

static data_point_t vgcomp_normal[] = {
	{{3200,},{-1 ,},{{27  ,8  ,15 ,115,},},},   /*SOC<0%		   & T<0oC	*/
	{{3765,},{-1 ,},{{22  ,4  ,15 ,115,},},},   /*0<=SOC<25%  & T<0oC	*/
	{{3853,},{-1 ,},{{27  ,5  ,21 ,115,},},},   /*25<=SOC<50%  & T<0oC	*/
	{{3926,},{-1 ,},{{27  ,7  ,21 ,115,},},},   /*50<=SOC<60%  & T<0oC	*/
	{{4218,},{-1 ,},{{35  ,25 ,15 ,115,},},},   /*60<=SOC  & T<0oC	*/
	{{3200,},{0  ,},{{100 ,43 ,55 ,115,},},},   /*SOC<0%		   & 0oC<T<50oC  */
	{{3765,},{0  ,},{{81  ,20 ,55 ,115,},},},   /*0<=SOC<25%  & 0oC<T<50oC  */
	{{3853,},{0  ,},{{100 ,26 ,77 ,115,},},},   /*25<=SOC<50%  & 0oC<T<50oC  */
	{{3926,},{0  ,},{{100 ,35 ,77 ,115,},},},   /*50<=SOC<60%  & 0oC<T<50oC  */
	{{4218,},{0  ,},{{130 ,130,55 ,115,},},},   /*60<=SOC<90%  & 0oC<T<50oC  */
	{{3200,},{500,},{{100 ,43 ,105,65 ,},},},   /*SOC<0%		   & T>50oC  */
	{{3765,},{500,},{{81  ,20 ,105,65 ,},},},   /*0<=SOC<25%  & T>50oC  */
	{{3853,},{500,},{{100 ,26 ,147,65 ,},},},   /*25<=SOC<50%  & T>50oC  */
	{{3926,},{500,},{{100 ,35 ,147,65 ,},},},   /*50<=SOC<60%  & T>50oC  */
	{{4218,},{500,},{{130 ,130,105,65 ,},},},   /*60<=SOC   & T>50oC  */

};

static data_point_t vgcomp_idle[] = {
	{{3200,},{-1 ,},{{27  ,8  ,15 ,115,},},},   /*SOC<0%		   & T<0oC	*/
	{{3765,},{-1 ,},{{22  ,4  ,15 ,115,},},},   /*0<=SOC<25%  & T<0oC	*/
	{{3853,},{-1 ,},{{27  ,5  ,21 ,115,},},},   /*25<=SOC<50%  & T<0oC	*/
	{{3926,},{-1 ,},{{27  ,7  ,21 ,115,},},},   /*50<=SOC<60%  & T<0oC	*/
	{{4218,},{-1 ,},{{35  ,25 ,15 ,115,},},},   /*60<=SOC  & T<0oC	*/
	{{3200,},{0  ,},{{100 ,43 ,55 ,115,},},},   /*SOC<0%		   & 0oC<T<50oC  */
	{{3765,},{0  ,},{{81  ,20 ,55 ,115,},},},   /*0<=SOC<25%  & 0oC<T<50oC  */
	{{3853,},{0  ,},{{100 ,26 ,77 ,115,},},},   /*25<=SOC<50%  & 0oC<T<50oC  */
	{{3926,},{0  ,},{{100 ,35 ,77 ,115,},},},   /*50<=SOC<60%  & 0oC<T<50oC  */
	{{4218,},{0  ,},{{130 ,130,55 ,115,},},},   /*60<=SOC   & 0oC<T<50oC  */
	{{3200,},{500,},{{100 ,43 ,105,65 ,},},},   /*SOC<0%		   & T>50oC  */
	{{3765,},{500,},{{81  ,20 ,105,65 ,},},},   /*0<=SOC<25%  & T>50oC  */
	{{3853,},{500,},{{100 ,26 ,147,65 ,},},},   /*25<=SOC<50%  & T>50oC  */
	{{3926,},{500,},{{100 ,35 ,147,65 ,},},},   /*50<=SOC<60%  & T>50oC  */
	{{4218,},{500,},{{130 ,130,105,65 ,},},},   /*60<=SOC   & T>50oC  */

};

static data_point_t offset_low_power[] = {
    {{0,}, {250,}, {{-12,},},},
    {{15,}, {250,}, {{-5,},},},
    {{19,}, {250,}, {{-9,},},},
    {{20,}, {250,}, {{-10,},},},
    {{100,}, {250,}, {{0,},},},
    {{500,}, {250,}, {{0,},},},
    {{1030,}, {250,}, {{-12,},},},
};

static data_point_t offset_charging[] = {
    {{0,}, {250,}, {{0,},},},
    {{1030,}, {250,}, {{0,},},},
};

static data_point_t offset_discharging[] = {
    {{0,}, {250,}, {{0,},},},
    {{1030,}, {250,}, {{0,},},},
};

static struct battery_data_t rt5033_comp_data[] =
{
	{
	    .vg_comp_interpolation_order = {1,1},
	    .vg_comp = {
	        [VGCOMP_NORMAL] = {
	            .voltNR = 5,
	            .tempNR = 3,
	            .vg_comp_data = vgcomp_normal,
	        },
	        [VGCOMP_IDLE] = {
	            .voltNR = 5,
	            .tempNR = 3,
	            .vg_comp_data = vgcomp_idle,
	        },
	    },
	    .offset_interpolation_order = {2,2},
	    .soc_offset = {
	        [OFFSET_LOW_POWER] = {
	            .soc_voltNR = 7,
	            .tempNR = 1,
	            .soc_offset_data = offset_low_power,

	        },
	        [OFFSET_CHARGING] = {
	            .soc_voltNR = 2,
	            .tempNR = 1,
	            .soc_offset_data = offset_charging,

	        },
	        [OFFSET_DISCHARGING] = {
	            .soc_voltNR = 2,
	            .tempNR = 1,
	            .soc_offset_data = offset_discharging,

	        },
	    },
	    .crate_idle_thres = 255,
	    .battery_type = 4350,
	},
};

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

/* assume that system's average current consumption is 150mA */
/* We will suggest use 250mA as EOC setting */
/* AICR of RT5033 is 100, 500, 700, 900, 1000, 1500, 2000, we suggest to use 500mA for USB */
sec_charging_current_t charging_current_table[] = {
	{0,     0,      0,	0},           /* POWER_SUPPLY_TYPE_UNKNOWN */
	{0,     0,      0,	0},           /* POWER_SUPPLY_TYPE_BATTERY */
	{0,     0,      0,	0},	      /* POWER_SUPPLY_TYPE_UPS */
	{1000,  1500,  150,	40 * 60},     /* POWER_SUPPLY_TYPE_MAINS */
	{500,   500,   150,	40 * 60},     /* POWER_SUPPLY_TYPE_USB */
	{500,   500,   150,	40 * 60},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{500,   500,   150,	40 * 60},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{500,   500,   150,	40 * 60},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_BMS*/
	{500,   500,   150,    	40 * 60},     /* POWER_SUPPLY_TYPE_MISC */
	{1000,	1500,  150,    	40 * 60},     /* POWER_SUPPLY_TYPE_WIRELESS */
	{0,     0,      0,	0},           /* POWER_SUPPLY_TYPE_DOCK */
	{1000,  1500,  150,	40 * 60},     /* POWER_SUPPLY_TYPE_UARTOFF */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_OTG */
	{0,	0,	0,	0},           /* POWER_SUPPLY_TYPE_LAN_HUB */
};

/* unit: seconds */
static int polling_time_table[] = {
	10,     /* BASIC */
	30,     /* CHARGING */
	30,     /* DISCHARGING */
	30,     /* NOT_CHARGING */
	1800,    /* SLEEP */
};

static bool sec_bat_adc_none_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_none_exit(void) { return true; }
static int sec_bat_adc_none_read(unsigned int channel) { return 0; }

static bool sec_bat_adc_ap_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ap_exit(void) { return true; }
static int sec_bat_adc_ap_read(unsigned int channel)
{
	int i;
	int ret = 0;

	ret = s3c_adc_read(temp_adc_client, 1);

	if (ret == -ETIMEDOUT) {
		for (i = 0; i < 5; i++) {
			msleep(20);
			ret = s3c_adc_read(temp_adc_client, 1);
			if (ret > 0)
				break;
		}

		if (i >= 5)
			pr_err("%s: Retry count exceeded\n", __func__);

	} else if (ret < 0) {
		pr_err("%s: Failed read adc value : %d\n",
		__func__, ret);
	}

	pr_debug("%s: temp acd : %d\n", __func__, ret);
	return ret;
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

	.adc_check_count = 5,

	.adc_type = {
		SEC_BATTERY_ADC_TYPE_NONE,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_NONE,	/* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_NONE,      /* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.battery_data = (void *)rt5033_comp_data,
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.bat_polarity_ta_nconnected = 1,        /* active HIGH */
	.bat_irq_attr = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_INT,
	.cable_source_type = SEC_BATTERY_CABLE_SOURCE_EXTERNAL,

	.event_check = true,
	.event_waiting_time = 600,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_FUELGAUGE, /* SEC_BATTERY_CHECK_FUELGAUGE,*/
	.check_count = 1,

	/* Battery check by ADC */
	.check_adc_max = 600,
	.check_adc_min = 50,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,

	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 1,
	.temp_highlimit_threshold_event = 800,
	.temp_highlimit_recovery_event = 750,
	.temp_highlimit_threshold_normal = 800,
	.temp_highlimit_recovery_normal = 750,
	.temp_highlimit_threshold_lpm = 800,
	.temp_highlimit_recovery_lpm = 750,
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

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_TIME,
	.full_check_count = 1,
	/* .full_check_adc_1st = 26500, */
	/*.full_check_adc_2nd = 25800, */
	.chg_polarity_full_check = 1,
	.full_condition_type =
		SEC_BATTERY_FULL_CONDITION_SOC |
	        SEC_BATTERY_FULL_CONDITION_VCELL |
	        SEC_BATTERY_FULL_CONDITION_NOTIMEFULL,
	.full_condition_soc = 97,
	.full_condition_ocv = 4350,
	.full_condition_vcell = 4350,

	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_VCELL,
	.recharge_check_count = 1,
	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4350,
	.recharge_condition_vcell = 4350,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time = 90 * 60,
	.charging_reset_time = 0,

	/* Fuel Gauge */
	.fg_irq = GPIO_FUEL_ALERT,
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
	.chg_float_voltage = 4400,
};

static struct i2c_gpio_platform_data tab3_gpio_i2c14_pdata = {
	.sda_pin = (GPIO_FG_SDA),
	.scl_pin = (GPIO_FG_SCL),
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device tab3_gpio_i2c14_device = {
	.name = "i2c-gpio",
	.id = FG_ID,
	.dev = {
		.platform_data = &tab3_gpio_i2c14_pdata,
	},
};

/* set MAX17050 Fuel Gauge gpio i2c */
static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_battery_pdata,
};

static struct i2c_board_info sec_brdinfo_fuelgauge[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
				RT5033FG_SLAVE_ADDR),
		.platform_data = &sec_battery_pdata,
	}
};

static struct platform_device *sec_battery_devices[] __initdata = {
	&tab3_gpio_i2c14_device,
	&sec_device_battery,
};

static void charger_gpio_init(void)
{
	s3c_gpio_setpull(GPIO_IF_PMIC_INT, S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(GPIO_FUEL_ALERT, S3C_GPIO_PULL_UP);
}

void __init exynos4_smdk4270_charger_init(void)
{
	pr_info("%s: KMINI charger init\n", __func__);

	charger_gpio_init();

	if (system_rev < 2) {
		sec_battery_pdata.temp_adc_table = kmini_temp_table_01;
		sec_battery_pdata.temp_adc_table_size = sizeof(kmini_temp_table_01)
			/ sizeof(sec_bat_adc_table_data_t);
		sec_battery_pdata.temp_amb_adc_table = kmini_temp_table_01;
		sec_battery_pdata.temp_amb_adc_table_size = sizeof(kmini_temp_table_01)
			/ sizeof(sec_bat_adc_table_data_t);
	} else {
		sec_battery_pdata.temp_adc_table = kmini_temp_table_02;
		sec_battery_pdata.temp_adc_table_size = sizeof(kmini_temp_table_02)
			/ sizeof(sec_bat_adc_table_data_t);
		sec_battery_pdata.temp_amb_adc_table = kmini_temp_table_02;
		sec_battery_pdata.temp_amb_adc_table_size = sizeof(kmini_temp_table_02)
			/ sizeof(sec_bat_adc_table_data_t);
	}

	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));

	i2c_register_board_info(FG_ID, sec_brdinfo_fuelgauge,
			ARRAY_SIZE(sec_brdinfo_fuelgauge));

	temp_adc_client = s3c_adc_register(&sec_device_battery, NULL, NULL, 0);
}

