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
#include "pablo-lib.h"

static void pablo_lib_dummy_kunit_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, 0);
}

static void parse_lib_parse_cpu_data_kunit_test(struct kunit *test)
{
	struct pablo_lib_platform_data *plpd;
	u32 lit_cluster, mid_cluster, big_cluster;

	plpd = pablo_lib_get_platform_data();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, plpd);

	lit_cluster = plpd->cpu_cluster[PABLO_CPU_CLUSTER_LIT];
	mid_cluster = plpd->cpu_cluster[PABLO_CPU_CLUSTER_MID];
	big_cluster = plpd->cpu_cluster[PABLO_CPU_CLUSTER_BIG];

	KUNIT_EXPECT_EQ(test, lit_cluster, (u32)0);
	KUNIT_EXPECT_GT(test, mid_cluster, lit_cluster);
	KUNIT_EXPECT_GE(test, big_cluster, mid_cluster);
}

static void parse_lib_parse_memlog_buf_size_data_kunit_test(struct kunit *test)
{
	struct pablo_lib_platform_data *plpd;
	u32 size_drv, size_ddk;

	plpd = pablo_lib_get_platform_data();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, plpd);

	size_drv = plpd->memlog_size[PABLO_MEMLOG_DRV];
	size_ddk = plpd->memlog_size[PABLO_MEMLOG_DDK];

	KUNIT_EXPECT_NE(test, size_drv, (u32)0);
	KUNIT_EXPECT_NE(test, size_ddk, (u32)0);
}

static int pablo_lib_kunit_test_init(struct kunit *test)
{
	return 0;
}

static void pablo_lib_kunit_test_exit(struct kunit *test)
{

}

static struct kunit_case pablo_lib_kunit_test_cases[] = {
	KUNIT_CASE(pablo_lib_dummy_kunit_test),
	KUNIT_CASE(parse_lib_parse_cpu_data_kunit_test),
	KUNIT_CASE(parse_lib_parse_memlog_buf_size_data_kunit_test),
	{},
};

struct kunit_suite pablo_lib_kunit_test_suite = {
	.name = "pablo-lib-kunit-test",
	.init = pablo_lib_kunit_test_init,
	.exit = pablo_lib_kunit_test_exit,
	.test_cases = pablo_lib_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_lib_kunit_test_suite);

MODULE_LICENSE("GPL v2");
