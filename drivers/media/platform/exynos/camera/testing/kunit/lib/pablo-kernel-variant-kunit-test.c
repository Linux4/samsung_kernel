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
#include "pablo-kernel-variant.h"

static struct pablo_kunit_test_ctx {
	struct tasklet_struct pkt_tasklet;
} pkt_ctx;

PKV_DECLARE_TASKLET_CALLBACK(pkt_dummy, t)
{
	/* Do nothing */
}

/* Define testcases */
#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
static void pkv_tasklet_setup_kunit_test(struct kunit *test)
{
	struct tasklet_struct *tasklet = &pkt_ctx.pkt_tasklet;

	pkv_tasklet_setup(tasklet, tasklet_pkt_dummy);
	KUNIT_EXPECT_TRUE(test, tasklet->use_callback);
	KUNIT_EXPECT_PTR_EQ(test, (void *)tasklet->callback, (void *)tasklet_pkt_dummy);
}
#else
static void pkv_tasklet_setup_kunit_test(struct kunit *test)
{
	struct tasklet_struct *tasklet = &pkt_ctx.pkt_tasklet;

	pkv_tasklet_setup(tasklet, tasklet_pkt_dummy);
	KUNIT_EXPECT_FALSE(test, tasklet->use_callback);
	KUNIT_EXPECT_PTR_EQ(test, (void *)tasklet->func, (void *)tasklet_pkt_dummy);
}
#endif

static void pkv_dma_buf_kunit_test(struct kunit *test)
{
	struct dma_heap *dh;
	struct dma_buf *db;
	void *kva;

	dh = dma_heap_find("system");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dh);

	db = dma_heap_buffer_alloc(dh, 0x10, 0, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, db);

	/* TC #1. Map DMA_BUF kernel virtual address */
	kva = pkv_dma_buf_vmap(db);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, kva);
	KUNIT_EXPECT_NE(test, db->vmapping_counter, (unsigned int)0);

	/* TC #2. Unmap DMA_BUF kernel virtual address */
	pkv_dma_buf_vunmap(db, kva);
	KUNIT_EXPECT_EQ(test, db->vmapping_counter, (unsigned int)0);

	dma_heap_buffer_free(db);
}

static struct kunit_case pablo_kernel_variant_kunit_test_cases[] = {
	KUNIT_CASE(pkv_tasklet_setup_kunit_test),
	KUNIT_CASE(pkv_dma_buf_kunit_test),
	{},
};

struct kunit_suite pablo_kernel_variant_kunit_test_suite = {
	.name = "pablo-kernel-variant-kunit-test",
	.test_cases = pablo_kernel_variant_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_kernel_variant_kunit_test_suite);

MODULE_LICENSE("GPL");
