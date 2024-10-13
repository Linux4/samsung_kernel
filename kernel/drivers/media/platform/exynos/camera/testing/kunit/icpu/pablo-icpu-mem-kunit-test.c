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

#include <linux/platform_device.h>

#include "pablo-kunit-test.h"
#include "icpu/mem/pablo-icpu-mem.h"
#include "pablo-debug.h"

static struct pablo_kunit_test_ctx {
	struct is_debug *debug;

	const struct pablo_memlog_operations *pml_ops_b;

	const struct pablo_bcm_operations *pbcm_ops_b;
	bool bcm_flag;

	const struct pablo_dss_operations *pdss_ops_b;
	bool dss_flag;
} pkt_ctx;

static int __debug_memlog_do_dump(struct memlog_obj *obj, int log_level)
{
	return 0;
}

const static struct pablo_memlog_operations pml_ops_mock = {
	.do_dump = __debug_memlog_do_dump,
};

static void pkt_bcm_dbg_start(void)
{
	pkt_ctx.bcm_flag = true;
}

static void pkt_bcm_dbg_stop(unsigned int bcm_stop_owner)
{
	pkt_ctx.bcm_flag = false;
}

const static struct pablo_bcm_operations pbcm_ops_mock = {
	.start = pkt_bcm_dbg_start,
	.stop = pkt_bcm_dbg_stop,
};

static int pkt_dss_expire_watchdog(void)
{
	pkt_ctx.dss_flag = true;

	return 0;
}

/* Define the test cases. */

const static struct pablo_dss_operations pdss_ops_mock = {
	.expire_watchdog = pkt_dss_expire_watchdog,
};

static void pablo_icpu_mem_alloc_pmem_kunit_test(struct kunit *test)
{
	void *buf;
	const char *heapname = "system";
	size_t size = 1024;
	unsigned long align = 0;
	struct pablo_icpu_buf_info buf_info = { 0, };
	u32 type = ICPU_MEM_TYPE_PMEM;

	buf = pablo_icpu_mem_alloc(type, size, heapname, align);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	buf_info = pablo_icpu_mem_get_buf_info(buf);
	KUNIT_EXPECT_EQ(test, buf_info.size, size);
	KUNIT_EXPECT_NE(test, buf_info.kva, 0);

	pablo_icpu_mem_sync_for_device(buf);

	pablo_icpu_mem_free(buf);
}

void *pablo_kunit_alloc_dummy_cma_buf(void);
static void pablo_icpu_mem_alloc_cma_kunit_test(struct kunit *test)
{
	void *buf;
	struct pablo_icpu_buf_info buf_info = { 0, };

	/* cma alloc could return null when absent cma-area at DT
	 * therefore test use dummy cam buf for consistency
	 */
	buf = pablo_kunit_alloc_dummy_cma_buf();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	buf_info = pablo_icpu_mem_get_buf_info(buf);

	pablo_icpu_mem_sync_for_device(buf);

	pablo_icpu_mem_free(buf);
}

static struct kunit_case pablo_icpu_mem_kunit_test_cases[] = {
	KUNIT_CASE(pablo_icpu_mem_alloc_pmem_kunit_test),
	KUNIT_CASE(pablo_icpu_mem_alloc_cma_kunit_test),
	{},
};

static int pablo_icpu_mem_kunit_test_init(struct kunit *test)
{
	struct is_debug *debug;

	debug = pkt_ctx.debug = is_debug_get();

	pkt_ctx.pml_ops_b = debug->memlog.ops;
	debug->memlog.ops = &pml_ops_mock;
	pkt_ctx.pbcm_ops_b = debug->bcm_ops;
	debug->bcm_ops = &pbcm_ops_mock;
	pkt_ctx.pdss_ops_b = debug->dss_ops;
	debug->dss_ops = &pdss_ops_mock;

	return 0;
}

static void pablo_icpu_mem_kunit_test_exit(struct kunit *test)
{
	struct is_debug *debug = pkt_ctx.debug;

	debug->memlog.ops = pkt_ctx.pml_ops_b;
	debug->bcm_ops = pkt_ctx.pbcm_ops_b;
	debug->dss_ops = pkt_ctx.pdss_ops_b;
	pkt_ctx.dss_flag = false;
}

struct kunit_suite pablo_icpu_mem_kunit_test_suite = {
	.name = "pablo-icpu-mem-kunit-test",
	.init = pablo_icpu_mem_kunit_test_init,
	.exit = pablo_icpu_mem_kunit_test_exit,
	.test_cases = pablo_icpu_mem_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_icpu_mem_kunit_test_suite);

MODULE_LICENSE("GPL");
