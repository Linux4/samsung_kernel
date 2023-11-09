/* drivers/motor/dc_motor.c
 *
 * Copyright (C) 2013 Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/dc_motor.h>
#include <linux/fs.h>

#if defined(CONFIG_OF)
#include <linux/of_gpio.h>
#endif
#include <../drivers/staging/android/timed_output.h>

struct dc_motor_drvdata {
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct work_struct work;
	struct dc_motor_platform_data *pdata;
	spinlock_t lock;
	bool running;
	int timeout;
};

static enum hrtimer_restart dc_motor_timer_func(struct hrtimer *_timer)
{
	struct dc_motor_drvdata *data =
		container_of(_timer, struct dc_motor_drvdata, timer);

	data->timeout = 0;

	schedule_work(&data->work);
	return HRTIMER_NORESTART;
}

static void dc_motor_work(struct work_struct *_work)
{
	struct dc_motor_drvdata *data =
		container_of(_work, struct dc_motor_drvdata, work);

	if (data->running == !!data->timeout)
		return ;

	data->running = data->timeout;

	gpio_direction_output(data->pdata->gpio,
		data->pdata->active_low ? !data->running : data->running);
}

static int dc_motor_get_time(struct timed_output_dev *_dev)
{
	struct dc_motor_drvdata	*data =
		container_of(_dev, struct dc_motor_drvdata, dev);

	if (hrtimer_active(&data->timer)) {
		ktime_t r = hrtimer_get_remaining(&data->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

#if defined(CONFIG_OF)
static struct dc_motor_platform_data *dc_motor_get_dt(struct device *dev)
{
	struct device_node *node = dev->of_node;
	struct dc_motor_platform_data *pdata;
	enum of_gpio_flags flags;
	int ret = 0;

	if (!node) {
		ret = -ENODEV;
		goto err_out;
	}

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto err_out;
	}

	pdata->gpio = of_get_gpio_flags(node, 0, &flags);
	if (gpio_is_valid(pdata->gpio))
		gpio_request(pdata->gpio, "MOTOR_EN");

	pdata->active_low = flags & OF_GPIO_ACTIVE_LOW;
	if (of_property_read_u32(node, "dc_motor,max_timeout",
				 &pdata->max_timeout)) {
		if (10000 < pdata->max_timeout)
			pdata->max_timeout = 10000;
	}

	return pdata;

err_out:
	return ERR_PTR(ret);
}

static struct of_device_id dc_motor_of_match[] = {
	{ .compatible = "dc_motor", },
	{ },
};
MODULE_DEVICE_TABLE(of, dc_motor_of_match);
#endif

static void dc_motor_enable(struct timed_output_dev *_dev, int value)
{
	struct dc_motor_drvdata *data =
		container_of(_dev, struct dc_motor_drvdata, dev);
	unsigned long	flags;

	printk(KERN_DEBUG "[VIB] time = %dms\n", value);

	cancel_work_sync(&data->work);
	hrtimer_cancel(&data->timer);
	data->timeout = value;
	schedule_work(&data->work);
	spin_lock_irqsave(&data->lock, flags);
	if (value > 0) {
		if (value > data->pdata->max_timeout)
			value = data->pdata->max_timeout;

		hrtimer_start(&data->timer,
			ns_to_ktime((u64)value * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&data->lock, flags);
}

static int __devinit dc_motor_driver_probe(struct platform_device *pdev)
{
	struct dc_motor_platform_data *pdata = pdev->dev.platform_data;
	struct dc_motor_drvdata *ddata;
	int ret = 0;

	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = dc_motor_get_dt(&pdev->dev);
		if (IS_ERR(pdata))
#endif
			return -EBUSY;
	}

	ddata = kzalloc(sizeof(struct dc_motor_drvdata), GFP_KERNEL);
	if (!ddata) {
		printk(KERN_ERR "[VIB] Failed to alloc memory\n");
		ret = -ENOMEM;
		goto err_free_mem;
	}

	ddata->pdata = pdata;
	ddata->dev.name = "vibrator";
	ddata->dev.get_time = dc_motor_get_time;
	ddata->dev.enable = dc_motor_enable;

	hrtimer_init(&ddata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ddata->timer.function = dc_motor_timer_func;
	INIT_WORK(&ddata->work, dc_motor_work);
	spin_lock_init(&ddata->lock);

	platform_set_drvdata(pdev, ddata);

	ret = timed_output_dev_register(&ddata->dev);
	if (ret < 0) {
		printk(KERN_ERR
			"[VIB] Failed to register timed_output : -%d\n", ret);
		goto err_to_dev_reg;
	}

	return 0;

err_to_dev_reg:
	kfree(ddata);
err_free_mem:
	return ret;
}

static int __devexit dc_motor_remove(struct platform_device *pdev)
{
	struct dc_motor_drvdata *ddata = platform_get_drvdata(pdev);
	timed_output_dev_unregister(&ddata->dev);
	kfree(ddata);
	return 0;
}

#ifdef CONFIG_PM
static int dc_motor_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dc_motor_drvdata *ddata = platform_get_drvdata(pdev);

	cancel_work_sync(&ddata->work);
	hrtimer_cancel(&ddata->timer);

	ddata->timeout = 0;
	ddata->running = false;
	gpio_direction_output(ddata->pdata->gpio,
		ddata->pdata->active_low);
	return 0;
}

static int dc_motor_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops dc_motor_pm_ops = {
	.suspend = dc_motor_suspend,
	.resume = dc_motor_resume,
};
#endif

static struct platform_driver dc_motor_driver = {
	.probe		= dc_motor_driver_probe,
	.remove		= __devexit_p(dc_motor_remove),
	.driver		= {
		.name	= "dc_motor",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &dc_motor_pm_ops,
#endif
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(dc_motor_of_match),
#endif
	}
};

module_platform_driver(dc_motor_driver);

/* Module information */
MODULE_DESCRIPTION("vibrator driver for the DC motor");
MODULE_LICENSE("GPL");

