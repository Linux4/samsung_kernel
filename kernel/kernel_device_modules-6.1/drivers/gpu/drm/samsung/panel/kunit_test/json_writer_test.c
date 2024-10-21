// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>

#include "panel_drv.h"
#include "ezop/json_writer.h"

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
static void json_writer_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void json_writer_test_jsonw_start_and_end_object_success(struct kunit *test)
{
	json_writer_t *w;
	char *buf = kunit_kzalloc(test, 10, GFP_KERNEL);

	w = jsonw_new(buf, 10);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, w);

	jsonw_start_object(w);
	jsonw_end_object(w);
	KUNIT_EXPECT_STREQ(test, buf, "{}");

	jsonw_destroy(&w);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int json_writer_test_init(struct kunit *test)
{
	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void json_writer_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case json_writer_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(json_writer_test_test),
	KUNIT_CASE(json_writer_test_jsonw_start_and_end_object_success),
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
static struct kunit_suite json_writer_test_module = {
	.name = "json_writer_test",
	.init = json_writer_test_init,
	.exit = json_writer_test_exit,
	.test_cases = json_writer_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&json_writer_test_module);

MODULE_LICENSE("GPL v2");
