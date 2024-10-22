// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>

#include "panel_drv.h"
#include "ezop/json_reader.h"

struct json_reader_test {
	json_reader_t *r;
	struct list_head *func_list;
};

/*
 * This is the most fundamental element of KUnit, the test case. A test case
 * makes a set EXPECTATIONs and ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct test *)`. `struct test` is a context object that stores
 * information about the current test.
 */

/* NOTE: UML TC */
static void json_reader_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void json_reader_test_jsonr_parse(struct kunit *test)
{
	struct json_reader_test *ctx = test->priv;
	char *js;

	js = "1";
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_EXPECT_EQ(test, current_token_type(ctx->r), (u32)JSMN_PRIMITIVE);

	js = "{}";
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_EXPECT_EQ(test, current_token_type(ctx->r), (u32)JSMN_OBJECT);

	js = "[1,2,3]";
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_EXPECT_EQ(test, current_token_type(ctx->r), (u32)JSMN_ARRAY);

	js = "\"test string\"";
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_EXPECT_EQ(test, current_token_type(ctx->r), (u32)JSMN_STRING);
}

static void json_reader_test_jsonr_uint_test(struct kunit *test)
{
	struct json_reader_test *ctx = test->priv;
	char *js;
	unsigned int value;

	js = "1";
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_EXPECT_TRUE(test, jsonr_uint(ctx->r, &value) == 0);

	js = "{}";
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_EXPECT_EQ(test, jsonr_uint(ctx->r, &value), -EIO);

	js = "[1,2,3]";
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_EXPECT_EQ(test, jsonr_uint(ctx->r, &value), -EIO);

	js = "\"test string\"";
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_EXPECT_EQ(test, jsonr_uint(ctx->r, &value), -EIO);

	js = "1";
	KUNIT_ASSERT_EQ(test, jsonr_parse(ctx->r, js), JSMN_SUCCESS);
	KUNIT_EXPECT_TRUE(test, jsonr_uint(ctx->r, &value) == 0);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int json_reader_test_init(struct kunit *test)
{
	struct json_reader_test *ctx =
		kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	struct list_head *func_list =
		kunit_kzalloc(test, sizeof(*func_list), GFP_KERNEL);

	INIT_LIST_HEAD(func_list);
	ctx->r = jsonr_new(SZ_128, func_list);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx->r);

	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void json_reader_test_exit(struct kunit *test)
{
	struct json_reader_test *ctx = test->priv;

	jsonr_destroy(&ctx->r);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case json_reader_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(json_reader_test_test),
	KUNIT_CASE(json_reader_test_jsonr_parse),
	KUNIT_CASE(json_reader_test_jsonr_uint_test),
	{},
};

/*
 * This defines a suite or grouping of tests.
 *
 * Test cases are defined as belonging to the suite by adding them to
 * `test_cases`.
 *
 * Often it is desirable to run some function which will set up things which
 * will be used by every test; this is accomplished with an `init` function
 * which runs before each test case is invoked. Similarly, an `exit` function
 * may be specified which runs after every test case and can be used to for
 * cleanup. For clarity, running tests in a test module would behave as follows:
 *
 * module.init(test);
 * module.test_case[0](test);
 * module.exit(test);
 * module.init(test);
 * module.test_case[1](test);
 * module.exit(test);
 * ...;
 */
static struct kunit_suite json_reader_test_module = {
	.name = "json_reader_test",
	.init = json_reader_test_init,
	.exit = json_reader_test_exit,
	.test_cases = json_reader_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&json_reader_test_module);

MODULE_LICENSE("GPL v2");
