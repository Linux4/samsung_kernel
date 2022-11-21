/*
 * linux/drivers/video/backlight/pwm_bl.c
 *
 * simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/lcd_mDNIe.h>
#define __BACKLIGHT_DEBUG__   0

#define BACKLIGHT_DEV_NAME	"panel"

int BL_brightness;
extern int is_poweron;
int current_brightness;
extern int wakeup_brightness;

#define GPIO_BL_CTRL	9

#define MAX_BRIGHTNESS	255
#define MIN_BRIGHTNESS	20
#define DEFAULT_BRIGHTNESS 140
#define DEFAULT_PULSE 18
#define DIMMING_VALUE	31
#define MAX_BRIGHTNESS_IN_BLU	32 // backlight-IC MAX VALUE

static int lcd_brightness = DEFAULT_PULSE;

struct brt_value{
	int level;				// Platform setting values
	int tune_level;			// Chip Setting values
};

struct class *lcd_class;
struct class *lcd_mDNIe_class;
struct class *lcd_mDNIeTuning_class;

static DEFINE_SPINLOCK(bl_ctrl_lock);

struct brt_value brt_table_ktd[] = {
	{ 255,	3  }, /* Max */
	{ 245,	5 },
	{ 235,	7 },
	{ 225,	9 },
	{ 215,	11 },
	{ 200,	13 },
	{ 185,	15 },
	{ 170,	16 },
	{ 155,	17 },
	{ 140,	18 }, /* default */
	{ 125,	20 },
	{ 110,	22 },
	{ 95,	24 },
	{ 80,	26 },
	{ 70,	27 },
	{ 60,	28 },
	{ 50,	29 },
	{ 40,	30 },
	{ 30,	31 }, /* Min */
	{ 20,	31 }, /* Dimming */
	{ 0,	32 }, /* Off */
};

#define NB_BRT_LEVEL (int)(sizeof(brt_table_ktd)/sizeof(struct brt_value))


void ktd_backlight_set_brightness(int level)
{
	int pulse;
	int tune_level = 0;
	int i;

	spin_lock(&bl_ctrl_lock);
	if (level > 0) {
		if (level < MIN_BRIGHTNESS) {
			tune_level = DIMMING_VALUE; /* DIMMING */
		} else {
			for (i = 0; i < NB_BRT_LEVEL; i++) {
				if (level <= brt_table_ktd[i].level
					&& level > brt_table_ktd[i+1].level) {
					tune_level = brt_table_ktd[i].tune_level;
					break;
				}
			}
		}
	} /*  BACKLIGHT is KTD model */
	printk("set_brightness : level(%d) tune (%d)\n",level, tune_level);
    current_brightness = level;
    
	if (!tune_level) {
		gpio_set_value(GPIO_BL_CTRL, 0);
		mdelay(3);
		lcd_brightness = tune_level;
	} else {
		if (unlikely(lcd_brightness < 0)) {
			int val = gpio_get_value(GPIO_BL_CTRL);
			if (val) {
				lcd_brightness = 0;
			gpio_set_value(GPIO_BL_CTRL, 0);
			mdelay(3);
				printk(KERN_INFO "LCD Baklight init in boot time on kernel\n");
			}
		}
		if (!lcd_brightness) {
			gpio_set_value(GPIO_BL_CTRL, 1);
			udelay(3);
			lcd_brightness = MAX_BRIGHTNESS_IN_BLU;
		}

		pulse = (tune_level - lcd_brightness + MAX_BRIGHTNESS_IN_BLU)
						% MAX_BRIGHTNESS_IN_BLU;

		for (; pulse > 0; pulse--) {
			gpio_set_value(GPIO_BL_CTRL, 0);
			udelay(3);
			gpio_set_value(GPIO_BL_CTRL, 1);
			udelay(3);
		}

		lcd_brightness = tune_level;
	}
	mdelay(1);
	spin_unlock(&bl_ctrl_lock);
	return 0;
}

static int ktd_backlight_update_status(struct backlight_device *bl)
{
    struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
    int brightness = bl->props.brightness;
    int max = bl->props.max_brightness;

    if (bl->props.power != FB_BLANK_UNBLANK)
        brightness = 0;

    if (bl->props.fb_blank != FB_BLANK_UNBLANK)
        brightness = 0;

    wakeup_brightness = brightness;
    if(is_poweron == 0 && current_brightness == 0)
    {
		printk(KERN_INFO "[Backlight] no need to set backlight ---\n");
    }
    else
    {
        ktd_backlight_set_brightness(brightness);
    }

    return 0;
}

static int ktd_backlight_get_brightness(struct backlight_device *bl)
{
	BL_brightness = bl->props.brightness;
	return BL_brightness;
}

static const struct backlight_ops ktd_backlight_ops = {
	.update_status	= ktd_backlight_update_status,
	.get_brightness	= ktd_backlight_get_brightness,
};

 ssize_t lcd_panelName_show(struct device *dev, struct device_attribute *attr, char *buf)
{
//    printk("lcd_panelName_show: %s\n", PANEL_NAME_HYDIS);

//    return sprintf(buf, "%s", PANEL_NAME_HYDIS);
    return sprintf(buf, "H430VAN01");
}

  ssize_t lcd_MTP_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    ((unsigned char*)buf)[0] = 0x83;
    ((unsigned char*)buf)[1] = 0x69;
    ((unsigned char*)buf)[2] = 0x5A;    

    printk("ldi mtpdata: %x %x %x\n", buf[0], buf[1], buf[2]);

    return 3;
}

static DEVICE_ATTR(lcd_MTP, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_MTP_show, NULL);
static DEVICE_ATTR(lcd_panelName, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_panelName_show, NULL);
static DEVICE_ATTR(tuning, 0664, mDNIeTuning_show, mDNIeTuning_store);
static DEVICE_ATTR(scenario, 0664, mDNIeScenario_show, mDNIeScenario_store);
static DEVICE_ATTR(outdoor, 0664, mDNIeOutdoor_show, mDNIeOutdoor_store);
static DEVICE_ATTR(negative, 0664, mDNIeNegative_show, mDNIeNegative_store);

static int ktd_backlight_probe(struct platform_device *pdev)
{
	struct backlight_device *bl;
       struct backlight_properties props;
	unsigned char value;
	int ret;
	struct device *dev_t;

	printk("[coko] %s\n",__FUNCTION__);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = MAX_BRIGHTNESS;
       props.type = BACKLIGHT_RAW;
       
	bl = backlight_device_register(BACKLIGHT_DEV_NAME, &pdev->dev, NULL,
					&ktd_backlight_ops, &props);
 
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		return PTR_ERR(bl);
	}
	
	bl->props.max_brightness = MAX_BRIGHTNESS;
	bl->props.brightness = DEFAULT_BRIGHTNESS;

	platform_set_drvdata(pdev, bl);
    
        if(gpio_request(GPIO_BL_CTRL,"BL_CTRL"))
        {
        	printk(KERN_ERR "Request GPIO failed,""gpio: %d \n", GPIO_BL_CTRL);
        }

    	lcd_class = class_create(THIS_MODULE, "lcd_status");

    	if (IS_ERR(lcd_class)) 
    	{
    		printk("Failed to create lcd_class!\n");
    		return PTR_ERR( lcd_class );
    	}

    	dev_t = device_create( lcd_class, NULL, 0, "%s", "lcd_status");

        if (device_create_file(dev_t, &dev_attr_lcd_panelName) < 0)
            pr_err("Failed to create device file(%s)!\n", dev_attr_lcd_panelName.attr.name);
       if (device_create_file(dev_t, &dev_attr_lcd_MTP) < 0)
           pr_err("Failed to create device file(%s)!\n", dev_attr_lcd_MTP.attr.name);

    	lcd_mDNIeTuning_class = class_create(THIS_MODULE, "lcd");

    	if (IS_ERR(lcd_mDNIeTuning_class)) 
    	{
    		printk("Failed to create mdnie_tuning!\n");
    		return PTR_ERR( lcd_mDNIeTuning_class );
    	}

    	dev_t = device_create( lcd_mDNIeTuning_class, NULL, 0, "%s", "panel");

        if (device_create_file(dev_t, &dev_attr_tuning) < 0)
            pr_err("Failed to create device file(%s)!\n", dev_attr_tuning.attr.name);

    	lcd_mDNIe_class = class_create(THIS_MODULE, "mdnie");

    	if (IS_ERR(lcd_mDNIe_class)) 
    	{
    		printk("Failed to create mdnie!\n");
    		return PTR_ERR( lcd_mDNIe_class );
    	}

    	dev_t = device_create( lcd_mDNIe_class, NULL, 0, "%s", "mdnie");
    
        if (device_create_file(dev_t, &dev_attr_scenario) < 0)
            pr_err("Failed to create device file(%s)!\n", dev_attr_scenario.attr.name);
        if (device_create_file(dev_t, &dev_attr_outdoor) < 0)
            pr_err("Failed to create device file(%s)!\n", dev_attr_outdoor.attr.name);
        if (device_create_file(dev_t, &dev_attr_negative) < 0)
            pr_err("Failed to create device file(%s)!\n", dev_attr_negative.attr.name);        
  
	backlight_update_status(bl);
	printk("[coko] %s end\n",__FUNCTION__);
	return 0;
out:
	return ret;
}

static int ktd_backlight_remove(struct platform_device *pdev)
{
    struct backlight_device *bl = platform_get_drvdata(pdev);
    backlight_device_unregister(bl);
    gpio_direction_output(GPIO_BL_CTRL, 0);
    mdelay(3);

    return 0;
}

void ktd_backlight_shutdown(struct platform_device *pdev)
{
    printk("[coko] %s\n",__FUNCTION__);    
    gpio_direction_output(GPIO_BL_CTRL, 0);
    mdelay(5);
}

static struct platform_driver ktd_backlight_driver = {
	.driver		= {
		.name	= BACKLIGHT_DEV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= ktd_backlight_probe,
	.remove		= ktd_backlight_remove,
	.shutdown       = ktd_backlight_shutdown,
};

static int __init ktd_backlight_init(void)
{
	return platform_driver_register(&ktd_backlight_driver);
}
module_init(ktd_backlight_init);

static void __exit ktd_backlight_exit(void)
{
	platform_driver_unregister(&ktd_backlight_driver);
}
module_exit(ktd_backlight_exit);

MODULE_DESCRIPTION("KTD based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ktd-backlight");



