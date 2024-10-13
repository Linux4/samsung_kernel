/* drivers/misc/timed_gpio.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>

#include "timed_output.h"
#include "timed_gpio.h"
#if defined(CONFIG_OF)
#include <linux/of_gpio.h>
#endif

struct timed_gpio_data {
	struct timed_output_dev dev;
	struct hrtimer timer;
	spinlock_t lock;
	unsigned gpio;
	int max_timeout;
	u8 active_low;
};

static enum hrtimer_restart gpio_timer_func(struct hrtimer *timer)
{
	struct timed_gpio_data *data =
		container_of(timer, struct timed_gpio_data, timer);

	gpio_direction_output(data->gpio, data->active_low ? 1 : 0);
	return HRTIMER_NORESTART;
}

static int gpio_get_time(struct timed_output_dev *dev)
{
	struct timed_gpio_data	*data =
		container_of(dev, struct timed_gpio_data, dev);

	if (hrtimer_active(&data->timer)) {
		ktime_t r = hrtimer_get_remaining(&data->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void gpio_enable(struct timed_output_dev *dev, int value)
{
	struct timed_gpio_data	*data =
		container_of(dev, struct timed_gpio_data, dev);
	unsigned long	flags;

	/* cancel previous timer and set GPIO according to value */
	hrtimer_cancel(&data->timer);
	gpio_direction_output(data->gpio, data->active_low ? !value : !!value);

	spin_lock_irqsave(&data->lock, flags);

	if (value > 0) {
		if (value > data->max_timeout)
			value = data->max_timeout;

		hrtimer_start(&data->timer,
			ktime_set(value / 1000, (value % 1000) * 1000000),
			HRTIMER_MODE_REL);
	}

	spin_unlock_irqrestore(&data->lock, flags);
}

#if defined(CONFIG_OF)
static struct timed_gpio_platform_data *timed_gpio_get_dt(struct device *dev)
{
	struct device_node *node, *pp;
	struct timed_gpio_platform_data *pdata;
	struct timed_gpio *timed_gpio;
	int ret = 0, num_gpios = 0, i = 0;

	node = dev->of_node;
	if (!node) {
		ret = -ENODEV;
		goto err_out;
	}

	num_gpios = of_get_child_count(node);
	if (!num_gpios) {
		ret = -ENODEV;
		goto err_out;
	}

	pdata = kzalloc(sizeof(*pdata) + num_gpios * (sizeof(*timed_gpio)),
		GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto err_out;
	}

	pdata->gpios = (struct timed_gpio *)(pdata + 1);
	pdata->num_gpios = num_gpios;

	for_each_child_of_node(node, pp) {
		int gpio = 0;
		enum of_gpio_flags flags;

		if (!of_find_property(pp, "gpios", NULL)) {
			dev_warn(dev, "Found timed gpio without gpios\n");
			pdata->num_gpios--;
			continue;
		}

		gpio = of_get_gpio_flags(pp, 0, &flags);
		if (gpio < 0) {
			ret = gpio;
			if (ret != -EPROBE_DEFER)
				dev_err(dev,
					"Failed to get gpio flags : %d\n",
					ret);
			goto err_free_pdata;
		}

		timed_gpio = &pdata->gpios[i++];
		timed_gpio->gpio = gpio;
		timed_gpio->active_low = flags & OF_GPIO_ACTIVE_LOW;
		timed_gpio->name = of_get_property(pp, "timed_gpio,name", NULL);

		if (of_property_read_u32(pp, "timed_gpio,max_timeout",
					 &timed_gpio->max_timeout)) {
			if (10000 < timed_gpio->max_timeout)
				timed_gpio->max_timeout = 10000;
		}
	}

	if (!pdata->num_gpios) {
		ret = -EINVAL;
		goto err_free_pdata;
	}

	return pdata;

err_free_pdata:
	kfree(pdata);
err_out:
	return ERR_PTR(ret);
}

static struct of_device_id timed_gpio_of_match[] = {
	{ .compatible = "timed_gpio", },
	{ },
};
MODULE_DEVICE_TABLE(of, timed_gpio_of_match);
#endif

static int timed_gpio_probe(struct platform_device *pdev)
{
	struct timed_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct timed_gpio *cur_gpio;
	struct timed_gpio_data *gpio_data, *gpio_dat;
	int i, ret;

	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = timed_gpio_get_dt(&pdev->dev);
		if (IS_ERR(pdata))
			return -EBUSY;
	}
#else
		return -EBUSY;
	}
#endif

	gpio_data = kzalloc(sizeof(struct timed_gpio_data) * pdata->num_gpios,
			GFP_KERNEL);
	if (!gpio_data)
		return -ENOMEM;

	for (i = 0; i < pdata->num_gpios; i++) {
		cur_gpio = &pdata->gpios[i];
		gpio_dat = &gpio_data[i];

		hrtimer_init(&gpio_dat->timer, CLOCK_MONOTONIC,
				HRTIMER_MODE_REL);
		gpio_dat->timer.function = gpio_timer_func;
		spin_lock_init(&gpio_dat->lock);

		gpio_dat->dev.name = cur_gpio->name;
		gpio_dat->dev.get_time = gpio_get_time;
		gpio_dat->dev.enable = gpio_enable;
		ret = gpio_request(cur_gpio->gpio, cur_gpio->name);
		if (ret < 0)
			goto err_out;
		ret = timed_output_dev_register(&gpio_dat->dev);
		if (ret < 0) {
			gpio_free(cur_gpio->gpio);
			goto err_out;
		}

		gpio_dat->gpio = cur_gpio->gpio;
		gpio_dat->max_timeout = cur_gpio->max_timeout;
		gpio_dat->active_low = cur_gpio->active_low;
		gpio_direction_output(gpio_dat->gpio, gpio_dat->active_low);
	}

	platform_set_drvdata(pdev, gpio_data);

	return 0;

err_out:
	while (--i >= 0) {
		timed_output_dev_unregister(&gpio_data[i].dev);
		gpio_free(gpio_data[i].gpio);
	}
	kfree(gpio_data);

	return ret;
}

static int timed_gpio_remove(struct platform_device *pdev)
{
	struct timed_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct timed_gpio_data *gpio_data = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < pdata->num_gpios; i++) {
		timed_output_dev_unregister(&gpio_data[i].dev);
		gpio_free(gpio_data[i].gpio);
	}

	kfree(gpio_data);

	return 0;
}

static struct platform_driver timed_gpio_driver = {
	.probe		= timed_gpio_probe,
	.remove		= timed_gpio_remove,
	.driver		= {
		.name		= TIMED_GPIO_NAME,
		.owner		= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(timed_gpio_of_match),
#endif
	},
};

module_platform_driver(timed_gpio_driver);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("timed gpio driver");
MODULE_LICENSE("GPL");
