/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef IS_HW_LME_V3_0_H
#define IS_HW_LME_V3_0_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "is-common-enum.h"
#include "pablo-internal-subdev-ctrl.h"

#define LME_DEBUG_LOG_LEVEL_MASK	0xf

#define IS_LME_BLOCK_PER_W_PIXELS	8
#define IS_LME_BLOCK_PER_H_PIXELS	4
#define IS_LME_MV_SIZE_PER_BLOCK	4 /* 2Bytes for two coordinates (x, y) */
#define IS_LME_SAD_SIZE_PER_BLOCK	2

enum is_hw_lme_rdma_index {
	LME_RDMA_CACHE_IN_0, /* video (0: prev, 1:cur) still (0: cur, 1: ref)*/
	LME_RDMA_CACHE_IN_1,
	LME_RDMA_MBMV_IN,
	LME_RDMA_MAX
};

enum is_hw_lme_wdma_index {
	LME_WDMA_MV_OUT,
	LME_WDMA_MBMV_OUT,
	LME_WDMA_SAD_OUT,
	LME_WDMA_MAX
};

enum is_hw_lme_irq_src {
	LME_INTR,
	LME_INTR_MAX,
};

enum is_hw_lme_mode {
	LME_MODE_FUSION	= 0,
	LME_MODE_TNR	= 1,
};

enum is_hw_lme_buf_id {
	LME_INTERNAL_BUF_MBMV_IN,
	LME_INTERNAL_BUF_MBMV_OUT,
	LME_INTERNAL_BUF_ID_MAX
};

struct is_hw_lme {
	struct lme_param_set		param_set[IS_STREAM_COUNT];
	struct is_lme_config		config[IS_STREAM_COUNT];

	u32				irq_state[LME_INTR_MAX];
	u32				instance;
	unsigned long	state;
	atomic_t			strip_index;

	u32				lme_mode;
	u32				corex_set_id;

	struct tasklet_struct		end_tasklet;
	struct pablo_internal_subdev	subdev[IS_STREAM_COUNT][LME_INTERNAL_BUF_ID_MAX];

	struct is_priv_buf		*pb_c_loader_payload;
	unsigned long			kva_c_loader_payload;
	dma_addr_t			dva_c_loader_payload;
	struct is_priv_buf		*pb_c_loader_header;
	unsigned long			kva_c_loader_header;
	dma_addr_t			dva_c_loader_header;
};

int is_hw_lme_probe(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);
#endif
