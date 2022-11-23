/*
 * Copyright (c) 2020-2021 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/string.h>
#include "include/defex_caches.h"

extern struct defex_file_cache_list file_cache;

static void defex_file_cache_update_test(struct test *test)
{
	/* EXPECT_EQ(test, 1, defex_file_cache_update()); */
}


static void defex_file_cache_init_test(struct test *test)
{
	struct defex_file_cache_list copy_file_cache;

	/* save current attribute status */
	memcpy(&copy_file_cache, &file_cache, sizeof(copy_file_cache));

	defex_file_cache_init();
	ASSERT_NOT_ERR_OR_NULL(test, &file_cache);
	EXPECT_EQ(test, file_cache.first_entry, 0);
	EXPECT_EQ(test, file_cache.last_entry, FILE_CACHE_SIZE - 1);

	/* restore saved attribute status */
	memcpy(&file_cache, &copy_file_cache, sizeof(copy_file_cache));
}


static void defex_file_cache_find_test(struct test *test)
{
	/* EXPECT_EQ(test, 1, defex_file_cache_find()); */
}


static void defex_file_cache_delete_test(struct test *test)
{
	/* EXPECT_EQ(test, 1, defex_file_cache_delete()); */
}


static void defex_file_cache_add_test(struct test *test)
{
	/* EXPECT_EQ(test, 1, defex_file_cache_add()); */
}


static void defex_caches_lock_test(struct test *test)
{
	/* EXPECT_EQ(test, 1, defex_caches_lock()); */
}


static int defex_caches_test_init(struct test *test)
{
	return 0;
}

static void defex_caches_test_exit(struct test *test)
{
}

static struct test_case defex_caches_test_cases[] = {
	/* TEST FUNC DEFINES */
	TEST_CASE(defex_file_cache_update_test),
	TEST_CASE(defex_file_cache_init_test),
	TEST_CASE(defex_file_cache_find_test),
	TEST_CASE(defex_file_cache_delete_test),
	TEST_CASE(defex_file_cache_add_test),
	TEST_CASE(defex_caches_lock_test),
	{},
};

static struct test_module defex_caches_test_module = {
	.name = "defex_caches_test",
	.init = defex_caches_test_init,
	.exit = defex_caches_test_exit,
	.test_cases = defex_caches_test_cases,
};
module_test(defex_caches_test_module);

