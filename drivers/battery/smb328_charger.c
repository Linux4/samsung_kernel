/*
 *  smb328_charger.c
 *  Samsung SMB328 Charger Driver
 *
 *  Copyright (C) 2013 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/battery/sec_charger.h>
#include <linux/battery/charger/smb328_charger.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>

#include <linux/gpio-pxa.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/usb_notify.h>

#if defined(SIOP_CHARGING_CURRENT_LIMIT_FEATURE)
#define SIOP_CHARGING_LIMIT_CURRENT 800
static bool is_siop_limited;
#endif

int otg_enable_flag;

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static int smb328_i2c_write(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		pr_err("%s: Error(%d) : 0x%x\n", __func__, ret, reg);

	return ret;
}

static int smb328_i2c_read(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		pr_err("%s: Error(%d) : 0x%x\n", __func__, ret, reg);

	return ret;
}

static void smb328_i2c_write_array(struct i2c_client *client,
				u8 *buf, int size)
{
	int i;
	for (i = 0; i < size; i += 3)
		smb328_i2c_write(client, (u8) (*(buf + i)), (buf + i) + 1);
}

static void smb328_set_command(struct i2c_client *client,
				int reg, int datum)
{
	int val;
	u8 data = 0;
	val = smb328_i2c_read(client, reg, &data);
	if (val >= 0) {
		pr_debug("%s : reg(0x%02x): 0x%02x", __func__, reg, data);
		if (data != datum) {
			data = datum;
			if (smb328_i2c_write(client, reg, &data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328_i2c_read(client, reg, &data);
			if (val >= 0)
				pr_debug(" => 0x%02x\n", data);
		}
	}
}

static void smb328_test_read(struct i2c_client *client)
{
	u8 data = 0;
	u32 addr = 0;
	for (addr = 0; addr < 0x0c; addr++) {
		smb328_i2c_read(client, addr, &data);
		pr_info("smb328 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
	for (addr = 0x31; addr < 0x3D; addr++) {
		smb328_i2c_read(client, addr, &data);
		pr_info("smb328 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
}

static void smb328_read_regs(struct i2c_client *client, char *str)
{
	u8 data = 0;
	u32 addr = 0;

	for (addr = 0x0; addr < 0x0A; addr++) {
		smb328_i2c_read(client, addr, &data);
		sprintf(str+strlen(str), "0x%x, ", data);
	}

	/* "#" considered as new line in application */
	sprintf(str+strlen(str), "#");

	for (addr = 0x31; addr < 0x39; addr++) {
		smb328_i2c_read(client, addr, &data);
		sprintf(str+strlen(str), "0x%x, ", data);
	}
}
static void smb328_charger_otg_control(struct sec_charger_info *charger,
			int enable);

static int smb328_otg_over_current_status(struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 irq_c, stat_c = 0;

#ifdef CONFIG_USB_NOTIFY_LAYER
	struct otg_notify *o_notify = get_otg_notify();
#endif

	smb328_i2c_read(client, SMB328_INTERRUPT_STATUS_C, &irq_c);
	pr_info("%s : Charging status C(0x%02x)\n", __func__, stat_c);
	if ((charger->cable_type == POWER_SUPPLY_TYPE_OTG) &&
		(irq_c & 0x40)) {
		pr_info("%s: otg overcurrent limit\n", __func__);
#ifdef CONFIG_USB_NOTIFY_LAYER
		send_otg_notify(o_notify, NOTIFY_EVENT_OVERCURRENT, 0);
#endif
		smb328_charger_otg_control(charger, 0);
	}
}

static int smb328_get_charging_status(struct i2c_client *client)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 stat_c = 0;

	smb328_i2c_read(client, SMB328_BATTERY_CHARGING_STATUS_C, &stat_c);
	pr_info("%s : Charging status C(0x%02x)\n", __func__, stat_c);

	/* At least one charge cycle terminated,
	 * Charge current < Termination Current
	 */
	if (stat_c & 0xc0) {
		/* top-off by full charging */
		status = POWER_SUPPLY_STATUS_FULL;
		return status;
	}

	/* Is enabled ? */
	if (stat_c & 0x01) {
		/* check for 0x30 : 'safety timer' (0b01 or 0b10) or
		 * 'waiting to begin charging' (0b11)
		 * check for 0x06 : no charging (0b00)
		 */
		/* not charging */
		if ((stat_c & 0x30) || !(stat_c & 0x06))
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	return (int)status;
}

static int smb328_get_battery_present(struct i2c_client *client)
{
	u8 reg_data, irq_data;

	smb328_i2c_read(client, SMB328_BATTERY_CHARGING_STATUS_B, &reg_data);
	smb328_i2c_read(client, SMB328_INTERRUPT_STATUS_C, &irq_data);

	reg_data = ((reg_data & 0x01) && (irq_data & 0x10));

	return !reg_data;
}

static void smb328_set_writable(struct i2c_client *client, int writable)
{
	u8 reg_data;

	smb328_i2c_read(client, SMB328_COMMAND, &reg_data);

	if (writable)
		reg_data |= CMD_A_ALLOW_WRITE;
	else
		reg_data &= ~CMD_A_ALLOW_WRITE;

	smb328_i2c_write(client, SMB328_COMMAND, &reg_data);
}

static u8 smb328_set_charge_enable(struct i2c_client *client, int enable)
{
	u8 chg_en;
	u8 reg_data;

	reg_data = 0x00;

	smb328_i2c_write(client, SMB328_FUNCTION_CONTROL_C, &reg_data);

	smb328_i2c_read(client, SMB328_COMMAND, &chg_en);
	if (!enable)
		chg_en |= CMD_CHARGE_EN;
	else
		chg_en &= ~CMD_CHARGE_EN;

	smb328_i2c_write(client, SMB328_COMMAND, &chg_en);

	return chg_en;
}

static u8 smb328_set_float_voltage(struct i2c_client *client, int float_voltage)
{
	u8 reg_data, float_data;

	if (float_voltage < 3460)
		float_data = 0;
	else if (float_voltage <= 4340)
		float_data = (float_voltage - 3500) / 20 + 2;
	else if ((float_voltage == 4350) || (float_voltage == 4360))
		float_data = 0x2D; /* (4340 -3500)/20 + 1 */
	else
		float_data = 0x3F;

	smb328_i2c_read(client,	SMB328_FLOAT_VOLTAGE, &reg_data);
	reg_data &= ~CFG_FLOAT_VOLTAGE_MASK;
	reg_data |= float_data << CFG_FLOAT_VOLTAGE_SHIFT;

	smb328_i2c_write(client, SMB328_FLOAT_VOLTAGE, &reg_data);

	pr_info("%s: float_voltage : %d\n", __func__, float_voltage);

	return reg_data;
}

static u8 smb328_set_input_current_limit(struct i2c_client *client,
				int input_current)
{
	u8 curr_data, reg_data;

	curr_data = input_current < 500 ? 0x0 :
		input_current > 1200 ? 0x7 :
		(input_current - 500) / 100;

	smb328_i2c_read(client, SMB328_INPUT_CURRENTLIMIT, &reg_data);
	reg_data &= ~CFG_INPUT_CURRENT_MASK;
	reg_data |= curr_data << CFG_INPUT_CURRENT_SHIFT;

	smb328_i2c_write(client, SMB328_INPUT_CURRENTLIMIT, &reg_data);

	pr_info("%s: set current limit : 0x%x\n", __func__, reg_data);

	return reg_data;
}

static u8 smb328_set_termination_current_limit(struct i2c_client *client,
				int termination_current)
{
	u8 reg_data, term_data;

	term_data = termination_current < 25 ? 0x0 :
		termination_current > 200 ? 0x7 :
		(termination_current - 25) / 25;

	/* Charge completion termination current */
	smb328_i2c_read(client, SMB328_CHARGE_CURRENT, &reg_data);
	reg_data &= ~CFG_TERMINATION_CURRENT_MASK;
	reg_data |= term_data << CFG_TERMINATION_CURRENT_SHIFT;

	smb328_i2c_write(client, SMB328_CHARGE_CURRENT, &reg_data);

	/* set STAT assertion termination current */
	smb328_i2c_read(client, SMB328_VARIOUS_FUNCTIONS, &reg_data);
	reg_data &= ~CFG_STAT_ASSETION_TERM_MASK;
	reg_data |= term_data << CFG_STAT_ASSETION_TERM_SHIFT;

	smb328_i2c_write(client, SMB328_VARIOUS_FUNCTIONS, &reg_data);

	return reg_data;
}

static u8 smb328_set_fast_charging_current(struct i2c_client *client,
				int fast_charging_current)
{
	u8 reg_data, chg_data;

	chg_data = fast_charging_current < 500 ? 0x0 :
		fast_charging_current > 1200 ? 0x7 :
		(fast_charging_current - 500) / 100;

	smb328_i2c_read(client, SMB328_CHARGE_CURRENT, &reg_data);
	reg_data &= ~CFG_CHARGE_CURRENT_MASK;
	reg_data |= chg_data << CFG_CHARGE_CURRENT_SHIFT;

	smb328_i2c_write(client, SMB328_CHARGE_CURRENT, &reg_data);

	pr_info("%s: Charge Current : 0x%x\n", __func__, reg_data);

	return reg_data;
}

static void smb328_charger_function_control(struct sec_charger_info *charger)
{
	u8 reg_data, charge_mode;

	smb328_set_writable(charger->client, 1);
	reg_data = 0x6E;
	smb328_i2c_write(charger->client, SMB328_FUNCTION_CONTROL_B, &reg_data);

	smb328_i2c_read(charger->client,
			SMB328_INTERRUPT_SIGNAL_SELECTION, &reg_data);
	reg_data |= 0x1;
	smb328_i2c_write(charger->client,
			SMB328_INTERRUPT_SIGNAL_SELECTION, &reg_data);

	if ((charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) ||
		(charger->cable_type == POWER_SUPPLY_TYPE_OTG)) {
		/* turn off charger */
		smb328_set_charge_enable(charger->client, 0);
	} else {
		pr_info("%s: Input : %d, Charge : %d\n", __func__,
			charger->pdata->charging_current[charger->cable_type].input_current_limit,
			charger->pdata->charging_current[charger->cable_type].fast_charging_current);
		/* AICL enable */
		smb328_i2c_read(charger->client,
				SMB328_INPUT_CURRENTLIMIT, &reg_data);
		reg_data &= ~CFG_AICL_ENABLE;
		reg_data &= ~CFG_AICL_VOLTAGE; /* AICL enable voltage 4.25V */
		smb328_i2c_write(charger->client,
				SMB328_INPUT_CURRENTLIMIT, &reg_data);

		/* Function control A */
		reg_data = 0xDA;
		smb328_i2c_write(charger->client,
				SMB328_FUNCTION_CONTROL_A, &reg_data);

		/* 4.2V float voltage */
		smb328_set_float_voltage(charger->client,
			charger->pdata->chg_float_voltage);

		/* Set termination current */
		smb328_set_termination_current_limit(charger->client,
			charger->pdata->charging_current[charger->
			cable_type].full_check_current_1st);
#if defined(CONFIG_MACH_GOYAVEWIFI) || defined(CONFIG_MACH_GOYAVE3G)
		if ((charger->pdata->siop_level < 100) &&
			(charger->cable_type != POWER_SUPPLY_TYPE_USB)) {
			smb328_set_input_current_limit(charger->client,
			charger->pdata->charging_current
                        [charger->cable_type].input_current_limit * charger->pdata->siop_level/100);
		} else {
			smb328_set_input_current_limit(charger->client,
			charger->pdata->charging_current
                        [charger->cable_type].input_current_limit);
		}
#else
		smb328_set_input_current_limit(charger->client,
			charger->pdata->charging_current
                        [charger->cable_type].input_current_limit);
#endif
		smb328_set_fast_charging_current(charger->client,
			charger->pdata->charging_current
			[charger->cable_type].fast_charging_current);

		/* SET USB5/1, AC/USB Mode */
		charge_mode = (charger->cable_type == POWER_SUPPLY_TYPE_MAINS) ||
					(charger->cable_type == POWER_SUPPLY_TYPE_UARTOFF) ||
					(charger->cable_type == POWER_SUPPLY_TYPE_MISC) ||
					(charger->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) ?
					0x3 : 0x2;
		smb328_i2c_read(charger->client, SMB328_COMMAND, &reg_data);
		reg_data &= ~0x0C;
		reg_data |= charge_mode << 2;
		smb328_i2c_write(charger->client, SMB328_COMMAND, &reg_data);

		smb328_set_charge_enable(charger->client, 1);
	}
	smb328_test_read(charger->client);
	smb328_set_writable(charger->client, 0);
}

static int smb328_get_charging_health(struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 status_reg, data;

	smb328_i2c_read(client, SMB328_INTERRUPT_STATUS_C, &status_reg);
	pr_info("%s : Interrupt status C(0x%02x)\n", __func__, status_reg);

	/* Is enabled ? */
	if (status_reg & 0x2) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (status_reg & 0x4) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (status_reg & 0x80) { /* watchdog enable workaround */
		/* clear IRQ */
		data = 0xFF;
		smb328_i2c_write(client, 0x30, &data);
		/* reset function control */
		smb328_charger_function_control(charger);
	}

	return (int)health;
}

static int smb328_get_charge_now(struct i2c_client *client)
{
	u8 chg_now, data, addr;

	smb328_i2c_read(client, SMB328_BATTERY_CHARGING_STATUS_A, &chg_now);

	for (addr = 0; addr < 0x0c; addr++) {
		smb328_i2c_read(client, addr, &data);
		pr_debug("smb328 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
	for (addr = 0x31; addr <= 0x3D; addr++) {
		smb328_i2c_read(client, addr, &data);
		pr_debug("smb328 addr : 0x%02x data : 0x%02x\n", addr, data);
	}

	/* Clear IRQ */
	data = 0xFF;
	smb328_i2c_write(client, 0x30, &data);

	return (chg_now & 0x2);
}

static void smb328_charger_otg_control(struct sec_charger_info *charger,
			int enable)
{
	u8 reg_data;

	smb328_i2c_read(charger->client, SMB328_COMMAND, &reg_data);
	if (enable) {
		otg_enable_flag = 1;
		reg_data |= 0x02;
	} else {
		otg_enable_flag = 0;
		reg_data &= ~0x02;
	}

	smb328_set_writable(charger->client, 1);
	smb328_i2c_write(charger->client, SMB328_COMMAND, &reg_data);
	smb328_set_writable(charger->client, 0);

	smb328_i2c_read(charger->client, SMB328_COMMAND, &reg_data);
	pr_info("%s : otg control!!(%d)(0x%02x)\n", __func__, enable, reg_data);
}

static void smb328_irq_enable(struct i2c_client *client)
{
	u8 data;

	smb328_set_writable(client, 1);
	data = 0x6D;
	smb328_i2c_write(client, 0x04, &data);

	smb328_i2c_read(client,
			SMB328_INTERRUPT_SIGNAL_SELECTION, &data);
	data |= 0x1;
	smb328_i2c_write(client,
			SMB328_INTERRUPT_SIGNAL_SELECTION, &data);

	smb328_set_writable(client, 0);
}

static void smb328_irq_disable(struct i2c_client *client)
{
	u8 data;

	smb328_set_writable(client, 1);
	data = 0x6E;
	smb328_i2c_write(client, 0x04, &data);

	smb328_i2c_read(client,
			SMB328_INTERRUPT_SIGNAL_SELECTION, &data);
	data &= 0xfe;
	smb328_i2c_write(client,
			SMB328_INTERRUPT_SIGNAL_SELECTION, &data);

	smb328_set_writable(client, 0);
}

static int smb328_debugfs_show(struct seq_file *s, void *data)
{
	struct sec_charger_info *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "SMB CHARGER IC :\n");
	seq_printf(s, "==================\n");
	for (reg = 0x00; reg <= 0x0A; reg++) {
		smb328_i2c_read(charger->client, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	for (reg = 0x30; reg <= 0x39; reg++) {
		smb328_i2c_read(charger->client, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");
	return 0;
}

static int smb328_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, smb328_debugfs_show, inode->i_private);
}

static const struct file_operations smb328_debugfs_fops = {
	.open           = smb328_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static bool smb328_chg_init(struct sec_charger_info *charger)
{
	u8 reg_data;

	smb328_irq_disable(charger->client);

	(void) debugfs_create_file("smb328-regs",
		S_IRUGO, NULL, (void *)charger, &smb328_debugfs_fops);

	smb328_i2c_read(charger->client, SMB328_OTG_POWER_AND_LDO, &reg_data);
	reg_data &= (~0x18);
	smb328_set_writable(charger->client, 1);
	smb328_i2c_write(charger->client, SMB328_OTG_POWER_AND_LDO, &reg_data);
	smb328_set_writable(charger->client, 0);

	return true;
}

static int sec_chg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct sec_charger_info *charger =
		container_of(psy, struct sec_charger_info, psy_chg);
	u8 data;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb328_get_charging_status(charger->client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!charger->is_charging)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = smb328_get_charging_health(charger->client);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb328_get_battery_present(charger->client);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		smb328_irq_disable(charger->client);
		if (charger->charging_current) {
			smb328_i2c_read(charger->client, SMB328_CHARGE_CURRENT, &data);
			switch (data >> 5) {
			case 0:
				val->intval = 450;
				break;
			case 1:
				val->intval = 600;
				break;
			case 2:
				val->intval = 700;
				break;
			case 3:
				val->intval = 800;
				break;
			case 4:
				val->intval = 900;
				break;
			case 5:
				val->intval = 1000;
				break;
			case 6:
				val->intval = 1100;
				break;
			case 7:
				val->intval = 1200;
				break;
			}
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		val->intval = smb328_get_charge_now(charger->client);
		break;
#if defined(CONFIG_FUELGAUGE_88PM822)
	case POWER_SUPPLY_PROP_POWER_STATUS:
		val->intval = smb328_get_power_status(charger);
		break;
#endif		
	default:
		return false;
	}
	return true;
}

static int sec_chg_set_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      const union power_supply_propval *val)
{
	struct sec_charger_info *charger =
		container_of(psy, struct sec_charger_info, psy_chg);
	int cable_type, current_now;

	switch (psp) {
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		smb328_set_writable(charger->client, 1);
		current_now =
			charger->charging_current * val->intval / 100;
		smb328_set_fast_charging_current(charger->client, current_now);
		smb328_set_writable(charger->client, 0);
		break;
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		smb328_charger_function_control(charger);

		if (val->intval == POWER_SUPPLY_TYPE_OTG) {
			smb328_charger_otg_control(charger, 1);
			smb328_otg_over_current_status(charger->client);
		} else if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			smb328_charger_otg_control(charger, 0);
		}
		break;
	case POWER_SUPPLY_PROP_OTG_OVERCURRENT:
		smb328_otg_over_current_status(charger->client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (charger->pdata->siop_level != val->intval) {
			charger->pdata->siop_level = val->intval;
			smb328_charger_function_control(charger);
		}
		break;
	default:
		return false;
	}
	return true;
}

#ifdef CONFIG_OF
static int smb328_charger_parse_dt(struct device *dev,
		sec_battery_platform_data_t *pdata)
{
	struct device_node *np = dev->of_node;
        int ret = 0;
        int i, len;
        const u32 *p;

        if (np == NULL) {
                pr_err("%s np NULL\n", __func__);
        } else {
                ret = of_property_read_u32(np, "chg-float-voltage",
                                           &pdata->chg_float_voltage);
        }

        np = of_find_node_by_name(NULL, "sec-battery");
        if (!np) {
                pr_err("%s np NULL\n", __func__);
        } else {
                p = of_get_property(np, "battery,input_current_limit", &len);
                if (!p)
                        return 1;

                len = len / sizeof(u32);

                pdata->charging_current = kzalloc(sizeof(sec_charging_current_t) * len,
                                                  GFP_KERNEL);

                for(i = 0; i < len; i++) {
                        ret = of_property_read_u32_index(np,
                                 "battery,input_current_limit", i,
                                 &pdata->charging_current[i].input_current_limit);
                        ret = of_property_read_u32_index(np,
                                 "battery,fast_charging_current", i,
                                 &pdata->charging_current[i].fast_charging_current);
                        ret = of_property_read_u32_index(np,
                                 "battery,full_check_current_1st", i,
                                 &pdata->charging_current[i].full_check_current_1st);
                        ret = of_property_read_u32_index(np,
                                 "battery,full_check_current_2nd", i,
                                 &pdata->charging_current[i].full_check_current_2nd);
                }
        }

        return ret;
}

static struct of_device_id sec_charger_dt_ids[] = {
	{ .compatible = "QC,smb328", },
	{}
};
MODULE_DEVICE_TABLE(of, sec_charger_dt_ids);

#else
static int max77843_charger_parse_dt(struct max77843_charger_data *charger)
{
	return -ENOSYS;
}

#define rt5033_charger_match_table NULL
#endif 


static int smb328_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	sec_battery_platform_data_t *pdata;
	struct sec_charger_info *charger;
	int ret = 0;

	otg_enable_flag = 0;

	dev_info(&client->dev,
			"%s: SEC Charger Driver Loading\n", __func__);

	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata)
			pdata = kzalloc(sizeof(sec_battery_platform_data_t),
					GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		ret = smb328_charger_parse_dt(&client->dev, pdata);
		if (ret)
			return ret;
	} else {
		pdata = client->dev.platform_data;
	}

	ret = i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA |
				I2C_FUNC_SMBUS_I2C_BLOCK);
	if (ret < 0)
		return -EIO;

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->client = client;
	charger->pdata = pdata;

	i2c_set_clientdata(client, charger);

	charger->psy_chg.name		= "sec-charger";
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= sec_chg_get_property;
	charger->psy_chg.set_property	= sec_chg_set_property;
	charger->psy_chg.properties	= sec_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(sec_charger_props);

	charger->pdata->siop_level = 100;

	if (charger->pdata->chg_gpio_init) {
		if (!charger->pdata->chg_gpio_init()) {
			dev_err(&client->dev,
					"%s: Failed to Initialize GPIO\n", __func__);
			goto err_free;
		}
	}

	pr_info("%s: Clinet Address : 0x%x, Add : 0x%x\n", __func__, charger->client->addr, &charger->client->addr);
	if (!smb328_chg_init(charger)) {
		dev_err(&client->dev,
				"%s: Failed to Initialize Charger\n", __func__);
		goto err_free;
	}

	ret = power_supply_register(&client->dev, &charger->psy_chg);
	if (ret) {
		dev_err(&client->dev,
				"%s: Failed to Register psy_chg\n", __func__);
		goto err_free;
	}

	dev_dbg(&client->dev,
			"%s: SEC Charger Driver Loaded\n", __func__);
	return 0;

err_supply_unreg:
	power_supply_unregister(&charger->psy_chg);
err_free:
	kfree(charger);
	kfree(pdata);

	return ret;
}

static int smb328_charger_remove(
		struct i2c_client *client)
{
	return 0;
}

static int smb328_charger_suspend(struct i2c_client *client,
		pm_message_t state)
{
	return 0;
}

static int smb328_charger_resume(struct i2c_client *client)
{
	return 0;
}

static void smb328_charger_shutdown(struct i2c_client *client)
{
}

static const struct i2c_device_id sec_charger_id[] = {
	{"smb328-charger", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sec_charger_id);

static struct i2c_driver sec_charger_driver = {
	.driver = {
		.name	= "smb328-charger",
		.of_match_table = sec_charger_dt_ids,
	},
	.probe		= smb328_charger_probe,
	.remove		= smb328_charger_remove,
	.suspend	= smb328_charger_suspend,
	.resume		= smb328_charger_resume,
	.shutdown	= smb328_charger_shutdown,
	.id_table	= sec_charger_id,
};

static int __init smb328_charger_init(void)
{
	return i2c_add_driver(&sec_charger_driver);
}

static void __exit smb328_charger_exit(void)
{
	i2c_del_driver(&sec_charger_driver);
}

module_init(smb328_charger_init);
module_exit(smb328_charger_exit);

MODULE_DESCRIPTION("SMB328 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
