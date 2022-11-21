/*
 * RGB-led driver for Maxim MAX77828
 *
 * Copyright (C) 2013 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/mfd/max77828.h>
#include <linux/mfd/max77828-private.h>
#include <linux/leds-max77828-rgb.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/sec_class.h>

#define SEC_LED_SPECIFIC

/* MAX77828_REG_LEDEN */
#define max77828_LED3_EN	0x08
#define max77828_LED2_EN	0x04
#define max77828_LED1_EN	0x02
#define max77828_LED0_EN	0x01

/* MAX77828_REG_LED0BRT */
#define MAX77828_LED0BRT	0xFF

/* MAX77828_REG_LED1BRT */
#define MAX77828_LED1BRT	0xFF

/* MAX77828_REG_LED2BRT */
#define MAX77828_LED2BRT	0xFF

/* MAX77828_REG_LED3BRT */
#define MAX77828_LED3BRT	0xFF

/* MAX77828_REG_LEDBLNK */
#define MAX77828_LEDBLINKD	0xF0
#define MAX77828_LEDBLINKP	0x0F

/* MAX77828_REG_LEDRMP */
#define MAX77828_RAMPUP		0xF0
#define MAX77828_RAMPDN		0x0F

#define LED_R_MASK		0x00FF0000
#define LED_G_MASK		0x0000FF00
#define LED_B_MASK		0x000000FF
#define LED_MAX_CURRENT		0xFF

static u8 led_dynamic_current = 0x14;
static u8 led_lowpower_mode = 0x0;

enum max77828_led_color {
	WHITE = 0,
	RED = 1,
	GREEN = 2,
	BLUE = 3
};
enum max77828_led_pattern {
	PATTERN_OFF,
	CHARGING,
	CHARGING_ERR,
	MISSED_NOTI,
	LOW_BATTERY,
	FULLY_CHARGED,
	POWERING,
};

static struct device *led_dev;

struct max77828_rgb {
	struct led_classdev led[4];
	struct i2c_client *i2c;
	unsigned int delay_on_times_ms;
	unsigned int delay_off_times_ms;
};

static int max77828_rgb_number(struct led_classdev *led_cdev,
				struct max77828_rgb **p)
{
	const struct device *parent = led_cdev->dev->parent;
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(parent);
	int i;

	*p = max77828_rgb;

	for (i = 0; i < 4; i++) {
		if (led_cdev == &max77828_rgb->led[i])
			return i;
	}

	return -ENODEV;
}

static void max77828_rgb_set(struct led_classdev *led_cdev,
				unsigned int brightness)
{
	const struct device *parent = led_cdev->dev->parent;
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(parent);
	struct device *dev;
	int n;
	int ret;

	ret = max77828_rgb_number(led_cdev, &max77828_rgb);
	if (IS_ERR_VALUE(ret)) {
		dev_err(led_cdev->dev,
			"max77828_rgb_number() returns %d.\n", ret);
		return;
	}

	dev = led_cdev->dev;
	n = ret;

	if (brightness == LED_OFF) {
		/* Flash OFF */
		ret = max77828_update_reg(max77828_rgb->i2c,
					MAX77828_LED_REG_LEDEN, 0 , 1 << n);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write LEDEN : %d\n", ret);
			return;
		}
	} else {
		/* Set current */
		ret = max77828_write_reg(max77828_rgb->i2c,
				MAX77828_LED_REG_LED0BRT + n, brightness);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write LEDxBRT : %d\n", ret);
			return;
		}
		/* Flash ON */
		ret = max77828_update_reg(max77828_rgb->i2c,
				MAX77828_LED_REG_LEDEN, 0xFF, 1 << n);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write FLASH_EN : %d\n", ret);
			return;
		}
	}
}

static unsigned int max77828_rgb_get(struct led_classdev *led_cdev)
{
	const struct device *parent = led_cdev->dev->parent;
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(parent);
	struct device *dev;
	int n;
	int ret;
	u8 value;

	ret = max77828_rgb_number(led_cdev, &max77828_rgb);
	if (IS_ERR_VALUE(ret)) {
		dev_err(led_cdev->dev,
			"max77828_rgb_number() returns %d.\n", ret);
		return 0;
	}
	n = ret;

	dev = led_cdev->dev;

	/* Get status */
	ret = max77828_read_reg(max77828_rgb->i2c,
				MAX77828_LED_REG_LEDEN, &value);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't read LEDEN : %d\n", ret);
		return 0;
	}
	if (!(value & (1 << n)))
		return LED_OFF;

	/* Get current */
	ret = max77828_read_reg(max77828_rgb->i2c,
				MAX77828_LED_REG_LED0BRT + n, &value);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't read LED0BRT : %d\n", ret);
		return 0;
	}

	return value;
}

static int max77828_rgb_ramp(struct device *dev, int ramp_up, int ramp_down)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	int value;
	int ret;

	ramp_up /= 100;
	ramp_down /= 100;
	value = (ramp_down) | (ramp_up << 4);
	ret = max77828_write_reg(max77828_rgb->i2c,
					MAX77828_LED_REG_LEDRMP, value);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't write REG_LEDRMP : %d\n", ret);
		return -ENODEV;
	}

	return 0;
}

static int max77828_rgb_blink(struct device *dev,
				unsigned int delay_on, unsigned int delay_off)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	int value;
	int ret;

	if (delay_on == 0 && delay_off == 0) {
		ret = max77828_write_reg(max77828_rgb->i2c,
					MAX77828_LED_REG_LEDBLNK, 0x50);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write REG_LEDBLNK : %d\n", ret);
			return -ENODEV;
		}
	} else {
		value = delay_on + delay_off;
		if (value <= 6000)
			value = (value - 1000) / 500;
		else if (value < 8000)
			value = (value - 6000) / 1000;
		else if (value < 12000)
			value = (value - 8000) / 2000;
		else
			return -EINVAL;

		value |= ((delay_on / 200) <<  4);
		ret = max77828_write_reg(max77828_rgb->i2c,
					MAX77828_LED_REG_LEDBLNK, value);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write REG_LEDBLNK : %d\n", ret);
			return -EINVAL;
		}
	}

	return 0;
}

#ifdef CONFIG_OF
static struct max77828_rgb_platform_data
			*max77828_rgb_parse_dt(struct device *dev)
{
	struct max77828_rgb_platform_data *pdata;
	struct device_node *np;
	int ret;
	int i;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	np = of_find_node_by_name(NULL, "rgb");
	if (unlikely(np == NULL)) {
		dev_err(dev, "rgb node not found\n");
		devm_kfree(dev, pdata);
		return ERR_PTR(-EINVAL);
	}

	for (i = 0; i < 4; i++)	{
		ret = of_property_read_string_index(np, "rgb-name", i,
						(const char **)&pdata->name[i]);
		if (IS_ERR_VALUE(ret)) {
			devm_kfree(dev, pdata);
			return ERR_PTR(ret);
		}
	}

	return pdata;
}
#endif

static void max77828_rgb_reset(struct device *dev)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	max77828_rgb_set(&max77828_rgb->led[RED], LED_OFF);
	max77828_rgb_set(&max77828_rgb->led[GREEN], LED_OFF);
	max77828_rgb_set(&max77828_rgb->led[BLUE], LED_OFF);
	max77828_rgb_ramp(dev, 0, 0);
}

static ssize_t store_max77828_rgb_lowpower(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int ret;
	u8 led_lowpower;

	ret = kstrtou8(buf, 0, &led_lowpower);
	if (ret != 0) {
		dev_err(dev, "fail to get led_lowpower.\n");
		return count;
	}

	led_lowpower_mode = led_lowpower;

	dev_dbg(dev, "led_lowpower mode set to %i\n", led_lowpower);

	return count;
}
static ssize_t store_max77828_rgb_brightness(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int ret;
	u8 brightness;

	ret = kstrtou8(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get led_brightness.\n");
		return count;
	}

	led_lowpower_mode = 0;

	if (brightness > LED_MAX_CURRENT)
		brightness = LED_MAX_CURRENT;

	led_dynamic_current = brightness;

	dev_dbg(dev, "led brightness set to %i\n", brightness);

	return count;
}

static ssize_t store_max77828_rgb_pattern(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	unsigned int mode = 0;
	int ret;

	ret = sscanf(buf, "%1d", &mode);
	if (ret == 0) {
		dev_err(dev, "fail to get led_pattern mode.\n");
		return count;
	}

	if (mode > POWERING)
		return count;

	/* Set all LEDs Off */
	max77828_rgb_reset(dev);
	if (mode == PATTERN_OFF)
		return count;

	/* Set to low power consumption mode */
	if (led_lowpower_mode == 1)
		led_dynamic_current = 0x5;
	else
		led_dynamic_current = 0x14;

	switch (mode) {

	case CHARGING:
		max77828_rgb_set(&max77828_rgb->led[RED], led_dynamic_current);
		max77828_rgb_blink(dev, 0, 0);
		break;
	case CHARGING_ERR:
		max77828_rgb_set(&max77828_rgb->led[RED], led_dynamic_current);
		max77828_rgb_blink(dev, 600, 400);
		break;
	case MISSED_NOTI:
		max77828_rgb_set(&max77828_rgb->led[BLUE], led_dynamic_current);
		max77828_rgb_blink(dev, 600, 4900);
		break;
	case LOW_BATTERY:
		max77828_rgb_set(&max77828_rgb->led[RED], led_dynamic_current);
		max77828_rgb_blink(dev, 600, 4900);
		break;
	case FULLY_CHARGED:
		max77828_rgb_set(&max77828_rgb->led[GREEN], led_dynamic_current);
		max77828_rgb_blink(dev, 0, 0);
		break;
	case POWERING:
		max77828_rgb_set(&max77828_rgb->led[GREEN], led_dynamic_current);
		max77828_rgb_set(&max77828_rgb->led[BLUE], led_dynamic_current);
		max77828_rgb_ramp(dev, 1500, 700);
		max77828_rgb_blink(dev, 200, 2500);
		break;
	default:
		break;
	}

	return count;
}

static ssize_t store_max77828_rgb_blink(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	int led_brightness = 0;
	int delay_on_time = 0;
	int delay_off_time = 0;
	u8 led_r_brightness = 0;
	u8 led_g_brightness = 0;
	u8 led_b_brightness = 0;
	int ret;

	ret = sscanf(buf, "0x%8x %5d %5d", &led_brightness,
					&delay_on_time, &delay_off_time);
	if (ret == 0) {
		dev_err(dev, "fail to get led_blink value.\n");
		return count;
	}

	/*Reset led*/
	max77828_rgb_reset(dev);

	led_r_brightness = (led_brightness & LED_R_MASK) >> 16;
	led_g_brightness = (led_brightness & LED_G_MASK) >> 8;
	led_b_brightness = led_brightness & LED_B_MASK;

	/* In user case, LED current is restricted to less than 2mA */
	led_r_brightness = (led_r_brightness * led_dynamic_current) / LED_MAX_CURRENT;
	led_g_brightness = (led_g_brightness * led_dynamic_current) / LED_MAX_CURRENT;
	led_b_brightness = (led_b_brightness * led_dynamic_current) / LED_MAX_CURRENT;

	if (led_r_brightness) {
		max77828_rgb_set(&max77828_rgb->led[RED], led_r_brightness);
	}
	if (led_g_brightness) {
		max77828_rgb_set(&max77828_rgb->led[GREEN], led_g_brightness);
	}
	if (led_b_brightness) {
		max77828_rgb_set(&max77828_rgb->led[BLUE], led_b_brightness);
	}
	/*Set LED blink mode*/
	max77828_rgb_blink(dev, delay_on_time, delay_off_time);

	dev_dbg(dev, "led_blink is called, Color:0x%X Brightness:%i\n",
			led_brightness, led_dynamic_current);
	return count;
}

static ssize_t store_led_r(struct device *dev,
			struct device_attribute *devattr,
				const char *buf, size_t count)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}
	if (brightness != 0) {
		max77828_rgb_set(&max77828_rgb->led[RED], brightness);
		max77828_rgb_blink(dev, 0, 0);
	} else {
		max77828_rgb_set(&max77828_rgb->led[RED], LED_OFF);
	}
out:
	return count;
}
static ssize_t store_led_g(struct device *dev,
			struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}
	if (brightness != 0) {
		max77828_rgb_set(&max77828_rgb->led[GREEN], brightness);
		max77828_rgb_blink(dev, 0, 0);
	} else {
		max77828_rgb_set(&max77828_rgb->led[GREEN], LED_OFF);
	}
out:
	return count;
}
static ssize_t store_led_b(struct device *dev,
		struct device_attribute *devattr,
		const char *buf, size_t count)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}
	if (brightness != 0) {
		max77828_rgb_set(&max77828_rgb->led[BLUE], brightness);
		max77828_rgb_blink(dev, 0, 0);
	} else	{
		max77828_rgb_set(&max77828_rgb->led[BLUE], LED_OFF);
	}
out:
	return count;
}

/* Added for led common class */
static ssize_t led_delay_on_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", max77828_rgb->delay_on_times_ms);
}

static ssize_t led_delay_on_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	unsigned int time;

	if (kstrtouint(buf, 0, &time)) {
		dev_err(dev, "can not write led_delay_on\n");
		return count;
	}

	max77828_rgb->delay_on_times_ms = time;

	return count;
}

static ssize_t led_delay_off_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", max77828_rgb->delay_off_times_ms);
}

static ssize_t led_delay_off_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	unsigned int time;

	if (kstrtouint(buf, 0, &time)) {
		dev_err(dev, "can not write led_delay_off\n");
		return count;
	}

	max77828_rgb->delay_off_times_ms = time;

	return count;
}

static ssize_t led_blink_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	const struct device *parent = dev->parent;
	struct max77828_rgb *max77828_rgb_num = dev_get_drvdata(parent);
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	unsigned int blink_set;
	int n = 0;
	int i;

	if (sscanf(buf, "%1d", &blink_set)) {
		dev_err(dev, "can not write led_blink\n");
		return count;
	}

	if (!blink_set) {
		max77828_rgb->delay_on_times_ms = LED_OFF;
		max77828_rgb->delay_off_times_ms = LED_OFF;
	}

	for (i = 0; i < 4; i++) {
		if (dev == max77828_rgb_num->led[i].dev)
			n = i;
	}

	max77828_rgb_blink(max77828_rgb_num->led[n].dev->parent,
		max77828_rgb->delay_on_times_ms,
		max77828_rgb->delay_off_times_ms);

	return count;
}

/* permission for sysfs node */
static DEVICE_ATTR(delay_on, 0640, led_delay_on_show, led_delay_on_store);
static DEVICE_ATTR(delay_off, 0640, led_delay_off_show, led_delay_off_store);
static DEVICE_ATTR(blink, 0640, NULL, led_blink_store);

#ifdef SEC_LED_SPECIFIC
/* below nodes is SAMSUNG specific nodes */
static DEVICE_ATTR(led_r, 0660, NULL, store_led_r);
static DEVICE_ATTR(led_g, 0660, NULL, store_led_g);
static DEVICE_ATTR(led_b, 0660, NULL, store_led_b);
/* led_pattern node permission is 222 */
/* To access sysfs node from other groups */
static DEVICE_ATTR(led_pattern, 0660, NULL, store_max77828_rgb_pattern);
static DEVICE_ATTR(led_blink, 0660, NULL,  store_max77828_rgb_blink);
static DEVICE_ATTR(led_brightness, 0660, NULL, store_max77828_rgb_brightness);
static DEVICE_ATTR(led_lowpower, 0660, NULL,  store_max77828_rgb_lowpower);
#endif

static struct attribute *led_class_attrs[] = {
	&dev_attr_delay_on.attr,
	&dev_attr_delay_off.attr,
	&dev_attr_blink.attr,
	NULL,
};

static struct attribute_group common_led_attr_group = {
	.attrs = led_class_attrs,
};

#ifdef SEC_LED_SPECIFIC
static struct attribute *sec_led_attributes[] = {
	&dev_attr_led_r.attr,
	&dev_attr_led_g.attr,
	&dev_attr_led_b.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_brightness.attr,
	&dev_attr_led_lowpower.attr,
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};
#endif
static int max77828_rgb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct max77828_rgb_platform_data *pdata;
	struct max77828_rgb *max77828_rgb;
	struct max77828_dev *max77828_dev = dev_get_drvdata(dev->parent);
	char temp_name[4][40] = {{0,},}, name[40] = {0,}, *p;
	int i, ret;

#ifdef CONFIG_OF
	pdata = max77828_rgb_parse_dt(dev);
	if (unlikely(IS_ERR(pdata)))
		return PTR_ERR(pdata);
#else
	pdata = dev_get_platdata(dev);
#endif
	pr_info("leds-max77828-rgb: %s\n", __func__);

	max77828_rgb = devm_kzalloc(dev, sizeof(struct max77828_rgb), GFP_KERNEL);
	if (unlikely(!max77828_rgb))
		return -ENOMEM;

	for (i = 0; i < 4; i++) {
		ret = snprintf(name, 30, "%s", pdata->name[i])+1;
		if (1 > ret)
			goto alloc_err_flash;

		p = devm_kzalloc(dev, ret, GFP_KERNEL);
		if (unlikely(!p))
			goto alloc_err_flash;

		strcpy(p, name);
		strcpy(temp_name[i], name);
		max77828_rgb->led[i].name = p;
		max77828_rgb->led[i].brightness_set = max77828_rgb_set;
		max77828_rgb->led[i].brightness_get = max77828_rgb_get;
		max77828_rgb->led[i].max_brightness = LED_MAX_CURRENT;

		ret = led_classdev_register(dev, &max77828_rgb->led[i]);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "unable to register RGB : %d\n", ret);
			goto alloc_err_flash_plus;
		}
		ret = sysfs_create_group(&max77828_rgb->led[i].dev->kobj,
						&common_led_attr_group);
		if (ret < 0) {
			dev_err(dev, "can not register sysfs attribute\n");
			goto register_err_flash;
		}
	}

#ifdef SEC_LED_SPECIFIC
	led_dev = device_create(sec_class, NULL, 0, max77828_rgb, "led");
	if (IS_ERR(led_dev)) {
		dev_err(dev, "Failed to create device for samsung specific led\n");
		goto alloc_err_flash;
	}

	ret = sysfs_create_group(&led_dev->kobj, &sec_led_attr_group);
	if (ret < 0) {
		dev_err(dev, "Failed to create sysfs group for samsung specific led\n");
		goto alloc_err_flash;
	}
#endif

	platform_set_drvdata(pdev, max77828_rgb);

	max77828_rgb->i2c = max77828_dev->led;

	return 0;

register_err_flash:
	led_classdev_unregister(&max77828_rgb->led[i]);
alloc_err_flash_plus:
	devm_kfree(dev, temp_name[i]);
alloc_err_flash:
	while (i--) {
		led_classdev_unregister(&max77828_rgb->led[i]);
		devm_kfree(dev, temp_name[i]);
	}
	devm_kfree(dev, max77828_rgb);
	return -ENOMEM;
}

static int max77828_rgb_remove(struct platform_device *pdev)
{
	struct max77828_rgb *max77828_rgb = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < 4; i++)
		led_classdev_unregister(&max77828_rgb->led[i]);

	return 0;
}

void max77828_rgb_shutdown(struct device *dev)
{
	struct max77828_rgb *max77828_rgb = dev_get_drvdata(dev);
	int i;

	if (!max77828_rgb->i2c)
		return;

	max77828_rgb_reset(dev);

#ifdef SEC_LED_SPECIFIC
	sysfs_remove_group(&led_dev->kobj, &sec_led_attr_group);
#endif
	for (i = 0; i < 4; i++){
		sysfs_remove_group(&max77828_rgb->led[i].dev->kobj,
						&common_led_attr_group);
		led_classdev_unregister(&max77828_rgb->led[i]);
	}
	devm_kfree(dev, max77828_rgb);
}
static struct platform_driver max77828_fled_driver = {
	.driver		= {
		.name	= "leds-max77828-rgb",
		.owner	= THIS_MODULE,
		.shutdown = max77828_rgb_shutdown,
	},
	.probe		= max77828_rgb_probe,
	.remove		= max77828_rgb_remove,
};

static int __init max77828_rgb_init(void)
{
	pr_info("leds-max77828-rgb: %s\n", __func__);
	return platform_driver_register(&max77828_fled_driver);
}
module_init(max77828_rgb_init);

static void __exit max77828_rgb_exit(void)
{
	platform_driver_unregister(&max77828_fled_driver);
}
module_exit(max77828_rgb_exit);

MODULE_ALIAS("platform:max77828-rgb");
MODULE_AUTHOR("Jeongwoong Lee<jell.lee@samsung.com>");
MODULE_DESCRIPTION("MAX77828 RGB driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
