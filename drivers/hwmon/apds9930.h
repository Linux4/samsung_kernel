/*
 * linux/arch/arm/mach-mmp/include/mach/apds9930.h
 *
 *  Copyright (C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _APDS9930_H_
#define _APDS9930_H_

struct apds9930_platform_data {
	unsigned int irq;
	char avdd_name[20];
};

#endif
