// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "panel_freq_hop.h"
#include "dev_ril_header.h"

#define TEST_PANEL_OSC_FREQ_0 (96500)
#define TEST_PANEL_OSC_FREQ_1 (94500)

#define TEST_PANEL_DSI_FREQ_0 (1328000)
#define TEST_PANEL_DSI_FREQ_1 (1362000)
#define TEST_PANEL_DSI_FREQ_2 (1368000)

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

struct panel_freq_hop_test {
	struct panel_device *panel;
	struct panel_freq_hop *freq_hop;
};

extern struct class *lcd_class;
extern struct freq_hop_elem *search_freq_hop_element(struct panel_freq_hop *freq_hop,
		unsigned int band, unsigned int channel);

static void panel_freq_hop_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_freq_hop_test_search_freq_hop_element_success(struct kunit *test)
{
	struct panel_freq_hop_test *ctx = test->priv;
	struct panel_freq_hop *freq_hop = ctx->freq_hop;
	struct freq_hop_elem freq_hop_elems[] = {
		[0] = DEFINE_FREQ_RANGE(LB41, 39650, 40089, TEST_PANEL_DSI_FREQ_2, TEST_PANEL_OSC_FREQ_0),
		[1] = DEFINE_FREQ_RANGE(LB41, 40090, 40477, TEST_PANEL_DSI_FREQ_0, TEST_PANEL_OSC_FREQ_0),
		[2] = DEFINE_FREQ_RANGE(LB41, 40478, 40589, TEST_PANEL_DSI_FREQ_1, TEST_PANEL_OSC_FREQ_0),
		[3] = DEFINE_FREQ_RANGE(LB41, 40590, 41589, TEST_PANEL_DSI_FREQ_2, TEST_PANEL_OSC_FREQ_0),
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(freq_hop_elems); i++)
		list_add_tail(&freq_hop_elems[i].list, &freq_hop->head);

	KUNIT_EXPECT_PTR_EQ(test, search_freq_hop_element(freq_hop, LB41, 39650), &freq_hop_elems[0]);
	KUNIT_EXPECT_PTR_EQ(test, search_freq_hop_element(freq_hop, LB41, 40089), &freq_hop_elems[0]);
	KUNIT_EXPECT_PTR_EQ(test, search_freq_hop_element(freq_hop, LB41, 40090), &freq_hop_elems[1]);
	KUNIT_EXPECT_PTR_EQ(test, search_freq_hop_element(freq_hop, LB41, 40477), &freq_hop_elems[1]);
	KUNIT_EXPECT_PTR_EQ(test, search_freq_hop_element(freq_hop, LB41, 40478), &freq_hop_elems[2]);
	KUNIT_EXPECT_PTR_EQ(test, search_freq_hop_element(freq_hop, LB41, 40589), &freq_hop_elems[2]);
	KUNIT_EXPECT_PTR_EQ(test, search_freq_hop_element(freq_hop, LB41, 40590), &freq_hop_elems[3]);
	KUNIT_EXPECT_PTR_EQ(test, search_freq_hop_element(freq_hop, LB41, 41589), &freq_hop_elems[3]);
}

static void panel_freq_hop_test_search_freq_hop_element_should_return_null_if_not_found(struct kunit *test)
{
	struct panel_freq_hop_test *ctx = test->priv;
	struct panel_freq_hop *freq_hop = ctx->freq_hop;
	struct freq_hop_elem freq_hop_elems[] = {
		[0] = DEFINE_FREQ_RANGE(LB41, 39650, 40089, TEST_PANEL_DSI_FREQ_2, TEST_PANEL_OSC_FREQ_0),
		[1] = DEFINE_FREQ_RANGE(LB41, 40090, 40477, TEST_PANEL_DSI_FREQ_0, TEST_PANEL_OSC_FREQ_0),
		[2] = DEFINE_FREQ_RANGE(LB41, 40478, 40589, TEST_PANEL_DSI_FREQ_1, TEST_PANEL_OSC_FREQ_0),
		[3] = DEFINE_FREQ_RANGE(LB41, 40590, 41589, TEST_PANEL_DSI_FREQ_2, TEST_PANEL_OSC_FREQ_0),
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(freq_hop_elems); i++)
		list_add_tail(&freq_hop_elems[i].list, &freq_hop->head);

	KUNIT_EXPECT_TRUE(test, !search_freq_hop_element(freq_hop, LB41, 41589 + 1));
	KUNIT_EXPECT_TRUE(test, !search_freq_hop_element(freq_hop, LB42, 39650));
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_freq_hop_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_freq_hop *freq_hop;
	struct panel_freq_hop_test *ctx = kunit_kzalloc(test,
			sizeof(*ctx), GFP_KERNEL);

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel->of_node_name = "test_panel_name";
	panel->dev = device_create(lcd_class,
			NULL, 0, panel, PANEL_DRV_NAME);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel->dev);

	panel_mutex_init(panel, &panel->io_lock);
	panel_mutex_init(panel, &panel->op_lock);

	freq_hop = &panel->freq_hop;
	INIT_LIST_HEAD(&freq_hop->head);

	ctx->panel = panel;
	ctx->freq_hop = freq_hop;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_freq_hop_test_exit(struct kunit *test)
{
	struct panel_freq_hop_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	device_unregister(panel->dev);
	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_freq_hop_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_freq_hop_test_test),
	KUNIT_CASE(panel_freq_hop_test_search_freq_hop_element_success),
	KUNIT_CASE(panel_freq_hop_test_search_freq_hop_element_should_return_null_if_not_found),
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
static struct kunit_suite panel_freq_hop_test_module = {
	.name = "panel_freq_hop_test",
	.init = panel_freq_hop_test_init,
	.exit = panel_freq_hop_test_exit,
	.test_cases = panel_freq_hop_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_freq_hop_test_module);

MODULE_LICENSE("GPL v2");
