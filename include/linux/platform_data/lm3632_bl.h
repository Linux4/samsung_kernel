/*
* Simple driver for Texas Instruments LM3632 LED Flash driver chip
* Copyright (C) 2012 Texas Instruments
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
*/

#ifndef __LINUX_LM3632_H
#define __LINUX_LM3632_H

#define LM3632_NAME "lm3632_bl"

enum lm3632_pwm {
	LM3632_PWM_DISABLE = 0x00,
	LM3632_PWM_EN_ACTLOW = 0x58,
	LM3632_PWM_EN_ACTHIGH = 0x50,
};

enum lm3632_strobe {
	LM3632_STROBE_DISABLE = 0x00,
	LM3632_STROBE_EN_ACTLOW = 0x10,
	LM3632_STROBE_EN_ACTHIGH = 0x30,
};

enum lm3632_txpin {
	LM3632_TXPIN_DISABLE = 0x00,
	LM3632_TXPIN_EN_ACTLOW = 0x04,
	LM3632_TXPIN_EN_ACTHIGH = 0x0C,
};

enum lm3632_fleds {
	LM3632_FLED_DIASBLE_ALL = 0x00,
	LM3632_FLED_EN_1 = 0x40,
	LM3632_FLED_EN_2 = 0x20,
	LM3632_FLED_EN_ALL = 0x60,
};

enum lm3632_bleds {
	LM3632_BLED_DIASBLE_ALL = 0x00,
	LM3632_BLED_EN_1 = 0x10,
	LM3632_BLED_EN_2 = 0x08,
	LM3632_BLED_EN_ALL = 0x18,
};
enum lm3632_bled_mode {
	LM3632_BLED_MODE_EXPONETIAL = 0x00,
	LM3632_BLED_MODE_LINEAR = 0x10,
};

struct lm3632_platform_data {
	unsigned int max_brt_led;
	unsigned int init_brt_led;

	/* input pins */
	enum lm3632_pwm pin_pwm;
	enum lm3632_strobe pin_strobe;
	enum lm3632_txpin pin_tx;

	/* output pins */
	enum lm3632_fleds fled_pins;
	enum lm3632_bleds bled_pins;
	enum lm3632_bled_mode bled_mode;

	void (*pwm_set_intensity) (int brightness, int max_brightness);
	int (*pwm_get_intensity) (void);
};
#endif /* __LINUX_LM3632_H */
