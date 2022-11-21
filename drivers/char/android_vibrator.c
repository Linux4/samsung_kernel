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
#include "../staging/android/timed_output.h"

#if defined(CONFIG_MACH_LT02)
#include <mach/mfp-pxa986-lt02.h>
#include <mach/pxa988.h>
#elif defined(CONFIG_MACH_COCOA7)
#include <mach/mfp-pxa986-cocoa7.h>
#include <mach/pxa988.h>
#endif

static struct timed_output_dev vibrator_timed_dev;
static struct timer_list	vibrate_timer;
static struct work_struct	vibrator_off_work;
static struct regulator *vib_avdd = NULL;

static void vibrator_on_off(int On)
{
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
}

static void vibrator_off_worker(struct work_struct *work)
{
	vibrator_on_off(0);
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
       }
       else
       {
            cancel_work_sync(&vibrator_off_work);
            vibrator_on_off(1);
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
    
	if (!vib_avdd) {
		vib_avdd = regulator_get(NULL, "v_motor_3v");
		if (IS_ERR(vib_avdd)) {
			pr_err("%s regulator get error!\n", __func__);
			vib_avdd = NULL;
			return 0;
		}
	}
    
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
	return 0;
}

static int __devexit vibrator_remove(struct platform_device *pdev)
{
	timed_output_dev_unregister(&vibrator_timed_dev);
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
#ifdef CONFIG_MACH_LT02
	if (system_rev < LT02_R0_1)
		return 0;
	else
#elif defined(CONFIG_MACH_COCOA7)
	// if (system_rev < COCOA7_R0_1)
		return 0;
	// else
#endif
	return platform_driver_register(&vibrator_driver);
}

static void __exit vibrator_exit(void)
{
#ifdef CONFIG_MACH_LT02
	if (system_rev < LT02_R0_1)
		return;
	else
#elif defined(CONFIG_MACH_COCOA7)
	// if (system_rev < COCOA7_R0_1)
		return;
	// else
#endif
	platform_driver_unregister(&vibrator_driver);
}

module_init(vibrator_init);
module_exit(vibrator_exit);

MODULE_DESCRIPTION("Android Vibrator driver");
MODULE_LICENSE("GPL");
