/*
 * 88PM886 PMIC Charger driver
 *
 * Copyright (c) 2014 Marvell Technology Ltd.
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
#include <linux/mfd/88pm886.h>
#include <linux/of_device.h>
#include <linux/platform_data/mv_usb.h>
#include <linux/i2c.h>
#include <linux/battery/sec_charger.h>

#include <linux/gpio-pxa.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of.h>

static int charger_health = POWER_SUPPLY_HEALTH_GOOD;

int otg_enable_flag;

extern int sec_chg_dt_init(struct device_node *np,
			 struct device *dev,
			 sec_battery_platform_data_t *pdata);

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
		POWER_SUPPLY_PROP_CURRENT_AVG,
		POWER_SUPPLY_PROP_CHARGING_ENABLED,
#endif
};

static int pm886_charger_init(struct pm886_charger_info *info);

static int pm886_get_charging_status(struct pm886_charger_info *info)
{
	static int cfg1, stat2, stat3;
	int status = POWER_SUPPLY_STATUS_UNKNOWN;

	regmap_read(info->chip->battery_regmap, PM886_CHG_STATUS2, &stat2);
	regmap_read(info->chip->battery_regmap, PM886_CHG_STATUS3, &stat3);
	regmap_read(info->chip->battery_regmap, PM886_CHG_CONFIG1, &cfg1);

	dev_dbg(info->dev, "%s : PM886_CHG_STATUS2 : 0x%02x\n", __func__, stat2);
	dev_dbg(info->dev, "%s : PM886_CHG_STATUS3 : 0x%02x\n", __func__, stat3);
	dev_dbg(info->dev, "%s : PM886_CHG_CONFIG1 : 0x%02x\n", __func__, cfg1);

	if ((cfg1 & PM886_CHG_ENABLE) && ((stat2 & PM886_CHG_TRICLE) ||
		(stat2 & PM886_CHG_PRECHG) || (stat2 & PM886_CHG_FASTCHG))) {
		if (charger_health == POWER_SUPPLY_HEALTH_GOOD)
			status = POWER_SUPPLY_STATUS_CHARGING;
		else
			status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else {
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	return (int)status;
}

static int pm886_get_charging_health(struct pm886_charger_info *info)
{
	static int health = POWER_SUPPLY_HEALTH_GOOD;
	static int stat2;

	regmap_read(info->chip->battery_regmap, PM886_CHG_STATUS2, &stat2);

	dev_info(info->dev, "%s : PM886_CHG_STATUS2 : 0x%02x\n", __func__, stat2);

	if ((stat2 & PM886_VBUS_SW_OV) && (stat2 & PM886_USB_LIM_OK)){
		dev_info(info->dev, "%s : OVP bit set but health status normal\n",
			__func__);
		health = POWER_SUPPLY_HEALTH_GOOD;
		charger_health = POWER_SUPPLY_HEALTH_GOOD;
	} else if ((stat2 & PM886_VBUS_SW_OV) &&
				!((stat2 & PM886_CHG_TRICLE) ||	(stat2 & PM886_CHG_PRECHG)
				|| (stat2 & PM886_CHG_FASTCHG))){
		dev_info(info->dev, "%s : Health OVP \n", __func__);
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if ((stat2 & (PM886_VBUS_SW_ON)) && (!(stat2 & (PM886_USB_LIM_OK))) &&
				(!((stat2 & PM886_CHG_TRICLE) || (stat2 & PM886_CHG_PRECHG) ||
					(stat2 & PM886_CHG_FASTCHG)))){
		dev_info(info->dev, "%s : Health UVLO \n", __func__);
		health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
		charger_health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
	} else if ((stat2 & PM886_VBUS_SW_ON) || (stat2 & PM886_USB_LIM_OK)){
		health = POWER_SUPPLY_HEALTH_GOOD;
		charger_health = POWER_SUPPLY_HEALTH_GOOD;
	}

	return (int)health;
}

static inline int pm886_set_batreg_voltage(
		struct pm886_charger_info *info, unsigned int batreg_voltage)
{
	unsigned int float_voltage;

	/* fastcharge voltage range is 3.6V - 4.5V */
	if (batreg_voltage > 4500)
		batreg_voltage = 4500;
	else if (batreg_voltage < 3600)
		batreg_voltage = 3600;

	float_voltage = (batreg_voltage - 3600) * 10 / 125;

	regmap_update_bits(info->chip->battery_regmap, PM886_FAST_CONFIG1,
		   PM886_VBAT_FAST_SET_MASK, float_voltage);

	dev_dbg(info->dev, "%s: float voltage (0x%x) = 0x%x\n",
		__func__, PM886_VBAT_FAST_SET_MASK, float_voltage);

	return float_voltage;
}

#if defined(CONFIG_BATTERY_SWELLING)
static u8 pm886_get_batreg_voltage(struct pm886_charger_info *info)
{
	unsigned int reg_data = 0;

	regmap_read(info->chip->battery_regmap, PM886_FAST_CONFIG1, &reg_data);
	pr_info("%s: battery cv voltage 0x%x\n", __func__, reg_data);
	return reg_data;
}
#endif

static inline int pm886_set_vbuslimit_current(
	struct pm886_charger_info *info, unsigned int input_current)
{
	unsigned int ilim_raw, ilim_fine, ilim;

	/* maximum current limit is 1600mA */
	if (input_current > 1600)
		input_current = 1600;

	/* ilim is integer multiples of 90, 88.75, 87.5 ,86.25, 85*/
	if (input_current <= 100) {
		ilim_raw = PM886_CHG_ILIM_RAW_100;
		ilim_fine = PM886_CHG_ILIM_FINE_10;
		/* Set ilim to 90mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 500) {
		// Typical USB current <=500mA
		ilim_raw = PM886_CHG_ILIM_RAW_500;
		ilim_fine = PM886_CHG_ILIM_FINE_10;
		/* Set ilim to 450mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 550) {
		ilim_raw = PM886_CHG_ILIM_RAW_600;
		ilim_fine = PM886_CHG_ILIM_FINE_10;
		/* Set ilim to 540mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 600) {
		ilim_raw = PM886_CHG_ILIM_RAW_700;
		ilim_fine = PM886_CHG_ILIM_FINE_15;
		/* Set ilim to 595mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 650) {
		ilim_raw = PM886_CHG_ILIM_RAW_700;
		ilim_fine = PM886_CHG_ILIM_FINE_10;
		/* Set ilim to 630mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 700) {
		ilim_raw = PM886_CHG_ILIM_RAW_800;
		ilim_fine = PM886_CHG_ILIM_FINE_12_5;
		/* Set ilim to 700mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 750) {
		ilim_raw = PM886_CHG_ILIM_RAW_800;
		ilim_fine = PM886_CHG_ILIM_FINE_10;
		/* Set ilim to 720mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 800) {
		ilim_raw = PM886_CHG_ILIM_RAW_900;
		ilim_fine = PM886_CHG_ILIM_FINE_11_25;
		/* Set ilim to 798.75mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 850) {
		ilim_raw = PM886_CHG_ILIM_RAW_1000;
		ilim_fine = PM886_CHG_ILIM_FINE_15;
		/* Set ilim to 850mA */
		ilim = ilim_raw || ilim_fine;
	} else if (input_current <= 900) {
		ilim_raw = PM886_CHG_ILIM_RAW_1000;
		ilim_fine = PM886_CHG_ILIM_FINE_10;
		/* Set ilim to 900mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 950) {
		ilim_raw = PM886_CHG_ILIM_RAW_1100;
		ilim_fine = PM886_CHG_ILIM_FINE_13_75;
		/* Set ilim to 948.75mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 1000) {
		ilim_raw = PM886_CHG_ILIM_RAW_1100;
		ilim_fine = PM886_CHG_ILIM_FINE_10;
		/* Set ilim to 990mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 1200) {
		ilim_raw = PM886_CHG_ILIM_RAW_1400;
		ilim_fine = PM886_CHG_ILIM_FINE_15;
		/* Set ilim to 1190mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 1350) {
		ilim_raw = PM886_CHG_ILIM_RAW_1500;
		ilim_fine = PM886_CHG_ILIM_FINE_10;
		/* Set ilim to 1350mA */
		ilim = ilim_raw | ilim_fine;
	} else if (input_current <= 1400) {
		ilim_raw = PM886_CHG_ILIM_RAW_1600;
		ilim_fine = PM886_CHG_ILIM_FINE_12_5;
		/* Set ilim to 1400mA */
		ilim = ilim_raw | ilim_fine;
	} else {
		// Typical USB current <=500mA
		ilim_raw = PM886_CHG_ILIM_RAW_500;
		ilim_fine = PM886_CHG_ILIM_FINE_10;
		/* Set ilim to 450mA */
		ilim = ilim_raw | ilim_fine;
	}

	regmap_write(info->chip->battery_regmap, PM886_EXT_ILIM_CONFIG, ilim);

	dev_dbg(info->dev, "%s: vbus current limit(0x%x) = 0x%x\n",
		__func__, PM886_EXT_ILIM_CONFIG, ilim);

	return ilim;
}

static inline int pm886_set_topoff(
	struct pm886_charger_info *info, unsigned int topoff_current)
{
	unsigned int eoc_current;

	/* fastcharge eoc current range is 10mA - 640mA */
	if (topoff_current > 640)
		topoff_current = 640;
	else if (topoff_current < 10)
		topoff_current = 10;

	eoc_current = (topoff_current - 10) / 10;
	regmap_update_bits(info->chip->battery_regmap, PM886_FAST_CONFIG3,
			   PM886_IBAT_EOC_TH, eoc_current);

	dev_dbg(info->dev, "%s: eoc_current (0x%x) = 0x%x\n", __func__,
		PM886_FAST_CONFIG3, eoc_current);

	return eoc_current;
}

static inline int pm886_set_fastchg_current(
	struct pm886_charger_info *info, unsigned int fast_charging_current)
{
	unsigned int fast_current;

	/* fastcharge current range is 300mA - 1500mA */
	if (fast_charging_current > 2000)
		fast_charging_current = 2000;
	else if (fast_charging_current < 300)
		fast_charging_current = 300;

	if (fast_charging_current == 300)
		fast_current = 0x0;
	else
		fast_current = ((fast_charging_current - 450) / 50) + 1;

	regmap_update_bits(info->chip->battery_regmap, PM886_FAST_CONFIG2,
			   PM886_ICHG_FAST_SET_MASK, fast_current);

	dev_dbg(info->dev, "%s: fastcharge current(0x%x) = 0x%x\n", __func__,
		PM886_FAST_CONFIG2, fast_current);

	return fast_current;
}

static inline int get_prechg_cur(struct pm886_charger_info *info)
{
	/* precharge current range is 300mA - 750mA */
	if (info->prechg_cur == 300)
		return 0;
	else
		return (info->prechg_cur - 450) / 50 + 1;
}

static inline int get_prechg_vol(struct pm886_charger_info *info)
{
	/* precharge voltage range is 2.3V - 3.0V */
	static int ret;
	ret = (info->prechg_vol - 2300) / 100;
	ret = (ret < 0) ? 0 : ((ret > 0x7) ? 0x7 : ret);
	dev_dbg(info->dev, "%s: precharge voltage = 0x%x\n", __func__, ret);
	return ret;
}

static inline int get_fastchg_cur(struct pm886_charger_info *info)
{
	/* fastcharge current range is 300mA - 1500mA */
	static int ret;
	ret = (info->fastchg_cur - 300) / 150;
	ret = (ret < 0) ? 0 : ((ret > 0x16) ? 0x16 : ret);
	dev_dbg(info->dev, "%s: fastcharge current = 0x%x\n", __func__, ret);
	return ret;
}

static void pm886_otg_control(struct pm886_charger_info *info, int enable)
{
	unsigned int data;

	if (enable) {
		dev_info(info->dev, "%s: OTG Enabled \n", __func__);
		otg_enable_flag = 1;
		regmap_update_bits(info->chip->battery_regmap, PM886_CHG_CONFIG1,
					PM886_USB_OTG, PM886_USB_OTG);
	} else {
		dev_info(info->dev, "%s: OTG Disabled \n", __func__);
		otg_enable_flag = 0;
		regmap_update_bits(info->chip->battery_regmap, PM886_CHG_CONFIG1,
					PM886_USB_OTG, 0);
	}
	regmap_read(info->chip->battery_regmap, PM886_CHG_CONFIG1, &data);
	dev_dbg(info->dev, "%s: PM886_CHG_CONFIG1(0x%x) = 0x%x\n",
		__func__, PM886_CHG_CONFIG1, data);
}

static int pm886_start_charging(struct pm886_charger_info *info)
{
	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return 0;
	}

	/* disable charger watchdog */
	regmap_update_bits(info->chip->battery_regmap, PM886_CHG_CONFIG1,
			   PM886_CHG_WDT_EN, (0 << 1));

	/* WA: disable OV_VBAT for A0 step */
	if (info->chip->chip_id == PM886_A0) {
		regmap_update_bits(info->chip->battery_regmap, PM886_CHG_CONFIG3,
				   PM886_OV_VBAT_EN, 0);
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
	regmap_update_bits(info->chip->battery_regmap, PM886_CHG_CONFIG1,
		PM886_CHG_ENABLE, PM886_CHG_ENABLE);
	/* Add delay to allow enabling of the charger IC */
	msleep(20);

	/* clear the previous settings */
	regmap_write(info->chip->test_regmap, 0x40, 0x00);
	regmap_write(info->chip->test_regmap, 0x43, 0x00);
	regmap_write(info->chip->test_regmap, 0x46, 0x00);

	/* close test page */
	regmap_write(info->chip->base_regmap, 0x1F, 0x00);


	pr_info("%s : Enable Charger!\n", __func__);

	return 0;
}

static int pm886_stop_charging(struct pm886_charger_info *info)
{
	unsigned int data;

	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return 0;
	}

	regmap_read(info->chip->battery_regmap, PM886_CHG_CONFIG1, &data);
	/* enable USB suspend */
	data |= PM886_USB_SUSP;
	regmap_write(info->chip->battery_regmap, PM886_CHG_CONFIG1, data);

	/* disable USB suspend and stop charging */
	data &= ~(PM886_USB_SUSP | PM886_CHG_ENABLE);
	regmap_write(info->chip->battery_regmap, PM886_CHG_CONFIG1, data);

	pr_info("%s : Disable Charger!\n", __func__);

	return 0;
}

static int pm886_charger_init(struct pm886_charger_info *info)
{
	unsigned int mask, data;

	/* WA: disable OV_VBAT for A0 step */
	if (info->chip->chip_id == PM886_A0) {
		regmap_update_bits(info->chip->battery_regmap, PM886_CHG_CONFIG3,
				   PM886_OV_VBAT_EN, 0);
	}

	/* set wall-adaptor threshold to 3.9V */
	regmap_write(info->chip->battery_regmap, PM886_MPPT_CONFIG3, 0x03);
	regmap_read(info->chip->battery_regmap, PM886_MPPT_CONFIG3, &data);
	dev_dbg(info->dev, "%s: PM886_MPPT_CONFIG3(0x%x) = 0x%x\n",
		__func__, PM886_MPPT_CONFIG3, data);

	/* Set RECHG_THR_SET and PM886_FASTCHG_TOUT_SET */
	regmap_write(info->chip->battery_regmap, PM886_TIMER_CONFIG, 0x07);

	/* Disable regular end of charge and battery monitoring (register 0x28 bits 3 and 4) */
	regmap_read(info->chip->battery_regmap, PM886_CHG_CONFIG1, &data);
	mask = data | 0x28;
	regmap_write(info->chip->battery_regmap, PM886_CHG_CONFIG1, mask);
	regmap_read(info->chip->battery_regmap, PM886_CHG_CONFIG1, &data);
	dev_dbg(info->dev, "%s: PM886_CHG_CONFIG1(0x%x) = 0x%x\n",
		__func__, PM886_CHG_CONFIG1, data);

	/* config pre-charge parameters */
	mask = PM886_ICHG_PRE_SET_MASK | PM886_VBAT_PRE_TERM_SET_MASK;
	data = get_prechg_cur(info) << PM886_ICHG_PRE_SET_OFFSET
		| get_prechg_vol(info) << PM886_VBAT_PRE_TERM_SET_OFFSET;
	regmap_update_bits(info->chip->battery_regmap, PM886_PRE_CONFIG1,
			   mask, data);

	/* Set float voltage */
	pr_info("%s : float voltage (%dmV)\n",
		__func__, info->pdata->chg_float_voltage);
	pm886_set_batreg_voltage(
		info, info->pdata->chg_float_voltage);

	return 0;
}

static irqreturn_t pm886_chg_fail_handler(int irq, void *data)
{
	static int value;
	struct pm886_charger_info *info = data;

	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return IRQ_NONE;
	}

	dev_info(info->dev,
		"%s: charge fail interrupt is served\n", __func__);
	/* charging is stopped by HW */

	regmap_read(info->chip->battery_regmap, PM886_CHG_LOG1, &value);
	if (value & PM886_BATT_TEMP_NOK) {
		dev_info(info->chip->dev,
			"%s : battery temperature is abnormal.\n", __func__);
	}
	if (value & PM886_CHG_WDT_EXPIRED) {
		dev_info(info->dev,
			"%s : charger wdt is triggered.\n", __func__);
	}
	if (value & PM886_OV_VBAT) {
		dev_info(info->chip->dev,
			"%s : battery voltage is abnormal.\n", __func__);
	}
	if (value & PM886_CHG_TIMEOUT) {
		dev_info(info->chip->dev,
			"%s : charge timeout.\n", __func__);
	}
	if (value & PM886_OV_ITEMP)
		dev_info(info->chip->dev,
			"%s : internal temperature abnormal.\n", __func__);

	/* write to clear */
	regmap_write(info->chip->battery_regmap, PM886_CHG_LOG1, 0x00);

	pm886_stop_charging(info);
	return IRQ_HANDLED;
}

static void pm886_chg_det_irq_thread(struct work_struct *work)
{
	union power_supply_propval val, value;
	struct pm886_charger_info *info = container_of(work, struct pm886_charger_info, isr_work.work);
	static int cfg1, stat2, stat3;

	regmap_read(info->chip->battery_regmap, PM886_CHG_STATUS2, &stat2);
	regmap_read(info->chip->battery_regmap, PM886_CHG_STATUS3, &stat3);
	regmap_read(info->chip->battery_regmap, PM886_CHG_CONFIG1, &cfg1);

	dev_info(info->dev, "%s : PM886_CHG_STATUS2 : 0x%02x\n", __func__, stat2);
	dev_info(info->dev, "%s : PM886_CHG_STATUS3 : 0x%02x\n", __func__, stat3);
	dev_info(info->dev, "%s : PM886_CHG_CONFIG1 : 0x%02x\n", __func__, cfg1);

	if (!sec_chg_get_property(&info->psy_chg,
				POWER_SUPPLY_PROP_HEALTH, &val))
				return;

	switch (val.intval) {
	case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		pr_info("%s: Interrupted by OVP/UVLO\n", __func__);
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_HEALTH, val);
		break;

	case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
		psy_do_property("battery", get,
			POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: CABLE TYPE IN UVLO: %d\n", __func__, value.intval);
		if (value.intval != POWER_SUPPLY_TYPE_BATTERY) {
			pr_info("%s: Interrupted by OVP/UVLO\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, val);
		}
		break;

	case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
		pr_err("%s: Interrupted but Unspec\n", __func__);
		break;

	case POWER_SUPPLY_HEALTH_GOOD:
		pr_err("%s: Interrupted but Good\n", __func__);
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_HEALTH, val);
		break;

	case POWER_SUPPLY_HEALTH_UNKNOWN:
	default:
		pr_err("%s: Invalid Charger Health\n", __func__);
		break;
	}
}

static irqreturn_t  pm886_chg_det_handler(int irq, void *irq_data)
{
	struct pm886_charger_info *info = irq_data;
	pr_info("*** %s ***\n", __func__);
	// Temporary delay of the interrupt to prevent MUIC interrupt running after it
	schedule_delayed_work(&info->isr_work, 0);
	return IRQ_HANDLED;
}

static irqreturn_t pm886_chg_done_handler(int irq, void *data)
{
	static int log1;
	union power_supply_propval val, present;
	struct pm886_charger_info *info = data;

	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return IRQ_NONE;
	}

	regmap_read(info->chip->battery_regmap, PM886_CHG_LOG1, &log1);

	dev_info(info->dev, "charge done interrupt is served\n");
	dev_dbg(info->dev, "%s : PM886_CHG_LOG1 : 0x%02x\n", __func__, log1);

	psy_do_property("sec-fuelgauge", get,
		POWER_SUPPLY_PROP_PRESENT, present);
	dev_dbg(info->dev, "%s : Battery Presence : %d\n", __func__, present.intval);

	if (present.intval) {
		int full_check_type, vcell, soc;

		if (log1 & PM886_BATT_REMOVAL) {
			dev_info(info->chip->dev,
				"%s : Battery is plugged out.\n", __func__);
		} else if (log1 & PM886_CHG_REMOVAL) {
			dev_info(info->chip->dev,
				"%s : Charger cable is plugged out.\n", __func__);
		} else {
			//check VCELL for premature full interrupt HW request
			psy_do_property("sec-fuelgauge", get,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, val);
			vcell = val.intval;
			psy_do_property("sec-fuelgauge", get,
				POWER_SUPPLY_PROP_CAPACITY, val);
			soc = val.intval;
			dev_info(info->chip->dev,
				"%s : vcell: %d soc: %d\n", __func__, vcell, soc);
			if ((vcell >= info->pdata->full_condition_vcell)
				&& (soc >= info->pdata->full_condition_soc)) {
				info->is_fullcharged = true;
				val.intval= POWER_SUPPLY_STATUS_FULL;
				sec_chg_set_property(&info->psy_chg,
					POWER_SUPPLY_PROP_STATUS, &val);
				dev_info(info->chip->dev,
					"%s : Status, Power Supply Full from HW.\n", __func__);
			} else {
				dev_info(info->chip->dev,
					"%s : Not enough VCELL or SOC.\n", __func__);
				return IRQ_HANDLED;
			}
		}

		/* write to clear */
		regmap_write(info->chip->battery_regmap, PM886_CHG_LOG1, 0x00);

		psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_NOW, val);
		if (val.intval == SEC_BATTERY_CHARGING_1ST)
			full_check_type = info->pdata->full_check_type;
		else
			full_check_type = info->pdata->full_check_type_2nd;

		if (full_check_type == SEC_BATTERY_FULLCHARGED_CHGINT) {
			if (!sec_chg_get_property(&info->psy_chg,
				POWER_SUPPLY_PROP_STATUS, &val))
				return -1;

			switch (val.intval) {
			case POWER_SUPPLY_STATUS_DISCHARGING:
				pr_err("%s: Interrupted but Discharging\n", __func__);
				break;

			case POWER_SUPPLY_STATUS_NOT_CHARGING:
				pr_err("%s: Interrupted but NOT Charging\n", __func__);
				break;

			case POWER_SUPPLY_STATUS_FULL:
				pr_err("%s: Interrupted by Full\n", __func__);
				psy_do_property("battery", set,
					POWER_SUPPLY_PROP_STATUS, val);
				break;

			case POWER_SUPPLY_STATUS_CHARGING:
				pr_err("%s: Interrupted but Charging\n", __func__);
				break;

			case POWER_SUPPLY_STATUS_UNKNOWN:
			default:
				pr_err("%s: Invalid Charger Status\n", __func__);
				break;
			}
		}
	} else {
		dev_info(info->chip->dev,
			"%s : Battery missing in full interrupt time.\n", __func__);
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_PRESENT, present);
	}
	return IRQ_HANDLED;
}

static struct pm886_irq_desc {
	const char *name;
	irqreturn_t (*handler)(int irq, void *data);
} pm886_irq_descs[] = {
	{"charge fail", pm886_chg_fail_handler},
	{"charge done", pm886_chg_done_handler},
	{"charge det", pm886_chg_det_handler},
};

static int pm886_charger_dt_init(struct device_node *np,
			     struct pm886_charger_info *info)
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

	ret = of_property_read_u32(np, "fastchg-voltage", &info->fastchg_vol);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "fastchg-cur", &info->fastchg_cur);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "fastchg-eoc", &info->fastchg_eoc);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "fastchg-tout", &info->fastchg_tout);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "recharge-thr", &info->recharge_thr);
	if (ret)
		return ret;

	return 0;
}

int sec_chg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct pm886_charger_info *charger =
		container_of(psy, struct pm886_charger_info, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (charger->is_fullcharged) {
			int tst_full_registers;
			val->intval = POWER_SUPPLY_STATUS_FULL;
			tst_full_registers = pm886_get_charging_status(charger);
		} else
			val->intval = pm886_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = pm886_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		//val->intval = pm886_get_battery_present(charger);
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
		else if (charger->aicl_on)
		{
			val->intval = POWER_SUPPLY_CHARGE_TYPE_SLOW;
			pr_info("%s: slow-charging mode\n", __func__);
		}
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		break;
#if defined(CONFIG_BATTERY_SWELLING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = pm886_get_batreg_voltage(charger);
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
	struct pm886_charger_info *charger =
		container_of(psy, struct pm886_charger_info, psy_chg);
	union power_supply_propval value;

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
				pm886_otg_control(charger, 1);
			} else {
				pm886_otg_control(charger, 0);
			}
			break;
		}

		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_HEALTH, value);
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {

			charger->is_charging = false;
			charger->aicl_on = false;
			set_charging_current = 0;
			charger->is_fullcharged = false;
			/* Set float voltage */
			pr_info("%s : float voltage (%dmV)\n",
				__func__, charger->pdata->chg_float_voltage);
			pm886_set_batreg_voltage(
				charger, charger->pdata->chg_float_voltage);
			/* Set topoff current */
			pr_info("%s : topoff current (%dmA)\n",
				__func__, charger->pdata->charging_current[
				POWER_SUPPLY_TYPE_USB].full_check_current_1st);
			pm886_set_topoff(
				charger, charger->pdata->charging_current[
				POWER_SUPPLY_TYPE_USB].full_check_current_1st);
			/* Set fast charge current */
			pr_info("%s : fast charging current (%dmA), siop_level=%d\n",
				__func__, charger->charging_current, charger->siop_level);
			pm886_set_fastchg_current(
				charger, charger->charging_current);

			/* Set input current limit */
			pr_info("%s : vbus current limit (%dmA)\n",
				__func__, charger->pdata->charging_current[
				POWER_SUPPLY_TYPE_USB].input_current_limit);

			pm886_set_vbuslimit_current(
				charger, charger->pdata->charging_current[
				POWER_SUPPLY_TYPE_USB].input_current_limit);
		} else {
			charger->is_charging = true;
			charger->charging_current =
					charger->pdata->charging_current
					[charger->cable_type].fast_charging_current;
			/* Set topoff current */
			pr_info("%s : topoff current (%dmA)\n",
				__func__, charger->pdata->charging_current[
				charger->cable_type].full_check_current_1st);
			pm886_set_topoff(
				charger, charger->pdata->charging_current[
					charger->cable_type].full_check_current_1st);
			/* Set fast charge current */
			pr_info("%s : fast charging current (%dmA), siop_level=%d\n",
				__func__, charger->charging_current, charger->siop_level);
			pm886_set_fastchg_current(
				charger, charger->charging_current);

			/* Set input current limit */
			pr_info("%s : vbus current limit (%dmA)\n",
				__func__, charger->pdata->charging_current
				[charger->cable_type].input_current_limit);

			pm886_set_vbuslimit_current(
				charger, charger->pdata->charging_current
				[charger->cable_type].input_current_limit);

			pm886_start_charging(charger);
		}

		/* if battery is removed, put vbus current limit to minimum */
		if ((value.intval == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) ||
		    (value.intval == POWER_SUPPLY_HEALTH_OVERHEATLIMIT))
			pm886_set_vbuslimit_current(charger, 100);

		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		pm886_set_fastchg_current(charger,
				val->intval);
		pm886_set_vbuslimit_current(charger,
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

		pr_debug("%s: Current Limit after SIOP set level: %d \n",
			__func__, set_charging_current);
		pm886_set_vbuslimit_current(
			charger, set_charging_current);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		charger_health = val->intval;
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		if (val->intval == 1) {
			pm886_otg_control(charger, 1);
		} else
			pm886_otg_control(charger, 0);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
#if defined(CONFIG_BATTERY_SWELLING)
		if (val->intval > charger->pdata->charging_current
			[charger->cable_type].fast_charging_current) {
			break;
		}
#endif
		pr_debug("%s: Current Limit after low temp swelling: %d \n",
			__func__, val->intval);
		pm886_set_vbuslimit_current(
			charger, val->intval);
		break;
#if defined(CONFIG_BATTERY_SWELLING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_info("%s: Float Voltage(%d)\n", __func__, val->intval);
		pm886_set_batreg_voltage(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		charger->is_charging = val->intval;
		if (val->intval)
			pm886_start_charging(charger);
		else
			pm886_stop_charging(charger);
		break;
#endif
	default:
		return -EINVAL;
	}
	return 1;
}

static int pm886_charger_probe(struct platform_device *pdev)
{
	struct pm886_charger_info *info;
	struct pm886_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm886_platform_data *pdata = dev_get_platdata(chip->dev);
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;
	int i, j;
	otg_enable_flag = 0;

	info = devm_kzalloc(&pdev->dev, sizeof(struct pm886_charger_info),
			GFP_KERNEL);

	pr_info("%s: 88PM866 Charger Probe Start\n", __func__);

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
		ret = sec_chg_dt_init(pdev->dev.of_node, &pdev->dev, info->pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		info->pdata = pdata->battery_data;
	}

	ret = pm886_charger_dt_init(node, info);
	if (ret) {
		dev_info(info->dev,"%s: Cannot parse dt file\n", __func__);
		return ret;
	}

	mutex_init(&info->lock);
	platform_set_drvdata(pdev, info);

    if (info->pdata->charger_name == NULL)
            info->pdata->charger_name = "sec-charger";

	info->psy_chg.name           = info->pdata->charger_name;
	info->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	info->psy_chg.get_property   = sec_chg_get_property;
	info->psy_chg.set_property   = sec_chg_set_property;
	info->psy_chg.properties     = sec_charger_props;
	info->psy_chg.num_properties = ARRAY_SIZE(sec_charger_props);

	info->siop_level = 100;

	for (i = 0, j = 0; i < pdev->num_resources; i++) {
		info->irq[j] = platform_get_irq(pdev, i);
		if (info->irq[j] < 0)
			continue;
		j++;
	}
	info->irq_nums = j;

	ret = pm886_charger_init(info);
	if (ret < 0) {
		dev_err(info->dev, "initial charger fails.\n");
		return ret;
	}

    ret = power_supply_register(&pdev->dev, &info->psy_chg);
    if (ret) {
            dev_err(info->dev, "register power_supply fail!\n");
            goto err_power_supply_register;
    }

	INIT_DEFERRABLE_WORK(
		&info->isr_work, pm886_chg_det_irq_thread);

	/* interrupt should be request in the last stage */
	for (i = 0; i < info->irq_nums; i++) {
		ret = devm_request_threaded_irq(info->dev, info->irq[i], NULL,
						pm886_irq_descs[i].handler,
						IRQF_ONESHOT | IRQF_NO_SUSPEND,
						pm886_irq_descs[i].name, info);
		if (ret < 0) {
			dev_err(info->dev, "failed to request IRQ: #%d: %d\n",
				info->irq[i], ret);
			goto err_irq;
		}
	}

	dev_info(info->dev, "%s is successful!\n", __func__);

	return 0;

err_irq:
	while (--i >= 0)
		devm_free_irq(info->dev, info->irq[i], info);
err_power_supply_register:
err_parse_dt:
	pr_info("%s: err_parse_dt error \n", __func__);
err_parse_dt_nomem:
	kfree(info);
	return ret;
}

static int pm886_charger_remove(struct platform_device *pdev)
{
	int i;
	struct pm886_charger_info *info = platform_get_drvdata(pdev);
	if (!info)
		return 0;

	for (i = 0; i < info->irq_nums; i++) {
		if (pm886_irq_descs[i].handler != NULL)
			devm_free_irq(info->dev, info->irq[i], info);
	}

	pm886_start_charging(info);

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static void pm886_charger_shutdown(struct platform_device *pdev)
{
	pm886_charger_remove(pdev);
	return;
}

static const struct of_device_id pm886_chg_dt_match[] = {
	{ .compatible = "marvell,88pm886-charger", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm886_chg_dt_match);

static struct platform_driver pm886_charger_driver = {
	.probe = pm886_charger_probe,
	.remove = pm886_charger_remove,
	.shutdown = pm886_charger_shutdown,
	.driver = {
		.owner = THIS_MODULE,
		.name = "88pm886-charger",
		.of_match_table = of_match_ptr(pm886_chg_dt_match),
	},
};

module_platform_driver(pm886_charger_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("88pm886 Charger Driver");
MODULE_ALIAS("platform:88pm886-charger");
MODULE_AUTHOR("Samsung Electronics");
MODULE_AUTHOR("Shay Pathov <shayp@marvell.com>");
