// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_RGBP_H
#define IS_HW_RGBP_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"

enum is_hw_rgbp_irq_src {
	RGBP_INTR0,
	RGBP_INTR1,
	RGBP_INTR_MAX,
};

struct is_hw_rgbp_sc_size {
	u32	input_h_size;
	u32	input_v_size;
	u32	dst_h_size;
	u32	dst_v_size;
};

struct is_hw_rgbp {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct rgbp_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		*rdma;
	struct is_common_dma		*wdma;
	struct is_rgbp_config		config;
	u32				irq_state[RGBP_INTR_MAX];
	u32				instance;
	u32				rdma_max_cnt;
	u32				wdma_max_cnt;
	unsigned long			state;
	atomic_t			strip_index;

	struct is_priv_buf		*pb_c_loader_payload;
	unsigned long			kva_c_loader_payload;
	dma_addr_t			dva_c_loader_payload;
	struct is_priv_buf		*pb_c_loader_header;
	unsigned long			kva_c_loader_header;
	dma_addr_t			dva_c_loader_header;
};

#endif
