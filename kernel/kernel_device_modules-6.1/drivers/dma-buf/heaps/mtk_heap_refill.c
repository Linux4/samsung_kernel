// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 * It can record information such as dmabuf alloc time and pool size,
 * and then output it through cat /proc/dma_heap/monitor.
 *
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <uapi/linux/dma-heap.h>

#include "mtk_heap.h"
#include "mtk_heap_priv.h"
#include "mtk_page_pool.h"

static int pool_refill_check_memory_pages = (4 * SZ_1M / PAGE_SIZE);

#define  POOL_REMOVE_HIGH_PAGES	(totalram_pages() / 5)
#define  POOL_REMOVE_LOW_PAGES	(totalram_pages() / 6)

static int pool_recycle_high_water = -1; //600;
static int pool_recycle_low_water = -1; //300;

static unsigned int pool_refill_high_water[NUM_ORDERS] = {0, 100, 0};
static unsigned int pool_refill_low_water[NUM_ORDERS] = {40, 40, 0};

static unsigned long total_high_wmark_pages;

int mtk_refill_order(unsigned int order, int value)
{
	if (order >= NUM_ORDERS)
		return -1;

	if (value >= 0) {
		pool_refill_high_water[order] = value;
		pool_refill_low_water[order] = value * 4/10;
	}

	return pool_refill_high_water[order];
}

int mtk_pool_total_pages(struct mtk_dmabuf_page_pool **pools)
{
	int num_pages = 0, i;

	for (i = 0; i < NUM_ORDERS; i++)
		num_pages += mtk_dmabuf_page_pool_total(pools[i], true);

	return num_pages;
}

bool mtk_pools_above_recycle_high(struct mtk_dmabuf_page_pool **pools)
{
	if (pool_recycle_high_water < 0)
		pool_recycle_high_water = POOL_REMOVE_HIGH_PAGES;

	return pool_recycle_high_water && mtk_pool_total_pages(pools) >= pool_recycle_high_water;
}

bool mtk_pool_above_recycle_high(struct mtk_dmabuf_page_pool *pool)
{
	return mtk_pools_above_recycle_high(pool->pool_list);
}

bool mtk_pools_above_recycle_low(struct mtk_dmabuf_page_pool **pools)
{
	if (pool_recycle_low_water < 0)
		pool_recycle_low_water = POOL_REMOVE_LOW_PAGES;

	return mtk_pool_total_pages(pools) >= pool_recycle_low_water;
}

int mtk_pool_get_total_high_water(void)
{
	int i, total = 0;

	for (i = 0; i < NUM_ORDERS; i++)
		total += pool_refill_high_water[i];
	return total;
}

int mtk_pool_get_total_low_water(void)
{
	int i, total = 0;

	for (i = 0; i < NUM_ORDERS; i++)
		total += pool_refill_low_water[i];
	return total;
}

bool mtk_pool_above_refill_high(struct mtk_dmabuf_page_pool *pool)
{
	return mtk_dmabuf_page_pool_total(pool, true) >> (20 - PAGE_SHIFT) >=
			pool_refill_high_water[pool->order_index];
}

bool mtk_pool_above_refill_high_double(struct mtk_dmabuf_page_pool *pool)
{
	return mtk_dmabuf_page_pool_total(pool, true) >> (20 - PAGE_SHIFT) >=
			pool_refill_high_water[pool->order_index] * 2;
}

bool mtk_pool_below_refill_low(struct mtk_dmabuf_page_pool *pool)
{
	return (mtk_dmabuf_page_pool_total(pool, true) >> (20 - PAGE_SHIFT) <
		pool_refill_low_water[pool->order_index]);
}

static inline
struct page *mtk_pool_alloc_pages(gfp_t gfp, unsigned int order)
{
	if (fatal_signal_pending(current))
		return NULL;

	return alloc_pages(gfp, order);
}

static void mtk_pool_recycle_page(struct mtk_dmabuf_page_pool **pools)
{
	struct page *page = NULL;
	u32 page_count = 0;
	u64 tm1, tm2;
	int i;

	for (i = NUM_ORDERS - 1; i >= 0; i--) {
		tm1 = sched_clock();
		page_count = 0;
		while (mtk_pools_above_recycle_low(pools) &&
		       mtk_pool_above_refill_high_double(pools[i])) {
			page = mtk_dmabuf_page_pool_fetch(pools[i]);
			if (!page)
				break;

			page_count += 1 << pools[i]->order;
			__free_pages(page, pools[i]->order);
		}
		tm2 = sched_clock();

		if (page_count > 0)
			dmabuf_log_recycle(pools[i]->heap_tag, tm2-tm1,
				pools[i]->order, page_count,
				mtk_dmabuf_page_pool_total(pools[i], true));
	}
}

__no_kcsan static inline
void mtk_pool_set_refilling(struct mtk_dmabuf_page_pool **pools,
					       bool refilling)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		pools[i]->refill_time = sched_clock();
		pools[i]->refilling = refilling;
	}
}

unsigned long total_high_wmark_pages_get(void)
{
	if (total_high_wmark_pages == 0) {
		unsigned long total = 0;
		struct zone *zone;
		int i;

		for (i = 0; i < MAX_NR_ZONES; i++) {
			zone = &contig_page_data.node_zones[i];
			total += high_wmark_pages(zone);
		}
		total_high_wmark_pages = total;
	}

	return total_high_wmark_pages;
}

static inline
bool mtk_pool_memory_may_refill(struct mtk_dmabuf_page_pool *pool)
{
	return (global_zone_page_state(NR_FREE_PAGES) > total_high_wmark_pages);
}

static int mtk_pool_refill_order_high(struct mtk_dmabuf_page_pool *pool)
{
	gfp_t gfp_refill = (pool->gfp_mask | __GFP_RECLAIM) & ~__GFP_NORETRY;
	u32 refill_page_cnt = 0;
	struct page *page;
	bool may_refill = true;
	u64 tm1, tm2;
	int ret = 0;

	may_refill = mtk_pool_memory_may_refill(pool);
	if (!may_refill)
		return -1;

	if (pool->order_index == 0)
		gfp_refill = pool->gfp_mask;

	tm1 = sched_clock();
	while (!mtk_pool_above_refill_high(pool) && may_refill) {
		page = mtk_pool_alloc_pages(gfp_refill, pool->order);
		if (!page) {
			ret = -2;
			break;
		}

		mtk_dmabuf_page_pool_add(pool, page);
		refill_page_cnt += 1 << pool->order;

		may_refill = mtk_pool_memory_may_refill(pool);
	}
	tm2 = sched_clock();

	if (!may_refill)
		ret = -1;

	dmabuf_log_refill(pool->heap_tag, tm2-tm1, ret, pool->order,
			  refill_page_cnt,
			  mtk_dmabuf_page_pool_total(pool, true));

	return ret;
}

static int mtk_pool_refill_order0(struct mtk_dmabuf_page_pool *pool, bool normal)
{
	u32 refill_page_cnt = 0, refill_page_chk = 0;
	struct page *page;
	u64 tm1, tm2;
	int ret = 0;
	bool filling = true, may_refill = true;

	may_refill = mtk_pool_memory_may_refill(pool);
	if (!may_refill)
		return -1;

	if (normal)
		filling = !mtk_pool_above_refill_high(pool);
	else
		filling = (mtk_pool_total_pages(pool->pool_list) >>
					(20 - PAGE_SHIFT) <
					mtk_pool_get_total_high_water()) ||
					!mtk_pool_above_refill_high(pool);

	if (!filling)
		return 0;

	tm1 = sched_clock();
	while (filling && may_refill) {
		page = mtk_pool_alloc_pages(pool->gfp_mask, pool->order);
		if (!page) {
			ret = -2;
			break;
		}

		mtk_dmabuf_page_pool_add(pool, page);
		refill_page_cnt++;

		if (normal)
			filling = !mtk_pool_above_refill_high(pool);
		else
			filling = (mtk_pool_total_pages(pool->pool_list) >>
					(20 - PAGE_SHIFT) <
					mtk_pool_get_total_high_water()) ||
					!mtk_pool_above_refill_high(pool);

		if (refill_page_cnt - refill_page_chk >
		    pool_refill_check_memory_pages) {
			refill_page_chk = refill_page_cnt;
			may_refill = mtk_pool_memory_may_refill(pool);
		}
	}
	tm2 = sched_clock();

	if (!may_refill)
		ret = -1;

	dmabuf_log_refill(pool->heap_tag, tm2-tm1, ret, pool->order,
			  refill_page_cnt,
			  mtk_dmabuf_page_pool_total(pool, true));

	return ret;
}

static void mtk_pool_refill_pages(struct mtk_dmabuf_page_pool **pools)
{
	bool order0_normal = true;
	int i, ret;

	for (i = 0; i < NUM_ORDERS - 1; i++) {
		if (!mtk_pool_below_refill_low(pools[i]))
			continue;

		ret = mtk_pool_refill_order_high(pools[i]);
		if (ret < 0 && order0_normal)
			order0_normal = false;
	}

	if (pool_refill_low_water[NUM_ORDERS - 1] > 0)
		mtk_pool_refill_order0(pools[NUM_ORDERS - 1], order0_normal);
}

static int refill_dma_heap_pool(void *data)
{
	struct mtk_dmabuf_page_pool **pools = data;

	while (1) {
		if (unlikely(mtk_pools_above_recycle_high(pools))) {
			pools[0]->recycling = true;
			mtk_pool_recycle_page(pools);
			pools[0]->recycling = false;
		} else {
			mtk_pool_set_refilling(pools, true);
			mtk_pool_refill_pages(pools);
			mtk_pool_set_refilling(pools, false);
		}

		set_current_state(TASK_INTERRUPTIBLE);
		if (unlikely(kthread_should_stop())) {
			set_current_state(TASK_RUNNING);
			break;
		}
		schedule();
		set_current_state(TASK_RUNNING);
	}

	return 0;
}

void mtk_pool_refill_create(struct dma_heap *heap)
{
	struct sched_param param = { .sched_priority = 0 };
	struct mtk_heap_priv_info *heap_priv;
	struct task_struct *refill_kthread;
	int i;

	heap_priv = dma_heap_get_drvdata(heap);
	if (!heap_priv || !heap_priv->page_pools) {
		pr_info("%s %s no page pools\n", __func__, dma_heap_get_name(heap));
		return;
	}

	if (heap_priv->page_pools[0]->refill_kthread) {
		pr_info("%s %s the refill thread exists\n", __func__,
			dma_heap_get_name(heap));
		return;
	}

	refill_kthread = kthread_run(refill_dma_heap_pool, heap_priv->page_pools,
				     "%s_refill_thread", dma_heap_get_name(heap));
	if (IS_ERR(refill_kthread)) {
		pr_info("%s, creating thread fail\n", __func__);
		return;
	}

	sched_setscheduler(refill_kthread, SCHED_IDLE, &param);
	wake_up_process(refill_kthread);
	for (i = 0; i < NUM_ORDERS; i++) {
		heap_priv->page_pools[i]->need_refill = mtk_pool_below_refill_low;
		heap_priv->page_pools[i]->need_recycle = mtk_pool_above_recycle_high;
		heap_priv->page_pools[i]->refill_kthread = refill_kthread;
	}
	pr_info("%s %s success\n", __func__, dma_heap_get_name(heap));
}

static int mtk_heap_refill_enable_init(void)
{
	const char **heap_names = mtk_refill_heap_names();
	struct dma_heap *heap;
	int i = 0;

	total_high_wmark_pages_get();

	if (!heap_names)
		return 0;

	while (heap_names[i]) {
		heap = dma_heap_find(heap_names[i]);
		i++;

		if (!heap)
			continue;

		mtk_pool_refill_create(heap);
		dma_heap_put(heap);
	}

#if IS_ENABLED(CONFIG_MTK_IOMMU_DEBUG)
	mtk_register_refill_order_callback(mtk_refill_order);
#endif

	return 0;
}

module_init(mtk_heap_refill_enable_init);
MODULE_LICENSE("GPL");
