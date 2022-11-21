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

#include "../staging/android/timed_output.h"
#if defined(CONFIG_IMM_VIB)
#include "imm_vib.h"
#endif
#if defined(CONFIG_VIBETONZ)
void (*vibtonz_en)(bool);
void (*vibtonz_pwm)(int);
static struct isa1000_ddata *g_ddata;
#endif

struct isa1000_ddata {
	struct isa1000_pdata *pdata;
	struct pwm_device *pwm;
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct work_struct work;
	unsigned int timeout;
	spinlock_t lock;
	bool running;
	int gpio_en;
};

static enum hrtimer_restart isa1000_timer_func(struct hrtimer *timer)
{
	struct isa1000_ddata *ddata
		= container_of(timer, struct isa1000_ddata, timer);

	ddata->timeout = 0;

	schedule_work(&ddata->work);
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

static void isa1000_enable(struct timed_output_dev *dev, int value)
{
	struct isa1000_ddata *ddata
		= container_of(dev, struct isa1000_ddata, dev);
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
	printk(KERN_DEBUG "[VIB] %s\n", en ? "on" : "off");

	gpio_direction_output(ddata->pdata->gpio_en, en);

	if (en)
		msleep(20);
}

#if defined(CONFIG_VIBETONZ)
void isa1000_vibtonz_en(bool en)
{
	struct isa1000_ddata *ddata = g_ddata;

	isa1000_en(ddata, en);
	isa1000_pwm_en(ddata, en);
}
void isa1000_vibtonz_pwm(int nforce)
{
	struct isa1000_ddata *ddata = g_ddata;
	int duty = ddata->pdata->period >> 1;
	int tmp = (ddata->pdata->period >> 8);
	tmp *= nforce;
	duty += tmp;

	isa1000_pwm_config(ddata, duty);
}
#endif

static void isa1000_work_func(struct work_struct *work)
{
	struct isa1000_ddata *ddata =
		container_of(work, struct isa1000_ddata, work);

	if (ddata->timeout) {
		if (ddata->running)
			return;
		ddata->running = true;
		isa1000_en(ddata, true);
		isa1000_pwm_config(ddata, ddata->pdata->duty);
		isa1000_pwm_en(ddata, true);
	} else {
		if (!ddata->running)
			return;
		ddata->running = false;
		isa1000_pwm_config(ddata, ddata->pdata->period >> 1);
		isa1000_pwm_en(ddata, false);
		isa1000_en(ddata, false);
	}
	return;
}

#if defined(CONFIG_IMM_VIB)
static void isa1000_set_force(void *_data, u8 index, int nforce)
{
	struct isa1000_ddata *ddata = _data;
	int duty = ddata->pdata->period >> 1;
	int tmp = (ddata->pdata->period >> 8);
	tmp *= nforce;
	duty += tmp;

	isa1000_pwm_config(ddata, duty);
}

static void isa1000_chip_enable(void *_data, bool en)
{
	struct isa1000_ddata *ddata = _data;

	isa1000_en(ddata, en);
	isa1000_pwm_en(ddata, en);
}
#endif	/* CONFIG_IMM_VIB */

static struct isa1000_pdata *
	isa1000_get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *child_node;
	struct isa1000_pdata *pdata;
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

	of_property_read_u32(child_node, "isa1000,max_timeout", &pdata->max_timeout);
	of_property_read_u32(child_node, "isa1000,duty", &pdata->duty);
	of_property_read_u32(child_node, "isa1000,period", &pdata->period);
	of_property_read_u32(child_node, "isa1000,pwm_id", &pdata->pwm_id);
	pdata->gpio_en = of_get_named_gpio(child_node, "isa1000,gpio_en", 0);

	if (pdata->gpio_en)
		gpio_request(pdata->gpio_en, "isa1000,gpio_en");

	printk(KERN_DEBUG "[VIB] max_timeout = %d\n", pdata->max_timeout);
	printk(KERN_DEBUG "[VIB] duty = %d\n", pdata->duty);
	printk(KERN_DEBUG "[VIB] period = %d\n", pdata->period);
	printk(KERN_DEBUG "[VIB] pwm_id = %d\n", pdata->pwm_id);
	printk(KERN_DEBUG "[VIB] gpio_en = %d\n", pdata->gpio_en);

	return pdata;

err_out:
	return ERR_PTR(ret);
}

static int isa1000_probe(struct platform_device *pdev)
{
	struct isa1000_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct isa1000_ddata *ddata;
#if defined(CONFIG_IMM_VIB)
	struct imm_vib_dev *vib_dev;
#endif
	int ret = 0;

	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = isa1000_get_devtree_pdata(&pdev->dev);
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
	INIT_WORK(&ddata->work, isa1000_work_func);

	ddata->pdata = pdata;
	ddata->timer.function = isa1000_timer_func;
	ddata->pwm = pwm_request(ddata->pdata->pwm_id, "vibrator");
	if (IS_ERR(ddata->pwm)) {
		printk(KERN_ERR "[VIB] failed to request pwm\n");
		ret = -EFAULT;
		goto err_pwm_request;
	}

	ddata->dev.name = "vibrator";
	ddata->dev.get_time = isa1000_get_time;
	ddata->dev.enable = isa1000_enable;

	ret = timed_output_dev_register(&ddata->dev);
	if (ret < 0) {
		printk(KERN_ERR "[VIB] failed to register timed output\n");
		goto err_dev_reg;
	}

#if defined(CONFIG_IMM_VIB)
	vib_dev = kzalloc(sizeof(struct imm_vib_dev), GFP_KERNEL);
	if (vib_dev) {
		vib_dev->private_data = ddata;
		vib_dev->set_force = isa1000_set_force;
		vib_dev->chip_en = isa1000_chip_enable;
		vib_dev->num_actuators = 1;
		imm_vib_register(vib_dev);
	} else
		printk(KERN_ERR "[VIB] Failed to alloc vibe memory.\n");
#endif

	platform_set_drvdata(pdev, ddata);

#if defined(CONFIG_VIBETONZ)
	g_ddata = ddata;
	vibtonz_en = isa1000_vibtonz_en;
	vibtonz_pwm = isa1000_vibtonz_pwm;
#endif

	return ret;

err_dev_reg:
	pwm_free(ddata->pwm);
err_pwm_request:
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
