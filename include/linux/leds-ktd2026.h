/*
 * Copyright (C) 2011 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LEDS_KTD2026_H
#define _LEDS_KTD2026_H

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/leds.h>

#define SEC_LED_SPECIFIC

/* KTD2026 register map */
#define KTD2026_REG_EN_RST		0x00
#define KTD2026_REG_FLASH_PERIOD	0x01
#define KTD2026_REG_PWM1_TIMER		0x02
#define KTD2026_REG_PWM2_TIMER		0x03
#define KTD2026_REG_LED_EN		0x04
#define KTD2026_REG_TRISE_TFALL		0x05
#define KTD2026_REG_LED1		0x06
#define KTD2026_REG_LED2		0x07
#define KTD2026_REG_LED3		0x08
#define KTD2026_REG_MAX			0x09
#define KTD2026_TIME_UNIT		500
/* MASK */
#define CNT_TIMER_SLOT_MASK		0x07
#define CNT_ENABLE_MASK			0x18
#define CNT_RISEFALL_TSCALE_MASK	0x60

#define CNT_TIMER_SLOT_SHIFT		0x00
#define CNT_ENABLE_SHIFT		0x03
#define CNT_RISEFALL_TSCALE_SHIFT	0x05

#define LED_R_MASK		0x00ff0000
#define LED_G_MASK		0x0000ff00
#define LED_B_MASK		0x000000ff
#define LED_R_SHIFT		16
#define LED_G_SHIFT		8

#define KTD2026_RESET		0x07

#define LED_MAX_CURRENT		0x20
#define LED_DEFAULT_CURRENT	0x10

#ifdef CONFIG_USE_LIGHT_SENSOR
#define LED_LOW_CURRENT		0x02
#endif

#define LED_OFF			0x00

#define MAX_NUM_LEDS		3

enum ktd2026_led_mode {
	LED_EN_OFF	= 0,
	LED_EN_ON,
	LED_EN_PWM1,
	LED_EN_PWM2,
};
enum ktd2026_pwm{
	PWM1 = 0,
	PWM2,
};

enum ktd2026_pattern {
	PATTERN_OFF,
	CHARGING,
	CHARGING_ERR,
	MISSED_NOTI,
	LOW_BATTERY,
	FULLY_CHARGED,
	POWERING,
};

enum ktd2026_led_enum {
	LED_R = 0,
	LED_G = 2,
	LED_B = 4,
};

struct ktd2026_led {
	struct led_classdev cdev;
	struct work_struct brightness_work;
	u8 channel;
	u8 brightness;
	unsigned long delay_on_time_ms;
	unsigned long delay_off_time_ms;
};

struct ktd2026_led_conf {
	const char *name;
	u8 brightness;
	u8 max_brightness;
	u8 flags;
};

struct ktd2026_led_pdata {
	struct ktd2026_led_conf *led_conf;
	int leds;
	int svcled_en;
};

struct ktd2026_drvdata {
	struct device *dev;
	struct i2c_client *client;
	struct mutex mutex;
	struct ktd2026_led	led[MAX_NUM_LEDS];
	struct ktd2026_led_pdata *pdata;
#ifdef CONFIG_SEC_FACTORY
	bool enable;
#endif
	u8 led_dynamic_current;
#ifdef CONFIG_USE_LIGHT_SENSOR
	u8 led_lowpower_mode;
#endif
	u8 shadow_reg[KTD2026_REG_MAX];
};

#endif						/* _LEDS_KTD2026_H */
