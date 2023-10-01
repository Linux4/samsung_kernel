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

static int pst_set_hw_rgbp(const char *val, const struct kernel_param *kp);
static int pst_get_hw_rgbp(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_rgbp = {
	.set = pst_set_hw_rgbp,
	.get = pst_get_hw_rgbp,
};
module_param_cb(test_hw_rgbp, &pablo_param_ops_hw_rgbp, NULL, 0644);

#define NUM_OF_RGBP_PARAM (PARAM_RGBP_RGB - PARAM_RGBP_CONTROL + 1)

static struct is_frame *frame_rgbp;
static u32 rgbp_param[NUM_OF_RGBP_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_RGBP_PARAM];
static struct size_cr_set rgbp_cr_set;

static const struct rgbp_param rgbp_param_preset_grp[] = {
	/* Param set[0]: Default */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].otf_input.cmd = OTF_INPUT_COMMAND_ENABLE,
	[0].otf_input.format = 0,
	[0].otf_input.bitwidth = 0,
	[0].otf_input.order = 0,
	[0].otf_input.width = 4032,
	[0].otf_input.height = 3024,
	[0].otf_input.bayer_crop_offset_x = 0,
	[0].otf_input.bayer_crop_offset_y = 0,
	[0].otf_input.bayer_crop_width = 4032,
	[0].otf_input.bayer_crop_height = 3024,
	[0].otf_input.source = 0,
	[0].otf_input.physical_width = 0,
	[0].otf_input.physical_height = 0,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE,
	[0].otf_output.format = 2,
	[0].otf_output.bitwidth = 12,
	[0].otf_output.order = 0,
	[0].otf_output.width = 1920,
	[0].otf_output.height = 1440,
	[0].otf_output.crop_offset_x = 0,
	[0].otf_output.crop_offset_y = 0,
	[0].otf_output.crop_width = 0,
	[0].otf_output.crop_height = 0,
	[0].otf_output.crop_enable = 0,

	[0].dma_input.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].dma_input.format = 5,
	[0].dma_input.bitwidth = 10,
	[0].dma_input.order = 15,
	[0].dma_input.plane = 1,
	[0].dma_input.width = 4032,
	[0].dma_input.height = 3024,
	[0].dma_input.dma_crop_offset = 0,
	[0].dma_input.dma_crop_width = 4032,
	[0].dma_input.dma_crop_height = 3024,
	[0].dma_input.bayer_crop_offset_x = 0,
	[0].dma_input.bayer_crop_offset_y = 0,
	[0].dma_input.bayer_crop_width = 4032,
	[0].dma_input.bayer_crop_height = 3024,
	[0].dma_input.scene_mode = 0,
	[0].dma_input.msb = 9,
	[0].dma_input.stride_plane0 = 4032,
	[0].dma_input.stride_plane1 = 4032,
	[0].dma_input.stride_plane2 = 4032,
	[0].dma_input.v_otf_enable = 0,
	[0].dma_input.orientation = 0,
	[0].dma_input.strip_mode = 0,
	[0].dma_input.overlab_width = 0,
	[0].dma_input.strip_count = 0,
	[0].dma_input.strip_max_count = 0,
	[0].dma_input.sequence_id = 0,
	[0].dma_input.sbwc_type = 5,

	[0].stripe_input.total_count = 0,

	[0].hf.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].sf.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].rgb.cmd = DMA_OUTPUT_COMMAND_DISABLE,
};

static const struct rgbp_param rgbp_param_preset[] = {
	/* Param set: STRGEN */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_START,

	[0].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].otf_input.format = 0,
	[0].otf_input.bitwidth = 0,
	[0].otf_input.order = 0,
	[0].otf_input.width = 4032,
	[0].otf_input.height = 3024,
	[0].otf_input.bayer_crop_offset_x = 0,
	[0].otf_input.bayer_crop_offset_y = 0,
	[0].otf_input.bayer_crop_width = 4032,
	[0].otf_input.bayer_crop_height = 3024,
	[0].otf_input.source = 0,
	[0].otf_input.physical_width = 0,
	[0].otf_input.physical_height = 0,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].otf_output.format = 2,
	[0].otf_output.bitwidth = 12,
	[0].otf_output.order = 0,
	[0].otf_output.width = 1920,
	[0].otf_output.height = 1080,
	[0].otf_output.crop_offset_x = 0,
	[0].otf_output.crop_offset_y = 0,
	[0].otf_output.crop_width = 0,
	[0].otf_output.crop_height = 0,
	[0].otf_output.crop_enable = 0,

	[0].dma_input.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].dma_input.format = 5,
	[0].dma_input.bitwidth = 10,
	[0].dma_input.order = 15,
	[0].dma_input.plane = 1,
	[0].dma_input.width = 4032,
	[0].dma_input.height = 3024,
	[0].dma_input.dma_crop_offset = 0,
	[0].dma_input.dma_crop_width = 4032,
	[0].dma_input.dma_crop_height = 3024,
	[0].dma_input.bayer_crop_offset_x = 0,
	[0].dma_input.bayer_crop_offset_y = 0,
	[0].dma_input.bayer_crop_width = 4032,
	[0].dma_input.bayer_crop_height = 4032,
	[0].dma_input.scene_mode = 0,
	[0].dma_input.msb = 9,
	[0].dma_input.stride_plane0 = 4032,
	[0].dma_input.stride_plane1 = 4032,
	[0].dma_input.stride_plane2 = 4032,
	[0].dma_input.v_otf_enable = 0,
	[0].dma_input.orientation = 0,
	[0].dma_input.strip_mode = 0,
	[0].dma_input.overlab_width = 0,
	[0].dma_input.strip_count = 0,
	[0].dma_input.strip_max_count = 0,
	[0].dma_input.sequence_id = 0,
	[0].dma_input.sbwc_type = 0,

	[0].stripe_input.total_count = 0,

	[0].hf.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].sf.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].rgb.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	/* Param set: Cur_image dma + YUV DMA*/
	[1].control.cmd = CONTROL_COMMAND_START,
	[1].control.bypass = 0,
	[1].control.strgen = CONTROL_COMMAND_START,

	[1].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[1].otf_input.format = DMA_INOUT_FORMAT_BAYER,
	[1].otf_input.bitwidth = DMA_INOUT_BIT_WIDTH_10BIT,
	[1].otf_input.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	[1].otf_input.width = 4032,
	[1].otf_input.height = 3024,
	[1].otf_input.bayer_crop_offset_x = 0,
	[1].otf_input.bayer_crop_offset_y = 0,
	[1].otf_input.bayer_crop_width = 4032,
	[1].otf_input.bayer_crop_height = 3024,
	[1].otf_input.source = 0,
	[1].otf_input.physical_width = 0,
	[1].otf_input.physical_height = 0,
	[1].otf_input.offset_x = 0,
	[1].otf_input.offset_y = 0,

	[1].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[1].otf_output.format = DMA_INOUT_FORMAT_YUV422,
	[1].otf_output.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[1].otf_output.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	[1].otf_output.width = 1920,
	[1].otf_output.height = 1444,
	[1].otf_output.crop_offset_x = 0,
	[1].otf_output.crop_offset_y = 0,
	[1].otf_output.crop_width = 0,
	[1].otf_output.crop_height = 0,
	[1].otf_output.crop_enable = 0,

	[1].yuv.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[1].yuv.format = DMA_INOUT_FORMAT_YUV422,
	[1].yuv.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[1].yuv.order = DMA_INOUT_ORDER_CrCb,
	[1].yuv.plane = 2,
	[1].yuv.width = 4032,
	[1].yuv.height = 3024,
	[1].yuv.dma_crop_offset_x = 0,
	[1].yuv.dma_crop_offset_x = 0,
	[1].yuv.dma_crop_width = 4032,
	[1].yuv.dma_crop_width = 3024,
	[1].yuv.crop_enable = 0,
	[1].yuv.msb = 11,
	[1].yuv.stride_plane0 = 4032,
	[1].yuv.stride_plane1 = 4032,
	[1].yuv.stride_plane2 = 4032,
	[1].yuv.sbwc_type = 0,

	/* Param set:Cur_image RGB DMA */
	[2].control.cmd = CONTROL_COMMAND_START,
	[2].control.bypass = 0,
	[2].control.strgen = CONTROL_COMMAND_STOP,

	[2].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[2].otf_input.format = DMA_INOUT_FORMAT_BAYER,
	[2].otf_input.bitwidth = DMA_INOUT_BIT_WIDTH_10BIT,
	[2].otf_input.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	[2].otf_input.width = 4032,
	[2].otf_input.height = 3024,
	[2].otf_input.bayer_crop_offset_x = 0,
	[2].otf_input.bayer_crop_offset_y = 0,
	[2].otf_input.bayer_crop_width = 4032,
	[2].otf_input.bayer_crop_height = 3024,
	[2].otf_input.source = 0,
	[2].otf_input.physical_width = 0,
	[2].otf_input.physical_height = 0,
	[2].otf_input.offset_x = 0,
	[2].otf_input.offset_y = 0,

	[2].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[2].otf_output.format = DMA_INOUT_FORMAT_YUV422,
	[2].otf_output.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[2].otf_output.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	[2].otf_output.width = 1920,
	[2].otf_output.height = 1444,
	[2].otf_output.crop_offset_x = 0,
	[2].otf_output.crop_offset_y = 0,
	[2].otf_output.crop_width = 0,
	[2].otf_output.crop_height = 0,
	[2].otf_output.crop_enable = 0,

	[2].dma_input.cmd = DMA_INPUT_COMMAND_ENABLE,
	[2].dma_input.format = DMA_INOUT_FORMAT_RGB,
	[2].dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_32BIT,
	[2].dma_input.order = DMA_INOUT_ORDER_RGBA,
	[2].dma_input.plane = 1,
	[2].dma_input.width = 4032,
	[2].dma_input.height = 3024,
	[2].dma_input.dma_crop_offset = 0,
	[2].dma_input.dma_crop_width = 4032,
	[2].dma_input.dma_crop_height = 3024,
	[2].dma_input.bayer_crop_offset_x = 0,
	[2].dma_input.bayer_crop_offset_y = 0,
	[2].dma_input.bayer_crop_width = 4032,
	[2].dma_input.bayer_crop_height = 3024,
	[2].dma_input.scene_mode = 0,
	[2].dma_input.msb = DMA_INOUT_BIT_WIDTH_32BIT - 1,
	[2].dma_input.stride_plane0 = 4032,
	[2].dma_input.stride_plane1 = 4032,
	[2].dma_input.stride_plane2 = 4032,
	[2].dma_input.v_otf_enable = 0,
	[2].dma_input.orientation = 0,
	[2].dma_input.strip_mode = 0,
	[2].dma_input.overlab_width = 0,
	[2].dma_input.strip_count = 0,
	[2].dma_input.strip_max_count = 0,
	[2].dma_input.sequence_id = 0,
	[2].dma_input.sbwc_type = 0,

	[2].rgb.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[2].rgb.format = DMA_INOUT_FORMAT_RGB,
	[2].rgb.bitwidth = DMA_INOUT_BIT_WIDTH_32BIT,
	[2].rgb.order = DMA_INOUT_ORDER_RGBA,
	[2].rgb.plane = 1,
	[2].rgb.width = 4032,
	[2].rgb.height = 3024,
	[2].rgb.dma_crop_offset_x = 0,
	[2].rgb.dma_crop_offset_x = 0,
	[2].rgb.dma_crop_width = 4032,
	[2].rgb.dma_crop_width = 3024,
	[2].rgb.crop_enable = 0,
	[2].rgb.msb = DMA_INOUT_BIT_WIDTH_32BIT - 1,
	[2].rgb.stride_plane0 = 4032,
	[2].rgb.stride_plane1 = 4032,
	[2].rgb.stride_plane2 = 4032,
	[2].rgb.sbwc_type = 0,
};

static DECLARE_BITMAP(result, ARRAY_SIZE(rgbp_param_preset));

static void pst_set_size_rgbp(void *in_param, void *out_param)
{
	struct rgbp_param *p = (struct rgbp_param *)rgbp_param;
	struct  param_size_byrp2yuvp *in = (struct param_size_byrp2yuvp *)in_param;
	struct  param_size_byrp2yuvp *out = (struct param_size_byrp2yuvp *)out_param;

	if (!in || !out)
		return;

	p->otf_input.width = in->w_byrp;
	p->otf_input.height = in->h_byrp;

	p->otf_output.width = out->w_rgbp;
	p->otf_output.height = out->h_rgbp;

	p->dma_input.width = in->w_byrp;
	p->dma_input.height = in->h_byrp;
}

static enum pst_blob_node pst_get_blob_node_rgbp(u32 idx)
{
	enum pst_blob_node bn;

	switch (PARAM_RGBP_CONTROL + idx) {
	case PARAM_RGBP_YUV:
		bn = PST_BLOB_RGBP_YUV;
		break;
	case PARAM_RGBP_RGB:
		bn = PST_BLOB_RGBP_RGB;
		break;
	default:
		bn = PST_BLOB_NODE_MAX;
		break;
	}

	return bn;
}

static void pst_set_buf_rgbp(struct is_frame *frame, u32 param_idx)
{
	size_t size[IS_MAX_PLANES];
	u32 align = 32;
	u32 block_w = RGBP_COMP_BLOCK_WIDTH;
	u32 block_h = RGBP_COMP_BLOCK_HEIGHT;
	dma_addr_t *dva;

	memset(size, 0x0, sizeof(size));

	switch (PARAM_RGBP_CONTROL + param_idx) {
	case PARAM_RGBP_DMA_INPUT:
		dva = frame->dvaddr_buffer;
		pst_get_size_of_dma_input(&rgbp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_RGBP_HF:
		dva = frame->dva_rgbp_hf;
		pst_get_size_of_dma_output(&rgbp_param[param_idx], align, size);
		break;
	case PARAM_RGBP_SF:
		dva = frame->dva_rgbp_sf;
		pst_get_size_of_dma_output(&rgbp_param[param_idx], align, size);
		break;
	case PARAM_RGBP_YUV:
		dva = frame->dva_rgbp_yuv;
		pst_get_size_of_dma_output(&rgbp_param[param_idx], align, size);
		break;
	case PARAM_RGBP_RGB:
		dva = frame->dva_rgbp_rgb;
		pst_get_size_of_dma_output(&rgbp_param[param_idx], align, size);
		break;
	case PARAM_RGBP_CONTROL:
	case PARAM_RGBP_OTF_INPUT:
	case PARAM_RGBP_OTF_OUTPUT:
	case PARAM_RGBP_STRIPE_INPUT:
		return;
	default:
		pr_err("%s: invalid param_idx(%d)", __func__, param_idx);
		return;
	}

	if (size[0]) {
		pb[param_idx] = pst_set_dva(frame, dva, size, GROUP_ID_RGBP);
		pst_blob_inject(pst_get_blob_node_rgbp(param_idx), pb[param_idx]);
	}
}

static void pst_init_param_rgbp(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;
	const struct rgbp_param *preset;

	if (type == PST_HW_IP_SINGLE)
		preset = rgbp_param_preset;
	else
		preset = rgbp_param_preset_grp;

	memcpy(rgbp_param[i++], (u32 *)&preset[index].control, PARAMETER_MAX_SIZE);
	memcpy(rgbp_param[i++], (u32 *)&preset[index].otf_input, PARAMETER_MAX_SIZE);
	memcpy(rgbp_param[i++], (u32 *)&preset[index].otf_output, PARAMETER_MAX_SIZE);
	memcpy(rgbp_param[i++], (u32 *)&preset[index].dma_input, PARAMETER_MAX_SIZE);
	memcpy(rgbp_param[i++], (u32 *)&preset[index].stripe_input, PARAMETER_MAX_SIZE);
	memcpy(rgbp_param[i++], (u32 *)&preset[index].hf, PARAMETER_MAX_SIZE);
	memcpy(rgbp_param[i++], (u32 *)&preset[index].sf, PARAMETER_MAX_SIZE);
	memcpy(rgbp_param[i++], (u32 *)&preset[index].yuv, PARAMETER_MAX_SIZE);
	memcpy(rgbp_param[i++], (u32 *)&preset[index].rgb, PARAMETER_MAX_SIZE);
}

static void pst_set_param_rgbp(struct is_frame *frame)
{
	int i = 0;

	frame->instance = 0;
	frame->fcount = 1234;
	frame->num_buffers = 1;

	for (i = 0; i < NUM_OF_RGBP_PARAM; i++) {
		pst_set_param(frame, rgbp_param[i], PARAM_RGBP_CONTROL + i);
		pst_set_buf_rgbp(frame, i);
	}
}

static void pst_clr_param_rgbp(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_RGBP_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_blob_dump(pst_get_blob_node_rgbp(i), pb[i]);

		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_rgbp(struct is_frame *frame, struct size_cr_set *cr_set)
{
	frame->kva_rgbp_rta_info[PLANE_INDEX_CR_SET] = (u64)cr_set;
}

static const struct pst_callback_ops pst_cb_rgbp = {
	.init_param = pst_init_param_rgbp,
	.set_param = pst_set_param_rgbp,
	.clr_param = pst_clr_param_rgbp,
	.set_rta_info = pst_set_rta_info_rgbp,
	.set_size = pst_set_size_rgbp,
};

const struct pst_callback_ops *pst_get_hw_rgbp_cb(void)
{
	return &pst_cb_rgbp;
}

static int pst_set_hw_rgbp(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val,
			DEV_HW_RGBP,
			frame_rgbp,
			rgbp_param,
			&rgbp_cr_set,
			ARRAY_SIZE(rgbp_param_preset),
			result,
			&pst_cb_rgbp);
}

static int pst_get_hw_rgbp(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "RGBP", ARRAY_SIZE(rgbp_param_preset), result);
}
