/*
* Copyright (C) 2012 Spreadtrum Communications Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/adc.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/of.h>
#include <linux/leds-sprd-kpled-2723.h>
//#define SPRD_KPLED_DBG
#ifdef SPRD_KPLED_DBG
#define ENTER printk(KERN_INFO "[SPRD_KPLED_DBG] func: %s  line: %04d\n", __func__, __LINE__);
#define PRINT_DBG(x...)  printk(KERN_INFO "[SPRD_KPLED_DBG] " x)
#define PRINT_INFO(x...)  printk(KERN_INFO "[SPRD_KPLED_INFO] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[SPRD_KPLED_WARN] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[SPRD_KPLED_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#else
#define ENTER
#define PRINT_DBG(x...)
#define PRINT_INFO(x...)  printk(KERN_INFO "[SPRD_KPLED_INFO] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[SPRD_KPLED_WARN] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[SPRD_KPLED_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#endif

#define SPRD_ANA_BASE 	        (SPRD_MISC_BASE + 0x8800)

#define ANA_REG_BASE            SPRD_ANA_BASE

#define ANA_LED_CTRL           (ANA_REG_BASE + 0xF4)

#define KPLED_CTL               ANA_LED_CTRL

#define KPLED_V_SHIFT           12
#define KPLED_V_MSK             (0x0F << KPLED_V_SHIFT)
#define LDO_KPLED_PD			(1 << 8)
#define LDO_KPLED_V_SHIFT		(0x00)
#define LDO_KPLED_V_MSK			(0xff)
#define KPLED_PD            	(1 << 11)
#define KPLED_PULLDOWN_EN		(1 << 10)
#if defined(CONFIG_MACH_GRANDNEOVE3G)
#define BRIGHTNESS_MULTIPLIER 255
#else
#define BRIGHTNESS_MULTIPLIER 1
#endif
/* sprd keypad backlight */
struct sprd_kpled {
        struct platform_device *dev;
        struct mutex mutex;
        struct work_struct work;
        spinlock_t value_lock;
        enum led_brightness value;
        struct led_classdev cdev;
        int enabled;
		int brightness_max;
		int brightness_min;
		int run_mode;
#ifdef CONFIG_HAS_EARLYSUSPEND
        struct early_suspend	early_suspend;
#endif

};

#define to_sprd_led(led_cdev) \
	container_of(led_cdev, struct sprd_kpled, cdev)

static inline uint32_t kpled_read(uint32_t reg)
{
        return sci_adi_read(reg);
}

static void sprd_kpled_set_brightness(struct sprd_kpled *led)
{
		unsigned long brightness = led->value;
		unsigned long brightness_level;
        brightness_level = brightness;

		PRINT_INFO("sprd_kpled_set_brightness:led->run_mode = %d\n",led->run_mode);
        if(brightness_level > 255)
                brightness_level = 255;

        /*brightness steps = 16*/
	if(led->run_mode == 1){
	        brightness_level = brightness_level/16;
	        brightness_level = 0;//set brightness_level = 0 for reducing power consumption
	        sci_adi_write(KPLED_CTL, ((brightness_level << KPLED_V_SHIFT) & KPLED_V_MSK), KPLED_V_MSK);
		}
	else
        sci_adi_write(KPLED_CTL, ((brightness_level << LDO_KPLED_V_SHIFT) & LDO_KPLED_V_MSK), LDO_KPLED_V_MSK);
		PRINT_INFO("reg:0x%08X set_val:0x%08X  brightness:%ld  brightness_level:%ld(0~15)\n", \
                   KPLED_CTL, kpled_read(KPLED_CTL), brightness, brightness_level);
}

static void sprd_kpled_enable(struct sprd_kpled *led)
{
	if(led->run_mode == 1){
	        sci_adi_clr(KPLED_CTL, KPLED_PD);
	        sci_adi_clr(KPLED_CTL, KPLED_PULLDOWN_EN);
			sci_adi_set(KPLED_CTL, LDO_KPLED_PD);
		}
	else{
	        sci_adi_set(KPLED_CTL, KPLED_PD);
	        sci_adi_set(KPLED_CTL, KPLED_PULLDOWN_EN);
			sci_adi_clr(KPLED_CTL, LDO_KPLED_PD);
		}
        PRINT_INFO("sprd_kpled_enable\n");
        sprd_kpled_set_brightness(led);
        led->enabled = 1;
}

static void sprd_kpled_disable(struct sprd_kpled *led)
{
	if(led->run_mode == 1){
	        sci_adi_set(KPLED_CTL, KPLED_PD);
	        sci_adi_set(KPLED_CTL, KPLED_PULLDOWN_EN);
			sci_adi_clr(KPLED_CTL, LDO_KPLED_PD);
		}
	else{
	        sci_adi_clr(KPLED_CTL, KPLED_PD);
	        sci_adi_clr(KPLED_CTL, KPLED_PULLDOWN_EN);
			sci_adi_set(KPLED_CTL, LDO_KPLED_PD);
		}
        PRINT_INFO("sprd_kpled_disable\n");
        led->enabled = 0;
}

static void sprd_kpled_work(struct work_struct *work)
{
        struct sprd_kpled *led = container_of(work, struct sprd_kpled, work);
        unsigned long flags;

        mutex_lock(&led->mutex);
        spin_lock_irqsave(&led->value_lock, flags);
        if (led->value == LED_OFF) {
                spin_unlock_irqrestore(&led->value_lock, flags);
                sprd_kpled_disable(led);
                goto out;
        }
        spin_unlock_irqrestore(&led->value_lock, flags);
        sprd_kpled_enable(led);
out:
        mutex_unlock(&led->mutex);
}

static void sprd_kpled_set(struct led_classdev *led_cdev,
                           enum led_brightness value)
{
        struct sprd_kpled *led = to_sprd_led(led_cdev);
        unsigned long flags;

        PRINT_INFO("sprd_kpled_set!\n");
        spin_lock_irqsave(&led->value_lock, flags);
        led->value = value * BRIGHTNESS_MULTIPLIER;
        spin_unlock_irqrestore(&led->value_lock, flags);

        schedule_work(&led->work);
}

static void sprd_kpled_shutdown(struct platform_device *dev)
{
        struct sprd_kpled *led = platform_get_drvdata(dev);

        mutex_lock(&led->mutex);
        sprd_kpled_disable(led);
        mutex_unlock(&led->mutex);
}

#ifdef CONFIG_EARLYSUSPEND
static void sprd_kpled_early_suspend(struct early_suspend *es)
{
        struct sprd_kpled *led = container_of(es, struct sprd_kpled, early_suspend);
        PRINT_INFO("sprd_kpled_early_suspend\n");
}

static void sprd_kpled_late_resume(struct early_suspend *es)
{
        struct sprd_kpled *led = container_of(es, struct sprd_kpled, early_suspend);
        PRINT_INFO("sprd_kpled_late_resume\n");
}
#endif

#ifdef CONFIG_OF
static struct sprd_kpled_2723_platform_data *sprd_kpled_2723_parse_dt(struct platform_device *pdev)
{
	int ret;
	struct device_node *np = pdev->dev.of_node;
	struct sprd_kpled_2723_platform_data *pdata = NULL;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(pdev, "sprd_kpled Could not allocate pdata");
		return NULL;
	}
	ret = of_property_read_u32(np, "brightness_max", &pdata->brightness_max);
	if(ret){
		dev_err(pdev, "fail to get pdata->brightness_max\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "brightness_min", &pdata->brightness_min);
	if(ret){
		dev_err(pdev, "fail to get pdata->brightness_min\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "run_mode", &pdata->run_mode);
	if(ret){
		dev_err(pdev, "fail to get pdata->run_mode\n");
		goto fail;
	}

	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif

static int sprd_kpled_probe(struct platform_device *dev)
{
        struct sprd_kpled *led;
        int ret;
		struct sprd_kpled_2723_platform_data *pdata = NULL;

	#ifdef CONFIG_OF

        struct device_node *np = dev->dev.of_node;

        if(np) {
                pdata = sprd_kpled_2723_parse_dt(dev);
                if (pdata == NULL) {
                        PRINT_ERR("get dts data failed!\n");
                        return -ENODEV;
                }
        } else {
                PRINT_ERR("dev.of_node is NULL!\n");
                return -ENODEV;
        }
	#else
		pdata = dev->dev.platform_data;
        if (!pdata){
                PRINT_ERR("No kpled_platform data!\n");
                return -ENODEV;
        }
	#endif

        led = kzalloc(sizeof(struct sprd_kpled), GFP_KERNEL);
        if (led == NULL) {
                dev_err(&dev->dev, "No memory for device\n");
                return -ENOMEM;
        }

        led->cdev.brightness_set = sprd_kpled_set;
        //led->cdev.default_trigger = "heartbeat";
        led->cdev.default_trigger = "none";
        //led->cdev.name = "keyboard-backlight";
        led->cdev.name = "button-backlight";
        led->cdev.brightness_get = NULL;
        led->cdev.flags |= LED_CORE_SUSPENDRESUME;
        led->enabled = 0;
		led->brightness_max = pdata->brightness_max;
		led->brightness_min = pdata->brightness_min;
		led->run_mode = pdata->run_mode;
		PRINT_INFO("led->run_mode = %d\n",led->run_mode);
		PRINT_INFO("led->brightness_max = %d\n",led->brightness_max);

        spin_lock_init(&led->value_lock);
        mutex_init(&led->mutex);
        INIT_WORK(&led->work, sprd_kpled_work);
        led->value = LED_OFF;
        platform_set_drvdata(dev, led);

        /* register our new led device */

        ret = led_classdev_register(&dev->dev, &led->cdev);
        if (ret < 0) {
                dev_err(&dev->dev, "led_classdev_register failed\n");
                kfree(led);
                return ret;
        }

#ifdef CONFIG_HAS_EARLYSUSPEND
        led->early_suspend.suspend = sprd_kpled_early_suspend;
        led->early_suspend.resume  = sprd_kpled_late_resume;
        led->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
        register_early_suspend(&led->early_suspend);
#endif

        sprd_kpled_disable(led);//disabled by default

        return 0;
}

static int sprd_kpled_remove(struct platform_device *dev)
{
        struct sprd_kpled *led = platform_get_drvdata(dev);

        led_classdev_unregister(&led->cdev);
        flush_scheduled_work();
        led->value = LED_OFF;
        led->enabled = 1;
        sprd_kpled_disable(led);
        kfree(led);

        return 0;
}

static const struct of_device_id kpled_2723_of_match[] = {
        { .compatible = "sprd,sprd-kpled-2723", },
        { }
};

static struct platform_driver sprd_kpled_2723_driver = {
        .driver = {
                .name  = "sprd-kpled-2723",
                .owner = THIS_MODULE,
                .of_match_table = kpled_2723_of_match,
        },
        .probe    = sprd_kpled_probe,
        .remove   = sprd_kpled_remove,
        .shutdown = sprd_kpled_shutdown,
};

static int __init sprd_kpled_2723_init(void)
{
		PRINT_INFO("Locate in sprd_kpled_2723_init!\n");
        return platform_driver_register(&sprd_kpled_2723_driver);
}

static void sprd_kpled_2723_exit(void)
{
        platform_driver_unregister(&sprd_kpled_2723_driver);
}

module_init(sprd_kpled_2723_init);
module_exit(sprd_kpled_2723_exit);

MODULE_AUTHOR("ya huang <ya.huang@spreadtrum.com>");
MODULE_DESCRIPTION("Sprd Keyboard backlight driver for 2723s");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sprd_kpled_2723");


