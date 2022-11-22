/* Copyright (c) 2010-2011,2013-2014, The Linux Foundation. All rights reserved.
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
#ifndef __SEC_PINMUX_H
#define __SEC_PINMUX_H

#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/secgpio_dvs.h>
#include <dt-bindings/pinctrl/mt65xx.h>

enum gpiomux_func {
	GPIOMUX_FUNC_GPIO = 0,
	GPIOMUX_FUNC_1,
	GPIOMUX_FUNC_2,
	GPIOMUX_FUNC_3,
	GPIOMUX_FUNC_4,
	GPIOMUX_FUNC_5,
	GPIOMUX_FUNC_6,
	GPIOMUX_FUNC_7,
};

enum gpiomux_dir {
	GPIOMUX_IN = 0,
	GPIOMUX_OUT,
};

enum gpiomux_level {
	GPIOMUX_LOW = 0,
	GPIOMUX_HIGH,
};

enum gpiomux_pull_en {
	GPIOMUX_PULL_DIS = 0, /* pull up = 0 & pull down = 0 */
	GPIOMUX_PULL_EN,
	GPIOMUX_PULL_NONE = MTK_PUPD_SET_R1R0_00,
	GPIOMUX_PULL1,
	GPIOMUX_PULL2,
	GPIOMUX_PULL3,
};

enum gpiomux_pull {
	GPIOMUX_PULL_DOWN = 0,
	GPIOMUX_PULL_UP,
};

struct gpiomux_setting {
	int func;
	int dir;
	u32 pull_en;
	u32 pull;
};

#endif /* __SEC_PINMUX_H */
