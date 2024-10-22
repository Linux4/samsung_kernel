// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "test_helper.h"
#include "panel_drv.h"
#include "oled_common/oled_common_cond.h"

enum {
	TEST_RESOL_1440x3200,
	TEST_RESOL_1080x2400,
	TEST_RESOL_720x1600,
};

static struct panel_prop_enum_item panel_state_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_OFF),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ON),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_NORMAL),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ALPM),
};

static struct panel_prop_enum_item vrr_mode_enum_items[] = {
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_NORMAL_MODE),
	__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_HS_MODE),
};

static struct panel_prop_list oled_common_cond_test_mandatory_property[] = {
	/* enum property */
	__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PANEL_STATE,
			PANEL_STATE_OFF, panel_state_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE,
			VRR_NORMAL_MODE, vrr_mode_enum_items),
	__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE,
			VRR_NORMAL_MODE, vrr_mode_enum_items),
	/* range property */
	__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE,
			60, 0, 120),
	__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE,
			60, 0, 120),
};

static struct panel_vrr oled_common_cond_test_panel_vrr[] = {
	/* 60NS */
	[0] = {
		.fps = 60,
		.base_fps = 60,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_NORMAL_MODE,
	},
	/* 60pHS */
	[1] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 1,
		.mode = VRR_HS_MODE,
	},
	/* 120HS */
	[2] = {
		.fps = 120,
		.base_fps = 120,
		.base_vactive = 2340,
		.base_vfp = 16,
		.base_vbp = 16,
		.te_sel = true,
		.te_v_st = 2341,
		.te_v_ed = 9,
		.te_sw_skip_count = 0,
		.te_hw_skip_count = 0,
		.mode = VRR_HS_MODE,
	},
};

static struct panel_vrr *oled_common_cond_test_panel_vrrtbl[] = {
	&oled_common_cond_test_panel_vrr[0],
	&oled_common_cond_test_panel_vrr[1],
	&oled_common_cond_test_panel_vrr[2],
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

#if !defined(CONFIG_UML)
/* NOTE: Target running TC must be in the #ifndef CONFIG_UML */
static void oled_common_cond_test_foo(struct kunit *test)
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
static void oled_common_cond_test_test(struct kunit *test)
{
	KUNIT_SUCCEED(test);
}

static void oled_common_cond_test_oled_cond_is_panel_display_mode_changed(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	panel->panel_data.props.vrr_origin_idx = 0;
	panel->panel_data.props.vrr_idx = 0;
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_display_mode_changed(panel));

	panel->panel_data.props.vrr_origin_idx = 0;
	panel->panel_data.props.vrr_idx = 1;
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_display_mode_changed(panel));
}

static void oled_common_cond_test_oled_cond_is_panel_refresh_rate_changed(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	panel->panel_data.props.vrr_origin_fps = 60;
	KUNIT_EXPECT_TRUE(test, !panel_set_property(panel, &panel->panel_data.props.vrr_fps, 60));
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_refresh_rate_changed(panel));

	panel->panel_data.props.vrr_origin_fps = 60;
	KUNIT_EXPECT_TRUE(test, !panel_set_property(panel, &panel->panel_data.props.vrr_fps, 120));
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_refresh_rate_changed(panel));
}

static void oled_common_cond_test_oled_cond_is_panel_refresh_mode_changed(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	panel->panel_data.props.vrr_origin_mode = VRR_HS_MODE;
	KUNIT_EXPECT_TRUE(test, !panel_set_property(panel, &panel->panel_data.props.vrr_mode, VRR_HS_MODE));
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_refresh_mode_changed(panel));

	panel->panel_data.props.vrr_origin_mode = VRR_HS_MODE;
	KUNIT_EXPECT_TRUE(test, !panel_set_property(panel, &panel->panel_data.props.vrr_mode, VRR_NORMAL_MODE));
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_refresh_mode_changed(panel));
}

static void oled_common_cond_test_oled_cond_is_panel_refresh_mode_ns(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct panel_properties *props = &panel->panel_data.props;

	props->vrr_idx = 0;
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_refresh_mode_ns(panel));

	props->vrr_idx = 1;
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_refresh_mode_ns(panel));

	props->vrr_idx = 2;
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_refresh_mode_ns(panel));
}

static void oled_common_cond_test_oled_cond_is_panel_refresh_mode_hs(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct panel_properties *props = &panel->panel_data.props;

	props->vrr_idx = 0;
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_refresh_mode_hs(panel));

	props->vrr_idx = 1;
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_refresh_mode_hs(panel));

	props->vrr_idx = 2;
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_refresh_mode_hs(panel));
}

static void oled_common_cond_test_oled_cond_is_panel_state_lpm(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	panel_set_cur_state(panel, PANEL_STATE_OFF);
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_state_lpm(panel));

	panel_set_cur_state(panel, PANEL_STATE_ON);
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_state_lpm(panel));

	panel_set_cur_state(panel, PANEL_STATE_NORMAL);
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_state_lpm(panel));

	panel_set_cur_state(panel, PANEL_STATE_ALPM);
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_state_lpm(panel));
}

static void oled_common_cond_test_oled_cond_is_panel_state_not_lpm(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	panel_set_cur_state(panel, PANEL_STATE_OFF);
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_state_not_lpm(panel));

	panel_set_cur_state(panel, PANEL_STATE_ON);
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_state_not_lpm(panel));

	panel_set_cur_state(panel, PANEL_STATE_NORMAL);
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_state_not_lpm(panel));

	panel_set_cur_state(panel, PANEL_STATE_ALPM);
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_state_not_lpm(panel));
}

static void oled_common_cond_test_is_first_set_bl(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct panel_bl_device *panel_bl = &panel->panel_bl;

	/* brightness updated not first time case */
	atomic_set(&panel_bl->props.brightness_set_count, 128);
	KUNIT_EXPECT_FALSE(test, oled_cond_is_first_set_bl(panel));

	/* brightness updated first time case */
	atomic_set(&panel_bl->props.brightness_set_count, 0);
	KUNIT_EXPECT_TRUE(test, oled_cond_is_first_set_bl(panel));
}

static void oled_common_cond_test_is_panel_mres_updated(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	panel->panel_data.props.mres_updated = false;
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_mres_updated(panel));

	panel->panel_data.props.mres_updated = true;
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_mres_updated(panel));
}

static void oled_common_cond_test_oled_cond_is_panel_mres_updated_bigger_false_with_invalid_argument(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_mres_updated_bigger(NULL));
}

static void oled_common_cond_test_oled_cond_is_panel_mres_updated_bigger_false_with_invalid_resolution(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_properties *props = &panel->panel_data.props;
	struct panel_resol resolution[] = {
		[TEST_RESOL_1440x3200] = { .w = 1440, .h = 3200, .comp_type = PN_COMP_TYPE_NONE, },
		[TEST_RESOL_1080x2400] = { .w = 1080, .h = 2400, .comp_type = PN_COMP_TYPE_MIC, },
		[TEST_RESOL_720x1600] = { .w = 720, .h = 1600, .comp_type = PN_COMP_TYPE_DSC, },
	};

	mres->resol = NULL;
	mres->nr_resol = 0;
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_mres_updated_bigger(panel));

	mres->resol = resolution;
	mres->nr_resol = ARRAY_SIZE(resolution);

	/* old_mres_mode is out of number of resolution case */
	props->old_mres_mode = mres->nr_resol;
	props->mres_mode = TEST_RESOL_1440x3200;
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_mres_updated_bigger(panel));

	/* mres_mode is out of number of resolution case */
	props->old_mres_mode = TEST_RESOL_1440x3200;
	props->mres_mode = mres->nr_resol;
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_mres_updated_bigger(panel));
}

static void oled_common_cond_test_oled_cond_is_panel_mres_updated_bigger_success(struct kunit *test)
{
	struct panel_device *panel = test->priv;
	struct panel_mres *mres = &panel->panel_data.mres;
	struct panel_properties *props = &panel->panel_data.props;
	struct panel_resol resolution[] = {
		[TEST_RESOL_1440x3200] = { .w = 1440, .h = 3200, .comp_type = PN_COMP_TYPE_NONE, },
		[TEST_RESOL_1080x2400] = { .w = 1080, .h = 2400, .comp_type = PN_COMP_TYPE_MIC, },
		[TEST_RESOL_720x1600] = { .w = 720, .h = 1600, .comp_type = PN_COMP_TYPE_DSC, },
	};

	mres->resol = NULL;
	mres->nr_resol = 0;
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_mres_updated_bigger(panel));

	mres->resol = resolution;
	mres->nr_resol = ARRAY_SIZE(resolution);

	/* mres not updated case */
	props->old_mres_mode = TEST_RESOL_1440x3200;
	props->mres_mode = TEST_RESOL_1440x3200;
	props->mres_updated = (props->old_mres_mode != props->mres_mode);
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_mres_updated_bigger(panel));

	/* mres updated smaller resolution case */
	props->old_mres_mode = TEST_RESOL_1440x3200;
	props->mres_mode = TEST_RESOL_720x1600;
	props->mres_updated = (props->old_mres_mode != props->mres_mode);
	KUNIT_EXPECT_FALSE(test, oled_cond_is_panel_mres_updated_bigger(panel));

	/* mres updated bigger resolution case */
	props->old_mres_mode = TEST_RESOL_720x1600;
	props->mres_mode = TEST_RESOL_1440x3200;
	props->mres_updated = (props->old_mres_mode != props->mres_mode);
	KUNIT_EXPECT_TRUE(test, oled_cond_is_panel_mres_updated_bigger(panel));
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int oled_common_cond_test_init(struct kunit *test)
{
	struct panel_device *panel;

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	panel->panel_data.vrrtbl = oled_common_cond_test_panel_vrrtbl;
	panel->panel_data.nr_vrrtbl = ARRAY_SIZE(oled_common_cond_test_panel_vrrtbl);
	panel->panel_data.ddi_props.support_vrr = true;

	INIT_LIST_HEAD(&panel->prop_list);

	KUNIT_ASSERT_TRUE(test, !panel_add_property_from_array(panel,
			oled_common_cond_test_mandatory_property,
			ARRAY_SIZE(oled_common_cond_test_mandatory_property)));

	test->priv = panel;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void oled_common_cond_test_exit(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	panel_delete_property_all(panel);
	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case oled_common_cond_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#if !defined(CONFIG_UML)
	/* NOTE: Target running TC */
	KUNIT_CASE(oled_common_cond_test_foo),
#endif
	/* NOTE: UML TC */
	KUNIT_CASE(oled_common_cond_test_test),
	KUNIT_CASE(oled_common_cond_test_oled_cond_is_panel_display_mode_changed),
	KUNIT_CASE(oled_common_cond_test_oled_cond_is_panel_refresh_rate_changed),
	KUNIT_CASE(oled_common_cond_test_oled_cond_is_panel_refresh_mode_changed),
	KUNIT_CASE(oled_common_cond_test_oled_cond_is_panel_refresh_mode_ns),
	KUNIT_CASE(oled_common_cond_test_oled_cond_is_panel_refresh_mode_hs),
	KUNIT_CASE(oled_common_cond_test_oled_cond_is_panel_state_lpm),
	KUNIT_CASE(oled_common_cond_test_oled_cond_is_panel_state_not_lpm),
	KUNIT_CASE(oled_common_cond_test_is_first_set_bl),
	KUNIT_CASE(oled_common_cond_test_is_panel_mres_updated),
	KUNIT_CASE(oled_common_cond_test_oled_cond_is_panel_mres_updated_bigger_false_with_invalid_argument),
	KUNIT_CASE(oled_common_cond_test_oled_cond_is_panel_mres_updated_bigger_false_with_invalid_resolution),
	KUNIT_CASE(oled_common_cond_test_oled_cond_is_panel_mres_updated_bigger_success),
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
static struct kunit_suite oled_common_cond_test_module = {
	.name = "oled_common_cond_test",
	.init = oled_common_cond_test_init,
	.exit = oled_common_cond_test_exit,
	.test_cases = oled_common_cond_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&oled_common_cond_test_module);

MODULE_LICENSE("GPL v2");
