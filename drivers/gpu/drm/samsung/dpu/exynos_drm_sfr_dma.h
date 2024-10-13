// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung DMA register setting mode
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXYNOS_DRM_SFR_DMA_H_
#define _EXYNOS_DRM_SFR_DMA_H_

#include <exynos_drm_drv.h>
#include <samsung_drm.h>

#include <regs-dpp.h>
#include <dpp_cal.h>
#include <exynos_drm_hdr.h>

#define DMA_IRQ_NAME_SIZE	16
#define DMA_DATA_SIZE		(128)
#define DMA_MAX_NUM		(2048)
#define DMA_ADDR_MASK(a)	(a & 0xFFFFFFFF00000000)
#define DMA_ADDR(a)		(DMA_ADDR_MASK(a) >> 32)
#define EXTRACT_VALUE(_d) 	(((_d << 32) >> 32) & 0xFFFFFFFF)

#define ALIGN(x, a)                     __ALIGN_KERNEL((x), (a))
#define ALIGN_DOWN(x, a)                __ALIGN_KERNEL((x) - ((a) - 1), (a))

/* For SFR_DMA */
int exynos_drm_sfr_dma_request(int id, u32 offset, u32 val, enum dpp_regs_type type);
int exynos_drm_sfr_dma_update(void);
int exynos_drm_sfr_dma_initialize(struct device *dev, size_t size, bool en);
bool exynos_drm_sfr_dma_is_enabled(int id);

/* For DPU_DMA COMMON */
int exynos_drm_sfr_dma_irq_enable(u32 dpuf, bool en);
void exynos_drm_sfr_dma_mode_switch(bool en);
#endif
