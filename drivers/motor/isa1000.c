/* drivers/motor/isa1000.c

 * Copyright (C) 2014 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[VIB] " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/isa1000.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#if defined(CONFIG_ISA1000_VDD_REGULATOR)
#include <linux/regulator/consumer.h>
#endif

#include "../staging/android/timed_output.h"

#define MAX_INTENSITY		10000

struct isa1000_ddata {
	struct isa1000_pdata *pdata;
	struct pwm_device *pwm;
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct kthread_worker kworker;
	struct kthread_work kwork;
	spinlock_t lock;
	bool running;
	u32 intensity;
	u32 timeout;
	int duty;
	int gpio_en;
};

static enum hrtimer_restart isa1000_timer_func(struct hrtimer *timer)
{
	struct isa1000_ddata *ddata
		= container_of(timer, struct isa1000_ddata, timer);

	ddata->timeout = 0;

	queue_kthread_work(&ddata->kworker, &ddata->kwork);
	return HRTIMER_NORESTART;
}

static int isa1000_get_time(struct timed_output_dev *dev)
{
	struct isa1000_ddata *ddata
		= container_of(dev, struct  isa1000_ddata, dev);

	struct hrtimer *timer = &ddata->timer;
	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void isa1000_pwm_config(struct isa1000_ddata *ddata, int duty)
{
	int max_duty = ddata->pdata->duty;
	int min_duty = ddata->pdata->period - max_duty;

	/* check the max dury ragne */
	if (duty > max_duty)
		duty = max_duty;
	else if (duty < min_duty)
		duty = min_duty;

	pwm_config(ddata->pwm, duty,
		ddata->pdata->period);
}

static void isa1000_pwm_en(struct isa1000_ddata *ddata, bool en)
{
	if (en)
		pwm_enable(ddata->pwm);
	else
		pwm_disable(ddata->pwm);
}

static void isa1000_en(struct isa1000_ddata *ddata, bool en)
{
#if defined(CONFIG_ISA1000_VDD_REGULATOR)
	int ret = 0;

	if (en)
		ret = regulator_enable(ddata->pdata->regulator);
	else
		ret = regulator_disable(ddata->pdata->regulator);

	if (ret < 0)
		pr_err("failed to regulator %sable", en ? "en" : "dis");
#endif
	pr_info("%s\n", en ? "on" : "off");

	if (ddata->pdata->gpio_en)
		gpio_direction_output(ddata->pdata->gpio_en, en);
}

static void isa1000_enable(struct timed_output_dev *dev, int value)
{
	struct isa1000_ddata *ddata
		= container_of(dev, struct isa1000_ddata, dev);
	struct hrtimer *timer = &ddata->timer;
	unsigned long flags;

	flush_kthread_worker(&ddata->kworker);
	hrtimer_cancel(timer);

	if (value > ddata->pdata->max_timeout)
		value = ddata->pdata->max_timeout;

	ddata->timeout = value;
	if (value > 0 ) {
		if (!ddata->running) {
			pr_info("%u %ums\n", ddata->duty, ddata->timeout);
			ddata->running = true;
			isa1000_pwm_config(ddata, ddata->duty);
			isa1000_pwm_en(ddata, true);
			isa1000_en(ddata, true);
		}
		spin_lock_irqsave(&ddata->lock, flags);
		hrtimer_start(timer, ns_to_ktime((u64)value * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
		spin_unlock_irqrestore(&ddata->lock, flags);
	} else {
		queue_kthread_work(&ddata->kworker, &ddata->kwork);
	}
}

static void isa1000_work_func(struct kthread_work *work)
{
	struct isa1000_ddata *ddata =
		container_of(work, struct isa1000_ddata, kwork);

	if (ddata->running) {
		ddata->running = false;
		isa1000_en(ddata, false);
		isa1000_pwm_config(ddata, ddata->pdata->period >> 1);
		isa1000_pwm_en(ddata, false);
	}

	return;
}

static ssize_t intensity_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct isa1000_ddata *drvdata
		= container_of(tdev, struct isa1000_ddata, dev);
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
		long long tmp = drvdata->pdata->duty >> 1;

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
	struct isa1000_ddata *drvdata
		= container_of(tdev, struct isa1000_ddata, dev);

	return sprintf(buf, "intensity: %u\n", drvdata->intensity);
}

static DEVICE_ATTR(intensity, 0660, intensity_show, intensity_store);

static struct isa1000_pdata *
	isa1000_get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *child_node;
	struct isa1000_pdata *pdata;
#if defined(CONFIG_ISA1000_VDD_REGULATOR)
	const char *regulator;
#endif
	int ret = 0;

	node = dev->of_node;
	if (!node) {
		ret = -ENODEV;
		goto err_out;
	}

	child_node = of_get_next_child(node, child_node);
	if (!child_node) {
		pr_err("failed to get dt node\n");
		ret = -EINVAL;
		goto err_out;
	}

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("failed to alloc\n");
		ret = -ENOMEM;
		goto err_out;
	}

	of_property_read_u32(child_node, "isa1000,max_timeout", &pdata->max_timeout);
	of_property_read_u32(child_node, "isa1000,duty", &pdata->duty);
	of_property_read_u32(child_node, "isa1000,period", &pdata->period);
	of_property_read_u32(child_node, "isa1000,pwm_id", &pdata->pwm_id);
	pdata->gpio_en = of_get_named_gpio(child_node, "isa1000,gpio_en", 0);
	if (pdata->gpio_en)
		gpio_request(pdata->gpio_en, "isa1000,gpio_en");

#if defined(CONFIG_ISA1000_VDD_REGULATOR)
	of_property_read_string(child_node, "isa1000,regulator", &regulator);
	pdata->regulator = regulator_get(NULL, regulator);
	if (IS_ERR(pdata->regulator)) {
		pr_err("failed regulator get %s\n", regulator);
		ret = PTR_ERR(pdata->regulator);
		goto err_out;
	}
#endif

	pr_info("max_timeout = %d\n", pdata->max_timeout);
	pr_info("duty = %d\n", pdata->duty);
	pr_info("period = %d\n", pdata->period);
	pr_info("pwm_id = %d\n", pdata->pwm_id);
	pr_info("gpio_en = %d\n", pdata->gpio_en);

	return pdata;

err_out:
	return ERR_PTR(ret);
}

static int isa1000_probe(struct platform_device *pdev)
{
	struct isa1000_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct isa1000_ddata *ddata;
	struct task_struct *kworker_task;
	int ret = 0;

	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = isa1000_get_devtree_pdata(&pdev->dev);
		if (IS_ERR(pdata)) {
			pr_err("there is no device tree!\n");
			ret = -ENODEV;
			goto err_pdata;
		}
#else
		pr_err("there is no platform data!\n");
#endif
	}

	ddata = kzalloc(sizeof(*ddata), GFP_KERNEL);
	if (!ddata) {
		pr_err("failed to alloc\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	hrtimer_init(&ddata->timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	spin_lock_init(&ddata->lock);

	init_kthread_worker(&ddata->kworker);
	kworker_task = kthread_run(kthread_worker_fn,
			&ddata->kworker, "isa1000_vib");
	if(IS_ERR(kworker_task)) {
		pr_err("Failed to create message pump task\n");
		ret = -ENOMEM;
		goto err_kthread;
	}
	init_kthread_work(&ddata->kwork, isa1000_work_func);

	ddata->pdata = pdata;
	ddata->timer.function = isa1000_timer_func;
	ddata->pwm = pwm_request(ddata->pdata->pwm_id, "vibrator");
	if (IS_ERR(ddata->pwm)) {
		pr_err("failed to request pwm\n");
		ret = -EFAULT;
		goto err_pwm_request;
	}

	ddata->dev.name = "vibrator";
	ddata->dev.get_time = isa1000_get_time;
	ddata->dev.enable = isa1000_enable;

	ret = timed_output_dev_register(&ddata->dev);
	if (ret < 0) {
		pr_err("failed to register timed output\n");
		goto err_dev_reg;
	}

	ret = sysfs_create_file(&ddata->dev.dev->kobj,
				&dev_attr_intensity.attr);
	if (ret < 0) {
		pr_err("Failed to register sysfs : %d\n", ret);
		goto err_dev_reg;
	}

	platform_set_drvdata(pdev, ddata);

	return ret;

err_dev_reg:
	pwm_free(ddata->pwm);
err_pwm_request:
err_kthread:
	kfree(ddata);
err_alloc:
err_pdata:
	return ret;
}

static int __devexit isa1000_remove(struct platform_device *pdev)
{
	struct isa1000_ddata *ddata = platform_get_drvdata(pdev);
	timed_output_dev_unregister(&ddata->dev);
	kfree(ddata);
	return 0;
}

static int isa1000_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	struct isa1000_ddata *ddata = platform_get_drvdata(pdev);

	flush_kthread_worker(&ddata->kworker);
	isa1000_pwm_en(ddata, false);
	isa1000_en(ddata, false);
	return 0;
}

static int isa1000_resume(struct platform_device *pdev)
{
	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id isa1000_dt_ids[] = {
	{ .compatible = "isa1000" },
	{ }
};
MODULE_DEVICE_TABLE(of, isa1000_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver isa1000_driver = {
	.probe		= isa1000_probe,
	.remove		= isa1000_remove,
	.suspend	= isa1000_suspend,
	.resume		= isa1000_resume,
	.driver = {
		.name	= "isa1000",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(isa1000_dt_ids),
	},
};

static int __init isa1000_init(void)
{
	return platform_driver_register(&isa1000_driver);
}
module_init(isa1000_init);

static void __exit isa1000_exit(void)
{
	platform_driver_unregister(&isa1000_driver);
}
module_exit(isa1000_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("isa1000 vibrator driver");
