/*
 * SY6974 battery charging driver
 *
 * Copyright (C) 2021 ETA
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"[sy6974]:%s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>

#include <charger_class.h>
#include <mtk_charger.h>

#include "sy6974_reg.h"
#include "sy6974.h"

/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 start*/
#include <linux/hardinfo_charger.h>
/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 end*/

enum {
	PN_SY6974,
};

enum sy6974_part_no {
	SY6974 = 0x09,
};

static int pn_data[] = {
	[PN_SY6974] = 0x09,
};

static char *pn_str[] = {
	[PN_SY6974] = "sy6974",
};

struct sy6974 {
	struct device *dev;
	struct i2c_client *client;

	enum sy6974_part_no part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;

	bool chg_det_enable;

	// enum charger_type chg_type;
	struct power_supply_desc psy_desc;
	int psy_usb_type;

	int status;
	int irq;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;

	struct sy6974_platform_data *platform_data;
	struct charger_device *chg_dev;

	struct power_supply *psy;
};

static const struct charger_properties sy6974_chg_props = {
	.alias_name = "sy6974",
};

static int __sy6974_read_reg(struct sy6974 *sy, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sy->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sy6974_write_reg(struct sy6974 *sy, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sy->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
				val, reg, ret);
		return ret;
	}
	return 0;
}

static int sy6974_read_byte(struct sy6974 *sy, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&sy->i2c_rw_lock);
	ret = __sy6974_read_reg(sy, reg, data);
	mutex_unlock(&sy->i2c_rw_lock);

	return ret;
}

static int sy6974_write_byte(struct sy6974 *sy, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&sy->i2c_rw_lock);
	ret = __sy6974_write_reg(sy, reg, data);
	mutex_unlock(&sy->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int sy6974_update_bits(struct sy6974 *sy, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&sy->i2c_rw_lock);
	ret = __sy6974_read_reg(sy, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sy6974_write_reg(sy, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sy->i2c_rw_lock);
	return ret;
}

static int sy6974_enable_otg(struct sy6974 *sy)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_01, REG01_OTG_CONFIG_MASK,
					val);

}

static int sy6974_disable_otg(struct sy6974 *sy)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_01, REG01_OTG_CONFIG_MASK,
					val);

}

static int sy6974_enable_charger(struct sy6974 *sy)
{
	int ret;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
		sy6974_update_bits(sy, SY6974_REG_01, REG01_CHG_CONFIG_MASK, val);

	return ret;
}

static int sy6974_disable_charger(struct sy6974 *sy)
{
	int ret;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
		sy6974_update_bits(sy, SY6974_REG_01, REG01_CHG_CONFIG_MASK, val);
	return ret;
}

int sy6974_set_chargecurrent(struct sy6974 *sy, int curr)
{
	u8 ichg;

	if (curr < REG02_ICHG_BASE)
		curr = REG02_ICHG_BASE;

	ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;
	return sy6974_update_bits(sy, SY6974_REG_02, REG02_ICHG_MASK,
					ichg << REG02_ICHG_SHIFT);

}

int sy6974_set_term_current(struct sy6974 *sy, int curr)
{
	u8 iterm;

	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return sy6974_update_bits(sy, SY6974_REG_03, REG03_ITERM_MASK,
					iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(sy6974_set_term_current);

int sy6974_set_prechg_current(struct sy6974 *sy, int curr)
{
	u8 iprechg;

	if (curr < REG03_IPRECHG_BASE)
		curr = REG03_IPRECHG_BASE;

	iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return sy6974_update_bits(sy, SY6974_REG_03, REG03_IPRECHG_MASK,
					iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(sy6974_set_prechg_current);

int sy6974_set_chargevolt(struct sy6974 *sy, int volt)
{
	u8 val;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

	val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
	return sy6974_update_bits(sy, SY6974_REG_04, REG04_VREG_MASK,
					val << REG04_VREG_SHIFT);
}

int sy6974_set_input_volt_limit(struct sy6974 *sy, int volt)
{
	u8 val;

	if (volt < REG06_VINDPM_BASE)
		volt = REG06_VINDPM_BASE;

	val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
	return sy6974_update_bits(sy, SY6974_REG_06, REG06_VINDPM_MASK,
					val << REG06_VINDPM_SHIFT);
}

int sy6974_set_input_current_limit(struct sy6974 *sy, int curr)
{
	u8 val;

	if (curr < REG00_IINLIM_BASE)
		curr = REG00_IINLIM_BASE;

	val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	return sy6974_update_bits(sy, SY6974_REG_00, REG00_IINLIM_MASK,
					val << REG00_IINLIM_SHIFT);
}

int sy6974_set_watchdog_timer(struct sy6974 *sy, u8 timeout)
{
	u8 temp;

	temp = (u8) (((timeout -
				REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return sy6974_update_bits(sy, SY6974_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(sy6974_set_watchdog_timer);

int sy6974_disable_watchdog_timer(struct sy6974 *sy)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(sy6974_disable_watchdog_timer);

int sy6974_reset_watchdog_timer(struct sy6974 *sy)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_01, REG01_WDT_RESET_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sy6974_reset_watchdog_timer);

int sy6974_reset_chip(struct sy6974 *sy)
{
	int ret;
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

	ret =
		sy6974_update_bits(sy, SY6974_REG_0B, REG0B_REG_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(sy6974_reset_chip);

int sy6974_enter_hiz_mode(struct sy6974 *sy)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sy6974_enter_hiz_mode);

int sy6974_exit_hiz_mode(struct sy6974 *sy)
{

	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(sy6974_exit_hiz_mode);

/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 start */
// int sy6974_get_hiz_mode(struct sy6974 *sy, u8 *state)
// {
// 	u8 val;
// 	int ret;

// 	ret = sy6974_read_byte(sy, SY6974_REG_00, &val);
// 	if (ret)
// 		return ret;
// 	*state = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

// 	return 0;
// }
// EXPORT_SYMBOL_GPL(sy6974_get_hiz_mode);
/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 end */

static int sy6974_enable_term(struct sy6974 *sy, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = sy6974_update_bits(sy, SY6974_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(sy6974_enable_term);

int sy6974_set_boost_current(struct sy6974 *sy, int curr)
{
	u8 val;

	val = REG02_BOOST_LIM_0P5A;
	if (curr == BOOSTI_1200)
		val = REG02_BOOST_LIM_1P2A;

	return sy6974_update_bits(sy, SY6974_REG_02, REG02_BOOST_LIM_MASK,
					val << REG02_BOOST_LIM_SHIFT);
}

int sy6974_set_boost_voltage(struct sy6974 *sy, int volt)
{
	u8 val;

	if (volt == BOOSTV_4850)
		val = REG06_BOOSTV_4P85V;
	else if (volt == BOOSTV_5150)
		val = REG06_BOOSTV_5P15V;
	else if (volt == BOOSTV_5300)
		val = REG06_BOOSTV_5P3V;
	else
		val = REG06_BOOSTV_5V;

	return sy6974_update_bits(sy, SY6974_REG_06, REG06_BOOSTV_MASK,
					val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(sy6974_set_boost_voltage);

static int sy6974_set_acovp_threshold(struct sy6974 *sy, int volt)
{
	u8 val;

	if (volt == VAC_OVP_14000)
		val = REG06_OVP_14P0V;
	else if (volt == VAC_OVP_10500)
		val = REG06_OVP_10P5V;
	else if (volt == VAC_OVP_6500)
		val = REG06_OVP_6P5V;
	else
		val = REG06_OVP_5P5V;

	return sy6974_update_bits(sy, SY6974_REG_06, REG06_OVP_MASK,
					val << REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(sy6974_set_acovp_threshold);

static int sy6974_set_stat_ctrl(struct sy6974 *sy, int ctrl)
{
	u8 val;

	val = ctrl;

	return sy6974_update_bits(sy, SY6974_REG_00, REG00_STAT_CTRL_MASK,
					val << REG00_STAT_CTRL_SHIFT);
}

static int sy6974_set_int_mask(struct sy6974 *sy, int mask)
{
	u8 val;

	val = mask;

	return sy6974_update_bits(sy, SY6974_REG_0A, REG0A_INT_MASK_MASK,
					val << REG0A_INT_MASK_SHIFT);
}

static int sy6974_enable_batfet(struct sy6974 *sy)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_07, REG07_BATFET_DIS_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sy6974_enable_batfet);

static int sy6974_disable_batfet(struct sy6974 *sy)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_07, REG07_BATFET_DIS_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sy6974_disable_batfet);

static int sy6974_set_batfet_delay(struct sy6974 *sy, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG07_BATFET_DLY_0S;
	else
		val = REG07_BATFET_DLY_10S;

	val <<= REG07_BATFET_DLY_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_07, REG07_BATFET_DLY_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sy6974_set_batfet_delay);

static int sy6974_enable_safety_timer(struct sy6974 *sy)
{
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_05, REG05_EN_TIMER_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sy6974_enable_safety_timer);

static int sy6974_disable_safety_timer(struct sy6974 *sy)
{
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

	return sy6974_update_bits(sy, SY6974_REG_05, REG05_EN_TIMER_MASK,
					val);
}
EXPORT_SYMBOL_GPL(sy6974_disable_safety_timer);

static struct sy6974_platform_data *sy6974_parse_dt(struct device_node *np,
								struct sy6974 *sy)
{
	int ret;
	struct sy6974_platform_data *pdata;

	pdata = devm_kzalloc(sy->dev, sizeof(struct sy6974_platform_data),
				 GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &sy->chg_dev_name) < 0) {
		sy->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &sy->eint_name) < 0) {
		sy->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	sy->chg_det_enable =
		of_property_read_bool(np, "sy6974,charge-detect-enable");

	ret = of_property_read_u32(np, "sy6974,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of sy6974,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "sy6974,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of sy6974,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "sy6974,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of sy6974,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "sy6974,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of sy6974,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "sy6974,stat-pin-ctrl",
					&pdata->statctrl);
	if (ret) {
		pdata->statctrl = 0;
		pr_err("Failed to read node of sy6974,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np, "sy6974,precharge-current",
					&pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of sy6974,precharge-current\n");
	}

	ret = of_property_read_u32(np, "sy6974,termination-current",
					&pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
			("Failed to read node of sy6974,termination-current\n");
	}

	ret =
		of_property_read_u32(np, "sy6974,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of sy6974,boost-voltage\n");
	}

	ret =
		of_property_read_u32(np, "sy6974,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of sy6974,boost-current\n");
	}

	ret = of_property_read_u32(np, "sy6974,vac-ovp-threshold",
					&pdata->vac_ovp);
	if (ret) {
		pdata->vac_ovp = 6500;
		pr_err("Failed to read node of sy6974,vac-ovp-threshold\n");
	}

	return pdata;
}

static int sy6974_get_charger_type(struct sy6974 *sy, int *type)
{
	int ret;

	u8 reg_val = 0;
	int vbus_stat = 0;
	int chg_type = POWER_SUPPLY_TYPE_UNKNOWN;

	ret = sy6974_read_byte(sy, SY6974_REG_08, &reg_val);

	if (ret)
		return ret;

	vbus_stat = (reg_val & REG08_VBUS_STAT_MASK);
	vbus_stat >>= REG08_VBUS_STAT_SHIFT;

	switch (vbus_stat) {

	case REG08_VBUS_TYPE_NONE:
		chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case REG08_VBUS_TYPE_SDP:
		chg_type = POWER_SUPPLY_TYPE_USB;
		break;
	case REG08_VBUS_TYPE_CDP:
		chg_type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case REG08_VBUS_TYPE_DCP:
		chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case REG08_VBUS_TYPE_UNKNOWN:
		chg_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case REG08_VBUS_TYPE_NON_STD:
		chg_type = POWER_SUPPLY_TYPE_USB_FLOAT;
		break;
	default:
		chg_type = POWER_SUPPLY_TYPE_USB_FLOAT;
		break;
	}

	*type = chg_type;

	return 0;
}

static int sy6974_get_chrg_stat(struct sy6974 *sy,
	int *chg_stat)
{
	int ret;
	u8 val;

	ret = sy6974_read_byte(sy, SY6974_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*chg_stat = val;
	}

	return ret;
}

static int sy6974_inform_charger_type(struct sy6974 *sy)
{
	int ret = 0;
	union power_supply_propval propval;

	if (!sy->psy) {
		sy->psy = power_supply_get_by_name("charger");
		if (!sy->psy) {
			pr_err("%s Couldn't get psy\n", __func__);
			return -ENODEV;
		}
	}

	if (sy->psy_usb_type != POWER_SUPPLY_TYPE_UNKNOWN)
		propval.intval = 1;
	else
		propval.intval = 0;

	ret = power_supply_set_property(sy->psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);

	if (ret < 0)
		pr_notice("inform power supply online failed:%d\n", ret);

	propval.intval = sy->psy_usb_type;

	ret = power_supply_set_property(sy->psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE,
					&propval);

	if (ret < 0)
		pr_notice("inform power supply charge type failed:%d\n", ret);

	return ret;
}

static irqreturn_t sy6974_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	int prev_chg_type;
	struct sy6974 *sy = (struct sy6974 *)data;

	ret = sy6974_read_byte(sy, SY6974_REG_08, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = sy->power_good;

	sy->power_good = !!(reg_val & REG08_PG_STAT_MASK);

	if (!prev_pg && sy->power_good)
		pr_notice("adapter/usb inserted\n");
	else if (prev_pg && !sy->power_good)
		pr_notice("adapter/usb removed\n");

	prev_chg_type = sy->psy_usb_type;

	ret = sy6974_get_charger_type(sy, &sy->psy_usb_type);
	if (!ret && prev_chg_type != sy->psy_usb_type && sy->chg_det_enable)
		sy6974_inform_charger_type(sy);

	return IRQ_HANDLED;
}

static int sy6974_register_interrupt(struct sy6974 *sy)
{
	int ret = 0;
	/*struct device_node *np;

	np = of_find_node_by_name(NULL, sy->eint_name);
	if (np) {
		sy->irq = irq_of_parse_and_map(np, 0);
	} else {
		pr_err("couldn't get irq node\n");
		return -ENODEV;
	}

	pr_info("irq = %d\n", sy->irq);*/

	if (! sy->client->irq) {
		pr_info("sy->client->irq is NULL\n");//remember to config dws
		return -ENODEV;
	}

	ret = devm_request_threaded_irq(sy->dev, sy->client->irq, NULL,
					sy6974_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"ti_irq", sy);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}

	enable_irq_wake(sy->irq);

	return 0;
}

static int sy6974_init_device(struct sy6974 *sy)
{
	int ret;

	sy6974_disable_watchdog_timer(sy);

	ret = sy6974_set_stat_ctrl(sy, sy->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

	ret = sy6974_set_prechg_current(sy, sy->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	ret = sy6974_set_term_current(sy, sy->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = sy6974_set_boost_voltage(sy, sy->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = sy6974_set_boost_current(sy, sy->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	ret = sy6974_set_acovp_threshold(sy, sy->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n", ret);

	/*HS03s for DEVAL5626-524 by liujie at 20210901 start*/
	ret = sy6974_disable_safety_timer(sy);
	if (ret)
		pr_err("Failed to set safety_timer stop, ret = %d\n", ret);
	/*HS03s for DEVAL5626-524 by liujie at 20210901 end*/

	ret = sy6974_set_int_mask(sy,
					REG0A_IINDPM_INT_MASK |
					REG0A_VINDPM_INT_MASK);
	if (ret)
		pr_err("Failed to set vindpm and iindpm int mask\n");

	return 0;
}

static void determine_initial_status(struct sy6974 *sy)
{
	sy6974_irq_handler(sy->irq, (void *) sy);
}

static int sy6974_detect_device(struct sy6974 *sy)
{
	int ret;
	u8 data;

	ret = sy6974_read_byte(sy, SY6974_REG_0B, &data);
	if (!ret) {
		sy->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		sy->revision =
			(data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}

	return ret;
}

static void sy6974_dump_regs(struct sy6974 *sy)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = sy6974_read_byte(sy, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static ssize_t
sy6974_show_registers(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct sy6974 *sy = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sy6974 Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = sy6974_read_byte(sy, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
						"Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t
sy6974_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct sy6974 *sy = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		sy6974_write_byte(sy, (unsigned char) reg,
					(unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sy6974_show_registers,
			sy6974_store_registers);

static struct attribute *sy6974_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sy6974_attr_group = {
	.attrs = sy6974_attributes,
};

static int sy6974_charging(struct charger_device *chg_dev, bool enable)
{

	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = sy6974_enable_charger(sy);
	else
		ret = sy6974_disable_charger(sy);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
			!ret ? "successfully" : "failed");

	ret = sy6974_read_byte(sy, SY6974_REG_01, &val);

	if (!ret)
		sy->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

	return ret;
}

static int sy6974_plug_in(struct charger_device *chg_dev)
{

	int ret;

	ret = sy6974_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int sy6974_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = sy6974_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int sy6974_dump_register(struct charger_device *chg_dev)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	sy6974_dump_regs(sy);

	return 0;
}

static int sy6974_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	*en = sy->charge_enabled;

	return 0;
}

static int sy6974_get_charging_status(struct charger_device *chg_dev,
	int *chg_stat)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = sy6974_read_byte(sy, SY6974_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*chg_stat = val;
	}

	return ret;
}

static int sy6974_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = sy6974_read_byte(sy, SY6974_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*done = (val == REG08_CHRG_STAT_CHGDONE);
	}

	return ret;
}

static int sy6974_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	return sy6974_set_chargecurrent(sy, curr / 1000);
}

static int sy6974_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = sy6974_read_byte(sy, SY6974_REG_02, &reg_val);
	if (!ret) {
		ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
		ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
		*curr = ichg * 1000;
	}

	return ret;
}

static int sy6974_set_iterm(struct charger_device *chg_dev, u32 uA)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	pr_err("termination curr = %d\n", uA);

	return sy6974_set_term_current(sy, uA / 1000);
}

static int sy6974_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int sy6974_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 100 * 1000;
	return 0;
}

static int sy6974_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge volt = %d\n", volt);

	return sy6974_set_chargevolt(sy, volt / 1000);
}

static int sy6974_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = sy6974_read_byte(sy, SY6974_REG_04, &reg_val);
	if (!ret) {
		vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
		vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int sy6974_get_ivl_state(struct charger_device *chg_dev, bool *in_loop)
{
	int ret = 0;
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;

	ret = sy6974_read_byte(sy, SY6974_REG_0A, &reg_val);
	if (!ret)
		*in_loop = (ret & REG0A_VINDPM_STAT_MASK) >> REG0A_VINDPM_STAT_SHIFT;

	return ret;
}

static int sy6974_get_ivl(struct charger_device *chg_dev, u32 *volt)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ivl;
	int ret;

	ret = sy6974_read_byte(sy, SY6974_REG_06, &reg_val);
	if (!ret) {
		ivl = (reg_val & REG06_VINDPM_MASK) >> REG06_VINDPM_SHIFT;
		ivl = ivl * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
		*volt = ivl * 1000;
	}

	return ret;
}

static int sy6974_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);

	return sy6974_set_input_volt_limit(sy, volt / 1000);

}

static int sy6974_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	pr_err("indpm curr = %d\n", curr);

	return sy6974_set_input_current_limit(sy, curr / 1000);
}

static int sy6974_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = sy6974_read_byte(sy, SY6974_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
		icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;

}

static int sy6974_enable_te(struct charger_device *chg_dev, bool en)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	pr_err("enable_term = %d\n", en);

	return sy6974_enable_term(sy, en);
}

static int sy6974_kick_wdt(struct charger_device *chg_dev)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	return sy6974_reset_watchdog_timer(sy);
}

static int sy6974_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	if (en)
		ret = sy6974_enable_otg(sy);
	else
		ret = sy6974_disable_otg(sy);

	pr_err("%s OTG %s\n", en ? "enable" : "disable",
			!ret ? "successfully" : "failed");

	return ret;
}

static int sy6974_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en)
		ret = sy6974_enable_safety_timer(sy);
	else
		ret = sy6974_disable_safety_timer(sy);

	return ret;
}

static int sy6974_is_safety_timer_enabled(struct charger_device *chg_dev,
						bool *en)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = sy6974_read_byte(sy, SY6974_REG_05, &reg_val);

	if (!ret)
		*en = !!(reg_val & REG05_EN_TIMER_MASK);

	return ret;
}

/*HS03s for SR-AL5625-01-511 by wenyaqi at 20210618 start*/
static int sy6974_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	if (!sy->psy) {
		dev_notice(sy->dev, "%s: cannot get psy\n", __func__);
		return -ENODEV;
	}

	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(sy->psy);
		break;
	default:
		break;
	}
	return 0;
}
/*HS03s for SR-AL5625-01-511 by wenyaqi at 20210618 end*/

static int sy6974_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = sy6974_set_boost_current(sy, curr / 1000);

	return ret;
}

static int sy6974_set_hiz_mode(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sy6974 *sy = dev_get_drvdata(&chg_dev->dev);

	if (en)
		ret = sy6974_enter_hiz_mode(sy);
	else
		ret = sy6974_exit_hiz_mode(sy);

	pr_err("%s hiz mode %s\n", en ? "enable" : "disable",
			!ret ? "successfully" : "failed");

	return ret;
}

/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 start */
static int sy6974_get_hiz_mode(struct charger_device *chg_dev)
{
	int ret;
	struct sy6974 *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;
	ret = sy6974_read_byte(bq, SY6974_REG_00, &val);
	if (ret == 0){
		pr_err("Reg[%.2x] = 0x%.2x\n", SY6974_REG_00, val);
	}

	/*HS03s added for DEVAL5626-463 by wangzikang at 20210823 start */
	ret = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;
	/*HS03s added for DEVAL5626-463 by wangzikang at 20210823 end */

	pr_err("%s:hiz mode %s\n",__func__, ret ? "enabled" : "disabled");

	return ret;
}
/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 end */

static struct charger_ops sy6974_chg_ops = {
	/* Normal charging */
	.plug_in = sy6974_plug_in,
	.plug_out = sy6974_plug_out,
	.dump_registers = sy6974_dump_register,
	.enable = sy6974_charging,
	.is_enabled = sy6974_is_charging_enable,
	.get_charging_current = sy6974_get_ichg,
	.set_charging_current = sy6974_set_ichg,
	.get_input_current = sy6974_get_icl,
	.set_input_current = sy6974_set_icl,
	.get_constant_voltage = sy6974_get_vchg,
	.set_constant_voltage = sy6974_set_vchg,
	.kick_wdt = sy6974_kick_wdt,
	.set_mivr = sy6974_set_ivl,
	.get_mivr = sy6974_get_ivl,
	.get_mivr_state = sy6974_get_ivl_state,
	.is_charging_done = sy6974_is_charging_done,
	.set_eoc_current = sy6974_set_iterm,
	.enable_termination = sy6974_enable_te,
	.reset_eoc_state = NULL,
	.get_min_charging_current = sy6974_get_min_ichg,
	.get_min_input_current = sy6974_get_min_aicr,

	/* Safety timer */
	.enable_safety_timer = sy6974_set_safety_timer,
	.is_safety_timer_enabled = sy6974_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = sy6974_set_otg,
	.set_boost_current_limit = sy6974_set_boost_ilmt,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	.get_ibus_adc = NULL,

	/* Event */
	.event = sy6974_do_event,

	.get_chr_status = sy6974_get_charging_status,
	.set_hiz_mode = sy6974_set_hiz_mode,
	/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 start */
	.get_hiz_mode = sy6974_get_hiz_mode,
	/*HS03s added for DEVAL5626-463 by wangzikang at 20210729 end */
};

static struct of_device_id sy6974_charger_match_table[] = {
	{
	 .compatible = "sy6974",
	 .data = &pn_data[PN_SY6974],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, sy6974_charger_match_table);

/*HS03s for SR-AL5625-01-511 by wenyaqi at 20210618 start*/
/* ======================= */
/* charger ic Power Supply Ops */
/* ======================= */

enum CHG_STATUS {
	CHG_STATUS_NOT_CHARGING = 0,
	CHG_STATUS_PRE_CHARGE,
	CHG_STATUS_FAST_CHARGING,
	CHG_STATUS_DONE,
};

static int charger_ic_get_online(struct sy6974 *sy,
				     bool *val)
{
	bool pwr_rdy = false;

	sy6974_get_charger_type(sy, &sy->psy_usb_type);
	if(sy->psy_usb_type != POWER_SUPPLY_TYPE_UNKNOWN)
		pwr_rdy = false;
	else
		pwr_rdy = true;

	dev_info(sy->dev, "%s: online = %d\n", __func__, pwr_rdy);
	*val = pwr_rdy;
	return 0;
}

static int charger_ic_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct sy6974 *sy = power_supply_get_drvdata(psy);
	bool pwr_rdy = false;
	int ret = 0;
	int chr_status = 0;

	dev_dbg(sy->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret = charger_ic_get_online(sy, &pwr_rdy);
		val->intval = pwr_rdy;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (sy->psy_usb_type == POWER_SUPPLY_USB_TYPE_UNKNOWN)
		{
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		}
		ret = sy6974_get_chrg_stat(sy, &chr_status);
		if (ret < 0) {
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		}
		switch(chr_status) {
		case CHG_STATUS_NOT_CHARGING:
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
			break;
		case CHG_STATUS_PRE_CHARGE:
		case CHG_STATUS_FAST_CHARGING:
			if(sy->charge_enabled)
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			else
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case CHG_STATUS_DONE:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			ret = -ENODATA;
			break;
		}
		break;
	default:
		ret = -ENODATA;
	}
	return ret;
}

static enum power_supply_property charger_ic_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
};

static const struct power_supply_desc charger_ic_desc = {
	.properties		= charger_ic_properties,
	.num_properties		= ARRAY_SIZE(charger_ic_properties),
	.get_property		= charger_ic_get_property,
};

static char *charger_ic_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};
/*HS03s for SR-AL5625-01-511 by wenyaqi at 20210618 end*/

static int sy6974_charger_remove(struct i2c_client *client);
static int sy6974_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct sy6974 *sy;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct power_supply_config charger_cfg = {};

	int ret = 0;

	sy = devm_kzalloc(&client->dev, sizeof(struct sy6974), GFP_KERNEL);
	if (!sy)
		return -ENOMEM;

	client->addr = 0x6B;
	sy->dev = &client->dev;
	sy->client = client;

	i2c_set_clientdata(client, sy);

	mutex_init(&sy->i2c_rw_lock);

	ret = sy6974_detect_device(sy);
	if (ret) {
		pr_err("No sy6974 device found!\n");
		return -ENODEV;
	}

	match = of_match_node(sy6974_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

	if (sy->part_no != *(int *)match->data)
	{
		pr_info("part no match, hw:%s, devicetree:%s\n",
			pn_str[sy->part_no], pn_str[*(int *) match->data]);
		sy6974_charger_remove(client);
		return -EINVAL;
	} else {
		pr_info("part match, hw:%s, devicetree:%s\n",
			pn_str[sy->part_no], pn_str[*(int *) match->data]);
	}

	sy->platform_data = sy6974_parse_dt(node, sy);

	if (!sy->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

	ret = sy6974_init_device(sy);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	sy6974_register_interrupt(sy);

	sy->chg_dev = charger_device_register(sy->chg_dev_name,
							&client->dev, sy,
							&sy6974_chg_ops,
							&sy6974_chg_props);
	if (IS_ERR_OR_NULL(sy->chg_dev)) {
		ret = PTR_ERR(sy->chg_dev);
		return ret;
	}

	ret = sysfs_create_group(&sy->dev->kobj, &sy6974_attr_group);
	if (ret)
		dev_err(sy->dev, "failed to register sysfs. err: %d\n", ret);

	determine_initial_status(sy);

	/*HS03s for SR-AL5625-01-511 by wenyaqi at 20210618 start*/
	/* power supply register */
	memcpy(&sy->psy_desc,
		&charger_ic_desc, sizeof(sy->psy_desc));
	sy->psy_desc.name = dev_name(&client->dev);

	charger_cfg.drv_data = sy;
	charger_cfg.of_node = sy->dev->of_node;
	charger_cfg.supplied_to = charger_ic_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(charger_ic_supplied_to);
	sy->psy = devm_power_supply_register(&client->dev,
					&sy->psy_desc, &charger_cfg);
	if (IS_ERR(sy->psy)) {
		dev_notice(&client->dev, "Fail to register power supply dev\n");
		ret = PTR_ERR(sy->psy);
	}
	/*HS03s for SR-AL5625-01-511 by wenyaqi at 20210618 end*/

	/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 start*/
	set_hardinfo_charger_data(CHG_INFO, "SY6974");
	/*HS03s for SR-AL5628-01-162 by zhangjiangbin at 20220915 end*/

	pr_err("sy6974 probe successfully, Part Num:%d, Revision:%d\n!",
			sy->part_no, sy->revision);

	return 0;
}

static int sy6974_charger_remove(struct i2c_client *client)
{
	struct sy6974 *sy = i2c_get_clientdata(client);

	mutex_destroy(&sy->i2c_rw_lock);

	sysfs_remove_group(&sy->dev->kobj, &sy6974_attr_group);

	return 0;
}

static void sy6974_charger_shutdown(struct i2c_client *client)
{

}

static struct i2c_driver sy6974_charger_driver = {
	.driver = {
			.name = "sy6974-charger",
			.owner = THIS_MODULE,
			.of_match_table = sy6974_charger_match_table,
			},

	.probe = sy6974_charger_probe,
	.remove = sy6974_charger_remove,
	.shutdown = sy6974_charger_shutdown,

};

module_i2c_driver(sy6974_charger_driver);

MODULE_DESCRIPTION("SY6974 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("ETA");
