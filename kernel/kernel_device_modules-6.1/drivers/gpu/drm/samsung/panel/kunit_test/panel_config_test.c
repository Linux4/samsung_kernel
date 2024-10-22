// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "panel_property.h"
#include "panel_config.h"

struct panel_config_test {
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
static void panel_config_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_config_test_panel_apply_pnobj_config_success(struct kunit *test)
{
	struct panel_config_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	DEFINE_PNOBJ_CONFIG(separate_tx_off, PANEL_PROPERTY_SEPARATE_TX, SEPARATE_TX_OFF);
	DEFINE_PNOBJ_CONFIG(separate_tx_on, PANEL_PROPERTY_SEPARATE_TX, SEPARATE_TX_ON);
	DEFINE_PNOBJ_CONFIG(wait_tx_done_manual_on, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_ON);
	DEFINE_PNOBJ_CONFIG(wait_tx_done_manual_off, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
	DEFINE_PNOBJ_CONFIG(wait_tx_done_auto, PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_AUTO);

	KUNIT_EXPECT_EQ(test, panel_apply_pnobj_config(panel,
				&PNOBJ_CONFIG(separate_tx_on)), 0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel,
				PANEL_PROPERTY_SEPARATE_TX), SEPARATE_TX_ON);

	KUNIT_EXPECT_EQ(test, panel_apply_pnobj_config(panel,
				&PNOBJ_CONFIG(separate_tx_off)), 0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel,
				PANEL_PROPERTY_SEPARATE_TX), SEPARATE_TX_OFF);

	KUNIT_EXPECT_EQ(test, panel_apply_pnobj_config(panel,
				&PNOBJ_CONFIG(wait_tx_done_auto)), 0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel,
				PANEL_PROPERTY_WAIT_TX_DONE), WAIT_TX_DONE_AUTO);

	KUNIT_EXPECT_EQ(test, panel_apply_pnobj_config(panel,
				&PNOBJ_CONFIG(wait_tx_done_manual_on)), 0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel,
				PANEL_PROPERTY_WAIT_TX_DONE), WAIT_TX_DONE_MANUAL_ON);

	KUNIT_EXPECT_EQ(test, panel_apply_pnobj_config(panel,
				&PNOBJ_CONFIG(wait_tx_done_manual_off)), 0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel,
				PANEL_PROPERTY_WAIT_TX_DONE), WAIT_TX_DONE_MANUAL_OFF);
}

static void panel_config_test_create_pnobj_config_test(struct kunit *test)
{
	struct pnobj_config *cfg;

	KUNIT_ASSERT_TRUE(test, !create_pnobj_config(NULL,
				PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF));
	KUNIT_ASSERT_TRUE(test, !create_pnobj_config("wait_tx_done_manual_off",
				NULL, WAIT_TX_DONE_MANUAL_OFF));

	cfg = create_pnobj_config("wait_tx_done_manual_on",
			PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_ON);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cfg);
	KUNIT_EXPECT_STREQ(test, cfg->prop_name, PANEL_PROPERTY_WAIT_TX_DONE);
	KUNIT_EXPECT_EQ(test, cfg->value, WAIT_TX_DONE_MANUAL_ON);
	destroy_pnobj_config(cfg);

	cfg = create_pnobj_config("wait_tx_done_manual_off",
			PANEL_PROPERTY_WAIT_TX_DONE, WAIT_TX_DONE_MANUAL_OFF);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cfg);
	KUNIT_EXPECT_STREQ(test, cfg->prop_name, PANEL_PROPERTY_WAIT_TX_DONE);
	KUNIT_EXPECT_EQ(test, cfg->value, WAIT_TX_DONE_MANUAL_OFF);
	destroy_pnobj_config(cfg);

	cfg = create_pnobj_config("wait_tx_done_manual_off",
			"01234567890123456789012345678901", WAIT_TX_DONE_MANUAL_OFF);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cfg);
	KUNIT_EXPECT_STREQ(test, cfg->prop_name, "0123456789012345678901234567890");
	KUNIT_EXPECT_EQ(test, cfg->value, WAIT_TX_DONE_MANUAL_OFF);
	destroy_pnobj_config(cfg);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_config_test_init(struct kunit *test)
{
	struct panel_config_test *ctx =
		kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	struct panel_device *panel
		= kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	struct panel_prop_enum_item wait_tx_done_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_AUTO),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_ON),
	};
	struct panel_prop_enum_item separate_tx_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_ON),
	};
	struct panel_prop_list prop_array[] = {
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_WAIT_TX_DONE,
				WAIT_TX_DONE_AUTO, wait_tx_done_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_SEPARATE_TX,
				SEPARATE_TX_OFF, separate_tx_enum_items),
	};

	INIT_LIST_HEAD(&panel->prop_list);

	KUNIT_ASSERT_TRUE(test,
			!panel_add_property_from_array(panel,
			prop_array, ARRAY_SIZE(prop_array)));

	ctx->panel = panel;
	test->priv = ctx;


	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_config_test_exit(struct kunit *test)
{
	struct panel_config_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_delete_property_all(panel);
	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_config_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_config_test_test),
	KUNIT_CASE(panel_config_test_panel_apply_pnobj_config_success),
	KUNIT_CASE(panel_config_test_create_pnobj_config_test),
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
static struct kunit_suite panel_config_test_module = {
	.name = "panel_config_test",
	.init = panel_config_test_init,
	.exit = panel_config_test_exit,
	.test_cases = panel_config_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_config_test_module);

MODULE_LICENSE("GPL v2");
