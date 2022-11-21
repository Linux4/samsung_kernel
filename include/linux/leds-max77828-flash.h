/*
 * Flash-led driver for Maxim MAX77828
 *
 * Copyright (C) 2013 Maxim Integrated Product
 * Gyungoh Yoo <jack.yoo@maximintegrated.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LEDS_MAX77828_FLASH_H__
#define __LEDS_MAX77828_FLASH_H__

struct max77828_flash_platform_data
{
	bool flash_en_control;
	bool torch_stb_control;
	int flash_ramp_up;		/* One of 384, 640, 1152, 2176, 4224, 8320, 16512, 32896 usec */
	int flash_ramp_down;		/* One of 384, 640, 1152, 2176, 4224, 8320, 16512, 32896 usec */
	int torch_ramp_up;		/* One of 16392, 32776, 65544, 131080, 262152, 524296, 1048000, 2097000 usec */
	int torch_ramp_down;		/* One of 16392, 32776, 65544, 131080, 262152, 524296, 1048000, 2097000 usec */
	bool enable_pulldown_resistor;	/* On/Off Control for Pulldown Resistor */
	bool enable_maxflash;		/* enable MAXFLASH */
	int maxflash_threshold;		/* 2400000uV to 3400000uV in 33333uV steps */
	int maxflash_hysteresis;	/* One of 50000uV, 100000uV, 300000uV, 350000uV or -1. -1 means only allowed to decrease */
	int maxflash_falling_timer;	/* 256us to 4096us in 256us steps */
	int maxflash_rising_timer;	/* 256us to 4096us in 256us steps */
	struct device_node *np;
	int flash_pin;
	int torch_pin;
	unsigned int flash_timer;
	unsigned int torch_timer;
	unsigned int flash_mode;
	unsigned int torch_mode;
	unsigned int flash_timer_cntl;
	unsigned int torch_timer_cntl;
};

#endif /* __LEDS_MAX77828_FLASH_H__ */
