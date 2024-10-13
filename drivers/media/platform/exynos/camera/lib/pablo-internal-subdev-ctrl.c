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

#include "pablo-internal-subdev-ctrl.h"
#include "pablo-debug.h"
#include "pablo-mem.h"

static size_t pis_sum_batch_size(struct pablo_internal_subdev *sd)
{
	int i, j;
	size_t single_buf_size = 0;

	for (i = 0; i < sd->num_planes; i++)
		single_buf_size += sd->size[i];

	for (i = 1; i < sd->num_batch; i++) {
		for (j = 0; j < sd->num_planes; j++)
			sd->size[j + (i * sd->num_planes)] = sd->size[j];
	}

	return single_buf_size * sd->num_batch;
}

static int pis_alloc(struct pablo_internal_subdev *sd, size_t buf_size)
{
	int ret, i, j;
	struct is_mem *mem = sd->mem;
	struct is_frame *frame;
	char heapname[25] = { 0, };

	if (sd->num_buffers > PABLO_SUBDEV_INTERNAL_BUF_MAX) {
		mserr("invalid internal num_buffers (%d)", sd, sd, sd->num_buffers);
		return -EINVAL;
	}

	ret = frame_manager_open(&sd->internal_framemgr, sd->num_buffers);
	if (ret) {
		mserr("frame_manager_open is fail(%d)", sd, sd, ret);
		return ret;
	}

	if (sd->secure)
		snprintf(heapname, sizeof(heapname), SECURE_HEAPNAME);

	for (i = 0; i < sd->num_buffers; i++) {
		sd->pb[i] = CALL_PTR_MEMOP(mem, alloc, mem->priv, buf_size, heapname, 0);
		if (IS_ERR_OR_NULL(sd->pb[i])) {
			mserr("failed to alloc", sd, sd);
			sd->pb[i] = NULL;
			ret = -ENOMEM;
			goto err_allocate_pb_subdev;
		}

		frame = &sd->internal_framemgr.frames[i];
		frame->subdev = sd;
		frame->pb_output = sd->pb[i];
		frame->planes = sd->num_planes;
		frame->num_buffers = sd->num_batch;
		set_bit(FRAME_MEM_INIT, &frame->mem_state);

		frame->dvaddr_buffer[0] = CALL_BUFOP(sd->pb[i], dvaddr, sd->pb[i]);

		if (sd->secure)
			frame->kvaddr_buffer[0] = 0;
		else
			frame->kvaddr_buffer[0] = CALL_BUFOP(sd->pb[i], kvaddr, sd->pb[i]);

		for (j = 1; j < IS_MAX_PLANES && sd->size[j]; j++) {
			frame->dvaddr_buffer[j] = frame->dvaddr_buffer[j - 1] + sd->size[j - 1];
			if (frame->kvaddr_buffer[0])
				frame->kvaddr_buffer[j] = frame->kvaddr_buffer[j - 1]
							+ sd->size[j - 1];
		}
	}

	return 0;

err_allocate_pb_subdev:
	while (i-- > 0) {
		if (sd->pb[i])
			CALL_VOID_BUFOP(sd->pb[i], free, sd->pb[i]);
	}

	frame_manager_close(&sd->internal_framemgr);

	return ret;
}

static int pablo_internal_subdev_alloc(struct pablo_internal_subdev *sd)
{
	int ret;
	size_t buf_size;

	if (test_bit(PABLO_SUBDEV_ALLOC, &sd->state)) {
		mserr("already alloc", sd, sd);
		return -EINVAL;
	}

	buf_size = pis_sum_batch_size(sd);

	ret = pis_alloc(sd, buf_size);
	if (ret) {
		mserr("failed to pis_alloc(%d)", sd, sd, ret);
		return ret;
	}

	msinfo(" %s (size: %dx%d, bpp: %d, num_batch: %d, num_buffers: %d, total: %zu)\n",
		sd, sd, __func__, sd->width, sd->height, sd->bits_per_pixel,
		sd->num_batch, sd->num_buffers, buf_size * sd->num_buffers);

	set_bit(PABLO_SUBDEV_ALLOC, &sd->state);

	return 0;
}

static int pablo_internal_subdev_free(struct pablo_internal_subdev *sd)
{
	int i;

	if (!test_bit(PABLO_SUBDEV_ALLOC, &sd->state)) {
		mswarn(" already free internal buffer", sd, sd);
		return -EINVAL;
	}

	for (i = 0; i < sd->num_buffers; i++) {
		if (sd->pb[i])
			CALL_VOID_BUFOP(sd->pb[i], free, sd->pb[i]);
	}

	frame_manager_close(&sd->internal_framemgr);

	clear_bit(PABLO_SUBDEV_ALLOC, &sd->state);

	return 0;
}

static const struct pablo_internal_subdev_ops pis_ops = {
	.alloc = pablo_internal_subdev_alloc,
	.free = pablo_internal_subdev_free,
};

int pablo_internal_subdev_probe(struct pablo_internal_subdev *sd, u32 instance, u32 id,
				struct is_mem *mem, const char *name)
{
	sd->ops = &pis_ops;
	sd->id = id;
	sd->stm = pablo_lib_get_stream_prefix(instance);
	sd->instance = instance;
	sd->mem = mem;
	snprintf(sd->name, sizeof(sd->name), name);
	sd->state = 0L;
	sd->secure = 0;

	frame_manager_probe(&sd->internal_framemgr, sd->id, name);

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_internal_subdev_probe);
