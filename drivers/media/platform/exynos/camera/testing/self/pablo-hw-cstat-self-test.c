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

static int pst_set_hw_cstat(const char *val, const struct kernel_param *kp);
static int pst_get_hw_cstat(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_cstat = {
	.set = pst_set_hw_cstat,
	.get = pst_get_hw_cstat,
};
module_param_cb(test_hw_cstat, &pablo_param_ops_hw_cstat, NULL, 0644);

#define NUM_OF_CSTAT_PARAM (PARAM_CSTAT_CDS - PARAM_CSTAT_CONTROL + 1)

static struct is_frame *frame_cstat;
static u32 cstat_param[NUM_OF_CSTAT_PARAM][PARAMETER_MAX_MEMBER];
static struct is_priv_buf *pb[NUM_OF_CSTAT_PARAM];
static struct size_cr_set cstat_cr_set;

static const struct cstat_param cstat_param_preset[] = {
	/* Param set[0]: DMA IN + LMEDS0/FDPIG/DRC DMA OUT */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_STOP,

	[0].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].otf_input.format = DMA_INOUT_FORMAT_BAYER,
	[0].otf_input.bitwidth = DMA_INOUT_BIT_WIDTH_10BIT,
	[0].otf_input.order = 0,
	[0].otf_input.width = 4032,
	[0].otf_input.height = 3024,
	[0].otf_input.bayer_crop_offset_x = 0,
	[0].otf_input.bayer_crop_offset_y = 0,
	[0].otf_input.bayer_crop_width = 4032,
	[0].otf_input.bayer_crop_height = 3024,
	[0].otf_input.source = 0,
	[0].otf_input.physical_width = 4032,
	[0].otf_input.physical_height = 3024,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].dma_input.cmd = DMA_INPUT_COMMAND_ENABLE,
	[0].dma_input.format = DMA_INOUT_FORMAT_BAYER,
	[0].dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_10BIT,
	[0].dma_input.order = OTF_INPUT_ORDER_BAYER_GB_RG,
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
	[0].dma_input.msb = DMA_INOUT_BIT_WIDTH_10BIT - 1,
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

	[0].lme_ds0.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].lme_ds0.format = DMA_INOUT_FORMAT_Y,
	[0].lme_ds0.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
	[0].lme_ds0.order = DMA_INOUT_FORMAT_BAYER,
	[0].lme_ds0.plane = DMA_INOUT_PLANE_1,
	[0].lme_ds0.width = 1008,
	[0].lme_ds0.height = 756,
	[0].lme_ds0.dma_crop_offset_x = 0,
	[0].lme_ds0.dma_crop_offset_y = 0,
	[0].lme_ds0.dma_crop_width = 1008,
	[0].lme_ds0.dma_crop_height = 756,
	[0].lme_ds0.crop_enable = 0,
	[0].lme_ds0.msb = DMA_INOUT_BIT_WIDTH_8BIT - 1,
	[0].lme_ds0.stride_plane0 = 0,
	[0].lme_ds0.stride_plane1 = 0,
	[0].lme_ds0.stride_plane2 = 0,
	[0].lme_ds0.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].lme_ds0.sbwc_type = NONE,

	[0].fdpig.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].fdpig.format = DMA_INOUT_FORMAT_RGB,
	[0].fdpig.bitwidth = DMA_INOUT_BIT_WIDTH_32BIT,
	[0].fdpig.order = DMA_INOUT_ORDER_BGRA,
	[0].fdpig.plane = DMA_INOUT_PLANE_1,
	[0].fdpig.width = 320,
	[0].fdpig.height = 240,
	[0].fdpig.dma_crop_offset_x = 0,
	[0].fdpig.dma_crop_offset_y = 0,
	[0].fdpig.dma_crop_width = 2016,
	[0].fdpig.dma_crop_height = 1512,
	[0].fdpig.crop_enable = 0,
	[0].fdpig.msb = DMA_INOUT_BIT_WIDTH_32BIT - 1,
	[0].fdpig.stride_plane0 = 640,
	[0].fdpig.stride_plane1 = 640,
	[0].fdpig.stride_plane2 = 640,
	[0].fdpig.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].fdpig.sbwc_type = NONE,

	[0].drc.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].drc.format = DMA_INOUT_FORMAT_Y,
	[0].drc.bitwidth = DMA_INOUT_BIT_WIDTH_8BIT,
	[0].drc.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	[0].drc.plane = DMA_INOUT_PLANE_1,
	[0].drc.width = 1024,
	[0].drc.height = 256,
	[0].drc.dma_crop_offset_x = 0,
	[0].drc.dma_crop_offset_y = 0,
	[0].drc.dma_crop_width = 1024,
	[0].drc.dma_crop_height = 256,
	[0].drc.crop_enable = 0,
	[0].drc.msb = DMA_INOUT_BIT_WIDTH_8BIT - 1,
	[0].drc.stride_plane0 = 0,
	[0].drc.stride_plane1 = 0,
	[0].drc.stride_plane2 = 0,
	[0].drc.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].drc.sbwc_type = NONE,
};

static DECLARE_BITMAP(result, ARRAY_SIZE(cstat_param_preset));

static enum pst_blob_node pst_get_blob_node_cstat(u32 idx)
{
	enum pst_blob_node bn;

	switch (PARAM_CSTAT_CONTROL + idx) {
	case PARAM_CSTAT_DMA_INPUT:
		bn = PST_BLOB_CSTAT_DMA_INPUT;
		break;
	case PARAM_CSTAT_FDPIG:
		bn = PST_BLOB_CSTAT_FDPIG;
		break;
	case PARAM_CSTAT_LME_DS0:
		bn = PST_BLOB_CSTAT_LMEDS0;
		break;
	case PARAM_CSTAT_DRC:
		bn = PST_BLOB_CSTAT_DRC;
		break;
	default:
		bn = PST_BLOB_NODE_MAX;
		break;
	}

	return bn;
}

static void pst_set_buf_cstat(struct is_frame *frame, u32 param_idx)
{
	size_t size[IS_MAX_PLANES];
	u32 align = 32;
	u32 block_w = CSTAT_COMP_BLOCK_WIDTH;
	u32 block_h = CSTAT_COMP_BLOCK_HEIGHT;
	dma_addr_t *dva;

	memset(size, 0x0, sizeof(size));

	switch (PARAM_CSTAT_CONTROL + param_idx) {
	case PARAM_CSTAT_DMA_INPUT:
		dva = frame->dvaddr_buffer;
		pst_get_size_of_dma_input(&cstat_param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_CSTAT_LME_DS0:
		dva = frame->txldsTargetAddress[0];
		pst_get_size_of_dma_output(&cstat_param[param_idx], align, size);
		break;
	case PARAM_CSTAT_LME_DS1:
		dva = frame->dva_cstat_lmeds1[0];
		pst_get_size_of_dma_output(&cstat_param[param_idx], align, size);
		break;
	case PARAM_CSTAT_FDPIG:
		dva = frame->efdTargetAddress[0];
		pst_get_size_of_dma_output(&cstat_param[param_idx], align, size);
		break;
	case PARAM_CSTAT_DRC:
		dva = frame->txdgrTargetAddress[0];
		pst_get_size_of_dma_output(&cstat_param[param_idx], align, size);
		break;
	case PARAM_CSTAT_CDS:
		dva = frame->txpTargetAddress[0];
		pst_get_size_of_dma_output(&cstat_param[param_idx], align, size);
		break;
	case PARAM_CSTAT_CONTROL:
	case PARAM_CSTAT_OTF_INPUT:
	case PARAM_CSTAT_RGBHIST:
		return;
	default:
		pr_err("%s: invalid param_idx(%d)", __func__, param_idx);
		return;
	}

	if (size[0]) {
		pb[param_idx] = pst_set_dva(frame, dva, size, GROUP_ID_3AA0);
		pst_blob_inject(pst_get_blob_node_cstat(param_idx), pb[param_idx]);
	}
}

static void pst_init_param_cstat(unsigned int index, enum pst_hw_ip_type type)
{
	int i = 0;

	memcpy(cstat_param[i++], (u32 *)&cstat_param_preset[index].control, PARAMETER_MAX_SIZE);
	memcpy(cstat_param[i++], (u32 *)&cstat_param_preset[index].otf_input, PARAMETER_MAX_SIZE);
	memcpy(cstat_param[i++], (u32 *)&cstat_param_preset[index].dma_input, PARAMETER_MAX_SIZE);
	memcpy(cstat_param[i++], (u32 *)&cstat_param_preset[index].lme_ds0, PARAMETER_MAX_SIZE);
	memcpy(cstat_param[i++], (u32 *)&cstat_param_preset[index].lme_ds1, PARAMETER_MAX_SIZE);
	memcpy(cstat_param[i++], (u32 *)&cstat_param_preset[index].fdpig, PARAMETER_MAX_SIZE);
	memcpy(cstat_param[i++], (u32 *)&cstat_param_preset[index].rgbhist, PARAMETER_MAX_SIZE);
	memcpy(cstat_param[i++], (u32 *)&cstat_param_preset[index].drc, PARAMETER_MAX_SIZE);
	memcpy(cstat_param[i++], (u32 *)&cstat_param_preset[index].cds, PARAMETER_MAX_SIZE);
}

static void pst_set_param_cstat(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_CSTAT_PARAM; i++) {
		pst_set_param(frame, cstat_param[i], PARAM_CSTAT_CONTROL + i);
		pst_set_buf_cstat(frame, i);
	}
}

static void pst_clr_param_cstat(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_CSTAT_PARAM; i++) {
		if (!pb[i])
			continue;

		pst_blob_dump(pst_get_blob_node_cstat(i), pb[i]);
		pst_clr_dva(pb[i]);
		pb[i] = NULL;
	}
}

static void pst_set_rta_info_cstat(struct is_frame *frame, struct size_cr_set *cr_set)
{

}

static const struct pst_callback_ops pst_cb_cstat = {
	.init_param = pst_init_param_cstat,
	.set_param = pst_set_param_cstat,
	.clr_param = pst_clr_param_cstat,
	.set_rta_info = pst_set_rta_info_cstat,
};

static int pst_set_hw_cstat(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val,
			DEV_HW_3AA0,
			frame_cstat,
			cstat_param,
			&cstat_cr_set,
			ARRAY_SIZE(cstat_param_preset),
			result,
			&pst_cb_cstat);
}

static int pst_get_hw_cstat(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "CSTAT", ARRAY_SIZE(cstat_param_preset), result);
}
