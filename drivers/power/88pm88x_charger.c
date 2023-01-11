/*
 * 88PM88X PMIC Charger driver
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
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/platform_data/mv_usb.h>

#define CHG_RESTART_DELAY		(5) /* in minutes */

#define PM88X_CHG_ENABLE		(1 << 0)
#define PM88X_CHG_WDT_EN		(1 << 1)
#define PM88X_USB_SUSP			(1 << 6)

#define PM88X_CHG_CONFIG3		(0x2A)
#define PM88X_OV_VBAT_EN		(1 << 0)

#define PM88X_PRE_CONFIG1		(0x2D)
#define PM88X_ICHG_PRE_SET_OFFSET	(0)
#define PM88X_ICHG_PRE_SET_MASK		(0X7 << PM88X_ICHG_PRE_SET_OFFSET)
#define PM88X_VBAT_PRE_TERM_SET_OFFSET	(4)
#define PM88X_VBAT_PRE_TERM_SET_MASK	(0x7 << PM88X_VBAT_PRE_TERM_SET_OFFSET)

#define PM88X_FAST_CONFIG1		(0x2E)
#define PM88X_VBAT_FAST_SET_MASK	(0x7F << 0)

#define PM88X_FAST_CONFIG2		(0x2F)
#define PM88X_ICHG_FAST_SET_MASK	(0x3F << 0)

#define PM88X_FAST_CONFIG3		(0x30)
#define PM88X_IBAT_EOC_TH		(0x3F << 0)

#define PM88X_TIMER_CONFIG		(0x31)
#define PM88X_FASTCHG_TOUT_OFFSET	(0)
#define PM88X_RECHG_THR_SET_50MV	(0x0 << 4)
#define PM88X_CHG_ILIM_EXTD_1X5		(0x3 << 6)

/* IR drop compensation for PM880 and it's subsequent versions */
#define PM88X_CHG_CONFIG5		(0x32)
#define PM88X_IR_COMP_EN		(1 << 5)
#define PM88X_IR_COMP_UPD_OFFSET	(0)
#define PM88X_IR_COMP_UPD_MSK		(0x1f << PM88X_IR_COMP_UPD_OFFSET)
#define PM88X_IR_COMP_UPD_SET(x)	((x) << PM88X_IR_COMP_UPD_OFFSET)
#define PM88X_IR_COMP_RES_SET		(0x33)

#define PM88X_EXT_ILIM_CONFIG		(0x34)
#define PM88X_CHG_ILIM_FINE_10		(0x4 << 4)

#define PM88X_MPPT_CONFIG3		(0x40)
#define PM88X_WA_TH_MASK		(0x3 << 0)

#define PM88X_CHG_LOG1			(0x45)
#define PM88X_BATT_REMOVAL		(1 << 0)
#define PM88X_CHG_REMOVAL		(1 << 1)
#define PM88X_BATT_TEMP_NOK		(1 << 2)
#define PM88X_CHG_WDT_EXPIRED		(1 << 3)
#define PM88X_OV_VBAT			(1 << 5)
#define PM88X_CHG_TIMEOUT		(1 << 6)
#define PM88X_OV_ITEMP			(1 << 7)

enum {
	AC_CHARGER_TYPE,
	USB_CHARGER_TYPE,
};

struct pm88x_charger_info {
	struct device *dev;
	struct power_supply ac_chg;
	struct power_supply usb_chg;
	struct delayed_work restart_chg_work;
	struct work_struct chg_state_machine_work;
	struct notifier_block nb;
	int ac_chg_online;
	int usb_chg_online;
	int used_chg_type;	/* type of used power supply */

	struct mutex lock;

	unsigned int ir_comp_res;	/* IR compensation resistor */
	unsigned int ir_comp_update;	/* IR compensation update time */

	unsigned int prechg_cur;	/* precharge current limit */
	unsigned int prechg_vol;	/* precharge voltage limit */

	unsigned int fastchg_vol;	/* fastcharge voltage */
	unsigned int fastchg_cur;	/* fastcharge current */
	unsigned int fastchg_eoc;	/* fastcharge end current */
	unsigned int fastchg_tout;	/* fastcharge voltage */

	unsigned int recharge_thr;	/* gap between VBAT_FAST_SET and VBAT */
	unsigned int dcp_limit;		/* current limit for DCP charger */
	unsigned int limit_cur;

	unsigned int allow_basic_charge;
	unsigned int allow_recharge;
	unsigned int allow_chg_after_tout;
	unsigned int allow_chg_after_overvoltage;
	unsigned int allow_chg_ext;

	unsigned int charging;
	unsigned int full;

	int irq_nums;
	int irq[7];

	struct pm88x_chip *chip;
	int pm88x_charger_status;
};

static enum power_supply_property pm88x_props[] = {
	POWER_SUPPLY_PROP_STATUS, /* Charger status output */
	POWER_SUPPLY_PROP_ONLINE, /* External power source */
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
};

static void pm88x_change_chg_status(struct pm88x_charger_info *info, int status);

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

static inline int get_fastchg_eoc(struct pm88x_charger_info *info)
{
	/* fastcharge eoc current range is 10mA - 640mA */
	if (info->fastchg_eoc > 640)
		info->fastchg_eoc = 640;
	else if (info->fastchg_eoc < 10)
		info->fastchg_eoc = 10;
	return (info->fastchg_eoc - 10) / 10;
}

static inline int get_fastchg_cur(struct pm88x_charger_info *info)
{
	int ret;

	/* fastcharge current range is 300mA - 2000mA */
	if (info->fastchg_cur >= 2000) {
		info->fastchg_cur = 2000;
		ret = 0x1F;
	} else if (info->fastchg_cur >= 1900)
		ret = 0x1E;
	else if (info->fastchg_cur < 450) {
		info->fastchg_cur = 300;
		ret = 0x0;
	} else
		ret = ((info->fastchg_cur - 450) / 50) + 1;

	/*
	 * in PM880 the first value is 0mA (for testing), so shift value by 1.
	 * also value of 1950mA is supported, so another shift is needed
	 */
	if (info->chip->type == PM880) {
		if (info->fastchg_cur >= 1950)
			ret += 2;
		else
			ret++;
	}

	return ret;
}

static inline int get_fastchg_vol(struct pm88x_charger_info *info)
{
	/* fastcharge voltage range is 3.6V - 4.5V */
	if (info->fastchg_vol > 4500)
		info->fastchg_vol = 4500;
	else if (info->fastchg_vol < 3600)
		info->fastchg_vol = 3600;
	return (info->fastchg_vol - 3600) * 10 / 125;
}

static inline int get_recharge_vol(struct pm88x_charger_info *info)
{
	/* recharge voltage range is 50mV - 200mV */
	if (info->recharge_thr > 200)
		info->recharge_thr = 200;
	else if (info->recharge_thr < 50)
		info->recharge_thr = 50;
	return (info->recharge_thr - 50) / 50;
}

static inline int get_ilim_cur(struct pm88x_charger_info *info, unsigned int limit_cur)
{
	/* currnet limit range is 100mA - 1600mA */
	if (limit_cur > 1600)
		limit_cur = 1600;
	else if (limit_cur < 100)
		limit_cur = 100;
	return (limit_cur - 100) / 100;
}

static inline int get_fastchg_timeout(struct pm88x_charger_info *info)
{
	static int ret;

	/* fastcharge timeout range is 2h - 16h */
	if (info->fastchg_tout >= 16) {
		info->fastchg_tout = 16;
		ret = 0x7;
	} else if (info->fastchg_tout <= 2) {
		info->fastchg_tout = 2;
		ret = 0x0;
	} else {
		ret = (info->fastchg_tout / 2);
	}

	return ret;
}

static char *supply_interface[] = {
	"battery",
};

static bool pm88x_charger_check_allowed(struct pm88x_charger_info *info)
{
	union power_supply_propval val;
	static struct power_supply *psy;
	int ret, i;

	if (!psy || !psy->get_property) {
		psy = power_supply_get_by_name(info->usb_chg.supplied_to[0]);
		if (!psy || !psy->get_property) {
			dev_err(info->dev, "get battery property failed.\n");
			return false;
		}
	}

	/*
	 * the allow_recharge needs to be checked and updated before all other parameters,
	 * because it is also used inside the state-machine
	 */
	if (!info->allow_recharge || !info->allow_chg_after_overvoltage) {
		/*
		 * perform 3 VBAT reads with 50ms delays,
		 * in order to get a stable value and ignore momentary drops
		 */
		for (i = 0; i < 3; i++) {
			/* check battery voltage */
			ret = psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
			if (ret) {
				dev_err(info->dev, "get battery property failed.\n");
				return false;
			}
			/* change to mili-volts */
			val.intval /= 1000;
			if (val.intval >= (info->fastchg_vol - info->recharge_thr)) {
				dev_dbg(info->dev, "voltage not low enough (%d). wait until (%d)\n",
					val.intval, info->fastchg_vol - info->recharge_thr);
				return false;
			}
			dev_dbg(info->dev, "voltage read (%d) is OK (%dmV)\n", i, val.intval);
			msleep(50);
		}
		dev_info(info->dev, "OK to start recharge!\n");
		info->allow_chg_after_overvoltage = 1;
		info->allow_recharge = 1;
	}

	if (!info->allow_basic_charge)
		return false;

	if (!info->allow_chg_after_tout)
		return false;

	if (!info->allow_chg_ext)
		return false;

	/* check if there is a battery present */
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &val);
	if (ret) {
		dev_err(info->dev, "get battery property failed.\n");
		return false;
	}
	if (val.intval == 0) {
		dev_dbg(info->dev, "battery not present.\n");
		return false;
	}

	/* check if battery is healthy */
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_HEALTH, &val);
	if (ret) {
		dev_err(info->dev, "get battery property failed.\n");
		return false;
	}
	if (val.intval != POWER_SUPPLY_HEALTH_GOOD) {
		dev_info(info->dev, "battery health is not good.\n");
		return false;
	}

	return true;
}

static int pm88x_property_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		return 1;
	default:
		return 0;
	}
}

static int pm88x_charger_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct pm88x_charger_info *info = dev_get_drvdata(psy->dev->parent);

	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		pm88x_change_chg_status(info, val->intval);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int pm88x_charger_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct pm88x_charger_info *info = dev_get_drvdata(psy->dev->parent);

	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (!strncmp(psy->name, "ac", 2))
			val->intval = info->ac_chg_online;
		else
			val->intval = info->usb_chg_online;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = info->pm88x_charger_status;
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		val->intval = info->allow_chg_ext;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int pm88x_config_charger(struct pm88x_charger_info *info)
{
	unsigned int limit_cur = info->limit_cur;
	unsigned int data;

	/* config fast charge timeout, rechare vol and ilim exted */
	data =  (get_fastchg_timeout(info) << PM88X_FASTCHG_TOUT_OFFSET)
		| PM88X_RECHG_THR_SET_50MV;
	if (limit_cur > 1600) {
		limit_cur *= 2;
		limit_cur /= 3;
		data |= PM88X_CHG_ILIM_EXTD_1X5;
	}
	regmap_write(info->chip->battery_regmap, PM88X_TIMER_CONFIG, data);

	/* config current limit raw and fine */
	regmap_write(info->chip->battery_regmap, PM88X_EXT_ILIM_CONFIG,
		     PM88X_CHG_ILIM_FINE_10 | get_ilim_cur(info, limit_cur));

	return 0;
}

static int pm88x_start_charging(struct pm88x_charger_info *info)
{
	dev_info(info->dev, "%s\n", __func__);
	info->charging = 1;

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

	/* need to wait before reseting the values */
	usleep_range(3000, 4000);

	/* clear the previous settings */
	regmap_write(info->chip->test_regmap, 0x40, 0x00);
	regmap_write(info->chip->test_regmap, 0x43, 0x00);
	regmap_write(info->chip->test_regmap, 0x46, 0x00);

	/* close test page */
	regmap_write(info->chip->base_regmap, 0x1F, 0x00);

	return 0;
}

static int pm88x_stop_charging(struct pm88x_charger_info *info)
{
	unsigned int data;

	dev_info(info->dev, "%s\n", __func__);
	/* disable charging */
	info->charging = 0;

	regmap_read(info->chip->battery_regmap, PM88X_CHG_CONFIG1, &data);
	/* enable USB suspend */
	data |= PM88X_USB_SUSP;
	regmap_write(info->chip->battery_regmap, PM88X_CHG_CONFIG1, data);

	/* disable USB suspend and stop charging */
	data &= ~(PM88X_USB_SUSP | PM88X_CHG_ENABLE);
	regmap_write(info->chip->battery_regmap, PM88X_CHG_CONFIG1, data);

	return 0;
}


static void pm88x_change_chg_status(struct pm88x_charger_info *info, int status)
{
	int chg_online = (info->ac_chg_online || info->usb_chg_online);
	int charging;

	if (status == 1) {
		/* allow charging */
		dev_info(info->dev, "%s, allow charging\n", __func__);
		info->allow_chg_ext = 1;

		/*
		 * start charging only if usb is connected and charging was enable
		 * while calling for stop charging
		 */
		if (chg_online && info->charging) {
			pm88x_start_charging(info);
			dev_info(info->dev, "%s, charging is resumed\n", __func__);
		}
	} else {
		charging = info->charging;
		/* block charging */
		dev_info(info->dev, "%s, block charging\n", __func__);
		info->allow_chg_ext = 0;

		if (chg_online) {
			pm88x_stop_charging(info);
			dev_info(info->dev, "%s, charging is paused\n", __func__);
			/*
			 * override charging flag in order to know if to
			 * restart charging while status = POWER_SUPPLY_STATUS_CHARGING
			 */
			info->charging = charging;
		}
	}
}

static void pm88x_chg_ext_power_changed(struct power_supply *psy)
{
	struct pm88x_charger_info *info = dev_get_drvdata(psy->dev->parent);

	if (!strncmp(psy->name, "ac", 2) && (info->ac_chg_online))
		dev_dbg(info->dev, "%s: ac charger.\n", __func__);
	else if (!strncmp(psy->name, "usb", 3) && (info->usb_chg_online))
		dev_dbg(info->dev, "%s: usb charger.\n", __func__);
	else
		return;

	schedule_work(&info->chg_state_machine_work);
}

static ssize_t pm88x_control_charging(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct pm88x_charger_info *info = dev_get_drvdata(dev);
	int enable;

	if (!info)
		return strnlen(buf, PAGE_SIZE);

	if (sscanf(buf, "%d", &enable) < 0)
		enable = 1;

	if (enable) {
		dev_info(info->dev, "enable charging by manual\n");
		info->allow_basic_charge = 1;
	} else {
		dev_info(info->dev, "disable charging by manual\n");
		info->allow_basic_charge = 0;
	}

	schedule_work(&info->chg_state_machine_work);

	return strnlen(buf, PAGE_SIZE);
}

static DEVICE_ATTR(control, S_IWUSR | S_IWGRP, NULL, pm88x_control_charging);

static int pm88x_power_supply_register(struct pm88x_charger_info *info)
{
	int ret = 0;

	/* private attribute */
	info->ac_chg.supplied_to = supply_interface;
	info->ac_chg.num_supplicants = ARRAY_SIZE(supply_interface);
	info->usb_chg.supplied_to = supply_interface;
	info->usb_chg.num_supplicants = ARRAY_SIZE(supply_interface);

	/* AC supply */
	info->ac_chg.name = "ac";
	info->ac_chg.type = POWER_SUPPLY_TYPE_MAINS;
	info->ac_chg.properties = pm88x_props;
	info->ac_chg.num_properties = ARRAY_SIZE(pm88x_props);
	info->ac_chg.get_property = pm88x_charger_get_property;
	info->ac_chg.set_property = pm88x_charger_set_property;
	info->ac_chg.property_is_writeable = pm88x_property_is_writeable;
	info->ac_chg.external_power_changed = pm88x_chg_ext_power_changed;

	ret = power_supply_register(info->dev, &info->ac_chg);
	if (ret < 0)
		return ret;

	/* USB supply */
	info->usb_chg.name = "usb";
	info->usb_chg.type = POWER_SUPPLY_TYPE_USB;
	info->usb_chg.properties = pm88x_props;
	info->usb_chg.num_properties = ARRAY_SIZE(pm88x_props);
	info->usb_chg.get_property = pm88x_charger_get_property;
	info->usb_chg.set_property = pm88x_charger_set_property;
	info->usb_chg.property_is_writeable = pm88x_property_is_writeable;
	info->usb_chg.external_power_changed = pm88x_chg_ext_power_changed;

	ret = power_supply_register(info->dev, &info->usb_chg);
	if (ret < 0)
		goto err_usb;

	return ret;

err_usb:
	power_supply_unregister(&info->ac_chg);
	return ret;
}

static int pm88x_charger_init(struct pm88x_charger_info *info)
{
	unsigned int mask, data;

	info->allow_basic_charge = 1;
	info->allow_recharge = 1;
	info->allow_chg_after_tout = 1;
	info->allow_chg_after_overvoltage = 1;
	info->allow_chg_ext = 1;

	info->ac_chg_online = 0;
	info->usb_chg_online = 0;
	info->charging = 0;
	info->pm88x_charger_status = POWER_SUPPLY_STATUS_DISCHARGING;

	/* stop charging first */
	pm88x_stop_charging(info);

	/* WA for A0 to prevent OV_VBAT fault and support dead battery case */
	switch (info->chip->type) {
	case PM886:
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

	/* config fast-charge parameters */
	regmap_update_bits(info->chip->battery_regmap, PM88X_FAST_CONFIG1,
			   PM88X_VBAT_FAST_SET_MASK, get_fastchg_vol(info));

	regmap_update_bits(info->chip->battery_regmap, PM88X_FAST_CONFIG2,
			   PM88X_ICHG_FAST_SET_MASK, get_fastchg_cur(info));

	regmap_update_bits(info->chip->battery_regmap, PM88X_FAST_CONFIG3,
			   PM88X_IBAT_EOC_TH, get_fastchg_eoc(info));

	/*
	 * set wall-adaptor threshold to 3.9V:
	 * on PM886 the value is the maximum (0x3),
	 * on PM880 the value is the minimum (0x0)
	 */
	data = (info->chip->type == PM886 ? PM88X_WA_TH_MASK : 0x0);
	regmap_update_bits(info->chip->battery_regmap, PM88X_MPPT_CONFIG3,
			   PM88X_WA_TH_MASK, data);

	if ((PM886 != info->chip->type) && (0 != info->ir_comp_res)) {
		/* set IR compensation resistor, 4.1667mohm/LSB */
		data = info->ir_comp_res * 10000 / 41667;
		data = (data < 0xff) ? data : 0xff;
		regmap_write(info->chip->battery_regmap, PM88X_IR_COMP_RES_SET, data);
		/* IR compensation update time is 1s ~ 31s */
		data = (info->ir_comp_update > 0x1f) ? 0x1f : info->ir_comp_update;
		regmap_update_bits(info->chip->battery_regmap, PM88X_CHG_CONFIG5,
					(PM88X_IR_COMP_EN | PM88X_IR_COMP_UPD_MSK),
					(PM88X_IR_COMP_EN | PM88X_IR_COMP_UPD_SET(data)));
	}

	return 0;
}

static int pm88x_charger_notifier_call(struct notifier_block *nb,
				       unsigned long type, void *chg_event)
{
	struct pm88x_charger_info *info =
		container_of(nb, struct pm88x_charger_info, nb);
	static unsigned long prev_chg_type = NULL_CHARGER;

	/* no change in charger type - nothing to do */
	if (type == prev_chg_type)
		return 0;

	/* disable the auto-charging on cable insertion and removal */
	if (prev_chg_type == NULL_CHARGER || type == NULL_CHARGER)
		pm88x_stop_charging(info);

	prev_chg_type = type;

	/* new charger - remove previous limitations */
	info->allow_recharge = 1;
	info->allow_chg_after_tout = 1;
	info->allow_chg_after_overvoltage = 1;

	switch (type) {
	case NULL_CHARGER:
		info->ac_chg_online = 0;
		info->usb_chg_online = 0;
		cancel_delayed_work(&info->restart_chg_work);
		break;
	case SDP_CHARGER:
	case NONE_STANDARD_CHARGER:
	case DEFAULT_CHARGER:
		info->ac_chg_online = 0;
		info->usb_chg_online = 1;
		info->used_chg_type = USB_CHARGER_TYPE;
		info->limit_cur = 500;
		break;
	case CDP_CHARGER:
		info->ac_chg_online = 1;
		info->usb_chg_online = 0;
		info->used_chg_type = AC_CHARGER_TYPE;
		/* the max current for CDP should be 1.5A */
		info->limit_cur = 1500;
		break;
	case DCP_CHARGER:
		info->ac_chg_online = 1;
		info->usb_chg_online = 0;
		info->used_chg_type = AC_CHARGER_TYPE;
		info->limit_cur = info->dcp_limit;
		break;
	default:
		info->ac_chg_online = 0;
		info->usb_chg_online = 1;
		info->used_chg_type = USB_CHARGER_TYPE;
		info->limit_cur = 500;
		break;
	}

	if (type != NULL_CHARGER)
		pm88x_config_charger(info);

	schedule_work(&info->chg_state_machine_work);

	dev_dbg(info->dev, "usb_chg inserted: ac_chg = %d, usb = %d\n",
		info->ac_chg_online, info->usb_chg_online);

	return 0;
}

static void pm88x_restart_chg_work(struct work_struct *work)
{
	struct pm88x_charger_info *info =
		container_of(work, struct pm88x_charger_info,
			restart_chg_work.work);

	info->allow_chg_after_tout = 1;
	schedule_work(&info->chg_state_machine_work);
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
	info->charging = 0;
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
		info->allow_chg_after_tout = 0;
		dev_err(info->dev, "charger WDT expired! restart charging in %d min\n",
				CHG_RESTART_DELAY);
		schedule_delayed_work(&info->restart_chg_work,
				CHG_RESTART_DELAY * 60 * HZ);
	}

	if (value & PM88X_OV_VBAT) {
		dev_err(info->chip->dev, "battery voltage is abnormal.\n");
		info->allow_chg_after_overvoltage = 0;
	}

	if (value & PM88X_CHG_TIMEOUT) {
		dev_err(info->chip->dev, "charge timeout.\n");
		info->allow_chg_after_tout = 0;
		dev_err(info->dev, "charger timeout! restart charging in %d min\n",
				CHG_RESTART_DELAY);
		schedule_delayed_work(&info->restart_chg_work,
				CHG_RESTART_DELAY * 60 * HZ);
	}

	if (value & PM88X_OV_ITEMP)
		/* handled in a dedicated interrupt */
		dev_err(info->chip->dev, "internal temperature abnormal.\n");

	/* write to clear */
	regmap_write(info->chip->battery_regmap, PM88X_CHG_LOG1, value);

	schedule_work(&info->chg_state_machine_work);

	return IRQ_HANDLED;
}

static irqreturn_t pm88x_chg_done_handler(int irq, void *data)
{
	struct pm88x_charger_info *info = data;

	if (!info) {
		pr_err("%s: charger chip info is empty!\n", __func__);
		return IRQ_NONE;
	}

	dev_info(info->chip->dev, "charging done, battery full.\n");

	/* charging is stopped by HW */
	info->full = 1;
	info->charging = 0;
	info->allow_recharge = 0;

	/* disable auto-recharge */
	pm88x_stop_charging(info);

	schedule_work(&info->chg_state_machine_work);

	return IRQ_HANDLED;
}

static void pm88x_chg_state_machine(struct pm88x_charger_info *info)
{
	static int prev_chg_online = -1;
	int chg_allowed, chg_online, prev_status, update_psy = 0;
	char *charger_status[] = {
		"UNKNOWN",
		"CHARGING",
		"DISCHARGING",
		"NOT_CHARGING",
		"FULL",
	};

	chg_allowed = pm88x_charger_check_allowed(info);
	prev_status = info->pm88x_charger_status;
	chg_online = info->ac_chg_online || info->usb_chg_online;

	mutex_lock(&info->lock);

	switch (info->pm88x_charger_status) {
	case POWER_SUPPLY_STATUS_FULL:
		/* charger removed */
		if (!chg_online) {
			info->pm88x_charger_status = POWER_SUPPLY_STATUS_DISCHARGING;
		/* recharge done */
		} else if (info->full) {
			info->full = 0;
			update_psy = 1;
		} else {
			/* start recharge */
			if (!info->charging && chg_allowed)
				pm88x_start_charging(info);
			/* something else is blocking - change to discharging */
			else if (info->allow_recharge && !chg_allowed) {
				info->pm88x_charger_status = POWER_SUPPLY_STATUS_DISCHARGING;
				if (info->charging)
					pm88x_stop_charging(info);
			}
		}
		break;

	case POWER_SUPPLY_STATUS_DISCHARGING:
		/* charger inserted & charging allowed */
		if (chg_online && chg_allowed) {
			info->pm88x_charger_status = POWER_SUPPLY_STATUS_CHARGING;
			pm88x_start_charging(info);
		}
		break;

	case POWER_SUPPLY_STATUS_CHARGING:
		/* charger removed */
		if (!chg_online) {
			info->pm88x_charger_status = POWER_SUPPLY_STATUS_DISCHARGING;
		/* charging done - battery full */
		} else if (info->full) {
			info->full = 0;
			info->pm88x_charger_status = POWER_SUPPLY_STATUS_FULL;
		/* charging stopped for some reason */
		} else if (!info->charging) {
			info->pm88x_charger_status = POWER_SUPPLY_STATUS_DISCHARGING;
		/* charging not allowed */
		} else if (!chg_allowed) {
			info->pm88x_charger_status = POWER_SUPPLY_STATUS_DISCHARGING;
			pm88x_stop_charging(info);
		}
		break;

	default:
		BUG();
		break;
	}

	if (prev_chg_online != chg_online) {
		prev_chg_online = chg_online;
		update_psy = 1;
	}

	mutex_unlock(&info->lock);

	/* notify when status or online is changed */
	if (prev_status != info->pm88x_charger_status) {
		dev_dbg(info->chip->dev, "charger status changed from %s to %s\n",
			charger_status[prev_status], charger_status[info->pm88x_charger_status]);
		update_psy = 1;
	}

	if (update_psy) {
		if (info->used_chg_type == USB_CHARGER_TYPE)
			power_supply_changed(&info->usb_chg);
		else
			power_supply_changed(&info->ac_chg);
	}
}

static void pm88x_chg_state_machine_work(struct work_struct *work)
{
	struct pm88x_charger_info *info =
		container_of(work, struct pm88x_charger_info, chg_state_machine_work);

	pm88x_chg_state_machine(info);
}

static struct pm88x_irq_desc {
	const char *name;
	irqreturn_t (*handler)(int irq, void *data);
} pm88x_irq_descs[] = {
	{"charge fail", pm88x_chg_fail_handler},
	{"charge done", pm88x_chg_done_handler},
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

	ret = of_property_read_u32(np, "dcp-limit", &info->dcp_limit);
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

static int pm88x_charger_probe(struct platform_device *pdev)
{
	struct pm88x_charger_info *info;
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct device_node *node = pdev->dev.of_node;
	int ret = 0;
	int i, j;

	info = devm_kzalloc(&pdev->dev, sizeof(struct pm88x_charger_info),
			GFP_KERNEL);

	if (!info) {
		dev_err(&pdev->dev, "Cannot allocate memory.\n");
		return -ENOMEM;
	}

	info->dev = &pdev->dev;
	info->chip = chip;

	ret = pm88x_charger_dt_init(node, info);
	if (ret)
		return ret;

	mutex_init(&info->lock);
	platform_set_drvdata(pdev, info);

	for (i = 0, j = 0; i < pdev->num_resources; i++) {
		info->irq[j] = platform_get_irq(pdev, i);
		if (info->irq[j] < 0)
			continue;
		j++;
	}
	info->irq_nums = j;

	ret = pm88x_charger_init(info);
	if (ret < 0) {
		dev_err(info->dev, "initial charger fails.\n");
		return ret;
	}

	ret = pm88x_power_supply_register(info);
	if (ret) {
		dev_err(info->dev, "register power_supply fail!\n");
		goto out;
	}

	/* register charger event notifier */
	info->nb.notifier_call = pm88x_charger_notifier_call;

#ifdef CONFIG_USB_MV_UDC
	ret = mv_udc_register_client(&info->nb);
	if (ret < 0) {
		dev_err(info->dev, "failed to register client!\n");
		goto err_psy;
	}
#endif

	INIT_WORK(&info->chg_state_machine_work, pm88x_chg_state_machine_work);
	INIT_DELAYED_WORK(&info->restart_chg_work, pm88x_restart_chg_work);

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

	ret = device_create_file(&pdev->dev, &dev_attr_control);
	if (ret < 0)
		dev_err(info->dev, "failed to create charging contol sys file!\n");

	dev_info(info->dev, "%s is successful!\n", __func__);

	return 0;

out_irq:
	while (--i >= 0)
		devm_free_irq(info->dev, info->irq[i], info);
#ifdef CONFIG_USB_MV_UDC
err_psy:
#endif
	power_supply_unregister(&info->ac_chg);
	power_supply_unregister(&info->usb_chg);
out:
	return ret;
}

static int pm88x_charger_remove(struct platform_device *pdev)
{
	int i;
	struct pm88x_charger_info *info = platform_get_drvdata(pdev);
	if (!info)
		return 0;

	for (i = 0; i < info->irq_nums; i++) {
		if (pm88x_irq_descs[i].handler != NULL)
			devm_free_irq(info->dev, info->irq[i], info);
	}

	pm88x_stop_charging(info);

#ifdef CONFIG_USB_MV_UDC
	mv_udc_unregister_client(&info->nb);
#endif

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
MODULE_AUTHOR("Yi Zhang <yizhang@marvell.com>");
MODULE_AUTHOR("Shay Pathov <shayp@marvell.com>");
