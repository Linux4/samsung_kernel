// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>

#include "panel.h"
#include "panel_obj.h"
#include "panel_resource.h"
#include "panel_dump.h"

static int show_dump(struct dumpinfo *info) { return 0; }

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
static void panel_dump_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_dump_test_create_dumpinfo_success_with_arguments(struct kunit *test)
{
	struct dumpinfo *dump;
	struct resinfo *res;
	struct res_update_info resui;
	u8 buf[3] = { 0, };
	struct dump_expect expects[] = {
		{ .offset = 0, .mask = 0x80, .value = 0x80, .msg = "Booster Mode : OFF(NG)" },
		{ .offset = 0, .mask = 0x40, .value = 0x00, .msg = "Idle Mode : ON(NG)" },
		{ .offset = 0, .mask = 0x10, .value = 0x10, .msg = "Sleep Mode : IN(NG)" },
		{ .offset = 0, .mask = 0x08, .value = 0x08, .msg = "Normal Mode : SLEEP(NG)" },
		{ .offset = 0, .mask = 0x04, .value = 0x04, .msg = "Display Mode : OFF(NG)" },
	};
	struct dump_ops ops = { .show = show_dump };
	int i;

	res = create_resource("test_res", buf, 3, &resui, 1);
	dump = create_dumpinfo("test_dump", res, &ops, expects, ARRAY_SIZE(expects));
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dump);

	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&dump->base), (u32)CMD_TYPE_DMP);
	KUNIT_EXPECT_STREQ(test, get_dump_name(dump), "test_dump");
	KUNIT_EXPECT_PTR_EQ(test, dump->res, res);
	KUNIT_EXPECT_PTR_NE(test, (void *)dump->expects, (void *)expects);
	for (i = 0; i < ARRAY_SIZE(expects); i++) {
		KUNIT_EXPECT_EQ(test, expects[i].offset, dump->expects[i].offset);
		KUNIT_EXPECT_EQ(test, expects[i].mask, dump->expects[i].mask);
		KUNIT_EXPECT_EQ(test, expects[i].value, dump->expects[i].value);
		KUNIT_EXPECT_STREQ(test, expects[i].msg, dump->expects[i].msg);
	}
	KUNIT_EXPECT_EQ(test, dump->nr_expects, (u32)ARRAY_SIZE(expects));
	destroy_dumpinfo(dump);

	dump = create_dumpinfo("test_dump", res, &ops, NULL, 0);
	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&dump->base), (u32)CMD_TYPE_DMP);
	KUNIT_EXPECT_STREQ(test, get_dump_name(dump), "test_dump");
	KUNIT_EXPECT_PTR_EQ(test, dump->res, res);
	KUNIT_EXPECT_TRUE(test, !(void *)dump->expects);
	KUNIT_EXPECT_EQ(test, dump->nr_expects, 0U);

	destroy_resource(res);
	destroy_dumpinfo(dump);
}

static void panel_dump_test_create_dumpinfo_should_return_null_with_no_callback(struct kunit *test)
{
	struct resinfo *res;
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	res = create_resource("test_res", buf, ARRAY_SIZE(buf), &resui, 1);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, res);
	KUNIT_EXPECT_TRUE(test, !create_dumpinfo("test_dump", res, NULL, NULL, 0));

	destroy_resource(res);
}

static void panel_dump_test_create_dumpinfo_should_return_null_with_no_resource(struct kunit *test)
{
	struct dump_ops ops = { .show = show_dump };

	KUNIT_EXPECT_TRUE(test, !create_dumpinfo("test_dump", NULL, &ops, NULL, 0));
}

static void panel_dump_test_destroy_dumpinfo_should_not_occur_exception_with_null_pointer(struct kunit *test)
{
	destroy_dumpinfo(NULL);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_dump_test_init(struct kunit *test)
{
	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_dump_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_dump_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_dump_test_test),
	KUNIT_CASE(panel_dump_test_create_dumpinfo_success_with_arguments),
	KUNIT_CASE(panel_dump_test_create_dumpinfo_should_return_null_with_no_callback),
	KUNIT_CASE(panel_dump_test_create_dumpinfo_should_return_null_with_no_callback),
	KUNIT_CASE(panel_dump_test_create_dumpinfo_should_return_null_with_no_resource),
	KUNIT_CASE(panel_dump_test_destroy_dumpinfo_should_not_occur_exception_with_null_pointer),
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
static struct kunit_suite panel_dump_test_module = {
	.name = "panel_dump_test",
	.init = panel_dump_test_init,
	.exit = panel_dump_test_exit,
	.test_cases = panel_dump_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_dump_test_module);

MODULE_LICENSE("GPL v2");
