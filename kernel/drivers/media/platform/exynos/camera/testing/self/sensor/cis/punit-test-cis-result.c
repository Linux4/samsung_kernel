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

#include <linux/module.h>
#include "punit-test-cis-result.h"

static int pst_set_cis_opt_result(const char *val, const struct kernel_param *kp);
static int pst_get_cis_opt_result(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_cis_opt = {
	.set = pst_set_cis_opt_result,
	.get = pst_get_cis_opt_result,
};
module_param_cb(test_result_cis_opt, &pablo_param_ops_cis_opt, NULL, 0644);
static unsigned long result_cis_opt;

static int pst_set_cis_opt_result(const char *val, const struct kernel_param *kp)
{
	result_cis_opt = 0;

	return 0;
}

static int pst_get_cis_opt_result(char *buffer, const struct kernel_param *kp)
{
	int ret;

	pr_info("%s: result(0x%lx)\n", __func__, result_cis_opt);
	ret = sprintf(buffer, "1 ");
	ret += sprintf(buffer + ret, "%lx\n", result_cis_opt);

	return ret;
}

void pst_result_cis_opt(int ret)
{
	if (!ret)
		set_bit(0, &result_cis_opt);
	else
		clear_bit(0, &result_cis_opt);
}
