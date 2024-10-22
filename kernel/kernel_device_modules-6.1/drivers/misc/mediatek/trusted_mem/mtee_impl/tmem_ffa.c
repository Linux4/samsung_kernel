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

typedef u16 ffa_partition_id_t;

struct tmem_carveout_heap {
	struct gen_pool *pool;
	phys_addr_t heap_base;
	size_t heap_size;
};
/*
 * enum MTEE_MCHUNKS_ID {
 *
 *	MTEE_MCHUNKS_PROT = 0,
 *	MTEE_MCHUNKS_HAPP = 1,
 *	MTEE_MCHUNKS_HAPP_EXTRA = 2,
 *	MTEE_MCHUNKS_SDSP = 3,
 *	MTEE_MCHUNKS_SDSP_SHARED_VPU_TEE = 4,
 *	MTEE_MCHUNKS_SDSP_SHARED_MTEE_TEE = 5,
 *	MTEE_MCHUNKS_SDSP_SHARED_VPU_MTEE_TEE = 6,
 *	MTEE_MCHUNKS_CELLINFO = 7,
 *	MTEE_MCHUNKS_SVP = 8,
 *	MTEE_MCHUNKS_WFD = 9,
 *	MTEE_MCHUNKS_SAPU_DATA_SHM = 10,
 *	MTEE_MCHUNKS_SAPU_ENGINE_SHM = 11,
 *	MTEE_MCHUNKS_TUI = 12,
 * }
 */
static struct tmem_carveout_heap *tmem_carveout_heap[MTEE_MCHUNKS_MAX_ID];

struct tmem_block {
	struct list_head head;
	dma_addr_t start;
	int size;
	u64 handle;
	struct sg_table *sgtbl;
};

static LIST_HEAD(tmem_block_list);
static DEFINE_MUTEX(tmem_block_mutex);

/* Store the discovered partition data globally. */
static const struct ffa_ops *tmem_ffa_ops;
static struct ffa_device *tmem_ffa_dev;

static struct timespec64 ffa_start_time;
static struct timespec64 ffa_end_time;

static u32 ffa_get_spend_nsec(void)
{
	struct timespec64 *start_time = &ffa_start_time;
	struct timespec64 *end_time = &ffa_end_time;

	return GET_TIME_DIFF_NSEC_P(start_time, end_time);
}

static void tmem_do_gettimeofday(struct timespec64 *tv)
{
	struct timespec64 now;

	ktime_get_real_ts64(&now);
	tv->tv_sec = now.tv_sec;
	tv->tv_nsec = now.tv_nsec;
}

/* bit[31]=1 is do retrieve_req in EL2 */
#define PAGED_BASED_FFA_FLAGS 0x80000000

/* receiver numbers */
#define VM_HA_NUM   0x64
#define ATTRS_NUM (VM_HA_NUM + 1)
/* HA receiver id should be a VM at normal world */
#define VM_HA_BASE   0x2
/* TA receiver id should be a SP at secure world */
#define SP_TA_1   0x8001

static void set_memory_region_attrs(enum MTEE_MCHUNKS_ID mchunk_id,
						struct ffa_mem_ops_args *ffa_args,
						struct ffa_mem_region_attributes *mem_region_attrs,
						int show_attr)
{
	int i;

	for (i = 0; i < VM_HA_NUM; i++) {
		mem_region_attrs[i] = (struct ffa_mem_region_attributes) {
			.receiver = VM_HA_BASE + i,
			.attrs = FFA_MEM_RW
		};
	}

	switch (mchunk_id) {
	case MTEE_MCHUNKS_SVP:
		mem_region_attrs[0] = (struct ffa_mem_region_attributes) {
			.receiver = SP_TA_1,
			.attrs = FFA_MEM_RW
		};
		ffa_args->nattrs = 1;
		if (show_attr)
			pr_info("%s: mchunk_id = MTEE_MCHUNKS_SVP\n", __func__);
		break;

	case MTEE_MCHUNKS_WFD:
		mem_region_attrs[0] = (struct ffa_mem_region_attributes) {
			.receiver = SP_TA_1,
			.attrs = FFA_MEM_RW
		};
		ffa_args->nattrs = 1;
		if (show_attr)
			pr_info("%s: mchunk_id = MTEE_MCHUNKS_WFD\n", __func__);
		break;

	case MTEE_MCHUNKS_PROT:
		ffa_args->nattrs = VM_HA_NUM;
		if (show_attr)
			pr_info("%s: mchunk_id = MTEE_MCHUNKS_PROT\n", __func__);
		break;

	case MTEE_MCHUNKS_TUI:
		mem_region_attrs[0] = (struct ffa_mem_region_attributes) {
			.receiver = SP_TA_1,
			.attrs = FFA_MEM_RW
		};
		ffa_args->nattrs = 1;
		pr_info("%s: mchunk_id = MTEE_MCHUNKS_TUI\n", __func__);
		break;

	case MTEE_MCHUNKS_TEE:
		mem_region_attrs[0] = (struct ffa_mem_region_attributes) {
			.receiver = SP_TA_1,
			.attrs = FFA_MEM_RW
		};
		ffa_args->nattrs = 1;
		pr_info("%s: mchunk_id = MTEE_MCHUNKS_TEE\n", __func__);
		break;

	case MTEE_MCUHNKS_INVALID:
		ffa_args->nattrs = 0;
		pr_info("%s: mchunk_id = MTEE_MCUHNKS_INVALID\n", __func__);
		break;

	default:
		ffa_args->nattrs = VM_HA_NUM;
		pr_info("%s: mchunk_id = %d\n", __func__, mchunk_id);
		break;
	}

	ffa_args->attrs = mem_region_attrs;
}

int tmem_ffa_page_alloc(enum MTEE_MCHUNKS_ID mchunk_id,
						struct sg_table *sg_tbl, u64 *handle)
{
	struct ffa_mem_ops_args ffa_args;
	struct ffa_mem_region_attributes mem_region_attrs[ATTRS_NUM];
	int ret;

	if (tmem_ffa_dev == NULL) {
		pr_info("%s: tmem_ffa_probe() failed\n", __func__);
		return TMEM_KPOOL_FFA_INIT_FAILED;
	}

	mutex_lock(&tmem_block_mutex);

	/* set ffa_mem_ops_args */
	set_memory_region_attrs(mchunk_id, &ffa_args, mem_region_attrs, 0);
	ffa_args.use_txbuf = true;
	if ((mchunk_id == MTEE_MCHUNKS_PROT) ||
		(mchunk_id == MTEE_MCHUNKS_SAPU_DATA_SHM)) {
		/* set bit[31]=1 then ffa_lend will do retrieve_req */
		ffa_args.flags = PAGED_BASED_FFA_FLAGS;
	} else
		ffa_args.flags = 0;
	ffa_args.tag = 0;
	ffa_args.g_handle = 0;
	ffa_args.sg = sg_tbl->sgl;

	tmem_do_gettimeofday(&ffa_start_time);
	ret = tmem_ffa_ops->mem_ops->memory_lend(&ffa_args);
	if (ret) {
		pr_info("page-based, failed to FF-A send the memory, ret=%d\n", ret);
		mutex_unlock(&tmem_block_mutex);
		return TMEM_KPOOL_FFA_PAGE_FAILED;
	}
	tmem_do_gettimeofday(&ffa_end_time);
	pr_debug("%s FF-A flow spend time: %d ns\n", __func__, ffa_get_spend_nsec());

	*handle = ffa_args.g_handle;

	mutex_unlock(&tmem_block_mutex);

	pr_info("%s PASS: handle=0x%llx\n", __func__, *handle);
	return TMEM_OK;
}

int tmem_ffa_page_free(enum MTEE_MCHUNKS_ID mchunk_id, u64 handle)
{
	struct ffa_mem_ops_args ffa_args;
	int ret;

	if (tmem_ffa_dev == NULL) {
		pr_info("%s: tmem_ffa_probe() failed\n", __func__);
		return TMEM_KPOOL_FFA_INIT_FAILED;
	}

	mutex_lock(&tmem_block_mutex);

	if ((mchunk_id == MTEE_MCHUNKS_PROT) ||
		(mchunk_id == MTEE_MCHUNKS_SAPU_DATA_SHM)) {
		/* set bit[31]=1 then ffa_lend will do retrieve_req */
		ffa_args.flags = PAGED_BASED_FFA_FLAGS;
	} else
		ffa_args.flags = 0;

	tmem_do_gettimeofday(&ffa_start_time);
	ret = tmem_ffa_ops->mem_ops->memory_reclaim(handle, ffa_args.flags);
	if (ret) {
		pr_info("page-based, handle=0x%llx failed to FF-A reclaim, ret=%d\n",
			handle, ret);
//		mutex_unlock(&tmem_block_mutex);
//		return TMEM_KPOOL_FFA_PAGE_FAILED;
	}
	tmem_do_gettimeofday(&ffa_end_time);
	pr_debug("%s FF-A flow spend time: %d ns\n", __func__, ffa_get_spend_nsec());

	mutex_unlock(&tmem_block_mutex);

	pr_info("%s PASS: handle=0x%llx\n", __func__, handle);
	return TMEM_OK;
}

int tmem_ffa_region_alloc(enum MTEE_MCHUNKS_ID mchunk_id,
						  unsigned long size,
						  unsigned long alignment,
						  u64 *handle)
{
	dma_addr_t paddr;
	void *vaddr;
	struct tmem_block *entry;
	unsigned long pool_idx = mchunk_id;
	struct ffa_mem_ops_args ffa_args;
	struct ffa_mem_region_attributes mem_region_attrs[ATTRS_NUM];
	struct sg_table *tmem_sgtbl;
	struct scatterlist *tmem_sgl;
	int ret;

	if (tmem_ffa_dev == NULL) {
		pr_info("%s: tmem_ffa_probe() failed\n", __func__);
		return TMEM_KPOOL_FFA_INIT_FAILED;
	}

	mutex_lock(&tmem_block_mutex);

	if (alignment == 0)
		alignment = PAGE_SIZE;
	vaddr = gen_pool_dma_alloc_align(tmem_carveout_heap[pool_idx]->pool, size,
								NULL, alignment);
	paddr = (dma_addr_t)vaddr;
	if (!paddr)
		goto out4;

	/* set sg_table */
	tmem_sgtbl = kmalloc(sizeof(*tmem_sgtbl), GFP_KERNEL);
	if (tmem_sgtbl == NULL)
		goto out3;

	ret = sg_alloc_table(tmem_sgtbl, 1, GFP_KERNEL);
	if (ret)
		goto out2;

	/* set scatterlist */
	tmem_sgl = tmem_sgtbl->sgl;
	sg_dma_len(tmem_sgl) = size;
	sg_set_page(tmem_sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	sg_dma_address(tmem_sgl) = paddr;

	/* set ffa_mem_ops_args */
	set_memory_region_attrs(mchunk_id, &ffa_args, mem_region_attrs, 1);
	ffa_args.use_txbuf = true;
	ffa_args.flags = 0;
	ffa_args.tag = 0;
	ffa_args.g_handle = 0;
	ffa_args.sg = tmem_sgl;

	tmem_do_gettimeofday(&ffa_start_time);
	ret = tmem_ffa_ops->mem_ops->memory_lend(&ffa_args);
	if (ret) {
		pr_info("region-based, failed to FF-A send the memory, ret=%d\n", ret);
		goto out1;
	}
	tmem_do_gettimeofday(&ffa_end_time);
	pr_debug("%s FF-A flow spend time: %d ns\n", __func__, ffa_get_spend_nsec());

	entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		goto out1;

	/* set tmem_block */
	*handle = ffa_args.g_handle;
	entry->start = paddr;
	entry->size = size;
	entry->handle = *handle;
	entry->sgtbl = tmem_sgtbl;

	list_add(&entry->head, &tmem_block_list);
	mutex_unlock(&tmem_block_mutex);

	pr_info("%s PASS: handle=0x%llx, paddr=0x%llx, size=0x%lx, alignment=0x%lx\n",
			__func__, *handle, paddr, size, alignment);

	return TMEM_OK;

out1:
	sg_free_table(tmem_sgtbl);
out2:
	kfree(tmem_sgtbl);
out3:
	gen_pool_free(tmem_carveout_heap[pool_idx]->pool, paddr, size);
out4:
	pr_info("%s fail: size=0x%lx, alignment=0x%lx, gen_pool_avail=0x%lx, pool_idx=%lu\n",
			__func__, size, alignment,
			gen_pool_avail(tmem_carveout_heap[pool_idx]->pool), pool_idx);
	mutex_unlock(&tmem_block_mutex);

	return -ENOMEM;
}

int tmem_ffa_region_free(enum MTEE_MCHUNKS_ID mchunk_id, u64 handle)
{
	struct tmem_block *tmp;
	unsigned long pool_idx = mchunk_id;
	int ret;

	if (tmem_ffa_dev == NULL) {
		pr_info("%s: tmem_ffa_probe() failed\n", __func__);
		return TMEM_KPOOL_FFA_INIT_FAILED;
	}

	mutex_lock(&tmem_block_mutex);

	tmem_do_gettimeofday(&ffa_start_time);
	ret = tmem_ffa_ops->mem_ops->memory_reclaim(handle, 0);
	if (ret) {
		pr_info("region-based, handle=0x%llx failed to FF-A reclaim, ret=%d\n",
			handle, ret);
//		mutex_unlock(&tmem_block_mutex);
//		return TMEM_KPOOL_FFA_PAGE_FAILED;
	}
	tmem_do_gettimeofday(&ffa_end_time);
	pr_debug("%s FF-A flow spend time: %d ns\n", __func__, ffa_get_spend_nsec());

	list_for_each_entry(tmp, &tmem_block_list, head) {
		if (tmp->handle == handle) {
			gen_pool_free(tmem_carveout_heap[pool_idx]->pool, tmp->start, tmp->size);
			list_del(&tmp->head);
			sg_free_table(tmp->sgtbl);
			mutex_unlock(&tmem_block_mutex);

			kfree(tmp->sgtbl);
			kfree(tmp);
			pr_info("%s PASS: handle=0x%llx\n", __func__, handle);
			return TMEM_OK;
		}
	}
	mutex_unlock(&tmem_block_mutex);

	pr_info("%s fail: handle=0x%llx, idx=0x%lx\n",
			__func__, handle, pool_idx);

	return TMEM_KPOOL_FREE_CHUNK_FAILED;
}

int tmem_carveout_create(int idx, phys_addr_t heap_base, size_t heap_size)
{
	if (tmem_carveout_heap[idx] != NULL) {
		pr_info("%s:%d: tmem_carveout_heap already created\n", __func__, __LINE__);
		return TMEM_KPOOL_HEAP_ALREADY_CREATED;
	}

	tmem_carveout_heap[idx] = kmalloc(sizeof(struct tmem_carveout_heap), GFP_KERNEL);
	if (!tmem_carveout_heap[idx])
		return -ENOMEM;

	mutex_lock(&tmem_block_mutex);

	tmem_carveout_heap[idx]->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!tmem_carveout_heap[idx]->pool) {
		pr_info("%s:%d: gen_pool_create() fail\n", __func__, __LINE__);
		kfree(tmem_carveout_heap[idx]);
		mutex_unlock(&tmem_block_mutex);
		return -ENOMEM;
	}

	tmem_carveout_heap[idx]->heap_base = heap_base;
	tmem_carveout_heap[idx]->heap_size = heap_size;
	gen_pool_add(tmem_carveout_heap[idx]->pool, heap_base, heap_size, -1);

	mutex_unlock(&tmem_block_mutex);

	return TMEM_OK;
}

int tmem_carveout_destroy(int idx)
{
	if (tmem_carveout_heap[idx] == NULL) {
		pr_info("%s:%d: tmem_carveout_heap is NULL\n", __func__, __LINE__);
		return TMEM_KPOOL_HEAP_IS_NULL;
	}

	mutex_lock(&tmem_block_mutex);

	gen_pool_destroy(tmem_carveout_heap[idx]->pool);

	kfree(tmem_carveout_heap[idx]);
	tmem_carveout_heap[idx] = NULL;

	mutex_unlock(&tmem_block_mutex);

	return TMEM_OK;
}

int tmem_query_ffa_handle_to_pa(u64 handle, uint64_t *phy_addr)
{
	struct tmem_block *tmp;

	mutex_lock(&tmem_block_mutex);
	list_for_each_entry(tmp, &tmem_block_list, head) {
		if (tmp->handle == handle) {
			*phy_addr = tmp->start;
			pr_info("%s: handle=0x%llx, pa=0x%llx\n", __func__, handle, tmp->start);
			mutex_unlock(&tmem_block_mutex);
			return TMEM_OK;
		}
	}
	mutex_unlock(&tmem_block_mutex);

	pr_info("%s: handle=0x%llx, query fail\n", __func__, handle);
	return TMEM_KPOOL_QUERY_FAILED;
}

phys_addr_t get_mem_pool_pa(u32 mchunk_id)
{
	return tmem_carveout_heap[mchunk_id]->heap_base;
}

unsigned long get_mem_pool_size(u32 mchunk_id)
{
	return gen_pool_size(tmem_carveout_heap[mchunk_id]->pool);
}

static const struct ffa_device_id tmem_ffa_device_id[] = {
	{ UUID_INIT(0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x19, 0x0, 0x0, 0x0) },
	{ UUID_INIT(0xd9d08fba, 0x8740, 0x8f4f,
		0xa1, 0xe4, 0xb4, 0x5c, 0x58, 0x08, 0x12, 0xa1) },
	{}
};

static int tmem_ffa_probe(struct ffa_device *ffa_dev)
{
	pr_info("%s:%d (start)\n", __func__, __LINE__);
	tmem_ffa_dev = ffa_dev;
	tmem_ffa_ops = ffa_dev->ops;
	pr_info("%s:%d (end)\n", __func__, __LINE__);

	return 0;
}

static void tmem_ffa_remove(struct ffa_device *ffa_dev)
{
	(void)ffa_dev;
}

static struct ffa_driver tmem_ffa_driver = {
	.name = "tmem_ffa",
	.probe = tmem_ffa_probe,
	.remove = tmem_ffa_remove,
	.id_table = tmem_ffa_device_id,
};

int tmem_register_ffa_module(void)
{
	int ret;

	pr_info("%s:%d (start)\n", __func__, __LINE__);

	ret = ffa_register(&tmem_ffa_driver);
	if (ret) {
		pr_info("%s: failed to get FFA-ops\n", __func__);
		return TMEM_KPOOL_FFA_INIT_FAILED;
	}

	pr_info("%s:%d (end)\n", __func__, __LINE__);

	return 0;
}
