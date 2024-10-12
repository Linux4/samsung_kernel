// SPDX-License-Identifier: GPL-2.0
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

#include <linux/module.h>

#include "pablo-self-test-hw-ip.h"
#include "pablo-self-test-file-io.h"
#include "pablo-framemgr.h"
#include "is-hw.h"
#include "is-core.h"
#include "is-device-ischain.h"

static int pst_set_hw_lme(const char *val, const struct kernel_param *kp);
static int pst_get_hw_lme(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_lme = {
	.set = pst_set_hw_lme,
	.get = pst_get_hw_lme,
};
module_param_cb(test_hw_lme, &pablo_param_ops_hw_lme, NULL, 0644);

#define NUM_OF_LME_PARAM (PARAM_LME_DMA - PARAM_LME_CONTROL + 1)

static struct is_frame *frame_lme;
static u32 lme_param[NUM_OF_LME_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_LME_PARAM];
static struct size_cr_set lme_cr_set;

static const struct lme_param lme_param_preset[] = {
	/* Param set[0]: RDMA */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].dma.cur_input_cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].dma.prev_input_cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].dma.output_cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].dma.sad_output_cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].dma.input_format = DMA_INOUT_FORMAT_Y,
	[0].dma.input_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
	[0].dma.input_order = DMA_INOUT_ORDER_NO,
	[0].dma.input_plane = DMA_INOUT_PLANE_1,
	[0].dma.input_msb = DMA_INOUT_BIT_WIDTH_8BIT - 1, /* last bit of data in memory size */
	[0].dma.cur_input_width = 1008,
	[0].dma.cur_input_height = 756,
	[0].dma.prev_input_width = 1008,
	[0].dma.prev_input_height = 756,
	[0].dma.output_format = DMA_INOUT_FORMAT_BAYER,
	[0].dma.output_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
	[0].dma.output_order = DMA_INOUT_ORDER_NO,
	[0].dma.output_plane = DMA_INOUT_PLANE_1,
	[0].dma.sad_output_plane = DMA_INOUT_PLANE_1,
	[0].dma.output_width = 504,
	[0].dma.output_height = 189,
	[0].dma.output_msb = DMA_INOUT_BIT_WIDTH_8BIT - 1,
	[0].dma.processed_output_cmd = DMA_INPUT_COMMAND_DISABLE,
};

static DECLARE_BITMAP(result, ARRAY_SIZE(lme_param_preset));

static void pst_set_buf_lme(struct is_frame *frame, u32 param_idx)
{
	size_t size[IS_MAX_PLANES];
	u32 align = 32;
	dma_addr_t *dva;

	memset(size, 0x0, sizeof(size));

	switch (PARAM_LME_CONTROL + param_idx) {
	case PARAM_LME_DMA:
		dva = frame->dvaddr_buffer;
		pst_get_size_of_lme_input(&lme_param[param_idx], align, size);
		break;
	case PARAM_LME_CONTROL:
		return;
	default:
		pr_err("%s: invalid param_idx(%d)", __func__, param_idx);
		return;
	}

	if (size[0])
		pb[param_idx] = pst_set_dva(frame, dva, size, GROUP_ID_LME0);
}

static void pst_init_param_lme(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;

	memcpy(lme_param[i++], (u32 *)&lme_param_preset[index].control, PARAMETER_MAX_SIZE);
	memcpy(lme_param[i++], (u32 *)&lme_param_preset[index].dma, PARAMETER_MAX_SIZE);
}

static void pst_set_param_lme(struct is_frame *frame)
{
	int i;

	frame->instance = 0;
	frame->fcount = 1234;
	frame->num_buffers = 1;

	for (i = 0; i < NUM_OF_LME_PARAM; i++) {
		pst_set_param(frame, lme_param[i], PARAM_LME_CONTROL + i);
		pst_set_buf_lme(frame, i);
	}
}

static void pst_clr_param_lme(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_LME_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_lme(struct is_frame *frame, struct size_cr_set *cr_set)
{
	frame->kva_lme_rta_info[PLANE_INDEX_CR_SET] = (u64)cr_set;
}

static const struct pst_callback_ops pst_cb_lme = {
	.init_param = pst_init_param_lme,
	.set_param = pst_set_param_lme,
	.clr_param = pst_clr_param_lme,
	.set_rta_info = pst_set_rta_info_lme,
};

static int pst_set_hw_lme(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val,
			DEV_HW_LME0,
			frame_lme,
			lme_param,
			&lme_cr_set,
			ARRAY_SIZE(lme_param_preset),
			result,
			&pst_cb_lme);
}

static int pst_get_hw_lme(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "LME", ARRAY_SIZE(lme_param_preset), result);
}
