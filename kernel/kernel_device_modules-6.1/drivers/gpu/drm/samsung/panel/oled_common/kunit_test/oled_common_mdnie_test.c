// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "test_helper.h"
#include "panel_drv.h"
#include "panel_packet.h"
#include "maptbl.h"
#include "oled_common/oled_function.h"
#include "oled_common/oled_common.h"
#include "oled_common/oled_common_mdnie.h"

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
static void oled_common_mdnie_test_foo(struct kunit *test)
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
static void oled_common_mdnie_test_test(struct kunit *test)
{
	KUNIT_SUCCEED(test);
}

static void oled_common_mdnie_test_oled_maptbl_getidx_mdnie_scenario_mode_function_with_invalid_parameter(struct kunit *test)
{
	struct maptbl uninitialized_maptbl = { .pdata = NULL };

	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_mdnie_scenario_mode(NULL));
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_mdnie_scenario_mode(&uninitialized_maptbl));
}

static void oled_common_mdnie_test_oled_maptbl_getidx_mdnie_scenario_mode_return_multiplication_nrow_and_mdnie_mode(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char mdnie_scenario_0_table[SCENARIO_MAX][MODE_MAX][1] = {
		[UI_MODE] = { [DYNAMIC] = { 0x10 }, [STANDARD] = { 0x11 }, [NATURAL] = { 0x12 }, [AUTO] = { 0x13 }, },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_mdnie_scenario_mode, oled_maptbl_getidx_mdnie_scenario_mode);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_default, oled_maptbl_copy_default);
	struct maptbl mdnie_scenario_0_maptbl = DEFINE_3D_MAPTBL(mdnie_scenario_0_table, &f_init, &f_getidx, &f_copy);

	mdnie_scenario_0_maptbl.pdata = to_panel_device(mdnie);
	mdnie_scenario_0_maptbl.initialized = true;

	mdnie->props.scenario = UI_MODE;
	mdnie->props.scenario_mode = STANDARD;
	KUNIT_EXPECT_GE(test, oled_maptbl_getidx_mdnie_scenario_mode(&mdnie_scenario_0_maptbl), 0);
	KUNIT_EXPECT_EQ(test, oled_maptbl_getidx_mdnie_scenario_mode(&mdnie_scenario_0_maptbl),
			maptbl_index(&mdnie_scenario_0_maptbl, UI_MODE, STANDARD, 0));
}

static void oled_common_mdnie_test_update_mdnie_scenario_packet_success(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char mdnie_scenario_0_table[SCENARIO_MAX][MODE_MAX][1] = {
		[UI_MODE] = { [DYNAMIC] = { 0x10 }, [STANDARD] = { 0x11 }, [NATURAL] = { 0x12 }, [AUTO] = { 0x13 }, },
	};
	unsigned char mdnie_scenario_1_table[SCENARIO_MAX][MODE_MAX][1] = {
		[UI_MODE] = { [DYNAMIC] = { 0x20 }, [STANDARD] = { 0x21 }, [NATURAL] = { 0x22 }, [AUTO] = { 0x23 }, },
	};
	unsigned char mdnie_scenario_2_table[SCENARIO_MAX][MODE_MAX][1] = {
		[UI_MODE] = { [DYNAMIC] = { 0x30 }, [STANDARD] = { 0x31 }, [NATURAL] = { 0x32 }, [AUTO] = { 0x33 }, },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_mdnie_scenario_mode, oled_maptbl_getidx_mdnie_scenario_mode);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_default, oled_maptbl_copy_default);
	struct maptbl mdnie_scenario_0_maptbl = DEFINE_3D_MAPTBL(mdnie_scenario_0_table, &f_init, &f_getidx, &f_copy);
	struct maptbl mdnie_scenario_1_maptbl = DEFINE_3D_MAPTBL(mdnie_scenario_1_table, &f_init, &f_getidx, &f_copy);
	struct maptbl mdnie_scenario_2_maptbl = DEFINE_3D_MAPTBL(mdnie_scenario_2_table, &f_init, &f_getidx, &f_copy);
	unsigned char TEST_MDNIE_0_COMMAND[2] = { 0xDF, 0x00, };
	unsigned char TEST_MDNIE_1_COMMAND[2] = { 0xDE, 0x00, };
	unsigned char TEST_MDNIE_2_COMMAND[2] = { 0xDD, 0x00, };
	DEFINE_PKTUI(test_mdnie_0, &mdnie_scenario_0_maptbl, 1);
	DEFINE_PKTUI(test_mdnie_1, &mdnie_scenario_1_maptbl, 1);
	DEFINE_PKTUI(test_mdnie_2, &mdnie_scenario_2_maptbl, 1);
	DEFINE_VARIABLE_PACKET(test_mdnie_0, DSI_PKT_TYPE_WR, TEST_MDNIE_0_COMMAND, 0);
	DEFINE_VARIABLE_PACKET(test_mdnie_1, DSI_PKT_TYPE_WR, TEST_MDNIE_1_COMMAND, 0);
	DEFINE_VARIABLE_PACKET(test_mdnie_2, DSI_PKT_TYPE_WR, TEST_MDNIE_2_COMMAND, 0);

	mdnie_scenario_0_maptbl.pdata = to_panel_device(mdnie);
	mdnie_scenario_1_maptbl.pdata = to_panel_device(mdnie);
	mdnie_scenario_2_maptbl.pdata = to_panel_device(mdnie);

	mdnie->props.scenario = UI_MODE;
	mdnie->props.scenario_mode = DYNAMIC;
	update_tx_packet(&PKTINFO(test_mdnie_0));
	KUNIT_EXPECT_EQ(test, mdnie_scenario_0_table[UI_MODE][DYNAMIC][0], TEST_MDNIE_0_COMMAND[1]);

	mdnie->props.scenario_mode = STANDARD;
	update_tx_packet(&PKTINFO(test_mdnie_1));
	KUNIT_EXPECT_EQ(test, mdnie_scenario_1_table[UI_MODE][STANDARD][0], TEST_MDNIE_1_COMMAND[1]);

	mdnie->props.scenario_mode = NATURAL;
	update_tx_packet(&PKTINFO(test_mdnie_2));
	KUNIT_EXPECT_EQ(test, mdnie_scenario_2_table[UI_MODE][NATURAL][0], TEST_MDNIE_2_COMMAND[1]);
}

static void oled_common_mdnie_test_oled_maptbl_getidx_mdnie_hdr_function_with_invalid_parameter(struct kunit *test)
{
	struct maptbl uninitialized_maptbl = { .pdata = NULL };

	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_mdnie_hdr(NULL));
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_mdnie_hdr(&uninitialized_maptbl));
}

static void oled_common_mdnie_test_oled_maptbl_getidx_mdnie_hdr_return_multiplication_nrow_and_mdnie_mode(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char mdnie_hdr_0_table[HDR_MAX][1] = { [HDR_1] = { 0x10 }, [HDR_2] = { 0x11 }, [HDR_3] = { 0x12 }, };
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_mdnie_hdr, oled_maptbl_getidx_mdnie_hdr);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_default, oled_maptbl_copy_default);
	struct maptbl mdnie_hdr_0_maptbl = DEFINE_2D_MAPTBL(mdnie_hdr_0_table, &f_init, &f_getidx, &f_copy);

	mdnie_hdr_0_maptbl.pdata = to_panel_device(mdnie);
	mdnie_hdr_0_maptbl.initialized = true;

	mdnie->props.hdr = HDR_1;
	KUNIT_EXPECT_GE(test, oled_maptbl_getidx_mdnie_hdr(&mdnie_hdr_0_maptbl), 0);
	KUNIT_EXPECT_EQ(test, oled_maptbl_getidx_mdnie_hdr(&mdnie_hdr_0_maptbl),
			maptbl_get_indexof_row(&mdnie_hdr_0_maptbl, HDR_1));
}

static void oled_common_mdnie_test_update_mdnie_hdr_packet_success(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char mdnie_hdr_0_table[HDR_MAX][1] = { [HDR_1] = { 0x10 }, [HDR_2] = { 0x11 }, [HDR_3] = { 0x12 }, };
	unsigned char mdnie_hdr_1_table[HDR_MAX][1] = { [HDR_1] = { 0x20 }, [HDR_2] = { 0x21 }, [HDR_3] = { 0x22 }, };
	unsigned char mdnie_hdr_2_table[HDR_MAX][1] = { [HDR_1] = { 0x30 }, [HDR_2] = { 0x31 }, [HDR_3] = { 0x32 }, };
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_mdnie_hdr, oled_maptbl_getidx_mdnie_hdr);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_default, oled_maptbl_copy_default);
	struct maptbl mdnie_hdr_0_maptbl = DEFINE_2D_MAPTBL(mdnie_hdr_0_table, &f_init, &f_getidx, &f_copy);
	struct maptbl mdnie_hdr_1_maptbl = DEFINE_2D_MAPTBL(mdnie_hdr_1_table, &f_init, &f_getidx, &f_copy);
	struct maptbl mdnie_hdr_2_maptbl = DEFINE_2D_MAPTBL(mdnie_hdr_2_table, &f_init, &f_getidx, &f_copy);
	unsigned char TEST_MDNIE_0_COMMAND[2] = { 0xDF, 0x00, };
	unsigned char TEST_MDNIE_1_COMMAND[2] = { 0xDE, 0x00, };
	unsigned char TEST_MDNIE_2_COMMAND[2] = { 0xDD, 0x00, };
	DEFINE_PKTUI(test_mdnie_0, &mdnie_hdr_0_maptbl, 1);
	DEFINE_PKTUI(test_mdnie_1, &mdnie_hdr_1_maptbl, 1);
	DEFINE_PKTUI(test_mdnie_2, &mdnie_hdr_2_maptbl, 1);
	DEFINE_VARIABLE_PACKET(test_mdnie_0, DSI_PKT_TYPE_WR, TEST_MDNIE_0_COMMAND, 0);
	DEFINE_VARIABLE_PACKET(test_mdnie_1, DSI_PKT_TYPE_WR, TEST_MDNIE_1_COMMAND, 0);
	DEFINE_VARIABLE_PACKET(test_mdnie_2, DSI_PKT_TYPE_WR, TEST_MDNIE_2_COMMAND, 0);

	mdnie_hdr_0_maptbl.pdata = to_panel_device(mdnie);
	mdnie_hdr_1_maptbl.pdata = to_panel_device(mdnie);
	mdnie_hdr_2_maptbl.pdata = to_panel_device(mdnie);

	mdnie->props.hdr = HDR_1;
	update_tx_packet(&PKTINFO(test_mdnie_0));
	KUNIT_EXPECT_EQ(test, mdnie_hdr_0_table[HDR_1][0], TEST_MDNIE_0_COMMAND[1]);

	mdnie->props.hdr = HDR_2;
	update_tx_packet(&PKTINFO(test_mdnie_1));
	KUNIT_EXPECT_EQ(test, mdnie_hdr_1_table[HDR_2][0], TEST_MDNIE_1_COMMAND[1]);

	mdnie->props.hdr = HDR_3;
	update_tx_packet(&PKTINFO(test_mdnie_2));
	KUNIT_EXPECT_EQ(test, mdnie_hdr_2_table[HDR_3][0], TEST_MDNIE_2_COMMAND[1]);
}

static void oled_common_mdnie_test_oled_maptbl_getidx_mdnie_trans_mode_function_with_invalid_parameter(struct kunit *test)
{
	struct maptbl uninitialized_maptbl = { .pdata = NULL };

	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_mdnie_trans_mode(NULL));
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_mdnie_trans_mode(&uninitialized_maptbl));
}

static void oled_common_mdnie_test_oled_maptbl_getidx_mdnie_trans_mode_return_multiplication_nrow_and_mdnie_mode(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char mdnie_trans_mode_table[MAX_TRANS_MODE][1] = {
		[TRANS_OFF] = { 0x00 },
		[TRANS_ON] = { 0xFF },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_mdnie_trans_mode, oled_maptbl_getidx_mdnie_trans_mode);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_default, oled_maptbl_copy_default);
	struct maptbl mdnie_trans_mode_maptbl = DEFINE_2D_MAPTBL(mdnie_trans_mode_table, &f_init, &f_getidx, &f_copy);

	mdnie_trans_mode_maptbl.pdata = to_panel_device(mdnie);
	mdnie_trans_mode_maptbl.initialized = true;

	mdnie->props.trans_mode = TRANS_ON;
	KUNIT_EXPECT_GE(test, oled_maptbl_getidx_mdnie_trans_mode(&mdnie_trans_mode_maptbl), 0);
	KUNIT_EXPECT_EQ(test, oled_maptbl_getidx_mdnie_trans_mode(&mdnie_trans_mode_maptbl),
		maptbl_get_indexof_row(&mdnie_trans_mode_maptbl, TRANS_ON));

	mdnie->props.trans_mode = TRANS_OFF;
	KUNIT_EXPECT_GE(test, oled_maptbl_getidx_mdnie_trans_mode(&mdnie_trans_mode_maptbl), 0);
	KUNIT_EXPECT_EQ(test, oled_maptbl_getidx_mdnie_trans_mode(&mdnie_trans_mode_maptbl),
		maptbl_get_indexof_row(&mdnie_trans_mode_maptbl, TRANS_OFF));
}

static void oled_common_mdnie_test_update_packet_using_mdnie_trans_mode_maptbl(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char mdnie_trans_mode_table[MAX_TRANS_MODE][1] = {
		[TRANS_OFF] = { 0x00 },
		[TRANS_ON] = { 0xFF },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_mdnie_trans_mode, oled_maptbl_getidx_mdnie_trans_mode);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_default, oled_maptbl_copy_default);
	struct maptbl mdnie_trans_mode_maptbl = DEFINE_2D_MAPTBL(mdnie_trans_mode_table, &f_init, &f_getidx, &f_copy);
	unsigned char TEST_MDNIE_2_COMMAND[2] = { 0xDD, 0x00, };
	DEFINE_PKTUI(test_mdnie_2, &mdnie_trans_mode_maptbl, 1);
	DEFINE_VARIABLE_PACKET(test_mdnie_2, DSI_PKT_TYPE_WR, TEST_MDNIE_2_COMMAND, 0);

	mdnie_trans_mode_maptbl.pdata = to_panel_device(mdnie);

	mdnie->props.trans_mode = TRANS_ON;
	update_tx_packet(&PKTINFO(test_mdnie_2));
	KUNIT_EXPECT_EQ(test, mdnie_trans_mode_table[TRANS_ON][0], TEST_MDNIE_2_COMMAND[1]);

	mdnie->props.trans_mode = TRANS_OFF;
	update_tx_packet(&PKTINFO(test_mdnie_2));
	KUNIT_EXPECT_EQ(test, mdnie_trans_mode_table[TRANS_OFF][0], TEST_MDNIE_2_COMMAND[1]);
}

static void oled_common_mdnie_test_oled_maptbl_getidx_mdnie_night_mode_function_with_invalid_argument(struct kunit *test)
{
	struct maptbl uninitialized_maptbl = { .pdata = NULL, };

	oled_maptbl_getidx_mdnie_night_mode(NULL);
	oled_maptbl_getidx_mdnie_night_mode(&uninitialized_maptbl);
}

static void oled_common_mdnie_test_oled_maptbl_getidx_mdnie_night_mode_function_success(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char mdnie_night_mode_table[NIGHT_MODE_MAX][10][1];
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_mdnie_night_mode, oled_maptbl_getidx_mdnie_night_mode);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_default, oled_maptbl_copy_default);
	struct maptbl mdnie_night_mode_maptbl = DEFINE_3D_MAPTBL(mdnie_night_mode_table, &f_init, &f_getidx, &f_copy);

	mdnie_night_mode_maptbl.pdata = to_panel_device(mdnie);
	mdnie_night_mode_maptbl.initialized = true;

	mdnie->props.scenario_mode = NATURAL;
	mdnie->props.night_level = 0;
	KUNIT_EXPECT_EQ(test, maptbl_index(&mdnie_night_mode_maptbl, NIGHT_MODE_ON, 0, 0),
			oled_maptbl_getidx_mdnie_night_mode(&mdnie_night_mode_maptbl));

	mdnie->props.scenario_mode = AUTO;
	mdnie->props.night_level = 9;
	KUNIT_EXPECT_EQ(test, maptbl_index(&mdnie_night_mode_maptbl, NIGHT_MODE_OFF, 9, 0),
			oled_maptbl_getidx_mdnie_night_mode(&mdnie_night_mode_maptbl));
}

static void oled_common_mdnie_test_update_packet_using_mdnie_night_mode_maptbl(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	static unsigned char mdnie_trans_mode_table[MAX_TRANS_MODE][1] = {
		[TRANS_OFF] = { 0x00 },
		[TRANS_ON] = { 0xFF },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_mdnie_trans_mode, oled_maptbl_getidx_mdnie_trans_mode);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_default, oled_maptbl_copy_default);
	struct maptbl mdnie_trans_mode_maptbl = DEFINE_2D_MAPTBL(mdnie_trans_mode_table, &f_init, &f_getidx, &f_copy);
	unsigned char TEST_MDNIE_2_COMMAND[2] = { 0xDD, 0x00, };
	DEFINE_PKTUI(test_mdnie_2, &mdnie_trans_mode_maptbl, 1);
	DEFINE_VARIABLE_PACKET(test_mdnie_2, DSI_PKT_TYPE_WR, TEST_MDNIE_2_COMMAND, 0);

	mdnie_trans_mode_maptbl.pdata = to_panel_device(mdnie);

	mdnie->props.trans_mode = TRANS_ON;
	update_tx_packet(&PKTINFO(test_mdnie_2));
	KUNIT_EXPECT_EQ(test, mdnie_trans_mode_table[TRANS_ON][0], TEST_MDNIE_2_COMMAND[1]);

	mdnie->props.trans_mode = TRANS_OFF;
	update_tx_packet(&PKTINFO(test_mdnie_2));
	KUNIT_EXPECT_EQ(test, mdnie_trans_mode_table[TRANS_OFF][0], TEST_MDNIE_2_COMMAND[1]);
}

static void oled_common_mdnie_test_oled_maptbl_getidx_color_lens_with_invalid_argument(struct kunit *test)
{
	struct maptbl uninitialized_maptbl = { .pdata = NULL };

	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_color_lens(NULL));
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_color_lens(&uninitialized_maptbl));
}

static void oled_common_mdnie_test_oled_maptbl_getidx_color_lens_with_invalid_color_lens(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	struct maptbl color_lens_maptbl = { .pdata = to_panel_device(mdnie) };

	/* must fail with invalid mode of mdnie color lens */
	mdnie->props.color_lens = 255;
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_color_lens(&color_lens_maptbl));

	/* must fail with COLOR_LENS_OFF */
	mdnie->props.color_lens = COLOR_LENS_OFF;
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_color_lens(&color_lens_maptbl));

	/* must fail with invalid color of mdnie color lens */
	mdnie->props.color_lens = COLOR_LENS_ON;
	mdnie->props.color_lens_color = 255;
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_color_lens(&color_lens_maptbl));

	/* must fail with invalid level of mdnie color lens */
	mdnie->props.color_lens = COLOR_LENS_ON;
	mdnie->props.color_lens_color = COLOR_LENS_COLOR_GREEN;
	mdnie->props.color_lens_level = 255;
	KUNIT_EXPECT_EQ(test, -EINVAL, oled_maptbl_getidx_color_lens(&color_lens_maptbl));
}

static void oled_common_mdnie_test_oled_maptbl_getidx_color_lens_success(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char mdnie_color_lens_initial_table[COLOR_LENS_COLOR_MAX][COLOR_LENS_LEVEL_MAX][1] = {
		[COLOR_LENS_COLOR_BLUE] = { [COLOR_LENS_LEVEL_20P] = { 0x10 } },
		[COLOR_LENS_COLOR_AZURE] = { [COLOR_LENS_LEVEL_25P] = { 0x20 } },
		[COLOR_LENS_COLOR_CYAN] = { [COLOR_LENS_LEVEL_30P] = { 0x30 } },
		[COLOR_LENS_COLOR_SPRING_GREEN] = { [COLOR_LENS_LEVEL_35P] = { 0x40 } },
		[COLOR_LENS_COLOR_GREEN] = { [COLOR_LENS_LEVEL_40P] = { 0x50 } },
		[COLOR_LENS_COLOR_CHARTREUSE_GREEN] = { [COLOR_LENS_LEVEL_45P] = { 0x60 } },
		[COLOR_LENS_COLOR_YELLOW] = { [COLOR_LENS_LEVEL_50P] = { 0x70 } },
		[COLOR_LENS_COLOR_ORANGE] = { [COLOR_LENS_LEVEL_55P] = { 0x80 } },
		[COLOR_LENS_COLOR_RED] = { [COLOR_LENS_LEVEL_60P] = { 0x90 } },
		[COLOR_LENS_COLOR_ROSE] = { [COLOR_LENS_LEVEL_20P] = { 0xA0 } },
		[COLOR_LENS_COLOR_MAGENTA] = { [COLOR_LENS_LEVEL_25P] = { 0xB0 } },
		[COLOR_LENS_COLOR_VIOLET] = { [COLOR_LENS_LEVEL_30P] = { 0xC0 } },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_color_lens, oled_maptbl_getidx_color_lens);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_default, oled_maptbl_copy_default);
	struct maptbl mdnie_color_lens_maptbl = DEFINE_3D_MAPTBL(mdnie_color_lens_initial_table, &f_init, &f_getidx, &f_copy);

	mdnie_color_lens_maptbl.pdata = to_panel_device(mdnie);
	mdnie_color_lens_maptbl.initialized = true;

	mdnie->props.color_lens = COLOR_LENS_ON;
	mdnie->props.color_lens_color = COLOR_LENS_COLOR_VIOLET;
	mdnie->props.color_lens_level = COLOR_LENS_LEVEL_30P;
	KUNIT_EXPECT_EQ(test, maptbl_index(&mdnie_color_lens_maptbl,
			COLOR_LENS_COLOR_VIOLET, COLOR_LENS_LEVEL_30P, 0),
			oled_maptbl_getidx_color_lens(&mdnie_color_lens_maptbl));
}

static void oled_common_mdnie_test_oled_maptbl_copy_scr_white_with_invalid_argument(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	struct maptbl uninitialized_maptbl = { .pdata = NULL };
	unsigned char coord_wrgb[MAX_WCRD_TYPE][MAX_COLOR] = {
		[WCRD_TYPE_ADAPTIVE] = { 0xff, 0xff, 0xff },
		[WCRD_TYPE_D65] = { 0xff, 0xfc, 0xf6 },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_default, oled_maptbl_getidx_default);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_scr_white, oled_maptbl_copy_scr_white);
	struct maptbl test_maptbl = DEFINE_0D_MAPTBL(test_table, &f_init, &f_getidx, &f_copy);
	unsigned char dst[MAX_COLOR * 2] = { 0, };

	test_maptbl.pdata = to_panel_device(mdnie);
	test_maptbl.initialized = true;

	memcpy(mdnie->props.coord_wrgb, coord_wrgb, sizeof(coord_wrgb));
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_COLOR_COORDINATE;

	/* def_wrgb must not be changed in failure case */
	oled_maptbl_copy_scr_white(NULL, dst);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.def_wrgb[0]);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.def_wrgb[1]);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.def_wrgb[2]);

	oled_maptbl_copy_scr_white(&uninitialized_maptbl, dst);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.def_wrgb[0]);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.def_wrgb[1]);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.def_wrgb[2]);

	oled_maptbl_copy_scr_white(&test_maptbl, NULL);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.def_wrgb[0]);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.def_wrgb[1]);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.def_wrgb[2]);
}

static void oled_common_mdnie_test_oled_maptbl_copy_scr_white_overflow_by_def_wrgb_ofs_when_scr_white_mode_is_color_coordinate(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char coord_wrgb[MAX_WCRD_TYPE][MAX_COLOR] = {
		[WCRD_TYPE_ADAPTIVE] = { 0xF0, 0xF1, 0xF2 },
		[WCRD_TYPE_D65] = { 0xF3, 0xF4, 0xF5 },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_default, oled_maptbl_getidx_default);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_scr_white, oled_maptbl_copy_scr_white);
	struct maptbl test_maptbl = DEFINE_0D_MAPTBL(test_table, &f_init, &f_getidx, &f_copy);
	unsigned char dst[MAX_COLOR * 2] = { 0, };

	test_maptbl.pdata = to_panel_device(mdnie);
	test_maptbl.initialized = true;

	memcpy(mdnie->props.coord_wrgb, coord_wrgb, sizeof(coord_wrgb));
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));

	/* def_wrgb_ofs is updated by whiteRGB sysfs node */
	mdnie->props.def_wrgb_ofs[0] = 0x0F;
	mdnie->props.def_wrgb_ofs[1] = 0x0F;
	mdnie->props.def_wrgb_ofs[2] = 0x0F;

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_COLOR_COORDINATE;
	mdnie->props.scenario_mode = AUTO;
	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	/* check overflow in case of cur_wrgb = def_wrgb + def_wrgb_ofs bigger than 0xFF */
	KUNIT_EXPECT_EQ(test, (u8)0xFF, mdnie->props.cur_wrgb[0]);
	KUNIT_EXPECT_EQ(test, (u8)0xFF, mdnie->props.cur_wrgb[1]);
	KUNIT_EXPECT_EQ(test, (u8)0xFF, mdnie->props.cur_wrgb[2]);
}

static void oled_common_mdnie_test_oled_maptbl_copy_scr_white_overflow_by_def_wrgb_ofs_when_scr_white_mode_is_adjust_ldu(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char adjust_ldu_wrgb[MAX_WCRD_TYPE][MAX_LDU_MODE][MAX_COLOR] = {
		{
			/* adjust_ldu_data_1 */
			{ 0xff, 0xff, 0xff }, /* LDU_MODE_OFF */
			{ 0xfd, 0xfd, 0xff }, /* LDU_MODE_1 */
			{ 0xfb, 0xfb, 0xff }, /* LDU_MODE_2 */
			{ 0xf9, 0xf8, 0xff }, /* LDU_MODE_3 */
			{ 0xf6, 0xf6, 0xff }, /* LDU_MODE_4 */
			{ 0xf4, 0xf4, 0xff }, /* LDU_MODE_5 */
		},
		{
			/* adjust_ldu_data_2 */
			{ 0xff, 0xfc, 0xf6 }, /* LDU_MODE_OFF */
			{ 0xfd, 0xfa, 0xf6 }, /* LDU_MODE_1 */
			{ 0xfb, 0xf7, 0xf6 }, /* LDU_MODE_2 */
			{ 0xf9, 0xf5, 0xf6 }, /* LDU_MODE_3 */
			{ 0xf4, 0xf0, 0xf6 }, /* LDU_MODE_4 */
			{ 0xf2, 0xee, 0xf6 }, /* LDU_MODE_5 */
		},
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_default, oled_maptbl_getidx_default);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_scr_white, oled_maptbl_copy_scr_white);
	struct maptbl test_maptbl = DEFINE_0D_MAPTBL(test_table, &f_init, &f_getidx, &f_copy);
	unsigned char dst[MAX_COLOR * 2] = { 0, };

	test_maptbl.pdata = to_panel_device(mdnie);
	test_maptbl.initialized = true;

	memcpy(mdnie->props.adjust_ldu_wrgb, adjust_ldu_wrgb, sizeof(adjust_ldu_wrgb));
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));

	/* def_wrgb_ofs is updated by whiteRGB sysfs node */
	mdnie->props.def_wrgb_ofs[0] = 0x0F;
	mdnie->props.def_wrgb_ofs[1] = 0x0F;
	mdnie->props.def_wrgb_ofs[2] = 0x0F;

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_ADJUST_LDU;
	mdnie->props.scenario_mode = AUTO;
	mdnie->props.scenario = BROWSER_MODE;
	mdnie->props.ldu = LDU_MODE_OFF;

	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	KUNIT_EXPECT_EQ(test, (u8)0xFF, mdnie->props.cur_wrgb[0]);
	KUNIT_EXPECT_EQ(test, (u8)0xFF, mdnie->props.cur_wrgb[1]);
	KUNIT_EXPECT_EQ(test, (u8)0xFF, mdnie->props.cur_wrgb[2]);
}

static void oled_common_mdnie_test_oled_maptbl_copy_scr_white_underflow_by_def_wrgb_ofs_when_scr_white_mode_is_color_coordinate(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char coord_wrgb[MAX_WCRD_TYPE][MAX_COLOR] = {
		[WCRD_TYPE_ADAPTIVE] = { 19, 20, 21 },
		[WCRD_TYPE_D65] = { 19, 20, 21 },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_default, oled_maptbl_getidx_default);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_scr_white, oled_maptbl_copy_scr_white);
	struct maptbl test_maptbl = DEFINE_0D_MAPTBL(test_table, &f_init, &f_getidx, &f_copy);
	unsigned char dst[MAX_COLOR * 2] = { 0, };

	test_maptbl.pdata = to_panel_device(mdnie);
	test_maptbl.initialized = true;

	memcpy(mdnie->props.coord_wrgb, coord_wrgb, sizeof(coord_wrgb));
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));

	/* def_wrgb_ofs is updated by whiteRGB sysfs node */
	mdnie->props.def_wrgb_ofs[0] = -20;
	mdnie->props.def_wrgb_ofs[1] = -20;
	mdnie->props.def_wrgb_ofs[2] = -20;

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_COLOR_COORDINATE;
	mdnie->props.scenario_mode = AUTO;
	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	/* check underflow in case of cur_wrgb = def_wrgb + def_wrgb_ofs is less than zero */
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.cur_wrgb[0]);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.cur_wrgb[1]);
	KUNIT_EXPECT_EQ(test, (u8)1, mdnie->props.cur_wrgb[2]);
}

static void oled_common_mdnie_test_oled_maptbl_copy_scr_white_underflow_by_def_wrgb_ofs_when_scr_white_mode_is_adjust_ldu(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char adjust_ldu_wrgb[MAX_WCRD_TYPE][MAX_LDU_MODE][MAX_COLOR] = {
		{
			/* adjust_ldu_data_1 */
			{ 0x0f, 0x0f, 0x0f }, /* LDU_MODE_OFF */
			{ 0x0d, 0x0d, 0x0f }, /* LDU_MODE_1 */
			{ 0x0b, 0x0b, 0x0f }, /* LDU_MODE_2 */
			{ 0x09, 0x08, 0x0f }, /* LDU_MODE_3 */
			{ 0x06, 0x06, 0x0f }, /* LDU_MODE_4 */
			{ 0x04, 0x04, 0x0f }, /* LDU_MODE_5 */
		},
		{
			/* adjust_ldu_data_2 */
			{ 0x0f, 0x0c, 0x06 }, /* LDU_MODE_OFF */
			{ 0x0d, 0x0a, 0x06 }, /* LDU_MODE_1 */
			{ 0x0b, 0x07, 0x06 }, /* LDU_MODE_2 */
			{ 0x09, 0x05, 0x06 }, /* LDU_MODE_3 */
			{ 0x04, 0x00, 0x06 }, /* LDU_MODE_4 */
			{ 0x02, 0x0e, 0x06 }, /* LDU_MODE_5 */
		},
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_default, oled_maptbl_getidx_default);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_scr_white, oled_maptbl_copy_scr_white);
	struct maptbl test_maptbl = DEFINE_0D_MAPTBL(test_table, &f_init, &f_getidx, &f_copy);
	unsigned char dst[MAX_COLOR * 2] = { 0, };

	test_maptbl.pdata = to_panel_device(mdnie);
	test_maptbl.initialized = true;

	memcpy(mdnie->props.adjust_ldu_wrgb, adjust_ldu_wrgb, sizeof(adjust_ldu_wrgb));

	/* def_wrgb_ofs is updated by whiteRGB sysfs node */
	mdnie->props.def_wrgb_ofs[0] = -20;
	mdnie->props.def_wrgb_ofs[1] = -20;
	mdnie->props.def_wrgb_ofs[2] = -20;

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_ADJUST_LDU;
	mdnie->props.scenario_mode = AUTO;
	mdnie->props.scenario = BROWSER_MODE;
	mdnie->props.ldu = LDU_MODE_OFF;

	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.cur_wrgb[0]);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.cur_wrgb[1]);
	KUNIT_EXPECT_EQ(test, (u8)0, mdnie->props.cur_wrgb[2]);
}

static void oled_common_mdnie_test_oled_maptbl_copy_scr_white_check_implicit_type_promotion_when_scr_white_mode_is_color_coordinate(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char coord_wrgb[MAX_WCRD_TYPE][MAX_COLOR] = {
		[WCRD_TYPE_ADAPTIVE] = { 128, 128, 128 },
		[WCRD_TYPE_D65] = { 128, 128, 128 },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_default, oled_maptbl_getidx_default);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_scr_white, oled_maptbl_copy_scr_white);
	struct maptbl test_maptbl = DEFINE_0D_MAPTBL(test_table, &f_init, &f_getidx, &f_copy);
	unsigned char dst[MAX_COLOR * 2] = { 0, };

	test_maptbl.pdata = to_panel_device(mdnie);
	test_maptbl.initialized = true;

	memcpy(mdnie->props.coord_wrgb, coord_wrgb, sizeof(coord_wrgb));
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));

	/* def_wrgb_ofs is updated by whiteRGB sysfs node */
	mdnie->props.def_wrgb_ofs[0] = -1;
	mdnie->props.def_wrgb_ofs[1] = 127;
	mdnie->props.def_wrgb_ofs[2] = -127;

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_COLOR_COORDINATE;
	mdnie->props.scenario_mode = AUTO;
	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	/* check implicit type promotion */
	KUNIT_EXPECT_EQ(test, (u8)127, mdnie->props.cur_wrgb[0]);
	KUNIT_EXPECT_EQ(test, (u8)255, mdnie->props.cur_wrgb[1]);
	KUNIT_EXPECT_EQ(test, (u8)1, mdnie->props.cur_wrgb[2]);
}

static void oled_common_mdnie_test_oled_maptbl_copy_scr_white_check_implicit_type_promotion_when_scr_white_mode_is_adjust_ldu(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char adjust_ldu_wrgb[MAX_WCRD_TYPE][MAX_LDU_MODE][MAX_COLOR] = {
		[WCRD_TYPE_ADAPTIVE] = { [LDU_MODE_5] = { 128, 128, 128 }, },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_default, oled_maptbl_getidx_default);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_scr_white, oled_maptbl_copy_scr_white);
	struct maptbl test_maptbl = DEFINE_0D_MAPTBL(test_table, &f_init, &f_getidx, &f_copy);
	unsigned char dst[MAX_COLOR * 2] = { 0, };

	test_maptbl.pdata = to_panel_device(mdnie);
	test_maptbl.initialized = true;

	memcpy(mdnie->props.adjust_ldu_wrgb, adjust_ldu_wrgb, sizeof(adjust_ldu_wrgb));

	/* def_wrgb_ofs is updated by whiteRGB sysfs node */
	mdnie->props.def_wrgb_ofs[0] = -1;
	mdnie->props.def_wrgb_ofs[1] = 127;
	mdnie->props.def_wrgb_ofs[2] = -127;

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_ADJUST_LDU;
	mdnie->props.scenario_mode = AUTO;
	mdnie->props.scenario = BROWSER_MODE;
	mdnie->props.ldu = LDU_MODE_5;

	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	KUNIT_EXPECT_EQ(test, (u8)127, mdnie->props.cur_wrgb[0]);
	KUNIT_EXPECT_EQ(test, (u8)255, mdnie->props.cur_wrgb[1]);
	KUNIT_EXPECT_EQ(test, (u8)1, mdnie->props.cur_wrgb[2]);
}

static void oled_common_mdnie_test_oled_maptbl_copy_scr_white_when_scr_white_mode_is_color_coordinate(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char coord_wrgb[MAX_WCRD_TYPE][MAX_COLOR] = {
		[WCRD_TYPE_ADAPTIVE] = { 0xff, 0xff, 0xff },
		[WCRD_TYPE_D65] = { 0xff, 0xfc, 0xf6 },
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_default, oled_maptbl_getidx_default);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_scr_white, oled_maptbl_copy_scr_white);
	struct maptbl test_maptbl = DEFINE_0D_MAPTBL(test_table, &f_init, &f_getidx, &f_copy);
	unsigned char dst[MAX_COLOR * 2] = { 0, };
	int wcrd_type[MODE_MAX] = {
		[DYNAMIC] = WCRD_TYPE_D65,
		[STANDARD] = WCRD_TYPE_D65,
		[NATURAL] = WCRD_TYPE_D65,
		[MOVIE] = WCRD_TYPE_ADAPTIVE,
		[AUTO] = WCRD_TYPE_ADAPTIVE,
	};

	test_maptbl.pdata = to_panel_device(mdnie);
	test_maptbl.initialized = true;

	memcpy(mdnie->props.coord_wrgb, coord_wrgb, sizeof(coord_wrgb));
	memset(mdnie->props.def_wrgb, 0, sizeof(mdnie->props.def_wrgb));
	memset(mdnie->props.cur_wrgb, 0, sizeof(mdnie->props.cur_wrgb));

	/* def_wrgb_ofs is updated by whiteRGB sysfs node */
	mdnie->props.def_wrgb_ofs[0] = MIN_WRGB_OFS;
	mdnie->props.def_wrgb_ofs[1] = MAX_WRGB_OFS;
	mdnie->props.def_wrgb_ofs[2] = -20;

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_COLOR_COORDINATE;
	mdnie->props.scenario_mode = DYNAMIC;
	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	KUNIT_EXPECT_TRUE(test, !memcmp(coord_wrgb[wcrd_type[DYNAMIC]],
				mdnie->props.def_wrgb, sizeof(mdnie->props.def_wrgb)));
	KUNIT_EXPECT_TRUE(test, !memcmp(coord_wrgb[wcrd_type[DYNAMIC]],
				mdnie->props.cur_wrgb, sizeof(mdnie->props.cur_wrgb)));
	KUNIT_EXPECT_EQ(test, coord_wrgb[wcrd_type[DYNAMIC]][0], dst[0]);
	KUNIT_EXPECT_EQ(test, coord_wrgb[wcrd_type[DYNAMIC]][1], dst[2]);
	KUNIT_EXPECT_EQ(test, coord_wrgb[wcrd_type[DYNAMIC]][2], dst[4]);

	mdnie->props.scenario_mode = AUTO;
	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	KUNIT_EXPECT_TRUE(test, !memcmp(coord_wrgb[wcrd_type[AUTO]],
				mdnie->props.def_wrgb, sizeof(mdnie->props.def_wrgb)));

	/* cur_wrgb = arr + def_wrgb_ofs in AUTO mode */
	KUNIT_EXPECT_EQ(test, (u8)(coord_wrgb[wcrd_type[AUTO]][0] +
			mdnie->props.def_wrgb_ofs[0]), mdnie->props.cur_wrgb[0]);
	KUNIT_EXPECT_EQ(test, (u8)(coord_wrgb[wcrd_type[AUTO]][1] +
			mdnie->props.def_wrgb_ofs[1]), mdnie->props.cur_wrgb[1]);
	KUNIT_EXPECT_EQ(test, (u8)(coord_wrgb[wcrd_type[AUTO]][2] +
			mdnie->props.def_wrgb_ofs[2]), mdnie->props.cur_wrgb[2]);

	/* cur_wrgb = arr + def_wrgb_ofs */
	KUNIT_EXPECT_EQ(test, (unsigned char)(coord_wrgb[wcrd_type[AUTO]][0] +
			mdnie->props.def_wrgb_ofs[0]), dst[0]);
	KUNIT_EXPECT_EQ(test, (unsigned char)(coord_wrgb[wcrd_type[AUTO]][1] +
			mdnie->props.def_wrgb_ofs[1]), dst[2]);
	KUNIT_EXPECT_EQ(test, (unsigned char)(coord_wrgb[wcrd_type[AUTO]][2] +
			mdnie->props.def_wrgb_ofs[2]), dst[4]);
}

static void oled_common_mdnie_test_oled_maptbl_copy_scr_white_when_scr_white_mode_is_sensor_rgb(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char sensor_rgb[MAX_COLOR] = { 0x11, 0x22, 0x33 };
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_default, oled_maptbl_getidx_default);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_scr_white, oled_maptbl_copy_scr_white);
	struct maptbl test_maptbl = DEFINE_0D_MAPTBL(test_table, &f_init, &f_getidx, &f_copy);
	unsigned char dst[MAX_COLOR * 2] = { 0, };

	test_maptbl.pdata = to_panel_device(mdnie);
	test_maptbl.initialized = true;
	mdnie->props.scr_white_mode = SCR_WHITE_MODE_SENSOR_RGB;

	memcpy(mdnie->props.ssr_wrgb, sensor_rgb, sizeof(sensor_rgb));

	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	KUNIT_EXPECT_EQ(test, sensor_rgb[0], mdnie->props.cur_wrgb[0]);
	KUNIT_EXPECT_EQ(test, sensor_rgb[1], mdnie->props.cur_wrgb[1]);
	KUNIT_EXPECT_EQ(test, sensor_rgb[2], mdnie->props.cur_wrgb[2]);

	KUNIT_EXPECT_EQ(test, sensor_rgb[0], dst[0]);
	KUNIT_EXPECT_EQ(test, sensor_rgb[1], dst[2]);
	KUNIT_EXPECT_EQ(test, sensor_rgb[2], dst[4]);
}

static void oled_common_mdnie_test_oled_maptbl_copy_scr_white_when_scr_white_mode_is_adjust_ldu(struct kunit *test)
{
	struct mdnie_info *mdnie = test->priv;
	unsigned char adjust_ldu_wrgb[MAX_WCRD_TYPE][MAX_LDU_MODE][MAX_COLOR] = {
		{
			/* adjust_ldu_data_1 */
			{ 0xff, 0xff, 0xff }, /* LDU_MODE_OFF */
			{ 0xfd, 0xfd, 0xff }, /* LDU_MODE_1 */
			{ 0xfb, 0xfb, 0xff }, /* LDU_MODE_2 */
			{ 0xf9, 0xf8, 0xff }, /* LDU_MODE_3 */
			{ 0xf6, 0xf6, 0xff }, /* LDU_MODE_4 */
			{ 0xf4, 0xf4, 0xff }, /* LDU_MODE_5 */
		},
		{
			/* adjust_ldu_data_2 */
			{ 0xff, 0xfc, 0xf6 }, /* LDU_MODE_OFF */
			{ 0xfd, 0xfa, 0xf6 }, /* LDU_MODE_1 */
			{ 0xfb, 0xf7, 0xf6 }, /* LDU_MODE_2 */
			{ 0xf9, 0xf5, 0xf6 }, /* LDU_MODE_3 */
			{ 0xf4, 0xf0, 0xf6 }, /* LDU_MODE_4 */
			{ 0xf2, 0xee, 0xf6 }, /* LDU_MODE_5 */
		},
	};
	struct pnobj_func f_init = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_init_default, oled_maptbl_init_default);
	struct pnobj_func f_getidx = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_getidx_default, oled_maptbl_getidx_default);
	struct pnobj_func f_copy = __PNOBJ_FUNC_INITIALIZER(oled_maptbl_copy_scr_white, oled_maptbl_copy_scr_white);
	struct maptbl test_maptbl = DEFINE_0D_MAPTBL(test_table, &f_init, &f_getidx, &f_copy);
	unsigned char dst[MAX_COLOR * 2] = { 0, };

	test_maptbl.pdata = to_panel_device(mdnie);
	test_maptbl.initialized = true;

	memcpy(mdnie->props.adjust_ldu_wrgb, adjust_ldu_wrgb, sizeof(adjust_ldu_wrgb));

	mdnie->props.scr_white_mode = SCR_WHITE_MODE_ADJUST_LDU;
	mdnie->props.scenario_mode = DYNAMIC;
	mdnie->props.ldu = LDU_MODE_OFF;

	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	KUNIT_EXPECT_EQ(test, adjust_ldu_wrgb[WCRD_TYPE_D65][LDU_MODE_OFF][0], mdnie->props.cur_wrgb[0]);
	KUNIT_EXPECT_EQ(test, adjust_ldu_wrgb[WCRD_TYPE_D65][LDU_MODE_OFF][1], mdnie->props.cur_wrgb[1]);
	KUNIT_EXPECT_EQ(test, adjust_ldu_wrgb[WCRD_TYPE_D65][LDU_MODE_OFF][2], mdnie->props.cur_wrgb[2]);

	KUNIT_EXPECT_EQ(test, adjust_ldu_wrgb[WCRD_TYPE_D65][LDU_MODE_OFF][0], dst[0]);
	KUNIT_EXPECT_EQ(test, adjust_ldu_wrgb[WCRD_TYPE_D65][LDU_MODE_OFF][1], dst[2]);
	KUNIT_EXPECT_EQ(test, adjust_ldu_wrgb[WCRD_TYPE_D65][LDU_MODE_OFF][2], dst[4]);

	mdnie->props.scenario_mode = AUTO;
	mdnie->props.scenario = EBOOK_MODE;
	mdnie->props.ldu = LDU_MODE_5;
	oled_maptbl_copy_scr_white(&test_maptbl, dst);
	KUNIT_EXPECT_EQ(test, adjust_ldu_wrgb[WCRD_TYPE_ADAPTIVE][LDU_MODE_5][0], mdnie->props.cur_wrgb[0]);
	KUNIT_EXPECT_EQ(test, adjust_ldu_wrgb[WCRD_TYPE_ADAPTIVE][LDU_MODE_5][1], mdnie->props.cur_wrgb[1]);
	KUNIT_EXPECT_EQ(test, adjust_ldu_wrgb[WCRD_TYPE_ADAPTIVE][LDU_MODE_5][2], mdnie->props.cur_wrgb[2]);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int oled_common_mdnie_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct mdnie_info *mdnie;

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);

	mdnie = &panel->mdnie;
	mdnie->props.scr_white_len = 2;

	test->priv = mdnie;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void oled_common_mdnie_test_exit(struct kunit *test)
{
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case oled_common_mdnie_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
#if !defined(CONFIG_UML)
	/* NOTE: Target running TC */
	KUNIT_CASE(oled_common_mdnie_test_foo),
#endif
	/* NOTE: UML TC */
	KUNIT_CASE(oled_common_mdnie_test_test),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_mdnie_scenario_mode_function_with_invalid_parameter),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_mdnie_scenario_mode_return_multiplication_nrow_and_mdnie_mode),
	KUNIT_CASE(oled_common_mdnie_test_update_mdnie_scenario_packet_success),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_mdnie_hdr_function_with_invalid_parameter),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_mdnie_hdr_return_multiplication_nrow_and_mdnie_mode),
	KUNIT_CASE(oled_common_mdnie_test_update_mdnie_hdr_packet_success),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_mdnie_trans_mode_function_with_invalid_parameter),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_mdnie_trans_mode_return_multiplication_nrow_and_mdnie_mode),
	KUNIT_CASE(oled_common_mdnie_test_update_packet_using_mdnie_trans_mode_maptbl),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_mdnie_night_mode_function_with_invalid_argument),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_mdnie_night_mode_function_success),
	KUNIT_CASE(oled_common_mdnie_test_update_packet_using_mdnie_night_mode_maptbl),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_color_lens_with_invalid_argument),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_color_lens_with_invalid_color_lens),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_getidx_color_lens_success),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_copy_scr_white_with_invalid_argument),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_copy_scr_white_overflow_by_def_wrgb_ofs_when_scr_white_mode_is_color_coordinate),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_copy_scr_white_overflow_by_def_wrgb_ofs_when_scr_white_mode_is_adjust_ldu),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_copy_scr_white_underflow_by_def_wrgb_ofs_when_scr_white_mode_is_color_coordinate),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_copy_scr_white_underflow_by_def_wrgb_ofs_when_scr_white_mode_is_adjust_ldu),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_copy_scr_white_check_implicit_type_promotion_when_scr_white_mode_is_color_coordinate),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_copy_scr_white_check_implicit_type_promotion_when_scr_white_mode_is_adjust_ldu),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_copy_scr_white_when_scr_white_mode_is_color_coordinate),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_copy_scr_white_when_scr_white_mode_is_sensor_rgb),
	KUNIT_CASE(oled_common_mdnie_test_oled_maptbl_copy_scr_white_when_scr_white_mode_is_adjust_ldu),
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
static struct kunit_suite oled_common_mdnie_test_module = {
	.name = "oled_common_mdnie_test",
	.init = oled_common_mdnie_test_init,
	.exit = oled_common_mdnie_test_exit,
	.test_cases = oled_common_mdnie_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&oled_common_mdnie_test_module);

MODULE_LICENSE("GPL v2");
