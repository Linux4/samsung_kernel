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


static void pablo_crta_bufmgr_open_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct pablo_crta_bufmgr *bufmgr;

	if (!IS_ENABLED(CRTA_CALL))
		return;

	instance = 0;
	bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_BASE, instance, 0);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, open);
	KUNIT_EXPECT_TRUE(test, ret == 0 || ret == -ENODEV);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_crta_bufmgr_open_negative_kunit_test(struct kunit *test)
{
	u32 instance;
	struct pablo_crta_bufmgr *bufmgr;

	if (!IS_ENABLED(CRTA_CALL))
		return;

	/* invalid instance */
	instance = IS_STREAM_COUNT;

	bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_MAX, instance, 0);
	KUNIT_EXPECT_PTR_EQ(test, NULL, bufmgr);
}

static void pablo_crta_bufmgr_get_buf_kunit_test(struct kunit *test)
{
	int ret = 0, ret_open;
	u32 instance, buf_type;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!IS_ENABLED(CRTA_CALL))
		return;

	instance = 0;

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		bufmgr = pablo_get_crta_bufmgr(buf_type, instance, 0);

		ret_open = CALL_CRTA_BUFMGR_OPS(bufmgr, open);
		KUNIT_EXPECT_TRUE(test, ret_open == 0 || ret_open == -ENODEV);

		buf_info.dva = 0;
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, 0, false, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);
		if (ret_open)
			KUNIT_EXPECT_EQ(test, 0, buf_info.dva);
		else
			KUNIT_EXPECT_NE(test, 0, buf_info.dva);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, 0, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_static_buf, &buf_info);
		KUNIT_EXPECT_EQ(test, 0, ret);
		KUNIT_EXPECT_NE(test, 0, buf_info.dva);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}

}

static void pablo_crta_bufmgr_get_buf_negative_kunit_test(struct kunit *test)
{
	int ret = 0, ret_open;
	u32 instance, buf_type;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!IS_ENABLED(CRTA_CALL))
		return;

	instance = 0;

	bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_BASE, instance, 0);

	/* get_buf before open */
	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, 0, false, &buf_info);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0, buf_info.dva);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, 1, &buf_info);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0, buf_info.dva);

	ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_static_buf, &buf_info);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0, buf_info.dva);

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		bufmgr = pablo_get_crta_bufmgr(buf_type, instance, 0);

		ret_open = CALL_CRTA_BUFMGR_OPS(bufmgr, open);
		KUNIT_EXPECT_TRUE(test, ret_open == 0 || ret_open == -ENODEV);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, 0, false, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		/* no process buf with fcount = 1 */
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, 1, &buf_info);
		KUNIT_EXPECT_NE(test, 0, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}


}

static void pablo_crta_bufmgr_put_buf_kunit_test(struct kunit *test)
{
	int ret = 0, ret_open;
	u32 instance, buf_type;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!IS_ENABLED(CRTA_CALL))
		return;

	instance = 0;

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		bufmgr = pablo_get_crta_bufmgr(buf_type, instance, 0);

		ret_open = CALL_CRTA_BUFMGR_OPS(bufmgr, open);
		KUNIT_EXPECT_TRUE(test, ret_open == 0 || ret_open == -ENODEV);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, 0, false, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		/* put_buf after get_free_buf */
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, 0, false, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_process_buf, 0, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		/* put_buf after get_process_buf */
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}
}

static void pablo_crta_bufmgr_put_buf_negative_kunit_test(struct kunit *test)
{
	int ret = 0, ret_open;
	u32 instance, buf_type;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!IS_ENABLED(CRTA_CALL))
		return;

	instance = 0;

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		bufmgr = pablo_get_crta_bufmgr(buf_type, instance, 0);

		/* put_buf before open */
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
		KUNIT_EXPECT_NE(test, 0, ret);
	}

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		bufmgr = pablo_get_crta_bufmgr(buf_type, instance, 0);

		ret_open = CALL_CRTA_BUFMGR_OPS(bufmgr, open);
		KUNIT_EXPECT_TRUE(test, ret_open == 0 || ret_open == -ENODEV);

		/* put_buf with invalid buf_info */
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
		KUNIT_EXPECT_NE(test, 0, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, 0, false, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		/* put_buf with free state frame */
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
		KUNIT_EXPECT_NE(test, 0, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}

}

static void pablo_crta_bufmgr_flush_buf_kunit_test(struct kunit *test)
{
	int ret = 0, ret_open;
	u32 instance, buf_type;
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	if (!IS_ENABLED(CRTA_CALL))
		return;

	instance = 0;

	for (buf_type = PABLO_CRTA_BUF_BASE; buf_type < PABLO_CRTA_BUF_MAX; buf_type++) {
		bufmgr = pablo_get_crta_bufmgr(buf_type, instance, 0);

		ret_open = CALL_CRTA_BUFMGR_OPS(bufmgr, open);
		KUNIT_EXPECT_TRUE(test, ret_open == 0 || ret_open == -ENODEV);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, 0, false, &buf_info);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, flush_buf, 0);
		KUNIT_EXPECT_EQ(test, ret_open, ret);

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, close);
		KUNIT_EXPECT_EQ(test, 0, ret);
	}
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
