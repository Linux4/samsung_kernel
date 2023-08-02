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

#ifdef CONFIG_ARCH_SCX35
#define SPRD_ANA_BASE 	        (SPRD_MISC_BASE + 0x8800)
#else
#define SPRD_ANA_BASE 	        (SPRD_MISC_BASE + 0x600)
#endif
#define ANA_REG_BASE            SPRD_ANA_BASE

#ifdef CONFIG_ARCH_SC8825
#define ANA_LED_CTRL           (ANA_REG_BASE + 0X70)
#else
#ifdef CONFIG_ARCH_SCX15
#define ANA_LED_CTRL           (ANA_REG_BASE + 0XE0)
#else
#ifdef CONFIG_ARCH_SCX35
#define ANA_LED_CTRL           (ANA_REG_BASE + 0XA0)
#ifdef CONFIG_ARCH_SCX30G
#define CA_CTRL2          (ANA_REG_BASE + 0X158)
#define LDO_KPLED_PD       (1 << 8)
#define SLP_LDOKPLED_PD_EN       (1 << 9)
#endif
#else
#define ANA_LED_CTRL           (ANA_REG_BASE + 0X68)
#endif
#endif
#endif

#define KPLED_CTL               ANA_LED_CTRL
#ifdef CONFIG_ARCH_SCX15
#define KPLED_PD_SET              (1 << 0)
#define KPLED_V_SHIFT            4
#define KPLED_V_MSK               (0x0F << KPLED_V_SHIFT)
#else
#ifdef CONFIG_ARCH_SCX35
#define KPLED_PD_SET            (1 << 1)
#define KPLED_V_SHIFT           4
#define KPLED_V_MSK             (0x0F << KPLED_V_SHIFT)
#else
#define KPLED_PD_SET            (1 << 11)
#define KPLED_PD_RST            (1 << 12)
#define KPLED_V_SHIFT           7
#define KPLED_V_MSK             (0x07 << KPLED_V_SHIFT)
#endif
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
	
static void sprd_kpled_set_brightness( unsigned long  brightness)
{
	unsigned long brightness_level;
	brightness_level = brightness;

	if(brightness_level > 255)
		brightness_level = 255;
	
#ifdef CONFIG_ARCH_SCX35
	/*brightness steps = 16*/
	brightness_level = brightness_level/16;
	brightness_level = 0;//set brightness_level = 0 for reducing power consumption
#else
	/*brightness steps = 8*/
	brightness_level = brightness_level/32;
#endif
	
	// Set Output Current
	sci_adi_write(KPLED_CTL, ((brightness_level << KPLED_V_SHIFT) & KPLED_V_MSK), KPLED_V_MSK);
	PRINT_INFO("reg:0x%08X set_val:0x%08X  brightness:%ld  brightness_level:%ld(0~15)\n", \
		KPLED_CTL, kpled_read(KPLED_CTL), brightness, brightness_level);
}

static void sprd_kpled_enable(struct sprd_kpled *led)
{
#ifdef CONFIG_ARCH_SCX35
	sci_adi_clr(KPLED_CTL, KPLED_PD_SET);
	#ifdef CONFIG_ARCH_SCX30G
	sci_adi_set(CA_CTRL2, LDO_KPLED_PD);
	#endif
#else
	sci_adi_clr(KPLED_CTL, KPLED_PD_SET|KPLED_PD_RST);
	sci_adi_set(KPLED_CTL, KPLED_PD_RST);
#endif

	PRINT_INFO("sprd_kpled_enable\n");
	sprd_kpled_set_brightness(led->value);
	led->enabled = 1;
}

static void sprd_kpled_disable(struct sprd_kpled *led)
{
#ifdef CONFIG_ARCH_SCX35
	sci_adi_set(KPLED_CTL, KPLED_PD_SET);
	#ifdef CONFIG_ARCH_SCX30G
	sci_adi_clr(CA_CTRL2, LDO_KPLED_PD);
	#endif
#else
	sci_adi_clr(KPLED_CTL, KPLED_PD_SET|KPLED_PD_RST);
	sci_adi_set(KPLED_CTL, KPLED_PD_SET);
#endif

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
	
	spin_lock_irqsave(&led->value_lock, flags);
	led->value = value;
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


static int sprd_kpled_probe(struct platform_device *dev)
{
	struct sprd_kpled *led;
	int ret;

	led = kzalloc(sizeof(struct sprd_kpled), GFP_KERNEL);
	if (led == NULL) {
		dev_err(&dev->dev, "No memory for device\n");
		return -ENOMEM;
	}

	led->cdev.brightness_set = sprd_kpled_set;
	//led->cdev.default_trigger = "heartbeat";
	led->cdev.default_trigger = "none";
	led->cdev.name = "keyboard-backlight";
	led->cdev.brightness_get = NULL;
	led->cdev.flags |= LED_CORE_SUSPENDRESUME;
	led->enabled = 0;

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

#ifdef CONFIG_ARCH_SCX30G
	sci_adi_clr(CA_CTRL2, SLP_LDOKPLED_PD_EN);
#endif

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

static const struct of_device_id keyboard_backlight_of_match[] = {
	{ .compatible = "sprd,keyboard-backlight", },
	{ }
};

static struct platform_driver sprd_kpled_driver = {
	.driver = {
		.name  = "keyboard-backlight",
		.owner = THIS_MODULE,
		.of_match_table = keyboard_backlight_of_match,
	},
	.probe    = sprd_kpled_probe,
	.remove   = sprd_kpled_remove,
	.shutdown = sprd_kpled_shutdown,
};

static int __init sprd_kpled_init(void)
{
	return platform_driver_register(&sprd_kpled_driver);
}

static void sprd_kpled_exit(void)
{
	platform_driver_unregister(&sprd_kpled_driver);
}

module_init(sprd_kpled_init);
module_exit(sprd_kpled_exit);

MODULE_AUTHOR("Ye Wu <ye.wu@spreadtrum.com>");
MODULE_DESCRIPTION("Sprd Keyboard backlight driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:keyboard-backlight");

