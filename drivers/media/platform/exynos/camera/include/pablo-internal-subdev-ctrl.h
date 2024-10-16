// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_INTERNAL_SUBDEV_CTRL_H
#define PABLO_INTERNAL_SUBDEV_CTRL_H

#include <linux/kernel.h>

#include "pablo-framemgr.h"
#include "is-type.h"

#define PABLO_SUBDEV_INTERNAL_BUF_MAX 8

struct pablo_internal_subdev;

enum pablo_subdev_state {
	PABLO_SUBDEV_ALLOC,
};

struct pablo_internal_subdev_ops {
	int (*alloc)(struct pablo_internal_subdev *sd);
	int (*free)(struct pablo_internal_subdev *sd);
};

struct pablo_internal_subdev {
	u32 id;
	char *stm;
	u32 instance;
	char name[IS_STR_LEN];
	unsigned long state;

	u32 width;
	u32 height;
	u32 num_planes;
	u32 num_batch;
	u32 num_buffers;
	u32 bits_per_pixel;
	u32 memory_bitwidth;
	u32 size[IS_MAX_PLANES];
	bool secure;

	struct is_mem *mem;
	struct is_priv_buf *pb[PABLO_SUBDEV_INTERNAL_BUF_MAX];
	struct is_framemgr internal_framemgr;

	const struct pablo_internal_subdev_ops *ops;
};

#define CALL_I_SUBDEV_OPS(sd, op, args...)	\
	(((sd)->ops && (sd)->ops->op) ? ((sd)->ops->op(args)) : 0)

int pablo_internal_subdev_probe(struct pablo_internal_subdev *sd, u32 instance, u32 id,
				struct is_mem *mem, const char *name);

#endif
