// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "panel_drv.h"
#include "maptbl.h"
#include "panel_obj.h"
#include "panel_packet.h"

struct panel_packet_test {
	struct panel_device *panel;
	struct pktinfo *static_packet;
};

static int test_index;
static int init_common_maptbl(struct maptbl *tbl) { return 0; }
static int getidx_test_maptbl(struct maptbl *m) { return test_index; }
static void copy_common_maptbl(struct maptbl *tbl, u8 *dst)
{
	int idx;

	idx = maptbl_getidx(tbl);
	if (idx < 0)
		return;

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * maptbl_get_sizeof_copy(tbl));
}

static DEFINE_PNOBJ_FUNC(init_common_maptbl, init_common_maptbl);
static DEFINE_PNOBJ_FUNC(getidx_test_maptbl, getidx_test_maptbl);
static DEFINE_PNOBJ_FUNC(copy_common_maptbl, copy_common_maptbl);

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
static void panel_packet_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_packet_test_update_tx_packet_should_fail_with_static_packet(struct kunit *test)
{
	u8 TEST_PACKET_DATA[] = { 0x11, 0x00 };
	DEFINE_STATIC_PACKET(test_static_packet, DSI_PKT_TYPE_WR, TEST_PACKET_DATA, 0);

	KUNIT_EXPECT_EQ(test, update_tx_packet(NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, update_tx_packet(&PKTINFO(test_static_packet)), -EINVAL);
}

static void panel_packet_test_update_tx_packet_should_fail_if_copy_to_out_of_bound(struct kunit *test)
{
	u8 test_table[2][4][1] = {
		{ { 0x00 }, { 0x01 }, { 0x02 }, { 0x03 } },
		{ { 0x10 }, { 0x11 }, { 0x12 }, { 0x13 } },
	};
	struct maptbl test_maptbl = DEFINE_3D_MAPTBL(test_table,
			&PNOBJ_FUNC(init_common_maptbl),
			&PNOBJ_FUNC(getidx_test_maptbl),
			&PNOBJ_FUNC(copy_common_maptbl));
	u8 TEST_PACKET_DATA[] = { 0x55, 0xFF };
	DECLARE_PKTUI(test_packet) = {
		{ .offset = 2, .maptbl = &test_maptbl },
	};
	DEFINE_VARIABLE_PACKET(test_packet, DSI_PKT_TYPE_WR, TEST_PACKET_DATA, 0);

	KUNIT_EXPECT_EQ(test, update_tx_packet(&PKTINFO(test_packet)), -EINVAL);
}

static void panel_packet_test_update_tx_packet_success(struct kunit *test)
{
	u8 test_table[2][4][1] = {
		{ { 0x00 }, { 0x01 }, { 0x02 }, { 0x03 } },
		{ { 0x10 }, { 0x11 }, { 0x12 }, { 0x13 } },
	};
	struct maptbl test_maptbl = DEFINE_3D_MAPTBL(test_table,
			&PNOBJ_FUNC(init_common_maptbl),
			&PNOBJ_FUNC(getidx_test_maptbl),
			&PNOBJ_FUNC(copy_common_maptbl));
	u8 TEST_PACKET_DATA[] = { 0x55, 0xFF };
	DECLARE_PKTUI(test_packet) = {
		{ .offset = 1, .maptbl = &test_maptbl },
	};
	DEFINE_VARIABLE_PACKET(test_packet, DSI_PKT_TYPE_WR, TEST_PACKET_DATA, 0);
	u8 *txbuf = get_pktinfo_txbuf(&PKTINFO(test_packet));

	KUNIT_ASSERT_EQ(test, (u32)TEST_PACKET_DATA[1], (u32)0xFF);

	test_index = maptbl_index(&test_maptbl, 0, 2, 0);
	KUNIT_EXPECT_EQ(test, update_tx_packet(&PKTINFO(test_packet)), 0);
	KUNIT_EXPECT_EQ(test, txbuf[1], (u8)0x02);
}

static void panel_packet_test_create_static_packet_success(struct kunit *test)
{
	struct pktinfo *tx_packet;
	u8 sleep_out[] = { 0x11 };
	struct panel_tx_msg tx_msg = {
		.gpara_offset = 0,
		.buf = sleep_out,
		.len = ARRAY_SIZE(sleep_out),
	};

	tx_packet = create_tx_packet("static_packet", DSI_PKT_TYPE_WR, &tx_msg, NULL, 0, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tx_packet);
	KUNIT_EXPECT_PTR_NE(test, (u8 *)get_pktinfo_txbuf(tx_packet), (u8 *)sleep_out);
	KUNIT_EXPECT_EQ(test, tx_packet->offset, tx_msg.gpara_offset);
	KUNIT_EXPECT_EQ(test, tx_packet->dlen, tx_msg.len);
	KUNIT_EXPECT_TRUE(test, !memcmp(get_pktinfo_txbuf(tx_packet), tx_msg.buf,
				tx_msg.len));
	destroy_tx_packet(tx_packet);
}

static void panel_packet_test_create_variable_packet_success(struct kunit *test)
{
	struct pktinfo *tx_packet;
	u8 te_frame_sel[] = {
		0xB9,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
	};
	struct panel_tx_msg tx_msg = {
		.gpara_offset = 0,
		.buf = te_frame_sel,
		.len = ARRAY_SIZE(te_frame_sel),
	};
	u8 te_mode_table[14][1] = {
		{ 0x61 },
		{ 0x61 },
		{ 0x61 },
		{ 0x61 },
		{ 0x61 },
		{ 0x61 },
		{ 0x61 },
		{ 0x61 },
		{ 0x61 },
		{ 0x04 },
		{ 0x61 },
		{ 0x04 },
		{ 0x61 },
		{ 0x61 },
	};
	u8 te_frame_sel_table[14][1] = {
		{ 0x03 },
		{ 0x03 },
		{ 0x01 },
		{ 0x03 },
		{ 0x07 },
		{ 0x09 },
		{ 0x17 },
		{ 0x01 },
		{ 0x03 },
		{ 0x00 },
		{ 0x07 },
		{ 0x00 },
		{ 0x07 },
		{ 0x13 },
	};
	u8 te_fixed_mode_timing_table[14][4] = {
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x09, 0x23, 0x00, 0x0F },
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x09, 0x23, 0x00, 0x0F },
		{ 0x00, 0x01, 0x00, 0x00 },
		{ 0x00, 0x01, 0x00, 0x00 },
	};
	u8 te_timing_table[14][8] = {
		{ 0x02, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x00, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x04, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x04, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x04, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x04, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x04, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x03, 0x4A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x03, 0x4A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
		{ 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 },
	};
	struct maptbl te_mode_maptbl = DEFINE_2D_MAPTBL(te_mode_table, NULL, NULL, NULL);
	struct maptbl te_frame_sel_maptbl = DEFINE_2D_MAPTBL(te_frame_sel_table, NULL, NULL, NULL);
	struct maptbl te_fixed_mode_timing_maptbl = DEFINE_2D_MAPTBL(te_fixed_mode_timing_table, NULL, NULL, NULL);
	struct maptbl te_timing_maptbl = DEFINE_2D_MAPTBL(te_timing_table, NULL, NULL, NULL);
	DECLARE_PKTUI(te_frame_sel) = {
		{ .offset = 1, .maptbl = &te_mode_maptbl },
		{ .offset = 3, .maptbl = &te_frame_sel_maptbl },
		{ .offset = 5, .maptbl = &te_fixed_mode_timing_maptbl },
		{ .offset = 13, .maptbl = &te_timing_maptbl },
	};

	tx_packet = create_tx_packet("variable_packet", DSI_PKT_TYPE_WR, &tx_msg,
			PKTUI(te_frame_sel), ARRAY_SIZE(PKTUI(te_frame_sel)), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tx_packet);
	KUNIT_EXPECT_PTR_NE(test, (u8 *)get_pktinfo_txbuf(tx_packet), (u8 *)te_frame_sel);
	KUNIT_EXPECT_PTR_NE(test, (u8 *)get_pktinfo_initdata(tx_packet), (u8 *)get_pktinfo_txbuf(tx_packet));
	KUNIT_EXPECT_EQ(test, tx_packet->offset, tx_msg.gpara_offset);
	KUNIT_EXPECT_EQ(test, tx_packet->dlen, tx_msg.len);
	KUNIT_EXPECT_TRUE(test, !memcmp(get_pktinfo_txbuf(tx_packet), tx_msg.buf,
				tx_msg.len));

	KUNIT_EXPECT_PTR_NE(test, (struct pkt_update_info *)tx_packet->pktui,
		(struct pkt_update_info *)PKTUI(te_frame_sel));
	KUNIT_EXPECT_TRUE(test, !memcmp(tx_packet->pktui, &PKTUI(te_frame_sel),
			sizeof(PKTUI(te_frame_sel))));
	KUNIT_EXPECT_EQ(test, tx_packet->nr_pktui, (u32)ARRAY_SIZE(PKTUI(te_frame_sel)));

	destroy_tx_packet(tx_packet);
}

static void panel_packet_test_create_rx_packet_success(struct kunit *test)
{
	struct rdinfo *rx_packet;
	struct panel_rx_msg rx_msg = {
		.addr = 0xA1,
		.gpara_offset = 4,
		.len = 7,
	};

	rx_packet = create_rx_packet("rx_packet", DSI_PKT_TYPE_RD, &rx_msg);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, rx_packet);
	KUNIT_EXPECT_EQ(test, rx_packet->offset, rx_msg.gpara_offset);
	KUNIT_EXPECT_EQ(test, rx_packet->len, rx_msg.len);
	destroy_rx_packet(rx_packet);
}

static void panel_packet_test_create_rx_packet_should_allocate_with_data(struct kunit *test)
{
	struct rdinfo *rdi;
	struct panel_rx_msg rx_msg = {
		.addr = 0x04,
		.gpara_offset = 0,
		.len = 3,
	};

	rdi = create_rx_packet("test_rd_info", DSI_PKT_TYPE_RD, &rx_msg);
	KUNIT_EXPECT_EQ(test, get_pnobj_cmd_type(&rdi->base), (u32)CMD_TYPE_RX_PACKET);
	KUNIT_EXPECT_EQ(test, get_rdinfo_type(rdi), (u32)DSI_PKT_TYPE_RD);
	KUNIT_EXPECT_STREQ(test, get_rdinfo_name(rdi), "test_rd_info");
	KUNIT_EXPECT_EQ(test, rdi->addr, (u32)0x04);
	KUNIT_EXPECT_EQ(test, rdi->offset, (u32)0);
	KUNIT_EXPECT_EQ(test, rdi->len, (u32)3);
	destroy_rx_packet(rdi);
}

static void panel_packet_test_create_rx_packet_should_return_null_with_zero_len(struct kunit *test)
{
	struct rdinfo *rdi;
	struct panel_rx_msg rx_msg = {
		.addr = 0x04,
		.gpara_offset = 3,
		.len = 0,
	};

	rdi = create_rx_packet("test_rd_info", DSI_PKT_TYPE_RD, &rx_msg);
	KUNIT_EXPECT_TRUE(test, !rdi);
}

static void panel_packet_test_create_rx_packet_should_return_null_with_no_name(struct kunit *test)
{
	struct rdinfo *rdi;
	struct panel_rx_msg rx_msg = {
		.addr = 0x04,
		.gpara_offset = 0,
		.len = 3,
	};

	rdi = create_rx_packet(NULL, DSI_PKT_TYPE_RD, &rx_msg);
	KUNIT_EXPECT_TRUE(test, !rdi);
}

static void panel_packet_test_create_rx_packet_should_return_null_with_zero_addr(struct kunit *test)
{
	struct rdinfo *rdi;
	struct panel_rx_msg rx_msg = {
		.addr = 0,
		.gpara_offset = 0,
		.len = 3,
	};

	rdi = create_rx_packet("test_rd_info", DSI_PKT_TYPE_RD, &rx_msg);
	KUNIT_EXPECT_TRUE(test, !rdi);
}

static void panel_packet_test_create_rx_packet_should_return_null_with_invalid_type(struct kunit *test)
{
	struct rdinfo *rdi;
	struct panel_rx_msg rx_msg = {
		.addr = 0x04,
		.gpara_offset = 3,
		.len = 3,
	};

	rdi = create_rx_packet("test_rd_info", DSI_PKT_TYPE_WR_MEM, &rx_msg);
	KUNIT_ASSERT_TRUE(test, !rdi);

	rdi = create_rx_packet("test_rd_info", SPI_PKT_TYPE_WR, &rx_msg);
	KUNIT_ASSERT_TRUE(test, !rdi);
}

static void panel_packet_test_destroy_rx_packet_should_not_occur_exception_with_null_pointer(struct kunit *test)
{
	destroy_rx_packet(NULL);
}

static void panel_packet_test_add_tx_packet_on_pnobj_refs_fail_with_invalid_args(struct kunit *test)
{
	struct panel_packet_test *ctx = test->priv;
	struct pnobj_refs pkt_refs;

	INIT_PNOBJ_REFS(&pkt_refs);

	KUNIT_EXPECT_EQ(test, add_tx_packet_on_pnobj_refs(ctx->static_packet, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, add_tx_packet_on_pnobj_refs(NULL, &pkt_refs), -EINVAL);
}

static void panel_packet_test_add_tx_packet_on_pnobj_refs_success(struct kunit *test)
{
	struct panel_packet_test *ctx = test->priv;
	struct pnobj_refs pkt_refs;

	INIT_PNOBJ_REFS(&pkt_refs);

	KUNIT_EXPECT_EQ(test, add_tx_packet_on_pnobj_refs(ctx->static_packet, &pkt_refs), 0);
	KUNIT_EXPECT_EQ(test, add_tx_packet_on_pnobj_refs(ctx->static_packet, &pkt_refs), 0);
	KUNIT_EXPECT_EQ(test, add_tx_packet_on_pnobj_refs(ctx->static_packet, &pkt_refs), 0);
	remove_all_pnobj_ref(&pkt_refs);
}

static void panel_packet_test_create_key_packet_test(struct kunit *test)
{
	struct panel_packet_test *ctx = test->priv;
	struct keyinfo *key0 = create_key_packet("level_0_key",
			CMD_LEVEL_NONE, KEY_NONE, ctx->static_packet);
	struct keyinfo *key1 = create_key_packet("level_1_key",
			CMD_LEVEL_1, KEY_DISABLE, ctx->static_packet);
	struct keyinfo *key2 = create_key_packet("level_2_key",
			CMD_LEVEL_2, KEY_ENABLE, ctx->static_packet);
	struct keyinfo *key3 = create_key_packet("level_3_key",
			3, KEY_ENABLE, ctx->static_packet);

	KUNIT_EXPECT_TRUE(test, !create_key_packet(NULL, CMD_LEVEL_NONE, KEY_NONE, ctx->static_packet));
	KUNIT_EXPECT_TRUE(test, !create_key_packet("test_key", CMD_LEVEL_NONE, MAX_KEY_TYPE, ctx->static_packet));
	KUNIT_EXPECT_TRUE(test, !create_key_packet("test_key", CMD_LEVEL_NONE, KEY_NONE, NULL));

	KUNIT_EXPECT_TRUE(test, key0 != NULL);
	KUNIT_EXPECT_EQ(test, key0->level, CMD_LEVEL_NONE);
	KUNIT_EXPECT_EQ(test, key0->en, KEY_NONE);
	destroy_key_packet(key0);

	KUNIT_EXPECT_TRUE(test, key1 != NULL);
	KUNIT_EXPECT_EQ(test, key1->level, CMD_LEVEL_1);
	KUNIT_EXPECT_EQ(test, key1->en, KEY_DISABLE);
	destroy_key_packet(key1);

	KUNIT_EXPECT_TRUE(test, key2 != NULL);
	KUNIT_EXPECT_EQ(test, key2->level, CMD_LEVEL_2);
	KUNIT_EXPECT_EQ(test, key2->en, KEY_ENABLE);
	destroy_key_packet(key2);

	KUNIT_EXPECT_TRUE(test, key3 != NULL);
	KUNIT_EXPECT_EQ(test, key3->level, 3);
	KUNIT_EXPECT_EQ(test, key3->en, KEY_ENABLE);
	destroy_key_packet(key3);
}

static void panel_packet_test_duplicate_key_packet_test(struct kunit *test)
{
	struct panel_packet_test *ctx = test->priv;
	struct keyinfo *key0 = create_key_packet("level_0_key",
			CMD_LEVEL_NONE, KEY_NONE, ctx->static_packet);
	struct keyinfo *key1 = create_key_packet("level_1_key",
			CMD_LEVEL_1, KEY_DISABLE, ctx->static_packet);
	struct keyinfo *key2 = create_key_packet("level_2_key",
			CMD_LEVEL_2, KEY_ENABLE, ctx->static_packet);
	struct keyinfo *key3 = create_key_packet("level_3_key",
			3, KEY_ENABLE, ctx->static_packet);
	struct keyinfo *key_dup;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, key0);
	key_dup = duplicate_key_packet(key0);
	KUNIT_EXPECT_TRUE(test, key_dup != NULL);
	KUNIT_EXPECT_EQ(test, key_dup->level, key0->level);
	KUNIT_EXPECT_EQ(test, key_dup->en, key0->en);
	destroy_key_packet(key_dup);
	destroy_key_packet(key0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, key1);
	key_dup = duplicate_key_packet(key1);
	KUNIT_EXPECT_TRUE(test, key_dup != NULL);
	KUNIT_EXPECT_EQ(test, key_dup->level, key1->level);
	KUNIT_EXPECT_EQ(test, key_dup->en, key1->en);
	destroy_key_packet(key_dup);
	destroy_key_packet(key1);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, key2);
	key_dup = duplicate_key_packet(key2);
	KUNIT_EXPECT_TRUE(test, key_dup != NULL);
	KUNIT_EXPECT_EQ(test, key_dup->level, key2->level);
	KUNIT_EXPECT_EQ(test, key_dup->en, key2->en);
	destroy_key_packet(key_dup);
	destroy_key_packet(key2);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, key3);
	key_dup = duplicate_key_packet(key3);
	KUNIT_EXPECT_TRUE(test, key_dup != NULL);
	KUNIT_EXPECT_EQ(test, key_dup->level, key3->level);
	KUNIT_EXPECT_EQ(test, key_dup->en, key3->en);
	destroy_key_packet(key_dup);
	destroy_key_packet(key3);
}


/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_packet_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_packet_test *ctx = kunit_kzalloc(test,
			sizeof(struct panel_packet_test), GFP_KERNEL);
	u8 test_packet_data[] = { 0x55, 0xF1, 0xF2 };
	struct panel_tx_msg tx_msg = {
		.gpara_offset = 0,
		.buf = test_packet_data,
		.len = ARRAY_SIZE(test_packet_data),
	};

	panel = kunit_kzalloc(test, sizeof(*panel), GFP_KERNEL);
	INIT_LIST_HEAD(&panel->prop_list);
	ctx->panel = panel;
	ctx->static_packet = create_tx_packet("static_packet", DSI_PKT_TYPE_WR, &tx_msg, NULL, 0, 0);
	test->priv = ctx;

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_packet_test_exit(struct kunit *test)
{
	struct panel_packet_test *ctx = test->priv;

	destroy_tx_packet(ctx->static_packet);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_packet_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_packet_test_test),
	KUNIT_CASE(panel_packet_test_update_tx_packet_should_fail_with_static_packet),
	KUNIT_CASE(panel_packet_test_update_tx_packet_should_fail_if_copy_to_out_of_bound),
	KUNIT_CASE(panel_packet_test_update_tx_packet_success),
	KUNIT_CASE(panel_packet_test_create_static_packet_success),
	KUNIT_CASE(panel_packet_test_create_variable_packet_success),
	KUNIT_CASE(panel_packet_test_create_rx_packet_success),
	KUNIT_CASE(panel_packet_test_create_rx_packet_should_allocate_with_data),
	KUNIT_CASE(panel_packet_test_create_rx_packet_should_return_null_with_zero_len),
	KUNIT_CASE(panel_packet_test_create_rx_packet_should_return_null_with_no_name),
	KUNIT_CASE(panel_packet_test_create_rx_packet_should_return_null_with_zero_addr),
	KUNIT_CASE(panel_packet_test_create_rx_packet_should_return_null_with_invalid_type),
	KUNIT_CASE(panel_packet_test_destroy_rx_packet_should_not_occur_exception_with_null_pointer),
	KUNIT_CASE(panel_packet_test_add_tx_packet_on_pnobj_refs_fail_with_invalid_args),
	KUNIT_CASE(panel_packet_test_add_tx_packet_on_pnobj_refs_success),
	KUNIT_CASE(panel_packet_test_create_key_packet_test),
	KUNIT_CASE(panel_packet_test_duplicate_key_packet_test),
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
static struct kunit_suite panel_packet_test_module = {
	.name = "panel_packet_test",
	.init = panel_packet_test_init,
	.exit = panel_packet_test_exit,
	.test_cases = panel_packet_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_packet_test_module);

MODULE_LICENSE("GPL v2");
