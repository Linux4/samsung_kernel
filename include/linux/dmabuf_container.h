/*
 * Copyright (c) 2017,2018 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DMABUF_CONTAINER_H__
#define __DMABUF_CONTAINER_H__

#include <linux/dma-buf.h>

#ifdef CONFIG_DMABUF_CONTAINER
bool is_dmabuf_container(struct dma_buf *dmabuf);
int dmabuf_container_get_count(struct dma_buf *dmabuf);
struct dma_buf *dmabuf_container_get_buffer(struct dma_buf *dmabuf, int index);
struct dma_buf *dma_buf_get_any(int fd);
#else
static inline bool is_dmabuf_container(struct dma_buf *dmabuf)
{
	return false;
}
static inline int dmabuf_container_get_count(struct dma_buf *dmabuf)
{
	return 0;
}
static inline struct dma_buf *dmabuf_container_get_buffer(struct dma_buf *dbuf,
							  int index)
{
	return NULL;
}
static inline struct dma_buf *dma_buf_get_any(int fd)
{
	return dma_buf_get(fd);
}
#endif

#endif /* __DMABUF_CONTAINER_H__ */
