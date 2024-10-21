/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_APUMMU_MEM_H__
#define __APUSYS_APUMMU_MEM_H__
#include <linux/types.h>

#include "apummu_mem_def.h"
#include "apummu_drv.h"

void apummu_mem_free(struct device *dev, struct apummu_mem *mem);
int apummu_mem_alloc(struct device *dev, struct apummu_mem *mem);

#if !(DRAM_FALL_BACK_IN_RUNTIME)
/* alloc DRAM fallback dram */
int apummu_dram_remap_alloc(void *drvinfo);
/* free the allocated DRAM fallback dram */
int apummu_dram_remap_free(void *drvinfo);
#else
int apummu_dram_remap_runtime_alloc(void *drvinfo);
int apummu_dram_remap_runtime_free(void *drvinfo);
#endif

int apummu_alloc_general_SLB(void *drvinfo);
int apummu_free_general_SLB(void *drvinfo);

void apummu_mem_init(void);

#endif
