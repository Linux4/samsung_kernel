/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#ifndef IOMMU_ENGINE_H
#define IOMMU_ENGINE_H

#include <linux/iommu.h>
#include <linux/platform_device.h>

#define DMA_ENGINE_CQDMA		(0x1)
#define DMA_ENGINE_GCE			(0x2)
#define DMA_ENGINE_NUM			(DMA_ENGINE_GCE + 1)

#define CQDMA_BUFLEN			(0x16)
#define CQDMA_BUFLEN2REG		(CQDMA_BUFLEN / 4)

struct dma_engine_data {
	int engine;
	struct platform_device *pdev;
	void __iomem *reg_base;
	int (*iommu_fault_handler)(struct iommu_fault *fault, void *cookie);
};

struct dma_device_res {
	size_t		size;
	void		*vaddr;
	dma_addr_t	dma_addr;
	phys_addr_t	phys_addr;
	unsigned long	attrs;
};

int engine_init(int engine, struct platform_device *pdev);
int engine_access(int engine, struct device *dev, struct dma_device_res *res);

#endif /* IOMMU_ENGINE_H */
