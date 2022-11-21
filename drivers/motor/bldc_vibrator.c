/* drivers/motor/bldc_vibrator.c

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
#include <linux/timed_output.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/bldc_vibrator.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/delay.h>

struct bldc_vibrator_data {
    struct timed_output_dev tout_dev;
	struct hrtimer timer;
	struct work_struct work;
	spinlock_t lock;
	unsigned int timeout;
	bool running;
	struct device* dev;
	struct bldc_vibrator_platform_data *pdata;
	struct regulator *regulator;
	bool resumed;
};

struct bldc_vibrator_data *g_hap_data;

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct bldc_vibrator_data *hap_data
		= container_of(timer, struct bldc_vibrator_data, timer);

	hap_data->timeout = 0;

	schedule_work(&hap_data->work);
	return HRTIMER_NORESTART;
}

static void haptic_work(struct work_struct *work)
{
	struct bldc_vibrator_data *hap_data
		= container_of(work, struct bldc_vibrator_data, work);
	int ret;
	if (hap_data->timeout == 0) {
		if (!hap_data->running)
			return;
		hap_data->running = false;
		gpio_set_value(hap_data->pdata->en, 0);
		regulator_disable(hap_data->regulator);
	} else {
		if (hap_data->running)
			return;
		ret = regulator_enable(hap_data->regulator);
		if (ret)
			goto error_reg_enable;
		gpio_set_value(hap_data->pdata->en, 1);
		hap_data->running = true;
	}
	return;
error_reg_enable :
	pr_err("[VIB] %s : Failed to enable vdd.\n", __func__);
}

static int haptic_get_time(struct timed_output_dev *tout_dev)
{
	struct bldc_vibrator_data *hap_data
		= container_of(tout_dev, struct  bldc_vibrator_data, tout_dev);

	struct hrtimer *timer = &hap_data->timer;
	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void haptic_enable(struct timed_output_dev *tout_dev, int value)
{
	struct bldc_vibrator_data *hap_data
		= container_of(tout_dev, struct bldc_vibrator_data, tout_dev);

	struct hrtimer *timer = &hap_data->timer;
	unsigned long flags;


	cancel_work_sync(&hap_data->work);
	hrtimer_cancel(timer);
	hap_data->timeout = value;
	schedule_work(&hap_data->work);
	spin_lock_irqsave(&hap_data->lock, flags);
	if (value > 0 ) {
		pr_info("[VIB] %s : value %d\n", __func__, value);
		if (value > hap_data->pdata->max_timeout)
			value = hap_data->pdata->max_timeout;
		hrtimer_start(timer, ns_to_ktime((u64)value * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&hap_data->lock, flags);
}

#if defined(CONFIG_OF)
static int of_bldc_vibrator_dt(struct bldc_vibrator_platform_data *pdata)
{
	struct device_node *np_haptic;
	int temp;
	const char *temp_str;
	int ret;

	pr_info("[VIB] ++ %s\n", __func__);

	np_haptic = of_find_node_by_path("/bldc-vibrator");
	if (np_haptic == NULL) {
		pr_err("[VIB] %s : error to get dt node\n", __func__);
		goto err_parsing_dt;
	}

	ret = of_property_read_u32(np_haptic, "haptic,max_timeout", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node max_timeout\n", __func__);
		goto err_parsing_dt;
	}
	pdata->max_timeout = temp;

	ret = of_property_read_string(np_haptic, "haptic,regulator_name", &temp_str);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node regulator_name\n", __func__);
		goto err_parsing_dt;
	}
	pdata->regulator_name = (char *)temp_str;

	ret = of_get_named_gpio(np_haptic, "haptic,en", 0);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node en\n", __func__);
		goto err_parsing_dt;
	}

	pdata->en = (unsigned int)ret;

	/* debugging */
	pr_info("[VIB] %s : max_timeout = %d\n", __func__, pdata->max_timeout);
	pr_info("[VIB] %s : regulator_name = %s\n", __func__, pdata->regulator_name);

	return 0;

err_parsing_dt:

	return -EINVAL;
}
#endif /* CONFIG_OF */

static int bldc_vibrator_probe(struct platform_device *pdev)
{
	int ret, error = 0;
#if !defined(CONFIG_OF)
	struct bldc_vibrator_platform_data *bldc_pdata
		= dev_get_platdata(&pdev->dev);
#endif
	struct bldc_vibrator_data *hap_data;

	pr_info("[VIB] ++ %s\n", __func__);

	hap_data = kzalloc(sizeof(struct bldc_vibrator_data), GFP_KERNEL);
	if (!hap_data)
		return -ENOMEM;

#if defined(CONFIG_OF)
	hap_data->pdata = kzalloc(sizeof(struct bldc_vibrator_data), GFP_KERNEL);
	if (!hap_data->pdata) {
		kfree(hap_data);
		return -ENOMEM;
	}

	ret = of_bldc_vibrator_dt(hap_data->pdata);
	if (ret < 0) {
		pr_info("[VIB] %s : not found haptic dt! ret[%d]\n",
				 __func__, ret);
		kfree(hap_data->pdata);
		kfree(hap_data);
		return ret;
	}
#else
	hap_data->pdata = bldc_pdata;
	if (hap_data->pdata == NULL) {
		pr_err("[VIB] %s : no pdata\n", __func__);
		kfree(hap_data);
		return -ENODEV;
	}
#endif /* CONFIG_OF */

	ret = devm_gpio_request_one(&pdev->dev, hap_data->pdata->en,
					GPIOF_OUT_INIT_LOW, "motor_en");
	if (ret) {
		pr_err("[VIB] %s : motor_en gpio request error\n", __func__);
		kfree(hap_data->pdata);
		kfree(hap_data);
		return ret;
	}

	platform_set_drvdata(pdev, hap_data);
	g_hap_data = hap_data;
	hap_data->dev = &pdev->dev;
	INIT_WORK(&(hap_data->work), haptic_work);
	spin_lock_init(&(hap_data->lock));

	hap_data->regulator
			= regulator_get(NULL, hap_data->pdata->regulator_name);

	if (IS_ERR(hap_data->regulator)) {
		pr_err("[VIB] %s : Failed to get vmoter regulator.\n", __func__);
		return -EINVAL;
	}
	/* hrtimer init */
	hrtimer_init(&hap_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hap_data->timer.function = haptic_timer_func;

	/* timed_output_dev init*/
	hap_data->tout_dev.name = "vibrator";
	hap_data->tout_dev.get_time = haptic_get_time;
	hap_data->tout_dev.enable = haptic_enable;

	hap_data->resumed = false;

	error = timed_output_dev_register(&hap_data->tout_dev);
	if (error < 0) {
		pr_err("[VIB] %s : Failed to register timed_output : %d\n", __func__, error);
		error = -EFAULT;
		goto err_timed_output_register;
	}

	pr_info("[VIB] -- %s\n", __func__);

	return error;

err_timed_output_register:
	regulator_put(hap_data->regulator);
	return error;
}

static int __devexit bldc_vibrator_remove(struct platform_device *pdev)
{
	struct bldc_vibrator_data *data = platform_get_drvdata(pdev);

	timed_output_dev_unregister(&data->tout_dev);
	regulator_put(data->regulator);
	kfree(data->pdata);
	kfree(data);
	g_hap_data = NULL;
	return 0;
}
#if defined(CONFIG_OF)
static struct of_device_id haptic_dt_ids[] = {
	{ .compatible = "bldc-vibrator" },
	{ },
};
MODULE_DEVICE_TABLE(of, haptic_dt_ids);
#endif /* CONFIG_OF */
static int bldc_vibrator_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	pr_info("[VIB] %s\n", __func__);
	if (g_hap_data != NULL) {
		cancel_work_sync(&g_hap_data->work);
		hrtimer_cancel(&g_hap_data->timer);
	}
	return 0;
}

static int bldc_vibrator_resume(struct platform_device *pdev)
{
	pr_info("[VIB] %s\n", __func__);
	g_hap_data->resumed = true;
	return 0;
}

static struct platform_driver bldc_vibrator_driver = {
	.probe		= bldc_vibrator_probe,
	.remove		= bldc_vibrator_remove,
	.suspend	= bldc_vibrator_suspend,
	.resume		= bldc_vibrator_resume,
	.driver = {
		.name	= "bldc-vibrator",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table	= haptic_dt_ids,
#endif /* CONFIG_OF */
	},
};

static int __init bldc_vibrator_init(void)
{
	pr_debug("[VIB] %s\n", __func__);
	return platform_driver_register(&bldc_vibrator_driver);
}
module_init(bldc_vibrator_init);

static void __exit bldc_vibrator_exit(void)
{
	platform_driver_unregister(&bldc_vibrator_driver);
}
module_exit(bldc_vibrator_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BLDC motor driver");
