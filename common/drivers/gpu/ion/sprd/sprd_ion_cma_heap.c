/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/spinlock.h>

#include <linux/err.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "../ion_priv.h"

#include <asm/mach/map.h>
#include <video/ion_sprd.h>
#include <linux/device.h>
#include <linux/dma-contiguous.h>
#include <linux/genalloc.h>

#ifdef CONFIG_ION_DEBUG
#ifndef DEBUG
#define DEBUG
#endif
#endif

#define ION_CMA_ALLOCATE_FAIL -1

struct device *ion_dev;

struct ion_cma_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t poolbase;
	size_t               poolsize;
};

ion_phys_addr_t ion_cma_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align)
{
	struct ion_cma_heap *cma_heap =
		container_of(heap, struct ion_cma_heap, heap);
	if(cma_heap->pool == NULL)
	{
		struct page *page;
		ion_phys_addr_t phys;
		int pagecount = ((PAGE_ALIGN(size)) >> PAGE_SHIFT);
		struct timeval val;
		int start, end;
		do_gettimeofday(&val);
		start = (val.tv_sec * 1000000 + val.tv_usec) / 1000;
		page = dma_alloc_from_contiguous(ion_dev, pagecount, get_order(size));
		do_gettimeofday(&val);
		end = (val.tv_sec * 1000000 + val.tv_usec) / 1000;
		if(!page) {
			pr_err("ion: malloc dma_alloc_from_contiguous() failed size:0x%lx , pageCount:%d\n" , size , pagecount);
			return ION_CMA_ALLOCATE_FAIL;
		}
		phys = page_to_phys(page);
		printk("ion: malloc from cma mem: size=%08lx,phy addr=%08lx, time=%dms\n", size, phys, end-start);
		return phys;
	} else {
		unsigned long offset = gen_pool_alloc(cma_heap->pool, size);
		pr_debug("ion: malloc from reserved mem: size=%08lx, pool=%p, offset=%08lx \n", size, cma_heap->pool, offset);
		if (!offset)
			return ION_CARVEOUT_ALLOCATE_FAIL;
		return offset;
	}
}

void ion_cma_free(struct ion_heap *heap, ion_phys_addr_t addr,
		       unsigned long size)
{
	struct ion_cma_heap *cma_heap =
		container_of(heap, struct ion_cma_heap, heap);

	if (addr == ION_CMA_ALLOCATE_FAIL)
		return;
	/*free reserved memory*/
	if((cma_heap->pool != NULL) && ((addr < (cma_heap->poolbase +cma_heap->poolsize)) && (addr >= cma_heap->poolbase)))
	{
		pr_debug("ion: free reserved mem: size=%08lx, pool=%p, base:%08lx , phyaddr=%08lx \n", size, cma_heap->pool, cma_heap->poolbase , addr);
		gen_pool_free(cma_heap->pool, addr, size);
	}
	else
	{
		struct page *page;
		int pagecount = ((PAGE_ALIGN(size)) >> PAGE_SHIFT);
		page = phys_to_page(addr);
		pr_debug("ion: free cma mem: size=%08lx, phyAddr=%08lx \n", size, addr);
		dma_release_from_contiguous(ion_dev, page, pagecount);
	}
	return;
}

static int ion_cma_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  ion_phys_addr_t *addr, size_t *len)
{
	*addr = buffer->priv_phys;
	*len = buffer->size;
	return 0;
}

static int ion_cma_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	buffer->priv_phys = ion_cma_allocate(heap, size, align);
	pr_debug("pgprot_noncached flags 0x%lx\n",flags);
	if(flags&(1<<31))
		buffer->flags |= (1<<31);
	else
		buffer->flags &= (~(1<<31));
	buffer->flags |= (flags & 0x7FFF0000);/*for debug*/
	return buffer->priv_phys == ION_CMA_ALLOCATE_FAIL ? -ENOMEM : 0;
}

static void ion_cma_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;

	ion_cma_free(heap, buffer->priv_phys, buffer->size);
	buffer->priv_phys = ION_CMA_ALLOCATE_FAIL;
}

struct sg_table *ion_cma_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{
	struct sg_table *table;
	int ret;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}
	sg_set_page(table->sgl, phys_to_page(buffer->priv_phys), buffer->size,
		    0);
	return table;
}

void ion_cma_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
	buffer->sg_table = NULL;
}

void *ion_cma_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
	int mtype = MT_MEMORY_NONCACHED;

	if (buffer->flags & ION_FLAG_CACHED)
		mtype = MT_MEMORY;

	return __arm_ioremap(buffer->priv_phys, buffer->size,
			      mtype);
}

void ion_cma_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	__arm_iounmap(buffer->vaddr);
	buffer->vaddr = NULL;
	return;
}

int ion_cma_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			       struct vm_area_struct *vma)
{
	if((buffer->flags & (1<<31)) )
	{
		pr_debug("pgprot_cached buffer->flags 0x%lx\n",buffer->flags);
		return remap_pfn_range(vma, vma->vm_start,
			       __phys_to_pfn(buffer->priv_phys) + vma->vm_pgoff,
			       buffer->size,
			       (vma->vm_page_prot));

	}
	else
	{
		pr_debug("pgprot_noncached buffer->flags 0x%lx\n",buffer->flags);
		return remap_pfn_range(vma, vma->vm_start,
			       __phys_to_pfn(buffer->priv_phys) + vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       pgprot_noncached(vma->vm_page_prot));

	}
}

static struct ion_heap_ops cma_heap_ops = {
	.allocate = ion_cma_heap_allocate,
	.free = ion_cma_heap_free,
	.map_dma = ion_cma_heap_map_dma,
	.unmap_dma = ion_cma_heap_unmap_dma,
	.phys = ion_cma_heap_phys,
	.map_user = ion_cma_heap_map_user,
	.map_kernel = ion_cma_heap_map_kernel,
	.unmap_kernel = ion_cma_heap_unmap_kernel,
};

struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *heap_data, struct device *dev)
{
	struct ion_cma_heap *cma_heap;
	cma_heap = kzalloc(sizeof(struct ion_cma_heap), GFP_KERNEL);
	if (!cma_heap)
		return ERR_PTR(-ENOMEM);
	if(heap_data->size != 0)
	{
		cma_heap->pool = gen_pool_create(12 , -1);
		if (!cma_heap->pool) {
			kfree(cma_heap);
			return ERR_PTR(-ENOMEM);
		}
		cma_heap->poolbase = heap_data->base;
		gen_pool_add(cma_heap->pool, cma_heap->poolbase, heap_data->size,
				     -1);
		cma_heap->poolsize = heap_data->size;
	}
	else
	{
		cma_heap->pool = NULL;
		cma_heap->poolbase = 0;
		cma_heap->poolsize = 0;
	}
	cma_heap->heap.ops = &cma_heap_ops;
	cma_heap->heap.type = ION_HEAP_TYPE_CUSTOM;
	ion_dev = dev;
	return &cma_heap->heap;
}

void ion_cma_heap_destroy(struct ion_heap *heap)
{
	struct ion_cma_heap *cma_heap =
		container_of(heap, struct  ion_cma_heap, heap);
	if(cma_heap->pool)
	{
		gen_pool_destroy(cma_heap->pool);
	}

	kfree(cma_heap);
	cma_heap = NULL;
}
