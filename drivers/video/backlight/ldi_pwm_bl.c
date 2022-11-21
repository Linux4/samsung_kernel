/*
 * linux/drivers/video/backlight/ldi_pwm_bl.c
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


void ldi_pwm_bl_set_brightness(int level)
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

static int ldi_pwm_bl_update_status(struct backlight_device *bl)
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
        ldi_pwm_bl_set_brightness(brightness);
    }

    return 0;
}

static int ldi_pwm_bl_get_brightness(struct backlight_device *bl)
{
	BL_brightness = bl->props.brightness;
	return BL_brightness;
}

static const struct backlight_ops ldi_pwm_bl_ops = {
	.update_status	= ldi_pwm_bl_update_status,
	.get_brightness	= ldi_pwm_bl_get_brightness,
};


static ssize_t show_lcd_info(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s","GP_GPM1316B0\n" );
}

static struct device_attribute lcd_device_attributes[] = {
	__ATTR(lcd_type, S_IRUGO, show_lcd_info, NULL),
	__ATTR_NULL,
};

static int ldi_pwm_bl_probe(struct platform_device *pdev)
{
	struct backlight_device *bl;
       struct backlight_properties props;
	unsigned char value;
	int ret;
	struct device *dev_t;

	printk("%s\n",__FUNCTION__);

      lcd_class = class_create(THIS_MODULE, "lcd");

    	if (IS_ERR(lcd_class)) {
    		printk("Failed to create lcd_class!\n");
    		return PTR_ERR( lcd_class );
    	}

     lcd_class->dev_attrs = lcd_device_attributes;

     dev_t = device_create( lcd_class, NULL, 0, "%s", "panel");

     printk("%s end\n",__FUNCTION__);

     return 0;
out:
	return ret;
}

extern int ldi_pwm_backlight_control(int brightness);
static int ldi_pwm_bl_remove(struct platform_device *pdev)
{
    struct backlight_device *bl = platform_get_drvdata(pdev);
    ldi_pwm_backlight_control(0);
    mdelay(3);

    return 0;
}

void ldi_pwm_bl_shutdown(struct platform_device *pdev)
{
    ldi_pwm_backlight_control(0);
    mdelay(5);
}

static struct platform_driver ldi_pwm_bl_driver = {
	.driver		= {
		.name	= BACKLIGHT_DEV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= ldi_pwm_bl_probe,
	.remove		= ldi_pwm_bl_remove,
	.shutdown       = ldi_pwm_bl_shutdown,
};

static int __init ldi_pwm_bl_init(void)
{
	return platform_driver_register(&ldi_pwm_bl_driver);
}
module_init(ldi_pwm_bl_init);

static void __exit ldi_pwm_bl_exit(void)
{
	platform_driver_unregister(&ldi_pwm_bl_driver);
}
module_exit(ldi_pwm_bl_exit);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:panel");



