/*
 * haptic motor driver for max77828_haptic.c
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
#include <linux/mfd/max77828.h>
#include <linux/mfd/max77828-private.h>

#define MOTOR_EN			(1<<6)
#define MAX77828_REG_MAINCTRL1_BIASEN	(1<<7)

struct max77828_haptic_data {
	struct max77828_dev *max77828;
	struct i2c_client *i2c;
	struct max77828_haptic_platform_data *pdata;
	u8 reg;
	spinlock_t lock;
	bool running;
};

struct max77828_haptic_data *g_hap_data;

static void max77828_haptic_i2c(struct max77828_haptic_data *hap_data, bool en)
{
	int ret;
	u8 lscnfg_val = 0x00;

	if (en)
		lscnfg_val = MAX77828_REG_MAINCTRL1_BIASEN;

	ret = max77828_update_reg(hap_data->i2c, MAX77828_PMIC_REG_MAINCTRL1,
				lscnfg_val, MAX77828_REG_MAINCTRL1_BIASEN);
	if (ret)
		pr_err("[VIB] i2c REG_BIASEN update error %d\n", ret);

	if (en)
		ret = max77828_update_reg(hap_data->i2c, MAX77828_PMIC_REG_MCONFIG, 0xff, MOTOR_EN);
	else
		ret = max77828_update_reg(hap_data->i2c, MAX77828_PMIC_REG_MCONFIG, 0xff, (0<<6));

	if (ret)
		pr_err("[VIB] i2c MOTOR_EN update error %d\n", ret);

	ret = max77828_update_reg(hap_data->i2c, MAX77828_PMIC_REG_MCONFIG, 0xff, MOTOR_LRA);
	if (ret)
		pr_err("[VIB] i2c MOTOR_LPA update error %d\n", ret);

}

#ifdef CONFIG_SS_VIBRATOR
void max77828_vibrator_en(bool en)
{
	if (g_hap_data == NULL) {
		return ;
	}

	if (en) {
		if (g_hap_data->running)
			return;

		max77828_haptic_i2c(g_hap_data, true);

		g_hap_data->running = true;
	} else {
		if (!g_hap_data->running)
			return;

		max77828_haptic_i2c(g_hap_data, false);

		g_hap_data->running = false;
	}
}
EXPORT_SYMBOL(max77828_vibrator_en);
#endif

static int max77828_haptic_probe(struct platform_device *pdev)
{
	int error = 0;
	struct max77828_dev *max77828 = dev_get_drvdata(pdev->dev.parent);
	struct max77828_platform_data *max77828_pdata
		= dev_get_platdata(max77828->dev);

#ifdef CONFIG_SS_VIBRATOR
	struct max77828_haptic_platform_data *pdata
		= max77828_pdata->haptic_data;
#endif
	struct max77828_haptic_data *hap_data;

	pr_err("[VIB] ++ %s\n", __func__);
	 if (pdata == NULL) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}

	hap_data = kzalloc(sizeof(struct max77828_haptic_data), GFP_KERNEL);
	if (!hap_data)
		return -ENOMEM;

	g_hap_data = hap_data;
	g_hap_data->reg = MOTOR_LRA | DIVIDER_128;
	hap_data->max77828 = max77828;
	hap_data->i2c = max77828->topsys;
	hap_data->pdata = pdata;
	platform_set_drvdata(pdev, hap_data);
	max77828_haptic_i2c(hap_data, true);

	spin_lock_init(&(hap_data->lock));

	pr_err("[VIB] -- %s\n", __func__);

	return error;
}

static int max77828_haptic_remove(struct platform_device *pdev)
{
	struct max77828_haptic_data *data = platform_get_drvdata(pdev);
	int ret;
	
	pr_info("%s: Disable HAPTIC\n", __func__);
	ret = max77828_update_reg(data->i2c, MAX77828_PMIC_REG_MCONFIG, 0x0, (1<<6));
	if (ret < 0) {
		pr_err("%s: fail to update reg\n", __func__);
		return -1;
	}
	
	kfree(data);
	g_hap_data = NULL;

	return 0;
}

void max77828_haptic_shutdown(struct device *dev)
{
	struct max77828_haptic_data *data = dev_get_drvdata(dev);
	int ret;
	
	pr_info("%s: Disable HAPTIC\n", __func__);
	ret = max77828_update_reg(data->i2c, MAX77828_PMIC_REG_MCONFIG, 0x0, (1<<6));
	if (ret < 0) {
		pr_err("%s: fail to update reg\n", __func__);
		return;
	}
}

static int max77828_haptic_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	struct max77828_haptic_data *data = platform_get_drvdata(pdev);
	int ret;
	
	pr_info("%s: Disable HAPTIC\n", __func__);
	ret = max77828_update_reg(data->i2c, MAX77828_PMIC_REG_MCONFIG, 0x0, (1<<6));
	if (ret < 0) {
		pr_err("%s: fail to update reg\n", __func__);
		return -1;
	}
	return 0;
}
static int max77828_haptic_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver max77828_haptic_driver = {
	.probe		= max77828_haptic_probe,
	.remove		= max77828_haptic_remove,
	.suspend	= max77828_haptic_suspend,
	.resume		= max77828_haptic_resume,
	.driver = {
		.name	= "max77828-haptic",
		.owner	= THIS_MODULE,
		.shutdown = max77828_haptic_shutdown,
	},
};

static int __init max77828_haptic_init(void)
{
	pr_debug("[VIB] %s\n", __func__);
	return platform_driver_register(&max77828_haptic_driver);
}
module_init(max77828_haptic_init);

static void __exit max77828_haptic_exit(void)
{
	platform_driver_unregister(&max77828_haptic_driver);
}
module_exit(max77828_haptic_exit);

MODULE_AUTHOR("Ravi Shekhar Singh <shekhar.sr@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("max77828 haptic driver");
