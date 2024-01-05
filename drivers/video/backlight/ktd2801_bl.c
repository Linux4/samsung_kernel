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
* @file	drivers/video/backlight/s2c_bl.c
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
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/ktd2801_bl.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#ifdef CONFIG_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

static DEFINE_SPINLOCK(bl_ctrl_lock);
#define BACKLIGHT_DEBUG 1
#if BACKLIGHT_DEBUG
#define BLDBG(fmt, args...) printk(fmt, ## args)
#else
#define BLDBG(fmt, args...)
#endif

#define EXPRESSWIRE_DIMMING

#ifdef EXPRESSWIRE_DIMMING
#define EW_DELAY 200
#define EW_DETECT 300
#define EW_WINDOW 900
#define DATA_START 40
#define LOW_BIT_L 40
#define LOW_BIT_H 10
#define HIGH_BIT_L 10
#define HIGH_BIT_H 40
#define END_DATA_L 10
#define END_DATA_H 400
#endif

#define BACKLIGHT_DEV_NAME	"sprd_backlight"

int BL_brightness;
int is_poweron = 1;
int current_brightness;
int wakeup_brightness;

#ifdef CONFIG_MACH_NEVISTD
#define GPIO_BL_CTRL	138
#else
#define GPIO_BL_CTRL	190
#endif

#ifdef CONFIG_MACH_NEVISTD
#define MAX_BRIGHTNESS	255
#define MIN_BRIGHTNESS	20
#define DEFAULT_BRIGHTNESS 130
#define DEFAULT_PULSE 20
#define DIMMING_VALUE	31
#define MAX_BRIGHTNESS_IN_BLU	32 // backlight-IC MAX VALUE
#else
#define MAX_BRIGHTNESS	255
#define MIN_BRIGHTNESS	20
#define DEFAULT_BRIGHTNESS 122
#define DEFAULT_PULSE 20
#define DIMMING_VALUE	31
#define MAX_BRIGHTNESS_IN_BLU	32 // backlight-IC MAX VALUE
#endif

static int lcd_brightness = DEFAULT_PULSE;

struct brt_value{
	int level;				// Platform setting values
	int tune_level;			// Chip Setting values
};

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend_BL;
#endif


struct brt_value brt_table_ktd[] = {
	{ 255,	0 }, /* Max */
	{ 250,	2 },
	{ 245,	3 },
	{ 240,	4 },
	{ 235,	5 },
	{ 230,	6 },
	{ 220,	7 },
	{ 210,	8 },
	{ 200,	9 },
	{ 190,	10 },
	{ 180,	11 },
	{ 170,	12 },
	{ 160,	13 },
	{ 150,	14 },
	{ 140,	15 },
	{ 130,	16 },
	{ 120,	17 }, /* default */
	{ 115,	18 },
	{ 110,	19 },
	{ 105,	20 },
	{ 100,	21 },
	{ 95,	22 },
	{ 90,	23 },
	{ 85,	24 },
	{ 80,	25 },
	{ 70,	26 }, /* Dimming */
	{ 60,	27 },
	{ 50,	28 },
	{ 40,	29 },
	{ 30,	30 },
	{ 20,	31 },
	{ 0,	31 }, /* Off */
};


#define NB_BRT_LEVEL (int)(sizeof(brt_table_ktd)/sizeof(struct brt_value))

#if defined(CONFIG_FB_SC8825) && defined(CONFIG_FB_LCD_NT35510_MIPI)
extern bool is_first_frame_done;
#endif

#ifdef CONFIG_MACH_NEVISTD
extern int spa_lpm_charging_mode_get(void);
#endif
#ifdef CONFIG_EARLYSUSPEND
static void ktd253_backlight_early_suspend(struct early_suspend *h)
{
	is_poweron = 0;
	ktd_backlight_set_brightness(0);
#ifdef EXPRESSWIRE_DIMMING
	gpio_set_value(GPIO_BL_CTRL, 0);
#endif
	return;
}

static void ktd253_backlight_late_resume(struct early_suspend *h)
{
	int i = 0;

#if defined(CONFIG_FB_SC8825) && defined(CONFIG_FB_LCD_NT35510_MIPI)
	while (i++ < 10 && !is_first_frame_done) {
		printk(KERN_INFO "[Backlight] wait first vsync_done, sleep 200msec\n", __func__);
		msleep(200);
	}
#endif
#ifdef EXPRESSWIRE_DIMMING
	spin_lock(&bl_ctrl_lock);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(200);
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(300);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(400);
	spin_unlock(&bl_ctrl_lock);
#endif
	ktd_backlight_set_brightness(wakeup_brightness);
	is_poweron = 1;
}
#endif

#ifdef EXPRESSWIRE_DIMMING
void ktd_backlight_set_brightness(int level)
{
	int i = 0;
	unsigned char brightness;
	int bit_map[8];
	brightness = level;
	printk("set_brightness : level(%d)\n", brightness);
	for(i = 0; i < 8; i++)
	{
		bit_map[i] = brightness & 0x01;
		brightness >>= 1;
	}
	spin_lock(&bl_ctrl_lock);
#if 0
	gpio_set_value(GPIO_BL_CTRL, 0);
	msleep(10);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(200);
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(300);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(400);
#endif
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(DATA_START);
	for(i = 7; i >= 0; i--)
	{
		if(bit_map[i])
		{
			gpio_set_value(GPIO_BL_CTRL, 0);
			udelay(HIGH_BIT_L);
			gpio_set_value(GPIO_BL_CTRL, 1);
			udelay(HIGH_BIT_H);
		}
		else
		{
			gpio_set_value(GPIO_BL_CTRL, 0);
			udelay(LOW_BIT_L);
			gpio_set_value(GPIO_BL_CTRL, 1);
			udelay(LOW_BIT_H);
		}
	}
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(END_DATA_L);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(END_DATA_H);
	spin_unlock(&bl_ctrl_lock);
	return;
}
#else
void ktd_backlight_set_brightness(int level)
{
	int tune_level = 0;


	spin_lock(&bl_ctrl_lock);
	if (level > 0) {
		if (level < MIN_BRIGHTNESS) {
			tune_level = DIMMING_VALUE; /* DIMMING */
		} else {
			int i;
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

	if (!level) {
		gpio_set_value(GPIO_BL_CTRL, 0);
		mdelay(3);
		lcd_brightness = tune_level;
	} else {
		int pulse;

		if (unlikely(lcd_brightness < 0)) {
			int val = gpio_get_value(GPIO_BL_CTRL);
			if (val) {
				lcd_brightness = 0;
			gpio_set_value(GPIO_BL_CTRL, 0);
			mdelay(3);
				printk(KERN_INFO "LCD Baklight init in boot time on kernel\n");
			}
		}

#ifdef CONFIG_MACH_NEVISTD
		if(spa_lpm_charging_mode_get())
		msleep(500);
#endif
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
	return;
}
#endif

static int ktd_backlight_update_status(struct backlight_device *bl)
{
	int brightness = bl->props.brightness;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	wakeup_brightness = brightness;
	if (is_poweron == 1)
		ktd_backlight_set_brightness(brightness);
	else
		BLDBG("[BACKLIGHT] warning : ignore set brightness\n");

	return 0;
}

static int ktd_backlight_get_brightness(struct backlight_device *bl)
{
	BLDBG("[BACKLIGHT] ktd_backlight_get_brightness\n");

	BL_brightness = bl->props.brightness;
	return BL_brightness;
}

static const struct backlight_ops ktd_backlight_ops = {
	.update_status	= ktd_backlight_update_status,
	.get_brightness	= ktd_backlight_get_brightness,
};

static int ktd_backlight_probe(struct platform_device *pdev)
{
//	struct platform_ktd253b_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct backlight_properties props;

	printk("[BACKLIGHT] ktd253b_backlight_probe\n");

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = MAX_BRIGHTNESS;
	props.type = BACKLIGHT_RAW;

	bl = backlight_device_register(BACKLIGHT_DEV_NAME, &pdev->dev, NULL,
					&ktd_backlight_ops, &props);
	if (IS_ERR(bl))
	{
		dev_err(&pdev->dev, "failed to register backlight\n");
		return PTR_ERR(bl);
	}

	bl->props.max_brightness = MAX_BRIGHTNESS;
	bl->props.brightness = DEFAULT_BRIGHTNESS;

	platform_set_drvdata(pdev, bl);

	if(gpio_request(GPIO_BL_CTRL,"BL_CTRL"))
		printk(KERN_ERR "Request GPIO failed,""gpio: %d \n", GPIO_BL_CTRL);
#ifdef EXPRESSWIRE_DIMMING
	spin_lock(&bl_ctrl_lock);
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(1500);
	udelay(1500);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(200);
	gpio_set_value(GPIO_BL_CTRL, 0);
	udelay(300);
	gpio_set_value(GPIO_BL_CTRL, 1);
	udelay(400);
	spin_unlock(&bl_ctrl_lock);
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	early_suspend_BL.suspend = ktd253_backlight_early_suspend;
	early_suspend_BL.resume  = ktd253_backlight_late_resume;
	early_suspend_BL.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&early_suspend_BL);
#endif
	ktd_backlight_update_status(bl);

	return 0;
out:
	return 1;
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

