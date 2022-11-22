
/*
 * Copyright (C) 2013 Texas Instruments
 *
 * License Terms: GNU General Public License v2
 *
 * Simple driver for Texas Instruments LM3561 LED driver chip
 *
 * Author: Daniel Jeong <daniel.jeong@ti.com>
 *
 */

#ifndef __LINUX_LM356x_H
#define __LINUX_LM356x_H

#define LM3560_NAME "leds-lm3560"

enum lm3560_ctrl {
	LM3560_DISABLE = 0x00,
	LM3560_ENABLE = 0x01,
};

/*struct lm356x_platform_data
 *@strobe : strobe pin usage
 *@torch  : torch  pin usage
 *@flash_time : flash duration time
 *              0~ 31 : 32*(flash_time+1)ms
 *@indi_led1 : led1 for privacy mode
 *@indi_led2 : led2 for privacy mode
 */
struct lm3560_platform_data {
	enum lm3560_ctrl strobe;
	enum lm3560_ctrl torch;
	u8 flash_time;

	enum lm3560_ctrl indi_led1;
	enum lm3560_ctrl indi_led2;
	int strobe_pin;
	int torch_pin;
	struct device_node *np;
};

#endif /* __LINUX_LM356x_H */
