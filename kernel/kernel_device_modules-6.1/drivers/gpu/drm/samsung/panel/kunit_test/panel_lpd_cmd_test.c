// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_obj.h"
#include "panel_lpd_cmd.h"
#include "lpd_config_for_uml.h"
#include "test_panel.h"

extern int panel_setup_command_initdata_list(struct panel_device *panel, struct common_panel_info *info);
extern int panel_sort_command_initdata_list(struct panel_device *panel, struct common_panel_info *info);
extern int panel_prepare_pnobj_list(struct panel_device *panel);
extern int panel_unprepare_pnobj_list(struct panel_device *panel);

extern int load_lpd_br_cmd(struct panel_device *panel, struct lpd_panel_cmd *panel_cmd);
extern unsigned int lpd_br_table[10];

struct panel_lpd_cmd_test {
	struct panel_device *panel;
	struct lpd_panel_cmd *lpd_cmd;
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
static void panel_lpd_cmd_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_lpd_cmd_test_load_lpd_br_cmd_fail_with_invalid_args(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, load_lpd_br_cmd(NULL, NULL), -EINVAL);
}

static void panel_lpd_cmd_test_load_lpd_br_cmd_success(struct kunit *test)
{
	struct panel_lpd_cmd_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct lpd_panel_cmd *lpd_cmd = ctx->lpd_cmd;
	u8 *dbuf, *buf = lpd_cmd->cmd_buf.buf;
	struct cmd_seq_header *seq_header;
	extern u8 watch_brt_table[10][2];
	extern u8 watch_02_brt_table[10][2];
	int i;
	u8 val;

	/* case 1 */
	panel_set_property(panel, &panel->panel_data.id[2], 0x00);
	KUNIT_ASSERT_EQ(test, probe_lpd_panel_cmd(panel), 0);
	KUNIT_ASSERT_TRUE(test, !list_empty(get_pnobj_refs_list(&panel->lpd_br_list)));
	KUNIT_ASSERT_EQ(test, get_count_of_pnobj_ref(&panel->lpd_br_list), 1);

	KUNIT_ASSERT_EQ(test, load_lpd_br_cmd(panel, lpd_cmd), 0);
	seq_header = (struct cmd_seq_header *)buf;
	KUNIT_EXPECT_EQ(test, seq_header->seq_id, (u32)LPD_BR_SEQ);
	KUNIT_EXPECT_EQ(test, seq_header->total_size,
			/* SEQ_HEADER: cmd_seq_header + BR[10] + CMD_OFFSET[10] */
			(u32)((sizeof(struct cmd_seq_header) + sizeof(u32) * 10U + sizeof(u32) * 1U) +
			/* SEQ_BODY: begin */
				/* DCMD_HEADER: dynamic_cmd_header + DCMD_OFFSET[10] */
				(sizeof(struct dynamic_cmd_header) + sizeof(u32) * 10U) +
				/* DCMD_BODY: DCMD_PAYLOAD[10] */
				(3U * 10U))
			/* SEQ_BODY: end */
			);
	KUNIT_EXPECT_EQ(test, seq_header->br_count, 10U);
	KUNIT_EXPECT_EQ(test, seq_header->cmd_count, 1U);
	KUNIT_EXPECT_EQ(test, seq_header->magic_code1, CMD_SEQ_MAGIC_CODE1);
	KUNIT_EXPECT_EQ(test, seq_header->magic_code2, CMD_SEQ_MAGIC_CODE2);

	for (i = 0; i < 10; i++) {
		dbuf = buf + GET_SEQ_CMD_OFFSET(buf, 10, 0);
		val = dbuf[GET_DLIST_IDX_VALUE(dbuf, i) + 2];
		KUNIT_EXPECT_EQ(test, watch_brt_table[i][1], val);
	}
	remove_all_pnobj_ref(&panel->lpd_br_list);

	/* case 2 */
	panel_set_property(panel, &panel->panel_data.id[2], 0x02);
	KUNIT_ASSERT_EQ(test, probe_lpd_panel_cmd(panel), 0);
	KUNIT_ASSERT_EQ(test, load_lpd_br_cmd(panel, lpd_cmd), 0);
	seq_header = (struct cmd_seq_header *)buf;
	KUNIT_EXPECT_EQ(test, seq_header->seq_id, (u32)LPD_BR_SEQ);
	KUNIT_EXPECT_EQ(test, seq_header->total_size,
			/* SEQ_HEADER: cmd_seq_header + BR[10] + CMD_OFFSET[10] */
			(u32)((sizeof(struct cmd_seq_header) + sizeof(u32) * 10U + sizeof(u32) * 1U) +
			/* SEQ_BODY: begin */
				/* DCMD_HEADER: dynamic_cmd_header + DCMD_OFFSET[10] */
				(sizeof(struct dynamic_cmd_header) + sizeof(u32) * 10U) +
				/* DCMD_BODY: DCMD_PAYLOAD[10] */
				(3U * 10U))
			/* SEQ_BODY: end */
			);
	KUNIT_EXPECT_EQ(test, seq_header->br_count, 10U);
	KUNIT_EXPECT_EQ(test, seq_header->cmd_count, 1U);
	KUNIT_EXPECT_EQ(test, seq_header->magic_code1, CMD_SEQ_MAGIC_CODE1);
	KUNIT_EXPECT_EQ(test, seq_header->magic_code2, CMD_SEQ_MAGIC_CODE2);

	for (i = 0; i < 10; i++) {
		dbuf = buf + GET_SEQ_CMD_OFFSET(buf, 10, 0);
		val = dbuf[GET_DLIST_IDX_VALUE(dbuf, i) + 2];
		KUNIT_EXPECT_EQ(test, watch_02_brt_table[i][1], val);
	}
	remove_all_pnobj_ref(&panel->lpd_br_list);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_lpd_cmd_test_init(struct kunit *test)
{
	struct panel_lpd_cmd_test *ctx = kunit_kzalloc(test,
			sizeof(struct panel_lpd_cmd_test), GFP_KERNEL);
	struct panel_prop_enum_item panel_state_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ON),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_NORMAL),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(PANEL_STATE_ALPM),
	};
	struct panel_prop_enum_item wait_tx_done_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_AUTO),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(WAIT_TX_DONE_MANUAL_ON),
	};
	struct panel_prop_enum_item separate_tx_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_OFF),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(SEPARATE_TX_ON),
	};
	struct panel_prop_enum_item vrr_mode_enum_items[] = {
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_NORMAL_MODE),
		__PANEL_PROPERTY_ENUM_ITEM_INITIALIZER(VRR_HS_MODE),
	};
	struct panel_prop_list prop_array[] = {
		/* enum property */
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PANEL_STATE,
				PANEL_STATE_OFF, panel_state_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_WAIT_TX_DONE,
				WAIT_TX_DONE_AUTO, wait_tx_done_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_SEPARATE_TX,
				SEPARATE_TX_OFF, separate_tx_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_MODE,
				VRR_NORMAL_MODE, vrr_mode_enum_items),
		__PANEL_PROPERTY_ENUM_INITIALIZER(PANEL_PROPERTY_PREV_PANEL_REFRESH_MODE,
				VRR_NORMAL_MODE, vrr_mode_enum_items),
		/* range property */
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_1,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_2,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_ID_3,
				0, 0, 0xFF),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PANEL_REFRESH_RATE,
				60, 0, 120),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_PREV_PANEL_REFRESH_RATE,
				60, 0, 120),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_RESOLUTION_CHANGED,
				false, false, true),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_DSI_FREQ,
				0, 0, 1000000000),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_OSC_FREQ,
				0, 0, 1000000000),
#ifdef CONFIG_USDM_FACTORY
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_IS_FACTORY_MODE,
				1, 0, 1),
#else
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_IS_FACTORY_MODE,
				0, 0, 1),
#endif
		/* DISPLAY TEST */
#ifdef CONFIG_USDM_FACTORY_BRIGHTDOT_TEST
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_PROPERTY_BRIGHTDOT_TEST_ENABLE,
				0, 0, 1),
#endif
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_BL_PROPERTY_BRIGHTNESS,
				128, 0, 1000000000),
		__PANEL_PROPERTY_RANGE_INITIALIZER(PANEL_BL_PROPERTY_PREV_BRIGHTNESS,
				128, 0, 1000000000),
	};
	struct panel_device *panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	struct lpd_panel_cmd *lpd_cmd = kunit_kzalloc(test, sizeof(*lpd_cmd), GFP_KERNEL);

	lpd_cmd->cmd_buf.buf = kunit_kzalloc(test, SZ_8K, GFP_KERNEL);
	lpd_cmd->cmd_buf.buf_size = SZ_8K;
	memcpy(lpd_cmd->br_info.br_list, lpd_br_table, sizeof(lpd_br_table));
	lpd_cmd->br_info.br_cnt = ARRAY_SIZE(lpd_br_table);

	panel_mutex_init(panel, &panel->io_lock);
	panel_mutex_init(panel, &panel->op_lock);
	panel_mutex_init(panel, &panel->cmdq.lock);
	panel_mutex_init(panel, &panel->panel_bl.lock);
#ifdef CONFIG_USDM_MDNIE
	panel_mutex_init(panel, &panel->mdnie.lock);
#endif

	INIT_LIST_HEAD(&panel->command_initdata_list);

	INIT_LIST_HEAD(&panel->maptbl_list);
	INIT_LIST_HEAD(&panel->seq_list);
	INIT_LIST_HEAD(&panel->rdi_list);
	INIT_LIST_HEAD(&panel->res_list);
	INIT_LIST_HEAD(&panel->dump_list);
	INIT_LIST_HEAD(&panel->key_list);
	INIT_LIST_HEAD(&panel->pkt_list);
	INIT_LIST_HEAD(&panel->dly_list);
	INIT_LIST_HEAD(&panel->cond_list);
	INIT_LIST_HEAD(&panel->pwrctrl_list);
	INIT_LIST_HEAD(&panel->cfg_list);
	INIT_LIST_HEAD(&panel->func_list);

	INIT_LIST_HEAD(&panel->gpio_list);
	INIT_LIST_HEAD(&panel->power_ctrl_list);
	INIT_LIST_HEAD(&panel->regulator_list);
	INIT_LIST_HEAD(&panel->panel_lut_list);
	INIT_LIST_HEAD(&panel->prop_list);

	KUNIT_ASSERT_TRUE(test, !panel_add_property_from_array(panel,
			prop_array, ARRAY_SIZE(prop_array)));

	KUNIT_ASSERT_TRUE(test, panel_setup_command_initdata_list(panel, &test_panel_info) == 0);
	KUNIT_ASSERT_EQ(test, panel_sort_command_initdata_list(panel, NULL), 0);
	KUNIT_ASSERT_EQ(test, panel_prepare_pnobj_list(panel), 0);

	ctx->panel = panel;
	ctx->lpd_cmd = lpd_cmd;
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_lpd_cmd_test_exit(struct kunit *test)
{
	struct panel_lpd_cmd_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	panel_delete_property_all(panel);
	KUNIT_ASSERT_TRUE(test, list_empty(&panel->prop_list));
	KUNIT_ASSERT_EQ(test, panel_unprepare_pnobj_list(panel), 0);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_lpd_cmd_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_lpd_cmd_test_test),
	KUNIT_CASE(panel_lpd_cmd_test_load_lpd_br_cmd_fail_with_invalid_args),
	KUNIT_CASE(panel_lpd_cmd_test_load_lpd_br_cmd_success),
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
static struct kunit_suite panel_lpd_cmd_test_module = {
	.name = "panel_lpd_cmd_test",
	.init = panel_lpd_cmd_test_init,
	.exit = panel_lpd_cmd_test_exit,
	.test_cases = panel_lpd_cmd_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_lpd_cmd_test_module);

MODULE_LICENSE("GPL v2");
