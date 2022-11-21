/*
 * s2mcs01_charger.c - S2MCS01 Charger Driver
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "include/charger/s2mcs01_charger.h"
#include <linux/of.h>
#include <linux/of_gpio.h>

#define DEBUG

#define ENABLE 1
#define DISABLE 0

static enum power_supply_property s2mcs01_charger_props[] = {
	POWER_SUPPLY_PROP_HEALTH,				/* status */
	POWER_SUPPLY_PROP_ONLINE,				/* buck enable/disable */
	POWER_SUPPLY_PROP_CURRENT_MAX,			/* input current */
	POWER_SUPPLY_PROP_CURRENT_NOW,			/* charge current */
};

static void s2mcs01_set_charger_state(
	struct s2mcs01_charger_data *charger, int enable);

static int s2mcs01_read_reg(struct i2c_client *client, u8 reg, u8 *dest)
{
	struct s2mcs01_charger_data *charger = i2c_get_clientdata(client);
	int ret = 0;
 
	mutex_lock(&charger->io_lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&charger->io_lock);

	if (ret < 0) {
		pr_err("%s: can't read reg(0x%x), ret(%d)\n", __func__, reg, ret);
		return ret;
	}

	reg &= 0xFF;
	*dest = ret;

	return 0;
}

static int s2mcs01_write_reg(struct i2c_client *client, u8 reg, u8 data)
{
	struct s2mcs01_charger_data *charger = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&charger->io_lock);
	ret = i2c_smbus_write_byte_data(client, reg, data);
	mutex_unlock(&charger->io_lock);

	if (ret < 0)
		pr_err("%s: can't write reg(0x%x), ret(%d)\n", __func__, reg, ret);

	return ret;
}

static int s2mcs01_update_reg(struct i2c_client *client, u8 reg, u8 val, u8 mask)
{
	struct s2mcs01_charger_data *charger = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&charger->io_lock);
	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		pr_err("%s: can't update reg(0x%x), ret(%d)\n", __func__, reg, ret);
	else {
		u8 old_val = ret & 0xFF;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(client, reg, new_val);
	}
	mutex_unlock(&charger->io_lock);

	return ret;
}

static void s2mcs01_charger_test_read(
	struct s2mcs01_charger_data *charger)
{
	u8 data = 0;
	u32 addr = 0;
	char str[1024]={0,};
	for (addr = 0x03; addr <= 0x06; addr++) {
		s2mcs01_read_reg(charger->i2c, addr, &data);
		sprintf(str + strlen(str), "[0x%02x]0x%02x, ", addr, data);
	}
	s2mcs01_read_reg(charger->i2c, 0xA, &data);
	sprintf(str + strlen(str), "[0xA]0x%02x, ", data);
	s2mcs01_read_reg(charger->i2c, 0xB, &data);
	sprintf(str + strlen(str), "[0xB]0x%02x, ", data);
	s2mcs01_read_reg(charger->i2c, 0x4A, &data);
	sprintf(str + strlen(str), "[0x4A]0x%02x, ", data);
	s2mcs01_read_reg(charger->i2c, 0x52, &data);
	sprintf(str + strlen(str), "[0x52]0x%02x, ", data);
	pr_info("%s: S2MCS01 : %s\n", __func__, str);

	str[0]='\0';
	for (addr = 0x54; addr <= 0x5B; addr++) {
		s2mcs01_read_reg(charger->i2c, addr, &data);
		sprintf(str + strlen(str), "[0x%02x]0x%02x, ", addr, data);
	}
	pr_info("%s: S2MCS01 : %s\n", __func__, str);

	str[0]='\0';
	for (addr = 0x5D; addr <= 0x5F; addr++) {
		s2mcs01_read_reg(charger->i2c, addr, &data);
		sprintf(str + strlen(str), "[0x%02x]0x%02x, ", addr, data);
	}
	pr_info("%s: S2MCS01 : %s\n", __func__, str);
}

static int s2mcs01_get_charger_state(struct s2mcs01_charger_data *charger)
{
	u8 reg_data;

	s2mcs01_read_reg(charger->i2c, S2MCS01_CHG_STATUS0, &reg_data);
	if (reg_data & CHGIN_OK_STATUS_MASK)
		return POWER_SUPPLY_STATUS_CHARGING;
	return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int s2mcs01_get_charger_health(struct s2mcs01_charger_data *charger)
{
	u8 reg_data;

	s2mcs01_read_reg(charger->i2c, S2MCS01_CHG_STATUS0, &reg_data);
	if (!(reg_data & CHGINOV_OK_STATUS_MASK)) {
			pr_info("%s: VIN overvoltage\n", __func__);
			return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (!(reg_data & CHGINUV_OK_STATUS_MASK)) {
		pr_info("%s: VIN undervoltage\n", __func__);
		return POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
	} else
		return POWER_SUPPLY_HEALTH_GOOD;
}

static void s2mcs01_set_charger_state(
	struct s2mcs01_charger_data *charger, int enable)
{
	pr_info("%s: BUCK_EN(%s)\n", __func__, enable > 0 ? "ENABLE" : "DISABLE");

	if (enable)
		s2mcs01_update_reg(charger->i2c,
			S2MCS01_CHG_SC_CTRL5, ENI2C_MASK, ENI2C_MASK);
	else
		s2mcs01_update_reg(charger->i2c, S2MCS01_CHG_SC_CTRL5, 0, ENI2C_MASK);

	s2mcs01_charger_test_read(charger);
}

static int s2mcs01_get_charge_current(struct s2mcs01_charger_data *charger)
{
	u8 reg_data;
	int charge_current;

	s2mcs01_read_reg(charger->i2c, S2MCS01_CHG_SC_CTRL3, &reg_data);
	charge_current = (reg_data * 12) + 8;

	return charge_current;
}

static void s2mcs01_set_charge_current(struct s2mcs01_charger_data *charger,
	int charge_current)
{
	u8 reg_data;

	if (!charge_current) {
		reg_data = 0x42;
	} else {
		charge_current = (charge_current < 800) ? 800 : \
			(charge_current > 2500) ? 2500 : charge_current;
			reg_data = charge_current / 12;
	}

	s2mcs01_write_reg(charger->i2c,	S2MCS01_CHG_SC_CTRL3, reg_data);
	pr_info("%s: charge_current(%d) : 0x%x\n", __func__, charge_current, reg_data);
}

static void s2mcs01_charger_initialize(struct s2mcs01_charger_data *charger)
{
	u8 reg_data;

	pr_info("%s: \n", __func__);
	s2mcs01_read_reg(charger->i2c, S2MCS01_PMIC_REV, &reg_data);

	if (reg_data == 0x00) {
		s2mcs01_update_reg(charger->i2c, 0x45, BIT(5) , BIT(5));
		s2mcs01_update_reg(charger->i2c, 0x4E, BIT(7) , BIT(7));
		s2mcs01_write_reg(charger->i2c, 0x50, 0x1F);
		s2mcs01_update_reg(charger->i2c, 0x57, 0x0C , 0x0C);
		s2mcs01_update_reg(charger->i2c, 0x5A, 0x06 , 0x06);
	}

	/* interrupt mask */
	s2mcs01_update_reg(charger->i2c, S2MCS01_CHG_INT1_MASK, BATCVD_INT_M_MASK, BATCVD_INT_M_MASK);
	s2mcs01_update_reg(charger->i2c, S2MCS01_CHG_INT2_MASK, BUCK_ILIM_INT_M_MASK, BUCK_ILIM_INT_M_MASK);

	/* Set CV_FLG to 4.7V */
	s2mcs01_update_reg(charger->i2c, S2MCS01_CHG_SC_CTRL9, 0x28, CV_FLG_MASK);
}

static irqreturn_t s2mcs01_irq_handler(int irq, void *data)
{
	u8 data_int_1 = 0;
	u8 data_int_2 = 0;
	struct s2mcs01_charger_data *charger = data;

	s2mcs01_read_reg(charger->i2c, S2MCS01_CHG_INT1, &data_int_1);
	s2mcs01_read_reg(charger->i2c, S2MCS01_CHG_INT2, &data_int_2);

	pr_info("%s: INT 1 : 0x%02x , INT 2 : 0x%02x\n", __func__, data_int_1, data_int_2);

	return IRQ_HANDLED;
}

static int s2mcs01_chg_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct s2mcs01_charger_data *charger =
		container_of(psy, struct s2mcs01_charger_data, psy_chg);
	enum power_supply_ext_property ext_psp = psp;

	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		if (charger->cable_type == POWER_SUPPLY_TYPE_HV_MAINS ||
			charger->cable_type == POWER_SUPPLY_TYPE_HV_MAINS_12V) {
			s2mcs01_charger_test_read(charger);
			val->intval = s2mcs01_get_charger_health(charger);
		} else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = s2mcs01_get_charger_state(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = s2mcs01_get_charge_current(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		return -ENODATA;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHECK_SLAVE_I2C:
			{
			u8 reg_data;
			val->intval = (s2mcs01_read_reg(charger->i2c, S2MCS01_CHG_SC_CTRL0, &reg_data) == 0);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CHECK_MULTI_CHARGE:
			val->intval = (s2mcs01_get_charger_health(charger) == POWER_SUPPLY_HEALTH_GOOD) ?
				s2mcs01_get_charger_state(charger) : POWER_SUPPLY_STATUS_NOT_CHARGING;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mcs01_chg_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct s2mcs01_charger_data *charger =
		container_of(psy, struct s2mcs01_charger_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		charger->is_charging = 
			(val->intval == SEC_BAT_CHG_MODE_CHARGING) ? ENABLE : DISABLE;
		s2mcs01_set_charger_state(charger, charger->is_charging);
		if (charger->is_charging == DISABLE)
			s2mcs01_set_charge_current(charger, 0);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger->charging_current = val->intval;
		s2mcs01_set_charge_current(charger, charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		charger->siop_level = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CURRENT_FULL:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_HEALTH:
		return -ENODATA;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mcs01_charger_parse_dt(struct device *dev,
	sec_charger_platform_data_t *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mcs01-charger");
	int ret = 0;

	if (!np) {
		pr_err("%s: np is NULL\n", __func__);
		return -1;
	} else {
		ret = of_get_named_gpio_flags(np, "s2mcs01-charger,irq-gpio",
			0, NULL);
		if (ret < 0) {
			pr_err("%s: s2mcs01-charger,irq-gpio is empty\n", __func__);
			pdata->irq_gpio = 0;
		} else {
			pdata->irq_gpio = ret;
			pr_info("%s: irq-gpio = %d\n", __func__, pdata->irq_gpio);
		}
	}
	return 0;
}

static ssize_t s2mcs01_store_addr(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mcs01_charger_data *charger = container_of(psy, struct s2mcs01_charger_data, psy_chg);
	int x;
	if (sscanf(buf, "0x%x\n", &x) == 1) {
		charger->addr = x;
	}
	return count;
}

static ssize_t s2mcs01_show_addr(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mcs01_charger_data *charger = container_of(psy, struct s2mcs01_charger_data, psy_chg);
	return sprintf(buf, "0x%x\n", charger->addr);
}

static ssize_t s2mcs01_store_size(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mcs01_charger_data *charger = container_of(psy, struct s2mcs01_charger_data, psy_chg);
	int x;
	if (sscanf(buf, "%d\n", &x) == 1) {
		charger->size = x;
	}
	return count;
}

static ssize_t s2mcs01_show_size(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mcs01_charger_data *charger = container_of(psy, struct s2mcs01_charger_data, psy_chg);
	return sprintf(buf, "0x%x\n", charger->size);
}

static ssize_t s2mcs01_store_data(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mcs01_charger_data *charger = container_of(psy, struct s2mcs01_charger_data, psy_chg);
	int x;

	if (sscanf(buf, "0x%x", &x) == 1) {
		u8 data = x;
		if (s2mcs01_write_reg(charger->i2c, charger->addr, data) < 0)
		{
			dev_info(charger->dev,
					"%s: addr: 0x%x write fail\n", __func__, charger->addr);
		}
	}
	return count;
}

static ssize_t s2mcs01_show_data(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mcs01_charger_data *charger = container_of(psy, struct s2mcs01_charger_data, psy_chg);
	u8 data;
	int i, count = 0;;
	if (charger->size == 0)
		charger->size = 1;

	for (i = 0; i < charger->size; i++) {
		if (s2mcs01_read_reg(charger->i2c, charger->addr+i, &data) < 0) {
			dev_info(charger->dev,
					"%s: read fail\n", __func__);
			count += sprintf(buf+count, "addr: 0x%x read fail\n", charger->addr+i);
			continue;
		}
		count += sprintf(buf+count, "addr: 0x%x, data: 0x%x\n", charger->addr+i,data);
	}
	return count;
}

static DEVICE_ATTR(addr, 0644, s2mcs01_show_addr, s2mcs01_store_addr);
static DEVICE_ATTR(size, 0644, s2mcs01_show_size, s2mcs01_store_size);
static DEVICE_ATTR(data, 0644, s2mcs01_show_data, s2mcs01_store_data);

static struct attribute *s2mcs01_attributes[] = {
	&dev_attr_addr.attr,
	&dev_attr_size.attr,
	&dev_attr_data.attr,
	NULL
};

static const struct attribute_group s2mcs01_attr_group = {
	.attrs = s2mcs01_attributes,
};

static int s2mcs01_charger_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct device_node *of_node = client->dev.of_node;
	struct s2mcs01_charger_data *charger;
	sec_charger_platform_data_t *pdata = client->dev.platform_data;
	int ret = 0;

	pr_info("%s: S2MCS01 Charger Driver Loading\n", __func__);

	if (of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		ret = s2mcs01_charger_parse_dt(&client->dev, pdata);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		pdata = client->dev.platform_data;
	}

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto err_nomem;
	}

	mutex_init(&charger->io_lock);
	charger->dev = &client->dev;
	ret = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(client->adapter);
		dev_err(charger->dev, "I2C functionality is not supported.\n");
		ret = -ENOSYS;
		goto err_i2cfunc_not_support;
	}
	charger->i2c = client;
	charger->pdata = pdata;
	i2c_set_clientdata(client, charger);

	charger->psy_chg.name			= "s2mcs01-charger";
	charger->psy_chg.type			= POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property	= s2mcs01_chg_get_property;
	charger->psy_chg.set_property	= s2mcs01_chg_set_property;
	charger->psy_chg.properties		= s2mcs01_charger_props;
	charger->psy_chg.num_properties	= ARRAY_SIZE(s2mcs01_charger_props);

	charger->cable_type = POWER_SUPPLY_TYPE_BATTERY;

	s2mcs01_charger_initialize(charger);

	ret = power_supply_register(charger->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = -1;
		goto err_power_supply_register;
	}

	charger->wqueue =
		create_singlethread_workqueue(dev_name(charger->dev));
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		ret = -1;
		goto err_create_wqueue;
	}

	if (pdata->irq_gpio) {
		charger->chg_irq = gpio_to_irq(pdata->irq_gpio);

		ret = request_threaded_irq(charger->chg_irq, NULL,
			s2mcs01_irq_handler,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"s2mcs01-irq", charger);
		if (ret < 0) {
			pr_err("%s: Failed to Request IRQ(%d)\n", __func__, ret);
			goto err_req_irq;
		}

		ret = enable_irq_wake(charger->chg_irq);
		if (ret < 0)
			pr_err("%s: Failed to Enable Wakeup Source(%d)\n",
				__func__, ret);
	}

	ret = sysfs_create_group(&charger->psy_chg.dev->kobj, &s2mcs01_attr_group);
	if (ret) {
		dev_info(&client->dev,
			"%s: sysfs_create_group failed\n", __func__);
	}

	s2mcs01_charger_test_read(charger);
	s2mcs01_get_charge_current(charger);

	pr_info("%s: SM7P10 Charger Driver Loaded\n", __func__);

	return 0;

err_req_irq:
err_create_wqueue:
	power_supply_unregister(&charger->psy_chg);
err_power_supply_register:
err_i2cfunc_not_support:
	mutex_destroy(&charger->io_lock);
	kfree(charger);
err_nomem:
err_parse_dt:
	kfree(pdata);

	return ret;
}

static int s2mcs01_charger_remove(struct i2c_client *client)
{
	struct s2mcs01_charger_data *charger = i2c_get_clientdata(client);

	free_irq(charger->chg_irq, NULL);
	destroy_workqueue(charger->wqueue);
	power_supply_unregister(&charger->psy_chg);
	mutex_destroy(&charger->io_lock);
	kfree(charger->pdata);
	kfree(charger);

	return 0;
}

static void s2mcs01_charger_shutdown(struct i2c_client *client)
{
}

static const struct i2c_device_id s2mcs01_charger_id_table[] = {
	{"s2mcs01-charger", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, s2mcs01_id_table);

#if defined(CONFIG_PM)
static int s2mcs01_charger_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mcs01_charger_data *charger = i2c_get_clientdata(i2c);

	if (charger->chg_irq) {
		if (device_may_wakeup(dev))
			enable_irq_wake(charger->chg_irq);
		disable_irq(charger->chg_irq);
	}

	return 0;
}

static int s2mcs01_charger_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mcs01_charger_data *charger = i2c_get_clientdata(i2c);

	if (charger->chg_irq) {
		if (device_may_wakeup(dev))
			disable_irq_wake(charger->chg_irq);
		enable_irq(charger->chg_irq);
	}

	return 0;
}
#else
#define s2mcs01_charger_suspend		NULL
#define s2mcs01_charger_resume		NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops s2mcs01_pm = {
	.suspend = s2mcs01_charger_suspend,
	.resume = s2mcs01_charger_resume,
};

#ifdef CONFIG_OF
static struct of_device_id s2mcs01_charger_match_table[] = {
	{.compatible = "samsung,s2mcs01-charger"},
	{},
};
#else
#define s2mcs01_charger_match_table NULL
#endif

static struct i2c_driver s2mcs01_charger_driver = {
	.driver = {
		.name	= "s2mcs01-charger",
		.owner	= THIS_MODULE,
		.of_match_table = s2mcs01_charger_match_table,
#if defined(CONFIG_PM)
		.pm = &s2mcs01_pm,
#endif /* CONFIG_PM */
	},
	.probe		= s2mcs01_charger_probe,
	.remove		= s2mcs01_charger_remove,
	.shutdown	= s2mcs01_charger_shutdown,
	.id_table	= s2mcs01_charger_id_table,
};

static int __init s2mcs01_charger_init(void)
{
	pr_info("%s: \n", __func__);
	return i2c_add_driver(&s2mcs01_charger_driver);
}

static void __exit s2mcs01_charger_exit(void)
{
	pr_info("%s: \n", __func__);
	i2c_del_driver(&s2mcs01_charger_driver);
}

module_init(s2mcs01_charger_init);
module_exit(s2mcs01_charger_exit);

MODULE_DESCRIPTION("Samsung S2MCS01 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
