/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2016 MediaTek Inc.
 */
#ifndef __CCCI_DPMAIF_PAGE_POOL_H__
#define __CCCI_DPMAIF_PAGE_POOL_H__

#ifdef RX_PAGE_POOL

#define POOL_NUMBER 2

extern unsigned int g_page_pool_is_on;
extern struct page_pool            *g_page_pool_arr[POOL_NUMBER];
extern atomic_t   g_alloc_from_kernel;
extern unsigned int sysctl_devalloc_threshold;
extern int skb_alloc_from_pool(struct sk_buff **ppskb, unsigned long long *p_base_addr,
	unsigned int pkt_buf_sz);
extern unsigned int ccci_dpmaif_page_pool_empty_index(void);
extern void ccci_dpmaif_create_page_pool(unsigned int index);
extern void ccci_dpmaif_cma_mem_alloc(unsigned int index);
extern void ccci_dpmaif_page_pool_init(void);
#endif
#endif

