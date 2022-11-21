/*
 * leds_ktd2026.c - driver for KTD2026 led driver chip
 *
 * Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * Contact: Hyoyoung Kim <hyway.kim@samsung.com>
 *
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
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/leds-ktd2026.h>
#ifdef SEC_LED_SPECIFIC
#include <linux/sec_sysfs.h>
#endif
#include <linux/of_gpio.h>

#ifdef CONFIG_LEDS_KTD2026_PANEL
static int panel_id;

static int get_panel_id(char *str)
{
	get_option(&str, &panel_id);
	panel_id = (panel_id & 0x00ff00) >> 8;
	printk(KERN_DEBUG "led : %s 0x%x\n", __func__, panel_id);

	return panel_id;
}
__setup("lcdtype=", get_panel_id);
#endif

static void ktd2026_set_brightness(struct led_classdev *cdev,
			enum led_brightness brightness)
{
	struct ktd2026_led *led = container_of(cdev, struct ktd2026_led, cdev);

	led->brightness = (u8)brightness;
	schedule_work(&led->brightness_work);
}

static int ktd2026_leds_on(struct ktd2026_drvdata *ddata,
	enum ktd2026_led_enum led, enum ktd2026_led_mode mode, u8 bright)
{
	u8 addr = 0;
	int ret = 0;
#ifdef CONFIG_SEC_FACTORY
	if (!ddata->enable)
		return -1;
#endif

	addr = KTD2026_REG_LED1 + (led >> 1);
	ddata->shadow_reg[addr] = bright;
	ret = i2c_smbus_write_byte_data(ddata->client, addr, ddata->shadow_reg[addr]);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}

	addr = KTD2026_REG_LED_EN;
	if (mode == LED_EN_OFF)
		ddata->shadow_reg[addr] &= ~(LED_EN_PWM2 << led);
	else
		ddata->shadow_reg[addr] |= mode << led;
	ret = i2c_smbus_write_byte_data(ddata->client, addr, ddata->shadow_reg[addr]);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static void ktd2026_led_brightness_work(struct work_struct *work)
{
	struct ktd2026_led *led = container_of(work,
		struct ktd2026_led, brightness_work);
	struct device *dev = led->cdev.dev->parent;
	struct ktd2026_drvdata *ddata = dev_get_drvdata(dev);

	printk(KERN_DEBUG "led : %s : %d\n", __func__, led->brightness);

	ktd2026_leds_on(ddata, led->channel, LED_EN_ON, led->brightness);
}

static int ktd2026_set_timerslot_control(struct ktd2026_drvdata *ddata,
	int timer_slot)
{
	u8 addr = KTD2026_REG_EN_RST;
	int ret = 0;

	ddata->shadow_reg[addr] &= ~(CNT_TIMER_SLOT_MASK);
	ddata->shadow_reg[addr] |= timer_slot << CNT_TIMER_SLOT_SHIFT;
	ret = i2c_smbus_write_byte_data(ddata->client, addr, ddata->shadow_reg[addr]);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

/*  Flash period = period * 0.128 + 0.256
	exception  0 = 0.128s
	please refer to data sheet for detail */
static int ktd2026_set_period(struct ktd2026_drvdata *ddata, int period)
{
	u8 addr = KTD2026_REG_FLASH_PERIOD;
	int ret = 0;

	ddata->shadow_reg[addr] = period;
	ret = i2c_smbus_write_byte_data(ddata->client, addr, ddata->shadow_reg[addr]);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

/* MAX duty = 0xFF (99.6%) , min duty = 0x0 (0%) , 0.4% scale */
static int ktd2026_set_pwm_duty(struct ktd2026_drvdata *ddata,
	enum ktd2026_pwm pwm, int duty)
{
	u8 addr = KTD2026_REG_PWM1_TIMER + pwm;
	int ret = 0;

	ddata->shadow_reg[addr] = duty;
	ret = i2c_smbus_write_byte_data(ddata->client, addr, ddata->shadow_reg[addr]);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

/* Rise Ramp Time = trise * 96 (ms) */
/* minimum rise ramp time = 1.5ms when traise is set to 0 */
/* Tscale */
/* 0 = 1x      1 = 2x slower      2 = 4x slower    3 = 8x slower */
static int ktd2026_set_trise_tfall(struct ktd2026_drvdata *ddata,
	int trise, int tfall, int tscale)
{
	u8 addr = KTD2026_REG_TRISE_TFALL;
	int ret = 0;

	ddata->shadow_reg[addr] = (tfall << 4) + trise;
	ret = i2c_smbus_write_byte_data(ddata->client, addr, ddata->shadow_reg[addr]);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}

	addr = KTD2026_REG_EN_RST;
	ddata->shadow_reg[addr] &= ~(CNT_RISEFALL_TSCALE_MASK);
	ddata->shadow_reg[addr] |= tscale << CNT_RISEFALL_TSCALE_SHIFT;
	ret = i2c_smbus_write_byte_data(ddata->client, addr, ddata->shadow_reg[addr]);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int ktd2026_set_sleep(struct ktd2026_drvdata *ddata, int sleep)
{
	u8 addr = KTD2026_REG_EN_RST;
	int ret = 0;

	if (sleep)
		ddata->shadow_reg[addr] |= 0x1 << CNT_ENABLE_SHIFT;
	else
		ddata->shadow_reg[addr] &= ~(CNT_ENABLE_MASK);
	ret = i2c_smbus_write_byte_data(ddata->client, addr, ddata->shadow_reg[addr]);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static void ktd2026_sw_reset(struct ktd2026_drvdata *ddata)
{
	/*
	 *	if the send the reset complete command,
	 *	couln't recieve the acknowledge.
	 */
	u8 val = KTD2026_RESET;
	u8 addr = KTD2026_REG_EN_RST;
	struct i2c_msg msg[2] = {
		{
			.addr = ddata->client->addr,
			.flags = 0,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = ddata->client->addr,
			.flags = I2C_M_RD | I2C_M_IGNORE_NAK,
			.len = 1,
			.buf = &val,
		},
	};

	i2c_transfer(ddata->client->adapter, msg, 2);

 	printk(KERN_DEBUG "led : %s\n", __func__);
}

static int ktd2026_init_leds(struct ktd2026_drvdata *ddata)
{
	int ret = 0;

	printk(KERN_DEBUG "led : %s\n", __func__);
	/* initialize LED */
	/* turn off all leds */
	ret = ktd2026_leds_on(ddata, LED_R, LED_EN_OFF, 0);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	ret = ktd2026_leds_on(ddata, LED_G, LED_EN_OFF, 0);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	ret = ktd2026_leds_on(ddata, LED_B, LED_EN_OFF, 0);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}

	ret = ktd2026_set_timerslot_control(ddata, 0); /* Tslot1 */
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	ret = ktd2026_set_period(ddata, 0);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	ret = ktd2026_set_pwm_duty(ddata, PWM1, 0);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	ret = ktd2026_set_pwm_duty(ddata, PWM2, 0);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}
	ret = ktd2026_set_trise_tfall(ddata, 0, 0, 0);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}

	/* get into sleep mode after turing off all the leds */
	ret = ktd2026_set_sleep(ddata, 1);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}

	/* reset sleep mode, so that next i2c command
		would not make the driver IC go into sleep mode */
	ret = ktd2026_set_sleep(ddata, 0);
	if (ret < 0) {
		printk(KERN_ERR "led : %s %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

static int get_led_max_current(struct ktd2026_drvdata *ddata,
	enum ktd2026_led_enum led)
{
	return ddata->led[led >>1].cdev.max_brightness;
}

#ifdef SEC_LED_SPECIFIC
static void ktd2026_start_led_pattern(struct ktd2026_drvdata *ddata,
	enum ktd2026_pattern mode)
{
	if (mode > POWERING)
		return;

	if (mode == LED_OFF) {
		ktd2026_set_sleep(ddata, 1);
		gpio_set_value(ddata->pdata->svcled_en,0);
		return;
	}

	/* Set all LEDs Off */
	ktd2026_init_leds(ddata);

	switch (mode) {
	case CHARGING:
#ifdef CONFIG_USE_LIGHT_SENSOR
		ddata->led_dynamic_current = ddata->led_lowpower_mode ? LED_LOW_CURRENT :
			get_led_max_current(ddata, LED_R);
#else
		ddata->led_dynamic_current = get_led_max_current(ddata, LED_R);
#endif
		ktd2026_leds_on(ddata, LED_R, LED_EN_ON, ddata->led_dynamic_current);
		break;

	case CHARGING_ERR:
#ifdef CONFIG_USE_LIGHT_SENSOR
		ddata->led_dynamic_current = ddata->led_lowpower_mode ? LED_LOW_CURRENT :
			get_led_max_current(ddata, LED_R);
#else
		ddata->led_dynamic_current = get_led_max_current(ddata, LED_R);
#endif
		ktd2026_set_timerslot_control(ddata, 1); /* Tslot2 */
		ktd2026_set_period(ddata, 6);
		ktd2026_set_pwm_duty(ddata, PWM1, 127);
		ktd2026_set_pwm_duty(ddata, PWM2, 127);
		ktd2026_set_trise_tfall(ddata, 0, 0, 0);
		ktd2026_leds_on(ddata, LED_R, LED_EN_PWM2, ddata->led_dynamic_current);
		break;

	case MISSED_NOTI:
#ifdef CONFIG_USE_LIGHT_SENSOR
		ddata->led_dynamic_current = ddata->led_lowpower_mode ? LED_LOW_CURRENT :
			get_led_max_current(ddata, LED_B);
#else
		ddata->led_dynamic_current = get_led_max_current(ddata, LED_B);
#endif
		ktd2026_set_timerslot_control(ddata, 1); /* Tslot2 */
		ktd2026_set_period(ddata, 41);
		ktd2026_set_pwm_duty(ddata, PWM1, 232);
		ktd2026_set_pwm_duty(ddata, PWM2, 23);
		ktd2026_set_trise_tfall(ddata, 0, 0, 0);
		ktd2026_leds_on(ddata, LED_B, LED_EN_PWM2, ddata->led_dynamic_current);
		break;

	case LOW_BATTERY:
#ifdef CONFIG_USE_LIGHT_SENSOR
		ddata->led_dynamic_current = ddata->led_lowpower_mode ? LED_LOW_CURRENT :
			get_led_max_current(ddata, LED_R);
#else
		ddata->led_dynamic_current = get_led_max_current(ddata, LED_R);
#endif
		ktd2026_set_timerslot_control(ddata, 1); /* Tslot2 */
		ktd2026_set_period(ddata, 41);
		ktd2026_set_pwm_duty(ddata, PWM1, 232);
		ktd2026_set_pwm_duty(ddata, PWM2, 23);
		ktd2026_set_trise_tfall(ddata, 0, 0, 0);
		ktd2026_leds_on(ddata, LED_R, LED_EN_PWM2, ddata->led_dynamic_current);
		break;

	case FULLY_CHARGED:
#ifdef CONFIG_USE_LIGHT_SENSOR
		ddata->led_dynamic_current = ddata->led_lowpower_mode ? LED_LOW_CURRENT :
			get_led_max_current(ddata, LED_G);
#else
		ddata->led_dynamic_current = get_led_max_current(ddata, LED_G);
#endif
		ktd2026_leds_on(ddata, LED_G, LED_EN_ON, ddata->led_dynamic_current);
		break;

	case POWERING:
		ktd2026_set_timerslot_control(ddata, 0); /* Tslot1 */
		ktd2026_set_period(ddata, 14);
		ktd2026_set_pwm_duty(ddata, PWM1, 128);
		ktd2026_set_trise_tfall(ddata, 7, 7, 0);
#ifdef CONFIG_USE_LIGHT_SENSOR
		ddata->led_dynamic_current = ddata->led_lowpower_mode ? LED_LOW_CURRENT :
			get_led_max_current(ddata, LED_B);
#else
		ddata->led_dynamic_current = get_led_max_current(ddata, LED_B);
#endif
		ktd2026_leds_on(ddata, LED_B, LED_EN_ON, ddata->led_dynamic_current/2);
#ifdef CONFIG_USE_LIGHT_SENSOR
		ddata->led_dynamic_current = ddata->led_lowpower_mode ? LED_LOW_CURRENT :
			get_led_max_current(ddata, LED_G);
#else
		ddata->led_dynamic_current = get_led_max_current(ddata, LED_G);
#endif
		ktd2026_leds_on(ddata, LED_G, LED_EN_PWM1, ddata->led_dynamic_current/3);
		break;

	default:
		break;
	}
}

static void ktd2026_set_led_blink(struct ktd2026_drvdata *ddata,
	enum ktd2026_led_enum led, unsigned int delay_on_time,
	unsigned int delay_off_time, u8 brightness)
{
	int on_time, off_time;

	if (brightness == LED_OFF) {
		ktd2026_leds_on(ddata, led, LED_EN_OFF, brightness);
		return;
	}

	if (brightness > get_led_max_current(ddata, led))
		brightness = get_led_max_current(ddata, led);

	if (delay_off_time == LED_OFF) {
		ktd2026_leds_on(ddata, led, LED_EN_ON, brightness);
		return;
	}

	if (100 < (delay_on_time/delay_off_time)) {
		ktd2026_leds_on(ddata, led, LED_EN_ON, brightness);
		return;
	}

	on_time = (delay_on_time + KTD2026_TIME_UNIT - 1) / KTD2026_TIME_UNIT;
	off_time = (delay_off_time + KTD2026_TIME_UNIT - 1) / KTD2026_TIME_UNIT;
	ktd2026_set_timerslot_control(ddata, 0); /* Tslot1 */
	ktd2026_set_period(ddata, (on_time+off_time) * 4 + 2);
	ktd2026_set_pwm_duty(ddata, PWM1, on_time*255 / (on_time + off_time));
	ktd2026_set_trise_tfall(ddata, 0, 0, 0);
	ktd2026_leds_on(ddata, led, LED_EN_PWM1, brightness);
	printk(KERN_DEBUG "led : on_time=%d, off_time=%d, period=%d, duty=%d\n" ,
		on_time, off_time, (on_time+off_time) * 4 + 2,
		on_time * 255 / (on_time + off_time));
}

#ifdef CONFIG_USE_LIGHT_SENSOR
static ssize_t store_ktd2026_led_lowpower(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct ktd2026_drvdata *ddata = dev_get_drvdata(dev);
	int retval;
	u8 led_lowpower;

	retval = kstrtou8(buf, 0, &led_lowpower);
	if (retval != 0) {
		printk(KERN_ERR "led : fail to get led_lowpower.\n");
		return count;
	}

	ddata->led_lowpower_mode = led_lowpower;

	printk(KERN_DEBUG "led : led_lowpower mode set to %i\n", led_lowpower);

	return count;
}
#endif

static ssize_t store_ktd2026_led_brightness(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct ktd2026_drvdata *ddata = dev_get_drvdata(dev);
	int retval;
	u8 brightness;

	retval = kstrtou8(buf, 0, &brightness);
	if (retval != 0) {
		printk(KERN_ERR "led : fail to get led_brightness.\n");
		return count;
	}

	if (brightness > LED_MAX_CURRENT)
		brightness = LED_MAX_CURRENT;

	ddata->led_dynamic_current = brightness;

	printk(KERN_DEBUG "led : led brightness set to %i\n", brightness);

	return count;
}

static ssize_t store_ktd2026_led_br_lev(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	unsigned long brightness_lev;

	retval = kstrtoul(buf, 16, &brightness_lev);
	if (retval != 0) {
		printk(KERN_ERR "led : fail to get led_br_lev.\n");
		return count;
	}

	printk(KERN_DEBUG "led : %s\n", __func__);

	return count;
}

static ssize_t store_ktd2026_led_pattern(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct ktd2026_drvdata *ddata = dev_get_drvdata(dev);
	int retval;
	unsigned int mode = 0;

	gpio_set_value(ddata->pdata->svcled_en,1);
	udelay(900);

	retval = sscanf(buf, "%d", &mode);

	if (retval == 0) {
		printk(KERN_ERR "led : fail to get led_pattern mode.\n");
		return count;
	}

	ktd2026_start_led_pattern(ddata, mode);
#ifdef CONFIG_USE_LIGHT_SENSOR
	printk(KERN_DEBUG "led : led pattern : %d is activated(Type:%d)\n",
		mode, ddata->led_lowpower_mode);
#else
	printk(KERN_DEBUG "led : led pattern : %d is activated\n", mode);
#endif
	return count;
}

/* ex) echo 0x201000 2000 500 > led_blink */
/* brightness r=20 g=10 b=00, ontime=2000ms, offtime=500ms */
/* minimum timeunit of 500ms */
static ssize_t store_ktd2026_led_blink(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct ktd2026_drvdata *ddata = dev_get_drvdata(dev);
	int retval;
	u8 led_r_brightness = 0;
	u8 led_g_brightness = 0;
	u8 led_b_brightness = 0;
	u32 led_brightness = 0;
	u32 delay_on_time = 0;
	u32 delay_off_time = 0;

	retval = sscanf(buf, "0x%x %d %d", &led_brightness,
		&delay_on_time, &delay_off_time);

	if (retval == 0) {
		printk(KERN_ERR "led : fail to get led_blink value.\n");
		return count;
	}

	if (led_brightness == LED_EN_OFF)
		gpio_set_value(ddata->pdata->svcled_en,0);
	else {
		gpio_set_value(ddata->pdata->svcled_en,1);
	        udelay(900);

		/*Reset ktd2026*/
		ktd2026_init_leds(ddata);

		/*Set LED blink mode*/
		led_r_brightness = ((u32)led_brightness & LED_R_MASK) >> LED_R_SHIFT;
		led_g_brightness = ((u32)led_brightness & LED_G_MASK) >> LED_G_SHIFT;
		led_b_brightness = ((u32)led_brightness & LED_B_MASK);

		ktd2026_set_led_blink(ddata, LED_R, delay_on_time,
				delay_off_time, led_r_brightness);
		ktd2026_set_led_blink(ddata, LED_G, delay_on_time,
				delay_off_time, led_g_brightness);
		ktd2026_set_led_blink(ddata, LED_B, delay_on_time,
				delay_off_time, led_b_brightness);

		printk(KERN_DEBUG "led : led_blink is called, Color:0x%X Brightness:%i\n",
				led_brightness, ddata->led_dynamic_current);
	}
	return count;
}

static ssize_t show_ktd2026_led_svcled_en(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ktd2026_drvdata *ddata = dev_get_drvdata(dev);

	unsigned long svcled_en_status;

	svcled_en_status = gpio_get_value(ddata->pdata->svcled_en);
	printk(KERN_DEBUG "led : %s\n", __func__);

	return sprintf(buf, "%lu\n", svcled_en_status);
}

#define LED_ATTR_STORE_FN(name, led)						\
static ssize_t store_##name(struct device *dev,					\
	struct device_attribute *devattr, const char *buf, size_t count)	\
{										\
	struct ktd2026_drvdata *ddata = dev_get_drvdata(dev);			\
	int brightness = 0, max_current = get_led_max_current(ddata, led);	\
	gpio_set_value(ddata->pdata->svcled_en,1);				\
	udelay(900);								\
	ktd2026_init_leds(ddata);						\
										\
	if (sscanf(buf, "%d", &brightness) != 1) {				\
		printk(KERN_ERR "led : fail to get brightness.\n");		\
		return -EINVAL;							\
	}									\
	printk(KERN_DEBUG "led : %d : %d\n", led, brightness);			\
	if (max_current < brightness)						\
		brightness = max_current;					\
	ktd2026_leds_on(ddata, led,						\
		brightness ? LED_EN_ON : LED_EN_OFF, brightness);		\
	return count;								\
}										\
static ssize_t show_##name(struct device *dev,					\
	struct device_attribute *devattr, char *buf)			\
{									\
	struct ktd2026_drvdata *ddata = dev_get_drvdata(dev);		\
	u8 addr = KTD2026_REG_LED1 + (led >> 1);			\
	return sprintf(buf, "%d\n", ddata->shadow_reg[addr]);		\
}									\
static DEVICE_ATTR(name, 0664, show_##name, store_##name)
#endif

/* Added for led common class */
static ssize_t led_delay_on_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct ktd2026_led *led = container_of(cdev, struct ktd2026_led, cdev);

	printk(KERN_DEBUG "led : %s\n", __func__);

	return sprintf(buf, "%lu\n", led->delay_on_time_ms);
}

static ssize_t led_delay_on_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct ktd2026_led *led = container_of(cdev, struct ktd2026_led, cdev);
	unsigned long time;

	if (kstrtoul(buf, 0, &time))
		return -EINVAL;

	printk(KERN_DEBUG "led : %s\n", __func__);

	led->delay_on_time_ms = (int)time;
	return len;
}

static ssize_t led_delay_off_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct ktd2026_led *led = container_of(cdev, struct ktd2026_led, cdev);

	printk(KERN_DEBUG "led : %s\n", __func__);

	return sprintf(buf, "%lu\n", led->delay_off_time_ms);
}

static ssize_t led_delay_off_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct ktd2026_led *led = container_of(cdev, struct ktd2026_led, cdev);
	unsigned long time;

	if (kstrtoul(buf, 0, &time))
		return -EINVAL;

	printk(KERN_DEBUG "led : %s\n", __func__);

	led->delay_off_time_ms = (int)time;

	return len;
}

static ssize_t led_blink_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t len)
{
	struct led_classdev *cdev = dev_get_drvdata(dev);
	struct ktd2026_led *led = container_of(cdev, struct ktd2026_led, cdev);
	unsigned long blink_set;

	if (kstrtoul(buf, 0, &blink_set))
		return -EINVAL;

	printk(KERN_DEBUG "led : %s\n", __func__);

	if (!blink_set) {
		led->delay_on_time_ms = LED_OFF;
		ktd2026_set_brightness(cdev, LED_OFF);
	}

	led_blink_set(cdev,
		&led->delay_on_time_ms, &led->delay_off_time_ms);

	return len;
}

/* permission for sysfs node */
static DEVICE_ATTR(delay_on, 0644, led_delay_on_show, led_delay_on_store);
static DEVICE_ATTR(delay_off, 0644, led_delay_off_show, led_delay_off_store);
static DEVICE_ATTR(blink, 0644, NULL, led_blink_store);

#ifdef SEC_LED_SPECIFIC
/* below nodes is SAMSUNG specific nodes */
LED_ATTR_STORE_FN(led_r, LED_R);
LED_ATTR_STORE_FN(led_g, LED_G);
LED_ATTR_STORE_FN(led_b, LED_B);
/* led_pattern node permission is 664 */
/* To access sysfs node from other groups */
static DEVICE_ATTR(led_pattern, 0664, NULL, \
					store_ktd2026_led_pattern);
static DEVICE_ATTR(led_blink, 0664, NULL, \
					store_ktd2026_led_blink);
static DEVICE_ATTR(led_br_lev, 0664, NULL, \
					store_ktd2026_led_br_lev);
static DEVICE_ATTR(led_brightness, 0664, NULL, \
					store_ktd2026_led_brightness);
#ifdef CONFIG_USE_LIGHT_SENSOR
static DEVICE_ATTR(led_lowpower, 0664, NULL, \
					store_ktd2026_led_lowpower);
#endif
static DEVICE_ATTR(led_svcled_en, 0644, show_ktd2026_led_svcled_en, NULL);
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
	&dev_attr_led_br_lev.attr,
	&dev_attr_led_brightness.attr,
#ifdef CONFIG_USE_LIGHT_SENSOR
	&dev_attr_led_lowpower.attr,
#endif
	&dev_attr_led_svcled_en.attr,
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};
#endif

static int __devinit initialize_channel(struct i2c_client *client,
	struct ktd2026_led *led, int channel)
{
	struct ktd2026_drvdata *ddata = i2c_get_clientdata(client);
	struct ktd2026_led_pdata *pdata = ddata->pdata;
	int ret;

	led->channel = channel * 2;
	led->cdev.brightness_set = ktd2026_set_brightness;
	led->cdev.name = pdata->led_conf[channel].name;
	led->cdev.brightness = pdata->led_conf[channel].brightness;
	led->cdev.max_brightness = pdata->led_conf[channel].max_brightness;
	led->cdev.flags = pdata->led_conf[channel].flags;

	ret = led_classdev_register(&client->dev, &led->cdev);
	if (ret < 0) {
		printk(KERN_ERR "led : can not register led channel : %d\n", channel);
		return ret;
	}

	ret = sysfs_create_group(&led->cdev.dev->kobj,
			&common_led_attr_group);
	if (ret < 0) {
		printk(KERN_ERR "led : can not register sysfs attribute\n");
		return ret;
	}

	return 0;
}

static struct ktd2026_led_pdata *
	ktd2026_get_devtree_pdata(struct i2c_client *client)
{
	struct ktd2026_led_pdata *pdata;
	struct device_node *node = client->dev.of_node;
	struct device_node *child_node;
	struct ktd2026_led_conf *led_conf;
	int leds = 0, i = 0, error = 0;

	if (!node) {
		printk(KERN_ERR "led : %s no of_node\n", __func__);
		error = -ENOMEM;
		goto err_out;
	}

	leds = of_get_child_count(node);
	if (leds == 0) {
		printk(KERN_ERR "led : %s no child node\n", __func__);
		error = -ENOMEM;
		goto err_out;
	}

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "led : %s failed to alloc\n", __func__);
		error = -ENOMEM;
		goto err_out;

	}

	led_conf = kzalloc(leds * (sizeof *led_conf), GFP_KERNEL);
	if (!led_conf) {
		printk(KERN_ERR "led : %s failed to alloc\n", __func__);
		error = -ENOMEM;
		goto error_pdata_free;

	}

	pdata->led_conf = led_conf;
	pdata->leds = leds;	
	pdata->svcled_en = of_get_named_gpio(node, "svcled_en", 0);

	for_each_child_of_node(node, child_node) {
		of_property_read_string(child_node, "led_conf-name", &led_conf[i].name);
		printk(KERN_DEBUG "led : %s", led_conf[i].name);
		if (of_property_read_u8(child_node, "brightness", &led_conf[i].brightness))
			led_conf[i].brightness = LED_OFF;
		printk(KERN_DEBUG " %d", led_conf[i].brightness);
#ifdef CONFIG_LEDS_KTD2026_PANEL
		if (panel_id == 0x60) {
			if (of_property_read_u8(child_node, "max_brightness",
						&led_conf[i].max_brightness))
				led_conf[i].max_brightness = LED_MAX_CURRENT;
		}
		else if (panel_id == 0x61) {
			if (of_property_read_u8(child_node, "max_brightness2",
						&led_conf[i].max_brightness))
				led_conf[i].max_brightness = LED_MAX_CURRENT;
		}
		else {
			if (of_property_read_u8(child_node, "max_brightness3",
						&led_conf[i].max_brightness))
				led_conf[i].max_brightness = LED_MAX_CURRENT;
		}
#else
		if (of_property_read_u8(child_node, "max_brightness", &led_conf[i].max_brightness))
			led_conf[i].max_brightness = LED_MAX_CURRENT;
#endif
		printk(KERN_DEBUG " %d", led_conf[i].max_brightness);
		if (of_property_read_u8(child_node, "flags", &led_conf[i].flags))
			led_conf[i].flags = 0;
		printk(KERN_DEBUG " %d\n", led_conf[i].flags);
		i++;
	}

	return pdata;
error_pdata_free:
	kfree(pdata);
err_out:
	return ERR_PTR(error);
}

static int ktd2026_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct ktd2026_led_pdata *pdata = client->dev.platform_data;
	struct ktd2026_drvdata *ddata;
	int ret = 0, i = 0;

	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = ktd2026_get_devtree_pdata(client);
		if (IS_ERR(pdata)) {
			printk(KERN_ERR "led : %s not found dt! ret[%d]\n",
					 __func__, ret);
			ret = -ENODEV;
			goto err_pdata;
		}
#else	 /* CONFIG_OF */
		printk(KERN_ERR "led : no platform data!\n");
		ret = -ENODEV;
		goto err_pdata;
#endif
	}

	gpio_direction_output(pdata->svcled_en, 1);
	udelay(900);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "led : need I2C_FUNC_I2C.\n");
		ret = -ENODEV;
		goto err_i2c;
	}

	ddata = kzalloc(sizeof(*ddata), GFP_KERNEL);
	if (!ddata) {
		printk(KERN_ERR "led : failed to allocate driver data.\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	i2c_set_clientdata(client, ddata);
	mutex_init(&ddata->mutex);
	ddata->pdata = pdata;
	ddata->client = client;
	ddata->led_dynamic_current = LED_DEFAULT_CURRENT;
#ifdef CONFIG_SEC_FACTORY
	ddata->enable = true;
#endif

	ret = ktd2026_init_leds(ddata);
	if (ret < 0) {
		printk(KERN_ERR "led : failure on initialization %d\n", ret);
#ifdef CONFIG_SEC_FACTORY
		ddata->enable = false;
#endif
	}

	for (i = 0; i < pdata->leds; i++) {
		struct ktd2026_led *led = &ddata->led[i];
		ret = initialize_channel(client, led, i);
		if (ret < 0) {
			printk(KERN_ERR "led : failure on initialization\n");
			goto err_init_channel;
		}
		INIT_WORK(&(led->brightness_work),
				 ktd2026_led_brightness_work);
	}

#ifdef SEC_LED_SPECIFIC
	ddata->dev = sec_device_create(ddata, "led");
	if (IS_ERR(ddata->dev)) {
		printk(KERN_ERR
			"led : Failed to create device for samsung specific led\n");
		ret = -ENODEV;
		goto err_device_create;
	}
	ret = sysfs_create_group(&ddata->dev->kobj, &sec_led_attr_group);
	if (ret) {
		printk(KERN_ERR
			"led : Failed to create sysfs group for samsung specific led\n");
		goto err_sysfs;
	}
#endif
	gpio_set_value(pdata->svcled_en,0);
	return ret;
err_sysfs:
err_device_create:
err_init_channel:
	for (i = 0; i < pdata->leds; i++) {
		struct ktd2026_led *led = &ddata->led[i];
		sysfs_remove_group(&led->cdev.dev->kobj,
						&common_led_attr_group);
		led_classdev_unregister(&led->cdev);
		cancel_work_sync(&led->brightness_work);
	}
	mutex_destroy(&ddata->mutex);
	kfree(ddata);
err_alloc:
err_i2c:
err_pdata:
	gpio_set_value(pdata->svcled_en,0);
	return ret;
}

static int __devexit ktd2026_remove(struct i2c_client *client)
{
	struct ktd2026_drvdata *ddata = i2c_get_clientdata(client);
	int i;

	gpio_set_value(ddata->pdata->svcled_en,1);
	udelay(900);

	/* WA for the abnormal state */
	ktd2026_sw_reset(ddata);

#ifdef SEC_LED_SPECIFIC
	sysfs_remove_group(&ddata->dev->kobj, &sec_led_attr_group);
#endif
	for (i = 0; i < MAX_NUM_LEDS; i++) {
		struct ktd2026_led *led = &ddata->led[i];
		sysfs_remove_group(&led->cdev.dev->kobj,
						&common_led_attr_group);
		led_classdev_unregister(&led->cdev);
		cancel_work_sync(&led->brightness_work);
	}
	mutex_destroy(&ddata->mutex);
	kfree(ddata);
	return 0;
}

static void ktd2026_shutdown(struct i2c_client *client)
{
 	struct ktd2026_drvdata *ddata = i2c_get_clientdata(client);
	gpio_set_value(ddata->pdata->svcled_en,0);
	printk(KERN_ERR "led : %s\n", __func__);
	return;
}

static struct i2c_device_id ktd2026_id[] = {
	{"ktd2026", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ktd2026_id);

#if defined(CONFIG_OF)
static struct of_device_id ktd2026_i2c_dt_ids[] = {
	{ .compatible = "ktd2026" },
	{ }
};
MODULE_DEVICE_TABLE(of, ktd2026_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver ktd2026_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ktd2026",
		.of_match_table	= of_match_ptr(ktd2026_i2c_dt_ids),
	},
	.id_table = ktd2026_id,
	.probe = ktd2026_probe,
	.remove = __devexit_p(ktd2026_remove),
	.shutdown = ktd2026_shutdown,
};

static int __init ktd2026_init(void)
{
	return i2c_add_driver(&ktd2026_i2c_driver);
}

static void __exit ktd2026_exit(void)
{
	i2c_del_driver(&ktd2026_i2c_driver);
}

module_init(ktd2026_init);
module_exit(ktd2026_exit);

MODULE_DESCRIPTION("KTD2026 LED driver");
MODULE_AUTHOR("Hyoyoung Kim <hyway.kim@samsung.com");
MODULE_LICENSE("GPL");
