/*
 * 88PM88X PMIC Charger driver
 *
 * Copyright (c) 2015 Marvell Technology Ltd.
 * Yi Zhang <yizhang@marvell.com>
 * Shay Pathov <shayp@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/jiffies.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>
#include <linux/delay.h>
#include <linux/of_device.h>

#include <linux/battery/sec_charger.h>
#include <linux/gpio-pxa.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of.h>

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif

static int charger_health = POWER_SUPPLY_HEALTH_GOOD;

extern bool sec_bat_check_cable_result_callback(int cable_type);

int otg_enable_flag;

static enum power_supply_property sec_charger_props[] = {
		POWER_SUPPLY_PROP_ONLINE,
		POWER_SUPPLY_PROP_STATUS,
		POWER_SUPPLY_PROP_HEALTH,
		POWER_SUPPLY_PROP_PRESENT,
		POWER_SUPPLY_PROP_CHARGE_NOW,
		POWER_SUPPLY_PROP_CURRENT_NOW,
		POWER_SUPPLY_PROP_CHARGE_TYPE,
		POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
		POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
#if defined(CONFIG_BATTERY_SWELLING)
		POWER_SUPPLY_PROP_VOLTAGE_MAX,
#endif
		POWER_SUPPLY_PROP_CURRENT_AVG,
		POWER_SUPPLY_PROP_CHARGING_ENABLED,
		POWER_SUPPLY_PROP_STATUS_FG,
};
static enum power_supply_property pm88x_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int pm88x_charger_init(struct pm88x_charger_info *info);
static void pm88x_otg_check_errors(struct pm88x_charger_info *info);

static int pm88x_get_charging_status(
	struct pm88x_charger_info *info, bool dbg_show)
{
	static int cfg1, stat1, stat2, stat3, otg_log1;
	int status = POWER_SUPPLY_STATUS_UNKNOWN;

	regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG1, &cfg1);
	regmap_read(info->chip->battery_regmap, PM88X_CHG_STATUS1, &stat1);
	regmap_read(info->chip->battery_regmap, PM88X_CHG_STATUS2, &stat2);
	regmap_read(info->chip->battery_regmap, PM88X_CHG_STATUS3, &stat3);
	regmap_read(info->chip->battery_regmap, PM88X_OTG_LOG1, &otg_log1);

	if (dbg_show) {
		dev_info(info->dev, "%s : PM88X_CHG_CONFIG1 : 0x%02x\n",
			__func__, cfg1);
		dev_info(info->dev, "%s : PM88X_CHG_STATUS1 : 0x%02x\n",
			__func__, stat1);
		dev_info(info->dev, "%s : PM88X_CHG_STATUS2 : 0x%02x\n",
			__func__, stat2);
		dev_dbg(info->dev, "%s : PM88X_CHG_STATUS3 : 0x%02x\n",
			__func__, stat3);
	}

	if ((cfg1 & PM88X_CHG_ENABLE) && ((stat2 & PM88X_CHG_TRICLE) ||
		/* Fast-charging */
		((stat2 & 0x20) && (stat2 & 0x40)) ||
		/* Pre-charging */
		((stat2 & 0x40) && (stat2 & 0x10)))) {
		if (charger_health == POWER_SUPPLY_HEALTH_GOOD)
			status = POWER_SUPPLY_STATUS_CHARGING;
		else
			status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else {
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	/* Check for overcurrent */
	if ((info->cable_type == POWER_SUPPLY_TYPE_OTG) &&
			(otg_log1 & PM88X_OTG_SHORT_DET)) {
		dev_info(info->chip->dev, "%s OTG overcurrent detected\n",
			__func__);
		pm88x_otg_check_errors(info);
	}

	dev_dbg(info->dev, "%s : Charger Status : %d\n", __func__, status);
	return (int)status;
}

static int pm88x_get_charging_health(struct pm88x_charger_info *info)
{
	static int health = POWER_SUPPLY_HEALTH_GOOD;
	union power_supply_propval chg_mode, bat_stat;
	static int stat2, data_msb, data_lsb, data, reg_val, vbus_avg, vbus_meas;

	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_CHARGE_NOW, chg_mode);
	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_STATUS, bat_stat);

	regmap_read(info->chip->battery_regmap, PM88X_CHG_STATUS2, &stat2);

	dev_info(info->dev, "%s : PM88X_CHG_STATUS2 : 0x%02x\n", __func__, stat2);

	if (((stat2 & PM88X_VBUS_SW_OV) && (stat2 & PM88X_USB_LIM_OK)) &&
		((stat2 & PM88X_CHG_TRICLE) || (stat2 & PM88X_CHG_PRECHG) ||
		(stat2 & PM88X_CHG_FASTCHG))){
		dev_dbg(info->dev, "%s : OVP bit set but health status normal\n",
			__func__);
		health = POWER_SUPPLY_HEALTH_GOOD;
		charger_health = POWER_SUPPLY_HEALTH_GOOD;
	} else if (((stat2 & PM88X_VBUS_SW_ON) || ((stat2 & PM88X_VBUS_SW_OV) &&
			(stat2 & PM88X_USB_LIM_OK))) && (!((stat2 & PM88X_CHG_TRICLE) ||
			(stat2 & PM88X_CHG_PRECHG) || (stat2 & PM88X_CHG_FASTCHG)))){

			/* VBUS AVG, register 0xA8 and 0xA9 page 0x32 */
			regmap_read(info->chip->gpadc_regmap, 0xA8, &data_msb);
			regmap_read(info->chip->gpadc_regmap, 0xA9, &data_lsb);
			data = ((data_msb & 0xFF) << 4) | (data_lsb & 0x0F);
			reg_val = data;
			data  = (data * 1709) / 1000;
			dev_info(info->dev, "%s: PM88X_VBUS_AVG(0x%x) = %dmV, 0x%x\n",
				__func__, 0xA9, data, reg_val);
			vbus_avg = data;

			/* VBUS MEAS, register 0x4A and 0x4B page 0x32 */
			regmap_read(info->chip->gpadc_regmap, 0x4A, &data_msb);
			regmap_read(info->chip->gpadc_regmap, 0x4B, &data_lsb);
			data = ((data_msb & 0xFF) << 4) | (data_lsb & 0x0F);
			reg_val = data;
			data  = (data * 1709) / 1000;
			dev_info(info->dev, "%s: PM88X_VBUS_MEAS(0x%x) = %dmV, 0x%x\n",
				__func__, 0x4B, data, reg_val);
			vbus_meas = data;

			/* Check VBUS voltage level to confirm UVLO case, prevent charger recovery issue */
			if ((vbus_meas >= 4500) && (vbus_avg >= 4500)) {
				health = POWER_SUPPLY_HEALTH_GOOD;
				charger_health = POWER_SUPPLY_HEALTH_GOOD;
				dev_info(info->dev, "%s : Charger Health Good/False UVLO detected \n", __func__);
			} else {
				dev_info(info->dev, "%s : Charger Health UVLO \n", __func__);
				health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
				charger_health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
				info->is_fullcharged = false;
			}
	} else if (((stat2 & PM88X_VBUS_SW_OV) && !(stat2 & PM88X_USB_LIM_OK)) &&
			(!((stat2 & PM88X_CHG_TRICLE) || (stat2 & PM88X_CHG_PRECHG) ||
			(stat2 & PM88X_CHG_FASTCHG)))){
			dev_info(info->dev, "%s : Charger Health OVP \n", __func__);
			health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			info->is_fullcharged = false;
	} else if ((stat2 & PM88X_VBUS_SW_ON) || (stat2 & PM88X_USB_LIM_OK)){
		health = POWER_SUPPLY_HEALTH_GOOD;
		charger_health = POWER_SUPPLY_HEALTH_GOOD;
	}

	if ((bat_stat.intval == POWER_SUPPLY_STATUS_CHARGING) &&
			(chg_mode.intval == SEC_BATTERY_CHARGING_NONE)){
		if ((health == POWER_SUPPLY_HEALTH_UNDERVOLTAGE) ||
			(health == POWER_SUPPLY_HEALTH_OVERVOLTAGE)){
			dev_info(info->dev,
				"%s : Force charger health to good in Charging-None (%d)\n",
				__func__, health);
			charger_health = POWER_SUPPLY_HEALTH_GOOD;
			health = POWER_SUPPLY_HEALTH_GOOD;
		}
	}

	return (int)health;
}

static inline int pm88x_set_batreg_voltage(
		struct pm88x_charger_info *info, unsigned int batreg_voltage)
{
	unsigned int float_voltage;

	/* fastcharge voltage range is 3.6V - 4.5V */
	if (batreg_voltage > 4500)
		batreg_voltage = 4500;
	else if (batreg_voltage < 3600)
		batreg_voltage = 3600;

	float_voltage = (batreg_voltage - 3600) * 10 / 125;

	regmap_update_bits(info->chip->battery_regmap, PM88X_FAST_CONFIG1,
		   PM88X_VBAT_FAST_SET_MASK, float_voltage);

	dev_dbg(info->dev, "%s: float voltage (0x%x) = 0x%x\n",
		__func__, PM88X_VBAT_FAST_SET_MASK, float_voltage);

	return float_voltage;
}

#if defined(CONFIG_BATTERY_SWELLING)
static u8 pm88x_get_batreg_voltage(struct pm88x_charger_info *info)
{
	unsigned int batreg_voltage, reg_data = 0;

	regmap_read(info->chip->battery_regmap, PM88X_FAST_CONFIG1, &reg_data);
	reg_data &= 0x7F;
	batreg_voltage = ((reg_data * 125) / 10) + 3600;
	pr_info("%s: battery float voltage: 0x%x, %d\n",
		__func__, reg_data, batreg_voltage);

	return reg_data;
}
#endif

static inline int pm88x_set_vbuslimit_current(
	struct pm88x_charger_info *info, unsigned int input_current)
{
	unsigned int ilim_raw, ilim_fine, ilim, extd;

	regmap_read(info->chip->battery_regmap,
		PM88X_TIMER_CONFIG, &extd);
	extd &= PM88X_CHG_ILIM_EXTD_CLR_MASK;
	regmap_write(info->chip->battery_regmap,
		PM88X_TIMER_CONFIG, extd);

	/* ilim is integer multiples of 90, 88.75, 87.5 ,86.25, 85*/
	if (input_current <= 100) {
		ilim_raw = PM88X_CHG_ILIM_RAW_100;
		ilim_fine = PM88X_CHG_ILIM_FINE_10;
		/* Set ilim to 90mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 500) {
		/* Typical USB current <=500mA */
		ilim_raw = PM88X_CHG_ILIM_RAW_400;
		ilim_fine = PM88X_CHG_ILIM_FINE_10;
		/* Set ilim to 468mA */
		regmap_read(info->chip->battery_regmap,
			PM88X_TIMER_CONFIG, &extd);
		extd |= PM88X_CHG_ILIM_EXTD_1_3;
		regmap_write(info->chip->battery_regmap,
			PM88X_TIMER_CONFIG, extd);
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 550) {
		ilim_raw = PM88X_CHG_ILIM_RAW_600;
		ilim_fine = PM88X_CHG_ILIM_FINE_10;
		/* Set ilim to 540mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 600) {
		ilim_raw = PM88X_CHG_ILIM_RAW_700;
		ilim_fine = PM88X_CHG_ILIM_FINE_15;
		/* Set ilim to 595mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 650) {
		ilim_raw = PM88X_CHG_ILIM_RAW_700;
		ilim_fine = PM88X_CHG_ILIM_FINE_12_5;
		/* Set ilim to 612.5mA, (maximum 643.125mA with +/- 5% error rate) */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 700) {
		ilim_raw = PM88X_CHG_ILIM_RAW_800;
#if defined(CONFIG_MACH_GRANDPRIMEVELTE)
		ilim_fine = PM88X_CHG_ILIM_FINE_12_5;
#else
		ilim_fine = PM88X_CHG_ILIM_FINE_15;
#endif
		/* Set ilim to 680mA, **Needs to change, not within 5% error rate** */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 750) {
		ilim_raw = PM88X_CHG_ILIM_RAW_800;
		ilim_fine = PM88X_CHG_ILIM_FINE_11_25;
		/* Set ilim to 710mA, +/- 5% error rate of 750mA*/
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 800) {
		ilim_raw = PM88X_CHG_ILIM_RAW_900;
		ilim_fine = PM88X_CHG_ILIM_FINE_11_25;
		/* Set ilim to 798.75mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 850) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1000;
		ilim_fine = PM88X_CHG_ILIM_FINE_15;
		/* Set ilim to 850mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 900) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1000;
		ilim_fine = PM88X_CHG_ILIM_FINE_10;
		/* Set ilim to 900mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 910) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1000;
		ilim_fine = PM88X_CHG_ILIM_FINE_10;
		/* Set ilim to 910mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 920) {
		ilim_raw = PM88X_CHG_ILIM_RAW_700;
		ilim_fine = PM88X_CHG_ILIM_FINE_12_5;
		regmap_read(info->chip->battery_regmap,
			PM88X_TIMER_CONFIG, &extd);
		extd |= PM88X_CHG_ILIM_EXTD_1_5;
		regmap_write(info->chip->battery_regmap,
			PM88X_TIMER_CONFIG, extd);
		/* Set ilim to 918.75mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 935) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1100;
		ilim_fine = PM88X_CHG_ILIM_FINE_15;
		/* Set ilim to 935mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 950) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1100;
		ilim_fine = PM88X_CHG_ILIM_FINE_13_75;
		/* Set ilim to 948.75mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 980) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1100;
		ilim_fine = PM88X_CHG_ILIM_FINE_11_25;
		/* Set ilim to 976.25mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 1000) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1100;
		ilim_fine = PM88X_CHG_ILIM_FINE_10;
		/* Set ilim to 990mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 1200) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1400;
		ilim_fine = PM88X_CHG_ILIM_FINE_15;
		/* Set ilim to 1190mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 1300) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1100;
		ilim_fine = PM88X_CHG_ILIM_FINE_13_75;
		regmap_read(info->chip->battery_regmap,
			PM88X_TIMER_CONFIG, &extd);
		extd |= PM88X_CHG_ILIM_EXTD_1_3;
		regmap_write(info->chip->battery_regmap,
			PM88X_TIMER_CONFIG, extd);
		/* Set ilim to maximum ~1295mA (+/- 5% error) */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 1350) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1500;
		ilim_fine = PM88X_CHG_ILIM_FINE_10;
		/* Set ilim to 1350mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 1400) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1600;
		ilim_fine = PM88X_CHG_ILIM_FINE_12_5;
		/* Set ilim to 1400mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 1500) {
		ilim_raw = PM88X_CHG_ILIM_RAW_1600;
		ilim_fine = PM88X_CHG_ILIM_FINE_11_25;
		/* Set ilim to 1420mA, (maximum 1491mA with +/- 5% error rate) */
		ilim = ilim_raw | ilim_fine;
	} else {
		/* Typical USB current <=500mA */
		ilim_raw = PM88X_CHG_ILIM_RAW_400;
		ilim_fine = PM88X_CHG_ILIM_FINE_10;
		/* Set ilim to 468mA */
		regmap_read(info->chip->battery_regmap,
			PM88X_TIMER_CONFIG, &extd);
		extd |= PM88X_CHG_ILIM_EXTD_1_3;
		regmap_write(info->chip->battery_regmap,
			PM88X_TIMER_CONFIG, extd);
		ilim = ilim_raw | ilim_fine;
	}

	regmap_write(info->chip->battery_regmap, PM88X_EXT_ILIM_CONFIG, ilim);

	dev_info(info->dev, "%s: VBUS current limit(0x%x) = 0x%x\n",
		__func__, PM88X_EXT_ILIM_CONFIG, ilim);
	dev_info(info->dev, "%s: Input current limit extension (0x%x) = 0x%x\n",
		__func__, PM88X_TIMER_CONFIG, extd);

	return ilim;
}

static inline int pm88x_set_topoff(
	struct pm88x_charger_info *info, unsigned int topoff_current)
{
	unsigned int eoc_current;

	/* fastcharge eoc current range is 10mA - 640mA */
	if (topoff_current > 640)
		topoff_current = 640;
	else if (topoff_current < 10)
		topoff_current = 10;

	eoc_current = (topoff_current - 10) / 10;
	regmap_update_bits(info->chip->battery_regmap, PM88X_FAST_CONFIG3,
			   PM88X_IBAT_EOC_TH, eoc_current);

	dev_dbg(info->dev, "%s: eoc_current (0x%x) = 0x%x\n", __func__,
		PM88X_FAST_CONFIG3, eoc_current);

	return eoc_current;
}

static inline int pm88x_set_fastchg_current(
	struct pm88x_charger_info *info, unsigned int fast_charging_current)
{
	unsigned int fast_current;

	if (fast_charging_current >= 2000)
		fast_charging_current = 2000;

	if (info->chip->type == PM886) {
		if (fast_charging_current <= 300)
			fast_current = 0x0;
		else
			fast_current = ((fast_charging_current - 450) / 50) + 1;
	} else {
		/* Account for 0mA test value */
		if (fast_charging_current == 0)
			fast_current = 0x0;
		else if (fast_charging_current <= 300)
			fast_current = 0x1;
		else {
			if (fast_charging_current <= 450)
				fast_current = 0x2;
			else
				fast_current = ((fast_charging_current - 450) / 50) + 2;
		}
	}
	regmap_update_bits(info->chip->battery_regmap, PM88X_FAST_CONFIG2,
			   PM88X_ICHG_FAST_SET_MASK, fast_current);

	dev_dbg(info->dev, "%s: Fastcharge current(0x%x) = 0x%x\n", __func__,
		PM88X_FAST_CONFIG2, fast_current);

	return fast_current;
}

static inline int get_fastchg_cur(struct pm88x_charger_info *info)
{
	/* fastcharge current range is 300mA - 2000mA */
	unsigned int fast_current, reg_data = 0;
	regmap_read(info->chip->battery_regmap, PM88X_FAST_CONFIG2, &reg_data);

	if (info->chip->type == PM886) {
		if (reg_data == 0x00)
			fast_current = 300;
		else
			fast_current = 450 + (50 * (reg_data - 1));
	} else {
		if (reg_data == 0x00)
			fast_current = 0;
		else if (reg_data == 0x01)
			fast_current = 300;
		else
			fast_current = ((reg_data - 2) * 50) + 450;
	}

	pr_debug("%s: Fastcharge current 0x%x, %d\n",
		__func__, reg_data, fast_current);
	return fast_current;
}

static inline int get_prechg_cur(struct pm88x_charger_info *info)
{
	/* precharge current range is 300mA - 750mA */
	if (info->prechg_cur > 750)
		info->prechg_cur = 750;
	else if (info->prechg_cur < 300)
		info->prechg_cur = 300;

	if (info->prechg_cur < 450)
		return 0;
	else
		return (info->prechg_cur - 450) / 50 + 1;
}

static inline int get_prechg_vol(struct pm88x_charger_info *info)
{
	/* precharge voltage range is 2.3V - 3.0V */
	if (info->prechg_vol > 3000)
		info->prechg_vol = 3000;
	else if (info->prechg_vol < 2300)
		info->prechg_vol = 2300;
	return (info->prechg_vol - 2300) / 100;
}

static int pm88x_start_charging(struct pm88x_charger_info *info)
{
	int stat2;

	printk("enter %s \n",__func__);
	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return 0;
	}

	/* open test page */
	regmap_write(info->chip->base_regmap, 0x1F, 0x1);

	/*
	 * override the status of the internal comparator after the 128ms filter,
	 * to assure charging is enabled regardless of the recharge threshold.
	 */
	regmap_write(info->chip->test_regmap, 0x40, 0x10);
	regmap_write(info->chip->test_regmap, 0x43, 0x10);
	regmap_write(info->chip->test_regmap, 0x46, 0x04);

	/* enable charging */
	regmap_update_bits(info->chip->battery_regmap, PM88X_CHG_CONFIG1,
		PM88X_CHG_ENABLE, PM88X_CHG_ENABLE);

	/* Add delay to allow enabling of the charger IC */
	msleep(20);

	regmap_read(info->chip->battery_regmap, PM88X_CHG_STATUS2, &stat2);
	dev_info(info->dev, "%s : PM88X_CHG_STATUS2 : 0x%02x\n", __func__, stat2);

	/* clear the previous settings */
	regmap_write(info->chip->test_regmap, 0x40, 0x00);
	regmap_write(info->chip->test_regmap, 0x43, 0x00);
	regmap_write(info->chip->test_regmap, 0x46, 0x00);

	/* close test page */
	regmap_write(info->chip->base_regmap, 0x1F, 0x00);

	pr_info("%s : Enable Charger!\n", __func__);

	return 0;
}

static int pm88x_stop_charging(struct pm88x_charger_info *info)
{
	unsigned int data;

	printk("enter %s \n",__func__);
	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return 0;
	}

	dev_info(info->dev, "%s\n", __func__);

	info->is_fullcharged = false;

	mutex_lock(&info->lock);
	regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG1, &data);
	/* enable USB suspend */
	data |= PM88X_USB_SUSP;
	regmap_write(info->chip->battery_regmap, PM88X_CHG_CONFIG1, data);

	/* disable USB suspend and stop charging */
	data &= ~(PM88X_USB_SUSP | PM88X_CHG_ENABLE);
	regmap_write(info->chip->battery_regmap, PM88X_CHG_CONFIG1, data);

	mutex_unlock(&info->lock);
	regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG1, &data);
	pr_info("%s : Disable Charger! PM88X_CHG_CONFIG1(0x%x) = 0x%x\n",
		__func__, PM88X_CHG_CONFIG1, data);

	return 0;
}

static void pm88x_otg_check_errors(struct pm88x_charger_info *info)
{
	int val = 0;
#ifdef CONFIG_USB_HOST_NOTIFY
	struct otg_notify *o_notify;
	o_notify = get_otg_notify();
#endif

	regmap_read(info->chip->battery_regmap, PM88X_OTG_LOG1, &val);
	dev_err(info->chip->dev, "%s: OTG error checking: 0x%x\n",
		__func__, val);

	if (val & PM88X_OTG_UVVBAT)
		dev_info(info->chip->dev, "%s: OTG error: OTG_UVVBAT: 0x%x\n",
			__func__, val);
	if (val & PM88X_OTG_SHORT_DET) {
		dev_info(info->chip->dev, "%s OTG error: OTG_SHORT_DET: 0x%x\n",
			__func__, val);
#ifdef CONFIG_USB_HOST_NOTIFY
		send_otg_notify(o_notify, NOTIFY_EVENT_OVERCURRENT, 1);
#endif
		otg_enable_flag = 0;
		regmap_update_bits(info->chip->battery_regmap, PM88X_CHG_CONFIG1,
			PM88X_USB_OTG, 0);
	}

	/* Clear OTG Log 1 register*/
	if (val)
		regmap_write(info->chip->battery_regmap, PM88X_OTG_LOG1, val);
}

static void pm88x_otg_control(struct pm88x_charger_info *info, int enable)
{
	unsigned int data;

	mutex_lock(&info->lock);
	if (enable) {
		regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG1, &data);
		/* enable USB suspend */
		data |= PM88X_USB_SUSP;
		regmap_write(info->chip->battery_regmap, PM88X_CHG_CONFIG1, data);

		/* disable USB suspend and stop charging */
		data &= ~(PM88X_USB_SUSP | PM88X_CHG_ENABLE);
		regmap_write(info->chip->battery_regmap, PM88X_CHG_CONFIG1, data);

		pr_info("%s : Disable Charger for OTG! PM88X_CHG_CONFIG1(0x%x) = 0x%x\n",
			__func__, PM88X_CHG_CONFIG1, data);

		otg_enable_flag = 1;
		regmap_update_bits(info->chip->battery_regmap, PM88X_CHG_CONFIG1,
					PM88X_USB_OTG, PM88X_USB_OTG);
		dev_info(info->dev, "%s: OTG Enabled \n", __func__);

		info->cable_type = POWER_SUPPLY_TYPE_OTG;

		regmap_read(info->chip->battery_regmap, PM88X_BOOST_CONFIG1, &data);
		dev_info(info->dev, "%s: PM88X_BOOST_CONFIG1: (0x%x) = 0x%x\n",
			__func__, PM88X_BOOST_CONFIG1, data);
	} else {
		otg_enable_flag = 0;
		regmap_update_bits(info->chip->battery_regmap, PM88X_CHG_CONFIG1,
					PM88X_USB_OTG, 0);
		pm88x_start_charging(info);

		info->cable_type = POWER_SUPPLY_TYPE_BATTERY;

		dev_info(info->dev, "%s: OTG Disabled \n", __func__);
	}
	mutex_unlock(&info->lock);

	regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG1, &data);
	dev_info(info->dev, "%s: PM88X_CHG_CONFIG1(0x%x) = 0x%x\n",
		__func__, PM88X_CHG_CONFIG1, data);
	data &= PM88X_USB_OTG;
	if (enable && !data) {
		dev_err(info->chip->dev, "%s: vbus set failed!\n", __func__);
		pm88x_otg_check_errors(info);
		regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG4, &data);
		dev_info(info->dev, "%s: PM88X_CHG_CONFIG4(0x%x) = 0x%x\n",
			__func__, PM88X_CHG_CONFIG4, data);
	}
}

static int pm88x_charger_init(struct pm88x_charger_info *info)
{
	unsigned int mask, data;

	switch (info->chip->type) {
	case PM886:
		/* WA for A0 to prevent OV_VBAT fault and support dead battery case */
		if (info->chip->chip_id == PM886_A0) {
			/* open test page */
			regmap_write(info->chip->base_regmap, 0x1F, 0x1);
			/* change defaults to disable OV_VBAT */
			regmap_write(info->chip->test_regmap, 0x50, 0x2A);
			regmap_write(info->chip->test_regmap, 0x51, 0x0C);
			/* change defaults to enable charging */
			regmap_write(info->chip->test_regmap, 0x52, 0x28);
			regmap_write(info->chip->test_regmap, 0x53, 0x01);
			/* change defaults to disable OV_VSYS1 and UV_VSYS1 */
			regmap_write(info->chip->test_regmap, 0x54, 0x23);
			regmap_write(info->chip->test_regmap, 0x55, 0x14);
			regmap_write(info->chip->test_regmap, 0x58, 0xbb);
			regmap_write(info->chip->test_regmap, 0x59, 0x08);
			/* close test page */
			regmap_write(info->chip->base_regmap, 0x1F, 0x0);
			/* disable OV_VBAT */
			regmap_update_bits(info->chip->battery_regmap, PM88X_CHG_CONFIG3,
					   PM88X_OV_VBAT_EN, 0);
			/* disable OV_VSYS1 and UV_VSYS1 */
			regmap_write(info->chip->base_regmap, PM88X_LOWPOWER4, 0x14);
		}
		break;
	default:
		break;
	}

	/* disable charger watchdog */
	regmap_update_bits(info->chip->battery_regmap, PM88X_CHG_CONFIG1,
			   PM88X_CHG_WDT_EN, 0);

	/* config pre-charge parameters */
	mask = PM88X_ICHG_PRE_SET_MASK | PM88X_VBAT_PRE_TERM_SET_MASK;
	data = get_prechg_cur(info) << PM88X_ICHG_PRE_SET_OFFSET
		| get_prechg_vol(info) << PM88X_VBAT_PRE_TERM_SET_OFFSET;
	regmap_update_bits(info->chip->battery_regmap, PM88X_PRE_CONFIG1,
			   mask, data);

	/*
	 * set wall-adaptor threshold to 3.9V:
	 * on PM886 the value is the maximum (0x3),
	 * on PM880 the value is the minimum (0x0)
	 */
	data = (info->chip->type == PM886 ? PM88X_WA_TH_SET_MASK : 0x0);
	regmap_update_bits(info->chip->battery_regmap, PM88X_MPPT_CONFIG3,
			   PM88X_WA_TH_SET_MASK, data);

	/* Set RECHG_THR_SET to 100mV to avoid the auto-recharge operation from
	 * starting before the set thresholds in SW.
	 * Also set the PM88X_FASTCHG_TOUT_SET to maximum (16hours) */
	regmap_write(info->chip->battery_regmap, PM88X_TIMER_CONFIG, 0x17);
	regmap_read(info->chip->battery_regmap, PM88X_TIMER_CONFIG, &data);
	dev_dbg(info->dev, "%s: PM88X_TIMER_CONFIG(0x%x) = 0x%x\n",
		__func__, PM88X_TIMER_CONFIG, data);

	/* Disable regular end of charge (register 0x28 bit 3) */
	regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG1, &data);
	if (info->chip->type == PM886)
		mask = data | 0x28;
	else
		mask = data | 0x08;
	regmap_write(info->chip->battery_regmap, PM88X_CHG_CONFIG1, mask);
	regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG1, &data);
	dev_dbg(info->dev, "%s: PM88X_CHG_CONFIG1(0x%x) = 0x%x\n",
		__func__, PM88X_CHG_CONFIG1, data);

	/* Set float voltage */
	pr_debug("%s : float voltage (%dmV)\n",
		__func__, info->pdata->chg_float_voltage);
	pm88x_set_batreg_voltage(
		info, info->pdata->chg_float_voltage);

	regmap_read(info->chip->battery_regmap, PM88X_BOOST_CONFIG1, &data);
	dev_dbg(info->dev, "%s: PM88X_BOOST_CONFIG1: (0x%x) = 0x%x\n",
		__func__, PM88X_BOOST_CONFIG1, data);
	data &= 0xf8;
	data |= 0x05;
	regmap_write(info->chip->battery_regmap, PM88X_BOOST_CONFIG1, data);
	dev_dbg(info->dev, "%s: PM88X_BOOST_CONFIG1: (0x%x) = 0x%x\n",
		__func__, 0x6B, data);

	regmap_read(info->chip->battery_regmap, PM88X_BOOST_CONFIG5, &data);
	dev_dbg(info->dev, "%s: PM88X_BOOST_CONFIG5 (0x%x) = 0x%x\n",
		__func__, PM88X_BOOST_CONFIG5, data);

	return 0;
}

static irqreturn_t pm88x_chg_fail_handler(int irq, void *data)
{
	static int value;
	struct pm88x_charger_info *info = data;

	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return IRQ_NONE;
	}

	/* charging is stopped by HW */
	dev_info(info->dev, "charge fail interrupt is served\n");

	regmap_read(info->chip->battery_regmap, PM88X_CHG_LOG1, &value);

	if (value & PM88X_BATT_REMOVAL)
		dev_info(info->chip->dev, "battery is plugged out.\n");

	if (value & PM88X_CHG_REMOVAL)
		dev_info(info->chip->dev, "charger cable is plugged out.\n");

	if (value & PM88X_BATT_TEMP_NOK) {
		dev_err(info->chip->dev, "battery temperature is abnormal.\n");
		/* handled in battery driver */
	}

	if (value & PM88X_CHG_WDT_EXPIRED) {
		dev_err(info->dev, "charger WDT expired! restart charging in %d min\n",
				CHG_RESTART_DELAY);
	}

	if (value & PM88X_OV_VBAT) {
		dev_err(info->chip->dev, "battery voltage is abnormal.\n");
	}

	if (value & PM88X_CHG_TIMEOUT) {
		dev_err(info->chip->dev, "charge timeout.\n");
	}

	if (value & PM88X_OV_ITEMP)
		/* handled in a dedicated interrupt */
		dev_err(info->chip->dev, "internal temperature abnormal.\n");

	/* write to clear */
	regmap_write(info->chip->battery_regmap, PM88X_CHG_LOG1, value);

	return IRQ_HANDLED;
}

static void pm88x_vbus_det_irq_thread(struct work_struct *work)
{
	union power_supply_propval val, value, bat_health, present;
	struct pm88x_charger_info *info =
		container_of(work, struct pm88x_charger_info, isr_work.work);
	static int cfg1, stat2, stat3;

	regmap_read(info->chip->battery_regmap, PM88X_CHG_STATUS2, &stat2);
	regmap_read(info->chip->battery_regmap, PM88X_CHG_STATUS3, &stat3);
	regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG1, &cfg1);

	dev_info(info->dev, "%s : PM88X_CHG_STATUS2 : 0x%02x\n", __func__, stat2);
	dev_info(info->dev, "%s : PM88X_CHG_STATUS3 : 0x%02x\n", __func__, stat3);
	dev_info(info->dev, "%s : PM88X_CHG_CONFIG1 : 0x%02x\n", __func__, cfg1);

	if (!sec_chg_get_property(&info->psy_chg,
				POWER_SUPPLY_PROP_HEALTH, &val))
				return;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_HEALTH, bat_health);

	switch (val.intval) {
	case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		psy_do_property("battery", get,
			POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: CABLE TYPE IN OVP: %d\n", __func__, value.intval);
		if (bat_health.intval != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
			if (value.intval != POWER_SUPPLY_TYPE_BATTERY) {
				pr_info("%s: Interrupted by OVP\n", __func__);
				psy_do_property("battery", set,
					POWER_SUPPLY_PROP_HEALTH, val);
			}
		break;

	case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
		psy_do_property("battery", get,
			POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: CABLE TYPE IN UVLO: %d\n", __func__, value.intval);
		if (bat_health.intval != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE)
			if (value.intval != POWER_SUPPLY_TYPE_BATTERY) {
				pr_info("%s: Interrupted by UVLO\n", __func__);
				psy_do_property("battery", set,
					POWER_SUPPLY_PROP_HEALTH, val);
			}
		break;

	case POWER_SUPPLY_HEALTH_GOOD:
		/* Need to check for battery presence
		 * in case timing issue causes ISR to force set device's health to good */
		psy_do_property("sec-fuelgauge", get,
			POWER_SUPPLY_PROP_PRESENT, present);
		/* Also check other possible battery health to prevent resetting of health */
		if ((present.intval) &&
			((bat_health.intval != POWER_SUPPLY_HEALTH_COLD) &&
			(bat_health.intval != POWER_SUPPLY_HEALTH_OVERHEAT) &&
			(bat_health.intval != POWER_SUPPLY_HEALTH_OVERHEATLIMIT))) {
			pr_err("%s: Interrupted but Good\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, val);
		} else {
			pr_err("%s: Battery missing / maintain status\n", __func__);
		}
		break;

	case POWER_SUPPLY_HEALTH_UNKNOWN:
	default:
		pr_err("%s: Invalid Charger Health\n", __func__);
		break;
	}
}

static irqreturn_t  pm88x_vbus_int_handler(int irq, void *irq_data)
{
	struct pm88x_charger_info *info = irq_data;
	pr_info("*** %s ***\n", __func__);
	/* Delay the interrupt to prevent MUIC timing issues */
	schedule_delayed_work(&info->isr_work, HZ * 0.25);
	return IRQ_HANDLED;
}

static irqreturn_t pm88x_chg_done_handler(int irq, void *data)
{
	struct pm88x_charger_info *info = data;
	union power_supply_propval val, present, swelling_mode;

	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return IRQ_NONE;
	}

	dev_info(info->dev, "charge done interrupt is served\n");
	psy_do_property("sec-fuelgauge", get,
		POWER_SUPPLY_PROP_PRESENT, present);
	dev_dbg(info->dev, "%s : Battery Presence : %d\n", __func__, present.intval);
	if (present.intval) {
		int vcell, soc;
		swelling_mode.intval = 0;
#if defined(CONFIG_BATTERY_SWELLING)
		/* check if swelling protection is enabled */
		psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, swelling_mode);
#endif
		/* check VCELL for premature full interrupt HW request */
		psy_do_property("sec-fuelgauge", get,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, val);

		if (swelling_mode.intval) {
			dev_info(info->chip->dev,
					"%s : EOC while Swelling Protection\n", __func__);
			vcell = info->pdata->full_condition_vcell;
		} else
			vcell = val.intval;

		psy_do_property("sec-fuelgauge", get,
			POWER_SUPPLY_PROP_CAPACITY, val);
		soc = val.intval;
		dev_info(info->chip->dev,
			"%s : vcell: %d soc: %d\n", __func__, vcell, soc);
		if (vcell >= info->pdata->full_condition_vcell) {
			info->is_fullcharged = true;
			val.intval= POWER_SUPPLY_STATUS_FULL;
			sec_chg_set_property(&info->psy_chg,
				POWER_SUPPLY_PROP_STATUS, &val);
			dev_info(info->chip->dev,
				"%s : EOC occured, Power Supply Full from HW.\n", __func__);
			if (soc < info->pdata->full_condition_soc)
				pr_info("%s: Waiting for FG to ramp-up...\n", __func__);
		} else {
			dev_info(info->chip->dev,
				"%s : Not enough VCELL.\n", __func__);
			return IRQ_HANDLED;
		}
	} else {
		dev_info(info->chip->dev,
			"%s : Battery missing in full interrupt time.\n", __func__);
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_PRESENT, present);
	}
	return IRQ_HANDLED;
}

int sec_chg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct pm88x_charger_info *charger =
		container_of(psy, struct pm88x_charger_info, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->is_charging;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (charger->is_fullcharged) {
			val->intval = POWER_SUPPLY_STATUS_FULL;
			pm88x_get_charging_status(charger, 1);
		} else
			val->intval = pm88x_get_charging_status(charger, 1);
		break;
	case POWER_SUPPLY_PROP_STATUS_FG:
		if (charger->is_fullcharged) {
			val->intval = POWER_SUPPLY_STATUS_FULL;
		} else
			val->intval = pm88x_get_charging_status(charger, 0);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = pm88x_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		/*val->intval = pm88x_get_battery_present(charger);*/
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = charger->charging_current;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_fastchg_cur(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!charger->is_charging)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		return -ENODATA;
		break;
#if defined(CONFIG_BATTERY_SWELLING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = pm88x_get_batreg_voltage(charger);
		break;
#endif
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = charger->is_charging;
		break;
	default:
		return -EINVAL;
	}
	return 1;
}

int sec_chg_set_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      const union power_supply_propval *val)
{
	struct pm88x_charger_info *charger =
		container_of(psy, struct pm88x_charger_info, psy_chg);
	union power_supply_propval value;
	union power_supply_propval chg_mode;
	int topoff;
	unsigned int data;

	int set_charging_current, set_charging_current_lim;
	const int usb_charging_current = charger->pdata->charging_current[
		POWER_SUPPLY_TYPE_USB].fast_charging_current;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;

		if (val->intval == POWER_SUPPLY_TYPE_POWER_SHARING) {
			psy_do_property("ps", get,
				POWER_SUPPLY_PROP_STATUS, value);
			if (value.intval) {
				pr_info("%s : OTG ON notification\n", __func__);
				pm88x_otg_control(charger, 1);
			} else {
				pr_info("%s : OTG OFF notification\n", __func__);
				pm88x_otg_control(charger, 0);
			}
			break;
		}

		regmap_read(charger->chip->battery_regmap, PM88X_TIMER_CONFIG, &data);
		/* Set auto RECHG_THR_SET to 100mV */
		data &= 0xcf;
		data |= 0x10;
		regmap_write(charger->chip->battery_regmap, PM88X_TIMER_CONFIG, data);

		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_HEALTH, value);
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			charger->is_charging = false;
			set_charging_current = 0;
			charger->is_fullcharged = false;

			/* Set topoff current */
			pr_info("%s : topoff current (%dmA)\n",
				__func__, charger->pdata->charging_current[
				POWER_SUPPLY_TYPE_USB].full_check_current_1st);
			pm88x_set_topoff(
				charger, charger->pdata->charging_current[
				POWER_SUPPLY_TYPE_USB].full_check_current_1st);

			/* Set fast charge current */
			pr_info("%s : fast charging current (%dmA), siop_level=%d\n",
				__func__, charger->charging_current, charger->siop_level);
			pm88x_set_fastchg_current(
				charger, charger->charging_current);

			/* Set input current limit */
			pr_info("%s : vbus current limit (%dmA)\n",
				__func__, charger->pdata->charging_current[
				POWER_SUPPLY_TYPE_USB].input_current_limit);
			pm88x_set_vbuslimit_current(
				charger, charger->pdata->charging_current[
				POWER_SUPPLY_TYPE_USB].input_current_limit);
		} else {
			charger->is_charging = true;
			charger->charging_current =
					charger->pdata->charging_current
					[charger->cable_type].fast_charging_current;

			/* Selecting termination current */
			if (charger->pdata->full_check_type_2nd == SEC_BATTERY_FULLCHARGED_CHGINT) {
				psy_do_property("battery", get,
						POWER_SUPPLY_PROP_CHARGE_NOW,
						chg_mode);
				if (chg_mode.intval == SEC_BATTERY_CHARGING_2ND) {
					pm88x_stop_charging(charger);
					topoff = charger->pdata->charging_current[
				charger->cable_type].full_check_current_2nd;
				} else {
					topoff = charger->pdata->charging_current[
				charger->cable_type].full_check_current_1st;
				}
			} else {
				topoff = charger->pdata->charging_current[
				charger->cable_type].full_check_current_1st;
			}

			/* Set topoff current */
			pr_info("%s : topoff current (%dmA)\n",
				__func__, topoff);
			pm88x_set_topoff(charger, topoff);

			/* Set fast charge current */
			pr_info("%s : fast charging current (%dmA), siop_level=%d\n",
				__func__, charger->charging_current, charger->siop_level);
			pm88x_set_fastchg_current(
				charger, charger->charging_current);

			/* Set input current limit, account for SIOP level sert before cable inserted */
			if (charger->siop_level < 100){
				set_charging_current_lim = charger->pdata->charging_current
								[charger->cable_type].input_current_limit;
				set_charging_current =
					set_charging_current_lim * charger->siop_level / 100;
				if (set_charging_current > 0 &&
						set_charging_current < usb_charging_current)
					set_charging_current = usb_charging_current;

				pr_info("%s: Current Limit after SIOP set level: %d \n",
					__func__, set_charging_current);
				pm88x_set_vbuslimit_current(
					charger, set_charging_current);
			} else {
				pr_info("%s : vbus current limit (%dmA)\n",
					__func__, charger->pdata->charging_current
					[charger->cable_type].input_current_limit);
				pm88x_set_vbuslimit_current(
					charger, charger->pdata->charging_current
					[charger->cable_type].input_current_limit);
			}
		}

		/* if battery is removed, put vbus current limit to minimum */
		if ((value.intval == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) ||
		    (value.intval == POWER_SUPPLY_HEALTH_OVERHEATLIMIT))
			pm88x_set_vbuslimit_current(charger, 100);

		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		pm88x_set_fastchg_current(charger,
				val->intval);
		pm88x_set_vbuslimit_current(charger,
				val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		/* decrease the input current limit according to siop level */
		set_charging_current_lim = charger->pdata->charging_current
						[charger->cable_type].input_current_limit;
		charger->siop_level = val->intval;
		set_charging_current =
			set_charging_current_lim * charger->siop_level / 100;
		if (set_charging_current > 0 &&
				set_charging_current < usb_charging_current)
			set_charging_current = usb_charging_current;

		pr_info("%s: Current Limit after SIOP set level: %d \n",
			__func__, set_charging_current);
		pm88x_set_vbuslimit_current(
			charger, set_charging_current);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		charger_health = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		if (val->intval == 1) {
			pr_info("%s : OTG ON notification\n", __func__);
			pm88x_otg_control(charger, 1);
		} else {
			pr_info("%s : OTG OFF notification\n", __func__);
			pm88x_otg_control(charger, 0);
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
#if defined(CONFIG_BATTERY_SWELLING)
		regmap_read(charger->chip->battery_regmap, PM88X_TIMER_CONFIG, &data);
		/* Set auto RECHG_THR_SET to 150mV */
		data &= 0xcf;
		data |= 0x20;
		regmap_write(charger->chip->battery_regmap, PM88X_TIMER_CONFIG, data);

		if (val->intval > charger->pdata->charging_current
			[charger->cable_type].fast_charging_current) {
			break;
		}
#endif
		pr_info("%s: Current Limit after low temp swelling: %d \n",
			__func__, val->intval);
		pm88x_set_vbuslimit_current(
			charger, val->intval);
		break;
#if defined(CONFIG_BATTERY_SWELLING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("%s: Float Voltage(%d)\n", __func__, val->intval);
		pm88x_set_batreg_voltage(charger, val->intval);
		break;
#endif
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		pr_info("%s: Always enable charger, enable value:%d\n",
			__func__, val->intval);
		charger->is_charging = val->intval;
		if (val->intval) {
			/* Delay charging start for 300ms.
			 * This is a fix for a HW controlled charging block issue at battery voltages
			 * that are not under the auto-recharge theshold, 50mV below regulation voltage */
			msleep(300);
			pm88x_start_charging(charger);
		} else {
			charger->is_fullcharged = false;
			pm88x_stop_charging(charger);
		}
		break;
	default:
		return -EINVAL;
	}
	return 1;
}

static int pm88x_otg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = otg_enable_flag;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int pm88x_otg_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	union power_supply_propval value;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		value.intval = val->intval;
		pr_info("%s: OTG %s\n", __func__, value.intval > 0 ? "on" : "off");
		psy_do_property("sec-charger", set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL, value);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static struct pm88x_irq_desc {
	const char *name;
	irqreturn_t (*handler)(int irq, void *data);
} pm88x_irq_descs[] = {
	{"charge fail", pm88x_chg_fail_handler},
	{"charge done", pm88x_chg_done_handler},
	{"vbus int", pm88x_vbus_int_handler},
};

static int pm88x_charger_dt_init(struct device_node *np,
			     struct pm88x_charger_info *info)
{
	int ret;

	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "prechg-current", &info->prechg_cur);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "prechg-voltage", &info->prechg_vol);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ir-comp-res", &info->ir_comp_res);
	if (ret)
		info->ir_comp_res = 0;

	ret = of_property_read_u32(np, "ir-comp-update", &info->ir_comp_update);
	if (ret)
		/* set IR compensation update time as 1s default */
		info->ir_comp_update = 1;

	return 0;
}

static int sec_chg_parse_dt(struct device_node *np,
			 struct device *dev,
			 sec_battery_platform_data_t *pdata)
{
	int ret = 0, len = 0;

	if (!np)
		return -EINVAL;

	np = of_find_node_by_name(NULL, "sec-battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		int i = 0;
		const u32 *p;
		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p) {
			return 1;
		}

		len = len / sizeof(u32);

		pdata->charging_current = kzalloc(sizeof(sec_charging_current_t) * len,
				GFP_KERNEL);

		for(i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
				"battery,input_current_limit", i,
				&pdata->charging_current[i].input_current_limit);
			if (ret)
				pr_info("%s : Input_current_limit is Empty\n",
					__func__);

			ret = of_property_read_u32_index(np,
				"battery,fast_charging_current", i,
				&pdata->charging_current[i].fast_charging_current);
			if (ret)
				pr_info("%s : Fast charging current is Empty\n",
						__func__);

			ret = of_property_read_u32_index(np,
				"battery,full_check_current_1st", i,
				&pdata->charging_current[i].full_check_current_1st);
			if (ret)
				pr_info("%s : Full check current 1st is Empty\n",
						__func__);

			ret = of_property_read_u32_index(np,
				"battery,full_check_current_2nd", i,
				&pdata->charging_current[i].full_check_current_2nd);
			if (ret)
				pr_info("%s : Full check current 2nd is Empty\n",
						__func__);
		}
	}

	ret = of_property_read_u32(np, "battery,chg_float_voltage",
					&pdata->chg_float_voltage);
	if (ret)
		pr_info("%s : chg_float_voltage is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,ovp_uvlo_check_type",
			&pdata->ovp_uvlo_check_type);
	if (ret)
		pr_info("%s : Ovp Uvlo check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_check_type",
			&pdata->full_check_type);
	if (ret)
		pr_info("%s : Full check type is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_check_type_2nd",
			&pdata->full_check_type_2nd);
	if (ret)
		pr_info("%s : Full check type 2nd is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_condition_vcell",
			&pdata->full_condition_vcell);
	if (ret)
		pr_info("%s : Full condition vcell is Empty\n", __func__);

	ret = of_property_read_u32(np, "battery,full_condition_soc",
			&pdata->full_condition_soc);
	if (ret)
		pr_info("%s : Full condition SOC is Empty\n", __func__);

	if (of_get_property(np, "battery,always_enable", NULL))
		pdata->always_enable = true;
	else
		pdata->always_enable = false;

	pdata->check_cable_result_callback =
		sec_bat_check_cable_result_callback;

	return 0;
}

static int pm88x_charger_probe(struct platform_device *pdev)
{
	struct pm88x_charger_info *info;
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm88x_platform_data *pdata = dev_get_platdata(chip->dev);
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;
	int i, j;
	otg_enable_flag = 0;

	pr_info("%s: 88PM88X Charger Probe Start\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(struct pm88x_charger_info),
			GFP_KERNEL);

	if (!info) {
		dev_err(&pdev->dev, "Cannot allocate memory.\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}

	info->dev = &pdev->dev;
	info->chip = chip;

    if (pdev->dev.of_node) {
		info->pdata = devm_kzalloc(&pdev->dev, sizeof(*(info->pdata)),
			GFP_KERNEL);
		if (!info->pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_parse_dt_nomem;
		}
		ret = sec_chg_parse_dt(pdev->dev.of_node, &pdev->dev, info->pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		info->pdata = pdata->battery_data;
	}

	ret = pm88x_charger_dt_init(node, info);
	if (ret) {
		dev_info(info->dev,"%s: Cannot parse dt file\n", __func__);
		return ret;
	}

	mutex_init(&info->lock);
	platform_set_drvdata(pdev, info);

	if (info->pdata->charger_name == NULL)
		info->pdata->charger_name = "sec-charger";

	info->psy_chg.name				= info->pdata->charger_name;
	info->psy_chg.type				= POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_chg.get_property		= sec_chg_get_property;
	info->psy_chg.set_property		= sec_chg_set_property;
	info->psy_chg.properties		= sec_charger_props;
	info->psy_chg.num_properties	= ARRAY_SIZE(sec_charger_props);
	info->psy_otg.name				= "otg";
	info->psy_otg.type				= POWER_SUPPLY_TYPE_OTG;
	info->psy_otg.get_property		= pm88x_otg_get_property;
	info->psy_otg.set_property		= pm88x_otg_set_property;
	info->psy_otg.properties		= pm88x_otg_props;
	info->psy_otg.num_properties	= ARRAY_SIZE(pm88x_otg_props);

	info->siop_level = 100;

	for (i = 0, j = 0; i < pdev->num_resources; i++) {
		info->irq[j] = platform_get_irq(pdev, i);
		if (info->irq[j] < 0)
			continue;
		j++;
	}
	info->irq_nums = j;

	ret = pm88x_charger_init(info);
	if (ret < 0) {
		dev_err(info->dev, "%s: Charger initialization fails.\n", __func__);
		return ret;
	}

    ret = power_supply_register(&pdev->dev, &info->psy_chg);
    if (ret) {
            dev_err(info->dev, "%s: Register power_supply fail!\n", __func__);
            goto err_power_supply_register;
    }

	ret = power_supply_register(&pdev->dev, &info->psy_otg);
	if (ret) {
		pr_err("%s: Failed to Register otg_chg\n", __func__);
		goto err_power_supply_register_otg;
	}

	INIT_DEFERRABLE_WORK(
		&info->isr_work, pm88x_vbus_det_irq_thread);

	/* interrupt should be request in the last stage */
	for (i = 0; i < info->irq_nums; i++) {
		ret = devm_request_threaded_irq(info->dev, info->irq[i], NULL,
						pm88x_irq_descs[i].handler,
						IRQF_ONESHOT | IRQF_NO_SUSPEND,
						pm88x_irq_descs[i].name, info);
		if (ret < 0) {
			dev_err(info->dev, "failed to request IRQ: #%d: %d\n",
				info->irq[i], ret);
			goto out_irq;
		}
	}

	dev_info(info->dev, "%s is successful!\n", __func__);

	return 0;

out_irq:
	while (--i >= 0)
		devm_free_irq(info->dev, info->irq[i], info);
err_power_supply_register_otg:
	power_supply_unregister(&info->psy_otg);
err_power_supply_register:
	power_supply_unregister(&info->psy_chg);
err_parse_dt:
	pr_info("%s: err_parse_dt error \n", __func__);
err_parse_dt_nomem:
	kfree(info);
	return ret;
}

static int pm88x_charger_remove(struct platform_device *pdev)
{
	int i;
	unsigned int data;
	struct pm88x_charger_info *info = platform_get_drvdata(pdev);
	if (!info)
		return 0;

	for (i = 0; i < info->irq_nums; i++) {
		if (pm88x_irq_descs[i].handler != NULL)
			devm_free_irq(info->dev, info->irq[i], info);
	}

	regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG1, &data);
	if (data & 0x80) {
		/* If the OTG bit is set, clear it to allow re-enabling of OTG after reboot */
		regmap_update_bits(info->chip->battery_regmap, PM88X_CHG_CONFIG1,
			PM88X_USB_OTG, 0);
	}

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void pm88x_charger_shutdown(struct platform_device *pdev)
{
	pm88x_charger_remove(pdev);
	return;
}

static const struct of_device_id pm88x_chg_dt_match[] = {
	{ .compatible = "marvell,88pm88x-charger", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm88x_chg_dt_match);

static struct platform_driver pm88x_charger_driver = {
	.probe = pm88x_charger_probe,
	.remove = pm88x_charger_remove,
	.shutdown = pm88x_charger_shutdown,
	.driver = {
		.owner = THIS_MODULE,
		.name = "88pm88x-charger",
		.of_match_table = of_match_ptr(pm88x_chg_dt_match),
	},
};

module_platform_driver(pm88x_charger_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("88pm88x Charger Driver");
MODULE_ALIAS("platform:88pm88x-charger");
MODULE_AUTHOR("Samsung Electronics");
MODULE_AUTHOR("Yi Zhang <yizhang@marvell.com>");
MODULE_AUTHOR("Shay Pathov <shayp@marvell.com>");
