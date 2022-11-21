/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <scsc/scsc_logring.h>
#include "scsc_mif_abs.h"

#include "miframman.h"

/* Caller should provide locking */
void miframman_init(struct miframman *ram, void *start_dram, size_t size_pool)
{
	mutex_init(&ram->lock);

	SCSC_TAG_INFO(MIF, "MIFRAMMAN_BLOCK_SIZE = %d\n", MIFRAMMAN_BLOCK_SIZE);
	ram->num_blocks = size_pool / MIFRAMMAN_BLOCK_SIZE;

	if (ram->num_blocks == 0) {
		SCSC_TAG_ERR(MIF, "Pool size < BLOCK_SIZE\n");
		return;
	}

	if (ram->num_blocks >= MIFRAMMAN_NUM_BLOCKS) {
		SCSC_TAG_ERR(MIF, "Not enough memory\n");
		return;
	}

	memset(ram->bitmap, BLOCK_FREE, sizeof(ram->bitmap));

	ram->start_dram = start_dram;
	ram->size_pool = size_pool;
	ram->free_mem = ram->num_blocks * MIFRAMMAN_BLOCK_SIZE;
}

void *__miframman_alloc(struct miframman *ram, size_t nbytes)
{
	unsigned int index = 0;
	unsigned int available;
	unsigned int i;
	size_t       num_blocks;
	void         *free_mem = NULL;

	if (!nbytes || nbytes > ram->free_mem)
		goto end;

	/* Number of blocks required (rounding up) */
	num_blocks = nbytes / MIFRAMMAN_BLOCK_SIZE +
		     ((nbytes % MIFRAMMAN_BLOCK_SIZE) > 0 ? 1 : 0);

	if (num_blocks > ram->num_blocks)
		goto end;

	while (index <= (ram->num_blocks - num_blocks)) {
		available = 0;

		/* Search consecutive blocks */
		for (i = 0; i < num_blocks; i++) {
			if (ram->bitmap[i + index] != BLOCK_FREE)
				break;
			available++;
		}
		if (available == num_blocks) {
			free_mem = ram->start_dram +
				   MIFRAMMAN_BLOCK_SIZE * index;

			/* Mark the blocks as used */
			ram->bitmap[index++] = BLOCK_BOUND;
			for (i = 1; i < num_blocks; i++)
				ram->bitmap[index++] = BLOCK_INUSE;

			ram->free_mem -= num_blocks * MIFRAMMAN_BLOCK_SIZE;
			goto exit;
		} else
			index = index + available + 1;
	}
end:
	SCSC_TAG_INFO(MIF, "Not enough memory\n");
	return NULL;
exit:
	return free_mem;
}


#define MIFRAMMAN_ALIGN(mem, align) \
	((void *)((((uintptr_t)(mem) + (align + sizeof(void *))) \
		   & (~(uintptr_t)(align - 1)))))

#define MIFRAMMAN_PTR(mem) \
	(*(((void **)((uintptr_t)(mem) & \
		      (~(uintptr_t)(sizeof(void *) - 1)))) - 1))

void *miframman_alloc(struct miframman *ram, size_t nbytes, size_t align)
{
	void *mem, *align_mem = NULL;

	mutex_lock(&ram->lock);
	if (!is_power_of_2(align) || nbytes == 0)
		goto end;

	if (align < sizeof(void *))
		align = sizeof(void *);

	mem = __miframman_alloc(ram, nbytes + align + sizeof(void *));
	if (!mem)
		goto end;

	align_mem = MIFRAMMAN_ALIGN(mem, align);

	/* Store allocated pointer */
	MIFRAMMAN_PTR(align_mem) = mem;
end:
	mutex_unlock(&ram->lock);
	return align_mem;
}

void __miframman_free(struct miframman *ram, void *mem)
{
	unsigned int index, num_blocks = 0;

	if (ram->start_dram == NULL || !mem) {
		SCSC_TAG_ERR(MIF, "Mem is NULL\n");
		return;
	}

	/* Get block index */
	index = (unsigned int)((mem - ram->start_dram)
			       / MIFRAMMAN_BLOCK_SIZE);

	/* Check */
	if (index >= ram->num_blocks) {
		SCSC_TAG_ERR(MIF, "Incorrect index %d\n", index);
		return;
	}

	/* Check it is a Boundary block */
	if (ram->bitmap[index] != BLOCK_BOUND) {
		SCSC_TAG_ERR(MIF, "Incorrect Block descriptor\n");
		return;
	}

	ram->bitmap[index++] = BLOCK_FREE;
	num_blocks++;
	while (index < ram->num_blocks && ram->bitmap[index] == BLOCK_INUSE) {
		ram->bitmap[index++] = BLOCK_FREE;
		num_blocks++;
	}

	ram->free_mem += num_blocks * MIFRAMMAN_BLOCK_SIZE;
}

void miframman_free(struct miframman *ram, void *mem)
{
	mutex_lock(&ram->lock);
	/* Restore allocated pointer */
	if (mem)
		__miframman_free(ram, MIFRAMMAN_PTR(mem));
	mutex_unlock(&ram->lock);
}

/* Caller should provide locking */
void miframman_deinit(struct miframman *ram)
{
	/* Mark all the blocks as INUSE to prevent new allocations */
	memset(ram->bitmap, BLOCK_INUSE, sizeof(ram->bitmap));

	ram->num_blocks = 0;
	ram->start_dram = NULL;
	ram->size_pool = 0;
	ram->free_mem = 0;
}
