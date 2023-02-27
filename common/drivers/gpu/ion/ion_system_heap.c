/*
 * drivers/gpu/ion/ion_system_heap.c
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

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/list_sort.h>
#include "ion_priv.h"

static unsigned int high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO |
					    __GFP_NOWARN | __GFP_NORETRY |
					    __GFP_NOMEMALLOC | __GFP_NO_KSWAPD) &
					   ~__GFP_WAIT;
static unsigned int low_order_gfp_flags  = (GFP_HIGHUSER | __GFP_ZERO |
					 __GFP_NOWARN);
#define HIGH_PAGE_ORDER	 (4)
static const unsigned int orders[] = {8, 4, 0};
static const int num_orders = ARRAY_SIZE(orders);
static int order_to_index(unsigned int order)
{
	int i;
	for (i = 0; i < num_orders; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool **pools;
	struct list_head deferred;
	struct rt_mutex lock;
};

struct page_info {
	struct page *page;
	unsigned int order;
	struct list_head list;
	bool split_pages;
};

static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order)
{
	bool cached = ion_buffer_cached(buffer);
	struct ion_page_pool *pool = heap->pools[order_to_index(order)];
	struct page *page;

	if (!cached) {
		page = ion_page_pool_alloc(pool);
	} else {
		gfp_t gfp_flags = low_order_gfp_flags;

		if (order >= HIGH_PAGE_ORDER)
			gfp_flags = high_order_gfp_flags;
		page = ion_heap_alloc_pages(buffer, gfp_flags, order);
		if (!page)
			return 0;
		arm_dma_ops.sync_single_for_device(NULL,
			pfn_to_dma(NULL, page_to_pfn(page)),
			PAGE_SIZE << order, DMA_BIDIRECTIONAL);
	}
	if (!page)
		return 0;

	return page;
}

static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *page,
			     unsigned int order)
{
	bool cached = ion_buffer_cached(buffer);
	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	int i;

	if (!cached && !(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE)) {
		struct ion_page_pool *pool = heap->pools[order_to_index(order)];
		ion_page_pool_free(pool, page);
	} else if (split_pages) {
		for (i = 0; i < (1 << order); i++)
			__free_page(page + i);
	} else {
		__free_pages(page, order);
	}
}

static struct page_info *alloc_deferred_page(struct ion_system_heap *heap,
					     bool split_pages,
					     unsigned int order)
{
	struct page_info *found = NULL;
	struct page_info *info;

	rt_mutex_lock(&heap->lock);
	if (!(list_empty(&heap->deferred))) {
		struct page_info *saved = NULL;
		list_for_each_entry(info, &heap->deferred, list) {
			if (info->order == order) {
				if (split_pages == info->split_pages) {
					found = info;
					list_del(&found->list);
					break;
				}
				if (!saved && split_pages && !info->split_pages)
					saved = info;
			}
			if (info->order < order) {
				if (saved) {
					split_page(saved->page, saved->order);
					saved->split_pages = 1;
					found = saved;
					list_del(&found->list);
				}
				break;
			}
		}
	}
	rt_mutex_unlock(&heap->lock);

	/* either low_order_gfp_flags or high_order_gfp_flags has __GFP_ZERO */
	if (found) {
		int i;

		for (i = 0; i < (1 << found->order); i++)
			clear_highpage(found->page + i);
	}
	return found;
}

static struct page_info *alloc_largest_available(struct ion_system_heap *heap,
						 struct ion_buffer *buffer,
						 unsigned long size,
						 unsigned int max_order)
{
	struct page *page;
	struct page_info *info;
	int i;
	bool cached = ion_buffer_cached(buffer);
	bool split_pages = ion_buffer_fault_user_mappings(buffer);

	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		info = NULL;
		if (cached)
			info = alloc_deferred_page(heap, split_pages,
						   orders[i]);
		if (!info) {
			page = alloc_buffer_page(heap, buffer, orders[i]);
			if (!page)
				continue;

			info = kmalloc(sizeof(struct page_info), GFP_KERNEL);
			/*
			Prevent checking defect fix by SRCN Ju Huaiwei(huaiwei.ju) Aug.27th.2014
			http://10.252.250.112:7010	=> [CXX]Galaxy-Core-Prime IN INDIA OPEN
			CID 26180: Dereference null return value (NULL_RETURNS)
			15. dereference: Dereferencing a null pointer info.
			*/
			if (!info)
				return NULL;
			info->page = page;
			info->order = orders[i];
			info->split_pages = split_pages;
		}
		return info;
	}
	return NULL;
}

static int deferred_freepages_cmp(void *priv, struct list_head *a,
				  struct list_head *b)
{
	struct page_info *infoa, *infob;

	infoa = list_entry(a, struct page_info, list);
	infob = list_entry(b, struct page_info, list);

	if (infoa->order > infob->order)
		return 1;
	if (infoa->order < infob->order)
		return -1;
	return 0;
}

struct ion_system_buffer_info {
	struct list_head pages;
	struct sg_table table;
};

static int ion_system_heap_allocate(struct ion_heap *heap,
				     struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct ion_system_buffer_info *priv;
	struct scatterlist *sg;
	int ret;
	struct page_info *info, *tmp_info;
	int i = 0;
	long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];

	priv = kmalloc(sizeof(struct ion_system_buffer_info), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	INIT_LIST_HEAD(&priv->pages);
	while (size_remaining > 0) {
		info = alloc_largest_available(sys_heap, buffer, size_remaining, max_order);
		if (!info)
			goto err;
		list_add_tail(&info->list, &priv->pages);
		size_remaining -= (1 << info->order) * PAGE_SIZE;
		max_order = info->order;
		i++;
	}

	ret = sg_alloc_table(&priv->table, i, GFP_KERNEL);
	if (ret)
		goto err;

	sg = priv->table.sgl;
	list_for_each_entry(info, &priv->pages, list) {
		struct page *page = info->page;
		sg_set_page(sg, page, (1 << info->order) * PAGE_SIZE, 0);
		sg = sg_next(sg);
	}

	buffer->priv_virt = priv;
	return 0;
err:
	if (ion_buffer_cached(buffer)) {
		rt_mutex_lock(&sys_heap->lock);
		list_splice(&priv->pages, &sys_heap->deferred);
		list_sort(NULL, &sys_heap->deferred, deferred_freepages_cmp);
		rt_mutex_unlock(&sys_heap->lock);
	} else {
		list_for_each_entry_safe(info, tmp_info, &priv->pages, list) {
			free_buffer_page(sys_heap, buffer, info->page, info->order);
			kfree(info);
		}
	}
	kfree(priv);
	return -ENOMEM;
}

#define BAD_PAGE_WORKAROUND
void ion_system_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	bool cached = ion_buffer_cached(buffer);
	struct scatterlist *sg;
	int i;
	struct page_info *info, *tmp_info;
	struct ion_system_buffer_info *priv = buffer->priv_virt;
#ifdef BAD_PAGE_WORKAROUND
	unsigned int last_length = 0;
	struct page *wrong_page = NULL;
	static int sg_err_count = 0;
#endif

	/* uncached pages come from the page pools, zero them before returning
	   for security purposes (other allocations are zerod at alloc time */
	if (!cached && !(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE)) {
		ion_heap_buffer_zero(buffer);

#ifndef BAD_PAGE_WORKAROUND	/* FIXME: this is a temp workaround */
	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				get_order(sg_dma_len(sg)));
#else
	/* we assume there's only one wrong sg node in the list, with 0 length,
	 * and we assume its order is the smaller one of its two neighbours'
	 */
	for_each_sg(table->sgl, sg, table->nents, i) {
		if (sg_dma_len(sg) == 0) {
			wrong_page = sg_page(sg);
			sg_err_count++;
			printk(KERN_ERR \
			  "ION_SYSTEM_HEAP: found 0 length sg (%d)!!!\n",\
			   sg_err_count);
			continue;
		}
		if (wrong_page != NULL) {
			free_buffer_page(sys_heap, buffer, wrong_page,
				get_order((min(sg_dma_len(sg), last_length))));
			wrong_page = NULL;
		}
		last_length = sg_dma_len(sg);
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				get_order(sg_dma_len(sg)));
	}
	if (wrong_page != NULL) {
		free_buffer_page(sys_heap, buffer, wrong_page,
			get_order(last_length));
	}
#endif
	} else {
		rt_mutex_lock(&sys_heap->lock);
		list_splice_init(&priv->pages, &sys_heap->deferred);
		list_sort(NULL, &sys_heap->deferred, deferred_freepages_cmp);
		rt_mutex_unlock(&sys_heap->lock);
	}

	sg_free_table(table);
	if (!list_empty(&priv->pages)) {
		list_for_each_entry_safe(info, tmp_info, &priv->pages, list) {
			list_del(&info->list);
			kfree(info);
		}
	}
	kfree(priv);
}

struct sg_table *ion_system_heap_map_dma(struct ion_heap *heap,
					 struct ion_buffer *buffer)
{
	return &(((struct ion_system_buffer_info *) buffer->priv_virt)->table);
}

void ion_system_heap_unmap_dma(struct ion_heap *heap,
			       struct ion_buffer *buffer)
{
	return;
}

#if defined(CONFIG_SPRD_IOMMU)
int ion_system_heap_map_iommu(struct ion_buffer *buffer, int domain_num, unsigned long *ptr_iova)
{
	int ret=0;
	if (0 == buffer->iomap_cnt[domain_num]) {
		buffer->iova[domain_num] = sprd_iova_alloc(domain_num,buffer->size);
		ret = sprd_iova_map(domain_num, buffer->iova[domain_num], buffer);
	}
	*ptr_iova=buffer->iova[domain_num];
	buffer->iomap_cnt[domain_num]++;
	return ret;
}
int ion_system_heap_unmap_iommu(struct ion_buffer *buffer, int domain_num)
{
	int ret=0;
	if (buffer->iomap_cnt[domain_num] > 0) {
		buffer->iomap_cnt[domain_num]--;
		if (0 == buffer->iomap_cnt[domain_num]) {
			ret = sprd_iova_unmap(domain_num, buffer->iova[domain_num], buffer);
			sprd_iova_free(domain_num, buffer->iova[domain_num], buffer->size);
			buffer->iova[domain_num] = 0;
		}
	}
	return ret;
}
#endif

static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.map_dma = ion_system_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
#if defined(CONFIG_SPRD_IOMMU)
	.map_iommu = ion_system_heap_map_iommu,
	.unmap_iommu = ion_system_heap_unmap_iommu,
#endif
};

static int ion_deferred_list_shrink(struct ion_system_heap *heap,
				    gfp_t gfp_mask,
				    int nr_to_scan)
{
	int nr_freed = 0;
	int i, j;
	struct page_info *info;

	rt_mutex_lock(&heap->lock);
	if (nr_to_scan == 0) {
		list_for_each_entry(info, &heap->deferred, list)
			nr_freed += 1 << info->order;
	} else {
		for (i = 0; i < nr_to_scan; i++) {
			struct list_head *last = &heap->deferred;

			if (list_empty(last))
				break;
			last = last->prev;
			list_del(last);
			info = list_entry(last, struct page_info, list);
			nr_freed += 1 << info->order;

			if (info->split_pages) {
				for (j = 0; j < (1 << info->order); j++)
					__free_page(info->page + j);
			} else {
				__free_pages(info->page, info->order);
			}
			kfree(info);
		}
	}
	rt_mutex_unlock(&heap->lock);

	return nr_freed;
}

static int ion_system_heap_shrink(struct shrinker *shrinker,
				  struct shrink_control *sc) {

	struct ion_heap *heap = container_of(shrinker, struct ion_heap,
					     shrinker);
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int nr_total = 0;
	int nr_freed = 0;
	int i, nr_to_scan = sc->nr_to_scan;

	if (nr_to_scan == 0)
		goto end;

	/* shrink the free list first, no point in zeroing the memory if
	   we're just going to reclaim it */
	nr_freed += ion_heap_freelist_shrink(heap, -1, nr_to_scan * PAGE_SIZE) /
		PAGE_SIZE;

	if (nr_freed >= nr_to_scan)
		goto end;

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];

		int freed = ion_page_pool_shrink(pool, sc->gfp_mask,
						 nr_to_scan);
		nr_freed += freed;
		nr_to_scan -= freed;
		if (nr_to_scan <= 0)
			break;
	}
	if (nr_to_scan > 0)
		nr_freed += ion_deferred_list_shrink(sys_heap, sc->gfp_mask,
						     nr_to_scan);

end:
	/* total number of items is whatever the page pools are holding
	   plus whatever's in the freelist */
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		nr_total += ion_page_pool_shrink(pool, sc->gfp_mask, 0);
	}
	nr_total += ion_deferred_list_shrink(sys_heap, sc->gfp_mask, 0);
	nr_total += ion_heap_freelist_size(heap) / PAGE_SIZE;
	return nr_total;

}

static int ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{

	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];
		seq_printf(s, "%d order %u highmem pages in pool = %lu total\n",
			   pool->high_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in pool = %lu total\n",
			   pool->low_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->low_count);
	}
	return 0;
}

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_heap *heap;
	int i;

	heap = kzalloc(sizeof(struct ion_system_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	heap->pools = kzalloc(sizeof(struct ion_page_pool *) * num_orders,
			      GFP_KERNEL);
	if (!heap->pools)
		goto err_alloc_pools;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i] >= HIGH_PAGE_ORDER)
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_pool;
		heap->pools[i] = pool;
	}

	INIT_LIST_HEAD(&heap->deferred);
	rt_mutex_init(&heap->lock);
	heap->heap.shrinker.shrink = ion_system_heap_shrink;
	heap->heap.shrinker.seeks = DEFAULT_SEEKS;
	heap->heap.shrinker.batch = 0;
	register_shrinker(&heap->heap.shrinker);
	heap->heap.debug_show = ion_system_heap_debug_show;
	return &heap->heap;
err_create_pool:
	for (i = 0; i < num_orders; i++)
		if (heap->pools[i])
			ion_page_pool_destroy(heap->pools[i]);
	kfree(heap->pools);
err_alloc_pools:
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}

void ion_system_heap_destroy(struct ion_heap *heap)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;

	for (i = 0; i < num_orders; i++)
		ion_page_pool_destroy(sys_heap->pools[i]);
	kfree(sys_heap->pools);
	kfree(sys_heap);
}

static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	buffer->priv_virt = kzalloc(len, GFP_KERNEL);
	if (!buffer->priv_virt)
		return -ENOMEM;
	return 0;
}

void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	kfree(buffer->priv_virt);
}

static int ion_system_contig_heap_phys(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       ion_phys_addr_t *addr, size_t *len)
{
	*addr = virt_to_phys(buffer->priv_virt);
	*len = buffer->size;
	return 0;
}

struct sg_table *ion_system_contig_heap_map_dma(struct ion_heap *heap,
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
	sg_set_page(table->sgl, virt_to_page(buffer->priv_virt), buffer->size,
		    0);
	return table;
}

void ion_system_contig_heap_unmap_dma(struct ion_heap *heap,
				      struct ion_buffer *buffer)
{
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

int ion_system_contig_heap_map_user(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    struct vm_area_struct *vma)
{
	unsigned long pfn = __phys_to_pfn(virt_to_phys(buffer->priv_virt));
	return remap_pfn_range(vma, vma->vm_start, pfn + vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot);

}

static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
	.phys = ion_system_contig_heap_phys,
	.map_dma = ion_system_contig_heap_map_dma,
	.unmap_dma = ion_system_contig_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_system_contig_heap_map_user,
};

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	return heap;
}

void ion_system_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

