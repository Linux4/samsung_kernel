/*
 * drivers/media/platform/exynos/mfc/mfc_meminfo.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mfc_meminfo.h"
#include "mfc_queue.h"

void __mfc_meminfo_add_buf(struct mfc_ctx *ctx, struct mfc_buf_queue *queue, struct vb2_buffer *vb)
{
	struct mfc_buf *buf = vb_to_mfc_buf(vb);
	struct mfc_mem *mfc_mem = NULL;
	unsigned long flags;
	int found = 0, plane;

	spin_lock_irqsave(&ctx->meminfo_queue_lock, flags);
	list_for_each_entry(mfc_mem, &queue->head, list) {
		if (mfc_mem->addr == buf->addr[0][0])
			found = 1;
	}
	spin_unlock_irqrestore(&ctx->meminfo_queue_lock, flags);

	if (!found) {
		mfc_mem = kzalloc(sizeof(struct mfc_mem), GFP_KERNEL);
		if (!mfc_mem)
			return;

		mfc_mem->addr = buf->addr[0][0];
		for (plane = 0; plane < vb->num_planes; ++plane) {
			mfc_mem->size += vb->planes[plane].length;
			mfc_ctx_debug(3, "plane[%d] size %u, mfc_mem size %zu\n", plane,
					vb->planes[plane].length, mfc_mem->size);
		}

		spin_lock_irqsave(&ctx->meminfo_queue_lock, flags);
		list_add_tail(&mfc_mem->list, &queue->head);
		queue->count++;
		spin_unlock_irqrestore(&ctx->meminfo_queue_lock, flags);
	}
}

void mfc_meminfo_add_inbuf(struct mfc_ctx *ctx, struct vb2_buffer *vb)
{
	struct mfc_buf_queue *queue  = &ctx->meminfo_inbuf_q;

	__mfc_meminfo_add_buf(ctx, queue, vb);
	mfc_ctx_debug(3, "[MEMINFO] input buffer count (%d)\n", queue->count);
}

void mfc_meminfo_add_outbuf(struct mfc_ctx *ctx, struct vb2_buffer *vb)
{
	struct mfc_buf_queue *queue  = &ctx->meminfo_outbuf_q;

	__mfc_meminfo_add_buf(ctx, queue, vb);
	mfc_ctx_debug(3, "[MEMINFO] output buffer count (%d)\n", queue->count);
}

void __mfc_meminfo_cleanup_queue(struct mfc_ctx *ctx, struct mfc_buf_queue *queue)
{
	struct mfc_mem *mfc_mem, *temp;
	unsigned long flags;

	mfc_ctx_debug(3, "[MEMINFO] clean up queue (%d)\n", queue->count);

	spin_lock_irqsave(&ctx->meminfo_queue_lock, flags);
	list_for_each_entry_safe(mfc_mem, temp, &queue->head, list) {
		list_del(&mfc_mem->list);
		kfree(mfc_mem);
	}

	mfc_init_queue(queue);
	spin_unlock_irqrestore(&ctx->meminfo_queue_lock, flags);
}

void mfc_meminfo_cleanup_inbuf_q(struct mfc_ctx *ctx)
{
	mfc_ctx_debug(3, "[MEMINFO] clean up inbuf_q\n");
	__mfc_meminfo_cleanup_queue(ctx, &ctx->meminfo_inbuf_q);
}

void mfc_meminfo_cleanup_outbuf_q(struct mfc_ctx *ctx)
{
	mfc_ctx_debug(3, "[MEMINFO] clean up outbuf_q\n");
	__mfc_meminfo_cleanup_queue(ctx, &ctx->meminfo_outbuf_q);
}

int mfc_meminfo_get_dev(struct mfc_dev *dev)
{
	struct mfc_ctx_buf_size *buf_size;
	int num = 0, i;

	dev->meminfo[num].type = MFC_MEMINFO_FW;
	dev->meminfo[num].name = "normal fw";
	dev->meminfo[num].count = 1;
	dev->meminfo[num].size = dev->variant->buf_size->firmware_code;
	dev->meminfo[num].total = dev->variant->buf_size->firmware_code;

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	++num;
	dev->meminfo[num].type = MFC_MEMINFO_FW;
	dev->meminfo[num].name = "secure fw";
	dev->meminfo[num].count = 1;
	dev->meminfo[num].size = dev->variant->buf_size->firmware_code;
	dev->meminfo[num].total = dev->variant->buf_size->firmware_code;
#endif

	++num;
	dev->meminfo[num].type = MFC_MEMINFO_INTERNAL;
	dev->meminfo[num].name = "common ctx";
	dev->meminfo[num].count = 1;
	buf_size = dev->variant->buf_size->ctx_buf;
	dev->meminfo[num].size = buf_size->dev_ctx;
	dev->meminfo[num].total = buf_size->dev_ctx;

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	++num;
	dev->meminfo[num].type = MFC_MEMINFO_INTERNAL;
	dev->meminfo[num].name = "secure common ctx";
	dev->meminfo[num].count = 1;
	buf_size = dev->variant->buf_size->ctx_buf;
	dev->meminfo[num].size = buf_size->dev_ctx;
	dev->meminfo[num].total = buf_size->dev_ctx;
#endif

	/*
	 * The dev total memory size is stored in dev->meminfo[6].
	 * If the number of dev->meminfo is over 6,
	 * MFC_MEMINFO_DEV_ALL should be changed.
	 */
	dev->meminfo[MFC_MEMINFO_DEV_ALL].total = 0;
	for (i = 0; i <= num; i++)
		dev->meminfo[MFC_MEMINFO_DEV_ALL].total += dev->meminfo[i].total;

	return ++num;
}
