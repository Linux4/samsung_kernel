/* drivers/motor/bd6478.c
 *
 * Copyright (C) 2015 Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include <linux/bd6478.h>
#include <linux/wakelock.h>

#include "../staging/android/timed_output.h"
#if defined(CONFIG_IMM_VIB)
#include "imm_vib.h"
#endif

struct bd6478_ddata {
	struct bd6478_pdata *pdata;
	struct pwm_device *pwm;
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct work_struct work;
	unsigned int timeout;
	spinlock_t lock;
	int gpio_en;
};

static enum hrtimer_restart bd6478_timer_func(struct hrtimer *timer)
{
	struct bd6478_ddata *ddata
		= container_of(timer, struct bd6478_ddata, timer);

	ddata->timeout = 0;

	schedule_work(&ddata->work);
	return HRTIMER_NORESTART;
}

static int bd6478_get_time(struct timed_output_dev *dev)
{
	struct bd6478_ddata *ddata
		= container_of(dev, struct  bd6478_ddata, dev);

	struct hrtimer *timer = &ddata->timer;
	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void bd6478_enable(struct timed_output_dev *dev, int value)
{
	struct bd6478_ddata *ddata
		= container_of(dev, struct bd6478_ddata, dev);
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

static void bd6478_pwm_config(struct bd6478_ddata *ddata, int duty)
{
	printk(KERN_DEBUG "[VIB] %s %d\n", __func__, duty);

	if (duty > ddata->pdata->duty)
		duty = ddata->pdata->duty;

	pwm_config(ddata->pwm, duty,
		ddata->pdata->period);
}

static void bd6478_pwm_en(struct bd6478_ddata *ddata, bool en)
{
	printk(KERN_DEBUG "[VIB] %s %s\n",
		__func__, en ? "on" : "off");

	if (en)
		pwm_enable(ddata->pwm);
	else {
		pwm_config(ddata->pwm, 0, ddata->pdata->period);
		pwm_disable(ddata->pwm);
	}
}

static void bd6478_work_func(struct work_struct *work)
{
	struct bd6478_ddata *ddata =
		container_of(work, struct bd6478_ddata, work);

	printk(KERN_DEBUG "[VIB] %ums\n", ddata->timeout);

	if (ddata->timeout) {
		bd6478_pwm_config(ddata, ddata->pdata->duty);
		bd6478_pwm_en(ddata, true);
	} else
		bd6478_pwm_en(ddata, false);
	return;
}

#if defined(CONFIG_IMM_VIB)
static void bd6478_set_force(void *_data, u8 index, int nforce)
{
	struct bd6478_ddata *ddata = _data;

	/* BD motor is not work to CCW */
	if (0 < nforce) {
		int duty = ddata->pdata->period >> 1;
		int tmp = (ddata->pdata->period >> 8);

		tmp *= nforce;
		duty += tmp;

		bd6478_pwm_config(ddata, duty);
	}
}

static void bd6478_chip_enable(void *_data, bool en)
{
	struct bd6478_ddata *ddata = _data;

	bd6478_pwm_en(ddata, en);
}
#endif	/* CONFIG_IMM_VIB */

static struct bd6478_pdata *
	bd6478_get_devtree_pdata(struct device *dev)
{
	struct device_node *node, *child_node;
	struct bd6478_pdata *pdata;
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

	of_property_read_u32(child_node, "bd6478,max_timeout", &pdata->max_timeout);
	of_property_read_u32(child_node, "bd6478,duty", &pdata->duty);
	of_property_read_u32(child_node, "bd6478,period", &pdata->period);
	of_property_read_u32(child_node, "bd6478,pwm_id", &pdata->pwm_id);

	printk(KERN_DEBUG "[VIB] max_timeout = %d\n", pdata->max_timeout);
	printk(KERN_DEBUG "[VIB] duty = %d\n", pdata->duty);
	printk(KERN_DEBUG "[VIB] period = %d\n", pdata->period);
	printk(KERN_DEBUG "[VIB] pwm_id = %d\n", pdata->pwm_id);

	return pdata;

err_out:
	return ERR_PTR(ret);
}

static int bd6478_probe(struct platform_device *pdev)
{
	struct bd6478_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct bd6478_ddata *ddata;
#if defined(CONFIG_IMM_VIB)
	struct imm_vib_dev *vib_dev;
#endif
	int ret = 0;

	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = bd6478_get_devtree_pdata(&pdev->dev);
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
	INIT_WORK(&ddata->work, bd6478_work_func);

	ddata->pdata = pdata;
	ddata->timer.function = bd6478_timer_func;
	ddata->pwm = pwm_request(ddata->pdata->pwm_id, "vibrator");
	if (IS_ERR(ddata->pwm)) {
		printk(KERN_ERR "[VIB] failed to request pwm\n");
		ret = -EFAULT;
		goto err_pwm_request;
	}

	bd6478_pwm_config(ddata, pdata->duty);

	ddata->dev.name = "vibrator";
	ddata->dev.get_time = bd6478_get_time;
	ddata->dev.enable = bd6478_enable;

	ret = timed_output_dev_register(&ddata->dev);
	if (ret < 0) {
		printk(KERN_ERR "[VIB] failed to register timed output\n");
		goto err_dev_reg;
	}

#if defined(CONFIG_IMM_VIB)
	vib_dev = kzalloc(sizeof(struct imm_vib_dev), GFP_KERNEL);
	if (vib_dev) {
		vib_dev->private_data = ddata;
		vib_dev->set_force = bd6478_set_force;
		vib_dev->chip_en = bd6478_chip_enable;
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

static int __devexit bd6478_remove(struct platform_device *pdev)
{
	struct bd6478_ddata *ddata = platform_get_drvdata(pdev);
	timed_output_dev_unregister(&ddata->dev);
	kfree(ddata);
	return 0;
}

static int bd6478_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	struct bd6478_ddata *ddata = platform_get_drvdata(pdev);

	bd6478_pwm_en(ddata, false);
	return 0;
}

static int bd6478_resume(struct platform_device *pdev)
{
	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id bd6478_dt_ids[] = {
	{ .compatible = "bd6478" },
	{ }
};
MODULE_DEVICE_TABLE(of, bd6478_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver bd6478_driver = {
	.probe		= bd6478_probe,
	.remove		= bd6478_remove,
	.suspend	= bd6478_suspend,
	.resume		= bd6478_resume,
	.driver = {
		.name	= "bd6478",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(bd6478_dt_ids),
	},
};

static int __init bd6478_init(void)
{
	return platform_driver_register(&bd6478_driver);
}
module_init(bd6478_init);

static void __exit bd6478_exit(void)
{
	platform_driver_unregister(&bd6478_driver);
}
module_exit(bd6478_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("bd6478 vibrator driver");
