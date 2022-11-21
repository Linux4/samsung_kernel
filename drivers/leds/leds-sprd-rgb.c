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

#define sprd_rgbled_DBG
#ifdef sprd_rgbled_DBG
#define ENTER printk(KERN_INFO "[sprd_rgbled_DBG] func: %s  line: %04d\n", __func__, __LINE__);
#define PRINT_DBG(x...)  printk(KERN_INFO "[sprd_rgbled_DBG] " x)
#define PRINT_INFO(x...)  printk(KERN_INFO "[sprd_rgbled_INFO] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[sprd_rgbled_WARN] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[sprd_rgbled_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#else
#define ENTER
#define PRINT_DBG(x...)
#define PRINT_INFO(x...)  printk(KERN_INFO "[sprd_rgbled_INFO] " x)
#define PRINT_WARN(x...)  printk(KERN_INFO "[sprd_rgbled_WARN] " x)
#define PRINT_ERR(format,x...)  printk(KERN_ERR "[sprd_rgbled_ERR] func: %s  line: %04d  info: " format, __func__, __LINE__, ## x)
#endif

#ifdef CONFIG_ARCH_SCX35
#define ANA_BTLC_BASE            (SPRD_MISC_BASE + 0x83c0)
#define ANA_BTLC_CTRL            (ANA_BTLC_BASE + 0x00)
#endif

enum sprd_led_type
{
	SPRD_LED_TYPE_R = 0,
	SPRD_LED_TYPE_G,
	SPRD_LED_TYPE_B,
	SPRD_LED_TYPE_TOTAL
};


static char * sprd_rgbled_name[SPRD_LED_TYPE_TOTAL] =
{
	"red",              
	"green",              
	"blue",
};


/* sprd rgb led */
struct sprd_rgbled {
	struct platform_device *dev;
	struct mutex mutex;
	struct work_struct work;
	spinlock_t value_lock;
	enum led_brightness value;
	struct led_classdev cdev;

	int suspend;

};

static struct sprd_rgbled *g_sprd_rgbled[SPRD_LED_TYPE_TOTAL];


#define to_sprd_rgbled(led_cdev) \
	container_of(led_cdev, struct sprd_rgbled, cdev)

static void sprd_rgbled_enable(struct sprd_rgbled *led)
{

#ifdef CONFIG_ARCH_SCX35
	if(strcmp(led->cdev.name,sprd_rgbled_name[SPRD_LED_TYPE_R]) == 0)
	{
		sci_adi_set(ANA_BTLC_CTRL, 0x00F);   
	}

	if(strcmp(led->cdev.name,sprd_rgbled_name[SPRD_LED_TYPE_G]) == 0)
	{
		sci_adi_set(ANA_BTLC_CTRL, 0x0F0); 
	}

	if(strcmp(led->cdev.name,sprd_rgbled_name[SPRD_LED_TYPE_B]) == 0)
	{
		sci_adi_set(ANA_BTLC_CTRL, 0xF00); 
	}
#endif

	PRINT_INFO("sprd_rgbled_enable\n");

}

static void sprd_rgbled_disable(struct sprd_rgbled *led)
{
#ifdef CONFIG_ARCH_SCX35

	if(strcmp(led->cdev.name,sprd_rgbled_name[SPRD_LED_TYPE_R]) == 0)
	{
		sci_adi_clr(ANA_BTLC_CTRL, 0x00F);   
	}

	if(strcmp(led->cdev.name,sprd_rgbled_name[SPRD_LED_TYPE_G]) == 0)
	{
		sci_adi_clr(ANA_BTLC_CTRL, 0x0F0); 
	}

	if(strcmp(led->cdev.name,sprd_rgbled_name[SPRD_LED_TYPE_B]) == 0)
	{
		sci_adi_clr(ANA_BTLC_CTRL, 0xF00); 
	}
#endif

	PRINT_INFO("sprd_rgbled_disable\n");
}

static void sprd_rgbled_work(struct work_struct *work)
{
	struct sprd_rgbled *led = container_of(work, struct sprd_rgbled, work);
	unsigned long flags;

	mutex_lock(&led->mutex);
	spin_lock_irqsave(&led->value_lock, flags);
	if (led->value == LED_OFF) {
		spin_unlock_irqrestore(&led->value_lock, flags);
		sprd_rgbled_disable(led);
		goto out;
	}
	spin_unlock_irqrestore(&led->value_lock, flags);
	sprd_rgbled_enable(led);
out:
	mutex_unlock(&led->mutex);
}

static void sprd_rgbled_set(struct led_classdev *led_cdev,
		enum led_brightness value)
{
	struct sprd_rgbled *led = to_sprd_rgbled(led_cdev);
	unsigned long flags;

	spin_lock_irqsave(&led->value_lock, flags);
	led->value = value;
	spin_unlock_irqrestore(&led->value_lock, flags);

	if(1 == led->suspend) {
		PRINT_WARN("Do NOT change brightness in suspend mode\n");
		return;
	}

	schedule_work(&led->work);	
}

static void sprd_rgbled_shutdown(struct platform_device *dev)
{
	int i;

	for (i = 0; i < SPRD_LED_TYPE_TOTAL; i++)
	{
		if (!g_sprd_rgbled[i])
			continue;

		mutex_lock(&g_sprd_rgbled[i]->mutex);
		sprd_rgbled_disable(g_sprd_rgbled[i]);
		mutex_unlock(&g_sprd_rgbled[i]->mutex);
	}
}


static int sprd_rgbled_probe(struct platform_device *dev)
{
	int i;
	int ret;

	for (i = 0; i < SPRD_LED_TYPE_TOTAL; i++) 
	{
		g_sprd_rgbled[i] = kzalloc(sizeof(struct sprd_rgbled), GFP_KERNEL);

		if (g_sprd_rgbled[i] == NULL) 
		{
			dev_err(&dev->dev, "No memory for device\n");
			ret = -ENOMEM;
			goto err;
		}

		g_sprd_rgbled[i]->cdev.brightness_set = sprd_rgbled_set;
		g_sprd_rgbled[i]->cdev.name = sprd_rgbled_name[i];
		g_sprd_rgbled[i]->cdev.brightness_get = NULL;
		//g_sprd_rgbled[i]->cdev.flags |= LED_CORE_SUSPENDRESUME;

		spin_lock_init(&g_sprd_rgbled[i]->value_lock);
		mutex_init(&g_sprd_rgbled[i]->mutex);
		INIT_WORK(&g_sprd_rgbled[i]->work, sprd_rgbled_work);
		g_sprd_rgbled[i]->value = LED_OFF;


		ret = led_classdev_register(&dev->dev, &g_sprd_rgbled[i]->cdev);
		if (ret < 0) 
		{
			goto err;
		}

		g_sprd_rgbled[i]->value = 15;//set default brightness
		g_sprd_rgbled[i]->suspend = 0;

	}

	return 0;
err:
	if (i) 
	{
		for (i = i-1; i >=0; i--) 
		{
			if (!g_sprd_rgbled[i])
				continue;
			led_classdev_unregister(&g_sprd_rgbled[i]->cdev);
			kfree(g_sprd_rgbled[i]);
			g_sprd_rgbled[i] = NULL;
		}
	}

	return ret;
}

static int sprd_rgbled_remove(struct platform_device *dev)
{
	int i;

	for (i = 0; i < SPRD_LED_TYPE_TOTAL; i++)
	{
		if (!g_sprd_rgbled[i])
			continue;

		led_classdev_unregister(&g_sprd_rgbled[i]->cdev);
		g_sprd_rgbled[i]->value = LED_OFF;
		sprd_rgbled_disable(g_sprd_rgbled[i]);
		cancel_work_sync(&g_sprd_rgbled[i]->work);
		kfree(g_sprd_rgbled[i]);
		g_sprd_rgbled[i] = NULL;
	}

	return 0;
}

static struct platform_driver sprd_rgbled_driver = {
	.driver = {
		.name  = "rgb-led",
		.owner = THIS_MODULE,
	},
	.probe    = sprd_rgbled_probe,
	.remove   = sprd_rgbled_remove,
	.shutdown = sprd_rgbled_shutdown,
};

module_platform_driver(sprd_rgbled_driver);

MODULE_AUTHOR("Lu Chai <lu.chai@spreadtrum.com>");
MODULE_DESCRIPTION("Sprd rgb led driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rgb-led");
