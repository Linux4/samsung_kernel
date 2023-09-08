/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include "ion_priv.h"
#include "ion_fb_heap.h"
#include "ion_drv_priv.h"
#include "mtk/ion_drv.h"
#include "mtk/mtk_ion.h"

#ifdef CONFIG_MTK_PSEUDO_M4U
#include <mach/pseudo_m4u.h>
#else
#include <m4u.h>
#endif

#define ION_DMA_RESERVED_ALLOCATE_FAIL	-1

/*fb heap base and size denamic access*/
struct ion_dma_reserved_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t phys_addr;
	void *cpu_addr;
	struct page *page;
	unsigned int mva;
	size_t size;
};

struct ion_dma_reserved_buffer_info {
	struct mutex lock;	/* lock for mva */
	int module_id;
	void *va;
	unsigned int mva;
	ion_phys_addr_t priv_phys;
};

static int ion_dma_reserved_heap_debug_show(struct ion_heap *heap,
					    struct seq_file *s, void *unused);

ion_phys_addr_t ion_dma_reserved_allocate(struct ion_heap *heap,
					  unsigned long size,
					  unsigned long align)
{
	struct ion_dma_reserved_heap
	*dma_reserved_heap = container_of(heap,
					  struct ion_dma_reserved_heap, heap);
	unsigned long offset = gen_pool_alloc(dma_reserved_heap->pool, size);

	if (!offset)
		return ION_DMA_RESERVED_ALLOCATE_FAIL;

	return offset;
}

void ion_dma_reserved_free(struct ion_heap *heap, ion_phys_addr_t addr,
			   unsigned long size)
{
	struct ion_dma_reserved_heap
	*dma_reserved_heap = container_of(heap,
					  struct ion_dma_reserved_heap, heap);

	if (addr == ION_DMA_RESERVED_ALLOCATE_FAIL)
		return;

	gen_pool_free(dma_reserved_heap->pool, addr, size);
}

int ion_dma_reserved_heap_iova(struct ion_heap *heap,
			       struct ion_buffer *buffer,
			       ion_phys_addr_t *addr, size_t *len)
{
	struct ion_dma_reserved_buffer_info *buffer_info =
		(struct ion_dma_reserved_buffer_info *)buffer->priv_virt;
	struct port_mva_info_t port_info;

	if (!buffer_info) {
		IONMSG("[%s]: Error. Invalid buffer.\n", __func__);
		return -EFAULT;
	}

	memset((void *)&port_info, 0, sizeof(port_info));
	port_info.emoduleid = buffer_info->module_id;
	port_info.buf_size = buffer->size;
	port_info.flags = 0;
	/*Allocate MVA*/
	mutex_lock(&buffer_info->lock);
	if (buffer_info->mva == 0) {
		int ret = m4u_alloc_mva_sg(&port_info, buffer->sg_table);

		if (ret < 0) {
			mutex_unlock(&buffer_info->lock);
			IONMSG("[dma_rsv]: Error.Allocate MVA failed.\n");
			return -EFAULT;
		}
		buffer_info->mva = port_info.mva;
		*addr = (ion_phys_addr_t)buffer_info->mva;
	} else {
		*addr = (ion_phys_addr_t)buffer_info->mva;
	}
	mutex_unlock(&buffer_info->lock);
	*len = buffer->size;

	return 0;
}

static int ion_dma_reserved_heap_phys(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      ion_phys_addr_t *addr, size_t *len)
{
	struct ion_dma_reserved_buffer_info *buffer_info =
		(struct ion_dma_reserved_buffer_info *)buffer->priv_virt;

	if (!buffer_info) {
		IONMSG("[%s]: Error. Invalid buffer.\n", __func__);
		return -EFAULT;
	}

	*addr = (ion_phys_addr_t)buffer_info->priv_phys;
	*len = buffer->size;

	return 0;
}

static int ion_dma_reserved_heap_allocate(struct ion_heap *heap,
					  struct ion_buffer *buffer,
					  unsigned long size,
					  unsigned long align,
					  unsigned long flags)
{
	struct ion_dma_reserved_buffer_info *buffer_info = NULL;
	struct sg_table *table;
	ion_phys_addr_t paddr;
	int ret;

	if (align > PAGE_SIZE)
		return -EINVAL;

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto err_free;

	paddr = ion_dma_reserved_allocate(heap, size, align);
	if (paddr == ION_DMA_RESERVED_ALLOCATE_FAIL) {
		IONMSG("%s alloc fail size=%ld, details are:====>\n",
		       __func__, size);
		dump_heap_info_to_log(heap, LOGLEVEL_ERR);
		ret = -ENOMEM;
		goto err_free_table;
	}

	/*create buffer info for it*/
	buffer_info = kzalloc(sizeof(*buffer_info), GFP_KERNEL);
	if (!buffer_info) {
		ret = -ENOMEM;
		goto err_free_gen_pool;
	}

	buffer_info->priv_phys = paddr;
	buffer_info->va = NULL;
	buffer_info->mva = 0;
	buffer_info->module_id = 0;
	mutex_init(&buffer_info->lock);
	buffer->priv_virt = buffer_info;

	sg_set_page(table->sgl, phys_to_page(buffer_info->priv_phys),
		    size, 0);
	buffer->sg_table = table;

	return 0;

err_free_gen_pool:
	ion_dma_reserved_free(heap, paddr, size);
err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_dma_reserved_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_dma_reserved_buffer_info *buffer_info =
		(struct ion_dma_reserved_buffer_info *)buffer->priv_virt;

	buffer->priv_virt = NULL;
	ion_dma_reserved_free(heap, buffer_info->priv_phys, buffer->size);

	mutex_lock(&buffer_info->lock);
	if (buffer_info->mva) {
		m4u_dealloc_mva_sg(buffer_info->module_id, buffer->sg_table,
				   buffer->size, buffer_info->mva);
	}
	mutex_unlock(&buffer_info->lock);

	buffer_info->priv_phys = ION_DMA_RESERVED_ALLOCATE_FAIL;
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	kfree(buffer_info);
}

static struct ion_heap_ops dma_reserved_heap_ops = {
		.allocate = ion_dma_reserved_heap_allocate,
		.free = ion_dma_reserved_heap_free,
		.phys = ion_dma_reserved_heap_phys,
		.map_user = ion_heap_map_user,
		.map_kernel = ion_heap_map_kernel,
		.unmap_kernel = ion_heap_unmap_kernel,
};

#define ION_DUMP(seq_files, fmt, args...) \
		do {\
			struct seq_file *file = (struct seq_file *)seq_files;\
			char *fmat = fmt;\
			if (file)\
				seq_printf(file, fmat, ##args);\
			else\
				pr_info(fmt, ##args);\
		} while (0)

static void ion_dma_reserved_chunk_show(struct gen_pool *pool,
					struct gen_pool_chunk *chunk,
					void *data)
{
	int order, nlongs, nbits, i;
	struct seq_file *s = (struct seq_file *)data;

	order = pool->min_alloc_order;
	nbits = (chunk->end_addr - chunk->start_addr) >> order;
	nlongs = BITS_TO_LONGS(nbits);

	ION_DUMP(s, "phys_addr=0x%x bits=", (unsigned int)chunk->phys_addr);

	for (i = 0; i < nlongs; i++)
		ION_DUMP(s, "0x%x ", (unsigned int)chunk->bits[i]);

	ION_DUMP(s, "\n");
}

static int ion_dma_reserved_heap_debug_show(struct ion_heap *heap,
					    struct seq_file *s,
					    void *unused)
{
	struct ion_dma_reserved_heap
	*dma_reserved_heap = container_of(heap,
					  struct ion_dma_reserved_heap, heap);
	size_t size_avail, total_size;

	total_size = gen_pool_size(dma_reserved_heap->pool);
	size_avail = gen_pool_avail(dma_reserved_heap->pool);

	ION_DUMP(s, "***************************************************\n");
	ION_DUMP(s, "total_size=0x%x, free=0x%x\n", (unsigned int)total_size,
		 (unsigned int)size_avail);
	ION_DUMP(s, "***************************************************\n");

	gen_pool_for_each_chunk(dma_reserved_heap->pool,
				ion_dma_reserved_chunk_show, s);
	return 0;
}

struct ion_heap *ion_dma_reserved_heap_create(struct ion_platform_heap
					      *heap_data)
{
	struct ion_dma_reserved_heap *dma_reserved_heap;
	int order = get_order(heap_data->size);
	struct page *page;
	size_t len;
	int i;

	IONMSG("%s size=%ld, order=%d\n", __func__, heap_data->size, order);

	dma_reserved_heap = kzalloc(sizeof(*dma_reserved_heap), GFP_KERNEL);
	if (!dma_reserved_heap)
		return ERR_PTR(-ENOMEM);

	dma_reserved_heap->pool = gen_pool_create(12, -1);
	if (!dma_reserved_heap->pool) {
		kfree(dma_reserved_heap);
		return ERR_PTR(-ENOMEM);
	}

	page = alloc_pages(GFP_DMA | __GFP_ZERO, order);
	if (!page) {
		gen_pool_destroy(dma_reserved_heap->pool);
		kfree(dma_reserved_heap);
		return ERR_PTR(-ENOMEM);
	}

	split_page(page, order);

	len = PAGE_ALIGN(heap_data->size);
	for (i = len >> PAGE_SHIFT; i < (1 << order); i++)
		__free_page(page + i);

	dma_reserved_heap->phys_addr = page_to_phys(page);
	dma_reserved_heap->size = heap_data->size;
	dma_reserved_heap->page = page;
	dma_reserved_heap->heap.ops = &dma_reserved_heap_ops;
	dma_reserved_heap->heap.type = (unsigned int)ION_HEAP_TYPE_DMA_RESERVED;
	dma_reserved_heap->heap.flags = 0;
	dma_reserved_heap->heap.debug_show = ion_dma_reserved_heap_debug_show;

	gen_pool_add(dma_reserved_heap->pool,
		     (unsigned long)dma_reserved_heap->phys_addr,
		     dma_reserved_heap->size, -1);
	return &dma_reserved_heap->heap;
}

void ion_dma_reserved_heap_destroy(struct ion_heap *heap)
{
	struct ion_dma_reserved_heap *dma_reserved_heap =
		container_of(heap, struct ion_dma_reserved_heap, heap);
	unsigned long pages, i;

	gen_pool_destroy(dma_reserved_heap->pool);

	pages = PAGE_ALIGN(dma_reserved_heap->size) >> PAGE_SHIFT;
	for (i = 0; i < pages; i++)
		__free_page(dma_reserved_heap->page + i);

	kfree(dma_reserved_heap);
	dma_reserved_heap = NULL;
}
