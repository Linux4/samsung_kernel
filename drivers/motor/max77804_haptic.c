/*
 * haptic motor driver for max77804 - max77673_haptic.c
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
#include <linux/mfd/max77804.h>
#include <linux/mfd/max77804-private.h>
#include <plat/devs.h>
#include <linux/kthread.h>

#define TEST_MODE_TIME 10000
#define MAX_INTENSITY 10000

struct max77804_haptic_data {
	struct max77804_dev *max77804;
	struct i2c_client *i2c;
	struct i2c_client *pmic_i2c;
	struct max77804_haptic_platform_data *pdata;

	struct pwm_device *pwm;
	struct regulator *regulator;
	struct timed_output_dev tout_dev;
	struct hrtimer timer;
	unsigned int timeout;

	struct kthread_worker kworker;
	struct kthread_work kwork;
	spinlock_t lock;
	bool running;

	u32 duty;
	u32 intensity;
};

struct max77804_haptic_data *g_hap_data;
static int prev_duty;

static int motor_vdd_en(struct max77804_haptic_data *hap_data, bool en)
{
	int ret = 0;

	if (en)
		ret = regulator_enable(hap_data->regulator);
	else
		ret = regulator_disable(hap_data->regulator);

	if (ret < 0)
		pr_err("failed to %sable regulator %d\n",
			en ? "en" : "dis", ret);

	return ret;
}

static void max77804_haptic_i2c(struct max77804_haptic_data *hap_data, bool en)
{
	int ret;
	u8 value = hap_data->pdata->reg2;
	u8 lscnfg_val = 0x00;

	pr_info("[VIB] %s %d\n", __func__, en);

	if (en) {
		value |= MOTOR_EN;
		lscnfg_val = 0x80;
	}

	ret = max77804_update_reg(hap_data->pmic_i2c, MAX77804_PMIC_REG_LSCNFG,
				lscnfg_val, 0x80);
	if (ret)
		pr_err("[VIB] i2c update error %d\n", ret);

	ret = max77804_write_reg(hap_data->i2c,
				 MAX77804_HAPTIC_REG_CONFIG2, value);
	if (ret)
		pr_err("[VIB] i2c write error %d\n", ret);
}

static ssize_t intensity_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77804_haptic_data *drvdata
		= container_of(tdev, struct max77804_haptic_data, tout_dev);
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
	struct max77804_haptic_data *drvdata
		= container_of(tdev, struct max77804_haptic_data, tout_dev);

	return sprintf(buf, "intensity: %u\n",
			(drvdata->intensity * 100));
}

static DEVICE_ATTR(intensity, 0660, intensity_show, intensity_store);

static int haptic_get_time(struct timed_output_dev *tout_dev)
{
	struct max77804_haptic_data *hap_data
		= container_of(tout_dev, struct max77804_haptic_data, tout_dev);

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
	struct max77804_haptic_data *hap_data
		= container_of(tout_dev, struct max77804_haptic_data, tout_dev);

	struct hrtimer *timer = &hap_data->timer;
	unsigned long flags;

	flush_kthread_worker(&hap_data->kworker);
	hrtimer_cancel(timer);
	hap_data->timeout = value;

	if (value > 0) {
		if (!hap_data->running) {
			pwm_config(hap_data->pwm, hap_data->duty,
				hap_data->pdata->period);
			pwm_enable(hap_data->pwm);

			max77804_haptic_i2c(hap_data, true);
			hap_data->running = true;
		}
		spin_lock_irqsave(&hap_data->lock, flags);
		pr_info("%s value %d %d\n", __func__, value, hap_data->duty);
		value = min(value, (int)hap_data->pdata->max_timeout);
		hrtimer_start(timer, ns_to_ktime((u64)value * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
		spin_unlock_irqrestore(&hap_data->lock, flags);
	}
	else
		queue_kthread_work(&hap_data->kworker, &hap_data->kwork);
}

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct max77804_haptic_data *hap_data
		= container_of(timer, struct max77804_haptic_data, timer);

	hap_data->timeout = 0;
	queue_kthread_work(&hap_data->kworker, &hap_data->kwork);
	return HRTIMER_NORESTART;
}

static void haptic_work(struct kthread_work *work)
{
	struct max77804_haptic_data *hap_data
		= container_of(work, struct max77804_haptic_data, kwork);

	pr_info("[VIB] %s\n", __func__);

	if (hap_data->running) {
		max77804_haptic_i2c(hap_data, false);
		pwm_disable(hap_data->pwm);
		hap_data->running = false;
	}
}

#if defined(CONFIG_OF)
static int of_max77804_haptic_dt(struct max77804_haptic_platform_data *pdata)
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

static int max77804_haptic_probe(struct platform_device *pdev)
{
	int ret, error = 0;
	struct max77804_dev *max77804 = dev_get_drvdata(pdev->dev.parent);
#if !defined(CONFIG_OF)
	struct max77804_platform_data *max77804_pdata
		= dev_get_platdata(max77804->dev);
#endif
	struct max77804_haptic_data *hap_data;
	struct task_struct *kworker_task;

	pr_info("[VIB] ++ %s\n", __func__);

	hap_data = kzalloc(sizeof(struct max77804_haptic_data), GFP_KERNEL);
	if (!hap_data)
		return -ENOMEM;

#if defined(CONFIG_OF)
	hap_data->pdata = kzalloc(sizeof(struct max77804_haptic_data), GFP_KERNEL);
	if (!hap_data->pdata) {
		kfree(hap_data);
		return -ENOMEM;
	}

	ret = of_max77804_haptic_dt(hap_data->pdata);
	if (ret < 0) {
		pr_err("max77804-haptic : %s not found haptic dt! ret[%d]\n",
				 __func__, ret);
		kfree(hap_data->pdata);
		kfree(hap_data);
		return -1;
	}
#else
	pdata = max77804_pdata->haptic_data;
	if (pdata == NULL) {
		pr_err("%s: no pdata\n", __func__);
		kfree(hap_data);
		return -ENODEV;
	}
#endif /* CONFIG_OF */

	platform_set_drvdata(pdev, hap_data);
	g_hap_data = hap_data;
	hap_data->max77804 = max77804;
	hap_data->i2c = max77804->haptic;
	hap_data->pmic_i2c = max77804->i2c;
	hap_data->intensity = MAX_INTENSITY;
	hap_data->duty = hap_data->pdata->duty;

	init_kthread_worker(&hap_data->kworker);
	kworker_task = kthread_run(kthread_worker_fn,
		   &hap_data->kworker, "max77804_haptic");
	if (IS_ERR(kworker_task)) {
		pr_err("Failed to create message pump task\n");
		error = -ENOMEM;
		goto err_kthread;
	}
	init_kthread_work(&hap_data->kwork, haptic_work);

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

	motor_vdd_en(g_hap_data, true);

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

	ret = sysfs_create_file(&hap_data->tout_dev.dev->kobj,
				&dev_attr_intensity.attr);
	if (ret < 0) {
		pr_err("[VIB] Failed to register sysfs : %d\n", ret);
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
err_kthread:
	kfree(hap_data->pdata);
	kfree(hap_data);
	g_hap_data = NULL;
	return error;
}

static int __devexit max77804_haptic_remove(struct platform_device *pdev)
{
	struct max77804_haptic_data *data = platform_get_drvdata(pdev);
#ifdef CONFIG_ANDROID_TIMED_OUTPUT
	timed_output_dev_unregister(&data->tout_dev);
#endif

	regulator_put(data->regulator);
	pwm_free(data->pwm);
	kfree(data->pdata);
	kfree(data);
	g_hap_data = NULL;

	return 0;
}

static int max77804_haptic_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	pr_info("[VIB] %s\n", __func__);
	if (g_hap_data != NULL) {
		flush_kthread_worker(&g_hap_data->kworker);
		hrtimer_cancel(&g_hap_data->timer);
		max77804_haptic_i2c(g_hap_data, false);
		motor_vdd_en(g_hap_data, false);
	}
	return 0;
}
static int max77804_haptic_resume(struct platform_device *pdev)
{
	pr_info("[VIB] %s\n", __func__);
	motor_vdd_en(g_hap_data, true);
	return 0;
}

static struct platform_driver max77804_haptic_driver = {
	.probe		= max77804_haptic_probe,
	.remove		= max77804_haptic_remove,
	.suspend	= max77804_haptic_suspend,
	.resume		= max77804_haptic_resume,
	.driver = {
		.name	= "max77804-haptic",
		.owner	= THIS_MODULE,
	},
};

static int __init max77804_haptic_init(void)
{
	pr_debug("[VIB] %s\n", __func__);
	return platform_driver_register(&max77804_haptic_driver);
}
module_init(max77804_haptic_init);

static void __exit max77804_haptic_exit(void)
{
	platform_driver_unregister(&max77804_haptic_driver);
}
module_exit(max77804_haptic_exit);

MODULE_AUTHOR("ByungChang Cha <bc.cha@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MAX77804 haptic driver");
