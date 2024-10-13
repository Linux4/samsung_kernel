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

#include <linux/module.h>

#include "punit-test-hw-ip.h"
#include "is-core.h"

static int pst_set_hw_mcfp2dlfe(const char *val, const struct kernel_param *kp);
static int pst_get_hw_mcfp2dlfe(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_mcfp2dlfe = {
	.set = pst_set_hw_mcfp2dlfe,
	.get = pst_get_hw_mcfp2dlfe,
};
module_param_cb(test_hw_mcfp2dlfe, &pablo_param_ops_hw_mcfp2dlfe, NULL, 0644);

#define NUM_OF_MCFP_PARAM (PARAM_MCFP_YUV - PARAM_MCFP_CONTROL + 1)

static const struct mcfp_param m2d_param_preset[] = {
	/* Param set[0]: MCFP STRGEN Input */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_START,

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

	[0].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
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
};

static struct mcfp_config_set {
	u32 size;
	struct is_mcfp_config cfg;
} mcfp_cfg;

static struct mcfp_cr_set {
	struct size_cr_set mcfp_cr_set;
	struct size_cr_set dlfe_cr_set;
} mcfp_cr;

static struct is_frame *frame_m2d;
static DECLARE_BITMAP(result, ARRAY_SIZE(m2d_param_preset));

static void pst_set_rta_info_mcfp2dlfe(struct is_frame *frame, struct size_cr_set *cr_set)
{
	u32 cr_i = 0;
	struct size_cr_set *dlfe_cr_set;

	/* Update DLFE CR set */
	dlfe_cr_set = (struct size_cr_set *)((u64)cr_set + sizeof(struct size_cr_set));

	dlfe_cr_set->cr[cr_i].reg_addr = 0x3404;
	dlfe_cr_set->cr[cr_i].reg_data = 1;
	cr_i++;
	dlfe_cr_set->cr[cr_i].reg_addr = 0x3404;
	dlfe_cr_set->cr[cr_i].reg_data = 0;
	cr_i++;

	dlfe_cr_set->size = cr_i;

	frame->kva_mcfp_rta_info[PLANE_INDEX_CR_SET] = (u64)cr_set;
}

static void pst_set_config_m2d(struct is_frame *frame)
{
	/* DLFE CFG */
	mcfp_cfg.cfg.dlfe_en = 1;

	frame->kva_mcfp_rta_info[PLANE_INDEX_CONFIG] = (u64)&mcfp_cfg;
}

static void pst_set_param_mcfp2dlfe(struct is_frame *frame)
{
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;
	u32 i, *param;

	for (i = 0, param = (u32 *)&m2d_param_preset[0]; i < NUM_OF_MCFP_PARAM;
	     i++, param += PARAMETER_MAX_MEMBER)
		     pst_set_param(frame, param, PARAM_MCFP_CONTROL + i);

	pst_set_config_m2d(frame);
	pst_set_rta_info_mcfp2dlfe(frame, &mcfp_cr.mcfp_cr_set);

	i = 0;
	memset(frame->hw_slot_id, HW_SLOT_MAX, sizeof(frame->hw_slot_id));
	frame->hw_slot_id[i++] = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_MCFP);
	frame->hw_slot_id[i++] = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_DLFE);
}

static void pst_clr_param_mcfp2dlfe(struct is_frame *frame)
{
}

static const struct pst_callback_ops pst_cb_m2d = {
	.set_param = pst_set_param_mcfp2dlfe,
	.clr_param = pst_clr_param_mcfp2dlfe,
	.set_rta_info = pst_set_rta_info_mcfp2dlfe,
};

static int pst_set_hw_mcfp2dlfe(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val,
			DEV_HW_END,
			frame_m2d,
			NULL,
			NULL,
			ARRAY_SIZE(m2d_param_preset),
			result,
			&pst_cb_m2d);
}

static int pst_get_hw_mcfp2dlfe(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "MCFP2DLFE", ARRAY_SIZE(m2d_param_preset), result);
}
