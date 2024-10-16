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
#include "../pablo-self-test-result.h"
#include "pablo-framemgr.h"
#include "is-hw.h"
#include "is-core.h"
#include "is-device-ischain.h"

static int pst_set_hw_mcfp(const char *val, const struct kernel_param *kp);
static int pst_get_hw_mcfp(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_mcfp = {
	.set = pst_set_hw_mcfp,
	.get = pst_get_hw_mcfp,
};
module_param_cb(test_hw_mcfp, &pablo_param_ops_hw_mcfp, NULL, 0644);

#define NUM_OF_MCFP_PARAM (PARAM_MCFP_YUV - PARAM_MCFP_CONTROL + 1)

static struct is_frame *frame_mcfp;
static u32 mcfp_param[NUM_OF_MCFP_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_MCFP_PARAM];
static struct size_cr_set mcfp_cr_set;
static struct mcfp_size_conf_set {
	u32 size;
	struct is_mcfp_config conf;
} mcfp_size_conf;

static const struct mcfp_param mcfp_param_preset_grp[] = {
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
	[0].otf_input.physical_width = 0,
	[0].otf_input.physical_height = 0,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE,
	[0].otf_output.format = OTF_OUTPUT_FORMAT_YUV422,
	[0].otf_output.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[0].otf_output.order = 2,
	[0].otf_output.width = 1920,
	[0].otf_output.height = 1440,
	[0].otf_output.crop_offset_x = 0,
	[0].otf_output.crop_offset_y = 0,
	[0].otf_output.crop_width = 0,
	[0].otf_output.crop_height = 0,
	[0].otf_output.crop_enable = 0,

	[0].dma_input.cmd = DMA_INPUT_COMMAND_DISABLE,
	[0].dma_input.width = 1920,
	[0].dma_input.height = 1440,

	[0].fto_input.cmd = OTF_INPUT_COMMAND_DISABLE,

	[0].stripe_input.total_count = 0,

	[0].prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].prev_w.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].cur_w.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].drc.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].dp.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].mv.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].mv_mixer.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].sf.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].w.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE,
};

static const struct mcfp_param mcfp_param_preset[] = {
	/* Param set[0]: STRGEN */
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
	[0].dma_input.width = 1920,
	[0].dma_input.height = 1440,

	[0].fto_input.cmd = OTF_INPUT_COMMAND_DISABLE,

	[0].stripe_input.total_count = 0,

	[0].prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].prev_w.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].cur_w.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].drc.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].dp.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].mv.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].mv_mixer.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].sf.cmd = DMA_INPUT_COMMAND_DISABLE,

	[0].w.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[0].yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	/* Param set[1]: cur_img DMA input */
	[1].control.cmd = CONTROL_COMMAND_START,
	[1].control.bypass = 0,
	[1].control.strgen = CONTROL_COMMAND_STOP,

	[1].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[1].otf_input.format = OTF_INPUT_FORMAT_YUV422,
	[1].otf_input.bitwidth = 0,
	[1].otf_input.order = 0,
	[1].otf_input.width = 1920,
	[1].otf_input.height = 1440,
	[1].otf_input.bayer_crop_offset_x = 0,
	[1].otf_input.bayer_crop_offset_y = 0,
	[1].otf_input.bayer_crop_width = 1920,
	[1].otf_input.bayer_crop_height = 1440,
	[1].otf_input.source = 0,
	[1].otf_input.physical_width = 0,
	[1].otf_input.physical_height = 0,
	[1].otf_input.offset_x = 0,
	[1].otf_input.offset_y = 0,

	[1].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[1].otf_output.format = OTF_OUTPUT_FORMAT_YUV422,
	[1].otf_output.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[1].otf_output.order = 0,
	[1].otf_output.width = 1920,
	[1].otf_output.height = 1440,
	[1].otf_output.crop_offset_x = 0,
	[1].otf_output.crop_offset_y = 0,
	[1].otf_output.crop_width = 0,
	[1].otf_output.crop_height = 0,
	[1].otf_output.crop_enable = 0,

	[1].dma_input.cmd = DMA_INPUT_COMMAND_ENABLE,
	[1].dma_input.format = DMA_INOUT_FORMAT_YUV422,
	[1].dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[1].dma_input.order = DMA_INOUT_ORDER_CrCb,
	[1].dma_input.plane = 2,
	[1].dma_input.width = 1920,
	[1].dma_input.height = 1440,
	[1].dma_input.dma_crop_offset = 0,
	[1].dma_input.dma_crop_width = 1920,
	[1].dma_input.dma_crop_height = 1440,
	[1].dma_input.bayer_crop_offset_x = 0,
	[1].dma_input.bayer_crop_offset_y = 0,
	[1].dma_input.bayer_crop_width = 1920,
	[1].dma_input.bayer_crop_height = 1440,
	[1].dma_input.scene_mode = 0,
	[1].dma_input.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[1].dma_input.stride_plane0 = 1920,
	[1].dma_input.stride_plane1 = 1920,
	[1].dma_input.stride_plane2 = 1920,
	[1].dma_input.v_otf_enable = 0,
	[1].dma_input.orientation = 0,
	[1].dma_input.strip_mode = 0,
	[1].dma_input.overlab_width = 0,
	[1].dma_input.strip_count = 0,
	[1].dma_input.strip_max_count = 0,
	[1].dma_input.sequence_id = 0,
	[1].dma_input.sbwc_type = 0,

	[1].fto_input.cmd = OTF_INPUT_COMMAND_DISABLE,

	[1].stripe_input.total_count = 0,

	[1].prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE,

	[1].prev_w.cmd = DMA_INPUT_COMMAND_DISABLE,

	[1].cur_w.cmd = DMA_INPUT_COMMAND_DISABLE,

	[1].drc.cmd = DMA_INPUT_COMMAND_DISABLE,

	[1].dp.cmd = DMA_INPUT_COMMAND_DISABLE,

	[1].mv.cmd = DMA_INPUT_COMMAND_DISABLE,

	[1].mv_mixer.cmd = DMA_INPUT_COMMAND_DISABLE,

	[1].sf.cmd = DMA_INPUT_COMMAND_DISABLE,

	[1].w.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	[1].yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE,

	/* Param set[2]: cur_img + wgt + yuv(video) */
	[2].control.cmd = CONTROL_COMMAND_START,
	[2].control.bypass = 0,
	[2].control.strgen = CONTROL_COMMAND_STOP,

	[2].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[2].otf_input.format = OTF_INPUT_FORMAT_YUV422,
	[2].otf_input.bitwidth = 0,
	[2].otf_input.order = 0,
	[2].otf_input.width = 1920,
	[2].otf_input.height = 1440,
	[2].otf_input.bayer_crop_offset_x = 0,
	[2].otf_input.bayer_crop_offset_y = 0,
	[2].otf_input.bayer_crop_width = 1920,
	[2].otf_input.bayer_crop_height = 1440,
	[2].otf_input.source = 0,
	[2].otf_input.physical_width = 0,
	[2].otf_input.physical_height = 0,
	[2].otf_input.offset_x = 0,
	[2].otf_input.offset_y = 0,

	[2].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[2].otf_output.format = OTF_OUTPUT_FORMAT_YUV422,
	[2].otf_output.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[2].otf_output.order = 0,
	[2].otf_output.width = 1920,
	[2].otf_output.height = 1440,
	[2].otf_output.crop_offset_x = 0,
	[2].otf_output.crop_offset_y = 0,
	[2].otf_output.crop_width = 0,
	[2].otf_output.crop_height = 0,
	[2].otf_output.crop_enable = 0,

	[2].dma_input.cmd = DMA_INPUT_COMMAND_ENABLE,
	[2].dma_input.format = DMA_INOUT_FORMAT_YUV422,
	[2].dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[2].dma_input.order = DMA_INOUT_ORDER_CrCb,
	[2].dma_input.plane = 2,
	[2].dma_input.width = 1920,
	[2].dma_input.height = 1440,
	[2].dma_input.dma_crop_offset = 0,
	[2].dma_input.dma_crop_width = 1920,
	[2].dma_input.dma_crop_height = 1440,
	[2].dma_input.bayer_crop_offset_x = 0,
	[2].dma_input.bayer_crop_offset_y = 0,
	[2].dma_input.bayer_crop_width = 1920,
	[2].dma_input.bayer_crop_height = 1440,
	[2].dma_input.scene_mode = 0,
	[2].dma_input.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[2].dma_input.stride_plane0 = 1920,
	[2].dma_input.stride_plane1 = 1920,
	[2].dma_input.stride_plane2 = 1920,
	[2].dma_input.v_otf_enable = 0,
	[2].dma_input.orientation = 0,
	[2].dma_input.strip_mode = 0,
	[2].dma_input.overlab_width = 0,
	[2].dma_input.strip_count = 0,
	[2].dma_input.strip_max_count = 0,
	[2].dma_input.sequence_id = 0,
	[2].dma_input.sbwc_type = 0,

	[2].fto_input.cmd = OTF_INPUT_COMMAND_DISABLE,

	[2].stripe_input.total_count = 0,

	[2].prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE,

	[2].prev_w.cmd = DMA_INPUT_COMMAND_DISABLE,

	[2].cur_w.cmd = DMA_INPUT_COMMAND_DISABLE,

	[2].drc.cmd = DMA_INPUT_COMMAND_DISABLE,

	[2].dp.cmd = DMA_INPUT_COMMAND_DISABLE,

	[2].mv.cmd = DMA_INPUT_COMMAND_DISABLE,

	[2].mv_mixer.cmd = DMA_INPUT_COMMAND_DISABLE,

	[2].sf.cmd = DMA_INPUT_COMMAND_DISABLE,

	[2].w.cmd = DMA_OUTPUT_COMMAND_ENABLE,

	[2].yuv.cmd = DMA_OUTPUT_COMMAND_DISABLE,
	[2].yuv.format = DMA_INOUT_FORMAT_BAYER_PACKED,
	[2].yuv.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[2].yuv.order = DMA_INOUT_ORDER_CrCb,
	[2].yuv.plane = DMA_INOUT_PLANE_2,
	[2].yuv.width = 1920,
	[2].yuv.height = 1440,
	[2].yuv.dma_crop_offset_x = 0,
	[2].yuv.dma_crop_offset_y = 0,
	[2].yuv.dma_crop_width = 1920,
	[2].yuv.dma_crop_height = 1440,
	[2].yuv.crop_enable = 0,
	[2].yuv.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[2].yuv.stride_plane0 = 1920,
	[2].yuv.stride_plane1 = 1920,
	[2].yuv.stride_plane2 = 1920,
	[2].yuv.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[2].yuv.sbwc_type = NONE,

	/* Param set[3]: cur_img + wgt + yuv(still) */
	[3].control.cmd = CONTROL_COMMAND_START,
	[3].control.bypass = 0,
	[3].control.strgen = CONTROL_COMMAND_STOP,

	[3].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[3].otf_input.format = OTF_INPUT_FORMAT_YUV422,
	[3].otf_input.bitwidth = 0,
	[3].otf_input.order = 0,
	[3].otf_input.width = 1920,
	[3].otf_input.height = 1440,
	[3].otf_input.bayer_crop_offset_x = 0,
	[3].otf_input.bayer_crop_offset_y = 0,
	[3].otf_input.bayer_crop_width = 1920,
	[3].otf_input.bayer_crop_height = 1440,
	[3].otf_input.source = 0,
	[3].otf_input.physical_width = 0,
	[3].otf_input.physical_height = 0,
	[3].otf_input.offset_x = 0,
	[3].otf_input.offset_y = 0,

	[3].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[3].otf_output.format = OTF_OUTPUT_FORMAT_YUV422,
	[3].otf_output.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
	[3].otf_output.order = 0,
	[3].otf_output.width = 1920,
	[3].otf_output.height = 1440,
	[3].otf_output.crop_offset_x = 0,
	[3].otf_output.crop_offset_y = 0,
	[3].otf_output.crop_width = 0,
	[3].otf_output.crop_height = 0,
	[3].otf_output.crop_enable = 0,

	[3].dma_input.cmd = DMA_INPUT_COMMAND_ENABLE,
	[3].dma_input.format = DMA_INOUT_FORMAT_YUV422,
	[3].dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[3].dma_input.order = DMA_INOUT_ORDER_CrCb,
	[3].dma_input.plane = 2,
	[3].dma_input.width = 1920,
	[3].dma_input.height = 1440,
	[3].dma_input.dma_crop_offset = 0,
	[3].dma_input.dma_crop_width = 1920,
	[3].dma_input.dma_crop_height = 1440,
	[3].dma_input.bayer_crop_offset_x = 0,
	[3].dma_input.bayer_crop_offset_y = 0,
	[3].dma_input.bayer_crop_width = 1920,
	[3].dma_input.bayer_crop_height = 1440,
	[3].dma_input.scene_mode = 0,
	[3].dma_input.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[3].dma_input.stride_plane0 = 1920,
	[3].dma_input.stride_plane1 = 1920,
	[3].dma_input.stride_plane2 = 1920,
	[3].dma_input.v_otf_enable = 0,
	[3].dma_input.orientation = 0,
	[3].dma_input.strip_mode = 0,
	[3].dma_input.overlab_width = 0,
	[3].dma_input.strip_count = 0,
	[3].dma_input.strip_max_count = 0,
	[3].dma_input.sequence_id = 0,
	[3].dma_input.sbwc_type = 0,

	[3].fto_input.cmd = OTF_INPUT_COMMAND_DISABLE,

	[3].stripe_input.total_count = 0,

	[3].prev_yuv.cmd = DMA_INPUT_COMMAND_DISABLE,

	[3].prev_w.cmd = DMA_INPUT_COMMAND_DISABLE,

	[3].cur_w.cmd = DMA_INPUT_COMMAND_DISABLE,

	[3].drc.cmd = DMA_INPUT_COMMAND_DISABLE,

	[3].dp.cmd = DMA_INPUT_COMMAND_DISABLE,

	[3].mv.cmd = DMA_INPUT_COMMAND_DISABLE,

	[3].mv_mixer.cmd = DMA_INPUT_COMMAND_DISABLE,

	[3].sf.cmd = DMA_INPUT_COMMAND_DISABLE,

	[3].w.cmd = DMA_OUTPUT_COMMAND_ENABLE,

	[3].yuv.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[3].yuv.format = DMA_INOUT_FORMAT_BAYER_PACKED,
	[3].yuv.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT,
	[3].yuv.order = DMA_INOUT_ORDER_CrCb,
	[3].yuv.plane = DMA_INOUT_PLANE_2,
	[3].yuv.width = 1920,
	[3].yuv.height = 1440,
	[3].yuv.dma_crop_offset_x = 0,
	[3].yuv.dma_crop_offset_y = 0,
	[3].yuv.dma_crop_width = 1920,
	[3].yuv.dma_crop_height = 1440,
	[3].yuv.crop_enable = 0,
	[3].yuv.msb = DMA_INOUT_BIT_WIDTH_12BIT - 1,
	[3].yuv.stride_plane0 = 1920,
	[3].yuv.stride_plane1 = 1920,
	[3].yuv.stride_plane2 = 1920,
	[3].yuv.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[3].yuv.sbwc_type = NONE,

};

static DECLARE_BITMAP(result, ARRAY_SIZE(mcfp_param_preset));

static void pst_set_size_mcfp(void *in_param, void *out_param)
{
	struct mcfp_param *p = (struct mcfp_param *)mcfp_param;
	struct  param_size_byrp2yuvp *in = (struct param_size_byrp2yuvp *)in_param;
	struct  param_size_byrp2yuvp *out = (struct param_size_byrp2yuvp *)out_param;

	if (!in || !out)
		return;

	p->otf_input.width = in->w_rgbp;
	p->otf_input.height = in->h_rgbp;

	p->otf_output.width = out->w_mcfp;
	p->otf_output.height = out->h_mcfp;

	p->dma_input.width = in->w_rgbp;
	p->dma_input.height = in->h_rgbp;
	p->dma_input.dma_crop_offset = 0;
	p->dma_input.dma_crop_width = in->w_rgbp;
	p->dma_input.dma_crop_height = in->h_rgbp;

	p->yuv.width = out->w_mcfp;
	p->yuv.height = out->h_mcfp;
	p->yuv.dma_crop_offset_x = out->x_mcfp;
	p->yuv.dma_crop_offset_y = out->y_mcfp;
	p->yuv.dma_crop_width = out->w_mcfp;
	p->yuv.dma_crop_height = out->h_mcfp;
}

static enum pst_blob_node pst_get_blob_node_mcfp(u32 idx)
{
	enum pst_blob_node bn;

	switch (PARAM_MCFP_CONTROL + idx) {
	case PARAM_MCFP_W:
		bn = PST_BLOB_MCFP_W;
		break;
	case PARAM_MCFP_YUV:
		bn = PST_BLOB_MCFP_YUV;
		break;
	default:
		bn = PST_BLOB_NODE_MAX;
		break;
	}

	return bn;
}

static void pst_set_buf_mcfp(struct is_frame *frame, u32 param_idx)
{
	size_t size[IS_MAX_PLANES];
	u32 align = 32;
	u32 block_w = MCFP_COMP_BLOCK_WIDTH;
	u32 block_h = MCFP_COMP_BLOCK_HEIGHT;
	dma_addr_t *dva;

	memset(size, 0x0, sizeof(size));

	switch (PARAM_MCFP_CONTROL + param_idx) {
	case PARAM_MCFP_DMA_INPUT:
		dva = frame->dvaddr_buffer;
		pst_get_size_of_dma_input(&mcfp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MCFP_PREV_YUV:
		dva = frame->dva_mcfp_prev_yuv;
		pst_get_size_of_dma_input(&mcfp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MCFP_PREV_W:
		dva = frame->dva_mcfp_prev_wgt;
		pst_get_size_of_dma_input(&mcfp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MCFP_CUR_W:
		dva = frame->dva_mcfp_cur_wgt;
		pst_get_size_of_dma_input(&mcfp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MCFP_DRC:
		dva = frame->dva_mcfp_cur_drc;
		pst_get_size_of_dma_input(&mcfp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MCFP_DP:
		dva = frame->dva_mcfp_prev_drc;
		pst_get_size_of_dma_input(&mcfp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MCFP_MV:
		dva = frame->dva_mcfp_motion;
		pst_get_size_of_dma_input(&mcfp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MCFP_SF:
		dva = frame->dva_mcfp_sat_flag;
		pst_get_size_of_dma_input(&mcfp_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_MCFP_W:
		dva = frame->dva_mcfp_wgt;
		pst_get_size_of_dma_output(&mcfp_param[param_idx], align, size);
		break;
	case PARAM_MCFP_YUV:
		dva = frame->dva_mcfp_yuv;
		pst_get_size_of_dma_output(&mcfp_param[param_idx], align, size);
		break;
	case PARAM_MCFP_CONTROL:
	case PARAM_MCFP_OTF_INPUT:
	case PARAM_MCFP_OTF_OUTPUT:
	case PARAM_MCFP_FTO_INPUT:
	case PARAM_MCFP_STRIPE_INPUT:
	case PARAM_MCFP_MVMIXER:
		return;
	default:
		pr_err("%s: invalid param_idx(%d)", __func__, param_idx);
		return;
	}

	if (size[0]) {
		pb[param_idx] = pst_set_dva(frame, dva, size, GROUP_ID_MCFP);
		pst_blob_inject(pst_get_blob_node_mcfp(param_idx), pb[param_idx]);
	}
}

static void pst_init_param_mcfp(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;
	const struct mcfp_param *preset;

	if (type == PST_HW_IP_SINGLE)
		preset = mcfp_param_preset;
	else
		preset = mcfp_param_preset_grp;

	memcpy(mcfp_param[i++], (u32 *)&preset[index].control, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].otf_input, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].otf_output, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].dma_input, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].fto_input, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].stripe_input, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].prev_yuv, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].prev_w, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].cur_w, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].drc, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].dp, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].mv, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].mv_mixer, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].sf, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].w, PARAMETER_MAX_SIZE);
	memcpy(mcfp_param[i++], (u32 *)&preset[index].yuv, PARAMETER_MAX_SIZE);
}

static void pst_set_conf_mcfp(struct mcfp_param *param, struct is_frame *frame)
{
	if (param->w.cmd == DMA_INPUT_COMMAND_ENABLE)
		mcfp_size_conf.conf.mixer_enable = 1;
	else
		mcfp_size_conf.conf.mixer_enable = 0;

	if (param->yuv.cmd == DMA_INPUT_COMMAND_ENABLE)
		mcfp_size_conf.conf.still_en = 1;
	else
		mcfp_size_conf.conf.still_en = 0;

	frame->kva_mcfp_rta_info[PLANE_INDEX_CONFIG] = (u64)&mcfp_size_conf;
}

static void pst_set_param_mcfp(struct is_frame *frame)
{
	int i;

	frame->instance = 0;
	frame->fcount = 1234;
	frame->num_buffers = 1;

	for (i = 0; i < NUM_OF_MCFP_PARAM; i++) {
		pst_set_param(frame, mcfp_param[i], PARAM_MCFP_CONTROL + i);
		pst_set_buf_mcfp(frame, i);
	}

	pst_set_conf_mcfp((struct mcfp_param *)mcfp_param, frame);
}

static void pst_clr_param_mcfp(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_MCFP_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_blob_dump(pst_get_blob_node_mcfp(i), pb[i]);

		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_mcfp(struct is_frame *frame, struct size_cr_set *cr_set)
{
	frame->kva_mcfp_rta_info[PLANE_INDEX_CR_SET] = (u64)cr_set;
}

static const struct pst_callback_ops pst_cb_mcfp = {
	.init_param = pst_init_param_mcfp,
	.set_param = pst_set_param_mcfp,
	.clr_param = pst_clr_param_mcfp,
	.set_rta_info = pst_set_rta_info_mcfp,
	.set_size = pst_set_size_mcfp,
};

const struct pst_callback_ops *pst_get_hw_mcfp_cb(void)
{
	return &pst_cb_mcfp;
}

static int pst_set_hw_mcfp(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val,
			DEV_HW_MCFP,
			frame_mcfp,
			mcfp_param,
			&mcfp_cr_set,
			ARRAY_SIZE(mcfp_param_preset),
			result,
			&pst_cb_mcfp);
}

static int pst_get_hw_mcfp(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "MCFP", ARRAY_SIZE(mcfp_param_preset), result);
}
