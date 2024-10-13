/*
 * leds_an30259a.c - driver for Panasonic AN30259A led control chip
 *
 * Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/leds.h>
#include <linux/leds-an30259a.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>

/* AN30259A register map */
#include "leds-an30259a_reg.h"


enum an30259a_led_enum {
	LED_R,
	LED_G,
	LED_B,
};

enum an30259a_pattern {
	PATTERN_OFF,
	CHARGING,
	CHARGING_ERR,
	MISSED_NOTI,
	LOW_BATTERY,
	FULLY_CHARGED,
	POWERING,
};


struct i2c_client *b_client;

#define SEC_LED_SPECIFIC
#define LED_DEEP_DEBUG

#ifdef SEC_LED_SPECIFIC
struct device *led_dev;
/*path : /sys/class/sec/led/led_pattern*/
/*path : /sys/class/sec/led/led_blink*/
/*path : /sys/class/sec/led/led_brightness*/
/*path : /sys/class/leds/led_r/brightness*/
/*path : /sys/class/leds/led_g/brightness*/
/*path : /sys/class/leds/led_b/brightness*/
#endif

#if defined (CONFIG_SEC_FACTORY)
#if defined (CONFIG_SEC_S_PROJECT)
static int f_jig_cable;
extern int get_lcd_attached(void);

static int __init get_jig_cable_cmdline(char *mode)
{
	f_jig_cable = mode[0]-48;
	return 0;
}

__setup( "uart_dbg=", get_jig_cable_cmdline);
#endif
#endif

static void leds_on(enum an30259a_led_enum led, bool on, bool slopemode,
					u8 ledcc);

static inline struct an30259a_led *cdev_to_led(struct led_classdev *cdev)
{
	return container_of(cdev, struct an30259a_led, cdev);
}




#ifdef SEC_LED_SPECIFIC
static void an30259a_start_led_pattern(int mode)
{
	int retval;
	struct i2c_client *client;
	struct work_struct *reset = 0;
	client = b_client;

	if (mode > POWERING)
		return;
	/* Set all LEDs Off */
	an30259a_reset_register_work(reset);
	if (mode == LED_OFF)
		return;

	/* Set to low power consumption mode */
	if (LED_LOWPOWER_MODE == 1)
		LED_DYNAMIC_CURRENT = (u8)led_lowpower_cur;
	else
		LED_DYNAMIC_CURRENT = (u8)led_default_cur;

	switch (mode) {
	/* leds_set_slope_mode(client, LED_SEL, DELAY,  MAX, MID, MIN,
		SLPTT1, SLPTT2, DT1, DT2, DT3, DT4) */
	case CHARGING:
		pr_info("LED Battery Charging Pattern on\n");
		leds_on(LED_R, true, false,
					LED_DYNAMIC_CURRENT);
		break;

	case CHARGING_ERR:
		pr_info("LED Battery Charging error Pattern on\n");
		leds_on(LED_R, true, true,
					LED_DYNAMIC_CURRENT);
		leds_set_slope_mode(client, LED_R,
				1, 15, 15, 0, 1, 1, 0, 0, 0, 0);
		break;

	case MISSED_NOTI:
		pr_info("LED Missed Notifications Pattern on\n");
		leds_on(LED_B, true, true,
					LED_DYNAMIC_CURRENT);
		leds_set_slope_mode(client, LED_B,
					10, 15, 15, 0, 1, 10, 0, 0, 0, 0);
		break;

	case LOW_BATTERY:
		pr_info("LED Low Battery Pattern on\n");
		leds_on(LED_R, true, true,
					LED_DYNAMIC_CURRENT);
		leds_set_slope_mode(client, LED_R,
					10, 15, 15, 0, 1, 10, 0, 0, 0, 0);
		break;

	case FULLY_CHARGED:
		pr_info("LED full Charged battery Pattern on\n");
		leds_on(LED_G, true, false,
					LED_DYNAMIC_CURRENT);
		break;

	case POWERING:
		pr_info("LED Powering Pattern on\n");
		leds_on(LED_B, true, true, LED_DYNAMIC_CURRENT);
		leds_set_slope_mode(client, LED_B,
				0, 15, 12, 8, 2, 2, 3, 3, 3, 3);
		break;

	default:
		return;
		break;
	}
	retval = leds_i2c_write_all(client);
	if (retval)
		printk(KERN_WARNING "leds_i2c_write_all failed\n");
}

static void an30259a_set_led_blink(enum an30259a_led_enum led,
					unsigned int delay_on_time,
					unsigned int delay_off_time,
					u8 brightness)
{
	struct i2c_client *client;
	client = b_client;

	if (brightness == LED_OFF) {
		leds_on(led, false, false, brightness);
		return;
	}

	if (brightness > LED_MAX_CURRENT)
		brightness = LED_MAX_CURRENT;

	if (led == LED_R)
		LED_DYNAMIC_CURRENT = LED_R_CURRENT;
	else if (led == LED_G)
		LED_DYNAMIC_CURRENT = LED_G_CURRENT;
	else if (led == LED_B)
		LED_DYNAMIC_CURRENT = LED_B_CURRENT;

	/* In user case, LED current is restricted */
	brightness = (brightness * LED_DYNAMIC_CURRENT) / LED_MAX_CURRENT;

	if (delay_on_time > SLPTT_MAX_VALUE)
		delay_on_time = SLPTT_MAX_VALUE;

	if (delay_off_time > SLPTT_MAX_VALUE)
		delay_off_time = SLPTT_MAX_VALUE;

	if (delay_off_time == LED_OFF) {
		leds_on(led, true, false, brightness);
		if (brightness == LED_OFF)
			leds_on(led, false, false, brightness);
		return;
	} else
		leds_on(led, true, true, brightness);

	/* leds_set_slope_mode(client, LED_SEL, DELAY,  MAX, MID, MIN,
		SLPTT1, SLPTT2, DT1, DT2, DT3, DT4) */
	leds_set_slope_mode(client, led, 0, 15, 15, 0,
				(delay_on_time + AN30259A_TIME_UNIT - 1) /
				AN30259A_TIME_UNIT,
				(delay_off_time + AN30259A_TIME_UNIT - 1) /
				AN30259A_TIME_UNIT,
				0, 0, 0, 0);
}

static ssize_t store_an30259a_led_lowpower(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	u8 led_lowpower;
	struct an30259a_data *data = dev_get_drvdata(dev);

	retval = kstrtou8(buf, 0, &led_lowpower);
	if (retval != 0) {
		dev_err(&data->client->dev, "fail to get led_lowpower.\n");
		return count;
	}

	LED_LOWPOWER_MODE = led_lowpower;

	printk(KERN_DEBUG "led_lowpower mode set to %i\n", led_lowpower);

	return count;
}

static ssize_t store_an30259a_led_br_lev(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	unsigned long brightness_lev;
	struct i2c_client *client;
	struct an30259a_data *data = dev_get_drvdata(dev);
	client = b_client;

	retval = kstrtoul(buf, 16, &brightness_lev);
	if (retval != 0) {
		dev_err(&data->client->dev, "fail to get led_br_lev.\n");
		return count;
	}

	leds_set_imax(client, brightness_lev);

	return count;
}

static ssize_t store_an30259a_led_pattern(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	unsigned int mode = 0;
	unsigned int type = 0;
	struct an30259a_data *data = dev_get_drvdata(dev);

	retval = sscanf(buf, "%d %d", &mode, &type);

	if (retval == 0) {
		dev_err(&data->client->dev, "fail to get led_pattern mode.\n");
		return count;
	}

	an30259a_start_led_pattern(mode);
	printk(KERN_DEBUG "led pattern : %d is activated\n", mode);

	return count;
}

static ssize_t store_an30259a_led_blink(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	unsigned int led_brightness = 0;
	unsigned int delay_on_time = 0;
	unsigned int delay_off_time = 0;
	struct an30259a_data *data = dev_get_drvdata(dev);
	u8 led_r_brightness = 0;
	u8 led_g_brightness = 0;
	u8 led_b_brightness = 0;

	retval = sscanf(buf, "0x%x %d %d", &led_brightness,
				&delay_on_time, &delay_off_time);

	if (retval == 0) {
		dev_err(&data->client->dev, "fail to get led_blink value.\n");
		return count;
	}
	/*Reset an30259a*/
	an30259a_start_led_pattern(LED_OFF);

	/*Set LED blink mode*/
	led_r_brightness = ((u32)led_brightness & LED_R_MASK)
					>> LED_R_SHIFT;
	led_g_brightness = ((u32)led_brightness & LED_G_MASK)
					>> LED_G_SHIFT;
	led_b_brightness = ((u32)led_brightness & LED_B_MASK);

	an30259a_set_led_blink(LED_R, delay_on_time,
				delay_off_time, led_r_brightness);
	an30259a_set_led_blink(LED_G, delay_on_time,
				delay_off_time, led_g_brightness);
	an30259a_set_led_blink(LED_B, delay_on_time,
				delay_off_time, led_b_brightness);

	leds_i2c_write_all(data->client);

	printk(KERN_DEBUG "led_blink is called, Color:0x%X Brightness:%i\n",
			led_brightness, LED_DYNAMIC_CURRENT);

	return count;
}


static ssize_t store_led_r(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct an30259a_data *data = dev_get_drvdata(dev);
	int ret;
	u8 brightness;

	ret = kstrtou8(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(&data->client->dev, "fail to get brightness.\n");
		goto out;
	}

	if (brightness == 0)
		leds_on(LED_R, false, false, 0);
	else
		leds_on(LED_R, true, false, brightness);

	leds_i2c_write_all(data->client);
	an30259a_debug(data->client);
out:
	return count;
}

static ssize_t store_led_g(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct an30259a_data *data = dev_get_drvdata(dev);
	int ret;
	u8 brightness;

	ret = kstrtou8(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(&data->client->dev, "fail to get brightness.\n");
		goto out;
	}

	if (brightness == 0)
		leds_on(LED_G, false, false, 0);
	else
		leds_on(LED_G, true, false, brightness);

	leds_i2c_write_all(data->client);
	an30259a_debug(data->client);
out:
	return count;
}

static ssize_t store_led_b(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct an30259a_data *data = dev_get_drvdata(dev);
	int ret;
	u8 brightness;

	ret = kstrtou8(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(&data->client->dev, "fail to get brightness.\n");
		goto out;
	}

	if (brightness == 0)
		leds_on(LED_B, false, false, 0);
	else
		leds_on(LED_B, true, false, brightness);

	leds_i2c_write_all(data->client);
	an30259a_debug(data->client);
out:
	return count;

}

/* below nodes is SAMSUNG specific nodes */
static DEVICE_ATTR(led_r, 0664, NULL, store_led_r);
static DEVICE_ATTR(led_g, 0664, NULL, store_led_g);
static DEVICE_ATTR(led_b, 0664, NULL, store_led_b);
/* led_pattern node permission is 664 */
/* To access sysfs node from other groups */
static DEVICE_ATTR(led_pattern, 0664, NULL, \
					store_an30259a_led_pattern);
static DEVICE_ATTR(led_blink, 0664, NULL, \
					store_an30259a_led_blink);
static DEVICE_ATTR(led_br_lev, 0664, NULL, \
					store_an30259a_led_br_lev);
static DEVICE_ATTR(led_lowpower, 0664, NULL, \
					store_an30259a_led_lowpower);

static struct attribute *sec_led_attributes[] = {
	&dev_attr_led_r.attr,
	&dev_attr_led_g.attr,
	&dev_attr_led_b.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_br_lev.attr,
	&dev_attr_led_lowpower.attr,
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};
#endif


================================
#if defined (CONFIG_SEC_FACTORY)
#if defined (CONFIG_SEC_S_PROJECT)
static int f_jig_cable;
extern int get_lcd_attached(void);

static int __init get_jig_cable_cmdline(char *mode)
{
	f_jig_cable = mode[0]-48;
	return 0;
}

__setup( "uart_dbg=", get_jig_cable_cmdline);
#endif //CONFIG_SEC_S_PROJECT

int GlowOnJigMode(....)
{
#if defined (CONFIG_SEC_S_PROJECT)
	if ( (f_jig_cable == 0) && (get_lcd_attached() == 0) ) {
		pr_info("%s:Factory MODE - No OCTA, Battery BOOTING\n", __func__);
		leds_on(LED_R, true, false, LED_R_CURRENT);
		leds_i2c_write_all(data->client);
	}
#endif //CONFIG_SEC_S_PROJECT

	return 0;
}
#endif //CONFIG_SEC_FACTORY

int sec_led_sysfs_create()
{

	led_dev = device_create(sec_class, NULL, 0, data, "led");
	if (IS_ERR(led_dev)) {
		dev_err(&client->dev,
			"Failed to create device for samsung specific led\n");
		ret = -ENODEV;
		goto exit1;
	}
	ret = sysfs_create_group(&led_dev->kobj, &sec_led_attr_group);
	if (ret) {
		dev_err(&client->dev,
			"Failed to create sysfs group for samsung specific led\n");
		goto exit1;
	}
	return 0;
}


void sec_led_sysfs_destroy(void)
{
	sysfs_remove_group(&led_dev->kobj, &sec_led_attr_group);
}

