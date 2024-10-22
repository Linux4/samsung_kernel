// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel.h"
#include "panel_drv.h"
#include "panel_delay.h"

struct panel_delay_test {
	struct panel_device *panel;
	struct common_panel_info *cpi;
};

extern u32 get_frame_delay(struct panel_device *panel, struct delayinfo *delay);
extern u32 get_total_delay(struct panel_device *panel, struct delayinfo *delay);
extern u32 get_remaining_delay(struct panel_device *panel, struct delayinfo *delay,
		ktime_t start, ktime_t now);

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

static void panel_delay_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_delay_test_is_valid_delay_return_false_with_invalid_delay(struct kunit *test)
{
	struct delayinfo *delay =
		kunit_kzalloc(test, sizeof(*delay), GFP_KERNEL);

	pnobj_init(&delay->base, CMD_TYPE_NONE, "test_delay");
	KUNIT_EXPECT_FALSE(test, is_valid_delay(delay));
}

static void panel_delay_test_is_valid_delay_return_true_with_valid_delay(struct kunit *test)
{
	struct delayinfo *delay =
		kunit_kzalloc(test, sizeof(*delay), GFP_KERNEL);

	pnobj_init(&delay->base, CMD_TYPE_DELAY, "test_delay");
	KUNIT_EXPECT_TRUE(test, is_valid_delay(delay));

	pnobj_init(&delay->base, CMD_TYPE_TIMER_DELAY, "test_timer_delay");
	KUNIT_EXPECT_TRUE(test, is_valid_delay(delay));
}

static void panel_delay_test_is_valid_timer_delay_return_false_with_invalid_delay(struct kunit *test)
{
	struct delayinfo *delay =
		kunit_kzalloc(test, sizeof(*delay), GFP_KERNEL);

	pnobj_init(&delay->base, CMD_TYPE_DELAY, "test_delay");
	KUNIT_EXPECT_FALSE(test, is_valid_timer_delay(delay));

	pnobj_init(&delay->base, CMD_TYPE_TIMER_DELAY_BEGIN, "test_vsync_delay_begin");
	KUNIT_EXPECT_FALSE(test, is_valid_timer_delay(delay));
}

static void panel_delay_test_is_valid_timer_delay_return_true_with_valid_delay(struct kunit *test)
{
	struct delayinfo *delay =
		kunit_kzalloc(test, sizeof(*delay), GFP_KERNEL);

	pnobj_init(&delay->base, CMD_TYPE_TIMER_DELAY, "test_timer_delay");
	KUNIT_EXPECT_TRUE(test, is_valid_timer_delay(delay));
}

static void panel_delay_test_create_delay_success(struct kunit *test)
{
	struct delayinfo *delay;

	delay = create_delay("test_udelay", CMD_TYPE_DELAY, 100, 0, 0, false);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, delay);
	destroy_delay(delay);

	delay = create_delay("test_1_frame_delay", CMD_TYPE_DELAY, 0, 1, 0, false);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, delay);
	destroy_delay(delay);

	delay = create_delay("test_1_vsync_delay", CMD_TYPE_DELAY, 0, 0, 1, false);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, delay);
	destroy_delay(delay);

	delay = create_delay("test_timer_delay", CMD_TYPE_TIMER_DELAY, 100, 0, 0, false);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, delay);
	destroy_delay(delay);
}

static void panel_delay_test_is_valid_timer_delay_begin_return_false_with_invalid_delay_type(struct kunit *test)
{
	struct delayinfo *delay;
	struct timer_delay_begin_info *begin =
		kunit_kzalloc(test, sizeof(*begin), GFP_KERNEL);

	pnobj_init(&begin->base, CMD_TYPE_DELAY, "test_delay");
	KUNIT_EXPECT_FALSE(test, is_valid_timer_delay_begin(begin));

	pnobj_init(&begin->base, CMD_TYPE_TIMER_DELAY, "test_timer_delay");
	KUNIT_EXPECT_FALSE(test, is_valid_timer_delay_begin(begin));

	pnobj_init(&begin->base, CMD_TYPE_TIMER_DELAY_BEGIN, "test_timer_delay_begin");
	KUNIT_EXPECT_FALSE(test, is_valid_timer_delay_begin(begin));

	delay = create_delay("test_delay", CMD_TYPE_DELAY, 100, 0, 0, false);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, delay);
	begin->delay = delay;
	KUNIT_EXPECT_FALSE(test, is_valid_timer_delay_begin(begin));
	destroy_delay(delay);

	delay = create_delay("test_vsync_delay", CMD_TYPE_DELAY, 0, 1, 0, false);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, delay);
	begin->delay = delay;
	KUNIT_EXPECT_FALSE(test, is_valid_timer_delay_begin(begin));
	destroy_delay(delay);
}

static void panel_delay_test_is_valid_timer_delay_begin_return_true_with_valid_timer_delay_begin(struct kunit *test)
{
	struct delayinfo *delay;
	struct timer_delay_begin_info *begin =
		kunit_kzalloc(test, sizeof(*begin), GFP_KERNEL);

	delay = create_delay("test_timer_delay", CMD_TYPE_TIMER_DELAY, 100, 0, 0, false);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, delay);

	pnobj_init(&begin->base, CMD_TYPE_TIMER_DELAY_BEGIN, "test_timer_delay_begin");
	begin->delay = delay;
	KUNIT_EXPECT_TRUE(test, is_valid_timer_delay_begin(begin));
}

static void panel_delay_test_create_timer_delay_begin_fail_if_not_timer_delay(struct kunit *test)
{
	struct delayinfo *delay;
	struct timer_delay_begin_info *begin;

	delay = create_delay("test_delay", CMD_TYPE_DELAY, 100, 0, 0, false);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, delay);

	begin = create_timer_delay_begin("test_timer_delay_begin", delay);
	KUNIT_ASSERT_TRUE(test, !begin);
	destroy_delay(delay);
}

static void panel_delay_test_create_timer_delay_begin_success(struct kunit *test)
{
	struct delayinfo *delay;
	struct timer_delay_begin_info *begin;

	delay = create_delay("test_timer_delay", CMD_TYPE_TIMER_DELAY, 100, 0, 0, false);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, delay);

	begin = create_timer_delay_begin("test_timer_delay_begin", delay);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, begin);
	destroy_delay(delay);
	destroy_timer_delay_begin(begin);
}

static void panel_delay_test_get_frame_delay(struct kunit *test)
{
	struct panel_delay_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	DEFINE_PANEL_FRAME_DELAY(wait_1_frame, 1);
	DEFINE_PANEL_FRAME_DELAY(wait_2_frame, 2);

	panel->panel_data.props.vrr_origin_fps = 60;
	panel->panel_data.props.vrr_fps = 120;

	KUNIT_EXPECT_EQ(test, get_frame_delay(panel,
				&DLYINFO(wait_1_frame)), (u32)(USEC_PER_SEC / 60));
	KUNIT_EXPECT_EQ(test, get_frame_delay(panel,
				&DLYINFO(wait_2_frame)), (u32)(USEC_PER_SEC / 60 + USEC_PER_SEC / 120));
}

static void panel_delay_test_get_total_delay(struct kunit *test)
{
	struct panel_delay_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	DEFINE_PANEL_MDELAY(wait_1_frame_and_5msec, 5);
	DEFINE_PANEL_MDELAY(wait_2_frame_and_5msec, 5);

	DLYINFO(wait_1_frame_and_5msec).nframe = 1;
	DLYINFO(wait_2_frame_and_5msec).nframe = 2;

	panel->panel_data.props.vrr_origin_fps = 60;
	panel->panel_data.props.vrr_fps = 120;

	KUNIT_EXPECT_EQ(test, get_total_delay(panel, &DLYINFO(wait_1_frame_and_5msec)),
			(u32)(USEC_PER_SEC / 60 + 5 * MSEC_PER_SEC));
	KUNIT_EXPECT_EQ(test, get_total_delay(panel, &DLYINFO(wait_2_frame_and_5msec)),
			(u32)(USEC_PER_SEC / 60 + USEC_PER_SEC / 120 + 5 * MSEC_PER_SEC));
}

static void panel_delay_test_get_remaining_delay(struct kunit *test)
{
	struct panel_delay_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	DEFINE_PANEL_UDELAY(wait_16p7msec, 16700);

	/* invalid delay */
	KUNIT_EXPECT_EQ(test, get_remaining_delay(panel,
				NULL, ktime_set(0, 0), ktime_set(0, 16700000)), 0U);

	/* elapsed time is greater or equal to delay */
	KUNIT_EXPECT_EQ(test, get_remaining_delay(panel,
				&DLYINFO(wait_16p7msec), ktime_set(0, 0), ktime_set(0, 16700000)), 0U);
	KUNIT_EXPECT_EQ(test, get_remaining_delay(panel,
				&DLYINFO(wait_16p7msec), ktime_set(0, 0), ktime_set(0, 16800000)), 0U);

	/* elapsed time is less than delay */
	KUNIT_EXPECT_EQ(test, get_remaining_delay(panel,
				&DLYINFO(wait_16p7msec), ktime_set(0, 0), ktime_set(0, 16600000)), 100U);
	KUNIT_EXPECT_EQ(test, get_remaining_delay(panel,
				&DLYINFO(wait_16p7msec), ktime_set(0, 0), ktime_set(0, 16699999)), 1U);
	KUNIT_EXPECT_EQ(test, get_remaining_delay(panel,
				&DLYINFO(wait_16p7msec), ktime_set(10000000000, 0), ktime_set(10000000000, 16600000)), 0U);
	KUNIT_EXPECT_EQ(test, get_remaining_delay(panel,
				&DLYINFO(wait_16p7msec), ktime_set(0, 0), ktime_set(10000000000, 16600000)), 0U);

	/* round to 6 decimal place(i.e. USEC) */
	KUNIT_EXPECT_EQ(test, get_remaining_delay(panel,
				&DLYINFO(wait_16p7msec), ktime_set(0, 0), ktime_set(0, 16600001)), 100U);
	KUNIT_EXPECT_EQ(test, get_remaining_delay(panel,
				&DLYINFO(wait_16p7msec), ktime_set(0, 0), ktime_set(0, 16600999)), 100U);

	/* start time is after now */
	KUNIT_EXPECT_EQ(test, get_remaining_delay(panel,
				&DLYINFO(wait_16p7msec), ktime_set(1000, 0), ktime_set(0, 0)), 0U);
}


/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_delay_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_delay_test *ctx =
		kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

	panel_mutex_init(panel, &panel->io_lock);
	panel_mutex_init(panel, &panel->op_lock);
	panel_mutex_init(panel, &panel->cmdq.lock);

	panel->cmdq.top = -1;
	panel->of_node_name = "test_panel";

	ctx->panel = panel;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_delay_test_exit(struct kunit *test)
{
	struct panel_delay_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_delay_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	KUNIT_CASE(panel_delay_test_test),
	KUNIT_CASE(panel_delay_test_is_valid_delay_return_false_with_invalid_delay),
	KUNIT_CASE(panel_delay_test_is_valid_delay_return_true_with_valid_delay),
	KUNIT_CASE(panel_delay_test_is_valid_timer_delay_return_false_with_invalid_delay),
	KUNIT_CASE(panel_delay_test_is_valid_timer_delay_return_true_with_valid_delay),
	KUNIT_CASE(panel_delay_test_create_delay_success),
	KUNIT_CASE(panel_delay_test_is_valid_timer_delay_begin_return_false_with_invalid_delay_type),
	KUNIT_CASE(panel_delay_test_is_valid_timer_delay_begin_return_true_with_valid_timer_delay_begin),
	KUNIT_CASE(panel_delay_test_create_timer_delay_begin_fail_if_not_timer_delay),
	KUNIT_CASE(panel_delay_test_create_timer_delay_begin_success),
	KUNIT_CASE(panel_delay_test_get_frame_delay),
	KUNIT_CASE(panel_delay_test_get_total_delay),
	KUNIT_CASE(panel_delay_test_get_remaining_delay),
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
static struct kunit_suite panel_delay_test_module = {
	.name = "panel_delay_test",
	.init = panel_delay_test_init,
	.exit = panel_delay_test_exit,
	.test_cases = panel_delay_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_delay_test_module);

MODULE_LICENSE("GPL v2");
