
/*******************************************************************************
*  Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* 	@file	drivers/video/backlight/ktd3102_bl.c
*
* Unless you and Broadcom execute a separate written software license agreement
* governing use of this software, this software is licensed to you under the
* terms of the GNU General Public License version 2, available at
* http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/ktd3102_bl.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <../sprdfb/sprdfb.h>

#include <linux/of.h>

#include <mach/board.h>

int current_intensity;
static int backlight_pin = 214;

static DEFINE_SPINLOCK(bl_ctrl_lock);

#define START_BRIGHTNESS 0 /*This value should be same as bootloader*/

int real_level = START_BRIGHTNESS;
EXPORT_SYMBOL(real_level);
#ifdef CONFIG_HAS_EARLYSUSPEND

#endif
static int backlight_mode=1;
#define MAX_BRIGHTNESS_IN_BLU	33
#define DIMMING_VALUE		31
#define MAX_BRIGHTNESS_VALUE	255
#define MIN_BRIGHTNESS_VALUE	20
#define BACKLIGHT_DEBUG 1
#define BACKLIGHT_SUSPEND 0
#define BACKLIGHT_RESUME 1
#if BACKLIGHT_DEBUG
#define printk(fmt, args...) printk(fmt, ## args)
#else
#define printk(fmt, args...)
#endif
struct ktd3102_bl_data {
	struct platform_device *pdev;
	unsigned int ctrl_pin;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_desc;
#endif
};
struct brt_value{
	int level;				// Platform setting values
	int tune_level;			// Chip Setting values
};
struct brt_value brt_table_ktd[] = {
  { MIN_BRIGHTNESS_VALUE,  31 },  /* Min pulse 32 */
   { 28,  31 },
   { 39,  30 },
   { 50,  29 },
   { 61,  28 },  
   { 72,  27 }, 
   { 83,  26 }, 
   { 94,  25 }, 
   { 105,  24 }, 
   { 116,  23 }, 
   { 127,  22 }, 
   { 138,  21 }, 
   { 149,  20 }, 
   { 160,  19 }, 
   { 171,  18 }, 
   { 182,  17 },   
   { 193,  16 },
   { 204,  15 }, 
   { 215,  14 }, 
   { 226,  13 },
   { 237,  12 },
   { 248,  12 },
   { MAX_BRIGHTNESS_VALUE,  11 },
};
#define MAX_BRT_STAGE_KTD (int)(sizeof(brt_table_ktd)/sizeof(struct brt_value))

struct mutex ktd3102_mutex;
DEFINE_MUTEX(ktd3102_mutex);

static void lcd_backlight_control(int num)
{
    int limit;
    
    limit = num;
    printk("[BACKLIGHT] lcd_backlight_control ==> pulse  : %d\n", num);
   spin_lock(&bl_ctrl_lock);
    for(;limit>0;limit--)
    {
       udelay(2);
       gpio_set_value(backlight_pin,0);
       udelay(2); 
       gpio_set_value(backlight_pin,1);
    }
   spin_unlock(&bl_ctrl_lock);
}
/* input: intensity in percentage 0% - 100% */
static int ktd3102_backlight_update_status(struct backlight_device *bd)
{
	int user_intensity = bd->props.brightness;
    int tune_level = 0;
	int pulse;
    int i;
    printk("[BACKLIGHT] ktd3102_backlight_update_status ==> user_intensity  : %d\n", user_intensity);

	if (bd->props.power != FB_BLANK_UNBLANK)
		user_intensity = 0;

	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		user_intensity = 0;

	if (bd->props.state & BL_CORE_SUSPENDED)
		user_intensity = 0;

	mutex_lock(&ktd3102_mutex);
	if(backlight_mode==BACKLIGHT_RESUME){
		if(user_intensity > 0) {
			if(user_intensity < MIN_BRIGHTNESS_VALUE) {
				tune_level = DIMMING_VALUE;
			} else if (user_intensity == MAX_BRIGHTNESS_VALUE) {
				tune_level =
				    brt_table_ktd[MAX_BRT_STAGE_KTD -
						  1].tune_level;
			} else {
				for(i = 0; i < MAX_BRT_STAGE_KTD; i++) {
					if (user_intensity <=
					    brt_table_ktd[i].level) {
						tune_level =
						    brt_table_ktd[i].tune_level;
						break;
					}
				}
			}
		}
		printk
		    ("[BACKLIGHT] ktd3102_backlight_update_status ==> user_intensity  : %d, tune_level : %d\n",
		     user_intensity, tune_level);
		if (real_level == tune_level) {
			mutex_unlock(&ktd3102_mutex);
			return 0;
		} else {
			if (tune_level <= 0) {
				gpio_set_value(backlight_pin,0);
				mdelay(3);
				real_level = 0;
			} else {
				if (real_level == 0) {
					gpio_set_value(backlight_pin, 1);
					printk("[BACKLIGHT] %s -> Control Pin Enable\n",__func__);
					udelay(100);

			}
				if (real_level <= tune_level) {
					pulse = tune_level - real_level;
				} else {
					pulse = 32 - (real_level - tune_level);
				}
				if (pulse == 0) {
					mutex_unlock(&ktd3102_mutex);
					return 0;
				}
				lcd_backlight_control(pulse);
			}
			real_level = tune_level;
		}
	}
	mutex_unlock(&ktd3102_mutex);

       return 0;
}
static int ktd3102_backlight_get_brightness(struct backlight_device *bl)
{
     printk("[BACKLIGHT] ktd3102_backlight_get_brightness\n");

	return current_intensity;
}
static struct backlight_ops ktd3102_backlight_ops = {
	.update_status	= ktd3102_backlight_update_status,
	.get_brightness	= ktd3102_backlight_get_brightness,
};
#ifdef CONFIG_HAS_EARLYSUSPEND
static void ktd3102_backlight_earlysuspend(struct early_suspend *desc)
{
    struct timespec ts;
    struct rtc_time tm;
#ifndef CONFIG_FB_BL_EVENT_CTRL
    backlight_mode=BACKLIGHT_SUSPEND;
#endif
     mutex_lock(&ktd3102_mutex);
       gpio_set_value(backlight_pin,0);
       mdelay(3);
	real_level=0;
    mutex_unlock(&ktd3102_mutex);
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
        printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] earlysuspend\n", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
}
static void ktd3102_backlight_earlyresume(struct early_suspend *desc)
{
	struct ktd3102_bl_data *ktd3102 = container_of(desc, struct ktd3102_bl_data,
				early_suspend_desc);
	struct backlight_device *bl = platform_get_drvdata(ktd3102->pdev);
	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
       backlight_mode=BACKLIGHT_RESUME;
       printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] earlyresume\n", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
       backlight_update_status(bl);
}
#else
#ifdef CONFIG_PM
static int ktd3102_backlight_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd3102_bl_data *ktd3102 = dev_get_drvdata(&bl->dev);
    
        printk("[BACKLIGHT] ktd3102_backlight_suspend\n");
        
	return 0;
}
static int ktd3102_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
        printk("[BACKLIGHT] ktd3102_backlight_resume\n");
        
	  backlight_update_status(bl);
        
	return 0;
}
#else
#define ktd3102_backlight_suspend  NULL
#define ktd3102_backlight_resume   NULL
#endif
#endif


#ifdef CONFIG_OF
static int ktd3102_backlight_parse_dt(struct device *dev,
                  struct platform_ktd3102_backlight_data *data)
{
	struct device_node *node = dev->of_node;
	struct property *prop;
	int length;
	u32 value;
	int ret;

	if (!node)
		return -ENODEV;

	memset(data, 0, sizeof(*data));

	ret = of_property_read_u32(node, "max_brightness", &value);
	if (ret < 0)
		return ret;
	data->max_brightness = value;

	ret = of_property_read_u32(node, "dft_brightness", &value);
	if (ret < 0)
		return ret;
	data->dft_brightness = value;

	ret = of_property_read_u32(node, "ctrl_pin", &value);
	if (ret < 0)
		return ret;
	data->ctrl_pin = value;

	return 0;
}

#else
static int ktd3102_backlight_parse_dt(struct device *dev,
                  struct platform_ktd3102_backlight_data *data)
{
	return -ENODEV;
}
#endif

#ifdef CONFIG_FB_BL_EVENT_CTRL
static int fb_bl_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct backlight_device *bl;
	struct fb_bl_event *evdata = data;

	if (event != FB_BL_EVENT_RESUME && event != FB_BL_EVENT_SUSPEND)
		return 0;

	bl = container_of(self, struct backlight_device, fb_notif);

	printk("fb_bl_notifier event=[0x%x]\n",event);

	switch (event) {
	case FB_BL_EVENT_RESUME:
		backlight_mode = BACKLIGHT_RESUME;
		backlight_update_status(bl);
		break;

	case FB_BL_EVENT_SUSPEND:
		backlight_mode = BACKLIGHT_SUSPEND;
		gpio_set_value(backlight_pin,0);
		mdelay(3);
		real_level=0;

		break;
	}
	return 0;
}

static int fb_backlight_register_fb(struct backlight_device *bd)
{
	memset(&bd->fb_notif, 0, sizeof(bd->fb_notif));
	bd->fb_notif.notifier_call = fb_bl_notifier_callback;

	return fb_bl_register_client(&bd->fb_notif);
}

static void fb_backlight_unregister_fb(struct backlight_device *bd)
{
	fb_bl_unregister_client(&bd->fb_notif);
}
#endif


static int ktd3102_backlight_probe(struct platform_device *pdev)
{
	struct platform_ktd3102_backlight_data *data = pdev->dev.platform_data;
	struct platform_ktd3102_backlight_data defdata;
	struct backlight_device *bl;
	struct ktd3102_bl_data *ktd3102;
    	struct backlight_properties props;
	int ret;
        printk("[BACKLIGHT] ktd3102_backlight_probe\n");

    if (!data)
	{
		ret = ktd3102_backlight_parse_dt(&pdev->dev, &defdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to find platform data\n");
			return ret;
		}
		data = &defdata;
	}	
	ktd3102 = kzalloc(sizeof(*ktd3102), GFP_KERNEL);
	if (!ktd3102) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	ktd3102->ctrl_pin = data->ctrl_pin;
	backlight_pin = data->ctrl_pin;
	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = data->max_brightness;
	props.type = BACKLIGHT_PLATFORM;
	bl = backlight_device_register("panel", &pdev->dev,
			ktd3102, &ktd3102_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}
#ifdef CONFIG_FB_BL_EVENT_CTRL
	ret = fb_backlight_register_fb(bl);
	if (ret) {
		device_unregister(&bl->dev);
		ret = ERR_PTR(ret);
		goto err_bl;
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	ktd3102->pdev = pdev;
	ktd3102->early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	ktd3102->early_suspend_desc.suspend = ktd3102_backlight_earlysuspend;
	ktd3102->early_suspend_desc.resume = ktd3102_backlight_earlyresume;
	register_early_suspend(&ktd3102->early_suspend_desc);
#endif
	bl->props.max_brightness = data->max_brightness;
	bl->props.brightness = data->dft_brightness;
	platform_set_drvdata(pdev, bl);

	ktd3102_backlight_update_status(bl);

	return 0;
err_bl:
	kfree(ktd3102);
err_alloc:
	return ret;
}
static int ktd3102_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd3102_bl_data *ktd3102 = dev_get_drvdata(&bl->dev);
	backlight_device_unregister(bl);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ktd3102->early_suspend_desc);
#endif
	kfree(ktd3102);
	return 0;
}

static int ktd3102_backlight_shutdown(struct platform_device *pdev)
{
	printk("[BACKLIGHT] ktd3102_backlight_shutdown\n");
       gpio_set_value(backlight_pin,0);
       mdelay(3); 	
	return 0;
}

static const struct of_device_id backlight_of_match[] = {
	{ .compatible = "sprd,sprd_backlight", },
	{ }
};

static struct platform_driver ktd3102_backlight_driver = {
	.driver		= {
		.name	= "panel",
		.owner	= THIS_MODULE,
		.of_match_table = backlight_of_match,
	},
	.probe		= ktd3102_backlight_probe,
	.remove		= ktd3102_backlight_remove,
	.shutdown      = ktd3102_backlight_shutdown,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend        = ktd3102_backlight_suspend,
	.resume         = ktd3102_backlight_resume,
#endif
};
static int __init ktd3102_backlight_init(void)
{
	return platform_driver_register(&ktd3102_backlight_driver);
}
module_init(ktd3102_backlight_init);
static void __exit ktd3102_backlight_exit(void)
{
	platform_driver_unregister(&ktd3102_backlight_driver);
}
module_exit(ktd3102_backlight_exit);
MODULE_DESCRIPTION("ktd3102 based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ktd3102-backlight");