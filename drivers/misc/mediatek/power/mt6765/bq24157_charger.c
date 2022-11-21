/*
 * BQ2591x battery charging driver
 *
 * Copyright (C) 2017 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#define pr_fmt(fmt) "[bq2415x]:%s: " fmt, __func__

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
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/time.h>

#include "mtk_charger_intf.h"
#include "bq24157_reg.h"
#include <linux/hardware_info.h>  //bug 436809  modify getian.wt 20190403 Add charger IC model information in factory mode

#undef pr_debug
#define pr_debug  pr_err

#define ETA6937_RE  4
#define	HL7057_PN 0
#define BQ24157_PN 2
#define	ETA6937  1
#define	BQ24157  2
#define	HL7057   3

#define KAL_FALSE 0
#define KAL_TRUE 1

struct bq2415x_config {
	int chg_mv;
	int chg_ma;

	int ivl_mv;
	int icl_ma;
	
	int safety_chg_mv;
	int safety_chg_ma;

	
	int iterm_ma;
	int batlow_mv;
	
	int sensor_mohm;
	
	bool enable_term;
};


struct bq2415x {
	struct device	*dev;
	struct i2c_client *client;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	
	int part_no;
	int revision;
	int vendor;
		
	struct bq2415x_config cfg;
	struct delayed_work monitor_work;

	bool charge_enabled;

	int chg_mv;
	int chg_ma;
	int ivl_mv;
	int icl_ma;
	
	bool boost_mode;
	bool otg_pin_status;
	
	int charge_state;
	int fault_status;

	int prev_stat_flag;
	int prev_fault_flag;

	int reg_stat;
	int reg_fault;
	int reg_stat_flag;
	int reg_fault_flag;

	int skip_reads;
	int skip_writes;

	struct mutex i2c_rw_lock;
	struct wakeup_source eta_charger_lock;
	bool  rst_timeout;
	wait_queue_head_t  wait_que;
	struct hrtimer kthread_timer;
	int polling_interval;
	bool chg_online;
	bool otg_online;
};

static const unsigned char * chg_state_str[] = {
	"Ready", "Charging", "Charge Done", "Charge Fault"
};

static const unsigned char * chg_fault_state_str[] = {
	"Normal", "VBUS OVP", "Sleep mode", "Bad Adapter",
	"Output ovp", "Thermal shutdown", "Timer fault",
	"No battery"
};

static const unsigned char * boost_fault_state_str[] = {
	"Normal", "VBUS OVP", "over load", "battery volt too low",
	"Battery OVP","Thermal shutdown", "Timer fault","N/A"
};

static int bq2415x_set_chargevoltage(struct charger_device *chg_dev, u32 volt);
static int bq2415x_set_chargecurrent(struct charger_device *chg_dev, u32 curr);
static int bq2415x_set_input_volt_limit(struct charger_device *chg_dev, u32 volt);
static int bq2415x_set_input_current_limit(struct charger_device *chg_dev, u32 curr);
static int bq2415x_charging(struct charger_device *chg_dev, bool enable);
static int eta6937_enable_input_current_selection(struct bq2415x *bq, bool en);
static unsigned int bmt_find_closest_level(const unsigned int *pList, unsigned int number,unsigned int level);
static unsigned int charging_parameter_to_value(const unsigned int *parameter,const unsigned int array_size, const unsigned int val);
static int eta6937_reinit(struct bq2415x *bq);
static void bq2415x_dump_regs(struct bq2415x *bq);
static int bq2415x_enable_otg(struct charger_device *chg_dev, bool en);
/*distinguish which charger ic we use,vendor:ETA6937,BQ24157,HL7057*/
static int distinguish_charger_ic(struct bq2415x *bq)
{

	if(bq->part_no == HL7057_PN)
	    return HL7057;
	if(bq->revision == ETA6937_RE)
		return ETA6937;

	return BQ24157;
}

static bool is_eta6937(struct bq2415x *bq)
{
	if(ETA6937 == distinguish_charger_ic(bq)){
		return KAL_TRUE;
	} else {
		return KAL_FALSE;
	}
}

static bool is_hl7057(struct bq2415x *bq)
{
	if(HL7057 == distinguish_charger_ic(bq)){
		return KAL_TRUE;
	} else {
		return KAL_FALSE;
	}
}

/************************bq2415x code start from here**********************/
static int __bq2415x_read_reg(struct bq2415x *bq, u8 reg, u8 *data)
{
	s32 ret;
	u8 val = 0;
	int status = 0;

	ret = i2c_smbus_read_byte_data(bq->client, reg);
	if (ret < 0) {
		pr_err("fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}
	*data = (u8)ret;
	
	val = (u8)ret;
	if(reg== BQ2415X_REG_00){
		status = (val & BQ2415X_FAULT_MASK)
				 >> BQ2415X_FAULT_SHIFT;
		if(status){
			bq->fault_status = status;
			pr_err("Fault Status:%s\n", bq->otg_online ?
						  boost_fault_state_str[bq->fault_status] 
						: chg_fault_state_str[bq->fault_status]);
		}
		
	}

	return 0;
}

static int __bq2415x_write_reg(struct bq2415x *bq, int reg, u8 val)
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

static int bq2415x_read_byte(struct bq2415x *bq, u8 reg, u8 *data)
{
	int ret;

	if (bq->skip_reads) {
		*data = 0;
		return 0;
	}

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2415x_read_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);
	
	return ret;
}


static int bq2415x_write_byte(struct bq2415x *bq, u8 reg, u8 data)
{
	int ret;

	if (bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2415x_write_reg(bq, reg, data);
	mutex_unlock(&bq->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}


static int bq2415x_update_bits(struct bq2415x *bq, u8 reg,
					u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (bq->skip_reads || bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2415x_read_reg(bq, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq2415x_write_reg(bq, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}



static int bq2415x_enable_charger(struct bq2415x *bq)
{
	int ret;
	u8 val;

	val = BQ2415X_CHARGE_ENABLE << BQ2415X_CHARGE_ENABLE_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_CHARGE_ENABLE_MASK, val);
	return ret;
}

static int bq2415x_disable_charger(struct bq2415x *bq)
{
	int ret;
	u8 val;

	val = BQ2415X_CHARGE_DISABLE << BQ2415X_CHARGE_ENABLE_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_CHARGE_ENABLE_MASK, val);

	return ret;
}

static int bq2415x_disable_low_charge(struct bq2415x *bq)
{
	int ret;
	u8 val;

	val = BQ2415X_DIS_LOW_CHG << BQ2415X_LOW_CHG_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_05,
				BQ2415X_LOW_CHG_MASK, val);

	return ret;
}

static int bq2415x_enable_term(struct bq2415x *bq, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = BQ2415X_TERM_ENABLE;
	else
		val = BQ2415X_TERM_DISABLE;

	val <<= BQ2415X_TERM_ENABLE_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_TERM_ENABLE_MASK, val);

	return ret;
}

int bq2415x_reset_chip(struct bq2415x *bq)
{
	int ret;
	u8 val = BQ2415X_RESET << BQ2415X_RESET_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_04,
				BQ2415X_RESET_MASK, val);
	return ret;
}
EXPORT_SYMBOL_GPL(bq2415x_reset_chip);

static int bq2415x_set_vbatlow_volt(struct bq2415x *bq, int volt)
{
	int ret;
	u8 val;

	val = (volt - BQ2415X_WEAK_BATT_VOLT_BASE) / BQ2415X_WEAK_BATT_VOLT_LSB;

	val <<= BQ2415X_WEAK_BATT_VOLT_SHIFT;

	pr_debug("bq2415x_set_vbatlow_volt val:0x%02X\n", val);
	
	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_WEAK_BATT_VOLT_MASK, val);

	return ret;
}
#if 0
static int bq2415x_set_safety_chg_curr(struct bq2415x *bq, int curr)
{
	u8 ichg;
	
	ichg = (curr * bq->cfg.sensor_mohm / 100 -  BQ2415X_MAX_ICHG_BASE) / BQ2415X_MAX_ICHG_LSB;

	ichg <<= BQ2415X_MAX_ICHG_SHIFT;

	return bq2415x_update_bits(bq, BQ2415X_REG_06,
				BQ2415X_MAX_ICHG_MASK, ichg);
	
}

static int bq2415x_set_safety_chg_volt(struct bq2415x *bq, int volt)
{
	u8 val;

	val = (volt - BQ2415X_MAX_VREG_BASE)/BQ2415X_MAX_VREG_LSB;
	val <<= BQ2415X_MAX_VREG_SHIFT;
	
	return bq2415x_update_bits(bq, BQ2415X_REG_06,
				BQ2415X_MAX_VREG_MASK, val);	
}
#endif


static int bq2415x_set_safety_reg(struct bq2415x *bq, int volt, int curr)
{
	u8 ichg;
	u8 vchg;
	u8 val;

	pr_err("bq2415x_set_safety_reg curr:%d\n", curr);

	ichg = (curr * bq->cfg.sensor_mohm / 100 -  BQ2415X_MAX_ICHG_BASE) / BQ2415X_MAX_ICHG_LSB;

	ichg <<= BQ2415X_MAX_ICHG_SHIFT;
	
	vchg = (volt - BQ2415X_MAX_VREG_BASE) / BQ2415X_MAX_VREG_LSB;
	vchg <<= BQ2415X_MAX_VREG_SHIFT;
	
	val = ichg | vchg;

	return bq2415x_update_bits(bq, BQ2415X_REG_06,
				BQ2415X_MAX_VREG_MASK | BQ2415X_MAX_ICHG_MASK, val);	
	
}

static ssize_t bq2415x_show_registers(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct bq2415x *bq = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[100];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "bq2415x Reg");
	for (addr = 0x0; addr <= 0x06; addr++) {
		ret = bq2415x_read_byte(bq, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
					"Reg[%02X] = 0x%02X\n",	addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t bq2415x_store_registers(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct bq2415x *bq = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= 0x06)
		bq2415x_write_byte(bq, (unsigned char)reg, (unsigned char)val);

	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, bq2415x_show_registers,
						bq2415x_store_registers);

static struct attribute *bq2415x_attributes[] = {
	&dev_attr_registers.attr,
	NULL,
};

static const struct attribute_group bq2415x_attr_group = {
	.attrs = bq2415x_attributes,
};


static int bq2415x_parse_dt(struct device *dev, struct bq2415x *bq)
{
	int ret;
	struct device_node *np = dev->of_node;

	bq->charge_enabled = !(of_property_read_bool(np, "ti,charging-disabled"));

	bq->cfg.enable_term = of_property_read_bool(np, "ti,bq2415x,enable-term");

	ret = of_property_read_u32(np, "ti,bq2415x,current-sensor-mohm",
					&bq->cfg.sensor_mohm);
	if (ret)
		return ret;
	
	if (bq->cfg.sensor_mohm == 0) {
		pr_err("invalid sensor resistor value, use 68mohm by default\n");
		bq->cfg.sensor_mohm = 55;
	}
	pr_debug("sensor_mohm:0x%02X\n", bq->cfg.sensor_mohm);
	
	ret = of_property_read_u32(np, "ti,bq2415x,charge-voltage",
					&bq->cfg.chg_mv);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2415x,charge-current",
					&bq->cfg.chg_ma);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2415x,input-current-limit",
					&bq->cfg.icl_ma);
	if (ret)
		return ret;

	ret = of_property_read_u32(np, "ti,bq2415x,input-voltage-limit",
					&bq->cfg.ivl_mv);
	if (ret)
		return ret;
	
	ret = of_property_read_u32(np, "ti,bq2415x,safety-max-charge-voltage",
					&bq->cfg.safety_chg_mv);
	if (ret)
		return ret;
	
	ret = of_property_read_u32(np, "ti,bq2415x,safety-max-charge-current",
					&bq->cfg.safety_chg_ma);
	if (ret)
		return ret;
	
	ret = of_property_read_u32(np, "ti,bq2415x,vbatlow-volt",
					&bq->cfg.batlow_mv);

	ret = of_property_read_u32(np, "ti,bq2415x,term-current",
					&bq->cfg.iterm_ma);

	return ret;
}

static int bq2415x_detect_device(struct bq2415x *bq)
{
	int ret;
	u8 data;

	ret = bq2415x_read_byte(bq, BQ2415X_REG_03, &data);
	if (ret == 0) {
		bq->part_no = (data & BQ2415X_PN_MASK) >> BQ2415X_PN_SHIFT;
		bq->revision = (data & BQ2415X_REVISION_MASK) >> BQ2415X_REVISION_SHIFT;
		bq->vendor= (data & BQ2415X_VENDOR_MASK) >> BQ2415X_VENDOR_SHIFT;
	}

	pr_debug("bq2415x:part_no:0x%x revision:0x%x vendor|0x%x \n", 
		       bq->part_no,bq->revision,bq->vendor);
	return ret;
}



static int bq2415x_set_term_current(struct bq2415x *bq, int curr_ma)
{
	u8 ichg;
	int val = curr_ma * bq->cfg.sensor_mohm / 100;

	if(val < BQ2415X_ITERM_BASE) {
		val = BQ2415X_ITERM_BASE;
	}

	ichg = (val -  BQ2415X_ITERM_BASE) / BQ2415X_ITERM_LSB;

	ichg <<= BQ2415X_ITERM_SHIFT;
	
	pr_debug("bq2415x_set_term_current ichg:0x%02X\n", ichg);

	return bq2415x_update_bits(bq, BQ2415X_REG_04,
				BQ2415X_ITERM_MASK, ichg);
}

static int bq2415x_set_charge_profile(struct bq2415x *bq)
{
	int ret;

	pr_err("chg_mv:%d, chg_ma:%d, icl_ma:%d, ivl_mv:%d\n",
			bq->cfg.chg_mv, bq->cfg.chg_ma, bq->cfg.icl_ma, bq->cfg.ivl_mv);
			
	ret = bq2415x_set_chargevoltage(bq->chg_dev, bq->cfg.chg_mv * 1000);
	if (ret < 0) {
		pr_err("Failed to set charge voltage:%d\n", ret);
		return ret;
	}

	ret = bq2415x_set_chargecurrent(bq->chg_dev, 500 * 1000);
	if (ret < 0) {
		pr_err("Failed to set charge current:%d\n", ret);
		return ret;
	}

	ret = bq2415x_set_input_current_limit(bq->chg_dev, bq->cfg.icl_ma * 1000);
	if (ret < 0) {
		pr_err("Failed to set input current limit:%d\n", ret);
		return ret;
	}

	ret = bq2415x_set_input_volt_limit(bq->chg_dev, bq->cfg.ivl_mv * 1000);
	if (ret < 0) {
		pr_err("Failed to set input voltage limit:%d\n", ret);
		return ret;
	}
	return 0;
}

static int bq2415x_init_device(struct bq2415x *bq)
{
	int ret;

	bq->chg_mv = bq->cfg.chg_mv;
	bq->chg_ma = bq->cfg.chg_ma;
	bq->ivl_mv = bq->cfg.ivl_mv;
	bq->icl_ma = bq->cfg.icl_ma;


	/*safety register can only be written before other writes occur,
	  if lk code exist, it should write this register firstly before
	  write any other registers
	*/
	ret = bq2415x_set_safety_reg(bq, bq->cfg.safety_chg_mv, bq->cfg.safety_chg_ma);
	if (ret < 0)
		pr_err("Failed to set safety register:%d\n", ret);

	bq2415x_disable_low_charge(bq);
	ret = bq2415x_enable_term(bq, bq->cfg.enable_term);
	if (ret < 0)
		pr_err("Failed to %s termination:%d\n",
			bq->cfg.enable_term ? "enable" : "disable", ret);

	ret = bq2415x_set_vbatlow_volt(bq, bq->cfg.batlow_mv);
	if (ret < 0)
		pr_err("Failed to set vbatlow volt to %d,rc=%d\n",
					bq->cfg.batlow_mv, ret);
//Bug439500,zhaosidong.wt,MODIFY,20190423,set charge termination current 150mA
	bq->cfg.iterm_ma = 200;
	bq2415x_set_term_current(bq, bq->cfg.iterm_ma);
	
	bq2415x_set_charge_profile(bq);

	if (bq->charge_enabled)
		ret = bq2415x_enable_charger(bq);
	else
		ret = bq2415x_disable_charger(bq);

	if (ret < 0)
		pr_err("Failed to %s charger:%d\n",
			bq->charge_enabled ? "enable" : "disable", ret);
	return 0;
}

static void bq2415x_dump_regs(struct bq2415x *bq)
{
	int ret;
	u8 addr;
	u8 val;

	for (addr = 0x00; addr <= 0x06; addr++) {
		msleep(2);
		ret = bq2415x_read_byte(bq, addr, &val);
		if (!ret)
			pr_err("bq2415x_dump_regs Reg[%02X] = 0x%02X\n", addr, val);
	}

}

static void bq2415x_check_status(struct bq2415x *bq)
{
	int ret;
	u8 val = 0;
	
	ret = bq2415x_read_byte(bq, BQ2415X_REG_00, &val);
	if (!ret) {
		bq->charge_state = (val & BQ2415X_STAT_MASK)
							>> BQ2415X_STAT_SHIFT;
		bq->boost_mode = !!(val & BQ2415X_BOOST_MASK);
		bq->otg_pin_status = !!(val & BQ2415X_OTG_MASK);
		pr_err("Charge State:%s\n", chg_state_str[bq->charge_state]);
		pr_err("%s In boost mode\n", bq->boost_mode ? "":"Not");
		pr_err("otg pin:%s\n", bq->otg_pin_status ? "High" : "Low");
		pr_err("Fault Status:%s\n", bq->otg_online ?
					  boost_fault_state_str[bq->fault_status] 
					: chg_fault_state_str[bq->fault_status]);
		pr_err("otg online:%d\n", bq->otg_online);
		pr_err("charger online:%d\n",bq->chg_online);
		
		if (bq->fault_status != 0){
			bq->fault_status=0;
			if(bq->chg_online){
				bq2415x_enable_charger(bq);
			}

			if(bq->otg_online){
				bq2415x_enable_otg(bq->chg_dev, true);
			}
		}

		ret = bq2415x_read_byte(bq, BQ2415X_REG_05, &val);
		if (!ret) {
			pr_err("DPM Status:%d\n", !!(val & BQ2415X_DPM_STATUS_MASK));
			pr_err("CD Status:%d\n", !!(val & BQ2415X_CD_STATUS_MASK));
		}
	}
}

static void bq2415x_monitor_workfunc(struct work_struct *work)
{
	struct bq2415x *bq = container_of(work, struct bq2415x, monitor_work.work);

	bq2415x_dump_regs(bq);
	bq2415x_check_status(bq);
	
	schedule_delayed_work(&bq->monitor_work, 5 * HZ);
}

/*
 *  Charger device interface
 */
static int bq2415x_plug_in(struct charger_device *chg_dev)
{
	int ret;
	
	ret = bq2415x_charging(chg_dev, true);
	if (!ret)
		pr_err("Failed to enable charging:%d\n", ret);
	
	return ret;
	
}

static int bq2415x_plug_out(struct charger_device *chg_dev)
{
	int ret;
	
	ret = bq2415x_charging(chg_dev, false);
	if (!ret)
		pr_err("Failed to enable charging:%d\n", ret);
	
	return ret;
}


static int bq2415x_dump_register(struct charger_device *chg_dev)
{

	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	bq2415x_dump_regs(bq);
	
	return 0;
}


static int bq2415x_charging(struct charger_device *chg_dev, bool enable)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = bq2415x_enable_charger(bq);
	else
		ret = bq2415x_disable_charger(bq);

	pr_err("bq2415x_charging%s charger %s\n", enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed");
	
	ret = bq2415x_read_byte(bq, BQ2415X_REG_01, &val);

	if (!ret)
		bq->charge_enabled = !!(val & BQ2415X_CHARGE_ENABLE_MASK);
	
	return ret;
}

static int bq2415x_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	
	*en = bq->charge_enabled;
	
	return 0;
}

static int bq2415x_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;
	
	ret = bq2415x_read_byte(bq, BQ2415X_REG_00, &val);
	if (!ret) {
		val = val & BQ2415X_STAT_MASK;
		val = val >> BQ2415X_STAT_SHIFT;
		*done = (val == BQ2415X_STAT_CHGDONE);	
	}
	
	return ret;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList, unsigned int number,
					   unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = KAL_TRUE;
	else
		max_value_in_last_element = KAL_FALSE;

	if (max_value_in_last_element == KAL_TRUE) {
		for (i = (number - 1); i != 0; i--) {
			if (pList[i] <= level)
				return pList[i];
		}

		pr_err("Can't find closest level \n");
		return pList[0];
	}

	for (i = 0; i < number; i++) {
		if (pList[i] <= level)
			return pList[i];
	}

	pr_err("Can't find closest level \n");
	return pList[number - 1];

}

static unsigned int charging_parameter_to_value(const unsigned int *parameter,
					 const unsigned int array_size, const unsigned int val)
{
	unsigned int i;

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
        }

	return 0;
}

static int bq2415x_set_chargecurrent(struct charger_device *chg_dev, u32 curr)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 ichg;
	int val;

	curr /= 1000; /*to mA */
	pr_debug("bq2415x_set_chargecurrent :%ld\n", curr);
	val = curr * bq->cfg.sensor_mohm / 100;
	if(val < BQ2415X_ICHG_BASE) {
		val = BQ2415X_ICHG_BASE;
	}

	ichg = (val -  BQ2415X_ICHG_BASE) / BQ2415X_ICHG_LSB;
	ichg <<= BQ2415X_ICHG_SHIFT;

	return bq2415x_update_bits(bq, BQ2415X_REG_04,
				BQ2415X_ICHG_MASK, ichg);

}

static int bq2415x_get_chargecurrent(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;
	int ret;
	int ichg;

	ret = bq2415x_read_byte(bq, BQ2415X_REG_04, &val);
	if (!ret) {
		ichg = ((u32)(val & BQ2415X_ICHG_MASK ) >> BQ2415X_ICHG_SHIFT) * BQ2415X_ICHG_LSB;
		ichg +=  BQ2415X_ICHG_BASE;
		ichg = ichg * 100 / bq->cfg.sensor_mohm;

		*curr = ichg * 1000; /*to uA*/
	}

	return ret;
}

static int bq2415x_set_chargevoltage(struct charger_device *chg_dev, u32 volt)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;

	volt /= 1000; /*to mV*/
	if(volt < BQ2415X_VREG_BASE) {
		volt = BQ2415X_VREG_BASE;
	}
	val = (volt - BQ2415X_VREG_BASE)/BQ2415X_VREG_LSB;
	val <<= BQ2415X_VREG_SHIFT;
	
	pr_debug("bq2415x_set_chargevoltage val:0x%02X\n", val);
		
	return bq2415x_update_bits(bq, BQ2415X_REG_02,
				BQ2415X_VREG_MASK, val);
}

static int bq2415x_get_chargevoltage(struct charger_device *chg_dev, u32 *cv)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;
	int ret;
	int volt;
	
	ret = bq2415x_read_byte(bq, BQ2415X_REG_02, &val);
	if (!ret) {
		volt = val & BQ2415X_VREG_MASK;
		volt = (volt >> BQ2415X_VREG_SHIFT) * BQ2415X_VREG_LSB;
		volt = volt + BQ2415X_VREG_BASE;
		*cv = volt * 1000; /*to uV*/
	}
	
	return ret;
}

static int bq2415x_set_input_volt_limit(struct charger_device *chg_dev, u32 volt)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;

	volt /= 1000; /*to mV*/
	val = (volt - BQ2415X_VSREG_BASE) / BQ2415X_VSREG_LSB;
	val <<= BQ2415X_VSREG_SHIFT;

	pr_debug("bq2415x_set_input_volt_limit val:0x%02X\n", val);
	
	return bq2415x_update_bits(bq, BQ2415X_REG_05,
				BQ2415X_VSREG_MASK, val);
}



static int bq2415x_set_input_current_limit(struct charger_device *chg_dev, u32 curr)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;

	pr_debug("bq2415x_set_input_current_limit curr:%ld\n", curr);

	curr /= 1000 ;

	if (curr <= 100)
		val = BQ2415X_IINLIM_100MA;
	else if (curr <= 500)
		val = BQ2415X_IINLIM_500MA;
	else if (curr <= 800)
		val = BQ2415X_IINLIM_800MA;
	else
		val = BQ2415X_IINLIM_NOLIM;

	val <<= BQ2415X_IINLIM_SHIFT;

	return bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_IINLIM_MASK, val);
}


static int bq2415x_get_input_current_limit(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;
	int ret;
	int ilim;

	ret = bq2415x_read_byte(bq, BQ2415X_REG_01, &val);
	if (!ret) {
		val = val & BQ2415X_IINLIM_MASK;
		val = val >> BQ2415X_IINLIM_SHIFT;
		if (val == BQ2415X_IINLIM_100MA)
			ilim = 100;
		else if (val == BQ2415X_IINLIM_500MA)
			ilim = 500;
		else if (val == BQ2415X_IINLIM_800MA)
			ilim = 800;
		else
			ilim = 0;

		*curr = ilim * 1000; /*to uA*/
	}

	return ret;
}


static int charger_reset_watchdog_timer(struct bq2415x *bq)
{
	u8 val = BQ2415X_TMR_RST << BQ2415X_TMR_RST_SHIFT;
	pr_err("charger ic reset watchdog\n");
	return bq2415x_update_bits(bq, BQ2415X_REG_00,
				BQ2415X_TMR_RST_MASK, val);
}



static int bq2415x_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	bq->otg_online = en;

	if (en){
		val = BQ2415X_BOOST_MODE;
	} else {
		val = BQ2415X_CHARGER_MODE;
	}

	val <<= BQ2415X_OPA_MODE_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_OPA_MODE_MASK, val);

	return ret;
}

static int bq2415x_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}
	return 0;
}

void bq2415x_charger_set_wakelock(struct charger_device *chg_dev,bool chg_online)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);

	bq->chg_online = chg_online;

}

static struct charger_ops bq2415x_chg_ops = {
	/* Normal charging */
	.plug_in = bq2415x_plug_in,
	.plug_out = bq2415x_plug_out,
	.dump_registers = bq2415x_dump_register,
	.enable = bq2415x_charging,
	.is_enabled = bq2415x_is_charging_enable,
	.get_charging_current = bq2415x_get_chargecurrent,
	.set_charging_current = bq2415x_set_chargecurrent,
	.get_input_current = bq2415x_get_input_current_limit,
	.set_input_current = bq2415x_set_input_current_limit,
	.get_constant_voltage = bq2415x_get_chargevoltage,
	.set_constant_voltage = bq2415x_set_chargevoltage,
	.kick_wdt = NULL,
	.set_mivr = NULL,
	.is_charging_done = bq2415x_is_charging_done,
	.get_min_charging_current = NULL,

	/* Safety timer */
	.enable_safety_timer = NULL,
	.is_safety_timer_enabled = NULL,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = bq2415x_enable_otg,
	.set_boost_current_limit = NULL,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	//.set_ta20_reset = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	.charger_set_wakelock = bq2415x_charger_set_wakelock,
	.event = bq2415x_do_event,
};


/************************bq2415x code is end**********************/




/**************ETA6937 code start from here***************/
static int eta6937_enable_charger(struct bq2415x *bq)
{
	int ret;
	u8 val ;

	eta6937_reinit(bq);

	val = BQ2415X_CHARGE_ENABLE << BQ2415X_CHARGE_ENABLE_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_CHARGE_ENABLE_MASK, val);

	val = BQ2415X_HZ_MODE_DISABLE << BQ2415X_HZ_MODE_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_HZ_MODE_MASK, val);

	val = 0 << BQ2415X_OPA_MODE_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_OPA_MODE_MASK, val);

	val = BQ2415X_CHARGE_ENABLE << BQ2415X_CHARGE_ENABLE_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_CHARGE_ENABLE_MASK, val);

	val = BQ2415X_TMR_RST << BQ2415X_TMR_RST_SHIFT;

	ret =bq2415x_update_bits(bq, BQ2415X_REG_00,
				BQ2415X_TMR_RST_MASK, val);

	return ret;
}

static int eta6937_disable_charger(struct bq2415x *bq)
{
	int ret;
	u8 val;

	val = BQ2415X_CHARGE_DISABLE << BQ2415X_CHARGE_ENABLE_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_CHARGE_ENABLE_MASK, val);

	//val = BQ2415X_HZ_MODE_ENABLE << BQ2415X_HZ_MODE_SHIFT;

	//ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
	//			BQ2415X_HZ_MODE_MASK, val);
	return ret;
}

static int eta6937_charging(struct charger_device *chg_dev, bool enable)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;

	if (enable)
		ret = eta6937_enable_charger(bq);
	else
		ret = eta6937_disable_charger(bq);

	pr_err("eta6937_charging%s charger %s\n", enable ? "enable" : "disable",
				  !ret ? "successfully" : "failed");

	ret = bq2415x_read_byte(bq, BQ2415X_REG_01, &val);

	if (!ret)
		bq->charge_enabled = !!(val & BQ2415X_CHARGE_ENABLE_MASK);

	return ret;
}

static int eta6937_plug_in(struct charger_device *chg_dev)
{
	int ret;

	ret = eta6937_charging(chg_dev, true);
	if (!ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;

}

static int eta6937_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = eta6937_charging(chg_dev, false);
	if (!ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int eta6937_set_safety_reg(struct bq2415x *bq, int volt, int curr)
{
	u8 ichg;
	u8 vchg;
	u8 val;

	pr_err("eta6937_set_safety_reg curr:%d volt:%d\n", curr, volt);
	ichg = (curr - 550) / 200;
	ichg <<= BQ2415X_MAX_ICHG_SHIFT;

	vchg = (volt - 4200) / 20;
	vchg <<= BQ2415X_MAX_VREG_SHIFT;

	val = ichg | vchg;

	return bq2415x_update_bits(bq, BQ2415X_REG_06,
				BQ2415X_MAX_VREG_MASK | BQ2415X_MAX_ICHG_MASK, val);

}

/*eta6937 REG04 ITERM[2:0]*/
const u32 ETA6937_ITERM[] = {
	50, 100, 150, 200,
	250, 300, 350, 400
};

static int eta6937_set_term_current(struct bq2415x *bq, int curr_ma)
{
	u32 set_term_current;
	u32 array_size;
	u32 register_value;

	pr_debug("eta6937_set_term_current :%ld\n", curr_ma);

	array_size = ARRAY_SIZE(ETA6937_ITERM);
	set_term_current = bmt_find_closest_level(ETA6937_ITERM, array_size, curr_ma);
	register_value = charging_parameter_to_value(ETA6937_ITERM, array_size, set_term_current);
	register_value <<= BQ2415X_ITERM_SHIFT;

	return bq2415x_update_bits(bq, BQ2415X_REG_04,
				BQ2415X_ITERM_MASK, register_value);
}

static int eta6937_reinit(struct bq2415x *bq)
{
	int ret;

	ret = eta6937_set_safety_reg(bq, bq->cfg.safety_chg_mv, bq->cfg.safety_chg_ma);
	if (ret < 0)
		pr_err("Failed to set safety register:%d\n", ret);

	ret = bq2415x_enable_term(bq, bq->cfg.enable_term);
	if (ret < 0)
		pr_err("Failed to %s termination:%d\n",
			bq->cfg.enable_term ? "enable" : "disable", ret);

	ret = bq2415x_set_vbatlow_volt(bq, bq->cfg.batlow_mv);
	if (ret < 0)
		pr_err("Failed to set vbatlow volt to %d,rc=%d\n",
					bq->cfg.batlow_mv, ret);

	eta6937_set_term_current(bq, bq->cfg.iterm_ma);

	ret = bq2415x_set_input_volt_limit(bq->chg_dev, bq->cfg.ivl_mv * 1000);
	if (ret < 0) {
		pr_err("Failed to set input voltage limit:%d\n", ret);
	}

	return 0;
}

static int eta6937_set_chargevoltage(struct charger_device *chg_dev, u32 volt)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;

	if(4400000 == volt)
		volt = 4420000;

	volt /= 1000; /*to mV*/
	val = (volt - BQ2415X_VREG_BASE)/BQ2415X_VREG_LSB;
	val <<= BQ2415X_VREG_SHIFT;

	pr_debug("eta6937_set_chargevoltage val:0x%02X\n", val);

	return bq2415x_update_bits(bq, BQ2415X_REG_02,
				BQ2415X_VREG_MASK, val);
}


/*eta6937 REG04 ICHG[6:0]*/
const u32 ETA6937_ICHG[] = {
	55000, 65000, 75000, 85000,
	95000, 105000, 115000, 125000,
	135000, 145000, 155000, 165000,
	175000, 185000, 195000, 205000,
	215000, 225000, 235000
};
void eta6937_set_io_level(struct bq2415x *bq,u8 reg)
{
	int ret;
	u8 val = reg << BQ2415X_LOW_CHG_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_05,
				BQ2415X_LOW_CHG_MASK, val);
}

void eta6937_set_iocharge(struct bq2415x *bq,u8 reg)
{
	int ret;
	u8 val1=reg&0x07;
	u8 val2=(reg>>3)&0x03;

	val1 = val1 << BQ2415X_ICHG_SHIFT;
	ret = bq2415x_update_bits(bq, BQ2415X_REG_04,
			BQ2415X_ICHG_MASK, val1);

	val2 = val2 << ETA6937_ICHG_SHIFT;
	ret = bq2415x_update_bits(bq, BQ2415X_REG_05,
			ETA6937_ICHG_MASK, val2);
}

static int eta6937_set_chargecurrent(struct charger_device *chg_dev, u32 ichg)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u32 set_chr_current;
	u32 array_size;
	u32 register_value;

	pr_debug("eta6937_set_chargecurrent: %ld\n", ichg);

	ichg /= 10;

	if(ichg <= 35000) {
		eta6937_set_io_level(bq,1);
	} else {
		array_size = ARRAY_SIZE(ETA6937_ICHG);
		set_chr_current = bmt_find_closest_level(ETA6937_ICHG, array_size, ichg);
		register_value = charging_parameter_to_value(ETA6937_ICHG, array_size, set_chr_current);
		pr_err("eta6937_set_ichg:  %d\n",register_value);

		eta6937_set_io_level(bq,0);
		eta6937_set_iocharge(bq,register_value);
	}

	return 0;
}

static int eta6937_enable_input_current_selection(struct bq2415x *bq, bool en)
{
	int ret;
	u8 val;
        
	if(en){ 
        	val= BQ2415X_EN_ILIM_ENABLE;
	}else{
		val= BQ2415X_EN_ILIM_DISABLE;
	}
	
	val<<= BQ2415X_EN_ILIM_SHIFT;
	ret = bq2415x_update_bits(bq, BQ2415X_REG_07,
				BQ2415X_EN_ILIM_MASK, val);
	return ret;
}

static int eta6937_get_input_current_limit(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val,input_sel,input_sel_iinlim;
	int ret;
	int ilim;
	
	ret = bq2415x_read_byte(bq, BQ2415X_REG_07, &input_sel);
	if(!ret){
		input_sel_iinlim=input_sel;
		input_sel = input_sel & BQ2415X_EN_ILIM_MASK;
		input_sel = input_sel >> BQ2415X_EN_ILIM_SHIFT;
		if(input_sel){
			input_sel_iinlim = input_sel_iinlim & BQ2415X_IINLIM_07_MASK;
			input_sel_iinlim = input_sel_iinlim >> BQ2415X_IINLIM_07_SHIFT;
			if (input_sel_iinlim == BQ2415X_IINLIM_07_300MA)
				ilim = 300;
			else if (input_sel_iinlim == BQ2415X_IINLIM_07_500MA)
				ilim = 500;
			else if (input_sel_iinlim ==  BQ2415X_IINLIM_07_800MA)
				ilim = 800;
			else if (input_sel_iinlim ==   BQ2415X_IINLIM_07_1200MA)
				ilim = 1200;
			else if (input_sel_iinlim ==   BQ2415X_IINLIM_07_1500MA)
				ilim = 1500;
			else if (input_sel_iinlim ==   BQ2415X_IINLIM_07_2000MA)
				ilim = 2000;
			else if (input_sel_iinlim ==   BQ2415X_IINLIM_07_3000MA)
				ilim = 3000;
			else if (input_sel_iinlim ==   BQ2415X_IINLIM_07_5000MA)
				ilim = 5000;
			else
				ilim = 0;

			*curr = ilim * 1000; /*to uA*/
	
			return ret;
		}
	}

	ret = bq2415x_read_byte(bq, BQ2415X_REG_01, &val);
	if (!ret) {
		val = val & BQ2415X_IINLIM_MASK;
		val = val >> BQ2415X_IINLIM_SHIFT;
		if (val == BQ2415X_IINLIM_100MA)
			ilim = 100;
		else if (val == BQ2415X_IINLIM_500MA)
			ilim = 500;
		else if (val == BQ2415X_IINLIM_800MA)
			ilim = 800;
		else
			ilim = 0;

		*curr = ilim * 1000; /*to uA*/
	}

	return ret;
}

/*
eta6937_set_input_current_limit
1.set reg7[3]  ----   1/0  enable/disable input current selection
2.set reg1[7:6]  ---- select input current level
*/
static int eta6937_set_input_current_limit(struct charger_device *chg_dev, u32 curr)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;

	pr_debug("eta6937_set_input_current_limit curr:%ld\n", curr);

	curr /= 1000 ;
       
	if(curr <=100){
		eta6937_enable_input_current_selection(bq,0);

		val = BQ2415X_IINLIM_100MA;
		val <<= BQ2415X_IINLIM_SHIFT;
		return bq2415x_update_bits(bq, BQ2415X_REG_01,
					 BQ2415X_IINLIM_MASK, val);
		
	}else{
		eta6937_enable_input_current_selection(bq,1);
	}

	if (curr <= 300)
		val = BQ2415X_IINLIM_07_300MA;
	else if (curr <= 500)
		val = BQ2415X_IINLIM_07_500MA;
	else if (curr <= 800)
		val = BQ2415X_IINLIM_07_800MA;
	else if (curr <= 1200)
		val = BQ2415X_IINLIM_07_1200MA;
	else
		val = BQ2415X_IINLIM_07_1500MA;

	val <<= BQ2415X_IINLIM_07_SHIFT;

	return bq2415x_update_bits(bq, BQ2415X_REG_07,
				BQ2415X_IINLIM_07_MASK, val);
}

static int eta6937_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	bq->otg_online = en;

	if(en) {
            val = BQ2415X_HZ_MODE_DISABLE << BQ2415X_HZ_MODE_SHIFT;
            ret = bq2415x_update_bits(bq, BQ2415X_REG_01,BQ2415X_HZ_MODE_MASK, val);

		eta6937_reinit(bq);
		val = BQ2415X_BOOST_MODE << BQ2415X_OPA_MODE_SHIFT;
		ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
			BQ2415X_OPA_MODE_MASK, val);
		bq->rst_timeout = true;
		wake_up(&bq->wait_que);
	} else {
		val = BQ2415X_CHARGER_MODE << BQ2415X_OPA_MODE_SHIFT;
		ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
			BQ2415X_OPA_MODE_MASK, val);
	}

	return ret;
}

static void eta6937_dump_regs(struct bq2415x *bq)
{
	int ret;
	u8 addr;
	u8 val;

	for (addr = 0x00; addr <= 0x07; addr++) {
		msleep(2);
		ret = bq2415x_read_byte(bq, addr, &val);
		if (!ret)
			pr_err("eta6937_dump_regs Reg[%02X] = 0x%02X\n", addr, val);
	}

}

static int eta6937_dump_register(struct charger_device *chg_dev)
{

	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	eta6937_dump_regs(bq);
	
	return 0;
}

static void eta6937_check_status(struct bq2415x *bq)
{
	int ret;
	u8 val = 0;

	ret = bq2415x_read_byte(bq, BQ2415X_REG_00, &val);
	if (!ret) {
		bq->charge_state = (val & BQ2415X_STAT_MASK)
							>> BQ2415X_STAT_SHIFT;
		bq->boost_mode = !!(val & BQ2415X_BOOST_MASK);
		bq->otg_pin_status = !!(val & BQ2415X_OTG_MASK);
		bq->fault_status = (val & BQ2415X_FAULT_MASK)
							>> BQ2415X_FAULT_SHIFT;
		pr_err("Charge State:%s\n", chg_state_str[bq->charge_state]);
		pr_err("%s In boost mode\n", bq->boost_mode ? "":"Not");
		pr_err("otg pin:%s\n", bq->otg_pin_status ? "High" : "Low");
		pr_err("Fault Status:%s\n", bq->boost_mode ?
					  boost_fault_state_str[bq->fault_status]
					: chg_fault_state_str[bq->fault_status]);
		pr_err("otg online:%d\n", bq->otg_online);
		pr_err("charger online:%d\n",bq->chg_online);

		if (bq->charge_state == BQ2415X_STAT_FAULT){
			if(bq->chg_online){
				eta6937_enable_charger(bq);
			}

			if(bq->otg_online){
				eta6937_enable_otg(bq->chg_dev, true);
			}
		}

		ret = bq2415x_read_byte(bq, BQ2415X_REG_05, &val);
		if (!ret) {
			pr_err("DPM Status:%d\n", !!(val & BQ2415X_DPM_STATUS_MASK));
			pr_err("CD Status:%d\n", !!(val & BQ2415X_CD_STATUS_MASK));
		}
	}
}

static void eta6937_monitor_workfunc(struct work_struct *work)
{
	struct bq2415x *bq = container_of(work, struct bq2415x, monitor_work.work);

	eta6937_dump_regs(bq);
	eta6937_check_status(bq);

	schedule_delayed_work(&bq->monitor_work, 5 * HZ);
}

static int eta6937_set_charge_profile(struct bq2415x *bq)
{
	int ret;

	pr_err("chg_mv:%d, chg_ma:%d, icl_ma:%d, ivl_mv:%d\n",
			bq->cfg.chg_mv, bq->cfg.chg_ma, bq->cfg.icl_ma, bq->cfg.ivl_mv);

	ret = eta6937_set_chargevoltage(bq->chg_dev, bq->cfg.chg_mv * 1000);
	if (ret < 0) {
		pr_err("Failed to set charge voltage:%d\n", ret);
		return ret;
	}

	ret = eta6937_set_chargecurrent(bq->chg_dev, 500 * 1000);
	if (ret < 0) {
		pr_err("Failed to set charge current:%d\n", ret);
		return ret;
	}

	ret = eta6937_set_input_current_limit(bq->chg_dev, bq->cfg.icl_ma * 1000);
	if (ret < 0) {
		pr_err("Failed to set input current limit:%d\n", ret);
		return ret;
	}

	ret = bq2415x_set_input_volt_limit(bq->chg_dev, bq->cfg.ivl_mv * 1000);
	if (ret < 0) {
		pr_err("Failed to set input voltage limit:%d\n", ret);
		return ret;
	}
	return 0;
}

static int eta6937_init_device(struct bq2415x *bq)
{
	int ret;

	bq->chg_mv = bq->cfg.chg_mv;
	bq->chg_ma = bq->cfg.chg_ma;
	bq->ivl_mv = bq->cfg.ivl_mv;
	bq->icl_ma = bq->cfg.icl_ma;


	/*safety register can only be written before other writes occur,
	  if lk code exist, it should write this register firstly before
	  write any other registers
	*/
	ret = eta6937_set_safety_reg(bq, bq->cfg.safety_chg_mv, bq->cfg.safety_chg_ma);
	if (ret < 0)
		pr_err("Failed to set safety register:%d\n", ret);

	bq2415x_disable_low_charge(bq);
	ret = bq2415x_enable_term(bq, bq->cfg.enable_term);
	if (ret < 0)
		pr_err("Failed to %s termination:%d\n",
			bq->cfg.enable_term ? "enable" : "disable", ret);

	ret = bq2415x_set_vbatlow_volt(bq, bq->cfg.batlow_mv);
	if (ret < 0)
		pr_err("Failed to set vbatlow volt to %d,rc=%d\n",
					bq->cfg.batlow_mv, ret);

	eta6937_set_term_current(bq, bq->cfg.iterm_ma);

	eta6937_set_charge_profile(bq);

	if (bq->charge_enabled)
		ret = eta6937_enable_charger(bq);
	else
		ret = eta6937_disable_charger(bq);

	if (ret < 0)
		pr_err("Failed to %s charger:%d\n",
			bq->charge_enabled ? "enable" : "disable", ret);
	return 0;
}

static void charger_start_timer(struct bq2415x *bq)
{
	ktime_t ktime = ktime_set(bq->polling_interval, 0);

	//pr_debug("eta6937 start timer\n");
	hrtimer_start(&bq->kthread_timer, ktime,
				HRTIMER_MODE_REL);
}

void charger_set_wakelock(struct charger_device *chg_dev,bool chg_online)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);

	pr_debug("charger_set_wakelock: %d\n",chg_online);

	if(chg_online){
		__pm_stay_awake(&bq->eta_charger_lock);
	}else{
		__pm_relax(&bq->eta_charger_lock);
	}

	bq->chg_online = chg_online;
	bq->rst_timeout = true;
	wake_up(&bq->wait_que);
}

static int charger_rst_thread(void *arg)
{
	struct bq2415x *bq = arg;

	while (1) {
		pr_debug("charger_rst_thread\n");
		wait_event(bq->wait_que,
			(bq->rst_timeout == true));
		charger_reset_watchdog_timer(bq);
		bq->rst_timeout = false;

	    if((bq->chg_online)||(bq->otg_online)){
	        charger_start_timer(bq);
		}
	}

	return 0;
}

enum hrtimer_restart reset_kthread_hrtimer_func(struct hrtimer *timer)
{
	struct bq2415x *bq  =
	container_of(timer, struct bq2415x, kthread_timer);
	bq->rst_timeout = true;
	wake_up(&bq->wait_que);
	return HRTIMER_NORESTART;
}

static struct charger_ops eta6937_chg_ops = {
	/* Normal charging */
	.plug_in = eta6937_plug_in,
	.plug_out = eta6937_plug_out,
	.dump_registers = eta6937_dump_register,
	.enable = eta6937_charging,
	.is_enabled = bq2415x_is_charging_enable,
	.get_charging_current = bq2415x_get_chargecurrent,
	.set_charging_current = eta6937_set_chargecurrent,
	.get_input_current = eta6937_get_input_current_limit,
	.set_input_current = eta6937_set_input_current_limit,
	.get_constant_voltage = bq2415x_get_chargevoltage,
	.set_constant_voltage = eta6937_set_chargevoltage,
	.kick_wdt = NULL,
	.set_mivr = NULL,
	.is_charging_done = bq2415x_is_charging_done,
	.get_min_charging_current = NULL,

	/* Safety timer */
	.enable_safety_timer = NULL,
	.is_safety_timer_enabled = NULL,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = eta6937_enable_otg,
	.set_boost_current_limit = NULL,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	//.set_ta20_reset = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	.charger_set_wakelock = charger_set_wakelock,
	.event = bq2415x_do_event,
};

/*****************ETA6937 code is end*****************/




/**************HL7057 code start from here***************/
static int hl7057_update_bits(struct bq2415x *bq, u8 reg,
					u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	if (bq->skip_reads || bq->skip_writes)
		return 0;

	mutex_lock(&bq->i2c_rw_lock);
	ret = __bq2415x_read_reg(bq, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	mask |= 0x80;
	tmp &= ~mask;
	tmp |= data & mask;

	ret = __bq2415x_write_reg(bq, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&bq->i2c_rw_lock);
	return ret;
}

static int hl7057_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	bq->otg_online = en;

	if (en) {
		bq->rst_timeout = true;
		wake_up(&bq->wait_que);
		val = BQ2415X_BOOST_MODE;
	} else {
		val = BQ2415X_CHARGER_MODE;
	}

	val <<= BQ2415X_OPA_MODE_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_01,
				BQ2415X_OPA_MODE_MASK, val);

	return ret;
}


const u32 HL7057_MICHG[] = {
	65000, 85000, 115000, 145000,
	165000, 185000, 215000, 245000
};
/*
hl7057_set_safety_reg
only write once,if safety register is already written in LK, kernel write it invailable
*/
static int hl7057_set_safety_reg(struct bq2415x *bq, int volt, int curr)
{
	u32 ichg;
	u32 vchg;
	u32 array_size;
	u32 set_chr_current;
	u32 val;

	curr*=100;
	pr_err("hl7057_set_safety_reg curr:%d\n", curr);
	array_size = ARRAY_SIZE(HL7057_MICHG);
	set_chr_current = bmt_find_closest_level(HL7057_MICHG, array_size, curr);
	ichg = charging_parameter_to_value(HL7057_MICHG, array_size, set_chr_current);
	ichg <<= BQ2415X_MAX_ICHG_SHIFT;

	vchg = (volt - BQ2415X_MAX_VREG_BASE) / BQ2415X_MAX_VREG_LSB;
	vchg <<= BQ2415X_MAX_VREG_SHIFT;
	val = ichg | vchg;

	return bq2415x_update_bits(bq, BQ2415X_REG_06,
				HL7057_MAX_ICHG_MASK | BQ2415X_MAX_VREG_MASK, val);
}

void hl7057_set_io_level(struct bq2415x *bq,u8 reg)
{
	int ret;
	u8 val = reg << BQ2415X_LOW_CHG_SHIFT;

	ret = bq2415x_update_bits(bq, BQ2415X_REG_05,
				BQ2415X_LOW_CHG_MASK, val);
}

void hl7057_set_iocharge(struct bq2415x *bq,u8 reg)
{
    int ret;
	u8 val;
	val = reg << HL7057_ICHG_SHIFT;
	ret = hl7057_update_bits(bq, BQ2415X_REG_04,
			HL7057_ICHG_MASK, val);
}

const u32 HL7057_ICHG[] = {
	55000, 65000, 75000, 85000,
	105000, 115000, 135000, 145000,
	155000, 165000, 175000, 185000,
	205000, 215000, 235000, 245000
};
/*
hl7057_set_chargecurrent
1.set reg5[5]   ---- 0 is low current selection, 1 is high current selection
2.set reg4[6:3]  ---- charge current select, based on  arrary HL7057_ICHG
*/

static int hl7057_set_chargecurrent(struct charger_device *chg_dev, u32 ichg)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u32 set_chr_current;
	u32 array_size;
	u32 register_value;

	pr_debug("hl7057_set_ichg : %ld\n", ichg);

	ichg /= 10;

	if(ichg <= 35000) {
		hl7057_set_io_level(bq,1);
	} else {
		array_size = ARRAY_SIZE(HL7057_ICHG);
		set_chr_current = bmt_find_closest_level(HL7057_ICHG, array_size, ichg);
		register_value = charging_parameter_to_value(HL7057_ICHG, array_size, set_chr_current);
		hl7057_set_io_level(bq,0);
		hl7057_set_iocharge(bq,register_value);
	}

	return 0;
}

static int hl7057_get_chargecurrent(struct charger_device *chg_dev, u32 *curr)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;
	int ret;

	ret = bq2415x_read_byte(bq, BQ2415X_REG_04, &val);
	if (!ret) {
		val &= HL7057_ICHG_MASK;
		val >>= HL7057_ICHG_SHIFT;
		val = HL7057_ICHG[val] * 10;
		pr_debug("hl7057_get_chargecurrent:%ld", val);
	}

	return ret;
}

static int hl7057_get_input_current_limit(struct charger_device *chg_dev,u32 *curr)
{
	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	u8 val;
	int ret;
	int ilim;

	ret = bq2415x_read_byte(bq, BQ2415X_REG_01, &val);
	if(!ret) {
		val = val & BQ2415X_IINLIM_MASK;
		val = val >> BQ2415X_IINLIM_SHIFT;
		if (val == BQ2415X_IINLIM_100MA)
			ilim = 100;
		else if (val == BQ2415X_IINLIM_500MA)
			ilim = 500;
		else if (val == BQ2415X_IINLIM_800MA)
			ilim = 800;
		else
			ilim = 0;
		*curr = ilim * 1000; /*to uA*/
		pr_debug("hl7057_get_input_current_limit:%ld\n", *curr);
	}

	return ret;
}

static int hl7057_set_term_current(struct bq2415x *bq, int curr_ma)
{
	u8 ichg;
//Bug439500,zhaosidong.wt,MODIFY,20190423,set charge termination current 150mA
	ichg = (curr_ma - 50) / 50;

	ichg <<= BQ2415X_ITERM_SHIFT;

	pr_debug("hl7057_set_term_current ichg:0x%02X\n", ichg);

	return hl7057_update_bits(bq, BQ2415X_REG_04,
				BQ2415X_ITERM_MASK, ichg);
}

static int hl7057_set_charge_profile(struct bq2415x *bq)
{
	int ret;

	pr_err("chg_mv:%d, chg_ma:%d, icl_ma:%d, ivl_mv:%d\n",
			bq->cfg.chg_mv, bq->cfg.chg_ma, bq->cfg.icl_ma, bq->cfg.ivl_mv);

	ret = bq2415x_set_chargevoltage(bq->chg_dev, bq->cfg.chg_mv * 1000);
	if (ret < 0) {
		pr_err("Failed to set charge voltage:%d\n", ret);
		return ret;
	}

	ret = hl7057_set_chargecurrent(bq->chg_dev, 500 * 1000);
	if (ret < 0) {
		pr_err("Failed to set charge current:%d\n", ret);
		return ret;
	}

	ret = bq2415x_set_input_current_limit(bq->chg_dev, bq->cfg.icl_ma * 1000);
	if (ret < 0) {
		pr_err("Failed to set input current limit:%d\n", ret);
		return ret;
	}

	ret = bq2415x_set_input_volt_limit(bq->chg_dev, bq->cfg.ivl_mv * 1000);
	if (ret < 0) {
		pr_err("Failed to set input voltage limit:%d\n", ret);
		return ret;
	}
	return 0;
}

static void hl7057_dump_regs(struct bq2415x *bq)
{
	int ret;
	u8 addr;
	u8 val;

	for (addr = 0x00; addr <= 0x06; addr++) {
		msleep(2);
		ret = bq2415x_read_byte(bq, addr, &val);
		if (!ret)
			pr_err("hl7057_dump_regs Reg[%02X] = 0x%02X\n", addr, val);
	}

	ret = bq2415x_read_byte(bq, 0x0a, &val);
	if (!ret)
		pr_err("hl7057_dump_regs Reg[0x0a] = 0x%02X\n", val);

}

static int hl7057_dump_register(struct charger_device *chg_dev)
{

	struct bq2415x *bq = dev_get_drvdata(&chg_dev->dev);
	hl7057_dump_regs(bq);
	
	return 0;
}

static void hl7057_check_status(struct bq2415x *bq)
{
	int ret;
	u8 val = 0;
	
	ret = bq2415x_read_byte(bq, BQ2415X_REG_00, &val);
	if (!ret) {
		bq->charge_state = (val & BQ2415X_STAT_MASK)
							>> BQ2415X_STAT_SHIFT;
		bq->boost_mode = !!(val & BQ2415X_BOOST_MASK);
		bq->otg_pin_status = !!(val & BQ2415X_OTG_MASK);
		pr_err("Charge State:%s\n", chg_state_str[bq->charge_state]);
		pr_err("%s In boost mode\n", bq->boost_mode ? "":"Not");
		pr_err("otg pin:%s\n", bq->otg_pin_status ? "High" : "Low");
		pr_err("Fault Status:%s\n", bq->otg_online ?
					  boost_fault_state_str[bq->fault_status] 
					: chg_fault_state_str[bq->fault_status]);
		pr_err("otg online:%d\n", bq->otg_online);
		pr_err("charger online:%d\n",bq->chg_online);
		
		if (bq->fault_status != 0){
			bq->fault_status=0;
			if(bq->chg_online){
				bq2415x_enable_charger(bq);
			}

			if(bq->otg_online){
				hl7057_enable_otg(bq->chg_dev, true);
			}
		}

		ret = bq2415x_read_byte(bq, BQ2415X_REG_05, &val);
		if (!ret) {
			pr_err("DPM Status:%d\n", !!(val & BQ2415X_DPM_STATUS_MASK));
			pr_err("CD Status:%d\n", !!(val & BQ2415X_CD_STATUS_MASK));
		}
	}
}

static void hl7057_monitor_workfunc(struct work_struct *work)
{
	struct bq2415x *bq = container_of(work, struct bq2415x, monitor_work.work);

	hl7057_dump_regs(bq);
	hl7057_check_status(bq);

	schedule_delayed_work(&bq->monitor_work, 5 * HZ);
}

static int hl7057_init_device(struct bq2415x *bq)
{
	int ret;

	bq->chg_mv = bq->cfg.chg_mv;
	bq->chg_ma = bq->cfg.chg_ma;
	bq->ivl_mv = bq->cfg.ivl_mv;
	bq->icl_ma = bq->cfg.icl_ma;


	/*safety register can only be written before other writes occur,
	  if lk code exist, it should write this register firstly before
	  write any other registers
	*/

	bq->cfg.safety_chg_ma += 100;
	ret = hl7057_set_safety_reg(bq, bq->cfg.safety_chg_mv, bq->cfg.safety_chg_ma);
	if (ret < 0)
		pr_err("Failed to set safety register:%d\n", ret);

	bq2415x_disable_low_charge(bq);
	ret = bq2415x_enable_term(bq, bq->cfg.enable_term);
	if (ret < 0)
		pr_err("Failed to %s termination:%d\n",
			bq->cfg.enable_term ? "enable" : "disable", ret);

	ret = bq2415x_set_vbatlow_volt(bq, bq->cfg.batlow_mv);
	if (ret < 0)
		pr_err("Failed to set vbatlow volt to %d,rc=%d\n",
					bq->cfg.batlow_mv, ret);

	hl7057_set_term_current(bq, bq->cfg.iterm_ma);

	hl7057_set_charge_profile(bq);

	if (bq->charge_enabled)
		ret = bq2415x_enable_charger(bq);
	else
		ret = bq2415x_disable_charger(bq);

	if (ret < 0)
		pr_err("Failed to %s charger:%d\n",
			bq->charge_enabled ? "enable" : "disable", ret);
	return 0;
}

static struct charger_ops hl7057_chg_ops = {
	/* Normal charging */
	.plug_in = bq2415x_plug_in,
	.plug_out = bq2415x_plug_out,
	.dump_registers = hl7057_dump_register,
	.enable = bq2415x_charging,
	.is_enabled = bq2415x_is_charging_enable,
	.get_charging_current = hl7057_get_chargecurrent,
	.set_charging_current = hl7057_set_chargecurrent,
	.get_input_current = hl7057_get_input_current_limit,
	.set_input_current = bq2415x_set_input_current_limit,
	.get_constant_voltage = bq2415x_get_chargevoltage,
	.set_constant_voltage = bq2415x_set_chargevoltage,
	.kick_wdt = NULL,
	.set_mivr = NULL,
	.is_charging_done = bq2415x_is_charging_done,
	.get_min_charging_current = NULL,

	/* Safety timer */
	.enable_safety_timer = NULL,
	.is_safety_timer_enabled = NULL,

	/* Power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,

	/* OTG */
	.enable_otg = hl7057_enable_otg,
	.set_boost_current_limit = NULL,
	.enable_discharge = NULL,

	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	//.set_ta20_reset = NULL,
	.enable_cable_drop_comp = NULL,

	/* ADC */
	.get_tchg_adc = NULL,
	.charger_set_wakelock = charger_set_wakelock,
	.event = bq2415x_do_event,
};


/*****************HL7057 code is end*****************/

static int bq2415x_charger_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct bq2415x *bq = NULL;
	struct charger_ops *ops= NULL;
	int ret;
	ktime_t ktime;

	bq = devm_kzalloc(&client->dev, sizeof(struct bq2415x), GFP_KERNEL);
	if (!bq) {
		pr_err("out of memory\n");
		return -ENOMEM;
	}

	bq->dev = &client->dev;
	bq->client = client;
	bq->polling_interval=10;
	bq->rst_timeout = false;
	
	i2c_set_clientdata(client, bq);
	init_waitqueue_head(&bq->wait_que);
	   
	mutex_init(&bq->i2c_rw_lock);
	
	ret = bq2415x_detect_device(bq);

//+bug 436809  modify getian.wt 20190403 Add charger IC model information in factory mode
	if(!ret){
		if(is_eta6937(bq)){
			ops=&eta6937_chg_ops;
			hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "ETA6937");
		}else if(is_hl7057(bq)){
			ops=&hl7057_chg_ops;
		    hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "HL7057");
		}else{
			ops=&bq2415x_chg_ops;
			hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "BQ24157");
		}
	}else{
		pr_info("no charger device found:%d\n", ret);
		return -ENODEV;
	}
//-bug 436809  modify getian.wt 20190403 Add charger IC model information in factory mode

	if(!ret && (is_eta6937(bq) || is_hl7057(bq))){
		wakeup_source_init(&bq->eta_charger_lock, "ChargerIc suspend wakelock");
		kthread_run(charger_rst_thread, bq, "charger_thread");

		ktime= ktime_set(bq->polling_interval, 0);
		hrtimer_init(&bq->kthread_timer,
	   	               CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		bq->kthread_timer.function =
				reset_kthread_hrtimer_func;
		hrtimer_start(&bq->kthread_timer, ktime,
				HRTIMER_MODE_REL);
	   
	}

	if (!ret && bq->vendor == 2) {
		pr_info("charger device detected, revision:%d\n",
							bq->revision);
	} else {
		pr_info("no charger device found:%d\n", ret);
		return -ENODEV;
	}

	if (client->dev.of_node) {
		ret = bq2415x_parse_dt(&client->dev, bq);
		if (ret) {
			pr_err("device tree parse error!\n");
			return ret;
		}
	} else {
		pr_err("No device tree node for bq24157\n");
		return -ENODEV;
	}

		/* Register charger device */
	bq->chg_dev = charger_device_register(
		"primary_chg", &client->dev, bq, ops,&bq->chg_props);
	if (IS_ERR_OR_NULL(bq->chg_dev)) {
		ret = PTR_ERR(bq->chg_dev);
		pr_err("charger device register fail\n");
		goto err_0;
	}

	if(is_eta6937(bq)){
		ret = eta6937_init_device(bq);
		if (ret) {
			pr_err("device init failure: %d\n", ret);
			goto err_0;
		}

		INIT_DELAYED_WORK(&bq->monitor_work, eta6937_monitor_workfunc);

	}else if(is_hl7057(bq)){
		ret = hl7057_init_device(bq);
		if (ret) {
			pr_err("device init failure: %d\n", ret);
			goto err_0;
		}

		INIT_DELAYED_WORK(&bq->monitor_work, hl7057_monitor_workfunc);

	}else{
		ret = bq2415x_init_device(bq);
		if (ret) {
			pr_err("device init failure: %d\n", ret);
			goto err_0;
		}

		INIT_DELAYED_WORK(&bq->monitor_work, bq2415x_monitor_workfunc);
	}

	ret = sysfs_create_group(&bq->dev->kobj, &bq2415x_attr_group);
	if (ret)
		dev_err(bq->dev, "failed to register sysfs. err: %d\n", ret);

	pr_info("BQ2419X charger driver probe successfully\n");

	schedule_delayed_work(&bq->monitor_work, 5 * HZ);

	return 0;

err_0:

	return ret;
}

static int bq2415x_charger_remove(struct i2c_client *client)
{
	struct bq2415x *bq = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&bq->monitor_work);

	mutex_destroy(&bq->i2c_rw_lock);

	sysfs_remove_group(&bq->dev->kobj, &bq2415x_attr_group);

	return 0;
}

static void bq2415x_charger_shutdown(struct i2c_client *client)
{
	pr_info("shutdown\n");

}

static struct of_device_id bq2415x_charger_match_table[] = {
	{.compatible = "ti,bq2415x"},
	{},
};

static const struct i2c_device_id bq2415x_charger_id[] = {
	{ "bq2415x", BQ24157 },
	{},
};

MODULE_DEVICE_TABLE(i2c, bq2415x_charger_id);

static struct i2c_driver bq2415x_charger_driver = {
	.driver		= {
		.name	= "bq2415x",
		.of_match_table = bq2415x_charger_match_table,
	},
	.id_table	= bq2415x_charger_id,

	.probe		= bq2415x_charger_probe,
	.remove		= bq2415x_charger_remove,
	.shutdown   = bq2415x_charger_shutdown,
};

module_i2c_driver(bq2415x_charger_driver);

MODULE_DESCRIPTION("TI BQ2415x Charger Driver");
MODULE_LICENSE("GPL2");
MODULE_AUTHOR("Texas Instruments");
