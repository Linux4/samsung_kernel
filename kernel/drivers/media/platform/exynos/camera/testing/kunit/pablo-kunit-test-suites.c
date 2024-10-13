// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/version.h>

#include "pablo-kunit-test.h"
#include "is-core.h"

#if (KERNEL_VERSION(6, 1, 0) <= LINUX_VERSION_CODE)
#define KUNIT_TEST_SUITE_INIT(s) __kunit_test_suites_init(s, pablo_kunit_test_subsuite_num(s))
#define KUNIT_TEST_SUITE_EXIT(s) __kunit_test_suites_exit(s, pablo_kunit_test_subsuite_num(s))
#else
#define KUNIT_TEST_SUITE_INIT(s) __kunit_test_suites_init(s)
#define KUNIT_TEST_SUITE_EXIT(s) __kunit_test_suites_exit(s)
#endif

extern struct kunit_suite *const *const pablo_kunit_test_suites[];

static bool pablo_kunit_test_suite_is_null(struct kunit_suite *const *suites)
{
	if (suites && suites[0]->test_cases)
		return false;

	return true;
}

static void pablo_test_suite_is_null_kunit_test(struct kunit *test)
{
	bool ret;
	struct kunit_suite test_suite_dummy = {
		.name = "test-suite-dummy",
		.test_cases = NULL,
	};
	struct kunit_suite *suite_array[] = { &test_suite_dummy, NULL };

	ret = pablo_kunit_test_suite_is_null(suite_array);
	KUNIT_EXPECT_EQ(test, ret, (bool)true);
}

static struct kunit_case pablo_kunit_test_cases[] = {
	KUNIT_CASE(pablo_test_suite_is_null_kunit_test),
	{},
};

struct kunit_suite pablo_kunit_test_suite = {
	.name = "pablo-kunit-test-suite",
	.test_cases = pablo_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_kunit_test_suite);

struct kunit_suite test_suite_end = {
	.name = "pablo-kunit-test-suite-end",
	.test_cases = NULL,
};
define_pablo_kunit_test_suites_end(&test_suite_end);

static int pablo_kunit_test_subsuite_num(struct kunit_suite *const *subsuite)
{
	int num_of_suites = 0;

	while (*subsuite) {
		num_of_suites++;
		subsuite++;
	}

	return num_of_suites;
}

static void pablo_kunit_print_tap_header(void)
{
	int num_of_suites = 0;

	struct kunit_suite *const *const *suites;

	suites = pablo_kunit_test_suites;
	while (!pablo_kunit_test_suite_is_null(*suites)) {
		num_of_suites += pablo_kunit_test_subsuite_num(*suites);
		suites++;
	}

	pr_info("PABLO TAP version 14\n");
	pr_info("1..%d\n", num_of_suites);
}

static struct orgin_ctx {
	struct is_core core;
	struct is_groupmgr grpmgr;
	struct is_minfo minfo;
} origin_ctx;

static void _save_origin_ctx(void)
{
	struct is_core *core = is_get_is_core();
	struct is_groupmgr *grpmgr = is_get_groupmgr();
	struct is_minfo *im = is_get_is_minfo();
	int i;

	memcpy(&origin_ctx.core, core, sizeof(struct is_core));
	memcpy(&origin_ctx.grpmgr, grpmgr, sizeof(struct is_groupmgr));
	memcpy(&origin_ctx.minfo, im, sizeof(struct is_minfo));

	for (i = 0; i < IS_STREAM_COUNT; i++)
		is_ischain_probe(&core->ischain[i], &core->resourcemgr, core->pdev, i);
}

static void _restore_origin_ctx(void)
{
	struct is_core *core = is_get_is_core();
	struct is_groupmgr *grpmgr = is_get_groupmgr();
	struct is_minfo *im = is_get_is_minfo();
	int i;

	for (i = 0; i < IS_STREAM_COUNT; i++)
		is_ischain_remove(&core->ischain[i]);

	memcpy(im, &origin_ctx.minfo, sizeof(struct is_minfo));
	memcpy(grpmgr, &origin_ctx.grpmgr, sizeof(struct is_groupmgr));
	memcpy(core, &origin_ctx.core, sizeof(struct is_core));
}

static int __get_active_dma_buf_size(void)
{
	const struct kernel_param *kp = pablo_mem_get_kernel_param();
	char buf[NAME_MAX];
	int val, ret;

	kp->ops->get(buf, kp);

	ret = kstrtoint(buf, 0, &val);

	if (ret)
		return 0;

	return val;
}

static int __dma_buf_sz;

static int __suite_init(struct kunit_suite *suite)
{
	__dma_buf_sz = __get_active_dma_buf_size();
	return 0;
}

static void __suite_exit(struct kunit_suite *suite)
{
	int exit_dma_buf_sz = __get_active_dma_buf_size(), dma_buf_leak;

	dma_buf_leak = exit_dma_buf_sz - __dma_buf_sz;
	if (dma_buf_leak) {
		kunit_log(KERN_INFO, suite,
			KUNIT_SUBTEST_INDENT "not ok suite - %s: detect to DMA-BUF leak(%d)",
			suite->name, dma_buf_leak);
		return;
	}

	kunit_log(KERN_INFO, suite, KUNIT_SUBTEST_INDENT "ok suite - %s", suite->name);
}

static void __run_kunit_suite(struct kunit_suite *const *const *suites)
{
	__suite_init(**suites);
	KUNIT_TEST_SUITE_INIT(*suites);
	__suite_exit(**suites);
}

static void __run_kunit_test(void)
{
	struct kunit_suite *const *const *suites;

	pablo_kunit_print_tap_header();
	suites = pablo_kunit_test_suites;
	while (!pablo_kunit_test_suite_is_null(*suites)) {
		__run_kunit_suite(suites);
		suites++;
	}
}

static void __teardown_kunit_test(void)
{
	struct kunit_suite *const *const *suites;

	suites = pablo_kunit_test_suites;
	while (!pablo_kunit_test_suite_is_null(*suites)) {
		KUNIT_TEST_SUITE_EXIT((struct kunit_suite **)*suites);
		suites++;
	}
}

static int __init pablo_kunit_test_suites_init(void)
{
	pr_info("pablo_kunit_test module init");
	return 0;
}
module_init(pablo_kunit_test_suites_init);

static void __exit pablo_kunit_test_suites_exit(void)
{
	pr_info("pablo_kunit_test module exit");
}
module_exit(pablo_kunit_test_suites_exit);

static int __run_pablo_kunit_test(const char *val, const struct kernel_param *kp)
{
	if (val[0] == '1') {
		_save_origin_ctx();
		__run_kunit_test();
		__teardown_kunit_test();
		_restore_origin_ctx();
	}

	return 0;
}

static const struct kernel_param_ops __run_kunit_ops = {
	.set = __run_pablo_kunit_test,
	.get = NULL,
};

module_param_cb(run_kunit_test, &__run_kunit_ops, NULL, 0644);

MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL");
