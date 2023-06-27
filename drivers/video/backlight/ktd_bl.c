/*
 * linux/drivers/video/backlight/ktd_bl.c
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
#include <linux/ktd_bl.h>
#define __BACKLIGHT_DEBUG__  0

#define BACKLIGHT_DEV_NAME	"panel"
#define MAX_BRIGHTNESS_IN_BLU	(32)

#if defined(CONFIG_BACKLIGHT_KTD253)
#define CTRL_T_EN_US	(55)	/* CTRL Start-up Time */
#elif defined(CONFIG_BACKLIGHT_KTD3102)
#define CTRL_T_EN_US	(500)	/* CTRL Start-up Time */
#else
#define CTRL_T_EN_US	(55)	/* CTRL Start-up Time */
#endif
#define CTRL_T_HIGH_US	(1)	/* CTRL High Pulse Width */
#define CTRL_T_LOW_US	(1)	/* CTRL Low Pulse Width */
#define CTRL_T_OFF_MS	(3)	/* CTRL Shutdown Pulse Width */
#define PWM_T_OFF_MS	(100)	/* PWM Shutdown Pulse Width */
#define VOUT_T_OFF_MS	(100)	/* Vout Off Stable Time */

static bool bl_enable;
int current_brightness;
int wakeup_brightness;
struct ktd_bl_info *g_bl_info;
static int prev_tune_level;
static DEFINE_MUTEX(bl_ctrl_mutex);
static DEFINE_SPINLOCK(bl_ctrl_lock);

u8 ktd_backlight_is_dimming(void)
{
	u8 is_dimming;

	if (!g_bl_info) {
		pr_err("%s, no bl_info\n", __func__);
		return 0;
	}

	mutex_lock(&bl_ctrl_mutex);
	is_dimming = (current_brightness <= g_bl_info->min_brightness) ? 1 : 0;
	mutex_unlock(&bl_ctrl_mutex);

	return is_dimming;
}

static inline int find_tune_level(struct brt_value *brt_table, int nr_brt_level, int level)
{
	int i;

	if (unlikely(!brt_table) ||
		unlikely(nr_brt_level <= 0) ||
		unlikely(level > 255 || level < 0))
		return -1;

	if (!level)
		return 0;

	for (i = 0; i < nr_brt_level - 1; i++) {
		if (level <= brt_table[i].level &&
			level > brt_table[i + 1].level)
			return brt_table[i].tune_level;
	}

	return brt_table[nr_brt_level - 1].tune_level;
}

int ktd_backlight_set_brightness(struct ktd_bl_info *bl_info, int level)
{
	int pulse;
	int tune_level;
	unsigned long flags;

	struct brt_value *brt_table_ktd = bl_info->brt_table;
	unsigned int nr_brt_level = bl_info->sz_table;
	int gpio_bl_ctrl = bl_info->gpio_bl_ctrl;
	int gpio_bl_pwm_en = bl_info->gpio_bl_pwm_en;

	mutex_lock(&bl_ctrl_mutex);
	if (level < 0 || level > 255) {
		printk("%s, warning - out of range (level : %d)\n",
				__func__, level);
		goto out;
	}

	tune_level = find_tune_level(brt_table_ktd, nr_brt_level, level);
	if (tune_level < 0) {
		printk("%s, failed to find tune_level. (level : %d)\n",
				__func__, level);
		goto out;
	}

	printk("set_brightness : level(%d) tune (%d)\n",level, tune_level);
	current_brightness = level;

	if (!tune_level) {
		if (gpio_bl_pwm_en >= 0) {
			gpio_set_value(gpio_bl_pwm_en, 0);
			/* wait until vout gets stable */
			msleep(PWM_T_OFF_MS);
		}
		gpio_set_value(gpio_bl_ctrl, 0);
		msleep(VOUT_T_OFF_MS);
		prev_tune_level = 0;
		goto out;
	}

	if (unlikely(prev_tune_level < 0)) {
		gpio_set_value(gpio_bl_ctrl, 0);
		msleep(CTRL_T_OFF_MS);
		prev_tune_level = 0;
		printk(KERN_INFO "LCD Baklight init in boot time on kernel\n");
	}

	if (!prev_tune_level) {
		gpio_set_value(gpio_bl_ctrl, 1);
		udelay(CTRL_T_EN_US);
		prev_tune_level = 1;
	}

	pulse = (tune_level - prev_tune_level + MAX_BRIGHTNESS_IN_BLU)
		% MAX_BRIGHTNESS_IN_BLU;

	for (; pulse > 0; pulse--) {
		spin_lock_irqsave(&bl_ctrl_lock, flags);
		gpio_set_value(gpio_bl_ctrl, 0);
		udelay(CTRL_T_LOW_US);
		gpio_set_value(gpio_bl_ctrl, 1);
		spin_unlock_irqrestore(&bl_ctrl_lock, flags);
		udelay(CTRL_T_HIGH_US);
	}

	usleep_range(1000, 1000);
	if ((gpio_bl_pwm_en >= 0) &&
			(!gpio_get_value(gpio_bl_pwm_en))) {
		gpio_set_value(gpio_bl_pwm_en, 1);
		if (!gpio_get_value(gpio_bl_pwm_en))
			printk(KERN_ERR "%s, error : gpio_bl_pwm_en is %s\n",
					__func__, gpio_get_value(gpio_bl_pwm_en) ? "HIGH" : "LOW");
		usleep_range(10, 10);
	}
	prev_tune_level = tune_level;
out:
	mutex_unlock(&bl_ctrl_mutex);
	return 0;
}

void backlight_set_brightness(int level)
{
	if (!g_bl_info) {
		pr_err("%s, no bl_info\n", __func__);
		return;
	}

	ktd_backlight_set_brightness(g_bl_info, level);
	return;
}

static int ktd_backlight_update_status(struct backlight_device *bl)
{
	struct device *dev = bl->dev.parent;
	struct ktd_bl_info *bl_info =
		(struct ktd_bl_info *)dev->platform_data;
	int brightness = bl->props.brightness;

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return 0;
	}

	if (bl->props.power != FB_BLANK_UNBLANK ||
		bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	wakeup_brightness = brightness;
	if (!bl_enable && !current_brightness) {
		printk(KERN_INFO "[Backlight] no need to set backlight ---\n");
	} else {
		ktd_backlight_set_brightness(bl_info, brightness);
	}

	return 0;
}

static int ktd_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops ktd_backlight_ops = {
	.update_status	= ktd_backlight_update_status,
	.get_brightness	= ktd_backlight_get_brightness,
};

int ktd_backlight_disable(void)
{
	bl_enable = false;

	return 0;
}

int ktd_backlight_enable(void)
{
	bl_enable = true;

	return 0;
}

static int ktd_backlight_probe(struct platform_device *pdev)
{
	struct backlight_device *bd;
	struct backlight_properties props;
	struct ktd_bl_info *bl_info;

	if (!pdev)
		return 0;

	bl_info = (struct ktd_bl_info *)pdev->dev.platform_data;
	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return 0;
	}

	g_bl_info = bl_info;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = bl_info->max_brightness;

	bd = backlight_device_register(BACKLIGHT_DEV_NAME, &pdev->dev, NULL,
			&ktd_backlight_ops, &props);

	if (IS_ERR(bd)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		return PTR_ERR(bd);
	}

	bd->props.brightness = bl_info->def_brightness;
	current_brightness = bl_info->def_brightness;
	prev_tune_level =
		find_tune_level(bl_info->brt_table,
				bl_info->sz_table,
				bl_info->def_brightness);

	platform_set_drvdata(pdev, bd);

	if (bl_info->gpio_bl_ctrl >= 0) {
		if (gpio_request(bl_info->gpio_bl_ctrl, "BL_CTRL"))
			printk(KERN_ERR "request gpio(%d) failed\n",
					bl_info->gpio_bl_ctrl);
		gpio_direction_output(bl_info->gpio_bl_ctrl, 1);
	}

	if (bl_info->gpio_bl_pwm_en >= 0) {
		if (gpio_request(bl_info->gpio_bl_pwm_en, "BL_PWM_EN"))
			printk(KERN_ERR "request gpio(%d) failed\n",
					bl_info->gpio_bl_pwm_en);
		gpio_direction_output(bl_info->gpio_bl_pwm_en, 1);
	}

	bl_enable = true;
	backlight_update_status(bd);

	return 0;
}

static int ktd_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct ktd_bl_info *bl_info =
		(struct ktd_bl_info *)bd->dev.platform_data;

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return 0;
	}

	ktd_backlight_set_brightness(bl_info, 0);
	backlight_device_unregister(bd);

	return 0;
}

void ktd_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);
	struct ktd_bl_info *bl_info =
		(struct ktd_bl_info *)bd->dev.platform_data;

	if (!bl_info) {
		pr_err("%s, no platform data\n", __func__);
		return;
	}

	ktd_backlight_set_brightness(bl_info, 0);
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
