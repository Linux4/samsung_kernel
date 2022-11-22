// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "test_helper.h"
#include "panel_drv.h"
#include "s6e3hae/s6e3hae.h"

/*
 * This is the most fundamental element of KUnit, the test case. A test case
 * makes a set EXPECTATIONs and ASSERTIONs about the behavior of some code; if
 * any expectations or assertions are not met, the test fails; otherwise, the
 * test passes.
 *
 * In KUnit, a test case is just a function with the signature
 * `void (*)(struct test *)`. `struct test` is a context object that stores
 * information about the current test.
 */

#if !defined(CONFIG_UML)
/* NOTE: Target running TC must be in the #ifndef CONFIG_UML */
static void s6e3hae_dynamic_mipi_test_foo(struct test *test)
{
	/*
	 * This is an EXPECTATION; it is how KUnit tests things. When you want
	 * to test a piece of code, you set some expectations about what the
	 * code should do. KUnit then runs the test and verifies that the code's
	 * behavior matched what was expected.
	 */
	EXPECT_EQ(test, 1, 2); // Obvious failure.
}
#endif

/* NOTE: UML TC */
static void s6e3hae_dynamic_mipi_test_test(struct test *test)
{
	SUCCEED(test);
}

static void s6e3hae_test_dynamic_mipi_set_ffc_fail_with_invalid_args(struct test *test)
{
	struct panel_device *panel = test->priv;
	unsigned char ffc_command[] = { 0xC5, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00 };
	unsigned char ffc_table[] = { 0xC5, 0x11, 0x10, 0x50, 0x2D, 0x58, 0x2D, 0x58 };
	struct maptbl ffc_maptbl = DEFINE_1D_MAPTBL(ffc_table,
			init_common_table, NULL, dynamic_mipi_set_ffc);

	ffc_maptbl.pdata = panel;
	ffc_maptbl.initialized = true;

	dynamic_mipi_set_ffc(NULL, ffc_command);
	dynamic_mipi_set_ffc(&ffc_maptbl, NULL);
}

static void s6e3hae_test_ffc_packet_should_be_updated_with_1st_dynamic_frequency(struct test *test)
{
	struct panel_device *panel = test->priv;
	const unsigned int FFC_GPARA = 0x00;
	unsigned char ffc_command[] = { 0xC5, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00 };
	unsigned char ffc_table[] = { 0xC5, 0x11, 0x10, 0x50, 0x2D, 0x58, 0x2D, 0x58 };
	struct maptbl ffc_maptbl = DEFINE_1D_MAPTBL(ffc_table,
			init_common_table, NULL, dynamic_mipi_set_ffc);
	DEFINE_PKTUI(ffc, &ffc_maptbl, 0);
	DEFINE_VARIABLE_PACKET(ffc, DSI_PKT_TYPE_WR, ffc_command, FFC_GPARA);
	struct dm_status_info *dm_status = &panel->dynamic_mipi.dm_status;
	struct dm_hs_info *hs_info;

	ffc_maptbl.pdata = panel;
	ffc_maptbl.initialized = true;

	dm_status->ffc_df = 2;
	dm_status->request_df = 0;
	hs_info = &panel->dynamic_mipi.dm_dt.dm_hs_list[dm_status->request_df];

	dm_status->current_ddi_osc = 0;
	panel_update_packet_data(panel, &PKTINFO(ffc));
	EXPECT_TRUE(test, !memcmp(ffc_command,
				hs_info->ffc_cmd[dm_status->current_ddi_osc], sizeof(ffc_command)));
	EXPECT_EQ(test, dm_status->request_df, dm_status->ffc_df);

	dm_status->current_ddi_osc = 1;
	panel_update_packet_data(panel, &PKTINFO(ffc));
	EXPECT_TRUE(test, !memcmp(ffc_command,
				hs_info->ffc_cmd[dm_status->current_ddi_osc], sizeof(ffc_command)));
	EXPECT_EQ(test, dm_status->request_df, dm_status->ffc_df);
}

static void s6e3hae_test_ffc_packet_should_be_updated_with_2nd_dynamic_frequency(struct test *test)
{
	struct panel_device *panel = test->priv;
	const unsigned int FFC_GPARA = 0x00;
	unsigned char ffc_command[] = { 0xC5, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00 };
	unsigned char ffc_table[] = { 0xC5, 0x11, 0x10, 0x50, 0x2D, 0x58, 0x2D, 0x58 };
	struct maptbl ffc_maptbl = DEFINE_1D_MAPTBL(ffc_table,
			init_common_table, NULL, dynamic_mipi_set_ffc);
	DEFINE_PKTUI(ffc, &ffc_maptbl, 0);
	DEFINE_VARIABLE_PACKET(ffc, DSI_PKT_TYPE_WR, ffc_command, FFC_GPARA);
	struct dm_status_info *dm_status = &panel->dynamic_mipi.dm_status;
	struct dm_hs_info *hs_info;

	ffc_maptbl.pdata = panel;
	ffc_maptbl.initialized = true;

	dm_status->ffc_df = 0;
	dm_status->request_df = 1;
	hs_info = &panel->dynamic_mipi.dm_dt.dm_hs_list[dm_status->request_df];

	dm_status->current_ddi_osc = 0;
	panel_update_packet_data(panel, &PKTINFO(ffc));
	EXPECT_TRUE(test, !memcmp(ffc_command,
				hs_info->ffc_cmd[dm_status->current_ddi_osc], sizeof(ffc_command)));
	EXPECT_EQ(test, dm_status->request_df, dm_status->ffc_df);

	dm_status->current_ddi_osc = 1;
	panel_update_packet_data(panel, &PKTINFO(ffc));
	EXPECT_TRUE(test, !memcmp(ffc_command,
				hs_info->ffc_cmd[dm_status->current_ddi_osc], sizeof(ffc_command)));
	EXPECT_EQ(test, dm_status->request_df, dm_status->ffc_df);
}

static void s6e3hae_test_ffc_packet_should_be_updated_with_3rd_dynamic_frequency(struct test *test)
{
	struct panel_device *panel = test->priv;
	const unsigned int FFC_GPARA = 0x00;
	unsigned char ffc_command[] = { 0xC5, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00 };
	unsigned char ffc_table[] = { 0xC5, 0x11, 0x10, 0x50, 0x2D, 0x58, 0x2D, 0x58 };
	struct maptbl ffc_maptbl = DEFINE_1D_MAPTBL(ffc_table,
			init_common_table, NULL, dynamic_mipi_set_ffc);
	DEFINE_PKTUI(ffc, &ffc_maptbl, 0);
	DEFINE_VARIABLE_PACKET(ffc, DSI_PKT_TYPE_WR, ffc_command, FFC_GPARA);
	struct dm_status_info *dm_status = &panel->dynamic_mipi.dm_status;
	struct dm_hs_info *hs_info;

	ffc_maptbl.pdata = panel;
	ffc_maptbl.initialized = true;

	dm_status->ffc_df = 1;
	dm_status->request_df = 2;
	hs_info = &panel->dynamic_mipi.dm_dt.dm_hs_list[dm_status->request_df];

	dm_status->current_ddi_osc = 0;
	panel_update_packet_data(panel, &PKTINFO(ffc));
	EXPECT_TRUE(test, !memcmp(ffc_command,
				hs_info->ffc_cmd[dm_status->current_ddi_osc], sizeof(ffc_command)));
	EXPECT_EQ(test, dm_status->request_df, dm_status->ffc_df);

	dm_status->current_ddi_osc = 1;
	panel_update_packet_data(panel, &PKTINFO(ffc));
	EXPECT_TRUE(test, !memcmp(ffc_command,
				hs_info->ffc_cmd[dm_status->current_ddi_osc], sizeof(ffc_command)));
	EXPECT_EQ(test, dm_status->request_df, dm_status->ffc_df);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int s6e3hae_dynamic_mipi_test_init(struct test *test)
{
	struct panel_device *panel;
	struct dm_dt_info dynamic_mipi_dt = {
		.dm_default = 0,
		.dm_hs_list_cnt = 3,
		.dm_hs_list = {
			{
				.hs_clk = 1328,
				.ffc_cmd_sz = 8,
				.ffc_cmd = {
					/* ffc_1328_osc_96_5 */
					{ 0xc5, 0x11, 0x10, 0x50, 0x2E, 0x82, 0x2E, 0x82 },
					/* ffc_1328_osc_94_5 */
					{ 0xc5, 0x11, 0x10, 0x50, 0x2D, 0x82, 0x2D, 0x82 },
				},
			},
			{
				.hs_clk = 1362,
				.ffc_cmd_sz = 8,
				.ffc_cmd = {
					/* ffc_1362_osc_96_5 */
					{ 0xC5, 0x11, 0x10, 0x50, 0x2D, 0x58, 0x2D, 0x58 },
					/* ffc_1362_osc_94_5 */
					{ 0xC5, 0x11, 0x10, 0x50, 0x2C, 0x68, 0x2C, 0x68 },
				},
			},
			{
				.hs_clk = 1328,
				.ffc_cmd_sz = 8,
				.ffc_cmd = {
					/* ffc_1368_osc_96_5 */
					{ 0xC5, 0x11, 0x10, 0x50, 0x2D, 0x25, 0x2D, 0x25 },
					/* ffc_1368_osc_94_5 */
					{ 0xC5, 0x11, 0x10, 0x50, 0x2C, 0x36, 0x2C, 0x36 },
				},
			},
		},
		.ffc_off_cmd_sz = 8,
		.ffc_off_cmd = { 0xC5, 0x11, 0x10, 0x50, 0x2D, 0x58, 0x2D, 0x58 },
	};

	panel = test_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	ASSERT_NOT_ERR_OR_NULL(test, panel);

	memcpy(&panel->dynamic_mipi.dm_dt, &dynamic_mipi_dt, sizeof(dynamic_mipi_dt));

	test->priv = panel;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void s6e3hae_dynamic_mipi_test_exit(struct test *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct test_case s6e3hae_dynamic_mipi_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#if !defined(CONFIG_UML)
	/* NOTE: Target running TC */
	TEST_CASE(s6e3hae_dynamic_mipi_test_foo),
#endif
	/* NOTE: UML TC */
	TEST_CASE(s6e3hae_dynamic_mipi_test_test),
	TEST_CASE(s6e3hae_test_dynamic_mipi_set_ffc_fail_with_invalid_args),
	TEST_CASE(s6e3hae_test_ffc_packet_should_be_updated_with_1st_dynamic_frequency),
	TEST_CASE(s6e3hae_test_ffc_packet_should_be_updated_with_2nd_dynamic_frequency),
	TEST_CASE(s6e3hae_test_ffc_packet_should_be_updated_with_3rd_dynamic_frequency),
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
static struct test_module s6e3hae_dynamic_mipi_test_module = {
	.name = "s6e3hae_dynamic_mipi_test",
	.init = s6e3hae_dynamic_mipi_test_init,
	.exit = s6e3hae_dynamic_mipi_test_exit,
	.test_cases = s6e3hae_dynamic_mipi_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
module_test(s6e3hae_dynamic_mipi_test_module);

MODULE_LICENSE("GPL v2");
