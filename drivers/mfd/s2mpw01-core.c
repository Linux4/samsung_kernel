/*
 * s2mpw01.c - mfd core driver for the s2mpw01
 *
 * Copyright (C) 2015 Samsung Electronics
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
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/samsung/s2mpw01.h>
#include <linux/mfd/samsung/s2mpw01-private.h>
#include <linux/regulator/machine.h>
#include <linux/rtc.h>

#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_TOP	0x00
#define I2C_ADDR_PMIC	0x01
#define I2C_ADDR_RTC	0x02
#define I2C_ADDR_CHG	0x04
#define I2C_ADDR_FG	0x05

static struct mfd_cell s2mpw01_devs[] = {
#ifdef CONFIG_REGULATOR_S2MPW01
	{ .name = "s2mpw01-regulator", },
#endif
#ifdef CONFIG_RTC_DRV_S2MPW01
	{ .name = "s2mp-rtc", },
#endif
#ifdef CONFIG_SEC_CHARGER_S2MPW01
	{ .name = "s2mpw01-charger", },
#endif
#ifdef CONFIG_SEC_FUELGAUGE_S2MPW01
	{ .name = "s2mpw01-fuelgauge", },
#endif
#ifdef CONFIG_KEYBOARD_S2MPW01
	{ .name = "s2mpw01-power-keys", },
#endif
};

#ifdef CONFIG_OF
static struct of_device_id s2mpw01_i2c_dt_ids[] = {
	{ .compatible = "samsung,s2mpw01mfd"},
	{},
};
#endif

int s2mpw01_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct s2mpw01_dev *s2mpw01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpw01->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&s2mpw01->i2c_lock);
	if (ret < 0) {
		dev_err(&i2c->dev, "%s() reg(0x%x), ret(%d)\n",
				__func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpw01_read_reg);

int s2mpw01_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mpw01_dev *s2mpw01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpw01->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&s2mpw01->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpw01_bulk_read);

int s2mpw01_read_word(struct i2c_client *i2c, u8 reg)
{
	struct s2mpw01_dev *s2mpw01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpw01->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&s2mpw01->i2c_lock);
	if (ret < 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL_GPL(s2mpw01_read_word);

int s2mpw01_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct s2mpw01_dev *s2mpw01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpw01->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&s2mpw01->i2c_lock);
	if (ret < 0)
		dev_err(&i2c->dev, "%s() reg(0x%x), ret(%d)\n",
				__func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(s2mpw01_write_reg);

int s2mpw01_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct s2mpw01_dev *s2mpw01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpw01->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&s2mpw01->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(s2mpw01_bulk_write);

int s2mpw01_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct s2mpw01_dev *s2mpw01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpw01->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&s2mpw01->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}
EXPORT_SYMBOL_GPL(s2mpw01_write_word);

int s2mpw01_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct s2mpw01_dev *s2mpw01 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&s2mpw01->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&s2mpw01->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(s2mpw01_update_reg);

#if defined(CONFIG_OF)
static int of_s2mpw01_dt(struct device *dev,
			struct s2mpw01_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret, strlen;
	const char *status;
	u32 val;

	if (!np)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio(np, "s2mpw01,irq-gpio", 0);
	status = of_get_property(np, "s2mpw01,wakeup", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wakeup = true;
		else
			pdata->wakeup = false;
	}

	if (of_get_property(np, "i2c-speedy-address", NULL))
		pdata->use_i2c_speedy = true;

	pr_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);

	ret = of_property_read_u32(np, "cache_data", &val);
	if (ret)
		return -EINVAL;
	pdata->cache_data = !!val;

	/* WTSR */
	pdata->wtsr_smpl = devm_kzalloc(dev, sizeof(*pdata->wtsr_smpl),
			GFP_KERNEL);
	if (!pdata->wtsr_smpl)
		return -ENOMEM;

	status = of_get_property(np, "wtsr_en", &strlen);
	if (status == NULL)
		return -EINVAL;
	if (strlen > 0) {
		if (!strcmp(status, "enabled") || !strcmp(status, "okay"))
			pdata->wtsr_smpl->wtsr_en = true;
		else
			pdata->wtsr_smpl->wtsr_en = false;
	}

	ret = of_property_read_u32(np, "wtsr_timer_val",
			&pdata->wtsr_smpl->wtsr_timer_val);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "check_jigon", &val);
	if (ret)
		return -EINVAL;
	pdata->wtsr_smpl->check_jigon = !!val;

	/* init time */
	pdata->init_time = devm_kzalloc(dev, sizeof(*pdata->init_time),
			GFP_KERNEL);
	if (!pdata->init_time)
		return -ENOMEM;

	ret = of_property_read_u32(np, "init_time,sec",
			&pdata->init_time->tm_sec);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,min",
			&pdata->init_time->tm_min);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,hour",
			&pdata->init_time->tm_hour);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,mday",
			&pdata->init_time->tm_mday);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,mon",
			&pdata->init_time->tm_mon);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,year",
			&pdata->init_time->tm_year);
	if (ret)
		return -EINVAL;

	ret = of_property_read_u32(np, "init_time,wday",
			&pdata->init_time->tm_wday);
	if (ret)
		return -EINVAL;
	return 0;
}
#else
static int of_s2mpw01_dt(struct device *dev, struct max77834_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int s2mpw01_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *dev_id)
{
	struct s2mpw01_dev *s2mpw01;
	struct s2mpw01_platform_data *pdata = i2c->dev.platform_data;

	u8 reg_data;
	int ret = 0;

	dev_info(&i2c->dev, "%s() started...\n", __func__);

	s2mpw01 = kzalloc(sizeof(struct s2mpw01_dev), GFP_KERNEL);
	if (!s2mpw01) {
		dev_err(&i2c->dev, "%s: Failed to alloc mem for s2mpw01\n", __func__);
		return -ENOMEM;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(struct s2mpw01_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory \n");
			ret = -ENOMEM;
			goto err;
		}

		ret = of_s2mpw01_dt(&i2c->dev, pdata);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to get device of_node \n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	s2mpw01->dev = &i2c->dev;
	s2mpw01->i2c = i2c;
	s2mpw01->irq = i2c->irq;

	if (pdata) {
		s2mpw01->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, S2MPW01_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			dev_err(&i2c->dev, "%s() irq_alloc_descs Fail! ret(%d)\n",
					__func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			s2mpw01->irq_base = pdata->irq_base;

		s2mpw01->irq_gpio = pdata->irq_gpio;
		s2mpw01->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&s2mpw01->i2c_lock);

	i2c_set_clientdata(i2c, s2mpw01);

	if (s2mpw01_read_reg(i2c, S2MPW01_PMIC_REG_PMICID, &reg_data) < 0) {
		dev_err(s2mpw01->dev,
			"device not found on this channel (this is not an error)\n");
		ret = -ENODEV;
		goto err_w_lock;
	} else {
		/* print rev */
		s2mpw01->pmic_rev = (reg_data & 0x0F);
		s2mpw01->pmic_ver = (reg_data & 0xF0);
		dev_info(&i2c->dev, "%s() device found: rev.0x%x, ver.0x%x\n",
				__func__,
				s2mpw01->pmic_rev, s2mpw01->pmic_ver);
	}

	s2mpw01->pmic = i2c_new_dummy(i2c->adapter, I2C_ADDR_PMIC);
	s2mpw01->rtc = i2c_new_dummy(i2c->adapter, I2C_ADDR_RTC);
	s2mpw01->charger = i2c_new_dummy(i2c->adapter, I2C_ADDR_CHG);
	s2mpw01->fuelgauge = i2c_new_dummy(i2c->adapter, I2C_ADDR_FG);

	if (pdata->use_i2c_speedy) {
		dev_err(s2mpw01->dev, "use_i2c_speedy was true\n");
		s2mpw01->pmic->flags |= I2C_CLIENT_SPEEDY;
		s2mpw01->rtc->flags |= I2C_CLIENT_SPEEDY;
		s2mpw01->charger->flags |= I2C_CLIENT_SPEEDY;
		s2mpw01->fuelgauge->flags |= I2C_CLIENT_SPEEDY;
	}

	i2c_set_clientdata(s2mpw01->pmic, s2mpw01);
	i2c_set_clientdata(s2mpw01->rtc, s2mpw01);
	i2c_set_clientdata(s2mpw01->charger, s2mpw01);
	i2c_set_clientdata(s2mpw01->fuelgauge, s2mpw01);

	ret = s2mpw01_irq_init(s2mpw01);
	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(s2mpw01->dev, -1, s2mpw01_devs,
			ARRAY_SIZE(s2mpw01_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(s2mpw01->dev, pdata->wakeup);

	return ret;

err_mfd:
	mfd_remove_devices(s2mpw01->dev);
err_irq_init:
	i2c_unregister_device(s2mpw01->i2c);
err_w_lock:
	mutex_destroy(&s2mpw01->i2c_lock);
err:
	kfree(s2mpw01);
	return ret;
}

static int s2mpw01_i2c_remove(struct i2c_client *i2c)
{
	struct s2mpw01_dev *s2mpw01 = i2c_get_clientdata(i2c);

	mfd_remove_devices(s2mpw01->dev);
	i2c_unregister_device(s2mpw01->i2c);
	kfree(s2mpw01);

	return 0;
}

static const struct i2c_device_id s2mpw01_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_S2MPW01 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, s2mpw01_i2c_id);

#if defined(CONFIG_PM)
static int s2mpw01_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mpw01_dev *s2mpw01 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(s2mpw01->irq);

	disable_irq(s2mpw01->irq);

	return 0;
}

static int s2mpw01_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mpw01_dev *s2mpw01 = i2c_get_clientdata(i2c);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	dev_info(dev, "%s()\n", __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	if (device_may_wakeup(dev))
		disable_irq_wake(s2mpw01->irq);

	enable_irq(s2mpw01->irq);

	return 0;
}
#else
#define s2mpw01_suspend	NULL
#define s2mpw01_resume NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops s2mpw01_pm = {
	.suspend_late = s2mpw01_suspend,
	.resume_early = s2mpw01_resume,
};

static struct i2c_driver s2mpw01_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &s2mpw01_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= s2mpw01_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= s2mpw01_i2c_probe,
	.remove		= s2mpw01_i2c_remove,
	.id_table	= s2mpw01_i2c_id,
};

static int __init s2mpw01_i2c_init(void)
{
	return i2c_add_driver(&s2mpw01_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(s2mpw01_i2c_init);

static void __exit s2mpw01_i2c_exit(void)
{
	i2c_del_driver(&s2mpw01_i2c_driver);
}
module_exit(s2mpw01_i2c_exit);

MODULE_DESCRIPTION("s2mpw01 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
