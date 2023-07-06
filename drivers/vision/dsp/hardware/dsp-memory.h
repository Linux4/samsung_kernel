/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_MEMORY_H__
#define __DSP_MEMORY_H__

#include <linux/dma-buf.h>

#include "dsp-hw-memory.h"

#define DSP_PRIV_MEM_NAME_LEN		(16)

struct dsp_system;

struct dsp_buffer {
	int				fd;
	size_t				size;
	unsigned int			offset;
	bool				cached;
	struct list_head		list;

	struct dma_buf			*dbuf;
	size_t				dbuf_size;
	enum dma_data_direction		dir;
	struct dma_buf_attachment	*attach;
	struct sg_table			*sgt;
	dma_addr_t			iova;
	void				*kvaddr;
};

struct dsp_priv_mem {
	char				name[DSP_PRIV_MEM_NAME_LEN];
	size_t				size;
	size_t				min_size;
	size_t				max_size;
	size_t				used_size;
	long				flags;
	bool				kmap;
	bool				fixed_iova;
	bool				backup;

	struct dma_buf			*dbuf;
	size_t				dbuf_size;
	enum dma_data_direction		dir;
	struct dma_buf_attachment	*attach;
	struct sg_table			*sgt;
	dma_addr_t			iova;
	void				*kvaddr;
	void				*bac_kvaddr;
};

int dsp_memory_map_buffer(struct dsp_memory *mem, struct dsp_buffer *buf);
int dsp_memory_unmap_buffer(struct dsp_memory *mem, struct dsp_buffer *buf);
int dsp_memory_sync_for_device(struct dsp_memory *mem, struct dsp_buffer *buf);
int dsp_memory_sync_for_cpu(struct dsp_memory *mem, struct dsp_buffer *buf);

int dsp_memory_alloc(struct dsp_memory *mem, struct dsp_priv_mem *pmem);
void dsp_memory_free(struct dsp_memory *mem, struct dsp_priv_mem *pmem);
int dsp_memory_ion_alloc(struct dsp_memory *mem, struct dsp_priv_mem *pmem);
void dsp_memory_ion_free(struct dsp_memory *mem, struct dsp_priv_mem *pmem);

int dsp_memory_open(struct dsp_memory *mem);
int dsp_memory_close(struct dsp_memory *mem);
int dsp_memory_probe(struct dsp_system *sys);
void dsp_memory_remove(struct dsp_memory *mem);

#endif
