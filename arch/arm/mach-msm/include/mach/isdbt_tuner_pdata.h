/*
* Copyright (C) (2011, Samsung Electronics)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation version 2.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#ifndef _ISDBT_TUNER_PDATA_H_
#define _ISDBT_TUNER_PDATA_H_

#if defined(CONFIG_ISDBT_FC8300) || defined(CONFIG_ISDBT_FC8150)
struct isdbt_platform_data {
	int	irq;
	int gpio_en;
	int gpio_rst;
	int gpio_int;
	int gpio_i2c_sda;
	int gpio_i2c_scl;
};
#endif
#endif
