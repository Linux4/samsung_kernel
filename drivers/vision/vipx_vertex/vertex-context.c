/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>

#include "vertex-log.h"
#include "vertex-util.h"
#include "vertex-context.h"

struct vertex_context *vertex_context_create(struct vertex_core *core)
{
	int ret;
	struct vertex_context *vctx;

	vertex_enter();
	vctx = kzalloc(sizeof(*vctx), GFP_KERNEL);
	if (!vctx) {
		ret = -ENOMEM;
		vertex_err("Failed to alloc context\n");
		goto p_err;
	}
	vctx->core = core;

	vctx->idx = vertex_util_bitmap_get_zero_bit(core->vctx_map,
			VERTEX_MAX_CONTEXT);
	if (vctx->idx == VERTEX_MAX_CONTEXT) {
		ret = -ENOMEM;
		vertex_err("Failed to get idx of context\n");
		goto p_err_bitmap;
	}

	core->vctx_count++;
	list_add_tail(&vctx->list, &core->vctx_list);

	mutex_init(&vctx->lock);

	vertex_queue_init(vctx);

	vctx->state = BIT(VERTEX_CONTEXT_OPEN);
	vertex_leave();
	return vctx;
p_err_bitmap:
	kfree(vctx);
p_err:
	return ERR_PTR(ret);
}

void vertex_context_destroy(struct vertex_context *vctx)
{
	struct vertex_core *core;

	vertex_enter();
	core = vctx->core;

	mutex_destroy(&vctx->lock);
	list_del(&vctx->list);
	core->vctx_count--;
	vertex_util_bitmap_clear_bit(core->vctx_map, vctx->idx);
	kfree(vctx);
	vertex_leave();
}
