
/*******************************************************************************
* Copyright 2013 Samsung Electronics Corporation.  All rights reserved.
*
* 	@file	drivers/video/backlight/rt4502_bl.c
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
#include <linux/rt4502_bl.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <../sprdfb/sprdfb.h>

#include <linux/of.h>

#include <mach/board.h>

int current_intensity;
#ifdef SPI0_BACKLIGHT_PWM_GPIO
static int backlight_pin = SPI0_BACKLIGHT_PWM_GPIO;
#else
static int backlight_pin = 190;
#endif

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
#define BLDBG(fmt, args...) printk(fmt, ## args)
#else
#define BLDBG(fmt, args...)
#endif
struct rt4502_bl_data {
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
#if defined (CONFIG_MACH_RHEA_SS_LUCAS)
struct brt_value brt_table_ktd[] = {
   { MIN_BRIGHTNESS_VALUE,  31 }, // Min pulse 32
   { 32,  31 },
   { 46,  30 },
   { 60,  29 },  
   { 73,  28 }, 
   { 86,  27 }, 
   { 98,  26 }, 
   { 105,  25 }, 
   { 110,  24 }, 
   { 115,  23 }, 
   { 120,  22 }, 
   { 125,  21 }, 
  	{ 130,  20 }, 
   { 140,  19 },//default value  
   { 155,  18 },   
   { 165,  17 },
   { 176,  16 }, 
   { 191,  15 }, 
   { 207,  14 },
   { 214,  13 },
   { 221,  12 },
   { 228,  10 },
   { 235,  8 },
   { 242,  7 },
   { 249,  5 },
   { MAX_BRIGHTNESS_VALUE,  5 }, // Max pulse 1
};
#elif defined (CONFIG_MACH_CORSICA_VE) || defined (CONFIG_MACH_VIVALTO)
struct brt_value brt_table_ktd[] = {
	{MIN_BRIGHTNESS_VALUE, 31},	/*Min pulse 32 */
	{30, 31},
	{41, 30},
	{52, 29},
	{63, 28},
	{74, 27},
	{85, 26},
	{96, 25},
	{107, 24},
	{118, 23},
	{127, 22},
	{135, 21},
	{143, 20},		/*default value */
	{155, 18},
	{168, 17},
	{177, 16},
	{186, 15},
	{194, 14},
	{202, 13},
	{210, 12},
	{218, 11},
	{226, 10},
	{234, 9},
	{241, 7},
	{248, 6},
	{MAX_BRIGHTNESS_VALUE, 5},	/*Max pulse 1 */
};
#elif defined(CONFIG_MACH_YOUNG23GDTV) || defined(CONFIG_MACH_J13G)
struct brt_value brt_table_ktd[] = {
	{MIN_BRIGHTNESS_VALUE, 31},	/*Min pulse 32 */
	{30, 31},
	{41, 30},
	{52, 29},
	{63, 28},
	{74, 27},
	{80, 26},
	{88, 25},
	{95, 24},
	{103, 23},
	{108, 22},
	{110, 21},
	{120, 18},		/*default value */
	{135, 17},
	{148, 16},
	{157, 15},
	{166, 14},
	{174, 13},
	{192, 12},
	{204, 11},
	{218, 10},
	{226, 9},
	{234, 8},
	{241, 7},
	{248, 6},
	{MAX_BRIGHTNESS_VALUE, 5},	/*Max pulse 1 */
};
#elif defined(CONFIG_MACH_KANAS_W) || defined(CONFIG_MACH_KANAS_TD) || defined(CONFIG_MACH_GOYAVE3G) || defined(CONFIG_MACH_GOYAVEWIFI)
struct brt_value brt_table_ktd[] = {
	{ MIN_BRIGHTNESS_VALUE,  31 }, // Min pulse 32
	{ 28,  31 },
	{ 36,  30 },
	{ 44,  29 },
	{ 52,  28 },
	{ 60,  27 },
	{ 68,  26 },
	{ 76,  25 },
	{ 84,  24 },
	{ 92,  23 },
	{ 100,  22 },
	{ 108,  21 },
	{ 116,  20 },
	{ 124,  19 },
	{ 132,  18 },
	{ 140,  17 },
	{ 148,  16 },  //default value
	{ 156,  15 },
	{ 164,  14 },
	{ 172,  13 },
	{ 180,  12 },
	{ 188,  11 },
	{ 196,  10 },
	{ 204,  9 },
	{ 212,  8 },
	{ 220,  7 },
	{ 228,  6 },
	{ 236,  5 },
	{ 244,  4 },
	{ 252,  3 },
	{ MAX_BRIGHTNESS_VALUE, 2 },
};
#else
struct brt_value brt_table_ktd[] = {
{ MIN_BRIGHTNESS_VALUE,  31 }, // Min pulse 32 
   { 30,  31 },   
   { 41,  30 },   
   { 52,  29 },     
   { 63,  28 },    
   { 74,  27 },    
   { 85,  26 },    
   { 96,  25 },    
   { 107,  24 },    
   { 118,  23 }, 
   { 127,  22 },    
   { 135,  21 },    
   { 143,  20 }, // defualt value
   { 155,  18 }, 
   { 168,  17 },    
   { 177,  16 },     
   { 186,  15 },       
   { 194,  14 },   
   { 202,  13 },   
   { 210,  12 },   
   { 218,  11 },    
   { 226,  10 },  
   { 234,  9 },   
   { 241,  5 },    
   { 248,  4 },      
   { MAX_BRIGHTNESS_VALUE, 3 }, 
 };

#endif
#define MAX_BRT_STAGE_KTD (int)(sizeof(brt_table_ktd)/sizeof(struct brt_value))

struct mutex rt4502_mutex;
DEFINE_MUTEX(rt4502_mutex);

static void lcd_backlight_control(int num)
{
    int limit;
    
    limit = num;
    BLDBG("[BACKLIGHT] lcd_backlight_control ==> pulse  : %d\n", num);
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
static int rt4502_backlight_update_status(struct backlight_device *bd)
{
	int user_intensity = bd->props.brightness;
    int tune_level = 0;
	int pulse;
    int i;
    BLDBG("[BACKLIGHT] rt4502_backlight_update_status ==> user_intensity  : %d\n", user_intensity);

	if (bd->props.power != FB_BLANK_UNBLANK)
		user_intensity = 0;

	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		user_intensity = 0;

	if (bd->props.state & BL_CORE_SUSPENDED)
		user_intensity = 0;

	mutex_lock(&rt4502_mutex);
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
		    ("[BACKLIGHT] rt4502_backlight_update_status ==> user_intensity  : %d, tune_level : %d\n",
		     user_intensity, tune_level);
		if (real_level == tune_level) {
			mutex_unlock(&rt4502_mutex);
			return 0;
		} else {
			if (tune_level <= 0) {
				gpio_set_value(backlight_pin,0);
				mdelay(3);
				real_level = 0;
			} else {
				if (real_level == 0) {
					gpio_set_value(backlight_pin, 1);
					BLDBG("[BACKLIGHT] %s -> Control Pin Enable\n",__func__);
					udelay(100);

			}
				if (real_level <= tune_level) {
					pulse = tune_level - real_level;
				} else {
					pulse = 32 - (real_level - tune_level);
				}
				if (pulse == 0) {
					mutex_unlock(&rt4502_mutex);
					return 0;
				}
				lcd_backlight_control(pulse);
			}
			real_level = tune_level;
		}
	}
	mutex_unlock(&rt4502_mutex);

       return 0;
}
static int rt4502_backlight_get_brightness(struct backlight_device *bl)
{
     BLDBG("[BACKLIGHT] rt4502_backlight_get_brightness\n");

	return current_intensity;
}
static struct backlight_ops rt4502_backlight_ops = {
	.update_status	= rt4502_backlight_update_status,
	.get_brightness	= rt4502_backlight_get_brightness,
};
#ifdef CONFIG_HAS_EARLYSUSPEND
static void rt4502_backlight_earlysuspend(struct early_suspend *desc)
{
    struct timespec ts;
    struct rtc_time tm;
#ifndef CONFIG_FB_BL_EVENT_CTRL
    backlight_mode=BACKLIGHT_SUSPEND;
#endif
     mutex_lock(&rt4502_mutex);
       gpio_set_value(backlight_pin,0);
       mdelay(3);
	real_level=0;
    mutex_unlock(&rt4502_mutex);
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
        printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] earlysuspend\n", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
}
static void rt4502_backlight_earlyresume(struct early_suspend *desc)
{
	struct rt4502_bl_data *rt4502 = container_of(desc, struct rt4502_bl_data,
				early_suspend_desc);
	struct backlight_device *bl = platform_get_drvdata(rt4502->pdev);
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
static int rt4502_backlight_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct rt4502_bl_data *rt4502 = dev_get_drvdata(&bl->dev);
    
        BLDBG("[BACKLIGHT] rt4502_backlight_suspend\n");
        
	return 0;
}
static int rt4502_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
        BLDBG("[BACKLIGHT] rt4502_backlight_resume\n");
        
	  backlight_update_status(bl);
        
	return 0;
}
#else
#define rt4502_backlight_suspend  NULL
#define rt4502_backlight_resume   NULL
#endif
#endif


#ifdef CONFIG_OF
static int rt4502_backlight_parse_dt(struct device *dev,
                  struct platform_rt4502_backlight_data *data)
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
static int rt4502_backlight_parse_dt(struct device *dev,
                  struct platform_rt4502_backlight_data *data)
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


static int rt4502_backlight_probe(struct platform_device *pdev)
{
	struct platform_rt4502_backlight_data *data = pdev->dev.platform_data;
	struct platform_rt4502_backlight_data defdata;
	struct backlight_device *bl;
	struct rt4502_bl_data *rt4502;
    	struct backlight_properties props;
	int ret;
        BLDBG("[BACKLIGHT] rt4502_backlight_probe\n");

    if (!data)
	{
		ret = rt4502_backlight_parse_dt(&pdev->dev, &defdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to find platform data\n");
			return ret;
		}
		data = &defdata;
	}	
	rt4502 = kzalloc(sizeof(*rt4502), GFP_KERNEL);
	if (!rt4502) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	rt4502->ctrl_pin = data->ctrl_pin;
	backlight_pin = data->ctrl_pin;
    
	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = data->max_brightness;
	props.type = BACKLIGHT_PLATFORM;
	bl = backlight_device_register("panel", &pdev->dev,
			rt4502, &rt4502_backlight_ops, &props);
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
	rt4502->pdev = pdev;
	rt4502->early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN-1;
	rt4502->early_suspend_desc.suspend = rt4502_backlight_earlysuspend;
	rt4502->early_suspend_desc.resume = rt4502_backlight_earlyresume;
	register_early_suspend(&rt4502->early_suspend_desc);
#endif
	bl->props.max_brightness = data->max_brightness;
	bl->props.brightness = data->dft_brightness;
	platform_set_drvdata(pdev, bl);

	rt4502_backlight_update_status(bl);

	return 0;
err_bl:
	kfree(rt4502);
err_alloc:
	return ret;
}
static int rt4502_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct rt4502_bl_data *rt4502 = dev_get_drvdata(&bl->dev);
	backlight_device_unregister(bl);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&rt4502->early_suspend_desc);
#endif
	kfree(rt4502);
	return 0;
}

static int rt4502_backlight_shutdown(struct platform_device *pdev)
{
	printk("[BACKLIGHT] rt4502_backlight_shutdown\n");
       gpio_set_value(backlight_pin,0);
       mdelay(3); 	
	return 0;
}

static const struct of_device_id backlight_of_match[] = {
	{ .compatible = "sprd,sprd_backlight", },
	{ }
};

static struct platform_driver rt4502_backlight_driver = {
	.driver		= {
		.name	= "panel",
		.owner	= THIS_MODULE,
		.of_match_table = backlight_of_match,
	},
	.probe		= rt4502_backlight_probe,
	.remove		= rt4502_backlight_remove,
	.shutdown      = rt4502_backlight_shutdown,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend        = rt4502_backlight_suspend,
	.resume         = rt4502_backlight_resume,
#endif
};
static int __init rt4502_backlight_init(void)
{
	return platform_driver_register(&rt4502_backlight_driver);
}
module_init(rt4502_backlight_init);
static void __exit rt4502_backlight_exit(void)
{
	platform_driver_unregister(&rt4502_backlight_driver);
}
module_exit(rt4502_backlight_exit);
MODULE_DESCRIPTION("rt4502 based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:rt4502-backlight");

