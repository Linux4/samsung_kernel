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

#include <linux/platform_device.h>

#include "pablo-device-iommu-group.h"
#include "pablo-kunit-test.h"

static void pablo_iommu_group_probe_kunit_test(struct kunit *test)
{
	int ret;

	platform_driver_unregister(pablo_iommu_group_get_platform_driver());

	ret = platform_driver_register(pablo_iommu_group_get_platform_driver());
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_iommu_group_driver_get_kunit_test(struct kunit *test)
{
	struct platform_driver *iommu_group_driver;

	iommu_group_driver = pablo_iommu_group_get_platform_driver();
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, iommu_group_driver);
}

static void pablo_iommu_group_module_get_kunit_test(struct kunit *test)
{
	struct pablo_device_iommu_group_module *iommu_group_module;

	iommu_group_module = pablo_iommu_group_module_get();
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, iommu_group_module);
}

static void pablo_iommu_group_get_kunit_test(struct kunit *test)
{
	struct pablo_device_iommu_group_module *backup_iommu_group_module;
	struct pablo_device_iommu_group_module *kunit_iommu_group_module;
	struct pablo_device_iommu_group *iommu_group;
	struct device test_dev;

	struct pablo_device_iommu_group_module test_iommu_group_module = {
		.dev = &test_dev,
		.index = 0,
		.num_of_groups = 2,
	};
	struct pablo_device_iommu_group test_iommu_group0 = {
		.dev = &test_dev,
		.index = 0,
	};
	struct pablo_device_iommu_group test_iommu_group1 = {
		.dev = &test_dev,
		.index = 1,
	};
	struct pablo_device_iommu_group *test_iommu_groups[] = {
		&test_iommu_group0,
		&test_iommu_group1,
	};

	/* backup iommu_group_module pointer */
	backup_iommu_group_module = pablo_iommu_group_module_get();

	/* set kunit_iommu_group_module pointer */
	kunit_iommu_group_module = &test_iommu_group_module;
	kunit_iommu_group_module->iommu_group = test_iommu_groups;
	pablo_iommu_group_module_set(kunit_iommu_group_module);

	/* SUB TC : check valid group index */
	iommu_group = pablo_iommu_group_get(0);
	KUNIT_EXPECT_PTR_EQ(test, iommu_group, &test_iommu_group0);
	KUNIT_EXPECT_EQ(test, iommu_group->index, 0);

	iommu_group = pablo_iommu_group_get(1);
	KUNIT_EXPECT_PTR_EQ(test, iommu_group, &test_iommu_group1);
	KUNIT_EXPECT_EQ(test, iommu_group->index, 1);

	/* SUB TC : check invalid group index */
	iommu_group = pablo_iommu_group_get(kunit_iommu_group_module->num_of_groups);
	KUNIT_EXPECT_NULL(test, (void *)iommu_group);

	/* SUB TC : iommu_group_module is NULL */
	pablo_iommu_group_module_set(NULL);
	iommu_group = pablo_iommu_group_get(0);
	KUNIT_EXPECT_NULL(test, (void *)iommu_group);

	/* restore */
	pablo_iommu_group_module_set(backup_iommu_group_module);
}

static struct kunit_case pablo_device_iommu_group_kunit_test_cases[] = {
	KUNIT_CASE(pablo_iommu_group_probe_kunit_test),
	KUNIT_CASE(pablo_iommu_group_driver_get_kunit_test),
	KUNIT_CASE(pablo_iommu_group_module_get_kunit_test),
	KUNIT_CASE(pablo_iommu_group_get_kunit_test),
	{},
};

struct kunit_suite pablo_device_iommu_group_kunit_test_suite = {
	.name = "pablo-device-iommu-group-kunit-test",
	.test_cases = pablo_device_iommu_group_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_device_iommu_group_kunit_test_suite);

MODULE_LICENSE("GPL");
