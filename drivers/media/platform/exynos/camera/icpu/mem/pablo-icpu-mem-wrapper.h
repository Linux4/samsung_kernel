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

#ifndef PABLO_ICPU_MEM_WRAPPER_H
#define PABLO_ICPU_MEM_WRAPPER_H

struct icpu_buf {
	u32 type;

	size_t size;
	unsigned long align;
	const char *heapname;

	dma_addr_t daddr;
	phys_addr_t paddr;
	void *vaddr;

	void *data;
};

struct icpu_buf_ops {
	int (*alloc)(struct device *dev, struct icpu_buf *buf);
	void (*free)(struct icpu_buf *buf);
	void (*sync_for_device)(struct icpu_buf *buf);
};

int pablo_icpu_pmem_init(void *pdev);
void pablo_icpu_pmem_deinit(void);

int pablo_icpu_cma_init(void *pdev);
void pablo_icpu_cma_deinit(void);

#endif
