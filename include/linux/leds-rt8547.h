/* include/linux/leds-rt8547.h
 * Include file of driver to Richtek RT8547 LED Flash IC
 *
 * Copyright (C) 2013 Richtek Technology Corporation
 * Author: CY Huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_LEDS_RT8547_H
#define __LINUX_LEDS_RT8547_H

enum STROBE_CURRENT {
	STROBE_CURRENT_100MA = 0,
	STROBE_CURRENT_150MA = 1,
	STROBE_CURRENT_200MA = 2,
	STROBE_CURRENT_250MA = 3,
	STROBE_CURRENT_300MA = 4,
	STROBE_CURRENT_350MA = 5,
	STROBE_CURRENT_400MA = 6,
	STROBE_CURRENT_450MA = 7,
	STROBE_CURRENT_500MA = 8,
	STROBE_CURRENT_550MA = 9,
	STROBE_CURRENT_600MA = 10,
	STROBE_CURRENT_650MA = 11,
	STROBE_CURRENT_700MA = 12,
	STROBE_CURRENT_750MA = 13,
	STROBE_CURRENT_800MA = 14,
	STROBE_CURRENT_850MA = 15,
	STROBE_CURRENT_900MA = 16,
	STROBE_CURRENT_950MA = 17,
	STROBE_CURRENT_1000MA= 18,
	STROBE_CURRENT_1050MA= 19,
	STROBE_CURRENT_1100MA= 20,
	STROBE_CURRENT_1150MA= 21,
	STROBE_CURRENT_1200MA= 22,
	STROBE_CURRENT_1250MA= 23,
	STROBE_CURRENT_1300MA= 24,
	STROBE_CURRENT_1350MA= 25,
	STROBE_CURRENT_1400MA= 26,
	STROBE_CURRENT_1450MA= 27,
	STROBE_CURRENT_1500MA= 28,
	STROBE_CURRENT_1550MA= 29,
	STROBE_CURRENT_1600MA= 30,
};

enum TORCH_CURRENT {
	TORCH_CURRENT_25MA = 0,
	TORCH_CURRENT_50MA = 1,
	TORCH_CURRENT_75MA = 2,
	TORCH_CURRENT_100MA= 3,
	TORCH_CURRENT_125MA= 4,
	TORCH_CURRENT_150MA= 5,
	TORCH_CURRENT_175MA= 6,
	TORCH_CURRENT_200MA= 7,
	TORCH_CURRENT_225MA= 8,
	TORCH_CURRENT_250MA= 9,
	TORCH_CURRENT_275MA= 10,
	TORCH_CURRENT_300MA= 11,
	TORCH_CURRENT_325MA= 12,
	TORCH_CURRENT_350MA= 13,
	TORCH_CURRENT_375MA= 14,
	TORCH_CURRENT_400MA= 15,
};

#define STROBE_CURRENT_MASK 0x1F
#define TORCH_CURRENT_MASK  0x0F
#define STROBE_TIMING_MASK  0x3F
#define LED_MODE_MASK	    0x10
#define LED_SW_RESET        0x20

struct rt8547_platform_data {
	int flen_gpio;
	int ctl_gpio;
	int flset_gpio;
	unsigned short strobe_current:5;
	unsigned short torch_current:4;
	unsigned short strobe_timing:6;
};

struct rt8547_chip {
	struct device *dev;
	int ref_cnt;
	int suspend;
	spinlock_t io_lock;
};

// RT8547 global function declaration
int rt8547_open_handle(void);
int rt8547_release_handle(void);
int rt8547_set_led_low(void);
int rt8547_set_led_high(void);
int rt8547_set_led_off(void);

#endif /* #ifndef __LINUX_LEDS_RT8547_H */
