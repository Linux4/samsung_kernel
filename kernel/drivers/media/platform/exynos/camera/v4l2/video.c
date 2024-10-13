// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include "is-video.h"
#include "pablo-debug.h"
#include "pablo-kernel-variant.h"
#include "pablo-v4l2.h"

static unsigned int mp_verify_v4l2_buffer;
module_param_named(verify_v4l2_buffer, mp_verify_v4l2_buffer, uint, S_IRUGO | S_IWUSR);

static void pablo_verify_v4l2_buffer(struct is_video_ctx *ivc, struct v4l2_buffer *buf)
{
	struct is_queue *queue = GET_QUEUE(ivc);
	struct is_video *video = GET_VIDEO(ivc);
	struct vb2_queue *vbq = queue->vbq;
	struct vb2_buffer *vb;
	struct vb2_plane *planes;
	struct dma_buf *dbuf;
	int p;

	if (buf->type != vbq->type) {
		mverr("buf type is invalid(%d, %d)", ivc, video, buf->type, vbq->type);
		return;
	}

	if (buf->index >= vbq->num_buffers) {
		mverr("buffer index%d out of range", ivc, video, buf->index);
		return;
	}

	vb = vbq->bufs[buf->index];
	if (!vb) {
		mverr("vb_buffer is NULL", ivc, video);
		return;
	}

	if (buf->memory != vbq->memory) {
		mverr("invalid memory type(%d, %d)", ivc, video, buf->memory, vbq->memory);
		return;
	}

	/* verify planes array */
	if (V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
		/* Is memory for copying plane information present? */
		if (buf->m.planes == NULL) {
			mverr("multi-planar buffer passed but "
					"planes array not provided", ivc, video);
			return;
		}

		if (buf->length < vb->num_planes || buf->length > VB2_MAX_PLANES) {
			mverr("incorrect planes array length, "
					"expected %d, got %d", ivc, video,
						vb->num_planes, buf->length);
			return;
		}
	}

	/* still have a problem, have to verify dma_buf */
	planes = kvcalloc(vb->num_planes, sizeof(struct vb2_plane), GFP_KERNEL);
	if (!planes) {
		mverr("failed to alloc planes to verify", ivc, video);
		return;
	}

	vb->vb2_queue->buf_ops->pablo_fill_vb2_buffer(vb, buf, planes);

	for (p = 0; p < vb->num_planes; p++) {
		dbuf = dma_buf_get(planes[p].m.fd);
		if (IS_ERR_OR_NULL(dbuf)) {
			mverr("invalid dmabuf fd(%d) for plane %d",
					ivc, video, planes[p].m.fd, p);
			break;
		}

		if (planes[p].length == 0)
			planes[p].length = (unsigned int)dbuf->size;

		if (planes[p].length < vb->planes[p].min_length) {
			mverr("invalid dmabuf length %u for plane %d, "
					"minimum length %u",
					ivc, video, planes[p].length, p,
					vb->planes[p].min_length);
			dma_buf_put(dbuf);
			break;
		}
		dma_buf_put(dbuf);
	}

	kvfree(planes);
}

int pablo_video_qbuf(struct is_video_ctx *vctx, struct v4l2_buffer *buf)
{
	int ret;
	struct is_queue *queue = GET_QUEUE(vctx);
	struct is_video *video = GET_VIDEO(vctx);

	TIME_QUEUE(TMQ_QS);

	if (!queue) {
		mverr("the queue of a video context is not allocated yet", vctx, video);
		TIME_QUEUE(TMQ_QE);
		return -EINVAL;
	}

	queue->buf_req++;

	ret = pablo_vb2_qbuf(queue->vbq, buf);
	if (ret) {
		mverr("vb2_qbuf is fail(index : %d, %d)", vctx, video, buf->index, ret);
		if (mp_verify_v4l2_buffer)
			pablo_verify_v4l2_buffer(vctx, buf);
	}

	TIME_QUEUE(TMQ_QE);

	return ret;
}

int pablo_video_dqbuf(struct is_video_ctx *vctx, struct v4l2_buffer *buf, bool blocking)
{
	int ret;
	struct is_queue *queue = GET_QUEUE(vctx);
	struct is_video *video = GET_VIDEO(vctx);
	struct is_sysfs_debug *sysfs_debug;

	if (!queue) {
		mverr("the queue of a video context is not allocated yet", vctx, video);
		TIME_QUEUE(TMQ_DQ);
		return -EINVAL;
	}

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		mverr("queue(0x%lx) is not streaming", vctx, video, queue->state);
		TIME_QUEUE(TMQ_DQ);
		return -EINVAL;
	}

	queue->buf_dqe++;

	ret = vb2_dqbuf(queue->vbq, buf, blocking);
	if (ret) {
		mverr("vb2_dqbuf is fail(%d)", vctx, video, ret);

		sysfs_debug = is_get_sysfs_debug();
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug->hal_debug_mode)
				&& ret == -ERESTARTSYS) {
			msleep(sysfs_debug->hal_debug_delay);
			panic("would be interrupted by a fatal signal(SIGKILL)");
		}
	}

	TIME_QUEUE(TMQ_DQ);

	return ret;
}

int pablo_video_buffer_done(struct is_video_ctx *vctx, u32 index, u32 state)
{
	struct is_queue *queue = GET_QUEUE(vctx);
	struct is_video *video = GET_VIDEO(vctx);

	if (!queue) {
		mverr("the queue of a video context is not allocated yet", vctx, video);
		return -EINVAL;
	}

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		warn("video%d's queue(0x%lx) is not streaming", video->id, queue->state);
		return -EINVAL;
	}

	queue->buf_com++;

	if ((index < queue->vbq->num_buffers) && queue->vbq->bufs[index])
		vb2_buffer_done(queue->vbq->bufs[index], state);

	return 0;
}

MODULE_DESCRIPTION("video module for Exynos Pablo");
MODULE_LICENSE("GPL v2");
