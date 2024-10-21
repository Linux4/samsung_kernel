// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/regulator/consumer.h>
#include "test_helper.h"
#include "panel_drv.h"
#include "panel_power_ctrl.h"
#include "mock_panel_drv_funcs.h"

#define DEF_VOLTAGE 3000000
#define LPM_VOLTAGE 1800000
#define DEF_CURRENT 4000
#define LPM_CURRENT 0

struct panel_power_ctrl_test {
	struct panel_device *panel;
	struct panel_power_ctrl *pctrl;
};

extern struct class *lcd_class;
extern struct panel_power_ctrl_funcs panel_power_ctrl_funcs;

extern struct panel_power_ctrl *panel_power_ctrl_find(struct panel_device *panel,
	const char *dev_name, const char *name);

int panel_power_ctrl_test_dummy_panel_power_ctrl_action_execute(struct panel_power_ctrl *pctrl)
{
	return 0;
}

struct panel_power_ctrl_funcs panel_power_ctrl_test_dummy_pctrl_functions = {
	.execute = panel_power_ctrl_test_dummy_panel_power_ctrl_action_execute,
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
static void panel_power_ctrl_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_power_ctrl_test_panel_power_ctrl_helper_execute_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(NULL), -EINVAL);
}

static void panel_power_ctrl_test_panel_power_ctrl_helper_execute_success_with_empty_action_list(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_power_ctrl *pctrl = ctx->pctrl;

	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), 0);
}

static void panel_power_ctrl_test_panel_power_ctrl_helper_execute_fail_with_invalid_regulator(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_power_ctrl *pctrl = ctx->pctrl;
	struct panel_power_ctrl_action *action;

	action = kunit_kzalloc(test, sizeof(*action), GFP_KERNEL);
	action->name = "test action";
	action->reg = 0;
	action->value = 0;

	action->type = PANEL_POWER_CTRL_ACTION_REGULATOR_ENABLE;
	list_add(&action->head, &pctrl->action_list);

	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), -ENODEV);

	action->type = PANEL_POWER_CTRL_ACTION_REGULATOR_DISABLE;
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), -ENODEV);

	action->type = PANEL_POWER_CTRL_ACTION_REGULATOR_SET_VOLTAGE;
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), -ENODEV);

	action->type = PANEL_POWER_CTRL_ACTION_REGULATOR_SET_VOLTAGE;
	action->reg = (struct panel_regulator *)0xAAAA00;
	action->value = 0;
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), -ENODEV);

	action->type = PANEL_POWER_CTRL_ACTION_REGULATOR_SSD_CURRENT;
	action->reg = 0;
	action->value = 2000;
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), -ENODEV);
}

static void panel_power_ctrl_test_panel_power_ctrl_helper_execute_fail_with_invalid_gpio(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_power_ctrl *pctrl = ctx->pctrl;
	struct panel_power_ctrl_action *action;

	action = kunit_kzalloc(test, sizeof(*action), GFP_KERNEL);
	action->name = "test action";
	action->gpio = 0;

	list_add(&action->head, &pctrl->action_list);

	action->type = PANEL_POWER_CTRL_ACTION_GPIO_ENABLE;
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), -ENODEV);

	action->type = PANEL_POWER_CTRL_ACTION_GPIO_DISABLE;
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), -ENODEV);
}

static void panel_power_ctrl_test_panel_power_ctrl_helper_execute_fail_with_invalid_delay(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_power_ctrl *pctrl = ctx->pctrl;
	struct panel_power_ctrl_action *action;

	action = kunit_kzalloc(test, sizeof(*action), GFP_KERNEL);
	action->name = "test action";
	action->value = 0;
	list_add(&action->head, &pctrl->action_list);

	action->type = PANEL_POWER_CTRL_ACTION_DELAY_MSLEEP;
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), -EINVAL);

	action->type = PANEL_POWER_CTRL_ACTION_DELAY_USLEEP;
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), -EINVAL);
}

#if 0
static void panel_power_ctrl_test_panel_power_ctrl_helper_execute_success_with_delay_action(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_power_ctrl *pctrl = ctx->pctrl;
	struct panel_power_ctrl_action *action;

	action = kunit_kzalloc(test, sizeof(*action), GFP_KERNEL);
	action->name = "test action";
	action->value = 10;
	list_add(&action->head, &pctrl->action_list);

	action->type = PANEL_POWER_CTRL_ACTION_DELAY_MSLEEP;
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), 0);

	action->type = PANEL_POWER_CTRL_ACTION_DELAY_USLEEP;
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_helper_execute(pctrl), 0);
}
#endif

static void panel_power_ctrl_test_panel_power_ctrl_find_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_TRUE(test, IS_ERR(panel_power_ctrl_find(NULL, "name1", "name2")));
	KUNIT_EXPECT_TRUE(test, IS_ERR(panel_power_ctrl_find(panel, NULL, "name2")));
	KUNIT_EXPECT_TRUE(test, IS_ERR(panel_power_ctrl_find(panel, "name1", NULL)));
}

static void panel_power_ctrl_test_panel_power_ctrl_find_fail_with_empty_list(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_TRUE(test, IS_ERR(panel_power_ctrl_find(panel, "name1", "name2")));
}

static void panel_power_ctrl_test_panel_power_ctrl_find_fail_with_name_mismatch(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_power_ctrl pctrl = {
		.dev_name = panel->of_node_name,
		.name = "test_pctrl_name",
		.funcs = &panel_power_ctrl_test_dummy_pctrl_functions,
	};
	struct panel_power_ctrl_action pact = {
		.type = PANEL_POWER_CTRL_ACTION_NONE,
		.name = "test_act_name",
		.value = 1,
	};

	INIT_LIST_HEAD(&pctrl.action_list);

	list_add_tail(&pact.head, &pctrl.action_list);
	list_add_tail(&pctrl.head, &panel->power_ctrl_list);

	KUNIT_EXPECT_TRUE(test, IS_ERR(panel_power_ctrl_find(panel, panel->of_node_name, "wrong_pctrl_name")));
	KUNIT_EXPECT_TRUE(test, IS_ERR(panel_power_ctrl_find(panel, "wrong_panel_name", "test_pctrl_name")));
}

static void panel_power_ctrl_test_panel_power_ctrl_find_success(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_power_ctrl pctrl = {
		.dev_name = panel->of_node_name,
		.name = "test_pctrl_name",
		.funcs = &panel_power_ctrl_test_dummy_pctrl_functions,
	};
	struct panel_power_ctrl_action pact = {
		.type = PANEL_POWER_CTRL_ACTION_NONE,
		.name = "test_act_name",
		.value = 1,
	};

	INIT_LIST_HEAD(&pctrl.action_list);

	list_add_tail(&pact.head, &pctrl.action_list);
	list_add_tail(&pctrl.head, &panel->power_ctrl_list);

	KUNIT_EXPECT_TRUE(test, !IS_ERR(panel_power_ctrl_find(panel, panel->of_node_name, "test_pctrl_name")));
}

static void panel_power_ctrl_test_panel_power_ctrl_exists_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_FALSE(test, panel_power_ctrl_exists(NULL, panel->of_node_name, "name1"));
	KUNIT_EXPECT_FALSE(test, panel_power_ctrl_exists(panel, panel->of_node_name, NULL));
}

static void panel_power_ctrl_test_panel_power_ctrl_execute_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_EXPECT_EQ(test, panel_power_ctrl_execute(NULL, panel->of_node_name, "name1"), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_power_ctrl_execute(panel, panel->of_node_name, NULL), -EINVAL);
}

static void panel_power_ctrl_test_create_pwrctrl_fail_with_invalid_arg(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, create_pwrctrl("fast_discharge_enable", NULL) == NULL);
	KUNIT_EXPECT_TRUE(test, create_pwrctrl(NULL, "panel_fd_enable") == NULL);
}

static void panel_power_ctrl_test_create_pwrctrl_success(struct kunit *test)
{
	struct pwrctrl *pwrctrl =
		create_pwrctrl("fast_discharge_enable", "panel_fd_enable");

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pwrctrl);
	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&pwrctrl->base), CMD_TYPE_PCTRL);
	KUNIT_EXPECT_STREQ(test, get_pwrctrl_name(pwrctrl), "fast_discharge_enable");
	KUNIT_EXPECT_STREQ(test, get_pwrctrl_key(pwrctrl), "panel_fd_enable");
	destroy_pwrctrl(pwrctrl);
}

static void panel_power_ctrl_test_duplicate_pwrctrl_success(struct kunit *test)
{
	struct pwrctrl *pwrctrl =
		create_pwrctrl("fast_discharge_enable", "panel_fd_enable");
	struct pwrctrl *pwrctrl_dup = duplicate_pwrctrl(pwrctrl);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pwrctrl);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pwrctrl_dup);

	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&pwrctrl_dup->base), CMD_TYPE_PCTRL);
	KUNIT_EXPECT_STREQ(test, get_pwrctrl_name(pwrctrl), get_pwrctrl_name(pwrctrl_dup));
	KUNIT_EXPECT_STREQ(test, get_pwrctrl_key(pwrctrl), get_pwrctrl_key(pwrctrl_dup));

	destroy_pwrctrl(pwrctrl);
	destroy_pwrctrl(pwrctrl_dup);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_power_ctrl_test_init(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx;
	struct panel_device *panel;
	struct panel_power_ctrl *pctrl;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel->of_node_name = "test_panel_name";
	panel->dev = device_create(lcd_class,
			NULL, 0, panel, PANEL_DRV_NAME);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->dev);

	panel_mutex_init(panel, &panel->io_lock);

	INIT_LIST_HEAD(&panel->gpio_list);
	INIT_LIST_HEAD(&panel->power_ctrl_list);
	INIT_LIST_HEAD(&panel->regulator_list);
	INIT_LIST_HEAD(&panel->panel_lut_list);
	INIT_LIST_HEAD(&panel->prop_list);

	panel->funcs = &panel_drv_funcs;
	ctx->panel = panel;

	pctrl = kunit_kzalloc(test, sizeof(*pctrl), GFP_KERNEL);
	pctrl->name = "panel_power_ctrl_test";
	INIT_LIST_HEAD(&pctrl->action_list);
	pctrl->funcs = &panel_power_ctrl_funcs;
	ctx->pctrl = pctrl;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_power_ctrl_test_exit(struct kunit *test)
{
	struct panel_power_ctrl_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	device_unregister(panel->dev);
	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_power_ctrl_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_power_ctrl_test_test),
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_helper_execute_fail_with_invalid_arg),
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_helper_execute_success_with_empty_action_list),
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_helper_execute_fail_with_invalid_regulator),
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_helper_execute_fail_with_invalid_gpio),
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_helper_execute_fail_with_invalid_delay),
#if 0
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_helper_execute_success_with_delay_action),
#endif
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_find_fail_with_invalid_arg),
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_find_fail_with_empty_list),
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_find_fail_with_name_mismatch),
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_find_success),
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_exists_fail_with_invalid_arg),
	KUNIT_CASE(panel_power_ctrl_test_panel_power_ctrl_execute_fail_with_invalid_arg),
	KUNIT_CASE(panel_power_ctrl_test_create_pwrctrl_fail_with_invalid_arg),
	KUNIT_CASE(panel_power_ctrl_test_create_pwrctrl_success),
	KUNIT_CASE(panel_power_ctrl_test_duplicate_pwrctrl_success),
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
static struct kunit_suite panel_power_ctrl_test_module = {
	.name = "panel_power_ctrl_test",
	.init = panel_power_ctrl_test_init,
	.exit = panel_power_ctrl_test_exit,
	.test_cases = panel_power_ctrl_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_power_ctrl_test_module);

MODULE_LICENSE("GPL v2");
