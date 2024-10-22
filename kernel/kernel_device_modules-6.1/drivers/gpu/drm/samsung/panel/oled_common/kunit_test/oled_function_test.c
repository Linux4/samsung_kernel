// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "test_helper.h"
#include "panel_drv.h"
#include "panel_function.h"
#include "oled_common/oled_function.h"

/*
 * This is the most fundamental element of KUnit, the test case. A test case
 * makes a set EXPECTATIONs and ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct kunit *)`. `struct kunit` is a context object that stores
 * information about the current test.
 */

/* NOTE: UML TC */
static void oled_function_test_test(struct kunit *test)
{
	KUNIT_SUCCEED(test);
}

static void oled_function_test_oled_function_table_should_be_initialized(struct kunit *test)
{
	int i, len;
	char message[SZ_256];

	for (i = 0; i < MAX_OLED_FUNCTION; i++) {
		len = snprintf(message, sizeof(message),
				"Expected (get_pnobj_function_name(&OLED_FUNC(%d)) != NULL), but it is NULL\n", i);
		KUNIT_EXPECT_TRUE_MSG(test, get_pnobj_function_name(&OLED_FUNC(i)) != NULL, message);

		len = snprintf(message, sizeof(message),
				"Expected (get_pnobj_function_addr(&OLED_FUNC(%d)) != 0), but it is NULL\n", i);
		KUNIT_EXPECT_TRUE_MSG(test, get_pnobj_function_addr(&OLED_FUNC(i)) != 0UL, message);
	}
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int oled_function_test_init(struct kunit *test)
{
	struct panel_device *panel;

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);

	test->priv = panel;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void oled_function_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case oled_function_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(oled_function_test_test),
	KUNIT_CASE(oled_function_test_oled_function_table_should_be_initialized),
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
static struct kunit_suite oled_function_test_module = {
	.name = "oled_function_test",
	.init = oled_function_test_init,
	.exit = oled_function_test_exit,
	.test_cases = oled_function_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&oled_function_test_module);

MODULE_LICENSE("GPL v2");
