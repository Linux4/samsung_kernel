// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "panel_enum_item.h"

struct panel_enum_item_test {
	struct panel_device *panel;
};

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
static void panel_enum_item_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_enum_item_test_panel_enum_item_add_success(struct kunit *test)
{
	struct panel_enum *panel_enum = kunit_kzalloc(test, sizeof(*panel_enum), GFP_KERNEL);
	struct panel_enum_item *first;

	INIT_LIST_HEAD(&panel_enum->enum_list);
	KUNIT_EXPECT_EQ(test, panel_enum_item_add(panel_enum, PANEL_STATE_OFF, "PANEL_STATE_OFF"), 0);
	KUNIT_ASSERT_FALSE(test, list_empty(&panel_enum->enum_list));
	first = list_first_entry_or_null(&panel_enum->enum_list, typeof(*first), head);
	KUNIT_EXPECT_EQ(test, first->value, (uint64_t)PANEL_STATE_OFF);
	KUNIT_EXPECT_STREQ(test, (char *)first->name, "PANEL_STATE_OFF");
	destroy_panel_enum(first);
}

static void panel_enum_item_test_create_panel_enum_success(struct kunit *test)
{
	const struct panel_enum_item_list panel_state_enum_list[] = {
		__PANEL_ENUM_LIST_INITIALIZER(PANEL_STATE_OFF),
		__PANEL_ENUM_LIST_INITIALIZER(PANEL_STATE_ON),
		__PANEL_ENUM_LIST_INITIALIZER(PANEL_STATE_NORMAL),
		__PANEL_ENUM_LIST_INITIALIZER(PANEL_STATE_ALPM),
	};
	struct panel_enum *panel_enum = create_panel_enum("panel_state",
			panel_state_enum_list, ARRAY_SIZE(panel_state_enum_list));

	KUNIT_EXPECT_TRUE(test, !!panel_enum);
	destroy_panel_enum(first);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_enum_item_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_enum_item_test *ctx = kunit_kzalloc(test,
		sizeof(struct panel_enum_item_test), GFP_KERNEL);

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	INIT_LIST_HEAD(&panel->enum_list);

	ctx->panel = panel;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_enum_item_test_exit(struct kunit *test)
{
	struct panel_enum_item_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_ASSERT_TRUE(test, list_empty(&panel->enum_list));
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_enum_item_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_enum_item_test_test),

	KUNIT_CASE(panel_enum_item_test_panel_enum_item_add_success),
	KUNIT_CASE(panel_enum_item_test_create_panel_enum_success),
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
static struct kunit_suite panel_enum_item_test_module = {
	.name = "panel_enum_item_test",
	.init = panel_enum_item_test_init,
	.exit = panel_enum_item_test_exit,
	.test_cases = panel_enum_item_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_enum_item_test_module);

MODULE_LICENSE("GPL v2");

