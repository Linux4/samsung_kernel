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
#include <linux/of_gpio.h>
#include <linux/mfd/sm5705/sm5705.h>
#include <linux/leds-sm5705-rgb.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/sec_class.h>

extern int get_lcd_attached(char *mode);

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

enum sm5705_dt_params {
	OCTA_CODE,
	NORMAL_CUR,
	LOW_CUR,
	NORMAL_R_RATIO,
	NORMAL_G_RATIO,
	NORMAL_B_RATIO,
	LOW_R_RATIO,
	LOW_G_RATIO,
	LOW_B_RATIO,
	SIZE_OF_PARAMS,
};

#define LED_MAX_CURRENT		0xFF
#define LED_MAX_STR		6
#define LED_PROP_LENGTH		16

#define DEFAULT_OCTA_NAME	"Default"
#define DEFAULT_NORMAL_CUR	255
#define DEFAULT_LOW_CUR		10
#define DEFAULT_NORMAL_RATIO	100
#define DEFAULT_LOW_RATIO	100

struct br_ratio {
	u32 r;
	u32 g;
	u32 b;
};

struct sm5705_rgb {
	struct device *dev;
	struct i2c_client *i2c;

	struct led_classdev led[LED_MAX];
	struct device *led_dev;

	unsigned int delay_on_times_ms;
	unsigned int delay_off_times_ms;

	unsigned char en_lowpower_mode;

	/* device configuartion parameters */
	int gpio_vdd;
	u32 normal_cur;
	u32 low_cur;
	struct br_ratio normal_ratio;
	struct br_ratio low_ratio;
	const char *octa_name;
};

/**
 * SM5705 RGB-LED device control functions 
 */

static unsigned char calc_delaytime_offset_to_ms(struct sm5705_rgb *sm5705_rgb, unsigned int ms)
{
	unsigned char time;

	if (ms > 7500) {
		dev_dbg(sm5705_rgb->dev, "SM5705 delay time limit = 7.5sec, correct input value (ms=%d)\n", ms);
		time = 0xF;
	} else
		time = ms / 500;

	return time;
}

static unsigned char calc_steptime_offset_to_ms(struct sm5705_rgb *sm5705_rgb, unsigned int ms)
{
	unsigned char time;

	if (ms > 60) {
		dev_dbg(sm5705_rgb->dev, "SM5705 step time limit = 60ms, correct input value (ms=%d)\n", ms);
		time = 0xF;
	} else
		time = ms / 4;

	return time;
}

static int sm5705_rgb_set_LEDx_enable(struct sm5705_rgb *sm5705_rgb, int index, bool mode, bool enable)
{
	unsigned char reg_val;
	int ret;

	dev_info(sm5705_rgb->dev, "%s: index=%d, mode=%d, enable=%d\n",
			__func__, index, mode, enable);

	if (gpio_is_valid(sm5705_rgb->gpio_vdd)) {
		gpio_direction_output(sm5705_rgb->gpio_vdd, enable ? 1 : 0);
		msleep(20);
	}

	ret = sm5705_read_reg(sm5705_rgb->i2c, SM5705_REG_CNTLMODEONOFF, &reg_val);
	if (IS_ERR_VALUE(ret)) {
		dev_err(sm5705_rgb->dev, "%s: fail to read REG:SM5705_REG_CNTLMODEONOFF\n", __func__);
		return ret;
	}

	if (mode)
		reg_val |= (1 << (4 + index));
	else
		reg_val &= ~(1 << (4 + index));


	if (enable)
		reg_val |= (1 << (0 + index));
	else
		reg_val &= ~(1 << (0 + index));

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

	dev_info(sm5705_rgb->dev, "%s: LED[%d] Current Set: %d\n",
						__func__, index, cur_value);

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
	if (!(reg_val & (1 << (0 + index))))
		return 0;

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
	for (i = 0; i < LED_MAX; ++i) {
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

	/* gpio_vdd OFF */
	if (gpio_is_valid(sm5705_rgb->gpio_vdd))
		gpio_direction_output(sm5705_rgb->gpio_vdd, 0);

	return 0;
}


/**
 * SM5705 RGB-LED SAMSUNG specific led device control functions
 */
static int store_led_color(struct sm5705_rgb *sm5705_rgb, int idx, int brightness)
{
	int ret;

	ret = sm5705_rgb_set_LEDx_current(sm5705_rgb, idx, brightness);
	if (IS_ERR_VALUE(ret))
		return -1;

	ret = sm5705_rgb_set_LEDx_enable(sm5705_rgb, idx, false, !!brightness);
	if (IS_ERR_VALUE(ret))
		return -1;

	pr_debug("%s: rgb current=0x%x, constant %s\n",
			__func__, brightness, (brightness) ? "ON": "OFF");
	return 0;
}

static ssize_t store_led_r(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
	unsigned int brightness;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	ret = kstrtouint(buf, 0, &brightness);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s: fail to get brightness.\n", __func__);
		return count;
	}
	ret = store_led_color(sm5705_rgb, LED_R, brightness);
	if (ret)
		dev_err(dev, "%s: failed to set LED_R\n", __func__);

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
		return count;
	}

	ret = store_led_color(sm5705_rgb, LED_G, brightness);
	if (ret)
		dev_err(dev, "%s: failed to set LED_G\n", __func__);

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
		return count;
	}

	ret = store_led_color(sm5705_rgb, LED_B, brightness);
	if (ret)
		dev_err(dev, "%s: failed to set LED_B\n", __func__);

	return count;
}

static ssize_t show_sm5705_rgb_brightness(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", sm5705_rgb->normal_cur);
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

	if (brightness > LED_MAX_CURRENT)
		sm5705_rgb->normal_cur = LED_MAX_CURRENT;
	else
		sm5705_rgb->normal_cur = brightness;

	dev_dbg(dev, "%s: store brightness = 0x%x\n", __func__, brightness);

out:
	return count;
}

static ssize_t show_sm5705_rgb_lowpower(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", sm5705_rgb->en_lowpower_mode);
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
	unsigned char led_current;
	struct br_ratio *br_ratio;

	led_current = (!sm5705_rgb->en_lowpower_mode) ?
		sm5705_rgb->normal_cur : sm5705_rgb->low_cur;

	br_ratio = (!sm5705_rgb->en_lowpower_mode) ?
		&sm5705_rgb->normal_ratio : &sm5705_rgb->low_ratio;


	led_current = led_current * brightness / LED_MAX_CURRENT;

	switch (index) {
	case LED_R:
		led_current = (led_current * br_ratio->r) / 255;
		break;
	case LED_G:
		led_current = (led_current * br_ratio->g) / 255;
		break;
	case LED_B:
		led_current = (led_current * br_ratio->b) / 255;
		break;
	}
	if (!led_current) { led_current = 1; }

	return led_current;
}

static ssize_t store_sm5705_rgb_blink(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
	unsigned int led_brightness;
	unsigned char led_br[3];
	unsigned char led_current;
	int i, ret;

	ret = sscanf(buf, "0x%8x %5d %5d", &led_brightness,
			&sm5705_rgb->delay_on_times_ms, &sm5705_rgb->delay_off_times_ms);
	if (!ret) {
		dev_err(dev, "%s: fail to get led_blink value.\n", __func__);
		return count;
	}
	dev_info(dev, "%s: led_brightness=0x%X\n", __func__, led_brightness);
	dev_info(dev, "%s: delay_on_times=%dms\n", __func__, sm5705_rgb->delay_on_times_ms);
	dev_info(dev, "%s: delay_off_time=%dms\n", __func__, sm5705_rgb->delay_off_times_ms);
	led_br[0] = ((led_brightness & 0xFF0000) >> 16);
	led_br[1] = ((led_brightness & 0x00FF00) >> 8);
	led_br[2] = ((led_brightness & 0x0000FF) >> 0);

	sm5705_rgb_reset(sm5705_rgb);

	for (i=0; i < LED_MAX; ++i) {
		if (led_br[i]) {
			led_current = calc_led_current_from_brightness(sm5705_rgb, i, led_br[i]);
			sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, i, 0xF, 0xF, 0x0);
			sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, i, 0,
						sm5705_rgb->delay_on_times_ms,
						sm5705_rgb->delay_off_times_ms, 4, 4);
			sm5705_rgb_set_LEDx_current(sm5705_rgb, i, led_current);
			sm5705_rgb_set_LEDx_enable(sm5705_rgb, i,
					sm5705_rgb->delay_off_times_ms ? true : false, true);
		}
	}

	return count;
}

#define CURRENT_RATIO	255
static ssize_t store_sm5705_rgb_pattern(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
	struct br_ratio *br_ratio;
	unsigned char led_current;
	unsigned int mode;
	int ret;

	ret = sscanf(buf, "%1d", &mode);
	if (!ret) {
		dev_err(dev, "%s: fail to get led_pattern mode.\n", __func__);
		return count;
	}

	led_current = (!sm5705_rgb->en_lowpower_mode) ?
		sm5705_rgb->normal_cur : sm5705_rgb->low_cur;

	br_ratio = (!sm5705_rgb->en_lowpower_mode) ?
		&sm5705_rgb->normal_ratio : &sm5705_rgb->low_ratio;

	dev_info(dev, "%s: pattern mode=%d r,g,b ratio=%d,%d,%d(lowpower=%d)\n",
			__func__, mode,	br_ratio->r, br_ratio->g, br_ratio->b,
			sm5705_rgb->en_lowpower_mode);

	ret = sm5705_rgb_reset(sm5705_rgb);
	if (IS_ERR_VALUE(ret)) {
		goto out;
	}

	switch (mode) {
	case CHARGING:
		/* LED_R constant mode ON */
		led_current = (led_current * br_ratio->r) / CURRENT_RATIO;
		sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_R, led_current);
		sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_R, false, true);
		break;
	case CHARGING_ERR:
		/* LED_R slope mode ON (500ms to 500ms) */
		led_current = (led_current * br_ratio->r) / CURRENT_RATIO;
		sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, LED_R, 0xF, 0xF, 0x0);
		sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, LED_R, 0, 500, 500, 4, 4);
		sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_R, led_current);
		sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_R, true, true);
		break;
	case MISSED_NOTI:
		/* LED_B slope mode ON (500ms to 5000ms) */
		led_current = (led_current * br_ratio->b) / CURRENT_RATIO;
		sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, LED_B, 0xF, 0xF, 0x0);
		sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, LED_B, 0, 500, 5000, 4, 4);
		sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_B, led_current);
		sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_B, true, true);
		break;
	case LOW_BATTERY:
		/* LED_R slope mode ON (500ms to 5000ms) */
		led_current = (led_current * br_ratio->r) / CURRENT_RATIO;
		sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, LED_R, 0xF, 0xF, 0x0);
		sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, LED_R, 0, 500, 5000, 4, 4);
		sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_R, led_current);
		sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_R, true, true);
		break;
	case FULLY_CHARGED:
		/* LED_G constant mode ON */
		led_current = (led_current * br_ratio->g) / CURRENT_RATIO;
		sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_G, led_current);
		sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_G, false, true);
		break;
	case POWERING:
		/* LED_G & LED_B slope mode ON (1000ms to 1000ms) */
		sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, LED_G, 0x8, 0x4, 0x1);
		sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, LED_G, 0, 1000, 1000, 12, 12);
		led_current = (led_current * br_ratio->g) / CURRENT_RATIO;
		sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_G, led_current);
		sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, LED_B, 0xF, 0xE, 0xC);
		sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, LED_B, 0, 1000, 1000, 28, 28);
		led_current = (led_current * br_ratio->b) / br_ratio->g;
		sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_B, led_current);
		sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_G, true, true);
		sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_B, true, true);
		break;
	case PATTERN_OFF:
	default:
		break;
	}

out:
	return count;
}

static ssize_t show_sm5705_rgb_regdump(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev);
	u8 reg_val[SM5705_REG_MAX];
	int i, count=0;

	for (i=SM5705_REG_CNTLMODEONOFF; i<=SM5705_REG_BLEDCNTL4; i++) {
		sm5705_read_reg(sm5705_rgb->i2c, i, &reg_val[i]);
		count += scnprintf(buf + count, PAGE_SIZE - count, "reg[0x%x] = 0x%x\n", i,
				reg_val[i]);
	}

	return count;
}

static ssize_t show_sm5705_dummy(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int count;

	count = scnprintf(buf, PAGE_SIZE, "doesn't support read it\n");

	return count;
}

/* SAMSUNG specific attribute nodes */
static DEVICE_ATTR(led_r, S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP, show_sm5705_dummy, store_led_r);
static DEVICE_ATTR(led_g, S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP, show_sm5705_dummy, store_led_g);
static DEVICE_ATTR(led_b, S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP, show_sm5705_dummy, store_led_b);
static DEVICE_ATTR(led_pattern, S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP, show_sm5705_dummy, store_sm5705_rgb_pattern);
static DEVICE_ATTR(led_blink, S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP, show_sm5705_dummy, store_sm5705_rgb_blink);
static DEVICE_ATTR(led_brightness, S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP, show_sm5705_rgb_brightness, store_sm5705_rgb_brightness);
static DEVICE_ATTR(led_lowpower, S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP, show_sm5705_rgb_lowpower,  store_sm5705_rgb_lowpower);
static DEVICE_ATTR(led_regdump, S_IRUSR| S_IRGRP, show_sm5705_rgb_regdump,  NULL);

static struct attribute *sec_led_attributes[] = {
	&dev_attr_led_r.attr,
	&dev_attr_led_g.attr,
	&dev_attr_led_b.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
	&dev_attr_led_brightness.attr,
	&dev_attr_led_lowpower.attr,
	&dev_attr_led_regdump.attr,
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};

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

static void sm5705_rgb_brightness_set(struct led_classdev *led_cdev,
						unsigned int brightness)
{
	const struct device *parent = led_cdev->dev->parent;
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(parent);
	struct device *dev = led_cdev->dev;
	int index, ret;

	if (brightness > LED_MAX_CURRENT)
		sm5705_rgb->normal_cur = LED_MAX_CURRENT;
	else
		sm5705_rgb->normal_cur = brightness;

	index = get_rgb_index_from_led_cdev(sm5705_rgb, led_cdev);
	if (IS_ERR_VALUE(index))
		return;

	ret = sm5705_rgb_set_LEDx_current(sm5705_rgb, index,
			sm5705_rgb->normal_cur);
	if (IS_ERR_VALUE(ret))
		return;
	ret = sm5705_rgb_set_LEDx_enable(sm5705_rgb, index, false,
			!!sm5705_rgb->normal_cur);
	if (IS_ERR_VALUE(ret))
		return;

	dev_info(dev, "%s: led[%d] brightness=0x%x current=0x%x, constant %s\n",
				__func__, index, brightness,
				sm5705_rgb->normal_cur,
				(brightness) ? "ON" : "OFF");
}

static unsigned int sm5705_rgb_brightness_get(struct led_classdev *led_cdev)
{
	const struct device *parent = led_cdev->dev->parent;
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(parent);
	struct device *dev = led_cdev->dev;
	int index, cur_value, ret;

	index = get_rgb_index_from_led_cdev(sm5705_rgb, led_cdev);
	if (IS_ERR_VALUE(index))
		return 0;

	cur_value = sm5705_rgb_get_LEDx_current(sm5705_rgb, index);
	if (IS_ERR_VALUE(ret))
		return 0;

	dev_dbg(dev, "%s: led[%d] get current : 0x%x\n", __func__, index, cur_value);

	return cur_value;
}

static ssize_t led_delay_on_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev->parent);

	return sprintf(buf, "%d\n", sm5705_rgb->delay_on_times_ms);
}

static ssize_t led_delay_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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

static ssize_t led_delay_off_show(struct device *dev,
			struct device_attribute *attr, char *buf)
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

static ssize_t led_blink_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct sm5705_rgb *sm5705_rgb = dev_get_drvdata(dev->parent);
	unsigned long blink_set;
	int index;

	dev_info(dev, ":%s\n", __func__);

	if (kstrtoul(buf, 0, &blink_set))
		goto out;

	index = get_rgb_index_from_led_cdev(sm5705_rgb, led_cdev);
	if (IS_ERR_VALUE(index))
		goto out;

	if (!blink_set) {
		sm5705_rgb->delay_on_times_ms = LED_OFF;
		sm5705_rgb_brightness_set(led_cdev, LED_OFF);
	}

	sm5705_rgb_set_LEDx_slopeduty(sm5705_rgb, index, 0xF, 0xF, 0x0);
	sm5705_rgb_set_LEDx_slopemode(sm5705_rgb, index, 0,
					sm5705_rgb->delay_on_times_ms,
					sm5705_rgb->delay_off_times_ms, 4, 4);
	sm5705_rgb_set_LEDx_current(sm5705_rgb, index,
					sm5705_rgb->normal_cur);
	sm5705_rgb_set_LEDx_enable(sm5705_rgb, index, true, true);

	dev_info(dev, "%s: delay_on_time:%d, delay_off_time:%d, current:0x%x\n",
					__func__, sm5705_rgb->delay_on_times_ms,
					sm5705_rgb->delay_off_times_ms,
					sm5705_rgb->normal_cur);

out:
	return count;
}

/* sysfs attribute nodes */
static DEVICE_ATTR(delay_on, S_IRUSR| S_IWUSR| S_IRGRP, led_delay_on_show, led_delay_on_store);
static DEVICE_ATTR(delay_off, S_IRUSR| S_IWUSR| S_IRGRP, led_delay_off_show, led_delay_off_store);
static DEVICE_ATTR(blink, S_IRUSR| S_IWUSR| S_IRGRP, NULL, led_blink_store);

static struct attribute *led_class_attrs[] = {
	&dev_attr_delay_on.attr,
	&dev_attr_delay_off.attr,
	&dev_attr_blink.attr,
	NULL,
};

static struct attribute_group common_led_attr_group = {
	.attrs = led_class_attrs,
};

#ifdef CONFIG_OF

static int sm5705_rgb_parse_dt(struct sm5705_rgb *sm5705_rgb)
{

	struct device *dev = sm5705_rgb->dev;
	struct device_node *np;
	int ret;
	u32 lcd_id;
	struct property *prop;
	char propname[LED_PROP_LENGTH];
	int octa_index, size;
	u32 params[SIZE_OF_PARAMS];

	np = of_find_node_by_name(NULL, "sm5705_rgb");
	if (unlikely(!np)) {
		dev_err(dev, "fail to find rgb node\n");
		return -EINVAL;
	}

	lcd_id = get_lcd_attached("GET");

#ifdef CONFIG_SEC_ON7XLTE_CHN
	lcd_id = lcd_id & 0x00000F00;
	lcd_id = lcd_id >> 8;
#else
	lcd_id = lcd_id & 0x000F0000;
	lcd_id = lcd_id >> 16;
#endif

	for (octa_index = 0; ; octa_index++) {
		snprintf(propname, sizeof(propname), "octa-%d", octa_index);

		prop = of_find_property(np, propname, &size);
		if (!prop) {
			dev_info(dev, "can't find octa code, set default ratio\n");
			sm5705_rgb->octa_name = DEFAULT_OCTA_NAME;
			sm5705_rgb->normal_cur = DEFAULT_NORMAL_CUR;
			sm5705_rgb->low_cur = DEFAULT_LOW_CUR;
			sm5705_rgb->normal_ratio.r = DEFAULT_NORMAL_RATIO;
			sm5705_rgb->normal_ratio.g = DEFAULT_NORMAL_RATIO;
			sm5705_rgb->normal_ratio.b = DEFAULT_NORMAL_RATIO;
			sm5705_rgb->low_ratio.r = DEFAULT_LOW_RATIO;
			sm5705_rgb->low_ratio.g = DEFAULT_LOW_RATIO;
			sm5705_rgb->low_ratio.b = DEFAULT_LOW_RATIO;
			break;
		}

		of_property_read_u32_array(np, propname, params,
				SIZE_OF_PARAMS);

		if (lcd_id == params[OCTA_CODE]) {
			of_property_read_string_index(np, "octa-names", octa_index,
					&sm5705_rgb->octa_name);
			sm5705_rgb->normal_cur = params[NORMAL_CUR];
			sm5705_rgb->low_cur = params[LOW_CUR];
			sm5705_rgb->normal_ratio.r = params[NORMAL_R_RATIO];
			sm5705_rgb->normal_ratio.g = params[NORMAL_G_RATIO];
			sm5705_rgb->normal_ratio.b = params[NORMAL_B_RATIO];
			sm5705_rgb->low_ratio.r = params[LOW_R_RATIO];
			sm5705_rgb->low_ratio.g = params[LOW_G_RATIO];
			sm5705_rgb->low_ratio.b = params[LOW_B_RATIO];
			break;
		}
	}

	sm5705_rgb->gpio_vdd = of_get_named_gpio(np, "rgb,vdd-gpio", 0);
	if (!gpio_is_valid(sm5705_rgb->gpio_vdd))
		dev_info(dev, "don't support gpio to lift up power supply!\n");
	else {
		dev_info(dev, "support vdd-gpio: %d\n", sm5705_rgb->gpio_vdd);
		ret = gpio_request(sm5705_rgb->gpio_vdd, "rgb_vdd");
		gpio_direction_output(sm5705_rgb->gpio_vdd, 0);
		if (ret < 0) {
			dev_err(dev, "failed to request gpio_vdd\n");
			return -EINVAL;
		}
	}

	return 0;
}
#endif

static inline int _register_led_commonclass_dev(struct device *dev,
					struct sm5705_rgb *sm5705_rgb)
{
	int i, ret;

	for (i = 0; i < LED_MAX; ++i) {
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
	} while (--i);

	return ret;
}

static inline void _unregister_led_commonclass_dev(struct sm5705_rgb *sm5705_rgb)
{
	int i;

	for (i = 0; i < LED_MAX; ++i)
		led_classdev_unregister(&sm5705_rgb->led[i]);
}

static int sm5705_rgb_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sm5705_rgb *sm5705_rgb;
	struct sm5705_dev *sm5705_dev = dev_get_drvdata(dev->parent);
	char leds_name[LED_MAX][LED_MAX_STR] = {"led_r", "led_g", "led_b"};
	int ret;

	sm5705_rgb = devm_kzalloc(dev, sizeof(struct sm5705_rgb), GFP_KERNEL);
	if (unlikely(!sm5705_rgb)) {
		dev_err(dev, "fail to allocate memory for sm5705_rgb\n");
		return -ENOMEM;
	}

	sm5705_rgb->dev = dev;
	sm5705_rgb->i2c = sm5705_dev->i2c;
	platform_set_drvdata(pdev, sm5705_rgb);

#ifdef CONFIG_OF
	ret = sm5705_rgb_parse_dt(sm5705_rgb);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "fail to parse dt for sm5705 rgb-led (ret=%d)\n", ret);
		goto rgb_dt_parse_err;
	}
#endif

	for (ret = 0 ; ret < LED_MAX ; ret++)
		sm5705_rgb->led[ret].name = leds_name[ret];

	ret = _register_led_commonclass_dev(dev, sm5705_rgb);
	if (IS_ERR_VALUE(ret))
		goto rgb_dt_parse_err;

	sm5705_rgb->led_dev = sec_device_create(0, sm5705_rgb, "led");
	if (unlikely(!sm5705_rgb->led_dev)) {
		dev_err(dev, "fail to create device for samsung specific led\n");
		goto sec_device_create_err;
	}

	ret = sysfs_create_group(&sm5705_rgb->led_dev->kobj, &sec_led_attr_group);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "fail to create sysfs group for samsung specific led\n");
		goto sec_sysfs_create_group_err;
	}

#if defined(CONFIG_LEDS_USE_ED28) && defined(CONFIG_SEC_FACTORY)
	if( lcdtype == 0 && jig_status == false) {
		/* LED_R constant mode ON */
		sm5705_rgb_set_LEDx_current(sm5705_rgb, LED_R,
				sm5705_rgb->normal_cur);
		sm5705_rgb_set_LEDx_enable(sm5705_rgb, LED_R, false, true);
	}
#endif

	dev_info(dev, "RGB-LED device driver probe done\n");
	dev_info(dev, "octa color is %s\n", sm5705_rgb->octa_name);
	dev_info(dev, "normal mode currnet = %d, ratio R,G,B = (%d,%d,%d)\n",
			sm5705_rgb->normal_cur,
			sm5705_rgb->normal_ratio.r,
			sm5705_rgb->normal_ratio.g,
			sm5705_rgb->normal_ratio.b);
	dev_info(dev, "low-power mode current = %d, ratio R,G,B = (%d,%d,%d)\n",
			sm5705_rgb->low_cur,
			sm5705_rgb->low_ratio.r,
			sm5705_rgb->low_ratio.g,
			sm5705_rgb->low_ratio.b);

	/*Set current range 0 ~ 12.75mA, step 0.05mA */
	ret = sm5705_write_reg(sm5705_rgb->i2c,SM5705_REG_CNTLPWM, 0x00);
	if (IS_ERR_VALUE(ret)) {
		dev_err(sm5705_rgb->dev, "%s: write reg fail\n", __func__);
		return ret;
	}

	return 0;

sec_sysfs_create_group_err:
	sec_device_destroy(sm5705_rgb->led_dev->devt);

sec_device_create_err:
	_unregister_led_commonclass_dev(sm5705_rgb);

rgb_dt_parse_err:
	devm_kfree(dev, sm5705_rgb);

	return -1;
}

static int sm5705_rgb_remove(struct platform_device *pdev)
{
	struct sm5705_rgb *sm5705_rgb = platform_get_drvdata(pdev);

	dev_info(sm5705_rgb->dev, "%s\n", __func__);
	sysfs_remove_group(&sm5705_rgb->led_dev->kobj, &sec_led_attr_group);
	_unregister_led_commonclass_dev(sm5705_rgb);
	devm_kfree(sm5705_rgb->dev, sm5705_rgb);

	return 0;
}

static void sm5705_rgb_shutdown(struct platform_device *pdev)
{
	struct sm5705_rgb *sm5705_rgb = platform_get_drvdata(pdev);

	if (!sm5705_rgb->i2c)
		return;

	dev_info(sm5705_rgb->dev, "%s\n", __func__);
	sm5705_rgb_reset(sm5705_rgb);
}

static struct platform_driver sm5705_rgbled_driver = {
	.driver		= {
		.name	= "leds-sm5705-rgb",
		.owner	= THIS_MODULE,
	},
	.probe		= sm5705_rgb_probe,
	.remove		= sm5705_rgb_remove,
	.shutdown	= sm5705_rgb_shutdown,
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
