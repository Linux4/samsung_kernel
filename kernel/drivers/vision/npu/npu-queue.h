/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_QUEUE_H_
#define _NPU_QUEUE_H_

#include <linux/types.h>
#include <linux/mutex.h>

#include "npu-memory.h"

#define NPU_MAX_QUEUE (32)

enum npu_queue_state {
	NPU_QUEUE_STATE_PREPARE = 1,
	NPU_QUEUE_STATE_UNPREPARE,
	NPU_QUEUE_STATE_QUEUED,
	NPU_QUEUE_STATE_DEQUEUED,
	NPU_QUEUE_STATE_ALLOC,
	NPU_QUEUE_STATE_QUEUED_CANCEL,
	NPU_QUEUE_STATE_DONE,
};

struct nq_roi {
	__u32			x;
	__u32			y;
	__u32			w;
	__u32			h;
};

struct nq_buffer {
	union {
		struct nq_roi		roi;
		__u32			offset;
	}roi;
	union {
		unsigned long	userptr;
		__s32	fd;
	} m;
	u32 reserved;
	void	*vaddr;
	size_t	size;
	dma_addr_t	base_daddr;
	dma_addr_t	daddr;
	struct dma_buf	*dma_buf;
	struct sg_table	*sgt;
	struct dma_buf_attachment	*attachment;
};

struct nq_container {
	u32				count;
	struct nq_buffer		*buffers;
};

struct npu_queue_list {
	u32	id;
	u32	index;
	u32	count;
	u32 b_count;
	u32 direction;
	unsigned long flags;
	unsigned long	state;
	wait_queue_head_t done_wq;
	struct nq_container *containers;
	struct nq_buffer *buffer_pool;
};

struct npu_queue {
	size_t	insize[NPU_FRAME_MAX_CONTAINERLIST];
	size_t	otsize[NPU_FRAME_MAX_CONTAINERLIST];
	struct npu_queue_list	inqueue[NPU_MAX_QUEUE];
	struct npu_queue_list	otqueue[NPU_MAX_QUEUE];
};

int npu_queue_open(struct npu_queue_list *queue);
int npu_queue_start(struct npu_queue *queue);
int npu_queue_stop(struct npu_queue *queue);
int npu_queue_poll(struct npu_queue *queue, struct file *file, poll_table *poll);
void npu_queue_close(struct npu_queue *queue, struct npu_queue_list *queue_list);
int npu_queue_qbuf(struct npu_queue *queue, struct vs4l_container_bundle *cbundle);
int npu_queue_dqbuf(struct npu_session *session, struct npu_queue *queue, struct vs4l_container_bundle *cbundle, bool nonblocking);
int npu_queue_prepare(struct npu_queue *queue, struct vs4l_container_bundle *cbundle);
int npu_queue_unprepare(struct npu_queue *queue, struct vs4l_container_bundle *cbundle);
void npu_queue_done(struct npu_queue *queue, struct npu_queue_list *incl, struct npu_queue_list *otcl, unsigned long flags);
int npu_queue_qbuf_cancel(struct npu_queue *queue, struct vs4l_container_bundle *cbundle);

#endif
