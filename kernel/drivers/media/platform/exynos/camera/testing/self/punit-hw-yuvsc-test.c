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
#include "punit-test-file-io.h"
#include "pablo-framemgr.h"
#include "is-hw.h"

static int pst_set_hw_yuvsc(const char *val, const struct kernel_param *kp);
static int pst_get_hw_yuvsc(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_hw_yuvsc = {
	.set = pst_set_hw_yuvsc,
	.get = pst_get_hw_yuvsc,
};
module_param_cb(test_hw_yuvsc, &pablo_param_ops_hw_yuvsc, NULL, 0644);

#define NUM_OF_YUVSC_PARAM (PARAM_YUVSC_OTF_OUTPUT - PARAM_YUVSC_CONTROL + 1)

static struct is_frame *frame_yuvsc;
static u32 yuvsc_param[NUM_OF_YUVSC_PARAM][PARAMETER_MAX_MEMBER];
static struct size_cr_set yuvsc_cr_set;

static const struct yuvsc_param yuvsc_param_preset_grp[] = {
	/* Param set[0]: Default */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_START,

	[0].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].otf_input.format = 0,
	[0].otf_input.bitwidth = 0,
	[0].otf_input.order = 0,
#if !IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	[0].otf_input.width = 1920,
	[0].otf_input.height = 1440,
#else
	[0].otf_input.width = 320,
	[0].otf_input.height = 240,
#endif
	[0].otf_input.bayer_crop_offset_x = 0,
	[0].otf_input.bayer_crop_offset_y = 0,
	[0].otf_input.bayer_crop_width = 0,
	[0].otf_input.bayer_crop_height = 0,
	[0].otf_input.source = 0,
	[0].otf_input.physical_width = 0,
	[0].otf_input.physical_height = 0,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].otf_output.format = 0,
	[0].otf_output.bitwidth = 0,
	[0].otf_output.order = 0,
#if !IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	[0].otf_output.width = 1920,
	[0].otf_output.height = 1440,
#else
	[0].otf_output.width = 320,
	[0].otf_output.height = 240,
#endif
	[0].otf_output.crop_offset_x = 0,
	[0].otf_output.crop_offset_y = 0,
	[0].otf_output.crop_width = 0,
	[0].otf_output.crop_height = 0,
	[0].otf_output.crop_enable = 0,
};

static const struct yuvsc_param yuvsc_param_preset[] = {
	/* Param set[0]: Default */
	[0].control.cmd = CONTROL_COMMAND_START,
	[0].control.bypass = 0,
	[0].control.strgen = CONTROL_COMMAND_START,

	[0].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[0].otf_input.format = 0,
	[0].otf_input.bitwidth = 0,
	[0].otf_input.order = 0,
#if !IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	[0].otf_input.width = 1920,
	[0].otf_input.height = 1440,
#else
	[0].otf_input.width = 320,
	[0].otf_input.height = 240,
#endif
	[0].otf_input.bayer_crop_offset_x = 0,
	[0].otf_input.bayer_crop_offset_y = 0,
	[0].otf_input.bayer_crop_width = 0,
	[0].otf_input.bayer_crop_height = 0,
	[0].otf_input.source = 0,
	[0].otf_input.physical_width = 0,
	[0].otf_input.physical_height = 0,
	[0].otf_input.offset_x = 0,
	[0].otf_input.offset_y = 0,

	[0].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[0].otf_output.format = 0,
	[0].otf_output.bitwidth = 0,
	[0].otf_output.order = 0,
#if !IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	[0].otf_output.width = 1920,
	[0].otf_output.height = 1440,
#else
	[0].otf_output.width = 320,
	[0].otf_output.height = 240,
#endif
	[0].otf_output.crop_offset_x = 0,
	[0].otf_output.crop_offset_y = 0,
	[0].otf_output.crop_width = 0,
	[0].otf_output.crop_height = 0,
	[0].otf_output.crop_enable = 0,

	/* Param set[1]: Scale down */
	[1].control.cmd = CONTROL_COMMAND_START,
	[1].control.bypass = 0,
	[1].control.strgen = CONTROL_COMMAND_START,

	[1].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[1].otf_input.format = 0,
	[1].otf_input.bitwidth = 0,
	[1].otf_input.order = 0,
#if !IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	[1].otf_input.width = 2880,
	[1].otf_input.height = 2160,
#else
	[1].otf_input.width = 480,
	[1].otf_input.height = 360,
#endif
	[1].otf_input.bayer_crop_offset_x = 0,
	[1].otf_input.bayer_crop_offset_y = 0,
	[1].otf_input.bayer_crop_width = 0,
	[1].otf_input.bayer_crop_height = 0,
	[1].otf_input.source = 0,
	[1].otf_input.physical_width = 0,
	[1].otf_input.physical_height = 0,
	[1].otf_input.offset_x = 0,
	[1].otf_input.offset_y = 0,

	[1].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[1].otf_output.format = 0,
	[1].otf_output.bitwidth = 0,
	[1].otf_output.order = 0,
#if !IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	[1].otf_output.width = 1920,
	[1].otf_output.height = 1440,
#else
	[1].otf_output.width = 320,
	[1].otf_output.height = 240,
#endif
	[1].otf_output.crop_offset_x = 0,
	[1].otf_output.crop_offset_y = 0,
	[1].otf_output.crop_width = 0,
	[1].otf_output.crop_height = 0,
	[1].otf_output.crop_enable = 0,

/* Param set[2]: Scale up for test of error case */
	[2].control.cmd = CONTROL_COMMAND_START,
	[2].control.bypass = 0,
	[2].control.strgen = CONTROL_COMMAND_START,

	[2].otf_input.cmd = OTF_INPUT_COMMAND_DISABLE,
	[2].otf_input.format = 0,
	[2].otf_input.bitwidth = 0,
	[2].otf_input.order = 0,
#if !IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	[2].otf_input.width = 1920,
	[2].otf_input.height = 1440,
#else
	[2].otf_input.width = 320,
	[2].otf_input.height = 240,
#endif
	[2].otf_input.bayer_crop_offset_x = 0,
	[2].otf_input.bayer_crop_offset_y = 0,
	[2].otf_input.bayer_crop_width = 0,
	[2].otf_input.bayer_crop_height = 0,
	[2].otf_input.source = 0,
	[2].otf_input.physical_width = 0,
	[2].otf_input.physical_height = 0,
	[2].otf_input.offset_x = 0,
	[2].otf_input.offset_y = 0,

	[2].otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE,
	[2].otf_output.format = 0,
	[2].otf_output.bitwidth = 0,
	[2].otf_output.order = 0,
#if !IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON)
	[2].otf_output.width = 2880,
	[2].otf_output.height = 2160,
#else
	[2].otf_output.width = 480,
	[2].otf_output.height = 360,
#endif
	[2].otf_output.crop_offset_x = 0,
	[2].otf_output.crop_offset_y = 0,
	[2].otf_output.crop_width = 0,
	[2].otf_output.crop_height = 0,
	[2].otf_output.crop_enable = 0,
};

static DECLARE_BITMAP(result, ARRAY_SIZE(yuvsc_param_preset));

static void pst_init_param_yuvsc(unsigned int index, enum pst_hw_ip_type type)
{
	const struct yuvsc_param *preset;
	int i = 0;

	if (type == PST_HW_IP_SINGLE)
		preset = yuvsc_param_preset;
	else
		preset = yuvsc_param_preset_grp;

	memcpy(yuvsc_param[i++], (u32 *)&preset[index].control, PARAMETER_MAX_SIZE);
	memcpy(yuvsc_param[i++], (u32 *)&preset[index].otf_input, PARAMETER_MAX_SIZE);
	memcpy(yuvsc_param[i++], (u32 *)&preset[index].otf_output, PARAMETER_MAX_SIZE);
}

static void pst_set_param_yuvsc(struct is_frame *frame)
{
	int i;

	for (i = 0; i < NUM_OF_YUVSC_PARAM; i++)
		pst_set_param(frame, yuvsc_param[i], PARAM_YUVSC_CONTROL + i);
}

static const struct pst_callback_ops pst_cb_yuvsc = {
	.init_param = pst_init_param_yuvsc,
	.set_param = pst_set_param_yuvsc,
};

const struct pst_callback_ops *pst_get_hw_yuvsc_cb(void)
{
	return &pst_cb_yuvsc;
}

static int pst_set_hw_yuvsc(const char *val, const struct kernel_param *kp)
{
	return pst_set_hw_ip(val,
			DEV_HW_YUVSC0,
			frame_yuvsc,
			yuvsc_param,
			&yuvsc_cr_set,
			ARRAY_SIZE(yuvsc_param_preset),
			result,
			&pst_cb_yuvsc);
}

static int pst_get_hw_yuvsc(char *buffer, const struct kernel_param *kp)
{
	return pst_get_hw_ip(buffer, "YUVSC", ARRAY_SIZE(yuvsc_param_preset), result);
}
