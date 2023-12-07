/*
 * sm5705.c - mfd core driver for the SM5705
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
#include <linux/mfd/sm5705/sm5705.h>
#include <linux/regulator/machine.h>

#include <linux/muic/muic.h>

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#endif /* CONFIG_OF */

#define I2C_ADDR_PMIC	(0x92 >> 1)

static struct mfd_cell sm5705_devs[] = {
#if defined(CONFIG_MUIC_UNIVERSAL)
	{ .name = "muic-universal", },
#endif
#if defined(CONFIG_MUIC_SM5705)
	{ .name = MUIC_DEV_NAME, },
#endif
};


#if defined(CONFIG_OF)
static int of_sm5705_dt(struct device *dev, struct sm5705_platform_data *pdata)
{
	struct device_node *np_sm5705 = dev->of_node;
	struct platform_device *pdev = to_platform_device(dev);

	if(!np_sm5705)
		return -EINVAL;

	pdata->irq_gpio = platform_get_irq_byname(pdev, "sm5705_irq");
	pr_info("%s: irq-gpio: %u\n", __func__, pdata->irq_gpio);
	pdata->wakeup = of_property_read_bool(np_sm5705, "sm5705,wakeup");


	return 0;
}
#else
static int of_sm5705_dt(struct device *dev, struct sm5705_platform_data *pdata)
{
	return 0;
}
#endif /* CONFIG_OF */

static int sm5705_universal_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *dev_id)
{
	struct sm5705_dev *sm5705;
	struct sm5705_platform_data *pdata = i2c->dev.platform_data;
	int ret = 0;

	pr_info("%s:%s\n", UNIVERSAL_MFD_DEV_NAME, __func__);

	sm5705 = kzalloc(sizeof(struct sm5705_dev), GFP_KERNEL);
	if (!sm5705) {
		dev_err(&i2c->dev, "%s: Failed to alloc mem for sm5705\n", __func__);
		return -ENOMEM;
	}

	if (i2c->dev.of_node) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(struct sm5705_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c->dev, "Failed to allocate memory \n");
			ret = -ENOMEM;
			goto err;
		}

		ret = of_sm5705_dt(&i2c->dev, pdata);
		if (ret < 0){
			dev_err(&i2c->dev, "Failed to get device of_node \n");
			goto err;
		}

		i2c->dev.platform_data = pdata;
	} else
		pdata = i2c->dev.platform_data;

	sm5705->dev = &i2c->dev;
	sm5705->i2c = sm5705->muic = i2c;
	sm5705->irq = i2c->irq;
	if (pdata) {
		sm5705->pdata = pdata;

		pdata->irq_base = irq_alloc_descs(-1, 0, SM5705_IRQ_NR, -1);
		if (pdata->irq_base < 0) {
			pr_err("%s:%s irq_alloc_descs Fail! ret(%d)\n",
					UNIVERSAL_MFD_DEV_NAME, __func__, pdata->irq_base);
			ret = -EINVAL;
			goto err;
		} else
			sm5705->irq_base = pdata->irq_base;

		sm5705->irq_gpio = pdata->irq_gpio;
		sm5705->wakeup = pdata->wakeup;
	} else {
		ret = -EINVAL;
		goto err;
	}
	mutex_init(&sm5705->i2c_lock);

	i2c_set_clientdata(i2c, sm5705);

	ret = sm5705_muic_irq_init(sm5705);

	if (ret < 0)
		goto err_irq_init;

	ret = mfd_add_devices(sm5705->dev, -1, sm5705_devs,
			ARRAY_SIZE(sm5705_devs), NULL, 0, NULL);
	if (ret < 0)
		goto err_mfd;

	device_init_wakeup(sm5705->dev, pdata->wakeup);

	pr_info("%s:%s DONE\n", UNIVERSAL_MFD_DEV_NAME, __func__);

	return ret;

err_mfd:
	mfd_remove_devices(sm5705->dev);
err_irq_init:
	mutex_destroy(&sm5705->i2c_lock);
err:
	kfree(sm5705);
	return ret;
}

static int sm5705_universal_i2c_remove(struct i2c_client *i2c)
{
	struct sm5705_dev *sm5705 = i2c_get_clientdata(i2c);

	mfd_remove_devices(sm5705->dev);
	kfree(sm5705);

	return 0;
}

static const struct i2c_device_id sm5705_universal_i2c_id[] = {
	{ UNIVERSAL_MFD_DEV_NAME, TYPE_SM5705 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sm5705_universal_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id sm5705_universal_i2c_dt_ids[] = {
	{ .compatible = "universal-sm5705" },
	{ },
};
MODULE_DEVICE_TABLE(of, sm5705_universal_i2c_dt_ids);
#endif /* CONFIG_OF */

#if defined(CONFIG_PM)
static int sm5705_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5705_dev *sm5705 = i2c_get_clientdata(i2c);

	if (device_may_wakeup(dev))
		enable_irq_wake(sm5705->irq);

	disable_irq(sm5705->irq);

	return 0;
}

static int sm5705_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5705_dev *sm5705 = i2c_get_clientdata(i2c);

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	pr_info("%s:%s\n", UNIVERSAL_MFD_DEV_NAME, __func__);
#endif /* CONFIG_SAMSUNG_PRODUCT_SHIP */

	if (device_may_wakeup(dev))
		disable_irq_wake(sm5705->irq);

	enable_irq(sm5705->irq);

	return 0;
}
#else
#define sm5705_suspend	NULL
#define sm5705_resume		NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_HIBERNATION

u8 sm5705_dumpaddr_led[] = {
	SM5705_RGBLED_REG_LEDEN,
	SM5705_RGBLED_REG_LED0BRT,
	SM5705_RGBLED_REG_LED1BRT,
	SM5705_RGBLED_REG_LED2BRT,
	SM5705_RGBLED_REG_LED3BRT,
	SM5705_RGBLED_REG_LEDBLNK,
	SM5705_RGBLED_REG_LEDRMP,
};

static int sm5705_freeze(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5705_dev *sm5705 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < ARRAY_SIZE(sm5705_dumpaddr_pmic); i++)
		sm5705_read_reg(i2c, sm5705_dumpaddr_pmic[i],
				&sm5705->reg_pmic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(sm5705_dumpaddr_led); i++)
		sm5705_read_reg(i2c, sm5705_dumpaddr_led[i],
				&sm5705->reg_led_dump[i]);

	disable_irq(sm5705->irq);

	return 0;
}

static int sm5705_restore(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5705_dev *sm5705 = i2c_get_clientdata(i2c);
	int i;

	enable_irq(sm5705->irq);

	for (i = 0; i < ARRAY_SIZE(sm5705_dumpaddr_pmic); i++)
		sm5705_write_reg(i2c, sm5705_dumpaddr_pmic[i],
				sm5705->reg_pmic_dump[i]);

	for (i = 0; i < ARRAY_SIZE(sm5705_dumpaddr_led); i++)
		sm5705_write_reg(i2c, sm5705_dumpaddr_led[i],
				sm5705->reg_led_dump[i]);

	return 0;
}
#endif

const struct dev_pm_ops sm5705_universal_pm = {
	.suspend = sm5705_suspend,
	.resume = sm5705_resume,
#ifdef CONFIG_HIBERNATION
	.freeze =  sm5705_freeze,
	.thaw = sm5705_restore,
	.restore = sm5705_restore,
#endif
};

static struct i2c_driver sm5705_universal_i2c_driver = {
	.driver		= {
		.name	= UNIVERSAL_MFD_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &sm5705_universal_pm,
#endif /* CONFIG_PM */
#if defined(CONFIG_OF)
		.of_match_table	= sm5705_universal_i2c_dt_ids,
#endif /* CONFIG_OF */
	},
	.probe		= sm5705_universal_i2c_probe,
	.remove		= sm5705_universal_i2c_remove,
	.id_table	= sm5705_universal_i2c_id,
};

static int __init sm5705_universal_i2c_init(void)
{
	int ret;

	pr_info("%s:%s\n", UNIVERSAL_MFD_DEV_NAME, __func__);
	ret = i2c_add_driver(&sm5705_universal_i2c_driver);
	if (ret != 0)
		pr_info("%s : Failed to register SM5705 MUIC MFD I2C driver\n",
		__func__);

	return ret;
}
/* init early so consumer devices can complete system boot */
subsys_initcall(sm5705_universal_i2c_init);

static void __exit sm5705_universal_i2c_exit(void)
{
	i2c_del_driver(&sm5705_universal_i2c_driver);
}
module_exit(sm5705_universal_i2c_exit);

MODULE_DESCRIPTION("SM5705 multi-function core driver");
MODULE_LICENSE("GPL");
