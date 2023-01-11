/* Copyright (C) 2014 Marvell */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/regulator/machine.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/wakelock.h>
#include "../staging/android/timed_output.h"

static struct wake_lock vib_wake_lock;

struct pwm_vibrator_info {
	struct device *dev;
	struct pwm_device *pwm;
	struct regulator *vib_regulator;
	struct timed_output_dev vibrator_timed_dev;
	struct timer_list vibrate_timer;
	struct work_struct vibrator_off_work;
	struct mutex vib_mutex;
	int enable;
	int min_timeout;
	unsigned int period;
	unsigned int duty_cycle;
	int vib_ldo_en;
	int vdd_type;
};

#define VIBRA_OFF_VALUE	0
#define VIBRA_ON_VALUE	1

static void vibrator_ldo_control(struct pwm_vibrator_info *info, int enable)
{
	int ret;

	if (info->vdd_type) {
		gpio_set_value(info->vib_ldo_en, enable);
	} else {
		info->vib_regulator = regulator_get(info->dev, "vibrator");
		if (IS_ERR(info->vib_regulator)) {
			dev_err(info->dev, "get vibrator ldo fail!\n");
			return;
		}

		if (enable) {
			regulator_set_voltage(info->vib_regulator,
						3300000, 3300000);
			ret = regulator_enable(info->vib_regulator);
			if (ret)
				dev_err(info->dev,
					"enable vibrator ldo fail!\n");
		} else {
			ret = regulator_disable(info->vib_regulator);
			if (ret)
				dev_err(info->dev,
					"disable vibrator ldo fail!\n");
		}

		regulator_put(info->vib_regulator);
	}
}

static int get_vibrator_platdata(struct device_node *np,
					struct pwm_vibrator_info *info)
{
	u32 value;
	int ret;
	ret = of_property_read_u32(np, "min_timeout", &value);
	if (ret < 0)
		dev_dbg(info->dev, "vibrator min time out is not set\n");
	else
		info->min_timeout = (int)value;

	ret = of_property_read_u32(np, "duty_cycle", &value);
	if (ret < 0)
		dev_dbg(info->dev, "vibrator duty cycle is not set\n");
	else
		info->duty_cycle = value;

	ret = of_property_read_u32(np, "vdd_type", &value);
	if (ret < 0)
		dev_dbg(info->dev, "vibrator vdd type is not set\n");
	else
		info->vdd_type = value;

	if (info->vdd_type) {
		ret = of_get_named_gpio(np, "vib_ldo_en", 0);
		if (ret < 0)
			dev_dbg(info->dev, "vibrator vdd type is not set\n");
		else
			info->vib_ldo_en = ret;
	}

	return 0;
}

static int pwm_control_vibrator(struct pwm_vibrator_info *info,
				unsigned char value)
{
	mutex_lock(&info->vib_mutex);
	if (info->enable == value) {
		mutex_unlock(&info->vib_mutex);
		return 0;
	}

	if (value == VIBRA_OFF_VALUE) {
		vibrator_ldo_control(info, 0);
		pwm_config(info->pwm, 0, info->period);
		pwm_disable(info->pwm);
	} else if (value == VIBRA_ON_VALUE) {
		pwm_config(info->pwm,
				info->duty_cycle ? info->duty_cycle : 0x50,
				info->period);
		pwm_enable(info->pwm);
		vibrator_ldo_control(info, 1);
	}
	info->enable = value;
	mutex_unlock(&info->vib_mutex);

	return 0;
}

static void vibrator_off_worker(struct work_struct *work)
{
	struct pwm_vibrator_info *info;

	info = container_of(work, struct pwm_vibrator_info, vibrator_off_work);
	pwm_control_vibrator(info, VIBRA_OFF_VALUE);
	if(wake_lock_active(&vib_wake_lock))
		wake_unlock(&vib_wake_lock);
}

static void on_vibrate_timer_expired(unsigned long x)
{
	struct pwm_vibrator_info *info;
	info = (struct pwm_vibrator_info *)x;
	schedule_work(&info->vibrator_off_work);
}

static void vibrator_enable_set_timeout(struct timed_output_dev *sdev,
					int timeout)
{
	struct pwm_vibrator_info *info;
	info = container_of(sdev, struct pwm_vibrator_info,
				vibrator_timed_dev);
	dev_dbg(info->dev, "Vibrator: Set duration: %dms\n", timeout);

	if (timeout <= 0) {
		pwm_control_vibrator(info, VIBRA_OFF_VALUE);
		del_timer(&info->vibrate_timer);
		if(wake_lock_active(&vib_wake_lock))
			wake_unlock(&vib_wake_lock);
	} else {
		if (info->min_timeout)
			timeout = (timeout < info->min_timeout) ?
				   info->min_timeout : timeout;

		pwm_control_vibrator(info, VIBRA_ON_VALUE);
		if(!wake_lock_active(&vib_wake_lock))
			wake_lock(&vib_wake_lock);
		mod_timer(&info->vibrate_timer,
			  jiffies + msecs_to_jiffies(timeout));
	}

	return;
}

static int vibrator_get_remaining_time(struct timed_output_dev *sdev)
{
	struct pwm_vibrator_info *info;
	int rettime;
	info = container_of(sdev, struct pwm_vibrator_info,
			vibrator_timed_dev);
	rettime = jiffies_to_msecs(jiffies - info->vibrate_timer.expires);
	dev_dbg(info->dev, "Vibrator: Current duration: %dms\n", rettime);
	return rettime;
}

static int vibrator_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	struct pwm_vibrator_info *info;

	info = devm_kzalloc(&pdev->dev, sizeof(struct pwm_vibrator_info),
								GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(info->pwm)) {
		dev_err(&pdev->dev, "unable to request PWM\n");
		ret = PTR_ERR(info->pwm);
		return ret;
	}
	dev_dbg(&pdev->dev, "got pwm for vibrator\n");
	info->period = pwm_get_period(info->pwm);
	/*get vibrator platdata from dts file*/
	if ((np != NULL) && (info != NULL))
		get_vibrator_platdata(np, info);

	if (info->vdd_type) {
		ret = gpio_request(info->vib_ldo_en, "pwm_vibrator");
		if (ret < 0)
			dev_err(&pdev->dev,
				"unable to request gpio(%d)\n", ret);
	}

	info->dev = &pdev->dev;
	/* Setup timed_output obj */
	info->vibrator_timed_dev.name = "vibrator";
	info->vibrator_timed_dev.enable = vibrator_enable_set_timeout;
	info->vibrator_timed_dev.get_time = vibrator_get_remaining_time;
	/* Vibrator dev register in /sys/class/timed_output/ */
	ret = timed_output_dev_register(&info->vibrator_timed_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Vibrator: timed output dev register fail\n");
		return ret;
	}

	INIT_WORK(&info->vibrator_off_work, vibrator_off_worker);
	mutex_init(&info->vib_mutex);
	info->enable = 0;

	init_timer(&info->vibrate_timer);
	info->vibrate_timer.function = on_vibrate_timer_expired;
	info->vibrate_timer.data = (unsigned long)info;
	platform_set_drvdata(pdev, info);
	wake_lock_init(&vib_wake_lock, WAKE_LOCK_SUSPEND, "vib_present");
	return 0;
}

static int vibrator_remove(struct platform_device *pdev)
{
	struct pwm_vibrator_info *info;
	info = platform_get_drvdata(pdev);
	timed_output_dev_unregister(&info->vibrator_timed_dev);
	wake_lock_destroy(&vib_wake_lock);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id vibrator_dt_match[] = {
	{ .compatible = "marvell,pwm-vibrator" },
	{},
};
#endif

static struct platform_driver vibrator_driver = {
	.probe = vibrator_probe,
	.remove = vibrator_remove,
	.driver = {
		   .name = "pwm-vibrator",
		   .of_match_table = of_match_ptr(vibrator_dt_match),
		   .owner = THIS_MODULE,
		   },
};

static int __init vibrator_init(void)
{
	return platform_driver_register(&vibrator_driver);
}

static void __exit vibrator_exit(void)
{
	platform_driver_unregister(&vibrator_driver);
}

module_init(vibrator_init);
module_exit(vibrator_exit);

MODULE_DESCRIPTION("Android Vibrator driver");
MODULE_LICENSE("GPL");

