/*
 *  drivers/power/rt9455.c
 *  Richtek RT9455 Switch Mode Charger Driver
 *
 *  Copyright (C) 2013 Richtek Technology Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/string.h>
#include <linux/battery/sec_charger.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#define MINVAL(a, b) ((a<=b) ? a: b)

static void rt9455_configure_charger(struct i2c_client *client);

static inline int rt9455_read_device(struct i2c_client *i2c,
				      int reg, int bytes, void *dest)
{
	int ret;

	if (bytes > 1)
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, bytes, dest);
	else {
		ret = i2c_smbus_read_byte_data(i2c, reg);
		if (ret < 0)
			return ret;
		*(unsigned char *)dest = (unsigned char)ret;
	}

	return ret;
}

static int rt9455_reg_read(struct i2c_client *i2c, int reg)
{
	int ret;

	pr_debug("I2C Read (client : 0x%x) reg = 0x%x\n",
           (unsigned int)i2c,(unsigned int)reg);

	ret = i2c_smbus_read_byte_data(i2c, reg);
	return ret;
}

static int rt9455_reg_write(struct i2c_client *i2c, int reg, unsigned char data)
{
	int ret;
	pr_debug("I2C Write (client : 0x%x) reg = 0x%x, data = 0x%x\n",
           (unsigned int)i2c,(unsigned int)reg,(unsigned int)data);

	ret = i2c_smbus_write_byte_data(i2c, reg, data);
	return ret;
}

static int rt9455_assign_bits(struct i2c_client *i2c, int reg,
		unsigned char mask, unsigned char data)
{
	unsigned char value;
	int ret;

	ret = rt9455_read_device(i2c, reg, 1, &value);
	if (ret < 0)
		goto out;
	value &= ~mask;
	value |= (data&mask);
	ret = i2c_smbus_write_byte_data(i2c,reg,value);
out:
	return ret;
}

static int rt9455_set_bits(struct i2c_client *i2c, int reg,
		unsigned char mask)
{
	return rt9455_assign_bits(i2c,reg,mask,mask);
}

static int rt9455_clr_bits(struct i2c_client *i2c, int reg,
		unsigned char mask)
{
	return rt9455_assign_bits(i2c,reg,mask,0);
}

/* Function name : rt9455_force_otamode
 * Function parameter : (onoff  1: on  0: off)
 * Function return value : (integer < 0 fail)
 * Function description:
 *	Let the user force enable OTA mode
 *	whenever the external pin mode is used or not
 */
static int rt9455_force_otamode(struct i2c_client *client, int onoff)
{
	int ret = 0;

	if (onoff)
		ret = rt9455_set_bits(client, RT9455_REG_CTRL2, RT9455_OPAMODE_MASK);
	else
		ret = rt9455_clr_bits(client, RT9455_REG_CTRL2, RT9455_OPAMODE_MASK);

	return ret;
}

static void rt9455_set_input_current_limit(struct i2c_client *i2c, int current_limit)
{
	u8 data;

	pr_info("current_limit = %d\n", current_limit);
	#if 1
	if (current_limit <= 100)
		data = 0;
	else if (current_limit > 100 && current_limit <= 500)
		data = 0x1;
	else if (current_limit > 500 && current_limit <= 1000)
		data = 0x2;
	else
		data = 0x3;
	#else
	//always set AICR to no limit (workaround)
	data = 0x3;
	#endif

	rt9455_assign_bits(i2c, RT9455_REG_CTRL2, RT9455_AICR_LIMIT_MASK,
		data << RT9455_AICR_LIMIT_SHIFT);
}

static void rt9455_set_fast_charging_current(struct i2c_client *i2c, int charging_current)
{
	u8 data;

	pr_debug("charging_current = %d\n", charging_current);
	if (charging_current < 500)
		data = 0;
	else if (charging_current >= 500 && charging_current <= 1550)
	{
		data = (charging_current-500)/150;
	}
	else
		data = 0x7;

	rt9455_assign_bits(i2c, RT9455_REG_CTRL6, RT9455_ICHRG_MASK,
		data << RT9455_ICHRG_SHIFT);
}

static void rt9455_set_termination_current_limit(struct i2c_client *i2c,
                                                 int charging_current,
                                                 int current_limit)
{
	int charging_10p = charging_current/10;
	u8 data;

	pr_debug("current_limit = %d\n", current_limit);
	if (current_limit < charging_10p*2)
		data = 0;
	else if (current_limit >= charging_10p*2 && current_limit < charging_10p*3)
		data = 0x2;
	else
		data = 0x1;

	rt9455_assign_bits(i2c, RT9455_REG_CTRL5, RT9455_IEOC_MASK,
		data << RT9455_IEOC_SHIFT);
}

/* Function name : rt9455_set_charger_current
 * Function parameter : (fast_chg_curr: xxx mA:unit)
 * Function return value : (integer < 0 fail)
 * Function description:
 *	Let the user set the charging current
 *      This function will set AICR & ICHG, both.
 *	Use this function, just send the current_value you want
 */
static int rt9455_set_charger_current(struct i2c_client *client)
{
	int ret = 0;
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&charger->delayed_work);
	queue_delayed_work(charger->wq,
		&charger->delayed_work, msecs_to_jiffies(250));

	return ret;
}

static void rt9455_work_func(struct work_struct *work)
{
	int eoc;
	int chg_current;
	int input_curr_limit;
	struct sec_charger_info *charger = container_of(work,
			struct sec_charger_info, delayed_work.work);

	input_curr_limit = charger->pdata->charging_current
		[charger->cable_type].input_current_limit;
	chg_current = charger->pdata->charging_current
		[charger->cable_type].fast_charging_current;
	charger->charging_current = chg_current;
	eoc = charger->pdata->charging_current
		[charger->cable_type].full_check_current_1st;

	rt9455_set_input_current_limit(charger->client, input_curr_limit);
	rt9455_set_fast_charging_current(charger->client, chg_current);
	rt9455_set_termination_current_limit(charger->client,
			chg_current, eoc);
}

static void rt9455_enable_te(struct i2c_client *i2c, int en)
{
	union power_supply_propval value;

	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_NOW, value);
	if (value.intval == SEC_BATTERY_CHARGING_2ND) {
		pr_info("disable_te : %d\n", en);
		rt9455_clr_bits(i2c, RT9455_REG_CTRL2, 1<<3);
	} else {
		pr_info("enable_te : %d\n", en);
		en ?  rt9455_set_bits(i2c, RT9455_REG_CTRL2, 1<<3):
			 rt9455_clr_bits(i2c, RT9455_REG_CTRL2, 1<<3);
	}
}

static void rt9455_ieoc_mask_enable(struct i2c_client *client, int onoff)
{
	if (onoff) {
		rt9455_set_bits(client, RT9455_REG_MASK2, RT9455_EVENT_CHTERMI);
	} else {
		rt9455_clr_bits(client, RT9455_REG_MASK2, RT9455_EVENT_CHTERMI);
	}
}

static void __rt9455_charger_switch(struct i2c_client *client, int onoff)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	charger->is_charging = onoff;

	pr_info("onoff = %d\n", onoff);

	if (onoff) {
		rt9455_set_bits(client, RT9455_REG_CTRL7, RT9455_CHGEN_MASK);
	} else {
		rt9455_clr_bits(client, RT9455_REG_CTRL7, RT9455_CHGEN_MASK);
	}

	rt9455_enable_te(client, onoff);
}

/* Function name : rt9455_charger_switch
 * Function parameter : (onoff  1: on  0: off)
 * Function return value : (integer < 0 fail)
 * Function description:
 *	Let the user set the charge buck switch
 */
static int rt9455_charger_switch(struct i2c_client *client, int onoff)
{
	__rt9455_charger_switch(client, onoff);
	rt9455_ieoc_mask_enable(client, !onoff);

	return 0;
}

static int rt9455_chip_reset(struct i2c_client *client)
{
	struct rt9455_chip *chip = i2c_get_clientdata(client);
	int ret;

	//Before the IC reset, inform the customer
	if (chip->cb)
		chip->cb->reset_callback(RT9455_EVENT_BEFORE_RST);

	ret = rt9455_set_bits(client, RT9455_REG_CTRL4, RT9455_RST_MASK);
	if (ret < 0)
		return -EIO;

	if (chip->cb)
		chip->cb->reset_callback(RT9455_EVENT_AFTER_RST);


	return 0;
}

static int rt9455_chip_reg_init(struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	//ctrl value
	rt9455_reg_write(client, RT9455_REG_CTRL2, 0x6C);
	rt9455_reg_write(client, RT9455_REG_CTRL3, 0x66);
	rt9455_reg_write(client, RT9455_REG_CTRL5, 0x1A);
	rt9455_reg_write(client, RT9455_REG_CTRL6, 0x22);
	rt9455_reg_write(client, RT9455_REG_CTRL7, 0x1A);

	// irq mask value
	/* VINOVP, BAT absence */
	rt9455_reg_write(client, RT9455_REG_MASK1, 0xFF);
	rt9455_reg_write(client, RT9455_REG_MASK2, 0xFF);
	rt9455_reg_write(client, RT9455_REG_MASK3, 0xFF);

	rt9455_reg_read(client, RT9455_REG_MASK1);
	rt9455_reg_read(client, RT9455_REG_MASK2);
	rt9455_reg_read(client, RT9455_REG_MASK3);

	charger->wq = create_workqueue("rt9455_workqueue");
	INIT_DELAYED_WORK(&charger->delayed_work, rt9455_work_func);

	return 0;
}

static int rt9455_get_charging_status(struct i2c_client *client)
{
	uint8_t reg;
	int ret;
	static int before_flag = 0;
	union power_supply_propval value;

	ret = rt9455_read_device(client, RT9455_REG_CTRL1, 1, &reg);
	if (ret < 0) {
		pr_err("%s: status read failed\n", __func__);
		return ret;
	}

	reg >>= 4;

	switch (reg & 0x3) {
	case 0:
		ret = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 1:
		ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 2:
		ret = POWER_SUPPLY_STATUS_FULL;
		break;
	default:
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}

	psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_NOW, value);
	if (value.intval == SEC_BATTERY_CHARGING_2ND) {
		if (before_flag != value.intval) {
			pr_info("2nd charging : %d\n", value.intval);
			/* Clear termination state & re-setting register */
			rt9455_set_bits(client, RT9455_REG_CTRL4, RT9455_RST_MASK);
			rt9455_reg_write(client, RT9455_REG_CTRL3, 0x66);
			rt9455_reg_write(client, RT9455_REG_CTRL5, 0x1A);
			rt9455_reg_write(client, RT9455_REG_CTRL6, 0x22);
			rt9455_reg_write(client, RT9455_REG_CTRL7, 0x1A);
			rt9455_reg_write(client, RT9455_REG_MASK1, 0xFF);
			rt9455_reg_write(client, RT9455_REG_MASK2, 0xFF);
			rt9455_reg_write(client, RT9455_REG_MASK3, 0xFF);
			mdelay(100);
			/* enable 2nd chrging & disable termination current */
			rt9455_configure_charger(client);
		}
		ret = POWER_SUPPLY_STATUS_FULL;
	}
	before_flag = value.intval;

	return ret;
}

static int rt9455_get_charging_health(struct i2c_client *client)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;
	uint8_t reg;
	int ret;

	ret = rt9455_read_device(client, RT9455_REG_CTRL1, 1, &reg);
	if (ret < 0) {
		pr_err("%s : Health state read failed\n", __func__);
		return ret;
	}

	pr_info("%s: health status : 0x%x\n", __func__, reg);
	switch (reg & 0x4) {
	case 0:
		health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
		break;
	case 4:
		health = POWER_SUPPLY_HEALTH_GOOD;
		break;
	default:
		health = POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	return health;
}

#if defined(CONFIG_FUELGAUGE_88PM822)
static int rt9455_get_power_status(struct i2c_client *client)
{
	uint8_t reg;
	int ret;

	ret = rt9455_read_device(client, RT9455_REG_CTRL1, 1, &reg);
	if (ret < 0) {
		pr_err("%s: status read failed\n", __func__);
		return ret;
	}

	reg >>= 2;

	switch (reg & 0x1) {
	case 0:
		ret = POWER_SUPPLY_PWR_RDY_FALSE;
		break;
	case 1:
		ret = POWER_SUPPLY_PWR_RDY_TRUE;
		break;
	default:
		ret = POWER_SUPPLY_PWR_RDY_FALSE;
	}

	return ret;
}
#endif

static void rt9455_set_regulation_voltage(struct i2c_client *i2c, int float_voltage)
{
	uint8_t data;
	pr_debug("float voltage = %d\n", float_voltage);

	if (float_voltage < 3500)
		data = 0;
	else if (float_voltage >= 3500 && float_voltage <= 4450)
		data = (float_voltage-3500)/20;
	else
		data = 0x3f;

	rt9455_assign_bits(i2c, RT9455_REG_CTRL3, RT9455_VOREG_MASK,
		data << RT9455_VOREG_SHIFT);
}

static void rt9455_configure_charger(struct i2c_client *client)
{
	int eoc;
	int chg_current;
	int float_voltage;
	int input_curr_limit;
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	pr_info("%s : Set cconfig harging\n", __func__);
	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		rt9455_charger_switch(client, 1);
		rt9455_set_bits(client, RT9455_REG_CTRL2, 1<<1);
	} else {
		/* Input current limit */
		pr_info("%s : input current (%dmA)\n",
			__func__, charger->pdata->charging_current
			[charger->cable_type].input_current_limit);


        	input_curr_limit = charger->pdata->charging_current
			[charger->cable_type].input_current_limit;

	        float_voltage = charger->pdata->chg_float_voltage;
		/* Float voltage */
		pr_info("%s : float voltage (%dmV)\n",
				__func__, float_voltage);


        	rt9455_set_regulation_voltage(charger->client, float_voltage);

	        chg_current = charger->pdata->charging_current
				[charger->cable_type].fast_charging_current;
        	eoc = charger->pdata->charging_current
	                [charger->cable_type].full_check_current_1st;
  		/* Fast charge and Termination current */
		pr_info("%s : fast charging current (%dmA)\n",
				__func__, chg_current);

		pr_info("%s : termination current (%dmA)\n",
				__func__, eoc);

			rt9455_set_charger_current(client);
			rt9455_charger_switch(client, 1);
		rt9455_clr_bits(client, RT9455_REG_CTRL2, 1<<1);
	}
}

bool sec_hal_chg_get_property(struct i2c_client *client,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int chg_curr, aicr;
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = rt9455_get_charging_status(client);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = rt9455_get_charging_health(client);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->is_charging)
			val->intval = 1000;
		else
			val->intval = 0;
		break;
#if defined(CONFIG_FUELGAUGE_88PM822)
	case POWER_SUPPLY_PROP_POWER_STATUS:
		val->intval = rt9455_get_power_status(client);
		break;
#endif
	default:
		return false;
	}

	return true;
}

bool sec_hal_chg_set_property(struct i2c_client *client,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	int chg_currenti, term_current, current_now;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (charger->cable_type  == POWER_SUPPLY_TYPE_BATTERY) {
			pr_info("%s:[BATT] Type Battery\n", __func__);
			rt9455_configure_charger(client);
		} else {
			pr_info("%s:[BATT] Set charging\n", __func__);
			/* Enable charger */
			rt9455_configure_charger(client);
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_POWER_NOW:
		current_now =
			charger->charging_current * val->intval / 100;
		rt9455_set_fast_charging_current(charger->client, current_now);
		break;
	default:
		return false;
	}

	return true;
}

static int rt9455_debugfs_show(struct seq_file *s, void *data)
{
	struct sec_charger_info *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "RT9455 CHARGER IC :\n");
	seq_printf(s, "==================\n");
	for (reg = 0x00; reg <= 0x0D; reg++) {
		rt9455_read_device(charger->client, reg, 1, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}
	seq_printf(s, "\n");
	return 0;
}

static int rt9455_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, rt9455_debugfs_show, inode->i_private);
}

static const struct file_operations rt9455_debugfs_fops = {
	.open           = rt9455_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

bool sec_hal_chg_init(struct i2c_client *client)
{
        struct sec_charger_info *charger = i2c_get_clientdata(client);

        dev_info(&client->dev,
                "%s: RT9455 Charger init(Start)!!\n", __func__);

		rt9455_chip_reg_init(client);

        (void) debugfs_create_file("rt9455-regs",
                S_IRUGO, NULL, (void *)charger, &rt9455_debugfs_fops);

        return true;
}

bool sec_hal_chg_suspend(struct i2c_client *client)
{
	return 0;
}

bool sec_hal_chg_resume(struct i2c_client *client)
{
	return 0;
}

ssize_t sec_hal_chg_show_attrs(struct device *dev,
                                const ptrdiff_t offset, char *buf)
{
	int ret = 0;

	return ret;
}

ssize_t sec_hal_chg_store_attrs(struct device *dev,
                                const ptrdiff_t offset,
                                const char *buf, size_t count)
{
	int ret = 0;

	return ret;
}
