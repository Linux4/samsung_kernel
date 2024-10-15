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

#include "pablo-kunit-test.h"
#include "pablo-json.h"

#define BUF_SIZE 512

static void pablo_lib_json_escape_kunit_test(struct kunit *test)
{
	char exp[] = "{\"name\":\"\\tHello: \\\"Pablo\\\"\\n\"}";
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_str(p, "name", "\tHello: \"Pablo\"\n", &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (unsigned long)(p - buf), sizeof(exp) - 1);
	KUNIT_EXPECT_STREQ(test, buf, (char *)exp);

	kunit_kfree(test, buf);
}

static void pablo_lib_json_len_kunit_test(struct kunit *test)
{
	char exp[] = "{\"name\":\"\\tHello\"}";
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_nstr(p, "name", "\tHello: \"Pablo\"\n", 6, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (unsigned long)(p - buf), sizeof(exp) - 1);
	KUNIT_EXPECT_STREQ(test, buf, (char *)exp);

	kunit_kfree(test, buf);
}

static void pablo_lib_json_empty_object_kunit_test(struct kunit *test)
{
	char exp[] = "{}";
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (unsigned long)(p - buf), sizeof(exp) - 1);
	KUNIT_EXPECT_STREQ(test, buf, (char *)exp);

	kunit_kfree(test, buf);
}

static void pablo_lib_json_empty_array_kunit_test(struct kunit *test)
{
	char exp[] = "{\"array\":[]}";
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_array_open(p, "array", &rem);
	p = pablo_json_array_close(p, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (unsigned long)(p - buf), sizeof(exp) - 1);
	KUNIT_EXPECT_STREQ(test, buf, (char *)exp);

	kunit_kfree(test, buf);
}

static void pablo_lib_json_empty_object_array_kunit_test(struct kunit *test)
{
	char exp[] = "{\"array\":[{},{}]}";
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_array_open(p, "array", &rem);
	p = pablo_json_object_open(p, NULL, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_object_open(p, NULL, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_array_close(p, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (unsigned long)(p - buf), sizeof(exp) - 1);
	KUNIT_EXPECT_STREQ(test, buf, (char *)exp);

	kunit_kfree(test, buf);
}

static void pablo_lib_json_null_kunit_test(struct kunit *test)
{
	char exp[] = "{\"null\":null}";
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_null(p, "null", &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (unsigned long)(p - buf), sizeof(exp) - 1);
	KUNIT_EXPECT_STREQ(test, buf, (char *)exp);

	kunit_kfree(test, buf);
}

static void pablo_lib_json_bool_kunit_test(struct kunit *test)
{
	char exp[] = "{\"bool0\":false,\"bool1\":true}";
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_bool(p, "bool0", false, &rem);
	p = pablo_json_bool(p, "bool1", true, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (unsigned long)(p - buf), sizeof(exp) - 1);
	KUNIT_EXPECT_STREQ(test, buf, (char *)exp);

	kunit_kfree(test, buf);
}

static void pablo_lib_json_int_kunit_test(struct kunit *test)
{
	char *exp = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;
	int len;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, exp);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	len = sprintf(exp, "{\"min\":%d,\"max\":%d}", INT_MIN, INT_MAX);
	KUNIT_ASSERT_LT(test, len, BUF_SIZE);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_int(p, "min", INT_MIN, &rem);
	p = pablo_json_int(p, "max", INT_MAX, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (p - buf), (long)len);
	KUNIT_EXPECT_STREQ(test, buf, exp);

	kunit_kfree(test, exp);
	kunit_kfree(test, buf);
}

static void pablo_lib_json_uint_kunit_test(struct kunit *test)
{
	char *exp = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;
	int len;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, exp);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	len = sprintf(exp, "{\"min\":%u,\"max\":%u}", 0U, UINT_MAX);
	KUNIT_ASSERT_LT(test, len, BUF_SIZE);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_uint(p, "min", 0, &rem);
	p = pablo_json_uint(p, "max", UINT_MAX, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (p - buf), (long)len);
	KUNIT_EXPECT_STREQ(test, buf, exp);

	kunit_kfree(test, exp);
	kunit_kfree(test, buf);
}

static void pablo_lib_json_long_kunit_test(struct kunit *test)
{
	char *exp = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;
	int len;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, exp);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	len = sprintf(exp, "{\"min\":%ld,\"max\":%ld}", LONG_MIN, LONG_MAX);
	KUNIT_ASSERT_LT(test, len, BUF_SIZE);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_long(p, "min", LONG_MIN, &rem);
	p = pablo_json_long(p, "max", LONG_MAX, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (p - buf), (long)len);
	KUNIT_EXPECT_STREQ(test, buf, exp);

	kunit_kfree(test, exp);
	kunit_kfree(test, buf);
}

static void pablo_lib_json_ulong_kunit_test(struct kunit *test)
{
	char *exp = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;
	int len;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, exp);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	len = sprintf(exp, "{\"min\":%lu,\"max\":%lu}", 0UL, ULONG_MAX);
	KUNIT_ASSERT_LT(test, len, BUF_SIZE);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_ulong(p, "min", 0, &rem);
	p = pablo_json_ulong(p, "max", ULONG_MAX, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (p - buf), (long)len);
	KUNIT_EXPECT_STREQ(test, buf, exp);

	kunit_kfree(test, exp);
	kunit_kfree(test, buf);
}

static void pablo_lib_json_llong_kunit_test(struct kunit *test)
{
	char *exp = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;
	int len;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, exp);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	len = sprintf(exp, "{\"min\":%lld,\"max\":%lld}", LLONG_MIN, LLONG_MAX);
	KUNIT_ASSERT_LT(test, len, BUF_SIZE);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_llong(p, "min", LLONG_MIN, &rem);
	p = pablo_json_llong(p, "max", LLONG_MAX, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (p - buf), (long)len);
	KUNIT_EXPECT_STREQ(test, buf, exp);

	kunit_kfree(test, exp);
	kunit_kfree(test, buf);
}

static void pablo_lib_json_array_kunit_test(struct kunit *test)
{
	char exp[] = "{\"array\":[0,1,2,3,4]}";
	char *buf = (char *)kunit_kzalloc(test, BUF_SIZE, GFP_KERNEL);
	size_t rem = BUF_SIZE;
	char *p;
	int i;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	p = pablo_json_object_open(buf, NULL, &rem);
	p = pablo_json_array_open(p, "array", &rem);
	for (i = 0; i < 5; i++)
		p = pablo_json_int(p, NULL, i, &rem);
	p = pablo_json_array_close(p, &rem);
	p = pablo_json_object_close(p, &rem);
	p = pablo_json_end(p, &rem);

	KUNIT_EXPECT_EQ(test, (unsigned long)(p - buf), sizeof(exp) - 1);
	KUNIT_EXPECT_STREQ(test, buf, (char *)exp);

	kunit_kfree(test, buf);
}

static struct kunit_case pablo_lib_json_kunit_test_cases[] = {
	KUNIT_CASE(pablo_lib_json_escape_kunit_test),
	KUNIT_CASE(pablo_lib_json_len_kunit_test),
	KUNIT_CASE(pablo_lib_json_empty_object_kunit_test),
	KUNIT_CASE(pablo_lib_json_empty_array_kunit_test),
	KUNIT_CASE(pablo_lib_json_empty_object_array_kunit_test),
	KUNIT_CASE(pablo_lib_json_null_kunit_test),
	KUNIT_CASE(pablo_lib_json_bool_kunit_test),
	KUNIT_CASE(pablo_lib_json_int_kunit_test),
	KUNIT_CASE(pablo_lib_json_uint_kunit_test),
	KUNIT_CASE(pablo_lib_json_long_kunit_test),
	KUNIT_CASE(pablo_lib_json_ulong_kunit_test),
	KUNIT_CASE(pablo_lib_json_llong_kunit_test),
	KUNIT_CASE(pablo_lib_json_array_kunit_test),
	{},
};

struct kunit_suite pablo_lib_json_kunit_test_suite = {
	.name = "pablo-lib-json-kunit-test",
	.test_cases = pablo_lib_json_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_lib_json_kunit_test_suite);

MODULE_LICENSE("GPL");
