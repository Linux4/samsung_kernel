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

#include "pablo-self-test-file-io.h"

#ifdef MODULE
static int pablo_self_test_init(void)
{
	int ret;

	ret = pablo_self_test_file_io_probe();
	if (ret) {
		pr_err("pablo_self_test_file_io_probe is filed\n");
		return ret;
	}

	return 0;
}
module_init(pablo_self_test_init);

void pablo_self_test_exit(void)
{
}
module_exit(pablo_self_test_exit);
#else
static int __init pablo_self_test_init(void)
{
	return 0;
}

device_initcall_sync(pablo_self_test_init);
#endif

MODULE_DESCRIPTION("Samsung EXYNOS SoC Pablo Self Test driver");
MODULE_ALIAS("platform:samsung-pablo-self-test");
MODULE_LICENSE("GPL v2");
