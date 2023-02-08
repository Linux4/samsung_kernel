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

#include <linux/battery/sec_battery.h>
#ifdef CONFIG_BATTERY_SAMSUNG
#include <linux/battery/sec_fuelgauge.h>
#endif
#include <linux/battery/sec_charger.h>
#include <linux/gpio_event.h>

#include <plat/gpio-cfg.h>

#include "board-universal222ap.h"
#include <asm/system_info.h>

#define SEC_BATTERY_PMIC_NAME ""

#define TA_ADC_LOW              800
#define TA_ADC_HIGH             2200

#define GPIO_CHG_SCL	EXYNOS4_GPC0(2)
#define GPIO_CHG_SDA	EXYNOS4_GPC0(0)
#define CHG_ID		13
#define FG_ID           15
#define GPIO_FG_SCL     EXYNOS4_GPF1(5)
#define GPIO_FG_SDA     EXYNOS4_GPF0(7)
#define GPIO_VB_INT	    EXYNOS4_GPX2(1)
#define LOW_BAT_DETECT	EXYNOS4_GPX1(0)

#define SEC_CHARGER_I2C_SLAVEADDR_SMB328 (0x69>>1)

/* cable state */
static struct s3c_adc_client *temp_adc_client;
extern int current_cable_type;
extern bool is_cable_attached;

#if defined(CONFIG_MACH_DELOSLTE_KOR_SKT) || defined(CONFIG_MACH_DELOSLTE_KOR_KT) || \
	defined(CONFIG_MACH_DELOSLTE_KOR_LGT)
static sec_bat_adc_table_data_t harrison_temp_table[] = {
	{  152,  800 },
	{  158,  790 },
	{  164,  780 },
	{  170,  770 },
	{  176,  760 },
	{  182,  750 },
	{  188,  740 },
	{  194,  730 },
	{  200,  720 },
	{  207,  710 },
	{  214,  700 },
	{  221,  690 },
	{  228,  680 },
	{  235,  670 },
	{  242,  660 },
	{  255,  650 },
	{  266,  640 },
	{  277,  630 },
	{  289,  620 },
	{  301,  610 },
	{  313,  600 },
	{  325,  590 },
	{  338,  580 },
	{  351,  570 },
	{  364,  560 },
	{  377,  550 },
	{  390,  540 },
	{  403,  530 },
	{  416,  520 },
	{  431,  510 },
	{  446,  500 },
	{  461,  490 },
	{  478,  480 },
	{  495,  470 },
	{  512,  460 },
	{  529,  450 },
	{  546,  440 },
	{  563,  430 },
	{  580,  420 },
	{  597,  410 },
	{  617,  400 },
	{  637,  390 },
	{  657,  380 },
	{  679,  370 },
	{  701,  360 },
	{  723,  350 },
	{  745,  340 },
	{  767,  330 },
	{  789,  320 },
	{  811,  310 },
	{  833,  300 },
	{  855,  290 },
	{  877,  280 },
	{  899,  270 },
	{  923,  260 },
	{  947,  250 },
	{  971,  240 },
	{  995,  230 },
	{ 1019,  220 },
	{ 1043,  210 },
	{ 1067,  200 },
	{ 1092,  190 },
	{ 1116,  180 },
	{ 1140,  170 },
	{ 1164,  160 },
	{ 1188,  150 },
	{ 1210,  140 },
	{ 1233,  130 },
	{ 1256,  120 },
	{ 1279,  110 },
	{ 1302,  100 },
	{ 1325,   90 },
	{ 1348,   80 },
	{ 1372,   70 },
	{ 1396,   60 },
	{ 1420,   50 },
	{ 1442,   40 },
	{ 1464,   30 },
	{ 1487,   20 },
	{ 1510,   10 },
	{ 1533,    0 },
	{ 1551,  -10 },
	{ 1570,  -20 },
	{ 1589,  -30 },
	{ 1608,  -40 },
	{ 1627,  -50 },
	{ 1644,  -60 },
	{ 1661,  -70 },
	{ 1678,  -80 },
	{ 1694,  -90 },
	{ 1710, -100 },
	{ 1723, -110 },
	{ 1736, -120 },
	{ 1748, -130 },
	{ 1760, -140 },
	{ 1772, -150 },
	{ 1784, -160 },
	{ 1796, -170 },
	{ 1808, -180 },
	{ 1820, -190 },
	{ 1832, -200 },
};
#else
static sec_bat_adc_table_data_t harrison_temp_table[] = {
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
#endif

static bool is_jig_on;

static bool sec_bat_gpio_init(void)
{
	return true;
}

static bool sec_fg_gpio_init(void)
{
	s3c_gpio_cfgpin(LOW_BAT_DETECT, S3C_GPIO_INPUT);

	return true;
}

static bool sec_chg_gpio_init(void)
{
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

static irqreturn_t chg_int_intr(int irq, void *arg)
{
	int ta_nconnected = 0;
	union power_supply_propval value, health_val;
	union power_supply_propval val, vf_val;
	struct power_supply *psy_charger =
			power_supply_get_by_name("sec-charger");
	struct power_supply *psy = power_supply_get_by_name("battery");
	int cable_type = POWER_SUPPLY_TYPE_BATTERY;
	int charge_state = POWER_SUPPLY_STATUS_DISCHARGING;

	msleep(100);
	if (system_rev < 10) {
		if (psy_charger) {
			psy_charger->get_property(psy_charger,
					POWER_SUPPLY_PROP_CHARGE_NOW, &value);
			ta_nconnected = value.intval;

			pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);
		}
	} else {
		ta_nconnected = gpio_get_value(GPIO_VB_INT);
		pr_info("%s : ta_nconnected(rev10) : %d\n", __func__, ta_nconnected);
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
	}

	if (!psy || !psy->set_property) {
		pr_err("%s: fail to get battery psy\n", __func__);
		return -ENODEV;
	} else if (current_cable_type == POWER_SUPPLY_TYPE_MISC ||
		current_cable_type == POWER_SUPPLY_TYPE_UARTOFF) {
		value.intval = cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}

	/* ovp/uvlo check */
	psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &vf_val);
	psy->get_property(psy, POWER_SUPPLY_PROP_HEALTH, &health_val);
	charge_state = val.intval;
	if ((is_cable_attached && vf_val.intval) &&
		((health_val.intval != POWER_SUPPLY_HEALTH_OVERHEAT) &&
		(health_val.intval != POWER_SUPPLY_HEALTH_COLD))) {
		if (ta_nconnected &&
			charge_state == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			val.intval = POWER_SUPPLY_HEALTH_GOOD;
			psy_charger->set_property(psy_charger, POWER_SUPPLY_PROP_ONLINE, &value);
			psy->set_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
			pr_info("%s: current state  Not Charging ->Charging\n",
				__func__);
		} else if (!ta_nconnected &&
				charge_state == POWER_SUPPLY_STATUS_CHARGING) {
			val.intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			psy->set_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
			pr_info("%s: current state  Charging ->Not Charging\n",
				__func__);
		}
	}
	return IRQ_HANDLED;
}

static void sec_bat_initial_check(void)
{
	int ta_nconnected = 0;
	struct power_supply *psy = power_supply_get_by_name("battery");
	struct power_supply *psy_charger =
			power_supply_get_by_name("sec-charger");
	union power_supply_propval value;

	if (system_rev < 10) {
		if (psy_charger) {
			psy_charger->get_property(psy_charger,
					POWER_SUPPLY_PROP_CHARGE_NOW, &value);
			ta_nconnected = value.intval;

			pr_info("%s : ta_nconnected : %d\n", __func__, ta_nconnected);
		}
	} else {
		ta_nconnected = gpio_get_value(GPIO_VB_INT);
		pr_info("%s : ta_nconnected(rev10) : %d\n", __func__, ta_nconnected);
	}

	if (current_cable_type == POWER_SUPPLY_TYPE_MISC) {
		current_cable_type = !ta_nconnected ?
			POWER_SUPPLY_TYPE_BATTERY : POWER_SUPPLY_TYPE_MISC;
		pr_info("%s: current cable : MISC, type : %s\n", __func__,
				!ta_nconnected ? "BATTERY" : "MISC");
	} else if (current_cable_type == POWER_SUPPLY_TYPE_UARTOFF) {
		current_cable_type = !ta_nconnected ?
			POWER_SUPPLY_TYPE_BATTERY : POWER_SUPPLY_TYPE_UARTOFF;
		pr_info("%s: current cable : UARTOFF, type : %s\n", __func__,
				!ta_nconnected ? "BATTERY" : "UARTOFF");
	}

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
	/* omap4_tab3_tsp_ta_detect(cable_type); */

	return ret;
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
static bool sec_bat_check_callback(void) { return true; }
static bool sec_bat_check_result_callback(void) { return true; }

#if defined(CONFIG_MACH_DELOSLTE_KOR_SKT) || defined(CONFIG_MACH_DELOSLTE_KOR_KT) || \
	defined(CONFIG_MACH_DELOSLTE_KOR_LGT)
static struct battery_data_t stc3115_battery_data[] = {
	{
		.Vmode= 0,       /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
		.Alm_SOC = 2,      /* SOC alm level %*/
		.Alm_Vbat = 3450,   /* Vbat alm level mV*/
		.CC_cnf = 400,      /* nominal CC_cnf, coming from battery characterisation*/
		.VM_cnf = 405,      /* nominal VM cnf , coming from battery characterisation*/
		.Cnom = 2000,       /* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,       /* sense resistor mOhms*/
		.RelaxCurrent = 150, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

		/* Elentec Co Ltd Battery pack - 80 means 8% */
		.CapDerating[6] = 190,            /* capacity derating in 0.1%, for temp = -20 */
		.CapDerating[5] = 70,             /* capacity derating in 0.1%, for temp = -10  */
		.CapDerating[4] = 30,             /* capacity derating in 0.1%, for temp = 0  */
		.CapDerating[3] = 0,             /* capacity derating in 0.1%, for temp = 10  */
		.CapDerating[2] = 0,              /* capacity derating in 0.1%, for temp = 25  */
		.CapDerating[1] = -20,             /* capacity derating in 0.1%, for temp = 40  */
		.CapDerating[0] = -40,             /* capacity derating in 0.1%, for temp = 60 */

		.OCVOffset[15] = -123,    /* OCV curve adjustment */
		.OCVOffset[14] = -30,   /* OCV curve adjustment */
		.OCVOffset[13] = -12,    /* OCV curve adjustment */
		.OCVOffset[12] = -27,    /* OCV curve adjustment */
		.OCVOffset[11] = 0,    /* OCV curve adjustment */
		.OCVOffset[10] = -27,    /* OCV curve adjustment */
		.OCVOffset[9] = 4,     /* OCV curve adjustment */
		.OCVOffset[8] = 1,      /* OCV curve adjustment */
		.OCVOffset[7] = 7,      /* OCV curve adjustment */
		.OCVOffset[6] = 9,    /* OCV curve adjustment */
		.OCVOffset[5] = 9,    /* OCV curve adjustment */
		.OCVOffset[4] = 16,     /* OCV curve adjustment */
		.OCVOffset[3] = 33,    /* OCV curve adjustment */
		.OCVOffset[2] = 34,     /* OCV curve adjustment */
		.OCVOffset[1] = 46,    /* OCV curve adjustment */
		.OCVOffset[0] = -3,     /* OCV curve adjustment */

		.OCVOffset2[15] = -109,    /* OCV curve adjustment */
		.OCVOffset2[14] = -86,   /* OCV curve adjustment */
		.OCVOffset2[13] = -59,    /* OCV curve adjustment */
		.OCVOffset2[12] = -59,    /* OCV curve adjustment */
		.OCVOffset2[11] = -29,    /* OCV curve adjustment */
		.OCVOffset2[10] = -46,    /* OCV curve adjustment */
		.OCVOffset2[9] = -8,     /* OCV curve adjustment */
		.OCVOffset2[8] = 0,      /* OCV curve adjustment */
		.OCVOffset2[7] = -2,      /* OCV curve adjustment */
		.OCVOffset2[6] = -6,    /* OCV curve adjustment */
		.OCVOffset2[5] = -7,    /* OCV curve adjustment */
		.OCVOffset2[4] = -9,     /* OCV curve adjustment */
		.OCVOffset2[3] = 19,    /* OCV curve adjustment */
		.OCVOffset2[2] = 44,     /* OCV curve adjustment */
		.OCVOffset2[1] = 81,    /* OCV curve adjustment */
		.OCVOffset2[0] = 0,     /* OCV curve adjustment */

		/*if the application temperature data is preferred than the STC3115 temperature*/
		.ExternalTemperature = NULL, /*External temperature fonction, return */
		.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3115 temperature */
		.TemperatureOffset = -20, /* temperature offset -2C */
	}
};
#else
static struct battery_data_t stc3115_battery_data[] = {
	{
		.Vmode= 0,       /*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
		.Alm_SOC = 2,      /* SOC alm level %*/
		.Alm_Vbat = 3450,   /* Vbat alm level mV*/
		.CC_cnf = 363,      /* nominal CC_cnf, coming from battery characterisation*/
		.VM_cnf = 368,      /* nominal VM cnf , coming from battery characterisation*/
		.Cnom = 1800,       /* nominal capacity in mAh, coming from battery characterisation*/
		.Rsense = 10,       /* sense resistor mOhms*/
		.RelaxCurrent = 170, /* current for relaxation in mA (< C/20) */
		.Adaptive = 1,     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

		/* Elentec Co Ltd Battery pack - 80 means 8% */
		.CapDerating[6] = 165,            /* capacity derating in 0.1%, for temp = -20 */
		.CapDerating[5] = 72,             /* capacity derating in 0.1%, for temp = -10  */
		.CapDerating[4] = 28,             /* capacity derating in 0.1%, for temp = 0  */
		.CapDerating[3] = 22,             /* capacity derating in 0.1%, for temp = 10  */
		.CapDerating[2] = 0,              /* capacity derating in 0.1%, for temp = 25  */
		.CapDerating[1] = 0,             /* capacity derating in 0.1%, for temp = 40  */
		.CapDerating[0] = 0,             /* capacity derating in 0.1%, for temp = 60 */

		.OCVOffset[15] = -117,    /* OCV curve adjustment */
		.OCVOffset[14] = -17,   /* OCV curve adjustment */
		.OCVOffset[13] = -12,    /* OCV curve adjustment */
		.OCVOffset[12] = -9,    /* OCV curve adjustment */
		.OCVOffset[11] = 0,    /* OCV curve adjustment */
		.OCVOffset[10] = -9,    /* OCV curve adjustment */
		.OCVOffset[9] = -13,     /* OCV curve adjustment */
		.OCVOffset[8] = -8,      /* OCV curve adjustment */
		.OCVOffset[7] = -1,      /* OCV curve adjustment */
		.OCVOffset[6] = 10,    /* OCV curve adjustment */
		.OCVOffset[5] = 7,    /* OCV curve adjustment */
		.OCVOffset[4] = 17,     /* OCV curve adjustment */
		.OCVOffset[3] = 41,    /* OCV curve adjustment */
		.OCVOffset[2] = 32,     /* OCV curve adjustment */
		.OCVOffset[1] = -69,    /* OCV curve adjustment */
		.OCVOffset[0] = 42,     /* OCV curve adjustment */

		.OCVOffset2[15] = -116,    /* OCV curve adjustment */
		.OCVOffset2[14] = -92,   /* OCV curve adjustment */
		.OCVOffset2[13] = -67,    /* OCV curve adjustment */
		.OCVOffset2[12] = -62,    /* OCV curve adjustment */
		.OCVOffset2[11] = -40,    /* OCV curve adjustment */
		.OCVOffset2[10] = -54,    /* OCV curve adjustment */
		.OCVOffset2[9] = -13,     /* OCV curve adjustment */
		.OCVOffset2[8] = -4,      /* OCV curve adjustment */
		.OCVOffset2[7] = -5,      /* OCV curve adjustment */
		.OCVOffset2[6] = -5,    /* OCV curve adjustment */
		.OCVOffset2[5] = -6,    /* OCV curve adjustment */
		.OCVOffset2[4] = -7,     /* OCV curve adjustment */
		.OCVOffset2[3] = 15,    /* OCV curve adjustment */
		.OCVOffset2[2] = 44,     /* OCV curve adjustment */
		.OCVOffset2[1] = 80,    /* OCV curve adjustment */
		.OCVOffset2[0] = 0,     /* OCV curve adjustment */

		/*if the application temperature data is preferred than the STC3115 temperature*/
		.ExternalTemperature = NULL, /*External temperature fonction, return */
		.ForceExternalTemperature = 0, /* 1=External temperature, 0=STC3115 temperature */
		.TemperatureOffset = -20, /* temperature offset -2C */
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

#if defined(CONFIG_MACH_DELOSLTE_KOR_SKT) || defined(CONFIG_MACH_DELOSLTE_KOR_KT) || \
	defined(CONFIG_MACH_DELOSLTE_KOR_LGT)
static sec_charging_current_t charging_current_table[] = {
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_UNKNOWN */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_BATTERY */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_UPS */
	{1200,  1200,   150,   40 * 60},     /* POWER_SUPPLY_TYPE_MAINS */
	{500,   500,    150,   40 * 60},     /* POWER_SUPPLY_TYPE_USB */
	{500,   500,    150,   40 * 60},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{500,   500,    150,   40 * 60},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{500,   500,    150,   40 * 60},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_BMS*/
	{500,   500,    150,   40 * 60},     /* POWER_SUPPLY_TYPE_MISC */
	{0,		0,		0,	   0},	   /* POWER_SUPPLY_TYPE_WIRELESS */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_DOCK */
	{1200,  1200,   150,   40 * 60},     /* POWER_SUPPLY_TYPE_UARTOFF */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_OTG */
	{0,	0,	0,	0},/* POWER_SUPPLY_TYPE_LAN_HUB */
};
#else
static sec_charging_current_t charging_current_table[] = {
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_UNKNOWN */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_BATTERY */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_UPS */
	{1000,  1200,   150,   40 * 60},     /* POWER_SUPPLY_TYPE_MAINS */
	{500,   500,   150,    40 * 60},     /* POWER_SUPPLY_TYPE_USB */
	{500,   500,   150,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_DCP */
	{500,   500,   150,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_CDP */
	{500,   500,   150,    40 * 60},     /* POWER_SUPPLY_TYPE_USB_ACA */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_BMS*/
	{500,   500,   150,    40 * 60},     /* POWER_SUPPLY_TYPE_MISC */
	{0,		0,		0,	   0},	   /* POWER_SUPPLY_TYPE_WIRELESS */
	{0,     0,      0,     0},     /* POWER_SUPPLY_TYPE_DOCK */
	{1000,  1200,   150,   40 * 60},     /* POWER_SUPPLY_TYPE_UARTOFF */
	{0,	0,	0,	0},	      /* POWER_SUPPLY_TYPE_OTG */
	{0,	0,	0,	0},/* POWER_SUPPLY_TYPE_LAN_HUB */
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

	pr_info("%s: temp acd : %d\n", __func__, ret);
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
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_data = (void *)stc3115_battery_data,
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
	.battery_check_type = SEC_BATTERY_CHECK_CHARGER,
	.check_count = 0,

	/* Battery check by ADC */
	.check_adc_max = 30000,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	.temp_adc_table = harrison_temp_table,
	.temp_adc_table_size = sizeof(harrison_temp_table)
					/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = harrison_temp_table,
	.temp_amb_adc_table_size = sizeof(harrison_temp_table)
					/sizeof(sec_bat_adc_table_data_t),

	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 1,
#if defined(CONFIG_MACH_DELOSLTE_KOR_SKT) || defined(CONFIG_MACH_DELOSLTE_KOR_KT) || \
	defined(CONFIG_MACH_DELOSLTE_KOR_LGT)
	.temp_highlimit_threshold_event = 800,
	.temp_highlimit_recovery_event = 750,
	.temp_highlimit_threshold_normal = 800,
	.temp_highlimit_recovery_normal = 750,
	.temp_highlimit_threshold_lpm = 800,
	.temp_highlimit_recovery_lpm = 750,
	.temp_high_threshold_event = 660,
	.temp_high_recovery_event = 415,
	.temp_low_threshold_event = -45,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 660,
	.temp_high_recovery_normal = 415,
	.temp_low_threshold_normal = -45,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 660,
	.temp_high_recovery_lpm = 415,
	.temp_low_threshold_lpm = -45,
	.temp_low_recovery_lpm = 0,
#else
	.temp_highlimit_threshold_event = 800,
	.temp_highlimit_recovery_event = 750,
	.temp_highlimit_threshold_normal = 800,
	.temp_highlimit_recovery_normal = 750,
	.temp_highlimit_threshold_lpm = 800,
	.temp_highlimit_recovery_lpm = 750,
	.temp_high_threshold_event = 610,
	.temp_high_recovery_event = 440,
	.temp_low_threshold_event = -40,
	.temp_low_recovery_event = 0,
	.temp_high_threshold_normal = 610,
	.temp_high_recovery_normal = 440,
	.temp_low_threshold_normal = -40,
	.temp_low_recovery_normal = 0,
	.temp_high_threshold_lpm = 490,
	.temp_high_recovery_lpm = 450,
	.temp_low_threshold_lpm = -30,
	.temp_low_recovery_lpm = 0,
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
	.recharge_check_count = 2,
	.recharge_condition_soc = 98,
	.recharge_condition_avgvcell = 4300,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time = 90 * 60,
	.charging_reset_time = 0,

	/* Fuel Gauge */
	.fg_irq = LOW_BAT_DETECT,
	.fg_irq_attr = IRQF_TRIGGER_FALLING,
	.fuel_alert_soc = 3,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL,
	.capacity_max = 1000,
	.capacity_min = 0,
	.capacity_max_margin = 30,

	/* Charger */
	.chg_polarity_en = 0,   /* active LOW charge enable */
	.chg_polarity_status = 0,
	.chg_irq_attr = IRQF_TRIGGER_RISING,

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

static struct i2c_gpio_platform_data tab3_gpio_i2c13_pdata = {
	.sda_pin = (GPIO_CHG_SDA),
	.scl_pin = (GPIO_CHG_SCL),
	.udelay = 10,
	.timeout = 0,
};

static struct platform_device tab3_gpio_i2c13_device = {
	.name = "i2c-gpio",
	.id = CHG_ID,
	.dev = {
		.platform_data = &tab3_gpio_i2c13_pdata,
	},
};

/* set MAX17050 Fuel Gauge gpio i2c */
static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_battery_pdata,
};

static struct i2c_board_info sec_brdinfo_charger[] __initdata = {
	{
		I2C_BOARD_INFO("sec-charger",
				SEC_CHARGER_I2C_SLAVEADDR),
		.platform_data = &sec_battery_pdata,
	}
};

static struct i2c_board_info sec_brdinfo_charger_smb328[] __initdata = {
	{
		I2C_BOARD_INFO("sec-charger",
				SEC_CHARGER_I2C_SLAVEADDR_SMB328),
		.platform_data = &sec_battery_pdata,
	}
};

static struct i2c_board_info sec_brdinfo_fuelgauge[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
				STC31xx_SLAVE_ADDRESS),
		.platform_data = &sec_battery_pdata,
	}
};

static struct platform_device *sec_battery_devices[] __initdata = {
	&tab3_gpio_i2c13_device,
	&tab3_gpio_i2c14_device,
	&sec_device_battery,
};

void __init exynos4_smdk4270_charger_init(void)
{
	pr_info("%s: harrison charger init\n", __func__);


	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));

	if (system_rev < 8) {
		i2c_register_board_info(CHG_ID, sec_brdinfo_charger,
				ARRAY_SIZE(sec_brdinfo_charger));
		sec_battery_pdata.chg_float_voltage = 4350;
	} else {
		i2c_register_board_info(CHG_ID, sec_brdinfo_charger_smb328,
				ARRAY_SIZE(sec_brdinfo_charger_smb328));
		sec_battery_pdata.chg_float_voltage = 4340;
	}

	if (system_rev > 9) {
#if defined(CONFIG_MACH_DELOSLTE_KOR_SKT) || defined(CONFIG_MACH_DELOSLTE_KOR_KT) || \
	defined(CONFIG_MACH_DELOSLTE_KOR_LGT)
	sec_battery_pdata.temp_high_threshold_event = 660;
	sec_battery_pdata.temp_high_recovery_event = 415;
	sec_battery_pdata.temp_low_threshold_event = -45;
	sec_battery_pdata.temp_low_recovery_event = 0;
	sec_battery_pdata.temp_high_threshold_normal = 660;
	sec_battery_pdata.temp_high_recovery_normal = 415;
	sec_battery_pdata.temp_low_threshold_normal = -45;
	sec_battery_pdata.temp_low_recovery_normal = 0;
	sec_battery_pdata.temp_high_threshold_lpm = 660;
	sec_battery_pdata.temp_high_recovery_lpm = 415;
	sec_battery_pdata.temp_low_threshold_lpm = -45;
	sec_battery_pdata.temp_low_recovery_lpm = 0;
#else
	sec_battery_pdata.temp_high_threshold_event = 610;
	sec_battery_pdata.temp_high_recovery_event = 430;
	sec_battery_pdata.temp_low_threshold_event = -40;
	sec_battery_pdata.temp_low_recovery_event = 20;
	sec_battery_pdata.temp_high_threshold_normal = 480;
	sec_battery_pdata.temp_high_recovery_normal = 430;
	sec_battery_pdata.temp_low_threshold_normal = -40;
	sec_battery_pdata.temp_low_recovery_normal = 20;
	sec_battery_pdata.temp_high_threshold_lpm = 480;
	sec_battery_pdata.temp_high_recovery_lpm = 450;
	sec_battery_pdata.temp_low_threshold_lpm = -30;
	sec_battery_pdata.temp_low_recovery_lpm = 0;
#endif
	}

#if defined(CONFIG_MACH_DELOSLTE_KOR_SKT) || defined(CONFIG_MACH_DELOSLTE_KOR_KT) || \
	defined(CONFIG_MACH_DELOSLTE_KOR_LGT)
	if (system_rev == 11 || system_rev == 12) {
		stc3115_battery_data[0].CC_cnf = 960;
		stc3115_battery_data[0].Rsense = 24;
		pr_info("%s: temp CC_cnf(%d), Rsense(%d)", __func__,
			stc3115_battery_data[0].CC_cnf, stc3115_battery_data[0].Rsense);
	} else if (system_rev == 13 || system_rev == 14 || system_rev == 15) {
		stc3115_battery_data[0].CC_cnf = 448;
		stc3115_battery_data[0].VM_cnf = 413;
		stc3115_battery_data[0].Rsense = 11;
		pr_info("%s: temp CC_cnf(%d), VM_cnf(%d), Rsense(%d)", __func__,
			stc3115_battery_data[0].CC_cnf, stc3115_battery_data[0].VM_cnf,
			stc3115_battery_data[0].Rsense);
	}

	sec_battery_pdata.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL;
#endif

	i2c_register_board_info(FG_ID, sec_brdinfo_fuelgauge,
			ARRAY_SIZE(sec_brdinfo_fuelgauge));

	temp_adc_client = s3c_adc_register(&sec_device_battery, NULL, NULL, 0);
}

static int __init exynos4_garda_battery_late_init(void)
{
	int ret;

	if (system_rev < 10) {
		ret = request_threaded_irq(gpio_to_irq(GPIO_CHG_INT), NULL,
			chg_int_intr,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT, "pogo_vbus", NULL);
	} else {
		ret = request_threaded_irq(gpio_to_irq(GPIO_VB_INT), NULL,
			chg_int_intr,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT, "pogo_vbus", NULL);
	}

	if (ret) {
		pr_err("%s: chg int irq register failed, ret=%d\n",
				__func__, ret);
	} else {
		if (system_rev < 10)
			ret = enable_irq_wake(gpio_to_irq(GPIO_CHG_INT));
		else
			ret = enable_irq_wake(gpio_to_irq(GPIO_VB_INT));

		if (ret)
			pr_warn("%s: failed to enable irq_wake for chg int\n",
				__func__);
	}

	return 0;
}

late_initcall(exynos4_garda_battery_late_init);
