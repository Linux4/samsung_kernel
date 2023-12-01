/*
 * s2mu106.c - mfd core driver for the s2mu106
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/samsung/s2mu106.h>
#include <linux/of_gpio.h>

static struct mfd_cell s2mu106_devs[] = {
#if IS_ENABLED(CONFIG_CHARGER_S2MU106)
	{ .name = "s2mu106-charger", },
#endif
#if IS_ENABLED(CONFIG_LEDS_S2MU106_FLASH)
	{ .name = "leds-s2mu106", },
#endif
#if IS_ENABLED(CONFIG_LEDS_S2MU106_RGB)
	{ .name = "leds-s2mu106-rgb", },
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MU106)
	{ .name = "s2mu106-muic", },
#endif
#if IS_ENABLED(CONFIG_AFC_S2MU106)
	{ .name = "s2mu106-afc", },
#endif
#if IS_ENABLED(CONFIG_MST_S2MU106)
	{ .name = "s2mu106-mst", },
#endif
#if IS_ENABLED(CONFIG_PM_S2MU106)
	{ .name = "s2mu106-powermeter", },
#endif
#if IS_ENABLED(CONFIG_INPUT_S2MU106_HAPTIC)
	{ .name = "s2mu106-haptic", },
#endif
#if IS_ENABLED(CONFIG_REGULATOR_S2MU106)
	{ .name = "s2mu106-regulator", },
#endif
};

int s2mu106_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mu106->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&s2mu106->i2c_lock);
	if (ret < 0) {
		pr_err("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME,
				__func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mu106_read_reg);

int s2mu106_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mu106->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&s2mu106->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mu106_bulk_read);

int s2mu106_read_word(struct i2c_client *i2c, u8 reg)
{
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mu106->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&s2mu106->i2c_lock);
	if (ret < 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL_GPL(s2mu106_read_word);

int s2mu106_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mu106->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&s2mu106->i2c_lock);
	if (ret < 0)
		pr_err("%s:%s reg(0x%x), ret(%d)\n",
				MFD_DEV_NAME, __func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(s2mu106_write_reg);

int s2mu106_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mu106->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&s2mu106->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mu106_bulk_write);

int s2mu106_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mu106->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&s2mu106->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mu106_write_word);

int s2mu106_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(i2c);
	int ret;
	u8 old_val, new_val;

	mutex_lock(&s2mu106->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		old_val = ret & 0xff;
		new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&s2mu106->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(s2mu106_update_reg);

#if IS_ENABLED(CONFIG_OF)
static int of_s2mu106_dt(struct device *dev,
				struct s2mu106_platform_data *pdata)
{
	struct device_node *np_s2mu106 = dev->of_node;
	int ret = 0;

	if (!np_s2mu106)
		return -EINVAL;

	ret = of_get_named_gpio(np_s2mu106, "s2mu106,irq-gpio", 0);
	if (!gpio_is_valid(ret))
		return ret;

	pdata->irq_gpio = ret;
	pdata->wakeup = of_property_read_bool(np_s2mu106, "s2mu106,wakeup");

	pr_debug("%s: irq-gpio: %u\n", __func__, pdata->irq_gpio);

	return 0;
}
#else
static int of_s2mu106_dt(struct device *dev,
				struct max77834_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mu106_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *dev_id)
{
	struct s2mu106_dev *s2mu106;
	struct s2mu106_platform_data *pdata = i2c->dev.platform_data;

	int ret = 0;
	u8 temp = 0;

	pr_info("%s: start\n", __func__);

	s2mu106 = devm_kzalloc(&i2c->dev, sizeof(struct s2mu106_dev), GFP_KERNEL);
	if (!s2mu106) {
		ret = -ENOMEM;
		goto err;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev,
			 sizeof(struct s2mu106_platform_data), GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto err;
		}

		ret = of_s2mu106_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node\n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	s2mu106->dev = &i2c->dev;
	s2mu106->i2c = i2c;
	s2mu106->irq = i2c->irq;
	if (pdata) {
		s2mu106->pdata = pdata;
#if IS_ENABLED(CONFIG_CHARGER_S2MU106) || IS_ENABLED(CONFIG_LEDS_S2MU106_FLASH) || \
	IS_ENABLED(CONFIG_HV_MUIC_S2MU106_AFC) || IS_ENABLED(CONFIG_MUIC_S2MU106) || \
	IS_ENABLED(CONFIG_PM_S2MU106) || IS_ENABLED(CONFIG_MST_S2MU106) || \
	IS_ENABLED(CONFIG_MOTOR_S2MU106) || IS_ENABLED(CONFIG_REGULATOR_S2MU106)
		pdata->irq_base = irq_alloc_descs(-1, 0, S2MU106_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
				MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			s2mu106->irq_base = pdata->irq_base;
#endif
		s2mu106->irq_gpio = pdata->irq_gpio;
		s2mu106->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&s2mu106->i2c_lock);

	i2c_set_clientdata(i2c, s2mu106);

	s2mu106_read_reg(s2mu106->i2c, S2MU106_REG_PMICID, &temp);

	s2mu106->pmic_ver = temp & S2MU106_REG_PMICID_MASK;
	pr_err("%s : ver=0x%x\n", __func__, s2mu106->pmic_ver);

	/* I2C enable for MUIC, AFC, MST, Powermeter */
	s2mu106->muic = i2c_new_dummy_device(i2c->adapter, I2C_ADDR_7C_SLAVE);
	i2c_set_clientdata(s2mu106->muic, s2mu106);

	/* I2C enable for Haptic, Haptic Boost */
	s2mu106->haptic = i2c_new_dummy_device(i2c->adapter, I2C_ADDR_HAPTIC);
	i2c_set_clientdata(s2mu106->haptic, s2mu106);

	ret = s2mu106_irq_init(s2mu106);

	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(s2mu106->dev, -1, s2mu106_devs,
			ARRAY_SIZE(s2mu106_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(s2mu106->dev, pdata->wakeup);

	pr_info("%s: end\n", __func__);

	return ret;

err_mfd:
	mutex_destroy(&s2mu106->i2c_lock);
	mfd_remove_devices(s2mu106->dev);
err_irq_init:
	i2c_unregister_device(s2mu106->i2c);
err:
	kfree(s2mu106);
	i2c_set_clientdata(i2c, NULL);

	return ret;
}

static int s2mu106_i2c_remove(struct i2c_client *i2c)
{
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(i2c);

	mfd_remove_devices(s2mu106->dev);
	i2c_unregister_device(s2mu106->i2c);
	i2c_set_clientdata(i2c, NULL);

	return 0;
}

static const struct i2c_device_id s2mu106_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_S2MU106 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mu106_i2c_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id s2mu106_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mu106mfd" },
	{ },
};
#endif /* CONFIG_OF */

#if IS_ENABLED(CONFIG_PM)
static void s2mu106_set_irq_mask(struct s2mu106_dev *s2mu106, int enable)
{
	pr_info("%s: s2mu106 interrupt masking %s\n", __func__, enable > 0 ? "enable" : "disable");

	if (enable)
		s2mu106_write_reg(s2mu106->i2c, 0x09, 0xE3);
	else
		s2mu106_write_reg(s2mu106->i2c, 0x09, 0xE1);
}

static int s2mu106_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(i2c);

	s2mu106_set_irq_mask(s2mu106, 1);

	if (device_may_wakeup(dev))
		enable_irq_wake(s2mu106->irq);

	disable_irq(s2mu106->irq);

	return 0;
}

static int s2mu106_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mu106_dev *s2mu106 = i2c_get_clientdata(i2c);

	pr_debug("%s:%s\n", MFD_DEV_NAME, __func__);

	s2mu106_set_irq_mask(s2mu106, 0);

	if (device_may_wakeup(dev))
		disable_irq_wake(s2mu106->irq);

	enable_irq(s2mu106->irq);

	return 0;
}
#else
#define s2mu106_suspend	NULL
#define s2mu106_resume	NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops s2mu106_pm = {
	.suspend = s2mu106_suspend,
	.resume = s2mu106_resume,
};

static struct i2c_driver s2mu106_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if IS_ENABLED(CONFIG_PM)
		.pm	= &s2mu106_pm,
#endif /* CONFIG_PM */
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= s2mu106_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= s2mu106_i2c_probe,
	.remove		= s2mu106_i2c_remove,
	.id_table	= s2mu106_i2c_id,
};

static int __init s2mu106_i2c_init(void)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&s2mu106_i2c_driver);
}
subsys_initcall(s2mu106_i2c_init);

static void __exit s2mu106_i2c_exit(void)
{
	i2c_del_driver(&s2mu106_i2c_driver);
}
module_exit(s2mu106_i2c_exit);

MODULE_DESCRIPTION("s2mu106 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
