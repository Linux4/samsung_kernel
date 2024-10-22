// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "panel_property.h"

extern struct panel_property *panel_property_create(u32 type, const char *name);

struct panel_property_test {
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
static void panel_property_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_property_test_prop_type_to_string(struct kunit *test)
{
	KUNIT_EXPECT_STREQ(test, prop_type_to_string(PANEL_PROP_TYPE_RANGE), "RANGE");
	KUNIT_EXPECT_STREQ(test, prop_type_to_string(PANEL_PROP_TYPE_ENUM), "ENUM");
	KUNIT_EXPECT_TRUE(test, prop_type_to_string(MAX_PANEL_PROP_TYPE) == NULL);
}

static void panel_property_test_string_to_prop_type(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, string_to_prop_type("RANGE"), PANEL_PROP_TYPE_RANGE);
	KUNIT_EXPECT_EQ(test, string_to_prop_type("ENUM"), PANEL_PROP_TYPE_ENUM);
	KUNIT_EXPECT_EQ(test, string_to_prop_type("XXXX"), -EINVAL);
}

static void panel_property_test_snprintf_property(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_property *property;
	struct panel_prop_enum_item vrr_mode_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_NORMAL_MODE),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_HS_MODE),
	};
	char buf[SZ_256];

	KUNIT_ASSERT_EQ(test, panel_add_range_property(panel,
				PANEL_PROPERTY_PANEL_REFRESH_RATE, 120, 0, 120, NULL), 0);
	property = panel_find_property(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE);
	KUNIT_ASSERT_TRUE(test, property != NULL);
	snprintf_property(buf, SZ_256, property);
	KUNIT_EXPECT_STREQ(test, buf, "panel_refresh_rate\ntype: RANGE\nvalue: 120 (min: 0, max: 120)");
	panel_delete_property(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE);

	KUNIT_ASSERT_EQ(test, panel_add_enum_property(panel,
				PANEL_PROPERTY_PANEL_REFRESH_MODE, VRR_HS_MODE, vrr_mode_enum_items, ARRAY_SIZE(vrr_mode_enum_items), NULL), 0);
	property = panel_find_property(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE);
	KUNIT_ASSERT_TRUE(test, property != NULL);
	snprintf_property(buf, SZ_256, property);
	KUNIT_EXPECT_STREQ(test, buf, "panel_refresh_mode\ntype: ENUM\nvalue: 1 (VRR_HS_MODE)\nenum:\n [0]: VRR_NORMAL_MODE\n*[1]: VRR_HS_MODE");
	panel_delete_property(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE);
}

static void panel_property_test_panel_property_add_range_success(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_TRUE(test, panel_add_range_property(panel, "test_name", 1, 0, 1, NULL) == 0);
	KUNIT_EXPECT_TRUE(test, panel_add_range_property(panel, "*******************************", 1, 0, 1, NULL) == 0); //len:31

	panel_delete_property(panel, "test_name");
	panel_delete_property(panel, "*******************************");
}

static void panel_property_test_panel_property_add_range_should_return_err_with_wrong_input(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_EQ(test, panel_add_range_property(NULL, NULL, 1, 0, 1, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_add_range_property(panel, NULL, 1, 0, 1, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_add_range_property(panel, "********************************", 1, 0, 1, NULL), -EINVAL); //len:32
}

static void panel_property_test_panel_property_add_range_should_return_err_with_duplicated_name(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_TRUE(test, panel_add_range_property(panel, "duplicated_name", 1, 0, 1, NULL) == 0);
	KUNIT_EXPECT_EQ(test, panel_add_range_property(panel, "duplicated_name", 1, 0, 1, NULL), -EINVAL);

	panel_delete_property(panel, "duplicated_name");
}

static void panel_property_test_panel_property_find_success(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_TRUE(test, !panel_find_property(panel, "test_name"));
	panel_add_range_property(panel, "test_name", 1, 0, 1, NULL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_find_property(panel, "test_name"));

	KUNIT_EXPECT_TRUE(test, !panel_find_property(panel, "test_name_2"));
	panel_add_range_property(panel, "test_name_2", 1, 0, 1, NULL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_find_property(panel, "test_name_2"));

	panel_add_range_property(panel, "test_name_aa_bb", 1, 0, 1, NULL);
	KUNIT_EXPECT_TRUE(test, panel_find_property(panel, "test_name_aa_bb") != NULL);

	panel_delete_property(panel, "test_name");
	panel_delete_property(panel, "test_name_2");
	panel_delete_property(panel, "test_name_aa_bb");
}

static void panel_property_test_panel_property_find_should_return_err_with_wrong_input(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_TRUE(test, !panel_find_property(NULL, "test_name_2"));
	KUNIT_EXPECT_TRUE(test, !panel_find_property(panel, NULL));
}

static void panel_property_test_panel_property_delete_success(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_add_range_property(panel, "test_name_1", 1, 0, 1, NULL);
	panel_add_range_property(panel, "test_name_2", 1, 0, 1, NULL);
	panel_add_range_property(panel, "test_name_3", 1, 0, 1, NULL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_find_property(panel, "test_name_1"));
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_find_property(panel, "test_name_2"));
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_find_property(panel, "test_name_3"));

	/* del 2 */
	KUNIT_EXPECT_TRUE(test, panel_delete_property(panel, "test_name_2") == 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_find_property(panel, "test_name_1"));
	KUNIT_EXPECT_TRUE(test, !panel_find_property(panel, "test_name_2"));
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_find_property(panel, "test_name_3"));

	/* del 1 */
	KUNIT_EXPECT_TRUE(test, panel_delete_property(panel, "test_name_1") == 0);
	KUNIT_EXPECT_TRUE(test, !panel_find_property(panel, "test_name_1"));
	KUNIT_EXPECT_TRUE(test, !panel_find_property(panel, "test_name_2"));
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_find_property(panel, "test_name_3"));

	/* del 3 */
	KUNIT_EXPECT_TRUE(test, panel_delete_property(panel, "test_name_3") == 0);
	KUNIT_EXPECT_TRUE(test, !panel_find_property(panel, "test_name_1"));
	KUNIT_EXPECT_TRUE(test, !panel_find_property(panel, "test_name_2"));
	KUNIT_EXPECT_TRUE(test, !panel_find_property(panel, "test_name_3"));
}

static void panel_property_test_panel_property_delete_should_return_err_with_wrong_input(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_add_range_property(panel, "test_name_2", 1, 0, 1, NULL);
	KUNIT_EXPECT_EQ(test, panel_delete_property(NULL, "test_name_2"), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_delete_property(panel, NULL), -EINVAL);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_find_property(panel, "test_name_2"));

	panel_delete_property(panel, "test_name_2");
}

static void panel_property_test_panel_property_set_range_value_success(struct kunit *test)
{
	struct panel_property *test_range_prop =
		panel_property_create_range("test_range_prop", 1, 0, 1);

	KUNIT_EXPECT_EQ(test, panel_property_set_range_value(test_range_prop, 0), 0);
	KUNIT_EXPECT_EQ(test, test_range_prop->value, 0);
	KUNIT_EXPECT_EQ(test, panel_property_set_range_value(test_range_prop, 1), 0);
	KUNIT_EXPECT_EQ(test, test_range_prop->value, 1);
	KUNIT_EXPECT_EQ(test, panel_property_set_range_value(test_range_prop, 2), -EINVAL);
	KUNIT_EXPECT_EQ(test, test_range_prop->value, 1);

	panel_property_destroy(test_range_prop);
}

static void panel_property_test_panel_property_set_enum_value_success(struct kunit *test)
{
	struct panel_prop_enum_item panel_state_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ON),
		/* __PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_NORMAL), */
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ALPM),
	};
	struct panel_property *test_enum_prop =
		panel_property_create_enum("test_enum_prop",
				PANEL_STATE_ON, panel_state_enum_items,
				ARRAY_SIZE(panel_state_enum_items));

	/* check initial value */
	KUNIT_EXPECT_EQ(test, test_enum_prop->value, (u32)PANEL_STATE_ON);
	KUNIT_EXPECT_EQ(test, test_enum_prop->min, (u32)PANEL_STATE_OFF);
	KUNIT_EXPECT_EQ(test, test_enum_prop->max, (u32)PANEL_STATE_ALPM);

	/* set enum value */
	KUNIT_EXPECT_EQ(test, panel_property_set_enum_value(test_enum_prop, PANEL_STATE_OFF), 0);
	KUNIT_EXPECT_EQ(test, test_enum_prop->value, (u32)PANEL_STATE_OFF);
	KUNIT_EXPECT_EQ(test, panel_property_set_enum_value(test_enum_prop, PANEL_STATE_ON), 0);
	KUNIT_EXPECT_EQ(test, test_enum_prop->value, (u32)PANEL_STATE_ON);
	KUNIT_EXPECT_EQ(test, panel_property_set_enum_value(test_enum_prop, PANEL_STATE_NORMAL), -EINVAL);
	KUNIT_EXPECT_EQ(test, test_enum_prop->value, (u32)PANEL_STATE_ON);
	KUNIT_EXPECT_EQ(test, panel_property_set_enum_value(test_enum_prop, PANEL_STATE_ALPM), 0);
	KUNIT_EXPECT_EQ(test, test_enum_prop->value, (u32)PANEL_STATE_ALPM);

	panel_property_destroy(test_enum_prop);
}

static void panel_property_test_panel_property_set_value_success(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_prop_enum_item panel_state_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ON),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_NORMAL),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ALPM),
	};

	panel_add_range_property(panel, "test_range_prop", 1, 0, 22, NULL);
	panel_add_enum_property(panel, "test_enum_prop",
			PANEL_STATE_NORMAL, panel_state_enum_items, ARRAY_SIZE(panel_state_enum_items), NULL);

	/* initial value */
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_range_prop"), 1);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_enum_prop"), PANEL_STATE_NORMAL);

	/* set value */
	KUNIT_EXPECT_EQ(test, panel_set_property_value(panel, "test_range_prop", 22), 0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_range_prop"), 22);

	KUNIT_EXPECT_EQ(test, panel_set_property_value(panel, "test_enum_prop", PANEL_STATE_ALPM), 0);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_enum_prop"), PANEL_STATE_ALPM);

	panel_delete_property(panel, "test_range_prop");
	panel_delete_property(panel, "test_enum_prop");
}

static void panel_property_test_panel_property_set_value_should_return_err_with_wrong_input(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_add_range_property(panel, "test_name_1", 1, 0, 22, NULL);

	KUNIT_EXPECT_EQ(test, panel_set_property_value(panel, "test_name_2", 22), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_set_property_value(NULL, "test_name_1", 22), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_set_property_value(panel, NULL, 22), -EINVAL);

	panel_delete_property(panel, "test_name_1");
}

static void panel_property_test_panel_property_get_value_success(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_add_range_property(panel, "test_name_1", 1, 0, 100, NULL);
	panel_add_range_property(panel, "test_name_2", 2, 0, 100, NULL);
	panel_add_range_property(panel, "test_name_3", 3, 0, 100, NULL);

	panel_set_property_value(panel, "test_name_1", 11);
	panel_set_property_value(panel, "test_name_2", 22);
	panel_set_property_value(panel, "test_name_3", 33);

	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_name_1"), 11);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_name_2"), 22);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_name_3"), 33);

	panel_set_property_value(panel, "test_name_1", 44);
	panel_set_property_value(panel, "test_name_2", 55);
	panel_set_property_value(panel, "test_name_3", 66);

	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_name_1"), 44);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_name_2"), 55);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_name_3"), 66);

	panel_delete_property(panel, "test_name_1");
	panel_delete_property(panel, "test_name_2");
	panel_delete_property(panel, "test_name_3");
}

static void panel_property_test_panel_property_get_value_should_return_err_with_wrong_input(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_add_range_property(panel, "test_name_1", 1, 0, 1, NULL);

	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_name_2"), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(NULL, "test_name_1"), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, NULL), -EINVAL);

	panel_delete_property(panel, "test_name_1");
}

static void panel_property_test_panel_property_add_enum_value_success(struct kunit *test)
{
	struct panel_prop_enum_item panel_state_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ON),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_NORMAL),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ALPM),
	};
	struct panel_property *property =
		panel_property_create(PANEL_PROP_TYPE_ENUM, "test_property");

	KUNIT_EXPECT_EQ(test, panel_property_add_enum_value(property,
			panel_state_enum_items[0].type, panel_state_enum_items[0].name), 0);
	KUNIT_EXPECT_FALSE(test, list_empty(&property->enum_list));
	panel_property_destroy(property);
}

static void panel_property_test_panel_property_add_enum_success(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_property *property;
	struct panel_prop_enum_item panel_state_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ON),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_NORMAL),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ALPM),
	};

	KUNIT_EXPECT_EQ(test, panel_add_enum_property(panel,
				PANEL_PROPERTY_PANEL_STATE, PANEL_STATE_OFF,
				panel_state_enum_items, ARRAY_SIZE(panel_state_enum_items), NULL), 0);

	property = panel_find_property(panel, PANEL_PROPERTY_PANEL_STATE);
	KUNIT_EXPECT_TRUE(test, property != NULL);
	KUNIT_EXPECT_FALSE(test, list_empty(&property->enum_list));

	KUNIT_EXPECT_EQ(test, panel_property_get_enum_value(property,
				"PANEL_STATE_OFF"), PANEL_STATE_OFF);
	KUNIT_EXPECT_EQ(test, panel_property_get_enum_value(property,
				"PANEL_STATE_ON"), PANEL_STATE_ON);
	KUNIT_EXPECT_EQ(test, panel_property_get_enum_value(property,
				"PANEL_STATE_NORMAL"), PANEL_STATE_NORMAL);
	KUNIT_EXPECT_EQ(test, panel_property_get_enum_value(property,
				"PANEL_STATE_ALPM"), PANEL_STATE_ALPM);
	panel_delete_property(panel, PANEL_PROPERTY_PANEL_STATE);
}

static void panel_property_test_if_property_is_not_same_should_return_err(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_add_range_property(panel, "test_value", 1, 0, 1, NULL);

	KUNIT_EXPECT_EQ(test, panel_set_property_value(panel, "test_str", 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_get_property_value(panel, "test_str"), -EINVAL);

	panel_delete_property(panel, "test_value");
}

static void panel_property_test_panel_property_add_prop_array(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
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

	KUNIT_ASSERT_TRUE(test, !panel_add_property_from_array(panel,
			prop_array, ARRAY_SIZE(prop_array)));

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test,
			panel_find_property(panel, PANEL_PROPERTY_PANEL_STATE));
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test,
			panel_find_property(panel, PANEL_PROPERTY_WAIT_TX_DONE));
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test,
			panel_find_property(panel, PANEL_PROPERTY_PANEL_REFRESH_RATE));
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test,
			panel_find_property(panel, PANEL_PROPERTY_PANEL_REFRESH_MODE));
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test,
			panel_find_property(panel, PANEL_PROPERTY_RESOLUTION_CHANGED));

	panel_delete_property_all(panel);
	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_property_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_property_test *ctx = kunit_kzalloc(test,
		sizeof(struct panel_property_test), GFP_KERNEL);

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	INIT_LIST_HEAD(&panel->prop_list);

	ctx->panel = panel;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_property_test_exit(struct kunit *test)
{
	struct panel_property_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_property_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_property_test_test),

	KUNIT_CASE(panel_property_test_prop_type_to_string),
	KUNIT_CASE(panel_property_test_string_to_prop_type),

	KUNIT_CASE(panel_property_test_snprintf_property),

	/* panel_add_range_property */
	KUNIT_CASE(panel_property_test_panel_property_add_range_success),
	KUNIT_CASE(panel_property_test_panel_property_add_range_should_return_err_with_wrong_input),
	KUNIT_CASE(panel_property_test_panel_property_add_range_should_return_err_with_duplicated_name),

	/* panel_find_property */
	KUNIT_CASE(panel_property_test_panel_property_find_success),
	KUNIT_CASE(panel_property_test_panel_property_find_should_return_err_with_wrong_input),

	/* panel_delete_property */
	KUNIT_CASE(panel_property_test_panel_property_delete_success),
	KUNIT_CASE(panel_property_test_panel_property_delete_should_return_err_with_wrong_input),

	/* panel_set_property_value */
	KUNIT_CASE(panel_property_test_panel_property_set_range_value_success),
	KUNIT_CASE(panel_property_test_panel_property_set_enum_value_success),
	KUNIT_CASE(panel_property_test_panel_property_set_value_success),
	KUNIT_CASE(panel_property_test_panel_property_set_value_should_return_err_with_wrong_input),

	/* panel_get_property_value */
	KUNIT_CASE(panel_property_test_panel_property_get_value_success),
	KUNIT_CASE(panel_property_test_panel_property_get_value_should_return_err_with_wrong_input),

	KUNIT_CASE(panel_property_test_panel_property_add_enum_value_success),
	KUNIT_CASE(panel_property_test_panel_property_add_enum_success),

	/* Type check */
	KUNIT_CASE(panel_property_test_if_property_is_not_same_should_return_err),

	/* panel_add_property_from_array and panel_delete_property_all */
	KUNIT_CASE(panel_property_test_panel_property_add_prop_array),
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
static struct kunit_suite panel_property_test_module = {
	.name = "panel_property_test",
	.init = panel_property_test_init,
	.exit = panel_property_test_exit,
	.test_cases = panel_property_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_property_test_module);

MODULE_LICENSE("GPL v2");
