/*
 * Copyright (c) 2015-2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stui_core.h"
#include "stui_hal.h"

int stui_i2c_protect(bool is_protect)
{
	printk(KERN_DEBUG "[STUI] stui_i2c_protect(%s) called\n",
			is_protect ? "true" : "false");

	return 0;
}
