// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/regulator/driver.h>

#include "panel_drv.h"
#include "panel_blic.h"
#ifdef CONFIG_USDM_BLIC_I2C
#include "panel_i2c.h"
#endif

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

struct panel_blic_test {
	struct panel_device *panel;
	struct panel_blic_dev *blic_dev;
	unsigned int nr_blic_dev;
};

extern struct panel_device *to_panel_drv(struct panel_blic_dev *blic_dev);
extern int panel_blic_get_blic_regulator(struct panel_blic_dev *blic_dev);
extern struct regulator_desc *panel_blic_find_regulator_desc(const char *name);
extern int panel_blic_regulator_enable(struct regulator_dev *rdev);
extern int panel_blic_regulator_disable(struct regulator_dev *rdev);
extern struct blic_data *panel_blic_find_blic_data(struct panel_blic_dev *blic);

#ifdef CONFIG_USDM_BLIC_I2C
extern struct panel_i2c_dev *panel_blic_find_i2c_drv(struct panel_blic_dev *blic_dev, struct panel_device *panel);

DEFINE_FUNCTION_MOCK(mock_panel_i2c_rx, RETURNS(int), PARAMS(struct panel_i2c_dev *, u8 *, u32));
DEFINE_FUNCTION_MOCK(mock_panel_i2c_tx, RETURNS(int), PARAMS(struct panel_i2c_dev *, u8 *, u32));

static struct i2c_drv_ops mock_panel_i2c_drv_ops = {
	.tx = mock_panel_i2c_tx,
	.rx = mock_panel_i2c_rx,
	.transfer = NULL,
};
#endif

DEFINE_FUNCTION_MOCK(mock_panel_blic_do_seq, RETURNS(int),
	PARAMS(struct panel_blic_dev *, char *));
DEFINE_FUNCTION_MOCK(mock_panel_blic_power_ctrl_execute, RETURNS(int),
	PARAMS(struct panel_blic_dev *, const char *));

static struct panel_blic_ops mock_panel_blic_ops = {
	.do_seq = mock_panel_blic_do_seq,
	.execute_power_ctrl = mock_panel_blic_power_ctrl_execute,
};

#ifdef CONFIG_UML
/* NOTE: UML TC */
static void panel_blic_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_blic_test_panel_drv_blic_to_panel_drv_should_return_null_with_null(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, !to_panel_drv(NULL));
}

static void panel_blic_test_panel_drv_blic_probe_should_return_err_with_abnormal_argument(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_blic_probe(NULL), -EINVAL);
}

static void panel_blic_test_panel_drv_blic_get_blic_regulator_success_with_blic_or_no_blic(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;

	panel->nr_blic_dev = 0;
	KUNIT_EXPECT_TRUE(test, panel_blic_get_blic_regulator(blic_dev) == 0);

	panel->nr_blic_dev = 1;
	KUNIT_EXPECT_TRUE(test, panel_blic_get_blic_regulator(&blic_dev[0]) == 0);
}

#ifdef CONFIG_USDM_BLIC_I2C
static void panel_blic_test_panel_blic_find_i2c_drv_success(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct panel_i2c_dev *i2c_dev;
	struct mock_expectation *expectation1;

	panel->nr_blic_dev = 1;
	blic_dev->i2c_reg = 0x11;
	blic_dev->i2c_match[0] = 0x01;
	blic_dev->i2c_match[1] = 0xFF;
	blic_dev->i2c_match[2] = 0x00;

	i2c_dev = &panel->i2c_dev[0];
	panel->nr_i2c_dev = 1;
	i2c_dev->reg = 0x11;
	i2c_dev->ops = &mock_panel_i2c_drv_ops;
	expectation1 = Times(1, KUNIT_EXPECT_CALL(mock_panel_i2c_rx(ptr_eq(test, i2c_dev),
				any(test), any(test))));
	expectation1->action = int_return(test, 0);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, panel_blic_find_i2c_drv(blic_dev, panel));
}

static void panel_blic_test_panel_blic_find_i2c_drv_should_return_null_with_no_i2c(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;

	panel->nr_blic_dev = 1;

	blic_dev->i2c_reg = 0x11;
	blic_dev->i2c_match[0] = 0x01;
	blic_dev->i2c_match[1] = 0xFF;
	blic_dev->i2c_match[2] = 0x00;

	KUNIT_EXPECT_TRUE(test, !panel_blic_find_i2c_drv(blic_dev, panel));
}

static void panel_blic_test_panel_blic_find_i2c_drv_should_return_null_with_wrong_id(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct panel_i2c_dev *i2c_dev;
	struct mock_expectation *expectation1;

	panel->nr_blic_dev = 1;

	i2c_dev = &panel->i2c_dev[0];
	panel->nr_i2c_dev = 1;

	blic_dev->i2c_reg = 0x11;
	blic_dev->i2c_match[0] = 0x01; /* addr		*/
	blic_dev->i2c_match[1] = 0xFF; /* mask		*/
	blic_dev->i2c_match[2] = 0x02; /* val & mask	*/

	i2c_dev->reg = 0x11;
	i2c_dev->ops = &mock_panel_i2c_drv_ops;

	expectation1 = Times(1, KUNIT_EXPECT_CALL(mock_panel_i2c_rx(any(test),
				any(test), any(test))));
	expectation1->action = int_return(test, 0);

	KUNIT_EXPECT_TRUE(test, !panel_blic_find_i2c_drv(blic_dev, panel));
}
#endif

static void panel_blic_test_panel_blic_regulator_enable_success(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct mock_expectation *expectation1, *expectation2;
	struct regulator_dev *rdev;
	struct panel_power_ctrl pctrl[] = {
		{
			.dev_name = "test_blic_name",
			.name = "panel_blic_pre_on",
		}, {
			.dev_name = "test_blic_name",
			.name = "panel_blic_post_on",
		}
	};

	panel->nr_blic_dev = 1;
	blic_dev->ops = &mock_panel_blic_ops;
	rdev = kunit_kzalloc(test, sizeof(*rdev), GFP_KERNEL);

	blic_dev->np->name = "test_blic_name";

	rdev->reg_data = blic_dev;

	blic_dev->enable = 0;
	blic_dev->ops->do_seq = mock_panel_blic_do_seq;
	blic_dev->ops->execute_power_ctrl = mock_panel_blic_power_ctrl_execute;

	list_add_tail(&pctrl[0].head, &panel->power_ctrl_list);
	list_add_tail(&pctrl[1].head, &panel->power_ctrl_list);

	expectation1 = Times(1, KUNIT_EXPECT_CALL(mock_panel_blic_do_seq(any(test), any(test))));
	expectation1->action = int_return(test, 0);

	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_blic_power_ctrl_execute(any(test), any(test))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_TRUE(test, panel_blic_regulator_enable(rdev) == 0);
	KUNIT_EXPECT_EQ(test, blic_dev->enable, (bool)1);
}

static void panel_blic_test_panel_blic_regulator_enable_should_return_err_with_ng_argument(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct regulator_dev *rdev;

	panel->nr_blic_dev = 1;
	rdev = kunit_kzalloc(test, sizeof(*rdev), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_blic_regulator_enable(NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_blic_regulator_enable(rdev), -EINVAL);

	rdev->reg_data = blic_dev;

	KUNIT_EXPECT_EQ(test, panel_blic_regulator_enable(rdev), -EINVAL);
}

static void panel_blic_test_panel_blic_regulator_disable_success(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct mock_expectation *expectation1, *expectation2;
	struct regulator_dev *rdev;
	struct panel_power_ctrl pctrl[] = {
		{
			.dev_name = "test_blic_name",
			.name = "panel_blic_pre_off",
		}, {
			.dev_name = "test_blic_name",
			.name = "panel_blic_post_off",
		}
	};

	panel->nr_blic_dev = 1;
	rdev = kunit_kzalloc(test, sizeof(*rdev), GFP_KERNEL);
	blic_dev->ops = kunit_kzalloc(test, sizeof(*blic_dev->ops), GFP_KERNEL);

	blic_dev->np->name = "test_blic_name";

	rdev->reg_data = blic_dev;

	blic_dev->enable = 1;
	blic_dev->ops->do_seq = mock_panel_blic_do_seq;
	blic_dev->ops->execute_power_ctrl = mock_panel_blic_power_ctrl_execute;

	list_add_tail(&pctrl[0].head, &panel->power_ctrl_list);
	list_add_tail(&pctrl[1].head, &panel->power_ctrl_list);

	expectation1 = Times(1, KUNIT_EXPECT_CALL(mock_panel_blic_do_seq(any(test), any(test))));
	expectation1->action = int_return(test, 0);

	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_blic_power_ctrl_execute(any(test), any(test))));
	expectation2->action = int_return(test, 0);

	KUNIT_EXPECT_TRUE(test, panel_blic_regulator_disable(rdev) == 0);
	KUNIT_EXPECT_EQ(test, blic_dev->enable, (bool)0);
}

static void panel_blic_test_panel_blic_regulator_disable_should_return_err_with_ng_argument(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct regulator_dev *rdev;

	panel->nr_blic_dev = 1;
	rdev = kunit_kzalloc(test, sizeof(*rdev), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_blic_regulator_disable(NULL), -EINVAL);

	rdev->reg_data = NULL;

	KUNIT_EXPECT_EQ(test, panel_blic_regulator_disable(rdev), -EINVAL);

	rdev->reg_data = blic_dev;

	KUNIT_EXPECT_EQ(test, panel_blic_regulator_disable(rdev), -EINVAL);
}

static void panel_blic_test_panel_blic_find_regulator_desc_should_return_err_with_ng_argument(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, !panel_blic_find_regulator_desc(NULL));
	KUNIT_EXPECT_TRUE(test, !panel_blic_find_regulator_desc("abnormal_input"));
}

static int pctrl_execute(struct panel_power_ctrl *pctrl)
{
	return 0;
}

struct panel_power_ctrl_funcs pctrl_funcs = {
	.execute = pctrl_execute,
};

static void panel_blic_test_panel_blic_power_ctrl_exists_fail_with_invalid_args(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;

	KUNIT_EXPECT_FALSE(test, panel_blic_power_ctrl_exists(NULL, "test_pctrl_name"));
	KUNIT_EXPECT_FALSE(test, panel_blic_power_ctrl_exists(blic_dev, NULL));
}

static void panel_blic_test_panel_blic_power_ctrl_exists_fail_with_not_initialized_of_node(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct panel_power_ctrl pctrl = {
		.dev_name = "test_blic_name",
		.name = "test_pctrl_name",
		.funcs = &pctrl_funcs,
	};
	struct panel_power_ctrl_action pact = {
		.type = PANEL_POWER_CTRL_ACTION_NONE,
		.name = "test_act_name",
		.value = 1,
	};

	INIT_LIST_HEAD(&pctrl.action_list);

	list_add_tail(&pact.head, &pctrl.action_list);
	list_add_tail(&pctrl.head, &panel->power_ctrl_list);

	blic_dev->np = NULL;
	KUNIT_EXPECT_FALSE(test, panel_blic_power_ctrl_exists(blic_dev, "test_pctrl_name"));
}

static void panel_blic_test_panel_blic_power_ctrl_exists_fail_with_name_mismatch(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct panel_power_ctrl pctrl = {
		.dev_name = "test_blic_name",
		.name = "test_pctrl_name",
		.funcs = &pctrl_funcs,
	};
	struct panel_power_ctrl_action pact = {
		.type = PANEL_POWER_CTRL_ACTION_NONE,
		.name = "test_act_name",
		.value = 1,
	};

	INIT_LIST_HEAD(&pctrl.action_list);

	list_add_tail(&pact.head, &pctrl.action_list);
	list_add_tail(&pctrl.head, &panel->power_ctrl_list);

	KUNIT_EXPECT_FALSE(test, panel_blic_power_ctrl_exists(blic_dev, "wrong_pctrl_name"));

	blic_dev->np->name = "test_wrong_blic_name";
	KUNIT_EXPECT_FALSE(test, panel_blic_power_ctrl_exists(blic_dev, "test_pctrl_name"));
}

static void panel_blic_test_panel_blic_power_ctrl_exists_success(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct panel_power_ctrl pctrl = {
		.dev_name = "test_blic_name",
		.name = "test_pctrl_name",
		.funcs = &pctrl_funcs,
	};
	struct panel_power_ctrl_action pact = {
		.type = PANEL_POWER_CTRL_ACTION_NONE,
		.name = "test_act_name",
		.value = 1,
	};

	INIT_LIST_HEAD(&pctrl.action_list);

	list_add_tail(&pact.head, &pctrl.action_list);
	list_add_tail(&pctrl.head, &panel->power_ctrl_list);

	KUNIT_EXPECT_TRUE(test, panel_blic_power_ctrl_exists(blic_dev, "test_pctrl_name"));
}

static void panel_blic_test_panel_blic_power_ctrl_execute_fail_with_invalid_args(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;

	KUNIT_EXPECT_EQ(test, panel_blic_power_ctrl_execute(NULL, "test_pctrl_name"), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_blic_power_ctrl_execute(blic_dev, NULL), -EINVAL);
}

static void panel_blic_test_panel_blic_power_ctrl_execute_fail_with_not_initialized_of_node(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct panel_power_ctrl pctrl = {
		.dev_name = "test_blic_name",
		.name = "test_pctrl_name",
		.funcs = &pctrl_funcs,
	};
	struct panel_power_ctrl_action pact = {
		.type = PANEL_POWER_CTRL_ACTION_NONE,
		.name = "test_act_name",
		.value = 1,
	};

	INIT_LIST_HEAD(&pctrl.action_list);

	list_add_tail(&pact.head, &pctrl.action_list);
	list_add_tail(&pctrl.head, &panel->power_ctrl_list);

	blic_dev->np = NULL;

	KUNIT_EXPECT_EQ(test, panel_blic_power_ctrl_execute(blic_dev, "test_pctrl_name"), -ENODEV);
}

static void panel_blic_test_panel_blic_power_ctrl_execute_fail_with_name_mismatch(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct panel_power_ctrl pctrl = {
		.dev_name = "test_blic_name",
		.name = "test_pctrl_name",
		.funcs = &pctrl_funcs,
	};
	struct panel_power_ctrl_action pact = {
		.type = PANEL_POWER_CTRL_ACTION_NONE,
		.name = "test_act_name",
		.value = 1,
	};

	INIT_LIST_HEAD(&pctrl.action_list);

	list_add_tail(&pact.head, &pctrl.action_list);
	list_add_tail(&pctrl.head, &panel->power_ctrl_list);

	KUNIT_EXPECT_EQ(test, panel_blic_power_ctrl_execute(blic_dev, "wrong_pctrl_name"), -ENODATA);

	blic_dev->np->name = "test_wrong_blic_name";
	KUNIT_EXPECT_EQ(test, panel_blic_power_ctrl_execute(blic_dev, "test_pctrl_name"), -ENODATA);
}

static void panel_blic_test_panel_blic_power_ctrl_execute_success(struct kunit *test)
{
	struct panel_blic_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_blic_dev *blic_dev = ctx->blic_dev;
	struct panel_power_ctrl pctrl = {
		.dev_name = "test_blic_name",
		.name = "test_pctrl_name",
		.funcs = &pctrl_funcs,
	};
	struct panel_power_ctrl_action pact = {
		.type = PANEL_POWER_CTRL_ACTION_NONE,
		.name = "test_act_name",
		.value = 1,
	};

	INIT_LIST_HEAD(&pctrl.action_list);

	list_add_tail(&pact.head, &pctrl.action_list);
	list_add_tail(&pctrl.head, &panel->power_ctrl_list);

	KUNIT_EXPECT_EQ(test, panel_blic_power_ctrl_execute(blic_dev, "test_pctrl_name"), 0);
}


#else
/* NOTE: On device TC */
static void panel_blic_test_foo(struct kunit *test)
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


/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_blic_test_init(struct kunit *test)
{
	struct panel_blic_test *ctx;
	struct panel_device *panel;
	struct panel_blic_dev *blic_dev;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	blic_dev = panel->blic_dev;
	blic_dev->np = kunit_kzalloc(test, sizeof(*blic_dev->np), GFP_KERNEL);
	blic_dev->np->name = "test_blic_name";

	INIT_LIST_HEAD(&panel->power_ctrl_list);
	INIT_LIST_HEAD(&panel->panel_lut_list);
	INIT_LIST_HEAD(&panel->prop_list);

	ctx->panel = panel;
	ctx->blic_dev = blic_dev;
	ctx->nr_blic_dev = 1;

	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_blic_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_blic_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#ifdef CONFIG_UML
	/* NOTE: UML TC */
	KUNIT_CASE(panel_blic_test_test),

	KUNIT_CASE(panel_blic_test_panel_drv_blic_to_panel_drv_should_return_null_with_null),

	KUNIT_CASE(panel_blic_test_panel_drv_blic_probe_should_return_err_with_abnormal_argument),
	KUNIT_CASE(panel_blic_test_panel_drv_blic_get_blic_regulator_success_with_blic_or_no_blic),
#ifdef CONFIG_USDM_BLIC_I2C
	KUNIT_CASE(panel_blic_test_panel_blic_find_i2c_drv_success),
	KUNIT_CASE(panel_blic_test_panel_blic_find_i2c_drv_should_return_null_with_no_i2c),
	KUNIT_CASE(panel_blic_test_panel_blic_find_i2c_drv_should_return_null_with_wrong_id),
#endif

	KUNIT_CASE(panel_blic_test_panel_blic_regulator_enable_success),
	KUNIT_CASE(panel_blic_test_panel_blic_regulator_enable_should_return_err_with_ng_argument),

	KUNIT_CASE(panel_blic_test_panel_blic_regulator_disable_success),
	KUNIT_CASE(panel_blic_test_panel_blic_regulator_disable_should_return_err_with_ng_argument),

	KUNIT_CASE(panel_blic_test_panel_blic_find_regulator_desc_should_return_err_with_ng_argument),

	KUNIT_CASE(panel_blic_test_panel_blic_power_ctrl_exists_fail_with_invalid_args),
	KUNIT_CASE(panel_blic_test_panel_blic_power_ctrl_exists_fail_with_not_initialized_of_node),
	KUNIT_CASE(panel_blic_test_panel_blic_power_ctrl_exists_fail_with_name_mismatch),
	KUNIT_CASE(panel_blic_test_panel_blic_power_ctrl_exists_success),
	KUNIT_CASE(panel_blic_test_panel_blic_power_ctrl_execute_fail_with_invalid_args),
	KUNIT_CASE(panel_blic_test_panel_blic_power_ctrl_execute_fail_with_not_initialized_of_node),
	KUNIT_CASE(panel_blic_test_panel_blic_power_ctrl_execute_fail_with_name_mismatch),
	KUNIT_CASE(panel_blic_test_panel_blic_power_ctrl_execute_success),

#else
	/* NOTE: On device TC */
	KUNIT_CASE(panel_blic_test_foo),
#endif
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
static struct kunit_suite panel_blic_test_module = {
	.name = "panel_blic_test",
	.init = panel_blic_test_init,
	.exit = panel_blic_test_exit,
	.test_cases = panel_blic_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_blic_test_module);

MODULE_LICENSE("GPL v2");
