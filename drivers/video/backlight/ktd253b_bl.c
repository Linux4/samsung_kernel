/*************** maintained by customer ***************************************/
#define CONFIG_BACKLIGHT_AMAZING 1
/*
 * linux/drivers/video/backlight/s2c_bl.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* 	@file	drivers/video/backlight/s2c_bl.c
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
#include <linux/ktd253b_bl.h>
//#include <plat/gpio.h>
#include <mach/gpio.h>
#include <linux/delay.h>
//#include <linux/broadcom/lcd.h>
#include <linux/spinlock.h>
//#include <linux/broadcom/PowerManager.h>

#include <linux/of.h>

int current_intensity;
#if defined(CONFIG_MACH_KANAS_W) || defined(CONFIG_MACH_KANAS_TD)
static int backlight_pin = 214;
#else
static int backlight_pin = 136;
#endif

static DEFINE_SPINLOCK(bl_ctrl_lock);
static int lcd_brightness = 0;
int real_level = 13;

#ifdef CONFIG_HAS_EARLYSUSPEND
/* early suspend support */
extern int gLcdfbEarlySuspendStopDraw;
#endif
static int backlight_mode=1;

#define MAX_BRIGHTNESS_IN_BLU	33
#if defined(CONFIG_BACKLIGHT_TOTORO)
#define DIMMING_VALUE		1
#elif defined(CONFIG_BACKLIGHT_TASSVE)
#define DIMMING_VALUE		2
#elif defined(CONFIG_BACKLIGHT_AMAZING)
#define DIMMING_VALUE		1
#elif defined(CONFIG_BACKLIGHT_CORI)
#define DIMMING_VALUE		1
#else
#define DIMMING_VALUE		1
#endif
#define MAX_BRIGHTNESS_VALUE	255
#define MIN_BRIGHTNESS_VALUE	5
#define BACKLIGHT_DEBUG 0
#define BACKLIGHT_SUSPEND 1
#define BACKLIGHT_RESUME 1

#if BACKLIGHT_DEBUG
#define BLDBG(fmt, args...) printk(fmt, ## args)
#else
#define BLDBG(fmt, args...)
#endif

struct ktd253b_bl_data {
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

#if defined(CONFIG_BACKLIGHT_TOTORO)
struct brt_value brt_table_ktd[] = {
	  { MIN_BRIGHTNESS_VALUE,  2 }, // Min pulse 27(33-6) by HW 
	  { 39,  3 }, 
	  { 48,  4 }, 
	  { 57,  5 }, 
	  { 66,  6 }, 
	  { 75,  7 }, 
	  { 84,  8 },  
	  { 93,  9 }, 
	  { 102,  10 }, 
	  { 111,  11 },   
	  { 120,  12 }, 
	  { 129,  13 }, 
         { 138,  14 }, 
	  { 147,  15 }, //default value  
        { 155,  16 },
	  { 163,  17 },  
	  { 170,  18 },  
	  { 178,  19 }, 
         { 186,  20 },
        { 194,  21 }, 
        { 202,  22 },
	  { 210,  23 },  
	  { 219,  24 }, 
	  { 228,  25 }, 
         { 237,  26 },  
	  { 246,  27 }, 
	  { MAX_BRIGHTNESS_VALUE,  28 }, // Max pulse 7(33-26) by HW
};
#elif defined(CONFIG_BACKLIGHT_AMAZING)
struct brt_value brt_table_ktd[] = {
		{ MIN_BRIGHTNESS_VALUE,  1 }, // Min pulse
		{ 39,  2 }, 
		{ 47,  3 }, 
		{ 55,  4 },
		{ 63,  5 }, 
		{ 72,  6 }, 
		{ 81,  7 }, 
		{ 90,  8 }, 
		{ 99,  9 },  
		{ 107,  10 }, 
		{ 115,	11 }, 
		{ 123,	12 },	
		{ 131,	13 }, 	
		{ 139,	14 }, 		
		{ 147,	15 }, //default value  
		{ 153,	16 },  		
		{ 160,	17 },  
		{ 166,	18 },
		{ 173,	19 },
		{ 180,	20 },  
		{ 187,	21 },  		
		{ 194,	22 }, 
		{ 201,	23 },  		
		{ 208,	24 }, 
		{ 214,	25 },
		{ 220,	26 },  
		{ 227,	27 },  		
		{ 234,	28 }, 
		{ 241,	29 },  		
		{ 248,	30 }, 	
		{ MAX_BRIGHTNESS_VALUE,  31 }, // Max pulse

};
#elif defined(CONFIG_BACKLIGHT_CORI)
struct brt_value brt_table_ktd[] = {
		{ MIN_BRIGHTNESS_VALUE,  1 }, // Min pulse
		{ 38,  2 }, 
		{ 47,  3 }, 
		{ 55,  4 },
		{ 63,  5 }, 
		{ 71,  6 }, 
		{ 79,  7 }, 
		{ 87,  8 }, 
		{ 95,  9 },  
		{ 103,  10 }, 
		{ 111,	11 }, 
		{ 120,	12 },	
		{ 129,	13 }, 
		{ 138,	14 }, 
		{ 147,	15 }, //default value  
		{ 154,  15 },
		{ 162,	16 },
		{ 170,	16 },  
		{ 177,	17 },  
		{ 185,	17 },
		{ 193,	18 },
		{ 200,	18 }, 
		{ 208,	19 },
		{ 216,	20 },  
		{ 224,	22 }, 
		{ 231,	24 }, 
		{ 239,	26 },
		{ 247,	28 },  
		{ MAX_BRIGHTNESS_VALUE,  29 }, // Max pulse

};
#else
struct brt_value brt_table_ktd[] = {
	  { MIN_BRIGHTNESS_VALUE,  2 }, // Min pulse 27(33-6) by HW 
	  { 39,  3 }, 
	  { 48,  4 }, 
	  { 57,  5 }, 
	  { 67,  6 }, 
	  { 77,  7 }, 
	  { 87,  8 },  
	  { 97,  9 }, 
	  { 107,  10 }, 
	  { 117,  11 },   
	  { 127,  12 }, 
	  { 137,  13 }, 
	  { 147,  14 }, 
        { 156,  15 },
	  { 165,  16 },  
	  { 174,  17 }, 
	  { 183,  18 }, 
	  { 192,  19 }, 
         { 201,  20 },
        { 210,  21 }, 
        { 219,  22 },
	  { 228,  23 },  
	  { 237,  24 }, 
	  { 246,  25 }, 
	  { MAX_BRIGHTNESS_VALUE,  26 }, // Max pulse 7(33-26) by HW
};
#endif

#define MAX_BRT_STAGE_KTD (int)(sizeof(brt_table_ktd)/sizeof(struct brt_value))


static void lcd_backlight_control(int num)
{
	int limit;

	limit = num;

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
int CurrDimmingPulse = 0;
int PrevDimmingPulse = 0;
static int ktd253b_backlight_update_status(struct backlight_device *bd)
{
	//struct ktd253b_bl_data *ktd253b= dev_get_drvdata(&bd->dev);
	int user_intensity = bd->props.brightness;
	int pulse = 0;
	int i;
	
	//printk("[BACKLIGHT] ktd253b_backlight_update_status ==> user_intensity  : %d\n", user_intensity);
	BLDBG("[BACKLIGHT] ktd253b_backlight_update_status ==> user_intensity  : %d\n", user_intensity);

	
	if(backlight_mode==BACKLIGHT_RESUME)
	{
	    //if(gLcdfbEarlySuspendStopDraw==0)
	    {
	    	if(user_intensity > 0) 
			{
				if(user_intensity < MIN_BRIGHTNESS_VALUE) 
				{
					CurrDimmingPulse = DIMMING_VALUE; //DIMMING
				} 
				else if (user_intensity == MAX_BRIGHTNESS_VALUE) 
				{
					CurrDimmingPulse = brt_table_ktd[MAX_BRT_STAGE_KTD-1].tune_level;
				} 
				else 
				{
					for(i = 0; i < MAX_BRT_STAGE_KTD; i++) 
					{
						if(user_intensity <= brt_table_ktd[i].level ) 
						{
							CurrDimmingPulse = brt_table_ktd[i].tune_level;
							break;
						}
					}
				}
			}
			else
			{
				BLDBG("[BACKLIGHT] ktd253b_backlight_update_status ==> OFF\n");
				CurrDimmingPulse = 0;
				gpio_set_value(backlight_pin,0);
				mdelay(10);
				PrevDimmingPulse = CurrDimmingPulse;

				return 0;
			}

			//printk("[BACKLIGHT] ktd253b_backlight_update_status ==> Prev = %d, Curr = %d\n", PrevDimmingPulse, CurrDimmingPulse);
			BLDBG("[BACKLIGHT] ktd253b_backlight_update_status ==> Prev = %d, Curr = %d\n", PrevDimmingPulse, CurrDimmingPulse);

		    if (PrevDimmingPulse == CurrDimmingPulse)
		    {
		    	//printk("[BACKLIGHT] ktd253b_backlight_update_status ==> Same brightness\n");
		    	PrevDimmingPulse = CurrDimmingPulse;
		        return 0;
		    }
		    else
		    {
			    if(PrevDimmingPulse == 0)
			    {
					mdelay(200);

					//When backlight OFF->ON, only first pulse should have 2ms HIGH level.
					gpio_set_value(backlight_pin,1);
					//mdelay(2);//for HW 0.2, FAN5345 
					udelay(2);//for HW 0.3, KTD253B
			    }

				if(PrevDimmingPulse < CurrDimmingPulse)
				{
					pulse = (32 + PrevDimmingPulse) - CurrDimmingPulse;
				}
				else if(PrevDimmingPulse > CurrDimmingPulse)
				{
					pulse = PrevDimmingPulse - CurrDimmingPulse;
				}

				lcd_backlight_control(pulse);
				
				//printk("[BACKLIGHT] ktd253b_backlight_update_status ==> Prev = %d, Curr = %d, pulse = %d\n", PrevDimmingPulse, CurrDimmingPulse, pulse);
				PrevDimmingPulse = CurrDimmingPulse;

				return 0;
		    }
	    }
	}
	PrevDimmingPulse = CurrDimmingPulse;

	return 0;
}

static int ktd253b_backlight_get_brightness(struct backlight_device *bl)
{
	BLDBG("[BACKLIGHT] ktd253b_backlight_get_brightness\n");
    
	return current_intensity;
}

static struct backlight_ops ktd253b_backlight_ops = {
	.update_status	= ktd253b_backlight_update_status,
	.get_brightness	= ktd253b_backlight_get_brightness,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ktd253b_backlight_earlysuspend(struct early_suspend *desc)
{
    //struct ktd253b_bl_data *ktd253b = container_of(desc, struct ktd253b_bl_data, early_suspend_desc);
	//struct backlight_device *bl = platform_get_drvdata(ktd253b->pdev);
	
	backlight_mode=BACKLIGHT_SUSPEND;
	PrevDimmingPulse=0;
    printk("[BACKLIGHT] ktd253b_backlight_earlysuspend\n");
}

static void ktd253b_backlight_earlyresume(struct early_suspend *desc)
{
	struct ktd253b_bl_data *ktd253b = container_of(desc, struct ktd253b_bl_data, early_suspend_desc);
	struct backlight_device *bl = platform_get_drvdata(ktd253b->pdev);

	backlight_mode=BACKLIGHT_RESUME;
    printk("[BACKLIGHT] ktd253b_backlight_earlyresume\n");
    
    backlight_update_status(bl);
}
#else
#ifdef CONFIG_PM
static int ktd253b_backlight_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd253b_bl_data *ktd253b = dev_get_drvdata(&bl->dev);
    
	printk("[BACKLIGHT] ktd253b_backlight_suspend\n");
        
	return 0;
}

static int ktd253b_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	printk("[BACKLIGHT] ktd253b_backlight_resume\n");
        
	backlight_update_status(bl);
        
	return 0;
}
#else
#define ktd253b_backlight_suspend  NULL
#define ktd253b_backlight_resume   NULL
#endif
#endif


#ifdef CONFIG_OF
static int ktd253b_backlight_parse_dt(struct device *dev,
                  struct platform_ktd253b_backlight_data *data)
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
static int ktd253b_backlight_parse_dt(struct device *dev,
                  struct platform_ktd253b_backlight_data *data)
{
	return -ENODEV;
}
#endif

static int ktd253b_backlight_probe(struct platform_device *pdev)
{
	struct platform_ktd253b_backlight_data *data = pdev->dev.platform_data;
	struct platform_ktd253b_backlight_data defdata;
	struct backlight_device *bl;
	struct ktd253b_bl_data *ktd253b;
	struct backlight_properties props;
	
	int ret;

	printk("[BACKLIGHT] ktd253b_backlight_probe\n");
    
	if (!data) 
	{
		ret = ktd253b_backlight_parse_dt(&pdev->dev, &defdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to find platform data\n");
			return ret;
		}
		data = &defdata;
	}

	ktd253b = kzalloc(sizeof(*ktd253b), GFP_KERNEL);
	if (!ktd253b) 
	{
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	ktd253b->ctrl_pin = data->ctrl_pin;
    
	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = data->max_brightness;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register(pdev->name, &pdev->dev, ktd253b, &ktd253b_backlight_ops, &props);
	if (IS_ERR(bl)) 
	{
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	gpio_request(backlight_pin,"lcd_backlight");
#ifdef CONFIG_HAS_EARLYSUSPEND
	ktd253b->pdev = pdev;
	ktd253b->early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	ktd253b->early_suspend_desc.suspend = ktd253b_backlight_earlysuspend;
	ktd253b->early_suspend_desc.resume = ktd253b_backlight_earlyresume;
	register_early_suspend(&ktd253b->early_suspend_desc);
#endif

	bl->props.max_brightness = data->max_brightness;
	bl->props.brightness = data->dft_brightness;

	platform_set_drvdata(pdev, bl);

	ktd253b_backlight_update_status(bl);
    
	return 0;

err_bl:
	kfree(ktd253b);
err_alloc:
	return ret;
}

static int ktd253b_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct ktd253b_bl_data *ktd253b = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);


#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ktd253b->early_suspend_desc);
#endif

	kfree(ktd253b);

	gpio_free(backlight_pin);

	return 0;
}

static const struct of_device_id backlight_of_match[] = {
	{ .compatible = "sprd,sprd_backlight", },
	{ }
};

static struct platform_driver ktd253b_backlight_driver = {
	.driver		= {
		.name	= "sprd_backlight",
		.owner	= THIS_MODULE,
		.of_match_table = backlight_of_match,
	},
	.probe		= ktd253b_backlight_probe,
	.remove		= ktd253b_backlight_remove,

#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend        = ktd253b_backlight_suspend,
	.resume         = ktd253b_backlight_resume,
#endif

};

static int __init ktd253b_backlight_init(void)
{
	return platform_driver_register(&ktd253b_backlight_driver);
}
module_init(ktd253b_backlight_init);

static void __exit ktd253b_backlight_exit(void)
{
	platform_driver_unregister(&ktd253b_backlight_driver);
}
module_exit(ktd253b_backlight_exit);

MODULE_DESCRIPTION("ktd253b based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ktd253b-backlight");
