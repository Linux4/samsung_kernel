/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "include/defex_internal.h"
#include "include/defex_test.h"

static void privesc_status_store_test(struct test *test)
{
	char *invalid_prefix = "test";
	char *over_range_prefix = "5";
	char *valid_prefix = "2";

	/* buffer null */
	EXPECT_EQ(test, privesc_status_store(NULL), -EINVAL);
	/* invalid prefix */
	EXPECT_EQ(test, privesc_status_store(invalid_prefix), -EINVAL);
	/* over range prefix */
	EXPECT_EQ(test, privesc_status_store(over_range_prefix), -EINVAL);
	/* valid prefix */
	EXPECT_EQ(test, privesc_status_store(valid_prefix), 0);
}

static int defex_privesc_test_init(struct test *test)
{
	return 0;
}

static void defex_privesc_test_exit(struct test *test)
{
}

static struct test_case defex_privesc_test_cases[] = {
	/* TEST FUNC DEFINES */
	TEST_CASE(privesc_status_store_test),
	{},
};

static struct test_module defex_privesc_test_module = {
	.name = "defex_privesc_test",
	.init = defex_privesc_test_init,
	.exit = defex_privesc_test_exit,
	.test_cases = defex_privesc_test_cases,
};
module_test(defex_privesc_test_module);
