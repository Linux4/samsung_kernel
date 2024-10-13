/*
 * BQ2560x battery charging driver
 *
 * Copyright (C) 2013 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define pr_fmt(fmt)	"[bq2560x]:%s: " fmt, __func__

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
#include "bq2560x_reg.h"
#include <charger_class.h>
#include <mtk_charger.h>
#include <mtk_battery.h>
#include <tcpm.h>
#include <mt-plat/mtk_boot.h>
#include <mtk_musb.h>
#include <linux/hardware_info.h>

struct bq2560x_charge_param {
	int vlim;
	int ilim;
	int ichg;
	int vreg;
};

enum stat_ctrl {
	STAT_CTRL_STAT,
	STAT_CTRL_ICHG,
	STAT_CTRL_INDPM,
	STAT_CTRL_DISABLE,
};

enum vboost {
	BOOSTV_4850 = 4850,
	BOOSTV_5000 = 5000,
	BOOSTV_5150 = 5150,
	BOOSTV_5300 = 5300,
};

enum iboost {
	BOOSTI_500 = 500,
	BOOSTI_1200 = 1200,
};

enum vac_ovp {
	VAC_OVP_5500 = 5500,
	VAC_OVP_6500 = 6500,
	VAC_OVP_10500 = 10500,
	VAC_OVP_14000 = 14000,
};


struct bq2560x_platform_data {
	struct bq2560x_charge_param usb;
	int iprechg;
	int iterm;

	enum stat_ctrl statctrl;
	enum vboost boostv;	// options are 4850,
	enum iboost boosti; // options are 500mA, 1200mA
	enum vac_ovp vac_ovp;

};

enum {
	PN_BQ2560X,
	PN_BQ25600D,
	PN_BQ25601,
	PN_BQ25601D,
};

enum bq2560x_part_no {
	SGM41511 = 0x02,
	SGM41512 = 0x05,
	SY6974 = 0x09,
};

static int pn_data[] = {
	[PN_BQ2560X] = 0x09,
	[PN_BQ25600D] = 0x01,
	[PN_BQ25601] = 0x02,
	[PN_BQ25601D] = 0x07,
};

static char *pn_str[] = {
	[PN_BQ2560X] = "sy6974",
	[PN_BQ25600D] = "bq25600d",
	[PN_BQ25601] = "sgm41511",
	[PN_BQ25601D] = "bq25601d",
};

struct bq2560x {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;

	bool chg_det_enable;

	int  chg_type;

	bool hz_mode;

	int status;
	int irq;
	int irq_gpio;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;

	struct bq2560x_platform_data *platform_data;
	struct charger_device *chg_dev;

	struct power_supply *psy;
	struct power_supply_desc psy_desc;
	int psy_usb_type;

#ifdef CONFIG_TCPC_CLASS
	struct power_supply *chg_psy;
	struct tcpc_device *tcpc_dev;
	struct notifier_block pd_nb;
	struct completion chrdet_start;
	struct task_struct *attach_task;
	struct mutex attach_lock;
	bool attach;
	bool tcpc_kpoc;
	struct delayed_work otg_detect_work;
#endif
	struct delayed_work chgstat_detect_work;
};
static void bq2560x_dump_regs(struct bq2560x *bq);
static struct charger_properties bq2560x_chg_props = {
	.alias_name = "bq2560x",
};

static int __bq2560x_read_reg(struct bq2560x *bq, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __bq2560x_write_reg(struct bq2560x *bq, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(bq->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

static int bq2560x_read_byte(struct bq2560x *bq, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2560x_read_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int bq2560x_write_byte(struct bq2560x *bq, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2560x_write_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int bq2560x_update_bits(struct bq2560x *bq, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2560x_read_reg(bq, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq2560x_write_reg(bq, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int bq2560x_enable_otg(struct bq2560x *bq)
{
	u8 val = REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_01, REG01_OTG_CONFIG_MASK,
				   val);

}

static int bq2560x_disable_otg(struct bq2560x *bq)
{
	u8 val = REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_01, REG01_OTG_CONFIG_MASK,
				   val);

}

static int bq2560x_enable_charger(struct bq2560x *bq)
{
	int ret;
	u8 val = REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
	    bq2560x_update_bits(bq, BQ2560X_REG_01, REG01_CHG_CONFIG_MASK, val);

	return ret;
}

static int bq2560x_disable_charger(struct bq2560x *bq)
{
	int ret;
	u8 val = REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT;

	ret =
	    bq2560x_update_bits(bq, BQ2560X_REG_01, REG01_CHG_CONFIG_MASK, val);
	return ret;
}

int bq2560x_set_chargecurrent(struct bq2560x *bq, int curr)
{
	u8 ichg;

	if (curr < REG02_ICHG_BASE)
		curr = REG02_ICHG_BASE;

	ichg = (curr - REG02_ICHG_BASE) / REG02_ICHG_LSB;
	return bq2560x_update_bits(bq, BQ2560X_REG_02, REG02_ICHG_MASK,
				   ichg << REG02_ICHG_SHIFT);

}

int bq2560x_set_term_current(struct bq2560x *bq, int curr)
{
	u8 iterm;

	if (curr < REG03_ITERM_BASE)
		curr = REG03_ITERM_BASE;

	iterm = (curr - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	return bq2560x_update_bits(bq, BQ2560X_REG_03, REG03_ITERM_MASK,
				   iterm << REG03_ITERM_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2560x_set_term_current);

int bq2560x_set_prechg_current(struct bq2560x *bq, int curr)
{
	u8 iprechg;

	if (curr < REG03_IPRECHG_BASE)
		curr = REG03_IPRECHG_BASE;

	iprechg = (curr - REG03_IPRECHG_BASE) / REG03_IPRECHG_LSB;

	return bq2560x_update_bits(bq, BQ2560X_REG_03, REG03_IPRECHG_MASK,
				   iprechg << REG03_IPRECHG_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2560x_set_prechg_current);

int bq2560x_set_chargevolt(struct bq2560x *bq, int volt)
{
	u8 val;

	if (volt < REG04_VREG_BASE)
		volt = REG04_VREG_BASE;

	val = (volt - REG04_VREG_BASE) / REG04_VREG_LSB;
	return bq2560x_update_bits(bq, BQ2560X_REG_04, REG04_VREG_MASK,
				   val << REG04_VREG_SHIFT);
}

int bq2560x_set_input_volt_limit(struct bq2560x *bq, int volt)
{
	u8 val;

	if (volt < REG06_VINDPM_BASE)
		volt = REG06_VINDPM_BASE;

	val = (volt - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
	return bq2560x_update_bits(bq, BQ2560X_REG_06, REG06_VINDPM_MASK,
				   val << REG06_VINDPM_SHIFT);
}

int bq2560x_set_input_current_limit(struct bq2560x *bq, int curr)
{
	u8 val;

	if (curr < REG00_IINLIM_BASE)
		curr = REG00_IINLIM_BASE;

	val = (curr - REG00_IINLIM_BASE) / REG00_IINLIM_LSB;
	return bq2560x_update_bits(bq, BQ2560X_REG_00, REG00_IINLIM_MASK,
				   val << REG00_IINLIM_SHIFT);
}

int bq2560x_set_watchdog_timer(struct bq2560x *bq, u8 timeout)
{
	u8 temp;

	temp = (u8) (((timeout -
		       REG05_WDT_BASE) / REG05_WDT_LSB) << REG05_WDT_SHIFT);

	return bq2560x_update_bits(bq, BQ2560X_REG_05, REG05_WDT_MASK, temp);
}
EXPORT_SYMBOL_GPL(bq2560x_set_watchdog_timer);

int bq2560x_disable_watchdog_timer(struct bq2560x *bq)
{
	u8 val = REG05_WDT_DISABLE << REG05_WDT_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_05, REG05_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2560x_disable_watchdog_timer);

int bq2560x_reset_watchdog_timer(struct bq2560x *bq)
{
	u8 val = REG01_WDT_RESET << REG01_WDT_RESET_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_01, REG01_WDT_RESET_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2560x_reset_watchdog_timer);

int bq2560x_reset_chip(struct bq2560x *bq)
{
	int ret;
	u8 val = REG0B_REG_RESET << REG0B_REG_RESET_SHIFT;

	ret =
	    bq2560x_update_bits(bq, BQ2560X_REG_0B, REG0B_REG_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2560x_reset_chip);

int bq2560x_enter_hiz_mode(struct bq2560x *bq)
{
	u8 val = REG00_HIZ_ENABLE << REG00_ENHIZ_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2560x_enter_hiz_mode);

int bq2560x_exit_hiz_mode(struct bq2560x *bq)
{

	u8 val = REG00_HIZ_DISABLE << REG00_ENHIZ_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_00, REG00_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2560x_exit_hiz_mode);

int bq2560x_hz_mode(struct charger_device *chg_dev, bool en)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);
	pr_err("%s en = %d\n", __func__, en);
	bq->hz_mode = en;

#ifndef WT_COMPILE_FACTORY_VERSION
	if(en) {
		bq2560x_enter_hiz_mode(bq);
	} else {
		bq2560x_exit_hiz_mode(bq);
	}
#endif

	return 0;
}

int bq2560x_get_hiz_mode(struct bq2560x *bq, bool *state)
{
	u8 val;
	int ret;

	ret = bq2560x_read_byte(bq, BQ2560X_REG_00, &val);
	if (ret)
		return ret;
	*state = (val & REG00_ENHIZ_MASK) >> REG00_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(bq2560x_get_hiz_mode);

static int bq2560x_enable_term(struct bq2560x *bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = REG05_TERM_ENABLE << REG05_EN_TERM_SHIFT;
	else
		val = REG05_TERM_DISABLE << REG05_EN_TERM_SHIFT;

	ret = bq2560x_update_bits(bq, BQ2560X_REG_05, REG05_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2560x_enable_term);

int bq2560x_set_boost_current(struct bq2560x *bq, int curr)
{
	u8 val;

	val = REG02_BOOST_LIM_0P5A;
	if (curr >= BOOSTI_1200)
		val = REG02_BOOST_LIM_1P2A;

	return bq2560x_update_bits(bq, BQ2560X_REG_02, REG02_BOOST_LIM_MASK,
				   val << REG02_BOOST_LIM_SHIFT);
}

int bq2560x_set_boost_voltage(struct bq2560x *bq, int volt)
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

	return bq2560x_update_bits(bq, BQ2560X_REG_06, REG06_BOOSTV_MASK,
				   val << REG06_BOOSTV_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2560x_set_boost_voltage);

static int bq2560x_set_acovp_threshold(struct bq2560x *bq, int volt)
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

	return bq2560x_update_bits(bq, BQ2560X_REG_06, REG06_OVP_MASK,
				   val << REG06_OVP_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2560x_set_acovp_threshold);

static int bq2560x_set_stat_ctrl(struct bq2560x *bq, int ctrl)
{
	u8 val;

	val = ctrl;

	return bq2560x_update_bits(bq, BQ2560X_REG_00, REG00_STAT_CTRL_MASK,
				   val << REG00_STAT_CTRL_SHIFT);
}

static int bq2560x_set_int_mask(struct bq2560x *bq, int mask)
{
	u8 val;

	val = mask;

	return bq2560x_update_bits(bq, BQ2560X_REG_0A, REG0A_INT_MASK_MASK,
				   val << REG0A_INT_MASK_SHIFT);
}

static int bq2560x_enable_batfet(struct bq2560x *bq)
{
	const u8 val = REG07_BATFET_ON << REG07_BATFET_DIS_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2560x_enable_batfet);

static int bq2560x_disable_batfet(struct bq2560x *bq)
{
	const u8 val = REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_07, REG07_BATFET_DIS_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2560x_disable_batfet);

static int bq2560x_disable_batfet_rst(struct bq2560x *bq)
{
	const u8 val = REG07_BATFET_RST_OFF << REG07_BATFET_RST_DIS_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_07, REG07_BATFET_RST_DIS_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2560x_disable_batfet_rst);

static int bq2560x_set_batfet_delay(struct bq2560x *bq, uint8_t delay)
{
	u8 val;

	if (delay == 0)
		val = REG07_BATFET_DLY_0S;
	else
		val = REG07_BATFET_DLY_10S;

	val <<= REG07_BATFET_DLY_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_07, REG07_BATFET_DLY_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2560x_set_batfet_delay);

static int bq2560x_enable_safety_timer(struct bq2560x *bq)
{
	const u8 val = REG05_CHG_TIMER_ENABLE << REG05_EN_TIMER_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_05, REG05_EN_TIMER_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2560x_enable_safety_timer);

static int bq2560x_disable_safety_timer(struct bq2560x *bq)
{
	const u8 val = REG05_CHG_TIMER_DISABLE << REG05_EN_TIMER_SHIFT;

	return bq2560x_update_bits(bq, BQ2560X_REG_05, REG05_EN_TIMER_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2560x_disable_safety_timer);

static struct bq2560x_platform_data *bq2560x_parse_dt(struct device_node *np,
						      struct bq2560x *bq)
{
	int ret;
	struct bq2560x_platform_data *pdata;

	pdata = devm_kzalloc(bq->dev, sizeof(struct bq2560x_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &bq->chg_dev_name) < 0) {
		bq->chg_dev_name = "primary_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &bq->eint_name) < 0) {
		bq->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	bq->chg_det_enable =
	    of_property_read_bool(np, "ti,bq2560x,charge-detect-enable");

	ret = of_property_read_u32(np, "ti,bq2560x,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of ti,bq2560x,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "ti,bq2560x,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of ti,bq2560x,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "ti,bq2560x,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of ti,bq2560x,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "ti,bq2560x,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of ti,bq2560x,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "ti,bq2560x,stat-pin-ctrl",
				   &pdata->statctrl);
	if (ret) {
		pdata->statctrl = 0;
		pr_err("Failed to read node of ti,bq2560x,stat-pin-ctrl\n");
	}

	ret = of_property_read_u32(np, "ti,bq2560x,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of ti,bq2560x,precharge-current\n");
	}

	ret = of_property_read_u32(np, "ti,bq2560x,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
		    ("Failed to read node of ti,bq2560x,termination-current\n");
	}

	ret = of_property_read_u32(np, "ti,bq2560x,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of ti,bq2560x,boost-voltage\n");
	}

	ret = of_property_read_u32(np, "ti,bq2560x,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of ti,bq2560x,boost-current\n");
	}

	ret = of_property_read_u32(np, "ti,bq2560x,vac-ovp-threshold",
				   &pdata->vac_ovp);
	if (ret) {
		pdata->vac_ovp = 6500;
		pr_err("Failed to read node of ti,bq2560x,vac-ovp-threshold\n");
	}

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "bq2560x,intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s no intr_gpio info\n", __func__);
	}

	bq->irq_gpio = ret;
#else
	ret = of_property_read_u32(np,
		"bq2560x,intr_gpio_num", &bq->irq_gpio);
	if (ret < 0)
		pr_err("%s no intr_gpio info\n", __func__);
#endif

	return pdata;
}

static int bq2560x_inform_charger_type(struct bq2560x *bq)
{
	// int ret = 0;
	struct power_supply *psy;
	// union power_supply_propval propval;

	psy = power_supply_get_by_name("mtk_charger_type");
	if (!psy)
		return -ENODEV;

	// if (bq->power_good || bq->chg_det_enable)
	// 	propval.intval = 1;
	// else
	// 	propval.intval = 0;

	// ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ONLINE, &propval);
	// if (ret < 0) {
	// 	pr_notice("inform power supply online failed:%d\n", ret);
	// 	return ret;
	// }

	//power_supply_changed(psy);
	//power_supply_changed(bq->psy);

	return 0;
}

static irqreturn_t bq2560x_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	struct bq2560x *bq = data;

	ret = bq2560x_read_byte(bq, BQ2560X_REG_08, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = bq->power_good;

	bq->power_good = !!(reg_val & REG08_PG_STAT_MASK);

	if (!prev_pg && bq->power_good) {
		pr_notice("adapter/usb inserted\n");
		schedule_delayed_work(&bq->chgstat_detect_work, msecs_to_jiffies(500));
	} else if (prev_pg && !bq->power_good) {
		pr_notice("adapter/usb removed\n");
		cancel_delayed_work(&bq->chgstat_detect_work);
		bq2560x_dump_regs(bq);
	}

	bq2560x_inform_charger_type(bq);

	return IRQ_HANDLED;
}

static int bq2560x_register_interrupt(struct bq2560x *bq)
{
	int ret = 0;
	ret = devm_gpio_request(bq->dev, bq->irq_gpio, "bq2560x-irq");
	if (ret < 0) {
		pr_err("Error: failed to request GPIO%d (ret = %d)\n",
		bq->irq_gpio, ret);
		return -EINVAL;
	}
	ret = gpio_direction_input(bq->irq_gpio);
	if (ret < 0) {
		pr_err("Error: failed to set GPIO%d as input pin(ret = %d)\n",
		bq->irq_gpio, ret);
		return -EINVAL;
	}

	bq->irq = gpio_to_irq(bq->irq_gpio);
	if (bq->irq <= 0) {
		pr_err("%s gpio to irq fail, bq->irq(%d)\n",
						__func__, bq->irq);
		return -EINVAL;
	}

	pr_info("%s : IRQ number = %d\n", __func__, bq->irq);

	ret = devm_request_threaded_irq(bq->dev, bq->irq, NULL,
					bq2560x_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					bq->eint_name, bq);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}

#if defined (CONFIG_N23_CHARGER_PRIVATE)
	disable_irq_wake(bq->irq);
#else
	enable_irq_wake(bq->irq);
#endif

	return 0;
}

static int bq2560x_init_device(struct bq2560x *bq)
{
	int ret;

	bq2560x_disable_watchdog_timer(bq);
	bq2560x_exit_hiz_mode(bq);
	bq2560x_disable_batfet_rst(bq);
	ret = bq2560x_set_stat_ctrl(bq, bq->platform_data->statctrl);
	if (ret)
		pr_err("Failed to set stat pin control mode, ret = %d\n", ret);

	ret = bq2560x_set_prechg_current(bq, bq->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	ret = bq2560x_set_term_current(bq, bq->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = bq2560x_set_boost_voltage(bq, bq->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = bq2560x_set_boost_current(bq, bq->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	ret = bq2560x_set_acovp_threshold(bq, bq->platform_data->vac_ovp);
	if (ret)
		pr_err("Failed to set acovp threshold, ret = %d\n", ret);

	ret = bq2560x_enable_term(bq, false);
	if (ret)
		pr_err("Failed to set disable term, ret = %d\n", ret);

	ret = bq2560x_disable_safety_timer(bq);
	if (ret)
		pr_err("Failed to set disable safety time, ret = %d\n", ret);

	ret = bq2560x_set_int_mask(bq,
				   REG0A_IINDPM_INT_MASK |
				   REG0A_VINDPM_INT_MASK);
	if (ret)
		pr_err("Failed to set vindpm and iindpm int mask\n");

	return 0;
}

static void determine_initial_status(struct bq2560x *bq)
{
	bq2560x_irq_handler(bq->irq, (void *) bq);
}

static int bq2560x_detect_device(struct bq2560x *bq)
{
	int ret;
	u8 data;

	ret = bq2560x_read_byte(bq, BQ2560X_REG_0B, &data);
	if (!ret) {
		bq->part_no = (data & REG0B_PN_MASK) >> REG0B_PN_SHIFT;
		bq->revision =
		    (data & REG0B_DEV_REV_MASK) >> REG0B_DEV_REV_SHIFT;
	}

	pr_notice("%s bq2560x:device_id = %d\n",__func__, bq->part_no);
	if(bq->part_no == SY6974){
		pr_notice("/******************charger ic is SY6974***************/\n");
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "SY6974");
	}else if(bq->part_no == SGM41511){
		pr_notice("/******************charger ic is SGM41511***************/\n");
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "SGM41511");
	} else if(bq->part_no == SGM41512){
		pr_notice("/******************charger ic is SGM41512***************/\n");
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "SGM41512");
	}

	return ret;
}

static void bq2560x_dump_regs(struct bq2560x *bq)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = bq2560x_read_byte(bq, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static ssize_t
bq2560x_show_registers(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct bq2560x *bq = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "bq2560x Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = bq2560x_read_byte(bq, addr, &val);
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
bq2560x_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct bq2560x *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x0B) {
		bq2560x_write_byte(bq, (unsigned char) reg,
				   (unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, bq2560x_show_registers,
		   bq2560x_store_registers);

static struct attribute *bq2560x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group bq2560x_attr_group = {
	.attrs = bq2560x_attributes,
};

static int bq2560x_charging(struct charger_device *chg_dev, bool enable)
{

	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = bq2560x_enable_charger(bq);
	else
		ret = bq2560x_disable_charger(bq);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	if(enable && bq->hz_mode)
		ret = bq2560x_hz_mode(chg_dev, true);
	if (ret)
		pr_err("%s set_hz fail\n", __func__);

	ret = bq2560x_read_byte(bq, BQ2560X_REG_01, &val);

	if (!ret)
		bq->charge_enabled = !!(val & REG01_CHG_CONFIG_MASK);

	return ret;
}

static int bq2560x_plug_in(struct charger_device *chg_dev)
{

	int ret;

	ret = bq2560x_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int bq2560x_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = bq2560x_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int bq2560x_dump_register(struct charger_device *chg_dev)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);

	bq2560x_dump_regs(bq);

	return 0;
}

static int bq2560x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);

	*en = bq->charge_enabled;

	return 0;
}

int wt_chg_sw_term = 0;
static int bq2560x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = bq2560x_read_byte(bq, BQ2560X_REG_08, &val);
	if (!ret) {
		val = val & REG08_CHRG_STAT_MASK;
		val = val >> REG08_CHRG_STAT_SHIFT;
		*done = (val == REG08_CHRG_STAT_CHGDONE);
	}

	if(wt_chg_sw_term == true || (val == REG08_CHRG_STAT_CHGDONE)) {
		*done = true;
	}

	return ret;
}

static int bq2560x_is_hz_mode(struct charger_device *chg_dev, bool *done)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	ret = bq2560x_get_hiz_mode(bq, done);
	pr_err("hz stat= %d\n", *done);

	return ret;
}

static int bq2560x_is_otg_enable(struct bq2560x *bq, bool *done)
{
	int ret;
	u8 val;

	ret = bq2560x_read_byte(bq, BQ2560X_REG_01, &val);
	if (!ret) {
		val = val & REG01_OTG_CONFIG_MASK;
		val = val >> REG01_OTG_CONFIG_SHIFT;
		*done = (val == REG01_OTG_ENABLE);
	}

	return ret;
}

static int bq2560x_get_charging_status(struct bq2560x *bq, int *chg_stat)
{
	int ret = 0;
	u8 reg_val = 0;

	ret = bq2560x_read_byte(bq, BQ2560X_REG_08, &reg_val);
	if (ret < 0)
		return ret;

	*chg_stat = (reg_val & REG08_CHRG_STAT_MASK) >> REG08_CHRG_STAT_SHIFT;


	return ret;
}

static int bq2560x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	return bq2560x_set_chargecurrent(bq, curr / 1000);
}

static int bq2560x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = bq2560x_read_byte(bq, BQ2560X_REG_02, &reg_val);
	if (!ret) {
		ichg = (reg_val & REG02_ICHG_MASK) >> REG02_ICHG_SHIFT;
		ichg = ichg * REG02_ICHG_LSB + REG02_ICHG_BASE;
		*curr = ichg * 1000;
	}

	return ret;
}

static int bq2560x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int bq2560x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge volt = %d\n", volt);

	return bq2560x_set_chargevolt(bq, volt / 1000);
}

static int bq2560x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = bq2560x_read_byte(bq, BQ2560X_REG_04, &reg_val);
	if (!ret) {
		vchg = (reg_val & REG04_VREG_MASK) >> REG04_VREG_SHIFT;
		vchg = vchg * REG04_VREG_LSB + REG04_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int bq2560x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);

	return bq2560x_set_input_volt_limit(bq, volt / 1000);

}

static int bq2560x_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);

#ifdef WT_COMPILE_FACTORY_VERSION
	if (bq->hz_mode) {
		curr = 100000;
	}
#endif

	pr_err("indpm curr = %d\n", curr);

	return bq2560x_set_input_current_limit(bq, curr / 1000);
}

static int bq2560x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = bq2560x_read_byte(bq, BQ2560X_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & REG00_IINLIM_MASK) >> REG00_IINLIM_SHIFT;
		icl = icl * REG00_IINLIM_LSB + REG00_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;

}

static int bq2560x_kick_wdt(struct charger_device *chg_dev)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);

	return bq2560x_reset_watchdog_timer(bq);
}

#if defined (CONFIG_N23_CHARGER_PRIVATE)
extern bool otg_enabled;
#endif

static int bq2560x_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);

	bq2560x_dump_regs(bq);

	if (en)
		ret = bq2560x_enable_otg(bq);
	else
		ret = bq2560x_disable_otg(bq);

	pr_err("%s OTG %s\n", en ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

#if defined (CONFIG_N23_CHARGER_PRIVATE)
	otg_enabled = en;
	if (en)
		schedule_delayed_work(&bq->otg_detect_work, 500);
	else
		cancel_delayed_work_sync(&bq->otg_detect_work);
#endif
	bq2560x_dump_regs(bq);

	return ret;
}

static int bq2560x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en)
		ret = bq2560x_enable_safety_timer(bq);
	else
		ret = bq2560x_disable_safety_timer(bq);

	return ret;
}

static int bq2560x_is_safety_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = bq2560x_read_byte(bq, BQ2560X_REG_05, &reg_val);

	if (!ret)
		*en = !!(reg_val & REG05_EN_TIMER_MASK);

	return ret;
}

static int bq2560x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = bq2560x_set_boost_current(bq, curr / 1000);
	bq2560x_dump_register(chg_dev);
	return ret;
}

static int bq2560x_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
#ifdef CONFIG_TCPC_CLASS
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("%s en = %d\n", __func__, en);
	bq->chg_det_enable = en;
	bq2560x_inform_charger_type(bq);
#endif /* CONFIG_TCPC_CLASS */
	return ret;
}

static int bq2560x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct bq2560x *bq = dev_get_drvdata(&chg_dev->dev);

	if (!bq->psy) {
		pr_err("%s: cannot get psy\n", __func__);
		return -ENODEV;
	}

	pr_err("%s enent = %d\n", __func__, event);
	switch (event) {
	case EVENT_FULL:
	case EVENT_RECHARGE:
	case EVENT_DISCHARGE:
		power_supply_changed(bq->psy);
		break;
	default:
		break;
	}
	return 0;
}

static struct charger_ops bq2560x_chg_ops = {
	/* Normal charging */
	.plug_in = bq2560x_plug_in,
	.plug_out = bq2560x_plug_out,
	.dump_registers = bq2560x_dump_register,
	.enable = bq2560x_charging,
	.is_enabled = bq2560x_is_charging_enable,
	.get_charging_current = bq2560x_get_ichg,
	.set_charging_current = bq2560x_set_ichg,
	.get_input_current = bq2560x_get_icl,
	.set_input_current = bq2560x_set_icl,
	.get_constant_voltage = bq2560x_get_vchg,
	.set_constant_voltage = bq2560x_set_vchg,
	.kick_wdt = bq2560x_kick_wdt,
	.set_mivr = bq2560x_set_ivl,
	.is_charging_done = bq2560x_is_charging_done,
	.get_min_charging_current = bq2560x_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = bq2560x_set_safety_timer,
	.is_safety_timer_enabled = bq2560x_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = bq2560x_set_otg,
	.set_boost_current_limit = bq2560x_set_boost_ilmt,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	.hz_mode = bq2560x_hz_mode,
	.is_hz_mode = bq2560x_is_hz_mode,

	.event = bq2560x_do_event,
};

static struct of_device_id bq2560x_charger_match_table[] = {
	{
	 .compatible = "ti,bq2560x",
	 .data = &pn_data[PN_BQ2560X],
	 },
	{
	 .compatible = "ti,bq25600D",
	 .data = &pn_data[PN_BQ25600D],
	 },
	{
	 .compatible = "ti,bq25601",
	 .data = &pn_data[PN_BQ25601],
	 },
	{
	 .compatible = "ti,bq25601D",
	 .data = &pn_data[PN_BQ25601D],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, bq2560x_charger_match_table);

/* ======================= */
/* BQ2560X Power Supply Ops */
/* ======================= */

static int  bq2560x_charger_get_online(struct bq2560x *bq,
				     bool *val)
{
	bool pwr_rdy = false;

	pwr_rdy	= bq->power_good;
	dev_info(bq->dev, "%s: online = %d\n", __func__, pwr_rdy);
	*val = pwr_rdy;

	return 0;
}

static int  bq2560x_charger_set_online(struct bq2560x *bq,
				     const union power_supply_propval *val)
{
	return  bq2560x_enable_chg_type_det(bq->chg_dev, val->intval);
}

static int  bq2560x_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct bq2560x *bq =
						  power_supply_get_drvdata(psy);
	int chg_stat = REG08_CHRG_STAT_IDLE;
	bool pwr_rdy = false;
	int ret = 0;

	dev_dbg(bq->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret =  bq2560x_charger_get_online(bq, &pwr_rdy);
		val->intval = pwr_rdy;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = bq2560x_get_charging_status(bq, &chg_stat);
		if (wt_chg_sw_term == 1) {
			chg_stat = REG08_CHRG_STAT_CHGDONE;
		}
		pr_info("%s chg_stat = %d\n", __func__, chg_stat);
		switch (chg_stat) {
		case REG08_CHRG_STAT_IDLE:
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case REG08_CHRG_STAT_PRECHG:
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case REG08_CHRG_STAT_FASTCHG:
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case REG08_CHRG_STAT_CHGDONE:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			ret = -ENODATA;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (bq->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (bq->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		break;
	default:
		ret = -ENODATA;
	}
	return ret;
}

static int  bq2560x_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct bq2560x *bq = power_supply_get_drvdata(psy);
	int ret;

	dev_dbg(bq->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret =  bq2560x_charger_set_online(bq, val);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int  bq2560x_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property  bq2560x_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_usb_type bq2560x_charger_usb_types[] = {
        POWER_SUPPLY_USB_TYPE_UNKNOWN,
        POWER_SUPPLY_USB_TYPE_SDP,
        POWER_SUPPLY_USB_TYPE_DCP,
        POWER_SUPPLY_USB_TYPE_CDP,
        POWER_SUPPLY_USB_TYPE_C,
        POWER_SUPPLY_USB_TYPE_PD,
        POWER_SUPPLY_USB_TYPE_PD_DRP,
        POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static const struct power_supply_desc  bq2560x_charger_desc = {
	.name			= "bq2560x",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		=  bq2560x_charger_properties,
	.num_properties	= ARRAY_SIZE( bq2560x_charger_properties),
	.get_property	=  bq2560x_charger_get_property,
	.set_property	=  bq2560x_charger_set_property,
	.property_is_writeable	=  bq2560x_charger_property_is_writeable,
	.usb_types		=  bq2560x_charger_usb_types,
	.num_usb_types	= ARRAY_SIZE( bq2560x_charger_usb_types),
};

static char * bq2560x_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};

#ifdef CONFIG_TCPC_CLASS
static int typec_attach_thread(void *data)
{
	struct bq2560x *bq = data;
	int ret = 0;
	bool attach;
	union power_supply_propval val;

	pr_info("%s: ++\n", __func__);
	while (!kthread_should_stop()) {
		wait_for_completion(&bq->chrdet_start);
		mutex_lock(&bq->attach_lock);
		attach = bq->attach;
		mutex_unlock(&bq->attach_lock);
		val.intval = attach;
		power_supply_set_property(bq->chg_psy,
						POWER_SUPPLY_PROP_ONLINE, &val);
	}
	return ret;
}

static void handle_typec_attach(struct bq2560x *bq,
				bool en)
{
	mutex_lock(&bq->attach_lock);
	bq->attach = en;
	complete(&bq->chrdet_start);
	mutex_unlock(&bq->attach_lock);
}
//+Bug682956,yangyuhang.wt ,20210813, add cc polarity node
extern int tcpc_set_cc_polarity_state(int state);
#ifdef CONFIG_TCPC_WUSB3801
extern int wusb3801_typec_cc_orientation(void);
#endif
//-Bug682956,yangyuhang.wt ,20210813, add cc polarity node

static int pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct bq2560x *bq =
		(struct bq2560x *)container_of(nb, struct bq2560x, pd_nb);

	switch (event) {
	case TCP_NOTIFY_TYPEC_STATE:
		if (noti->typec_state.old_state == TYPEC_UNATTACHED &&
		    (noti->typec_state.new_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_CUSTOM_SRC ||
		    noti->typec_state.new_state == TYPEC_ATTACHED_NORP_SRC)) {
			pr_info("%s USB Plug in, pol = %d\n", __func__,
					noti->typec_state.polarity);
			// bq->ignore_usb = false;
			handle_typec_attach(bq, true);
		} else if ((noti->typec_state.old_state == TYPEC_ATTACHED_SNK ||
		    noti->typec_state.old_state == TYPEC_ATTACHED_CUSTOM_SRC ||
			noti->typec_state.old_state == TYPEC_ATTACHED_NORP_SRC)
			&& noti->typec_state.new_state == TYPEC_UNATTACHED) {
			pr_info("%s USB Plug out\n", __func__);
			if (bq->tcpc_kpoc) {
				pr_info("%s: typec unattached, power off\n",
					__func__);
#ifdef FIXME
				kernel_power_off();
#endif
			}
			// bq->ignore_usb = false;
			handle_typec_attach(bq, false);
		} else if (noti->typec_state.old_state == TYPEC_ATTACHED_SRC &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SNK) {
			pr_info("%s Source_to_Sink\n", __func__);
			// bq->ignore_usb = true;
			handle_typec_attach(bq, true);
		}  else if (noti->typec_state.old_state == TYPEC_ATTACHED_SNK &&
			noti->typec_state.new_state == TYPEC_ATTACHED_SRC) {
			pr_info("%s Sink_to_Source\n", __func__);
			// bq->ignore_usb = true;
			handle_typec_attach(bq, false);
		}
//+Bug682956,yangyuhang.wt ,20210813, add cc polarity node
		if(noti->typec_state.new_state != TYPEC_UNATTACHED) {
#ifdef CONFIG_TCPC_WUSB3801
			if(wusb3801_typec_cc_orientation() >= 0){
				tcpc_set_cc_polarity_state(wusb3801_typec_cc_orientation());
				pr_info("%s wusb3801_typec_polarity = %d\n", __func__,wusb3801_typec_cc_orientation());
			}else{
#endif
				tcpc_set_cc_polarity_state(noti->typec_state.polarity + 1);
				pr_info("%s sgm7220_typec_polarity = %d\n", __func__,noti->typec_state.polarity);
#ifdef CONFIG_TCPC_WUSB3801
			}
#endif
		}else{
				pr_info("%s typec_polarity TYPEC_UNATTACHED\n", __func__);
				tcpc_set_cc_polarity_state(0);
		}
//-Bug682956,yangyuhang.wt ,20210813, add cc polarity node
		break;
	default:
		break;
	};
	return NOTIFY_OK;
}

static void get_otg_status_work(struct work_struct *work)
{
	struct bq2560x *bq = container_of(work,
		struct bq2560x, otg_detect_work.work);
	bool is_otg_enabled = false;

	bq2560x_is_otg_enable(bq, &is_otg_enabled);

	pr_err("%s otg_enabled = %d, is_otg_enable = %d\n", __func__, otg_enabled, is_otg_enabled);
	if (otg_enabled && is_otg_enabled == false) {
		bq2560x_enable_otg(bq);
		bq2560x_set_boost_current(bq, 1500);
	}
	schedule_delayed_work(&bq->otg_detect_work, 500);
}

#endif

bool is_bc12_done = false;
static void get_chg_status_work(struct work_struct *work)
{
	struct bq2560x *bq = container_of(work,
				struct bq2560x, chgstat_detect_work.work);
	struct mtk_battery *gm;
	static int detect_cnt = 0;

	gm = get_mtk_battery();
	if (IS_ERR_OR_NULL(gm) || IS_ERR_OR_NULL(bq->psy)) {
		pr_err("%s get battery fail\n", __func__);
		return;
	}

	pr_err("%s online:%d bat_status:%d\n", __func__, bq->power_good, gm->bs_data.bat_status);
	if (bq->power_good && gm->bs_data.bat_status != POWER_SUPPLY_STATUS_CHARGING
					   && gm->bs_data.bat_status != POWER_SUPPLY_STATUS_FULL) {
		pr_err("%s chg_stat cnt = %d\n", __func__, detect_cnt);
		if (detect_cnt > 10) {
			detect_cnt = 0;
			return;
		}
		if (is_bc12_done)
			power_supply_changed(bq->psy);
		schedule_delayed_work(&bq->chgstat_detect_work, msecs_to_jiffies(100));
		detect_cnt++;
	} else {
		detect_cnt = 0;
		pr_err("%s chg_stat ok!\n", __func__);
	}
}

static int bq2560x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct bq2560x *bq;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct power_supply_config charger_cfg = {};

	int ret = 0;

	pr_err("bq2560x probe enter\n");
	bq = devm_kzalloc(&client->dev, sizeof(struct bq2560x), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->dev = &client->dev;
	bq->client = client;

	i2c_set_clientdata(client, bq);

	mutex_init(&bq->i2c_rw_lock);
#ifdef CONFIG_TCPC_CLASS
	init_completion(&bq->chrdet_start);
	mutex_init(&bq->attach_lock);
#endif

	ret = bq2560x_detect_device(bq);
	if (ret) {
		pr_err("No bq2560x device found!\n");
		return -ENODEV;
	}

	match = of_match_node(bq2560x_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

	if (bq->part_no != *(int *)match->data)
		pr_info("part no mismatch, hw:%s, devicetree:%s\n",
			pn_str[bq->part_no], pn_str[*(int *) match->data]);

	bq->platform_data = bq2560x_parse_dt(node, bq);

	if (!bq->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}

#ifdef FIXME
	ret = get_boot_mode();
	if (ret == KERNEL_POWER_OFF_CHARGING_BOOT ||
	    ret == LOW_POWER_OFF_CHARGING_BOOT)
		bq->tcpc_kpoc = true;
#endif

	ret = bq2560x_init_device(bq);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	bq2560x_register_interrupt(bq);

	bq->chg_dev = charger_device_register(bq->chg_dev_name,
					      &client->dev, bq,
					      &bq2560x_chg_ops,
					      &bq2560x_chg_props);
	if (IS_ERR_OR_NULL(bq->chg_dev)) {
		ret = PTR_ERR(bq->chg_dev);
		return ret;
	}

	/* power supply register */
	memcpy(&bq->psy_desc, &bq2560x_charger_desc, sizeof(bq->psy_desc));
	charger_cfg.drv_data = bq;
	charger_cfg.of_node = client->dev.of_node;
	charger_cfg.supplied_to =  bq2560x_charger_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE( bq2560x_charger_supplied_to);
	bq->psy = devm_power_supply_register(&client->dev,
					&bq->psy_desc, &charger_cfg);
	if (IS_ERR(bq->psy)) {
		pr_err("Fail to register power supply dev\n");
		ret = PTR_ERR(bq->psy);
		goto err_register_psy;
	}

	ret = sysfs_create_group(&bq->dev->kobj, &bq2560x_attr_group);
	if (ret)
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);

#ifdef CONFIG_TCPC_CLASS
	bq->chg_psy = power_supply_get_by_name("bq2560x");
	if (IS_ERR(bq->chg_psy)) {
		pr_err("Failed to get charger psy\n");
		ret = PTR_ERR(bq->chg_psy);
		goto err_psy_get_phandle;
	}

	bq->attach_task = kthread_run(typec_attach_thread, bq,
					"attach_thread");
	if (IS_ERR(bq->attach_task)) {
		ret = PTR_ERR(bq->attach_task);
		goto err_attach_task;
	}

	bq->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!bq->tcpc_dev) {
		pr_notice("%s get tcpc device type_c_port0 fail\n", __func__);
		ret = -ENODEV;
		goto err_get_tcpcdev;
	}
	bq->pd_nb.notifier_call = pd_tcp_notifier_call;
	ret = register_tcp_dev_notifier(bq->tcpc_dev, &bq->pd_nb,
					TCP_NOTIFY_TYPE_ALL);
	if (ret < 0) {
		pr_notice("%s: register tcpc notifer fail\n", __func__);
		ret = -EINVAL;
		goto err_register_tcp_notifier;
	}

	INIT_DELAYED_WORK(&bq->otg_detect_work, get_otg_status_work);
#endif
	INIT_DELAYED_WORK(&bq->chgstat_detect_work, get_chg_status_work);
	ret = sysfs_create_group(&bq->dev->kobj, &bq2560x_attr_group);
	if (ret)
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);

#if defined (CONFIG_N23_CHARGER_PRIVATE)
	enable_irq_wake(bq->irq);
#endif
	determine_initial_status(bq);

	pr_err("bq2560x probe successfully, Part Num:%d, Revision:%d\n",
	       bq->part_no, bq->revision);

	return 0;

#ifdef CONFIG_TCPC_CLASS
err_register_tcp_notifier:
err_get_tcpcdev:
	kthread_stop(bq->attach_task);
err_attach_task:
err_psy_get_phandle:
#endif
err_register_psy:
	charger_device_unregister(bq->chg_dev);
	return ret;
}

static int bq2560x_charger_remove(struct i2c_client *client)
{
	struct bq2560x *bq = i2c_get_clientdata(client);

	mutex_destroy(&bq->i2c_rw_lock);

	sysfs_remove_group(&bq->dev->kobj, &bq2560x_attr_group);

	return 0;
}

static void bq2560x_charger_shutdown(struct i2c_client *client)
{
	struct bq2560x *bq = i2c_get_clientdata(client);

	bq2560x_disable_otg(bq);
}

static struct i2c_driver bq2560x_charger_driver = {
	.driver = {
		   .name = "bq2560x-charger",
		   .owner = THIS_MODULE,
		   .of_match_table = bq2560x_charger_match_table,
		   },

	.probe = bq2560x_charger_probe,
	.remove = bq2560x_charger_remove,
	.shutdown = bq2560x_charger_shutdown,

};

module_i2c_driver(bq2560x_charger_driver);

MODULE_DESCRIPTION("TI BQ2560x Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");