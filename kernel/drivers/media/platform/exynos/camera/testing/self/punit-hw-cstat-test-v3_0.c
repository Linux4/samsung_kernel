// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "punit-test-hw-ip.h"
#include "punit-test-file-io.h"
#include "punit-hw-cstat-test.h"

#define CSTAT_PARAM_IDX(param) (param - PARAM_CSTAT_CONTROL)

static const enum pst_blob_node param_to_blob_cstat[NUM_OF_CSTAT_PARAM] = {
	[0 ... NUM_OF_CSTAT_PARAM - 1] = PST_BLOB_NODE_MAX,
	[CSTAT_PARAM_IDX(PARAM_CSTAT_DMA_INPUT)] = PST_BLOB_CSTAT_DMA_INPUT,
	[CSTAT_PARAM_IDX(PARAM_CSTAT_LME_DS0)] = PST_BLOB_CSTAT_LMEDS0,
	[CSTAT_PARAM_IDX(PARAM_CSTAT_FDPIG)] = PST_BLOB_CSTAT_FDPIG,
	[CSTAT_PARAM_IDX(PARAM_CSTAT_DRC)] = PST_BLOB_CSTAT_DRC,
	[CSTAT_PARAM_IDX(PARAM_CSTAT_SAT)] = PST_BLOB_CSTAT_SAT,
	[CSTAT_PARAM_IDX(PARAM_CSTAT_CAV)] = PST_BLOB_CSTAT_CAV,
};

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
	[0].fdpig.dma_crop_width = 2688,
	[0].fdpig.dma_crop_height = 2016,
	[0].fdpig.crop_enable = 0,
	[0].fdpig.msb = DMA_INOUT_BIT_WIDTH_32BIT - 1,
	[0].fdpig.stride_plane0 = 0,
	[0].fdpig.stride_plane1 = 0,
	[0].fdpig.stride_plane2 = 0,
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

	[0].sat.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].sat.format = DMA_INOUT_FORMAT_RGB,
	[0].sat.bitwidth = DMA_INOUT_BIT_WIDTH_24BIT,
	[0].sat.order = DMA_INOUT_ORDER_BGR,
	[0].sat.plane = DMA_INOUT_PLANE_1,
	[0].sat.width = 1920,
	[0].sat.height = 1440,
	[0].sat.dma_crop_offset_x = 0,
	[0].sat.dma_crop_offset_y = 0,
	[0].sat.dma_crop_width = 2688,
	[0].sat.dma_crop_height = 2016,
	[0].sat.crop_enable = 0,
	[0].sat.msb = DMA_INOUT_BIT_WIDTH_24BIT - 1,
	[0].sat.stride_plane0 = 0,
	[0].sat.stride_plane1 = 0,
	[0].sat.stride_plane2 = 0,
	[0].sat.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].sat.sbwc_type = NONE,

	[0].cav.cmd = DMA_OUTPUT_COMMAND_ENABLE,
	[0].cav.format = DMA_INOUT_FORMAT_RGB,
	[0].cav.bitwidth = DMA_INOUT_BIT_WIDTH_24BIT,
	[0].cav.order = DMA_INOUT_ORDER_BGR,
	[0].cav.plane = DMA_INOUT_PLANE_1,
	[0].cav.width = 640,
	[0].cav.height = 480,
	[0].cav.dma_crop_offset_x = 0,
	[0].cav.dma_crop_offset_y = 0,
	[0].cav.dma_crop_width = 2688,
	[0].cav.dma_crop_height = 2016,
	[0].cav.crop_enable = 0,
	[0].cav.msb = DMA_INOUT_BIT_WIDTH_24BIT - 1,
	[0].cav.stride_plane0 = 0,
	[0].cav.stride_plane1 = 0,
	[0].cav.stride_plane2 = 0,
	[0].cav.v_otf_enable = OTF_OUTPUT_COMMAND_DISABLE,
	[0].cav.sbwc_type = NONE,
};

static DECLARE_BITMAP(preset_test_result, ARRAY_SIZE(cstat_param_preset));

size_t pst_get_preset_test_size_cstat(void)
{
	return ARRAY_SIZE(cstat_param_preset);
}

unsigned long *pst_get_preset_test_result_buf_cstat(void)
{
	return preset_test_result;
}

const enum pst_blob_node *pst_get_blob_node_cstat(void)
{
	return param_to_blob_cstat;
}

void pst_set_buf_cstat(struct is_frame *frame, u32 param[][PARAMETER_MAX_MEMBER], u32 param_idx,
		       struct is_priv_buf *pb[NUM_OF_CSTAT_PARAM])
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
		pst_get_size_of_dma_input(&param[param_idx], align, block_w, block_h, size);
		break;
	case PARAM_CSTAT_LME_DS0:
		dva = frame->txldsTargetAddress[0];
		pst_get_size_of_dma_output(&param[param_idx], align, size);
		break;
	case PARAM_CSTAT_LME_DS1:
		dva = frame->dva_cstat_lmeds1[0];
		pst_get_size_of_dma_output(&param[param_idx], align, size);
		break;
	case PARAM_CSTAT_FDPIG:
		dva = frame->efdTargetAddress[0];
		pst_get_size_of_dma_output(&param[param_idx], align, size);
		break;
	case PARAM_CSTAT_DRC:
		dva = frame->txdgrTargetAddress[0];
		pst_get_size_of_dma_output(&param[param_idx], align, size);
		break;
	case PARAM_CSTAT_SAT:
		dva = frame->dva_cstat_sat[0];
		pst_get_size_of_dma_output(&param[param_idx], align, size);
		break;
	case PARAM_CSTAT_CAV:
		dva = frame->dva_cstat_cav[0];
		pst_get_size_of_dma_output(&param[param_idx], align, size);
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
		pst_blob_inject(param_to_blob_cstat[param_idx], pb[param_idx]);
	}
}

void pst_init_param_cstat(u32 param[][PARAMETER_MAX_MEMBER], unsigned int index)
{
	memcpy(param[0], (u32 *)&cstat_param_preset[index],
	       PARAMETER_MAX_SIZE * NUM_OF_CSTAT_PARAM);
}
