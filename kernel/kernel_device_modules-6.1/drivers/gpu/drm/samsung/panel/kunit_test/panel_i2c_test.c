// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>

#include "panel_drv.h"
#include "panel_i2c.h"

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


extern int panel_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
extern int panel_i2c_tx(struct panel_i2c_dev *i2c_dev, u8 *arr, u32 arr_len);
extern int panel_i2c_rx(struct panel_i2c_dev *i2c_dev, u8 *arr, u32 arr_len);


DEFINE_FUNCTION_MOCK(mock_panel_i2c_transfer, RETURNS(int), PARAMS(struct i2c_adapter *, struct i2c_msg *, int));

static struct i2c_drv_ops mock_panel_i2c_drv_ops = {
	.tx = panel_i2c_tx,
	.rx = panel_i2c_rx,
	.transfer = mock_panel_i2c_transfer,
};

static struct i2c_device_id panel_i2c_id[] = {
	{"i2c", 0},
	{},
};

#ifdef CONFIG_UML

/* NOTE: UML TC */
static void panel_i2c_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_i2c_test_panel_i2c_drv_init_should_return_err_with_abnormal_argument(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_i2c_drv_init(NULL), -EINVAL);
}

static void panel_i2c_test_panel_i2c_probe_should_return_err_with_abnormal_argument(struct kunit *test)
{
	struct i2c_client *client;

	client = kunit_kzalloc(test, sizeof(*client), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, panel_i2c_probe(NULL, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_i2c_probe(client, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_i2c_probe(NULL, panel_i2c_id), -EINVAL);
}

static void panel_i2c_test_panel_i2c_probe_should_return_err_with_I2C_num_exceed(struct kunit *test)
{
	struct i2c_client *client;
	struct panel_device *panel;

	client = kunit_kzalloc(test, sizeof(*client), GFP_KERNEL);
	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	panel->nr_i2c_dev = PANEL_I2C_MAX;

	panel_i2c_id[0].driver_data = (kernel_ulong_t)panel;

	KUNIT_EXPECT_EQ(test, panel_i2c_probe(client, panel_i2c_id), -EINVAL);
}

static void panel_i2c_test_panel_i2c_tx_should_return_err_with_abnormal_argument(struct kunit *test)
{
	struct i2c_client *client;
	struct panel_i2c_dev *i2c_dev;
	struct i2c_adapter *adapter;
	struct panel_device *panel = test->priv;

	u8 data[10] = {0, };

	client = kunit_kzalloc(test, sizeof(*client), GFP_KERNEL);
	i2c_dev = kunit_kzalloc(test, sizeof(*i2c_dev), GFP_KERNEL);
	adapter = kunit_kzalloc(test, sizeof(*adapter), GFP_KERNEL);

	client->adapter = adapter;

	i2c_dev->client = client;
	i2c_dev->addr_len = 1;
	i2c_dev->data_len = 1;
	panel_mutex_init(panel, &i2c_dev->lock);

	KUNIT_EXPECT_EQ(test, panel_i2c_tx(NULL, data, 2), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_i2c_tx(i2c_dev, NULL, 2), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_i2c_tx(i2c_dev, data, 0), -EINVAL);
}

static void panel_i2c_test_panel_i2c_tx_success(struct kunit *test)
{
	struct i2c_client *client;
	struct panel_i2c_dev *i2c_dev;
	struct i2c_adapter *adapter;
	struct mock_expectation *expectation1;
	struct panel_device *panel = test->priv;

	u8 data[10] = {0, };

	client = kunit_kzalloc(test, sizeof(*client), GFP_KERNEL);
	i2c_dev = kunit_kzalloc(test, sizeof(*i2c_dev), GFP_KERNEL);
	adapter = kunit_kzalloc(test, sizeof(*adapter), GFP_KERNEL);

	client->adapter = adapter;

	i2c_dev->client = client;
	i2c_dev->addr_len = 1;
	i2c_dev->data_len = 1;
	panel_mutex_init(panel, &i2c_dev->lock);

	i2c_dev->ops = &mock_panel_i2c_drv_ops;

	expectation1 = Times(1, KUNIT_EXPECT_CALL(mock_panel_i2c_transfer(ptr_eq(test, i2c_dev->client->adapter),
				any(test), any(test))));
	expectation1->action = int_return(test, 0);

	KUNIT_EXPECT_TRUE(test, panel_i2c_tx(i2c_dev, data, 2) == 0);
}

static void panel_i2c_test_panel_i2c_rx_should_return_err_with_abnormal_argument(struct kunit *test)
{
	struct i2c_client *client;
	struct panel_i2c_dev *i2c_dev;
	struct i2c_adapter *adapter;
	struct panel_device *panel = test->priv;

	u8 data[10] = {0, };

	client = kunit_kzalloc(test, sizeof(*client), GFP_KERNEL);
	i2c_dev = kunit_kzalloc(test, sizeof(*i2c_dev), GFP_KERNEL);
	adapter = kunit_kzalloc(test, sizeof(*adapter), GFP_KERNEL);

	client->adapter = adapter;

	i2c_dev->client = client;
	i2c_dev->addr_len = 1;
	i2c_dev->data_len = 1;
	panel_mutex_init(panel, &i2c_dev->lock);

	i2c_dev->ops = &mock_panel_i2c_drv_ops;

	KUNIT_EXPECT_EQ(test, panel_i2c_rx(NULL, data, 2), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_i2c_rx(i2c_dev, NULL, 2), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_i2c_rx(i2c_dev, data, 0), -EINVAL);
}

static void panel_i2c_test_panel_i2c_rx_success(struct kunit *test)
{
	struct i2c_client *client;
	struct panel_i2c_dev *i2c_dev;
	struct i2c_adapter *adapter;
	struct mock_expectation *expectation1;
	struct panel_device *panel = test->priv;

	u8 data[10] = {0, };

	client = kunit_kzalloc(test, sizeof(*client), GFP_KERNEL);
	i2c_dev = kunit_kzalloc(test, sizeof(*i2c_dev), GFP_KERNEL);
	adapter = kunit_kzalloc(test, sizeof(*adapter), GFP_KERNEL);

	client->adapter = adapter;

	i2c_dev->client = client;
	i2c_dev->addr_len = 1;
	i2c_dev->data_len = 1;
	panel_mutex_init(panel, &i2c_dev->lock);

	i2c_dev->ops = &mock_panel_i2c_drv_ops;

	expectation1 = Times(1, KUNIT_EXPECT_CALL(mock_panel_i2c_transfer(ptr_eq(test, i2c_dev->client->adapter),
				any(test), any(test))));
	expectation1->action = int_return(test, 0);

	KUNIT_EXPECT_TRUE(test, panel_i2c_rx(i2c_dev, data, 2) == 0);
}

#else
/* NOTE: On device TC */
static void panel_i2c_test_foo(struct kunit *test)
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
static int panel_i2c_test_init(struct kunit *test)
{
	struct panel_device *panel = panel_device_create();

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);
	test->priv = panel;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_i2c_test_exit(struct kunit *test)
{
	struct panel_device *panel = test->priv;

	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_i2c_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#ifdef CONFIG_UML
	/* NOTE: UML TC */
	KUNIT_CASE(panel_i2c_test_test),

	/* panel_i2c_drv_init */
	KUNIT_CASE(panel_i2c_test_panel_i2c_drv_init_should_return_err_with_abnormal_argument),

	/* panel_i2c_probe */
	KUNIT_CASE(panel_i2c_test_panel_i2c_probe_should_return_err_with_abnormal_argument),
	KUNIT_CASE(panel_i2c_test_panel_i2c_probe_should_return_err_with_I2C_num_exceed),

	/* panel_i2c_tx */
	KUNIT_CASE(panel_i2c_test_panel_i2c_tx_should_return_err_with_abnormal_argument),
	KUNIT_CASE(panel_i2c_test_panel_i2c_tx_success),

	KUNIT_CASE(panel_i2c_test_panel_i2c_rx_should_return_err_with_abnormal_argument),
	KUNIT_CASE(panel_i2c_test_panel_i2c_rx_success),

#else
	/* NOTE: On device TC */
	KUNIT_CASE(panel_i2c_test_foo),
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
static struct kunit_suite panel_i2c_test_module = {
	.name = "i2c_test_test",
	.init = panel_i2c_test_init,
	.exit = panel_i2c_test_exit,
	.test_cases = panel_i2c_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_i2c_test_module);

MODULE_LICENSE("GPL v2");
