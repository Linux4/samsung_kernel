// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/staging/android/ion/ion_mem_pool.c
 *
 * Copyright (C) 2011 Google, Inc.
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/sched/signal.h>

#include <asm/cacheflush.h>

#include "ion.h"

#ifdef CONFIG_HUGEPAGE_POOL
#include <linux/hugepage_pool.h>
#endif
static void *ion_page_pool_alloc_pages(struct ion_page_pool *pool, unsigned long flags)
{
	gfp_t gfpmask = pool->gfp_mask;
	struct page *page;

	if (fatal_signal_pending(current))
		return NULL;

	if (flags & ION_FLAG_NOZEROED)
		gfpmask &= ~__GFP_ZERO;

	if (!(flags & ION_FLAG_MAY_HWRENDER))
		gfpmask |= (__GFP_MOVABLE | __GFP_NOCMA);

#ifdef CONFIG_HUGEPAGE_POOL
	/* we assume that this path is only being used by system heap */
	if (pool->order == HUGEPAGE_ORDER)
		page = alloc_zeroed_hugepage(gfpmask, pool->order, true,
					     HPAGE_ION);
	else
		page = alloc_pages(gfpmask, pool->order);
#else
	page = alloc_pages(gfpmask, pool->order);
#endif
	if (!page) {
		if (pool->order == 0)
			perrfn("failed to alloc order-0 page (gfp %pGg)", &gfpmask);
		return NULL;
	}
	return page;
}

static void ion_page_pool_free_pages(struct ion_page_pool *pool,
				     struct page *page)
{
	__free_pages(page, pool->order);
}

static void ion_page_pool_add(struct ion_page_pool *pool, struct page *page)
{
	mutex_lock(&pool->mutex);
	if (zone_idx(page_zone(page)) == ZONE_MOVABLE) {
		list_add_tail(&page->lru, &pool->high_items);
		pool->high_count++;
	} else {
		list_add_tail(&page->lru, &pool->low_items);
		pool->low_count++;
	}

	mod_node_page_state(page_pgdat(page), NR_INDIRECTLY_RECLAIMABLE_BYTES,
			    (1 << (PAGE_SHIFT + pool->order)));
	mutex_unlock(&pool->mutex);
}

static struct page *ion_page_pool_remove(struct ion_page_pool *pool, bool high)
{
	struct page *page;

	if (high) {
		BUG_ON(!pool->high_count);
		page = list_first_entry(&pool->high_items, struct page, lru);
		pool->high_count--;
	} else {
		BUG_ON(!pool->low_count);
		page = list_first_entry(&pool->low_items, struct page, lru);
		pool->low_count--;
	}

	list_del(&page->lru);
	mod_node_page_state(page_pgdat(page), NR_INDIRECTLY_RECLAIMABLE_BYTES,
			    -(1 << (PAGE_SHIFT + pool->order)));
	return page;
}

struct page *ion_page_pool_only_alloc(struct ion_page_pool *pool)
{
	struct page *page = NULL;

	BUG_ON(!pool);

	if (!pool->high_count && !pool->low_count)
		goto done;

	if (mutex_trylock(&pool->mutex)) {
		if (pool->high_count)
			page = ion_page_pool_remove(pool, true);
		else if (pool->low_count)
			page = ion_page_pool_remove(pool, false);
		mutex_unlock(&pool->mutex);
	}
done:
	return page;
}

struct page *ion_page_pool_alloc(struct ion_page_pool *pool, unsigned long flags)
{
	struct page *page = NULL;

	BUG_ON(!pool);

	mutex_lock(&pool->mutex);
	if (pool->high_count && !(flags & ION_FLAG_MAY_HWRENDER))
		page = ion_page_pool_remove(pool, true);
	else if (pool->low_count)
		page = ion_page_pool_remove(pool, false);
	mutex_unlock(&pool->mutex);

	if (!page)
		return ion_page_pool_alloc_pages(pool, flags);

	return page;
}

void ion_page_pool_free(struct ion_page_pool *pool, struct page *page)
{
#ifndef CONFIG_ION_RBIN_HEAP
	/*
	 * ION RBIN heap can utilize ion_page_pool_free() for pages which are
	 * not compound pages. Thus, comment out the below line.
	 */
	BUG_ON(pool->order != compound_order(page));
#endif

	ion_page_pool_add(pool, page);
}

static int ion_page_pool_total(struct ion_page_pool *pool, bool high)
{
	int count = pool->low_count;

	if (high)
		count += pool->high_count;

	return count << pool->order;
}

int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
			 int nr_to_scan)
{
	int freed = 0;
	bool high;

	if (current_is_kswapd())
		high = true;
	else
		high = !!(gfp_mask & GFP_HIGHUSER_MOVABLE);

	if (nr_to_scan == 0)
		return ion_page_pool_total(pool, high);

	while (freed < nr_to_scan) {
		struct page *page;

		mutex_lock(&pool->mutex);
		if (pool->low_count) {
			page = ion_page_pool_remove(pool, false);
		} else if (high && pool->high_count) {
			page = ion_page_pool_remove(pool, true);
		} else {
			mutex_unlock(&pool->mutex);
			break;
		}
		mutex_unlock(&pool->mutex);
		ion_page_pool_free_pages(pool, page);
		freed += (1 << pool->order);
	}

	return freed;
}

struct ion_page_pool *ion_page_pool_create(gfp_t gfp_mask, unsigned int order)
{
	struct ion_page_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);

	if (!pool)
		return NULL;
	pool->high_count = 0;
	pool->low_count = 0;
	INIT_LIST_HEAD(&pool->low_items);
	INIT_LIST_HEAD(&pool->high_items);
	pool->gfp_mask = gfp_mask | __GFP_COMP;
	pool->order = order;
	mutex_init(&pool->mutex);
	plist_node_init(&pool->list, order);

	return pool;
}

void ion_page_pool_destroy(struct ion_page_pool *pool)
{
	kfree(pool);
}
