/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Motto Support Driver
 * Author: Minwoo Kim <minwoo7945.kim@samsung.com>
 *         Kimyung Lee <kernel.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DSIM_MOTTO_H__
#define __DSIM_MOTTO_H__
#include <linux/device.h>

struct dsim_device;

struct dsim_motto_info {
	struct device *dev;
	struct class *class;
	struct kobject *kobj;
	unsigned int init_swing;
	unsigned int init_impedance;
	unsigned int init_emphasis;
	unsigned int tune_swing;
	unsigned int tune_impedance;
	unsigned int tune_emphasis;
};

int dsim_motto_probe(struct dsim_device *dsim);
#endif
