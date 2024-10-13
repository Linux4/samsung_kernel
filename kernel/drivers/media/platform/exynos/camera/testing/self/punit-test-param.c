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

#include "punit-test-param.h"
#include "is-core.h"
#include "videodev2_exynos_camera.h"

static int punit_set_param_handler(u32 cmd, u32 value)
{
	switch (cmd) {
	case V4L2_CID_IS_S_DVFS_SCENARIO: {
		struct is_resourcemgr *resourcemgr;
		struct is_dvfs_ctrl *dvfs_ctrl;

		resourcemgr = &is_get_is_core()->resourcemgr;
		dvfs_ctrl = &resourcemgr->dvfs_ctrl;

		dvfs_ctrl->dvfs_scenario = value;
		pr_info("[S_PARAM][DVFS] dvfs_scenario(%d)\n", dvfs_ctrl->dvfs_scenario);
	} break;
	default:
		pr_info("[S_PARAM] Can't find set_paramhandler cmd(%d)\n", cmd);
		break;
	}

	return 0;
}

int punit_set_test_param_info(int argc, char **argv)
{
	int ret, arg_i = 1;
	u32 set_param_cmd;
	u32 value;

	if (argc < 2) {
		pr_err("%s: Not enough parameters. %d < 5\n", __func__, argc);
		return -EINVAL;
	}

	ret = kstrtouint(argv[arg_i++], 0, &set_param_cmd);
	if (ret) {
		pr_err("%s: Invalid parameter[%d](ret %d)\n", __func__, arg_i, ret);
		return ret;
	}

	ret = kstrtouint(argv[arg_i++], 0, &value);
	if (ret) {
		pr_err("%s: Invalid parameter[%d](ret %d)\n", __func__, arg_i, ret);
		return ret;
	}

	return punit_set_param_handler(set_param_cmd, value);
}
