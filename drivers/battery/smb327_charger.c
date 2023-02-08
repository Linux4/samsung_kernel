/*
 *  smb327_charger.c
 *  Samsung SMB327 Charger Driver
 *
 *  Copyright (C) 2013 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/battery/sec_charger.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#if defined(SIOP_CHARGING_CURRENT_LIMIT_FEATURE)
#define SIOP_CHARGING_LIMIT_CURRENT 800
static bool is_siop_limited;
#endif

extern int system_rev;

static int smb327_i2c_write(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		pr_err("%s: Error(%d) : 0x%x\n", __func__, ret, reg);
	return ret;
}

static int smb327_i2c_read(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		pr_err("%s: Error(%d) : 0x%x\n", __func__, ret, reg);
	return ret;
}

#if 0 // to fix build error - unused function
static void smb327_i2c_write_array(struct i2c_client *client,
				u8 *buf, int size)
{
	int i;
	for (i = 0; i < size; i += 3)
		smb327_i2c_write(client, (u8) (*(buf + i)), (buf + i) + 1);
}

static void smb327_set_command(struct i2c_client *client,
				int reg, int datum)
{
	int val;
	u8 data = 0;
	val = smb327_i2c_read(client, reg, &data);
	if (val >= 0) {
		pr_debug("%s : reg(0x%02x): 0x%02x", __func__, reg, data);
		if (data != datum) {
			data = datum;
			if (smb327_i2c_write(client, reg, &data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb327_i2c_read(client, reg, &data);
			if (val >= 0)
				pr_debug(" => 0x%02x\n", data);
		}
	}
}
#endif

static void smb327_test_read(struct i2c_client *client)
{
	u8 data = 0;
	u32 addr = 0;
	for (addr = 0; addr < 0x0c; addr++) {
		smb327_i2c_read(client, addr, &data);
		pr_info("smb327 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
	for (addr = 0x31; addr < 0x3D; addr++) {
		smb327_i2c_read(client, addr, &data);
		pr_info("smb327 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
}

static void smb327_read_regs(struct i2c_client *client, char *str)
{
	u8 data = 0;
	u32 addr = 0;

	for (addr = 0x0; addr < 0x0A; addr++) {
		smb327_i2c_read(client, addr, &data);
		sprintf(str+strlen(str), "0x%x, ", data);
	}

	/* "#" considered as new line in application */
	sprintf(str+strlen(str), "#");

	for (addr = 0x31; addr < 0x39; addr++) {
		smb327_i2c_read(client, addr, &data);
		sprintf(str+strlen(str), "0x%x, ", data);
	}
}

static int smb327_get_charging_status(struct i2c_client *client)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 stat_c = 0;

	smb327_i2c_read(client, SMB327_BATTERY_CHARGING_STATUS_C, &stat_c);
	pr_debug("%s : Charging status C(0x%02x)\n", __func__, stat_c);

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

static int smb327_get_battery_present(struct i2c_client *client)
{
	u8 reg_data, irq_data;

	smb327_i2c_read(client, SMB327_BATTERY_CHARGING_STATUS_B, &reg_data);
	smb327_i2c_read(client, SMB327_INTERRUPT_STATUS_C, &irq_data);

	reg_data = ((reg_data & 0x01) && (irq_data & 0x10));

	return !reg_data;
}

static void smb327_set_writable(struct i2c_client *client, int writable)
{
	u8 reg_data;

	smb327_i2c_read(client, SMB327_COMMAND, &reg_data);

	if (writable)
		reg_data |= CMD_A_ALLOW_WRITE;
	else
		reg_data &= ~CMD_A_ALLOW_WRITE;

	smb327_i2c_write(client, SMB327_COMMAND, &reg_data);
}

static u8 smb327_set_charge_enable(struct i2c_client *client, int enable)
{
	u8 chg_en;
	u8 reg_data;

	reg_data = 0x00;

	smb327_i2c_write(client, SMB327_FUNCTION_CONTROL_C, &reg_data);

	smb327_i2c_read(client, SMB327_COMMAND, &chg_en);
	if (!enable)
		chg_en |= CMD_CHARGE_EN;
	else
		chg_en &= ~CMD_CHARGE_EN;

	smb327_i2c_write(client, SMB327_COMMAND, &chg_en);

	return chg_en;
}

static u8 smb327_set_float_voltage(struct i2c_client *client, int float_voltage)
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

	smb327_i2c_read(client,	SMB327_FLOAT_VOLTAGE, &reg_data);
	reg_data &= ~CFG_FLOAT_VOLTAGE_MASK;
	reg_data |= float_data << CFG_FLOAT_VOLTAGE_SHIFT;

	smb327_i2c_write(client, SMB327_FLOAT_VOLTAGE, &reg_data);

	return reg_data;
}

static u8 smb327_set_input_current_limit(struct i2c_client *client,
				int input_current)
{
	u8 curr_data, reg_data;

	curr_data = input_current < 500 ? 0x0 :
		input_current > 1200 ? 0x7 :
		(input_current - 500) / 100;

	smb327_i2c_read(client, SMB327_INPUT_CURRENTLIMIT, &reg_data);
	reg_data &= ~CFG_INPUT_CURRENT_MASK;
	reg_data |= curr_data << CFG_INPUT_CURRENT_SHIFT;

	smb327_i2c_write(client, SMB327_INPUT_CURRENTLIMIT, &reg_data);

	pr_info("%s: set current limit : 0x%x\n", __func__, reg_data);

	return reg_data;
}

static u8 smb327_set_termination_current_limit(struct i2c_client *client,
				int termination_current)
{
	u8 reg_data, term_data;

	term_data = termination_current < 25 ? 0x0 :
		termination_current > 200 ? 0x7 :
		(termination_current - 25) / 25;

	/* Charge completion termination current */
	smb327_i2c_read(client, SMB327_CHARGE_CURRENT, &reg_data);
	reg_data &= ~CFG_TERMINATION_CURRENT_MASK;
	reg_data |= term_data << CFG_TERMINATION_CURRENT_SHIFT;

	smb327_i2c_write(client, SMB327_CHARGE_CURRENT, &reg_data);

	/* set STAT assertion termination current */
	smb327_i2c_read(client, SMB327_VARIOUS_FUNCTIONS, &reg_data);
	reg_data &= ~CFG_STAT_ASSETION_TERM_MASK;
	reg_data |= term_data << CFG_STAT_ASSETION_TERM_SHIFT;

	smb327_i2c_write(client, SMB327_VARIOUS_FUNCTIONS, &reg_data);

	return reg_data;
}

static u8 smb327_set_fast_charging_current(struct i2c_client *client,
				int fast_charging_current)
{
	u8 reg_data, chg_data;

	chg_data = fast_charging_current < 500 ? 0x0 :
		fast_charging_current > 1200 ? 0x7 :
		(fast_charging_current - 500) / 100;

	smb327_i2c_read(client, SMB327_CHARGE_CURRENT, &reg_data);
	reg_data &= ~CFG_CHARGE_CURRENT_MASK;
	reg_data |= chg_data << CFG_CHARGE_CURRENT_SHIFT;

	smb327_i2c_write(client, SMB327_CHARGE_CURRENT, &reg_data);

	pr_info("%s: Charge Current : 0x%x\n", __func__, reg_data);

	return reg_data;
}

static void smb327_charger_function_control(struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 reg_data, charge_mode;

	smb327_set_writable(client, 1);

	if (system_rev < 10)
		reg_data = 0x4E;
	else
		reg_data = 0x6E;

	smb327_i2c_write(client, SMB327_FUNCTION_CONTROL_B, &reg_data);

	smb327_i2c_read(client,
			SMB327_INTERRUPT_SIGNAL_SELECTION, &reg_data);
	reg_data |= 0x1;
	smb327_i2c_write(client,
			SMB327_INTERRUPT_SIGNAL_SELECTION, &reg_data);

	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		/* turn off charger */
		smb327_set_charge_enable(client, 0);
	} else {
		pr_info("%s: Input : %d, Charge : %d\n", __func__,
			charger->pdata->charging_current[charger->cable_type].input_current_limit,
			charger->charging_current);
		/* AICL enable */
		smb327_i2c_read(client, SMB327_INPUT_CURRENTLIMIT, &reg_data);
		reg_data &= ~CFG_AICL_ENABLE;
		reg_data &= ~CFG_AICL_VOLTAGE; /* AICL enable voltage 4.25V */
		smb327_i2c_write(client, SMB327_INPUT_CURRENTLIMIT, &reg_data);

		/* Function control A */
		reg_data = 0xDA;
		smb327_i2c_write(client, SMB327_FUNCTION_CONTROL_A, &reg_data);

		/* 4.2V float voltage */
		smb327_set_float_voltage(client,
			charger->pdata->chg_float_voltage);

		/* Set termination current */
		smb327_set_termination_current_limit(client,
			charger->pdata->charging_current[charger->
			cable_type].full_check_current_1st);

		smb327_set_input_current_limit(client,
			charger->pdata->charging_current
                        [charger->cable_type].input_current_limit);

		smb327_set_fast_charging_current(client,
			charger->charging_current);

		/* SET USB5/1, AC/USB Mode */
		charge_mode = (charger->cable_type == POWER_SUPPLY_TYPE_MAINS) ||
					(charger->cable_type == POWER_SUPPLY_TYPE_UARTOFF) ?
					0x3 : 0x2;
		smb327_i2c_read(client, SMB327_COMMAND, &reg_data);
		reg_data &= ~0x0C;
		reg_data |= charge_mode << 2;
		smb327_i2c_write(client, SMB327_COMMAND, &reg_data);

		smb327_set_charge_enable(client, 1);
	}
	smb327_test_read(client);
	smb327_set_writable(client, 0);
}

static int smb327_get_charging_health(struct i2c_client *client)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 status_reg, data;

	smb327_i2c_read(client, SMB327_INTERRUPT_STATUS_C, &status_reg);
	pr_info("%s : Interrupt status C(0x%02x)\n", __func__, status_reg);

	/* Is enabled ? */
	if (status_reg & 0x2) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (status_reg & 0x4) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (status_reg & 0x80) { /* watchdog enable workaround */
		/* clear IRQ */
		data = 0xFF;
		smb327_i2c_write(client, 0x30, &data);
		/* reset function control */
		smb327_charger_function_control(client);
	}

	return (int)health;
}

static int smb327_get_charge_now(struct i2c_client *client)
{
	u8 chg_now, data, addr;

	smb327_i2c_read(client, SMB327_BATTERY_CHARGING_STATUS_A, &chg_now);

	for (addr = 0; addr < 0x0c; addr++) {
		smb327_i2c_read(client, addr, &data);
		pr_info("smb327 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
	for (addr = 0x31; addr <= 0x3D; addr++) {
		smb327_i2c_read(client, addr, &data);
		pr_info("smb327 addr : 0x%02x data : 0x%02x\n", addr, data);
	}

	/* Clear IRQ */
	data = 0xFF;
	smb327_i2c_write(client, 0x30, &data);

	return (chg_now & 0x2);
}

static void smb327_charger_otg_control(struct i2c_client *client, int enable)
{
	u8 reg_data;

	smb327_i2c_read(client, SMB327_OTG_POWER_AND_LDO, &reg_data);

	if (!enable)
		reg_data |= CFG_OTG_ENABLE;
	else
		reg_data &= ~CFG_OTG_ENABLE;

	smb327_set_writable(client, 1);
	smb327_i2c_write(client, SMB327_OTG_POWER_AND_LDO, &reg_data);
	smb327_set_writable(client, 0);
}

#if 0 // to fix build error - unused function 
static void smb327_irq_enable(struct i2c_client *client)
{
	u8 data;

	smb327_set_writable(client, 1);

	if (system_rev < 10)
		data = 0x4D;
	else
		data = 0x6D;

	smb327_i2c_write(client, 0x04, &data);

	smb327_i2c_read(client,
			SMB327_INTERRUPT_SIGNAL_SELECTION, &data);
	data |= 0x1;
	smb327_i2c_write(client,
			SMB327_INTERRUPT_SIGNAL_SELECTION, &data);

	smb327_set_writable(client, 0);
}
#endif 

static void smb327_irq_disable(struct i2c_client *client)
{
	u8 data;

	smb327_set_writable(client, 1);

	if (system_rev < 10)
		data = 0x4E;
	else
		data = 0x6E;

	smb327_i2c_write(client, 0x04, &data);

	smb327_i2c_read(client,
			SMB327_INTERRUPT_SIGNAL_SELECTION, &data);
	data &= 0xfe;
	smb327_i2c_write(client,
			SMB327_INTERRUPT_SIGNAL_SELECTION, &data);

	smb327_set_writable(client, 0);
}

static int smb327_debugfs_show(struct seq_file *s, void *data)
{
	struct sec_charger_info *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "SMB CHARGER IC :\n");
	seq_printf(s, "==================\n");
	for (reg = 0x00; reg <= 0x0A; reg++) {
		smb327_i2c_read(charger->client, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	for (reg = 0x30; reg <= 0x39; reg++) {
		smb327_i2c_read(charger->client, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");
	return 0;
}

static int smb327_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, smb327_debugfs_show, inode->i_private);
}

static const struct file_operations smb327_debugfs_fops = {
	.open           = smb327_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

bool sec_hal_chg_init(struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	smb327_irq_disable(client);

	(void) debugfs_create_file("smb327-regs",
		S_IRUGO, NULL, (void *)charger, &smb327_debugfs_fops);

	return true;
}

bool sec_hal_chg_suspend(struct i2c_client *client)
{
	return true;
}

bool sec_hal_chg_resume(struct i2c_client *client)
{
	return true;
}

bool sec_hal_chg_shutdown(struct i2c_client *client)
{
	smb327_charger_otg_control(client, 1);

	return true;
}

bool sec_hal_chg_get_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = smb327_get_charging_status(client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!charger->is_charging)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = smb327_get_charging_health(client);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = smb327_get_battery_present(client);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		smb327_irq_disable(client);
		if (charger->charging_current) {
			smb327_i2c_read(client, SMB327_CHARGE_CURRENT, &data);
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
		val->intval = smb327_get_charge_now(client);
		break;
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
#if defined(SIOP_CHARGING_CURRENT_LIMIT_FEATURE)
	int fast_charging_current =
		charger->pdata->charging_current[charger->cable_type].fast_charging_current;
#endif
	switch (psp) {
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
#if defined(SIOP_CHARGING_CURRENT_LIMIT_FEATURE)
		if (charger->is_charging &&
			charger->charging_current > 0 &&
			fast_charging_current > SIOP_CHARGING_LIMIT_CURRENT) {
			if (charger->charging_current < fast_charging_current &&
				charger->charging_current != SIOP_CHARGING_LIMIT_CURRENT) {
				pr_info("%s: current now : %d\n",
					__func__, charger->charging_current);
				charger->charging_current = SIOP_CHARGING_LIMIT_CURRENT;
				pr_info("%s: current siop-set : %d\n",
					__func__, charger->charging_current);
				smb327_charger_function_control(client);
				is_siop_limited = true;
			} else if (charger->charging_current == fast_charging_current &&
					is_siop_limited) {
				pr_info("%s: current siop-recovery : %d\n",
					__func__, charger->charging_current);
				smb327_charger_function_control(client);
				is_siop_limited = false;
			}
		} else {
			is_siop_limited = false;
		}
		break;
#endif
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		if (charger->charging_current < 0)
			smb327_charger_otg_control(client, 1);
		else if (charger->charging_current > 0) {
			smb327_charger_function_control(client);
			smb327_charger_otg_control(client, 1);
		 } else {
			smb327_charger_function_control(client);
			smb327_charger_otg_control(client, 0);
		}
		break;
	default:
		return false;
	}
	return true;
}

ssize_t sec_hal_chg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_charger_info *chg =
		container_of(psy, struct sec_charger_info, psy_chg);
	int i = 0;
	char *str = NULL;

	switch (offset) {
/*	case CHG_REG: */
/*		break; */
	case CHG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%02x\n",
			chg->reg_data);
		break;
	case CHG_REGS:
		str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		smb327_read_regs(chg->client, str);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			str);

		kfree(str);
		break;
	default:
		i = -EINVAL;
	}

	return i;
}

ssize_t sec_hal_chg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct sec_charger_info *chg =
		container_of(psy, struct sec_charger_info, psy_chg);
	int ret = 0;
	int x = 0;
	u8 data = 0;

	switch (offset) {
	case CHG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			chg->reg_addr = x;
			smb327_i2c_read(chg->client,
				chg->reg_addr, &data);
			chg->reg_data = data;
			pr_debug("%s: (read) addr = 0x%x, data = 0x%x\n",
				__func__, chg->reg_addr, chg->reg_data);
		}
		ret = count;
		break;
	case CHG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			data = (u8)x;
			pr_debug("%s: (write) addr = 0x%x, data = 0x%x\n",
				__func__, chg->reg_addr, data);
			smb327_set_writable(chg->client, 1);
			smb327_i2c_write(chg->client, chg->reg_addr, &data);
			smb327_set_writable(chg->client, 0);
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

