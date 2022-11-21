/* drivers/motor/drv2603.c

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
#include <linux/drv2603.h>
#include <linux/wakelock.h>

#include "../staging/android/timed_output.h"
#if defined(CONFIG_IMM_VIB)
#include "imm_vib.h"
#endif

struct drv2603_ddata {
	struct drv2603_pdata *pdata;
	struct pwm_device *pwm;
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct work_struct work;
	unsigned int timeout;
	spinlock_t lock;
	bool running;
	int gpio_en;
};

static enum hrtimer_restart drv2603_timer_func(struct hrtimer *timer)
{
	struct drv2603_ddata *ddata
		= container_of(timer, struct drv2603_ddata, timer);

	ddata->timeout = 0;

	schedule_work(&ddata->work);
	return HRTIMER_NORESTART;
}

static int drv2603_get_time(struct timed_output_dev *dev)
{
	struct drv2603_ddata *ddata
		= container_of(dev, struct  drv2603_ddata, dev);

	struct hrtimer *timer = &ddata->timer;
	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void drv2603_enable(struct timed_output_dev *dev, int value)
{
	struct drv2603_ddata *ddata
		= container_of(dev, struct drv2603_ddata, dev);
	struct hrtimer *timer = &ddata->timer;
	unsigned long flags;

	cancel_work_sync(&ddata->work);
	hrtimer_cancel(timer);
	schedule_work(&ddata->work);

	if (value > ddata->pdata->max_timeout)
		value = ddata->pdata->max_timeout;

	spin_lock_irqsave(&ddata->lock, flags);
	ddata->timeout = value;
	if (value > 0 ) {
		hrtimer_start(timer, ns_to_ktime((u64)value * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&ddata->lock, flags);
}

static void drv2603_pwm_config(struct drv2603_ddata *ddata, int duty)
{
	pwm_config(ddata->pwm, duty,
		ddata->pdata->period);
}

static void drv2603_pwm_en(struct drv2603_ddata *ddata, bool en)
{
	if (en)
		pwm_enable(ddata->pwm);
	else
		pwm_disable(ddata->pwm);
}

static void drv2603_en(struct drv2603_ddata *ddata, bool en)
{
	gpio_direction_output(ddata->pdata->gpio_en, en);

	if (en)
		msleep(20);
}

static void drv2603_work_func(struct work_struct *work)
{
	struct drv2603_ddata *ddata =
		container_of(work, struct drv2603_ddata, work);

	printk(KERN_DEBUG "[VIB] %ums\n", ddata->timeout);

	if (ddata->timeout) {
		if (ddata->running)
			return;
		ddata->running = true;
		drv2603_en(ddata, true);
		drv2603_pwm_config(ddata, ddata->pdata->duty);
		drv2603_pwm_en(ddata, true);
	} else {
		if (!ddata->running)
			return;
		ddata->running = false;
		drv2603_pwm_config(ddata, ddata->pdata->period >> 1);
		drv2603_pwm_en(ddata, false);
		drv2603_en(ddata, false);
	}
	return;
}

#if defined(CONFIG_IMM_VIB)
static void drv2603_set_force(void *_data, u8 index, int nforce)
{
	struct drv2603_ddata *ddata = _data;
	int duty = ddata->pdata->period * nforce >> 7;

	drv2603_pwm_config(ddata, duty);
}

static void drv2603_chip_enable(void *_data, bool en)
{
	struct drv2603_ddata *ddata = _data;

	drv2603_en(ddata, en);
	drv2603_pwm_en(ddata, en);
}
#endif	/* CONFIG_IMM_VIB */

static struct drv2603_pdata *
	drv2603_get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *child_node;
	struct drv2603_pdata *pdata;
	int ret = 0;

	node = dev->of_node;
	if (!node) {
		ret = -ENODEV;
		goto err_out;
	}

	child_node = of_get_next_child(node, child_node);
	if (!child_node) {
		printk("[VIB] failed to get dt node\n");
		ret = -EINVAL;
		goto err_out;
	}

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		printk("[VIB] failed to alloc\n");
		ret = -ENOMEM;
		goto err_out;
	}

	of_property_read_u32(child_node, "drv2603,max_timeout", &pdata->max_timeout);
	of_property_read_u32(child_node, "drv2603,duty", &pdata->duty);
	of_property_read_u32(child_node, "drv2603,period", &pdata->period);
	of_property_read_u32(child_node, "drv2603,pwm_id", &pdata->pwm_id);
	pdata->gpio_en = of_get_named_gpio(child_node, "drv2603,gpio_en", 0);

	printk(KERN_DEBUG "[VIB] max_timeout = %d\n", pdata->max_timeout);
	printk(KERN_DEBUG "[VIB] duty = %d\n", pdata->duty);
	printk(KERN_DEBUG "[VIB] period = %d\n", pdata->period);
	printk(KERN_DEBUG "[VIB] pwm_id = %d\n", pdata->pwm_id);
	printk(KERN_DEBUG "[VIB] gpio_en = %d\n", pdata->gpio_en);

	return pdata;

err_out:
	return ERR_PTR(ret);
}

static int drv2603_probe(struct platform_device *pdev)
{
	struct drv2603_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct drv2603_ddata *ddata;
#if defined(CONFIG_IMM_VIB)
	struct imm_vib_dev *vib_dev;
#endif
	int ret = 0;

	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = drv2603_get_devtree_pdata(&pdev->dev);
		if (IS_ERR(pdata)) {
			printk(KERN_ERR "[VIB] there is no device tree!\n");
			ret = -ENODEV;
			goto err_pdata;
		}
#else
		printk(KERN_ERR "[VIB] there is no platform data!\n");
#endif
	}

	ddata = kzalloc(sizeof(*ddata), GFP_KERNEL);
	if (!ddata) {
		printk(KERN_ERR "[VIB] failed to alloc\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	hrtimer_init(&ddata->timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	spin_lock_init(&ddata->lock);
	INIT_WORK(&ddata->work, drv2603_work_func);

	ddata->pdata = pdata;
	ddata->timer.function = drv2603_timer_func;
	ddata->pwm = pwm_request(ddata->pdata->pwm_id, "vibrator");
	if (IS_ERR(ddata->pwm)) {
		printk(KERN_ERR "[VIB] failed to request pwm\n");
		ret = -EFAULT;
		goto err_pwm_request;
	}

	ddata->dev.name = "vibrator";
	ddata->dev.get_time = drv2603_get_time;
	ddata->dev.enable = drv2603_enable;

	ret = timed_output_dev_register(&ddata->dev);
	if (ret < 0) {
		printk(KERN_ERR "[VIB] failed to register timed output\n");
		goto err_dev_reg;
	}

#if defined(CONFIG_IMM_VIB)
	vib_dev = kzalloc(sizeof(struct imm_vib_dev), GFP_KERNEL);
	if (vib_dev) {
		vib_dev->private_data = ddata;
		vib_dev->set_force = drv2603_set_force;
		vib_dev->chip_en = drv2603_chip_enable;
		vib_dev->num_actuators = 1;
		imm_vib_register(vib_dev);
	} else
		printk(KERN_ERR "[VIB] Failed to alloc vibe memory.\n");
#endif

	platform_set_drvdata(pdev, ddata);

	return ret;

err_dev_reg:
	pwm_free(ddata->pwm);
err_pwm_request:
	kfree(ddata);
err_alloc:
err_pdata:
	return ret;
}

static int __devexit drv2603_remove(struct platform_device *pdev)
{
	struct drv2603_ddata *ddata = platform_get_drvdata(pdev);
	timed_output_dev_unregister(&ddata->dev);
	kfree(ddata);
	return 0;
}

static int drv2603_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	struct drv2603_ddata *ddata = platform_get_drvdata(pdev);

	drv2603_pwm_en(ddata, false);
	drv2603_en(ddata, false);
	return 0;
}

static int drv2603_resume(struct platform_device *pdev)
{
	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id drv2603_dt_ids[] = {
	{ .compatible = "drv2603" },
	{ }
};
MODULE_DEVICE_TABLE(of, drv2603_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver drv2603_driver = {
	.probe		= drv2603_probe,
	.remove		= drv2603_remove,
	.suspend	= drv2603_suspend,
	.resume		= drv2603_resume,
	.driver = {
		.name	= "drv2603",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(drv2603_dt_ids),
	},
};

static int __init drv2603_init(void)
{
	return platform_driver_register(&drv2603_driver);
}
module_init(drv2603_init);

static void __exit drv2603_exit(void)
{
	platform_driver_unregister(&drv2603_driver);
}
module_exit(drv2603_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("drv2603 vibrator driver");
