// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>

#include "panel_drv.h"
#include "ezop/json.h"

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
static void json_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void json_test_parse_json_object(struct kunit *test)
{
	int numtok;
	jsmntok_t *tokens, *tok;
	char *json;
	size_t size;

	/* single object */
	json = "{}";
	size = strlen(json);

	tokens = parse_json(json, size, &numtok);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tokens);
	KUNIT_EXPECT_EQ(test, numtok, 1);

	tok = tokens;
	KUNIT_EXPECT_EQ(test, (u32)(tok++)->type, (u32)JSMN_OBJECT);

	free_json(json, size, tokens);
}

static void json_test_parse_json_nested_object(struct kunit *test)
{
	int numtok;
	jsmntok_t *tokens, *tok;
	char *json;
	size_t size;

	/* nested object */
	json = "{\"key\":{\"key:\":1}}";
	size = strlen(json);

	tokens = parse_json(json, size, &numtok);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tokens);
	KUNIT_EXPECT_EQ(test, numtok, 5);

	tok = tokens;

	/* {\"key\":{\"key:\":1}} */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_OBJECT);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 2);
	/* \"key\":{\"key:\":1} */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_STRING);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 0);
	/* {\"key:\":1} */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_OBJECT);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 2);
	/* \"key:\":1 */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_STRING);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 0);
	/* 1 */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_PRIMITIVE);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 0);

	KUNIT_EXPECT_EQ(test, (int)numtok, (int)(tok - tokens));

	free_json(json, size, tokens);
}

static void json_test_parse_json_empty_array(struct kunit *test)
{
	int numtok;
	jsmntok_t *tokens, *tok;
	char *json;
	size_t size;

	/* empty array */
	json = "[]";
	size = strlen(json);

	tokens = parse_json(json, size, &numtok);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tokens);
	KUNIT_EXPECT_EQ(test, numtok, 1);

	tok = tokens;
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_ARRAY);
	KUNIT_EXPECT_EQ(test, (tok)->size, 0);

	free_json(json, size, tokens);
}

static void json_test_parse_json_nested_array(struct kunit *test)
{
	int numtok;
	jsmntok_t *tokens, *tok;
	char *json;
	size_t size;

	/* nested array */
	json = "[[1],[[2],3],[[4,[5]]]]";
	size = strlen(json);

	tokens = parse_json(json, size, &numtok);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tokens);

	tok = tokens;

	/* [[1],[[2],3],[[4,[5]]]] */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_ARRAY);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 3);

	/* [1],[[2],3],[[4,[5]]] */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_ARRAY);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 1);
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_PRIMITIVE);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 0);

	/* [[2],3],[[4,[5]]] */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_ARRAY);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 2);
	/* [2],3 */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_ARRAY);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 1);
	/* 2 */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_PRIMITIVE);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 0);
	/* 3 */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_PRIMITIVE);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 0);

	/* [[4,[5]]] */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_ARRAY);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 1);
	/* [4,[5]] */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_ARRAY);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 2);
	/* 4,[5] */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_PRIMITIVE);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 0);
	/* [5] */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_ARRAY);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 1);
	/* 5 */
	KUNIT_EXPECT_EQ(test, (u32)(tok)->type, (u32)JSMN_PRIMITIVE);
	KUNIT_EXPECT_EQ(test, (tok++)->size, 0);

	KUNIT_EXPECT_EQ(test, numtok, (int)(tok - tokens));

	free_json(json, size, tokens);
}

static void json_test_json_strcpy_test(struct kunit *test)
{
	int numtok;
	jsmntok_t *tokens, *tok;
	char *json = "\"test string\"";
	size_t size = strlen(json);
	char buf[SZ_32] = "12345678901234567890";

	tokens = parse_json(json, size, &numtok);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tokens);
	tok = tokens;

	json_strcpy(json, tok, buf);
	KUNIT_ASSERT_STREQ(test, (char *)buf, "test string");

	free_json(json, size, tokens);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int json_test_init(struct kunit *test)
{
	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void json_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case json_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(json_test_test),
	KUNIT_CASE(json_test_parse_json_object),
	KUNIT_CASE(json_test_parse_json_nested_object),
	KUNIT_CASE(json_test_parse_json_empty_array),
	KUNIT_CASE(json_test_parse_json_nested_array),
	KUNIT_CASE(json_test_json_strcpy_test),
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
static struct kunit_suite json_test_module = {
	.name = "json_test",
	.init = json_test_init,
	.exit = json_test_exit,
	.test_cases = json_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&json_test_module);

MODULE_LICENSE("GPL v2");
