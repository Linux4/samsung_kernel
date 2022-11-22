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
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>

int current_intensity;

static int backlight_mode=1;

#define DIMMING_VALUE		8
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

struct lcd_panel_cabc_pwm_bl_data {
	struct platform_device *pdev;
	unsigned int ctrl_pin;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend_desc;
#endif
};

#if defined(CONFIG_FB_LCD_OLED_BACKLIGHT)
extern int sprdfb_panel_update_brightness(struct backlight_device *bd);
#endif

struct brt_value{
	int level;				// Platform setting values
	int tune_level;			// Chip Setting values
};


struct brt_value brt_table[] = {
   { MIN_BRIGHTNESS_VALUE,  8 }, 
   { 25,  11 },
   { 30,  14 },
   { 35,  17 },  
   { 40,  20 }, 
   { 45,  23 }, 
   { 50,  26 },
   { 55,  29 }, 
   { 60,  32 },
   { 65,  35 }, 
   { 70,  38 },
   { 75,  41 },
   { 80,  44 }, 
   { 85,  47 }, 
   { 90,  50 }, 
   { 95,  53 }, 
   { 100,  56 },   
   { 105,  59 },
   { 110,  62 }, 
   { 115,  65 }, 
   { 120,  68 },
   { 125,  71 },
   { 130,  74 },
   { 135,  77 },
   { 140,  80 },
   { 145,  83 },  
   { 150,  86 },
   { 155,  89 },
   { 160,  91 }, /* default 160 */
   { 165,  96 },
   { 170,  101 },
   { 175,  106 },
   { 180,  111 },
   { 185,  116 }, 
   { 190,  121 },
   { 195,  126 },
   { 200,  131 },
   { 205,  136 },
   { 210,  141 },
   { 215,  146 },
   { 220,  151 },
   { 225,  156 },
   { 230,  161 },
   { 235,  166 },
   { 240,  171 },
   { 245,  176 },
   { 250,  181 },
   { MAX_BRIGHTNESS_VALUE,  186 }, 
};


#define MAX_BRT_STAGE (int)(sizeof(brt_table)/sizeof(struct brt_value))

extern void backlight_control(int brigtness);

#if defined(CONFIG_FB_LCD_OLED_BACKLIGHT)
extern int sprdfb_panel_update_brightness(struct backlight_device *bd);
#endif

/* input: intensity in percentage 0% - 100% */
static int lcd_panel_cabc_pwm_backlight_update_status(struct backlight_device *bd)
{
	int user_intensity = bd->props.brightness;
 	int tune_level = 0;
	int i;
  
	BLDBG("[BACKLIGHT] lcd_panel_cabc_pwm_backlight_update_status ==> user_intensity  : %d\n", user_intensity);
		
	if (bd->props.power != FB_BLANK_UNBLANK)
		user_intensity = 0;

	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		user_intensity = 0;

	if (bd->props.state & BL_CORE_SUSPENDED)
		user_intensity = 0;
		
	if(backlight_mode != BACKLIGHT_RESUME)
	{
	//	BLDBG("[BACKLIGHT] Returned with invalid backlight mode %d\n", backlight_mode);
		return 0;
	}

#if 1
	tune_level = user_intensity;
#else
	if(user_intensity > 0) {
		if(user_intensity < MIN_BRIGHTNESS_VALUE) {
			tune_level = DIMMING_VALUE; //DIMMING
		} else if (user_intensity == MAX_BRIGHTNESS_VALUE) {
			tune_level = brt_table[MAX_BRT_STAGE-1].tune_level;
		} else {
			for(i = 0; i < MAX_BRT_STAGE; i++) {
				if(user_intensity <= brt_table[i].level ) {
					tune_level = brt_table[i].tune_level;
					break;
				}
			}
		}
	}
#endif
		
	BLDBG("[BACKLIGHT] lcd_panel_cabc_pwm_backlight_update_status ==> tune_level : %d\n", tune_level);

#if defined(CONFIG_FB_LCD_OLED_BACKLIGHT)
	sprdfb_panel_update_brightness(bd); //OLED control
#else
	backlight_control(tune_level);
#endif

	current_intensity = tune_level;
	
	return 0;
}

static int lcd_panel_cabc_pwm_backlight_get_brightness(struct backlight_device *bl)
{
	BLDBG("[BACKLIGHT] lcd_panel_cabc_pwm_backlight_get_brightness\n");
    
	return current_intensity;
}

static struct backlight_ops lcd_panel_cabc_pwm_backlight_ops = {
	.update_status	= lcd_panel_cabc_pwm_backlight_update_status,
	.get_brightness	= lcd_panel_cabc_pwm_backlight_get_brightness,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lcd_panel_cabc_pwm_backlight_earlysuspend(struct early_suspend *desc)
{
	struct timespec ts;
	struct rtc_time tm;
	
	backlight_mode=BACKLIGHT_SUSPEND; 

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] earlysuspend\n", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
}

static void lcd_panel_cabc_pwm_backlight_lateresume(struct early_suspend *desc)
{
	struct lcd_panel_cabc_pwm_bl_data *lcd_panel_cabc_pwm = container_of(desc, struct lcd_panel_cabc_pwm_bl_data, early_suspend_desc);
	struct backlight_device *bl = platform_get_drvdata(lcd_panel_cabc_pwm->pdev);
	struct timespec ts;
	struct rtc_time tm;
	
	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);
	backlight_mode = BACKLIGHT_RESUME;
	printk("[%02d:%02d:%02d.%03lu][BACKLIGHT] late resume\n", tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);

	backlight_update_status(bl);
}
#else
#ifdef CONFIG_PM
static int lcd_panel_cabc_pwm_backlight_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct lcd_panel_cabc_pwm_bl_data *lcd_panel_cabc_pwm = dev_get_drvdata(&bl->dev);
	  
	BLDBG("[BACKLIGHT] lcd_panel_cabc_pwm_backlight_suspend\n");
	      
	return 0;
}
static int lcd_panel_cabc_pwm_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
  BLDBG("[BACKLIGHT] lcd_panel_cabc_pwm_backlight_resume\n");
	    
	backlight_update_status(bl);
	    
	return 0;
}
#else
#define lcd_panel_cabc_pwm_backlight_suspend  NULL
#define lcd_panel_cabc_pwm_backlight_resume   NULL
#endif /* CONFIG_PM */
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int lcd_panel_cabc_pwm_backlight_probe(struct platform_device *pdev)
{
	struct backlight_device *bl;
	struct lcd_panel_cabc_pwm_bl_data *lcd_panel_cabc_pwm;
	struct backlight_properties props;
	int ret;
	int max_brightness = 255;
	int default_brightness = 160;
	
	BLDBG("[BACKLIGHT] lcd_panel_cabc_pwm_backlight_probe\n");
	
	lcd_panel_cabc_pwm = kzalloc(sizeof(*lcd_panel_cabc_pwm), GFP_KERNEL);
	if (!lcd_panel_cabc_pwm) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = max_brightness;
	props.type = BACKLIGHT_PLATFORM;
	bl = backlight_device_register("sprd_backlight", &pdev->dev,
			lcd_panel_cabc_pwm, &lcd_panel_cabc_pwm_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	lcd_panel_cabc_pwm->pdev = pdev;
	lcd_panel_cabc_pwm->early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	lcd_panel_cabc_pwm->early_suspend_desc.suspend = lcd_panel_cabc_pwm_backlight_earlysuspend;
	lcd_panel_cabc_pwm->early_suspend_desc.resume = lcd_panel_cabc_pwm_backlight_lateresume;
	register_early_suspend(&lcd_panel_cabc_pwm->early_suspend_desc);
#endif

	bl->props.max_brightness = max_brightness;
	bl->props.brightness = default_brightness;
	platform_set_drvdata(pdev, bl);
  //lcd_panel_cabc_pwm_backlight_update_status(bl);
    
	return 0;
	
err_bl:
	kfree(lcd_panel_cabc_pwm);
err_alloc:
	return ret;
}

static int lcd_panel_cabc_pwm_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct lcd_panel_cabc_pwm_bl_data *lcd_panel_cabc_pwm = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&lcd_panel_cabc_pwm->early_suspend_desc);
#endif

	kfree(lcd_panel_cabc_pwm);

	return 0;
}

static int lcd_panel_cabc_pwm_backlight_shutdown(struct platform_device *pdev)
{
	printk("[BACKLIGHT] lcd_panel_cabc_pwm_backlight_shutdown\n");
	return 0;
}

static const struct of_device_id backlight_of_match[] = {
	{ .compatible = "sprd,panel_cabc_bl", },
	{ }
};

static struct platform_driver lcd_panel_cabc_pwm_backlight_driver = {
	.driver		= {
		.name	= "lcd_panel_cabc",
		.owner	= THIS_MODULE,
		.of_match_table = backlight_of_match,
	},
	.probe		= lcd_panel_cabc_pwm_backlight_probe,
	.remove		= lcd_panel_cabc_pwm_backlight_remove,
	.shutdown      = lcd_panel_cabc_pwm_backlight_shutdown,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend        = lcd_panel_cabc_pwm_backlight_suspend,
	.resume         = lcd_panel_cabc_pwm_backlight_resume,
#endif
};

static int __init lcd_panel_cabc_pwm_backlight_init(void)
{
	return platform_driver_register(&lcd_panel_cabc_pwm_backlight_driver);
}

module_init(lcd_panel_cabc_pwm_backlight_init);

static void __exit lcd_panel_cabc_pwm_backlight_exit(void)
{
	platform_driver_unregister(&lcd_panel_cabc_pwm_backlight_driver);
}

module_exit(lcd_panel_cabc_pwm_backlight_exit);
MODULE_DESCRIPTION("lcd_panel_cabc_pwm_bl based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lcd_panel_cabc_pwm_bl");

