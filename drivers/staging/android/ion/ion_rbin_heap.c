/*
 * drivers/staging/android/ion/ion_rbin_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/rbinregion.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <asm/cacheflush.h>

#include "ion.h"
#include "ion_priv.h"
#include <trace/events/kmem.h>

#include "mtk/ion_mm_heap.h"

#define NUM_ORDERS ARRAY_SIZE(orders)

static const unsigned int orders[] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

static int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

struct ion_rbin_heap {
	struct ion_heap heap;
	unsigned long count;
	wait_queue_head_t waitqueue;
	struct task_struct *task;
	struct task_struct *task_shrink;
	bool task_run;
	bool shrink_run;
	struct ion_page_pool *pools[NUM_ORDERS];
};

static struct page *alloc_rbin_page(struct ion_rbin_heap *rbin_heap,
				    unsigned long size,
				    unsigned long last_size)
{
	struct page *page = ERR_PTR(-ENOMEM);
	phys_addr_t paddr = -ENOMEM;
	int order, i;

	trace_ion_rbin_partial_alloc_start(NULL, NULL, size, NULL);
	order = min(get_order(last_size), get_order(size));
	for (; order >= 0; order--) {
		size = min_t(unsigned long, size, PAGE_SIZE << order);
		paddr = ion_rbin_allocate(size);
		if (paddr == -ENOMEM)
			continue;
		if (paddr == -EBUSY)
			page = ERR_PTR(-EBUSY);
		break;
	}

	if (!IS_ERR_VALUE(paddr)) {
		page = phys_to_page(paddr);
		INIT_LIST_HEAD(&page->lru);
		memset(page_address(page), 0, size);
		set_page_private(page, size);
		ion_pages_sync_for_device(g_ion_device->dev.this_device,
					  page, size, DMA_BIDIRECTIONAL);
		for (i = 0; i < (1 << order); i++)
			SetPageIommu(&page[i]);
	}
	trace_ion_rbin_partial_alloc_end(NULL, NULL, size, page);
	return page;
}

static inline void do_expand(struct ion_rbin_heap *rbin_heap,
			     struct page *page, unsigned int nr_pages)
{
	unsigned int rem_nr_pages;
	unsigned int order;
	unsigned int total_nr_pages;
	unsigned int free_nr_page;
	struct page *free_page;
	struct ion_page_pool *pool;

	total_nr_pages = page_private(page) >> PAGE_SHIFT;
	rem_nr_pages = total_nr_pages - nr_pages;
	free_page = page + total_nr_pages;

	while (rem_nr_pages) {
		order = ilog2(rem_nr_pages);
		free_nr_page = 1 << order;
		free_page -= free_nr_page;
		set_page_private(free_page, free_nr_page << PAGE_SHIFT);
		pool = rbin_heap->pools[order_to_index(order)];
		ion_page_pool_free(pool, free_page);
		rem_nr_pages -= free_nr_page;
	}
	set_page_private(page, nr_pages << PAGE_SHIFT);
}

static struct page *alloc_rbin_page_from_pool(struct ion_rbin_heap *rbin_heap,
					      unsigned long size)
{
	struct page *page = NULL;
	unsigned int size_order;
	unsigned int nr_pages;
	int i;

	trace_ion_rbin_pool_alloc_start(rbin_heap->heap.name, NULL, size, NULL);
	size_order = get_order(size);
	nr_pages = size >> PAGE_SHIFT;

	/* try the same or higher order */
	for (i = NUM_ORDERS - 1; i >= 0; i--) {
		if (orders[i] < size_order)
			continue;
		page = ion_page_pool_only_alloc(rbin_heap->pools[i]);
		if (!page)
			continue;
		if (nr_pages < (1 << orders[i]))
			do_expand(rbin_heap, page, nr_pages);
		goto done;
	}

	/* try lower order */
	for (i = 0; i < NUM_ORDERS; i++) {
		if (orders[i] >= size_order)
			continue;
		page = ion_page_pool_only_alloc(rbin_heap->pools[i]);
		if (!page)
			continue;
		goto done;
	}
done:
	trace_ion_rbin_pool_alloc_end(rbin_heap->heap.name, NULL,
				      page ? page_private(page) : 0, page);
	if (page)
		atomic_sub(page_private(page) >> PAGE_SHIFT, &rbin_pool_pages);
	return page;
}

static int ion_rbin_heap_allocate(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  unsigned long size,
				  unsigned long align,
				  unsigned long flags)
{
	struct ion_rbin_heap *rbin_heap =
		container_of(heap, struct ion_rbin_heap, heap);
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp;
	unsigned long size_remain = PAGE_ALIGN(size);
	unsigned long last_size = size_remain;
	unsigned long nr_free;
	int i = 0;
	struct ion_mm_buffer_info *buffer_info = NULL;
	unsigned long user_va = 0;

	nr_free = rbin_heap->count - atomic_read(&rbin_allocated_pages);
	if (size_remain > nr_free << PAGE_SHIFT)
		return -ENOMEM;

	trace_ion_rbin_alloc_start(heap->name, buffer, size, NULL);
	INIT_LIST_HEAD(&pages);
	while (size_remain > 0) {
		if (atomic_read(&rbin_pool_pages)) {
			page = alloc_rbin_page_from_pool(rbin_heap,
							 size_remain);
			if (page)
				goto got_pg;
		}
		page = alloc_rbin_page(rbin_heap, size_remain, last_size);
		if (IS_ERR(page))
			goto free_pages;
		else
			last_size = page_private(page);
got_pg:
		list_add_tail(&page->lru, &pages);
		size_remain -= page_private(page);
		i++;
	}
	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		goto free_pages;
	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_table;

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp, &pages, lru) {
		sg_set_page(sg, page, page_private(page), 0);
		sg_dma_len(sg) = sg->length;
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	trace_ion_rbin_alloc_end(heap->name, buffer, size, NULL);
	atomic_add(size >> PAGE_SHIFT, &rbin_allocated_pages);

	/* create MM buffer info for it */
	buffer_info = kzalloc(sizeof(*buffer_info), GFP_KERNEL);
	if (IS_ERR_OR_NULL(buffer_info)) {
		IONMSG(" %s: Error. alloc ion_buffer failed.\n", __func__);
		goto free_table;
	}

	buffer->sg_table = table;
#ifdef CONFIG_MTK_IOMMU_V2
	buffer->sg_table_orig = table;
#endif
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	buffer_info->table_orig = table;
#endif
	buffer_info->VA = (void *)user_va;
	for (i = 0; i < DOMAIN_NUM; i++) {
		buffer_info->MVA[i] = 0;
		buffer_info->FIXED_MVA[i] = 0;
		buffer_info->iova_start[i] = 0;
		buffer_info->iova_end[i] = 0;
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		buffer_info->port[i] = -1;
#endif
	}
	buffer_info->module_id = -1;
	buffer_info->fix_module_id = -1;
	buffer_info->mva_cnt = 0;
	buffer_info->dbg_info.value1 = 0;
	buffer_info->dbg_info.value2 = 0;
	buffer_info->dbg_info.value3 = 0;
	buffer_info->dbg_info.value4 = 0;
	buffer_info->pid = buffer->pid;
	mutex_init(&buffer_info->lock);
	strncpy((buffer_info->dbg_info.dbg_name), "nothing",
		ION_MM_DBG_NAME_LEN);
	buffer->size = size;
	buffer->priv_virt = buffer_info;

	return 0;

free_table:
	sg_free_table(table);
	kfree(table);
free_pages:
	list_for_each_entry_safe(page, tmp, &pages, lru)
		ion_rbin_free(page_to_phys(page), page_private(page));
	trace_ion_rbin_alloc_end(heap->name, buffer, size, (void *)-1UL);
	return -ENOMEM;
}

static void ion_rbin_heap_free(struct ion_buffer *buffer)
{
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	struct ion_mm_buffer_info *buffer_info =
	    (struct ion_mm_buffer_info *)buffer->priv_virt;
	struct sg_table *table = buffer_info->table_orig;
#else
	struct sg_table *table = buffer->sg_table;
#endif
	struct scatterlist *sg;
	struct page *page;
	unsigned long size = buffer->size;
	int i;

	trace_ion_rbin_free_start(buffer->heap->name, buffer, size, NULL);
	ion_mm_heap_free_buffer_info(buffer);

	for_each_sg(table->sgl, sg, table->nents, i) {
		page = sg_page(sg);
		ion_rbin_free(page_to_phys(page), page_private(page));
	}
	sg_free_table(table);
	kfree(table);
	trace_ion_rbin_free_end(buffer->heap->name, buffer, size, NULL);
	atomic_sub(size >> PAGE_SHIFT, &rbin_allocated_pages);
}

static struct ion_heap_ops rbin_heap_ops = {
	.allocate = ion_rbin_heap_allocate,
	.free = ion_rbin_heap_free,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.phys = ion_mm_heap_phys,
#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
	.get_table = ion_mm_heap_get_table,
#endif
#ifdef MTK_ION_DMABUF_SUPPORT
	.dma_buf_config = ion_mm_heap_dma_buf_config,
#endif
};

static struct ion_rbin_heap *g_rbin_heap;

void wake_ion_rbin_heap_prereclaim(void)
{
	if (g_rbin_heap) {
		g_rbin_heap->task_run = 1;
		wake_up(&g_rbin_heap->waitqueue);
	}
}

void wake_ion_rbin_heap_shrink(void)
{
	if (g_rbin_heap) {
		g_rbin_heap->shrink_run = 1;
		wake_up(&g_rbin_heap->waitqueue);
	}
}

static void ion_rbin_heap_destroy_pools(struct ion_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (pools[i])
			ion_page_pool_destroy(pools[i]);
}

static int ion_rbin_heap_create_pools(struct ion_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		pools[i] = ion_page_pool_create(GFP_KERNEL, orders[i], false);
		if (!pools[i])
			goto err_create_pool;
	}
	return 0;

err_create_pool:
	ion_rbin_heap_destroy_pools(pools);
	return -ENOMEM;
}

static int ion_rbin_heap_prereclaim(void *data)
{
	struct ion_rbin_heap *rbin_heap = data;
	unsigned int order;
	unsigned long total_size;
	unsigned long size = PAGE_SIZE << orders[0];
	unsigned long last_size;
	struct ion_page_pool *pool;
	struct page *page;
	unsigned long jiffies_bstop;

	while (true) {
		wait_event_freezable(rbin_heap->waitqueue, rbin_heap->task_run);
		jiffies_bstop = jiffies + (HZ / 10);
		trace_printk("%s\n", "start");
		total_size = 0;
		last_size = size;
		while (true) {
			page = alloc_rbin_page(rbin_heap, size, last_size);
			if (PTR_ERR(page) == -ENOMEM)
				break;
			if (PTR_ERR(page) == -EBUSY) {
				if (time_is_after_jiffies(jiffies_bstop))
					continue;
				else
					break;
			}
			last_size = page_private(page);
			order = get_order(page_private(page));
			pool = rbin_heap->pools[order_to_index(order)];
			ion_page_pool_free(pool, page);
			total_size += page_private(page);
			atomic_add(1 << order, &rbin_pool_pages);
		}
		trace_printk("end %lu\n", total_size);
		rbin_heap->task_run = 0;
	}
	return 0;
}

static int ion_rbin_heap_shrink(void *data)
{
	struct ion_rbin_heap *rbin_heap = data;
	unsigned long total_size;
	unsigned long size = PAGE_SIZE << orders[0];
	struct page *page;

	while (true) {
		wait_event_freezable(rbin_heap->waitqueue, rbin_heap->shrink_run);
		trace_printk("start\n");
		total_size = 0;
		while (true) {
			page = alloc_rbin_page_from_pool(rbin_heap, size);
			if (!page)
				break;
			ion_rbin_free(page_to_phys(page), page_private(page));
			total_size += page_private(page);
		}
		trace_printk("%lu\n", total_size);
		rbin_heap->shrink_run = 0;
	}
	return 0;
}

struct ion_heap *ion_rbin_heap_create(struct ion_platform_heap *data)
{
	struct ion_rbin_heap *rbin_heap;

	if (!data->base || !data->size)
		return ERR_PTR(-EINVAL);

	rbin_heap = kzalloc(sizeof(struct ion_rbin_heap), GFP_KERNEL);
	if (!rbin_heap)
		return ERR_PTR(-ENOMEM);

	rbin_heap->heap.ops = &rbin_heap_ops;
	rbin_heap->heap.type = (enum ion_heap_type)ION_HEAP_TYPE_RBIN;
	rbin_heap->heap.name = kstrndup(data->name, MAX_HEAP_NAME - 1, GFP_KERNEL);
	rbin_heap->count = data->size >> PAGE_SHIFT;
	if (ion_rbin_heap_create_pools(rbin_heap->pools)) {
		kfree(rbin_heap);
		return ERR_PTR(-ENOMEM);
	}
	if (init_rbinregion(data->base, data->size)) {
		ion_rbin_heap_destroy_pools(rbin_heap->pools);
		kfree(rbin_heap);
		return ERR_PTR(-ENOMEM);
	}
	init_waitqueue_head(&rbin_heap->waitqueue);
	rbin_heap->task = kthread_run(ion_rbin_heap_prereclaim, rbin_heap, "rbin");
	rbin_heap->task_shrink = kthread_run(ion_rbin_heap_shrink, rbin_heap, "rbin_shrink");
	g_rbin_heap = rbin_heap;

	return &rbin_heap->heap;
}
