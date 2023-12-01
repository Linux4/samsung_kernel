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

#include "is-core.h"

static int pst_set_power(const char *val, const struct kernel_param *kp);
static int pst_get_power(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_param_ops_power = {
	.set = pst_set_power,
	.get = pst_get_power,
};
module_param_cb(test_power, &pablo_param_ops_power, NULL, 0644);

enum pst_power_state {
	PST_POWER_OFF,
	PST_POWER_ON,
};

static unsigned long result_power;

static void pst_result_power_on(unsigned long *result)
{
	/* TODO */
	set_bit(0, result);
}

static int pst_power_off(void)
{
	int ret;
	struct is_core *core = is_get_is_core();

	ret = is_resource_put(&core->resourcemgr, RESOURCE_TYPE_ISCHAIN);
	if (ret)
		pr_err("is_resource_put is fail (%d)\n", ret);

	return 0;
}

static int pst_power_on(void)
{
	int ret;
	struct is_core *core = is_get_is_core();

	ret = is_resource_get(&core->resourcemgr, RESOURCE_TYPE_ISCHAIN);
	if (ret) {
		pr_err("is_resource_get is fail (%d)\n", ret);
		return -EINVAL;
	}

	pst_result_power_on(&result_power);

	return 0;
}

static int pst_set_power(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;
	u32 act;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		pr_err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &act);
	if (ret) {
		pr_err("Invalid act %d ret %d", act, ret);
		goto func_exit;
	}

	switch (act) {
	case PST_POWER_OFF:
		pr_info("power off\n");

		pst_power_off();
		break;
	case PST_POWER_ON:
		pr_info("power on\n");

		pst_power_on();
		break;
	default:
		pr_err("NOT supported act %u", act);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int pst_get_power(char *buffer, const struct kernel_param *kp)
{
	int ret;

	pr_info("%s: result(0x%lx)\n", __func__, result_power);
	ret = sprintf(buffer, "1 ");
	ret += sprintf(buffer + ret, "%lx\n", result_power);

	return ret;
}
