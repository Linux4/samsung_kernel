/*
 * SGM41542  charging driver
 *
 * Copyright (C) 2022 SGMICRO
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

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
#include <linux/of_device.h>
#include <asm/unaligned.h>
#include <linux/pinctrl/consumer.h>

#include <linux/of_gpio.h>
#include <linux/qti_power_supply.h>
#include "sgm41542_charger.h"
#include "sgm41542_iio.h"
#include <linux/hardware_info.h>
//#include "sgm41542_reg.h"

#define CHARGER_IC_NAME "SGM41543D"

enum {
	AFC_INIT,
	NOT_AFC,
	AFC_FAIL,
	AFC_DISABLE,
	NON_AFC_MAX
};

enum {
	AFC_5V = NON_AFC_MAX,
	AFC_9V,
	AFC_12V,
};

static struct sgm41542_device *g_sgm_chg;

static DEFINE_MUTEX(sgm41542_i2c_lock);

static struct of_device_id sgm41542_charger_match_table[] = {
	{.compatible = "sgmicro,sgm41542",},
	{},
};

//static void sgm41542_notify_device_mode(struct sgm41542_device *sgm_chg, bool enable);
static int sgm41542_get_vbus_volt(struct sgm41542_device *sgm_chg,  int *val);
static int sgm41542_get_usb_type(struct sgm41542_device *sgm_chg);
static int sgm41542_request_dpdm(struct sgm41542_device *sgm_chg, bool enable);
static void sgm41542_update_wtchg_work(struct sgm41542_device *sgm_chg);
static int sgm41542_get_hiz_mode(struct sgm41542_device *sgm_chg, u8 *state);
static int sgm41542_set_input_suspend(struct sgm41542_device *sgm_chg, int suspend);
static int sgm41542_set_charge_voltage(struct sgm41542_device *sgm_chg, u32 uV);
static int sgm41542_get_ibat_curr(struct sgm41542_device *sgm_chg,  int *val);
static int sgm41542_get_vbat_volt(struct sgm41542_device *sgm_chg,  int *val);
static void sgm41542_dynamic_adjust_charge_voltage(struct sgm41542_device *sgm_chg, int vbat);
//P231018-03087 gudi.wt 20231103,fix add hiz func
void typec_set_input_suspend_for_usbif_upm6910(bool enable);
//+P86801EA2-300 gudi.wt battery protect function
#ifdef CONFIG_QGKI_BUILD
bool sgm41542_if_been_used(void);
#endif
//-P86801EA2-300 gudi.wt battery protect function

static int sgm41542_read_byte(struct sgm41542_device *sgm_chg, u8 *data, u8 reg)
{
	int ret;

	//mutex_lock(&sgm41542_i2c_lock);
	ret = i2c_smbus_read_byte_data(sgm_chg->client, reg);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "failed to read 0x%.2x\n", reg);
	//	mutex_unlock(&sgm41542_i2c_lock);
		return ret;
	}

	*data = (u8)ret;
	//mutex_unlock(&sgm41542_i2c_lock);

	return 0;
}

static int sgm41542_write_byte(struct sgm41542_device *sgm_chg, u8 reg, u8 data)
{
	int ret;

//	mutex_lock(&sgm41542_i2c_lock);
	ret = i2c_smbus_write_byte_data(sgm_chg->client, reg, data);
//	mutex_unlock(&sgm41542_i2c_lock);

	return ret;
}

static int sgm41542_update_bits(struct sgm41542_device *sgm_chg,
													u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	ret = sgm41542_read_byte(sgm_chg, &tmp, reg);

	if (ret)
		return ret;

	tmp &= ~mask;
	tmp |= data & mask;

	return sgm41542_write_byte(sgm_chg, reg, tmp);
}

static void sgm41542_stay_awake(struct sgm41542_wakeup_source *source)
{
	if (__test_and_clear_bit(0, &source->disabled)) {
		__pm_stay_awake(source->source);
		pr_err("enabled source %s\n", source->source->name);
	}
}

static void sgm41542_relax(struct sgm41542_wakeup_source *source)
{
	if (!__test_and_set_bit(0, &source->disabled)) {
		__pm_relax(source->source);
		pr_err("disabled source %s\n", source->source->name);
	}
}

static bool sgm41542_wake_active(struct sgm41542_wakeup_source *source)
{
	return !source->disabled;
}
static int sgm41542_enter_hiz_mode(struct sgm41542_device *sgm_chg);
static int sgm41542_enter_ship_mode(struct sgm41542_device *sgm_chg)
{
	int ret = 0;
	u8 val;
	sgm41542_enter_hiz_mode(sgm_chg);
	val = SGM41542_BATFET_OFF << SGM41542_BATFET_DIS_SHIFT;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_07, SGM41542_BATFET_DIS_MASK, val);

	return ret;
}

static int sgm41542_ship_mode_delay_enable(struct sgm41542_device *sgm_chg, bool enable)
{
	int ret = 0;
	u8 val;
	if (enable) {
		val = SGM41542_BATFET_DLY_ON << SGM41542_BATFET_DLY_SHIFT;
	} else {
		val = SGM41542_BATFET_DLY_OFF << SGM41542_BATFET_DLY_SHIFT;
	}

	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_07, SGM41542_BATFET_DLY_MASK, val);

	return ret;
}

static int sgm41542_chg_enable(struct sgm41542_device *sgm_chg, bool en)
{
	gpio_direction_output(sgm_chg->chg_en_gpio, !en);
	msleep(5);

	return 0;
}

/*static int sgm41542_jeita_enable(struct sgm41542_device *sgm_chg, bool en)
{
	u8 val;

	val = en << SGM41542_JEITA_EN_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_0D,
							   SGM41542_JEITA_EN_MASK, val);
}*/


#if 0
static int sgm41542_set_otg_volt(struct sgm41542_device *sgm_chg, int volt)
{
	u8 val = 0;

	if (volt < SGM41542_BOOSTV_BASE)
		volt = SGM41542_BOOSTV_BASE;
	if (volt > SGM41542_BOOSTV_BASE + (SGM41542_BOOSTV_MASK >> SGM41542_BOOSTV_SHIFT) * SGM41542_BOOSTV_LSB)
		volt = SGM41542_BOOSTV_BASE + (SGM41542_BOOSTV_MASK >> SGM41542_BOOSTV_SHIFT) * SGM41542_BOOSTV_LSB;

	val = ((volt - SGM41542_BOOSTV_BASE) / SGM41542_BOOSTV_LSB) << SGM41542_BOOSTV_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_06, SGM41542_BOOSTV_MASK, val);

}
#endif

static int sgm41542_set_otg_current(struct sgm41542_device *sgm_chg, int curr)
{
	u8 val;
	u8 temp;

	if (curr < 1200) {
		temp = SGM41542_BOOST_LIM_500MA;
	} else {
		temp = SGM41542_BOOST_LIM_1200MA;
	}

	val = temp << SGM41542_BOOST_LIM_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_02, SGM41542_BOOST_LIM_MASK, val);
}


static int sgm41542_enable_otg(struct sgm41542_device *sgm_chg)
{
	u8 val;

	val = SGM41542_OTG_ENABLE << SGM41542_OTG_CONFIG_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_01,
							   SGM41542_OTG_CONFIG_MASK, val);
}

static int sgm41542_disable_otg(struct sgm41542_device *sgm_chg)
{
	u8 val;

	val = SGM41542_OTG_DISABLE << SGM41542_OTG_CONFIG_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_01,
							   SGM41542_OTG_CONFIG_MASK, val);
}

static int sgm41542_set_otg(struct sgm41542_device *sgm_chg, int enable)
{
	int ret;
	u8 val=0;

	pr_err("sgm41542_set_otg enter enable=%d \n", enable);
	if (enable) {
//+chk3594, liyiying.wt, 2022/7/18, add, disable hiz-mode when plug on otg
		ret = sgm41542_get_hiz_mode(sgm_chg, &val);
		if (ret < 0) {
			dev_err(sgm_chg->dev, "Failed to get hiz val-%d\n", ret);
		}
		if (val == 1) {
			sgm41542_set_input_suspend(sgm_chg, false);
			dev_err(sgm_chg->dev, "sgm41542_exit_hiz_mode -%d\n", val);
		} else {
			dev_err(sgm_chg->dev, "sgm41542_not_exit_hiz_mode -%d\n", val);
		}
//-chk3594, liyiying.wt, 2022/7/18, add, disable hiz-mode when plug on otg

		ret = sgm41542_enable_otg(sgm_chg);
		if (ret < 0) {
			dev_err(sgm_chg->dev, "Failed to enable otg-%d\n", ret);
			return ret;
		}
		gpio_direction_output(sgm_chg->otg_en_gpio, 1);
	} else {
		ret = sgm41542_disable_otg(sgm_chg);
		if (ret < 0) {
			dev_err(sgm_chg->dev, "Failed to disable otg-%d\n", ret);
		}
		gpio_direction_output(sgm_chg->otg_en_gpio, 0);
	}

	return ret;
}

static int sgm41542_enable_charger(struct sgm41542_device *sgm_chg)
{
	int ret =0;
	u8 val = SGM41542_CHG_ENABLE << SGM41542_CHG_CONFIG_SHIFT;

	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_01, SGM41542_CHG_CONFIG_MASK, val);

	return ret;
}

static int sgm41542_disable_charger(struct sgm41542_device *sgm_chg)
{
	int ret = 0;
	u8 val = SGM41542_CHG_DISABLE << SGM41542_CHG_CONFIG_SHIFT;

	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_01, SGM41542_CHG_CONFIG_MASK, val);

	return ret;
}

static int sgm41542_set_chg_enable(struct sgm41542_device *sgm_chg, int enable)
{
	int ret =0;

	dev_err(sgm_chg->dev, "set charge: enable: %d\n", enable);
	if (enable) {
		sgm41542_chg_enable(sgm_chg, true);
		sgm41542_enable_charger(sgm_chg);
	} else {
		sgm41542_chg_enable(sgm_chg, false);
		sgm41542_disable_charger(sgm_chg);
	}

	return ret;
}

static int sgm41542_set_charge_current(struct sgm41542_device *sgm_chg, int curr)
{
	u8 ichg;
	u8 val;

	dev_err(sgm_chg->dev, "%s: %d\n", __func__, curr);

	//bug 816469,tankaikun.wt,mod,2023/1/30, IR compensation
	sgm_chg->final_cc = curr;

	ichg = (curr - SGM41542_ICHG_BASE)/SGM41542_ICHG_LSB;
	val= ichg << SGM41542_ICHG_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_02, SGM41542_ICHG_MASK, val);

}

static int sgm41542_set_term_current(struct sgm41542_device *sgm_chg, int curr)
{
	u8 iterm;
	u8 val;

	dev_err(sgm_chg->dev, "%s: %d\n", __func__, curr);
	if (curr < SGM41542_ITERM_MIN)
		curr = SGM41542_ITERM_MIN;
	if (curr > SGM41542_ITERM_MAX)
		curr = SGM41542_ITERM_MAX;
	iterm = (curr - SGM41542_ITERM_BASE) / SGM41542_ITERM_LSB;
	val = iterm << SGM41542_ITERM_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_03, SGM41542_ITERM_MASK, val);
}

static int sgm41542_set_prechg_current(struct sgm41542_device *sgm_chg, int curr)
{
	u8 iprechg;
	u8 val;

	dev_err(sgm_chg->dev, "%s: %d\n", __func__, curr);
	iprechg = (curr - SGM41542_IPRECHG_BASE) / SGM41542_IPRECHG_LSB;
	val = iprechg << SGM41542_IPRECHG_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_03, SGM41542_IPRECHG_MASK, val);
}

static int sgm41542_set_input_current_limit(struct sgm41542_device *sgm_chg, int curr)
{
	u8 val;
	u8 input_curr;

	dev_err(sgm_chg->dev, "%s: %d\n", __func__, curr);
	dev_err(sgm_chg->dev, "%s: after %d\n", __func__, curr);
	printk("set_input_current_limit curr val:%d\n", curr);
	input_curr = (curr - SGM41542_IINLIM_BASE) / SGM41542_IINLIM_LSB;
	val = input_curr << SGM41542_IINLIM_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_00, SGM41542_IINLIM_MASK, val);
}

static int sgm41542_set_vac_ovp(struct sgm41542_device *sgm_chg, int ovp)
{
	int val;

	if (ovp <= SGM41542_OVP_5000mV) {
		val = SGM41542_OVP_5500mV;
	} else if (ovp > SGM41542_OVP_5000mV && ovp <= SGM41542_OVP_6000mV) {
		val = SGM41542_OVP_6500mV;
	} else if (ovp > SGM41542_OVP_6000mV && ovp <= SGM41542_OVP_8000mV) {
		val = SGM41542_OVP_10500mV;
	} else {
		val = SGM41542_OVP_14000mV;
	}

	val = val << SGM41542_OVP_SHIFT;
	return sgm41542_update_bits(sgm_chg, SGM41542_REG_06, SGM41542_OVP_MASK, val);
}

static int sgm41542_set_bootstv(struct sgm41542_device *sgm_chg, int volt)
{
	int val;

	if (volt <= SGM41542_BOOSTV_4850mV) {
		val = SGM41542_BOOSTV_SET_4850MV;
	} else if (volt > SGM41542_BOOSTV_4850mV && volt <= SGM41542_BOOSTV_5000mV) {
		val = SGM41542_BOOSTV_SET_5000MV;
	} else if (volt > SGM41542_BOOSTV_5000mV && volt <= SGM41542_BOOSTV_5150mV) {
		val = SGM41542_BOOSTV_SET_5150MV;
	} else {
		val = SGM41542_BOOSTV_SET_5300MV;
	}

	val = val << SGM41542_BOOSTV_SHIFT;
	return sgm41542_update_bits(sgm_chg, SGM41542_REG_06, SGM41542_BOOSTV_MASK, val);
}

static int sgm41542_enable_bc12_detect(struct sgm41542_device *sgm_chg)
{
	int ret = 0;
	u8 val = SGM41542_IINDET_ENABLE << SGM41542_IINDET_SHIFT;

	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_07, SGM41542_IINDET_MASK, val);

	return ret;
}

//+bug 816469,tankaikun.wt,mod,2023/1/30, IR compensation
static int sgm41542_set_chargevoltage(struct sgm41542_device *sgm_chg, int volt)
{
	u8 vchg;
	u8 vchg1;

	dev_err(sgm_chg->dev, "%s: %d\n", __func__, volt);

	if (volt == 4400)
		vchg1 = 0x11;
	else if (volt == 4350)
		vchg1 = 0x10;
	else if (volt == 4330)
		vchg1 = 0x0F;
	else if (volt == 4310)
		vchg1 = 0x0E;
	else if (volt == 4280)
		vchg1 = 0x0D;
	else if (volt == 4240)
		vchg1 = 0x0C;
	else
		vchg1 = (u8)((volt - SGM41542_VREG_BASE) / SGM41542_VREG_LSB);

	vchg = vchg1 << SGM41542_VREG_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_04, SGM41542_VREG_MASK, vchg);
}


static int sgm41542_set_charge_voltage(struct sgm41542_device *sgm_chg, u32 mV)
{
	int ret=0;
	sgm_chg->final_cv = mV * 1000;

	ret = sgm41542_set_chargevoltage(sgm_chg, mV);

	return ret;
}
//-bug 816469,tankaikun.wt,mod,2023/1/30, IR compensation

//+P86801EA2-300 gudi.wt battery protect function
#ifdef CONFIG_QGKI_BUILD
bool sgm41542_if_been_used(void)
{
	if(g_sgm_chg == NULL){
		return false;
	}

	return true;
}
EXPORT_SYMBOL(sgm41542_if_been_used);
#endif
//-P86801EA2-300 gudi.wt battery protect function

static int sgm41542_enable_term(struct sgm41542_device *sgm_chg, bool enable)
{
	u8 val;
	int ret;

	if (enable) {
		val = SGM41542_TERM_ENABLE << SGM41542_EN_TERM_SHIFT;
	} else {
		val = SGM41542_TERM_DISABLE << SGM41542_EN_TERM_SHIFT;
	}

	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_05, SGM41542_EN_TERM_MASK, val);

	return ret;
}

static int sgm41542_enable_safety_timer(struct sgm41542_device *sgm_chg, bool enable)
{
	u8 val;
	int ret;

	if (enable) {
		val = SGM41542_CHG_TIMER_ENABLE << SGM41542_EN_TIMER_SHIFT;
	} else {
		val = SGM41542_CHG_TIMER_DISABLE << SGM41542_EN_TIMER_SHIFT;
	}

	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_05, SGM41542_EN_TIMER_MASK, val);

	return ret;
}

//+bug 816469,tankaikun.wt,mod,2023/1/30, IR compensation
static int sgm41542_disable_watchdog_timer(struct sgm41542_device *sgm_chg, bool disable_wtd)
{
	u8 val;

	if (disable_wtd)
		val = SGM41542_WDT_DISABLE << SGM41542_WDT_SHIFT;
	else
		val = SGM41542_WDT_160S << SGM41542_WDT_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_05, SGM41542_WDT_MASK, val);
}

static int sgm41542_feed_watchdog_timer(struct sgm41542_device *sgm_chg)
{
	u8 val;
	int vbat_uV, ibat_ma;

	if (sgm_chg->cfg.enable_ir_compensation) {
		sgm41542_get_ibat_curr(sgm_chg, &ibat_ma);
		sgm41542_get_vbat_volt(sgm_chg, &vbat_uV);
		if ((ibat_ma > 10) && (vbat_uV >= sgm_chg->final_cv)) {
			sgm41542_dynamic_adjust_charge_voltage(sgm_chg, vbat_uV);
		}
	}

	dev_err(sgm_chg->dev, "main chg feed wtd \n");
	val = 1 << SGM41542_WDT_RST_SHIFT;
	sgm41542_update_bits(sgm_chg, SGM41542_REG_01, SGM41542_WDT_RST_MASK, val);

	return 0;
}
//-bug 816469,tankaikun.wt,mod,2023/1/30, IR compensation

static int sgm41542_reset_chip(struct sgm41542_device *sgm_chg)
{
	u8 val;
	int ret;

	val = SGM41542_RESET << SGM41542_RESET_SHIFT;

	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0B, SGM41542_RESET_MASK, val);

	return ret;
}

static int sgm41542_get_hiz_mode(struct sgm41542_device *sgm_chg, u8 *state)
{
	u8 val;
	int ret;

	ret = sgm41542_read_byte(sgm_chg, &val, SGM41542_REG_00);
	if (ret)
		return ret;

	*state = (val & SGM41542_ENHIZ_MASK) >> SGM41542_ENHIZ_SHIFT;

	return 0;
}

static int sgm41542_set_indpm_mask(struct sgm41542_device *sgm_chg, bool enable)
{
	u8 val;

	dev_err(sgm_chg->dev, "%s: %d\n", __func__, enable);
	if (enable) {
		val = SGM41542_IINDPM_INT_ENABLE;
	} else {
		val = SGM41542_IINDPM_INT_DISABLE;
	}

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_0A, SGM41542_IINDPM_INT_MASK, val);
}

static int sgm41542_set_vndpm_mask(struct sgm41542_device *sgm_chg, bool enable)
{
	u8 val;

	dev_err(sgm_chg->dev, "%s: %d\n", __func__, enable);
	if (enable) {
		val = SGM41542_VINDPM_INT_ENABLE << SGM41542_VINDPM_INT_SHIFT;
	} else {
		val = SGM41542_VINDPM_INT_DISABLE << SGM41542_VINDPM_INT_SHIFT;
	}

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_0A, SGM41542_VINDPM_INT_MASK, val);
}

static int sgm41542_enable_battery_rst_en(struct sgm41542_device *sgm_chg, bool enable)
{
	u8 val;

	dev_err(sgm_chg->dev, "%s: %d\n", __func__, enable);
	if (enable) {
		val = SGM41542_BATFET_RST_ENABLE << SGM41542_BATFET_RST_EN_SHIFT;
	} else {
		val = SGM41542_BATFET_RST_DISABLE<< SGM41542_BATFET_RST_EN_SHIFT;
	}

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_07, SGM41542_BATFET_RST_EN_MASK, val);
}

static int sgm41542_set_vndpm(struct sgm41542_device *sgm_chg, int vol)
{
	u8 vindpm;
	u8 val;

	dev_err(sgm_chg->dev, "%s: %d\n", __func__, vol);


	vindpm = (vol - SGM41542_VINDPM_BASE)/SGM41542_VINDPM_LSB;
	val= vindpm << SGM41542_VINDPM_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_06, SGM41542_VINDPM_MASK, val);

}


static int sgm41542_enter_hiz_mode(struct sgm41542_device *sgm_chg)
{
	u8 val;

	val = SGM41542_HIZ_ENABLE << SGM41542_ENHIZ_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_00, SGM41542_ENHIZ_MASK, val);

}

static int sgm41542_exit_hiz_mode(struct sgm41542_device *sgm_chg)
{

	u8 val;

	val = SGM41542_HIZ_DISABLE << SGM41542_ENHIZ_SHIFT;

	return sgm41542_update_bits(sgm_chg, SGM41542_REG_00, SGM41542_ENHIZ_MASK, val);

}

static int sgm41542_set_input_suspend(struct sgm41542_device *sgm_chg, int suspend)
{

	int  ret = 0;

	dev_err(sgm_chg->dev, "set input suspend: %d\n", suspend);

	if (suspend) {
		sgm_chg->hiz_mode = true; //bug 761884, tankaikun@wt, add 20220806, fix user set hiz mode, vbus online update slowly
		sgm41542_enter_hiz_mode(sgm_chg);
	} else {
		sgm_chg->hiz_mode = false; //bug 761884, tankaikun@wt, add 20220806, fix user set hiz mode, vbus online update slowly
		sgm41542_exit_hiz_mode(sgm_chg);
	}

	return ret;
}

//+P231018-03087 gudi.wt 20231103,fix add hiz func
void typec_set_input_suspend_for_usbif_upm6910(bool enable)
{

	if(g_sgm_chg == NULL){
		return;
	}

	dev_err(g_sgm_chg->dev, "typec set input suspend: %d\n", enable);
	if (enable) {
		g_sgm_chg->hiz_mode = true; //bug 761884, tankaikun@wt, add 20220806, fix user set hiz mode, vbus online update slowly
		sgm41542_enter_hiz_mode(g_sgm_chg);
	} else {
		g_sgm_chg->hiz_mode = false; //bug 761884, tankaikun@wt, add 20220806, fix user set hiz mode, vbus online update slowly
		sgm41542_exit_hiz_mode(g_sgm_chg);
	}
}
//-P231018-03087 gudi.wt 20231103,fix add hiz func
//+ bug 761884, tankaikun@wt, add 20220708, charger bring up
#ifdef CONFIG_SGM41542_ENABLE_HVDCP
static int sgm41542_enable_qc20_hvdcp_5v(struct sgm41542_device *sgm_chg)
{
	int ret;
	int dp_val, dm_val;

	/*dp and dm connected,dp 0.6V dm Hiz*/
    dp_val = 0x2<<3;
    ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DP_VSET_MASK, dp_val); //dp 0.6V
    if (ret)
        return ret;

	dm_val = 0;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DM_VSET_MASK, dm_val); //dm Hiz
    if (ret)
        return ret;
	msleep(100);

	return ret;
}

static int sgm41542_set_enhvdcp(struct sgm41542_device *sgm_chg, bool enable)
{
	int ret;
	int val;


	if (enable) {
		val = SGM41542_ENHVDCP_MASK;
	} else {
		val = SGM41542_ENHVDCP_DISABLE << SGM41542_ENHVDCP_SHIFT;
	}

	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_ENHVDCP_MASK, val);
	return ret;
}


static int sgm41542_enable_qc20_hvdcp_9v(struct sgm41542_device *sgm_chg)
{
	int ret;
	int dp_val, dm_val;

	/*dp and dm connected,dp 0.6V dm Hiz*/
    dp_val = 0x2<<3;
    ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DP_VSET_MASK, dp_val); //dp 0.6V
    if (ret)
        return ret;

	dm_val = 0;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DM_VSET_MASK, dm_val); //dm Hiz
    if (ret)
        return ret;
	msleep(100);

	/* dp 3.3v and dm 0.6v out 9V */
	dp_val = SGM41542_DP_VSET_MASK;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DP_VSET_MASK, dp_val); //dp 3.3v
	if (ret)
		return ret;

	dm_val = 0x2<<1;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DM_VSET_MASK, dm_val); //dm 0.6v

	return ret;
}

/* step 1. entry QC3.0 mode
   step 2. up or down 200mv
   step 3. retry step 2 */
static int sgm41542_enable_qc30_hvdcp(struct sgm41542_device *sgm_chg)
{
	int ret;
	int dp_val, dm_val;

	dp_val = 0x2<<3;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DP_VSET_MASK, dp_val); //dp 0.6v
	if (ret)
		return ret;

	dm_val = 0x2<<1;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DM_VSET_MASK, dm_val); //dm 0.6v
	if (ret)
        return ret;

	msleep(1250);
	dm_val = 0x2;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DM_VSET_MASK, dm_val); //dm 0V
	mdelay(100);

	dm_val = SGM41542_DM_VSET_MASK;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DM_VSET_MASK, dm_val); //dm 3.3v

	return ret;
}

static int sgm41542_set_dp0p6_for_afc(struct sgm41542_device *sgm_chg)
{
	int ret;
	int dp_val;

	dp_val = 0x2<<3;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DP_VSET_MASK, dp_val); //dp 0.6v

	return ret;
}


// Must enter 3.0 mode to call ,otherwise cannot step correctly.
static int sgm41542_qc30_step_up_vbus(struct sgm41542_device *sgm_chg)
{
	int ret;
	int dp_val;

	/*  dm 3.3v to dm 0.6v  step up 200mV when IC is QC3.0 mode*/
	dp_val = SGM41542_DP_VSET_MASK;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DP_VSET_MASK, dp_val); //dp 3.3v
	if (ret)
		return ret;
	msleep(10);
	dp_val = 0x2<<3;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DP_VSET_MASK, dp_val); //dp 0.6v
	if (ret)
		return ret;

	udelay(100);
	return ret;
}
// Must enter 3.0 mode to call ,otherwise cannot step correctly.
static int sgm41542_qc30_step_down_vbus(struct sgm41542_device *sgm_chg)
{
	int ret;
	int dm_val;

	/* dp 0.6v and dm 0.6v step down 200mV when IC is QC3.0 mode*/
	dm_val = 0x2<<1;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DM_VSET_MASK, dm_val); //dm 0.6V
    if (ret)
        return ret;
	msleep(10);

	dm_val = SGM41542_DM_VSET_MASK;
	ret = sgm41542_update_bits(sgm_chg, SGM41542_REG_0C,
				  SGM41542_DM_VSET_MASK, dm_val); //dm 3.3v
	udelay(100);

	return ret;
}

static int sgm4154x_hvdcp_detect_work(struct sgm41542_device *sgm_chg)
{
	int i = 0;
	int vbus_volt=0;
	int vbus_type = 0;

	return 0;

	if (sgm_chg->usb_online == 0) {
		pr_err("sgm4154x_hvdcp_detect_work online = 0 return \n");
		return 0;
	}
	
	pr_err("sgm4154x_hvdcp_detect_work enter \n");

	sgm41542_enable_qc20_hvdcp_9v(sgm_chg);
	msleep(1000);
	sgm41542_get_vbus_volt(sgm_chg, &vbus_volt);
	pr_err("sgm4154x_hvdcp_detect_work vbus_volt = %d  \n", sgm_chg->vbus_type);
	if(vbus_volt>7000){
		sgm_chg->vbus_type = SGM41542_VBUS_USB_HVDCP;
	}
	pr_err("sgm4154x_hvdcp_detect_work vbus_type =%d \n", sgm_chg->vbus_type);
	pr_err("sgm4154x_hvdcp_detect_work end \n");
	return 0;

	sgm41542_enable_qc30_hvdcp(sgm_chg);
	msleep(100);
	for(i=0; i<16; i++){
		sgm41542_qc30_step_up_vbus(sgm_chg);
		msleep(20);
	}
	msleep(1000);
	sgm41542_get_vbus_volt(sgm_chg, &vbus_volt);
	pr_err("sgm4154x_hvdcp_detect_work vbus_volt = %d 1 \n", vbus_volt);
	if(vbus_volt>6000){
		for(i=0; i<16; i++){
			sgm41542_qc30_step_down_vbus(sgm_chg);
			msleep(20);
		}
		vbus_type = SGM41542_VBUS_USB_HVDCP3;
	}else{
		sgm41542_enable_qc20_hvdcp_9v(sgm_chg);
		msleep(1000);
		sgm41542_get_vbus_volt(sgm_chg, &vbus_volt);
		pr_err("sgm4154x_hvdcp_detect_work vbus_volt = %d 2 \n", vbus_volt);
		if(vbus_volt>7000){
			vbus_type = SGM41542_VBUS_USB_HVDCP;
			sgm41542_enable_qc20_hvdcp_5v(sgm_chg);
		}
	}
	msleep(100);

	sgm_chg->vbus_type = vbus_type;
	sgm41542_get_usb_type(sgm_chg);
	pr_err("sgm4154x_hvdcp_detect_work vbus_type =%d \n", vbus_type);
	pr_err("sgm4154x_hvdcp_detect_work end \n");
	return 0;

}
#endif /* CONFIG_SGM41542_ENABLE_HVDCP */

static void sgm41542_dp_dm(struct sgm41542_device *sgm_chg, int val){
	sgm41542_request_dpdm(sgm_chg, true);
	switch (val) {
	case QTI_POWER_SUPPLY_DP_DM_HVDCP3_SUPPORTED:
		/* force 5.6V */
		pr_err("sgm41542_dp_dm QTI_POWER_SUPPLY_DP_DM_HVDCP3_SUPPORTED \n");
		if(sgm_chg->vbus_type == SGM41542_VBUS_USB_HVDCP3){
			pr_err("-------------------- request 5.6V\n");
			for(; sgm_chg->pulse_cnt<3; sgm_chg->pulse_cnt++){
				sgm41542_qc30_step_up_vbus(sgm_chg);
				msleep(20);
			}
		}
		break;
	case QTI_POWER_SUPPLY_DP_DM_FORCE_5V:
		pr_err("sgm41542_dp_dm QTI_POWER_SUPPLY_DP_DM_FORCE_5V \n");
		sgm41542_enable_qc20_hvdcp_5v(sgm_chg);
		break;
	case QTI_POWER_SUPPLY_DP_DM_FORCE_9V:
		pr_err("sgm41542_dp_dm QTI_POWER_SUPPLY_DP_DM_FORCE_9V \n");
		sgm41542_enable_qc20_hvdcp_9v(sgm_chg);
		break;
	case QTI_POWER_SUPPLY_DP_DM_FORCE_12V:
		break;
	case QTI_POWER_SUPPLY_DP_DM_CONFIRMED_HVDCP3P5:
		break;
	default:
		break;
	}
	sgm41542_request_dpdm(sgm_chg, false);
}
//- bug 761884, tankaikun@wt, add 20220708, charger bring up

static int sgm41542_get_chg_status(struct sgm41542_device *sgm_chg)
{
	int ret;
	u8 status = 0;
	u8 charge_status = 0;

	/* Read STATUS registers */
	ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_08);
	if (ret) {
		dev_err(sgm_chg->dev, "%s: read regs:0x0b fail !\n", __func__);
		return sgm_chg->chg_status;
	}

	charge_status = (status & SGM41542_CHRG_STAT_MASK) >> SGM41542_CHRG_STAT_SHIFT;
	switch (charge_status) {
	case SGM41542_NOT_CHARGING:
		sgm_chg->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case SGM41542_PRE_CHARGE:
	case SGM41542_FAST_CHARGING:
		if (sgm_chg->hiz_mode)
			sgm_chg->chg_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else
			sgm_chg->chg_status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case SGM41542_CHARGE_DONE:
		sgm_chg->chg_status = POWER_SUPPLY_STATUS_FULL;
		break;
	default:
		sgm_chg->chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return sgm_chg->chg_status;
}

static int sgm41542_get_vbus_type(struct sgm41542_device *sgm_chg)
{
	int i, ret;
	u8 status = 0;
	u8 bc12_vbus_type = 0;
	u8 input_current = 0;
	int bc12_current;

	//get vbus_type
	for (i = 1; i <= 3; i++) {
		ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_08);
		if (!ret) {
			break;
		}
		dev_err(sgm_chg->dev, "read regs:0x08 fail count: %d !\n", i);
		mdelay(5);
	}
	bc12_vbus_type = (status & SGM41542_VBUS_STAT_MASK) >> SGM41542_VBUS_STAT_SHIFT;

	//get input current limit by bc12 detect done
	for (i = 1; i <= 3; i++) {
		ret = sgm41542_read_byte(sgm_chg, &input_current, SGM41542_REG_00);
		if (!ret) {
			break;
		}
		dev_err(sgm_chg->dev, "read regs:0x00 fail count: %d !\n", i);
		mdelay(5);
	}
	input_current = (input_current & SGM41542_IINLIM_MASK);
	bc12_current = (input_current * SGM41542_IINLIM_LSB) + SGM41542_IINLIM_BASE;

	if (bc12_vbus_type == SGM41542_VBUS_NONSTAND) {
		if (bc12_current <= SGM41542_VBUS_NONSTAND_1000MA) {
			sgm_chg->vbus_type = SGM41542_VBUS_NONSTAND_1A;
		} else if (bc12_current <= SGM41542_VBUS_NONSTAND_1500MA) {
			sgm_chg->vbus_type = SGM41542_VBUS_NONSTAND_1P5A;
		} else {
			sgm_chg->vbus_type = SGM41542_VBUS_NONSTAND_2A;
		}
	} else {
		sgm_chg->vbus_type = bc12_vbus_type;
	}

	//If the vbus_type detect is incorrect, force vbus_type to usb
	if (sgm_chg->bc12_float_check <= BC12_FLOAT_CHECK_MAX
			&& (sgm_chg->vbus_type == SGM41542_VBUS_NONE
				|| sgm_chg->vbus_type == SGM41542_VBUS_UNKNOWN)) {
		sgm_chg->vbus_type = SGM41542_VBUS_USB_SDP;
	} else if (sgm_chg->bc12_float_check > BC12_FLOAT_CHECK_MAX
					&& (sgm_chg->vbus_type == SGM41542_VBUS_USB_SDP
						|| sgm_chg->vbus_type == SGM41542_VBUS_NONE
						|| sgm_chg->vbus_type == SGM41542_VBUS_UNKNOWN)) {
		sgm_chg->vbus_type = SGM41542_VBUS_USB_DCP;
	}

	dev_err(sgm_chg->dev, "bc12_vbus_type: %d, bc12_current: %d, vbus_type: %d, bc12_check: %d\n",
					bc12_vbus_type, bc12_current, sgm_chg->vbus_type, sgm_chg->bc12_float_check);

	return sgm_chg->vbus_type;
}

static int sgm41542_get_usb_type(struct sgm41542_device *sgm_chg)
{
	switch (sgm_chg->vbus_type) {
	case SGM41542_VBUS_NONE:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case SGM41542_VBUS_USB_SDP:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB;
		//sgm41542_notify_device_mode(sgm_chg, 1);
		break;
	case SGM41542_VBUS_USB_CDP:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB_CDP;
		//sgm41542_notify_device_mode(sgm_chg, 1);
		break;
	case SGM41542_VBUS_USB_DCP:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case SGM41542_VBUS_USB_HVDCP:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB_HVDCP;
		break;
	case SGM41542_VBUS_USB_HVDCP3:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB_HVDCP_3;
		break;
	case SGM41542_VBUS_UNKNOWN:
		//sgm41542_notify_device_mode(sgm_chg, 1);
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB_FLOAT;
		break;
#ifdef CONFIG_QGKI_BUILD
	case SGM41542_VBUS_NONSTAND_1A:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB_NONSTAND_1A;
		break;
	case SGM41542_VBUS_NONSTAND_1P5A:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB_NONSTAND_1P5A;
		break;
	case SGM41542_VBUS_NONSTAND_2A:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case SGM41542_VBUS_USB_AFC:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB_AFC;
		break;
#endif
	default:
		sgm_chg->usb_type = POWER_SUPPLY_TYPE_USB_FLOAT;
		break;
	}

	return sgm_chg->usb_type;
}

static int sgm41542_get_vbus_online(struct sgm41542_device *sgm_chg)
{

	return sgm_chg->vbus_good;
}

static void sgm41542_dump_regs(struct sgm41542_device *sgm_chg)
{

	int addr, ret;
	u8 val;

	dev_err(sgm_chg->dev, "sgm41542_dump_regs:\n");
	for (addr = 0; addr <= SGM41542_REGISTER_MAX; addr++) {
		ret = sgm41542_read_byte(sgm_chg, &val, addr);
		if (ret == 0) {
			dev_err(sgm_chg->dev, "Reg[%.2x] = 0x%.2x \n", addr, val);
		}
	}

}

static int sgm41542_detect_device(struct sgm41542_device *sgm_chg)
{
	int ret;
	u8 data;

	ret = sgm41542_read_byte(sgm_chg, &data, SGM41542_REG_0B);
	if (ret == 0) {
		sgm_chg->part_no = (data & SGM41542_PN_MASK) >> SGM41542_PN_SHIFT;
		sgm_chg->revision = (data & SGM41542_DEV_REV_MASK) >> SGM41542_DEV_REV_SHIFT;
	}

	return ret;
}

static int sgm41542_request_dpdm(struct sgm41542_device *sgm_chg, bool enable)
{
	int rc = 0;

	/* fetch the DPDM regulator */
	if (!sgm_chg->dpdm_reg && of_get_property(sgm_chg->dev->of_node,
				"dpdm-supply", NULL)) {
		sgm_chg->dpdm_reg = devm_regulator_get(sgm_chg->dev, "dpdm");
		if (IS_ERR(sgm_chg->dpdm_reg)) {
			rc = PTR_ERR(sgm_chg->dpdm_reg);
			pr_err("Couldn't get dpdm regulator rc=%d\n",rc);
			sgm_chg->dpdm_reg = NULL;
			return rc;
		}
	}

	//mutex_lock(&sgm_chg->dpdm_lock);
	if (enable) {
		if (sgm_chg->dpdm_reg && !sgm_chg->dpdm_enabled) {
			pr_err("enabling DPDM regulator\n");
			rc = regulator_enable(sgm_chg->dpdm_reg);
			if (rc < 0)
				pr_err("Couldn't enable dpdm regulator rc=%d\n",rc);
			else
				sgm_chg->dpdm_enabled = true;
		}
	} else {
		if (sgm_chg->dpdm_reg && sgm_chg->dpdm_enabled) {
			pr_err("disabling DPDM regulator\n");
			rc = regulator_disable(sgm_chg->dpdm_reg);
			if (rc < 0)
				pr_err("Couldn't disable dpdm regulator rc=%d\n",rc);
			else
				sgm_chg->dpdm_enabled = false;
		}
	}
	//mutex_unlock(&sgm_chg->dpdm_lock);

	return rc;
}

//+ bug 761884, tankaikun@wt, add 20220701, charger bring up
#ifdef CONFIG_MAIN_CHG_EXTCON
static void sgm41542_notify_extcon_props(struct sgm41542_device *sgm_chg, int id)
{
	union extcon_property_value val;

	val.intval = false;
	extcon_set_property(sgm_chg->extcon, id,
			EXTCON_PROP_USB_SS, val);
}

static void sgm41542_notify_device_mode(struct sgm41542_device *sgm_chg, bool enable)
{
	if (enable)
		sgm41542_notify_extcon_props(sgm_chg, EXTCON_USB);

	extcon_set_state_sync(sgm_chg->extcon, EXTCON_USB, enable);
}
#endif /* CONFIG_MAIN_CHG_EXTCON */
//- bug 761884, tankaikun@wt, add 20220701, charger bring up

static int sgm41542_init_device(struct sgm41542_device *sgm_chg)
{
	int ret;

	sgm41542_reset_chip(sgm_chg);
	msleep(50);

	/*common initialization*/
	sgm41542_disable_watchdog_timer(sgm_chg, 1);
	sgm41542_ship_mode_delay_enable(sgm_chg, false);
	sgm41542_set_otg(sgm_chg, false);
	sgm41542_set_input_suspend(sgm_chg, false);
	sgm41542_set_vac_ovp(sgm_chg, SGM41542_OVP_10000mV);
	sgm41542_set_bootstv(sgm_chg, SGM41542_BOOSTV_5300mV);
	sgm41542_set_otg_current(sgm_chg, SGM41542_BOOST_CURR_1200MA);
	//sgm41542_jeita_enable(sgm_chg, false);
	sgm41542_set_vndpm(sgm_chg, 4700);
	sgm41542_set_indpm_mask(sgm_chg, 0);
	sgm41542_set_vndpm_mask(sgm_chg, 0);
	sgm41542_enable_battery_rst_en(sgm_chg, false);
	ret = sgm41542_set_term_current(sgm_chg, sgm_chg->cfg.term_current);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "Failed to set termination current:%d\n", ret);
		return ret;
	}

	ret = sgm41542_set_charge_voltage(sgm_chg, sgm_chg->cfg.charge_voltage);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "Failed to set charge voltage:%d\n",  ret);
		return ret;
	}

	ret = sgm41542_set_charge_current(sgm_chg, sgm_chg->cfg.charge_current);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "Failed to set charge current:%d\n", ret);
		return ret;
	}

	ret = sgm41542_set_prechg_current(sgm_chg, sgm_chg->cfg.prechg_current);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "Failed to set charge current:%d\n", ret);
		return ret;
	}

	sgm41542_set_input_current_limit(sgm_chg, 500);
	sgm41542_enable_term(sgm_chg, true);
	sgm41542_enable_safety_timer(sgm_chg, false);
	sgm41542_set_chg_enable(sgm_chg, true);
	sgm41542_dump_regs(sgm_chg);

	return ret;
}

static ssize_t sgm41542_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret ;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "Charger 1");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = sgm41542_read_byte(g_sgm_chg, &val, addr);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,"Reg[0x%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static DEVICE_ATTR(registers, S_IRUGO, sgm41542_show_registers, NULL);

static struct attribute *sgm41542_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group sgm41542_attr_group = {
	.attrs = sgm41542_attributes,
};

//+bug 816469,tankaikun.wt,add,2023/1/30, IR compensation
static void sgm41542_dynamic_adjust_charge_voltage(struct sgm41542_device *sgm_chg, int vbat)
{
	int ret;
	int cv_adjust = 0;
	u8 status = 0;
	int tune_cv=0;
	int min_tune_cv=3;

	if(time_is_before_jiffies(sgm_chg->dynamic_adjust_charge_update_timer + 5*HZ)) {
		sgm_chg->dynamic_adjust_charge_update_timer = jiffies;
		if (sgm_chg->final_cv > vbat && (sgm_chg->final_cv - vbat) < 9000)
			return;

		ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_0A);
		if ((ret==0) && ((status & SGM41542_VINDPM_STAT_MASK) ||
				(status & SGM41542_IINDPM_STAT_MASK)) &&
				(sgm_chg->final_cv > vbat))
			return;

		tune_cv = sgm_chg->tune_cv;

		if (sgm_chg->final_cv > vbat) {
			tune_cv++;
		} else if (sgm_chg->tune_cv > 0) {
			tune_cv--;
		}

		if (sgm_chg->final_cc < 2000) {
			min_tune_cv = 0;
		}
		sgm_chg->tune_cv = min(tune_cv, min_tune_cv); // 3*8=24mv

		dev_err(sgm_chg->dev, "sgm41542_dynamic_adjust_charge_voltage tune_cv = %d \n", sgm_chg->tune_cv);
		cv_adjust = sgm_chg->tune_cv * 8000;
		cv_adjust += sgm_chg->final_cv;
		cv_adjust /= 1000;

		dev_err(sgm_chg->dev, "sgm41542_dynamic_adjust_charge_voltage cv_adjust=%d min_tune_cv=%d \n",cv_adjust, min_tune_cv);
		ret |= sgm41542_set_chargevoltage(sgm_chg, cv_adjust);
		if (!ret)
			return;

		sgm_chg->tune_cv = 0;
		sgm41542_set_chargevoltage(sgm_chg, sgm_chg->final_cv/1000);
	}

	return;
}
//-bug 816469,tankaikun.wt,add,2023/1/30, IR compensation

static void sgm41542_monitor_workfunc(struct work_struct *work)
{
	int ret;
	u8 status = 0;
	u8 fault = 0;
	u8 charge_status = 0;
	int vbus_volt=0;
	int vbus_type=0;
	struct sgm41542_device *sgm_chg = container_of(work,
								struct sgm41542_device, monitor_work.work);
	int vbat_uV, ibat_ma;

	/* Read STATUS and FAULT registers */
	ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_08);
	if (ret) {
		dev_err(sgm_chg->dev, "read regs:0x08 fail !\n");
		schedule_delayed_work(&sgm_chg->monitor_work, msecs_to_jiffies(10000));
		return;
	}

	ret = sgm41542_read_byte(sgm_chg, &fault, SGM41542_REG_09);
	if (ret) {
		dev_err(sgm_chg->dev, "read regs:0x09 fail !\n");
		schedule_delayed_work(&sgm_chg->monitor_work, msecs_to_jiffies(10000));
		return;
	}
	sgm41542_get_vbus_volt(sgm_chg, &vbus_volt);

	vbus_type = (status & SGM41542_VBUS_STAT_MASK) >> SGM41542_VBUS_STAT_SHIFT;
	sgm_chg->usb_online = (status & SGM41542_PG_STAT_MASK) >> SGM41542_PG_STAT_SHIFT;
	charge_status = (status & SGM41542_CHRG_STAT_MASK) >> SGM41542_CHRG_STAT_SHIFT;
	switch (charge_status) {
	case SGM41542_NOT_CHARGING:
		sgm_chg->chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case SGM41542_PRE_CHARGE:
	case SGM41542_FAST_CHARGING:
		sgm_chg->chg_status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case SGM41542_CHARGE_DONE:
		sgm_chg->chg_status = POWER_SUPPLY_STATUS_FULL;
		break;
	default:
		sgm_chg->chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	//+bug816469,tankaikun.wt,add,2023/1/30, IR compensation
	if (sgm_chg->cfg.enable_ir_compensation && POWER_SUPPLY_TYPE_USB_DCP == sgm_chg->usb_type) {
		sgm41542_get_ibat_curr(sgm_chg, &ibat_ma);
		sgm41542_get_vbat_volt(sgm_chg, &vbat_uV);
		if ((ibat_ma > 10) || (vbat_uV > sgm_chg->final_cv)) {
			sgm41542_dynamic_adjust_charge_voltage(sgm_chg, vbat_uV);
		}
		dev_err(sgm_chg->dev, "ibat_ma=%d final_cc=%d vbat_uV=%d final_cv=%d\n",
					ibat_ma, sgm_chg->final_cc, vbat_uV, sgm_chg->final_cv);
	}
	//-bug816469,tankaikun.wt,add,2023/1/30, IR compensation

	//sgm41542_dump_regs(sgm_chg);
	dev_err(sgm_chg->dev, "%s:usb_online: %d, chg_status: %d, chg_fault: 0x%x vbus_type:%d vbus_volt=%d\n",
							__func__, sgm_chg->usb_online, sgm_chg->chg_status, fault, vbus_type, vbus_volt);

	schedule_delayed_work(&sgm_chg->monitor_work, msecs_to_jiffies(10000));
}
//+ bug 761884, tankaikun@wt, add 20220701, charger bring up

extern void charger_enable_device_mode(bool enable);

static void sgm41542_rerun_apsd_workfunc(struct work_struct *work)
{
	struct sgm41542_device *sgm_chg = container_of(work,
			struct sgm41542_device, rerun_apsd_work.work);

	if(!sgm41542_wake_active(&sgm_chg->sgm41542_apsd_wake_source)){
		sgm41542_stay_awake(&sgm_chg->sgm41542_apsd_wake_source);
	}

	if(!sgm_chg->apsd_rerun){
		pr_err("wt rerun apsd begin\n");
		sgm_chg->apsd_rerun = true;
		sgm41542_request_dpdm(sgm_chg, true);
		msleep(20);
		if (sgm_chg->usb_online || (!sgm_chg->init_bc12_detect))
			sgm_chg->init_bc12_detect = true;
			sgm41542_enable_bc12_detect(sgm_chg);
	}

#if 0
	if (!sgm_chg->init_bc12_detect) {
		sgm_chg->init_bc12_detect = true;
		schedule_delayed_work(&sgm_chg->irq_work, msecs_to_jiffies(5));
	}
#endif

	if(sgm41542_wake_active(&sgm_chg->sgm41542_apsd_wake_source)){
		sgm41542_relax(&sgm_chg->sgm41542_apsd_wake_source);
	}
}
//- bug 761884, tankaikun@wt, add 20220701, charger bring up

struct iio_channel ** sgm41542_get_ext_channels(struct device *dev,
		 const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);

		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
	}

	return iio_ch_ext;
}

static bool  sgm41542_is_wtchg_chan_valid(struct sgm41542_device *chg,
		enum wtcharge_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->wtchg_ext_iio_chans) {
		iio_list = sgm41542_get_ext_channels(chg->dev, wtchg_iio_chan_name,
		ARRAY_SIZE(wtchg_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->wtchg_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->wtchg_ext_iio_chans = iio_list;
	}

	return true;
}

static bool sgm41542_is_afc_chan_valid(struct sgm41542_device *chg,
		enum afc_chg_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->afc_ext_iio_chans) {
		iio_list = sgm41542_get_ext_channels(chg->dev, afc_chg_iio_chan_name,
		ARRAY_SIZE(afc_chg_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->afc_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->afc_ext_iio_chans = iio_list;
	}

	return true;
}

static bool sgm41542_is_batt_qg_chan_valid(struct sgm41542_device *chg,
		enum batt_qg_exit_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!chg->qg_batt_ext_iio_chans) {
		iio_list = sgm41542_get_ext_channels(chg->dev, qg_ext_iio_chan_name,
		ARRAY_SIZE(qg_ext_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(chg->dev, "Failed to get channels, %d\n",
					rc);
				chg->qg_batt_ext_iio_chans = NULL;
			}
			return false;
		}
		chg->qg_batt_ext_iio_chans = iio_list;
	}

	return true;
}


static int sgm41542_write_iio_prop(struct sgm41542_device *sgm_chg,
				enum sgm41542_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int ret;

	switch (type) {
	case WT_CHG:
		if (!sgm41542_is_wtchg_chan_valid(sgm_chg, channel)) {
			return -ENODEV;
		}
		iio_chan_list = sgm_chg->wtchg_ext_iio_chans[channel];
		break;
	case AFC:
		dev_err(sgm_chg->dev,"afcsgm41542_is_afc_chan_valid\n");
		if (!sgm41542_is_afc_chan_valid(sgm_chg, channel)) {
			return -ENODEV;
		}
		iio_chan_list = sgm_chg->afc_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	ret = iio_write_channel_raw(iio_chan_list, val);

	return ret < 0 ? ret : 0;
}

static int sgm41542_read_iio_prop(struct sgm41542_device *sgm_chg,
				enum sgm41542_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;
	switch (type) {
	case AFC:
		dev_err(sgm_chg->dev,"afc sgm41542_is_afc_chan_valid\n");
		if (!sgm41542_is_afc_chan_valid(sgm_chg, channel)) {
			return -ENODEV;
		}
		iio_chan_list = sgm_chg->afc_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}

static int sgm41542_get_vbus_volt(struct sgm41542_device *sgm_chg,  int *val)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!sgm41542_is_wtchg_chan_valid(sgm_chg, WTCHG_VBUS_VAL_NOW)) {
		dev_err(sgm_chg->dev,"read vbus_pwr channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = sgm_chg->wtchg_ext_iio_chans[WTCHG_VBUS_VAL_NOW];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(sgm_chg->dev,
		"read vbus_pwr channel fail, ret=%d\n", ret);
		return ret;
	}
	*val = temp;

	dev_err(sgm_chg->dev,
		"read vbus_pwr =%d\n", temp);

	return ret;
}

static int sgm41542_get_vbat_volt(struct sgm41542_device *sgm_chg,  int *val)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!sgm41542_is_batt_qg_chan_valid(sgm_chg, BATT_QG_VOLTAGE_NOW)) {
		dev_err(sgm_chg->dev,"read BATT_QG_VOLTAGE_NOW channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = sgm_chg->qg_batt_ext_iio_chans[BATT_QG_VOLTAGE_NOW];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(sgm_chg->dev,
		"read BATT_QG_VOLTAGE_NOW channel fail, ret=%d\n", ret);
		return ret;
	}
	*val = temp;

	return ret;
}

static void sgm41542_set_afc_9v(struct sgm41542_device *sgm_chg)
{
	int ret;

	dev_err(sgm_chg->dev, "%s: sgm afc enter:\n", __func__);
	ret = sgm41542_write_iio_prop(sgm_chg, AFC, AFC_DETECT, MAIN_CHG);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static void sgm41542_set_afc_vol(struct sgm41542_device *sgm_chg, int val)
{
	int ret;

	dev_err(sgm_chg->dev, "%s: sgm afc enter:\n", __func__);
	ret = sgm41542_write_iio_prop(sgm_chg, AFC, AFC_SET_VOL, val);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static void sgm41542_remove_afc(struct sgm41542_device *sgm_chg)
{
	int ret;

	dev_err(sgm_chg->dev, "%s: sgm afc remove:\n", __func__);
	ret = sgm41542_write_iio_prop(sgm_chg, AFC, AFC_DETECH, MAIN_CHG);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}

static int sgm41542_get_ibat_curr(struct sgm41542_device *sgm_chg,  int *val)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!sgm41542_is_batt_qg_chan_valid(sgm_chg, BATT_QG_CURRENT_NOW)) {
		dev_err(sgm_chg->dev,"read BATT_QG_VOLTAGE_NOW channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = sgm_chg->qg_batt_ext_iio_chans[BATT_QG_CURRENT_NOW];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(sgm_chg->dev,
		"read BATT_QG_VOLTAGE_NOW channel fail, ret=%d\n", ret);
		return ret;
	}
	*val = temp;

	return ret;
}

//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
static int sgm41542_get_hv_dis_status(struct sgm41542_device *sgm_chg,  int *val)
{
	int ret, temp;
	struct iio_channel *iio_chan_list;

	if (!sgm41542_is_wtchg_chan_valid(sgm_chg, HV_DISABLE_DETECT)) {
		dev_err(sgm_chg->dev,"read HV_DISABLE_DETECT channel fail\n");
		return -ENODEV;
	}

	iio_chan_list = sgm_chg->wtchg_ext_iio_chans[HV_DISABLE_DETECT];
	ret = iio_read_channel_processed(iio_chan_list, &temp);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "read HV_DISABLE_DETECT channel fail, ret=%d\n", ret);
		return ret;
	}
	*val = temp;

	dev_err(sgm_chg->dev, "read HV_DISABLE_DETECT return val = %d\n", temp);

	return ret;
}
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE

static void sgm41542_update_wtchg_work(struct sgm41542_device *sgm_chg)
{
	int ret;

	ret = sgm41542_write_iio_prop(sgm_chg, WT_CHG, WTCHG_UPDATE_WORK, MAIN_CHG);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "%s: fail: %d\n", __func__, ret);
	}
}
#if 0
static void sgm41542_charger_irq_workfunc(struct work_struct *work)
{
	int ret;
	u8 status;
	u8 chg_fault;
	u8 vindpm_status;
	u8 bc12_status;
	u8 hiz_mode=0;
	struct sgm41542_device *sgm_chg = container_of(work,
								struct sgm41542_device, irq_work.work);

	if(!sgm41542_wake_active(&sgm_chg->sgm41542_wake_source)){
		sgm41542_stay_awake(&sgm_chg->sgm41542_wake_source);
	}

	dev_err(sgm_chg->dev, "sgm41542_charger_irq_workfunc enter\n");

	/* Read Power & vbus type  registers */
	ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_08);
	if (ret) {
		dev_err(sgm_chg->dev, "read regs:0x08 fail !\n");
	}
	sgm_chg->usb_online = (status & SGM41542_PG_STAT_MASK) >> SGM41542_PG_STAT_SHIFT;

	/* Read BC1.2 done registers */
	ret = sgm41542_read_byte(sgm_chg, &bc12_status, SGM41542_REG_0E);
	if (ret) {
		dev_err(sgm_chg->dev, "read regs:0x0E fail !\n");
	}
	sgm_chg->bc12_done = ((bc12_status & SGM41542_INPUT_DET_DONE_MASK)
							>> SGM41542_INPUT_DET_DONE_SHIFT);
	if ((sgm_chg->usb_online == false)
			|| (sgm_chg->usb_online && sgm_chg->bc12_done)) {
		if(sgm_chg->apsd_rerun){
			sgm_chg->vbus_type = ((status & SGM41542_VBUS_STAT_MASK)
									>> SGM41542_VBUS_STAT_SHIFT);
		}else
			sgm_chg->vbus_type = 0;
	}
	sgm41542_get_usb_type(sgm_chg);

	if (sgm_chg->hiz_mode && sgm_chg->usb_online) {
		sgm41542_get_hiz_mode(sgm_chg, &hiz_mode);
		sgm_chg->hiz_mode = hiz_mode;
	}

	if(sgm_chg->bc12_done && sgm_chg->usb_type == POWER_SUPPLY_TYPE_USB_DCP && sgm_chg->apsd_rerun
			&& !sgm_chg->hvdcp_det && !sgm_chg->cfg.disable_hvdcp) {
		sgm41542_update_wtchg_work(sgm_chg);
		sgm_chg->hvdcp_det = 1;
		sgm_chg->pulse_cnt = 0;
		sgm4154x_hvdcp_detect_work(sgm_chg);
	}else if(SGM41542_VBUS_USB_SDP == sgm_chg->vbus_type || SGM41542_VBUS_USB_CDP == sgm_chg->vbus_type){
		sgm41542_request_dpdm(sgm_chg, false);
	}

	ret = sgm41542_read_byte(sgm_chg, &chg_fault, SGM41542_REG_09);
	if (ret) {
		dev_err(sgm_chg->dev, "read regs:0x09 fail !\n");
	}

	ret = sgm41542_read_byte(sgm_chg, &vindpm_status, SGM41542_REG_0A);
	if (ret) {
		dev_err(sgm_chg->dev, "read regs:0x0A fail !\n");
	}
	dev_dbg(sgm_chg->dev, "%s:usb_online: %d, pre_usb_online: %d,"
							"vbus_type: %d, pre_vbus_type: %d\n",
							__func__, sgm_chg->usb_online, sgm_chg->pre_usb_online,
							sgm_chg->vbus_type, sgm_chg->pre_vbus_type);

	if ((sgm_chg->usb_online != sgm_chg->pre_usb_online)
			|| (sgm_chg->vbus_type != sgm_chg->pre_vbus_type)) {
		sgm41542_update_wtchg_work(sgm_chg);

		if(!sgm_chg->usb_online){
			sgm_chg->hvdcp_det = 0;
			sgm_chg->apsd_rerun = false;
			sgm_chg->pulse_cnt = 0;
			sgm41542_request_dpdm(sgm_chg, false);
		}
	}

	sgm_chg->pre_usb_online = sgm_chg->usb_online;
	sgm_chg->pre_vbus_type = sgm_chg->vbus_type;

	if(sgm41542_wake_active(&sgm_chg->sgm41542_wake_source)){
		sgm41542_relax(&sgm_chg->sgm41542_wake_source);
	}

	dev_dbg(sgm_chg->dev, "%s:chg_fault: 0x%x, vindpm_status: 0x%x bc12_done: %d apsd_rerun: %d\n",
							__func__, chg_fault, vindpm_status, sgm_chg->bc12_done, sgm_chg->apsd_rerun);
}

static irqreturn_t sgm41542_charger_interrupt(int irq, void *data)
{
	struct sgm41542_device *sgm_chg = data;

	if(!sgm41542_wake_active(&sgm_chg->sgm41542_wake_source)){
		sgm41542_stay_awake(&sgm_chg->sgm41542_wake_source);
	}
	schedule_delayed_work(&sgm_chg->irq_work, msecs_to_jiffies(10));
	return IRQ_HANDLED;
}
#endif

static int sgm41542_get_afc_type(struct sgm41542_device *sgm_chg)
{
	int ret;
	int val;

	ret = sgm41542_read_iio_prop(sgm_chg, AFC, AFC_TYPE, &val);
	if (ret < 0)
	{
		dev_err(sgm_chg->dev, "%s: failed: %d", __func__, ret);
	} else {
		sgm_chg->afc_type = val;
	}
	return sgm_chg->afc_type;
}

#define HVDCP_QC20_VOLT_MIN 7000
#define HVDCP_AFC_VOLT_MIN 7000

static void sgm41542_charger_detect_workfunc(struct work_struct *work)
{
	int ret;
	int vbus;
//+ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
#ifdef CONFIG_QGKI_BUILD
	int val_hv;
#endif
//-ReqP86801AA1-3595, liyiying.wt, add, 20230801, Configure SEC_BAT_CURRENT_EVENT_HV_DISABLE
	int i;

	struct sgm41542_device *sgm_chg = container_of(work,
								struct sgm41542_device, detect_work.work);

	if (sgm_chg->vbus_type != SGM41542_VBUS_USB_AFC &&
			sgm_chg->vbus_type != SGM41542_VBUS_USB_HVDCP &&
			sgm_chg->vbus_type != SGM41542_VBUS_USB_HVDCP3) {
		sgm41542_get_vbus_volt(sgm_chg, &vbus);
		if (vbus > HVDCP_AFC_VOLT_MIN) {
			//sgm41542_request_dpdm(sgm_chg, false);
#ifdef CONFIG_QGKI_BUILD
			sgm41542_get_hv_dis_status(sgm_chg, &val_hv);
#endif
			return;
		}
	}
	//start afc detect
	sgm41542_set_enhvdcp(sgm_chg, true);
	sgm41542_set_input_current_limit(sgm_chg, 500);
	sgm41542_set_dp0p6_for_afc(sgm_chg);
	sgm41542_set_afc_9v(sgm_chg);

	// P231130-06621 liwei19.wt 20231218,reduce the number of AFC and QC identification
	for (i = 0; i <= AFC_DETECT_TIME; i++) {
		msleep(100);
		sgm41542_get_vbus_volt(sgm_chg, &vbus);
		dev_err(sgm_chg->dev, "afc_detect: time: %d, vbus_volt: %d\n", i, vbus);

		if (vbus > HVDCP_AFC_VOLT_MIN && sgm41542_get_afc_type(sgm_chg) == AFC_9V) {
			sgm_chg->vbus_type = SGM41542_VBUS_USB_AFC;
			dev_err(sgm_chg->dev, "afc_detect: succeed !\n");
			goto qc20_detect_exit;
		}

		if (sgm_chg->vbus_good == false) {
			sgm41542_remove_afc(sgm_chg);
			goto hvdcp_detect_exit;
		}
	}

	sgm41542_remove_afc(sgm_chg);
	dev_err(sgm_chg->dev, "afc_detect: fail !\n");
	//afc detect end

	//start QC20 detect
	ret = sgm4154x_hvdcp_detect_work(sgm_chg);
	sgm41542_enable_qc20_hvdcp_9v(sgm_chg);
	// P231130-06621 liwei19.wt 20231218,reduce the number of AFC and QC identification
	for (i = 0; i <= QC_DETECT_TIME; i++) {
		msleep(100);
		sgm41542_get_vbus_volt(sgm_chg, &vbus);
		dev_err(sgm_chg->dev, "qc20_detect: time: %d, vbus_volt: %d\n", i, vbus);

		if (vbus > HVDCP_QC20_VOLT_MIN) {
			sgm_chg->vbus_type = SGM41542_VBUS_USB_HVDCP;
			dev_err(sgm_chg->dev, "qc20_detect: succeed !\n");
			goto qc20_detect_exit;
		}

		if (sgm_chg->vbus_good == false) {
			goto hvdcp_detect_exit;
		}
	}
	dev_err(sgm_chg->dev, "qc20_detect: fail !\n");
	//QC20 detect end

//afc_detect_exit:
//	sgm41542_request_dpdm(sgm_chg, false);
qc20_detect_exit:
	sgm41542_get_usb_type(sgm_chg);
	sgm41542_update_wtchg_work(sgm_chg);
#ifdef CONFIG_QGKI_BUILD
	sgm41542_get_hv_dis_status(sgm_chg, &val_hv);
#endif
hvdcp_detect_exit:
	return;
}

static void sgm41542_charger_check_adapter_workfunc(struct work_struct *work)
{
	int ret;
	u8 status;

	struct sgm41542_device *sgm_chg = container_of(work,
								struct sgm41542_device, check_adapter_work.work);
	printk("Disable check_adapter function\n");
	return;
	dev_err(sgm_chg->dev, "slow adapter check \n");
//sdp Recognition error err,gudi.wt,20230717
	if(sgm_chg->vbus_type == SGM41542_VBUS_USB_SDP)
		return;

	ret = sgm41542_set_input_current_limit(sgm_chg, 500);
	msleep(90);
	ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_0A);
	sgm_chg->vindpm_stat = (status & SGM41542_VINDPM_STAT_MASK) >> 6;
	if (sgm_chg->vindpm_stat) {
		ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_00);
		dev_err(sgm_chg->dev, "slow adapter ! input current = 500 reg00 = 0x%02x\n", status);
		return;
	}

	ret = sgm41542_set_input_current_limit(sgm_chg, 700);
	msleep(90);
	ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_0A);
	sgm_chg->vindpm_stat = (status & SGM41542_VINDPM_STAT_MASK) >> 6;
	if (sgm_chg->vindpm_stat) {
		ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_00);
		dev_err(sgm_chg->dev, "slow adapter ! input current = 700 reg00 = 0x%02x\n", status);
		return;
	}

	ret = sgm41542_set_input_current_limit(sgm_chg, 1000);
	msleep(90);
	ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_0A);
	sgm_chg->vindpm_stat = (status & SGM41542_VINDPM_STAT_MASK) >> 6;
	if (sgm_chg->vindpm_stat) {
		ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_00);
		dev_err(sgm_chg->dev, "slow adapter ! input current = 1000 reg00 = 0x%02x\n", status);
		return;
	}

	ret = sgm41542_set_input_current_limit(sgm_chg, 1200);
	msleep(90);
	ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_0A);
	sgm_chg->vindpm_stat = (status & SGM41542_VINDPM_STAT_MASK) >> 6;
	if (sgm_chg->vindpm_stat) {
		ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_00);
		dev_err(sgm_chg->dev, "slow adapter ! input current = 1200 reg00 = 0x%02x\n", status);
		return;
	}

	dev_err(sgm_chg->dev, "bad adapter check end \n");

}

static void sgm41542_charger_irq_workfunc(struct work_struct *work)
{
	int ret;
	u8 status;
	u8 value;
	bool prev_pg;
	bool prev_vbus_gd;
	int i;

	struct sgm41542_device *sgm_chg = container_of(work,
								struct sgm41542_device, irq_work.work);

	if (!sgm41542_wake_active(&sgm_chg->sgm41542_wake_source)) {
		sgm41542_stay_awake(&sgm_chg->sgm41542_wake_source);
	}

	dev_err(sgm_chg->dev, "upm6918 adapter/usb irq handler\n");

	/* +Read Power begin+ */
	for (i = 1; i <= 3; i++) {
		ret = sgm41542_read_byte(sgm_chg, &value, SGM41542_REG_0A);
		if (!ret) {
			break;
		}
		dev_err(sgm_chg->dev, "read regs:0x0A fail count: %d !\n", i);
		mdelay(20);
	}
	prev_vbus_gd = sgm_chg->vbus_good;
	sgm_chg->vbus_good = (value & REG0A_VBUS_GD_MASK) >> REG0A_VBUS_GD_SHIFT;
	/* -Read Power end- */

	/* +Read  CHRG_STAT begin+ */
	for (i = 1; i <= 3; i++) {
		ret = sgm41542_read_byte(sgm_chg, &status, SGM41542_REG_08);
		if (!ret) {
			break;
		}
		dev_err(sgm_chg	->dev, "read regs:0x0A fail count: %d !\n", i);
		mdelay(5);
	}
	prev_pg = sgm_chg->usb_online;
	sgm_chg->usb_online = (status & SGM41542_PG_STAT_MASK) >> SGM41542_PG_STAT_SHIFT;
	/* -Read  CHRG_STAT end- */

	dev_err(sgm_chg->dev, "prev_vbus_gd: %d, vbus_gd: %d, prev_pg: %d, usb_online: %d, apsd_rerun %d\n",
				prev_vbus_gd, sgm_chg->vbus_good, prev_pg,
				sgm_chg->usb_online, sgm_chg->apsd_rerun);

	if (!prev_vbus_gd && sgm_chg->vbus_good) {
		dev_err(sgm_chg->dev, "upm6918 adapter/usb inserted\n");
		if (sgm_chg->usb_online) {
			msleep(200);
		}
		sgm41542_set_chg_enable(sgm_chg, true);
	} else if (prev_vbus_gd && !sgm_chg->vbus_good) {
		sgm_chg->vbus_type = SGM41542_VBUS_NONE;
		sgm_chg->apsd_rerun = false;
		sgm_chg->bc12_float_check = 0;
		sgm41542_remove_afc(sgm_chg);
		sgm41542_request_dpdm(sgm_chg, false);

		cancel_delayed_work(&sgm_chg->check_adapter_work);
		cancel_delayed_work(&sgm_chg->detect_work);
		cancel_delayed_work(&sgm_chg->monitor_work);
		dev_err(sgm_chg->dev, "upm6918 adapter/usb removed\n");
		sgm41542_update_wtchg_work(sgm_chg);
		goto irq_exit;
	}
//P231102-08573 gudi.wt 20231128,remove rerun for upm6918
	if (sgm_chg->vbus_good && sgm_chg->usb_online) {
		sgm41542_get_vbus_type(sgm_chg);

		if (sgm_chg->vbus_type == SGM41542_VBUS_USB_DCP
				|| sgm_chg->vbus_type == SGM41542_VBUS_NONSTAND_1A
				|| sgm_chg->vbus_type == SGM41542_VBUS_NONSTAND_1P5A) {
			schedule_delayed_work(&sgm_chg->detect_work, msecs_to_jiffies(0));
			schedule_delayed_work(&sgm_chg->check_adapter_work, msecs_to_jiffies(15000));
		} else if ((sgm_chg->vbus_type == SGM41542_VBUS_USB_SDP
					|| sgm_chg->vbus_type == SGM41542_VBUS_USB_CDP)
					&& sgm_chg->apsd_rerun != 10) {
			sgm41542_request_dpdm(sgm_chg, false);
			msleep(10);
			if (sgm_chg->bc12_float_check >= BC12_FLOAT_CHECK_MAX) {
				msleep(200);
				charger_enable_device_mode(true);
			}
		}
		sgm41542_update_wtchg_work(sgm_chg);
		schedule_delayed_work(&sgm_chg->monitor_work, msecs_to_jiffies(500));
	}

irq_exit:
	if (sgm41542_wake_active(&sgm_chg->sgm41542_wake_source)) {
		sgm41542_relax(&sgm_chg->sgm41542_wake_source);
	}

	return;

}

static irqreturn_t sgm41542_irq_handler(int irq, void *data)
{
	struct sgm41542_device *sgm_chg = data;

	if(!sgm41542_wake_active(&sgm_chg->sgm41542_wake_source)){
		sgm41542_stay_awake(&sgm_chg->sgm41542_wake_source);
	}
	//schedule_delayed_work(&sgm_chg->irq_work, msecs_to_jiffies(10));
	schedule_delayed_work(&sgm_chg->irq_work, 0);
	return IRQ_HANDLED;
}

static bool sgm41542_ext_iio_init(struct sgm41542_device *chip)
{
	int rc = 0;
	struct iio_channel **iio_list;

	if (IS_ERR(chip->wtchg_ext_iio_chans))
		return false;

	if (!chip->wtchg_ext_iio_chans) {
		iio_list = sgm41542_get_ext_channels(chip->dev,
			wtchg_iio_chan_name, ARRAY_SIZE(wtchg_iio_chan_name));
	 if (IS_ERR(iio_list)) {
		 rc = PTR_ERR(iio_list);
		 if (rc != -EPROBE_DEFER) {
			 dev_err(chip->dev, "Failed to get wtchg channels, rc=%d\n",
					 rc);
			 chip->wtchg_ext_iio_chans = NULL;
		 }
		 return false;
	 }
	 chip->wtchg_ext_iio_chans = iio_list;
	}

	if (!chip->afc_ext_iio_chans) {
		iio_list = sgm41542_get_ext_channels(chip->dev,
			afc_chg_iio_chan_name, ARRAY_SIZE(afc_chg_iio_chan_name));
	 if (IS_ERR(iio_list)) {
		 rc = PTR_ERR(iio_list);
		 if (rc != -EPROBE_DEFER) {
			 dev_err(chip->dev, "Failed to get afc channels, rc=%d\n",
					 rc);
			 chip->afc_ext_iio_chans = NULL;
		 }
		 return false;
	 }
	 chip->afc_ext_iio_chans = iio_list;
	}

	return true;
}


static int sgm41542_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct sgm41542_device *sgm_chg = iio_priv(indio_dev);
	int ret = 0;
	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_CHARGER_STATUS:
		*val1 = sgm41542_get_chg_status(sgm_chg);
		break;
	case PSY_IIO_CHARGE_TYPE:
		*val1 = sgm41542_get_usb_type(sgm_chg);
		break;
	case PSY_IIO_MAIN_CHARGE_VBUS_ONLINE:
		*val1 = sgm41542_get_vbus_online(sgm_chg);
		break;
	case PSY_IIO_MAIN_CHAGER_HZ:
		*val1 = sgm_chg->hiz_mode;
		break;
	case PSY_IIO_MAIN_INPUT_VOLTAGE_SETTLED:
		sgm41542_get_vbus_volt(sgm_chg, val1);
		if(ret<0)
			*val1 = 0;
		break;
	case PSY_IIO_CHARGE_PG_STAT:
		*val1 = sgm_chg->usb_online;
		if(*val1 < 0)
			*val1 = 0;
		break;
	default:
		dev_err(sgm_chg->dev, "Unsupported SGM4154X IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		dev_err(sgm_chg->dev, "Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, ret);
		return ret;
	}

	return IIO_VAL_INT;
}

static int sgm41542_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct sgm41542_device *sgm_chg = iio_priv(indio_dev);
	int ret = 0;

	switch (chan->channel) {
	case PSY_IIO_MAIN_CHAGER_HZ:
		sgm41542_set_input_suspend(sgm_chg, val1);
		break;
	case PSY_IIO_MAIN_INPUT_CURRENT_SETTLED:
		sgm41542_set_input_current_limit(sgm_chg, val1);
		break;
	case PSY_IIO_MAIN_INPUT_VOLTAGE_SETTLED:

		break;
	case PSY_IIO_MAIN_CHAGER_CURRENT:
		sgm41542_set_charge_current(sgm_chg, val1);
		break;
	case PSY_IIO_CHARGING_ENABLED:
		sgm41542_set_chg_enable(sgm_chg, val1);
		break;
	case PSY_IIO_OTG_ENABLE:
		sgm41542_set_otg(sgm_chg, val1);
		break;
	case PSY_IIO_MAIN_CHAGER_TERM:
		sgm41542_set_term_current(sgm_chg, val1);
		break;
	case PSY_IIO_BATTERY_VOLTAGE_TERM:
		sgm41542_set_charge_voltage(sgm_chg, val1);
		break;
	case PSY_IIO_APSD_RERUN:
		if (val1 == 0 || val1 == 1 || val1 == 10)
			sgm_chg->apsd_rerun = val1;
		else {
			if (val1 == 2) {
				sgm_chg->bc12_float_check++;
			}
			sgm_chg->apsd_rerun = false;
			schedule_delayed_work(&sgm_chg->rerun_apsd_work, msecs_to_jiffies(10));
		}
		break;
	case PSY_IIO_SET_SHIP_MODE:
		sgm41542_enter_ship_mode(sgm_chg);
		break;
	case PSY_IIO_DP_DM:
		sgm41542_dp_dm(sgm_chg, val1);
		break;
	case PSY_IIO_MAIN_CHARGE_FEED_WDT:
		sgm41542_feed_watchdog_timer(sgm_chg);
		break;
	default:
		dev_err(sgm_chg->dev, "Unsupported SGM4154X IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0)
		dev_err(sgm_chg->dev, "Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, ret);

	return ret;
}

static int sgm41542_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct sgm41542_device *sgm_chg = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = sgm_chg->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(sgm41542_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info sgm41542_iio_info = {
	.read_raw	= sgm41542_iio_read_raw,
	.write_raw	= sgm41542_iio_write_raw,
	.of_xlate	= sgm41542_iio_of_xlate,
};

static int sgm41542_init_iio_psy(struct sgm41542_device *sgm_chg)
{
	struct iio_dev *indio_dev = sgm_chg->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(sgm41542_iio_psy_channels);
	int ret, i;

	sgm_chg->iio_chan = devm_kcalloc(sgm_chg->dev, num_iio_channels,
				sizeof(*sgm_chg->iio_chan), GFP_KERNEL);
	if (!sgm_chg->iio_chan)
		return -ENOMEM;

	sgm_chg->int_iio_chans = devm_kcalloc(sgm_chg->dev,
				num_iio_channels,
				sizeof(*sgm_chg->int_iio_chans),
				GFP_KERNEL);
	if (!sgm_chg->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &sgm41542_iio_info;
	indio_dev->dev.parent = sgm_chg->dev;
	indio_dev->dev.of_node = sgm_chg->dev->of_node;
	indio_dev->name = "sgm41542_chg";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = sgm_chg->iio_chan;
	indio_dev->num_channels = num_iio_channels;

	for (i = 0; i < num_iio_channels; i++) {
		sgm_chg->int_iio_chans[i].indio_dev = indio_dev;
		chan = &sgm_chg->iio_chan[i];
		sgm_chg->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = sgm41542_iio_psy_channels[i].channel_num;
		chan->type = sgm41542_iio_psy_channels[i].type;
		chan->datasheet_name =
			sgm41542_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			sgm41542_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			sgm41542_iio_psy_channels[i].info_mask;
	}

	ret = devm_iio_device_register(sgm_chg->dev, indio_dev);
	if (ret)
		dev_err(sgm_chg->dev, "Failed to register QG IIO device, rc=%d\n", ret);

	dev_err(sgm_chg->dev, "SGM4154X IIO device, rc=%d\n", ret);
	return ret;
}

//+ bug 761884, tankaikun@wt, add 20220701, charger bring up
#ifdef CONFIG_MAIN_CHG_EXTCON
int sgm41542_extcon_init(struct sgm41542_device *sgm_chg)
{
	int rc;

	/* extcon registration */
	sgm_chg->extcon = devm_extcon_dev_allocate(sgm_chg->dev, sgm41542_extcon_cable);
	if (IS_ERR(sgm_chg->extcon)) {
		rc = PTR_ERR(sgm_chg->extcon);
		dev_err(sgm_chg->dev, "failed to allocate extcon device rc=%d\n",
				rc);
		return rc;
	}

	rc = devm_extcon_dev_register(sgm_chg->dev, sgm_chg->extcon);
	if (rc < 0) {
		dev_err(sgm_chg->dev, "failed to register extcon device rc=%d\n",
				rc);
		return rc;
	}

	/* Support reporting polarity and speed via properties */
	rc = extcon_set_property_capability(sgm_chg->extcon,
			EXTCON_USB, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(sgm_chg->extcon,
			EXTCON_USB, EXTCON_PROP_USB_SS);
	rc |= extcon_set_property_capability(sgm_chg->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_TYPEC_POLARITY);
	rc |= extcon_set_property_capability(sgm_chg->extcon,
			EXTCON_USB_HOST, EXTCON_PROP_USB_SS);
	if (rc < 0)
		dev_err(sgm_chg->dev,
			"failed to configure extcon capabilities\n");

	return rc;
}
#endif /* CONFIG_MAIN_CHG_EXTCON */
//- bug 761884, tankaikun@wt, add 20220701, charger bring up

static void sgm41542_shutdown_set_vbus_5V(struct sgm41542_device *sgm_chg)
{
	int vbus;

	sgm41542_get_vbus_volt(sgm_chg, &vbus);
	pr_err("%s vbus before is %d\n", __func__, vbus);

	if (vbus > HVDCP_AFC_VOLT_MIN) {
		switch(sgm_chg->vbus_type) {
			case SGM41542_VBUS_USB_HVDCP:
				sgm41542_enable_qc20_hvdcp_5v(sgm_chg);
				break;
			case SGM41542_VBUS_USB_AFC:
				sgm41542_set_afc_vol(sgm_chg, 5000);
				break;
			default:
				break;
		}
	}

	sgm41542_get_vbus_volt(sgm_chg, &vbus);
	pr_err("%s vbus after is %d\n", __func__, vbus);
}

static int sgm41542_parse_dt(struct device *dev, struct sgm41542_device *sgm_chg)
{
	struct device_node *np = dev->of_node;
	int ret=0;

	sgm_chg->irq_gpio = of_get_named_gpio(np, "sgm41542,intr-gpio", 0);
    if (sgm_chg->irq_gpio < 0) {
		pr_err("%s get irq_gpio false\n", __func__);
	} else {
		pr_err("%s irq_gpio info %d\n", __func__, sgm_chg->irq_gpio);
	}

	sgm_chg->chg_en_gpio = of_get_named_gpio(np, "sgm41542,chg-en-gpio", 0);
	if (sgm_chg->chg_en_gpio < 0) {
		pr_err("%s get chg_en_gpio fail !\n", __func__);
	} else {
		pr_err("%s chg_en_gpio info %d\n", __func__, sgm_chg->chg_en_gpio);
	}

	sgm_chg->otg_en_gpio = of_get_named_gpio(np, "sgm41542,otg-en-gpio", 0);
	if (sgm_chg->otg_en_gpio < 0) {
		pr_err("%s get otg_en_gpio fail !\n", __func__);
	} else {
		pr_err("%s otg_en_gpio info %d\n", __func__, sgm_chg->otg_en_gpio);
		gpio_direction_output(sgm_chg->otg_en_gpio, 0);
	}

	sgm_chg->cfg.disable_hvdcp = of_property_read_bool(np, "sgm41542,disable-hvdcp");
	if (sgm_chg->cfg.disable_hvdcp < 0) {
		sgm_chg->cfg.disable_hvdcp = 1;
	} else {
		dev_err(sgm_chg->dev, "disable_hvdcp: %d\n", sgm_chg->cfg.disable_hvdcp);
	}

	sgm_chg->cfg.enable_auto_dpdm = of_property_read_bool(np, "sgm41542,enable-auto-dpdm");
	if (sgm_chg->cfg.enable_auto_dpdm<0) {
		sgm_chg->cfg.enable_auto_dpdm = 1;
	} else {
		dev_err(sgm_chg->dev, "enable_auto_dpdm: %d\n", sgm_chg->cfg.enable_auto_dpdm);
	}

	sgm_chg->cfg.enable_term = of_property_read_bool(np, "sgm41542,enable-termination");
	if (sgm_chg->cfg.enable_term < 0) {
		sgm_chg->cfg.enable_term = 1;
	} else {
		dev_err(sgm_chg->dev, "enable_term: %d\n", sgm_chg->cfg.enable_term);
	}

	sgm_chg->cfg.enable_ico = of_property_read_bool(np, "sgm41542,enable-ico");
	if (sgm_chg->cfg.enable_ico < 0) {
		sgm_chg->cfg.enable_ico = 1;
	} else {
		dev_err(sgm_chg->dev, "enable_ico: %d\n", sgm_chg->cfg.enable_ico);
	}

	sgm_chg->cfg.use_absolute_vindpm = of_property_read_bool(np, "sgm41542,use-absolute-vindpm");
	if (sgm_chg->cfg.use_absolute_vindpm < 0) {
		sgm_chg->cfg.use_absolute_vindpm = 1;
	} else {
		dev_err(sgm_chg->dev, "use_absolute_vindpm: %d\n", sgm_chg->cfg.use_absolute_vindpm);
	}

	//+bug816469,tankaikun.wt,add,2023/1/30, IR compensation
	sgm_chg->cfg.enable_ir_compensation = of_property_read_bool(np, "sgm41542,enable-ir-compensation");
	if (sgm_chg->cfg.enable_ir_compensation < 0) {
		sgm_chg->cfg.enable_ir_compensation = 0;
	} else {
		dev_err(sgm_chg->dev, "enable_ir_compensation: %d\n", sgm_chg->cfg.enable_ir_compensation);
		sgm_chg->cfg.enable_ir_compensation = 0;
	}
	//-bug816469,tankaikun.wt,add,2023/1/30, IR compensation

	ret = of_property_read_u32(np, "sgm41542,charge-voltage", &sgm_chg->cfg.charge_voltage);
	if (ret < 0) {
		sgm_chg->cfg.charge_voltage = 4432;
	} else {
		dev_err(sgm_chg->dev, "charge_voltage: %d\n", sgm_chg->cfg.charge_voltage);
	}

	ret = of_property_read_u32(np, "sgm41542,charge-current", &sgm_chg->cfg.charge_current);
	if (ret < 0) {
		sgm_chg->cfg.charge_current = DEFAULT_CHG_CURR;
	} else {
		dev_err(sgm_chg->dev, "charge_current: %d\n", sgm_chg->cfg.charge_current);
	}

	ret = of_property_read_u32(np, "sgm41542,term-current", &sgm_chg->cfg.term_current);
	if (ret < 0) {
		sgm_chg->cfg.term_current = DEFAULT_TERM_CURR;
	} else {
		dev_err(sgm_chg->dev, "term-curren: %d\n", sgm_chg->cfg.term_current);
	}

	ret = of_property_read_u32(np, "sgm41542,prechg_current", &sgm_chg->cfg.prechg_current);
	if (ret < 0) {
		sgm_chg->cfg.prechg_current = DEFAULT_PRECHG_CURR;
	} else {
		dev_err(sgm_chg->dev, "prechg_current: %d\n", sgm_chg->cfg.prechg_current);
	}

	return 0;
}

static int sgm41542_charger_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int ret;
	int irqn;
	struct iio_dev *indio_dev;
	const struct of_device_id *id_t;
	struct sgm41542_device *sgm_chg;

	printk("sgm41542 probe start");

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*sgm_chg));
	sgm_chg = iio_priv(indio_dev);
	if (!sgm_chg) {
		dev_err(&client->dev, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	sgm_chg->indio_dev = indio_dev;
	sgm_chg->dev = &client->dev;
	sgm_chg->client = client;
	i2c_set_clientdata(client, sgm_chg);

	id_t = of_match_device(of_match_ptr(sgm41542_charger_match_table), sgm_chg->dev);
	if (!id_t) {
		pr_err("Couldn't find a matching device\n");
		return -ENODEV;
	}

	if (client->dev.of_node)
		sgm41542_parse_dt(&client->dev, sgm_chg);

	ret = sgm41542_detect_device(sgm_chg);
	if (!ret && (sgm_chg->part_no == SGM41543D || sgm_chg->part_no ==SGM41511)) {
		sgm_chg->is_sgm41542 = true;
		dev_err(sgm_chg->dev, "charger:sgm41542 detected, part_no: %d, revision:%d\n",
						sgm_chg->part_no, sgm_chg->revision);
	} else {
		dev_err(sgm_chg->dev, "no sgm41542 charger device found:%d\n",  ret);
		//bug 784499, liyiying@wt, add, 2022/8/5, plug usb not reaction
		return -ENODEV;
	}

	g_sgm_chg = sgm_chg;
	ret = sgm41542_init_iio_psy(sgm_chg);
	if (ret < 0) {
		dev_err(sgm_chg->dev, "Failed to initialize sgm41542 iio psy: %d\n", ret);
		goto err_1;
	}

	sgm41542_ext_iio_init(sgm_chg);

	sgm_chg->sgm41542_wake_source.source = wakeup_source_register(NULL,
							"sgm41542_wake");
	sgm_chg->sgm41542_wake_source.disabled = 1;
	sgm_chg->sgm41542_apsd_wake_source.source = wakeup_source_register(NULL,
							"sgm41542_apsd_wake");
	sgm_chg->sgm41542_apsd_wake_source.disabled = 1;
	sgm_chg->hvdcp_det = 0;
	sgm_chg->apsd_rerun = 0;
	sgm_chg->vbus_good = 0;
	sgm_chg->init_bc12_detect = false;
	sgm_chg->bc12_float_check = 0;

	ret = sgm41542_init_device(sgm_chg);
	if (ret) {
		dev_err(sgm_chg->dev, "device init failure: %d\n", ret);
		goto err_0;
	}

	ret = gpio_request(sgm_chg->irq_gpio, "sgm41542 irq pin");
	if (ret) {
		dev_err(sgm_chg->dev, "%d gpio request failed\n", sgm_chg->irq_gpio);
		goto err_0;
	}

	ret = gpio_request(sgm_chg->chg_en_gpio, "chg_en_gpio");
	if (ret) {
		dev_err(sgm_chg->dev, "%d chg_en_gpio request failed\n",
						sgm_chg->chg_en_gpio);
		goto err_0;
	}

	irqn = gpio_to_irq(sgm_chg->irq_gpio);
	if (irqn < 0) {
		dev_err(sgm_chg->dev, "%d gpio_to_irq failed\n", irqn);
		ret = irqn;
		goto err_0;
	}
	client->irq = irqn;

	/*ret = sgm41542_extcon_init(sgm_chg);
    if (ret) {
	    dev_err(sgm_chg->dev, "sgm41542 extcon init failure: %d\n", ret);
	    goto err_0;
    }*/


	INIT_DELAYED_WORK(&sgm_chg->irq_work, sgm41542_charger_irq_workfunc);
	INIT_DELAYED_WORK(&sgm_chg->check_adapter_work, sgm41542_charger_check_adapter_workfunc);
	INIT_DELAYED_WORK(&sgm_chg->detect_work, sgm41542_charger_detect_workfunc);
	INIT_DELAYED_WORK(&sgm_chg->monitor_work, sgm41542_monitor_workfunc);
	INIT_DELAYED_WORK(&sgm_chg->rerun_apsd_work, sgm41542_rerun_apsd_workfunc);

	ret = sysfs_create_group(&sgm_chg->dev->kobj, &sgm41542_attr_group);
	if (ret) {
		dev_err(sgm_chg->dev, "failed to register sysfs. err: %d\n", ret);
		goto err_irq;
	}

	ret = devm_request_threaded_irq(sgm_chg->dev,client->irq, NULL,
								sgm41542_irq_handler,IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
								"sgm41542_charger1_irq", sgm_chg);
	if (ret) {
		dev_err(sgm_chg->dev, "Request IRQ %d failed: %d\n", client->irq, ret);
		goto err_irq;
	} else {
		dev_err(sgm_chg->dev, "Request irq = %d\n", client->irq);
	}

	enable_irq_wake(irqn);

	if (sgm_chg->is_sgm41542) {
		//hardwareinfo_set_prop(HARDWARE_SLAVE_CHARGER, "SGM41542_CHARGER");
	} else {
		//hardwareinfo_set_prop(HARDWARE_SLAVE_CHARGER, "UNKNOWN");
	}

//sdp Recognition error err,gudi.wt,20230717
	//sgm41542_set_input_current_limit(sgm_chg, 100);

	//schedule_delayed_work(&sgm_chg->irq_work, msecs_to_jiffies(5000));
	//schedule_delayed_work(&sgm_chg->monitor_work, msecs_to_jiffies(10000));
//+P86801AA1-1797, xubuchao1.wt, add, 2023/4/25, add charger hardware info
	hardwareinfo_set_prop(HARDWARE_CHARGER_IC, CHARGER_IC_NAME);
//-P86801AA1-1797, xubuchao1.wt, add, 2023/4/25, add charger hardware info

	//bug816469,tankaikun.wt,add,2023/1/30, IR compensation
	sgm_chg->dynamic_adjust_charge_update_timer = jiffies;

	printk("sgm41542 driver probe succeed !!! \n");
	return 0;

err_irq:
	cancel_delayed_work_sync(&sgm_chg->irq_work);
	cancel_delayed_work_sync(&sgm_chg->monitor_work);
err_1:
err_0:
	g_sgm_chg = NULL;
	return ret;
}

static int sgm41542_charger_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sgm41542_device *sgm_chg = i2c_get_clientdata(client);

	if(sgm_chg->vbus_good == true)
		schedule_delayed_work(&sgm_chg->monitor_work, msecs_to_jiffies(20));
	return 0;
}

static int sgm41542_charger_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sgm41542_device *sgm_chg = i2c_get_clientdata(client);
	if(sgm_chg->vbus_good == true)
		cancel_delayed_work(&sgm_chg->monitor_work);
	return 0;
}

static void sgm41542_charger_shutdown(struct i2c_client *client)
{
	struct sgm41542_device *sgm_chg = i2c_get_clientdata(client);

	dev_info(sgm_chg->dev, "%s: shutdown\n", __func__);

	//+bug 816469,tankaikun.wt,add,2023/1/30, IR compensation
	sgm41542_set_chargevoltage(sgm_chg, sgm_chg->final_cv/1000);
	//-bug 816469,tankaikun.wt,add,2023/1/30, IR compensation

	sgm41542_shutdown_set_vbus_5V(sgm_chg);

	sysfs_remove_group(&sgm_chg->dev->kobj, &sgm41542_attr_group);
	cancel_delayed_work_sync(&sgm_chg->irq_work);
	cancel_delayed_work_sync(&sgm_chg->monitor_work);

	//free_irq(sgm_chg->client->irq, NULL);
	g_sgm_chg = NULL;
}

static const struct i2c_device_id sgm41542_charger_id[] = {
	{ "sgm41543D", SGM41543D },
	{},
};

MODULE_DEVICE_TABLE(i2c, sgm41542_charger_id);

static const struct dev_pm_ops sgm41542_charger_pm_ops = {
	.resume		= sgm41542_charger_resume,
	.suspend	= sgm41542_charger_suspend,
};

static struct i2c_driver sgm41542_charger_driver = {
	.driver		= {
		.name	= "sgm41542",
		.of_match_table = sgm41542_charger_match_table,
		.pm = &sgm41542_charger_pm_ops,
	},
	.id_table	= sgm41542_charger_id,

	.probe		= sgm41542_charger_probe,
	.shutdown   = sgm41542_charger_shutdown,
};

module_i2c_driver(sgm41542_charger_driver);

MODULE_DESCRIPTION("SGMICRO SGM41542 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Allan.ouyang <yangpingao@wingtech.com>");

