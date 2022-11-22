/*
 * max77888.c - mfd core driver for the Maxim 77888
 *
 * Copyright (C) 2011 Samsung Electronics
 * SeoYoung Jeong <seo0.jeong@samsung.com>
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
 * This driver is based on max8997.c
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77888.h>
#include <linux/mfd/max77888-private.h>

#include <linux/muic/max77888-muic.h>

#if !defined(CONFIG_SEC_FACTORY)
#include <linux/muic/muic.h>
#endif /* CONFIG_SEC_FACTORY */

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_PMIC	(0xCC >> 1)	/* Charger, Flash LED */
#define I2C_ADDR_MUIC	(0x4A >> 1)
#define I2C_ADDR_HAPTIC	(0x90 >> 1)
#define I2C_ADDR_TEST	(0xCE >> 1)	/* TEST register */

static struct mfd_cell max77888_devs[] = {
#if defined(CONFIG_CHARGER_MAX77888)
	{ .name = "max77888-charger", },
#endif /* CONFIG_CHARGER_MAX77888 */
#if defined(CONFIG_LEDS_MAX77888)
	{ .name = "max77888-led", },
#endif /* CONFIG_LEDS_MAX77888 */
#if defined(CONFIG_MUIC_MAX77888)
	{ .name = MUIC_DEV_NAME, },
#endif /* CONFIG_MUIC_MAX77888 */
#if defined(CONFIG_USE_SAFEOUT)
	{ .name = "max77888-safeout", },
#endif /* CONFIG_USE_SAFEOUT */
#if defined(CONFIG_MOTOR_DRV_MAX77888)
	{ .name = "max77888-haptic", },
#endif /* CONFIG_MAX77888_HAPTIC */
};

/* WA for MUIC RESET */
static u8 muic_reg_snapshot[MAX77888_MUIC_REG_END];

static void max77888_reg_snapshot(u8 reg, u8 value)
{
	if (reg < MAX77888_MUIC_REG_END)
		muic_reg_snapshot[reg] = value;
}

u8 max77888_restore_last_snapshot(u8 reg)
{
	return muic_reg_snapshot[reg];
}
/* WA for MUIC RESET */

int max77888_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77888->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&max77888->i2c_lock);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
		return ret;
	}

	ret &= 0xff;
	*dest = ret;
	return 0;
}
EXPORT_SYMBOL_GPL(max77888_read_reg);

int max77888_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77888->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77888->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(max77888_bulk_read);

int max77888_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77888->i2c_lock);
/* WA for MUIC RESET */
	if (i2c->addr == I2C_ADDR_MUIC)
		max77888_reg_snapshot(reg, value);
/* WA for MUIC RESET */
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&max77888->i2c_lock);
	if (ret < 0)
		pr_info("%s:%s reg(0x%x), ret(%d)\n",
				MFD_DEV_NAME, __func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(max77888_write_reg);

int max77888_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77888->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&max77888->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(max77888_bulk_write);

static int max77888_read_word(struct i2c_client *i2c, u8 reg)
{
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77888->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&max77888->i2c_lock);

	return ret;
}

int max77888_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&max77888->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
/* WA for MUIC RESET */
		if (i2c->addr == I2C_ADDR_MUIC)
			max77888_reg_snapshot(reg, new_val);
/* WA for MUIC RESET */
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&max77888->i2c_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(max77888_update_reg);

#if defined(CONFIG_OF)
static int of_max77888_dt(struct device *dev, struct max77888_platform_data *pdata)
{
	struct device_node *np_max77888 = dev->of_node;

	if(!np_max77888)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio(np_max77888, "max77888,irq-gpio", 0);
	/* WA for MUIC RESET */
	pdata->muic_reset_irq = of_get_named_gpio(np_max77888, "max77888,muic-reset-int", 0);
	pdata->wakeup = of_property_read_bool(np_max77888, "max77888,wakeup");

	pr_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);

	return 0;
}
#else
static int of_max77888_dt(struct device *dev, struct max77888_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int max77888_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *dev_id)
{
	struct max77888_dev *max77888;
	struct max77888_platform_data *pdata = i2c->dev.platform_data;

	u8 reg_data;
	u16 reg_data16;
	u8 str_data[10] = {0,};
	int i;
	int ret = 0;

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);

	max77888 = kzalloc(sizeof(struct max77888_dev), GFP_KERNEL);
	if (!max77888) {
		dev_err(&i2c->dev, "%s: Failed to alloc mem for max77888\n", __func__);
		return -ENOMEM;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(struct max77888_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory \n");
			ret = -ENOMEM;
			goto err;
		}

		ret = of_max77888_dt(&i2c->dev, pdata);
		if (ret < 0){
			dev_err(&i2c->dev, "Failed to get device of_node \n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	max77888->dev = &i2c->dev;
	max77888->i2c = i2c;
	max77888->irq = i2c->irq;
	if (pdata) {
		max77888->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, MAX77888_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
					MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			max77888->irq_base = pdata->irq_base;

		max77888->irq_gpio = pdata->irq_gpio;
		/* WA for MUIC RESET */
		max77888->muic_reset_irq = pdata->muic_reset_irq;
		max77888->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&max77888->i2c_lock);

	i2c_set_clientdata(i2c, max77888);

	if (max77888_read_reg(i2c, MAX77888_PMIC_REG_PMIC_ID2, &reg_data) < 0) {
		pr_err("%s:%s device not found on this channel (this is not an error)\n",
			MFD_DEV_NAME, __func__);
		ret = -ENODEV;
		goto err_w_lock;
	} else {
		/* print rev */
		max77888->pmic_rev = (reg_data & 0x7);
		max77888->pmic_ver = ((reg_data & 0xF8) >> 0x3);
		pr_info("%s:%s device found: rev.0x%x, ver.0x%x\n",
				MFD_DEV_NAME, __func__,
				max77888->pmic_rev, max77888->pmic_ver);
	}

	/* No active discharge on safeout ldo 1,2 */
	max77888_update_reg(i2c, MAX77888_CHG_REG_SAFEOUT_CTRL, 0x00, 0x30);

	max77888->muic = i2c_new_dummy(i2c->adapter, I2C_ADDR_MUIC);
	i2c_set_clientdata(max77888->muic, max77888);

	max77888->haptic = i2c_new_dummy(i2c->adapter, I2C_ADDR_HAPTIC);
	i2c_set_clientdata(max77888->haptic, max77888);

	// Set TEST Reigster Slave address
	max77888->test = i2c_new_dummy(i2c->adapter, I2C_ADDR_TEST);
	i2c_set_clientdata(max77888->test, max77888);
	// Start Over-write wrong-Trimmed bit //
	// 1. Test Register Access Enabled
	max77888_write_reg(max77888->i2c, 0xFE, 0xC5);
	// 2. Enable TST_KEY
	max77888_write_reg(max77888->test, 0xB3, 0x0C);
	// 3. Read 0x2E with word unit.
	reg_data16 = max77888_read_word(max77888->test, 0x2E);
	// 4. Check Bit5 of First bit(Bit13)
	if ((reg_data16 & 0x2000) == 0) {
		// Wrong Trimmed
		// 5. Read and Store
		// 5-1. Read and Store from 0x21 to 0x2A
		for (i = 0x21; i <= 0x2A; i++) {
			if (i == 0x25) {
				continue;
			}
			reg_data16 = max77888_read_word(max77888->test, i);
			str_data[i-0x21] = (reg_data16 >> 8);
		}
		// 5-2. Read and Store 0x2E
		reg_data16 = max77888_read_word(max77888->test, i);
		reg_data = (reg_data16 >> 8);
		// 6. Write Stored data from 0x21 to 0x2A.
		for (i = 0x21; i <= 0x2A; i++) {
			if ( i == 0x25) {
				continue;
			}
			max77888_write_reg(max77888->test, i, str_data[i-0x21]);
		}
		// 7. Write 0x2E
		max77888_write_reg(max77888->test, 0x2E, (reg_data | 0x20));
		// 8. Write 0x20 to 0x40.
		max77888_write_reg(max77888->test, 0x20, 0x40);
	}
	// 9. Disable TST_KEY, Write 0xB3 to 0x00
	max77888_write_reg(max77888->test, 0xB3, 0x00);
	// 10. Test Register Access Disabled, Write 0xFE to 0x00
	max77888_write_reg(max77888->i2c, 0xFE, 0x00);

	ret = max77888_irq_init(max77888);

	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(max77888->dev, -1, max77888_devs,
			ARRAY_SIZE(max77888_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(max77888->dev, pdata->wakeup);

	return ret;

err_mfd:
	mfd_remove_devices(max77888->dev);
err_irq_init:
	i2c_unregister_device(max77888->muic);
	i2c_unregister_device(max77888->haptic);
err_w_lock:
	mutex_destroy(&max77888->i2c_lock);
err:
	kfree(max77888);
	return ret;
}

static int max77888_i2c_remove(struct i2c_client *i2c)
{
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);

	mfd_remove_devices(max77888->dev);
	i2c_unregister_device(max77888->muic);
	i2c_unregister_device(max77888->haptic);
	kfree(max77888);

	return 0;
}

static const struct i2c_device_id max77888_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_MAX77888 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max77888_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id max77888_i2c_dt_ids[] = {
	{ .compatible = "maxim,max77888" },
	{ },
};
MODULE_DEVICE_TABLE(of, max77888_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int max77888_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(max77888->irq);

	disable_irq(max77888->irq);

	return 0;
}

static int max77888_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	if (device_may_wakeup(dev))
		disable_irq_wake(max77888->irq);

	enable_irq(max77888->irq);

	return 0;
}
#else
#define max77888_suspend	NULL
#define max77888_resume		NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_HIBERNATION

u8 max77888_dumpaddr_pmic[] = {
	MAX77888_LED_REG_IFLASH,
	MAX77888_LED_REG_ITORCH,
	MAX77888_LED_REG_ITORCHTORCHTIMER,
	MAX77888_LED_REG_FLASH_TIMER,
	MAX77888_LED_REG_FLASH_EN,
	MAX77888_LED_REG_MAX_FLASH1,
	MAX77888_LED_REG_MAX_FLASH2,
	MAX77888_LED_REG_VOUT_CNTL,
	MAX77888_LED_REG_VOUT_FLASH,
	MAX77888_LED_REG_FLASH_INT_STATUS,
	MAX77888_PMIC_REG_TOPSYS_INT_MASK,
	MAX77888_PMIC_REG_MAINCTRL1,
	MAX77888_PMIC_REG_LSCNFG,
};

u8 max77888_dumpaddr_muic[] = {
	MAX77888_MUIC_REG_INTMASK1,
	MAX77888_MUIC_REG_INTMASK2,
	MAX77888_MUIC_REG_CDETCTRL1,
	MAX77888_MUIC_REG_CDETCTRL2,
	MAX77888_MUIC_REG_CTRL1,
	MAX77888_MUIC_REG_CTRL2,
	MAX77888_MUIC_REG_CTRL3,
	MAX77888_MUIC_REG_CTRL4,
};

u8 max77888_dumpaddr_haptic[] = {
	MAX77888_HAPTIC_REG_CONFIG1,
	MAX77888_HAPTIC_REG_CONFIG2,
	MAX77888_HAPTIC_REG_CONFIG_CHNL,
	MAX77888_HAPTIC_REG_CONFG_CYC1,
	MAX77888_HAPTIC_REG_CONFG_CYC2,
	MAX77888_HAPTIC_REG_CONFIG_PER1,
	MAX77888_HAPTIC_REG_CONFIG_PER2,
	MAX77888_HAPTIC_REG_CONFIG_PER3,
	MAX77888_HAPTIC_REG_CONFIG_PER4,
	MAX77888_HAPTIC_REG_CONFIG_DUTY1,
	MAX77888_HAPTIC_REG_CONFIG_DUTY2,
	MAX77888_HAPTIC_REG_CONFIG_PWM1,
	MAX77888_HAPTIC_REG_CONFIG_PWM2,
	MAX77888_HAPTIC_REG_CONFIG_PWM3,
	MAX77888_HAPTIC_REG_CONFIG_PWM4,
};

static int max77888_freeze(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < ARRAY_SIZE(max77888_dumpaddr_pmic); i++)
		max77888_read_reg(i2c, max77888_dumpaddr_pmic[i],
				&max77888->reg_pmic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77888_dumpaddr_muic); i++)
		max77888_read_reg(i2c, max77888_dumpaddr_muic[i],
				&max77888->reg_muic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77888_dumpaddr_haptic); i++)
		max77888_read_reg(i2c, max77888_dumpaddr_haptic[i],
				&max77888->reg_haptic_dump[i]);

	disable_irq(max77888->irq);

	return 0;
}

static int max77888_restore(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct max77888_dev *max77888 = i2c_get_clientdata(i2c);
	int i;

	enable_irq(max77888->irq);

	for (i = 0; i < ARRAY_SIZE(max77888_dumpaddr_pmic); i++)
		max77888_write_reg(i2c, max77888_dumpaddr_pmic[i],
				max77888->reg_pmic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77888_dumpaddr_muic); i++)
		max77888_write_reg(i2c, max77888_dumpaddr_muic[i],
				max77888->reg_muic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(max77888_dumpaddr_haptic); i++)
		max77888_write_reg(i2c, max77888_dumpaddr_haptic[i],
				max77888->reg_haptic_dump[i]);

	return 0;
}
#endif

const struct dev_pm_ops max77888_pm = {
	.suspend = max77888_suspend,
	.resume = max77888_resume,
#ifdef CONFIG_HIBERNATION
	.freeze =  max77888_freeze,
	.thaw = max77888_restore,
	.restore = max77888_restore,
#endif
};

static struct i2c_driver max77888_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &max77888_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= max77888_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= max77888_i2c_probe,
	.remove		= max77888_i2c_remove,
	.id_table	= max77888_i2c_id,
};

static int __init max77888_i2c_init(void)
{
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	return i2c_add_driver(&max77888_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(max77888_i2c_init);

static void __exit max77888_i2c_exit(void)
{
	i2c_del_driver(&max77888_i2c_driver);
}
module_exit(max77888_i2c_exit);

MODULE_DESCRIPTION("MAXIM 77888 multi-function core driver");
MODULE_AUTHOR("SeoYoung Jeong <seo0.jeong@samsung.com>");
MODULE_LICENSE("GPL");
