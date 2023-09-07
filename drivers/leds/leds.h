/*
 * LED Core
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LEDS_H_INCLUDED
#define __LEDS_H_INCLUDED

#include <linux/rwsem.h>
#include <linux/leds.h>

#ifndef CONFIG_LEDS_S2MU005
extern struct class *camera_class;
#elif defined(CONFIG_LEDS_S2MU005)
void ss_rear_flash_led_flash_on(void);
void ss_rear_flash_led_torch_on(void);
void ss_rear_flash_led_turn_off(void);
void ss_rear_torch_set_flashlight(int torch_mode);
void ss_front_flash_led_turn_on(void);
void ss_front_flash_led_turn_off(void);
void s2mu005_led_enable_ctrl(int mode);
int s2mu005_led_mode_ctrl(int enable_mode, int state);
#else
#endif

#if defined(CONFIG_GET_REAR_MODULE_ID)
extern void get_imx219_moduleid(u8 *data);
#endif

static inline void led_set_brightness_async(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	value = min(value, led_cdev->max_brightness);
	led_cdev->brightness = value;

	if (!(led_cdev->flags & LED_SUSPENDED))
		led_cdev->brightness_set(led_cdev, value);
}

static inline int led_set_brightness_sync(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	int ret = 0;

	led_cdev->brightness = min(value, led_cdev->max_brightness);

	if (!(led_cdev->flags & LED_SUSPENDED))
		ret = led_cdev->brightness_set_sync(led_cdev,
						led_cdev->brightness);
	return ret;
}

static inline int led_get_brightness(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

void led_init_core(struct led_classdev *led_cdev);
void led_stop_software_blink(struct led_classdev *led_cdev);

extern struct rw_semaphore leds_list_lock;
extern struct list_head leds_list;

#endif	/* __LEDS_H_INCLUDED */
