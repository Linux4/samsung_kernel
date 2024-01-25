/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */
#ifndef _SDE_HW_REG_DMA_V1_H
#define _SDE_HW_REG_DMA_V1_H

#include "sde_reg_dma.h"

/**
 * init_v1() - initialize the reg dma v1 driver by installing v1 ops
 * @reg_dma - reg_dma hw info structure exposing capabilities.
 */
int init_v1(struct sde_hw_reg_dma *reg_dma);

/**
 * init_v11() - initialize the reg dma v11 driver by installing v11 ops
 * @reg_dma - reg_dma hw info structure exposing capabilities.
 */
int init_v11(struct sde_hw_reg_dma *reg_dma);

/**
 * init_v12() - initialize the reg dma v12 driver by installing v12 ops
 * @reg_dma - reg_dma hw info structure exposing capabilities.
 */
int init_v12(struct sde_hw_reg_dma *reg_dma);

/**
 * init_v2() - initialize the reg dma v2 driver by installing v2 ops
 * @reg_dma - reg_dma hw info structure exposing capabilities.
 */
int init_v2(struct sde_hw_reg_dma *reg_dma);

/**
 * deinit_v1() - free up any resources allocated during the v1 reg dma init
 */
void deinit_v1(void);

/**
 * verify_lut_dma_status() - Debug function to report when LUTDMA is showing an error status
 * @ctl - ctl to check LUTDMA status for
 * @type - sde_reg_dma_type indicating which LUTDMA engine to query
 */
int verify_lut_dma_status(struct sde_hw_ctl *ctl, enum sde_reg_dma_type type);
#endif /* _SDE_HW_REG_DMA_V1_H */
