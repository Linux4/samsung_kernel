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

static int pst_set_hw_yuvp(const char *val, const struct kernel_param *kp);
static int pst_get_hw_yuvp(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_yuvp = {
	.set = pst_set_hw_yuvp,
	.get = pst_get_hw_yuvp,
};
module_param_cb(test_hw_yuvp, &pablo_param_ops_hw_yuvp, NULL, 0644);

#define NUM_OF_YUVP_PARAM (PARAM_YUVP_YUV - PARAM_YUVP_CONTROL + 1)

static struct is_frame *frame_yuvp;
static u32 yuvp_param[NUM_OF_YUVP_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_YUVP_PARAM];
static struct size_cr_set yuvp_cr_set;
static struct yuvp_size_conf_set {
	u32 size;
	struct is_yuvp_config conf;
} yuvp_size_conf;

static const struct yuvp_param yuvp_param_preset_grp[] = {
	/* Param set[0]: Default */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].otf_input.cmd = OTF_INPUT_COMMAND_ENABLE,
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
	[0].otf_input.physical_width = 1920,
	[0].otf_input.physical_height = 1440,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE,
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
	[0].dma_input.format = 0,
	[0].dma_input.bitwidth = 0,
	[0].dma_input.order = 0,
	[0].dma_input.plane = 0,
	[0].dma_input.width = 1920,
	[0].dma_input.height = 1440,
	[0].dma_input.dma_crop_offset = 0,
	[0].dma_input.dma_crop_width = 1920,
	[0].dma_input.dma_crop_height = 1440,
	[0].dma_input.bayer_crop_offset_x = 0,
	[0].dma_input.bayer_crop_offset_y = 0,
	[0].dma_input.bayer_crop_width = 1920,
	[0].dma_input.bayer_crop_height = 1440,
	[0].dma_input.scene_mode = 0,
	[0].dma_input.msb = DMA_INOUT_BIT_WIDTH_10BIT - 1,
	[0].dma_input.stride_plane0 = 1920,
	[0].dma_input.stride_plane1 = 1920,
	[0].dma_input.stride_plane2 = 1920,
	[0].dma_input.v_otf_enable = 0,
	[0].dma_input.orientation = 0,
	[0].dma_input.strip_mode = 0,
	[0].dma_input.overlab_width = 0,
	[0].dma_input.strip_count = 0,
	[0].dma_input.strip_max_count = 0,
	[0].dma_input.sequence_id = 0,
	[0].dma_input.sbwc_type = 0,

	[0].fto_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,

	[0].stripe_input.total_count = 0,

	[0].seg.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].drc.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].clahe.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].svhist.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE,
};

static const struct yuvp_param yuvp_param_preset[] = {
	/* Param set: STRGEN */
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
	[0].otf_input.physical_width = 1920,
	[0].otf_input.physical_height = 1440,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].otf_output.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].otf_output.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].otf_output.order = 0,
	[0].otf_output.width = 1920,
	[0].otf_output.height = 1080,
	[0].otf_output.crop_offset_x = 0,
	[0].otf_output.crop_offset_y = 0,
	[0].otf_output.crop_width = 0,
	[0].otf_output.crop_height = 0,
	[0].otf_output.crop_enable = 0,

	[0].dma_input.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].dma_input.format = 0,
	[0].dma_input.bitwidth = 0,
	[0].dma_input.order = 0,
	[0].dma_input.plane = 0,
	[0].dma_input.width = 1920,
	[0].dma_input.height = 1440,
	[0].dma_input.dma_crop_offset = 0,
	[0].dma_input.dma_crop_width = 1920,
	[0].dma_input.dma_crop_height = 1440,
	[0].dma_input.bayer_crop_offset_x = 0,
	[0].dma_input.bayer_crop_offset_y = 0,
	[0].dma_input.bayer_crop_width = 1920,
	[0].dma_input.bayer_crop_height = 1440,
	[0].dma_input.scene_mode = 0,
	[0].dma_input.msb = DMA_INOUT_BIT_WIDTH_10BIT - 1,
	[0].dma_input.stride_plane0 = 1920,
	[0].dma_input.stride_plane1 = 1920,
	[0].dma_input.stride_plane2 = 1920,
	[0].dma_input.v_otf_enable = 0,
	[0].dma_input.orientation = 0,
	[0].dma_input.strip_mode = 0,
	[0].dma_input.overlab_width = 0,
	[0].dma_input.strip_count = 0,
	[0].dma_input.strip_max_count = 0,
	[0].dma_input.sequence_id = 0,
	[0].dma_input.sbwc_type = 0,

	[0].fto_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,

	[0].stripe_input.total_count = 0,

	[0].seg.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].drc.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].clahe.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].svhist.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE,
};

static DECLARE_BITMAP(result, ARRAY_SIZE(yuvp_param_preset));

static void pst_set_size_yuvp(void *in_param, void *out_param)
{
	struct yuvp_param *p = (struct yuvp_param *)yuvp_param;
	struct  param_size_byrp2yuvp *in = (struct param_size_byrp2yuvp *)in_param;
	struct  param_size_byrp2yuvp *out = (struct param_size_byrp2yuvp *)out_param;

	if (!in || !out)
		return;

	p->otf_input.width = in->w_mcfp;
	p->otf_input.height = in->h_mcfp;

	p->otf_output.width = out->w_yuvp;
	p->otf_output.height = out->h_yuvp;

	p->dma_input.width = in->w_mcfp;
	p->dma_input.height = in->h_mcfp;
}

static enum pst_blob_node pst_get_blob_node_yuvp(u32 idx)
{
	enum pst_blob_node bn;

	switch (PARAM_YUVP_CONTROL + idx) {
	case PARAM_YUVP_YUV:
		bn = PST_BLOB_YUVP_YUV;
		break;
	default:
		bn = PST_BLOB_NODE_MAX;
		break;
	}

	return bn;
}

static void pst_set_buf_yuvp(struct is_frame *frame, u32 param_idx)
{
	size_t size[IS_MAX_PLANES];
	u32 align = 32;
	u32 block_w = YUVP_COMP_BLOCK_WIDTH;
	u32 block_h = YUVP_COMP_BLOCK_HEIGHT;
	dma_addr_t *dva;

	memset(size, 0x0, sizeof(size));

	switch (PARAM_YUVP_CONTROL + param_idx) {
	case PARAM_YUVP_DMA_INPUT:
		dva = frame->dvaddr_buffer;
		pst_get_size_of_dma_input(&yuvp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_YUVP_SEG:
		dva = frame->ixscmapTargetAddress;
		pst_get_size_of_dma_input(&yuvp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_YUVP_DRC:
		dva = frame->ypdgaTargetAddress;
		pst_get_size_of_dma_input(&yuvp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_YUVP_CLAHE:
		dva = frame->ypclaheTargetAddress;
		pst_get_size_of_dma_input(&yuvp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_YUVP_SVHIST:
		dva = frame->ypsvhistTargetAddress;
		pst_get_size_of_dma_output(&yuvp_param[param_idx], align, size);
		break;
	case PARAM_YUVP_YUV:
		dva = frame->ypnrdsTargetAddress;
		pst_get_size_of_dma_output(&yuvp_param[param_idx], align, size);
		break;
	case PARAM_YUVP_CONTROL:
	case PARAM_YUVP_OTF_INPUT:
	case PARAM_YUVP_OTF_OUTPUT:
	case PARAM_YUVP_FTO_OUTPUT:
	case PARAM_YUVP_STRIPE_INPUT:
		return;
	default:
		pr_err("%s: invalid param_idx(%d)", __func__, param_idx);
		return;
	}

	if (size[0]) {
		pb[param_idx] = pst_set_dva(frame, dva, size, GROUP_ID_YUVP);
		pst_blob_inject(pst_get_blob_node_yuvp(param_idx), pb[param_idx]);
	}
}

static void pst_init_param_yuvp(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;
	const struct yuvp_param *preset;

	if (type == PST_HW_IP_SINGLE)
		preset = yuvp_param_preset;
	else
		preset = yuvp_param_preset_grp;

	memcpy(yuvp_param[i++], (u32 *)&preset[index].control, PARAMETER_MAX_SIZE);
	memcpy(yuvp_param[i++], (u32 *)&preset[index].otf_input, PARAMETER_MAX_SIZE);
	memcpy(yuvp_param[i++], (u32 *)&preset[index].otf_output, PARAMETER_MAX_SIZE);
	memcpy(yuvp_param[i++], (u32 *)&preset[index].dma_input, PARAMETER_MAX_SIZE);
	memcpy(yuvp_param[i++], (u32 *)&preset[index].fto_output, PARAMETER_MAX_SIZE);
	memcpy(yuvp_param[i++], (u32 *)&preset[index].stripe_input, PARAMETER_MAX_SIZE);
	memcpy(yuvp_param[i++], (u32 *)&preset[index].seg, PARAMETER_MAX_SIZE);
	memcpy(yuvp_param[i++], (u32 *)&preset[index].drc, PARAMETER_MAX_SIZE);
	memcpy(yuvp_param[i++], (u32 *)&preset[index].clahe, PARAMETER_MAX_SIZE);
	memcpy(yuvp_param[i++], (u32 *)&preset[index].svhist, PARAMETER_MAX_SIZE);
	memcpy(yuvp_param[i++], (u32 *)&preset[index].yuv, PARAMETER_MAX_SIZE);
}

static void pst_set_conf_yuvp(struct yuvp_param *param, struct is_frame *frame)
{
	if (param->seg.cmd == DMA_INPUT_COMMAND_DISABLE) {
		yuvp_size_conf.conf.yuvnr_contents_aware_isp_en = 0;
		yuvp_size_conf.conf.ccm_contents_aware_isp_en = 0;
		yuvp_size_conf.conf.sharpen_contents_aware_isp_en = 0;
	} else {
		yuvp_size_conf.conf.yuvnr_contents_aware_isp_en = 1;
		yuvp_size_conf.conf.ccm_contents_aware_isp_en = 1;
		yuvp_size_conf.conf.sharpen_contents_aware_isp_en = 1;
	}

	if (param->drc.cmd == DMA_INPUT_COMMAND_DISABLE)
		yuvp_size_conf.conf.drc_bypass = 1;
	else
		yuvp_size_conf.conf.drc_bypass = 0;

	if (param->clahe.cmd == DMA_INPUT_COMMAND_DISABLE)
		yuvp_size_conf.conf.clahe_bypass = 1;
	else
		yuvp_size_conf.conf.clahe_bypass = 0;

	if (param->svhist.cmd == DMA_INPUT_COMMAND_DISABLE)
		yuvp_size_conf.conf.svhist_bypass = 1;
	else
		yuvp_size_conf.conf.svhist_bypass = 0;

	frame->kva_yuvp_rta_info[PLANE_INDEX_CONFIG] = (u64)&yuvp_size_conf;
}

static void pst_set_param_yuvp(struct is_frame *frame)
{
	int i;

	frame->instance = 0;
	frame->fcount = 1234;
	frame->num_buffers = 1;

	for (i = 0; i < NUM_OF_YUVP_PARAM; i++) {
		pst_set_param(frame, yuvp_param[i], PARAM_YUVP_CONTROL + i);
		pst_set_buf_yuvp(frame, i);
	}

	pst_set_conf_yuvp((struct yuvp_param *)yuvp_param, frame);
}

static void pst_clr_param_yuvp(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_YUVP_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_blob_dump(pst_get_blob_node_yuvp(i), pb[i]);

		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_yuvp(struct is_frame *frame, struct size_cr_set *cr_set)
{
	frame->kva_yuvp_rta_info[PLANE_INDEX_CR_SET] = (u64)cr_set;
}

static const struct pst_callback_ops pst_cb_yuvp = {
	.init_param = pst_init_param_yuvp,
	.set_param = pst_set_param_yuvp,
	.clr_param = pst_clr_param_yuvp,
	.set_rta_info = pst_set_rta_info_yuvp,
	.set_size = pst_set_size_yuvp,
};

const struct pst_callback_ops *pst_get_hw_yuvp_cb(void)
{
	return &pst_cb_yuvp;
}

static int pst_set_hw_yuvp(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val,
			DEV_HW_YPP,
			frame_yuvp,
			yuvp_param,
			&yuvp_cr_set,
			ARRAY_SIZE(yuvp_param_preset),
			result,
			&pst_cb_yuvp);
}

static int pst_get_hw_yuvp(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "YUVP", ARRAY_SIZE(yuvp_param_preset), result);
}
