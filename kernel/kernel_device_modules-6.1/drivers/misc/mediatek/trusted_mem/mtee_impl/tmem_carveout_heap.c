// SPDX-License-Identifier: GPL-2.0
/*
 * MTK carveout heap for partly continuous physical memory
 *
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/arm_ffa.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/proc_fs.h>

#include "public/mtee_regions.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "ssmr/memory_ssmr.h"

int tmem_register_ffa_module(void)
{
	return 0;
}
int tmem_query_ffa_handle_to_pa(u64 handle, uint64_t *phy_addr)
{
	pr_info("%s: trusted_mem.ko Not Support FF-A\n", __func__);
	return 0;
}
int tmem_ffa_region_alloc(enum MTEE_MCHUNKS_ID mchunk_id,
						  unsigned long size,
						  unsigned long alignment,
						  u64 *handle)
{
	pr_info("%s: trusted_mem.ko Not Support FF-A allocation\n", __func__);
	return 0;
}
int tmem_ffa_region_free(enum MTEE_MCHUNKS_ID mchunk_id, u64 handle)
{
	pr_info("%s: trusted_mem.ko Not Support FF-A free\n", __func__);
	return 0;
}
int tmem_ffa_page_alloc(enum MTEE_MCHUNKS_ID mchunk_id,
		struct sg_table *sg_tbl, u64 *handle)
{
	pr_info("%s: trusted_mem.ko Not Support FF-A allocation\n", __func__);
	return 0;
}
int tmem_ffa_page_free(u64 handle)
{
	pr_info("%s: trusted_mem.ko Not Support FF-A free\n", __func__);
	return 0;
}
int tmem_carveout_create(int idx, phys_addr_t heap_base, size_t heap_size)
{
	return 0;
}
int tmem_carveout_destroy(int idx)
{
	return 0;
}
