// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/list_sort.h>

#include "panel_drv.h"
#include "panel_obj.h"
#include "panel_function.h"

extern struct list_head panel_function_list;

struct panel_function_test {
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
static void panel_function_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_function_test_create_and_destroy_pnobj_function_fail_with_invalid_argument(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, !create_pnobj_function(NULL, NULL));
	destroy_pnobj_function(NULL);
}

static int getidx_test(struct maptbl *tbl) { return 0; }

static void panel_function_test_create_and_destroy_pnobj_function_success(struct kunit *test)
{
	struct pnobj_func *f = create_pnobj_function("func", getidx_test);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, f);
	KUNIT_EXPECT_EQ(test, f->symaddr, (unsigned long)getidx_test);

	destroy_pnobj_function(f);
}

static void panel_function_test_deepcopy_pnobj_function_success(struct kunit *test)
{
	struct pnobj_func *src = create_pnobj_function("func", getidx_test);
	struct pnobj_func *dst = kzalloc(sizeof(*dst), GFP_KERNEL);

	KUNIT_EXPECT_PTR_EQ(test, deepcopy_pnobj_function(dst, src), dst);
	KUNIT_EXPECT_PTR_NE(test, get_pnobj_function_name(dst),
			get_pnobj_function_name(src));
	KUNIT_EXPECT_STREQ(test, get_pnobj_function_name(dst),
			get_pnobj_function_name(src));
	KUNIT_EXPECT_EQ(test, dst->symaddr, (unsigned long)getidx_test);

	destroy_pnobj_function(src);
	destroy_pnobj_function(dst);
}

static void panel_function_test_panel_function_insert_sholud_failed_with_invalid_pnobj_func(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, panel_function_insert(NULL), -EINVAL);
}

static void panel_function_test_panel_function_insert_success(struct kunit *test)
{
	struct pnobj_func f = __PNOBJ_FUNC_INITIALIZER(func, getidx_test);

	KUNIT_EXPECT_EQ(test, panel_function_insert(&f), 0);
}

static void panel_function_test_panel_function_lookup_success(struct kunit *test)
{
	struct pnobj_func f = __PNOBJ_FUNC_INITIALIZER(func, getidx_test);
	struct pnobj_func *fp;


	KUNIT_ASSERT_EQ(test, panel_function_insert(&f), 0);
	fp = panel_function_lookup("func");
	KUNIT_EXPECT_PTR_NE(test, &f, fp);
	KUNIT_EXPECT_STREQ(test, get_pnobj_function_name(&f), get_pnobj_function_name(fp));
	KUNIT_EXPECT_EQ(test, get_pnobj_function_addr(&f), get_pnobj_function_addr(fp));
	KUNIT_EXPECT_TRUE(test, panel_function_lookup("func1") == NULL);
}

static void panel_function_test_panel_function_insert_array_success(struct kunit *test)
{
	static struct pnobj_func ddi_function_table[] = {
		[0] = __PNOBJ_FUNC_INITIALIZER(func1, getidx_test),
		[1] = __PNOBJ_FUNC_INITIALIZER(func2, getidx_test),
		[3] = __PNOBJ_FUNC_INITIALIZER(func3, getidx_test),
		[4] = __PNOBJ_FUNC_INITIALIZER(func4, getidx_test),
	};

	KUNIT_EXPECT_EQ(test, panel_function_insert_array(ddi_function_table, ARRAY_SIZE(ddi_function_table)), 0);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_function_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_function_test *ctx = kunit_kzalloc(test,
		sizeof(struct panel_function_test), GFP_KERNEL);

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	INIT_LIST_HEAD(&panel->prop_list);
	panel_function_init();

	ctx->panel = panel;
	test->priv = ctx;


	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_function_test_exit(struct kunit *test)
{
	struct panel_function_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));
	panel_function_deinit();
	KUNIT_ASSERT_TRUE(test, list_empty(&panel_function_list));
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_function_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_function_test_test),
	KUNIT_CASE(panel_function_test_create_and_destroy_pnobj_function_fail_with_invalid_argument),
	KUNIT_CASE(panel_function_test_create_and_destroy_pnobj_function_success),
	KUNIT_CASE(panel_function_test_deepcopy_pnobj_function_success),
	KUNIT_CASE(panel_function_test_panel_function_insert_sholud_failed_with_invalid_pnobj_func),
	KUNIT_CASE(panel_function_test_panel_function_insert_success),
	KUNIT_CASE(panel_function_test_panel_function_lookup_success),
	KUNIT_CASE(panel_function_test_panel_function_insert_array_success),
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
static struct kunit_suite panel_function_test_module = {
	.name = "panel_function_test",
	.init = panel_function_test_init,
	.exit = panel_function_test_exit,
	.test_cases = panel_function_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_function_test_module);

MODULE_LICENSE("GPL v2");
