// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/regulator/consumer.h>
#include "test_helper.h"
#include "panel_drv.h"
#include "panel_regulator.h"
#include "mock_panel_drv_funcs.h"

#define DEF_VOLTAGE 3000000
#define LPM_VOLTAGE 1800000
#define DEF_CURRENT 4000
#define LPM_CURRENT 0

struct panel_regulator_test {
	struct panel_device *panel;
	struct panel_regulator *regulator_vci; /* for voltage control */
	struct panel_regulator *regulator_ssd; /* for current limitation control */
};

extern struct class *lcd_class;

DECLARE_REDIRECT_MOCKABLE(regulator_enable_wrapper, RETURNS(int), PARAMS(struct regulator *));
DECLARE_REDIRECT_MOCKABLE(regulator_disable_wrapper, RETURNS(int), PARAMS(struct regulator *));
DECLARE_REDIRECT_MOCKABLE(regulator_get_voltage_wrapper, RETURNS(int), PARAMS(struct regulator *));
DECLARE_REDIRECT_MOCKABLE(regulator_set_voltage_wrapper, RETURNS(int), PARAMS(struct regulator *, int, int));
DECLARE_REDIRECT_MOCKABLE(regulator_get_current_limit_wrapper, RETURNS(int), PARAMS(struct regulator *));
DECLARE_REDIRECT_MOCKABLE(regulator_set_current_limit_wrapper, RETURNS(int), PARAMS(struct regulator *, int, int));

DEFINE_FUNCTION_MOCK(regulator_enable_wrapper, RETURNS(int), PARAMS(struct regulator *));
DEFINE_FUNCTION_MOCK(regulator_disable_wrapper, RETURNS(int), PARAMS(struct regulator *));
DEFINE_FUNCTION_MOCK(regulator_get_voltage_wrapper, RETURNS(int), PARAMS(struct regulator *));
DEFINE_FUNCTION_MOCK(regulator_set_voltage_wrapper, RETURNS(int), PARAMS(struct regulator *, int, int));
DEFINE_FUNCTION_MOCK(regulator_get_current_limit_wrapper, RETURNS(int), PARAMS(struct regulator *));
DEFINE_FUNCTION_MOCK(regulator_set_current_limit_wrapper, RETURNS(int), PARAMS(struct regulator *, int, int));

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
static void panel_regulator_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_regulator_test_panel_regulator_enable_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_enable(NULL), -EINVAL);

	/* test 'struct regulator *reg' of panel_regulator is null case */
	regulator->reg = NULL;
	KUNIT_EXPECT_EQ(test, panel_regulator_helper_enable(regulator), -EINVAL);
}

static void panel_regulator_test_panel_regulator_enable_should_skip_if_not_panel_regulator_type_pwr(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_ssd;

	/* panel_regulator type is not PANEL_REGULATOR_TYPE_PWR */
	KUNIT_EXPECT_EQ(test, panel_regulator_helper_enable(regulator), 0);
}

static void panel_regulator_test_panel_regulator_enable_fail_if_regulator_enable_is_failed(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;
	struct mock_expectation *expectation;

	expectation = Times(1, KUNIT_EXPECT_CALL(regulator_enable_wrapper(ptr_eq(test, regulator->reg))));
	expectation->action = int_return(test, -EINVAL);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_enable(regulator), -EINVAL);
}

static void panel_regulator_test_panel_regulator_enable_success(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;
	struct mock_expectation *expectation;

	expectation = Times(1, KUNIT_EXPECT_CALL(regulator_enable_wrapper(ptr_eq(test, regulator->reg))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_enable(regulator), 0);
}

static void panel_regulator_test_panel_regulator_disable_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_disable(NULL), -EINVAL);

	/* test 'struct regulator *reg' of panel_regulator is null case */
	regulator->reg = NULL;
	KUNIT_EXPECT_EQ(test, panel_regulator_helper_disable(regulator), -EINVAL);
}

static void panel_regulator_test_panel_regulator_disable_should_skip_if_not_panel_regulator_type_pwr(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_ssd;

	/* panel_regulator type is not PANEL_REGULATOR_TYPE_PWR */
	KUNIT_EXPECT_EQ(test, panel_regulator_helper_disable(regulator), 0);
}

static void panel_regulator_test_panel_regulator_disable_fail_if_regulator_disable_is_failed(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;
	struct mock_expectation *expectation;

	expectation = Times(1, KUNIT_EXPECT_CALL(regulator_disable_wrapper(ptr_eq(test, regulator->reg))));
	expectation->action = int_return(test, -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_regulator_helper_disable(regulator), -EINVAL);
}

static void panel_regulator_test_panel_regulator_disable_success(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;
	struct mock_expectation *expectation;

	expectation = Times(1, KUNIT_EXPECT_CALL(regulator_disable_wrapper(ptr_eq(test, regulator->reg))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_disable(regulator), 0);
}

static void panel_regulator_test_panel_regulator_set_voltage_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_voltage(NULL, DEF_VOLTAGE), -EINVAL);

	/* test 'struct regulator *reg' of panel_regulator is null case */
	regulator->reg = NULL;
	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_voltage(regulator, DEF_VOLTAGE), -EINVAL);
}

static void panel_regulator_test_panel_regulator_set_voltage_skip_if_voltage_is_zero(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_voltage(regulator, 0), 0);
}

static void panel_regulator_test_panel_regulator_set_voltage_should_skip_if_not_panel_regulator_type_pwr(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_ssd;

	/* panel_regulator type is not PANEL_REGULATOR_TYPE_PWR */
	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_voltage(regulator, DEF_VOLTAGE), 0);
}

static void panel_regulator_test_panel_regulator_set_voltage_fail_if_regulator_get_voltage_is_failed(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;
	struct mock_expectation *expectation;

	expectation = Times(1, KUNIT_EXPECT_CALL(regulator_get_voltage_wrapper(ptr_eq(test, regulator->reg))));
	expectation->action = int_return(test, -EPERM);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_voltage(regulator, DEF_VOLTAGE), -EPERM);
}

static void panel_regulator_test_panel_regulator_set_voltage_fail_if_regulator_set_voltage_is_failed(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;
	struct mock_expectation *expectation1, *expectation2;

	expectation1 = Times(1, KUNIT_EXPECT_CALL(regulator_get_voltage_wrapper(ptr_eq(test, regulator->reg))));
	expectation1->action = int_return(test, LPM_VOLTAGE);
	expectation2 = Times(1, KUNIT_EXPECT_CALL(regulator_set_voltage_wrapper(
					ptr_eq(test, regulator->reg), int_eq(test, DEF_VOLTAGE), int_eq(test, DEF_VOLTAGE))));
	expectation2->action = int_return(test, -EPERM);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_voltage(regulator, DEF_VOLTAGE), -EPERM);
}

static void panel_regulator_test_panel_regulator_set_voltage_skip_if_current_voltage_is_same_with_requested_voltage(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;
	struct mock_expectation *expectation;

	expectation = Times(1, KUNIT_EXPECT_CALL(regulator_get_voltage_wrapper(ptr_eq(test, regulator->reg))));
	expectation->action = int_return(test, DEF_VOLTAGE);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_voltage(regulator, DEF_VOLTAGE), 0);
}

static void panel_regulator_test_panel_regulator_set_voltage_success(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;
	struct mock_expectation *expectation1, *expectation2;

	expectation1 = Times(1, KUNIT_EXPECT_CALL(regulator_get_voltage_wrapper(ptr_eq(test, regulator->reg))));
	expectation1->action = int_return(test, LPM_VOLTAGE);
	expectation2 = Times(1, KUNIT_EXPECT_CALL(regulator_set_voltage_wrapper(
					ptr_eq(test, regulator->reg), int_eq(test, DEF_VOLTAGE), int_eq(test, DEF_VOLTAGE))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_voltage(regulator, DEF_VOLTAGE), 0);
}

static void panel_regulator_test_panel_regulator_set_current_limit_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_ssd;

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_current_limit(NULL, DEF_CURRENT), -EINVAL);

	/* test 'struct regulator *reg' of panel_regulator is null case */
	regulator->reg = NULL;
	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_current_limit(regulator, DEF_CURRENT), -EINVAL);
}

static void panel_regulator_test_panel_regulator_set_current_limit_should_skip_if_not_panel_regulator_type_ssd(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_vci;

	/* panel_regulator type is not PANEL_REGULATOR_TYPE_PWR */
	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_current_limit(regulator, DEF_CURRENT), 0);
}

static void panel_regulator_test_panel_regulator_set_current_limit_fail_if_regulator_get_current_limit_is_failed(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_ssd;
	struct mock_expectation *expectation;

	expectation = Times(1, KUNIT_EXPECT_CALL(regulator_get_current_limit_wrapper(ptr_eq(test, regulator->reg))));
	expectation->action = int_return(test, -EPERM);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_current_limit(regulator, DEF_CURRENT), -EPERM);
}

static void panel_regulator_test_panel_regulator_set_current_limit_fail_if_regulator_set_current_limit_is_failed(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_ssd;
	struct mock_expectation *expectation1, *expectation2;

	expectation1 = Times(1, KUNIT_EXPECT_CALL(regulator_get_current_limit_wrapper(ptr_eq(test, regulator->reg))));
	expectation1->action = int_return(test, LPM_CURRENT);
	expectation2 = Times(1, KUNIT_EXPECT_CALL(regulator_set_current_limit_wrapper(
					ptr_eq(test, regulator->reg), int_eq(test, DEF_CURRENT), int_eq(test, DEF_CURRENT))));
	expectation2->action = int_return(test, -EPERM);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_current_limit(regulator, DEF_CURRENT), -EPERM);
}

static void panel_regulator_test_panel_regulator_set_current_limit_skip_if_current_limit_is_same_with_requested_current_limit(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_ssd;
	struct mock_expectation *expectation1, *expectation2;

	expectation1 = Times(1, KUNIT_EXPECT_CALL(regulator_get_current_limit_wrapper(ptr_eq(test, regulator->reg))));
	expectation1->action = int_return(test, DEF_CURRENT);
	expectation2 = Never(KUNIT_EXPECT_CALL(regulator_set_current_limit_wrapper(
					ptr_eq(test, regulator->reg), int_eq(test, DEF_CURRENT), int_eq(test, DEF_CURRENT))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_current_limit(regulator, DEF_CURRENT), 0);
}

static void panel_regulator_test_panel_regulator_set_current_limit_success(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_regulator *regulator = ctx->regulator_ssd;
	struct mock_expectation *expectation1, *expectation2;

	expectation1 = Times(1, KUNIT_EXPECT_CALL(regulator_get_current_limit_wrapper(ptr_eq(test, regulator->reg))));
	expectation1->action = int_return(test, LPM_CURRENT);
	expectation2 = Times(1, KUNIT_EXPECT_CALL(regulator_set_current_limit_wrapper(
					ptr_eq(test, regulator->reg), int_eq(test, DEF_CURRENT), int_eq(test, DEF_CURRENT))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_regulator_helper_set_current_limit(regulator, DEF_CURRENT), 0);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_regulator_test_init(struct kunit *test)
{
	struct panel_regulator_test *ctx;
	struct panel_device *panel;
	struct panel_regulator *regulator_vci, *regulator_ssd;
	struct panel_regulator_funcs *orig_funcs;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel->dev = device_create(lcd_class,
			NULL, 0, panel, PANEL_DRV_NAME);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->dev);

	panel_mutex_init(panel, &panel->io_lock);

	panel->funcs = &panel_drv_funcs;
	ctx->panel = panel;

	/* initialize regulator for voltage control */
	regulator_vci = panel_regulator_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regulator_vci->funcs);
	orig_funcs = regulator_vci->funcs;
	regulator_vci->funcs = kunit_kzalloc(test, sizeof(*regulator_vci->funcs), GFP_KERNEL);
	memcpy(regulator_vci->funcs, orig_funcs, sizeof(*orig_funcs));

	regulator_vci->name = PANEL_REGULATOR_NAME_DDI_VCI;
	/* unable to create 'struct regulator', so create dummy buffer */
	regulator_vci->reg = kunit_kzalloc(test, 1, GFP_KERNEL);
	regulator_vci->type = PANEL_REGULATOR_TYPE_PWR;
	ctx->regulator_vci = regulator_vci;

	/* initialize regulator for current limit control */
	regulator_ssd = panel_regulator_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, regulator_ssd->funcs);
	orig_funcs = regulator_ssd->funcs;
	regulator_ssd->funcs = kunit_kzalloc(test, sizeof(*regulator_ssd->funcs), GFP_KERNEL);
	memcpy(regulator_ssd->funcs, orig_funcs, sizeof(*orig_funcs));

	regulator_ssd->name = PANEL_REGULATOR_NAME_SSD;
	/* unable to create 'struct regulator', so create dummy buffer */
	regulator_ssd->reg = kunit_kzalloc(test, 1, GFP_KERNEL);
	regulator_ssd->type = PANEL_REGULATOR_TYPE_SSD;
	ctx->regulator_ssd = regulator_ssd;

	test->priv = ctx;

	/* set default action as INVOKE_REAL() */
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(regulator_enable_wrapper);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(regulator_disable_wrapper);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(regulator_get_voltage_wrapper);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(regulator_set_voltage_wrapper);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(regulator_get_current_limit_wrapper);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(regulator_set_current_limit_wrapper);

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_regulator_test_exit(struct kunit *test)
{
	struct panel_regulator_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_regulator_destroy(ctx->regulator_vci);
	panel_regulator_destroy(ctx->regulator_ssd);
	device_unregister(panel->dev);
	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_regulator_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_regulator_test_test),

	/* panel_regulator_enable */
	KUNIT_CASE(panel_regulator_test_panel_regulator_enable_fail_with_invalid_arg),
	KUNIT_CASE(panel_regulator_test_panel_regulator_enable_should_skip_if_not_panel_regulator_type_pwr),
	KUNIT_CASE(panel_regulator_test_panel_regulator_enable_fail_if_regulator_enable_is_failed),
	KUNIT_CASE(panel_regulator_test_panel_regulator_enable_success),

	/* panel_regulator_disable */
	KUNIT_CASE(panel_regulator_test_panel_regulator_disable_fail_with_invalid_arg),
	KUNIT_CASE(panel_regulator_test_panel_regulator_disable_should_skip_if_not_panel_regulator_type_pwr),
	KUNIT_CASE(panel_regulator_test_panel_regulator_disable_fail_if_regulator_disable_is_failed),
	KUNIT_CASE(panel_regulator_test_panel_regulator_disable_success),

	/* panel_regulator_set_voltage */
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_voltage_fail_with_invalid_arg),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_voltage_skip_if_voltage_is_zero),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_voltage_should_skip_if_not_panel_regulator_type_pwr),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_voltage_fail_if_regulator_get_voltage_is_failed),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_voltage_fail_if_regulator_set_voltage_is_failed),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_voltage_skip_if_current_voltage_is_same_with_requested_voltage),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_voltage_success),

	/* panel_regulator_set_current_limit */
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_current_limit_fail_with_invalid_arg),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_current_limit_should_skip_if_not_panel_regulator_type_ssd),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_current_limit_fail_if_regulator_get_current_limit_is_failed),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_current_limit_fail_if_regulator_set_current_limit_is_failed),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_current_limit_skip_if_current_limit_is_same_with_requested_current_limit),
	KUNIT_CASE(panel_regulator_test_panel_regulator_set_current_limit_success),
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
static struct kunit_suite panel_regulator_test_module = {
	.name = "panel_regulator_test",
	.init = panel_regulator_test_init,
	.exit = panel_regulator_test_exit,
	.test_cases = panel_regulator_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_regulator_test_module);

MODULE_LICENSE("GPL v2");
