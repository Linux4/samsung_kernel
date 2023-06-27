/*
 *  bq24157_charger.c
 *  Samsung BQ24157 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
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
#if defined(CONFIG_MACH_BAFFINQ)
#include <mach/mfp-pxa1088-baffinq.h>
#else
#include <mach/mfp-pxa1088-wilcox.h>
#endif

static int bq24157_i2c_write(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	return ret;
}

static int bq24157_i2c_read(struct i2c_client *client,
				int reg, u8 *buf)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	return ret;
}

static void bq24157_i2c_write_array(struct i2c_client *client,
				u8 *buf, int size)
{
	int i;
	for (i = 0; i < size; i += 3)
		bq24157_i2c_write(client, (u8) (*(buf + i)), (buf + i) + 1);
}

static void bq24157_test_read(struct i2c_client *client)
{
	u8 data = 0;
	u32 addr = 0;
	for (addr = 0; addr <= 0x06; addr++) {
		bq24157_i2c_read(client, addr, &data);
		dev_info(&client->dev,
			"bq24157 addr : 0x%02x data : 0x%02x\n", addr, data);
	}
}

static void bq24157_read_regs(struct i2c_client *client, char *str)
{
	u8 data = 0;
	u32 addr = 0;

	for (addr = 0; addr <= 0x06; addr++) {
		bq24157_i2c_read(client, addr, &data);
		sprintf(str+strlen(str), "0x%x, ", data);
	}
}


static int bq24157_get_charging_status(struct i2c_client *client)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 data = 0;

	bq24157_i2c_read(client, BQ24157_STATUS, &data);
	dev_info(&client->dev,
		"%s : charger status(0x%02x)\n", __func__, data);

	data = (data & 0x30);

	switch (data) {
	case 0x00:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 0x10:
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x20:
		status = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x30:
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	}

	return (int)status;
}

static int bq24157_get_charging_health(struct i2c_client *client)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;
	u8 data = 0;

	bq24157_i2c_read(client, BQ24157_STATUS, &data);
	dev_info(&client->dev,
		"%s : charger status(0x%02x)\n", __func__, data);

	if ((data & 0x30) == 0x30) {	/* check for fault */
		data = (data & 0x07);

		switch (data) {
		case 0x01:
			health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			break;
		case 0x03:
			health = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
			break;
		}
	}

	return (int)health;
}

static u8 bq24157_set_float_voltage_data(
	struct i2c_client *client, int float_voltage)
{
	u8 data;
	u8 float_reg;

	bq24157_i2c_read(client, BQ24157_VOLTAGE, &data);

	data &= ~FLOAT_VOLTAGE_MASK;

	if (float_voltage < 3500)
		float_voltage = 3500;

	float_reg = (float_voltage - 3500) / 20;
	data |= (float_reg << 2);

	bq24157_i2c_write(client, BQ24157_VOLTAGE, &data);

	return float_reg;
}

static u8 bq24157_set_input_current_limit_data(
	struct i2c_client *client, int input_current)
{
	u8 data;
	u8 input_current_reg;

	bq24157_i2c_read(client, BQ24157_CONTROL, &data);

	data &= ~INPUT_CURRENT_MASK;

	if (input_current <= 100)
		input_current_reg = 0x00;
	else if (input_current <= 500)
		input_current_reg = 0x01;
	else if (input_current <= 800)
		input_current_reg = 0x02;
	else	/* No limit */
		input_current_reg = 0x03;

	data |= (input_current_reg << 6);

	bq24157_i2c_write(client, BQ24157_CONTROL, &data);

	return input_current_reg;
}

static u8 bq24157_set_termination_current_limit_data(
	struct i2c_client *client, int termination_current)
{
	u8 data;
	u8 termination_reg;

	/* Rsns 0.068 Ohm */
	/* default offset 50mA */
	bq24157_i2c_read(client, BQ24157_CURRENT, &data);

	data &= ~TERMINATION_CURRENT_MASK;

	termination_reg = (termination_current - 50) / 50;
	data |= termination_reg;

	bq24157_i2c_write(client, BQ24157_CURRENT, &data);

	return data;
}

static u8 bq24157_set_fast_charging_current_data(
	struct i2c_client *client, int fast_charging_current)
{
	u8 data;
	u8 fast_charging_reg;

	/* Rsns 0.068 Ohm */
	/* default offset 550mA */
	bq24157_i2c_read(client, BQ24157_CURRENT, &data);

	data &= ~FAST_CHARGING_CURRENT_MASK;
	if(fast_charging_current<550)
			fast_charging_current = 550;
	else if (fast_charging_current>1550)
		fast_charging_current = 1550;

	fast_charging_reg = (fast_charging_current - 550) / 100;
	data |= (fast_charging_reg << 4);

	bq24157_i2c_write(client, BQ24157_CURRENT, &data);

	return fast_charging_reg;
}

static void bq24157_set_safety_limits(struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;

	bq24157_i2c_read(client, BQ24157_CURRENT, &data);
	data |= ((charger->pdata->chg_float_voltage - 4200) / 20);
	bq24157_i2c_write(client, BQ24157_SAFETY, &data);
}

static void bq24157_charger_function_conrol(
				struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	union power_supply_propval val;
	int full_check_type;
	u8 data;

	if (charger->charging_current < 0) {
		dev_dbg(&client->dev,
			"%s : OTG is activated. Ignore command!\n", __func__);
		return;
	}

	if (charger->cable_type ==
		POWER_SUPPLY_TYPE_BATTERY) {
		gpio_direction_output(mfp_to_gpio(GPIO098_GPIO_98), 1);
		/* Do Nothing */
	} else {
		dev_dbg(&client->dev, "%s : float voltage (%dmV)\n",
			__func__, charger->pdata->chg_float_voltage);
		bq24157_set_float_voltage_data(
			client, charger->pdata->chg_float_voltage);

#if defined(CONFIG_MACH_BAFFIN)
		dev_dbg(&client->dev, "%s : fast charging current (%dmA),siop_level=%d\n",
				__func__, charger->charging_current,charger->pdata->siop_level);
		bq24157_set_fast_charging_current_data(
			client, charger->charging_current*charger->pdata->siop_level/100);
#elif defined(CONFIG_MACH_WILCOX) || defined(CONFIG_MACH_BAFFINQ)
		if ((charger->pdata->siop_level) &&
		    ((charger->cable_type == POWER_SUPPLY_TYPE_MAINS) ||
		     (charger->cable_type == POWER_SUPPLY_TYPE_MISC)))
			charger->charging_current = 50 + charger->pdata->siop_level*7;

		dev_dbg(&client->dev, "%s : fast charging current (%dmA), siop_level=%d\n",
				__func__, charger->charging_current, charger->pdata->siop_level);

		bq24157_set_fast_charging_current_data(
			client, charger->charging_current);
#else
		if ((charger->pdata->siop_level) &&
		    ((charger->cable_type == POWER_SUPPLY_TYPE_MAINS) ||
		     (charger->cable_type == POWER_SUPPLY_TYPE_MISC)))
			charger->charging_current = 50 + charger->pdata->siop_level*10;

		dev_dbg(&client->dev, "%s : fast charging current (%dmA), siop_level=%d\n",
				__func__, charger->charging_current, charger->pdata->siop_level);

		bq24157_set_fast_charging_current_data(
			client, charger->charging_current);
#endif

		dev_dbg(&client->dev, "%s : termination current (%dmA)\n",
			__func__, charger->pdata->charging_current[
			charger->cable_type].full_check_current_1st);
		bq24157_set_termination_current_limit_data(
			client, charger->pdata->charging_current[
				charger->cable_type].full_check_current_1st);

		/* Input current limit */
		dev_dbg(&client->dev, "%s : input current (%dmA)\n",
			__func__, charger->pdata->charging_current
			[charger->cable_type].input_current_limit);
		bq24157_set_input_current_limit_data(
			client, charger->pdata->charging_current
			[charger->cable_type].input_current_limit);

		/* Special Charger Voltage : 4.520V
		 * Normal charge current
		 */
		bq24157_i2c_read(client, BQ24157_SPECIAL, &data);
		data &= 0xdf;
		bq24157_i2c_write(client, BQ24157_SPECIAL, &data);

		/* Enable charging & Termanation setting */
		bq24157_i2c_read(client, BQ24157_CONTROL, &data);
		data &= 0xf2;

		gpio_direction_output(mfp_to_gpio(GPIO098_GPIO_98), 0);
#if 1
		psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_NOW, val);
		if (val.intval == SEC_BATTERY_CHARGING_1ST)
			full_check_type = charger->pdata->full_check_type;
		else
			full_check_type = charger->pdata->full_check_type_2nd;
		/* Termination setting */
		switch (full_check_type) {
		case SEC_BATTERY_FULLCHARGED_CHGGPIO:
		case SEC_BATTERY_FULLCHARGED_CHGINT:
		case SEC_BATTERY_FULLCHARGED_CHGPSY:
			/* Enable Current Termination */
			data |= 0x08;
			break;
		}
#endif
		bq24157_i2c_write(client, BQ24157_CONTROL, &data);

		gpio_direction_output(mfp_to_gpio(GPIO098_GPIO_98), 0);
	}
}

static void bq24157_charger_otg_conrol(
				struct i2c_client *client)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;

	if (charger->cable_type ==
		POWER_SUPPLY_TYPE_BATTERY) {
		dev_info(&client->dev, "%s : turn off OTG\n", __func__);
		/* turn off OTG */
		bq24157_i2c_read(client, BQ24157_VOLTAGE, &data);
		data &= 0xfe;
		bq24157_i2c_write(client, BQ24157_VOLTAGE, &data);
	} else {
		dev_info(&client->dev, "%s : turn on OTG\n", __func__);
		/* turn on OTG */
		bq24157_i2c_read(client, BQ24157_VOLTAGE, &data);
		data |= 0x01;
		bq24157_i2c_write(client, BQ24157_VOLTAGE, &data);
	}
}

bool sec_hal_chg_init(struct i2c_client *client)
{
	u8 data = 0x02;
	bq24157_i2c_write(client, BQ24157_VOLTAGE, &data);
	bq24157_test_read(client);
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

bool sec_hal_chg_get_property(struct i2c_client *client,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct sec_charger_info *charger = i2c_get_clientdata(client);
	u8 data;
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq24157_get_charging_status(client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bq24157_get_charging_health(client);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			/* Rsns 0.068 Ohm */
			bq24157_i2c_read(client, BQ24157_CURRENT, &data);
			val->intval = ((data & 0x78) >> 3) * 100;
		} else
			val->intval = 0;
		dev_dbg(&client->dev,
			"%s : set-current(%dmA), current now(%dmA)\n",
			__func__, charger->charging_current, val->intval);
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
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current < 0)
			bq24157_charger_otg_conrol(client);
		else if (charger->charging_current > 0)
			bq24157_charger_function_conrol(client);
		else {
			bq24157_charger_function_conrol(client);
			bq24157_charger_otg_conrol(client);
		}
		bq24157_test_read(client);
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
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
			chg->reg_data);
		break;
	case CHG_REGS:
		str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		bq24157_read_regs(chg->client, str);
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
			bq24157_i2c_read(chg->client,
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
			bq24157_i2c_write(chg->client,
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
