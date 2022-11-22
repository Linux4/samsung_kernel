/*
 * haptic motor driver for max77888 - max77673_haptic.c
 *
 * Copyright (C) 2011 ByungChang Cha <bc.cha@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timed_output.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max77888.h>
#include <linux/mfd/max77888-private.h>
#include <plat/devs.h>

#define TEST_MODE_TIME 10000
#define MAX_INTENSITY 10000

struct max77888_haptic_data {
	struct max77888_dev *max77888;
	struct i2c_client *i2c;
	struct i2c_client *pmic_i2c;
	struct max77888_haptic_platform_data *pdata;

	struct pwm_device *pwm;
	struct regulator *regulator;
	struct timed_output_dev tout_dev;
	struct hrtimer timer;
	unsigned int timeout;

	struct workqueue_struct *workqueue;
	struct work_struct work;
	spinlock_t lock;
	bool running;

	u32 duty;
	u32 intensity;
};

struct max77888_haptic_data *g_hap_data;
static int prev_duty;

static void max77888_haptic_i2c(struct max77888_haptic_data *hap_data, bool en)
{
	int ret;
	u8 value = hap_data->pdata->reg2;
	u8 lscnfg_val = 0x00;

	pr_info("[VIB] %s %d\n", __func__, en);

	if (en) {
		value |= MOTOR_EN;
		lscnfg_val = 0x80;
	}

	ret = max77888_update_reg(hap_data->pmic_i2c, MAX77888_PMIC_REG_LSCNFG,
				lscnfg_val, 0x80);
	if (ret)
		pr_err("[VIB] i2c update error %d\n", ret);

	ret = max77888_write_reg(hap_data->i2c,
				 MAX77888_HAPTIC_REG_CONFIG2, value);
	if (ret)
		pr_err("[VIB] i2c write error %d\n", ret);
}

static ssize_t intensity_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77888_haptic_data *drvdata
		= container_of(tdev, struct max77888_haptic_data, tout_dev);
	int duty = drvdata->pdata->period >> 1;
	int intensity = 0, ret = 0;

	ret = kstrtoint(buf, 0, &intensity);

	if (intensity < 0 || MAX_INTENSITY < intensity) {
		pr_err("out of rage\n");
		return -EINVAL;
	}

	if (MAX_INTENSITY == intensity)
		duty = drvdata->pdata->duty;
	else if (0 != intensity) {
		long tmp = drvdata->pdata->duty >> 1;

		tmp *= (intensity / 100);
		duty += (int)(tmp / 100);
	}

	drvdata->intensity = intensity;
	drvdata->duty = duty;

	pwm_config(drvdata->pwm, duty, drvdata->pdata->period);

	return count;
}

static ssize_t intensity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77888_haptic_data *drvdata
		= container_of(tdev, struct max77888_haptic_data, tout_dev);

	return sprintf(buf, "intensity: %u\n",
			(drvdata->intensity * 100));
}

static DEVICE_ATTR(intensity, 0660, intensity_show, intensity_store);

static int haptic_get_time(struct timed_output_dev *tout_dev)
{
	struct max77888_haptic_data *hap_data
		= container_of(tout_dev, struct max77888_haptic_data, tout_dev);

	struct hrtimer *timer = &hap_data->timer;
	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	return 0;
}

static void haptic_enable(struct timed_output_dev *tout_dev, int value)
{
	struct max77888_haptic_data *hap_data
		= container_of(tout_dev, struct max77888_haptic_data, tout_dev);

	struct hrtimer *timer = &hap_data->timer;
	unsigned long flags;


	cancel_work_sync(&hap_data->work);
	hrtimer_cancel(timer);
	hap_data->timeout = value;
	queue_work(hap_data->workqueue, &hap_data->work);
	spin_lock_irqsave(&hap_data->lock, flags);
	if (value > 0 && value != TEST_MODE_TIME) {
		pr_info("%s value %d %d\n", __func__, value, hap_data->duty);
		value = min(value, (int)hap_data->pdata->max_timeout);
		hrtimer_start(timer, ns_to_ktime((u64)value * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&hap_data->lock, flags);
}

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct max77888_haptic_data *hap_data
		= container_of(timer, struct max77888_haptic_data, timer);

	hap_data->timeout = 0;
	queue_work(hap_data->workqueue, &hap_data->work);
	return HRTIMER_NORESTART;
}

static void haptic_work(struct work_struct *work)
{
	struct max77888_haptic_data *hap_data
		= container_of(work, struct max77888_haptic_data, work);
	int ret;

	pr_info("[VIB] %s\n", __func__);
	if (hap_data->timeout > 0 && hap_data->intensity) {
		if (hap_data->running)
			return;

		max77888_haptic_i2c(hap_data, true);

		pwm_config(hap_data->pwm, hap_data->duty,
			   hap_data->pdata->period);
		pwm_enable(hap_data->pwm);
		if (hap_data->pdata->motor_en)
			hap_data->pdata->motor_en(true);
		else
			ret = regulator_enable(hap_data->regulator);
		hap_data->running = true;
	} else {
		if (!hap_data->running)
			return;
		if (hap_data->pdata->motor_en)
			hap_data->pdata->motor_en(false);
		else
			regulator_disable(hap_data->regulator);
		pwm_disable(hap_data->pwm);

		max77888_haptic_i2c(hap_data, false);

		hap_data->running = false;
	}
	return;
}

#if defined(CONFIG_OF)
static int of_max77888_haptic_dt(struct max77888_haptic_platform_data *pdata)
{
	struct device_node *np_haptic;
	u32 temp;
	const char *temp_str;

	printk("%s : start dt parsing\n", __func__);

	np_haptic = of_find_node_by_path("/haptic");
	if (np_haptic == NULL) {
		printk("%s : error to get dt node\n", __func__);
		return -EINVAL;
	}

	of_property_read_u32(np_haptic, "haptic,max_timeout", &temp);
	pdata->max_timeout = (u16)temp;

	of_property_read_u32(np_haptic, "haptic,duty", &temp);
	pdata->duty = (u16)temp;

	of_property_read_u32(np_haptic, "haptic,period", &temp);
	pdata->period = (u16)temp;

	of_property_read_u32(np_haptic, "haptic,reg2", &temp);
	pdata->reg2 = (u16)temp;

	of_property_read_u32(np_haptic, "haptic,pwm_id", &temp);
	pdata->pwm_id = (u16)temp;

	of_property_read_string(np_haptic, "haptic,regulator_name", &temp_str);
	pdata->regulator_name = (char *)temp_str;

/* debugging */
	printk("%s : max_timeout = %d\n", __func__, pdata->max_timeout);
	printk("%s : duty = %d\n", __func__, pdata->duty);
	printk("%s : period = %d\n", __func__, pdata->period);
	printk("%s : reg2 = %d\n", __func__, pdata->reg2);
	printk("%s : pwm_id = %d\n", __func__, pdata->pwm_id);
	printk("%s : regulator_name = %s\n", __func__, pdata->regulator_name);

	pdata->init_hw = NULL;
	pdata->motor_en = NULL;

	return 0;
}
#endif /* CONFIG_OF */

static int max77888_haptic_probe(struct platform_device *pdev)
{
	int ret, error = 0;
	struct max77888_dev *max77888 = dev_get_drvdata(pdev->dev.parent);
#if !defined(CONFIG_OF)
	struct max77888_platform_data *max77888_pdata
		= dev_get_platdata(max77888->dev);
#endif
	struct max77888_haptic_data *hap_data;

	pr_info("[VIB] ++ %s\n", __func__);

	hap_data = kzalloc(sizeof(struct max77888_haptic_data), GFP_KERNEL);
	if (!hap_data)
		return -ENOMEM;

#if defined(CONFIG_OF)
	hap_data->pdata = kzalloc(sizeof(struct max77888_haptic_data), GFP_KERNEL);
	if (!hap_data->pdata) {
		kfree(hap_data);
		return -ENOMEM;
	}

	ret = of_max77888_haptic_dt(hap_data->pdata);
	if (ret < 0) {
		pr_err("max77888-haptic : %s not found haptic dt! ret[%d]\n",
				 __func__, ret);
		kfree(hap_data->pdata);
		kfree(hap_data);
		return -1;
	}
#else
	pdata = max77888_pdata->haptic_data;
	if (pdata == NULL) {
		pr_err("%s: no pdata\n", __func__);
		kfree(hap_data);
		return -ENODEV;
	}
#endif /* CONFIG_OF */

	platform_set_drvdata(pdev, hap_data);
	g_hap_data = hap_data;
	hap_data->max77888 = max77888;
	hap_data->i2c = max77888->haptic;
	hap_data->pmic_i2c = max77888->i2c;
	hap_data->intensity = MAX_INTENSITY;
	hap_data->duty = hap_data->pdata->duty;

	hap_data->workqueue = create_singlethread_workqueue("hap_work");
	INIT_WORK(&(hap_data->work), haptic_work);
	spin_lock_init(&(hap_data->lock));

	hap_data->pwm = pwm_request(hap_data->pdata->pwm_id, "vibrator");
	if (IS_ERR(hap_data->pwm)) {
		pr_err("[VIB] Failed to request pwm\n");
		error = -EFAULT;
		goto err_pwm_request;
	}

	pwm_config(hap_data->pwm, hap_data->pdata->period / 2, hap_data->pdata->period);
	prev_duty = hap_data->pdata->period / 2;

	if (hap_data->pdata->init_hw)
		hap_data->pdata->init_hw();
	else
		hap_data->regulator
			= regulator_get(NULL, hap_data->pdata->regulator_name);

	if (IS_ERR(hap_data->regulator)) {
		pr_err("[VIB] Failed to get vmoter regulator.\n");
		error = -EFAULT;
		goto err_regulator_get;
	}
	/* hrtimer init */
	hrtimer_init(&hap_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hap_data->timer.function = haptic_timer_func;

	/* timed_output_dev init*/
	hap_data->tout_dev.name = "vibrator";
	hap_data->tout_dev.get_time = haptic_get_time;
	hap_data->tout_dev.enable = haptic_enable;

#ifdef CONFIG_ANDROID_TIMED_OUTPUT
	error = timed_output_dev_register(&hap_data->tout_dev);
	if (error < 0) {
		pr_err("[VIB] Failed to register timed_output : %d\n", error);
		error = -EFAULT;
		goto err_timed_output_register;
	}

	error = sysfs_create_file(&hap_data->tout_dev.dev->kobj,
				&dev_attr_intensity.attr);
	if (error < 0) {
		pr_err("[VIB] Failed to register sysfs : %d\n", error);
		goto err_timed_output_register;
	}
#endif

	pr_debug("[VIB] -- %s\n", __func__);

	return error;

err_timed_output_register:
	regulator_put(hap_data->regulator);
err_regulator_get:
	pwm_free(hap_data->pwm);
err_pwm_request:
	kfree(hap_data->pdata);
	kfree(hap_data);
	g_hap_data = NULL;
	return error;
}

static int __devexit max77888_haptic_remove(struct platform_device *pdev)
{
	struct max77888_haptic_data *data = platform_get_drvdata(pdev);
#ifdef CONFIG_ANDROID_TIMED_OUTPUT
	timed_output_dev_unregister(&data->tout_dev);
#endif

	regulator_put(data->regulator);
	pwm_free(data->pwm);
	destroy_workqueue(data->workqueue);
	kfree(data->pdata);
	kfree(data);
	g_hap_data = NULL;

	return 0;
}

static int max77888_haptic_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	struct max77888_haptic_data *data = platform_get_drvdata(pdev);

	cancel_work_sync(&data->work);
	hrtimer_cancel(&data->timer);
	if (data->running) {
		if (data->pdata->motor_en)
			data->pdata->motor_en(false);
		else
			regulator_disable(data->regulator);
		pwm_disable(data->pwm);
		max77888_haptic_i2c(data, false);
		data->running = false;
	}

	return 0;
}
static int max77888_haptic_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver max77888_haptic_driver = {
	.probe		= max77888_haptic_probe,
	.remove		= max77888_haptic_remove,
	.suspend	= max77888_haptic_suspend,
	.resume		= max77888_haptic_resume,
	.driver = {
		.name	= "max77888-haptic",
		.owner	= THIS_MODULE,
	},
};

static int __init max77888_haptic_init(void)
{
	pr_debug("[VIB] %s\n", __func__);
	return platform_driver_register(&max77888_haptic_driver);
}
module_init(max77888_haptic_init);

static void __exit max77888_haptic_exit(void)
{
	platform_driver_unregister(&max77888_haptic_driver);
}
module_exit(max77888_haptic_exit);

MODULE_AUTHOR("ByungChang Cha <bc.cha@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAX77888 haptic driver");
