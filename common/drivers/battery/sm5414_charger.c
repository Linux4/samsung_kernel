/*
 *  SM5414_charger.c
 *  SiliconMitus SM5414 Charger Driver
 *
 *  Copyright (C) 2013 SiliconMitus
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define DEBUG

#include <linux/battery/sec_charger.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/gpio-pxa.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

static int charger_health = POWER_SUPPLY_HEALTH_GOOD;

static int SM5414_i2c_write(struct i2c_client *client,
		int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

	return ret;
}

static int SM5414_i2c_read(struct i2c_client *client,
		int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

	return ret;
}

#if 0
static void SM5414_test_read(struct i2c_client *client)
{
	u8 data = 0;
	u32 addr = 0;

    //0x00~02 are R/C
	for (addr = SM5414_INTMASK1; addr <= SM5414_CHGCTRL5; addr++) {
		SM5414_i2c_read(client, addr, &data);
		dev_info(&client->dev,
			"SM5414 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
}
#endif

static void SM5414_read_regs(struct i2c_client *client, char *str)
{
	u8 data = 0;
	u32 addr = 0;

	//0x00~02 are R/C (read and clear)
	for (addr = SM5414_INTMASK1; addr <= SM5414_CHGCTRL5; addr++) {
		SM5414_i2c_read(client, addr, &data);
		sprintf(str+strlen(str), "0x%x, ", data);
	}
}

static int SM5414_get_battery_present(struct i2c_client *client)
{
	u8 data;

	SM5414_i2c_read(client, SM5414_INT2, &data);

	pr_info("%s: SM5414_INT2 (0x%02x)\n", __func__, data);

	data = ((data & SM5414_INT2_NOBAT) >> 6);

	return data;
}

static int SM5414_get_charging_status(struct i2c_client *client)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	int nCHG;
	u8 int2, chg_en;
	union power_supply_propval value;

	SM5414_i2c_read(client, SM5414_INT2, &int2);
	SM5414_i2c_read(client, SM5414_CTRL, &chg_en);

	if((int2 & SM5414_INT2_DONE) || (int2 & SM5414_INT2_TOPOFF)) {
		psy_do_property("sec-fuelgauge", get,
				POWER_SUPPLY_PROP_CAPACITY, value);
		if ((value.intval > 94) &&
			(charger->cable_type != POWER_SUPPLY_TYPE_BATTERY)) {
#if defined(CONFIG_MACH_YOUNG23GDTV)
			psy_do_property("sec-fuelgauge", get,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
			if (value.intval >= 4150) {
				status = POWER_SUPPLY_STATUS_FULL;
				charger->is_fullcharged = true;
				dev_info(&client->dev,
					"%s : Power Supply Full\n", __func__);
			}
#else
			status = POWER_SUPPLY_STATUS_FULL;
			charger->is_fullcharged = true;
			dev_info(&client->dev,
				"%s : Power Supply Full\n", __func__);
#endif
		}
	} else if (chg_en & CHARGE_EN) {
		nCHG = gpio_get_value(charger->pdata->chg_gpio_en);
		if ((nCHG) || (charger_health != POWER_SUPPLY_HEALTH_GOOD))
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	return (int)status;
}

int sec_get_charging_health(struct i2c_client *client)
{
	static int health = POWER_SUPPLY_HEALTH_GOOD;
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 int1;
	int vf_status;

	SM5414_i2c_read(client, SM5414_INT1, &int1);

	dev_info(&client->dev,
		"%s : SM5414_INT1 : 0x%02x\n", __func__, int1);

#if defined(CONFIG_MACH_YOUNG23GDTV) || defined(CONFIG_MACH_J13G)
	/* VF check by adc detection */
	if (!sec_vf_adc_check())
		return POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
#endif

	if (int1 & SM5414_INT1_VBUSOVP) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (int1 & SM5414_INT1_VBUSUVLO) {
		msleep(1000);
		if (charger->cable_type != POWER_SUPPLY_TYPE_BATTERY) {
			health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			charger_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		}
	} else if (int1 & SM5414_INT1_VBUSINOK) {
		health = POWER_SUPPLY_HEALTH_GOOD;
		charger_health = POWER_SUPPLY_HEALTH_GOOD;
	}

	return (int)health;
}

static u8 SM5414_set_float_voltage_data(
		struct i2c_client *client, int float_voltage)
{
	u8 data, float_reg;

	SM5414_i2c_read(client, SM5414_CHGCTRL3, &data);

	data &= BATREG_MASK;

	if (float_voltage < 4100)
		float_voltage = 4100;
	else if (float_voltage > 4475)
		float_voltage = 4475;

	float_reg = (float_voltage - 4100) / 25;
	data |= (float_reg << 4);

	SM5414_i2c_write(client, SM5414_CHGCTRL3, &data);

	SM5414_i2c_read(client, SM5414_CHGCTRL3, &data);
	dev_dbg(&client->dev,
		"%s : SM5414_CHGCTRL3 (float) : 0x%02x\n", __func__, data);

	return data;
}

static u8 SM5414_set_input_current_limit_data(
		struct i2c_client *client, int input_current)
{
	u8 set_reg, curr_reg = 0;
	u8 chg_en;
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	if(input_current < 100)
		input_current = 100;
	else if (input_current >= 2050)
		input_current = 2050;

	set_reg = (input_current - 100) / 50;

	curr_reg = ((input_current % 100) >= 50) ? 1 : 0;

	chg_en = gpio_get_value(charger->pdata->chg_gpio_en);

	if (!chg_en) {
		SM5414_i2c_write(client, SM5414_VBUSCTRL, &set_reg);
	} else {
		while (set_reg >= curr_reg) {
			SM5414_i2c_write(client, SM5414_VBUSCTRL, &curr_reg);
			curr_reg += 2;
			msleep(50);
		};
	}

	SM5414_i2c_read(client, SM5414_VBUSCTRL, &curr_reg);
	dev_dbg(&client->dev,
		"%s : SM5414_VBUSCTRL (Input limit) : 0x%02x\n",
		__func__, curr_reg);

	return curr_reg;
}

static u8 SM5414_set_topoff_current_limit_data(
		struct i2c_client *client, int topoff_current)
{
	u8 data;
	u8 topoff_reg;

	SM5414_i2c_read(client, SM5414_CHGCTRL4, &data);

	data &= TOPOFF_MASK;

	if(topoff_current < 100)
		topoff_current = 100;
	else if (topoff_current > 650)
		topoff_current = 650;

	topoff_reg = (topoff_current - 100) / 50;
	data = data | (topoff_reg<<3);

	SM5414_i2c_write(client, SM5414_CHGCTRL4, &data);

	SM5414_i2c_read(client, SM5414_CHGCTRL4, &data);
	dev_dbg(&client->dev,
		"%s : SM5414_CHGCTRL4 (Top-off limit) : 0x%02x\n",
		__func__, data);

	return data;
}

static u8 SM5414_set_fast_charging_current_data(
		struct i2c_client *client, int fast_charging_current)
{
	u8 data = 0;

	if(fast_charging_current < 100)
		fast_charging_current = 100;
	else if (fast_charging_current > 2500)
		fast_charging_current = 2500;

	data = (fast_charging_current - 100) / 50;

	SM5414_i2c_write(client, SM5414_CHGCTRL2, &data);
	SM5414_i2c_read(client, SM5414_CHGCTRL2, &data);

	dev_dbg(&client->dev,
		"%s : SM5414_CHGCTRL2 (fast) : 0x%02x\n", __func__, data);

	return data;
}

static u8 SM5414_set_toggle_charger(struct i2c_client *client, int enable)
{
	u8 chg_en=0;
	u8 data=0;

	SM5414_i2c_read(client, SM5414_CTRL, &chg_en);
	if (enable)
		chg_en |= CHARGE_EN;
	else
		chg_en &= ~CHARGE_EN;

	SM5414_i2c_write(client, SM5414_CTRL, &chg_en);

	dev_info(&client->dev, "%s: SM5414 Charger toggled!! \n", __func__);

	SM5414_i2c_read(client, SM5414_CTRL, &chg_en);
	dev_info(&client->dev,
		"%s : chg_en value(07h register): 0x%02x\n", __func__, chg_en);

	SM5414_i2c_read(client, SM5414_CHGCTRL2, &data);
	dev_info(&client->dev,
		"%s : SM5414_CHGCTRL2 value: 0x%02x", __func__, data);

	return chg_en;
}

static void SM5414_charger_function_control(
				struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	union power_supply_propval value;
	u8 suspend_en;

	if (charger->charging_current < 0) {
		dev_info(&client->dev,
			"%s : OTG is activated. Ignore command!\n", __func__);
		return;
	}

	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		/* Disable Charger */
		dev_info(&client->dev,
			"%s : Disable Charger, Battery Supply!\n", __func__);
		// nCHG_EN is logic low so set 1 to disable charger
		charger->is_fullcharged = false;
		gpio_direction_output((charger->pdata->chg_gpio_en), 1);
	} else {
		psy_do_property("sec-fuelgauge", get,
				POWER_SUPPLY_PROP_CAPACITY, value);
		if (value.intval > 0) {
			SM5414_i2c_read(client, SM5414_CTRL, &suspend_en);

			/* Suspend enable for register reset */
			suspend_en |= 0x04;
			SM5414_i2c_write(client, SM5414_CTRL, &suspend_en);
			msleep(20);
			suspend_en &= ~0x04;
			SM5414_i2c_write(client, SM5414_CTRL, &suspend_en);
		}

		dev_info(&client->dev, "%s : float voltage (%dmV)\n",
			__func__, charger->pdata->chg_float_voltage);

		/* Set float voltage */
		SM5414_set_float_voltage_data(
			client, charger->pdata->chg_float_voltage);

		dev_info(&client->dev, "%s : topoff current (%dmA)\n",
			__func__, charger->pdata->charging_current[
			charger->cable_type].full_check_current_1st);
		SM5414_set_topoff_current_limit_data(
			client, charger->pdata->charging_current[
				charger->cable_type].full_check_current_1st);

		SM5414_set_input_current_limit_data(client, 100);
		// nCHG_EN is logic low so set 0 to enable charger
		gpio_direction_output((charger->pdata->chg_gpio_en), 0);
		msleep(100);

		/* Young2 DTV event charging set */
		if (charger->pdata->siop_level == CALL_EVENT_SIOP) {
			charger->charging_current = CALL_EVENT_CURRENT;
			charger->input_current_limit = CALL_EVENT_CURRENT;
		} else if (charger->pdata->siop_level == HIGH_TEMP_SIOP) {
			charger->charging_current = HIGH_TEMP_CURRENT;
			charger->input_current_limit = HIGH_TEMP_CURRENT;
		} else {
			charger->charging_current =
				charger->pdata->charging_current
				[charger->cable_type].fast_charging_current;
			charger->input_current_limit =
				charger->pdata->charging_current
				[charger->cable_type].input_current_limit;
		}

		/* Input current limit */
		dev_info(&client->dev, "%s : input current (%dmA)\n",
			__func__, charger->input_current_limit);

		SM5414_set_input_current_limit_data(
			client, charger->input_current_limit);

		/* Set fast charge current */
		dev_info(&client->dev, "%s : fast charging current (%dmA), siop_level=%d\n",
			__func__, charger->charging_current, charger->pdata->siop_level);
		SM5414_set_fast_charging_current_data(
			client, charger->charging_current);

		dev_info(&client->dev,
			"%s : Enable Charger!\n", __func__);
	}
}

static void SM5414_charger_otg_control(
				struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;
//turn on/off ENBOOST
	if (charger->cable_type ==
		POWER_SUPPLY_TYPE_BATTERY) {
		dev_info(&client->dev, "%s : turn off OTG\n", __func__);
		/* turn off OTG */
		SM5414_i2c_read(client, SM5414_CTRL, &data);
		data &= 0xfe;
		SM5414_i2c_write(client, SM5414_CTRL, &data);
	} else {
		dev_info(&client->dev, "%s : turn on OTG\n", __func__);
		/* turn on OTG */
		SM5414_i2c_read(client, SM5414_CTRL, &data);
		data |= 0x01;
		SM5414_i2c_write(client, SM5414_CTRL, &data);
	}
}

static int SM5414_debugfs_show(struct seq_file *s, void *data)
{
	struct sec_charger_info *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "SM CHARGER IC :\n");
	seq_printf(s, "==================\n");
	for (reg = SM5414_INTMASK1; reg <= SM5414_CHGCTRL5; reg++) {
		SM5414_i2c_read(charger->client, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");
	return 0;
}

static int SM5414_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, SM5414_debugfs_show, inode->i_private);
}

static const struct file_operations SM5414_debugfs_fops = {
	.open           = SM5414_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

bool sec_hal_chg_init(struct i2c_client *client)
{
	u8 reg_data;
	u8 int1 = 0;
	u8 chg_en = 0;
	struct sec_charger_info *charger = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s: SM5414 Charger init (Starting)!! \n", __func__);
	charger->is_fullcharged = false;

	SM5414_i2c_read(client, SM5414_CTRL, &chg_en);
	chg_en |= CHARGE_EN;
	SM5414_i2c_write(client, SM5414_CTRL, &chg_en);
	msleep(50);
	SM5414_i2c_read(client, SM5414_CTRL, &chg_en);
	dev_info(&client->dev,
		"%s : chg_en value(07h register): 0x%02x\n", __func__, chg_en);

	SM5414_i2c_read(client, SM5414_INT1, &int1);
	dev_info(&client->dev,
		"%s : SM5414_INT1 : 0x%02x\n", __func__, int1);

	reg_data = 0x1F;
	SM5414_i2c_write(client, SM5414_INTMASK1, &reg_data);

	reg_data = 0xFC;
	SM5414_i2c_write(client, SM5414_INTMASK2, &reg_data);

	SM5414_i2c_read(client, SM5414_CHGCTRL1, &reg_data);
	reg_data &= ~SM5414_CHGCTRL1_AUTOSTOP;
	SM5414_i2c_write(client, SM5414_CHGCTRL1, &reg_data);

	(void) debugfs_create_file("SM5414_regs",
		S_IRUGO, NULL, (void *)charger, &SM5414_debugfs_fops);

	return true;
}

bool sec_hal_chg_suspend(struct i2c_client *client)
{
	dev_info(&client->dev,
                "%s: CHARGER - SM5414(suspend mode)!!\n", __func__);

	return true;
}

bool sec_hal_chg_resume(struct i2c_client *client)
{
	dev_info(&client->dev,
                "%s: CHARGER - SM5414(resume mode)!!\n", __func__);

	return true;
}

bool sec_hal_chg_shutdown(struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	dev_info(&client->dev,
		"%s: CHARGER - SM5414(suspend)!!\n", __func__);
	SM5414_set_toggle_charger(client, 1);
	gpio_direction_output((charger->pdata->chg_gpio_en), 0);
}

bool sec_hal_chg_get_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;
	u8 int2;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (charger->is_fullcharged) {
			SM5414_i2c_read(client, SM5414_INT2, &int2);
			val->intval = POWER_SUPPLY_STATUS_FULL;
		} else
			val->intval = SM5414_get_charging_status(client);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = charger_health;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		break;
	// Have to mention the issue about POWER_SUPPLY_PROP_CHARGE_NOW to Marvel
	case POWER_SUPPLY_PROP_CHARGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			SM5414_i2c_read(client, SM5414_VBUSCTRL, &data);
			data &= 0x3f;
			val->intval = (100 + (data * 50));
			if (val->intval < 2050)
				val->intval = (100 + (data * 50));
			/*dev_dbg(&client->dev,
				"%s 1: set-current(%dmA), current now(%dmA)\n",
				__func__, charger->charging_current, val->intval);*/
		} else {
			val->intval = 100;
			/*dev_dbg(&client->dev,
				"%s 2: set-current(%dmA), current now(%dmA)\n",
				__func__, charger->charging_current, val->intval);*/
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = SM5414_get_battery_present(client);
		break;
	case POWER_SUPPLY_PROP_POWER_STATUS:
		val->intval = POWER_SUPPLY_PWR_RDY_UNKNOWN;
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
	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		charger->pdata->siop_level = val->intval;
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		if (charger->charging_current < 0)
			SM5414_charger_otg_control(client);
		else if (charger->charging_current > 0)
			SM5414_charger_function_control(client);
		else {
			SM5414_charger_function_control(client);
			SM5414_charger_otg_control(client);
		}
		dev_dbg(&client->dev, "%s: PROP_ONLINE \n", __func__);
		break;
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		dev_dbg(&client->dev, "%s: PROP_CURRENT_NOW \n", __func__);
		break;
#ifdef CONFIG_CHARGER_SM5414
	case POWER_SUPPLY_PROP_HEALTH:
		charger_health = val->intval;
		break;
#endif
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
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
			chg->reg_data);
		break;
	case CHG_REGS:
		str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		SM5414_read_regs(chg->client, str);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			str);

		kfree(str);
		break;
	default:
		i = -EINVAL;
		break;
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
			SM5414_i2c_read(chg->client,
				chg->reg_addr, &data);
			chg->reg_data = data;
			dev_dbg(dev, "%s: (read) addr = 0x%x, data = 0x%x\n",
				__func__, chg->reg_addr, chg->reg_data);
			ret = count;
		}
		break;
	case CHG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			data = (u8)x;
			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%x\n",
				__func__, chg->reg_addr, data);
			SM5414_i2c_write(chg->client,
				chg->reg_addr, &data);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
