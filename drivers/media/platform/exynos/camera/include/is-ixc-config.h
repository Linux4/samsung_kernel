/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_I2C_CONFIG_H
#define IS_I2C_CONFIG_H

#include <linux/i2c.h>

/* I2C PIN MODE */
enum {
	I2C_PIN_STATE_OFF = 0,
	I2C_PIN_STATE_ON,
};

#if IS_ENABLED(CONFIG_I2C) || IS_ENABLED(CONFIG_I3C)
int is_ixc_pin_control(struct is_module_enum *module, u32 scenario, u32 value);
#else
#define is_ixc_pin_control(p, q, r)	0
#endif

#endif
