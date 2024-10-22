// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/list_sort.h>

#include "panel_drv.h"
#include "panel_property.h"
#include "panel_condition.h"

static bool condition_true(struct panel_device *panel) { return true; }
static bool condition_false(struct panel_device *panel) { return false; }

static DEFINE_PNOBJ_FUNC(condition_true, condition_true);
static DEFINE_PNOBJ_FUNC(condition_false, condition_false);

struct panel_condition_test {
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
static void panel_condition_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_condition_test_create_and_destroy_condition(struct kunit *test)
{
	struct condinfo *condition_1;
	struct condinfo *condition_2;
	struct panel_expr_data item1 = __COND_RULE_ITEM_FUNC_INITIALIZER(&PNOBJ_FUNC(condition_true));
	struct panel_expr_data item2[] = { __COND_RULE_ITEM_RULE_INITIALIZER("0123456789012345678901", EQ, 120, COND_RULE_MASK_ALL) };
	struct cond_rule rule1 = { .item = &item1, .num_item = 1 };
	struct cond_rule rule2 = { .item = item2, .num_item = ARRAY_SIZE(item2) };

	condition_1 = create_condition("condition_1", CMD_TYPE_COND_IF, &rule1);
	KUNIT_ASSERT_TRUE(test, condition_1 != NULL);
	destroy_condition(condition_1);

	condition_2 = create_condition("condition_2", CMD_TYPE_COND_IF, &rule2);
	KUNIT_ASSERT_TRUE(test, condition_2 != NULL);
	KUNIT_EXPECT_STREQ(test, condition_2->rule.item[1].op.str, item2[1].op.str);
	destroy_condition(condition_2);
}

static void panel_condition_test_duplicate_condition(struct kunit *test)
{
	struct panel_condition_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct condinfo *cond_is_true;
	struct condinfo *cond_is_true_dup;
	struct panel_expr_data item[] = {
		__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 120, COND_RULE_MASK_ALL),
		PANEL_EXPR_OPERATOR(AND),
		__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE, EQ, VRR_HS_MODE, COND_RULE_MASK_ALL),
		PANEL_EXPR_OPERATOR(AND),
		__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_3, LT, 0x04, 0x0F),
		PANEL_EXPR_OPERATOR(OR),
		__COND_RULE_ITEM_FUNC_INITIALIZER(&PNOBJ_FUNC(condition_true)),
	};
	struct cond_rule rule = { .item = item, .num_item = ARRAY_SIZE(item), };

	cond_is_true = create_condition("cond_is_true",
			CMD_TYPE_COND_IF, &rule);
	KUNIT_ASSERT_TRUE(test, cond_is_true != NULL);

	cond_is_true_dup = duplicate_condition(cond_is_true);
	KUNIT_ASSERT_TRUE(test, cond_is_true_dup != NULL);

	KUNIT_EXPECT_STREQ(test, get_condition_name(cond_is_true),
			get_condition_name(cond_is_true_dup));
	KUNIT_EXPECT_EQ(test, get_condition_type(cond_is_true),
			get_condition_type(cond_is_true_dup));

	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, cond_is_true) == true);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, cond_is_true_dup) == true);

	destroy_condition(cond_is_true);
	destroy_condition(cond_is_true_dup);
}

static void panel_condition_test_panel_do_condition_success(struct kunit *test)
{
	struct panel_condition_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_expr_data item_and[] = {
		__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 120, COND_RULE_MASK_ALL),
		PANEL_EXPR_OPERATOR(AND),
		__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE, EQ, VRR_HS_MODE, COND_RULE_MASK_ALL),
	};
	struct panel_expr_data item_or[] = {
		__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE, EQ, 120, COND_RULE_MASK_ALL),
		PANEL_EXPR_OPERATOR(OR),
		__COND_RULE_ITEM_RULE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE, EQ, VRR_HS_MODE, COND_RULE_MASK_ALL),
	};
	DEFINE_CONDITION(condition_and, item_and);
	DEFINE_CONDITION(condition_or, item_or);
	DEFINE_FUNC_BASED_COND(cond_is_true, &PNOBJ_FUNC(condition_true));
	DEFINE_FUNC_BASED_COND(cond_is_false, &PNOBJ_FUNC(condition_false));

	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, &CONDINFO_IF(cond_is_true)) == true);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, &CONDINFO_IF(cond_is_false)) == false);

	/* and rules */
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, &CONDINFO_IF(condition_and)) == true);

	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_NORMAL_MODE);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, &CONDINFO_IF(condition_and)) == false);

	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, &CONDINFO_IF(condition_and)) == false);

	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_NORMAL_MODE);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, &CONDINFO_IF(condition_and)) == false);

	/* or rules */
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, &CONDINFO_IF(condition_or)) == true);

	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 120);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_NORMAL_MODE);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, &CONDINFO_IF(condition_or)) == true);

	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, &CONDINFO_IF(condition_or)) == true);

	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE, 60);
	panel_set_property_value(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_NORMAL_MODE);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, &CONDINFO_IF(condition_or)) == false);
}

static void panel_condition_test_panel_do_condition_failed_with_invalid_arg(struct kunit *test)
{
	struct panel_condition_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	DEFINE_FUNC_BASED_COND(cond_is_true, &PNOBJ_FUNC(condition_true));

	KUNIT_EXPECT_TRUE(test, panel_do_condition(NULL, &CONDINFO_IF(cond_is_true)) == false);
	KUNIT_EXPECT_TRUE(test, panel_do_condition(panel, NULL) == false);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_condition_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_condition_test *ctx = kunit_kzalloc(test,
			sizeof(struct panel_condition_test), GFP_KERNEL);
	struct panel_prop_enum_item panel_state_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ON),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_NORMAL),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ALPM),
	};
	struct panel_prop_enum_item wait_tx_done_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_AUTO),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_ON),
	};
	struct panel_prop_enum_item separate_tx_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_ON),
	};
	struct panel_prop_enum_item vrr_mode_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_NORMAL_MODE),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_HS_MODE),
	};
	struct panel_prop_list prop_array[] = {
		/* enum property */
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PANEL_STATE,
				PANEL_STATE_OFF, panel_state_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_WAIT_TX_DONE,
				WAIT_TX_DONE_AUTO, wait_tx_done_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_SEPARATE_TX,
				SEPARATE_TX_OFF, separate_tx_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE,
				VRR_NORMAL_MODE, vrr_mode_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE,
				VRR_NORMAL_MODE, vrr_mode_enum_items),
		/* range property */
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_1,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_2,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_3,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE,
				60, 0, 120),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE,
				60, 0, 120),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_RESOLUTION_CHANGED,
				false, false, true),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_DSI_FREQ,
				0, 0, 1000000000),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_OSC_FREQ,
				0, 0, 1000000000),
#ifdef CONFIG_USDM_FACTORY
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_IS_FACTORY_MODE,
				1, 0, 1),
#else
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_IS_FACTORY_MODE,
				0, 0, 1),
#endif
		/* DISPLAY TEST */
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE,
				0, 0, 1),
#endif
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_BL_PROPERTY_BRIGHTNESS,
				128, 0, 1000000000),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_BL_PROPERTY_PREV_BRIGHTNESS,
				128, 0, 1000000000),
	};

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	INIT_LIST_HEAD(&panel->prop_list);

	KUNIT_ASSERT_TRUE(test, !panel_add_property_from_array(panel,
			prop_array, ARRAY_SIZE(prop_array)));

	ctx->panel = panel;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_condition_test_exit(struct kunit *test)
{
	struct panel_condition_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_delete_property_all(panel);
	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_condition_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_condition_test_test),
	KUNIT_CASE(panel_condition_test_create_and_destroy_condition),
	KUNIT_CASE(panel_condition_test_duplicate_condition),
	KUNIT_CASE(panel_condition_test_panel_do_condition_success),
	KUNIT_CASE(panel_condition_test_panel_do_condition_failed_with_invalid_arg),
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
static struct kunit_suite panel_condition_test_module = {
	.name = "panel_condition_test",
	.init = panel_condition_test_init,
	.exit = panel_condition_test_exit,
	.test_cases = panel_condition_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_condition_test_module);

MODULE_LICENSE("GPL v2");
