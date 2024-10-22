/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#ifndef __MTK_MML_FG_FW_H__
#define __MTK_MML_FG_FW_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include "mtk-mml-core.h"
#include "mtk-mml-pq.h"
#include "mtk-mml-pq-core.h"

struct mml_pq_fg_alg_data {
	struct mml_pq_film_grain_params *metadata;
	bool is_yuv_444;
	s32 bit_depth;
	u32 random_register;
	s32 grain_center;
	s32 grain_min;
	s32 grain_max;
	s32 luma_grain_width;
	s32 luma_grain_height;
	s32 chroma_grain_width;
	s32 chroma_grain_height;
	s32 luma_grain_size;
	s32 cb_grain_size;
	s32 cr_grain_size;

	char *allocated_grain_y_va;
	char *allocated_grain_cb_va;
	char *allocated_grain_cr_va;
	char *allocated_scaling_va;
};

void mml_pq_fg_calc(struct mml_pq_dma_buffer **lut,
	struct mml_pq_film_grain_params *metadata, bool isYUV444, s32 bitDepth);

u32 mml_pq_fg_get_pps0(struct mml_pq_film_grain_params *metadata);
u32 mml_pq_fg_get_pps1(struct mml_pq_film_grain_params *metadata);
u32 mml_pq_fg_get_pps2(struct mml_pq_film_grain_params *metadata);
u32 mml_pq_fg_get_pps3(struct mml_pq_film_grain_params *metadata);

#endif	/* __MTK_MML_FG_FW_H__ */
