/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_INTERFACE_IRTA_H
#define PABLO_INTERFACE_IRTA_H

enum interface_irta_buf_type {
	ITF_IRTA_BUF_TYPE_RESULT,
	ITF_IRTA_BUF_TYPE_MAX,
};

struct pablo_interface_irta {
	u32 instance;
	struct is_device_ischain *idi;
	struct pablo_icpu_adt *icpu_adt;
	int dma_buf_fd[ITF_IRTA_BUF_TYPE_MAX];
	struct dma_buf *dma_buf[ITF_IRTA_BUF_TYPE_MAX];
	struct dma_buf_attachment *attachment[ITF_IRTA_BUF_TYPE_MAX];
	struct sg_table *sgt[ITF_IRTA_BUF_TYPE_MAX];
	void *kva[ITF_IRTA_BUF_TYPE_MAX];
	dma_addr_t dva[ITF_IRTA_BUF_TYPE_MAX];
};

int pablo_interface_irta_dma_buf_attach(struct pablo_interface_irta *pii,
				enum interface_irta_buf_type type, int fd);
int pablo_interface_irta_dma_buf_detach(struct pablo_interface_irta *pii,
				enum interface_irta_buf_type type);
int pablo_interface_irta_kva_map(struct pablo_interface_irta *pii,
				enum interface_irta_buf_type type);
int pablo_interface_irta_kva_unmap(struct pablo_interface_irta *pii,
				enum interface_irta_buf_type type);

#if IS_ENABLED(CONFIG_PABLO_ICPU)
int pablo_interface_irta_result_buf_set(struct pablo_interface_irta *pii, int fd);
int pablo_interface_irta_result_fcount_set(struct pablo_interface_irta *pii, u32 fcount);
int pablo_interface_irta_start(struct pablo_interface_irta *pii);
int pablo_interface_irta_open(struct pablo_interface_irta *pii, struct is_device_ischain *idi);
int pablo_interface_irta_close(struct pablo_interface_irta *pii);

struct pablo_interface_irta *pablo_interface_irta_get(u32 instance);
void pablo_interface_irta_set(struct pablo_interface_irta *pii);
int pablo_interface_irta_probe(void);
int pablo_interface_irta_remove(void);
#else
#define pablo_interface_irta_result_buf_set(pii, fd) 0
#define pablo_interface_irta_result_fcount_set(pii, fcount) 0
#define pablo_interface_irta_start(pii) 0
#define pablo_interface_irta_open(pii, idi) 0
#define pablo_interface_irta_close(pii) 0

#define pablo_interface_irta_get(instance) NULL
#define pablo_interface_irta_set(pii)
#define pablo_interface_irta_probe() 0
#define pablo_interface_irta_remove() 0
#endif

#endif
