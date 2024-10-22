// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>

#include "panel.h"
#include "panel_obj.h"
#include "panel_resource.h"

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
static void panel_resource_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_resource_test_set_resource_state_success(struct kunit *test)
{
	u8 DATE[7] = {1, 2, 3, 4, 5, 6, 7};
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);
	DEFINE_RESUI(date, &RDINFO(date), 0);
	DEFINE_RESOURCE(date, DATE, RESUI(date));

	KUNIT_ASSERT_EQ(test, RESINFO(date).state, RES_UNINITIALIZED);

	set_resource_state(&RESINFO(date), 100);
	KUNIT_EXPECT_EQ(test, RESINFO(date).state, RES_UNINITIALIZED);

	set_resource_state(&RESINFO(date), RES_INITIALIZED);
	KUNIT_EXPECT_EQ(test, RESINFO(date).state, RES_INITIALIZED);

	set_resource_state(&RESINFO(date), RES_UNINITIALIZED);
	KUNIT_EXPECT_EQ(test, RESINFO(date).state, RES_UNINITIALIZED);
}

static void panel_resource_test_create_resource_success_without_initdata(struct kunit *test)
{
	struct resinfo *res;
	struct res_update_info resui;

	res = create_resource("test_res", NULL, 3, &resui, 1);

	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&res->base), (u32)CMD_TYPE_RES);
	KUNIT_EXPECT_STREQ(test, get_resource_name(res), "test_res");
	KUNIT_EXPECT_EQ(test, res->state, RES_UNINITIALIZED);
	KUNIT_EXPECT_EQ(test, res->dlen, (u32)3);
	KUNIT_EXPECT_PTR_NE(test, res->resui, &resui);
	KUNIT_EXPECT_TRUE(test, !memcmp(res->resui, &resui, sizeof(resui)));
	KUNIT_EXPECT_EQ(test, res->nr_resui, (u32)1);

	destroy_resource(res);
}

static void panel_resource_test_create_resource_success_with_initdata_and_no_resui(struct kunit *test)
{
	struct resinfo *res;
	u8 initdata[3] = { 0x11, 0x22, 0x33 };

	res = create_resource("test_res", initdata, 3, NULL, 0);

	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&res->base), (u32)CMD_TYPE_RES);
	KUNIT_EXPECT_STREQ(test, get_resource_name(res), "test_res");
	KUNIT_EXPECT_EQ(test, res->state, RES_INITIALIZED);
	KUNIT_EXPECT_EQ(test, res->dlen, (u32)3);
	KUNIT_EXPECT_TRUE(test, (res->resui == NULL));
	KUNIT_EXPECT_EQ(test, res->nr_resui, (u32)0);
	KUNIT_EXPECT_PTR_NE(test, (void *)res->data, (void *)initdata);
	KUNIT_EXPECT_TRUE(test, !memcmp(res->data, initdata, res->dlen));

	destroy_resource(res);
}

static void panel_resource_test_create_resource_success_with_arguments(struct kunit *test)
{
	struct resinfo *res;
	struct res_update_info resui;
	u8 initdata[3] = { 0x11, 0x22, 0x33 };

	res = create_resource("test_res", initdata, 3, &resui, 1);

	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&res->base), (u32)CMD_TYPE_RES);
	KUNIT_EXPECT_STREQ(test, get_resource_name(res), "test_res");
	KUNIT_EXPECT_EQ(test, res->state, RES_INITIALIZED);
	KUNIT_EXPECT_EQ(test, res->dlen, (u32)3);
	KUNIT_EXPECT_PTR_NE(test, res->resui, &resui);
	KUNIT_EXPECT_TRUE(test, !memcmp(res->resui, &resui, sizeof(resui)));
	KUNIT_EXPECT_EQ(test, res->nr_resui, (u32)1);
	KUNIT_EXPECT_PTR_NE(test, (void *)res->data, (void *)initdata);
	KUNIT_EXPECT_TRUE(test, !memcmp(res->data, initdata, res->dlen));

	destroy_resource(res);
}

static void panel_resource_test_create_resource_success_with_no_resui(struct kunit *test)
{
	struct resinfo *res;
	u8 initdata[3] = { 0x11, 0x22, 0x33 };

	res = create_resource("test_res", initdata, 3, NULL, 0);

	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&res->base), (u32)CMD_TYPE_RES);
	KUNIT_EXPECT_STREQ(test, get_resource_name(res), "test_res");
	KUNIT_EXPECT_EQ(test, res->state, RES_INITIALIZED);
	KUNIT_EXPECT_EQ(test, res->dlen, (u32)3);
	KUNIT_EXPECT_TRUE(test, !res->resui);
	KUNIT_EXPECT_EQ(test, res->nr_resui, (u32)0);
	KUNIT_EXPECT_PTR_NE(test, (void *)res->data, (void *)initdata);
	KUNIT_EXPECT_TRUE(test, !memcmp(res->data, initdata, res->dlen));

	destroy_resource(res);
}

static void panel_resource_test_create_resource_should_return_null_with_ng_arguments(struct kunit *test)
{
	struct res_update_info resui;
	u8 buf[3] = { 0, };

	KUNIT_EXPECT_TRUE(test, !create_resource("test_res", buf, 0, &resui, 1));
}

static void panel_resource_test_destroy_resource_should_not_occur_exception_with_null_pointer(struct kunit *test)
{
	destroy_resource(NULL);
}

static void panel_resource_test_copy_resource_slice_should_be_fail_with_invalid_argument(struct kunit *test)
{
	u8 initdata[3] = { 0x11, 0x22, 0x33 };
	struct resinfo *res = create_resource("test_res", initdata, 3, NULL, 0);
	u8 *buf = kunit_kzalloc(test, sizeof(initdata), GFP_KERNEL);

	KUNIT_ASSERT_TRUE(test, res != NULL);
	KUNIT_EXPECT_EQ(test, copy_resource_slice(NULL, res, 0, 3), -EINVAL);
	KUNIT_EXPECT_EQ(test, copy_resource_slice(buf, NULL, 0, 3), -EINVAL);
	KUNIT_EXPECT_EQ(test, copy_resource_slice(buf, res, 0, 0), -EINVAL);
	destroy_resource(res);
}

static void panel_resource_test_copy_resource_slice_check_boundardy(struct kunit *test)
{
	u8 initdata[3] = { 0x11, 0x22, 0x33 };
	struct resinfo *res = create_resource("test_res", initdata, 3, NULL, 0);
	u8 *buf = kunit_kzalloc(test, sizeof(initdata), GFP_KERNEL);

	KUNIT_ASSERT_TRUE(test, res != NULL);
	KUNIT_EXPECT_EQ(test, copy_resource_slice(buf, res, 0, 4), -EINVAL);
	KUNIT_EXPECT_EQ(test, copy_resource_slice(buf, res, 1, 3), -EINVAL);
	KUNIT_EXPECT_EQ(test, copy_resource_slice(buf, res, 4, 1), -EINVAL);
	destroy_resource(res);
}

static void panel_resource_test_copy_resource_slice_success(struct kunit *test)
{
	u8 initdata[3] = { 0x11, 0x22, 0x33 };
	struct resinfo *res = create_resource("test_res", initdata, 3, NULL, 0);
	u8 *buf = kunit_kzalloc(test, sizeof(initdata), GFP_KERNEL);

	KUNIT_ASSERT_TRUE(test, res != NULL);
	KUNIT_EXPECT_EQ(test, copy_resource_slice(buf, res, 0, 3), 0);
	KUNIT_EXPECT_TRUE(test, !memcmp(buf, &initdata[0], 3));
	KUNIT_EXPECT_EQ(test, copy_resource_slice(buf, res, 1, 2), 0);
	KUNIT_EXPECT_TRUE(test, !memcmp(buf, &initdata[1], 2));
	KUNIT_EXPECT_EQ(test, copy_resource_slice(buf, res, 2, 1), 0);
	KUNIT_EXPECT_TRUE(test, !memcmp(buf, &initdata[2], 1));
	destroy_resource(res);
}

static void panel_resource_test_snprintf_resource(struct kunit *test)
{
	char *buf = kunit_kzalloc(test, 256, GFP_KERNEL);
	u8 initdata[17] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10,
	};
	u8 DATE[7] = {1, 2, 3, 4, 5, 6, 7};
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);
	DEFINE_RESUI(date, &RDINFO(date), 0);
	struct resinfo *immut_res = create_resource("immutable_resource",
			initdata, ARRAY_SIZE(initdata), NULL, 0);
	struct resinfo *mut_res = create_resource("mutable_resource",
			NULL, ARRAY_SIZE(DATE), RESUI(date), 1);

	snprintf_resource(buf, 256, mut_res);
	KUNIT_EXPECT_STREQ(test, buf, 
		"mutable_resource\n"
		"state: 0 (UNINITIALIZED)\n"
		"resui: 1 (MUTABLE)\n"
		"[0]: offset: 0, rdi: date\n"
		"size: 7");
	destroy_resource(mut_res);

	snprintf_resource(buf, 256, immut_res);
	KUNIT_EXPECT_STREQ(test, buf, 
		"immutable_resource\n"
		"state: 1 (INITIALIZED)\n"
		"resui: 0 (IMMUTABLE)\n"
		"size: 17\n"
		"data:\n"
		"00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n"
		"10");
	destroy_resource(immut_res);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_resource_test_init(struct kunit *test)
{
	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_resource_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_resource_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_resource_test_test),
	KUNIT_CASE(panel_resource_test_set_resource_state_success),
	KUNIT_CASE(panel_resource_test_create_resource_success_without_initdata),
	KUNIT_CASE(panel_resource_test_create_resource_success_with_initdata_and_no_resui),
	KUNIT_CASE(panel_resource_test_create_resource_success_with_arguments),
	KUNIT_CASE(panel_resource_test_create_resource_success_with_no_resui),
	KUNIT_CASE(panel_resource_test_create_resource_should_return_null_with_ng_arguments),
	KUNIT_CASE(panel_resource_test_destroy_resource_should_not_occur_exception_with_null_pointer),
	KUNIT_CASE(panel_resource_test_copy_resource_slice_should_be_fail_with_invalid_argument),
	KUNIT_CASE(panel_resource_test_copy_resource_slice_check_boundardy),
	KUNIT_CASE(panel_resource_test_copy_resource_slice_success),
	KUNIT_CASE(panel_resource_test_snprintf_resource),
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
static struct kunit_suite panel_resource_test_module = {
	.name = "panel_resource_test",
	.init = panel_resource_test_init,
	.exit = panel_resource_test_exit,
	.test_cases = panel_resource_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_resource_test_module);

MODULE_LICENSE("GPL v2");
