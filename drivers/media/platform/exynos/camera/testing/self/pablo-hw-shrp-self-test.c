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

static int pst_set_hw_shrp(const char *val, const struct kernel_param *kp);
static int pst_get_hw_shrp(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_shrp = {
	.set = pst_set_hw_shrp,
	.get = pst_get_hw_shrp,
};
module_param_cb(test_hw_shrp, &pablo_param_ops_hw_shrp, NULL, 0644);

#define NUM_OF_SHRP_PARAM (PARAM_SHRP_SEG - PARAM_SHRP_CONTROL + 1)

static struct is_frame *frame_shrp;
static u32 shrp_param[NUM_OF_SHRP_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_SHRP_PARAM];
static struct size_cr_set shrp_cr_set;

static const struct shrp_param shrp_param_preset_grp[] = {
	/* Param set[0]: Default */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].otf_input.format = OTF_INPUT_FORMAT_YUV422,
	[0].otf_input.bitwidth = 0,
	[0].otf_input.order = 0,
	[0].otf_input.width = 1920,
	[0].otf_input.height = 1440,
	[0].otf_input.bayer_crop_offset_x = 0,
	[0].otf_input.bayer_crop_offset_y = 0,
	[0].otf_input.bayer_crop_width = 1920,
	[0].otf_input.bayer_crop_height = 1440,
	[0].otf_input.source = 0,
	[0].otf_input.physical_width = 0,
	[0].otf_input.physical_height = 0,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].otf_output.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].otf_output.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].otf_output.order = 0,
	[0].otf_output.width = 1920,
	[0].otf_output.height = 1440,
	[0].otf_output.crop_offset_x = 0,
	[0].otf_output.crop_offset_y = 0,
	[0].otf_output.crop_width = 0,
	[0].otf_output.crop_height = 0,
	[0].otf_output.crop_enable = 0,

	[0].dma_input.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].fto_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,

	[0].stripe_input.total_count = 0,

	[0].hf.cmd = DMA_INPUT_COMMAND_DISABLE,
};

static const struct shrp_param shrp_param_preset[] = {
	/* Param set[0]: Default */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_START,

	[0].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].otf_input.format = OTF_INPUT_FORMAT_YUV422,
	[0].otf_input.bitwidth = 0,
	[0].otf_input.order = 0,
	[0].otf_input.width = 1920,
	[0].otf_input.height = 1440,
	[0].otf_input.bayer_crop_offset_x = 0,
	[0].otf_input.bayer_crop_offset_y = 0,
	[0].otf_input.bayer_crop_width = 1920,
	[0].otf_input.bayer_crop_height = 1440,
	[0].otf_input.source = 0,
	[0].otf_input.physical_width = 0,
	[0].otf_input.physical_height = 0,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].otf_output.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].otf_output.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].otf_output.order = 0,
	[0].otf_output.width = 1920,
	[0].otf_output.height = 1440,
	[0].otf_output.crop_offset_x = 0,
	[0].otf_output.crop_offset_y = 0,
	[0].otf_output.crop_width = 0,
	[0].otf_output.crop_height = 0,
	[0].otf_output.crop_enable = 0,

	[0].dma_input.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].fto_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,

	[0].stripe_input.total_count = 0,

	[0].hf.cmd = DMA_INPUT_COMMAND_DISABLE,
};

static DECLARE_BITMAP(result, ARRAY_SIZE(shrp_param_preset));

static void pst_set_size_shrp(void *in_param, void *out_param)
{
	struct shrp_param *p = (struct shrp_param *)shrp_param;
	struct  param_size_byrp2yuvp *in = (struct param_size_byrp2yuvp *)in_param;
	struct  param_size_byrp2yuvp *out = (struct param_size_byrp2yuvp *)out_param;

	if (!in || !out)
		return;

	p->otf_input.width = in->w_mcfp;
	p->otf_input.height = in->h_mcfp;

	p->otf_output.width = out->w_yuvp;
	p->otf_output.height = out->h_yuvp;
}

static enum pst_blob_node pst_get_blob_node_shrp(u32 idx)
{
	enum pst_blob_node bn;

	switch (PARAM_SHRP_CONTROL + idx) {
	/* TODO: DMA in/out */
	default:
		bn = PST_BLOB_NODE_MAX;
		break;
	}

	return bn;
}

static void pst_set_buf_shrp(struct is_frame *frame, u32 param_idx)
{
	size_t size[IS_MAX_PLANES];
	dma_addr_t *dva;

	memset(size, 0x0, sizeof(size));

	switch (PARAM_SHRP_CONTROL + param_idx) {
	/* TODO: DMA in/out */
	case PARAM_SHRP_CONTROL:
	case PARAM_SHRP_OTF_INPUT:
	case PARAM_SHRP_OTF_OUTPUT:
		return;
	default:
		pr_err("%s: invalid param_idx(%d)", __func__, param_idx);
		return;
	}

	if (size[0]) {
		pb[param_idx] = pst_set_dva(frame, dva, size, GROUP_ID_SHRP);
		pst_blob_inject(pst_get_blob_node_shrp(param_idx), pb[param_idx]);
	}
}

static void pst_init_param_shrp(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;
	const struct shrp_param *preset;

	if (type == PST_HW_IP_SINGLE)
		preset = shrp_param_preset;
	else
		preset = shrp_param_preset_grp;

	memcpy(shrp_param[i++], (u32 *)&preset[index].control, PARAMETER_MAX_SIZE);
	/* TODO: param */
	memcpy(shrp_param[i++], (u32 *)&preset[index].otf_input, PARAMETER_MAX_SIZE);
	memcpy(shrp_param[i++], (u32 *)&preset[index].otf_output, PARAMETER_MAX_SIZE);
}

static void pst_set_param_shrp(struct is_frame *frame)
{
	int i;

	frame->instance = 0;
	frame->fcount = 1234;
	frame->num_buffers = 1;

	for (i = 0; i < NUM_OF_SHRP_PARAM; i++) {
		pst_set_param(frame, shrp_param[i], PARAM_SHRP_CONTROL + i);
		pst_set_buf_shrp(frame, i);
	}
}

static void pst_clr_param_shrp(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_SHRP_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_blob_dump(pst_get_blob_node_shrp(i), pb[i]);

		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_shrp(struct is_frame *frame, struct size_cr_set *cr_set)
{
	frame->kva_shrp_rta_info[PLANE_INDEX_CR_SET] = (u64)cr_set;
}

static const struct pst_callback_ops pst_cb_shrp = {
	.init_param = pst_init_param_shrp,
	.set_param = pst_set_param_shrp,
	.clr_param = pst_clr_param_shrp,
	.set_rta_info = pst_set_rta_info_shrp,
	.set_size = pst_set_size_shrp,
};

const struct pst_callback_ops *pst_get_hw_shrp_cb(void)
{
	return &pst_cb_shrp;
}

static int pst_set_hw_shrp(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val,
			DEV_HW_SHRP,
			frame_shrp,
			shrp_param,
			&shrp_cr_set,
			ARRAY_SIZE(shrp_param_preset),
			result,
			&pst_cb_shrp);
}

static int pst_get_hw_shrp(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "SHRP", ARRAY_SIZE(shrp_param_preset), result);
}
