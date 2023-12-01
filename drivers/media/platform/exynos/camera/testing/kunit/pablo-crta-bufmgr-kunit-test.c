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
#include "pablo-crta-bufmgr.h"
#include "is-config.h"

/* Define the test cases. */
static struct test_ctx {
	struct pablo_crta_bufmgr	bufmgr;
} pablo_crta_bufmgr_test_ctx;


static void pablo_crta_bufmgr_open_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_crta_bufmgr *bufmgr;

	if (!IS_ENABLED(CRTA_CALL))
		return;

	test_ctx = &pablo_crta_bufmgr_test_ctx;
	bufmgr = &test_ctx->bufmgr;
	instance = 0;

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, open, instance);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_crta_bufmgr_open_negative_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_crta_bufmgr *bufmgr;

	if (!IS_ENABLED(CRTA_CALL))
		return;

	test_ctx = &pablo_crta_bufmgr_test_ctx;
	bufmgr = &test_ctx->bufmgr;

	/* invalid instance */
	instance = IS_STREAM_COUNT;

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, open, instance);
	KUNIT_EXPECT_NE(test, 0, ret);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
	KUNIT_EXPECT_NE(test, 0, ret);
}

static void pablo_crta_bufmgr_get_buf_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance, buf_type;
	struct test_ctx *test_ctx;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!IS_ENABLED(CRTA_CALL))
		return;

	test_ctx = &pablo_crta_bufmgr_test_ctx;
	bufmgr = &test_ctx->bufmgr;
	instance = 0;

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, open, instance);
	KUNIT_EXPECT_EQ(test, 0, ret);

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, buf_type,
			0, false, &buf_info);
		KUNIT_EXPECT_TRUE(test, ret == 0 || ret == -ENODEV);

		if (!ret) {
			KUNIT_EXPECT_NE(test, 0, buf_info.dva);
			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, buf_type,
				0, &buf_info);
			KUNIT_EXPECT_EQ(test, 0, ret);
		}
	}

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_static_buf, PABLO_CRTA_STATIC_BUF_SENSOR, &buf_info);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_NE(test, 0, buf_info.dva);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_crta_bufmgr_get_buf_negative_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance, buf_type;
	struct test_ctx *test_ctx;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!IS_ENABLED(CRTA_CALL))
		return;

	test_ctx = &pablo_crta_bufmgr_test_ctx;
	bufmgr = &test_ctx->bufmgr;
	instance = 0;

	/* get_buf before open */
	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, PABLO_CRTA_BUF_BASE, 0, false, &buf_info);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0, buf_info.dva);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, PABLO_CRTA_BUF_BASE, 1, &buf_info);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0, buf_info.dva);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_static_buf, PABLO_CRTA_STATIC_BUF_SENSOR, &buf_info);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0, buf_info.dva);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, open, instance);
	KUNIT_EXPECT_EQ(test, 0, ret);

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, buf_type,
			0, false, &buf_info);
		KUNIT_EXPECT_TRUE(test, ret == 0 || ret == -ENODEV);

		/* no process buf with fcount = 1 */
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, buf_type,
			1, &buf_info);
		KUNIT_EXPECT_NE(test, 0, ret);
	}

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_crta_bufmgr_put_buf_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance, buf_type;
	struct test_ctx *test_ctx;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!IS_ENABLED(CRTA_CALL))
		return;

	test_ctx = &pablo_crta_bufmgr_test_ctx;
	bufmgr = &test_ctx->bufmgr;
	instance = 0;

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, open, instance);
	KUNIT_EXPECT_EQ(test, 0, ret);

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, buf_type,
			0, false, &buf_info);
		KUNIT_EXPECT_TRUE(test, ret == 0 || ret == -ENODEV);

		if (!ret) {
			/* put_buf after get_free_buf */
			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
			KUNIT_EXPECT_EQ(test, 0, ret);
		}
	}

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, buf_type,
			0, false, &buf_info);
		KUNIT_EXPECT_TRUE(test, ret == 0 || ret == -ENODEV);

		if (!ret) {
			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, buf_type,
				0, &buf_info);
			KUNIT_EXPECT_EQ(test, 0, ret);

			/* put_buf after get_process_buf */
			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
			KUNIT_EXPECT_EQ(test, 0, ret);
		}
	}

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_crta_bufmgr_put_buf_negative_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance, buf_type;
	struct test_ctx *test_ctx;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!IS_ENABLED(CRTA_CALL))
		return;

	test_ctx = &pablo_crta_bufmgr_test_ctx;
	bufmgr = &test_ctx->bufmgr;
	instance = 0;

	/* put_buf before open */
	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
	KUNIT_EXPECT_NE(test, 0, ret);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, open, instance);
	KUNIT_EXPECT_EQ(test, 0, ret);

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		/* put_buf with invalid buf_info */
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
		KUNIT_EXPECT_NE(test, 0, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, buf_type,
			0, false, &buf_info);
		KUNIT_EXPECT_TRUE(test, ret == 0 || ret == -ENODEV);

		if (!ret) {
			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
			KUNIT_EXPECT_EQ(test, 0, ret);

			/* put_buf with free state frame */
			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
			KUNIT_EXPECT_NE(test, 0, ret);
		}
	}

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_crta_bufmgr_flush_buf_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance, buf_type;
	struct test_ctx *test_ctx;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!IS_ENABLED(CRTA_CALL))
		return;

	test_ctx = &pablo_crta_bufmgr_test_ctx;
	bufmgr = &test_ctx->bufmgr;
	instance = 0;

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, open, instance);
	KUNIT_EXPECT_EQ(test, 0, ret);

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, buf_type,
			0, false, &buf_info);
		KUNIT_EXPECT_TRUE(test, ret == 0 || ret == -ENODEV);

		if (!ret) {
			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, flush_buf, buf_type, 0);
			KUNIT_EXPECT_EQ(test, 0, ret);
		}
	}

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static struct kunit_case pablo_crta_bufmgr_kunit_test_cases[] = {
	KUNIT_CASE(pablo_crta_bufmgr_open_kunit_test),
	KUNIT_CASE(pablo_crta_bufmgr_open_negative_kunit_test),
	KUNIT_CASE(pablo_crta_bufmgr_get_buf_kunit_test),
	KUNIT_CASE(pablo_crta_bufmgr_get_buf_negative_kunit_test),
	KUNIT_CASE(pablo_crta_bufmgr_put_buf_kunit_test),
	KUNIT_CASE(pablo_crta_bufmgr_put_buf_negative_kunit_test),
	KUNIT_CASE(pablo_crta_bufmgr_flush_buf_kunit_test),
	{},
};

static int pablo_crta_bufmgr_kunit_test_init(struct kunit *test)
{
	/* probe crta_bufmgr */
	pablo_crta_bufmgr_probe(&pablo_crta_bufmgr_test_ctx.bufmgr);

	return 0;
}

static void pablo_crta_bufmgr_kunit_test_exit(struct kunit *test)
{

}

struct kunit_suite pablo_crta_bufmgr_kunit_test_suite = {
	.name = "pablo-crta_bufmgr-kunit-test",
	.init = pablo_crta_bufmgr_kunit_test_init,
	.exit = pablo_crta_bufmgr_kunit_test_exit,
	.test_cases = pablo_crta_bufmgr_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_crta_bufmgr_kunit_test_suite);

MODULE_LICENSE("GPL");
