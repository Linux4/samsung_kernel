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

#include "pablo-kunit-test.h"
#include "is-core.h"

#ifdef USE_CAMERA_ADAPTIVE_MIPI
#include <linux/dev_ril_bridge.h>
#endif

static int pablo_vendor_kunit_test_init(struct kunit *test)
{
	int ret = 0;

	return ret;
}

static void pablo_vendor_kunit_test_exit(struct kunit *test)
{
}

static struct kunit_case pablo_vendor_kunit_test_cases[] = {
	{},
};

struct kunit_suite pablo_vendor_kunit_test_suite = {
	.name = "pablo-vendor-kunit-test",
	.init = pablo_vendor_kunit_test_init,
	.exit = pablo_vendor_kunit_test_exit,
	.test_cases = pablo_vendor_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_vendor_kunit_test_suite);

MODULE_LICENSE("GPL");