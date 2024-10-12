/* SPDX-License-Identifier: GPL */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_ICPU_MEM_H
#define PABLO_ICPU_MEM_H

enum icpu_mem_type {
	ICPU_MEM_TYPE_PMEM = 0, /* Pablo-mem */
	ICPU_MEM_TYPE_CMA = 1,
	ICPU_MEM_TYPE_MAX,
};

struct pablo_icpu_buf_info {
	size_t size;
	ulong kva;
	dma_addr_t dva;
	phys_addr_t pa;
};

void *pablo_icpu_mem_alloc(u32 type, size_t size, const char *heapname, unsigned long align);
void pablo_icpu_mem_free(void *buf);
struct pablo_icpu_buf_info pablo_icpu_mem_get_buf_info(void *buf);
void pablo_icpu_mem_sync_for_device(void *buf);

int pablo_icpu_mem_probe(void *pdev);
void pablo_icpu_mem_remove(void);

#endif
