// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2014-2022 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include "tlcTui.h"
#include "tui-hal.h"
#include "GP/tee_client_api.h"

struct tui_mempool {
	void *va;
	unsigned long pa;
	size_t size;
	struct dma_buf *tui_dma_buffer;
};

static struct tui_mempool g_tui_mem_pool;

static bool allocate_tui_memory_pool(struct tui_mempool *pool, size_t size)
{
	struct dma_heap *system_heap;

	if (!size) {
		tui_dev_info("TUI frame buffer: nothing to allocate.\n");
		return false;
	}
	size_t aligned_alloc_size = ALIGN(size, SZ_1M);

	tui_dev_info("%zu  requested, aligned to %zu.\n",
		     size, aligned_alloc_size);

	system_heap = dma_heap_find("linux,cma");
	if (!system_heap) {
		tui_dev_info("dma_heap_find failed\n");
		return false;
	}

	pool->tui_dma_buffer = dma_heap_buffer_alloc(system_heap,
						     aligned_alloc_size, 0, 0);
	if (IS_ERR(pool->tui_dma_buffer)) {
		tui_dev_info("dma_heap_buffer_alloc failed\n");
		return false;
	}
	tui_dev_info("dma_heap_buffer_alloc succeeded (%p)\n",
		     pool->tui_dma_buffer);

	pool->va =  dma_buf_vmap(pool->tui_dma_buffer);

	if (IS_ERR(pool->va)) {
		tui_dev_info("dma_buf_vmap failed\n");
		return false;
	}

	tui_dev_info("%s:%d Success of the allocation of tui memory pool\n",
		     __func__, __LINE__);

	pool->pa = virt_to_phys(pool->va);
	pool->size = aligned_alloc_size;

	tui_dev_info("%s:%d va(%p) pa(%lx) size(%zu)\n",
		     __func__, __LINE__, pool->va, pool->pa, pool->size);
	return true;
}

static void free_tui_memory_pool(struct tui_mempool *pool)
{
	tui_dev_info("%s:%d\n", __func__, __LINE__);

	kfree(pool->va);
	memset(pool, 0, sizeof(*pool));
}

/* WARNING:
 * This value should be update accordingly to the screen resolution
 * of DrTui for HIKEY (hikey/hikey-simulation)
 * TUI_MEMPOOL_SIZE >= ( WIDTH * HEIGHT * (BITSPERPIXEL/8) *  NB_OF_BUFFERS )
 */
#define TUI_MEMPOOL_SIZE (3 * 3276800)

u32 hal_tui_init(void)
{
	tui_dev_info("%s:%d\n", __func__, __LINE__);

	/* Allocate memory pool for the framebuffer
	 */
	if (!allocate_tui_memory_pool(&g_tui_mem_pool, TUI_MEMPOOL_SIZE))
		return TUI_DCI_ERR_INTERNAL_ERROR;

	return TUI_DCI_OK;
}

void hal_tui_exit(void)
{
	tui_dev_info("%s:%d\n", __func__, __LINE__);

	/* delete memory pool if any */
	if (g_tui_mem_pool.va)
		free_tui_memory_pool(&g_tui_mem_pool);
}

/**
 * hal_tui_alloc() - allocator for secure framebuffer and working buffer
 * @allocbuffer:    putput parameter that the allocator fills with the physical
 *                  addresses of the allocated buffers
 * @allocsize:      size of the buffer to allocate.  All the buffer are of the
 *                  same size
 * @number:         Number to allocate.
 *
 * This function is called when the module receives a CMD_TUI_SW_OPEN_SESSION
 * message from the secure driver.  The function must allocate 'number'
 * buffer(s) of physically contiguous memory, where the length of each buffer
 * is at least 'allocsize' bytes.  The physical address of each buffer must be
 * stored in the array of structure 'allocbuffer' which is provided as
 * arguments.
 *
 * Physical address of the first buffer must be put in allocate[0].pa , the
 * second one on allocbuffer[1].pa, and so on.  The function must return 0 on
 * success, non-zero on error.  For integrations where the framebuffer is not
 * allocated by the Normal World, this function should do nothing and return
 * success (zero).
 * If the working buffer allocation is different from framebuffers, ensure that
 * the physical address of the working buffer is at index 0 of the allocbuffer
 * table (allocbuffer[0].pa).
 */
u32 hal_tui_alloc(
	struct tui_alloc_buffer_t allocbuffer[MAX_DCI_BUFFER_NUMBER],
	size_t allocsize, u32 number)
{
	tui_dev_info("%s:%d\n", __func__, __LINE__);

	u32 ret = TUI_DCI_ERR_INTERNAL_ERROR;

	if (!allocbuffer) {
		tui_dev_info("%s(%d): allocbuffer is null\n",
			     __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	tui_dev_info("%s(%d): Requested size=%zu x %u chunks\n", __func__,
		     __LINE__, allocsize, number);

	if ((size_t)allocsize == 0) {
		tui_dev_info("%s(%d): Nothing to allocate\n",
			     __func__, __LINE__);
		return TUI_DCI_OK;
	}

	if (number != 3) {
		tui_dev_info("%s(%d): Unexpected number of buffers requested\n",
			     __func__, __LINE__);
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	if ((size_t)(allocsize * number) <= g_tui_mem_pool.size) {
		/* requested buffer fits in the memory pool */
		unsigned int i;

#if defined TUI_USE_FFA
	struct teec_context scontext;
	struct teec_shared_memory shared_mem;
	u64 ffa_handle;

	teec_initialize_context(NULL, &scontext);
	for (i = 0; i < number; i++) {
		shared_mem.buffer = g_tui_mem_pool.va + i * allocsize;
		shared_mem.size = allocsize;
		shared_mem.flags = TEEC_VALUE_INOUT;
		ret = teec_tt_lend_shared_memory(&scontext, &shared_mem,
						 DR_TUI_DRIVER_ID,
						 &ffa_handle);
		if (ret != TEEC_SUCCESS) {
			tui_dev_err(ret, "(%d):error lend_shared_memory\n",
				    __LINE__);
			return TUI_DCI_ERR_INTERNAL_ERROR;
		}
		// Use pa to share IOMMU PA, and ffa_handle for ffa_handle
		allocbuffer[i].ffa_handle = ffa_handle;
		tui_dev_info("%s(%d):lend_shared_memory OK [%d]: %x\n",
			     __func__, __LINE__, i, ffa_handle);
		allocbuffer[i].pa = shared_mem.buffer;
	}
#else
	for (i = 0; i < number; i++) {
		allocbuffer[i].pa = (u64)(g_tui_mem_pool.pa + i * allocsize);
		tui_dev_info("%s(%d):framebuffer[%d]: PA(0x%llx)\n",
			     __func__, __LINE__, i,
			     allocbuffer[i].pa + i * allocsize);
	}
#endif
	ret = TUI_DCI_OK;
	} else {
		/*
		 * requested buffer is bigger than the memory pool, return an
		 * error
		 */
		tui_dev_info("%s(%d): Memory pool too small ",
			     __func__, __LINE__);
		tui_dev_info("(%zu * %d) = requested [%zu] > available[%zu]",
			     allocsize, number, allocsize * number,
			     g_tui_mem_pool.size);
		ret = TUI_DCI_ERR_INTERNAL_ERROR;
	}

	return ret;
}

void hal_tui_free(void)
{
}

u32 hal_tui_deactivate(void)
{
	return TUI_DCI_OK;
}

u32 hal_tui_activate(void)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
u32 hal_tui_process_cmd(struct tui_hal_cmd_t *cmd, struct tui_hal_rsp_t *rsp)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
u32 hal_tui_notif(void)
{
	return TUI_DCI_OK;
}

/* Do nothing it's only use for QC */
void hal_tui_post_start(struct tlc_tui_response_t *rsp)
{
	tui_dev_info("%s(%d)\n", __func__, __LINE__);
}
