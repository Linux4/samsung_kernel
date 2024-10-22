/* SPDX-License-Identifier: GPL-2.0
 */
#include <kunit/test.h>
#include <kunit/mock.h>
#include "test_helper.h"
#include "panel_drv.h"
#include "panel_testmode.h"
#include "mock_panel_drv_funcs.h"

struct panel_testmode_test {
	struct panel_device *panel;
};
extern struct panel_drv_funcs panel_drv_funcs;
extern struct panel_drv_funcs mock_panel_drv_funcs;
extern struct class *lcd_class;
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
static void panel_testmode_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_testmode_test_panel_testmode_on_off_success(struct kunit *test)
{
	struct panel_testmode_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_drv_funcs *original_funcs = panel->funcs;

	KUNIT_EXPECT_EQ(test, panel->testmode.enabled, false);

	panel_testmode_on(panel);
	KUNIT_EXPECT_EQ(test, panel->testmode.enabled, true);
	KUNIT_EXPECT_PTR_NE(test, panel->funcs, original_funcs);

	panel_testmode_off(panel);
	KUNIT_EXPECT_EQ(test, panel->testmode.enabled, false);
	KUNIT_EXPECT_PTR_EQ(test, panel->funcs, original_funcs);
}

static void panel_testmode_test_panel_testmode_command_fail_with_invalid_args(struct kunit *test)
{
	struct panel_testmode_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel->funcs = &mock_panel_drv_funcs;

	panel_testmode_on(panel);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(NULL, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(NULL, "ON"), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "NONE"), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, ""), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,"), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS"), -EINVAL);
}

static void panel_testmode_test_panel_testmode_command_fail_not_ready_state(struct kunit *test)
{
	struct panel_testmode_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel->funcs = &mock_panel_drv_funcs;

	panel_testmode_off(panel);

	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "SLEEP_OUT"), -EBUSY);
}

static void panel_testmode_test_panel_testmode_command_success(struct kunit *test)
{
	struct panel_testmode_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct mock_expectation *expectation;

	panel->funcs = &mock_panel_drv_funcs;

	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "ON"), 0);
	KUNIT_EXPECT_EQ(test, panel->testmode.enabled, true);

	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "OFF"), 0);
	KUNIT_EXPECT_EQ(test, panel->testmode.enabled, false);

	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "ON"), 0);
	KUNIT_EXPECT_EQ(test, panel->testmode.enabled, true);

	expectation = Times(1, KUNIT_EXPECT_CALL(mock_display_on(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "DISPLAY_ON"), 0);

	expectation = Times(1, KUNIT_EXPECT_CALL(mock_display_off(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "DISPLAY_OFF"), 0);

	expectation = Times(1, KUNIT_EXPECT_CALL(mock_sleep_out(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "SLEEP_OUT"), 0);

	expectation = Times(1, KUNIT_EXPECT_CALL(mock_sleep_in(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "SLEEP_IN"), 0);

	expectation = Times(2, KUNIT_EXPECT_CALL(mock_doze(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "DOZE"), 0);
	expectation->action = int_return(test, 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "DOZE_SUSPEND"), 0);
}

/*
DEFINE_FUNCTION_MOCK(panel_bl_set_brightness, RETURNS(int), PARAMS(struct panel_bl_device *, int, u32));
static void panel_testmode_test_panel_testmode_brightness_command_success(struct kunit *test)
{
	struct panel_testmode_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct mock_expectation *expectation;

	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "ON"), 0);
	KUNIT_EXPECT_EQ(test, panel->testmode.enabled, true);

	expectation = Times(10, KUNIT_EXPECT_CALL(panel_bl_set_brightness(ptr_eq(test, panel_bl), any(test), any(test))));
	expectation->action = int_return(test, 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,0"), 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,10"), 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,30"), 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,128"), 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,183"), 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,255"), 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,256"), 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,306"), 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,461"), 0);
	KUNIT_EXPECT_EQ(test, panel_testmode_command(panel, "BRIGHTNESS,510"), 0);
}
*/

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_testmode_test_init(struct kunit *test)
{
	struct panel_testmode_test *ctx;
	struct panel_device *panel;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel->dev = device_create(lcd_class,
			NULL, 0, panel, PANEL_DRV_NAME);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->dev);

	panel->funcs = &panel_drv_funcs;
	ctx->panel = panel;
	test->priv = ctx;

	//MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(panel_bl_set_brightness);

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_testmode_test_exit(struct kunit *test)
{
	struct panel_testmode_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	device_unregister(panel->dev);
	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_testmode_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_testmode_test_test),
	KUNIT_CASE(panel_testmode_test_panel_testmode_on_off_success),
	KUNIT_CASE(panel_testmode_test_panel_testmode_command_fail_with_invalid_args),
	KUNIT_CASE(panel_testmode_test_panel_testmode_command_fail_not_ready_state),
	KUNIT_CASE(panel_testmode_test_panel_testmode_command_success),
	//KUNIT_CASE(panel_testmode_test_panel_testmode_brightness_command_success),
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
static struct kunit_suite panel_testmode_test_module = {
	.name = "panel_testmode_test",
	.init = panel_testmode_test_init,
	.exit = panel_testmode_test_exit,
	.test_cases = panel_testmode_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_testmode_test_module);

MODULE_LICENSE("GPL v2");
