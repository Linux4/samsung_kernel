// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include "pablo-self-test-result.h"

static int pst_set_csi_irq(const char *val, const struct kernel_param *kp);
static int pst_get_csi_irq(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_csi_irq = {
	.set = pst_set_csi_irq,
	.get = pst_get_csi_irq,
};
module_param_cb(test_result_csi_irq, &pablo_param_ops_csi_irq, NULL, 0644);
static unsigned long result_csi_irq;

static int pst_set_csi_irq(const char *val, const struct kernel_param *kp)
{
	result_csi_irq = 0;

	return 0;
}

static int pst_get_csi_irq(char *buffer, const struct kernel_param *kp)
{
	int ret;

	pr_info("%s: result(0x%lx)\n", __func__, result_csi_irq);
	ret = sprintf(buffer, "1 ");
	ret += sprintf(buffer + ret, "%lx\n", result_csi_irq);

	return ret;
}

void pst_result_csi_irq(struct is_device_csi *csi)
{
	if (csi->state_cnt.str && csi->state_cnt.end && !csi->state_cnt.err)
		set_bit(0, &result_csi_irq);
	else
		clear_bit(0, &result_csi_irq);
}
