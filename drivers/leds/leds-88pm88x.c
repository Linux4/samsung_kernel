/*
 * 88PM886 RGB LED driver
 *
 * Copyright (C) 2014 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm88x.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/of.h>

#define PM88X_RGB_CLK_EN	(1 << 0)	/* RGB clock enable */

#define PM88X_RGB_I_MASK	(1 << 7)
#define PM88X_RGB_4_MA		(0 << 7)
#define PM88X_RGB_8_MA		(1 << 7)
#define PM88X_ON_TIME_MASK	(0x7f)

#define PM88X_RGB_CTRL1		(0x40)		/* current and on-time */
#define PM88X_RGB_CTRL2		(0x41)
#define PM88X_RGB_CTRL3		(0x42)

#define PM88X_RGB_CTRL4		(0x43)		/* led blinking period */
#define PM88X_RGB_CTRL5		(0x44)		/* led blinking off time */

#define PM88X_RGB_SPEED_MASK	(3 << 0)
#define PM88X_RGB_CTRL6		(0x45)		/* led breath speed */

#define PM88X_LED_EN0		(1 << 1)
#define PM88X_LED_EN1		(1 << 2)
#define PM88X_LED_EN2		(1 << 3)

#define PM88X_RGB_MODE_MASK	(1 << 4)
#define PM88X_RGB_SWITCH_MODE	(0 << 4)
#define PM88X_RGB_BREATH_MODE	(1 << 4)
#define PM88X_RGB_LED_EN_MASK	(7 << 1)
#define PM88X_RGB_PWM_EN_MASK	(1 << 0)

#define PM88X_RGB_PWM_EN	(1 << 0)

#define PM88X_RGB_CTRL7		(0x46)		/* mode and enable register */

enum {
	SWITCH_MODE,
	BREATH_MODE,
};

struct pm88x_rgb_info {
	struct led_classdev cdev;

	struct work_struct work;
	struct regmap *map;

	struct pm88x_chip *chip;
	struct mutex lock;
	const char *name;
	const char *trigger;

	int led_en;
	int led_ctrl_reg;
	int id;

	u8 mode;
	u8 breath_speed;
	u8 pwm_cur;
	u8 on_percent;

	u8 brightness;
	u8 current_brightness;
};

static void pm88x_rgb_enable(struct pm88x_rgb_info *info, bool enable)
{
	unsigned int mask, value;

	if (!info) {
		pr_err("%s: No chip information!\n", __func__);
		return;
	}

	mask = info->led_en | PM88X_RGB_PWM_EN_MASK;

	if (enable) {
		/* enable rgb clock */
		regmap_update_bits(info->map, PM88X_CLK_CTRL1,
				   PM88X_RGB_CLK_EN, PM88X_RGB_CLK_EN);
		value = mask;
		regmap_update_bits(info->map, PM88X_RGB_CTRL7, mask, value);
	} else {
		/* disble rgb */
		regmap_update_bits(info->map, PM88X_RGB_CTRL7, info->led_en,
				   ~(info->led_en));
		regmap_read(info->map, PM88X_RGB_CTRL7, &value);
		if (!(value & 0x0e)) {
			/* disable rgb clock */
			regmap_update_bits(info->map, PM88X_CLK_CTRL1,
					   PM88X_RGB_CLK_EN, ~PM88X_RGB_CLK_EN);
			/* disable PWM if there is no rgb enabled */
			regmap_update_bits(info->map, PM88X_RGB_CTRL7,
					   PM88X_RGB_PWM_EN_MASK,
					   ~PM88X_RGB_PWM_EN);
		}

	}
}

static void pm88x_rgb_work(struct work_struct *work)
{
	struct pm88x_rgb_info *info;

	info = container_of(work, struct pm88x_rgb_info, work);
	if (!info) {
		pr_err("%s: No chip information!\n", __func__);
		return;
	}

	mutex_lock(&info->lock);

	if ((info->current_brightness == 0) && (info->brightness))
		pm88x_rgb_enable(info, true);

	if (info->brightness == 0)
		pm88x_rgb_enable(info, false);
	else
		/* we don't change the current generator */
		regmap_update_bits(info->map, info->led_ctrl_reg,
				  PM88X_ON_TIME_MASK, info->brightness);

	info->current_brightness = info->brightness;

	mutex_unlock(&info->lock);
}

static int pm88x_rgb_dt_init(struct device_node *np,
			     struct pm88x_rgb_info *info)
{
	int ret;

	if (!info) {
		pr_err("%s: No chip information!\n", __func__);
		return -ENODEV;
	}

	if (of_get_property(np, "pm88x-rgb-breath-mode", NULL)) {
		info->mode = BREATH_MODE;
		ret = of_property_read_u8(np, "pm88x-rgb-breath-speed",
					  &info->breath_speed);
		if (ret)
			info->breath_speed = 0;
	} else {
		info->mode = SWITCH_MODE;
	}

	ret = of_property_read_u8(np, "pm88x-rgb-current", &info->pwm_cur);
	if (ret < 0)
		info->pwm_cur = 0;

	ret = of_property_read_u8(np, "pm88x-rgb-on-percent",
				  &info->on_percent);
	if (ret < 0)
		info->on_percent = 50;

	if (info->on_percent > 100)
		info->on_percent = 50;
	if (info->on_percent < 0)
		info->on_percent = 50;

	ret = of_property_read_string(np, "led-name", &info->name);
	if (ret < 0)
		return -EINVAL;

	ret = of_property_read_string(np, "led-trigger-name", &info->trigger);
	if (ret < 0)
		info->trigger = "timer";

	return 0;
}

static void pm88x_rgb_bright_set(struct led_classdev *cdev,
				 enum led_brightness value)
{
	struct pm88x_rgb_info *info;
	info = container_of(cdev, struct pm88x_rgb_info, cdev);
	if (!info) {
		pr_err("%s: No chip information!\n", __func__);
		return;
	}

	info->brightness = value >> 1;
	schedule_work(&info->work);
}

/*
 * delay_off: the period that PWM is off, blk_off
 * delay_on: the period that PWM is on, blinking_period - blk_off
 */
static int pm88x_rgb_blink_set(struct led_classdev *cdev,
			       unsigned long *delay_on,
			       unsigned long *delay_off)
{
	struct pm88x_rgb_info *info;
	unsigned long period, tmp;

	info = container_of(cdev, struct pm88x_rgb_info, cdev);
	if (!info) {
		pr_err("%s: No chip information!\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&info->lock);

	if (*delay_on == 0 && *delay_off == 0) {
		*delay_on = 0x7f;
		*delay_off = 0x7f;
	}

	if (info->mode == BREATH_MODE) {
		if (*delay_on < 0x40)
			info->breath_speed = 0x0;
		else if (*delay_on < 0x80)
			info->breath_speed = 0x1;
		else if (*delay_on < 0xb0)
			info->breath_speed = 0x2;
		else
			info->breath_speed = 0x3;

		regmap_update_bits(info->map, PM88X_RGB_CTRL6,
				   PM88X_RGB_SPEED_MASK, info->breath_speed);
		goto out;
	}

	/* Wer'e not in breathing mode, so, as a fixup,
	 * we should set breath_speed to 0
	 */
	regmap_update_bits(info->map, PM88X_RGB_CTRL6,
				   PM88X_RGB_SPEED_MASK, 0x0);

	/*
	 * the frequence of PWM is 128HZ, the period is 7.8125ms
	 * then the blinking period is 7.8125 * LED_BLK_PER
	 * because LED_BLK_PER: [0x1a, 0xff] -> blinking_period: [200ms, 2s]
	 *
	 * the blk_off is [0, 2s]
	 *
	 * if blk_off > blinking_period, then the led is turned off
	 *
	 * for PWM:
	 * high period / low period = (blinking_period - blk_off) / blk_off;
	 *
	 * the value passed from upper layer should be in milliseconds;
	 */
	if (*delay_off > 2000)
		*delay_off = 2000;
	period = *delay_on + *delay_off;
	if (period > 2000)
		period = 2000;
	if (period < 200)
		period = 200;

	tmp = *delay_off * 10000 / 78125;
	period = period * 10000 / 78125;

	regmap_write(info->map, PM88X_RGB_CTRL4, period);
	regmap_write(info->map, PM88X_RGB_CTRL5, tmp);

out:
	mutex_unlock(&info->lock);
	return 0;
}

static int pm88x_rgb_setup(struct pm88x_rgb_info *info)
{
	/* set the current generator and on-time */
	u8 pwm_cur, on_percent;

	if (!info) {
		pr_err("%s: No chip information!\n", __func__);
		return -ENODEV;
	}

	pwm_cur = info->pwm_cur ? PM88X_RGB_8_MA : PM88X_RGB_4_MA;

	on_percent = info->on_percent;
	on_percent = (on_percent > 100) ? 100 : on_percent;
	/* [0%, 100%] -> [0, 128] */
	on_percent = 0x7f * on_percent / 100;

	mutex_lock(&info->lock);
	regmap_write(info->map, info->led_ctrl_reg, pwm_cur | on_percent);

	/* set the rgb working mode */
	if (info->mode == BREATH_MODE) {
		regmap_update_bits(info->map, PM88X_RGB_CTRL7,
				   PM88X_RGB_MODE_MASK, PM88X_RGB_BREATH_MODE);

		regmap_update_bits(info->map, PM88X_RGB_CTRL6,
				   PM88X_RGB_SPEED_MASK, info->breath_speed);

	} else {
		regmap_update_bits(info->map, PM88X_RGB_CTRL7,
				   PM88X_RGB_MODE_MASK, PM88X_RGB_SWITCH_MODE);
	}
	pm88x_rgb_enable(info, false);

	mutex_unlock(&info->lock);
	return 0;
}

static ssize_t led_mode_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct pm88x_rgb_info *info;
	struct led_classdev *cdev;
	int s;

	cdev = dev_get_drvdata(dev);
	info = container_of(cdev, struct pm88x_rgb_info, cdev);

	s = sprintf(buf, "led mode: %s\n", info->mode == BREATH_MODE ? "breath" : "switch");
	return s;
}

static ssize_t led_mode_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct pm88x_rgb_info *info;
	struct led_classdev *cdev;
	int ret, led_mode, tmp;

	cdev = dev_get_drvdata(dev);
	info = container_of(cdev, struct pm88x_rgb_info, cdev);

	ret = sscanf(buf, "%d", &tmp);
	if (ret < 0) {
		dev_dbg(dev, "%s: get led mode error, set to breath mode as default\n", __func__);
		led_mode = BREATH_MODE;
	} else
		led_mode = tmp ? BREATH_MODE : SWITCH_MODE;

	mutex_lock(&info->lock);
	/* set the rgb working mode */
	if (info->mode == SWITCH_MODE && led_mode == BREATH_MODE) {
		regmap_update_bits(info->map, PM88X_RGB_CTRL7,
				   PM88X_RGB_MODE_MASK, PM88X_RGB_BREATH_MODE);
		regmap_update_bits(info->map, PM88X_RGB_CTRL6,
				   PM88X_RGB_SPEED_MASK, info->breath_speed);
	} else if (info->mode == BREATH_MODE && led_mode == SWITCH_MODE) {
		regmap_update_bits(info->map, PM88X_RGB_CTRL7,
				   PM88X_RGB_MODE_MASK, PM88X_RGB_SWITCH_MODE);
	}
	info->mode = led_mode;
	mutex_unlock(&info->lock);

	return size;
}

static DEVICE_ATTR(led_mode, S_IRUGO | S_IWUSR, led_mode_show, led_mode_store);

static int pm88x_rgb_probe(struct platform_device *pdev)
{
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm88x_rgb_info *info;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(struct pm88x_rgb_info),
			    GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	switch (pdev->id) {
	case PM88X_RGB_LED0:
		info->led_en = PM88X_LED_EN0;
		info->led_ctrl_reg = PM88X_RGB_CTRL1;
		break;
	case PM88X_RGB_LED1:
		info->led_en = PM88X_LED_EN1;
		info->led_ctrl_reg = PM88X_RGB_CTRL2;
		break;
	case PM88X_RGB_LED2:
		info->led_en = PM88X_LED_EN2;
		info->led_ctrl_reg = PM88X_RGB_CTRL3;
		break;
	}

	ret = pm88x_rgb_dt_init(node, info);
	if (ret < 0)
		return -ENODEV;

	info->id = pdev->id;
	info->chip = chip;
	info->map = chip->base_regmap;

	info->current_brightness = 0;
	info->cdev.name = info->name;
	info->cdev.brightness_set = pm88x_rgb_bright_set;
	info->cdev.blink_set = pm88x_rgb_blink_set;
	info->cdev.default_trigger = info->trigger;

	platform_set_drvdata(pdev, info);

	mutex_init(&info->lock);

	ret = pm88x_rgb_setup(info);
	if (ret < 0)
		return -ENODEV;

	INIT_WORK(&info->work, pm88x_rgb_work);

	ret = led_classdev_register(&pdev->dev, &info->cdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fairgb to register LED: %d\n", ret);
		return ret;
	}
	pm88x_rgb_bright_set(&info->cdev, 0);

	ret = device_create_file(info->cdev.dev, &dev_attr_led_mode);
	if (ret < 0) {
		dev_err(&pdev->dev, "device attr create fail: %d\n", ret);
		goto DEV_FAIL;
	}
	return 0;

DEV_FAIL:
	led_classdev_unregister(&info->cdev);
	return ret;
}

static int pm88x_rgb_remove(struct platform_device *pdev)
{
	struct pm88x_rgb_info *info = platform_get_drvdata(pdev);

	if (info)
		led_classdev_unregister(&info->cdev);
	return 0;
}

static const struct of_device_id pm88x_rgb_dt_match[] = {
	{ .compatible = "marvell,88pm88x-rgb0", },
	{ .compatible = "marvell,88pm88x-rgb1", },
	{ .compatible = "marvell,88pm88x-rgb2", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm88x_rgb_dt_match);

static struct platform_driver pm88x_rgb_driver = {
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "88pm88x-rgb",
		.of_match_table = of_match_ptr(pm88x_rgb_dt_match),
	},
	.probe	= pm88x_rgb_probe,
	.remove	= pm88x_rgb_remove,
};
module_platform_driver(pm88x_rgb_driver);

MODULE_DESCRIPTION("RGB LED driver for Marvell 88PM88X");
MODULE_AUTHOR("Yi Zhang <yizhang@marvell.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:88PM88X-rgb");
