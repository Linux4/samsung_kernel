/*
 *  bq24773_charger.c
 *  Samsung bq24773 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/battery/charger/bq24773_charger.h>

#define ENABLE 1
#define DISABLE 0

static enum power_supply_property bq24773_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_PROP_USB_HC,
	POWER_SUPPLY_PROP_CHARGE_NOW,
};

static enum power_supply_property bq24773_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

int bq24773_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct bq24773_charger *bq24773 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bq24773->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&bq24773->i2c_lock);
	if (ret < 0) {
		pr_info("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}

int bq24773_read_word(struct i2c_client *i2c, u8 reg)
{
	struct bq24773_charger *bq24773 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bq24773->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&bq24773->i2c_lock);
	if (ret < 0)
		return ret;

	return ret;
}

int bq24773_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct bq24773_charger *bq24773 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bq24773->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&bq24773->i2c_lock);
	if (ret < 0)
		pr_info("%s reg(0x%x), ret(%d)\n",
			__func__, reg, ret);

	return ret;
}

int bq24773_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct bq24773_charger *bq24773 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bq24773->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&bq24773->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}

int bq24773_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct bq24773_charger *bq24773 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bq24773->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&bq24773->i2c_lock);
	return ret;
}

static void bq24773_test_read(struct bq24773_charger *charger)
{
	u8 reg;
	u8 reg_data;

	pr_info("bq24773 CHARGER IC :\n");
	pr_info("===================\n");
	for (reg = 0x00; reg <= 0x10; reg++) {
		bq24773_read_reg(charger->i2c, reg, &reg_data);
		pr_info("0x%02x:\t0x%02x\n", reg, reg_data);
	}

}

#if 0
static int bq24773_get_charge_current(struct bq24773_charger *charger)
{
	u16 data;
	u32 charge_current;

	data = bq24773_read_word(charger->i2c, BQ24773_CHG_CURR_1);
	charge_current = (data >> 5) * 64;

	return charge_current;
}
#endif

static int bq24773_get_input_current(struct bq24773_charger *charger)
{
	u8 data;
	u32 input_current;

	bq24773_read_reg(charger->i2c, BQ24773_INPUT_CURR, &data);
	input_current = data * 64;

	return input_current;
}

static void bq24773_set_charge_current(struct bq24773_charger *charger, int charging_current)
{
	u16 data;

	charging_current = charging_current / 64;
	pr_info("%s %d \n", __func__, charging_current);
	data = (charging_current << 6);
	pr_info("%s :::: 0x%02x:\t0x%04x \n", __func__, data, charging_current);

	bq24773_write_word(charger->i2c, BQ24773_CHG_CURR_1, data);
}

static void bq24773_set_input_current(struct bq24773_charger *charger, int input_current)
{
	u16 data;

	pr_info ("%s : SET INPUT CURRENT(%d)\n", __func__, input_current);

	data = input_current / 64;
	bq24773_write_word(charger->i2c, BQ24773_INPUT_CURR, data);
}

static void bq24773_set_max_voltage(struct bq24773_charger *charger, int float_voltage)
{
	u16 data;
	u32 max_voltage;

	max_voltage = float_voltage / 16;
	data = (max_voltage << 4);
	bq24773_write_word(charger->i2c, BQ24773_MAX_CHG_VOLT_1, data);
}

#if 0
static void bq24773_set_min_voltage(struct bq24773_charger *charger,
				    int min_voltage)
{
	u8 data;
	data = min_voltage / 256;
	bq24773_write_reg(charger->i2c, BQ24773_MIN_SYS_VOLT, data);
}

static void bq24773_set_charger_state(struct bq24773_charger *charger,
				      bool en)
{
	if (en) {
		bq24773_update_reg(charger->i2c, BQ24773_CHG_OPTION0_1, 0, 0x1);
	} else {
		bq24773_update_reg(charger->i2c, BQ24773_CHG_OPTION0_1, 1, 0x1);
	}
}
#endif

static void bq24773_charger_function_control(struct bq24773_charger *charger)
{
	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		charger->is_charging = false;
		charger->afc_detect = false;
		charger->chg_float_voltage = charger->pdata->chg_float_voltage;
		charger->charging_current = 0;
		charger->input_current =
			charger->pdata->charging_current
			[POWER_SUPPLY_TYPE_BATTERY].input_current_limit;

	} else {
		union power_supply_propval value;

		charger->is_charging = true;
		charger->afc_detect = false;
		charger->chg_float_voltage = charger->pdata->chg_float_voltage;
		charger->charging_current =
			charger->pdata->charging_current
			[charger->cable_type].fast_charging_current;
		charger->input_current =
			charger->pdata->charging_current
			[charger->cable_type].input_current_limit;

		psy_do_property("smart-fuelgauge", get,
				POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN, value);

#if 0
		if ((value.intval < 93) && (value.intval >= 80)) {
			charger->chg_float_voltage = charger->pdata->chg_float_voltage - (100 - value.intval) * 12;
			charger->charging_current = (charger->charging_current * value.intval) / 100;
		} else if (value.intval >= 26) {
			charger->chg_float_voltage = charger->pdata->chg_float_voltage - (100 * 29) / 10;
			charger->charging_current = (charger->charging_current * value.intval) / 100;
		} else if (value.intval >= 11) {
			charger->chg_float_voltage = charger->pdata->chg_float_voltage - (100 * 29) / 10;
			charger->charging_current = (charger->charging_current * 25) / 100;
		} else {
			charger->chg_float_voltage = charger->pdata->chg_float_voltage;
			charger->charging_current = 0;
		}
#endif

		pr_info ("%s : AGE_FORECAST(%d) FLOAT_VOLTAGE(%d) INPUT_CURR(%d) CHARGING_CURR(%d)\n",
			 __func__, value.intval, charger->chg_float_voltage, charger->input_current,
			 charger->charging_current);
	}

	bq24773_set_max_voltage(charger, charger->chg_float_voltage);
	bq24773_set_charge_current(charger, charger->charging_current);
	mdelay(100);
	bq24773_set_input_current(charger, charger->input_current);

	bq24773_test_read(charger);
}

static void bq24773_charger_initialize(struct bq24773_charger *charger)
{
	u16 data;

	data = 0x834e;
	bq24773_write_word(charger->i2c, BQ24773_CHG_OPTION0_1, data);
	bq24773_set_max_voltage(charger, charger->pdata->chg_float_voltage);
	bq24773_set_input_current(charger, charger->pdata->charging_current
				  [POWER_SUPPLY_TYPE_BATTERY].input_current_limit);
	bq24773_test_read(charger);
}

static int bq24773_chg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct bq24773_charger *charger =
		container_of(psy, struct bq24773_charger, psy_chg);

	val->intval = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->cable_type;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = charger->input_current;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = bq24773_get_input_current(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		break;
	case POWER_SUPPLY_PROP_USB_HC:
		return -ENODATA;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq24773_chg_set_property(struct power_supply *psy,
			  enum power_supply_property psp,
			  const union power_supply_propval *val)
{
	struct bq24773_charger *charger =
		container_of(psy, struct bq24773_charger, psy_chg);

	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		bq24773_charger_function_control(charger);
		break;
	/* val->intval : input charging current */
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		charger->input_current = val->intval;
		bq24773_set_input_current(charger, val->intval);
		break;
	/*  val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		charger->charging_current = val->intval;
		bq24773_set_charge_current(charger, val->intval);
		break;
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		break;
	case POWER_SUPPLY_PROP_USB_HC:
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq24773_otg_get_property(struct power_supply *psy,
			      enum power_supply_property psp,
			      union power_supply_propval *val)
{
	struct bq24773_charger *charger =
		container_of(psy, struct bq24773_charger, psy_otg);

	if (charger->otg_en < 0)
		return 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		gpio_get_value(charger->otg_en);
		if (charger->otg_en)
			val->intval = 1;
		else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int bq24773_otg_set_property(struct power_supply *psy,
			  enum power_supply_property psp,
			  const union power_supply_propval *val)
{
	struct bq24773_charger *charger =
		container_of(psy, struct bq24773_charger, psy_otg);

	if (charger->otg_en < 0)
		return 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (val->intval) {
			pr_info("%s : OTG EN\n", __func__);
			gpio_set_value(charger->otg_en, 1);
			if (charger->otg_en2)
				gpio_set_value(charger->otg_en2, 1);
		} else {
			pr_info("%s : OTG DISEN\n", __func__);
			gpio_set_value(charger->otg_en, 0);
			if (charger->otg_en2)
				gpio_set_value(charger->otg_en2, 0);
		}
		pr_info("%s : OTG SET(%d)\n", __func__, gpio_get_value(charger->otg_en));
		break;
	default:
		return -EINVAL;
	}

	return 0;

}

static int bq24773_debugfs_show(struct seq_file *s, void *data)
{
	struct bq24773_charger *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "bq24773 CHARGER IC :\n");
	seq_printf(s, "===================\n");
	for (reg = 0x00; reg <= 0x10; reg++) {
		bq24773_read_reg(charger->i2c, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");
	return 0;
}

static int bq24773_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, bq24773_debugfs_show, inode->i_private);
}

static const struct file_operations bq24773_debugfs_fops = {
	.open           = bq24773_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#ifdef CONFIG_OF
static int bq24773_charger_parse_dt(struct bq24773_charger *charger)
{
	struct device_node *np = of_find_node_by_name(NULL, "bq24773-charger");
	sec_charger_platform_data_t *pdata = charger->pdata;
	int ret = 0;
	int i, len;
	const u32 *p;

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
					   &pdata->chg_float_voltage);
		charger->otg_en = of_get_named_gpio(np, "charger,otg_en", 0);
		if (charger->otg_en < 0) {
			charger->otg_en = -1;
			pr_info("%s: No use gpios for otg accessory power\n", __func__);
		} else {
			pr_info("%s, gpios_otg_power  %d\n", __func__, charger->otg_en);
		}

		charger->otg_en2 = of_get_named_gpio(np, "charger,otg_en2", 0);
		if (charger->otg_en2 < 0) {
			charger->otg_en2 = -1;
			pr_info("%s: No use gpios for otg accessory power\n", __func__);
		} else {
			pr_info("%s, gpios_otg_power  %d\n", __func__, charger->otg_en2);
		}
	}

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,full_check_type_2nd",
					&pdata->full_check_type_2nd);
		if (ret)
			pr_info("%s : Full check type 2nd is Empty\n", __func__);

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
#endif

static int __devinit bq24773_charger_probe(struct i2c_client *client,
					   const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq24773_charger *charger;

	int ret = 0;

	pr_info("%s: bq24773 Charger Driver Loading\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->i2c_lock);

	charger->i2c = client;

	if (client->dev.of_node) {

		charger->pdata = devm_kzalloc(&client->dev, sizeof(*(charger->pdata)),
					      GFP_KERNEL);
		if (!charger->pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_parse_dt_nomem;
		}
#if defined(CONFIG_OF)
		ret = bq24773_charger_parse_dt(charger);
		if (ret < 0) {
			pr_err("%s not found charger dt! ret[%d]\n",
			       __func__, ret);
			goto err_parse_dt;
		}
#endif
	}

	i2c_set_clientdata(client, charger);

	charger->psy_chg.name		= "bq24773-charger";
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= bq24773_chg_get_property;
	charger->psy_chg.set_property	= bq24773_chg_set_property;
	charger->psy_chg.properties	= bq24773_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(bq24773_charger_props);
	charger->psy_otg.name		= "otg";
	charger->psy_otg.type		= POWER_SUPPLY_TYPE_OTG;
	charger->psy_otg.get_property	= bq24773_otg_get_property;
	charger->psy_otg.set_property	= bq24773_otg_set_property;
	charger->psy_otg.properties	= bq24773_otg_props;
	charger->psy_otg.num_properties	= ARRAY_SIZE(bq24773_otg_props);

	bq24773_charger_initialize(charger);

	(void) debugfs_create_file("bq24773-regs",
		S_IRUGO, NULL, (void *)charger, &bq24773_debugfs_fops);

	ret = power_supply_register(&client->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_data_free;
	}
	ret = power_supply_register(&client->dev, &charger->psy_otg);
	if (ret) {
		pr_err("%s: Failed to Register psy_otg\n", __func__);
		goto err_otg_psy;
	}

	if (charger->otg_en > 0) {
		ret = gpio_request(charger->otg_en, "OTG_EN");
		pr_info("%s[AFTER] : OTG_EN(%d)\n", __func__, charger->otg_en);
		if (ret) {
			pr_err("failed to request GPIO %u\n", charger->otg_en);
			goto err_otg_set;
		}
	}

	if (charger->otg_en2 > 0) {
		ret = gpio_request(charger->otg_en2, "OTG_EN2");
		pr_info("%s[AFTER] : OTG_EN(%d)\n", __func__, charger->otg_en2);
		if (ret) {
			pr_err("failed to request GPIO %u\n", charger->otg_en2);
			goto err_otg2_set;
		}
	}

	pr_info("%s: bq24773 Charger Driver Loaded\n", __func__);

	return 0;

err_otg2_set:
err_otg_set:
	power_supply_unregister(&charger->psy_otg);
err_otg_psy:
	power_supply_unregister(&charger->psy_chg);
err_data_free:
	if (client->dev.of_node)
		devm_kfree(&client->dev, charger->pdata);
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->i2c_lock);
	kfree(charger);

	return ret;
}

static const struct i2c_device_id bq24773_charger_id[] = {
	{"bq24773-charger", 0},
	{}
};

static void bq24773_charger_shutdown(struct i2c_client *client)
{
	struct bq24773_charger *charger = i2c_get_clientdata(client);

	pr_info("%s: bq24773 Charger driver shutdown\n", __func__);
	if (!charger->i2c) {
		pr_err("%s: no bq24773 i2c client\n", __func__);
		return;
	}
}

static int bq24773_charger_remove(struct i2c_client *client)
{
	struct bq24773_charger *charger = i2c_get_clientdata(client);

	power_supply_unregister(&charger->psy_chg);
	power_supply_unregister(&charger->psy_otg);
	mutex_destroy(&charger->i2c_lock);
	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int bq24773_charger_suspend(struct device *dev)
{
	return 0;
}

static int bq24773_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define bq24773_charger_suspend NULL
#define bq24773_charger_resume NULL
#endif


static SIMPLE_DEV_PM_OPS(bq24773_charger_pm_ops, bq24773_charger_suspend,
			 bq24773_charger_resume);

static struct i2c_driver bq24773_charger_driver = {
	.driver = {
		.name = "bq24773-charger",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &bq24773_charger_pm_ops,
#endif
	},
	.probe = bq24773_charger_probe,
	.remove = bq24773_charger_remove,
	.shutdown = bq24773_charger_shutdown,
	.id_table = bq24773_charger_id,
};

static int __init bq24773_charger_init(void)
{
	pr_info("%s : \n", __func__);
	return i2c_add_driver(&bq24773_charger_driver);
}

static void __exit bq24773_charger_exit(void)
{
	i2c_del_driver(&bq24773_charger_driver);
}

module_init(bq24773_charger_init);
module_exit(bq24773_charger_exit);

MODULE_DESCRIPTION("Samsung bq24773 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
