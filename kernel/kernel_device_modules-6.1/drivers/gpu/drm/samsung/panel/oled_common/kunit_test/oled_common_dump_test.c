// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "test_helper.h"
#include "panel_drv.h"
#include "oled_common/oled_common_dump.h"

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

#if !defined(CONFIG_UML)
/* NOTE: Target running TC must be in the #ifndef CONFIG_UML */
static void oled_common_dump_test_foo(struct kunit *test)
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

/* NOTE: UML TC */
static void oled_common_dump_test_test(struct kunit *test)
{
	KUNIT_SUCCEED(test);
}

#if 0
static void oled_common_dump_test_show_ssr_err_fail_with_invalid_argument(struct kunit *test)
{
	struct dumpinfo uninitialized_dumpinfo = { .res = NULL };

	KUNIT_EXPECT_EQ(test, -EINVAL, oled_dump_show_ssr_err(NULL));
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_dump_show_ssr_err(&uninitialized_dumpinfo));
}

static void oled_common_dump_test_show_ssr_err_fail_with_invalid_resource(struct kunit *test)
{
	struct resinfo ssr_err_resinfo = {
		.base = __PNOBJ_INITIALIZER(ssr_err, CMD_TYPE_RES),
		.state = (RES_UNINITIALIZED),
		.data = NULL,
		.dlen = 0,
		.resui = NULL,
		.nr_resui = 0,
	};
	struct dumpinfo ssr_err_dumpinfo =
		DUMPINFO_INIT(ssr_err, &ssr_err_resinfo, oled_dump_show_ssr_err);

	/* ssr_err resource uninitialized case */
	ssr_err_resinfo.dlen = S6E3FAC_SSR_TEST_LEN;
	ssr_err_resinfo.data = kunit_kzalloc(test, ssr_err_resinfo.dlen, GFP_KERNEL);
	ssr_err_resinfo.state = RES_UNINITIALIZED;
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_dump_show_ssr_err(&ssr_err_dumpinfo));
}

static void oled_common_dump_test_show_ssr_err_success(struct kunit *test)
{
	unsigned char SSR_DATA[S6E3FAC_SSR_TEST_LEN];
	struct rdinfo ssr_err_readinfo_1 =
		RDINFO_INIT(ssr_err, DSI_PKT_TYPE_RD, S6E3FAC_SSR_ONOFF_REG, S6E3FAC_SSR_ONOFF_OFS, S6E3FAC_SSR_ONOFF_LEN);
	struct rdinfo ssr_err_readinfo_2 =
		RDINFO_INIT(ssr_err, DSI_PKT_TYPE_RD, S6E3FAC_SSR_STATE_REG, S6E3FAC_SSR_STATE_OFS, S6E3FAC_SSR_STATE_LEN);

	struct res_update_info RESUI(ssr_err)[] = {
		{
			.offset = 0,
		}, {
			.offset = S6E3FAC_SSR_ONOFF_LEN,
		},
	};
	struct resinfo ssr_err_resinfo =
		RESINFO_INIT(ssr_err, SSR_DATA, RESUI(ssr_err));
	struct dumpinfo ssr_err_dumpinfo =
		DUMPINFO_INIT(ssr_err, &ssr_err_resinfo, oled_dump_show_ssr_err);

	RESUI(ssr_err)[0].rditbl = &ssr_err_readinfo_1;
	RESUI(ssr_err)[1].rditbl = &ssr_err_readinfo_2;

	ssr_err_resinfo.data = kunit_kzalloc(test, ssr_err_resinfo.dlen, GFP_KERNEL);
	ssr_err_resinfo.state = RES_INITIALIZED;

	/* ssr disabled case */
	ssr_err_resinfo.data[0] = 0x00;
	ssr_err_resinfo.data[1] = S6E3FAC_SSR_STATE_VALUE;
	KUNIT_EXPECT_EQ(test, 0, oled_dump_show_ssr_err(&ssr_err_dumpinfo));

	/* ecc total error case */
	ssr_err_resinfo.data[0] = S6E3FAC_SSR_ENABLE_VALUE;
	ssr_err_resinfo.data[1] = 0x01;
	KUNIT_EXPECT_EQ(test, 0, oled_dump_show_ssr_err(&ssr_err_dumpinfo));

	/* ecc unrecoverable case */
	ssr_err_resinfo.data[0] = S6E3FAC_SSR_ENABLE_VALUE;
	ssr_err_resinfo.data[1] = S6E3FAC_SSR_STATE_VALUE;
	KUNIT_EXPECT_EQ(test, 0, oled_dump_show_ssr_err(&ssr_err_dumpinfo));
}

static void oled_common_dump_test_show_ecc_err_fail_with_invalid_argument(struct kunit *test)
{
	struct dumpinfo uninitialized_dumpinfo = { .res = NULL };

	KUNIT_EXPECT_EQ(test, -EINVAL, oled_dump_show_ecc_err(NULL));
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_dump_show_ecc_err(&uninitialized_dumpinfo));
}

static void oled_common_dump_test_show_ecc_err_fail_with_invalid_resource(struct kunit *test)
{
	struct resinfo ecc_err_resinfo = {
		.base = __PNOBJ_INITIALIZER(ecc_err, CMD_TYPE_RES),
		.state = (RES_UNINITIALIZED),
		.data = NULL,
		.dlen = 0,
		.resui = NULL,
		.nr_resui = 0,
	};
	struct dumpinfo ecc_err_dumpinfo =
		DUMPINFO_INIT(ecc_err, &ecc_err_resinfo, oled_dump_show_ecc_err);

	/* ecc_err resource uninitialized case */
	ecc_err_resinfo.dlen = S6E3FAC_ECC_TEST_LEN;
	ecc_err_resinfo.data = kunit_kzalloc(test, ecc_err_resinfo.dlen, GFP_KERNEL);
	ecc_err_resinfo.state = RES_UNINITIALIZED;
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_dump_show_ecc_err(&ecc_err_dumpinfo));
}

static void oled_common_dump_test_show_ecc_err_success(struct kunit *test)
{
	unsigned char ECC_DATA[S6E3FAC_ECC_TEST_LEN];
	struct rdinfo ecc_err_readinfo =
		RDINFO_INIT(ecc_err, DSI_PKT_TYPE_RD, S6E3FAC_ECC_TEST_REG, S6E3FAC_ECC_TEST_OFS, S6E3FAC_ECC_TEST_LEN);
	DEFINE_RESUI(ecc_err, &ecc_err_readinfo, 0);
	struct resinfo ecc_err_resinfo =
		RESINFO_INIT(ecc_err, ECC_DATA, RESUI(ecc_err));
	struct dumpinfo ecc_err_dumpinfo =
		DUMPINFO_INIT(ecc_err, &ecc_err_resinfo, oled_dump_show_ecc_err);

	ecc_err_resinfo.data = kunit_kzalloc(test, ecc_err_resinfo.dlen, GFP_KERNEL);
	ecc_err_resinfo.state = RES_INITIALIZED;

	/* ecc disabled case */
	ecc_err_resinfo.data[0] = 0x01;
	ecc_err_resinfo.data[1] = 0x00;
	ecc_err_resinfo.data[2] = 0x00;
	KUNIT_EXPECT_EQ(test, 0, oled_dump_show_ecc_err(&ecc_err_dumpinfo));

	/* ecc total error case */
	ecc_err_resinfo.data[0] = S6E3FAC_ECC_ENABLE_VALUE;
	ecc_err_resinfo.data[1] = 0x01;
	ecc_err_resinfo.data[2] = 0x00;
	KUNIT_EXPECT_EQ(test, 0, oled_dump_show_ecc_err(&ecc_err_dumpinfo));

	/* ecc unrecoverable case */
	ecc_err_resinfo.data[0] = S6E3FAC_ECC_ENABLE_VALUE;
	ecc_err_resinfo.data[1] = 0x01;
	ecc_err_resinfo.data[2] = 0x01;
	KUNIT_EXPECT_EQ(test, 0, oled_dump_show_ecc_err(&ecc_err_dumpinfo));
}

static void oled_common_dump_test_show_flash_loaded_fail_with_invalid_argument(struct kunit *test)
{
	struct dumpinfo uninitialized_dumpinfo = { .res = NULL };

	KUNIT_EXPECT_EQ(test, -EINVAL, oled_dump_show_flash_loaded(NULL));
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_dump_show_flash_loaded(&uninitialized_dumpinfo));
}

static void oled_common_dump_test_show_flash_loaded_fail_with_invalid_resource(struct kunit *test)
{
	struct resinfo flash_loaded_resinfo = {
		.base = __PNOBJ_INITIALIZER(flash_loaded, CMD_TYPE_RES),
		.state = (RES_UNINITIALIZED),
		.data = NULL,
		.dlen = 0,
		.resui = NULL,
		.nr_resui = 0,
	};

	struct dumpinfo flash_loaded_dumpinfo =
		DUMPINFO_INIT(flash_loaded, &flash_loaded_resinfo, oled_dump_show_flash_loaded);

	/* flash_loaded resource uninitialized case */
	flash_loaded_resinfo.dlen = S6E3FAC_FLASH_LOADED_LEN;
	flash_loaded_resinfo.data = kunit_kzalloc(test, flash_loaded_resinfo.dlen, GFP_KERNEL);
	flash_loaded_resinfo.state = RES_UNINITIALIZED;
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_dump_show_flash_loaded(&flash_loaded_dumpinfo));
}

static void oled_common_dump_test_show_flash_loaded_success(struct kunit *test)
{
	unsigned char FLASH_LOADED_DATA[S6E3FAC_FLASH_LOADED_LEN];
	struct rdinfo flash_loaded_readinfo =
		RDINFO_INIT(flash_loaded, DSI_PKT_TYPE_RD, S6E3FAC_FLASH_LOADED_REG, S6E3FAC_FLASH_LOADED_OFS, S6E3FAC_FLASH_LOADED_LEN);
	DEFINE_RESUI(flash_loaded, &flash_loaded_readinfo, 0);
	struct resinfo flash_loaded_resinfo =
		RESINFO_INIT(flash_loaded, FLASH_LOADED_DATA, RESUI(flash_loaded));
	struct dumpinfo flash_loaded_dumpinfo =
		DUMPINFO_INIT(flash_loaded, &flash_loaded_resinfo, oled_dump_show_flash_loaded);

	flash_loaded_resinfo.data = kunit_kzalloc(test, flash_loaded_resinfo.dlen, GFP_KERNEL);
	flash_loaded_resinfo.state = RES_INITIALIZED;

	/* flash not loaded case */
	flash_loaded_resinfo.data[0] = 0x01;
	KUNIT_EXPECT_EQ(test, 0, oled_dump_show_flash_loaded(&flash_loaded_dumpinfo));

	/* flash loaded case */
	flash_loaded_resinfo.data[0] = S6E3FAC_FLASH_LOADED_VALUE;
	KUNIT_EXPECT_EQ(test, 0, oled_dump_show_flash_loaded(&flash_loaded_dumpinfo));
}
#endif


/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int oled_common_dump_test_init(struct kunit *test)
{
	struct panel_device *panel;

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);

	test->priv = panel;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void oled_common_dump_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case oled_common_dump_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#if !defined(CONFIG_UML)
	/* NOTE: Target running TC */
	KUNIT_CASE(oled_common_dump_test_foo),
#endif
	/* NOTE: UML TC */
	KUNIT_CASE(oled_common_dump_test_test),
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
static struct kunit_suite oled_common_dump_test_module = {
	.name = "oled_common_dump_test",
	.init = oled_common_dump_test_init,
	.exit = oled_common_dump_test_exit,
	.test_cases = oled_common_dump_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&oled_common_dump_test_module);

MODULE_LICENSE("GPL v2");
