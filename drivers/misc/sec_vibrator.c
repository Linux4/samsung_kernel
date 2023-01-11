/* Copyright (C) 2015 Marvell */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/of.h>

#include "../staging/android/timed_output.h"

struct sec_vibrator_info {
	struct device *dev;
	struct regulator *vib_regulator;
	struct timed_output_dev vibrator_timed_dev;
	struct timer_list vibrate_timer;
	struct work_struct vibrator_off_work;
	struct mutex vib_mutex;
	int enable;
	int min_timeout;
};

#define VIBRA_OFF_VALUE	0
#define VIBRA_ON_VALUE		1

static void vibrator_ldo_control(struct sec_vibrator_info *info, bool onoff)
{
	int ret;
	info->vib_regulator = regulator_get(info->dev, "vibrator");
	if (IS_ERR(info->vib_regulator)) {
		dev_err(info->dev, "get vibrator ldo fail!\n");
		return;
	}

	if (onoff) {
		regulator_set_voltage(info->vib_regulator, 3000000, 3000000);
		ret = regulator_enable(info->vib_regulator);
		if (ret)
			dev_err(info->dev, "enable vibrator ldo fail!\n");
	} else {
		ret = regulator_disable(info->vib_regulator);
		if (ret)
			dev_err(info->dev, "disable vibrator ldo fail!\n");
	}
	regulator_put(info->vib_regulator);
}

static int get_vibrator_platdata(struct device_node *np,
					struct sec_vibrator_info *info)
{
	u32 value;
	int ret;
	ret = of_property_read_u32(np, "min_timeout", &value);
	if (ret < 0)
		printk(KERN_ERR"%s:vibrator min time out is not set\n", __func__);
	else
		info->min_timeout = (int)value;
	return 0;
}

int sec_control_vibrator(struct sec_vibrator_info *info,
				unsigned char value)
{
	mutex_lock(&info->vib_mutex);
	if (info->enable == value) {
		mutex_unlock(&info->vib_mutex);
		return 0;
	}

	if (value == VIBRA_OFF_VALUE) {
		vibrator_ldo_control(info, false);
	} else if (value == VIBRA_ON_VALUE) {
		vibrator_ldo_control(info, true);
	}
	info->enable = value;
	mutex_unlock(&info->vib_mutex);

	return 0;
}

static void vibrator_off_worker(struct work_struct *work)
{
	struct sec_vibrator_info *info;

	info = container_of(work, struct sec_vibrator_info,
				vibrator_off_work);
	sec_control_vibrator(info, VIBRA_OFF_VALUE);
}

static void on_vibrate_timer_expired(unsigned long x)
{
	struct sec_vibrator_info *info;
	info = (struct sec_vibrator_info *)x;
	schedule_work(&info->vibrator_off_work);
}

static void vibrator_enable_set_timeout(struct timed_output_dev *sdev,
					int timeout)
{
	struct sec_vibrator_info *info;
	info = container_of(sdev, struct sec_vibrator_info,
				vibrator_timed_dev);
	printk(KERN_DEBUG"Vibrator: Set duration: %dms\n", timeout);

	if (timeout <= 0) {
		sec_control_vibrator(info, VIBRA_OFF_VALUE);
		del_timer(&info->vibrate_timer);
	} else {
		if (info->min_timeout)
			timeout = (timeout < info->min_timeout) ?
				   info->min_timeout : timeout;

		sec_control_vibrator(info, VIBRA_ON_VALUE);
		mod_timer(&info->vibrate_timer,
			  jiffies + msecs_to_jiffies(timeout));
	}

	return;
}

static int vibrator_get_remaining_time(struct timed_output_dev *sdev)
{
	struct sec_vibrator_info *info;
	int rettime;

	info = container_of(sdev, struct sec_vibrator_info,
		vibrator_timed_dev);
	rettime = jiffies_to_msecs(jiffies - info->vibrate_timer.expires);
	printk(KERN_INFO"Vibrator: Current duration: %dms\n", rettime);
	return rettime;
}

static int vibrator_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct sec_vibrator_info *info;
	struct device_node *np = pdev->dev.of_node;
	
	info = kzalloc(sizeof(struct sec_vibrator_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	/*get vibrator platdata from dts file*/

	if ((np != NULL) && (info != NULL))
		get_vibrator_platdata(np, info);

	/* Setup timed_output obj */
	info->vibrator_timed_dev.name = "vibrator";
	info->vibrator_timed_dev.enable = vibrator_enable_set_timeout;
	info->vibrator_timed_dev.get_time = vibrator_get_remaining_time;

	/* Vibrator dev register in /sys/class/timed_output/ */
	ret = timed_output_dev_register(&info->vibrator_timed_dev);
	if (ret < 0) {
		dev_err(&pdev->dev,
			   "Vibrator: timed_output dev registration failure\n");
		timed_output_dev_unregister(&info->vibrator_timed_dev);
	}

	INIT_WORK(&info->vibrator_off_work, vibrator_off_worker);
	mutex_init(&info->vib_mutex);
	info->enable = 0;

	init_timer(&info->vibrate_timer);
	info->vibrate_timer.function = on_vibrate_timer_expired;
	info->vibrate_timer.data = (unsigned long)info;
	info->dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	return 0;
}

static int vibrator_remove(struct platform_device *pdev)
{
	struct sec_vibrator_info *info;
	info = platform_get_drvdata(pdev);

	del_timer(&info->vibrate_timer);
	cancel_work_sync(&info->vibrator_off_work);

	timed_output_dev_unregister(&info->vibrator_timed_dev);
	return 0;
}

static void vibrator_shutdown(struct platform_device *pdev)
{
	vibrator_remove(pdev);
	return;
}

static const struct of_device_id vibrator_dt_match[] = {
	{ .compatible = "marvell,sec-vibrator" },
	{},
};

static struct platform_driver vibrator_driver = {
	.probe = vibrator_probe,
	.remove = vibrator_remove,
	.shutdown = vibrator_shutdown,
	.driver = {
		.name = "sec-vibrator",
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
