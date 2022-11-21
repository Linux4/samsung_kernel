/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_CORE_H__
#define __VERTEX_CORE_H__

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>

#include "vertex-system.h"

/* Information about VIPx device file */
#define VERTEX_DEV_NAME_LEN		(16)
#define VERTEX_DEV_NAME			"vertex0"

struct vertex_device;
struct vertex_core;

struct vertex_core_refcount {
	atomic_t			refcount;
	struct vertex_core		*core;
	int				(*first)(struct vertex_core *core);
	int				(*final)(struct vertex_core *core);
};

struct vertex_dev {
	int                             minor;
	char                            name[VERTEX_DEV_NAME_LEN];
	struct miscdevice               miscdev;
};

struct vertex_core {
	struct vertex_dev			vdev;
	struct mutex			lock;
	struct vertex_core_refcount	open_cnt;
	struct vertex_core_refcount	start_cnt;
	const struct vertex_ioctl_ops	*ioc_ops;
	DECLARE_BITMAP(vctx_map, VERTEX_MAX_CONTEXT);
	struct list_head		vctx_list;
	unsigned int			vctx_count;

	struct vertex_device		*device;
	struct vertex_system		*system;
};

int vertex_core_probe(struct vertex_device *device);
void vertex_core_remove(struct vertex_core *core);

#endif
