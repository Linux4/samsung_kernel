/*
 * Samsung Exynos SoC series FIMC-IS driver
 *
 * Exynos fimc-is PSV vender configuration
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_VENDER_CONFIG_H
#define FIMC_IS_VENDER_CONFIG_H

/* configurations */
#define USE_IOMMU
#undef USE_ION_DIRECTLY

/* version dependent */
#define IO_MEM_BASE			0x14400000
#define IO_MEM_SIZE			0x00100000
#define IO_MEM_VECTOR_OFS	0x00400000

#if defined(CONFIG_SOC_EXYNOS7880)
/* only for ver 5.10, should be flexible */
#define SYSMMU_ISP0_OFS	0x00070000	/* CSIS/BNA/3AA */
#define SYSMMU_ISP1_OFS	0x00090000	/* ISP/MCSC */
#define SYSMMU_ISP2_OFS	0x000A0000	/* VRA */

#define PDEV_IRQ_NUM_3AA0	0
#define PDEV_IRQ_NUM_3AA1	1
#define PDEV_IRQ_NUM_ISP0	2
#define PDEV_IRQ_NUM_ISP1	3
#define PDEV_IRQ_NUM_MCSC	4
#define PDEV_IRQ_NUM_VRA0	5
#define PDEV_IRQ_NUM_VRA1	6

#define MAX_PDEV_IRQ_NUM	7

#elif defined(CONFIG_SOC_EXYNOS7570)
#define SYSMMU_ISP0_OFS	0x00070000	/* FIMC */

/* need to match fimc_is interrupts number in 7570.dtsi */
#define PDEV_IRQ_NUM_ISP0	0
#define PDEV_IRQ_NUM_ISP1	1
#define PDEV_IRQ_NUM_ISP2	2
#define PDEV_IRQ_NUM_MCSC	3
#define PDEV_IRQ_NUM_VRA1	4
#define PDEV_IRQ_NUM_MFCMCSC	5

#define MAX_PDEV_IRQ_NUM	6

#endif

#endif
