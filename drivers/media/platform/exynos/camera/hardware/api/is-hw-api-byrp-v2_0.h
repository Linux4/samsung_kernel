/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * byrp HW control APIs
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_API_BYRP_2_0_H
#define IS_HW_API_BYRP_2_0_H

#include "is-hw-common-dma.h"
#include "is-type.h"
#include "is-hw-api-type.h"
#include "pablo-mmio.h"

#define	BYRP_USE_MMIO	0

#define COREX_IGNORE			0
#define COREX_COPY			1
#define COREX_SWAP			2

#define HW_TRIGGER			0
#define SW_TRIGGER			1

enum is_hw_byrp_irq_src {
	BYRP_INT0,
	BYRP_INT1,
	BYRP_INTR_MAX,
};

enum byrp_input_path_type {
	OTF = 0,
	STRGEN = 1,
	DMA = 2
};

enum set_status {
	SET_SUCCESS,
	SET_ERROR
};

enum byrp_event_type {
	INTR_FRAME_START,
	INTR_FRAME_CINROW,
	INTR_FRAME_END,
	INTR_COREX_END_0,
	INTR_COREX_END_1,
	INTR_SETTING_DONE,
	INTR_ERR0,
	INTR_ERR1
};

enum byrp_bcrop_type {
	BYRP_BCROP_BYR = 0, /* Dzoom */
	BYRP_BCROP_ZSL = 1, /* Not Use */
	BYRP_BCROP_RGB = 2, /* DNG capture */
	BYRP_BCROP_MAX
};

enum byrp_img_fmt {
	BYRP_IMG_FMT_8BIT	= 0,
	BYRP_IMG_FMT_10BIT	= 1,
	BYRP_IMG_FMT_12BIT	= 2,
	BYRP_IMG_FMT_14BIT	= 3,
	BYRP_IMG_FMT_9BIT	= 4,
	BYRP_IMG_FMT_11BIT	= 5,
	BYRP_IMG_FMT_13BIT	= 6,
	BYRP_IMG_FMT_MAX
};

struct byrp_grid_cfg {
	u32 binning_x; /* @4.10 */
	u32 binning_y; /* @4.10 */
	u32 step_x; /* @10.10, virtual space */
	u32 step_y; /* @10.10, virtual space */
	u32 crop_x; /* 0-32K@15.10, virtual space */
	u32 crop_y; /* 0-24K@15.10, virtual space */
	u32 crop_radial_x;
	u32 crop_radial_y;
};

int byrp_hw_s_reset(struct pablo_mmio *base);
void byrp_hw_s_init(struct pablo_mmio *base, u32 set_id);
void byrp_hw_s_clock(struct pablo_mmio *base, bool on);
unsigned int byrp_hw_is_occurred(unsigned int status, enum byrp_event_type type);
int byrp_hw_wait_idle(struct pablo_mmio *base);
void byrp_hw_dump(void __iomem *base);
void byrp_hw_s_core(struct pablo_mmio *base, u32 num_buffers, u32 set_id,
	struct byrp_param_set *param_set);
void byrp_hw_s_path(struct pablo_mmio *base, u32 set_id,
	struct byrp_param_set *param_set, struct is_byrp_config *config);
void byrp_hw_g_input_param(struct byrp_param_set *param_set, u32 instance, u32 id,
	struct param_dma_input **dma_input, dma_addr_t **input_dva,
	struct is_byrp_config *config);
void byrp_hw_g_output_param(struct byrp_param_set *param_set, u32 instance, u32 id,
	struct param_dma_output **dma_output, dma_addr_t **output_dva);
int byrp_hw_s_rdma_init(struct is_common_dma *dma,
	struct param_dma_input *dma_input, struct param_stripe_input *stripe_input,
	u32 enable, struct is_crop *dma_crop_cfg,
	u32 *sbwc_en, u32 *payload_size, struct is_byrp_config *config);
int byrp_hw_rdma_create(struct is_common_dma *dma, void *base, u32 input_id);
void byrp_hw_s_rdma_corex_id(struct is_common_dma *dma, u32 set_id);
int byrp_hw_s_rdma_addr(struct is_common_dma *dma, dma_addr_t *addr, u32 plane, u32 num_buffers,
	int buf_idx, u32 comp_sbwc_en, u32 payload_size,
	u32 image_addr_offset, u32 header_addr_offset);
int byrp_hw_s_wdma_init(struct is_common_dma *dma, struct byrp_param_set *param_set,
	struct pablo_mmio *base, u32 set_id,
	u32 enable, u32 in_crop_size_x,
	u32 *sbwc_en, u32 *payload_size);
int byrp_hw_wdma_create(struct is_common_dma *dma, void *base, u32 input_id);
void byrp_hw_s_wdma_corex_id(struct is_common_dma *dma, u32 set_id);
int byrp_hw_s_wdma_addr(struct is_common_dma *dma, dma_addr_t *addr, u32 plane, u32 num_buffers, int buf_idx,
	u32 comp_sbwc_en, u32 payload_size, u32 image_addr_offset, u32 header_addr_offset);
int byrp_hw_s_corex_update_type(struct pablo_mmio *base, u32 set_id, u32 type);
void byrp_hw_s_cmdq(struct pablo_mmio *base, u32 set_id, u32 num_buffers, dma_addr_t clh, u32 noh);
int byrp_hw_s_corex_init(struct pablo_mmio *base, bool enable);
unsigned int byrp_hw_g_int0_state(struct pablo_mmio *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int byrp_hw_g_int0_mask(struct pablo_mmio *base);
unsigned int byrp_hw_g_int1_state(struct pablo_mmio *base, bool clear, u32 num_buffers, u32 *irq_state);
unsigned int byrp_hw_g_int1_mask(struct pablo_mmio *base);
void byrp_hw_s_bcrop_size(struct pablo_mmio *base, u32 set_id, u32 bcrop_num, u32 x, u32 y, u32 width, u32 height);
void byrp_hw_s_grid_cfg(struct pablo_mmio *base, struct byrp_grid_cfg *cfg);
void byrp_hw_s_disparity_size(struct pablo_mmio *base, u32 set_id, struct is_hw_size_config *size_config);
void byrp_hw_s_chain_size(struct pablo_mmio *base, u32 set_id, u32 dma_width, u32 dma_height);
void byrp_hw_s_mpc_size(struct pablo_mmio *base, u32 set_id, u32 width, u32 height);
void byrp_hw_s_otf_hdr_size(struct pablo_mmio *base, bool enable, u32 set_id, u32 height);
void byrp_hw_s_block_bypass(struct pablo_mmio *base, u32 set_id);
void byrp_hw_s_strgen(struct pablo_mmio *base, u32 set_id);
u32 byrp_hw_g_rdma_max_cnt(void);
u32 byrp_hw_g_wdma_max_cnt(void);
u32 byrp_hw_g_reg_cnt(void);
void byrp_hw_init_pmio_config(struct pmio_config *cfg);
#endif
