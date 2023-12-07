/*
 *  bq25898s_charger.c
 *  Samsung bq25898s Charger Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
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
#include <linux/battery/charger/bq25898s_charger.h>

#define ENABLE 1
#define DISABLE 0

static enum power_supply_property bq25898s_charger_props[] = {
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
#if defined(CONFIG_BATTERY_SWELLING)
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
#endif
};

int bq25898s_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct bq25898s_charger *bq25898s = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bq25898s->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&bq25898s->i2c_lock);
	if (ret < 0) {
		pr_info("%s reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}

int bq25898s_read_word(struct i2c_client *i2c, u8 reg)
{
	struct bq25898s_charger *bq25898s = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bq25898s->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&bq25898s->i2c_lock);
	if (ret < 0)
		return ret;

	return ret;
}

int bq25898s_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct bq25898s_charger *bq25898s = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bq25898s->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&bq25898s->i2c_lock);
	if (ret < 0)
		pr_info("%s reg(0x%x), ret(%d)\n",
				__func__, reg, ret);

	return ret;
}

int bq25898s_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct bq25898s_charger *bq25898s = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bq25898s->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&bq25898s->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}

int bq25898s_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct bq25898s_charger *bq25898s = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&bq25898s->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&bq25898s->i2c_lock);
	return ret;
}

static void bq25898s_test_read(struct bq25898s_charger *charger)
{
	u8 reg;
	u8 reg_data;

	pr_info("bq25898s CHARGER IC :\n");
	pr_info("===================\n");
	for (reg = 0x00; reg <= 0x10; reg++) {
		bq25898s_read_reg(charger->i2c, reg, &reg_data);
		pr_info("0x%02x:\t0x%02x\n", reg, reg_data);
	}

}

#if 0
static int bq25898s_get_charge_current(struct bq25898s_charger *charger)
{
	u16 data;
	u32 charge_current;

	data = bq25898s_read_word(charger->i2c, BQ25898S_CHG_CURR_1);
	charge_current = (data >> 5) * 64;

	return charge_current;
}
#endif

/*
static int bq25898s_get_float_voltage(struct bq25898s_charger *charger)
{
	u16 data;
	u32 max_voltage;

	data = bq25898s_read_word(charger->i2c, BQ25898S_CHG_REG_06);
	max_voltage = (data >> 4) * 16;
	pr_info("%s : DATA(0x%04x) VOLTAGE(%d)\n", __func__, data, max_voltage);

	return max_voltage;
}

static int bq25898s_get_input_current(struct bq25898s_charger *charger)
{
	u8 data;
	u32 input_current;

	bq25898s_read_reg(charger->i2c, BQ25898S_INPUT_CURR, &data);
	input_current = data * 64;

	return input_current;
}
*/

static void bq25898s_set_charge_current(struct bq25898s_charger *charger, int charging_current)
{
	//u16 data;

	pr_info("%s: charging_current(%d) \n", __func__, charging_current);

	bq25898s_update_reg(charger->i2c, BQ25898S_CHG_REG_04,
			0x18, BQ25898S_CHG_ICHG_MASK);
}

static void bq25898s_set_input_current(struct bq25898s_charger *charger, int input_current)
{
	//u16 data;

	pr_info ("%s : SET INPUT CURRENT(%d)\n", __func__, input_current);

	bq25898s_update_reg(charger->i2c, BQ25898S_CHG_REG_00,
			0x3F, BQ25898S_CHG_IINLIM_MASK);
}

static void bq25898s_set_watchdog_timer(struct bq25898s_charger *charger, int time)
{
	bq25898s_update_reg(charger->i2c, BQ25898S_CHG_REG_07,
			time << BQ25898S_CHG_WATCHDOG_SHIFT, BQ25898S_CHG_WATCHDOG_MASK);
}

static void bq25898s_set_float_voltage(struct bq25898s_charger *charger, int float_voltage)
{
	//u16 data;
	//u32 offset = 3840; // 3.840V

	bq25898s_update_reg(charger->i2c, BQ25898S_CHG_REG_06,
			0x84, BQ25898S_CHG_VREG_MASK);
}

static void bq25898s_charger_function_control(struct bq25898s_charger *charger)
{
	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		charger->is_charging = false;
		charger->chg_float_voltage = charger->pdata->chg_float_voltage;
		charger->charging_current = 0;
		/*
		charger->input_current =
			charger->pdata->charging_current
			[POWER_SUPPLY_TYPE_BATTERY].input_current_limit;
			*/

	} else {
		charger->is_charging = true;
		charger->chg_float_voltage = charger->pdata->chg_float_voltage;
		charger->charging_current =
			charger->pdata->charging_current
			[charger->cable_type].fast_charging_current;
		/*
		charger->input_current =
			charger->pdata->charging_current
			[charger->cable_type].input_current_limit;
			*/

		pr_info ("%s : FLOAT_VOLTAGE(%d) INPUT_CURR(%d) CHARGING_CURR(%d)\n",
				__func__, charger->chg_float_voltage, charger->input_current,
				charger->charging_current);
	}

	bq25898s_set_float_voltage(charger, charger->chg_float_voltage);

	bq25898s_test_read(charger);
}

static void bq25898s_charger_initialize(struct bq25898s_charger *charger)
{
	//u16 data;

	bq25898s_set_float_voltage(charger, charger->pdata->chg_float_voltage);
	bq25898s_set_input_current(charger, 3150);
	bq25898s_set_watchdog_timer(charger, WATCHDOG_TIMER_DISABLE);
	/*
	bq25898s_set_input_current(charger, charger->pdata->charging_current
			[POWER_SUPPLY_TYPE_BATTERY].input_current_limit);
			*/
	bq25898s_test_read(charger);
}

static void bq25898s_set_charger_state(struct bq25898s_charger *charger,
	int enable)
{
	pr_info("%s: CHARGE_EN(%s)\n",__func__, enable > 0 ? "ENABLE" : "DISABLE");

	/*
	bq25898s_update_reg(charger->i2c, BQ25898S_CHG_REG_00,
			(!enable << BQ25898S_CHG_ENABLE_HIZ_MODE_SHIFT), BQ25898S_CHG_ENABLE_HIZ_MODE_MASK);
			*/
	bq25898s_update_reg(charger->i2c, BQ25898S_CHG_REG_03,
			(enable << BQ25898S_CHG_CONFIG_SHIFT), BQ25898S_CHG_CONFIG_MASK);
	bq25898s_test_read(charger);
}

static int bq25898s_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct bq25898s_charger *charger =
		container_of(psy, struct bq25898s_charger, psy_chg);

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
			bq25898s_test_read(charger);
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			val->intval = charger->input_current;
			break;
		case POWER_SUPPLY_PROP_CURRENT_AVG:
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			//val->intval = bq25898s_get_input_current(charger);
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			break;
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
			break;
#if defined(CONFIG_BATTERY_SWELLING)
		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			//val->intval = bq25898s_float_max_voltage(charger);
			break;
#endif
		case POWER_SUPPLY_PROP_USB_HC:
			return -ENODATA;
		case POWER_SUPPLY_PROP_CHARGE_NOW:
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static int bq25898s_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct bq25898s_charger *charger =
		container_of(psy, struct bq25898s_charger, psy_chg);

	switch (psp) {
		/* val->intval : type */
		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
			charger->is_charging =
				(val->intval == SEC_BAT_CHG_MODE_CHARGING) ? ENABLE : DISABLE;
			bq25898s_set_charger_state(charger, charger->is_charging);
			break;
		case POWER_SUPPLY_PROP_STATUS:
			charger->status = val->intval;
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			charger->cable_type = val->intval;
			bq25898s_charger_function_control(charger);
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			charger->charging_current = val->intval;
			bq25898s_set_charge_current(charger, charger->charging_current);
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			charger->siop_level = val->intval;
			break;
		case POWER_SUPPLY_PROP_USB_HC:
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
			break;
		case POWER_SUPPLY_PROP_CHARGE_NOW:
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static int bq25898s_debugfs_show(struct seq_file *s, void *data)
{
	struct bq25898s_charger *charger = s->private;
	u8 reg;
	u8 reg_data;

	seq_printf(s, "bq25898s CHARGER IC :\n");
	seq_printf(s, "===================\n");
	for (reg = 0x00; reg <= 0x10; reg++) {
		bq25898s_read_reg(charger->i2c, reg, &reg_data);
		seq_printf(s, "0x%02x:\t0x%02x\n", reg, reg_data);
	}

	seq_printf(s, "\n");
	return 0;
}

static int bq25898s_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, bq25898s_debugfs_show, inode->i_private);
}

static const struct file_operations bq25898s_debugfs_fops = {
	.open           = bq25898s_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

#ifdef CONFIG_OF
static int bq25898s_charger_parse_dt(struct bq25898s_charger *charger)
{
	struct device_node *np;
	sec_charger_platform_data_t *pdata = charger->pdata;
	int ret = 0;
	int i, len;
	const u32 *p;

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_u32(np, "battery,chg_float_voltage",
				&pdata->chg_float_voltage);
		if (ret)
			pr_info("%s : battery,chg_float_voltage is Empty\n", __func__);

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

static int bq25898s_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq25898s_charger *charger;

	int ret = 0;

	pr_info("%s: bq25898s Charger Driver Loading\n", __func__);

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
		ret = bq25898s_charger_parse_dt(charger);
		if (ret < 0) {
			pr_err("%s not found charger dt! ret[%d]\n",
					__func__, ret);
			goto err_parse_dt;
		}
#endif
	}

	i2c_set_clientdata(client, charger);

	charger->psy_chg.name		= "bq25898s-charger";
	charger->psy_chg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= bq25898s_chg_get_property;
	charger->psy_chg.set_property	= bq25898s_chg_set_property;
	charger->psy_chg.properties	= bq25898s_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(bq25898s_charger_props);

	charger->siop_level = 100;

	bq25898s_charger_initialize(charger);

	(void) debugfs_create_file("bq25898s-regs",
			S_IRUGO, NULL, (void *)charger, &bq25898s_debugfs_fops);

	ret = power_supply_register(&client->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_data_free;
	}
	pr_info("%s: bq25898s Charger Driver Loaded\n", __func__);

	return 0;

err_data_free:
	if (client->dev.of_node)
		devm_kfree(&client->dev, charger->pdata);
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->i2c_lock);
	kfree(charger);

	return ret;
}

static const struct i2c_device_id bq25898s_charger_id[] = {
	{"ti,bq25898s-charger", 0},
	{}
};

static void bq25898s_charger_shutdown(struct i2c_client *client)
{
	struct bq25898s_charger *charger = i2c_get_clientdata(client);

	pr_info("%s: bq25898s Charger driver shutdown\n", __func__);
	if (!charger->i2c) {
		pr_err("%s: no bq25898s i2c client\n", __func__);
		return;
	}
}

static int bq25898s_charger_remove(struct i2c_client *client)
{
	struct bq25898s_charger *charger = i2c_get_clientdata(client);

	power_supply_unregister(&charger->psy_chg);
	mutex_destroy(&charger->i2c_lock);
	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int bq25898s_charger_suspend(struct device *dev)
{
	return 0;
}

static int bq25898s_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define bq25898s_charger_suspend NULL
#define bq25898s_charger_resume NULL
#endif


static SIMPLE_DEV_PM_OPS(bq25898s_charger_pm_ops, bq25898s_charger_suspend,
		bq25898s_charger_resume);

static struct i2c_driver bq25898s_charger_driver = {
	.driver = {
		.name = "bq25898s-charger",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &bq25898s_charger_pm_ops,
#endif
	},
	.probe = bq25898s_charger_probe,
	.remove = bq25898s_charger_remove,
	.shutdown = bq25898s_charger_shutdown,
	.id_table = bq25898s_charger_id,
};

static int __init bq25898s_charger_init(void)
{
	pr_info("%s : \n", __func__);
	return i2c_add_driver(&bq25898s_charger_driver);
}

static void __exit bq25898s_charger_exit(void)
{
	i2c_del_driver(&bq25898s_charger_driver);
}

module_init(bq25898s_charger_init);
module_exit(bq25898s_charger_exit);

MODULE_DESCRIPTION("Samsung BQ25898S Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
