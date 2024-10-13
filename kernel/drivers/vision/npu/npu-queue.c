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

#include "npu-device.h"
#include "npu-system.h"
#include "npu-util-memdump.h"
#include "npu-queue.h"
#include "npu-vertex.h"
#include "npu-config.h"
#include "npu-session.h"
#include "npu-log.h"
#include "npu-hw-device.h"

/*
 * queue: npu_queue instance strucure
 * memory: Reference to memory management helper
 * lock: Session-wide mutex to protect access to vb_queue
 */

int npu_queue_unmap(struct npu_memory *memory, struct nq_buffer *buffer);
int npu_queue_flush(struct npu_queue *queue, struct npu_queue_list *queue_list);

int npu_queue_open(struct npu_queue_list *queue)
{
	int ret = 0, i = 0;

	for (i = 0; i < NPU_MAX_QUEUE; i++) {
		init_waitqueue_head(&queue[i].done_wq);
		queue[i].state = 0;
		set_bit(NPU_QUEUE_STATE_DEQUEUED, &queue[i].state);
	}

	return ret;
}

void npu_queue_close(struct npu_queue *queue, struct npu_queue_list *queue_list)
{
	int i = 0, j = 0, k = 0;
	struct nq_container *temp_container;
	struct nq_buffer *temp_buffer;
	struct nq_buffer *temp_buffer_pool;
	struct npu_vertex_ctx *vctx = container_of(queue, struct npu_vertex_ctx, queue);
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);

	if (!test_bit(NPU_QUEUE_STATE_UNPREPARE, &queue_list->state))
		npu_queue_flush(queue, queue_list);

	for (i = 0; i < NPU_MAX_QUEUE; i++)
		wake_up_all(&queue_list[i].done_wq);

	for (i = 0; i < NPU_MAX_QUEUE; i++) {
		if (!test_bit(NPU_QUEUE_STATE_ALLOC, &queue_list[i].state))
			continue;

		for (j = 0; j < queue_list[i].count; j++) {
			temp_buffer = queue_list[i].containers[j].buffers;
			if (temp_buffer)
				kfree(temp_buffer);
		}
		temp_container = queue_list[i].containers;
		if (temp_container)
			kfree(temp_container);

		for (k = 0; k < queue_list[i].b_count; k++) {
			temp_buffer_pool = &queue_list[i].buffer_pool[k];
			npu_queue_unmap(session->memory, temp_buffer_pool);
		}
		queue_list[i].b_count = 0;
		temp_buffer_pool = queue_list[i].buffer_pool;
		if (temp_buffer_pool)
			kfree(temp_buffer_pool);
	}

	clear_bit(NPU_QUEUE_STATE_ALLOC, &queue_list->state);
}

int npu_queue_start(struct npu_queue *queue)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	session->ss_state |= BIT(NPU_SESSION_STATE_START);

	return ret;
}

int npu_queue_stop(struct npu_queue *queue)
{
	int ret = 0;
	struct npu_session *session;
	struct npu_vertex_ctx *vctx;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	session->ss_state |= BIT(NPU_SESSION_STATE_STOP);

	return ret;
}

static void fill_vs4l_buffer(struct npu_queue_list *queue_list, struct vs4l_container_list *c, bool fill)
{
	struct nq_container *container;
	u32 i, j, k;

	c->flags &= ~(1 << VS4L_CL_FLAG_TIMESTAMP);
	c->flags &= ~(1 << VS4L_CL_FLAG_PREPARE);
	c->flags &= ~(1 << VS4L_CL_FLAG_INVALID);
	c->flags &= ~(1 << VS4L_CL_FLAG_DONE);
	c->flags &= ~(1 << VS4L_CL_FLAG_FW_TIMEOUT);
	c->flags &= ~(1 << VS4L_CL_FLAG_HW_TIMEOUT_RECOVERED);
	c->flags &= ~(1 << VS4L_CL_FLAG_HW_TIMEOUT_NOTRECOVERABLE);
	c->flags &= ~(1 << VS4L_CL_FLAG_CANCEL_DD);

	if (test_bit(NPU_QUEUE_STATE_PREPARE, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_PREPARE);

	if (test_bit(VS4L_CL_FLAG_INVALID, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_INVALID);

	if (test_bit(VS4L_CL_FLAG_DONE, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_DONE);

	if (test_bit(VS4L_CL_FLAG_FW_TIMEOUT, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_FW_TIMEOUT);

	if (test_bit(VS4L_CL_FLAG_HW_TIMEOUT_RECOVERED, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_HW_TIMEOUT_RECOVERED);

	if (test_bit(VS4L_CL_FLAG_HW_TIMEOUT_NOTRECOVERABLE, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_HW_TIMEOUT_NOTRECOVERABLE);

	if (test_bit(VS4L_CL_FLAG_CANCEL_DD, &queue_list->flags))
		c->flags |= (1 << VS4L_CL_FLAG_CANCEL_DD);

	if (!fill)
		return;

	/* sync buffers */
	for (i = 0; i < queue_list->count; ++i) {
		container = &queue_list->containers[i];
		k = container->count;
		for (j = 0; j < k; ++j)
			c->containers[i].buffers[j].reserved = container->buffers[j].reserved;
	}
}

static void dma_buf_sync(struct nq_buffer *buffers, u32 direction, u32 action)
{
	unsigned long flags;

	if (is_kpi_mode_enabled(true))
		local_irq_save(flags);

	if (action == NPU_FRAME_QBUF)
		dma_buf_end_cpu_access(buffers->dma_buf, direction);
	else // action == VISION_DQBUF
		dma_buf_begin_cpu_access(buffers->dma_buf, direction);

	if (is_kpi_mode_enabled(true))
		local_irq_restore(flags);
}

void npu_queue_buf_sync(struct npu_queue_list *queue_list, u32 action)
{
	int i = 0, j = 0, k = 0, direction = 0;
	struct nq_container	*container;

	if (queue_list->direction == VS4L_DIRECTION_OT)
		direction = DMA_FROM_DEVICE;
	else
		direction = DMA_TO_DEVICE;

	/* sync buffers */
	for (i = 0; i < queue_list->count; ++i) {
		container = &queue_list->containers[i];
		k = container->count;
		for (j = 0; j < k; ++j) {
			dma_buf_sync(&(container->buffers[j]), direction, action);
		}
	}
}

static dma_addr_t npu_dma_buf_dva_map(struct nq_buffer *buffer,
													__attribute__((unused))u32 size)
{
	return sg_dma_address(buffer->sgt->sgl);
}

static void npu_dma_buf_dva_unmap(
													__attribute__((unused))struct nq_buffer *buffer)
{
	return;
}

int npu_queue_unmap(struct npu_memory *memory, struct nq_buffer *buffer)
{
	int ret = 0;

	if (buffer == NULL) {
		npu_err("vb_buffer(buffer) is NULL\n");
		ret = -EFAULT;
		goto p_err;
	}

	if (buffer->vaddr)
		npu_dma_buf_vunmap(buffer->dma_buf, buffer->vaddr);
	if (buffer->daddr)
		npu_dma_buf_dva_unmap(buffer);
	if (buffer->sgt)
		dma_buf_unmap_attachment(
			buffer->attachment, buffer->sgt, DMA_BIDIRECTIONAL);
	if (buffer->attachment)
		dma_buf_detach(buffer->dma_buf, buffer->attachment);
	if (buffer->dma_buf) {
		dma_buf_put(buffer->dma_buf);
		npu_memory_update_dbg_info(memory, buffer->dma_buf);
	}

	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->base_daddr = 0;
	buffer->vaddr = NULL;
	buffer->size = 0;

	buffer->m.fd = 0;
	buffer->size = 0;

p_err:
	return ret;
}

int npu_queue_unmapping(struct npu_memory *memory, size_t	*size, struct npu_queue_list *queue_list)
{
	int ret = 0, i = 0, j = 0;
	struct nq_container	*q_container;
	struct nq_buffer		*q_buffer_pool;
	struct nq_buffer		*q_buffer;

	if (!test_bit(NPU_QUEUE_STATE_PREPARE, &queue_list->state))
		return ret;

	for (j = 0; j < queue_list->b_count; ++j) {
		q_buffer_pool = &queue_list->buffer_pool[j];
		ret = npu_queue_unmap(memory, q_buffer_pool);
		if (ret) {
			npu_err("__vb_qbuf_dmabuf is fail(%d)\n",ret);
			return ret;
		}
	}

	for (i = 0; i < queue_list->count; i++) {
		q_container = &queue_list->containers[i];
		for (j = 0; j < q_container->count; ++j) {
			q_buffer = &q_container->buffers[j];
			q_buffer->m.fd = 0;
		}
	}
	queue_list->b_count = 0;

	return ret;
}

int npu_queue_map(struct npu_queue_list *queue_list, struct npu_memory *memory, struct nq_buffer *buffer, size_t size, bool is_vmap_req)
{
	int ret = 0;
	bool complete_suc = false;

	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t base_daddr;
	void *vaddr;
	u32 direction;

	buffer->dma_buf = NULL;
	buffer->attachment = NULL;
	buffer->sgt = NULL;
	buffer->daddr = 0;
	buffer->vaddr = NULL;

	dma_buf = dma_buf_get(buffer->m.fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		npu_err("dma_buf_get is fail(%p)\n", dma_buf);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->dma_buf = dma_buf;

	if (buffer->dma_buf->size < size) {
		npu_err("Allocate buffer size(%zu) is smaller than expectation(%zu)\n",
			buffer->dma_buf->size, size);
		ret = -EINVAL;
		goto p_err;
	}
	buffer->size = size;

	attachment = dma_buf_attach(buffer->dma_buf, memory->dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		goto p_err;
	}
	buffer->attachment = attachment;

	npu_dma_buf_add_attr(attachment);

	if (queue_list->direction == VS4L_DIRECTION_OT)
		direction = DMA_FROM_DEVICE;
	else
		direction = DMA_TO_DEVICE;

	sgt = dma_buf_map_attachment(attachment, direction);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		goto p_err;
	}
	buffer->sgt = sgt;

	base_daddr = npu_dma_buf_dva_map(buffer, size);
	if (IS_ERR_VALUE(base_daddr)) {
		npu_err("Failed to allocate iova (err 0x%pK)\n", &base_daddr);
		ret = -ENOMEM;
		goto p_err;
	}
	buffer->base_daddr = base_daddr;
	buffer->daddr = base_daddr;

	/* Only DSP requires vaddr, hence condition check is added */
	if (is_vmap_req) {
		vaddr = npu_dma_buf_vmap(buffer->dma_buf);
		if (IS_ERR(vaddr)) {
			npu_err("Failed to get vaddr (err 0x%pK)\n", &vaddr);
			ret = -EFAULT;
			goto p_err;
		}
		buffer->vaddr = vaddr;
	}

	complete_suc = true;
	npu_memory_alloc_dbg_info(memory, buffer->dma_buf, buffer->size);

p_err:
	if (complete_suc != true)
		npu_queue_unmap(memory, buffer);
	return ret;
}

static void npu_queue_replacement(struct nq_buffer *src, struct nq_buffer *dest, struct vs4l_buffer *buffer)
{
	dest->m.fd = src->m.fd;
	dest->dma_buf = src->dma_buf;
	dest->attachment = src->attachment;
	dest->sgt = src->sgt;
	dest->vaddr = src->vaddr;
	dest->roi.offset = src->roi.offset = buffer->roi.offset;
}

int npu_queue_mapping(struct npu_memory *memory, size_t	*size, struct npu_queue_list *queue_list, struct vs4l_container_list *c, bool is_vmap_req)
{
	int ret = 0, i = 0, j = 0, k = 0, flag = 0;
	struct vs4l_container *container;
	struct vs4l_buffer *buffer;
	struct nq_container *q_container;
	struct nq_buffer *q_buffer;

	queue_list->id = c->id;
	queue_list->index = c->index;
	queue_list->count = c->count;
	queue_list->flags = c->flags;
	queue_list->direction = c->direction;

	for (i = 0; i < c->count; ++i) {
		container = &c->containers[i];
		q_container = &queue_list->containers[i];
		q_container->count = container->count;
		switch (container->memory) {
		case VS4L_MEMORY_DMABUF:
			for (j = 0; j < container->count; ++j) {
				buffer = &container->buffers[j];
				q_buffer = &q_container->buffers[j];

				if (q_buffer->m.fd != buffer->m.fd) {
					for (k = 0; k < queue_list->b_count; k++) {
						if (buffer->m.fd == queue_list->buffer_pool[k].m.fd) {
							flag = 1;
							break;
						}
					}

					if (flag) {
						npu_queue_replacement(&queue_list->buffer_pool[k], q_buffer, buffer);
					} else {
						if (queue_list->b_count > NPU_FRAME_MAX_CONTAINERLIST)
							return -ENOMEM;

						q_buffer->m.fd = buffer->m.fd;
						q_buffer->size = size[i];
						q_buffer->roi.offset = buffer->roi.offset;

						ret = npu_queue_map(queue_list, memory, q_buffer, size[i], is_vmap_req);
						if (ret) {
							npu_err("__vb_qbuf_dmabuf is fail(%d)\n", ret);
							return ret;
						}

						npu_queue_replacement(q_buffer, &queue_list->buffer_pool[k], buffer);
						queue_list->b_count++;
					}
				}
			}
			break;
		default:
			buffer = &container->buffers[j];
			npu_err("unsupported container memory type i %d, memory %d, fd %d\n",
					i, container->memory, buffer->m.fd);
			ret = -EINVAL;
			return ret;
		}
	}

	return ret;
}

int npu_queue_check(unsigned long state, struct vs4l_container_list *c)
{
	int ret = 0;
	int i;

	if (c->index >= NPU_FRAME_MAX_CONTAINERLIST) {
		npu_err("qbuf: invalid container list index\n");
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < c->count; i++) {
		if (c->containers[i].count > NPU_FRAME_MAX_BUFFER) {
			npu_err("qbuf: Max buffers are %d; passed %d buffers\n",
				NPU_FRAME_MAX_BUFFER, c->containers[i].count);
			ret = -EINVAL;
			goto p_err;
		}
	}

	if (!test_bit(NPU_QUEUE_STATE_DEQUEUED, &state)) {
		npu_err("buffer already in use\n");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static int npu_queue_update(struct npu_queue_list *queue_list, struct vs4l_container_list *c)
{
	int ret = 0, i = 0, j = 0;
	struct vs4l_container *container;
	struct vs4l_buffer *buffer;
	struct nq_container *q_container;
	struct nq_buffer *q_buffer;

	queue_list->id = c->id;
	queue_list->flags = c->flags;

	for (i = 0; i < c->count; ++i) {
		container = &c->containers[i];
		q_container = &queue_list->containers[i];
		q_container->count = container->count;
		switch (container->memory) {
		case VS4L_MEMORY_DMABUF:
			for (j = 0; j < container->count; ++j) {
				buffer = &container->buffers[j];
				q_buffer = &q_container->buffers[j];

				if (q_buffer->roi.offset != buffer->roi.offset)
					q_buffer->roi.offset = buffer->roi.offset;
			}
			break;
		default:
			buffer = &container->buffers[j];
			npu_err("unsupported container memory type i %d, memory %d, fd %d\n",
					i, container->memory, buffer->m.fd);
			ret = -EINVAL;
			return ret;
		}
	}

	return ret;
}

int npu_queue_qbuf_cancel(struct npu_queue *queue, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_queue_list *inqueue;
	struct npu_queue_list *otqueue;

	inqueue = &queue->inqueue[cbundle->m[0].clist->index];
	otqueue = &queue->otqueue[cbundle->m[1].clist->index];

	if (test_bit(VS4L_CL_FLAG_CANCEL_DD, &inqueue->state) || test_bit(VS4L_CL_FLAG_CANCEL_DD, &otqueue->state)
				|| !test_bit(NPU_QUEUE_STATE_QUEUED, &inqueue->state))  {
		set_bit(VS4L_CL_FLAG_CANCEL_DD, &inqueue->flags);
		set_bit(VS4L_CL_FLAG_CANCEL_DD, &otqueue->flags);
		goto p_err;
	}

	if ((inqueue->id != cbundle->m[0].clist->id) || (otqueue->id != cbundle->m[1].clist->id)) {
		ret = -EINVAL;
		goto p_err;
	}

	ret = npu_session_queue_cancel(queue, inqueue, otqueue);
	if (ret) {
		npu_err("npu_session_queue_cancel is fail(%d)\n", ret);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &otqueue->state);

	set_bit(VS4L_CL_FLAG_DONE, &inqueue->flags);
	set_bit(VS4L_CL_FLAG_DONE, &otqueue->flags);

p_err:
	fill_vs4l_buffer(inqueue, cbundle->m[0].clist, true);
	fill_vs4l_buffer(otqueue, cbundle->m[1].clist, true);
	return ret;
}

int npu_queue_qbuf(struct npu_queue *queue, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx = container_of(queue, struct npu_vertex_ctx, queue);
	struct npu_session *session = container_of(vctx, struct npu_session, vctx);
	struct npu_queue_list *inqueue;
	struct npu_queue_list *otqueue;

	npu_profile_record_start(PROFILE_DD_QUEUE, cbundle->m[0].clist->index, ktime_to_us(ktime_get_boottime()), 2, session->uid);

	inqueue = &queue->inqueue[cbundle->m[0].clist->index];
	otqueue = &queue->otqueue[cbundle->m[1].clist->index];

	if (test_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &inqueue->state)) {
		if (test_bit(NPU_QUEUE_STATE_DEQUEUED, &inqueue->state)) {
			clear_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &inqueue->state);
			clear_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &otqueue->state);
		} else {
			return -EAGAIN;
		}
	}

	if (!test_bit(NPU_QUEUE_STATE_PREPARE, &inqueue->state)) {
		ret = npu_queue_prepare(queue, cbundle);
		if (ret) {
			npu_ierr("vpu_queue_qbuf is fail(%d)\n", vctx, ret);
			goto p_err;
		}
	}

	ret = npu_queue_update(inqueue, cbundle->m[0].clist);
	if (ret) {
		npu_err("npu_queue_update(in) is fail(%d)\n", ret);
		goto p_err;
	}

	ret = npu_queue_update(otqueue, cbundle->m[1].clist);
	if (ret) {
		npu_err("npu_queue_update(out) is fail(%d)\n", ret);
		goto p_err;
	}

	npu_queue_buf_sync(inqueue, NPU_FRAME_QBUF);
	npu_queue_buf_sync(otqueue, NPU_FRAME_QBUF);

	ret = npu_session_update_iofm(queue, inqueue);
	if (ret) {
		npu_err("npu_session_update_iofm(in) is fail(%d)\n", ret);
		goto p_err;
	}

	ret = npu_session_update_iofm(queue, otqueue);
	if (ret) {
		npu_err("npu_session_update_iofm(out) is fail(%d)\n", ret);
		goto p_err;
	}
	npu_profile_record_end(PROFILE_DD_QUEUE, cbundle->m[0].clist->index, ktime_to_us(ktime_get_boottime()), 2, session->uid);

	npu_profile_record_start(PROFILE_DD_SESSION, cbundle->m[0].clist->index, ktime_to_us(ktime_get_boottime()), 2, session->uid);
	ret = npu_session_queue(queue, inqueue, otqueue);
	if (ret) {
		npu_err("fail(%d) in npu_session_queue\n", ret);
		goto p_err;
	}

#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
	fill_vs4l_buffer(otqueue, cbundle->m[1].clist, true);
#endif

	set_bit(NPU_QUEUE_STATE_QUEUED, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_QUEUED, &otqueue->state);

p_err:
	return ret;
}

int npu_queue_alloc(struct npu_queue_list *queue_list, struct vs4l_container_list *c)
{
	int ret = 0, i = 0;
	struct nq_container *temp_container;
	struct nq_buffer *temp_buffer;
	struct nq_buffer *temp_buffer_pool;

	if (test_bit(NPU_QUEUE_STATE_ALLOC, &queue_list->state))
		return ret;

	temp_container = kzalloc(c->count * sizeof(struct nq_container), GFP_KERNEL);
	if (temp_container == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	queue_list->containers = temp_container;
	for (i = 0; i < c->count; i++) {
		temp_buffer = kzalloc(c->containers[i].count * sizeof(struct nq_buffer), GFP_KERNEL);
		if (temp_buffer == NULL) {
			ret = -ENOMEM;
			return ret;
		}
		queue_list->containers[i].buffers = temp_buffer;
	}

	temp_buffer_pool = kzalloc(NPU_FRAME_MAX_CONTAINERLIST * sizeof(struct nq_buffer), GFP_KERNEL);
	if (temp_buffer_pool == NULL) {
		ret = -ENOMEM;
		return ret;
	}
	queue_list->buffer_pool = temp_buffer_pool;
	queue_list->b_count = 0;

	return ret;
}

int npu_queue_prepare(struct npu_queue *queue, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;
	struct npu_queue_list *inqueue;
	struct npu_queue_list *otqueue;
	bool is_vmap_req = true;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	inqueue = &queue->inqueue[cbundle->m[0].clist->index];
	otqueue = &queue->otqueue[cbundle->m[1].clist->index];

	inqueue->flags = cbundle->m[0].clist->flags;
	otqueue->flags = cbundle->m[1].clist->flags;

	if (!test_bit(NPU_QUEUE_STATE_DEQUEUED, &inqueue->state)) {
		npu_err("invalid in invb state(%lu)\n", inqueue->state);
		return -EAGAIN;
	}

	if (!test_bit(NPU_QUEUE_STATE_DEQUEUED, &otqueue->state)) {
		npu_err("invalid in otvb state(%lu)\n", otqueue->state);
		return -EAGAIN;
	}

	/* vaddr is not used for NPU, but DSP uses vaddr */
	is_vmap_req = ((session->hids & NPU_HWDEV_ID_DSP) ? true : false);

	ret = npu_queue_check(inqueue->state, cbundle->m[0].clist);
	if (ret) {
		npu_err("npu_queue_check(in) is fail(%d)\n", ret);
		goto p_err;
	}

	ret = npu_queue_check(otqueue->state, cbundle->m[1].clist);
	if (ret) {
		npu_err("npu_queue_check(out) is fail(%d)\n", ret);
		goto p_err;
	}

	ret = npu_queue_alloc(inqueue, cbundle->m[0].clist);
	if (ret) {
		npu_err("npu_queue_alloc(in) is fail(%d)\n", ret);
		goto p_err;
	}

	ret = npu_queue_alloc(otqueue, cbundle->m[1].clist);
	if (ret) {
		npu_err("npu_queue_alloc(out) is fail(%d)\n", ret);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_ALLOC, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_ALLOC, &otqueue->state);

	ret = npu_queue_mapping(session->memory, queue->insize, inqueue, cbundle->m[0].clist, is_vmap_req);
	if (ret) {
		npu_err("npu_queue_mapping(in) is fail(%d)\n", ret);
		goto p_err;
	}

	ret = npu_queue_mapping(session->memory, queue->otsize, otqueue, cbundle->m[1].clist, is_vmap_req);
	if (ret) {
		npu_err("npu_queue_mapping(out) is fail(%d)\n", ret);
		goto p_err;
	}

	ret = npu_session_prepare(queue, inqueue);
	if (ret) {
		npu_err("npu_session_prepare(in) is fail(%d)\n", ret);
		goto p_err;
	}
	ret = npu_session_prepare(queue, otqueue);
	if (ret) {
		npu_err("npu_session_prepare(out) is fail(%d)\n", ret);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_PREPARE, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_PREPARE, &otqueue->state);

p_err:
	return ret;
}

int npu_queue_flush(struct npu_queue *queue, struct npu_queue_list *queue_list)
{
	int ret = 0;
	struct npu_vertex_ctx *vctx;
	struct npu_session *session;

	vctx = container_of(queue, struct npu_vertex_ctx, queue);
	session = container_of(vctx, struct npu_session, vctx);

	ret = npu_queue_unmapping(session->memory, queue->insize, queue_list);
	if (ret) {
		npu_err("npu_queue_unmapping(in) is fail(%d)\n", ret);
		goto p_err;
	}

	ret = npu_session_unprepare(queue, queue_list);
	if (ret) {
		npu_err("npu_session_unprepare(out) is fail(%d)\n", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int npu_queue_unprepare(struct npu_queue *queue, struct vs4l_container_bundle *cbundle)
{
	int ret = 0;
	struct npu_queue_list *inqueue;
	struct npu_queue_list *otqueue;

	inqueue = &queue->inqueue[cbundle->m[0].clist->index];
	otqueue = &queue->otqueue[cbundle->m[1].clist->index];

	ret = npu_queue_flush(queue, inqueue);
	if (ret) {
		npu_err("npu_queue_flush(in) is fail(%d)\n", ret);
		goto p_err;
	}

	ret = npu_queue_flush(queue, otqueue);
	if (ret) {
		npu_err("npu_queue_flush(out) is fail(%d)\n", ret);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_UNPREPARE, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_UNPREPARE, &otqueue->state);
	clear_bit(NPU_QUEUE_STATE_PREPARE, &inqueue->state);
	clear_bit(NPU_QUEUE_STATE_PREPARE, &otqueue->state);

p_err:
	return ret;
}

bool check_done_for_me(struct npu_queue_list *queue)
{
	if (!test_bit(NPU_QUEUE_STATE_DONE, &queue->state))
		return false;

	return true;
}

int npu_queue_wait_for_done(struct npu_session *session, struct npu_queue_list *queue_list, int nonblocking)
{
	int ret = 0;
	struct npu_vertex *vertex;
	struct npu_device *device;

	vertex = session->vctx.vertex;
	device = container_of(vertex, struct npu_device, vertex);

	if (nonblocking) {
		npu_info("Nonblocking and no buffers to dequeue, will not wait\n");
		ret = -EWOULDBLOCK;
		goto p_err;
	}

	if (wait_event_timeout(queue_list->done_wq, check_done_for_me(queue_list), msecs_to_jiffies(10000)) == 0) {
		npu_uerr("timeout executed! [10 secs], index = %d, fid = %d\n", session, queue_list->index, queue_list->id);
		npu_util_dump_handle_nrespone(&device->system);
	}

p_err:
	return ret;
}

int npu_queue_dqbuf(struct npu_session *session, struct npu_queue *queue, struct vs4l_container_bundle *cbundle, bool nonblocking)
{
	int ret = 0;
	struct npu_queue_list *inqueue;
	struct npu_queue_list *otqueue;

	inqueue = &queue->inqueue[cbundle->m[0].clist->index];
	otqueue = &queue->otqueue[cbundle->m[1].clist->index];

	ret = npu_queue_wait_for_done(session, otqueue, nonblocking);
	if (ret) {
		if (ret != -EWOULDBLOCK)
			npu_err("npu_queue_wait_for_done is fail(%d)\n", ret);
		return ret;
	}

	if (!test_bit(NPU_QUEUE_STATE_DONE, &inqueue->state)) {
		npu_err("invalid in invb state(%lu)\n", inqueue->state);
	}

	if (!test_bit(NPU_QUEUE_STATE_DONE, &otqueue->state)) {
		npu_err("invalid in otvb state(%lu)\n", otqueue->state);
	}

	clear_bit(NPU_QUEUE_STATE_DONE, &inqueue->state);
	clear_bit(NPU_QUEUE_STATE_DONE, &otqueue->state);

	fill_vs4l_buffer(inqueue, cbundle->m[0].clist, false);
	fill_vs4l_buffer(otqueue, cbundle->m[1].clist, false);

	npu_queue_buf_sync(inqueue, NPU_FRAME_DQBUF);
	npu_queue_buf_sync(otqueue, NPU_FRAME_DQBUF);

	ret = npu_session_deque(queue);
	if (ret) {
		npu_err("fail(%d) in npu_session_deque\n", ret);
		goto p_err;
	}

	set_bit(NPU_QUEUE_STATE_DEQUEUED, &inqueue->state);
	set_bit(NPU_QUEUE_STATE_DEQUEUED, &otqueue->state);
	clear_bit(NPU_QUEUE_STATE_QUEUED, &inqueue->state);
	clear_bit(NPU_QUEUE_STATE_QUEUED, &otqueue->state);

	return ret;
p_err:
	return ret;
}

void npu_queue_done(struct npu_queue *queue,
	struct npu_queue_list *inqueue,
	struct npu_queue_list *otqueue,
	unsigned long flags)
{
	BUG_ON(!queue);
	BUG_ON(!inqueue);
	BUG_ON(!otqueue);

	inqueue->flags |= flags;
	otqueue->flags |= flags;

	if (test_bit(VS4L_CL_FLAG_ENABLE_FENCE, &inqueue->flags)) {
		set_bit(NPU_QUEUE_STATE_DEQUEUED, &inqueue->state);
		set_bit(NPU_QUEUE_STATE_DEQUEUED, &otqueue->state);
	} else {
		set_bit(NPU_QUEUE_STATE_DONE, &inqueue->state);
		set_bit(NPU_QUEUE_STATE_DONE, &otqueue->state);
		clear_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &inqueue->state);
		clear_bit(NPU_QUEUE_STATE_QUEUED_CANCEL, &otqueue->state);
		wake_up(&otqueue->done_wq);
	}
}
