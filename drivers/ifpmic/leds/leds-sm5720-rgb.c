/*
 *  leds-sm5720-rgb.c
 *  Samsung SM5720 RGB-LED Driver
 *
 *  Copyright (C) 2016 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/leds.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/mfd/sm5720.h>
#include <linux/mfd/sm5720-private.h>
#include <linux/leds-sm5720-rgb.h>
#include <linux/sec_class.h>

#define SEC_LED_SPECIFIC

#define DIMTT_TIME(time)	((time == 0) ? 0 :			\
				(time < 500) ? 1 :			\
				(time < 8000) ? time/500 : 15)

enum {
    RGBW_MODE_CC          = 0x0,
    RGBW_MODE_Dimming,
};

extern int get_lcd_attached(char*);

/**
 * SM5720 RGBW register control functions 
 */

static u8 _calc_delay_time_offset_to_ms(unsigned int delay_ms)
{
    u8 offset;

    offset = DIMTT_TIME(delay_ms) & 0xf; /* step = 500ms */

    return offset;
}

static u8 _calc_step_time_offset_to_ms(unsigned int step_ms)
{
    u8 offset = (step_ms / 4) & 0xf; /* step = 4ms */

    return offset;
}

static int sm5720_RGBW_set_xLEDON(struct sm5720_rgb *rgb, int color_index, bool enable)
{
    if(WARN_ON(color_index<0 || color_index>LED_MAX)){
		pr_warn("sm5720-rgb: %s: invalid color_index=%d\n", __func__, color_index);
		return -1;
	}
	sm5720_update_reg(rgb->i2c, SM5720_CHG_REG_RGBWCNTL1, 
                      ((enable & 0x1) << (0 + color_index)), (0x1 << (0 + color_index)));

    return 0;
}

static u8 sm5720_RGBW_get_xLEDON(struct sm5720_rgb *rgb, int color_index)
{
    u8 reg = 0x0;

	if(WARN_ON(color_index<0 || color_index>LED_MAX)){
		pr_warn("sm5720-rgb: %s: invalid color_index=%d\n", __func__, color_index);
		return -1;
	}
    sm5720_read_reg(rgb->i2c, SM5720_CHG_REG_RGBWCNTL1, &reg);

    return ((reg & (0x1 << color_index)) >> color_index);
}

static int sm5720_RGBW_set_xLEDMODE(struct sm5720_rgb *rgb, int color_index, bool mode)
{
	if(WARN_ON(color_index<0 || color_index>LED_MAX)){
		pr_warn("sm5720-rgb: %s: invalid color_index=%d\n", __func__, color_index);
		return -1;
	}
    sm5720_update_reg(rgb->i2c, SM5720_CHG_REG_RGBWCNTL1, 
                      ((mode & 0x1) << (4 + color_index)), (0x1 << (4 + color_index)));

    return 0;
}

static int sm5720_RGBW_set_xLEDCURRENT(struct sm5720_rgb *rgb, int color_index, u8 curr)
{
	if(WARN_ON(color_index<0 || color_index>LED_MAX)){
		pr_warn("sm5720-rgb: %s: invalid color_index=%d\n", __func__, color_index);
		return -1;
	}
    sm5720_write_reg(rgb->i2c, SM5720_CHG_REG_RLEDCURRENT + color_index, curr);

    return 0;
}

static u8 sm5720_RGBW_get_xLEDCURRENT(struct sm5720_rgb *rgb, int color_index)
{
	u8 reg;

	if(WARN_ON(color_index<0 || color_index>LED_MAX)){
		pr_warn("sm5720-rgb: %s: invalid color_index=%d\n", __func__, color_index);
		return -1;
	}
    sm5720_read_reg(rgb->i2c, SM5720_CHG_REG_RLEDCURRENT + color_index, &reg);

    return reg;
}

static int sm5720_RGBW_set_DIMSLPxLEDCNTL(struct sm5720_rgb *rgb, int color_index, 
                                          unsigned int delay_on, unsigned int delay_off, unsigned int step_on, unsigned int step_off)
{
    u8 reg_value = ((_calc_delay_time_offset_to_ms(delay_off) & 0xf) << 4) | 
                    (_calc_delay_time_offset_to_ms(delay_on) & 0xf);

    u8 step_on_reg_value = ((_calc_step_time_offset_to_ms(step_on) & 0xf) << 4) |
                    (_calc_step_time_offset_to_ms(step_on) & 0xf);
    
    u8 step_off_reg_value = ((_calc_step_time_offset_to_ms(step_off) & 0xf) << 4) |
                    (_calc_step_time_offset_to_ms(step_off) & 0xf);

	if(WARN_ON(color_index<0 || color_index>LED_MAX)){
		pr_warn("sm5720-rgb: %s: invalid color_index=%d\n", __func__, color_index);
		return -1;
	}

    sm5720_write_reg(rgb->i2c, SM5720_CHG_REG_DIMSLPRLEDCNTL + color_index, reg_value);

    sm5720_write_reg(rgb->i2c, SM5720_CHG_REG_RLEDCNTL3 + (color_index * 4), step_on_reg_value);
    sm5720_write_reg(rgb->i2c, SM5720_CHG_REG_RLEDCNTL4 + (color_index * 4), step_off_reg_value);

    return 0;
}
/*
static int sm5720_RGBW_set_MAXDUTYx(struct sm5720_rgb *rgb, int color_index, u8 duty)
{
    sm5720_update_reg(rgb->i2c, SM5720_CHG_REG_RLEDCNTL1 + (color_index * 4), ((duty & 0xf) << 4), (0xf << 4));

    return 0;
}

static int sm5720_RGBW_set_MIDDUTYx(struct sm5720_rgb *rgb, int color_index, u8 duty)
{
    sm5720_update_reg(rgb->i2c, SM5720_CHG_REG_RLEDCNTL1 + (color_index * 4), ((duty & 0xf) << 0), (0xf << 0));

    return 0;
}

static int sm5720_RGBW_set_MINDUTYx(struct sm5720_rgb *rgb, int color_index, u8 duty)
{
    sm5720_update_reg(rgb->i2c, SM5720_CHG_REG_RLEDCNTL2 + (color_index * 4), ((duty & 0xf) << 0), (0xf << 0));

    return 0;
}
*/
static int sm5720_RGBW_print_reg(struct sm5720_rgb *rgb)
{
	u8 regs[SM5720_CHG_REG_END] = {0x0, };
	unsigned short cnt = SM5720_CHG_REG_HAPTICCNTL - SM5720_CHG_REG_RGBWCNTL1;
	int i;

	sm5720_bulk_read(rgb->i2c, SM5720_CHG_REG_RGBWCNTL1, cnt, regs);

	pr_info("sm5720-rgb: ");

	for (i = 0; i < cnt; ++i) {
		pr_info("0x%x:0x%x ", SM5720_CHG_REG_RGBWCNTL1 + i, regs[i]);
	}

	pr_info("\n");

	return 0;
}

/**
 * SM5705 RGBW control support functions
 */
/*
static int sm5720_rgb_duty(struct sm5720_rgb *sm5720_rgb, 
                           int color_index, u8 max_duty, u8 mid_duty, u8 min_duty)
{
    sm5720_RGBW_set_MAXDUTYx(sm5720_rgb, color_index, max_duty);
    sm5720_RGBW_set_MIDDUTYx(sm5720_rgb, color_index, mid_duty);
    sm5720_RGBW_set_MINDUTYx(sm5720_rgb, color_index, min_duty);

    return 0;
}
*/
static int sm5720_rgb_reset(struct sm5720_rgb *sm5720_rgb)
{
    sm5720_update_reg(sm5720_rgb->i2c, SM5720_CHG_REG_RGBWCNTL2, (0x1 << 4), (0x1 << 4));

    return 0;
}

static int sm5720_rgb_blink(struct sm5720_rgb *sm5720_rgb, int color_index, 
                            unsigned int delay_on_time_ms, unsigned int delay_off_time_ms, unsigned int step_on_time_ms, unsigned int step_off_time_ms)
{
    sm5720_RGBW_set_DIMSLPxLEDCNTL(sm5720_rgb, color_index, delay_on_time_ms, delay_off_time_ms, step_on_time_ms, step_off_time_ms);

    return 0;
}

static int sm5720_rgb_set_state(struct sm5720_rgb *sm5720_rgb, int color_index,
                                u8 brightness, u8 mode)
{
	int value;
	value = brightness;

	pr_info("sm5720-rgb: %s: color_index=%d, brightness=%d, mode=%d\n", __func__, color_index, brightness, mode);

	if(brightness != 0) {
		/* apply brightness ratio for optimize each led brightness*/
		switch(color_index) {
			case RED:
				if (led_lowpower_mode == 1 && (device_type == 4 || device_type == 5))
					value = brightness * brightness_ratio_r_low;
				else
					value = brightness * brightness_ratio_r;
				break;
			case GREEN:
				if (led_lowpower_mode == 1 && (device_type == 4 || device_type == 5))
					value = brightness * brightness_ratio_g_low;
				else
					value = brightness * brightness_ratio_g;
				break;
			case BLUE:
				if (led_lowpower_mode == 1 && (device_type == 4 || device_type == 5))
					value = brightness * brightness_ratio_b_low;
				else
					value = brightness * brightness_ratio_b;
				break;
		}
		/*
		   There is possibility that value becomes 0 even brightness>0 && ratio>0.
		   ex) brightness is 10 & brightness_ratio_r is 9
		   value = 10 * 9 / 100 = 0.9
		   value is inteager, so value is 0.
		   In this case, it is need to assign 1 of value.
		 */
		if(value != 0){
			value /= 100;
			if(value == 0)
				value = 1;
			if(value > LED_MAX_CURRENT)
				value = LED_MAX_CURRENT;
		}
	}
	pr_info("sm5720-rgb: LED[%d] CURRENT = %02d.%dmA\n", color_index, (value / 10), (value % 10));

	sm5720_RGBW_set_xLEDCURRENT(sm5720_rgb, color_index, value);

	switch (mode) {
		case LED_DISABLE:
			sm5720_RGBW_set_xLEDON(sm5720_rgb, color_index, 0);
			break;
		case LED_ALWAYS_ON:
			sm5720_RGBW_set_xLEDMODE(sm5720_rgb, color_index, RGBW_MODE_CC);
			sm5720_RGBW_set_xLEDON(sm5720_rgb, color_index, 1);
			break;
		case LED_BLINK:
			sm5720_RGBW_set_xLEDMODE(sm5720_rgb, color_index, RGBW_MODE_Dimming);
			sm5720_RGBW_set_xLEDON(sm5720_rgb, color_index, 1);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

/**
 * SM5720 RGBW SAMSUNG specific led device control functions
 */

#ifdef SEC_LED_SPECIFIC

static ssize_t store_led_r(struct device *dev,
			struct device_attribute *devattr,
				const char *buf, size_t count)
{
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}

	if (brightness > LED_MAX_CURRENT)
		brightness = LED_MAX_CURRENT;	

	if (brightness != 0) {
		sm5720_rgb_set_state(sm5720_rgb, RED, brightness, LED_ALWAYS_ON);
	} else {
		sm5720_rgb_set_state(sm5720_rgb,RED, LED_OFF, LED_DISABLE);
	}
out:
	pr_info("leds-sm5720-rgb: %s\n", __func__);
	return count;
}
static ssize_t store_led_g(struct device *dev,
			struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}

	if (brightness > LED_MAX_CURRENT)
		brightness = LED_MAX_CURRENT;

	if (brightness != 0) {
		sm5720_rgb_set_state(sm5720_rgb, GREEN, brightness, LED_ALWAYS_ON);
	} else {
		sm5720_rgb_set_state(sm5720_rgb, GREEN, LED_OFF, LED_DISABLE);
	}
out:
	pr_info("leds-sm5720-rgb: %s\n", __func__);
	return count;
}
static ssize_t store_led_b(struct device *dev,
		struct device_attribute *devattr,
		const char *buf, size_t count)
{
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

	ret = kstrtouint(buf, 0, &brightness);
	if (ret != 0) {
		dev_err(dev, "fail to get brightness.\n");
		goto out;
	}

	if (brightness > LED_MAX_CURRENT)
		brightness = LED_MAX_CURRENT;

	if (brightness != 0) {
		sm5720_rgb_set_state(sm5720_rgb, BLUE, brightness, LED_ALWAYS_ON);
	} else	{
		sm5720_rgb_set_state(sm5720_rgb, BLUE, LED_OFF, LED_DISABLE);
	}
out:
	pr_info("leds-sm5720-rgb: %s\n", __func__);
	return count;
}

static ssize_t store_sm5720_rgb_pattern(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);
	unsigned int mode = 0;
	int ret;
	pr_info("leds-sm5720-rgb: %s, lowpower_mode : %d\n", __func__,led_lowpower_mode);

	ret = sscanf(buf, "%1d", &mode);
	if (ret == 0) {
		dev_err(dev, "fail to get led_pattern mode.\n");
		return count;
	}

	if (mode > POWERING)
		return count;

	/* Set all LEDs Off */
	sm5720_rgb_reset(sm5720_rgb);
	if (mode == PATTERN_OFF)
		return count;

	/* Set to low power consumption mode */
	if (led_lowpower_mode == 1)
		led_dynamic_current = low_powermode_current;
	else
		led_dynamic_current = normal_powermode_current;

	switch (mode) {
	case CHARGING:
		sm5720_rgb_set_state(sm5720_rgb, RED, led_dynamic_current, LED_ALWAYS_ON);
		break;
	case CHARGING_ERR:
		sm5720_rgb_blink(sm5720_rgb, RED, 500, 500, 0, 0);
		sm5720_rgb_set_state(sm5720_rgb, RED, led_dynamic_current, LED_BLINK);
		break;
	case MISSED_NOTI:
		sm5720_rgb_blink(sm5720_rgb, BLUE, 500, 5000, 0, 0);
		sm5720_rgb_set_state(sm5720_rgb, BLUE, led_dynamic_current, LED_BLINK);
		break;
	case LOW_BATTERY:
		sm5720_rgb_blink(sm5720_rgb, RED, 500, 5000, 0, 0);
		sm5720_rgb_set_state(sm5720_rgb, RED, led_dynamic_current, LED_BLINK);
		break;
	case FULLY_CHARGED:
		sm5720_rgb_set_state(sm5720_rgb, GREEN, led_dynamic_current, LED_ALWAYS_ON);
		break;
	case POWERING:
		sm5720_rgb_blink(sm5720_rgb, GREEN, 1000, 1000, 8, 8);
		sm5720_rgb_set_state(sm5720_rgb, GREEN, led_dynamic_current, LED_BLINK);
		sm5720_rgb_set_state(sm5720_rgb, BLUE, led_dynamic_current, LED_ALWAYS_ON);
		break;
	default:
		break;
	}

	pr_info("sm5720-rgb: %s: mode=%d, led_dynamic_current=%d\n", __func__, mode, led_dynamic_current);
	sm5720_RGBW_print_reg(sm5720_rgb);

	return count;
}

static ssize_t store_sm5720_rgb_blink(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);
	int led_brightness = 0;
	int delay_on_time = 0;
	int delay_off_time = 0;
	int step_on_time = 0;
	int step_off_time = 0;
	u8 led_r_brightness = 0;
	u8 led_g_brightness = 0;
	u8 led_b_brightness = 0;
	unsigned int led_total_br = 0;
	unsigned int led_max_br = 0;
	int ret;

	ret = sscanf(buf, "0x%8x %5d %5d %5d %5d", &led_brightness,
					&delay_on_time, &delay_off_time, &step_on_time, &step_off_time);
	if (ret == 0) {
		dev_err(dev, "fail to get led_blink value.\n");
		return count;
	}

	/* Set to low power consumption mode */
	led_dynamic_current = normal_powermode_current;
	/*Reset led*/
	sm5720_rgb_reset(sm5720_rgb);

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

	/* Each color decreases according to the limit at the same rate. */
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

	if (led_r_brightness) {
        sm5720_rgb_blink(sm5720_rgb, RED, delay_on_time, delay_off_time, step_on_time, step_off_time);
		sm5720_rgb_set_state(sm5720_rgb, RED, led_r_brightness, LED_BLINK);
	}
	if (led_g_brightness) {
        sm5720_rgb_blink(sm5720_rgb, GREEN, delay_on_time, delay_off_time, step_on_time, step_off_time);
		sm5720_rgb_set_state(sm5720_rgb, GREEN, led_g_brightness, LED_BLINK);
	}
	if (led_b_brightness) {
        sm5720_rgb_blink(sm5720_rgb, BLUE, delay_on_time, delay_off_time, step_on_time, step_off_time);
		sm5720_rgb_set_state(sm5720_rgb, BLUE, led_b_brightness, LED_BLINK);
	}

	pr_info("leds-sm5720-rgb: %s, on_time= %x, off_time= %x, step_on_time= %x, step_off_time= %x\n", __func__, delay_on_time, delay_off_time, step_on_time, step_off_time);
	dev_dbg(dev, "led_blink is called, Color:0x%X Brightness:%i\n",
			led_brightness, led_dynamic_current);

	sm5720_RGBW_print_reg(sm5720_rgb);

	return count;
}

static ssize_t store_sm5720_rgb_lowpower(struct device *dev,
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

	pr_info("led_lowpower mode set to %i\n", led_lowpower);
	dev_dbg(dev, "led_lowpower mode set to %i\n", led_lowpower);

	return count;
}
static ssize_t store_sm5720_rgb_brightness(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int ret;
	u8 brightness;
	pr_info("leds-sm5720-rgb: %s\n", __func__);

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

static ssize_t rgb_tuning_show(struct device *dev,
				struct device_attribute *attr, 
				char *buf)
{
	//struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);

	return sprintf(buf, "sm5720-rgb: %s [RED : %d, GREEN : %d, BLUE : %d]\n", __func__, brightness_ratio_r , brightness_ratio_g, brightness_ratio_b);
}

static ssize_t rgb_tuning_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	//struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);
	int ret,red,green,blue;

	ret = sscanf(buf, "%d %d %d", &red, &green, &blue);
	brightness_ratio_r = red;
	brightness_ratio_g = green;
	brightness_ratio_b = blue;

	return count;
}


/* below nodes is SAMSUNG specific nodes */
static DEVICE_ATTR(led_r, 0660, NULL, store_led_r);
static DEVICE_ATTR(led_g, 0660, NULL, store_led_g);
static DEVICE_ATTR(led_b, 0660, NULL, store_led_b);
/* led_pattern node permission is 222 */
/* To access sysfs node from other groups */
static DEVICE_ATTR(led_pattern, 0660, NULL, store_sm5720_rgb_pattern);
static DEVICE_ATTR(led_blink, 0660, NULL,  store_sm5720_rgb_blink);
static DEVICE_ATTR(led_brightness, 0660, NULL, store_sm5720_rgb_brightness);
static DEVICE_ATTR(led_lowpower, 0660, NULL,  store_sm5720_rgb_lowpower);
static DEVICE_ATTR(led_tuning, 0660, rgb_tuning_show, rgb_tuning_store);

static struct attribute *sec_led_attributes[] = {
	&dev_attr_led_r.attr,
	&dev_attr_led_g.attr,
	&dev_attr_led_b.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_brightness.attr,
	&dev_attr_led_lowpower.attr,
	&dev_attr_led_tuning.attr,
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};
#endif

/**
 * SM5720 RGBW common led-class device control functions
 */

static int sm5720_get_color_index_to_led_dev(struct sm5720_rgb *sm5720_rgb, 
                                             struct led_classdev *led_cdev)
{
    int i;

    for (i=0; i < LED_MAX; ++i) {
        if (&sm5720_rgb->led[i] == led_cdev) {
            return i;
        }
    }

    return -ENODEV;
}

static void sm5720_rgb_set(struct led_classdev *led_cdev,
				unsigned int brightness)
{
	const struct device *parent = led_cdev->dev->parent;
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(parent);
	int color_index;

	color_index = sm5720_get_color_index_to_led_dev(sm5720_rgb, led_cdev);
	if (IS_ERR_VALUE(color_index)) {
		dev_err(led_cdev->dev,
			"sm5720_rgb_number() returns %d.\n", color_index);
		return;
	}

	if (brightness == LED_OFF) {
		sm5720_RGBW_set_xLEDON(sm5720_rgb, color_index, 0);
	} else {
		sm5720_RGBW_set_xLEDCURRENT(sm5720_rgb, color_index, brightness);
		sm5720_RGBW_set_xLEDMODE(sm5720_rgb, color_index, RGBW_MODE_CC);
        	sm5720_RGBW_set_xLEDON(sm5720_rgb, color_index, 1);
	}

	pr_info("leds-sm5720-rgb: %s: color_index=%d, brightness=%d\n", __func__, color_index, brightness);
	sm5720_RGBW_print_reg(sm5720_rgb);
}

static unsigned int sm5720_rgb_get(struct led_classdev *led_cdev)
{
	const struct device *parent = led_cdev->dev->parent;
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(parent);
	int color_index;
	u8 value;

	pr_info("leds-sm5720-rgb: %s\n", __func__);

    	color_index = sm5720_get_color_index_to_led_dev(sm5720_rgb, led_cdev);
	if (IS_ERR_VALUE(color_index)) {
		dev_err(led_cdev->dev,
			"sm5720_rgb_number() returns %d.\n", color_index);
		return 0;
	}

	/* Get status */
    	value = sm5720_RGBW_get_xLEDON(sm5720_rgb, color_index);
	if (!value) {
    	    return LED_OFF;
    	}

	/* Get current */
	value = sm5720_RGBW_get_xLEDCURRENT(sm5720_rgb, color_index);

	return value;
}

static ssize_t led_delay_on_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", sm5720_rgb->delay_on_times_ms);
}

static ssize_t led_delay_on_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);
	unsigned int time;

	if (kstrtouint(buf, 0, &time)) {
		dev_err(dev, "can not write led_delay_on\n");
		return count;
	}

	sm5720_rgb->delay_on_times_ms = time;

	return count;
}

static ssize_t led_delay_off_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", sm5720_rgb->delay_off_times_ms);
}

static ssize_t led_delay_off_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);
	unsigned int time;

	if (kstrtouint(buf, 0, &time)) {
		dev_err(dev, "can not write led_delay_off\n");
		return count;
	}

	sm5720_rgb->delay_off_times_ms = time;

	return count;
}

static ssize_t led_blink_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sm5720_rgb *sm5720_rgb = dev_get_drvdata(dev);
	unsigned int blink_set;
	int n = 0;
	int i;

	if (!sscanf(buf, "%1d", &blink_set)) {
		dev_err(dev, "can not write led_blink\n");
		return count;
	}

	if (!blink_set) {
		sm5720_rgb->delay_on_times_ms = LED_OFF;
		sm5720_rgb->delay_off_times_ms = LED_OFF;
	}

	for (i = 0; i < 4; i++) {
		if (dev == sm5720_rgb->led[i].dev)
			n = i;
	}

	sm5720_rgb_blink(sm5720_rgb, n , 
                     sm5720_rgb->delay_on_times_ms, 
                     sm5720_rgb->delay_off_times_ms, 0, 0);
	sm5720_rgb_set_state(sm5720_rgb, n,
                         led_dynamic_current, 
                         LED_BLINK);

	pr_info("leds-sm5720-rgb: %s\n", __func__);
	return count;
}

/* permission for sysfs node */
static DEVICE_ATTR(delay_on, 0640, led_delay_on_show, led_delay_on_store);
static DEVICE_ATTR(delay_off, 0640, led_delay_off_show, led_delay_off_store);
static DEVICE_ATTR(blink, 0640, NULL, led_blink_store);

static struct attribute *led_class_attrs[] = {
	&dev_attr_delay_on.attr,
	&dev_attr_delay_off.attr,
	&dev_attr_blink.attr,
	NULL,
};

static struct attribute_group common_led_attr_group = {
	.attrs = led_class_attrs,
};

/**
 * SM5720 RGBW Platform driver handling functions 
 */

struct device_type_match {
	char name[20];
	unsigned int dev_num;
};

static struct device_type_match device_type_table[] = {
	{"hero", 0},
	{"hero2", 1},
	{"poseidon", 2},
	{"grace", 3},
	{"dream1", 4},
	{"dream2", 5},
};

#ifdef CONFIG_OF
static struct sm5720_rgb_platform_data
			*sm5720_rgb_parse_dt(struct device *dev)
{
	struct sm5720_rgb_platform_data *pdata;
	struct device_node *np;
	int ret;
	int i;
	int temp;
	const char *temp_str;
	char octa[4] = {0, };
	char br_ratio_r[23] = "br_ratio_r";
	char br_ratio_g[23] = "br_ratio_g";
	char br_ratio_b[23] = "br_ratio_b";
	char br_ratio_r_low[23] = "br_ratio_r_low";
	char br_ratio_g_low[23] = "br_ratio_g_low";
	char br_ratio_b_low[23] = "br_ratio_b_low";
	char normal_po_cur[29] = "normal_powermode_current";
	char low_po_cur[26] = "low_powermode_current";

	pr_info("leds-sm5720-rgb: %s\n", __func__);

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

		pr_info("leds-sm5720-rgb: %s, %s\n", __func__,pdata->name[i]);

		if (IS_ERR_VALUE(ret)) {
			devm_kfree(dev, pdata);
			return ERR_PTR(ret);
		}
	}

	/* get device_type value in dt */
	ret = of_property_read_string(np, "device_type", &temp_str);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-sm5720-rgb: %s, can't parsing device_type in dt\n", __func__);
	} else {
		for (i = 0; i < ARRAY_SIZE(device_type_table); i++) {
			if(!strncmp(device_type_table[i].name,
				temp_str, strlen(device_type_table[i].name))) {
					device_type = device_type_table[i].dev_num;
				break;
			}
		}
		if(i == ARRAY_SIZE(device_type_table))
			pr_err("leds-max77854-rgb: can't find matching device_type\n");
	}
	pr_info("leds-sm5720-rgb: %s, device_type = %x\n", __func__, device_type);

	/* HERO */
	if(device_type == 0) {
		switch(octa_color) {
		case 0:
			strcpy(octa, "_bk");
			break;
		case 1:
			strcpy(octa, "_wh");
			break;
		case 2:
			strcpy(octa, "_sv");
			break;
		case 3:
			strcpy(octa, "_gd");
			break;
		default:
			break;
		}
	}
	/* HERO2 */
	else if(device_type == 1) {
		switch(octa_color) {
		case 0:
			strcpy(octa, "_bk");
			break;
		case 1:
			strcpy(octa, "_wh");
			break;
		case 2:
			strcpy(octa, "_gd");
			break;
		case 3:
			strcpy(octa, "_sv");
			break;
		default:
			break;
		}
	}
	/* DREAM1 */
	else if(device_type == 4) {
		switch(octa_color) {
		case 0:
			strcpy(octa, "_u");
			break;
		case 1:
			strcpy(octa, "_bk");
			break;
		case 3:
			strcpy(octa, "_gd");
			break;
		case 4:
			strcpy(octa, "_sv");
			break;
		case 6:
			strcpy(octa, "_bl");
			break;
		default:
			break;
		}
	}
	/* DREAM2 */
	else if(device_type == 5) {
		switch(octa_color) {
		case 0:
			strcpy(octa, "_u");
			break;
		case 1:
			strcpy(octa, "_bk");
			break;
		case 3:
			strcpy(octa, "_gd");
			break;
		case 4:
			strcpy(octa, "_sv");
			break;
		case 6:
			strcpy(octa, "_bl");
			break;
		default:
			break;
		}
	}
	strcat(normal_po_cur, octa);
	strcat(low_po_cur, octa);
	strcat(br_ratio_r, octa);
	strcat(br_ratio_g, octa);
	strcat(br_ratio_b, octa);
	strcat(br_ratio_r_low, octa);
	strcat(br_ratio_g_low, octa);
	strcat(br_ratio_b_low, octa);

	/* get normal_powermode_current value in dt */
	ret = of_property_read_u32(np, normal_po_cur, &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-sm5720-rgb: %s, can't parsing normal_powermode_current in dt\n", __func__);
	}
	else {
		normal_powermode_current = (u8)temp;
	}
	pr_info("leds-sm5720-rgb: %s, normal_powermode_current = %x\n", __func__, normal_powermode_current);

	/* get low_powermode_current value in dt */
	ret = of_property_read_u32(np, low_po_cur, &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-sm5720-rgb: %s, can't parsing low_powermode_current in dt\n", __func__);
	}
	else
		low_powermode_current = (u8)temp;
	pr_info("leds-sm5720-rgb: %s, low_powermode_current = %x\n", __func__, low_powermode_current);

	/* get led red brightness ratio */
	ret = of_property_read_u32(np, br_ratio_r, &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-sm5720-rgb: %s, can't parsing brightness_ratio_r in dt\n", __func__);
	}
	else {
		brightness_ratio_r = (int)temp;
	}
	pr_info("leds-sm5720-rgb: %s, brightness_ratio_r = %x\n", __func__, brightness_ratio_r);

	/* get led green brightness ratio */
	ret = of_property_read_u32(np, br_ratio_g, &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-sm5720-rgb: %s, can't parsing brightness_ratio_g in dt\n", __func__);
	}
	else {
		brightness_ratio_g = (int)temp;
	}
	pr_info("leds-sm5720-rgb: %s, brightness_ratio_g = %x\n", __func__, brightness_ratio_g);

	/* get led blue brightness ratio */
	ret = of_property_read_u32(np, br_ratio_b, &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_info("leds-sm5720-rgb: %s, can't parsing brightness_ratio_b in dt\n", __func__);
	}
	else {
		brightness_ratio_b = (int)temp;
	}
	pr_info("leds-sm5720-rgb: %s, brightness_ratio_b = %x\n", __func__, brightness_ratio_b);

	/* For Dream */
	if (device_type == 4 || device_type == 5) {
		/* get led red brightness ratio lowpower */
		ret = of_property_read_u32(np, br_ratio_r_low, &temp);
		if (IS_ERR_VALUE(ret)) {
			pr_info("leds-sm5720-rgb: %s, can't parsing "\
					"brightness_ratio_r_low in dt\n", __func__);
		}
		else {
			brightness_ratio_r_low = (int)temp;
		}
		pr_info("leds-sm5720-rgb: %s, brightness_ratio_r_low = %x\n",
				__func__, brightness_ratio_r_low);

		/* get led green brightness ratio lowpower*/
		ret = of_property_read_u32(np, br_ratio_g_low, &temp);
		if (IS_ERR_VALUE(ret)) {
			pr_info("leds-sm5720-rgb: %s, can't parsing "\
					"brightness_ratio_g_low in dt\n", __func__);
		}
		else {
			brightness_ratio_g_low = (int)temp;
		}
		pr_info("leds-sm5720-rgb: %s, brightness_ratio_g_low = %x\n",
				__func__, brightness_ratio_g_low);

		/* get led blue brightness ratio lowpower */
		ret = of_property_read_u32(np, br_ratio_b_low, &temp);
		if (IS_ERR_VALUE(ret)) {
			pr_info("leds-sm5720-rgb: %s, can't parsing "\
					"brightness_ratio_b_low in dt\n", __func__);
		}
		else {
			brightness_ratio_b_low = (int)temp;
		}
		pr_info("leds-sm5720-rgb: %s, brightness_ratio_b_low = %x\n",
				__func__, brightness_ratio_b_low);
	}

	return pdata;
}
#endif

static int sm5720_rgb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sm5720_rgb_platform_data *pdata;
	struct sm5720_rgb *sm5720_rgb;
	struct sm5720_dev *sm5720_dev = dev_get_drvdata(dev->parent);
	char temp_name[4][40] = {{0,},}, name[40] = {0,}, *p;
	int i, ret;

	pr_info("leds-sm5720-rgb: %s\n", __func__);

	octa_color = (get_lcd_attached("GET") >> 16) & 0x0000000f;
#ifdef CONFIG_OF
	pdata = sm5720_rgb_parse_dt(dev);
	if (unlikely(IS_ERR(pdata)))
		return PTR_ERR(pdata);

	led_dynamic_current = normal_powermode_current;
#else
	pdata = dev_get_platdata(dev);
#endif

	pr_info("leds-sm5720-rgb: %s : octa_color=%x device_type=%x \n",
		__func__, octa_color, device_type);

	sm5720_rgb = devm_kzalloc(dev, sizeof(struct sm5720_rgb), GFP_KERNEL);
	if (unlikely(!sm5720_rgb))
		return -ENOMEM;

	platform_set_drvdata(pdev, sm5720_rgb);
	sm5720_rgb->i2c = sm5720_dev->charger;

	for (i = 0; i < 4; i++) {
		ret = snprintf(name, 30, "%s", pdata->name[i])+1;
		if (1 > ret)
			goto alloc_err_flash;

		p = devm_kzalloc(dev, ret, GFP_KERNEL);
		if (unlikely(!p))
			goto alloc_err_flash;

		strcpy(p, name);
		strcpy(temp_name[i], name);
		sm5720_rgb->led[i].name = p;
		sm5720_rgb->led[i].brightness_set = sm5720_rgb_set;
		sm5720_rgb->led[i].brightness_get = sm5720_rgb_get;
		sm5720_rgb->led[i].max_brightness = LED_MAX_CURRENT;

		ret = led_classdev_register(dev, &sm5720_rgb->led[i]);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "unable to register RGB : %d\n", ret);
			goto alloc_err_flash_plus;
		}
		ret = sysfs_create_group(&sm5720_rgb->led[i].dev->kobj,
						&common_led_attr_group);
		if (ret < 0) {
			dev_err(dev, "can not register sysfs attribute\n");
			goto register_err_flash;
		}
	}

#ifdef SEC_LED_SPECIFIC
	//sm5720_rgb->led_dev = device_create(sec_class, NULL, 0, sm5720_rgb, "led");
	sm5720_rgb->led_dev = sec_device_create(0, sm5720_rgb, "led");
	if (IS_ERR(sm5720_rgb->led_dev)) {
		dev_err(dev, "Failed to create device for samsung specific led\n");
		goto alloc_err_flash;
	}


	ret = sysfs_create_group(&sm5720_rgb->led_dev->kobj, &sec_led_attr_group);
	if (ret < 0) {
		dev_err(dev, "Failed to create sysfs group for samsung specific led\n");
		goto alloc_err_flash;
	}
#endif

	sm5720_rgb->dev = dev;

	pr_info("leds-sm5720-rgb: %s done\n", __func__);

	return 0;

register_err_flash:
	led_classdev_unregister(&sm5720_rgb->led[i]);
alloc_err_flash_plus:
	devm_kfree(dev, temp_name[i]);
alloc_err_flash:
	while (i--) {
		led_classdev_unregister(&sm5720_rgb->led[i]);
		devm_kfree(dev, temp_name[i]);
	}
	devm_kfree(dev, sm5720_rgb);
	return -ENOMEM;
}

static int sm5720_rgb_remove(struct platform_device *pdev)
{
	struct sm5720_rgb *sm5720_rgb = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < 4; i++)
		led_classdev_unregister(&sm5720_rgb->led[i]);

	return 0;
}

static void sm5720_rgb_shutdown(struct platform_device *pdev)
{
	struct sm5720_rgb *sm5720_rgb = platform_get_drvdata(pdev);
	int i;

	if (!sm5720_rgb->i2c)
		return;
	
	pr_info("leds-sm5720-rgb: entering %s\n", __func__);

	sm5720_rgb_reset(sm5720_rgb);

#ifdef SEC_LED_SPECIFIC
	sysfs_remove_group(&sm5720_rgb->led_dev->kobj, &sec_led_attr_group);
#endif

	for (i = 0; i < 4; i++){
		sysfs_remove_group(&sm5720_rgb->led[i].dev->kobj,
						&common_led_attr_group);
		led_classdev_unregister(&sm5720_rgb->led[i]);
	}
	devm_kfree(&pdev->dev, sm5720_rgb);
}

static struct platform_driver sm5720_fled_driver = {
	.driver		= {
		.name	= "leds-sm5720-rgb",
		.owner	= THIS_MODULE,
	},
	.probe		= sm5720_rgb_probe,
	.remove		= sm5720_rgb_remove,
	.shutdown       = sm5720_rgb_shutdown,
};

static int __init sm5720_rgb_init(void)
{
	pr_info("leds-sm5720-rgb: %s\n", __func__);
	return platform_driver_register(&sm5720_fled_driver);
}
module_init(sm5720_rgb_init);

static void __exit sm5720_rgb_exit(void)
{
	platform_driver_unregister(&sm5720_fled_driver);
}
module_exit(sm5720_rgb_exit);

MODULE_DESCRIPTION("Samsung SM5720 RGB-LED Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");

