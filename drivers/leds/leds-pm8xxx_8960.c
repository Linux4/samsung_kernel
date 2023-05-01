/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pwm.h>
#include <linux/leds-pm8xxx.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/export.h>

#define SSBI_REG_ADDR_DRV_KEYPAD	0x48
#define PM8XXX_DRV_KEYPAD_BL_MASK	0xf0
#define PM8XXX_DRV_KEYPAD_BL_SHIFT	0x04

#define SSBI_REG_ADDR_FLASH_DRV0        0x49
#define PM8XXX_DRV_FLASH_MASK           0xf0
#define PM8XXX_DRV_FLASH_SHIFT          0x04

#define SSBI_REG_ADDR_FLASH_DRV1        0xFB

#define SSBI_REG_ADDR_LED_CTRL_BASE	0x131
#define SSBI_REG_ADDR_LED_CTRL(n)	(SSBI_REG_ADDR_LED_CTRL_BASE + (n))
#define PM8XXX_DRV_LED_CTRL_MASK	0xf8
#define PM8XXX_DRV_LED_CTRL_SHIFT	0x03

#define MAX_FLASH_LED_CURRENT		300
#define MAX_LC_LED_CURRENT		40
#define MAX_KP_BL_LED_CURRENT		300

#define PM8XXX_ID_LED_CURRENT_FACTOR	2  /* Iout = x * 2mA */
#define PM8XXX_ID_FLASH_CURRENT_FACTOR	20 /* Iout = x * 20mA */

#define PM8XXX_FLASH_MODE_DBUS1		1
#define PM8XXX_FLASH_MODE_DBUS2		2
#define PM8XXX_FLASH_MODE_PWM		3

#define MAX_LC_LED_BRIGHTNESS		20
#define MAX_FLASH_BRIGHTNESS		15
#define MAX_KB_LED_BRIGHTNESS		15

#define PM8XXX_LED_OFFSET(id) ((id) - PM8XXX_ID_LED_0)

#define PM8XXX_LED_PWM_FLAGS	(PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP |\
		PM_PWM_LUT_REVERSE | PM_PWM_LUT_PAUSE_HI_EN | \
		PM_PWM_LUT_PAUSE_LO_EN)

/*  low_powermode is for led blinking level */
int low_powermode;
#define LOW_POWERMODE_DIVIDER	9

/**
 * struct pm8xxx_led_data - internal led data structure
 * @led_classdev - led class device
 * @id - led index
 * @work - workqueue for led
 * @lock - to protect the transactions
 * @reg - cached value of led register
 * @pwm_dev - pointer to PWM device if LED is driven using PWM
 * @pwm_channel - PWM channel ID
 * @pwm_period_us - PWM period in micro seconds
 * @pwm_duty_cycles - struct that describes PWM duty cycles info
 */
struct pm8xxx_led_data {
	struct led_classdev	cdev;
	int			id;
	u8			reg;
	struct device		*dev;
	struct work_struct	work;
	struct mutex		lock;
	struct pwm_device	*pwm_dev;
	int			pwm_channel;
	u32			pwm_period_us;
	struct pm8xxx_pwm_duty_cycles *pwm_duty_cycles;
};


static void led_kp_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc;
	u8 level;

	level = (value << PM8XXX_DRV_KEYPAD_BL_SHIFT) &
				 PM8XXX_DRV_KEYPAD_BL_MASK;

	led->reg &= ~PM8XXX_DRV_KEYPAD_BL_MASK;
	led->reg |= level;

	rc = pm8xxx_writeb(led->dev->parent, SSBI_REG_ADDR_DRV_KEYPAD,
								led->reg);
	if (rc < 0)
		dev_err(led->cdev.dev,
			"can't set keypad backlight level rc=%d\n", rc);
}

static void led_lc_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc, offset;
	u8 level;
	level = (value << PM8XXX_DRV_LED_CTRL_SHIFT) &
				PM8XXX_DRV_LED_CTRL_MASK;

	offset = PM8XXX_LED_OFFSET(led->id);

	led->reg &= ~PM8XXX_DRV_LED_CTRL_MASK;
	led->reg |= level;

	rc = pm8xxx_writeb(led->dev->parent, SSBI_REG_ADDR_LED_CTRL(offset),
								led->reg);
	if (rc)
		dev_err(led->cdev.dev, "can't set (%d) led value rc=%d\n",
				led->id, rc);
}

static void
led_flash_set(struct pm8xxx_led_data *led, enum led_brightness value)
{
	int rc;
	u8 level;
	u16 reg_addr;

	level = (value << PM8XXX_DRV_FLASH_SHIFT) &
				 PM8XXX_DRV_FLASH_MASK;

	led->reg &= ~PM8XXX_DRV_FLASH_MASK;
	led->reg |= level;

	if (led->id == PM8XXX_ID_FLASH_LED_0)
		reg_addr = SSBI_REG_ADDR_FLASH_DRV0;
	else
		reg_addr = SSBI_REG_ADDR_FLASH_DRV1;

	rc = pm8xxx_writeb(led->dev->parent, reg_addr, led->reg);
	if (rc < 0)
		dev_err(led->cdev.dev, "can't set flash led%d level rc=%d\n",
			 led->id, rc);
}

static int pm8xxx_led_pwm_work(struct pm8xxx_led_data *led)
{
	int duty_us;
	int rc = 0;

	if (led->pwm_duty_cycles == NULL) {
		duty_us = (led->pwm_period_us * led->cdev.brightness) /
								LED_FULL;
		rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
		if (led->cdev.brightness)
			rc = pwm_enable(led->pwm_dev);
		else
			pwm_disable(led->pwm_dev);
	} else {
		rc = pm8xxx_pwm_lut_enable(led->pwm_dev, led->cdev.brightness);
	}

	return rc;
}

static void __pm8xxx_led_work(struct pm8xxx_led_data *led,
					enum led_brightness level)
{
	mutex_lock(&led->lock);

	switch (led->id) {
	case PM8XXX_ID_LED_KB_LIGHT:
		led_kp_set(led, level);
	break;
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		level = level / PM8XXX_ID_LED_CURRENT_FACTOR;
		led_lc_set(led, level);
	break;
	case PM8XXX_ID_FLASH_LED_0:
	case PM8XXX_ID_FLASH_LED_1:
		led_flash_set(led, level);
	break;
	}

	mutex_unlock(&led->lock);
}

static void pm8xxx_led_work(struct work_struct *work)
{
	int rc;

	struct pm8xxx_led_data *led = container_of(work,
					 struct pm8xxx_led_data, work);

	if (led->pwm_dev == NULL) {
		__pm8xxx_led_work(led, led->cdev.brightness);
	} else {
		rc = pm8xxx_led_pwm_work(led);
		if (rc)
			pr_err("could not configure PWM mode for LED:%d\n",
								led->id);
	}
}

static void pm8xxx_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct	pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	if (value < LED_OFF || value > led->cdev.max_brightness) {
		dev_err(led->cdev.dev, "Invalid brightness value exceeds\n");
		return;
	}
	led->cdev.brightness = value;
	schedule_work(&led->work);
}

static int pm8xxx_set_led_mode_and_max_brightness(struct pm8xxx_led_data *led,
		enum pm8xxx_led_modes led_mode, int max_current)
{
	int rc = 0;

	switch (led->id) {
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		led->cdev.max_brightness = LED_FULL;
		led->reg = led_mode;
		break;
	case PM8XXX_ID_LED_KB_LIGHT:
	case PM8XXX_ID_FLASH_LED_0:
	case PM8XXX_ID_FLASH_LED_1:
		led->cdev.max_brightness = max_current /
						PM8XXX_ID_FLASH_CURRENT_FACTOR;
		if (led->cdev.max_brightness > MAX_FLASH_BRIGHTNESS)
			led->cdev.max_brightness = MAX_FLASH_BRIGHTNESS;

		switch (led_mode) {
		case PM8XXX_LED_MODE_PWM1:
		case PM8XXX_LED_MODE_PWM2:
		case PM8XXX_LED_MODE_PWM3:
			led->reg = PM8XXX_FLASH_MODE_PWM;
			break;
		case PM8XXX_LED_MODE_DTEST1:
			led->reg = PM8XXX_FLASH_MODE_DBUS1;
			break;
		case PM8XXX_LED_MODE_DTEST2:
			led->reg = PM8XXX_FLASH_MODE_DBUS2;
			break;
		default:
			led->reg = PM8XXX_LED_MODE_MANUAL;
			break;
		}
		break;
	default:
		rc = -EINVAL;
		pr_err("LED Id is invalid");
		break;
	}

	return rc;
}

static enum led_brightness pm8xxx_led_get(struct led_classdev *led_cdev)
{
	struct pm8xxx_led_data *led;

	led = container_of(led_cdev, struct pm8xxx_led_data, cdev);

	return led->cdev.brightness;
}

static int __devinit get_init_value(struct pm8xxx_led_data *led, u8 *val)
{
	int rc = -1 , offset = 0;
	u16 addr = 0;

	switch (led->id) {
	case PM8XXX_ID_LED_KB_LIGHT:
		addr = SSBI_REG_ADDR_DRV_KEYPAD;
		break;
	case PM8XXX_ID_LED_0:
	case PM8XXX_ID_LED_1:
	case PM8XXX_ID_LED_2:
		offset = PM8XXX_LED_OFFSET(led->id);
		addr = SSBI_REG_ADDR_LED_CTRL(offset);
		break;
	case PM8XXX_ID_FLASH_LED_0:
		addr = SSBI_REG_ADDR_FLASH_DRV0;
		break;
	case PM8XXX_ID_FLASH_LED_1:
		addr = SSBI_REG_ADDR_FLASH_DRV1;
		break;
	default:
		return rc;
	}

	rc = pm8xxx_readb(led->dev->parent, addr, val);
	if (rc)
		dev_err(led->cdev.dev, "can't get led(%d) level rc=%d\n",
							led->id, rc);

	return rc;
}

static int pm8xxx_led_pwm_configure(struct pm8xxx_led_data *led,
		int lo_pause, int hi_pause)
{
	int start_idx, idx_len, duty_us, rc;

	led->pwm_dev = pwm_request(led->pwm_channel,
					led->cdev.name);

	if (IS_ERR_OR_NULL(led->pwm_dev)) {
		pr_err("could not acquire PWM Channel %d, "
			"error %ld\n", led->pwm_channel,
			PTR_ERR(led->pwm_dev));
		led->pwm_dev = NULL;
		return -ENODEV;
	}

	if (led->pwm_duty_cycles != NULL) {
		start_idx = led->pwm_duty_cycles->start_idx;
		idx_len = led->pwm_duty_cycles->num_duty_pcts;

		if (idx_len >= PM_PWM_LUT_SIZE && start_idx) {
			pr_err("Wrong LUT size or index\n");
			return -EINVAL;
		}
		if ((start_idx + idx_len) > PM_PWM_LUT_SIZE) {
			pr_err("Exceed LUT limit\n");
			return -EINVAL;
		}

		rc = pm8xxx_pwm_lut_config(led->pwm_dev, led->pwm_period_us,
				led->pwm_duty_cycles->duty_pcts,
				led->pwm_duty_cycles->duty_ms,
				start_idx, idx_len, lo_pause, hi_pause,
				PM8XXX_LED_PWM_FLAGS);
	} else {
		duty_us = led->pwm_period_us;
		rc = pwm_config(led->pwm_dev, duty_us, led->pwm_period_us);
	}

	return rc;
}

#ifndef CONFIG_LEDS_PM8XXX
#define CONFIG_LEDS_PM8XXX
#endif
struct leds_dev_data {
	struct pm8xxx_led_data *led;
	struct pm8xxx_led_platform_data *pdata ;
	atomic_t op_flag;
	struct mutex led_work_lock;
	struct work_struct work_pat_batt_chrg;
	struct work_struct work_pat_chrg_err;
	struct work_struct work_pat_miss_noti;
	struct work_struct work_pat_in_lowbat;
	struct work_struct work_pat_full_chrg;
	struct work_struct work_pat_powering;
};
#ifdef CONFIG_LEDS_PM8XXX

static void pm8xxx_led_work_pat_led_off(struct leds_dev_data *info)
{
	int loop_cnt;

	mutex_lock(&info->led_work_lock);
	if (info->pdata->led_power_on)
		info->pdata->led_power_on(0);
	for (loop_cnt = 0 ; loop_cnt < ((info->pdata->led_core->num_leds) - 1) ;
		      loop_cnt++) {
		__pm8xxx_led_work(&info->led[loop_cnt], 0);
		if (info->led[loop_cnt].pwm_dev != NULL)
			pwm_free(info->led[loop_cnt].pwm_dev);
	}

	mutex_unlock(&info->led_work_lock);

}
static void pm8xxx_led_work_pat_powering(struct work_struct *work)
{
	struct leds_dev_data *info =
		container_of(work, struct leds_dev_data, work_pat_powering);
	struct pm8xxx_led_config *led_cfg;
	int loop_cnt;

	if (!atomic_read(&info->op_flag)) {
		pr_info("LED turns off before turns on, op:%d\n",
						atomic_read(&info->op_flag));
		return;
	}

	pm8xxx_led_work_pat_led_off(info);
	mutex_lock(&info->led_work_lock);

	for (loop_cnt = PM8XXX_LED_PAT6_GREEN ;
			loop_cnt <= PM8XXX_LED_PAT6_BLUE ;
			loop_cnt++) {
		led_cfg = &info->pdata->configs[loop_cnt];

		pm8xxx_set_led_mode_and_max_brightness(&info->led[loop_cnt],
				led_cfg->mode, led_cfg->max_current);

		__pm8xxx_led_work(&info->led[loop_cnt], led_cfg->max_current);

		if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
			pm8xxx_led_pwm_configure(&info->led[loop_cnt],
					200, 200);
	}
	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT6_GREEN].cdev,
			led_cfg->max_current);
	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT6_BLUE].cdev,
			led_cfg->max_current);

	mutex_unlock(&info->led_work_lock);
}
static void pm8xxx_led_work_pat_full_chrg(struct work_struct *work)
{
	struct leds_dev_data *info =
		container_of(work, struct leds_dev_data, work_pat_full_chrg);
	struct pm8xxx_led_config *led_cfg;

	if (!atomic_read(&info->op_flag)) {
		pr_info("LED turns off before turns on, op:%d\n",
						atomic_read(&info->op_flag));
		return;
	}

	pm8xxx_led_work_pat_led_off(info);
	mutex_lock(&info->led_work_lock);

		led_cfg = &info->pdata->configs[PM8XXX_LED_PAT5_GREEN];

	pm8xxx_set_led_mode_and_max_brightness(
	&info->led[PM8XXX_LED_PAT5_GREEN],
	led_cfg->mode, led_cfg->max_current);

	__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT5_GREEN],
	led_cfg->max_current);

	if (low_powermode) {
		led_cfg->pwm_duty_cycles->duty_pcts[0] 
				= 100/LOW_POWERMODE_DIVIDER;
		led_cfg->pwm_duty_cycles->duty_pcts[1] 
				= 100/LOW_POWERMODE_DIVIDER;
	} else {
		led_cfg->pwm_duty_cycles->duty_pcts[0] = 100;
		led_cfg->pwm_duty_cycles->duty_pcts[1] = 100;
	}

	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT5_GREEN],
		0, 0);

	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT5_GREEN].cdev,
			led_cfg->max_current);

	mutex_unlock(&info->led_work_lock);

}
static void pm8xxx_led_work_pat_in_lowbat(struct work_struct *work)
{
	struct leds_dev_data *info =
		container_of(work, struct leds_dev_data, work_pat_in_lowbat);
	struct pm8xxx_led_config *led_cfg;

	if (!atomic_read(&info->op_flag)) {
		pr_info("LED turns off before turns on, op:%d\n",
						atomic_read(&info->op_flag));
		return;
	}

	pm8xxx_led_work_pat_led_off(info);

	mutex_lock(&info->led_work_lock);

	led_cfg = &info->pdata->configs[PM8XXX_LED_PAT4_RED];
	pm8xxx_set_led_mode_and_max_brightness(&info->led[PM8XXX_LED_PAT4_RED],
	led_cfg->mode, led_cfg->max_current);
	__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT4_RED],
	led_cfg->max_current);

	if (low_powermode) {
		led_cfg->pwm_duty_cycles->duty_pcts[1] 
				= 100/LOW_POWERMODE_DIVIDER;
	} else {
		led_cfg->pwm_duty_cycles->duty_pcts[1] = 100;
	}

	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT4_RED],
					5000, 500);
	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT4_RED].cdev,
			led_cfg->max_current);
	mutex_unlock(&info->led_work_lock);

}
static void pm8xxx_led_work_pat_miss_noti(struct work_struct *work)
{
	struct leds_dev_data *info =
		container_of(work, struct leds_dev_data, work_pat_miss_noti);
	struct pm8xxx_led_config *led_cfg;

	if (!atomic_read(&info->op_flag)) {
		pr_info("LED turns off before turns on, op:%d\n",
						atomic_read(&info->op_flag));
		return;
	}

	pm8xxx_led_work_pat_led_off(info);
	mutex_lock(&info->led_work_lock);

	led_cfg = &info->pdata->configs[PM8XXX_LED_PAT3_BLUE];

	pm8xxx_set_led_mode_and_max_brightness(&info->led[PM8XXX_LED_PAT3_BLUE],
				led_cfg->mode, led_cfg->max_current);

	__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT3_BLUE],
	led_cfg->max_current);

	if (low_powermode) {
		led_cfg->pwm_duty_cycles->duty_pcts[1] 
				= 100/LOW_POWERMODE_DIVIDER;
	} else {
		led_cfg->pwm_duty_cycles->duty_pcts[1] = 100;
	}

	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT3_BLUE],
					5000, 500);

		pm8xxx_led_set(&info->led[PM8XXX_LED_PAT3_BLUE].cdev,
			led_cfg->max_current);

	mutex_unlock(&info->led_work_lock);
}

static void pm8xxx_led_work_pat_chrg_err(struct work_struct *work)
{
	struct leds_dev_data *info =
		container_of(work, struct leds_dev_data, work_pat_chrg_err);
	struct pm8xxx_led_config *led_cfg;

	if (!atomic_read(&info->op_flag)) {
		pr_info("LED turns off before turns on, op:%d\n",
						atomic_read(&info->op_flag));
		return;
	}

	pm8xxx_led_work_pat_led_off(info);
	mutex_lock(&info->led_work_lock);

	led_cfg = &info->pdata->configs[PM8XXX_LED_PAT2_RED];

	pm8xxx_set_led_mode_and_max_brightness(&info->led[PM8XXX_LED_PAT2_RED],
				led_cfg->mode, led_cfg->max_current);

		__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT2_RED],
				led_cfg->max_current);

		if (low_powermode) {
		led_cfg->pwm_duty_cycles->duty_pcts[1] 
				= 100/LOW_POWERMODE_DIVIDER;
	} else {
		led_cfg->pwm_duty_cycles->duty_pcts[1] = 100;
	}

	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT2_RED],
					500, 500);

	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT2_RED].cdev,
			led_cfg->max_current);

	mutex_unlock(&info->led_work_lock);
}


static void pm8xxx_led_work_pat_batt_chrg(struct work_struct *work)
{
	struct leds_dev_data *info =
		container_of(work, struct leds_dev_data, work_pat_batt_chrg);
	struct pm8xxx_led_config *led_cfg;

	if (!atomic_read(&info->op_flag)) {
		pr_info("LED turns off before turns on, op:%d\n",
						atomic_read(&info->op_flag));
		return;
	}
	pm8xxx_led_work_pat_led_off(info);
	mutex_lock(&info->led_work_lock);
		led_cfg = &info->pdata->configs[PM8XXX_LED_PAT1_RED];

	pm8xxx_set_led_mode_and_max_brightness(&info->led[PM8XXX_LED_PAT1_RED],
				led_cfg->mode, led_cfg->max_current);

		__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT1_RED],
				led_cfg->max_current);

		if (low_powermode) {
		led_cfg->pwm_duty_cycles->duty_pcts[0] 
				= 100/LOW_POWERMODE_DIVIDER;
		led_cfg->pwm_duty_cycles->duty_pcts[1] 
				= 100/LOW_POWERMODE_DIVIDER;
	} else {
		led_cfg->pwm_duty_cycles->duty_pcts[0] = 100;
		led_cfg->pwm_duty_cycles->duty_pcts[1] = 100;
	}

	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT1_RED],
					0, 0);
	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT1_RED].cdev,
			led_cfg->max_current);
	mutex_unlock(&info->led_work_lock);

}

static ssize_t led_pattern_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct leds_dev_data *info = dev_get_drvdata(dev);
	return snprintf(buf, 4, "%u\n", atomic_read(&info->op_flag));
}

static ssize_t led_pattern_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct leds_dev_data *info = dev_get_drvdata(dev);

	if (buf[0] == '1') {
		if ((info->pdata->led_power_on))
			info->pdata->led_power_on(1);
		atomic_set(&info->op_flag , 1);
		pr_info("LED Battery Charging Pattern on\n");
		schedule_work(&info->work_pat_batt_chrg);

	} else if (buf[0] == '2') {
		if ((info->pdata->led_power_on))
			info->pdata->led_power_on(1);
		atomic_set(&info->op_flag , 2);
		pr_info("LED Battery Charging Error Pattern on\n");
		schedule_work(&info->work_pat_chrg_err);

	} else if (buf[0] == '3') {
		if ((info->pdata->led_power_on))
			info->pdata->led_power_on(1);
		atomic_set(&info->op_flag , 3);
		pr_info("LED Missed Call Notifications pattern on\n");
		schedule_work(&info->work_pat_miss_noti);

	} else if (buf[0] == '4') {
		if ((info->pdata->led_power_on))
			info->pdata->led_power_on(1);
		atomic_set(&info->op_flag , 4);
		pr_info("LED Low Battery Pattern on\n");
		schedule_work(&info->work_pat_in_lowbat);

	} else if (buf[0] == '5') {
		if ((info->pdata->led_power_on))
			info->pdata->led_power_on(1);

		atomic_set(&info->op_flag , 5);
		pr_info("LED Full Battery Charging Pattern on\n");
		schedule_work(&info->work_pat_full_chrg);

	} else if (buf[0] == '6') {
		if ((info->pdata->led_power_on))
			info->pdata->led_power_on(1);
		atomic_set(&info->op_flag , 6);
		pr_info("LED Powering on pattern\n");
		schedule_work(&info->work_pat_powering);

	} else if (buf[0] == '7') {
		if ((info->pdata->led_power_on))
			info->pdata->led_power_on(1);
		atomic_set(&info->op_flag , 7);
		schedule_work(&info->work_pat_powering);

	} else if (buf[0] == '0') {
		atomic_set(&info->op_flag , 0);
		pr_info("LED turned off\n");
		pm8xxx_led_work_pat_led_off(info);
	}
	return size;

}

static DEVICE_ATTR(led_pattern, S_IRUGO | S_IWUSR | S_IWGRP,
			led_pattern_show, led_pattern_store);

static ssize_t led_lowpower_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	//struct leds_dev_data *info = dev_get_drvdata(dev);
	return snprintf(buf, 4, "%d\n", low_powermode);
}

static ssize_t led_lowpower_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	//struct leds_dev_data *info = dev_get_drvdata(dev);

	if (buf[0] == '1')
		low_powermode = 1;
	else
		low_powermode = 0;
	return size;
}

static DEVICE_ATTR(led_lowpower, S_IRUGO | S_IWUSR | S_IWGRP,
			led_lowpower_show, led_lowpower_store);


static ssize_t led_r_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int brightness;
	struct leds_dev_data *info = dev_get_drvdata(dev);
	brightness = pm8xxx_led_get(&info->led[PM8XXX_LED_PAT7_RED].cdev);
	return snprintf(buf, 4, "%d\n", brightness);
}

static ssize_t led_r_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct leds_dev_data *info = dev_get_drvdata(dev);
	struct pm8xxx_led_config *led_cfg;
	int brightness = 0;
	unsigned int num_digits = size;
	unsigned int loop_cnt = 0;
	int temp;

	printk(KERN_DEBUG "led_r brightness =%s, numdigs=%u\n",
			buf, num_digits);
	while ((buf[loop_cnt] >= '0') && (buf[loop_cnt] <= '9') &&
			(loop_cnt < num_digits)) {
		brightness = brightness*10 + (buf[loop_cnt] - '0');
		loop_cnt++;
	}

	printk(KERN_DEBUG "led_r brightness =%u\n", brightness);

	if (brightness < 0 || brightness > 255) {
		printk(KERN_WARNING "led_r brightness is out of range");
		return -1;
	}
	temp = pm8xxx_led_get(&info->led[PM8XXX_LED_PAT7_RED].cdev);

	if (brightness == 0 && temp == 0)
		return size;
	if (atomic_read(&info->op_flag))
		atomic_set(&info->op_flag, 0);

	pm8xxx_led_work_pat_led_off(info);

	mutex_lock(&info->led_work_lock);

	led_cfg = &info->pdata->configs[PM8XXX_LED_PAT7_RED];

	pm8xxx_set_led_mode_and_max_brightness(&info->led[PM8XXX_LED_PAT7_RED],
				led_cfg->mode, led_cfg->max_current);
	__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT7_RED],
			led_cfg->max_current);
	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT7_RED], 0, 0);

	if (brightness && (info->pdata->led_power_on))
		info->pdata->led_power_on(1);
	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT7_RED].cdev, brightness);
	mutex_unlock(&info->led_work_lock);

	return size;

}

static DEVICE_ATTR(led_r, S_IRUGO | S_IWUSR | S_IWGRP,
			led_r_show, led_r_store);

static ssize_t led_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int brightness;
	struct leds_dev_data *info = dev_get_drvdata(dev);
	brightness = pm8xxx_led_get(&info->led[PM8XXX_LED_PAT7_GREEN].cdev);
	return snprintf(buf, 4, "%d\n", brightness);
}

static ssize_t led_g_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct leds_dev_data *info = dev_get_drvdata(dev);
	struct pm8xxx_led_config *led_cfg;
	int brightness = 0;
	unsigned int num_digits = size;
	unsigned int loop_cnt = 0;
	int temp;
	printk(KERN_DEBUG "led_g brightness =%s, numdigs=%u\n",
			buf, num_digits);
	while ((buf[loop_cnt] >= '0') && (buf[loop_cnt] <= '9') &&
			(loop_cnt < num_digits)) {
		brightness = brightness*10 + (buf[loop_cnt] - '0');
		loop_cnt++;
	}
	printk(KERN_DEBUG "led_g brightness =%u\n", brightness);

	if (brightness < 0 || brightness > 255) {
		printk(KERN_WARNING "led_g brightness is out of range");
		return -1;
	}
	temp = pm8xxx_led_get(&info->led[PM8XXX_LED_PAT7_GREEN].cdev);
	if (brightness == 0 && temp == 0)
		return size;
	if (atomic_read(&info->op_flag))
		atomic_set(&info->op_flag, 0);

	pm8xxx_led_work_pat_led_off(info);
	mutex_lock(&info->led_work_lock);

	led_cfg = &info->pdata->configs[PM8XXX_LED_PAT7_GREEN];

	pm8xxx_set_led_mode_and_max_brightness(
			&info->led[PM8XXX_LED_PAT7_GREEN],
			led_cfg->mode, led_cfg->max_current);
	__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT7_GREEN],
			led_cfg->max_current);
	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT7_GREEN],
				0, 0);
	if (brightness && (info->pdata->led_power_on))
		info->pdata->led_power_on(1);
	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT7_GREEN].cdev, brightness);
	mutex_unlock(&info->led_work_lock);
	return size;

}

static DEVICE_ATTR(led_g, S_IRUGO | S_IWUSR | S_IWGRP,
			led_g_show, led_g_store);

static ssize_t led_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int brightness;
	struct leds_dev_data *info = dev_get_drvdata(dev);
	brightness = pm8xxx_led_get(&info->led[PM8XXX_LED_PAT7_BLUE].cdev);
	return snprintf(buf, 4, "%d\n", brightness);
}

static ssize_t led_b_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct leds_dev_data *info = dev_get_drvdata(dev);
	struct pm8xxx_led_config *led_cfg;
	int brightness = 0;
	unsigned int num_digits = size;
	unsigned int loop_cnt = 0;
	int temp;
	printk(KERN_DEBUG "led_b brightness =%s, numdigs=%u\n",
			buf, num_digits);
	while ((buf[loop_cnt] >= '0') && (buf[loop_cnt] <= '9') &&
			(loop_cnt < num_digits)) {
		brightness = brightness*10 + (buf[loop_cnt] - '0');
		loop_cnt++;
	}
	printk(KERN_DEBUG "led_b brightness =%u\n", brightness);

	if (brightness < 0 || brightness > 255) {
		printk(KERN_WARNING "led_b brightness is out of range");
		return -1;
	}
	temp = pm8xxx_led_get(&info->led[PM8XXX_LED_PAT7_BLUE].cdev);
	if (brightness == 0 && temp == 0)
		return size;
	if (atomic_read(&info->op_flag))
		atomic_set(&info->op_flag, 0);

	pm8xxx_led_work_pat_led_off(info);
	mutex_lock(&info->led_work_lock);

	led_cfg = &info->pdata->configs[PM8XXX_LED_PAT7_BLUE];

	pm8xxx_set_led_mode_and_max_brightness(&info->led[PM8XXX_LED_PAT7_BLUE],
				led_cfg->mode, led_cfg->max_current);
	__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT7_BLUE],
			led_cfg->max_current);
	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT7_BLUE],
				0, 0);
	if (brightness && (info->pdata->led_power_on))
		info->pdata->led_power_on(1);
	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT7_BLUE].cdev, brightness);
	mutex_unlock(&info->led_work_lock);

	return size;

}

static DEVICE_ATTR(led_b, S_IRUGO | S_IWUSR | S_IWGRP,
			led_b_show, led_b_store);

static ssize_t led_blink_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int brightness;
	int size = 0;
	struct leds_dev_data *info = dev_get_drvdata(dev);
	brightness = pm8xxx_led_get(&info->led[PM8XXX_LED_PAT8_RED].cdev);
	size += snprintf(buf, 24, "%x ", brightness);
	brightness = pm8xxx_led_get(&info->led[PM8XXX_LED_PAT8_GREEN].cdev);
	size += snprintf(buf+size, 24, "%x ", brightness);
	brightness = pm8xxx_led_get(&info->led[PM8XXX_LED_PAT8_BLUE].cdev);
	size += snprintf(buf+size, 24, "%x ", brightness);
	buf[size] = 0;
	return size;
}

static unsigned int hex_to_dec(char c1, char c2)
{
	unsigned int ret_val = 0;
	if (c1 >= '0' && c1 <= '9')
		ret_val = (c1 - '0');
	else if (c1 >= 'A' && c1 <= 'F')
		ret_val = (c1 - 'A') + 10;
	else if (c1 >= 'a' && c1 <= 'f')
		ret_val = (c1 - 'a') + 10;

	ret_val = ret_val * 16;

	if (c2 >= '0' && c2 <= '9')
		ret_val += (c2 - '0');
	else if (c2 >= 'A' && c2 <= 'F')
		ret_val += (c2 - 'A') + 10;
	else if (c2 >= 'a' && c2 <= 'f')
		ret_val += (c2 - 'a') + 10;

	return ret_val;

}
static ssize_t led_blink_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct leds_dev_data *info = dev_get_drvdata(dev);
	struct pm8xxx_led_config *led_cfg;
	unsigned int brightness_r = 0;
	unsigned int brightness_g = 0;
	unsigned int brightness_b = 0;
	unsigned int loop_cnt = 0;
	unsigned int delayon = 0;
	unsigned int delayoff = 0;
	unsigned int argb_count = 0;

	printk(KERN_ALERT "[LED_blink_store] is \"%s\" (pid %i)\n",
	current->comm, current->pid);
	printk(KERN_ALERT "led_blink input =%s, size=%d\n", buf, size);

	if (size < 7) {
		printk(KERN_DEBUG "led_blink: Invlid input\n");
		return size;
	}
	if (buf[8] == ' ') { /*case of RGB delay_on delay_off*/
		for (loop_cnt = 9; loop_cnt < size-1; loop_cnt++) {
			delayon = delayon*10 + (buf[loop_cnt] - '0');
			if (buf[loop_cnt+1] == ' ') {
				loop_cnt += 2;
				break;
			}
		}
		for (; loop_cnt < size-1; loop_cnt++)
			delayoff = delayoff*10 + (buf[loop_cnt] - '0');
	}
	 else if (buf[10] == ' ') { /*case of ARGB delay_on delay_off*/
		argb_count = 1;
		for (loop_cnt = 11; loop_cnt < size-1; loop_cnt++) {
				delayon = delayon*10 + (buf[loop_cnt] - '0');
				if (buf[loop_cnt+1] == ' ') {
					loop_cnt += 2;
					break;
				}
			}
		for (; loop_cnt < size-1; loop_cnt++)
			delayoff = delayoff*10 + (buf[loop_cnt] - '0');
		}
	 else if (size > 9) {  /*case of ARGB*/
		argb_count = 1;
	}
	atomic_set(&info->op_flag , 0);
	/*buf[0], buf[1] contains 0x, so ignore it. case of RGB*/
	if (!argb_count) {
		brightness_r = hex_to_dec(buf[2], buf[3]);
		brightness_g = hex_to_dec(buf[4], buf[5]);
		brightness_b = hex_to_dec(buf[6], buf[7]);
	}
	/*buf[0], buf[1] contains 0x, so ignore it.
	buf[2], buf[3] contains A (alpha value), ignore it.case of ARGB*/
	 else {
		brightness_r = hex_to_dec(buf[4], buf[5]);
		brightness_g = hex_to_dec(buf[6], buf[7]);
		brightness_b = hex_to_dec(buf[8], buf[9]);
	}

	pm8xxx_led_work_pat_led_off(info);
	mutex_lock(&info->led_work_lock);

	led_cfg = &info->pdata->configs[PM8XXX_LED_PAT8_BLUE];
	brightness_b = brightness_b * 100 / 255;
	led_cfg->pwm_duty_cycles->duty_pcts[1] = brightness_b;
	pm8xxx_set_led_mode_and_max_brightness(&info->led[PM8XXX_LED_PAT8_BLUE],
				led_cfg->mode, led_cfg->max_current);
	__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT8_BLUE],
			led_cfg->max_current);
	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT8_BLUE],
				delayoff, delayon);

	led_cfg = &info->pdata->configs[PM8XXX_LED_PAT8_GREEN];
	brightness_g = brightness_g * 100 / 255;
	led_cfg->pwm_duty_cycles->duty_pcts[1] = brightness_g;
	pm8xxx_set_led_mode_and_max_brightness(
			&info->led[PM8XXX_LED_PAT8_GREEN],
			led_cfg->mode, led_cfg->max_current);
	__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT8_GREEN],
			led_cfg->max_current);
	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT8_GREEN],
				delayoff, delayon);

	led_cfg = &info->pdata->configs[PM8XXX_LED_PAT8_RED];
	brightness_r = brightness_r * 100 / 255;
	led_cfg->pwm_duty_cycles->duty_pcts[1] = brightness_r;
	pm8xxx_set_led_mode_and_max_brightness(&info->led[PM8XXX_LED_PAT8_RED],
				led_cfg->mode, led_cfg->max_current);
	__pm8xxx_led_work(&info->led[PM8XXX_LED_PAT8_RED],
			led_cfg->max_current);
	if (led_cfg->mode != PM8XXX_LED_MODE_MANUAL)
		pm8xxx_led_pwm_configure(&info->led[PM8XXX_LED_PAT8_RED],
		delayoff, delayon);

	if ((brightness_r || brightness_g || brightness_b) &&
	(info->pdata->led_power_on))
		info->pdata->led_power_on(1);
	printk(KERN_DEBUG "[LED] USER : R:%d,G:%d,B:%d\n",
		brightness_r, brightness_g, brightness_b);
	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT8_RED].cdev,
		led_cfg->max_current);
	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT8_GREEN].cdev,
		led_cfg->max_current);
	pm8xxx_led_set(&info->led[PM8XXX_LED_PAT8_BLUE].cdev,
		led_cfg->max_current);

	mutex_unlock(&info->led_work_lock);

	return size;

}

static DEVICE_ATTR(led_blink, S_IRUGO | S_IWUSR | S_IWGRP,
			led_blink_show, led_blink_store);

static void led_virtual_dev(struct leds_dev_data *info)
{
	struct device *sec_led;
	int error = 0;
	mutex_init(&info->led_work_lock);

	INIT_WORK(&info->work_pat_batt_chrg, pm8xxx_led_work_pat_batt_chrg);
	PREPARE_WORK(&info->work_pat_batt_chrg, pm8xxx_led_work_pat_batt_chrg);

	INIT_WORK(&info->work_pat_chrg_err, pm8xxx_led_work_pat_chrg_err);
	PREPARE_WORK(&info->work_pat_chrg_err, pm8xxx_led_work_pat_chrg_err);

	INIT_WORK(&info->work_pat_miss_noti, pm8xxx_led_work_pat_miss_noti);
	PREPARE_WORK(&info->work_pat_miss_noti, pm8xxx_led_work_pat_miss_noti);

	INIT_WORK(&info->work_pat_in_lowbat, pm8xxx_led_work_pat_in_lowbat);
	PREPARE_WORK(&info->work_pat_in_lowbat, pm8xxx_led_work_pat_in_lowbat);

	INIT_WORK(&info->work_pat_full_chrg, pm8xxx_led_work_pat_full_chrg);
	PREPARE_WORK(&info->work_pat_full_chrg, pm8xxx_led_work_pat_full_chrg);

	INIT_WORK(&info->work_pat_powering, pm8xxx_led_work_pat_powering);
	PREPARE_WORK(&info->work_pat_powering, pm8xxx_led_work_pat_powering);

	sec_led = device_create(sec_class, NULL, 0, NULL, "led");
	error = dev_set_drvdata(sec_led, info);
	if (error)
		pr_err("Failed to set sec_led driver data");
	error = device_create_file(sec_led, &dev_attr_led_pattern);
	if (error)
		pr_err("Failed to create /sys/class/sec/led/led_pattern");
	error = device_create_file(sec_led, &dev_attr_led_lowpower);
	if (error)
		pr_err("Failed to create /sys/class/sec/led/led_lowpower");
	error = device_create_file(sec_led, &dev_attr_led_r);
	if (error)
		pr_err("Failed to create /sys/class/sec/led/led_r");
	error = device_create_file(sec_led, &dev_attr_led_g);
	if (error)
		pr_err("Failed to create /sys/class/sec/led/led_g");
	error = device_create_file(sec_led, &dev_attr_led_b);
	if (error)
		pr_err("Failed to create /sys/class/sec/led/led_b");
	error = device_create_file(sec_led, &dev_attr_led_blink);
	if (error)
		pr_err("Failed to create /sys/class/sec/led/led_blink");

}
#endif

static int __devinit pm8xxx_led_probe(struct platform_device *pdev)
{
	const struct led_platform_data *pcore_data;
	struct led_info *curr_led;
	struct pm8xxx_led_config *led_cfg;
	struct pm8xxx_led_data *led, *led_dat;
	struct leds_dev_data *info;
	struct pm8xxx_led_platform_data *pdata ;
	int rc = -1, i = 0;
	pdata = pdev->dev.platform_data;
	if (pdata == NULL) {
		dev_err(&pdev->dev, "platform data not supplied\n");
		return -EINVAL;
	}

	pcore_data = pdata->led_core;

	if (pcore_data->num_leds != pdata->num_configs) {
		dev_err(&pdev->dev, "#no. of led configs and #no. of led"
				"entries are not equal\n");
		return -EINVAL;
	}

	led = kcalloc(pcore_data->num_leds, sizeof(*led), GFP_KERNEL);
	if (led == NULL) {
		dev_err(&pdev->dev, "failed to alloc memory\n");
		return -ENOMEM;
	}

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "fail to memory allocation.\n");
		rc = -ENOMEM;
		goto fail_mem_check;
	}

	info->pdata = pdata;
	info->led = led;

	for (i = 0; i < pcore_data->num_leds; i++) {
		curr_led	= &pcore_data->leds[i];
		led_dat		= &led[i];
		led_cfg		= &pdata->configs[i];

		led_dat->id     = led_cfg->id;
		led_dat->pwm_channel = led_cfg->pwm_channel;
		led_dat->pwm_period_us = led_cfg->pwm_period_us;
		led_dat->pwm_duty_cycles = led_cfg->pwm_duty_cycles;

		if (!((led_dat->id >= PM8XXX_ID_LED_KB_LIGHT) &&
				(led_dat->id <= PM8XXX_ID_FLASH_LED_1))) {
			dev_err(&pdev->dev, "invalid LED ID (%d) specified\n",
						 led_dat->id);
			rc = -EINVAL;
			goto fail_id_check;
		}

		led_dat->cdev.name		= curr_led->name;
		led_dat->cdev.default_trigger   = curr_led->default_trigger;
		led_dat->cdev.brightness_set    = pm8xxx_led_set;
		led_dat->cdev.brightness_get    = pm8xxx_led_get;
		led_dat->cdev.brightness	= LED_OFF;
		led_dat->cdev.flags		= curr_led->flags;
		led_dat->dev			= &pdev->dev;

		rc =  get_init_value(led_dat, &led_dat->reg);
		if (rc < 0)
			goto fail_id_check;

		rc = pm8xxx_set_led_mode_and_max_brightness(led_dat,
					led_cfg->mode, led_cfg->max_current);
		if (rc < 0)
			goto fail_id_check;

		mutex_init(&led_dat->lock);
		INIT_WORK(&led_dat->work, pm8xxx_led_work);
		if (led_dat->id == PM8XXX_ID_LED_KB_LIGHT)
			__pm8xxx_led_work(led_dat, LED_FULL);
		else
			__pm8xxx_led_work(led_dat, LED_OFF);
	}

	platform_set_drvdata(pdev, info);

	led_virtual_dev(info);

	low_powermode = 0;

	return 0;

fail_id_check:
	if (i > 0) {
		for (i = i - 1; i >= 0; i--) {
			mutex_destroy(&led[i].lock);
			led_classdev_unregister(&led[i].cdev);
			if (led[i].pwm_dev != NULL)
				pwm_free(led[i].pwm_dev);
		}
	}
	kfree(info);
fail_mem_check:
	kfree(led);
	return rc;
}

static int __devexit pm8xxx_led_remove(struct platform_device *pdev)
{
	int i;
	const struct led_platform_data *pdata =
				pdev->dev.platform_data;
	struct leds_dev_data *info = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
		cancel_work_sync(&info->led[i].work);
		mutex_destroy(&info->led[i].lock);
		led_classdev_unregister(&info->led[i].cdev);
		if (info->led[i].pwm_dev != NULL)
			pwm_free(info->led[i].pwm_dev);
	}

	kfree(info->led);
	kfree(info);
	return 0;
}

static struct platform_driver pm8xxx_led_driver = {
	.probe		= pm8xxx_led_probe,
	.remove		= __devexit_p(pm8xxx_led_remove),
	.driver		= {
		.name	= PM8XXX_LEDS_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8xxx_led_init(void)
{
	return platform_driver_register(&pm8xxx_led_driver);
}
subsys_initcall(pm8xxx_led_init);

static void __exit pm8xxx_led_exit(void)
{
	platform_driver_unregister(&pm8xxx_led_driver);
}
module_exit(pm8xxx_led_exit);


