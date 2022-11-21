/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_DEVICE_H__
#define __VERTEX_DEVICE_H__

#include <linux/device.h>

#include "vertex-system.h"
#include "vertex-core.h"
#include "vertex-debug.h"

struct vertex_device;

enum vertex_device_state {
	VERTEX_DEVICE_STATE_OPEN,
	VERTEX_DEVICE_STATE_START
};

struct vertex_device {
	struct device		*dev;
	unsigned long		state;

	struct vertex_system	system;
	struct vertex_core	core;
	struct vertex_debug	debug;
};

int vertex_device_open(struct vertex_device *device);
int vertex_device_close(struct vertex_device *device);
int vertex_device_start(struct vertex_device *device);
int vertex_device_stop(struct vertex_device *device);

#endif
