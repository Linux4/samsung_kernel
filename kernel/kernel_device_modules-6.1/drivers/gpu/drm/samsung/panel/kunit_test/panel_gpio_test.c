// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <media/v4l2-subdev.h>
#include "panel_drv.h"
#include "panel_gpio.h"
#include "mock_panel_drv_funcs.h"

struct panel_gpio_test {
	struct panel_device *panel;
	struct panel_gpio *gpio;
	struct panel_gpio_funcs *orig_funcs;
	struct panel_gpio_funcs *test_funcs;
};

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
static void panel_gpio_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_gpio_test_is_valid_return_false_with_invalid_gpio_number(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;

	gpio->num = -1;
	KUNIT_EXPECT_FALSE(test, panel_gpio_helper_is_valid(gpio));

	gpio->num = 4096;
	KUNIT_EXPECT_FALSE(test, panel_gpio_helper_is_valid(gpio));
}

static void panel_gpio_test_is_valid_return_false_with_invalid_gpio_name(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;

	gpio->name = NULL;
	KUNIT_EXPECT_FALSE(test, panel_gpio_helper_is_valid(gpio));
}

static void panel_gpio_test_is_valid_return_true_with_valid_gpio(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, true);

	KUNIT_EXPECT_TRUE(test, panel_gpio_helper_is_valid(gpio));
}

static void panel_gpio_test_panel_gpio_helper_get_num_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_num(gpio), -EINVAL);
}

static void panel_gpio_test_panel_gpio_helper_get_num_success(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, true);

	gpio->num = 128;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_num(gpio), 128);
}

static void panel_gpio_test_panel_gpio_helper_get_name_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);

	gpio->name = "test-gpio";
	KUNIT_EXPECT_TRUE(test, !panel_gpio_helper_get_name(gpio));
}

static void panel_gpio_test_panel_gpio_helper_get_name_success(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, true);

	gpio->name = "test-gpio";
	KUNIT_EXPECT_PTR_EQ(test, panel_gpio_helper_get_name(gpio), gpio->name);
}

static void panel_gpio_test_panel_gpio_helper_set_value_fail_with_invalid_gpio(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_set_value(gpio, 1), -EINVAL);
}

static void panel_gpio_test_panel_gpio_helper_get_value_fail_with_invalid_gpio(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_value(gpio), -EINVAL);
}

static void panel_gpio_test_panel_gpio_helper_get_state_fail_with_invalid_gpio(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_state(gpio), -EINVAL);
}

static void panel_gpio_test_panel_gpio_helper_get_state_with_gpio_is_high(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation1, *expectation2;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation1->action = bool_return(test, true);

	gpio->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gpio))));
	expectation2->action = int_return(test, 1);

	/* ACTIVE_LOW(NORMAL_HIGH) and GPIO:HIGH */
	gpio->active_low = true;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_state(gpio), PANEL_GPIO_NORMAL_STATE);

	/* ACTIVE_HIGH(NORMAL_LOW) and GPIO:LOW */
	gpio->active_low = false;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_state(gpio), PANEL_GPIO_ABNORMAL_STATE);
}

static void panel_gpio_test_panel_gpio_helper_get_state_with_gpio_is_low(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation1, *expectation2;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation1 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation1->action = bool_return(test, true);

	gpio->funcs->get_value = mock_panel_gpio_get_value;
	expectation2 = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_get_value(ptr_eq(test, gpio))));
	expectation2->action = int_return(test, 0);

	/* ACTIVE_LOW(NORMAL_HIGH) and GPIO:LOW */
	gpio->active_low = true;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_state(gpio), PANEL_GPIO_ABNORMAL_STATE);

	/* ACTIVE_HIGH(NORMAL_LOW) and GPIO:HIGH */
	gpio->active_low = false;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_state(gpio), PANEL_GPIO_NORMAL_STATE);
}

static void panel_gpio_test_panel_gpio_helper_get_irq_num_fail_with_invalid_gpio(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_irq_num(gpio), -EINVAL);
}

static void panel_gpio_test_panel_gpio_helper_get_irq_num_success(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, true);

	gpio->irq = 1;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_irq_num(gpio), 1);
}

static void panel_gpio_test_panel_gpio_helper_get_irq_type_fail_with_invalid_gpio(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_irq_type(gpio), -EINVAL);
}

static void panel_gpio_test_panel_gpio_helper_get_irq_type_success(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, true);

	gpio->irq_type = 1;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_get_irq_type(gpio), 1);
}

static void panel_gpio_test_panel_gpio_helper_is_irq_valid_should_return_false_with_invalid_gpio(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_FALSE(test, panel_gpio_helper_is_irq_valid(gpio));
}

static void panel_gpio_test_panel_gpio_helper_is_irq_valid_should_return_false_with_invalid_type(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, true);

	gpio->irq_type = -1;
	KUNIT_EXPECT_FALSE(test, panel_gpio_helper_is_irq_valid(gpio));
}

static void panel_gpio_test_panel_gpio_helper_is_irq_valid_should_return_true_with_valid_irq_type(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_valid = mock_panel_gpio_is_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, true);

	gpio->irq_type = 1;
	KUNIT_EXPECT_TRUE(test, panel_gpio_helper_is_irq_valid(gpio));
}

static void panel_gpio_test_panel_gpio_helper_enable_irq_fail_with_invalid_irq(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_irq_valid = mock_panel_gpio_is_irq_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_irq_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_EQ(test, panel_gpio_helper_enable_irq(gpio), -EINVAL);
}

static void panel_gpio_test_panel_gpio_helper_enable_irq_success(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_irq_valid = mock_panel_gpio_is_irq_valid;
	expectation = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_is_irq_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, true);

	gpio->irq_enable = false;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_enable_irq(gpio), 0);
	KUNIT_EXPECT_TRUE(test, panel_gpio_helper_is_irq_enabled(gpio));
}

static void panel_gpio_test_panel_gpio_helper_disable_irq_fail_with_invalid_irq(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_irq_valid = mock_panel_gpio_is_irq_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_irq_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_EQ(test, panel_gpio_helper_disable_irq(gpio), -EINVAL);
}

static void panel_gpio_test_panel_gpio_helper_disable_irq_success(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_irq_valid = mock_panel_gpio_is_irq_valid;
	expectation = Times(2, KUNIT_EXPECT_CALL(mock_panel_gpio_is_irq_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, true);

	gpio->irq_enable = true;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_disable_irq(gpio), 0);
	KUNIT_EXPECT_FALSE(test, panel_gpio_helper_is_irq_enabled(gpio));
}

static void panel_gpio_test_unbalanced_enable_disable_irq(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_irq_valid = mock_panel_gpio_is_irq_valid;
	expectation = Times(4, KUNIT_EXPECT_CALL(mock_panel_gpio_is_irq_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, true);

	/* precondition: irq enabled */
	gpio->irq_enable = true;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_enable_irq(gpio), 0);
	KUNIT_EXPECT_TRUE(test, panel_gpio_helper_is_irq_enabled(gpio));

	/* precondition: irq enabled */
	gpio->irq_enable = false;
	KUNIT_EXPECT_EQ(test, panel_gpio_helper_disable_irq(gpio), 0);
	KUNIT_EXPECT_FALSE(test, panel_gpio_helper_is_irq_enabled(gpio));
}

static void panel_gpio_test_panel_gpio_helper_clear_irq_pending_bit_fail_with_invalid_irq(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_irq_valid = mock_panel_gpio_is_irq_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_irq_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_EQ(test, panel_gpio_helper_clear_irq_pending_bit(gpio), -EINVAL);
}

static void panel_gpio_test_panel_gpio_helper_clear_irq_pending_bit_fail_with_invalid_irq_number(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation1, *expectation2;

	gpio->funcs->is_irq_valid = mock_panel_gpio_is_irq_valid;
	expectation1 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_irq_valid(ptr_eq(test, gpio))));
	expectation1->action = bool_return(test, true);

	gpio->funcs->get_irq_num = mock_panel_gpio_get_irq_num;
	expectation2 = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_get_irq_num(ptr_eq(test, gpio))));
	expectation2->action = int_return(test, 123456789);

	KUNIT_EXPECT_EQ(test, panel_gpio_helper_clear_irq_pending_bit(gpio), -EINVAL);
}

static void panel_gpio_test_panel_gpio_helper_devm_request_irq_fail_with_invalid_irq(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_gpio *gpio = ctx->gpio;
	struct mock_expectation *expectation;

	gpio->funcs->is_irq_valid = mock_panel_gpio_is_irq_valid;
	expectation = Times(1, KUNIT_EXPECT_CALL(mock_panel_gpio_is_irq_valid(ptr_eq(test, gpio))));
	expectation->action = bool_return(test, false);

	KUNIT_EXPECT_EQ(test, panel_gpio_helper_devm_request_irq(gpio,
				panel->dev, NULL, "panel0:disp-det",
				&panel->work[PANEL_WORK_DISP_DET]), -EINVAL);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_gpio_test_init(struct kunit *test)
{
	struct panel_gpio_test *ctx;
	struct panel_device *panel;
	struct panel_gpio *gpio;

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

	INIT_LIST_HEAD(&panel->gpio_list);
	INIT_LIST_HEAD(&panel->power_ctrl_list);
	INIT_LIST_HEAD(&panel->regulator_list);
	INIT_LIST_HEAD(&panel->prop_list);

	gpio = panel_gpio_create();

	/* replace gpio->funcs with duplicated test_funcs */
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, gpio->funcs);
	ctx->test_funcs = kunit_kzalloc(test, sizeof(*ctx->test_funcs), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx->test_funcs);
	memcpy(ctx->test_funcs, gpio->funcs, sizeof(*ctx->test_funcs));
	ctx->orig_funcs = gpio->funcs;
	gpio->funcs = ctx->test_funcs;

	/* initialize 'disp-det' gpio */
	gpio->name = "disp-det";
	gpio->num = 250;
	gpio->active_low = true;
	gpio->dir = 0;

	ctx->gpio = gpio;

	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_gpio_test_exit(struct kunit *test)
{
	struct panel_gpio_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_gpio *gpio = ctx->gpio;

	panel_gpio_destroy(gpio);
	device_unregister(panel->dev);
	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_gpio_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_gpio_test_test),

	KUNIT_CASE(panel_gpio_test_is_valid_return_false_with_invalid_gpio_number),
	KUNIT_CASE(panel_gpio_test_is_valid_return_false_with_invalid_gpio_name),
	KUNIT_CASE(panel_gpio_test_is_valid_return_true_with_valid_gpio),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_num_fail_with_invalid_arg),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_num_success),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_name_fail_with_invalid_arg),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_name_success),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_set_value_fail_with_invalid_gpio),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_value_fail_with_invalid_gpio),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_state_fail_with_invalid_gpio),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_state_with_gpio_is_high),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_state_with_gpio_is_low),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_irq_num_fail_with_invalid_gpio),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_irq_num_success),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_irq_type_fail_with_invalid_gpio),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_get_irq_type_success),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_is_irq_valid_should_return_false_with_invalid_gpio),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_is_irq_valid_should_return_false_with_invalid_type),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_is_irq_valid_should_return_true_with_valid_irq_type),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_enable_irq_fail_with_invalid_irq),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_enable_irq_success),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_disable_irq_fail_with_invalid_irq),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_disable_irq_success),

	KUNIT_CASE(panel_gpio_test_unbalanced_enable_disable_irq),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_clear_irq_pending_bit_fail_with_invalid_irq),
	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_clear_irq_pending_bit_fail_with_invalid_irq_number),

	KUNIT_CASE(panel_gpio_test_panel_gpio_helper_devm_request_irq_fail_with_invalid_irq),
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
static struct kunit_suite panel_gpio_test_module = {
	.name = "panel_gpio_test",
	.init = panel_gpio_test_init,
	.exit = panel_gpio_test_exit,
	.test_cases = panel_gpio_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_gpio_test_module);

MODULE_LICENSE("GPL v2");
