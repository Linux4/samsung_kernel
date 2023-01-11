/*
 * Copyright (C) 2014 Marvell Technology Group Ltd.
 *
 * Authors: Jane Li <jiel@marvell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#ifndef _LINUX_LED_RGB_H
#define _LINUX_LED_RGB_H
enum led_rgb_mode {
	LED_R = 0,
	LED_G,
	LED_B,
};

struct led_rgb_platform_data {
	int		gpio_r;
	int		gpio_g;
	int		gpio_b;
};

#if defined(CONFIG_LEDS_RGB)
extern void led_rgb_output(enum led_rgb_mode led_mode, int on);
#else
void led_rgb_output(enum led_rgb_mode led_mode, int on) {}
#endif
#endif
