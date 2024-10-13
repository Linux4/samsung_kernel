// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/moduleparam.h>

#include "is-core.h"

static int set_punit_test_hw_mock(const char *val, const struct kernel_param *kp);
static int get_punit_test_hw_mock(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops param_ops_punit_test_hw_mock = {
	.set = set_punit_test_hw_mock,
	.get = get_punit_test_hw_mock,
};
module_param_cb(punit_test_hw_mock, &param_ops_punit_test_hw_mock, NULL, 0644);

static void punit_fake_m2m_frame_start(struct is_hw_ip *hw_ip, u32 instance)
{
	msinfo_hw("%s\n", instance, hw_ip, __func__);
	CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
}

static void punit_fake_m2m_frame_end(struct is_hw_ip *hw_ip, u32 instance)
{
	msinfo_hw("%s\n", instance, hw_ip, __func__);
	CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END, IS_SHOT_SUCCESS, true);
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
}

static void punit_fake_otf_frame_start(struct is_hw_ip *hw_ip, u32 instance)
{
	msinfo_hw("%s\n", instance, hw_ip, __func__);
	CALL_HW_OPS(hw_ip, frame_start, hw_ip, instance);
}

static void punit_fake_otf_frame_end(u32 *hw_slot_id, u32 instance)
{
	int i;
	struct is_hw_ip *hw_ip;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;

	for (i = HW_SLOT_MAX - 1; i >= 0; i--) {
		if (hw_slot_id[i] >= HW_SLOT_MAX)
			continue;

		hw_ip = &hw->hw_ip[hw_slot_id[i]];

		msinfo_hw("%s\n", instance, hw_ip, __func__);
		CALL_HW_OPS(hw_ip, frame_done, hw_ip, NULL, -1, IS_HW_CORE_END, IS_SHOT_SUCCESS,
			    true);
	}
}

static int set_punit_test_hw_mock(const char *val, const struct kernel_param *kp)
{
	int ret, argc, arg_i = 0;
	char **argv;
	int act;
	struct is_core *core = is_get_is_core();
	struct is_hardware *hw = &core->hardware;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		pr_err("No argument!\n");
		return -EINVAL;
	}

	ret = kstrtouint(argv[arg_i++], 0, &act);
	if (ret) {
		pr_err("Invalid act %d ret %d\n", act, ret);
		goto func_exit;
	}

	switch (act) {
	case 0:
		hw->fake_m2m_frame_start = NULL;
		hw->fake_m2m_frame_end = NULL;
		hw->fake_otf_frame_start = NULL;
		hw->fake_otf_frame_end = NULL;
		break;
	case 1:
		hw->fake_m2m_frame_start = punit_fake_m2m_frame_start;
		hw->fake_m2m_frame_end = punit_fake_m2m_frame_end;
		hw->fake_otf_frame_start = punit_fake_otf_frame_start;
		hw->fake_otf_frame_end = punit_fake_otf_frame_end;
		break;
	default:
		err("NOT supported act %d", act);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int get_punit_test_hw_mock(char *buffer, const struct kernel_param *kp)
{
	return 0;
}
