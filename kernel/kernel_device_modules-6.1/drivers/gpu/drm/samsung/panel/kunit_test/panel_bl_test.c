// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "panel_bl.h"

extern int panel_create_lcd_device(struct panel_device *panel, unsigned int id);
extern int panel_destroy_lcd_device(struct panel_device *panel);
extern int panel_bl_set_name(struct panel_bl_device *panel_bl, unsigned int id);
extern const char *panel_bl_get_name(struct panel_bl_device *panel_bl);
extern int panel_bl_init_property(struct panel_bl_device *panel_bl);
extern int panel_bl_init_backlight_device_property(struct panel_bl_device *panel_bl);

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
static void panel_bl_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_bl_test_panel_bl_set_name_fail_with_invalid_args(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_bl_set_name(NULL, 0), -EINVAL);
}

static void panel_bl_test_panel_bl_get_name_return_null_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, !panel_bl_get_name(NULL));
}

static void panel_bl_test_panel_bl_set_and_get_name_success(struct kunit *test)
{
	struct panel_bl_device *panel_bl = test->priv;

	KUNIT_EXPECT_EQ(test, panel_bl_set_name(panel_bl, 0), 0);
	KUNIT_EXPECT_STREQ(test, panel_bl_get_name(panel_bl), "panel");

	KUNIT_EXPECT_EQ(test, panel_bl_set_name(panel_bl, 1), 0);
	KUNIT_EXPECT_STREQ(test, panel_bl_get_name(panel_bl), "panel-1");
}

static void panel_bl_test_panel_bl_init_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_bl_init(NULL), -EINVAL);
}

static void panel_bl_test_panel_bl_exit_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_bl_exit(NULL), -EINVAL);
}

static void panel_bl_test_panel_bl_init_and_exit_success(struct kunit *test)
{
	struct panel_bl_device *panel_bl = test->priv;

	KUNIT_ASSERT_TRUE(test, !panel_bl->bd);

	KUNIT_EXPECT_EQ(test, panel_bl_init(panel_bl), 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_bl->bd);

	KUNIT_EXPECT_EQ(test, panel_bl_exit(panel_bl), 0);
	KUNIT_EXPECT_TRUE(test, !panel_bl->bd);
}

static void panel_bl_test_panel_bl_init_property_fail_with_invalid_args(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_bl_init_property(NULL), -EINVAL);
}

static void panel_bl_test_panel_bl_init_property_success(struct kunit *test)
{
	struct panel_bl_device *panel_bl = test->priv;

	KUNIT_EXPECT_EQ(test, panel_bl_init_property(panel_bl), 0);
	KUNIT_EXPECT_EQ(test, panel_bl->props.id, PANEL_BL_SUBDEV_TYPE_DISP);
	KUNIT_EXPECT_EQ(test, panel_bl->props.brightness, UI_DEF_BRIGHTNESS);
	KUNIT_EXPECT_EQ(test, panel_bl->props.prev_brightness, UI_DEF_BRIGHTNESS);
	KUNIT_EXPECT_EQ(test, panel_bl->props.acl_pwrsave, ACL_PWRSAVE_OFF);
	KUNIT_EXPECT_EQ(test, panel_bl->props.acl_opr, 1);
	KUNIT_EXPECT_EQ(test, panel_bl->props.smooth_transition, SMOOTH_TRANS_ON);
}

static void panel_bl_test_panel_bl_init_backlight_device_property_fail_with_invalid_args(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_bl_init_backlight_device_property(NULL), -EINVAL);
}

static void panel_bl_test_panel_bl_init_backlight_device_property_fail_if_backlight_device_is_null(struct kunit *test)
{
	struct panel_bl_device *panel_bl = test->priv;

	KUNIT_EXPECT_EQ(test, panel_bl_init_backlight_device_property(panel_bl), -EINVAL);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_bl_test_init(struct kunit *test)
{
	struct panel_device *panel;

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel_mutex_init(panel, &panel->io_lock);

	INIT_LIST_HEAD(&panel->prop_list);

	panel_create_lcd_device(panel, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->lcd_dev);

	test->priv = &panel->panel_bl;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_bl_test_exit(struct kunit *test)
{
	struct panel_bl_device *panel_bl = test->priv;
	struct panel_device *panel = to_panel_device(panel_bl);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel_bl);

	panel_destroy_lcd_device(panel);
	KUNIT_ASSERT_TRUE(test, !panel->lcd_dev);

	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_bl_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_bl_test_test),

	KUNIT_CASE(panel_bl_test_panel_bl_set_name_fail_with_invalid_args),
	KUNIT_CASE(panel_bl_test_panel_bl_get_name_return_null_with_invalid_arg),
	KUNIT_CASE(panel_bl_test_panel_bl_set_and_get_name_success),

	KUNIT_CASE(panel_bl_test_panel_bl_init_fail_with_invalid_arg),
	KUNIT_CASE(panel_bl_test_panel_bl_exit_fail_with_invalid_arg),
	KUNIT_CASE(panel_bl_test_panel_bl_init_and_exit_success),

	KUNIT_CASE(panel_bl_test_panel_bl_init_property_fail_with_invalid_args),
	KUNIT_CASE(panel_bl_test_panel_bl_init_property_success),

	KUNIT_CASE(panel_bl_test_panel_bl_init_backlight_device_property_fail_with_invalid_args),
	KUNIT_CASE(panel_bl_test_panel_bl_init_backlight_device_property_fail_if_backlight_device_is_null),
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
static struct kunit_suite panel_bl_test_module = {
	.name = "panel_bl_test",
	.init = panel_bl_test_init,
	.exit = panel_bl_test_exit,
	.test_cases = panel_bl_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_bl_test_module);

MODULE_LICENSE("GPL v2");
