/*
 * drivers/media/platform/exynos/mfc/base/mfc_regs.h
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_REGS_H
#define __MFC_REGS_H __FILE__

#include "mfc_regs_mfc.h"
#include "mfc_regs_fg.h"
#include "mfc_data_struct.h"

extern struct mfc_dev *g_mfc_dev;

#define MFC_MIN_BITMASK		32

#define MFC_CORE_READL(offset)					\
	readl(core->regs_base + (offset))
#define MFC_CORE_WRITEL(data, offset)				\
	writel((data), core->regs_base + (offset))

#define MFC_CORE_RAW_READL(offset)				\
	__raw_readl(core->regs_base + (offset))
#define MFC_CORE_RAW_WRITEL(data, offset)			\
	__raw_writel((data), core->regs_base + (offset))

#define MFC_CORE_DMA_READL(offset)						\
({										\
	dma_addr_t data;							\
	if (core->dev->pdata->dma_bit_mask == MFC_MIN_BITMASK)			\
		data = readl(core->regs_base + (offset));			\
	else									\
		data = (dma_addr_t)readl(core->regs_base + (offset)) << 4;	\
	data;									\
})

#define MFC_CORE_DMA_WRITEL(data, offset)					\
({										\
	if (core->dev->pdata->dma_bit_mask == MFC_MIN_BITMASK)			\
		writel((data), core->regs_base + (offset));			\
	else									\
		writel((data >> 4), core->regs_base + (offset));		\
})

#define MFC_NALQ_DMA_READL(offset)						\
({										\
	dma_addr_t data;							\
	if (g_mfc_dev->pdata->dma_bit_mask == MFC_MIN_BITMASK)			\
		data = offset;							\
	else									\
		data = (dma_addr_t)offset << 4;						\
	data;									\
})

#define MFC_NALQ_DMA_WRITEL(offset)						\
({										\
	dma_addr_t data;							\
	if (g_mfc_dev->pdata->dma_bit_mask == MFC_MIN_BITMASK)			\
		data = offset;							\
	else									\
		data = offset >> 4;						\
	data;									\
})

#define MFC_FG_DMA_READL(offset)						\
({										\
	dma_addr_t data, data_lsb;						\
	data = (dma_addr_t)readl(core->regs_base + (offset)) << 4;		\
	data_lsb = (dma_addr_t)readl(core->regs_base + (offset) + 0x20);	\
	data |= data_lsb;							\
	data;									\
})

#define MFC_FG_DMA_WRITEL(data, offset)						\
({										\
	writel((data >> 4), core->regs_base + (offset));			\
	writel((data & 0xf), core->regs_base + (offset) + 0x20);		\
})

#define MFC_MMU0_READL(offset)		readl(core->sysmmu0_base + (offset))
#define MFC_MMU1_READL(offset)		readl(core->sysmmu1_base + (offset))

#define VOTF_WRITEL(data, offset)	writel((data), core->votf_base + (offset))
#define HWFC_WRITEL(data)		writel((data), core->hwfc_base)

#endif /* __MFC_REGS_H */
