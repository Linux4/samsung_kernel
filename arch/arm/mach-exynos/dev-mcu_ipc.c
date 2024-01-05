/*
 * Copyright (c) 2012 Samsung Electronics
 *
 * Base MCU_IPC resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <mach/map.h>

static struct resource exynos4_mcu_ipc_resource[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_MCU_IPC, SZ_1K),
	[1] = DEFINE_RES_IRQ(EXYNOS4_IRQ_MCU_IPC),
};

static u64 exynos4_mcu_ipc_dma_mask = DMA_BIT_MASK(32);

struct platform_device exynos4_device_mcu_ipc = {
	.name		= "mcu_ipc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(exynos4_mcu_ipc_resource),
	.resource	= exynos4_mcu_ipc_resource,
	.dev		= {
		.dma_mask		= &exynos4_mcu_ipc_dma_mask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
};