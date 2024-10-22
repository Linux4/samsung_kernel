// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <dt-bindings/display/panel-display.h>
#include "panel_drv.h"
#include "panel_modes.h"

extern int panel_mode_snprintf(const struct panel_display_mode *mode, char *buf, size_t size);

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
static void panel_modes_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_modes_test_panel_mode_snprintf_success(struct kunit *test)
{
	struct panel_display_mode pdm = {
		PANEL_MODE(PANEL_DISPLAY_MODE_1080x2400_120HS, 1080, 2400,
				120, REFRESH_MODE_HS, 120, PANEL_REFRESH_MODE_HS, 0, 0, 0, 0,
				true, 2, 2, 40, 1, 20, 30, 40, 50, 60, 70, 3337, false, false)
	};
	char *approval_modeline =
		"Panel Modeline \"1080x2400_120HS\" 1080 2400 120 1 120 1 0 0 0 0 1 2 2 540 40 1 20 30 40 50 60 70 3337 0 0\n";
	char *buf;

	buf = kunit_kzalloc(test, 256, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buf);

	panel_mode_snprintf(&pdm, buf, 256);
	KUNIT_EXPECT_STREQ(test, buf, approval_modeline);
}

static void panel_modes_test_refresh_mode_to_str_success(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, refresh_mode_to_str(REFRESH_MODE_HS), "hs");
	KUNIT_EXPECT_STREQ(test, refresh_mode_to_str(REFRESH_MODE_NS), "ns");
	KUNIT_EXPECT_STREQ(test, refresh_mode_to_str(REFRESH_MODE_PASSIVE_HS), "phs");
}

static void panel_modes_test_str_to_refresh_mode_success(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, str_to_refresh_mode("hs"), REFRESH_MODE_HS);
	KUNIT_EXPECT_EQ(test, str_to_refresh_mode("ns"), REFRESH_MODE_NS);
	KUNIT_EXPECT_EQ(test, str_to_refresh_mode("phs"), REFRESH_MODE_PASSIVE_HS);
}

static void panel_modes_test_panel_mode_vscan_success(struct kunit *test)
{
	struct panel_display_mode pdm;

	pdm.panel_te_sw_skip_count = 0;
	pdm.panel_te_hw_skip_count = 0;
	KUNIT_EXPECT_EQ(test, panel_mode_vscan(&pdm), 1);

	pdm.panel_te_sw_skip_count = 1;
	pdm.panel_te_hw_skip_count = 0;
	KUNIT_EXPECT_EQ(test, panel_mode_vscan(&pdm), 2);

	pdm.panel_te_sw_skip_count = 0;
	pdm.panel_te_hw_skip_count = 1;
	KUNIT_EXPECT_EQ(test, panel_mode_vscan(&pdm), 2);

	pdm.panel_te_sw_skip_count = 3;
	pdm.panel_te_hw_skip_count = 3;
	KUNIT_EXPECT_EQ(test, panel_mode_vscan(&pdm), 16);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_modes_test_init(struct kunit *test)
{
	struct panel_device *panel;

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	test->priv = panel;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_modes_test_exit(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_modes_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_modes_test_test),
	KUNIT_CASE(panel_modes_test_panel_mode_snprintf_success),
	KUNIT_CASE(panel_modes_test_refresh_mode_to_str_success),
	KUNIT_CASE(panel_modes_test_str_to_refresh_mode_success),
	KUNIT_CASE(panel_modes_test_panel_mode_vscan_success),
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
static struct kunit_suite panel_modes_test_module = {
	.name = "panel_modes_test",
	.init = panel_modes_test_init,
	.exit = panel_modes_test_exit,
	.test_cases = panel_modes_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_modes_test_module);

MODULE_LICENSE("GPL v2");
