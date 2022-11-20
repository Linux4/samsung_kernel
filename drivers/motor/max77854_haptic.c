/*
 * haptic motor driver for max77854_haptic.c
 *
 * Copyright (C) 2013 Ravi Shekhar Singh <shekhar.sr@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max77854.h>
#include <linux/mfd/max77854-private.h>

#define MOTOR_LRA			(1<<7)
#define MOTOR_EN			(1<<6)
#define EXT_PWM				(0<<5)
#define DIVIDER_128			(1<<1)
#define MAX77854_REG_MAINCTRL1_MRDBTMER_MASK	(0x7)
#define MAX77854_REG_MAINCTRL1_MREN		(1 << 3)
#define MAX77854_REG_MAINCTRL1_BIASEN		(1 << 7)

struct max77854_haptic_data {
	struct max77854_dev *max77854;
	struct i2c_client *i2c;
	struct max77854_haptic_platform_data *pdata;

	bool running;
};

struct max77854_haptic_data *max77854_g_hap_data;

static void max77854_haptic_i2c(struct max77854_haptic_data *hap_data, bool en)
{
	int ret;
	u8 lscnfg_val = 0x00;

	if (en)
		lscnfg_val = MAX77854_REG_MAINCTRL1_BIASEN;

	ret = max77854_update_reg(hap_data->i2c, MAX77854_PMIC_REG_MAINCTRL1,
				lscnfg_val, MAX77854_REG_MAINCTRL1_BIASEN);
	if (ret)
		pr_err("[VIB] i2c REG_BIASEN update error %d\n", ret);

	if (en)
		ret = max77854_update_reg(hap_data->i2c, MAX77854_PMIC_REG_MCONFIG, 0xff, MOTOR_EN);
	else
		ret = max77854_update_reg(hap_data->i2c, MAX77854_PMIC_REG_MCONFIG, 0x0, MOTOR_EN);

	if (ret)
		pr_err("[VIB] i2c MOTOR_EN update error %d\n", ret);

	ret = max77854_update_reg(hap_data->i2c, MAX77854_PMIC_REG_MCONFIG, 0xff, MOTOR_LRA);
	if (ret)
		pr_err("[VIB] i2c MOTOR_LPA update error %d\n", ret);
}

#if defined(CONFIG_SS_VIBRATOR)
void max77854_vibtonz_en(bool en)
{
	if (max77854_g_hap_data == NULL) {
		return ;
	}

	if (en) {
		if (max77854_g_hap_data->running)
			return;
		max77854_haptic_i2c(max77854_g_hap_data, true);

		max77854_g_hap_data->running = true;
	} else {
		if (!max77854_g_hap_data->running)
			return;
		max77854_haptic_i2c(max77854_g_hap_data, false);

		max77854_g_hap_data->running = false;
	}
}
EXPORT_SYMBOL(max77854_vibtonz_en);
#endif

#if defined(CONFIG_OF)
static struct max77854_haptic_platform_data *of_max77854_haptic_dt(struct device *dev)
{
	struct device_node *np = dev->parent->of_node;
	struct max77854_haptic_platform_data *pdata;

	pdata = kzalloc(sizeof(struct max77854_haptic_platform_data), GFP_KERNEL);
	if (pdata == NULL)
	{
		pr_err("[VIB] %s: failed to allocate driver data\n", __func__);
		return NULL;
	}

	if (!of_property_read_u32(np, "haptic,mode", &pdata->mode))
		pdata->mode = 1;
	if (!of_property_read_u32(np, "haptic,divisor", &pdata->divisor))
		pdata->divisor = 128;
	pr_info("[VIB] %s: mode: %d \n", __func__, pdata->mode);
	pr_info("[VIB] %s: divisor: %d \n", __func__, pdata->divisor);

	return pdata;
}
#endif

static int max77854_haptic_probe(struct platform_device *pdev)
{
	int error = 0;
	struct max77854_dev *max77854 = dev_get_drvdata(pdev->dev.parent);
	struct max77854_platform_data *max77854_pdata
		= dev_get_platdata(max77854->dev);
	struct max77854_haptic_platform_data *pdata
		= max77854_pdata->haptic_data;
	struct max77854_haptic_data *hap_data;
#if defined(CONFIG_OF)
	if (pdata == NULL)
		pdata = of_max77854_haptic_dt(&pdev->dev);
#endif

	pr_err("[VIB] ++ %s\n", __func__);
	 if (pdata == NULL) {
		pr_err("[VIB] %s: no pdata\n", __func__);
		return -ENODEV;
	}

	hap_data = kzalloc(sizeof(struct max77854_haptic_data), GFP_KERNEL);
	if (!hap_data)
		return -ENOMEM;

	max77854_g_hap_data = hap_data;
	hap_data->max77854 = max77854;
	hap_data->i2c = max77854->i2c;
	hap_data->pdata = pdata;
	platform_set_drvdata(pdev, hap_data);
	max77854_haptic_i2c(hap_data, true);

	pr_err("[VIB] -- %s\n", __func__);

	return error;
}

static int max77854_haptic_remove(struct platform_device *pdev)
{
	struct max77854_haptic_data *data = platform_get_drvdata(pdev);
	int ret;

	pr_info("[VIB] %s: Disable HAPTIC\n", __func__);
	ret = max77854_update_reg(data->i2c, MAX77854_PMIC_REG_MCONFIG, 0x0, MOTOR_EN);
	if (ret < 0) {
		pr_err("[VIB] %s: fail to update reg\n", __func__);
		return -1;
	}

	kfree(data);
	max77854_g_hap_data = NULL;

	return 0;
}

void max77854_haptic_shutdown(struct device *dev)
{
	struct max77854_haptic_data *data = dev_get_drvdata(dev);
	int ret;

	pr_info("[VIB] %s: Disable HAPTIC\n", __func__);
	ret = max77854_update_reg(data->i2c, MAX77854_PMIC_REG_MCONFIG, 0x0, MOTOR_EN);
	if (ret < 0) {
		pr_err("[VIB] %s: fail to update reg\n", __func__);
		return;
	}
}

static int max77854_haptic_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	return 0;
}
static int max77854_haptic_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver max77854_haptic_driver = {
	.probe		= max77854_haptic_probe,
	.remove		= max77854_haptic_remove,
	.suspend	= max77854_haptic_suspend,
	.resume		= max77854_haptic_resume,
	.driver = {
		.name	= "max77854-haptic",
		.owner	= THIS_MODULE,
		.shutdown = max77854_haptic_shutdown,
	},
};

static int __init max77854_haptic_init(void)
{
	pr_debug("[VIB] %s\n", __func__);
	return platform_driver_register(&max77854_haptic_driver);
}
module_init(max77854_haptic_init);

static void __exit max77854_haptic_exit(void)
{
	platform_driver_unregister(&max77854_haptic_driver);
}
module_exit(max77854_haptic_exit);

MODULE_AUTHOR("Ravi Shekhar Singh <shekhar.sr@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("max77854 haptic driver");
