/*
 * BQ2589x battery charging driver
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

#define pr_fmt(fmt)	"[bq2589x]:%s: " fmt, __func__

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
#include <tcpm.h>
#include <mt-plat/mtk_boot.h>
#include <mtk_musb.h>
#include <linux/hardware_info.h>

#define _BQ25890H_
#include "bq2589x_reg.h"

enum bq2589x_part_no {
	SY6970 = 0x01,
	BQ25890 = 0x03,
};

enum {
	PN_SY6970,
	PN_BQ25890,
	PN_BQ25892,
	PN_BQ25895,
};

static int pn_data[] = {
	[PN_SY6970] = 0x01,
	[PN_BQ25890] = 0x03,
	[PN_BQ25892] = 0x00,
	[PN_BQ25895] = 0x07,
};

static char *pn_str[] = {
	[PN_SY6970] = "sy6970",
	[PN_BQ25890] = "bq25890",
	[PN_BQ25892] = "bq25892",
	[PN_BQ25895] = "bq25895",
};

struct chg_para{
	int vlim;
	int ilim;

	int vreg;
	int ichg;
};

struct bq2589x_platform_data {
	int iprechg;
	int iterm;

	int boostv;
	int boosti;

	struct chg_para usb;
	bool enable_term;
};

struct bq2589x {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;
	bool hz_mode;
	bool chg_det_enable;

	int chg_type;

	int status;
	int irq;
	int irq_gpio;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;

	struct bq2589x_platform_data *platform_data;
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
#endif
};

static const struct charger_properties bq2589x_chg_props = {
	.alias_name = "bq2589x",
};

static int __bq2589x_read_reg(struct bq2589x *bq, u8 reg, u8 *data)
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

static int __bq2589x_write_reg(struct bq2589x *bq, int reg, u8 val)
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

static int bq2589x_read_byte(struct bq2589x *bq, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2589x_read_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	return ret;
}

static int bq2589x_write_byte(struct bq2589x *bq, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2589x_write_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int bq2589x_update_bits(struct bq2589x *bq, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2589x_read_reg(bq, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq2589x_write_reg(bq, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int bq2589x_enable_otg(struct bq2589x *bq)
{

	u8 val = BQ2589X_OTG_ENABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
				   BQ2589X_OTG_CONFIG_MASK, val);
}

static int bq2589x_disable_otg(struct bq2589x *bq)
{
	u8 val = BQ2589X_OTG_DISABLE << BQ2589X_OTG_CONFIG_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03,
				   BQ2589X_OTG_CONFIG_MASK, val);
}

static int bq2589x_enable_hvdcp(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_HVDCP_ENABLE << BQ2589X_HVDCPEN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, 
				BQ2589X_HVDCPEN_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_enable_hvdcp);

static int bq2589x_disable_hvdcp(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_HVDCP_DISABLE << BQ2589X_HVDCPEN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, 
				BQ2589X_HVDCPEN_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_disable_hvdcp);
static int bq2589x_enable_charger(struct bq2589x *bq)
{
	int ret;

	u8 val = BQ2589X_CHG_ENABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, 
				BQ2589X_CHG_CONFIG_MASK, val);
	return ret;
}

static int bq2589x_disable_charger(struct bq2589x *bq)
{
	int ret;

	u8 val = BQ2589X_CHG_DISABLE << BQ2589X_CHG_CONFIG_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_03, 
				BQ2589X_CHG_CONFIG_MASK, val);
	return ret;
}

int bq2589x_adc_start(struct bq2589x *bq, bool oneshot)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_02, &val);
	if (ret < 0) {
		dev_err(bq->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
		return ret;
	}

	if (((val & BQ2589X_CONV_RATE_MASK) >> BQ2589X_CONV_RATE_SHIFT) == BQ2589X_ADC_CONTINUE_ENABLE)
		return 0; /*is doing continuous scan*/
	if (oneshot)
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_START_MASK,
					BQ2589X_CONV_START << BQ2589X_CONV_START_SHIFT);
	else
		ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK,  
					BQ2589X_ADC_CONTINUE_ENABLE << BQ2589X_CONV_RATE_SHIFT);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_adc_start);

int bq2589x_adc_stop(struct bq2589x *bq)
{
	return bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_CONV_RATE_MASK, 
				BQ2589X_ADC_CONTINUE_DISABLE << BQ2589X_CONV_RATE_SHIFT);
}
EXPORT_SYMBOL_GPL(bq2589x_adc_stop);


int bq2589x_adc_read_battery_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0E, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read battery voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_BATV_BASE + ((val & BQ2589X_BATV_MASK) >> BQ2589X_BATV_SHIFT) * BQ2589X_BATV_LSB ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_battery_volt);


int bq2589x_adc_read_sys_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_0F, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read system voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_SYSV_BASE + ((val & BQ2589X_SYSV_MASK) >> BQ2589X_SYSV_SHIFT) * BQ2589X_SYSV_LSB ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_sys_volt);

int bq2589x_adc_read_vbus_volt(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_11, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		volt = BQ2589X_VBUSV_BASE + ((val & BQ2589X_VBUSV_MASK) >> BQ2589X_VBUSV_SHIFT) * BQ2589X_VBUSV_LSB ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_vbus_volt);

int bq2589x_adc_read_temperature(struct bq2589x *bq)
{
	uint8_t val;
	int temp;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_10, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read temperature failed :%d\n", ret);
		return ret;
	} else{
		temp = BQ2589X_TSPCT_BASE + ((val & BQ2589X_TSPCT_MASK) >> BQ2589X_TSPCT_SHIFT) * BQ2589X_TSPCT_LSB ;
		return temp;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_temperature);

int bq2589x_adc_read_charge_current(struct bq2589x *bq)
{
	uint8_t val;
	int volt;
	int ret;
	ret = bq2589x_read_byte(bq, BQ2589X_REG_12, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read charge current failed :%d\n", ret);
		return ret;
	} else{
		volt = (int)(BQ2589X_ICHGR_BASE + ((val & BQ2589X_ICHGR_MASK) >> BQ2589X_ICHGR_SHIFT) * BQ2589X_ICHGR_LSB) ;
		return volt;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_adc_read_charge_current);
int bq2589x_set_chargecurrent(struct bq2589x *bq, int curr)
{
	u8 ichg;

	if (curr < BQ2589X_ICHG_BASE)
		curr = BQ2589X_ICHG_BASE;

	ichg = (curr - BQ2589X_ICHG_BASE)/BQ2589X_ICHG_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_04, 
						BQ2589X_ICHG_MASK, ichg << BQ2589X_ICHG_SHIFT);

}

int bq2589x_set_term_current(struct bq2589x *bq, int curr)
{
	u8 iterm;

	if (curr < BQ2589X_ITERM_BASE)
		curr = BQ2589X_ITERM_BASE;

	iterm = (curr - BQ2589X_ITERM_BASE) / BQ2589X_ITERM_LSB;

	return bq2589x_update_bits(bq, BQ2589X_REG_05, 
						BQ2589X_ITERM_MASK, iterm << BQ2589X_ITERM_SHIFT);

}
EXPORT_SYMBOL_GPL(bq2589x_set_term_current);

int bq2589x_set_prechg_current(struct bq2589x *bq, int curr)
{
	u8 iprechg;

	if (curr < BQ2589X_IPRECHG_BASE)
		curr = BQ2589X_IPRECHG_BASE;

	iprechg = (curr - BQ2589X_IPRECHG_BASE) / BQ2589X_IPRECHG_LSB;

	return bq2589x_update_bits(bq, BQ2589X_REG_05, 
						BQ2589X_IPRECHG_MASK, iprechg << BQ2589X_IPRECHG_SHIFT);

}
EXPORT_SYMBOL_GPL(bq2589x_set_prechg_current);

int bq2589x_set_chargevolt(struct bq2589x *bq, int volt)
{
	u8 val;

	if (volt < BQ2589X_VREG_BASE)
		volt = BQ2589X_VREG_BASE;

	val = (volt - BQ2589X_VREG_BASE)/BQ2589X_VREG_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_06, 
						BQ2589X_VREG_MASK, val << BQ2589X_VREG_SHIFT);
}

int bq2589x_set_input_volt_limit(struct bq2589x *bq, int volt)
{
	u8 val;

	if (volt < BQ2589X_VINDPM_BASE)
		volt = BQ2589X_VINDPM_BASE;

	val = (volt - BQ2589X_VINDPM_BASE) / BQ2589X_VINDPM_LSB;
	return bq2589x_update_bits(bq, BQ2589X_REG_0D, 
						BQ2589X_VINDPM_MASK, val << BQ2589X_VINDPM_SHIFT);
}

int bq2589x_set_input_current_limit(struct bq2589x *bq, int curr)
{
	u8 val;

	if (curr < BQ2589X_IINLIM_BASE)
		curr = BQ2589X_IINLIM_BASE;

	val = (curr - BQ2589X_IINLIM_BASE) / BQ2589X_IINLIM_LSB;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, BQ2589X_IINLIM_MASK, 
						val << BQ2589X_IINLIM_SHIFT);
}

int bq2589x_set_watchdog_timer(struct bq2589x *bq, u8 timeout)
{
	u8 val;

	val = (timeout - BQ2589X_WDT_BASE) / BQ2589X_WDT_LSB;
	val <<= BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, 
						BQ2589X_WDT_MASK, val); 
}
EXPORT_SYMBOL_GPL(bq2589x_set_watchdog_timer);

int bq2589x_disable_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_DISABLE << BQ2589X_WDT_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, 
						BQ2589X_WDT_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2589x_disable_watchdog_timer);

int bq2589x_reset_watchdog_timer(struct bq2589x *bq)
{
	u8 val = BQ2589X_WDT_RESET << BQ2589X_WDT_RESET_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_03, 
						BQ2589X_WDT_RESET_MASK, val);
}
EXPORT_SYMBOL_GPL(bq2589x_reset_watchdog_timer);

int bq2589x_force_dpdm(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_FORCE_DPDM << BQ2589X_FORCE_DPDM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, 
						BQ2589X_FORCE_DPDM_MASK, val);

	pr_info("Force DPDM %s\n", !ret ? 
"successfully" : "failed");
	
	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_force_dpdm);
int bq2589x_reset_chip(struct bq2589x *bq)
{
	int ret;
	u8 val = BQ2589X_RESET << BQ2589X_RESET_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_14, 
						BQ2589X_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_reset_chip);

int bq2589x_enter_hiz_mode(struct bq2589x *bq)
{
	u8 val = BQ2589X_HIZ_ENABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, 
						BQ2589X_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_enter_hiz_mode);

int bq2589x_exit_hiz_mode(struct bq2589x *bq)
{

	u8 val = BQ2589X_HIZ_DISABLE << BQ2589X_ENHIZ_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_00, 
						BQ2589X_ENHIZ_MASK, val);

}
EXPORT_SYMBOL_GPL(bq2589x_exit_hiz_mode);


int bq2589x_hz_mode(struct charger_device *chg_dev, bool en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	pr_err("%s en = %d\n", __func__, en);
	bq->hz_mode = en;

	if(en) {
		bq2589x_enter_hiz_mode(bq);
	} else {
		bq2589x_exit_hiz_mode(bq);
	}

	return 0;
}

int bq2589x_get_hiz_mode(struct bq2589x *bq, u8 *state)
{
	u8 val;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_00, &val);
	if (ret)
		return ret;
	*state = (val & BQ2589X_ENHIZ_MASK) >> BQ2589X_ENHIZ_SHIFT;

	return 0;
}
EXPORT_SYMBOL_GPL(bq2589x_get_hiz_mode);

static int bq2589x_enable_term(struct bq2589x *bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_TERM_ENABLE << BQ2589X_EN_TERM_SHIFT;
	else
		val = BQ2589X_TERM_DISABLE << BQ2589X_EN_TERM_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_07, 
						BQ2589X_EN_TERM_MASK, val);

	return ret;
}
EXPORT_SYMBOL_GPL(bq2589x_enable_term);

int bq2589x_set_boost_current(struct bq2589x *bq, int curr)
{
	u8 temp;

	if (curr == 500)
		temp = BQ2589X_BOOST_LIM_500MA;
	else if (curr == 700)
		temp = BQ2589X_BOOST_LIM_700MA;
	else if (curr == 1100)
		temp = BQ2589X_BOOST_LIM_1100MA;
	else if (curr == 1600)
		temp = BQ2589X_BOOST_LIM_1600MA;
	else if (curr == 1800)
		temp = BQ2589X_BOOST_LIM_1800MA;
	else if (curr == 2100)
		temp = BQ2589X_BOOST_LIM_2100MA;
	else if (curr == 2400)
		temp = BQ2589X_BOOST_LIM_2400MA;
	else
		temp = BQ2589X_BOOST_LIM_1300MA;

	return bq2589x_update_bits(bq, BQ2589X_REG_0A, 
				BQ2589X_BOOST_LIM_MASK, 
				temp << BQ2589X_BOOST_LIM_SHIFT);

}

static int bq2589x_enable_auto_dpdm(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_AUTO_DPDM_ENABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;
	else
		val = BQ2589X_AUTO_DPDM_DISABLE << BQ2589X_AUTO_DPDM_EN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, 
						BQ2589X_AUTO_DPDM_EN_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_enable_auto_dpdm);

int bq2589x_set_boost_voltage(struct bq2589x *bq, int volt)
{
	u8 val = 0;

	if (volt < BQ2589X_BOOSTV_BASE)
		volt = BQ2589X_BOOSTV_BASE;
	if (volt > BQ2589X_BOOSTV_BASE 
			+ (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) 
			* BQ2589X_BOOSTV_LSB)
		volt = BQ2589X_BOOSTV_BASE 
			+ (BQ2589X_BOOSTV_MASK >> BQ2589X_BOOSTV_SHIFT) 
			* BQ2589X_BOOSTV_LSB;

	val = ((volt - BQ2589X_BOOSTV_BASE) / BQ2589X_BOOSTV_LSB) 
			<< BQ2589X_BOOSTV_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_0A, 
				BQ2589X_BOOSTV_MASK, val);


}
EXPORT_SYMBOL_GPL(bq2589x_set_boost_voltage);

static int bq2589x_enable_ico(struct bq2589x* bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2589X_ICO_ENABLE << BQ2589X_ICOEN_SHIFT;
	else
		val = BQ2589X_ICO_DISABLE << BQ2589X_ICOEN_SHIFT;

	ret = bq2589x_update_bits(bq, BQ2589X_REG_02, BQ2589X_ICOEN_MASK, val);

	return ret;

}
EXPORT_SYMBOL_GPL(bq2589x_enable_ico);

static int bq2589x_read_idpm_limit(struct charger_device *chg_dev, bool *in_loop)
{
	uint8_t val;
	int curr;
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	ret = bq2589x_read_byte(bq, BQ2589X_REG_13, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		curr = BQ2589X_IDPM_LIM_BASE + ((val & BQ2589X_IDPM_LIM_MASK) >> BQ2589X_IDPM_LIM_SHIFT) * BQ2589X_IDPM_LIM_LSB ;
		return curr;
	}
}
EXPORT_SYMBOL_GPL(bq2589x_read_idpm_limit);

static int bq2589x_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	int ret = 0;
	uint8_t val;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	ret = bq2589x_read_byte(bq, BQ2589X_REG_13, &val);
	if (ret < 0) {
		dev_err(bq->dev, "read vbus voltage failed :%d\n", ret);
		return ret;
	} else{
		ret = (val & BQ2589X_VDPM_STAT_MASK) >> BQ2589X_VDPM_STAT_SHIFT;
		return ret;
	}
}

static int bq2589x_enable_safety_timer(struct bq2589x *bq)
{
	const u8 val = BQ2589X_CHG_TIMER_ENABLE << BQ2589X_EN_TIMER_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TIMER_MASK,
				   val);
}
EXPORT_SYMBOL_GPL(bq2589x_enable_safety_timer);

static int bq2589x_disable_safety_timer(struct bq2589x *bq)
{
	const u8 val = BQ2589X_CHG_TIMER_DISABLE << BQ2589X_EN_TIMER_SHIFT;

	return bq2589x_update_bits(bq, BQ2589X_REG_07, BQ2589X_EN_TIMER_MASK,
				   val);

}
EXPORT_SYMBOL_GPL(bq2589x_disable_safety_timer);

static struct bq2589x_platform_data *bq2589x_parse_dt(struct device_node *np,
						      struct bq2589x *bq)
{
	int ret;
	struct bq2589x_platform_data *pdata;

	pdata = devm_kzalloc(bq->dev, sizeof(struct bq2589x_platform_data),
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

	pdata->enable_term = of_property_read_bool(np, "ti,bq2589x,enable-term");

	bq->chg_det_enable =
	    of_property_read_bool(np, "ti,bq2589x,charge-detect-enable");

	ret = of_property_read_u32(np, "ti,bq2589x,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of ti,bq2589x,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 1550;
		pr_err("Failed to read node of ti,bq2589x,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of ti,bq2589x,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 1550;
		pr_err("Failed to read node of ti,bq2589x,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of ti,bq2589x,precharge-current\n");
	}

	ret = of_property_read_u32(np, "ti,bq2589x,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
		    ("Failed to read node of ti,bq2589x,termination-current\n");
	}

	ret =
	    of_property_read_u32(np, "ti,bq2589x,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of ti,bq2589x,boost-voltage\n");
	}

	ret =
	    of_property_read_u32(np, "ti,bq2589x,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of ti,bq2589x,boost-current\n");
	}

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "bq2589x,intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s no intr_gpio info\n", __func__);
	} else
		bq->irq_gpio = ret;
#else
	ret = of_property_read_u32(np, "bq2589x,intr_gpio_num", &bq->irq_gpio);
	if (ret < 0)
		pr_err("%s no intr_gpio info\n", __func__);
#endif

	return pdata;
}

static int bq2589x_get_charging_status(struct bq2589x *bq, int *chg_stat)
{
	int ret = 0;
	u8 reg_val = 0;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	if (ret < 0)
		return ret;

	*chg_stat = (reg_val & BQ2589X_CHRG_STAT_MASK) >> BQ2589X_CHRG_STAT_SHIFT;
	pr_err("%s reg0b = 0x%x, stat = %d\n", __func__, reg_val, *chg_stat);
	return ret;
}

static int bq2589x_check_dpdm_done(struct bq2589x *bq, bool *done)
{
	int ret;
	u8 val;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_02, &val);
	if (!ret)
		*done = !!(val & BQ2589X_FORCE_DPDM_MASK);
	
	return ret;
}

extern void mt_usb_connect(void);
extern void mt_usb_disconnect(void);

static int  bq2589x_charger_get_online(struct bq2589x *bq,bool *val);

static int bq2589x_get_charger_type(struct bq2589x *bq)
{
	int ret;

	u8 reg_val = 0;
	int vbus_stat = 0;
	int bc_count = 20;
	bool done;
	int retry = 0;

#ifdef FIXME /* TODO: wait get_boot_mode */
	if (is_meta_mode()) {
		/* Skip charger type detection to speed up meta boot.*/
		dev_notice(bq->dev, "force Standard USB Host in meta\n");
		bq->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		bq->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		power_supply_changed(bq->psy);
		return 0;
	}
#endif

	bq2589x_force_dpdm(bq);
	while(retry++ < 200) {
		mdelay(20);
		ret = bq2589x_check_dpdm_done(bq, &done);
		if (!ret && !done)
			break;
	}

	pr_err("bq2589x_check_dpdm_done:%d\n",retry);

	while (bc_count-- && bq->power_good) {
		ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
		if (ret)
			return ret;
		vbus_stat = (reg_val & BQ2589X_VBUS_STAT_MASK);
		vbus_stat >>= BQ2589X_VBUS_STAT_SHIFT;
		mdelay(10);
		pr_err("%s bc_count = %d vbus_stat = %d\n", __func__, bc_count, vbus_stat);
		if (bq->power_good && BQ2589X_VBUS_TYPE_NONE != vbus_stat)
			break;
	}

	switch (vbus_stat) {
		case BQ2589X_VBUS_TYPE_NONE:
			bq->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			bq->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
			break;
		case BQ2589X_VBUS_TYPE_SDP:
			bq->psy_desc.type = POWER_SUPPLY_TYPE_USB;
			bq->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
			break; 
		case BQ2589X_VBUS_TYPE_CDP:
			bq->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
			bq->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
			break;
		case BQ2589X_VBUS_TYPE_DCP:
			bq->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
			bq->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			break;
		default:
			bq->psy_desc.type = POWER_SUPPLY_TYPE_USB_FLOAT;
			bq->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
			break;
	}
	printk("usb type = $d\n",bq->psy_usb_type);

	if (bq->psy_usb_type != POWER_SUPPLY_USB_TYPE_DCP) {
 		Charger_Detect_Release();
  	}

	if (bq->psy_usb_type == POWER_SUPPLY_USB_TYPE_SDP)
		mt_usb_connect();
	else
		mt_usb_disconnect();

	return 0;
}

static int bq2589x_inform_charger_type(struct bq2589x *bq)
{
	int ret = 0;
	struct power_supply *psy;
	union power_supply_propval propval;

	psy = power_supply_get_by_name("mtk_charger_type");
	if (!psy)
		return -ENODEV;

	if (bq->psy_desc.type != POWER_SUPPLY_TYPE_UNKNOWN)
		propval.intval = 1;
	else
		propval.intval = 0;

	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_ONLINE, &propval);
	if (ret < 0) {
		pr_notice("inform power supply online failed:%d\n", ret);
		return ret;
	}

	propval.intval = bq->psy_usb_type;
 	ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_USB_TYPE, &propval);

#if defined (CONFIG_N23_CHARGER_PRIVATE)
	if (ret < 0) {
			pr_notice("inform power supply type failed:%d\n", ret);
			return ret;
	}
#endif

	power_supply_changed(psy);
	power_supply_changed(bq->psy);

	return ret;
}

static irqreturn_t bq2589x_irq_handler(int irq, void *data)
{
	int ret;
	u8 reg_val;
	bool prev_pg;
	int prev_chg_type;
	struct bq2589x *bq = data;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = bq->power_good;

	bq->power_good = !!(reg_val & BQ2589X_PG_STAT_MASK);
	prev_chg_type = bq->psy_usb_type;

	if (!prev_pg && bq->power_good) {
		pr_notice("adapter/usb inserted\n");
		Charger_Detect_Init();
		bq2589x_get_charger_type(bq);
	} else if (prev_pg && !bq->power_good) {
		pr_notice("adapter/usb removed\n");
	}

	if (prev_chg_type != bq->psy_usb_type && bq->chg_det_enable)
		bq2589x_inform_charger_type(bq);

	return IRQ_HANDLED;
}

static int bq2589x_register_interrupt(struct bq2589x *bq)
{
	int ret = 0;

	ret = devm_gpio_request(bq->dev, bq->irq_gpio, "bq2589x-irq");
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
					bq2589x_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					bq->eint_name, bq);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}

	enable_irq_wake(bq->irq);

	return 0;
}

static void determine_initial_status(struct bq2589x *bq)
{
	bq2589x_irq_handler(bq->irq, (void *) bq);
}

static int bq2589x_init_device(struct bq2589x *bq)
{
	int ret;

	bq2589x_disable_watchdog_timer(bq);

	ret = bq2589x_set_prechg_current(bq, bq->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	ret = bq2589x_set_term_current(bq, bq->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = bq2589x_set_boost_voltage(bq, bq->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = bq2589x_set_boost_current(bq, bq->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	ret = bq2589x_enable_term(bq, bq->platform_data->enable_term);
	if (ret < 0)
		pr_err("Failed to %s termination:%d\n",
			bq->platform_data->enable_term ? "enable" : "disable", ret);

	return 0;
}



static int bq2589x_detect_device(struct bq2589x *bq)
{
	int ret;
	u8 data;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_14, &data);
	if (!ret) {
		bq->part_no = (data & BQ2589X_PN_MASK) >> BQ2589X_PN_SHIFT;
		bq->revision =
		    (data & BQ2589X_DEV_REV_MASK) >> BQ2589X_DEV_REV_SHIFT;
	}
	pr_notice("%s bq2589x:device_id = %d\n",__func__,bq->part_no);
	if(bq->part_no == SY6970){
		pr_notice("/******************charger ic is SY6970***************/\n");
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "SY6970");
	}else if(bq->part_no == BQ25890){
		pr_notice("/******************charger ic is BQ25890***************/\n");
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "BQ25890");
	}
	return ret;
}

static void bq2589x_dump_regs(struct bq2589x *bq)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(bq, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static ssize_t
bq2589x_show_registers(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct bq2589x *bq = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "bq2589x Reg");
	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = bq2589x_read_byte(bq, addr, &val);
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
bq2589x_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct bq2589x *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x14) {
		bq2589x_write_byte(bq, (unsigned char) reg,
				   (unsigned char) val);
	}

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, bq2589x_show_registers,
		   bq2589x_store_registers);

static struct attribute *bq2589x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group bq2589x_attr_group = {
	.attrs = bq2589x_attributes,
};

static int bq2589x_charging(struct charger_device *chg_dev, bool enable)
{

	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = bq2589x_enable_charger(bq);
	else
		ret = bq2589x_disable_charger(bq);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	ret = bq2589x_read_byte(bq, BQ2589X_REG_03, &val);

	if (!ret)
		bq->charge_enabled = !!(val & BQ2589X_CHG_CONFIG_MASK);

	return ret;
}

static int bq2589x_plug_in(struct charger_device *chg_dev)
{

	int ret;

	ret = bq2589x_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int bq2589x_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = bq2589x_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int bq2589x_dump_register(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	bq2589x_dump_regs(bq);

	return 0;
}

static int bq2589x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	*en = bq->charge_enabled;

	return 0;
}

static int bq2589x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_0B, &val);
	if (!ret) {
		val = val & BQ2589X_CHRG_STAT_MASK;
		val = val >> BQ2589X_CHRG_STAT_SHIFT;
		*done = (val == BQ2589X_CHRG_STAT_CHGDONE);
	}

	return ret;
}

static int bq2589x_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	return bq2589x_set_chargecurrent(bq, curr / 1000);
}

static int bq2589x_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_04, &reg_val);
	if (!ret) {
		ichg = (reg_val & BQ2589X_ICHG_MASK) >> BQ2589X_ICHG_SHIFT;
		ichg = ichg * BQ2589X_ICHG_LSB + BQ2589X_ICHG_BASE;
		*curr = ichg * 1000;
	}

	return ret;
}

static int bq2589x_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int bq2589x_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge volt = %d\n", volt);

	return bq2589x_set_chargevolt(bq, volt / 1000);
}

static int bq2589x_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int vchg;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_06, &reg_val);
	if (!ret) {
		vchg = (reg_val & BQ2589X_VREG_MASK) >> BQ2589X_VREG_SHIFT;
		vchg = vchg * BQ2589X_VREG_LSB + BQ2589X_VREG_BASE;
		*volt = vchg * 1000;
	}

	return ret;
}

static int bq2589x_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);

	return bq2589x_set_input_volt_limit(bq, volt / 1000);

}

static int bq2589x_get_ivl(struct charger_device *chg_dev, u32 *mivr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ivl;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_0D, &reg_val);
	if (!ret) {
		ivl = (reg_val & BQ2589X_VINDPM_MASK) >> BQ2589X_VINDPM_SHIFT;
		ivl = ivl * BQ2589X_VINDPM_LSB + BQ2589X_VINDPM_BASE;
		*mivr = ivl * 1000;
	}

	return ret;

}

static int bq2589x_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("indpm curr = %d\n", curr);

	return bq2589x_set_input_current_limit(bq, curr / 1000);
}

static int bq2589x_set_ieoc(struct charger_device *chg_dev, u32 curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("ieoc curr = %d\n", curr);

	return bq2589x_set_term_current(bq, curr / 1000);
}

static int bq2589x_enable_te(struct charger_device *chg_dev, bool en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("enable te = %d\n", en);

	return bq2589x_enable_term(bq, en);
}

static int bq2589x_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int icl;
	int ret;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & BQ2589X_IINLIM_MASK) >> BQ2589X_IINLIM_SHIFT;
		icl = icl * BQ2589X_IINLIM_LSB + BQ2589X_IINLIM_BASE;
		*curr = icl * 1000;
	}

	return ret;
}

static int bq2589x_safety_check(struct charger_device *chg_dev, u32 polling_ieoc)
{
	int ret = 0;
	int adc_ibat = 0;
	static int counter;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	adc_ibat = bq2589x_adc_read_charge_current(bq);

	dev_info(bq->dev, "%s: polling_ieoc = %d, ibat = %d\n",
			__func__, polling_ieoc, adc_ibat);

	if (adc_ibat <= polling_ieoc)
		counter++;
	else
		counter = 0;

	/* If IBAT is less than polling_ieoc for 3 times, trigger EOC event */
	if (counter == 3) {
		dev_info(bq->dev, "%s: polling_ieoc = %d, ibat = %d\n",
			__func__, polling_ieoc, adc_ibat);
		charger_dev_notify(bq->chg_dev, CHARGER_DEV_NOTIFY_EOC);
		counter = 0;
	}

	return ret;
}

static int bq2589x_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 100000;
	return 0;
}

static int bq2589x_kick_wdt(struct charger_device *chg_dev)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	return bq2589x_reset_watchdog_timer(bq);
}

static int bq2589x_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	if (en)
		ret = bq2589x_enable_otg(bq);
	else
		ret = bq2589x_disable_otg(bq);

	pr_err("%s OTG %s\n", en ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	return ret;
}

static int bq2589x_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en)
		ret = bq2589x_enable_safety_timer(bq);
	else
		ret = bq2589x_disable_safety_timer(bq);

	return ret;
}

static int bq2589x_is_safety_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = bq2589x_read_byte(bq, BQ2589X_REG_07, &reg_val);

	if (!ret)
		*en = !!(reg_val & BQ2589X_EN_TIMER_MASK);

	return ret;
}

static int bq2589x_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = bq2589x_set_boost_current(bq, curr / 1000);

	return ret;
}

static int bq2589x_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("%s en = %d\n", __func__, en);

	if(!en)
		return bq2589x_enter_hiz_mode(bq);
	else
		return bq2589x_exit_hiz_mode(bq);

}

static int bq2589x_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	int ret = 0;
	u8 val;
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	ret = bq2589x_get_hiz_mode(bq, &val);
	*en = !(bool)val;
	pr_err("%s en = %d\n", __func__, *en);

	return ret;
}

static int bq2589x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	if (!bq->psy) {
		pr_err("%s: cannot get psy\n", __func__);
		return -ENODEV;
	}

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

static int bq2589x_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
#ifdef CONFIG_TCPC_CLASS
	struct bq2589x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_err("%s en = %d\n", __func__, en);
	bq->chg_det_enable = en;
	if (en) {
		//Charger_Detect_Init();
		bq2589x_get_charger_type(bq);
	} else {
		bq->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		bq->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}
	//bq2589x_inform_charger_type(bq);
#endif /* CONFIG_TCPC_CLASS */
	return ret;
}

static struct charger_ops bq2589x_chg_ops = {
	/* Normal charging */
	.plug_in = bq2589x_plug_in,
	.plug_out = bq2589x_plug_out,
	.dump_registers = bq2589x_dump_register,
	.enable = bq2589x_charging,
	.is_enabled = bq2589x_is_charging_enable,
	.get_charging_current = bq2589x_get_ichg,
	.set_charging_current = bq2589x_set_ichg,
	.get_input_current = bq2589x_get_icl,
	.set_input_current = bq2589x_set_icl,
	.get_constant_voltage = bq2589x_get_vchg,
	.set_constant_voltage = bq2589x_set_vchg,
	.kick_wdt = bq2589x_kick_wdt,
	.set_mivr = bq2589x_set_ivl,
	.get_mivr = bq2589x_get_ivl,
	.get_mivr_state = bq2589x_get_mivr_state,
	.set_eoc_current = bq2589x_set_ieoc,
	.enable_termination = bq2589x_enable_te,
	.safety_check = bq2589x_safety_check,
	.get_min_input_current = bq2589x_get_min_aicr,
	.is_charging_done = bq2589x_is_charging_done,
	.get_min_charging_current = bq2589x_get_min_ichg,

	/* Safety timer */
	.enable_safety_timer = bq2589x_set_safety_timer,
	.is_safety_timer_enabled = bq2589x_is_safety_timer_enabled,

	/* Power path */
	.enable_powerpath = bq2589x_enable_powerpath,
	.is_powerpath_enabled = bq2589x_is_powerpath_enabled,

	/* Charger type detection */
	.enable_chg_type_det =  bq2589x_enable_chg_type_det,

	/* OTG */
	.enable_otg = bq2589x_set_otg,
	.set_boost_current_limit = bq2589x_set_boost_ilmt,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,

	/* Event */
	.event =  bq2589x_do_event,
};

static struct of_device_id bq2589x_charger_match_table[] = {
	{
	 .compatible = "sy,sy6970",
	 .data = &pn_data[PN_SY6970],
	},
	{
	 .compatible = "ti,bq2589x",
	 .data = &pn_data[PN_BQ25890],
	},
	{
	 .compatible = "ti,bq25892",
	 .data = &pn_data[PN_BQ25892],
	},
	{
	 .compatible = "ti,bq25895",
	 .data = &pn_data[PN_BQ25895],
	},
	{},
};
MODULE_DEVICE_TABLE(of, bq2589x_charger_match_table);

/* ======================= */
/* BQ2589X Power Supply Ops */
/* ======================= */

static int  bq2589x_charger_get_online(struct bq2589x *bq,
				     bool *val)
{
	bool pwr_rdy = false;

#ifdef CONFIG_TCPC_CLASS
	mutex_lock(&bq->attach_lock);
	pwr_rdy = bq->attach;
	mutex_unlock(&bq->attach_lock);
#else
	pwr_rdy	= bq->power_good;
#endif
	dev_info(bq->dev, "%s: online = %d\n", __func__, pwr_rdy);
	*val = pwr_rdy;
	return 0;
}



static int  bq2589x_charger_set_online(struct bq2589x *bq,
				     const union power_supply_propval *val)
{
	printk("jun6:%s\n",__func__);
	dump_stack();
	return  bq2589x_enable_chg_type_det(bq->chg_dev, val->intval);
}

static int  bq2589x_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct bq2589x *bq =
						  power_supply_get_drvdata(psy);
	int chg_stat = BQ2589X_CHRG_STAT_IDLE;
	bool pwr_rdy = false;
	int ret = 0;

	dev_dbg(bq->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret =  bq2589x_charger_get_online(bq, &pwr_rdy);
		val->intval = pwr_rdy;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret =  bq2589x_charger_get_online(bq, &pwr_rdy);
		ret =  bq2589x_get_charging_status(bq, &chg_stat);
		if (!pwr_rdy) {
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			return ret;
		}
		switch (chg_stat) {
		case BQ2589X_CHRG_STAT_IDLE:
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		case BQ2589X_CHRG_STAT_PRECHG:
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
			break;
		case BQ2589X_CHRG_STAT_FASTCHG:
			if (bq->charge_enabled) {
				val->intval = POWER_SUPPLY_STATUS_CHARGING;
			} else {
				val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
			break;
		case BQ2589X_CHRG_STAT_CHGDONE:
			val->intval = POWER_SUPPLY_STATUS_FULL;
			break;
		default:
			ret = -ENODATA;
			break;
		}
		break;
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = bq->psy_desc.type;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = bq->psy_usb_type;
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

static int  bq2589x_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct bq2589x *bq =
						  power_supply_get_drvdata(psy);
	int ret;

	dev_dbg(bq->dev, "%s: prop = %d\n", __func__, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		ret =  bq2589x_charger_set_online(bq, val);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int  bq2589x_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	default:
		return 0;
	}
}

static enum power_supply_property  bq2589x_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static enum power_supply_usb_type bq2589x_charger_usb_types[] = {
        POWER_SUPPLY_USB_TYPE_UNKNOWN,
        POWER_SUPPLY_USB_TYPE_SDP,
        POWER_SUPPLY_USB_TYPE_DCP,
        POWER_SUPPLY_USB_TYPE_CDP,
        POWER_SUPPLY_USB_TYPE_C,
        POWER_SUPPLY_USB_TYPE_PD,
        POWER_SUPPLY_USB_TYPE_PD_DRP,
        POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};


static const struct power_supply_desc  bq2589x_charger_desc = {
	.name			= "bq2589x",
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		=  bq2589x_charger_properties,
	.num_properties	= ARRAY_SIZE( bq2589x_charger_properties),
	.get_property	=  bq2589x_charger_get_property,
	.set_property	=  bq2589x_charger_set_property,
	.property_is_writeable	=  bq2589x_charger_property_is_writeable,
	.usb_types		=  bq2589x_charger_usb_types,
	.num_usb_types	= ARRAY_SIZE( bq2589x_charger_usb_types),
};

static char * bq2589x_charger_supplied_to[] = {
	"battery",
	"mtk-master-charger"
};

#ifdef CONFIG_TCPC_CLASS
static int typec_attach_thread(void *data)
{
	struct bq2589x *bq = data;
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

#if defined (CONFIG_N23_CHARGER_PRIVATE)
		if (attach)
			bq2589x_enable_charger(bq);
#endif

		power_supply_set_property(bq->chg_psy,
						POWER_SUPPLY_PROP_ONLINE, &val);
	}
	return ret;
}

static void handle_typec_attach(struct bq2589x *bq,
				bool en)
{
	mutex_lock(&bq->attach_lock);
	bq->attach = en;
	complete(&bq->chrdet_start);
	mutex_unlock(&bq->attach_lock);
}

#if defined (CONFIG_N23_CHARGER_PRIVATE)
//+Bug682956,yangyuhang.wt ,20210813, add cc polarity node
extern int tcpc_set_cc_polarity_state(int state);
#ifdef CONFIG_TCPC_WUSB3801
extern int wusb3801_typec_cc_orientation(void);
#endif
//-Bug682956,yangyuhang.wt ,20210813, add cc polarity node
#endif

static int pd_tcp_notifier_call(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	struct bq2589x *bq =
		(struct bq2589x *)container_of(nb, struct bq2589x, pd_nb);
	printk("jun: %s event = %d\n",__func__,event);
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

#if defined (CONFIG_N23_CHARGER_PRIVATE)
		if(noti->typec_state.new_state != TYPEC_UNATTACHED) {
			tcpc_set_cc_polarity_state(noti->typec_state.polarity + 1);
		} else {
			tcpc_set_cc_polarity_state(0);
		}
#endif
		break;
	default:
		break;
	};
	return NOTIFY_OK;
}
#endif

static int bq2589x_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct bq2589x *bq;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;
	struct power_supply_config charger_cfg = {};

	int ret = 0;

	bq = devm_kzalloc(&client->dev, sizeof(struct bq2589x), GFP_KERNEL);
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
	ret = bq2589x_detect_device(bq);
	if (ret) {
		pr_err("No bq2589x device found!\n");
		return -ENODEV;
	}

	match = of_match_node(bq2589x_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

	if (bq->part_no != *(int *)match->data)
		pr_info("part no mismatch, hw:%s, devicetree:%s\n",
			pn_str[bq->part_no], pn_str[*(int *) match->data]);

	bq->platform_data = bq2589x_parse_dt(node, bq);

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

	ret = bq2589x_init_device(bq);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	bq2589x_register_interrupt(bq);

	bq->chg_dev = charger_device_register(bq->chg_dev_name,
					      &client->dev, bq,
					      &bq2589x_chg_ops,
					      &bq2589x_chg_props);
	if (IS_ERR_OR_NULL(bq->chg_dev)) {
		ret = PTR_ERR(bq->chg_dev);
		return ret;
	}

	/* power supply register */
	memcpy(&bq->psy_desc, &bq2589x_charger_desc, sizeof(bq->psy_desc));

	charger_cfg.drv_data = bq;
	charger_cfg.of_node = client->dev.of_node;
	charger_cfg.supplied_to =  bq2589x_charger_supplied_to;
	charger_cfg.num_supplicants = ARRAY_SIZE(bq2589x_charger_supplied_to);
	bq->psy = devm_power_supply_register(bq->dev,
					&bq->psy_desc, &charger_cfg);
	if (IS_ERR(bq->psy)) {
		pr_err("Fail to register power supply dev\n");
		ret = PTR_ERR(bq->psy);
		goto err_register_psy;
	}

	ret = sysfs_create_group(&bq->dev->kobj, &bq2589x_attr_group);
	if (ret)
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);

#ifdef CONFIG_TCPC_CLASS
	bq->chg_psy = power_supply_get_by_name("bq2589x");
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
#endif

	determine_initial_status(bq);

	pr_err("bq2589x probe successfully, Part Num:%d, Revision:%d\n!",
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

static int bq2589x_charger_remove(struct i2c_client *client)
{
	struct bq2589x *bq = i2c_get_clientdata(client);

	mutex_destroy(&bq->i2c_rw_lock);

	sysfs_remove_group(&bq->dev->kobj, &bq2589x_attr_group);

	return 0;
}

static void bq2589x_charger_shutdown(struct i2c_client *client)
{

}

static struct i2c_driver bq2589x_charger_driver = {
	.driver = {
		   .name = "bq2589x-charger",
		   .owner = THIS_MODULE,
		   .of_match_table = bq2589x_charger_match_table,
		   },

	.probe = bq2589x_charger_probe,
	.remove = bq2589x_charger_remove,
	.shutdown = bq2589x_charger_shutdown,

};

module_i2c_driver(bq2589x_charger_driver);

MODULE_DESCRIPTION("TI BQ2589X Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments");


