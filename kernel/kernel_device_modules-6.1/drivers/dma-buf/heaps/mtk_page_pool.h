/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 * DMA BUF PagePool implementation
 * Based on earlier ION code by Google
 *
 */

#ifndef _MTK_DMABUF_PAGE_POOL_H
#define _MTK_DMABUF_PAGE_POOL_H

#include <linux/device.h>
#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/shrinker.h>
#include <linux/types.h>
#include "mtk_heap_priv.h"

#define LOW_ORDER_GFP (GFP_HIGHUSER | __GFP_ZERO | __GFP_COMP)
#define HIGH_ORDER_GFP  (((GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN \
				| __GFP_NORETRY) & ~__GFP_RECLAIM) \
				| __GFP_COMP)
//static gfp_t order_flags[] = {HIGH_ORDER_GFP, LOW_ORDER_GFP | __GFP_NOWARN, LOW_ORDER_GFP};
static gfp_t order_flags[] = {HIGH_ORDER_GFP, HIGH_ORDER_GFP, LOW_ORDER_GFP};

/*
 * The selection of the orders used for allocation (1MB, 64K, 4K) is designed
 * to match with the sizes often found in IOMMUs. Using order 4 pages instead
 * of order 0 pages can significantly improve the performance of many IOMMUs
 * by reducing TLB pressure and time spent updating page tables.
 */
static unsigned int orders[] = {9, 4, 0};
#define NUM_ORDERS ARRAY_SIZE(orders)

/* page types we track in the pool */
enum {
	POOL_LOWPAGE,      /* Clean lowmem pages */
	POOL_HIGHPAGE,     /* Clean highmem pages */

	POOL_TYPE_SIZE,
};

/**
 * struct mtk_dmabuf_page_pool - pagepool struct
 * @count[]:		array of number of pages of that type in the pool
 * @items[]:		array of list of pages of the specific type
 * @mutex:		lock protecting this struct and especially the count
 *			item list
 * @gfp_mask:		gfp_mask to use from alloc
 * @order:		order of pages in the pool
 * @list:		list node for list of pools
 *
 * Allows you to keep a pool of pre allocated pages to use
 */
struct mtk_dmabuf_page_pool {
	int count[POOL_TYPE_SIZE];
	struct list_head items[POOL_TYPE_SIZE];
	struct mutex mutex;
	gfp_t gfp_mask;
	unsigned int order;
	struct list_head list;
	struct mtk_dmabuf_page_pool **pool_list;

	struct task_struct *refill_kthread;
	bool (*need_refill)(struct mtk_dmabuf_page_pool *pool);
	bool (*need_recycle)(struct mtk_dmabuf_page_pool *pool);

	int order_index;
	int heap_tag;
	bool refilling;
	bool recycling;
	u64 alloc_time;
	u64 refill_time;
	u64 shrink_time;
	u64 log_time;
};

struct mtk_dmabuf_page_pool *mtk_dmabuf_page_pool_create(gfp_t gfp_mask,
						     unsigned int order);
void mtk_dmabuf_page_pool_destroy(struct mtk_dmabuf_page_pool *pool);
struct page *mtk_dmabuf_page_pool_fetch(struct mtk_dmabuf_page_pool *pool);
struct page *mtk_dmabuf_page_pool_alloc(struct mtk_dmabuf_page_pool *pool);
void mtk_dmabuf_page_pool_add(struct mtk_dmabuf_page_pool *pool, struct page *page);
void mtk_dmabuf_page_pool_free(struct mtk_dmabuf_page_pool *pool, struct page *page);
int mtk_dmabuf_page_pool_init_shrinker(void);
int mtk_dmabuf_page_pool_total(struct mtk_dmabuf_page_pool *pool, bool high);
long mtk_dmabuf_page_pool_size(struct dma_heap *heap);

struct mtk_dmabuf_page_pool **dynamic_page_pools_create(unsigned int *orders,
							unsigned int num_orders);
void dynamic_page_pools_free(struct mtk_dmabuf_page_pool **pool_list, unsigned int num_orders);

int mtk_dmabuf_page_pool_do_shrink(struct mtk_dmabuf_page_pool *pool, gfp_t gfp_mask,
				   int nr_to_scan);

int mtk_cache_pool_init(void);

#endif /* _MTK_DMABUF_PAGE_POOL_H */
