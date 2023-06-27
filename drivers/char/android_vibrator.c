/* Copyright (C) 2010 Marvell */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <plat/mfp.h>
#include <linux/regulator/machine.h>
#include <linux/wakelock.h>
#include <mach/mfp-pxa1088-delos.h>

#include "../staging/android/timed_output.h"

#ifdef CONFIG_MACH_BAFFIN
#define MOTOR_USE_LDO_POWER
#endif

static struct timed_output_dev vibrator_timed_dev;
static struct timer_list	vibrate_timer;
static struct work_struct	vibrator_off_work;
static struct wake_lock vib_wake_lock;

#if defined(MOTOR_USE_LDO_POWER)	
static struct regulator *vib_avdd = NULL;
#else
#include <linux/vibrator.h>
int motor_en;
#endif

static void vibrator_on_off(int On)
{
#if defined(MOTOR_USE_LDO_POWER)
    if(vib_avdd == NULL)
        return;
	 
    if(On)
    {
         if(regulator_is_enabled(vib_avdd) == 0)
        {
            regulator_set_voltage(vib_avdd, 3300000, 3300000);
            regulator_enable(vib_avdd);
         }
    }
    else
    {
        if(regulator_is_enabled(vib_avdd) > 0)
        {
            regulator_disable(vib_avdd);
        }
    }
#else
	if(On)
	{
		gpio_direction_output(motor_en, 1);
		//mdelay(1);
	}
	else
		gpio_direction_output(motor_en, 0);

#endif
}

static void vibrator_off_worker(struct work_struct *work)
{
	printk("%s++\n",__func__);
	vibrator_on_off(0);
	if(wake_lock_active(&vib_wake_lock))
		wake_unlock(&vib_wake_lock);
	printk("%s--\n",__func__);
}

static void on_vibrate_timer_expired(unsigned long x)
{
	schedule_work(&vibrator_off_work);
}

static void vibrator_enable_set_timeout(struct timed_output_dev *sdev,
	int timeout)
{
	printk(KERN_NOTICE "Vibrator: Set duration: %dms\n", timeout);
	if(timeout == 0)
	{
		vibrator_on_off(0);
		if(wake_lock_active(&vib_wake_lock))
			wake_unlock(&vib_wake_lock);
	}
	else
	{
		cancel_work_sync(&vibrator_off_work);
		vibrator_on_off(1);
		if(!wake_lock_active(&vib_wake_lock))
			wake_lock(&vib_wake_lock);
		mod_timer(&vibrate_timer, jiffies + msecs_to_jiffies(timeout));
	}
	return;
}

static int vibrator_get_remaining_time(struct timed_output_dev *sdev)
{
	int retTime = jiffies_to_msecs(jiffies-vibrate_timer.expires);
	printk(KERN_NOTICE "Vibrator: Current duration: %dms\n", retTime);
	return retTime;
}

static int vibrator_probe(struct platform_device *pdev)
{
	int ret = 0;

#if defined(MOTOR_USE_LDO_POWER)
	if (!vib_avdd) {
		vib_avdd = regulator_get(NULL, "v_motor_3v3");
		if (IS_ERR(vib_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			vib_avdd = NULL;
			return 0;
		}
	}
#else
	struct vib_info *pdata = pdev->dev.platform_data;
	motor_en = pdata->gpio;
	if (gpio_request(motor_en, "MOT_EN")) {
	  pr_err("Vibrator: Request GPIO failed, gpio: %d\n", motor_en);
	}
 #endif 
	/* Setup timed_output obj */
	vibrator_timed_dev.name = "vibrator";
	vibrator_timed_dev.enable = vibrator_enable_set_timeout;
	vibrator_timed_dev.get_time = vibrator_get_remaining_time;
	/* Vibrator dev register in /sys/class/timed_output/ */
	ret = timed_output_dev_register(&vibrator_timed_dev);
	if (ret < 0) {
		printk(KERN_ERR "Vibrator: timed_output dev registration failure\n");
		timed_output_dev_unregister(&vibrator_timed_dev);
	}

	init_timer(&vibrate_timer);
	vibrate_timer.function = on_vibrate_timer_expired;
	vibrate_timer.data = (unsigned long)NULL;
	INIT_WORK(&vibrator_off_work,
		 vibrator_off_worker);
	wake_lock_init(&vib_wake_lock, WAKE_LOCK_SUSPEND, "vib_present");
	return 0;
}

static int __devexit vibrator_remove(struct platform_device *pdev)
{
	timed_output_dev_unregister(&vibrator_timed_dev);
	wake_lock_destroy(&vib_wake_lock);
	return 0;
}

static struct platform_driver vibrator_driver = {
	.probe = vibrator_probe,
	.remove = __devexit_p(vibrator_remove),
	.driver = {
		   .name = "android-vibrator",
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
