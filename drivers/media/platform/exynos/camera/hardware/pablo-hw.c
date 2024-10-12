// SPDX-License-Identifier: GPL
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

static int __init pablo_hw_init(void)
{
	return 0;
}
module_init(pablo_hw_init);

static void __exit pablo_hw_exit(void)
{
}
module_exit(pablo_hw_exit)

MODULE_DESCRIPTION("Exynos Pablo HW");
MODULE_LICENSE("GPL");
