/* drivers/motor/drv2603v_vibrator.c

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
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/drv2603v_vibrator.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/sec_sysfs.h>

#if defined(CONFIG_VIBETONZ)
void (*vibtonz_en)(bool);
void (*vibtonz_pwm)(int);
#endif
struct drv2603v_vibrator_data {
    struct device* dev;
	struct drv2603v_vibrator_platform_data *pdata;
	struct pwm_device *pwm;
	struct timed_output_dev tout_dev;
	struct hrtimer timer;
	unsigned int timeout;
	struct work_struct work;
	spinlock_t lock;
	bool running;
};

static struct device *motor_dev;
struct drv2603v_vibrator_data *g_hap_data;
static int prev_duty;

static int haptic_get_time(struct timed_output_dev *tout_dev)
{
	struct drv2603v_vibrator_data *hap_data
		= container_of(tout_dev, struct  drv2603v_vibrator_data, tout_dev);

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
	struct drv2603v_vibrator_data *hap_data
		= container_of(tout_dev, struct drv2603v_vibrator_data, tout_dev);

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

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct drv2603v_vibrator_data *hap_data
		= container_of(timer, struct drv2603v_vibrator_data, timer);

	hap_data->timeout = 0;

	schedule_work(&hap_data->work);
	return HRTIMER_NORESTART;
}

static int vibetonz_clk_on(struct device *dev, bool en)
{
	struct clk *vibetonz_clk = NULL;
#if defined(CONFIG_OF)
	struct device_node *np;
	np = of_find_node_by_name(NULL,"pwm");
	if (np == NULL) {
		pr_err("[VIB] %s : pwm error to get dt node\n", __func__);
		return -EINVAL;
	}
	vibetonz_clk = of_clk_get_by_name(np, "gate_timers");
	if (!vibetonz_clk) {
		pr_err("[VIB] %s : fail to get the vibetonz_clk\n", __func__);
		return -EINVAL;
	}
#else
	vibetonz_clk = clk_get(dev, "timers");
#endif
	pr_info("[VIB] %s : DEV NAME %s %lu\n", __func__,
		 dev_name(dev), clk_get_rate(vibetonz_clk));

	if (IS_ERR(vibetonz_clk)) {
		pr_err("[VIB] %s : failed to get clock for the motor\n", __func__);
		goto err_clk_get;
	}

	if (en)
		clk_enable(vibetonz_clk);
	else
		clk_disable(vibetonz_clk);

	clk_put(vibetonz_clk);
	return 0;

err_clk_get:
	clk_put(vibetonz_clk);
	return -EINVAL;
}

static void haptic_work(struct work_struct *work)
{
	struct drv2603v_vibrator_data *hap_data
		= container_of(work, struct drv2603v_vibrator_data, work);

	if (hap_data->timeout == 0) {
		if (!hap_data->running)
			return;
		hap_data->running = false;
		gpio_set_value(hap_data->pdata->en, 0);
		pwm_disable(hap_data->pwm);
	} else {
		if (hap_data->running)
			return;
		pwm_config(hap_data->pwm, hap_data->pdata->duty,
			   hap_data->pdata->period);
		pwm_enable(hap_data->pwm);
		gpio_set_value(hap_data->pdata->en, 1);
		hap_data->running = true;
	}
	return;
}

#ifdef CONFIG_VIBETONZ
void drv2603v_vibtonz_en(bool en)
{

	pr_info("[VIB] %s %s\n", __func__, en ? "on" : "off");

	if (g_hap_data == NULL) {
		pr_err("[VIB] %s : the motor is not ready!!!", __func__);
		return ;
	}
	if (en) {
		if (g_hap_data->running)
			return;

		pwm_config(g_hap_data->pwm, prev_duty, g_hap_data->pdata->period);
		pwm_enable(g_hap_data->pwm);
		gpio_set_value(g_hap_data->pdata->en, 1);
		g_hap_data->running = true;
	} else {
		if (!g_hap_data->running)
			return;
		gpio_set_value(g_hap_data->pdata->en, 0);
		pwm_disable(g_hap_data->pwm);
		g_hap_data->running = false;
	}
	return;
}
EXPORT_SYMBOL(vibtonz_en);

void drv2603v_vibtonz_pwm(int nForce)
{
	int pwm_period = 0, pwm_duty = 0;

	if (g_hap_data == NULL) {
		pr_err("[VIB] %s : the motor is not ready!!!", __func__);
		return ;
	}

	pwm_period = g_hap_data->pdata->period;
	pwm_duty = pwm_period / 2 + ((pwm_period / 2 - 2) * nForce) / 127;

	if (pwm_duty > g_hap_data->pdata->duty)
		pwm_duty = g_hap_data->pdata->duty;
	else if (pwm_period - pwm_duty > g_hap_data->pdata->duty)
		pwm_duty = pwm_period - g_hap_data->pdata->duty;

	/* add to avoid the glitch issue */
	if (prev_duty != pwm_duty) {
		prev_duty = pwm_duty;
		pwm_config(g_hap_data->pwm, pwm_duty, pwm_period);
	}
}
EXPORT_SYMBOL(vibtonz_pwm);
#endif

#if defined(CONFIG_OF)
static int of_drv2603v_vibrator_dt(struct drv2603v_vibrator_platform_data *pdata)
{
	struct device_node *np_haptic;
	int temp;
	int ret;

	pr_info("[VIB] ++ %s\n", __func__);

	np_haptic = of_find_node_by_path("/haptic");
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

	ret = of_property_read_u32(np_haptic, "haptic,duty", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node duty\n", __func__);
		goto err_parsing_dt;
	}
	pdata->duty = temp;

	ret = of_property_read_u32(np_haptic, "haptic,period", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node period\n", __func__);
		goto err_parsing_dt;
	}
	pdata->period = temp;

	ret = of_property_read_u32(np_haptic, "haptic,pwm_id", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node pwm_id\n", __func__);
		goto err_parsing_dt;
	}
	pdata->pwm_id = temp;

	ret = of_get_named_gpio(np_haptic, "haptic,en", 0);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node en\n", __func__);
		goto err_parsing_dt;
	}

	pdata->en = (unsigned int)ret;

	/* debugging */
	pr_info("[VIB] %s : max_timeout = %d\n", __func__, pdata->max_timeout);
	pr_info("[VIB] %s : duty = %d\n", __func__, pdata->duty);
	pr_info("[VIB] %s : period = %d\n", __func__, pdata->period);
	pr_info("[VIB] %s : pwm_id = %d\n", __func__, pdata->pwm_id);

	return 0;

err_parsing_dt:

	return -EINVAL;
}
#endif /* CONFIG_OF */

static ssize_t store_duty(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	char buff[10] = {0,};
	int cnt, ret;
	u16 duty;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou16(buff, 0, &duty);
	if (ret != 0) {
		dev_err(dev, "[VIB] fail to get duty.\n");
		return count;
	}
	g_hap_data->pdata->duty = (u16)duty;

	return count;
}

static ssize_t store_period(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	char buff[10] = {0,};
	int cnt, ret;
	u16 period;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou16(buff, 0, &period);
	if (ret != 0) {
		dev_err(dev, "[VIB] fail to get period.\n");
		return count;
	}
	g_hap_data->pdata->period = (u16)period;

	return count;
}

static ssize_t show_duty(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", g_hap_data->pdata->duty);
}

static ssize_t show_period(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", g_hap_data->pdata->period);
}

/* below nodes is SAMSUNG specific nodes */
static DEVICE_ATTR(set_duty, 0666, show_duty, store_duty);
static DEVICE_ATTR(set_period, 0666, show_period, store_period);

static struct attribute *sec_motor_attributes[] = {
	&dev_attr_set_duty.attr,
	&dev_attr_set_period.attr,
	NULL,
};

static struct attribute_group sec_motor_attr_group = {
	.attrs = sec_motor_attributes,
};

static int drv2603v_vibrator_probe(struct platform_device *pdev)
{
	int ret, error = 0;
#if !defined(CONFIG_OF)
	struct drv2603v_vibrator_platform_data *drv2603v_pdata
		= dev_get_platdata(&pdev->dev);
#endif
	struct drv2603v_vibrator_data *hap_data;

	pr_info("[VIB] ++ %s\n", __func__);

	hap_data = kzalloc(sizeof(struct drv2603v_vibrator_data), GFP_KERNEL);
	if (!hap_data)
		return -ENOMEM;

#if defined(CONFIG_OF)
	hap_data->pdata = kzalloc(sizeof(struct drv2603v_vibrator_data), GFP_KERNEL);
	if (!hap_data->pdata) {
		kfree(hap_data);
		return -ENOMEM;
	}

	ret = of_drv2603v_vibrator_dt(hap_data->pdata);
	if (ret < 0) {
		pr_info("[VIB] %s : not found haptic dt! ret[%d]\n",
				 __func__, ret);
		kfree(hap_data->pdata);
		kfree(hap_data);
		return ret;
	}
#else
	hap_data->pdata = drv2603v_pdata;
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
	hap_data->pwm = pwm_request(hap_data->pdata->pwm_id, "vibrator");
	if (IS_ERR(hap_data->pwm)) {
		pr_err("[VIB] %s : Failed to request pwm\n", __func__);
		error = -EFAULT;
		goto err_pwm_request;
	}

	pwm_config(hap_data->pwm, hap_data->pdata->period / 2, hap_data->pdata->period);
	prev_duty = hap_data->pdata->period / 2;

	vibetonz_clk_on(&pdev->dev, true);

	/* hrtimer init */
	hrtimer_init(&hap_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hap_data->timer.function = haptic_timer_func;

	/* timed_output_dev init*/
	hap_data->tout_dev.name = "vibrator";
	hap_data->tout_dev.get_time = haptic_get_time;
	hap_data->tout_dev.enable = haptic_enable;

	motor_dev = sec_device_create(hap_data, "motor");
	if (IS_ERR(motor_dev)) {
		error = -ENODEV;
		pr_err("[VIB] %s : Failed to create device\
				for samsung specific motor, err num: %d\n", __func__, error);
		goto exit_sec_devices;
	}

	error = sysfs_create_group(&motor_dev->kobj, &sec_motor_attr_group);
	if (error) {
		error = -ENODEV;
		pr_err("[VIB] %s : Failed to create sysfs group\
				for samsung specific motor, err num: %d\n", __func__, error);
		goto exit_sysfs;
	}

	error = timed_output_dev_register(&hap_data->tout_dev);
	if (error < 0) {
		pr_err("[VIB] %s : Failed to register timed_output : %d\n", __func__, error);
		error = -EFAULT;
		goto err_timed_output_register;
	}
#if defined(CONFIG_VIBETONZ)
	vibtonz_en = drv2603v_vibtonz_en;
	vibtonz_pwm = drv2603v_vibtonz_pwm;
#endif

	pr_info("[VIB] -- %s\n", __func__);

	return error;

err_timed_output_register:
	sysfs_remove_group(&motor_dev->kobj, &sec_motor_attr_group);
exit_sysfs:
	sec_device_destroy(motor_dev->devt);
exit_sec_devices:
	pwm_free(hap_data->pwm);
err_pwm_request:
	kfree(hap_data->pdata);
	kfree(hap_data);
	g_hap_data = NULL;
	return error;
}

static int __devexit drv2603v_vibrator_remove(struct platform_device *pdev)
{
	struct drv2603v_vibrator_data *data = platform_get_drvdata(pdev);

	timed_output_dev_unregister(&data->tout_dev);
	pwm_free(data->pwm);
	kfree(data->pdata);
	kfree(data);
	g_hap_data = NULL;
	return 0;
}
#if defined(CONFIG_OF)
static struct of_device_id haptic_dt_ids[] = {
	{ .compatible = "drv2603v-vibrator" },
	{ },
};
MODULE_DEVICE_TABLE(of, haptic_dt_ids);
#endif /* CONFIG_OF */
static int drv2603v_vibrator_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	pr_info("[VIB] %s\n", __func__);
	if (g_hap_data != NULL) {
		cancel_work_sync(&g_hap_data->work);
		hrtimer_cancel(&g_hap_data->timer);
	}
	vibetonz_clk_on(&pdev->dev, false);
	return 0;
}

static int drv2603v_vibrator_resume(struct platform_device *pdev)
{
	pr_info("[VIB] %s\n", __func__);
	vibetonz_clk_on(&pdev->dev, true);
	return 0;
}

static struct platform_driver drv2603v_vibrator_driver = {
	.probe		= drv2603v_vibrator_probe,
	.remove		= drv2603v_vibrator_remove,
	.suspend	= drv2603v_vibrator_suspend,
	.resume		= drv2603v_vibrator_resume,
	.driver = {
		.name	= "drv2603v-vibrator",
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table	= haptic_dt_ids,
#endif /* CONFIG_OF */
	},
};

static int __init drv2603v_vibrator_init(void)
{
	pr_debug("[VIB] %s\n", __func__);
	return platform_driver_register(&drv2603v_vibrator_driver);
}
module_init(drv2603v_vibrator_init);

static void __exit drv2603v_vibrator_exit(void)
{
	platform_driver_unregister(&drv2603v_vibrator_driver);
}
module_exit(drv2603v_vibrator_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DRV2603V motor driver");
