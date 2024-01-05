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

#define TA_ADC_LOW              800
#define TA_ADC_HIGH             2200

#define FG_ID           15
#define GPIO_FG_SCL			EXYNOS4_GPM3(0)
#define GPIO_FG_SDA			EXYNOS4_GPM3(1)
#define GPIO_FUEL_ALERT		EXYNOS4_GPX2(3)
#define GPIO_FG_SCL_REV01		EXYNOS4_GPM0(2)
#define GPIO_FG_SDA_REV01		EXYNOS4_GPM1(5)

bool is_wpc_cable_attached;

#ifdef CONFIG_RT5033_SADDR
#define RT5033FG_SLAVE_ADDR_MSB (0x40)
#else
#define RT5033FG_SLAVE_ADDR_MSB (0x00)
#endif

#define RT5033FG_SLAVE_ADDR (0x35|RT5033FG_SLAVE_ADDR_MSB)

/* cable state */
struct s3c_adc_client *temp_adc_client;
extern int current_cable_type;

extern bool is_cable_attached;

static sec_bat_adc_table_data_t mega2_temp_table_01[] = {
	{  204,  800 },
	{  210,  790 },
	{  216,  780 },
	{  223,  770 },
	{  230,  760 },
	{  237,  750 },
	{  244,  740 },
	{  252,  730 },
	{  260,  720 },
	{  268,  710 },
	{  276,  700 },
	{  285,  690 },
	{  294,  680 },
	{  303,  670 },
	{  312,  660 },
	{  321,  650 },
	{  331,  640 },
	{  340,  630 },
	{  351,  620 },
	{  361,  610 },
	{  372,  600 },
	{  383,  590 },
	{  395,  580 },
	{  407,  570 },
	{  419,  560 },
	{  431,  550 },
	{  446,  540 },
	{  457,  530 },
	{  470,  520 },
	{  485,  510 },
	{  500,  500 },
	{  514,  490 },
	{  529,  480 },
	{  544,  470 },
	{  560,  460 },
	{  577,  450 },
	{  594,  440 },
	{  612,  430 },
	{  628,  420 },
	{  647,  410 },
	{  665,  400 },
	{  684,  390 },
	{  703,  380 },
	{  723,  370 },
	{  743,  360 },
	{  763,  350 },
	{  783,  340 },
	{  804,  330 },
	{  825,  320 },
	{  847,  310 },
	{  869,  300 },
	{  891,  290 },
	{  913,  280 },
	{  936,  270 },
	{  958,  260 },
	{  980,  250 },
	{ 1002,  240 },
	{ 1027,  230 },
	{ 1050,  220 },
	{ 1075,  210 },
	{ 1098,  200 },
	{ 1121,  190 },
	{ 1145,  180 },
	{ 1169,  170 },
	{ 1193,  160 },
	{ 1216,  150 },
	{ 1239,  140 },
	{ 1262,  130 },
	{ 1286,  120 },
	{ 1309,  110 },
	{ 1332,  100 },
	{ 1356,   90 },
	{ 1377,   80 },
	{ 1400,   70 },
	{ 1422,   60 },
	{ 1444,   50 },
	{ 1465,   40 },
	{ 1486,   30 },
	{ 1506,   20 },
	{ 1526,   10 },
	{ 1546,    0 },
	{ 1566,  -10 },
	{ 1585,  -20 },
	{ 1603,  -30 },
	{ 1621,  -40 },
	{ 1639,  -50 },
	{ 1656,  -60 },
	{ 1673,  -70 },
	{ 1689,  -80 },
	{ 1704,  -90 },
	{ 1720, -100 },
	{ 1734, -110 },
	{ 1749, -120 },
	{ 1763, -130 },
	{ 1776, -140 },
	{ 1789, -150 },
	{ 1801, -160 },
	{ 1815, -170 },
	{ 1828, -180 },
	{ 1839, -190 },
	{ 1850, -200 },
	{ 1859, -210 },
	{ 1868, -220 },
	{ 1876, -230 },
	{ 1887, -240 },
	{ 1895, -250 },
};

static sec_bat_adc_table_data_t mega2_temp_table_02[] = {
	{344,	850},
	{371,	800},
	{439,	750},
	{519,	700},
	{569,	650},
	{724,   600},
    {849,	550},
	{993,	500},
	{1159,  450},
	{1347,	400},
	{1551,	350},
	{1775,	300},
	{2015,  250},
	{2269,	200},
	{2519,	150},
	{2729,	100},
	{2959,	50},
	{3169,    0},
	{3346,  -50},
	{3502,	-100},
	{3632,	-150},
	{3827,	-200},
};

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

static int battery_get_lpm_state(char *str)
{
	if (strncmp(str, "charger", 7) == 0)
		lpcharge = 1;

	pr_info("%s: Low power charging mode: %d\n", __func__, lpcharge);

	return lpcharge;
}
__setup("androidboot.mode=", battery_get_lpm_state);

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

/* Vasta battery parameter version 20140515 */

static data_point_t vgcomp_normal[] = {
	{{3200,},	{-1 ,},{{35,	20, 25, 65 ,},},},    /*SOC<20% & T<0oC*/
	{{3735,},	{-1 ,},{{35,	20, 25, 65 ,},},},    /*20%<=SOC<50% & T<0oC*/
	{{3853,},	{-1 ,},{{35,	20, 25, 65 ,},},},    /*50%<=SOC & T<0oC*/
	{{3200,},	{0  ,},{{20,	51, 25, 135,},},},    /*SOC<20% & 0oC<=T<45oC*/
	{{3735,},	{0  ,},{{20,	43, 25, 135,},},},    /*20%<=SOC<50% & 0oC<=T<45oC*/
	{{3853,},	{0  ,},{{28,	50, 25, 135,},},},    /*50%<=SOC & 0oC<=T<45oC*/
	{{3200,},	{450,},{{50,	55, 25, 135,},},},    /*SOC<20% & 45oC<=T<50oC*/
	{{3735,},	{450,},{{30,	50, 25, 135,},},},    /*20%<=SOC<50% & 45oC<=T<50oC*/
	{{3853,},	{450,},{{55,	58, 25, 135,},},},    /*50%<=SOC & 45oC<=T<50oC*/
	{{3200,},	{500,},{{50,	55, 45, 135,},},},    /*SOC<20% & 50oC<=T*/
	{{3735,},	{500,},{{30,	50, 45, 135,},},},    /*20%<=SOC<50% & 50oC<=T*/
	{{3853,},	{500,},{{55,	58, 45, 135,},},},    /*50%<=SOC & 50oC<=T*/
};

static data_point_t vgcomp_idle[] = {
	{{3200,},	{-1 ,},{{35,	20, 25, 65 ,},},},    /*SOC<20% & T<0oC*/
	{{3735,},	{-1 ,},{{35,	20, 25, 65 ,},},},    /*20%<=SOC<50% & T<0oC*/
	{{3853,},	{-1 ,},{{35,	20, 25, 65 ,},},},    /*50%<=SOC & T<0oC*/
	{{3200,},	{0  ,},{{20,	51, 25, 135,},},},    /*SOC<20% & 0oC<=T<45oC*/
	{{3735,},	{0  ,},{{20,	43, 25, 135,},},},    /*20%<=SOC<50% & 0oC<=T<45oC*/
	{{3853,},	{0  ,},{{28,	50, 25, 135,},},},    /*50%<=SOC & 0oC<=T<45oC*/
	{{3200,},	{450,},{{50,	55, 25, 135,},},},    /*SOC<20% & 45oC<=T<50oC*/
	{{3735,},	{450,},{{30,	50, 25, 135,},},},    /*20%<=SOC<50% & 45oC<=T<50oC*/
	{{3853,},	{450,},{{55,	58, 25, 135,},},},    /*50%<=SOC & 45oC<=T<50oC*/
	{{3200,},	{500,},{{50,	55, 45, 135,},},},    /*SOC<20% & 50oC<=T*/
	{{3735,},	{500,},{{30,	50, 45, 135,},},},    /*20%<=SOC<50% & 50oC<=T*/
	{{3853,},	{500,},{{55,	58, 45, 135,},},},    /*50%<=SOC & 50oC<=T*/
};

static data_point_t offset_low_power[] = {
    {{0,}, {250,}, {{-12,},},},
    {{15,}, {250,}, {{-5,},},},
    {{19,}, {250,}, {{-9,},},},
    {{20,}, {250,}, {{-10,},},},
    {{100,}, {250,}, {{0,},},},
    {{1000,}, {250,}, {{0,},},},
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
	            .voltNR = 3,
	            .tempNR = 4,
	            .vg_comp_data = vgcomp_normal,
	        },
	        [VGCOMP_IDLE] = {
	            .voltNR = 3,
	            .tempNR = 4,
	            .vg_comp_data = vgcomp_idle,
	        },
	    },
	    .offset_interpolation_order = {2,2},
	    .soc_offset = {
	        [OFFSET_LOW_POWER] = {
	            .soc_voltNR = 6,
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
#if defined(CONFIG_MACH_MEGA2LTE_USA_ATT)
sec_charging_current_t charging_current_table[] = {
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_UNKNOWN */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_BATTERY */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_UPS */
	{2100,  1800,  150,   40 * 60},     /* POWER_SUPPLY_TYPE_MAINS */
	{500,   500,   150,    40 * 60},     /* POWER_SUPPLY_TYPE_USB */
	{500,   500,   150,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{1000,   1000,   150,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{500,   500,   150,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_BMS*/
	{500,   500,   150,    40 * 60},     /* POWER_SUPPLY_TYPE_MISC */
	{1000,	1500,  150,    40 * 60},     /* POWER_SUPPLY_TYPE_WIRELESS */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_DOCK */
	{2100,  1800,  150,	40 * 60},     /* POWER_SUPPLY_TYPE_UARTOFF */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_OTG */
	{0,	0,	0,	0},           /* POWER_SUPPLY_TYPE_LAN_HUB */
};
#else
sec_charging_current_t charging_current_table[] = {
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_UNKNOWN */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_BATTERY */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_UPS */
	{2100,  1500,  200,   40 * 60},     /* POWER_SUPPLY_TYPE_MAINS */
	{500,   500,   200,    40 * 60},     /* POWER_SUPPLY_TYPE_USB */
	{500,   500,   200,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{1000,   1000,   200,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{500,   500,   200,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_BMS*/
	{500,   500,   200,    40 * 60},     /* POWER_SUPPLY_TYPE_MISC */
	{1000,	1500,  200,    40 * 60},     /* POWER_SUPPLY_TYPE_WIRELESS */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_DOCK */
	{2100,  1500,  200,	40 * 60},     /* POWER_SUPPLY_TYPE_UARTOFF */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_OTG */
	{0,	0,	0,	0},           /* POWER_SUPPLY_TYPE_LAN_HUB */
};
#endif
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

	if (channel == SEC_BAT_ADC_CHANNEL_BAT_CHECK)
		channel = 3;
	else if ((channel == SEC_BAT_ADC_CHANNEL_TEMP) ||\
		(channel ==SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT))
		channel = 1;

	ret = s3c_adc_read(temp_adc_client, channel);

	if (ret == -ETIMEDOUT) {
		for (i = 0; i < 5; i++) {
			msleep(20);
			ret = s3c_adc_read(temp_adc_client, channel);
			if (ret > 0)
				break;
		}

		if (i >= 5)
			pr_err("%s: Retry count exceeded\n", __func__);

	} else if (ret < 0) {
		pr_err("%s: Failed read adc value : %d\n",
		__func__, ret);
	}

	/*if (channel == 1)
		pr_info("%s: temp adc : %d\n", __func__, ret);
	else if (channel == 3)
		pr_info("%s: battery adc : %d\n", __func__, ret);*/
	return ret;
}

static bool sec_bat_adc_ic_init(struct platform_device *pdev) { return true; }
static bool sec_bat_adc_ic_exit(void) { return true; }
/*static int sec_bat_adc_ic_read(unsigned int channel) { return 0; }*/

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
		.read = sec_bat_adc_ap_read
	},
	.cable_adc_value = cable_adc_value_table,
	.charging_current = charging_current_table,
	.polling_time = polling_time_table,
	/* NO NEED TO BE CHANGED */

	.pmic_name = SEC_BATTERY_PMIC_NAME,

	.adc_check_count = 5,

	.adc_type = {
		SEC_BATTERY_ADC_TYPE_NONE,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* BAT_CHECK */
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
	.battery_check_type = SEC_BATTERY_CHECK_FUELGAUGE,/* SEC_BATTERY_CHECK_CHARGER,*/
	.check_count = 1,

	/* Battery check by ADC */
	.check_adc_max = 3500,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,/* SEC_BATTERY_THERMAL_SOURCE_ADC,*/

	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 1,
#if defined(CONFIG_MACH_MEGA2LTE_USA_ATT)
	.temp_highlimit_threshold_event = 800,
	.temp_highlimit_recovery_event = 750,
	.temp_highlimit_threshold_normal = 800,
	.temp_highlimit_recovery_normal = 750,
	.temp_highlimit_threshold_lpm = 800,
	.temp_highlimit_recovery_lpm = 750,
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 470,
	.temp_low_threshold_event = -40,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 560,
	.temp_high_recovery_normal = 470,
	.temp_low_threshold_normal = -40,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 540,
	.temp_high_recovery_lpm = 470,
	.temp_low_threshold_lpm = 0,
	.temp_low_recovery_lpm = 10,
#else
	.temp_highlimit_threshold_event = 800,
	.temp_highlimit_recovery_event = 750,
	.temp_highlimit_threshold_normal = 800,
	.temp_highlimit_recovery_normal = 750,
	.temp_highlimit_threshold_lpm = 800,
	.temp_highlimit_recovery_lpm = 750,
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 450,
	.temp_low_threshold_event = -40,
	.temp_low_recovery_event = 10,
	.temp_high_threshold_normal = 600,
	.temp_high_recovery_normal = 450,
	.temp_low_threshold_normal = -40,
	.temp_low_recovery_normal = 10,
	.temp_high_threshold_lpm = 600,
	.temp_high_recovery_lpm = 450,
	.temp_low_threshold_lpm = -40,
	.temp_low_recovery_lpm = 10,
#endif
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
	.full_condition_ocv = 4300,
	.full_condition_vcell = 4300,

	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_VCELL,
	.recharge_check_count = 3,
	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4300,
	.recharge_condition_vcell = 4300,

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
	.chg_float_voltage = 4350,
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
	pr_info("%s: system_rev(%d)\n", __func__, system_rev);
	if (system_rev < 0x04) {
		tab3_gpio_i2c14_pdata.scl_pin = GPIO_FG_SCL_REV01;
		tab3_gpio_i2c14_pdata.sda_pin = GPIO_FG_SDA_REV01;
	}
}

void __init exynos5_universal4415_battery_init(void)
{
	pr_info("%s: Mega2 charger init\n", __func__);

	charger_gpio_init();

	if (system_rev < 0x07)
		sec_battery_pdata.battery_check_type = SEC_BATTERY_CHECK_ADC;

	if (system_rev < 4) {
		sec_battery_pdata.temp_adc_table = mega2_temp_table_01;
		sec_battery_pdata.temp_adc_table_size = sizeof(mega2_temp_table_01)
			/ sizeof(sec_bat_adc_table_data_t);
		sec_battery_pdata.temp_amb_adc_table = mega2_temp_table_01;
		sec_battery_pdata.temp_amb_adc_table_size = sizeof(mega2_temp_table_01)
			/ sizeof(sec_bat_adc_table_data_t);
	} else {
		sec_battery_pdata.temp_adc_table = mega2_temp_table_02;
		sec_battery_pdata.temp_adc_table_size = sizeof(mega2_temp_table_02)
			/ sizeof(sec_bat_adc_table_data_t);
		sec_battery_pdata.temp_amb_adc_table = mega2_temp_table_02;
		sec_battery_pdata.temp_amb_adc_table_size = sizeof(mega2_temp_table_02)
			/ sizeof(sec_bat_adc_table_data_t);
	}

	is_wpc_cable_attached = false;

	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));

	i2c_register_board_info(FG_ID, sec_brdinfo_fuelgauge,
			ARRAY_SIZE(sec_brdinfo_fuelgauge));

	temp_adc_client = s3c_adc_register(&sec_device_battery, NULL, NULL, 0);
}

