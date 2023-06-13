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

#include <linux/dma-heap.h>

#include "pablo-kunit-test.h"
#include "pablo-interface-irta.h"
#include "is-config.h"
#include "is-device-ischain.h"

static struct pablo_kunit_test_ctx {
	struct pablo_interface_irta *itf_irta;
	struct is_device_ischain idi;
} pkt_ctx;

static void pablo_interface_irta_dummy_kunit_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, 0);
}

static void pablo_interface_irta_probe_kunit_test(struct kunit *test)
{
	int ret;
	int i;
	struct pablo_interface_irta *itf_irta;

	ret = pablo_interface_irta_probe();
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		itf_irta = pablo_interface_irta_get(i);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, itf_irta);
		KUNIT_EXPECT_EQ(test, itf_irta->instance, (u32)i);
	}

	ret = pablo_interface_irta_remove();
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_interface_irta_remove_kunit_test(struct kunit *test)
{
	int ret;
	int i;
	struct pablo_interface_irta *itf_irta;

	ret = pablo_interface_irta_probe();
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pablo_interface_irta_remove();
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		itf_irta = pablo_interface_irta_get(i);
		KUNIT_EXPECT_EQ(test, (ulong)itf_irta, 0UL);
	}
}

static void pablo_interface_irta_dma_buf_attach_kunit_test(struct kunit *test)
{
	int ret;
	int instance = 0;
	int dma_buf_fd;
	struct pablo_interface_irta *pii;
	struct dma_buf *dma_buf;
	dma_addr_t dva;

	pablo_interface_irta_probe();
	pii = pablo_interface_irta_get(instance);

	dma_buf_fd = dma_heap_bufferfd_alloc(dma_heap_find("system"), SZ_4K, 0, 0);
	ret = pablo_interface_irta_dma_buf_attach(pii, ITF_IRTA_BUF_TYPE_RESULT, dma_buf_fd);
	KUNIT_EXPECT_EQ(test, ret, 0);

	dma_buf = pii->dma_buf[ITF_IRTA_BUF_TYPE_RESULT];
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, dma_buf);

	dva = pii->dva[ITF_IRTA_BUF_TYPE_RESULT];
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, (void *)dva);

	ret = pablo_interface_irta_dma_buf_detach(pii, ITF_IRTA_BUF_TYPE_RESULT);
	KUNIT_EXPECT_EQ(test, ret, 0);

	dva = pii->dva[ITF_IRTA_BUF_TYPE_RESULT];
	KUNIT_EXPECT_EQ(test, (ulong)dva, 0UL);

	pablo_interface_irta_remove();
}

static void pablo_interface_irta_dma_buf_detach_kunit_test(struct kunit *test)
{
	int ret;
	int instance = 0;
	struct pablo_interface_irta *pii;

	pablo_interface_irta_probe();
	pii = pablo_interface_irta_get(instance);

	ret = pablo_interface_irta_dma_buf_detach(pii, ITF_IRTA_BUF_TYPE_RESULT);
	KUNIT_EXPECT_NE(test, ret, 0);

	pablo_interface_irta_remove();
}

static void pablo_interface_irta_kva_map_kunit_test(struct kunit *test)
{
	int ret;
	int instance = 0;
	int dma_buf_fd;
	struct pablo_interface_irta *pii;
	void *kva;

	pablo_interface_irta_probe();
	pii = pablo_interface_irta_get(instance);

	dma_buf_fd = dma_heap_bufferfd_alloc(dma_heap_find("system"), SZ_4K, 0, 0);

	ret = pablo_interface_irta_dma_buf_attach(pii, ITF_IRTA_BUF_TYPE_RESULT, dma_buf_fd);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pablo_interface_irta_kva_map(pii, ITF_IRTA_BUF_TYPE_RESULT);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kva = pii->kva[ITF_IRTA_BUF_TYPE_RESULT];
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kva);

	memset(kva, 0, SZ_4K);

	ret = pablo_interface_irta_kva_unmap(pii, ITF_IRTA_BUF_TYPE_RESULT);
	KUNIT_EXPECT_EQ(test, ret, 0);

	kva = pii->kva[ITF_IRTA_BUF_TYPE_RESULT];
	KUNIT_EXPECT_EQ(test, (ulong)kva, 0UL);

	pablo_interface_irta_remove();
}

static void pablo_interface_irta_kva_unmap_kunit_test(struct kunit *test)
{
	int ret;
	int instance = 0;
	struct pablo_interface_irta *pii;
	int dma_buf_fd;

	pablo_interface_irta_probe();
	pii = pablo_interface_irta_get(instance);

	ret = pablo_interface_irta_kva_unmap(pii, ITF_IRTA_BUF_TYPE_RESULT);
	KUNIT_EXPECT_NE(test, ret, 0);

	dma_buf_fd = dma_heap_bufferfd_alloc(dma_heap_find("system"), SZ_4K, 0, 0);

	ret = pablo_interface_irta_dma_buf_attach(pii, ITF_IRTA_BUF_TYPE_RESULT, dma_buf_fd);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pablo_interface_irta_kva_unmap(pii, ITF_IRTA_BUF_TYPE_RESULT);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_interface_irta_remove();
}

static void test_pablo_interface_irta_result_buf_set(struct kunit *test,
				struct pablo_interface_irta *pii)
{
	int ret;
	int dma_buf_fd = dma_heap_bufferfd_alloc(dma_heap_find("system"), SZ_4K, 0, 0);

	ret = pablo_interface_irta_result_buf_set(pii, dma_buf_fd);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, dma_buf_fd,
		pii->dma_buf_fd[ITF_IRTA_BUF_TYPE_RESULT]);
}

static void pablo_interface_irta_result_buf_set_kunit_test(struct kunit *test)
{
	int ret;
	int instance = 0;
	int dma_buf_fd;
	struct pablo_interface_irta *pii;

	pablo_interface_irta_probe();
	pii = pablo_interface_irta_get(instance);

	test_pablo_interface_irta_result_buf_set(test, pii);

	dma_buf_fd = -EBADF;
	ret = pablo_interface_irta_result_buf_set(pii, dma_buf_fd);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	pablo_interface_irta_remove();
}

static void pablo_interface_irta_result_fcount_set_kunit_test(struct kunit *test)
{
	int ret;
	int instance = 0;
	u32 fcount;
	struct pablo_interface_irta *pii;

	pablo_interface_irta_probe();
	pii = pablo_interface_irta_get(instance);

	fcount = 1;
	ret = pablo_interface_irta_result_fcount_set(pii, fcount);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_interface_irta_remove();
}

static void pablo_interface_irta_start_kunit_test(struct kunit *test)
{
	int ret;
	int instance = 0;
	struct pablo_interface_irta *pii;
	struct is_device_ischain *idi = &pkt_ctx.idi;

	pablo_interface_irta_probe();
	pii = pablo_interface_irta_get(instance);

	ret = pablo_interface_irta_open(pii, idi);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pablo_interface_irta_start(pii);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_interface_irta_remove();
}

static void pablo_interface_irta_open_kunit_test(struct kunit *test)
{
	int ret;
	int instance = 0;
	struct pablo_interface_irta *pii;
	struct is_device_ischain *idi = &pkt_ctx.idi;

	pablo_interface_irta_probe();
	pii = pablo_interface_irta_get(instance);

	ret = pablo_interface_irta_open(pii, idi);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_interface_irta_remove();
}

static void pablo_interface_irta_close_kunit_test(struct kunit *test)
{
	int ret;
	int instance = 0;
	int dma_buf_fd;
	struct pablo_interface_irta *pii;
	struct is_device_ischain *idi = &pkt_ctx.idi;

	pablo_interface_irta_probe();
	pii = pablo_interface_irta_get(instance);

	ret = pablo_interface_irta_open(pii, idi);
	KUNIT_EXPECT_EQ(test, ret, 0);

	dma_buf_fd = dma_heap_bufferfd_alloc(dma_heap_find("system"), SZ_4K, 0, 0);
	ret = pablo_interface_irta_result_buf_set(pii, dma_buf_fd);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pablo_interface_irta_close(pii);
	KUNIT_EXPECT_EQ(test, ret, 0);

	KUNIT_EXPECT_NE(test, dma_buf_fd,
		pii->dma_buf_fd[ITF_IRTA_BUF_TYPE_RESULT]);
	KUNIT_EXPECT_EQ(test, 0,
		pii->dma_buf_fd[ITF_IRTA_BUF_TYPE_RESULT]);

	pablo_interface_irta_remove();
}

static int pablo_interface_irta_kunit_test_init(struct kunit *test)
{
	/* Backup the original pablo_interface_irta object */
	pkt_ctx.itf_irta = pablo_interface_irta_get(0);
	memset(&pkt_ctx.idi, 0, sizeof(pkt_ctx.idi));

	return 0;
}

static void pablo_interface_irta_kunit_test_exit(struct kunit *test)
{
	/* Retore the pablo_interface_irta object */
	pablo_interface_irta_set(pkt_ctx.itf_irta);
}

static struct kunit_case pablo_interface_irta_kunit_test_cases[] = {
	KUNIT_CASE(pablo_interface_irta_dummy_kunit_test),
	KUNIT_CASE(pablo_interface_irta_probe_kunit_test),
	KUNIT_CASE(pablo_interface_irta_remove_kunit_test),
	KUNIT_CASE(pablo_interface_irta_dma_buf_attach_kunit_test),
	KUNIT_CASE(pablo_interface_irta_dma_buf_detach_kunit_test),
	KUNIT_CASE(pablo_interface_irta_kva_map_kunit_test),
	KUNIT_CASE(pablo_interface_irta_kva_unmap_kunit_test),
	KUNIT_CASE(pablo_interface_irta_result_buf_set_kunit_test),
	KUNIT_CASE(pablo_interface_irta_result_fcount_set_kunit_test),
	KUNIT_CASE(pablo_interface_irta_start_kunit_test),
	KUNIT_CASE(pablo_interface_irta_open_kunit_test),
	KUNIT_CASE(pablo_interface_irta_close_kunit_test),
	{},
};

struct kunit_suite pablo_interface_irta_kunit_test_suite = {
	.name = "pablo-interface-irta-kunit-test",
	.init = pablo_interface_irta_kunit_test_init,
	.exit = pablo_interface_irta_kunit_test_exit,
	.test_cases = pablo_interface_irta_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_interface_irta_kunit_test_suite);

MODULE_LICENSE("GPL");
