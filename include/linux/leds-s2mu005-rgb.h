/*
 * RGB-led driver for IF PMIC s2mu005
 *
 * Copyright (C) 2015 Samsung Electronics
 * Suji Lee <suji0908.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LEDS_S2MU005_RGB_H__
#define __LEDS_S2MU005_RGB_H__

/* s2mu005 LED numbers */
#define S2MU005_RGBLED_NUM	3

/* s2mu005_REG_LED_DUR */
#define S2MU005_LED_ON_DUR	0xF0
#define S2MU005_LED_OFF_DUR	0x0F

/* s2mu005_REG_LEDRMP */
#define S2MU005_RAMP_UP		0xF0
#define S2MU005_RAMP_DN		0x0F

#define LED_R_MASK		0x00FF0000
#define LED_G_MASK		0x0000FF00
#define LED_B_MASK		0x000000FF
#define LED_MAX_CURRENT		0xFF

/* s2mu005_Reset */
#define RGBLED_REG_RESET_MASK	0x40

/* s2mu005_STATE*/
#define RGBLED_DISABLE		0
#define RGBLED_ALWAYS_ON	1
#define RGBLED_BLINK		2

#define RGBLED_ENMASK		3
#define LEDBLNK_ON(time)	((time < 100) ? 0 :			\
				(time < 500) ? time/100-1 :		\
				(time < 3250) ? (time-500)/250+4 : 15)

#define LEDBLNK_OFF(time)	((time < 500) ? 0x00 :			\
				(time < 5000) ? time/500 :		\
				(time < 8000) ? (time-5000)/1000+10 :	 \
				(time < 12000) ? (time-8000)/2000+13 : 15)

/* 2mA */
static u8 led_dynamic_current = 0x14;
static u8 normal_powermode_current = 0x14;
#ifdef CONFIG_USE_LIGHT_SENSOR
/* 0.5mA */
static u8 low_powermode_current = 0x05;
#endif
static unsigned int brightness_ratio_r = 100;
static unsigned int brightness_ratio_g = 100;
static unsigned int brightness_ratio_b = 100;
#ifdef CONFIG_USE_LIGHT_SENSOR
static u8 led_lowpower_mode = 0x0;
#endif

enum s2mu005_led_color {
	BLUE,
	GREEN,
	RED,
	RGBLED_MAX = S2MU005_RGBLED_NUM
};
enum s2mu005_led_pattern {
	PATTERN_OFF,
	CHARGING,
	CHARGING_ERR,
	MISSED_NOTI,
	LOW_BATTERY,
	FULLY_CHARGED,
	POWERING,
	VOICE_REC,
};
struct s2mu005_rgb_platform_data
{
	char *name[S2MU005_RGBLED_NUM];
	unsigned int ch[S2MU005_RGBLED_NUM];
};

#endif /* __LEDS_MAX77843_RGB_H__ */
