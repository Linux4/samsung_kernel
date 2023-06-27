/*
 * linux/arch/arm/mach-mmp/include/mach/isl29043.h
 *
 *  Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ISL29043_H_
#define _ISL29043_H_

struct isl29043_platform_data {
	int wakup_gpio;
	int (*set_power)(int);
};

#endif
