// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "../include/sec_battery.h"

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

#if !defined(CONFIG_UML)
/* NOTE: Target running TC must be in the #ifndef CONFIG_UML */
static void sec_battery_dt_test_foo(struct test *test)
{

}
#endif

/* NOTE: UML TC */
static void sec_battery_dt_test_bar(struct test *test)
{
	/* Test cases for UML */
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int sec_battery_dt_test_init(struct test *test)
{
	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void sec_battery_dt_test_exit(struct test *test)
{
}

static void sec_battery_dt_test_common(struct test *test)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	struct sec_battery_info *battery = power_supply_get_drvdata(psy);

	test_info(test, "START %s test\n", __func__);

	EXPECT_EQ(test, sec_bat_parse_dt(battery->dev, battery), 0);

	__pm_stay_awake(battery->parse_mode_dt_wake_lock);
	queue_delayed_work(battery->monitor_wqueue, &battery->parse_mode_dt_work, 0);

	test_info(test, "%s test done\n", __func__);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct test_case sec_battery_dt_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#if !defined(CONFIG_UML)
	/* NOTE: Target running TC */
	TEST_CASE(sec_battery_dt_test_foo),
#endif
	TEST_CASE(sec_battery_dt_test_common),
	/* NOTE: UML TC */
	TEST_CASE(sec_battery_dt_test_bar),
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
static struct test_module sec_battery_dt_test_module = {
	.name = "sec_battery_dt_test",
	.init = sec_battery_dt_test_init,
	.exit = sec_battery_dt_test_exit,
	.test_cases = sec_battery_dt_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
module_test(sec_battery_dt_test_module);
