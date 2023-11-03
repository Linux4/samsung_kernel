// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "test_helper.h"
#include "../panel_drv.h"

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

#if !defined(CONFIG_UML)
/* NOTE: Target running TC must be in the #ifndef CONFIG_UML */
static void s6e3fc3_r11s_test_foo(struct kunit *test)
{
	/*
	 * This is an EXPECTATION; it is how KUnit tests things. When you want
	 * to test a piece of code, you set some expectations about what the
	 * code should do. KUnit then runs the test and verifies that the code's
	 * behavior matched what was expected.
	 */
	KUNIT_EXPECT_EQ(test, 1, 2); // Obvious failure.
}
#endif

/* NOTE: UML TC */
static void s6e3fc3_r11s_test_test(struct kunit *test)
{
	KUNIT_SUCCEED(test);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int s6e3fc3_r11s_test_init(struct kunit *test)
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
static void s6e3fc3_r11s_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case s6e3fc3_r11s_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#if !defined(CONFIG_UML)
	/* NOTE: Target running TC */
	KUNIT_CASE(s6e3fc3_r11s_test_foo),
#endif
	/* NOTE: UML TC */
	KUNIT_CASE(s6e3fc3_r11s_test_test),
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
static struct kunit_suite s6e3fc3_r11s_test_module = {
	.name = "s6e3fc3_r11s_test",
	.init = s6e3fc3_r11s_test_init,
	.exit = s6e3fc3_r11s_test_exit,
	.test_cases = s6e3fc3_r11s_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&s6e3fc3_r11s_test_module);

MODULE_LICENSE("GPL v2");
