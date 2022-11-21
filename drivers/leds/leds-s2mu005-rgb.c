/*
 * RGB-led driver for Maxim s2mu005
 *
 * Copyright (C) 2013 Maxim Integrated Product
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/sec_sysfs.h>
#include <linux/mfd/samsung/s2mu005.h>
#include <linux/mfd/samsung/s2mu005-private.h>
#include <linux/leds-s2mu005-rgb.h>

#define SEC_LED_SPECIFIC

#ifdef SEC_LED_SPECIFIC
#ifndef CONFIG_SEC_SYSFS
extern struct class *sec_class;
#endif

static struct device *led_dev;
#endif

static unsigned int window_color = 0x0;

extern int get_lcd_attached(char*);

struct s2mu005_rgb {
	struct led_classdev led[S2MU005_RGBLED_NUM];
	struct i2c_client *i2c;
	unsigned int ch[S2MU005_RGBLED_NUM];
	unsigned int delay_on_times_ms;
	unsigned int delay_off_times_ms;
};

static int s2mu005_rgb_ch_number(struct led_classdev *led_cdev,
				struct s2mu005_rgb **p)
{
	const struct device *parent = led_cdev->dev->parent;
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(parent);
	int i;

	*p = s2mu005_rgb;

	for (i = 0; i < S2MU005_RGBLED_NUM ; i++) {
		if (led_cdev == &s2mu005_rgb->led[i]) {
			pr_info("leds-s2mu005-rgb: %s, %d\n", __func__, s2mu005_rgb->ch[i]);
			if (s2mu005_rgb->ch[i] > S2MU005_RGBLED_NUM)
				return -ENODEV;
			return (s2mu005_rgb->ch[i]-1);
		}
	}

	return -ENODEV;
}

static void s2mu005_rgb_set(struct led_classdev *led_cdev,
				unsigned int brightness)
{
	const struct device *parent = led_cdev->dev->parent;
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(parent);
	struct device *dev;
	int n;
	int ret;

	ret = s2mu005_rgb_ch_number(led_cdev, &s2mu005_rgb);
	if (IS_ERR_VALUE(ret)) {
		dev_err(led_cdev->dev,
			"s2mu005_rgb_ch_number() returns %d.\n", ret);
		return;
	}

	dev = led_cdev->dev;
	n = ret;

	if (brightness == LED_OFF) {
		/* Flash OFF */
		ret = s2mu005_update_reg(s2mu005_rgb->i2c,
				S2MU005_REG_LED_EN, RGBLED_DISABLE,
				RGBLED_ENMASK << (2*n));
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write LEDEN : %d\n", ret);
			return;
		}
	} else {
		/* Set current */
		ret = s2mu005_write_reg(s2mu005_rgb->i2c,
				S2MU005_REG_LED1_CURRENT + n, brightness);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write LEDxBRT : %d\n", ret);
			return;
		}
		/* Flash ON */
		ret = s2mu005_update_reg(s2mu005_rgb->i2c,
				S2MU005_REG_LED_EN, RGBLED_ALWAYS_ON << (2*n),
				RGBLED_ENMASK << (2*n));
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can't write FLASH_EN : %d\n", ret);
			return;
		}
	}
}

static void s2mu005_rgb_set_state(struct led_classdev *led_cdev,
				unsigned int brightness, unsigned int led_state)
{
	const struct device *parent = led_cdev->dev->parent;
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(parent);
	struct device *dev;
	int n;
	int ret;

	pr_info("leds-s2mu005-rgb: %s: %u: %u\n", __func__, brightness, led_state);

	ret = s2mu005_rgb_ch_number(led_cdev, &s2mu005_rgb);

	if (IS_ERR_VALUE(ret)) {
		dev_err(led_cdev->dev,
			"s2mu005_rgb_ch_number() returns %d.\n", ret);
		return;
	}

	dev = led_cdev->dev;
	n = ret;

	if(brightness != 0) {
		/* apply brightness ratio for optimize each led brightness*/
		switch(n) {
		case RED:
			brightness = brightness * brightness_ratio_r / 100;
			break;
		case GREEN:
			brightness = brightness * brightness_ratio_g / 100;
			break;
		case BLUE:
			brightness = brightness * brightness_ratio_b / 100;
			break;
		}

		/*
			There is possibility that low_powermode_current is 0.
			ex) low_powermode_current is 1 & brightness_ratio_r is 90
			brightness = 1 * 90 / 100 = 0.9
			brightness is inteager, so brightness is 0.
			In this case, it is need to assign 1 of value.
		*/
		if(brightness == 0)
			brightness = 1;
	}

	s2mu005_rgb_set(led_cdev, brightness);

	pr_info("leds-s2mu005-rgb: %s, ch_num = %d, brightness = %d\n", __func__, ret, brightness);

	ret = s2mu005_update_reg(s2mu005_rgb->i2c,
			S2MU005_REG_LED_EN, led_state << (2*n), RGBLED_ENMASK << 2*n);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't write FLASH_EN : %d\n", ret);
		return;
	}
}

static unsigned int s2mu005_rgb_get(struct led_classdev *led_cdev)
{
	const struct device *parent = led_cdev->dev->parent;
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(parent);
	struct device *dev;
	int n;
	int ret;
	u8 value;

	pr_info("leds-s2mu005-rgb: %s\n", __func__);

	ret = s2mu005_rgb_ch_number(led_cdev, &s2mu005_rgb);
	if (IS_ERR_VALUE(ret)) {
		dev_err(led_cdev->dev,
			"s2mu005_rgb_ch_number() returns %d.\n", ret);
		return 0;
	}
	n = ret;

	dev = led_cdev->dev;

	/* Get status */
	ret = s2mu005_read_reg(s2mu005_rgb->i2c,
				S2MU005_REG_LED_EN, &value);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't read LED_EN : %d\n", ret);
		return 0;
	}
	if (!(value & (RGBLED_ENMASK << 2*n))) 
		return LED_OFF;

	/* Get current */
	ret = s2mu005_read_reg(s2mu005_rgb->i2c,
				S2MU005_REG_LED1_CURRENT + n, &value);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't read LED0BRT : %d\n", ret);
		return 0;
	}

	return value;
}

static int s2mu005_rgb_ramp(struct led_classdev *led_cdev,
					 int ramp_up, int ramp_down)
{
	const struct device *parent = led_cdev->dev->parent;
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(parent);
	struct device *dev = led_cdev->dev;
	int ret;
	int value, n;

	pr_info("leds-s2mu005-rgb: %s\n", __func__);

	ret = s2mu005_rgb_ch_number(led_cdev, &s2mu005_rgb);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "s2mu005_rgb_ch_number() returns %d.\n", ret);
		return 0;
	}
	n = ret;

	if (ramp_up <= 800) {
		ramp_up /= 100;
	}
	else if(ramp_up < 2400) {
		ramp_up = (ramp_up - 800) / 2 + 800;
		ramp_up /= 100;
	}
	else {
		ramp_up = 15;	
	}

	if (ramp_down <= 800) {
		ramp_down /= 100;
	}
	else if(ramp_down < 2400) {
		ramp_down = (ramp_down - 800) / 2 + 800;
		ramp_down /= 100;
	}
	else {
		ramp_down = 15;	
	}

	value = (ramp_down) | (ramp_up << 4);
	ret = s2mu005_write_reg(s2mu005_rgb->i2c,
				S2MU005_REG_LED1_RAMP + 2*n, value);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't write REG_LEDRMP : %d\n", ret);
		return -ENODEV;
	}

	return 0;
}

static int s2mu005_rgb_blink(struct led_classdev *led_cdev,
				unsigned int delay_on, unsigned int delay_off)
{
	const struct device *parent = led_cdev->dev->parent;
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(parent);
	struct device *dev = led_cdev->dev;
	int n;
	int ret = 0;
	int value;

	pr_info("leds-s2mu005-rgb: %s\n", __func__);

	ret = s2mu005_rgb_ch_number(led_cdev, &s2mu005_rgb);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev,"s2mu005_rgb_ch_number() returns %d.\n", ret);
		return 0;
	}
	n = ret;

	value = (LEDBLNK_ON(delay_on) << 4) | LEDBLNK_OFF(delay_off);
	ret = s2mu005_write_reg(s2mu005_rgb->i2c,
					S2MU005_REG_LED1_DUR + 2*n, value);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "can't write REG_LEDBLNK : %d\n", ret);
		return -EINVAL;
	}

	return ret;
}

static void s2mu005_rgb_reset(struct device *dev)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	int ret;
	ret = s2mu005_update_reg(s2mu005_rgb->i2c,
			S2MU005_REG_LED_EN, RGBLED_REG_RESET_MASK,
			RGBLED_REG_RESET_MASK);
}

#ifdef CONFIG_OF
static struct s2mu005_rgb_platform_data
			*s2mu005_rgb_parse_dt(struct device *dev)
{
	struct s2mu005_rgb_platform_data *pdata;
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *np;
	int ret;
	int i;
	int temp;
	char window[4] = {0, };
	char br_ratio_r[23] = "br_ratio_r";
	char br_ratio_g[23] = "br_ratio_g";
	char br_ratio_b[23] = "br_ratio_b";
	char normal_po_cur[29] = "normal_powermode_current";
#ifdef CONFIG_USE_LIGHT_SENSOR
	char low_po_cur[26] = "low_powermode_current";
#endif

	pr_info("leds-s2mu005-rgb: %s\n", __func__);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (unlikely(pdata == NULL))
		return ERR_PTR(-ENOMEM);

	np = of_find_node_by_name(nproot, "rgb");
	if (unlikely(np == NULL)) {
		dev_err(dev, "rgb node not found\n");
		devm_kfree(dev, pdata);
		return ERR_PTR(-EINVAL);
	}

	for (i = 0; i < S2MU005_RGBLED_NUM; i++)	{
		ret = of_property_read_string_index(np, "rgb-name", i,
						(const char **)&pdata->name[i]);
		if (IS_ERR_VALUE(ret)) {
			devm_kfree(dev, pdata);
			return ERR_PTR(ret);
		}

		ret = of_property_read_u32_index(np, "rgb-channel", i,
						&pdata->ch[i]);
		if (IS_ERR_VALUE(ret)) {
			devm_kfree(dev, pdata);
			return ERR_PTR(ret);
		}

		pr_info("leds-s2mu005-rgb: %s, %s: %u\n", __func__,
						pdata->name[i], pdata->ch[i]);
	}

	switch(window_color) {
	case 0:
		strcpy(window, "_wh");
		break;
	case 1:
		strcpy(window, "_gd");
		break;
	case 2:
		strcpy(window, "_bk");
		break;
	default:
		break;
	}
	strcat(normal_po_cur, window);
#ifdef CONFIG_USE_LIGHT_SENSOR
	strcat(low_po_cur, window);
#endif
	strcat(br_ratio_r, window);
	strcat(br_ratio_g, window);
	strcat(br_ratio_b, window);

	/* get normal_powermode_current value in dt */
	ret = of_property_read_u32(np, normal_po_cur, &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-s2mu005-rgb: %s, can't parsing normal_powermode_current in dt\n", __func__);
	}
	else {
		normal_powermode_current = (u8)temp;
	}
	pr_info("leds-s2mu005-rgb: %s, normal_powermode_current = %x\n", __func__, normal_powermode_current);

#ifdef CONFIG_USE_LIGHT_SENSOR
	/* get low_powermode_current value in dt */
	ret = of_property_read_u32(np, low_po_cur, &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-s2mu005-rgb: %s, can't parsing low_powermode_current in dt\n", __func__);
	}
	else
		low_powermode_current = (u8)temp;
	pr_info("leds-s2mu005-rgb: %s, low_powermode_current = %x\n", __func__, low_powermode_current);
#endif

	/* get led red brightness ratio */
	ret = of_property_read_u32(np, br_ratio_r, &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-s2mu005-rgb: %s, can't parsing brightness_ratio_r in dt\n", __func__);
	}
	else {
		brightness_ratio_r = (int)temp;
	}
	pr_info("leds-s2mu005-rgb: %s, brightness_ratio_r = %x\n", __func__, brightness_ratio_r);

	/* get led green brightness ratio */
	ret = of_property_read_u32(np, br_ratio_g, &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-s2mu005-rgb: %s, can't parsing brightness_ratio_g in dt\n", __func__);
	}
	else {
		brightness_ratio_g = (int)temp;
	}
	pr_info("leds-s2mu005-rgb: %s, brightness_ratio_g = %x\n", __func__, brightness_ratio_g);

	/* get led blue brightness ratio */
	ret = of_property_read_u32(np, br_ratio_b, &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-s2mu005-rgb: %s, can't parsing brightness_ratio_b in dt\n", __func__);
	}
	else {
		brightness_ratio_b = (int)temp;
	}
	pr_info("leds-s2mu005-rgb: %s, brightness_ratio_b = %x\n", __func__, brightness_ratio_b);

	return pdata;
}
#endif


#ifdef SEC_LED_SPECIFIC
#ifdef CONFIG_USE_LIGHT_SENSOR
static ssize_t store_s2mu005_rgb_lowpower(struct device *dev,
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
#endif
static ssize_t store_s2mu005_rgb_brightness(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int ret;
	u8 brightness;

	pr_info("leds-s2mu005-rgb: %s\n", __func__);

	ret = kstrtou8(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get led_brightness.\n");
		return count;
	}
#ifdef CONFIG_USE_LIGHT_SENSOR
	led_lowpower_mode = 0;
#endif
	if (brightness > LED_MAX_CURRENT)
		brightness = LED_MAX_CURRENT;

	led_dynamic_current = brightness;

	dev_dbg(dev, "led brightness set to %i\n", brightness);

	return count;
}

static ssize_t store_s2mu005_rgb_pattern(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	unsigned int mode = 0;
	int ret;
	pr_info("leds-s2mu005-rgb: %s\n", __func__);

	ret = sscanf(buf, "%1d", &mode);
	if (ret == 0) {
		dev_err(dev, "fail to get led_pattern mode.\n");
		return count;
	}

	/* Set all LEDs Off */
	s2mu005_rgb_reset(dev);
	if (mode == PATTERN_OFF)
		return count;

#ifdef CONFIG_USE_LIGHT_SENSOR
	/* Set to low power consumption mode */
	if (led_lowpower_mode == 1)
		led_dynamic_current = low_powermode_current;
	else
#endif
		led_dynamic_current = normal_powermode_current;

	switch (mode) {

	case CHARGING:
		s2mu005_rgb_set_state(&s2mu005_rgb->led[RED], led_dynamic_current, RGBLED_ALWAYS_ON);
		break;
	case CHARGING_ERR:
		s2mu005_rgb_blink(&s2mu005_rgb->led[RED], 500, 500);
		s2mu005_rgb_set_state(&s2mu005_rgb->led[RED], led_dynamic_current, RGBLED_BLINK);
		break;
	case MISSED_NOTI:
		s2mu005_rgb_blink(&s2mu005_rgb->led[BLUE], 500, 5000);
		s2mu005_rgb_set_state(&s2mu005_rgb->led[BLUE], led_dynamic_current, RGBLED_BLINK);
		break;
	case LOW_BATTERY:
		s2mu005_rgb_blink(&s2mu005_rgb->led[RED], 500, 5000);
		s2mu005_rgb_set_state(&s2mu005_rgb->led[RED], led_dynamic_current, RGBLED_BLINK);
		break;
	case FULLY_CHARGED:
		s2mu005_rgb_set_state(&s2mu005_rgb->led[GREEN], led_dynamic_current, RGBLED_ALWAYS_ON);
		break;
	case VOICE_REC:
		s2mu005_rgb_blink(&s2mu005_rgb->led[BLUE], 500, 5000);
		s2mu005_rgb_set_state(&s2mu005_rgb->led[BLUE], led_dynamic_current, RGBLED_BLINK);
		break;
	case POWERING:
		s2mu005_rgb_ramp(&s2mu005_rgb->led[GREEN], 800, 800);
		s2mu005_rgb_blink(&s2mu005_rgb->led[GREEN], 200, 200);
		s2mu005_rgb_set_state(&s2mu005_rgb->led[BLUE], led_dynamic_current, RGBLED_ALWAYS_ON);
		s2mu005_rgb_set_state(&s2mu005_rgb->led[GREEN], led_dynamic_current, RGBLED_BLINK);
		break;
	default:
		break;
	}

	return count;
}

static ssize_t store_s2mu005_rgb_blink(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	int led_brightness = 0;
	int delay_on_time = 0;
	int delay_off_time = 0;
	u8 led_r_brightness = 0;
	u8 led_g_brightness = 0;
	u8 led_b_brightness = 0;
	unsigned int led_total_br = 0;
	unsigned int led_max_br = 0;
	int ret;
	int stable_on_time = 0;
	int ramp_up_time = 0;
	int ramp_down_time = 0;
	int temp;

	ret = sscanf(buf, "0x%8x %5d %5d", &led_brightness,
					&delay_on_time, &delay_off_time);
	if (ret == 0) {
		dev_err(dev, "fail to get led_blink value.\n");
		return count;
	}

	/* Set to low power consumption mode */
	led_dynamic_current = normal_powermode_current;

	/*Reset led*/
	s2mu005_rgb_reset(dev);

	led_r_brightness = (led_brightness & LED_R_MASK) >> 16;
	led_g_brightness = (led_brightness & LED_G_MASK) >> 8;
	led_b_brightness = led_brightness & LED_B_MASK;

	/* In user case, LED current is restricted to less than tuning value */
	if (led_r_brightness != 0) {
		led_r_brightness = (led_r_brightness * led_dynamic_current) / LED_MAX_CURRENT;
		if (led_r_brightness == 0)
			led_r_brightness = 1;
	}
	if (led_g_brightness != 0) {
		led_g_brightness = (led_g_brightness * led_dynamic_current) / LED_MAX_CURRENT;
		if (led_g_brightness == 0)
			led_g_brightness = 1;
	}
	if (led_b_brightness != 0) {
		led_b_brightness = (led_b_brightness * led_dynamic_current) / LED_MAX_CURRENT;
		if (led_b_brightness == 0)
			led_b_brightness = 1;
	}

	led_total_br += led_r_brightness * brightness_ratio_r / 100;
	led_total_br += led_g_brightness * brightness_ratio_g / 100;
	led_total_br += led_b_brightness * brightness_ratio_b / 100;

	if (brightness_ratio_r >= brightness_ratio_g &&
		brightness_ratio_r >= brightness_ratio_b) {
		led_max_br = normal_powermode_current * brightness_ratio_r / 100;
	} else if (brightness_ratio_g >= brightness_ratio_r &&
		brightness_ratio_g >= brightness_ratio_b) {
		led_max_br = normal_powermode_current * brightness_ratio_g / 100;
	} else if (brightness_ratio_b >= brightness_ratio_r &&
		brightness_ratio_b >= brightness_ratio_g) {
		led_max_br = normal_powermode_current * brightness_ratio_b / 100;
	}

	if (led_total_br > led_max_br) {
		if (led_r_brightness != 0) {
			led_r_brightness = led_r_brightness * led_max_br / led_total_br;
			if (led_r_brightness == 0)
				led_r_brightness = 1;
		}
		if (led_g_brightness != 0) {
			led_g_brightness = led_g_brightness * led_max_br / led_total_br;
			if (led_g_brightness == 0)
				led_g_brightness = 1;
		}
		if (led_b_brightness != 0) {
			led_b_brightness = led_b_brightness * led_max_br / led_total_br;
			if (led_b_brightness == 0)
				led_b_brightness = 1;
		}
	}

	if ( delay_off_time == 0 ) {
		if (led_r_brightness) {
			s2mu005_rgb_set_state(&s2mu005_rgb->led[RED], led_r_brightness, RGBLED_ALWAYS_ON);
		}
		if (led_g_brightness) {
			s2mu005_rgb_set_state(&s2mu005_rgb->led[GREEN], led_g_brightness, RGBLED_ALWAYS_ON);
		}
		if (led_b_brightness) {
			s2mu005_rgb_set_state(&s2mu005_rgb->led[BLUE], led_b_brightness, RGBLED_ALWAYS_ON);
		}
	} else {
		if (led_r_brightness) {
			s2mu005_rgb_set_state(&s2mu005_rgb->led[RED], led_r_brightness, RGBLED_BLINK);
		}
		if (led_g_brightness) {
			s2mu005_rgb_set_state(&s2mu005_rgb->led[GREEN], led_g_brightness, RGBLED_BLINK);
		}
		if (led_b_brightness) {
			s2mu005_rgb_set_state(&s2mu005_rgb->led[BLUE], led_b_brightness, RGBLED_BLINK);
		}

		//set 2/5 of delay_on_time for ramp up/down time each and 1/5 for stable_on_time
		if(delay_on_time < 300){
			ramp_up_time = ramp_down_time=0;
			stable_on_time = delay_on_time;
		}
		else{
			temp = delay_on_time/5;
			ramp_down_time = ramp_up_time = temp*2;
			stable_on_time = delay_on_time - (temp << 2);
		}

		/*Set LED ramp mode*/
		s2mu005_rgb_ramp(&s2mu005_rgb->led[RED],ramp_up_time,ramp_down_time);
		s2mu005_rgb_ramp(&s2mu005_rgb->led[GREEN],ramp_up_time,ramp_down_time);
		s2mu005_rgb_ramp(&s2mu005_rgb->led[BLUE],ramp_up_time,ramp_down_time);

		/*Set LED blink mode*/
		s2mu005_rgb_blink(&s2mu005_rgb->led[RED], stable_on_time, delay_off_time);
		s2mu005_rgb_blink(&s2mu005_rgb->led[GREEN], stable_on_time, delay_off_time);
		s2mu005_rgb_blink(&s2mu005_rgb->led[BLUE], stable_on_time, delay_off_time);
	}

	pr_info("leds-s2mu005-rgb: %s, delay_on_time= %x, delay_off_time= %x\n",
			__func__, delay_on_time, delay_off_time);
	dev_dbg(dev, "led_blink is called, Color:0x%X Brightness:%i\n",
			led_brightness, led_dynamic_current);
	return count;
}

static ssize_t store_led_r(struct device *dev,
			struct device_attribute *devattr,
				const char *buf, size_t count)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}
	if (brightness != 0) {
		s2mu005_rgb_set_state(&s2mu005_rgb->led[RED], brightness, RGBLED_ALWAYS_ON);
	} else {
		s2mu005_rgb_set_state(&s2mu005_rgb->led[RED], LED_OFF, RGBLED_DISABLE);
	}
out:
	pr_info("leds-s2mu005-rgb: %s\n", __func__);
	return count;
}
static ssize_t store_led_g(struct device *dev,
			struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}
	if (brightness != 0) {
		s2mu005_rgb_set_state(&s2mu005_rgb->led[GREEN], brightness, RGBLED_ALWAYS_ON);
	} else {
		s2mu005_rgb_set_state(&s2mu005_rgb->led[GREEN], LED_OFF, RGBLED_DISABLE);
	}
out:
	pr_info("leds-s2mu005-rgb: %s\n", __func__);
	return count;
}
static ssize_t store_led_b(struct device *dev,
		struct device_attribute *devattr,
		const char *buf, size_t count)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}
	if (brightness != 0) {
		s2mu005_rgb_set_state(&s2mu005_rgb->led[BLUE], brightness, RGBLED_ALWAYS_ON);
	} else	{
		s2mu005_rgb_set_state(&s2mu005_rgb->led[BLUE], LED_OFF, RGBLED_DISABLE);
	}
out:
	pr_info("leds-s2mu005-rgb: %s\n", __func__);
	return count;
}
#endif

/* Added for led common class */
static ssize_t led_delay_on_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", s2mu005_rgb->delay_on_times_ms);
}

static ssize_t led_delay_on_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	unsigned int time;

	if (kstrtouint(buf, 0, &time)) {
		dev_err(dev, "can not write led_delay_on\n");
		return count;
	}

	s2mu005_rgb->delay_on_times_ms = time;

	return count;
}

static ssize_t led_delay_off_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", s2mu005_rgb->delay_off_times_ms);
}

static ssize_t led_delay_off_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	unsigned int time;

	if (kstrtouint(buf, 0, &time)) {
		dev_err(dev, "can not write led_delay_off\n");
		return count;
	}

	s2mu005_rgb->delay_off_times_ms = time;

	return count;
}

static ssize_t led_blink_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	const struct device *parent = dev->parent;
	struct s2mu005_rgb *s2mu005_rgb_num = dev_get_drvdata(parent);
	//struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	unsigned int blink_set;
	int n = 0;
	int i;

	if (!sscanf(buf, "%1d", &blink_set)) {
		dev_err(dev, "can not write led_blink\n");
		return count;
	}

	if (!blink_set) {
		s2mu005_rgb_num->delay_on_times_ms = LED_OFF;
		s2mu005_rgb_num->delay_off_times_ms = LED_OFF;
	}

	for (i = 0; i < S2MU005_RGBLED_NUM; i++) {
		if (dev == s2mu005_rgb_num->led[i].dev)
			n = i;
	}

	s2mu005_rgb_blink(&s2mu005_rgb_num->led[n],
		s2mu005_rgb_num->delay_on_times_ms,
		s2mu005_rgb_num->delay_off_times_ms);
	s2mu005_rgb_set_state(&s2mu005_rgb_num->led[n], led_dynamic_current, RGBLED_BLINK);

	pr_info("leds-s2mu005-rgb: %s\n", __func__);
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
static DEVICE_ATTR(led_pattern, 0660, NULL, store_s2mu005_rgb_pattern);
static DEVICE_ATTR(led_blink, 0660, NULL,  store_s2mu005_rgb_blink);
static DEVICE_ATTR(led_brightness, 0660, NULL, store_s2mu005_rgb_brightness);
#ifdef CONFIG_USE_LIGHT_SENSOR
static DEVICE_ATTR(led_lowpower, 0660, NULL,  store_s2mu005_rgb_lowpower);
#endif
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
#ifdef CONFIG_USE_LIGHT_SENSOR
	&dev_attr_led_lowpower.attr,
#endif
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};
#endif
static int s2mu005_rgb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct s2mu005_rgb_platform_data *pdata;
	struct s2mu005_rgb *s2mu005_rgb;
	struct s2mu005_dev *s2mu005_dev = dev_get_drvdata(dev->parent);
	char temp_name[S2MU005_RGBLED_NUM][40] = {{0,},}, name[40] = {0,}, *p;
	int i, ret;

	pr_info("leds-s2mu005-rgb: %s\n", __func__);

	window_color = (get_lcd_attached("GET") & 0x700) >> 8;
#ifdef CONFIG_OF
	pdata = s2mu005_rgb_parse_dt(dev);
	if (unlikely(IS_ERR(pdata)))
		return PTR_ERR(pdata);

	led_dynamic_current = normal_powermode_current;
#else
	pdata = dev_get_platdata(dev);
#endif

	pr_info("leds-s2mu005-rgb: %s : window_color=%x \n", __func__, window_color);

	s2mu005_rgb = devm_kzalloc(dev, sizeof(struct s2mu005_rgb), GFP_KERNEL);
	if (unlikely(!s2mu005_rgb))
		return -ENOMEM;
	
	platform_set_drvdata(pdev, s2mu005_rgb);
	s2mu005_rgb->i2c = s2mu005_dev->i2c;

	for (i = 0; i < S2MU005_RGBLED_NUM; i++) {
		s2mu005_rgb->ch[i] = pdata->ch[i];

		ret = snprintf(name, 30, "%s", pdata->name[i])+1;
		if (1 > ret)
			goto alloc_err_flash;

		p = devm_kzalloc(dev, ret, GFP_KERNEL);
		if (unlikely(!p))
			goto alloc_err_flash;

		strcpy(p, name);
		strcpy(temp_name[i], name);
		s2mu005_rgb->led[i].name = p;
		s2mu005_rgb->led[i].brightness_set = s2mu005_rgb_set;
		s2mu005_rgb->led[i].brightness_get = s2mu005_rgb_get;
		s2mu005_rgb->led[i].max_brightness = LED_MAX_CURRENT;

		ret = led_classdev_register(dev, &s2mu005_rgb->led[i]);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "unable to register RGB : %d\n", ret);
			goto alloc_err_flash_plus;
		}
		ret = sysfs_create_group(&s2mu005_rgb->led[i].dev->kobj,
						&common_led_attr_group);
		if (ret) {
			dev_err(dev, "can not register sysfs attribute\n");
			goto register_err_flash;
		}
	}

#ifdef SEC_LED_SPECIFIC
#ifdef CONFIG_SEC_SYSFS
	led_dev = sec_device_create(s2mu005_rgb, "led");
#else
	led_dev = device_create(sec_class, NULL, 0, s2mu005_rgb, "led");
#endif
	if (IS_ERR(led_dev)) {
		dev_err(dev, "Failed to create device for samsung specific led\n");
		goto create_err_flash;
	}

	ret = sysfs_create_group(&led_dev->kobj, &sec_led_attr_group);
	if (ret < 0) {
		dev_err(dev, "Failed to create sysfs group for samsung specific led\n");
		goto device_create_err;
	}
#endif

	pr_info("leds-s2mu005-rgb: %s done\n", __func__);

	return 0;

#ifdef SEC_LED_SPECIFIC
device_create_err:
#ifdef CONFIG_SEC_SYSFS
	sec_device_destroy(led_dev->devt);
#endif
create_err_flash:
    	sysfs_remove_group(&led_dev->kobj, &common_led_attr_group);
#endif
register_err_flash:
	led_classdev_unregister(&s2mu005_rgb->led[i]);
alloc_err_flash_plus:
	devm_kfree(dev, temp_name[i]);
alloc_err_flash:
	while (i--) {
		led_classdev_unregister(&s2mu005_rgb->led[i]);
		devm_kfree(dev, temp_name[i]);
	}
	devm_kfree(dev, s2mu005_rgb);
	return -ENOMEM;
}

#ifdef MODULE
static int __exit s2mu005_rgb_remove(struct platform_device *pdev)
{
	struct s2mu005_rgb *s2mu005_rgb = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < S2MU005_RGBLED_NUM; i++)
		led_classdev_unregister(&s2mu005_rgb->led[i]);

	return 0;
}
#endif

static void s2mu005_rgb_shutdown(struct device *dev)
{
	struct s2mu005_rgb *s2mu005_rgb = dev_get_drvdata(dev);
	int i;

	if (!s2mu005_rgb->i2c)
		return;

	s2mu005_rgb_reset(dev);

#ifdef SEC_LED_SPECIFIC
	sysfs_remove_group(&led_dev->kobj, &sec_led_attr_group);
#endif

	for (i = 0; i < S2MU005_RGBLED_NUM; i++){
		sysfs_remove_group(&s2mu005_rgb->led[i].dev->kobj,
						&common_led_attr_group);
		led_classdev_unregister(&s2mu005_rgb->led[i]);
	}
	devm_kfree(dev, s2mu005_rgb);
}
static struct platform_driver s2mu005_fled_driver = {
	.driver		= {
		.name	= "leds-s2mu005-rgb",
		.owner	= THIS_MODULE,
		.shutdown = s2mu005_rgb_shutdown,
	},
	.probe		= s2mu005_rgb_probe,
	.remove		= __exit_p(s2mu005_rgb_remove),
};

static int __init s2mu005_rgb_init(void)
{
	pr_info("leds-s2mu005-rgb: %s\n", __func__);
	return platform_driver_register(&s2mu005_fled_driver);
}
module_init(s2mu005_rgb_init);

static void __exit s2mu005_rgb_exit(void)
{
	platform_driver_unregister(&s2mu005_fled_driver);
}
module_exit(s2mu005_rgb_exit);

MODULE_ALIAS("platform:s2mu005-rgb");
MODULE_DESCRIPTION("s2mu005 RGB driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
