// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/genalloc.h>
#include <linux/trusty/trusty.h>
#include <linux/trusty/trusty_shm.h>
#include <linux/trusty/smcall.h>

static struct gen_pool *gpool;
static struct device *gdev;

struct trusty_shm gshm;

struct trusty_shm_ops {
	void *(*alloc)(size_t size, gfp_t gfp_mask);
	void (*free)(void *vaddr, size_t size);
	void *(*phys_to_virt)(phys_addr_t x);
	phys_addr_t (*virt_to_phys)(const volatile void *x);
};

static void *default_shm_alloc(size_t sz, gfp_t gfp)
{
	return alloc_pages_exact(sz, gfp);
}

static void default_shm_free(void *va, size_t sz)
{
	free_pages_exact(va, sz);
}

static void *gpool_shm_alloc(size_t size, gfp_t gfp)
{
	unsigned long vaddr;

	BUG_ON(!gpool);
	BUG_ON(!size);

	vaddr = gen_pool_alloc_algo_owner(gpool, size, gpool->algo, gpool->data, NULL);
	if (!vaddr) {
		dev_err(gdev, "%s: alloc shm failed, request 0x%zx avail 0x%zx\n",
				__func__, size, gen_pool_avail(gpool));
		return NULL;
	}

	if (gfp & __GFP_ZERO)
		memset((void *)vaddr, 0, size);

	dev_dbg(gdev, "%s: 0x%zx alloced, avail 0x%zx\n",
			__func__, size, gen_pool_avail(gpool));

	return (void *)vaddr;
}

static void gpool_shm_free(void *vaddr, size_t size)
{
	BUG_ON(!gpool);
	BUG_ON(!vaddr);
	BUG_ON(!size);

	gen_pool_free_owner(gpool, (uintptr_t)vaddr, size, NULL);

	dev_dbg(gdev, "%s: 0x%zx freed, avail 0x%zx\n",
			__func__, size, gen_pool_avail(gpool));
}

static void *gpool_shm_phys_to_virt(phys_addr_t paddr)
{
	struct gen_pool_chunk *chunk;
	size_t chunk_size = 0;
	unsigned long vaddr = 0;

	BUG_ON(!gpool);

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &gpool->chunks, next_chunk) {
		chunk_size = chunk->end_addr - chunk->start_addr + 1;
		if (paddr >= chunk->phys_addr &&
				paddr <= (chunk->phys_addr + chunk_size)) {
			vaddr = chunk->start_addr + (paddr - chunk->phys_addr);
			break;
		}
	}
	rcu_read_unlock();

	return (void *)vaddr;
}

static phys_addr_t gpool_shm_virt_to_phys(const volatile void *vaddr)
{
	BUG_ON(!gpool);

	return gen_pool_virt_to_phys(gpool, (uintptr_t)vaddr);
}

static struct trusty_shm_ops shm_ops = {
	.alloc = default_shm_alloc,
	.free = default_shm_free,
	.virt_to_phys = virt_to_phys,
	.phys_to_virt = phys_to_virt,
};

void *trusty_shm_alloc(size_t size, gfp_t gfp)
{
	return shm_ops.alloc(size, gfp);
}
EXPORT_SYMBOL(trusty_shm_alloc);

void trusty_shm_free(void *vaddr, size_t size)
{
	shm_ops.free(vaddr, size);
}
EXPORT_SYMBOL(trusty_shm_free);

void *trusty_shm_phys_to_virt(unsigned long paddr)
{
	return shm_ops.phys_to_virt(paddr);
}
EXPORT_SYMBOL(trusty_shm_phys_to_virt);

phys_addr_t trusty_shm_virt_to_phys(void *vaddr)
{
	return shm_ops.virt_to_phys(vaddr);
}
EXPORT_SYMBOL(trusty_shm_virt_to_phys);

int trusty_shm_init(struct device *dev)
{
	gdev = dev;

	return ise_fast_call32(gdev, SMC_FC_IOREMAP_PA_INFO,
			(u32)gshm.ioremap_paddr,
			(u32)(gshm.ioremap_paddr >> 32),
			(u32)gshm.mem_size);
}
EXPORT_SYMBOL(trusty_shm_init);

int trusty_shm_init_pool(struct device *dev)
{
	int rc;

	gdev = dev;

	if (!gpool) {
		gpool = gen_pool_create(3 /* 8 bytes aligned */, -1);
		if (!gpool) {
			dev_err(dev, "%s: gen_pool_create failed\n", __func__);
			goto err_pool_create;
		}

		gen_pool_set_algo(gpool, gen_pool_best_fit, NULL);
		rc = gen_pool_add_owner(gpool, (uintptr_t)gshm.vaddr, gshm.paddr, gshm.mem_size, -1, NULL);
		if (rc < 0) {
			dev_err(dev, "%s: gen_pool_add_virt failed (%d)\n",
					__func__, rc);
			goto err_pool_add_virt;
		}

		shm_ops.alloc = gpool_shm_alloc;
		shm_ops.free = gpool_shm_free;
		shm_ops.virt_to_phys = gpool_shm_virt_to_phys;
		shm_ops.phys_to_virt = gpool_shm_phys_to_virt;

		dev_info(dev, "shm available, use shm alloc/free\n");
		return 0;
	}

err_pool_add_virt:
	if (gpool) {
		gen_pool_destroy(gpool);
		gpool = NULL;
	}
err_pool_create:
	dev_info(dev, "shm not available, fallback to default alloc/free\n");
	return 0;
}
EXPORT_SYMBOL(trusty_shm_init_pool);

void trusty_shm_destroy_pool(struct device *dev)
{
	if (gpool)
		gen_pool_destroy(gpool);
}
EXPORT_SYMBOL(trusty_shm_destroy_pool);
