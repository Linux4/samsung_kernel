// SPDX-License-Identifier: GPL-2.0
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

#include "pablo-kunit-test.h"
#include "is-core.h"
#include "is-hw-chain.h"

static struct is_device_ischain *idi;
static struct is_frame *frame;

static int pablo_subdev_mcs_kunit_test_init(struct kunit *test)
{
	idi = kunit_kzalloc(test, sizeof(struct is_device_ischain), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, idi);

	idi->is_region = kunit_kzalloc(test, sizeof(struct is_region), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, idi->is_region);

	frame = kunit_kzalloc(test, sizeof(struct is_frame), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);

	frame->parameter = kunit_kzalloc(test, sizeof(struct is_param_region), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame->parameter);

	return 0;
}

static void pablo_subdev_mcs_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, frame->parameter);
	kunit_kfree(test, frame);
	kunit_kfree(test, idi->is_region);
	kunit_kfree(test, idi);
}

static struct kunit_case pablo_subdev_mcs_kunit_test_cases[] = {
	{},
};

struct kunit_suite pablo_subdev_mcs_kunit_test_suite = {
	.name = "pablo-subdev-mcs-kunit-test",
	.init = pablo_subdev_mcs_kunit_test_init,
	.exit = pablo_subdev_mcs_kunit_test_exit,
	.test_cases = pablo_subdev_mcs_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_mcs_kunit_test_suite);

MODULE_LICENSE("GPL v2");
