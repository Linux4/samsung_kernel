/*
 *  sm5703.c
 *  Samsung mfd core driver for the SM5703.
 *
 *  Copyright (C) 2016 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/mfd/core.h>
#include <linux/mfd/sm5703.h>
#include <linux/mfd/sm5703-private.h>
#include <linux/regulator/machine.h>

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_MUIC	(0x4A >> 1)

static struct mfd_cell sm5703_devs[] = {
#if defined(CONFIG_MUIC_UNIVERSAL)
	{ .name = "muic-universal", },
#endif
#if defined(CONFIG_MUIC_SM5703)
	{ .name = MUIC_DEV_NAME, },
#endif
#if defined(CONFIG_MOTOR_DRV_SM5703)
	{ .name = "sm5703-haptic", },
#endif
#if defined(CONFIG_LEDS_SM5703_RGB)
	{ .name = "leds-sm5703-rgb", },
#endif
};

static struct sm5703_dev *g_sm5703_dev = NULL;

int sm5703_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest)
{
	struct sm5703_dev *sm5703 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&sm5703->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	mutex_unlock(&sm5703->i2c_lock);
	if (ret < 0) {
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);
		return ret;
	}
	*dest = (ret & 0xff);

    return 0;
}
EXPORT_SYMBOL(sm5703_read_reg);

int sm5703_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct sm5703_dev *sm5703 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&sm5703->i2c_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&sm5703->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(sm5703_bulk_read);

int sm5703_read_word(struct i2c_client *i2c, u8 reg)
{
	struct sm5703_dev *sm5703 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&sm5703->i2c_lock);
	ret = i2c_smbus_read_word_data(i2c, reg);
	mutex_unlock(&sm5703->i2c_lock);
	if (ret < 0)
		return ret;

	return ret;
}
EXPORT_SYMBOL(sm5703_read_word);

int sm5703_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	struct sm5703_dev *sm5703 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&sm5703->i2c_lock);
	ret = i2c_smbus_write_byte_data(i2c, reg, value);
	mutex_unlock(&sm5703->i2c_lock);
	if (ret < 0)
		pr_info("%s:%s reg(0x%x), ret(%d)\n", MFD_DEV_NAME, __func__, reg, ret);

	return ret;
}
EXPORT_SYMBOL(sm5703_write_reg);

int sm5703_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf)
{
	struct sm5703_dev *sm5703 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&sm5703->i2c_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, reg, count, buf);
	mutex_unlock(&sm5703->i2c_lock);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(sm5703_bulk_write);

int sm5703_write_word(struct i2c_client *i2c, u8 reg, u16 value)
{
	struct sm5703_dev *sm5703 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&sm5703->i2c_lock);
	ret = i2c_smbus_write_word_data(i2c, reg, value);
	mutex_unlock(&sm5703->i2c_lock);
	if (ret < 0)
		return ret;
	return 0;
}
EXPORT_SYMBOL(sm5703_write_word);

int sm5703_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask)
{
	struct sm5703_dev *sm5703 = i2c_get_clientdata(i2c);
	int ret;

	mutex_lock(&sm5703->i2c_lock);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret >= 0) {
		u8 old_val = ret & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		ret = i2c_smbus_write_byte_data(i2c, reg, new_val);
	}
	mutex_unlock(&sm5703->i2c_lock);
	return ret;
}
EXPORT_SYMBOL(sm5703_update_reg);

#if defined(CONFIG_OF)
static int of_sm5703_dt(struct device *dev, struct sm5703_platform_data *pdata)
{
	struct device_node *np_sm5703 = dev->of_node;

	if(!np_sm5703)
		return -EINVAL;

	pdata->irq_gpio = of_get_named_gpio(np_sm5703, "muic-universal,irq-gpio", 0);
	pdata->wakeup = of_property_read_bool(np_sm5703, "sm5703, wakeup");

	pr_info("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);
	
	return 0;
}

static int sm5703_pinctrl_select(struct sm5703_dev *sm5703, bool on) {
	struct pinctrl_state *pins_i2c_state;
	int ret;

	pins_i2c_state = on ? sm5703->i2c_gpio_state_active : sm5703->i2c_gpio_state_suspend;

	if (!IS_ERR_OR_NULL(pins_i2c_state)) {
		ret = pinctrl_select_state(sm5703->i2c_pinctrl, pins_i2c_state);
		if(ret) {
			dev_err(sm5703->dev, "%s: fail to set sm5703 i2c pin state.\n", __func__);
			return ret;
		}
	}

	return 0;
}

static int sm5703_i2c_pinctrl_init(struct sm5703_dev *sm5703) {
	int ret = 0;
	struct pinctrl *i2c_pinctrl;

	sm5703->i2c_pinctrl = devm_pinctrl_get(&sm5703->muic->dev);
	if (IS_ERR_OR_NULL(sm5703->i2c_pinctrl)) {
		dev_err(sm5703->dev, "%s: fail to alloc mem for sm5703 i2c pinctrl.\n", __func__);
		ret = PTR_ERR(sm5703->i2c_pinctrl);
		return -ENOMEM;
	}
	i2c_pinctrl = sm5703->i2c_pinctrl;
	
	sm5703->i2c_gpio_state_active = pinctrl_lookup_state(i2c_pinctrl, "muic_active");
	if (IS_ERR_OR_NULL(sm5703->i2c_gpio_state_active)) {
		dev_err(sm5703->dev, "%s: fail to set active state for sm5703 i2c\n", __func__);
		ret = PTR_ERR(sm5703->i2c_gpio_state_active);
		goto err_i2c_active_state;
	}

	sm5703->i2c_gpio_state_suspend = pinctrl_lookup_state(i2c_pinctrl, "muic_suspend");
	if (IS_ERR_OR_NULL(sm5703->i2c_gpio_state_suspend)) {
		dev_err(sm5703->dev, "%s: fail to set suspend state for sm5703 i2c\n", __func__);
		ret = PTR_ERR(sm5703->i2c_gpio_state_suspend);
		goto err_i2c_suspend_state;
	}

#if defined(CONFIG_OF)
	sm5703_pinctrl_select(sm5703, true);
#endif
	return ret;

err_i2c_suspend_state:
	sm5703->i2c_gpio_state_suspend = 0;
err_i2c_active_state:
	sm5703->i2c_gpio_state_active = 0;
	return ret;
}
#else
static int of_sm5703_dt(struct device *dev, struct sm5703_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int sm5703_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *dev_id)
{
	struct sm5703_dev *sm5703;
	struct sm5703_platform_data *pdata = i2c->dev.platform_data;

	int ret = 0;

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);

	sm5703 = kzalloc(sizeof(struct sm5703_dev), GFP_KERNEL);
	if (!sm5703) {
		dev_err(&i2c->dev, "%s: fail to alloc mem for sm5703\n", __func__);
		return -ENOMEM;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(struct sm5703_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "fail to allocate memory \n");
			ret = -ENOMEM;
			goto err;
		}

		ret = of_sm5703_dt(&i2c->dev, pdata);
		if (ret < 0){
			dev_err(&i2c->dev, "fail to get device of_node \n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	}

	sm5703->dev = &i2c->dev;
	sm5703->muic = i2c;
	sm5703->irq = i2c->irq;

	if (pdata) {
		sm5703->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, SM5703_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n", MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else {
			sm5703->irq_base = pdata->irq_base;
        }

		sm5703->irq_gpio = pdata->irq_gpio;
		sm5703->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}

	mutex_init(&sm5703->i2c_lock);

	i2c_set_clientdata(i2c, sm5703);

#if defined(CONFIG_OF)
	/* Initialize pinctrl for i2c & irq */
	ret = sm5703_i2c_pinctrl_init(sm5703);
	if(ret) {
        pr_err("%s:%s sm5703 pinctrl is failed.\n", MFD_DEV_NAME, __func__);
		/* if pnictrl is not supported, -EINVAL is returned*/
		if(ret == -EINVAL)
			ret = 0;
	} else {
		pr_info("%s:%s sm5703 pinctrl is succeed.\n",	MFD_DEV_NAME, __func__);
	}
#endif

	ret = sm5703_irq_init(sm5703);

	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(sm5703->dev, -1, sm5703_devs, ARRAY_SIZE(sm5703_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(sm5703->dev, pdata->wakeup);

	g_sm5703_dev = sm5703;

	return ret;

err_mfd:
	mfd_remove_devices(sm5703->dev);
    sm5703_irq_exit(sm5703);
err_irq_init:
	i2c_unregister_device(sm5703->muic);
#if 0
err_w_lock:
#endif
	mutex_destroy(&sm5703->i2c_lock);

err:
	kfree(sm5703);

    return ret;
}

static int sm5703_i2c_remove(struct i2c_client *i2c)
{
	struct sm5703_dev *sm5703 = i2c_get_clientdata(i2c);

	mfd_remove_devices(sm5703->dev);
    sm5703_irq_exit(sm5703);

	i2c_unregister_device(sm5703->muic);

    mutex_destroy(&sm5703->i2c_lock);

    kfree(sm5703);

	return 0;
}

static const struct i2c_device_id sm5703_i2c_id[] = {
	{ MFD_DEV_NAME, TYPE_SM5703 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sm5703_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id sm5703_i2c_dt_ids[] = {
	{ .compatible = "muic-universal" },
	{ },
};
MODULE_DEVICE_TABLE(of, sm5703_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int sm5703_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5703_dev *sm5703 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(sm5703->irq);

	disable_irq(sm5703->irq);

	return 0;
}

static int sm5703_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5703_dev *sm5703 = i2c_get_clientdata(i2c);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	if (device_may_wakeup(dev))
		disable_irq_wake(sm5703->irq);

	enable_irq(sm5703->irq);

	return 0;
}
#else
#define sm5703_suspend	    NULL
#define sm5703_resume		NULL
#endif /* CONFIG_PM */

const struct dev_pm_ops sm5703_muic_pm = {
	.suspend    = sm5703_suspend,
	.resume     = sm5703_resume,
};

static struct i2c_driver sm5703_i2c_driver = {
	.driver		= {
		.name	= MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	    = &sm5703_muic_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= sm5703_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= sm5703_i2c_probe,
	.remove		= sm5703_i2c_remove,
	.id_table	= sm5703_i2c_id,
};

static int __init sm5703_i2c_init(void)
{
	int ret;

	pr_info("%s:%s\n", MFD_DEV_NAME, __func__);
	ret = i2c_add_driver(&sm5703_i2c_driver);
	if (ret != 0)
		pr_info("%s : Failed to register SM5703 MUIC MFD I2C driver\n",
		__func__);

	return ret;
}
/* init early so consumer devices can complete system boot */
subsys_initcall(sm5703_i2c_init);

static void __exit sm5703_i2c_exit(void)
{
	i2c_del_driver(&sm5703_i2c_driver);
}
module_exit(sm5703_i2c_exit);

MODULE_DESCRIPTION("SM5703 multi-function core driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
