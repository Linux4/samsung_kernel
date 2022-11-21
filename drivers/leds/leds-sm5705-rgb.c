/*
 * RGB-LED device driver for SM5705
 *
 * Copyright (C) 2015 Silicon Mitus 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
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
#include <linux/mfd/sm5705/sm5705.h>
#include <linux/leds-sm5705-rgb.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/sec_sysfs.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

enum sm5705_rgb_color_index {
	LED_R = 0x0,
	LED_G,
	LED_B,
    LED_MAX,
};

enum se_led_pattern_index {
	PATTERN_OFF,
	CHARGING,
	CHARGING_ERR,
	MISSED_NOTI,
	LOW_BATTERY,
	FULLY_CHARGED,
	POWERING,
};

#define LED_MAX_CURRENT     0xFF

struct sm5705_rgb {
    struct device *dev;
    struct i2c_client *i2c;

    struct led_classdev led[LED_MAX];
    struct device *led_dev;

	unsigned int delay_on_times_ms;
	unsigned int delay_off_times_ms;

    unsigned char en_lowpower_mode;
};

/* device configuartion parameters */
static unsigned char normal_powermode_current   = 0x28;
static unsigned char low_powermode_current      = 0x05;
static unsigned char br_ratio_r = 100;
static unsigned char br_ratio_g = 100;
static unsigned char br_ratio_b = 100;
static int gpio_vdd = -1;

//extern unsigned int lcdtype;
extern struct class *sec_class;

/**
 * SM5705 RGB-LED device control functions 
 */

static unsigned char calc_delaytime_offset_to_ms(struct sm5705_rgb *sm5705_rgb, unsigned int ms)
{
    unsigned char time;

    if (ms > 7500) {
        dev_dbg(sm5705_rgb->dev, "SM5705 delay time limit = 7.5sec, correct input value (ms=%d)\n", ms);
        time = 0xF;
    } else {
        time = ms / 500;
    }

    return time;
}

static unsigned char calc_steptime_offset_to_ms(struct sm5705_rgb *sm5705_rgb, unsigned int ms)
{
    unsigned char time;

    if (ms > 60) {
        dev_dbg(sm5705_rgb->dev, "SM5705 step time limit = 60ms, correct input value (ms=%d)\n", ms);
        time = 0xF;
    } else {
        time = ms / 4;
    }

    return time;
}

static int sm5705_rgb_set_LEDx_enable(struct sm5705_rgb *sm5705_rgb, int index, bool mode, bool enable)
{
    unsigned char reg_val;
    int ret;

	if(gpio_vdd > 0)
	{
		gpio_direction_output(gpio_vdd, enable ? 1 : 0);
		msleep(5);
	}

    ret = sm5705_read_reg(sm5705_rgb->i2c, SM5705_REG_CNTLMODEONOFF, &reg_val);
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to read REG:SM5705_REG_CNTLMODEONOFF\n", __func__);
        return ret;
    }

    if (mode) {
        reg_val |= (1 << (4 + index));
    } else {
        reg_val &= ~(1 << (4 + index));
    }

    if (enable) {
        reg_val |= (1 << (0 + index));
    } else {
        reg_val &= ~(1 << (0 + index));
    }

    ret = sm5705_write_reg(sm5705_rgb->i2c, SM5705_REG_CNTLMODEONOFF, reg_val);
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to write REG:SM5705_REG_CNTLMODEONOFF\n", __func__);
        return ret;
    }

    dev_info(sm5705_rgb->dev, "%s: REG:SM5705_REG_CNTLMODEONOFF = 0x%x\n", __func__, reg_val);

    return 0;
}

static int sm5705_rgb_set_LEDx_current(struct sm5705_rgb *sm5705_rgb, int index, unsigned char cur_value)
{
    int ret;

    ret = sm5705_write_reg(sm5705_rgb->i2c, SM5705_REG_RLEDCURRENT + index, cur_value);
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to write LED[%d] CURRENT REG (value=%d)\n", __func__, index, cur_value);
        return ret;
    }

    dev_info(sm5705_rgb->dev, "%s: led current is %d\n", __func__, cur_value);

    return 0;
}

static int sm5705_rgb_get_LEDx_current(struct sm5705_rgb *sm5705_rgb, int index)
{
    unsigned char reg_val;
    int ret;

    /* check led on/off state, if led off than current must be 0 */
    ret = sm5705_read_reg(sm5705_rgb->i2c, SM5705_REG_CNTLMODEONOFF, &reg_val);
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to read REG:SM5705_REG_CNTLMODEONOFF\n", __func__);
        return ret;
    }
    if (!(reg_val & (1 << (0 + index)))) {
        return 0;
    }

    ret = sm5705_read_reg(sm5705_rgb->i2c, SM5705_REG_RLEDCURRENT + index, &reg_val);
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to read LED[%d] CURRENT REG\n", __func__, index);
        return ret;
    }

    return reg_val;
}

static int sm5705_rgb_set_LEDx_slopeduty(struct sm5705_rgb *sm5705_rgb, int index, unsigned char max_duty, unsigned char mid_duty, unsigned char min_duty)
{
    unsigned char reg_val;
    int ret;

    reg_val = ((max_duty & 0xF) << 4) | (mid_duty & 0xF);
    ret = sm5705_write_reg(sm5705_rgb->i2c, SM5705_REG_RLEDCNTL1 + (4 * index), reg_val);
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to write LED[%d] LEDCNTL1 REG (value=%d)\n", __func__, index, reg_val);
        return ret;
    }

    ret = sm5705_update_reg(sm5705_rgb->i2c, SM5705_REG_RLEDCNTL2 + (index * 4), (min_duty & 0xF), 0x0F);
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to update LED[%d] MINDUTY REG (value=%d)\n", __func__, index, min_duty);
        return ret;
    }

    return 0;
}

static int sm5705_rgb_set_LEDx_slopemode(struct sm5705_rgb *sm5705_rgb, u32 index, u32 delay_ms, u32 delay_on_time_ms, u32 delay_off_time_ms, u32 step_on_time_ms, u32 step_off_time_ms)
{
    unsigned char delay, tt1, tt2, dt1, dt2, dt3, dt4;
    int ret;

    delay = calc_delaytime_offset_to_ms(sm5705_rgb, delay_ms);
    tt1 = calc_delaytime_offset_to_ms(sm5705_rgb, delay_on_time_ms);
    tt2 = calc_delaytime_offset_to_ms(sm5705_rgb, delay_off_time_ms);
    dt1 = dt2 = calc_steptime_offset_to_ms(sm5705_rgb, step_on_time_ms);
    dt3 = dt4 = calc_steptime_offset_to_ms(sm5705_rgb, step_off_time_ms);
    dev_info(sm5705_rgb->dev, "%s: LED[%d] slopemode (delay:0x%x, tt1:0x%x, tt2:0x%x, dt1:0x%x, dt2:0x%x, dt3:0x%x, dt4:0x%x)\n", __func__, index, delay, tt1, tt2, dt1, dt2, dt3, dt4);

    ret = sm5705_update_reg(sm5705_rgb->i2c, SM5705_REG_RLEDCNTL2 + (index * 4), (delay & 0xF), 0xF0);
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to update LED[%d] START DELAY REG (value=%d)\n", __func__, index, delay);
        return ret;
    }

    ret = sm5705_write_reg(sm5705_rgb->i2c, SM5705_REG_DIMSLPRLEDCNTL + index, (((tt2 & 0xF) << 4) | (tt1 & 0xF)));
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to write LED[%d] DIMSLPLEDCNTL REG (value=%d)\n", __func__, index, (((tt2 & 0xF) << 4) | (tt1 & 0xF)));
        return ret;
    }

    ret = sm5705_write_reg(sm5705_rgb->i2c, SM5705_REG_RLEDCNTL3 + (index * 4), (((dt2 & 0xF) << 4) | (dt1 & 0xF)));
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to write LED[%d] LEDCNTL3\n", __func__, index);
        return ret;
    }

    ret = sm5705_write_reg(sm5705_rgb->i2c, SM5705_REG_RLEDCNTL4 + (index * 4), (((dt4 & 0xF) << 4) | (dt3 & 0xF)));
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to write LED[%d] LEDCNTL3\n", __func__, index);
        return ret;
    }

    return 0;
}

static int sm5705_rgb_reset(struct sm5705_rgb *sm5705_rgb)
{
    int i, ret;

    /* RGB current & start delay reset */
    for (i=0; i < LED_MAX; ++i) {
        ret = sm5705_write_reg(sm5705_rgb->i2c, SM5705_REG_RLEDCURRENT + i, 0);
        if (IS_ERR_VALUE(ret)) {
            dev_err(sm5705_rgb->dev, "%s: fail to write LED[%d] CURRENT REG\n", __func__, i);
            return ret;
        }

        ret = sm5705_update_reg(sm5705_rgb->i2c, SM5705_REG_RLEDCNTL2 + (i * 4), 0, 0xF0);
        if (IS_ERR_VALUE(ret)) {
            dev_err(sm5705_rgb->dev, "%s: fail to update LED[%d] START DELAY REG\n", __func__, i);
            return ret;
        }
    }

    /* RGB LED OFF */
    ret = sm5705_write_reg(sm5705_rgb->i2c, SM5705_REG_CNTLMODEONOFF, 0);
    if (IS_ERR_VALUE(ret)) {
        dev_err(sm5705_rgb->dev, "%s: fail to write REG:SM5705_REG_CNTLMODEONOFF\n", __func__);
        return ret;
    }

    return 0;
}


/**
 * SM5705 RGB-LED SAMSUNG specific led device control functions
 */
static ssize_t store_led_r(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

    dev_dbg(dev, "%s\n", __func__);

	ret = kstrtouint(buf, 0, &brightness);
    if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s: fail to get brightness.\n", __func__);
        goto out;
	}

    ret = sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_R, brightness);
    if (IS_ERR_VALUE(ret)) {
        goto out;
    }
    ret = sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_R, 0, (bool)brightness);
    if (IS_ERR_VALUE(ret)) {
        goto out;
    }

    dev_dbg(dev, "%s:current=0x%x, constant %s\n", __func__, brightness, (brightness) ? "ON" : "OFF");

out:
    return count;
}

static ssize_t store_led_g(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

    dev_dbg(dev, "%s\n", __func__);

	ret = kstrtouint(buf, 0, &brightness);
    if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s: fail to get brightness.\n", __func__);
        goto out;
	}

    ret = sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_G, brightness);
    if (IS_ERR_VALUE(ret)) {
        goto out;
    }
    ret = sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_G, 0, (bool)brightness);
    if (IS_ERR_VALUE(ret)) {
        goto out;
    }

    dev_dbg(dev, "%s: current=0x%x, constant %s\n", __func__, brightness, (brightness) ? "ON" : "OFF");

out:
    return count;
}

static ssize_t store_led_b(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

    dev_dbg(dev, "%s\n", __func__);

	ret = kstrtouint(buf, 0, &brightness);
    if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s: fail to get brightness.\n", __func__);
        goto out;
	}

    ret = sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_B, brightness);
    if (IS_ERR_VALUE(ret)) {
        goto out;
    }
    ret = sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_B, 0, (bool)brightness);
    if (IS_ERR_VALUE(ret)) {
        goto out;
    }

    dev_dbg(dev, "%s: current=0x%x, constant %s\n", __func__, brightness, (brightness) ? "ON" : "OFF");

out:
    return count;
}

static ssize_t show_sm5705_rgb_brightness(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 4, "%d\n", normal_powermode_current);
}

static ssize_t store_sm5705_rgb_brightness(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
	u8 brightness;
    int ret;

	ret = kstrtou8(buf, 0, &brightness);
    if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s: fail to get brightness.\n", __func__);
        goto out;
	}

	sm5705_rgb->en_lowpower_mode = 0;

	if(brightness > LED_MAX_CURRENT)
		normal_powermode_current = LED_MAX_CURRENT;
	else
		normal_powermode_current = brightness;

    dev_info(dev, "%s: store brightness = 0x%x\n", __func__, brightness);

out:
	return count;
}

static ssize_t show_sm5705_rgb_lowpower(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", sm5705_rgb->en_lowpower_mode);
}

static ssize_t store_sm5705_rgb_lowpower(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
	int ret;

	ret = kstrtou8(buf, 0, &sm5705_rgb->en_lowpower_mode);
    if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s : fail to get led_lowpower_mode.\n", __func__);
        goto out;
	}

	dev_info(dev, "%s: led_lowpower mode set to %i\n", __func__, sm5705_rgb->en_lowpower_mode);

out:
	return count;
}

static unsigned char calc_led_current_from_brightness(struct sm5705_rgb *sm5705_rgb, int index, unsigned char brightness)
{
    unsigned char cur_value;

    cur_value = normal_powermode_current * brightness / LED_MAX_CURRENT;
    if (!cur_value) { cur_value = 1; }

    switch (index) {
    case LED_R:
        cur_value = (cur_value * br_ratio_r) / 100;
        break;
    case LED_G:
        cur_value = (cur_value * br_ratio_g) / 100;
        break;
    case LED_B:
        cur_value = (cur_value * br_ratio_b) / 100;
        break;
    }
    if (!cur_value) { cur_value = 1; }

    return cur_value;
}

static ssize_t store_sm5705_rgb_blink(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
    unsigned int led_brightness;
    unsigned char led_br[3];
    unsigned char led_current;
    int i, ret;

	ret = sscanf(buf, "0x%8x %5d %5d", &led_brightness, &sm5705_rgb->delay_on_times_ms, &sm5705_rgb->delay_off_times_ms);
    if (!ret) {
		dev_err(dev, "%s: fail to get led_blink value.\n", __func__);
		return count;
	}
    dev_info(dev, "%s: led_brightness=0x%x, delay_on_times=%dms, delay_off_time=%dms\n", __func__, led_brightness, sm5705_rgb->delay_on_times_ms, sm5705_rgb->delay_off_times_ms);
    led_br[0] = ((led_brightness & 0xFF0000) >> 16);
    led_br[1] = ((led_brightness & 0x00FF00) >> 8);
    led_br[2] = ((led_brightness & 0x0000FF) >> 0);

    sm5705_rgb_reset(sm5705_rgb);

    for (i=0; i < 3; ++i) {
        if (led_br[i]) {
            led_current = calc_led_current_from_brightness(sm5705_rgb, i, led_br[i]);
            sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, i, 0xF, 0xF, 0x0);
            sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, i, 0, sm5705_rgb->delay_on_times_ms, sm5705_rgb->delay_off_times_ms, 4, 4);
            sm5705_rgb_set_LEDx_current(sm5705_rgb, i, led_current);
            sm5705_rgb_set_LEDx_enable(sm5705_rgb, i, sm5705_rgb->delay_off_times_ms?1:0, 1);

            dev_info(dev, "%s: led[%d] slope mode ON (MAX:0x%x, MID:0x%x, MIN:0x%x, delay:%d)\n", __func__, i, 0xF, 0xF, 0x0, 0);
        }
    }

	return count;
}

static ssize_t store_sm5705_rgb_pattern(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
    unsigned char led_current;
	unsigned int mode;
	int ret;

	ret = sscanf(buf, "%1d", &mode);
	if (!ret) {
		dev_err(dev, "%s: fail to get led_pattern mode.\n", __func__);
		return count;
	}
    led_current = (sm5705_rgb->en_lowpower_mode) ? low_powermode_current : normal_powermode_current;

    dev_info(dev, "%s: pattern mode=%d led_current=0x%x(lowpower=%d)\n", __func__, mode, led_current, sm5705_rgb->en_lowpower_mode);

    ret = sm5705_rgb_reset(sm5705_rgb);
	if (IS_ERR_VALUE(ret)) {
        goto out;
    }

    switch (mode) {
    case CHARGING:
        /* LED_R constant mode ON */
        led_current = led_current * br_ratio_r / 100;
        sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_R, led_current);
        sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_R, 0, 1);
		break;
    case CHARGING_ERR:
        /* LED_R slope mode ON (500ms to 500ms) */
        led_current = led_current * br_ratio_r / 100;
        sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, LED_R, 0xF, 0xF, 0x0);
        sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, LED_R, 0, 500, 500, 4, 4);
        sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_R, led_current);
        sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_R, 1, 1);
		break;
    case MISSED_NOTI:
        /* LED_B slope mode ON (500ms to 5000ms) */
        led_current = led_current * br_ratio_b / 100;
        sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, LED_B, 0xF, 0xF, 0x0);
        sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, LED_B, 0, 500, 5000, 4, 4);
        sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_B, led_current);
        sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_B, 1, 1);
		break;
    case LOW_BATTERY:
        /* LED_R slope mode ON (500ms to 5000ms) */
        led_current = led_current * br_ratio_r / 100;
        sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, LED_R, 0xF, 0xF, 0x0);
        sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, LED_R, 0, 500, 5000, 4, 4);
        sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_R, led_current);
        sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_R, 1, 1);
		break;
    case FULLY_CHARGED:
        /* LED_G constant mode ON */
        led_current = led_current * br_ratio_g / 100;
        sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_G, led_current);
        sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_G, 0, 1);
		break;
    case POWERING:
        /* LED_G & LED_B slope mode ON (1000ms to 1000ms) */
        sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, LED_G, 0x8, 0x4, 0x1);
        sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, LED_G, 0, 1000, 1000, 12, 12);
        led_current = led_current * br_ratio_g / 100;
        sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_G, led_current);
        sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, LED_B, 0xF, 0xE, 0xC);
        sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, LED_B, 0, 1000, 1000, 28, 28);
        led_current = led_current * br_ratio_b / br_ratio_g;
        sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_B, led_current);

        sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_G, 1, 1);
        sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_B, 1, 1);
		break;
    case PATTERN_OFF:
	default:
		break;
    }

out:
	return count;
}

/* SAMSUNG specific attribute nodes */
static DEVICE_ATTR(led_r, 0660, NULL, store_led_r);
static DEVICE_ATTR(led_g, 0660, NULL, store_led_g);
static DEVICE_ATTR(led_b, 0660, NULL, store_led_b);
static DEVICE_ATTR(led_pattern, 0660, NULL, store_sm5705_rgb_pattern);
static DEVICE_ATTR(led_blink, 0660, NULL,  store_sm5705_rgb_blink);
static DEVICE_ATTR(led_brightness, 0660, show_sm5705_rgb_brightness, store_sm5705_rgb_brightness);
static DEVICE_ATTR(led_lowpower, 0660, show_sm5705_rgb_lowpower,  store_sm5705_rgb_lowpower);

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

/**
 * SM5705 RGB-LED common led-class device control functions
 */
static int get_rgb_index_from_led_cdev(struct sm5705_rgb *sm5705_rgb, struct led_classdev *led_cdev)
{
    int i;

    for (i=0; i < LED_MAX; ++i) {
        if (led_cdev == &sm5705_rgb->led[i]) {
            return i;
        }
    }

    dev_err(sm5705_rgb->dev, "%s: invalid led_cdev=%p\n", __func__, led_cdev);
    return -EINVAL;
}

static void sm5705_rgb_brightness_set(struct led_classdev *led_cdev, unsigned int brightness)
{
	const struct device *parent = led_cdev->dev->parent;
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(parent);
	struct device *dev = led_cdev->dev;
	int index, ret;

    dev_dbg(dev,"%s\n", __func__);

    index = get_rgb_index_from_led_cdev(sm5705_rgb, led_cdev);
	if (IS_ERR_VALUE(index)) {
        return;
    }

    ret = sm5705_rgb_set_LEDx_current(sm5705_rgb, index, brightness);
    if (IS_ERR_VALUE(ret)) {
        return;
    }
    ret = sm5705_rgb_set_LEDx_enable(sm5705_rgb, index, 0, (bool)brightness);
    if (IS_ERR_VALUE(ret)) {
        return;
    }

    dev_dbg(dev, "%s: led[%d] current=0x%x, constant %s\n", __func__, index, brightness, (brightness) ? "ON" : "OFF");
}

static unsigned int sm5705_rgb_brightness_get(struct led_classdev *led_cdev)
{
	const struct device *parent = led_cdev->dev->parent;
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(parent);
	struct device *dev = led_cdev->dev;
	int index, cur_value;

    dev_dbg(dev, "%s\n", __func__);

    index = get_rgb_index_from_led_cdev(sm5705_rgb, led_cdev);
	if (IS_ERR_VALUE(index)) {
        return 0;
    }

    cur_value = sm5705_rgb_get_LEDx_current(sm5705_rgb, index);

    dev_dbg(dev, "%s: led[%d] get current : 0x%x\n", __func__, index, cur_value);

    return cur_value;
}

static ssize_t led_delay_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev->parent);

	return sprintf(buf, "%d\n", sm5705_rgb->delay_on_times_ms);
}

static ssize_t led_delay_on_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev->parent);
	unsigned int time;

	if (kstrtouint(buf, 0, &time)) {
		dev_err(dev, "can not write led_delay_on (buf:%s)\n", buf);
        goto out;
	}
	sm5705_rgb->delay_on_times_ms = time;

out:
	return count;
}

static ssize_t led_delay_off_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev->parent);

	return sprintf(buf, "%d\n", sm5705_rgb->delay_off_times_ms);
}

static ssize_t led_delay_off_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev->parent);
	unsigned int time;

	if (kstrtouint(buf, 0, &time)) {
		dev_err(dev, "can not write led_delay_off (buf:%s)\n", buf);
        goto out;
	}
	sm5705_rgb->delay_off_times_ms = time;

out:
	return count;
}

static ssize_t led_blink_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev->parent);
	unsigned long blink_set;
    int index;

    dev_info(dev, ":%s\n", __func__);

	if (strict_strtoul(buf, 0, &blink_set)) {
        goto out;
    }

    index = get_rgb_index_from_led_cdev(sm5705_rgb, led_cdev);
	if (IS_ERR_VALUE(index)) {
        goto out;
    }

	if (!blink_set) {
		sm5705_rgb->delay_on_times_ms = LED_OFF;
		sm5705_rgb_brightness_set(led_cdev, LED_OFF);
	}

    sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, index, 0xF, 0xF, 0x0);
    sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, index, 0, sm5705_rgb->delay_on_times_ms, sm5705_rgb->delay_off_times_ms, 4, 4);
    sm5705_rgb_set_LEDx_current(sm5705_rgb, index, normal_powermode_current);
    sm5705_rgb_set_LEDx_enable(sm5705_rgb, index, 1, 1);

    dev_info(dev, "%s: delay_on_time:%d, delay_off_time:%d, current:0x%x\n", __func__, sm5705_rgb->delay_on_times_ms, sm5705_rgb->delay_off_times_ms, normal_powermode_current);

out:
    return count;
}

/* sysfs attribute nodes */
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
 * SM5705 RGB-LED device driver management functions 
 */

#ifdef CONFIG_OF
static inline void _decide_octa(char *octa, unsigned char octa_color)
{
    switch(octa_color) {
    case 0:
        strcpy(octa, "_bk");    break;
    case 1:
        strcpy(octa, "_wt");    break;
    case 2:
        strcpy(octa, "_gd");    break;
    default:
        break;
    }
}

static void make_property_string(char *dst, const char *src, const char *octa)
{
    strcpy(dst, src);
    strcat(dst, octa);
}

static int sm5705_rgb_parse_dt(struct device *dev, unsigned char octa_color, char **leds_name)
{
	struct device_node *nproot = dev->parent->of_node;
	struct device_node *np;
	char property_str[255] = {0, };
	char octa[16] = {0, };
	int i, ret, temp;

	np = of_find_node_by_name(nproot, "rgb");
	if (unlikely(np == NULL)) {
		dev_err(dev, "fail to find rgb node\n");
		return -ENOENT;
	}

	for (i = 0; i < LED_MAX; i++)	{
		ret = of_property_read_string_index(np, "rgb-name", i, (const char **)&leds_name[i]);
		dev_info(dev, "rgb-name[%d] string: ""%s""\n", i, leds_name[i]);
		if (IS_ERR_VALUE(ret)) {
			return -ENOENT;
		}
	}

	gpio_vdd = of_get_named_gpio(np, "rgb,vdd-gpio", 0);
	if(gpio_vdd < 0){
		dev_info(dev, "don't support gpio to lift up power supply!\n");
	}

    /* device type property is not necessary now */
#if 0
	ret = of_property_read_u32(np, "device_type", (unsigned int *)&i);
    if (IS_ERR_VALUE(ret)) {
        return -ENOENT;
    }
#endif

    _decide_octa(octa, octa_color);

    /* parsing dt:rgb-normal_powermode_current */
    make_property_string(property_str, "normal_powermode_current", octa);
	ret = of_property_read_u32(np, property_str, &temp);
	if (IS_ERR_VALUE(ret)) {
        dev_err(dev, "can't parsing [%s] in RGB dt\n", property_str);
	} else {
		normal_powermode_current = (unsigned char)temp;
	}

    /* parsing dt:rgb-low_powermode_current */
    make_property_string(property_str, "low_powermode_current", octa);
	ret = of_property_read_u32(np, property_str, &temp);
	if (IS_ERR_VALUE(ret)) {
        dev_err(dev, "can't parsing [%s] in RGB dt\n", property_str);
	} else {
		low_powermode_current = (unsigned char)temp;
	}

    /* parsing dt:rgb-br_ratio_r */
    make_property_string(property_str, "br_ratio_r", octa);
	ret = of_property_read_u32(np, property_str, &temp);
	if (IS_ERR_VALUE(ret)) {
        dev_err(dev, "can't parsing [%s] in RGB dt\n", property_str);
	} else {
		br_ratio_r = (unsigned char)temp;
	}

    /* parsing dt:rgb-br_ratio_g */
    make_property_string(property_str, "br_ratio_g", octa);
	ret = of_property_read_u32(np, property_str, &temp);
	if (IS_ERR_VALUE(ret)) {
        dev_err(dev, "can't parsing [%s] in RGB dt\n", property_str);
	} else {
		br_ratio_g = (unsigned char)temp;
	}

    /* parsing dt:rgb-br_ratio_b */
    make_property_string(property_str, "br_ratio_b", octa);
	ret = of_property_read_u32(np, property_str, &temp);
	if (IS_ERR_VALUE(ret)) {
        dev_err(dev, "can't parsing [%s] in RGB dt\n", property_str);
	} else {
		br_ratio_b = (unsigned char)temp;
	}

    dev_info(dev, "SM5705 RGB-LED device configuration info\n");
    dev_info(dev, "- normal_powermode_current = %d\n", normal_powermode_current);

    return 0;
}
#endif

static inline int _register_led_commonclass_dev(struct device *dev, struct sm5705_rgb *sm5705_rgb, char **leds_name)
{
    int i, ret;

    for (i=0; i < LED_MAX; ++i) {
        sm5705_rgb->led[i].name = leds_name[i];
        sm5705_rgb->led[i].max_brightness = LED_MAX_CURRENT;
        sm5705_rgb->led[i].brightness_set = sm5705_rgb_brightness_set;
        sm5705_rgb->led[i].brightness_get = sm5705_rgb_brightness_get;

		ret = led_classdev_register(dev, &sm5705_rgb->led[i]);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "unable to register led_classdev[%d] (ret=%d)\n", i, ret);
            goto led_classdev_register_err;
		}

		ret = sysfs_create_group(&sm5705_rgb->led[i].dev->kobj, &common_led_attr_group);
		if (IS_ERR_VALUE(ret)) {
			dev_err(dev, "can not register sysfs attribute for led[%d]\n", i);
            led_classdev_unregister(&sm5705_rgb->led[i]);
            goto led_classdev_register_err;
		}
    }

    return 0;

led_classdev_register_err:
    do {
		led_classdev_unregister(&sm5705_rgb->led[i]);        
        sysfs_remove_group(&sm5705_rgb->led[i].dev->kobj, &common_led_attr_group);
    } while (--i);

    return ret;
}

static inline void _unregister_led_commonclass_dev(struct sm5705_rgb *sm5705_rgb)
{
    int i;

    for (i=0; i < LED_MAX; ++i) {
		led_classdev_unregister(&sm5705_rgb->led[i]);        
        sysfs_remove_group(&sm5705_rgb->led[i].dev->kobj, &common_led_attr_group);
    }
}

extern int get_lcd_attached(char *mode);

static int sm5705_rgb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sm5705_rgb *sm5705_rgb;
	struct sm5705_dev *sm5705_dev = dev_get_drvdata(dev->parent);
	unsigned char octa_color = (get_lcd_attached("GET") & 0x0F0000) >> 16;
	char *leds_name[LED_MAX];
	int ret;

	sm5705_rgb = devm_kzalloc(dev, sizeof(struct sm5705_rgb), GFP_KERNEL);
	if (unlikely(!sm5705_rgb)) {
		dev_err(dev, "fail to allocate memory for sm5705_rgb\n");
		return -ENOMEM;
	}

	dev_info(dev," %s: lcdtype=0x%x, octa_color=0x%x\n", __func__, get_lcd_attached("GET"), octa_color);

#ifdef CONFIG_OF
	ret = sm5705_rgb_parse_dt(dev, octa_color, leds_name);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "fail to parse dt for sm5705 rgb-led (ret=%d)\n", ret);
		goto rgb_dt_parse_err;
	}
#endif

	sm5705_rgb->led_dev = device_create(sec_class, NULL, 0, sm5705_rgb, "led");
	if (unlikely(!sm5705_rgb->led_dev)) {
		dev_err(dev, "fail to create device for samsung specific led\n");
		goto rgb_dt_parse_err;
	}

	ret = sysfs_create_group(&sm5705_rgb->led_dev->kobj, &sec_led_attr_group);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "fail to create sysfs group for samsung specific led\n");
		goto sec_device_create_err;
	}

	sm5705_rgb->dev = dev;
	sm5705_rgb->i2c = sm5705_dev->i2c;
	platform_set_drvdata(pdev, sm5705_rgb);

#if 0
	ret = _register_led_commonclass_dev(dev, sm5705_rgb, leds_name);
	if (IS_ERR_VALUE(ret)) {
		goto sec_device_create_err;
	}
#endif

	if (gpio_vdd > 0) {
		ret = gpio_request(gpio_vdd, "sm5705-rgb_vdd_supply");
		if(ret < 0){
			dev_err(dev, "unable to request rgb_vdd_supply\n");
		}
	}

#if defined(CONFIG_LEDS_USE_ED28) && defined(CONFIG_SEC_FACTORY)
	if( lcdtype == 0 && jig_status == false) {
        /* LED_R constant mode ON */
        sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_R, normal_powermode_current);
        sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_R, 0, 1);
	}
#endif

    dev_info(dev, "RGB-LED device driver probe done\n");
    dev_info(dev, "normal_powermode_current: 0x%x\n", normal_powermode_current);
    dev_info(dev, "low_powermode_current: 0x%x\n", low_powermode_current);
    dev_info(dev, "br_ratio_r: %d\n", (int)br_ratio_r);
    dev_info(dev, "br_ratio_g: %d\n", (int)br_ratio_g);
    dev_info(dev, "br_ratio_b: %d\n", (int)br_ratio_b);

	return 0;

//sec_sysfs_create_group_err:
//    _unregister_led_commonclass_dev(sm5705_rgb);

sec_device_create_err:
	device_destroy(sec_class, sm5705_rgb->led_dev->devt);	

rgb_dt_parse_err:
    devm_kfree(dev, sm5705_rgb);

    return ret;
}

static int sm5705_rgb_remove(struct platform_device *pdev)
{
	struct sm5705_rgb *sm5705_rgb = platform_get_drvdata(pdev);

	int i;

	for (i = 0; i < 4; ++i) {
		led_classdev_unregister(&sm5705_rgb->led[i]);
    }

	return 0;
}

static void sm5705_rgb_shutdown(struct device *dev)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);

    dev_dbg(dev, "%s\n", __func__);

	if (!sm5705_rgb->i2c)
		return;

	sm5705_rgb_reset(sm5705_rgb);

	sysfs_remove_group(&sm5705_rgb->led_dev->kobj, &sec_led_attr_group);
    //_unregister_led_commonclass_dev(sm5705_rgb);

    devm_kfree(dev, sm5705_rgb);
}

static struct platform_driver sm5705_rgbled_driver = {
	.driver		= {
		.name	= "leds-sm5705-rgb",
		.owner	= THIS_MODULE,
		.shutdown = sm5705_rgb_shutdown,
	},
	.probe		= sm5705_rgb_probe,
	.remove		= sm5705_rgb_remove,
};

static int __init sm5705_rgb_init(void)
{
	return platform_driver_register(&sm5705_rgbled_driver);
}
module_init(sm5705_rgb_init);

static void __exit sm5705_rgb_exit(void)
{
	platform_driver_unregister(&sm5705_rgbled_driver);
}
module_exit(sm5705_rgb_exit);

MODULE_ALIAS("platform:sm5705-rgb");
MODULE_DESCRIPTION("SM5705 RGB driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
