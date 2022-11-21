/*
 * TI LM3632 Backlight Driver
 *
 * Copyright 2015 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/mfd/lm3632.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
//#include "./lm3632.h"

#define LM3632_MAX_BRIGHTNESS		2047

#define DEFAULT_BL_NAME			"lcd-backlight"

enum lm3632_bl_ctrl_mode {
	LMU_BL_I2C,
	LMU_BL_PWM,
};
struct lm3632_bl {
	struct device *dev;
	struct backlight_device *bl_dev;

	struct lm3632 *lm3632;
	struct lm3632_backlight_platform_data *pdata;
	enum lm3632_bl_ctrl_mode mode;

	struct pwm_device *pwm;
};

static int lm3632_bl_string_configure(struct lm3632_bl *lm3632_bl, int enable)
{
	u8 val;

	if (enable) {
		if (lm3632_bl->pdata->is_full_strings)
			val = LM3632_BL_TWO_STRINGS;
		else
			val = LM3632_BL_ONE_STRING;
	} else {
		val = 0;
	}

	return lm3632_update_bits(lm3632_bl->lm3632, LM3632_REG_ENABLE,
				  LM3632_BL_STRING_MASK, val);
}

static int lm3632_bl_enable(struct lm3632_bl *lm3632_bl, int enable)
{
	int ret;

	ret = lm3632_bl_string_configure(lm3632_bl, enable);
	if (ret) {
		dev_err(lm3632_bl->dev,
			"Backlight string config error: %d\n", ret);
		return ret;
	}

	return lm3632_update_bits(lm3632_bl->lm3632, LM3632_REG_ENABLE,
			LM3632_BL_EN_MASK, enable << LM3632_BL_EN_SHIFT);
}

static void lm3632_bl_pwm_ctrl(struct lm3632_bl *lm3632_bl, int br, int max_br)
{
	unsigned int period;
	unsigned int duty;
	struct pwm_device *pwm;

	if (!lm3632_bl->pdata)
		return;

	period = lm3632_bl->pdata->pwm_period;
	duty = br * period / max_br;

	/* Request a PWM device with the consumer name */
	if (!lm3632_bl->pwm) {
		pwm = devm_pwm_get(lm3632_bl->dev, "lm3632-backlight");
		if (IS_ERR(pwm)) {
			dev_err(lm3632_bl->dev, "can not get PWM device\n");
			return;
		}
		lm3632_bl->pwm = pwm;
	}

	pwm_config(lm3632_bl->pwm, duty, period);
	if (duty)
		pwm_enable(lm3632_bl->pwm);
	else
		pwm_disable(lm3632_bl->pwm);
}

static int lm3632_bl_set_brightness(struct lm3632_bl *lm3632_bl, int val)
{
	u8 data;
	int ret;


	data = val & LM3632_BRT_LSB_MASK;
	ret = lm3632_update_bits(lm3632_bl->lm3632, LM3632_REG_BRT_LSB,
				 LM3632_BRT_LSB_MASK, data);
	if (ret)
		return ret;

	data = (val >> LM3632_BRT_MSB_SHIFT) & 0xFF;

	return lm3632_write_byte(lm3632_bl->lm3632, LM3632_REG_BRT_MSB,
				 data);
}

static int lm3632_bl_update_status(struct backlight_device *bl_dev)
{
	struct lm3632_bl *lm3632_bl = bl_get_data(bl_dev);
	int brt;
	int ret;
	

	if (bl_dev->props.state & BL_CORE_SUSPENDED)
		brt = 0;
	else
		brt = bl_dev->props.brightness;

	printk("zyun lm3632 update status=%d\n", brt);

	if (brt > 0)
		ret = lm3632_bl_enable(lm3632_bl, 1);
	else
		ret = lm3632_bl_enable(lm3632_bl, 0);

	if (ret)
		return ret;
  
       // we don't want to update brightness here
	if (lm3632_bl->mode == LMU_BL_PWM)
		lm3632_bl_pwm_ctrl(lm3632_bl, brt,
				   bl_dev->props.max_brightness);
	else
		ret = lm3632_bl_set_brightness(lm3632_bl, brt);

	return ret;
}

static int lm3632_bl_get_brightness(struct backlight_device *bl_dev)
{
	return bl_dev->props.brightness;
}

static const struct backlight_ops lm3632_bl_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3632_bl_update_status,
	.get_brightness = lm3632_bl_get_brightness,
};

static int lm3632_bl_register(struct lm3632_bl *lm3632_bl)
{
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	struct lm3632_backlight_platform_data *pdata = lm3632_bl->pdata;
	char name[20];

	props.type = BACKLIGHT_PLATFORM;
	props.brightness = 1637;
	props.max_brightness = LM3632_MAX_BRIGHTNESS;

	if (!pdata || !pdata->name)
		snprintf(name, sizeof(name), "%s", DEFAULT_BL_NAME);
	else
		snprintf(name, sizeof(name), "%s", pdata->name);

	bl_dev = backlight_device_register(name, lm3632_bl->dev, lm3632_bl,
					   &lm3632_bl_ops, &props);
	if (IS_ERR(bl_dev))
		return PTR_ERR(bl_dev);

	lm3632_bl->bl_dev = bl_dev;

	return 0;
}

static void lm3632_bl_unregister(struct lm3632_bl *lm3632_bl)
{
	if (lm3632_bl->bl_dev)
		backlight_device_unregister(lm3632_bl->bl_dev);
}

static int lm3632_bl_set_ovp(struct lm3632_bl *lm3632_bl)
{
	return lm3632_update_bits(lm3632_bl->lm3632, LM3632_REG_CONFIG1,
				  LM3632_OVP_MASK, LM3632_OVP_25V);
}

static int lm3632_bl_set_swfreq(struct lm3632_bl *lm3632_bl)
{
	return lm3632_update_bits(lm3632_bl->lm3632, LM3632_REG_CONFIG2,
				  LM3632_SWFREQ_MASK, LM3632_SWFREQ_1MHZ);
}

static int lm3632_bl_bias_output(struct lm3632_bl *lm3632_bl)
{
	return lm3632_update_bits(lm3632_bl->lm3632, 0x0C,
				  BIT(0), BIT(0));
}


static int lm3632_bl_set_ctrl_mode(struct lm3632_bl *lm3632_bl)
{
	struct lm3632_backlight_platform_data *pdata = lm3632_bl->pdata;
#if 0
	if (pdata->pwm_period > 0)
		lm3632_bl->mode = LMU_BL_PWM;
	else
#endif
		lm3632_bl->mode = LMU_BL_I2C;

	return lm3632_update_bits(lm3632_bl->lm3632, LM3632_REG_IO_CTRL,
				  LM3632_PWM_MASK,
				  lm3632_bl->mode << LM3632_PWM_SHIFT);
}

static int lm3632_bl_configure(struct lm3632_bl *lm3632_bl)
{
	int ret;

	/* Select OVP level */
	ret = lm3632_bl_set_ovp(lm3632_bl);
	if (ret)
		return ret;

	/* Select switch frequency */
	ret = lm3632_bl_set_swfreq(lm3632_bl);
	if (ret)
		return ret;

	/* Set vpos/vneg out */
	ret = lm3632_bl_bias_output(lm3632_bl);
	if (ret)
		return ret;
	/* Backlight control mode - PWM or I2C */
	return lm3632_bl_set_ctrl_mode(lm3632_bl);
}

static int lm3632_bl_parse_dt(struct device *dev, struct lm3632_bl *lm3632_bl)
{
	struct device_node *node = dev->of_node;
	struct lm3632_backlight_platform_data *pdata;
	int brightness = 0;
	int ret;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	/* Channel name */
	of_property_read_string(node, "bl-name", &pdata->name);

	/* String configuration */
	if (of_find_property(node, "full-strings-used", NULL))
		pdata->is_full_strings = true;

	/* PWM mode */
	of_property_read_u32(node, "pwm-period", &pdata->pwm_period);
	of_property_read_u32(node, "pwm-max-brightness", &brightness);

	if (pdata->pwm_period > 0) {
		if (brightness == 0) {
			dev_err(lm3632_bl->dev,
			"PWM max brightness should be greater than 0\n");
			return -EINVAL;
		}

		ret = lm3632_bl_set_brightness(lm3632_bl, brightness);
		if (ret) {
			dev_err(lm3632_bl->dev,
				"PWM max brightness set error: %d\n", ret);
			return ret;
		}
	}

	lm3632_bl->pdata = pdata;

	return 0;
}

static int lm3632_bl_probe(struct platform_device *pdev)
{
	struct lm3632 *lm3632 = dev_get_drvdata(pdev->dev.parent);
	struct lm3632_backlight_platform_data *pdata = lm3632->pdata->bl_pdata;
	struct lm3632_bl *lm3632_bl;
	int ret;
	printk("zyun lm3632_bl_probe\n");
	printk("zyun get lm3632 adress=0x%x\n",(int)lm3632);
	lm3632_bl = devm_kzalloc(&pdev->dev, sizeof(*lm3632_bl), GFP_KERNEL);
	if (!lm3632_bl)
		return -ENOMEM;

	lm3632_bl->dev = &pdev->dev;
	lm3632_bl->lm3632 = lm3632;
	lm3632_bl->pdata = pdata;

	if (!lm3632_bl->pdata) {
		if (IS_ENABLED(CONFIG_OF))
			ret = lm3632_bl_parse_dt(&pdev->dev, lm3632_bl);
		else
			return -ENODEV;

		if (ret)
			return ret;
	}

	platform_set_drvdata(pdev, lm3632_bl);

	/* Backlight configuration */
	ret = lm3632_bl_configure(lm3632_bl);
	if (ret) {
		dev_err(&pdev->dev, "backlight config err: %d\n", ret);
		return ret;
	}

	if (gpio_request(52, "LCD_3V3")) {
		printk("Failed ro request GPIO_%d \n",52);
		return -ENODEV;
	}
	if (gpio_request(53, "CABC_EN1")) {
		printk("Failed ro request GPIO_%d \n",53);
		return -ENODEV;
	}

	gpio_direction_output(53, 0);
	gpio_direction_output(52, 0);
	gpio_set_value(53,0);
	gpio_set_value(52,0);
	
	gpio_set_value(53,1);
	mdelay(5);
	gpio_set_value(52,1);

	/* Register backlight subsystem */
	ret = lm3632_bl_register(lm3632_bl);
	if (ret) {
		dev_err(&pdev->dev, "register backlight err: %d\n", ret);
		return ret;
	}

	backlight_update_status(lm3632_bl->bl_dev);

	return 0;
}

static int lm3632_bl_remove(struct platform_device *pdev)
{
	struct lm3632_bl *lm3632_bl = platform_get_drvdata(pdev);
	struct backlight_device *bl_dev = lm3632_bl->bl_dev;

	bl_dev->props.brightness = 0;
	backlight_update_status(bl_dev);
	lm3632_bl_unregister(lm3632_bl);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id lm3632_bl_of_match[] = {
	{ .compatible = "ti,lm3632-backlight", },
	{ }
};
MODULE_DEVICE_TABLE(of, lm3632_bl_of_match);
#endif

static struct platform_driver lm3632_bl_driver = {
	.probe = lm3632_bl_probe,
	.remove = lm3632_bl_remove,
	.driver = {
		.name = "lm3632-backlight",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lm3632_bl_of_match),
	},
};
module_platform_driver(lm3632_bl_driver);

MODULE_DESCRIPTION("TI LM3632 Backlight Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:lm3632-backlight");
