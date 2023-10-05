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

#ifndef IS_HW_MCFP_H
#define IS_HW_MCFP_H

#include "is-hw.h"
#include "is-param.h"
#include "is-hw-common-dma.h"
#include "is-interface-ddk.h"
#include "pablo-internal-subdev-ctrl.h"

#define SET_MCFP_MUTLI_BUFFER_ADDR(nb, np, a)				\
	do {								\
		int __b, __p;						\
		for (__b = 1; __b < (nb); ++__b)			\
			for (__p = 0; __p < (np); ++__p)		\
				(a)[(__b * (np)) + __p] = (a)[__p];	\
	} while(0)

#define SET_MCFP_MUTLI_BUFFER_ADDR_SWAP(nb, np, a1, a2)				\
	do {									\
		int __b, __p;							\
		for (__b = 1; __b < (nb); ++__b) {				\
			for (__p = 0; __p < (np); ++__p) {			\
				(a1)[(__b * (np)) + __p] = (a1)[__p];		\
				(a2)[(__b * (np)) + __p] = (a2)[__p];		\
				if (__b % 2)					\
					SWAP((a1)[(__b * (np)) + __p],		\
					(a2)[(__b * (np)) + __p],		\
					typeof((a1)[(__b * (np)) + __p]));	\
			}							\
		}								\
	} while(0)

#define SKIP_MIX(config) \
	(config->skip_wdma && !config->still_en && (config->mixer_mode == MCFP_TNR_MODE_NORMAL))
#define PARTIAL(stripe_input) \
	(stripe_input->total_count > 1 && stripe_input->index < (stripe_input->total_count - 1))
#define EVEN_BATCH(frame) \
	(frame->num_buffers > 1 && (frame->num_buffers % 2) == 0)

enum is_hw_mcfp_rdma_index {
	MCFP_RDMA_CUR_IN_Y,
	MCFP_RDMA_CUR_IN_UV,
	MCFP_RDMA_PRE_MV,
	MCFP_RDMA_GEOMATCH_MV,
	MCFP_RDMA_MIXER_MV,
	MCFP_RDMA_CUR_DRC,
	MCFP_RDMA_PREV_DRC,
	MCFP_RDMA_SAT_FLAG,
	MCFP_RDMA_PREV_IN0_Y,
	MCFP_RDMA_PREV_IN0_UV,
	MCFP_RDMA_PREV_IN1_Y,
	MCFP_RDMA_PREV_IN1_UV,
	MCFP_RDMA_PREV_IN_WGT,
	MCFP_RDMA_CUR_IN_WGT,
	MCFP_RDMA_MAX
};

enum is_hw_mcfp_wdma_index {
	MCFP_WDMA_PREV_OUT_Y,
	MCFP_WDMA_PREV_OUT_UV,
	MCFP_WDMA_PREV_OUT_WGT,
	MCFP_WDMA_MAX
};

enum is_hw_mcfp_irq_src {
	MCFP_INTR_0,
	MCFP_INTR_1,
	MCFP_INTR_MAX,
};

enum is_hw_tnr_mode {
	MCFP_TNR_MODE_PREPARE,
	MCFP_TNR_MODE_FIRST,
	MCFP_TNR_MODE_NORMAL,
	MCFP_TNR_MODE_FUSION,
};

enum is_hw_mcfp_dbg_mode {
	MCFP_DBG_DUMP_REG,
	MCFP_DBG_DUMP_REG_ONCE,
	MCFP_DBG_S2D,
	MCFP_DBG_SKIP_DDK,
	MCFP_DBG_BYPASS,
	MCFP_DBG_DTP,
	MCFP_DBG_TNR,
};

enum is_hw_mcfp_subdev {
	MCFP_SUBDEV_PREV_YUV,
	MCFP_SUBDEV_PREV_W,
	MCFP_SUBDEV_YUV,
	MCFP_SUBDEV_W,
	MCFP_SUBDEV_END,
};

static const char *mcfp_internal_buf_name[MCFP_SUBDEV_END] = {
	"MCFP_PY",
	"MCFP_PW",
	"MCFP_CY",
	"MCFP_CW",
};

struct is_hw_mcfp {
	struct is_lib_isp		lib[IS_STREAM_COUNT];
	struct mcfp_param_set		param_set[IS_STREAM_COUNT];
	struct is_common_dma		rdma[MCFP_RDMA_MAX];
	struct is_common_dma		wdma[MCFP_WDMA_MAX];
	struct is_mcfp_config		config[IS_STREAM_COUNT];
	u32				irq_state[MCFP_INTR_MAX];
	u32				instance;
	unsigned long			state;
	atomic_t			strip_index;
	struct pablo_internal_subdev	subdev[IS_STREAM_COUNT][MCFP_SUBDEV_END];
	struct is_priv_buf		*pb_c_loader_payload;
	unsigned long			kva_c_loader_payload;
	dma_addr_t			dva_c_loader_payload;
	struct is_priv_buf		*pb_c_loader_header;
	unsigned long			kva_c_loader_header;
	dma_addr_t			dva_c_loader_header;
};

void is_hw_mcfp_s_debug_type(int type);
void is_hw_mcfp_c_debug_type(int type);
const struct kernel_param_ops *is_hw_mcfp_get_param_ops_debug_mcfp_kunit_wrapper(void);

#endif
