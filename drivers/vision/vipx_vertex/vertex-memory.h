/*
 * Samsung Exynos5 SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_MEMORY_H__
#define __VERTEX_MEMORY_H__

#include <linux/dma-buf.h>

#include "vs4l.h"

/* TODO temp for test */
#define VERTEX_CC_DRAM_BIN_DVADDR		(0xB8000000)
#define VERTEX_CC_DRAM_BIN_SIZE			(SZ_4M)
#define VERTEX_DEBUG_SIZE			(SZ_16M)

#define VERTEX_PRIV_MEM_NAME_LEN		(30)

struct vertex_system;
struct vertex_memory;

struct vertex_buffer {
	unsigned char			addr_type;
	unsigned char			mem_attr;
	unsigned char			mem_type;
	unsigned char			reserved;
	size_t                          size;
	size_t				aligned_size;
	union {
		unsigned long           userptr;
		int                     fd;
	} m;

	struct dma_buf                  *dbuf;
	struct dma_buf_attachment       *attachment;
	struct sg_table                 *sgt;
	dma_addr_t                      dvaddr;
	void                            *kvaddr;

	struct vs4l_roi                 roi;
};

struct vertex_memory_ops {
	int (*map_dmabuf)(struct vertex_memory *mem,
			struct vertex_buffer *buf);
	int (*unmap_dmabuf)(struct vertex_memory *mem,
			struct vertex_buffer *buf);
	int (*map_userptr)(struct vertex_memory *mem,
			struct vertex_buffer *buf);
	int (*unmap_userptr)(struct vertex_memory *mem,
			struct vertex_buffer *buf);
};

struct vertex_priv_mem {
	char				name[VERTEX_PRIV_MEM_NAME_LEN];
	size_t				size;
	size_t				dbuf_size;
	long				flags;
	enum dma_data_direction		direction;
	bool				kmap;
	bool				fixed_dvaddr;

	void				*kvaddr;
	dma_addr_t			dvaddr;

	struct dma_buf			*dbuf;
	struct dma_buf_attachment	*attachment;
	struct sg_table			*sgt;
};

struct vertex_memory {
	struct device			*dev;
	struct iommu_domain		*domain;
	const struct vertex_memory_ops	*mops;

	struct vertex_priv_mem		fw;
	struct vertex_priv_mem		mbox;
	struct vertex_priv_mem		heap;
	struct vertex_priv_mem		debug;
};

dma_addr_t vertex_memory_allocate_heap(struct vertex_memory *mem,
		int id, size_t size);
void vertex_memory_free_heap(struct vertex_memory *mem);

int vertex_memory_open(struct vertex_memory *mem);
int vertex_memory_close(struct vertex_memory *mem);
int vertex_memory_probe(struct vertex_system *sys);
void vertex_memory_remove(struct vertex_memory *mem);

#endif
