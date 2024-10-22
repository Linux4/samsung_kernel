// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <linux/dma-mapping.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/errno.h>
#include <linux/dma-map-ops.h>
#ifdef RX_PAGE_POOL
#include <net/page_pool.h>
#endif
#include "ccci_debug.h"
#include "ccci_dpmaif_reg_com.h"
#include "ccci_dpmaif_page_pool.h"
#include "ccci_dpmaif_com.h"
#ifdef RX_PAGE_POOL

#define TAG "dpmaif_page_pool"


struct pool_type {
	struct page *pages;
	unsigned int pool_page_num;
};

#define MAX_POOL_SIZE 32768
#ifdef NET_SKBUFF_DATA_USES_OFFSET
  #define skb_data_size(x) ((x)->head + (x)->end - (x)->data)
#else
  #define skb_data_size(x) ((x)->end - (x)->data)
#endif


static struct reserved_mem *rmem;
static struct pool_type resv_skb_mem[POOL_NUMBER] = {0};
struct page_pool            *g_page_pool_arr[POOL_NUMBER] = {0};

unsigned int g_page_pool_is_on;

atomic_t   g_alloc_from_kernel;
unsigned int sysctl_devalloc_threshold = 2000;

/*
 * Register the sysctl to set threshold of allocing skb from kernel.
 */

static struct ctl_table devalloc_threshold_table[] = {
	{
		.procname	= "devalloc_threshold",
		.data		= &sysctl_devalloc_threshold,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
	},
	{}
};
static struct ctl_table devalloc_threshold_root[] = {
	{
		.procname	= "net",
		.mode		= 0555,
		.child		= devalloc_threshold_table,
	},
	{}
};

static struct ctl_table_header *sysctl_header;
static int register_devalloc_threshold_sysctl(void)
{
	sysctl_header = register_sysctl_table(devalloc_threshold_root);
	if (sysctl_header == NULL) {
		pr_info("CCCI:dpmaif:register devalloc_threshold failed\n");
		return -1;
	}
	return 0;
}

int skb_alloc_from_pool(
		struct sk_buff **ppskb,
		unsigned long long *p_base_addr,
		unsigned int pkt_buf_sz)
{
	struct page *page = NULL;
	int index_usable = 0;

	index_usable = ccci_dpmaif_page_pool_empty_index();
	if (index_usable != POOL_NUMBER)
		page = page_pool_alloc_pages(g_page_pool_arr[index_usable], GFP_ATOMIC);
	if (!page) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: alloc page fail!\n", __func__);
		return LOW_MEMORY_SKB;
	}

	page->pp = g_page_pool_arr[index_usable];
	page->pp_magic = PP_SIGNATURE;
	(*ppskb) = build_skb(page_to_virt(page), PAGE_SIZE);

	if (!(*ppskb)) {
		page_pool_recycle_direct(g_page_pool_arr[index_usable], page);
		CCCI_ERROR_LOG(-1, TAG,
				"[%s:%d] error: alloc skb fail!\n", __func__, __LINE__);
		return LOW_MEMORY_SKB;
	}

	skb_reserve(*ppskb, NET_SKB_PAD);
	skb_mark_for_recycle(*ppskb);
	(*p_base_addr) = dma_map_single(
		dpmaif_ctl->dev, (*ppskb)->data,
		1500, DMA_FROM_DEVICE);

	if (dma_mapping_error(dpmaif_ctl->dev, (*p_base_addr))) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: dma mapping fail: %ld!\n",
			__func__, skb_data_size(*ppskb));

		dev_kfree_skb_any(*ppskb);
		(*ppskb) = NULL;

		return DMA_MAPPING_ERR;
	}

	return 0;
}


static void dpmaif_put_page_pool(struct page_pool *pool, struct page *page)
{
	init_page_count(page);
	page->pp = pool;
	page->pp_magic = PP_SIGNATURE;
	page_pool_recycle_direct(pool, page);
	pool->pages_state_hold_cnt++;
}

static void dpmaif_allocmem_page_pool(unsigned int pool_size,  unsigned int index)
{
	struct page *page = NULL;
	unsigned int i = 0;

	for (; i < pool_size; i++) {
		page = virt_to_page(page_address(resv_skb_mem[index].pages) + PAGE_SIZE*i);
		if (!page) {
			CCCI_ERROR_LOG(0, TAG, "alloc page for page_pool:%d failed\n", i);
			continue;
		}
		dpmaif_put_page_pool(g_page_pool_arr[index], page);
	}
}

void ccci_dpmaif_create_page_pool(unsigned int index)
{
	struct page_pool_params pp = { 0 };
	struct page_pool *g_page_pool = NULL;

	if (resv_skb_mem[index].pages) {
		pp.max_len = PAGE_SIZE;
		pp.flags = 0;
		pp.pool_size = resv_skb_mem[index].pool_page_num;
		pp.nid = dev_to_node(dpmaif_ctl->dev);
		pp.dev = dpmaif_ctl->dev;
		pp.dma_dir = DMA_FROM_DEVICE;

		g_page_pool = page_pool_create(&pp);
		if (IS_ERR(g_page_pool)) {
			CCCI_ERROR_LOG(0, TAG,
			"[%s] error: create page pool fail.\n", __func__);
			g_page_pool_arr[index] = NULL;
			return;
		}
		g_page_pool_is_on = 1;
		g_page_pool_arr[index] = g_page_pool;
		dpmaif_allocmem_page_pool(resv_skb_mem[index].pool_page_num, index);
	}
}


void ccci_dpmaif_destroy_page_pool(struct page_pool *g_page_pool)
{
	page_pool_destroy(g_page_pool);
}

unsigned int ccci_dpmaif_page_pool_empty_index(void)
{
	unsigned int i = 0;

	for (; i < POOL_NUMBER; i++)
		if (g_page_pool_arr[i])
			if (g_page_pool_arr[i]->alloc.count ||
				!__ptr_ring_empty(&g_page_pool_arr[i]->ring))
				return i;
	return i;
}


static int dpmaif_cma_mem_init(void)
{
	struct device_node  *rmem_node;
	int ret;

	/*page pool for cma memory*/
	rmem_node = of_parse_phandle(dpmaif_ctl->dev->of_node, "memory-region", 0);
	if (!rmem_node) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: no node for page pool.\n", __func__);
		return -1;
	}
	rmem = of_reserved_mem_lookup(rmem_node);
	of_node_put(rmem_node);
	if (!rmem) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: cannot lookup cma mem,full name:%s,%s\n", __func__,
			rmem_node->full_name, kbasename(rmem_node->full_name));
		return -1;
	}
	ret = of_reserved_mem_device_init_by_idx(dpmaif_ctl->dev, dpmaif_ctl->dev->of_node, 0);
	if (ret) {
		CCCI_ERROR_LOG(0, TAG, "%s: of_reserved_mem_device_init failed(%d).",
			__func__, ret);
		return -1;
	}
	return 0;
}

void ccci_dpmaif_cma_mem_alloc(unsigned int index)
{
	struct page *pages;

	if (!dpmaif_ctl->dev->cma_area) {
		CCCI_ERROR_LOG(0, TAG, "[%s] cma_area == NULL\n", __func__);
		return;
	}

	resv_skb_mem[index].pool_page_num = min(rmem->size/PAGE_SIZE/POOL_NUMBER,
			(unsigned long long)MAX_POOL_SIZE);
	pages = cma_alloc(dpmaif_ctl->dev->cma_area, resv_skb_mem[index].pool_page_num,
			0, false);

	if (pages) {
		resv_skb_mem[index].pages = pages;
		CCCI_NORMAL_LOG(0, TAG,
			"[%s] resv_skb_mem[%d].pool_page_num:%d.reserv_base:%llx,\n",
			__func__, index, resv_skb_mem[index].pool_page_num,
			(unsigned long long)rmem->base);
	} else
		CCCI_ERROR_LOG(0, TAG,
		"[%s] error: alloc failed resv_skb_mem[index].pool_page_num:%d.\n",
		__func__, resv_skb_mem[index].pool_page_num);
}


void ccci_dpmaif_page_pool_init(void)
{
	if (!dpmaif_ctl->dev->of_node) {
		CCCI_ERROR_LOG(0, TAG, "dpmaif page pool driver of_node is null, exit\n");
		return;
	}

	if (dpmaif_cma_mem_init() == 0) {
		ccci_dpmaif_cma_mem_alloc(0);
		ccci_dpmaif_create_page_pool(0);
		if (g_page_pool_is_on) {
			atomic_set(&g_alloc_from_kernel, 0);
			register_devalloc_threshold_sysctl();
		}
	}
}
#endif
