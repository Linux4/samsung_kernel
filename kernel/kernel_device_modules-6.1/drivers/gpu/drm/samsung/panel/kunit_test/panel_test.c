// SPDX-License-Identifier: GPL-2.0
/*
 * TODO: Add test description.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include "test_helper.h"
#include "test_panel.h"
#include "panel.h"
#include "panel_drv.h"
#include "maptbl.h"

struct panel_test {
	struct panel_device *panel;
	struct common_panel_info *cpi;
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

extern int panel_do_dsi_tx_packet(struct panel_device *panel, struct pktinfo *info, bool block);
extern u8 dsi_tx_packet_type_to_cmd_id(u32 packet_type);
extern int panel_dsi_write_img(struct panel_device *panel,
		u8 cmd_id, const u8 *buf, u32 ofs, int size, u32 option);
extern int panel_do_power_ctrl(struct panel_device *panel, struct pwrctrl *info);
extern struct panel_dt_lut *find_panel_lut(struct panel_device *panel, u32 id);
extern u32 get_frame_delay(struct panel_device *panel, struct delayinfo *delay);
extern u32 get_total_delay(struct panel_device *panel, struct delayinfo *delay);
extern u32 get_remaining_delay(struct panel_device *panel, struct delayinfo *delay,
		ktime_t start, ktime_t now);
extern int strcmp_suffix(const char *str, const char *suffix);
extern int panel_prepare(struct panel_device *panel, struct common_panel_info *info);
extern int panel_unprepare(struct panel_device *panel);
extern struct rdinfo *find_panel_rdinfo(struct panel_device *panel, char *name);
extern int panel_setup_command_initdata_list(struct panel_device *panel,
		struct common_panel_info *info);

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
static void panel_test_test(struct kunit *test)
{
	/* Test cases for UML */
	KUNIT_EXPECT_EQ(test, 1, 1); // Pass
}

static void panel_test_dsi_tx_packet_type_to_cmd_id_success(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (u32)dsi_tx_packet_type_to_cmd_id(DSI_PKT_TYPE_WR_COMP), (u32)MIPI_DSI_WR_DSC_CMD);
	KUNIT_EXPECT_EQ(test, (u32)dsi_tx_packet_type_to_cmd_id(DSI_PKT_TYPE_WR_PPS), (u32)MIPI_DSI_WR_PPS_CMD);
	KUNIT_EXPECT_EQ(test, (u32)dsi_tx_packet_type_to_cmd_id(DSI_PKT_TYPE_WR), (u32)MIPI_DSI_WR_GEN_CMD);
	KUNIT_EXPECT_EQ(test, (u32)dsi_tx_packet_type_to_cmd_id(DSI_PKT_TYPE_WR_MEM), (u32)MIPI_DSI_WR_GRAM_CMD);
	KUNIT_EXPECT_EQ(test, (u32)dsi_tx_packet_type_to_cmd_id(DSI_PKT_TYPE_WR_SR), (u32)MIPI_DSI_WR_SRAM_CMD);
	KUNIT_EXPECT_EQ(test, (u32)dsi_tx_packet_type_to_cmd_id(DSI_PKT_TYPE_WR_SR_FAST), (u32)MIPI_DSI_WR_SR_FAST_CMD);
}

static void panel_test_panel_dsi_write_img_fail_with_invalid_args(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	u8 buffer[8] = { 0x00, };
	u32 offset = 0, option = 0;

	KUNIT_EXPECT_EQ(test, panel_dsi_write_img(NULL, MIPI_DSI_WR_GEN_CMD, buffer, offset, 8, option), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_dsi_write_img(panel, 0, buffer, offset, 8, option), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_dsi_write_img(panel, MIPI_DSI_WR_GEN_CMD, NULL, offset, 8, option), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_dsi_write_img(panel, MIPI_DSI_WR_GEN_CMD, buffer, offset, 0, option), -EINVAL);
}

static void panel_test_panel_dsi_write_img_fail_with_invalid_fifo_size(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	u8 buffer[8] = { 0x00, };
	u32 offset = 0, option = 0;

	panel->panel_data.ddi_props.img_fifo_size = -1;

	KUNIT_EXPECT_EQ(test, panel_dsi_write_img(panel, MIPI_DSI_WR_GEN_CMD, buffer, offset, 8, option), -EINVAL);
}

DEFINE_FUNCTION_MOCK(panel_cmdq_flush, RETURNS(int), PARAMS(struct panel_device *));

static void panel_test_panel_dsi_write_img_success_with_16byte_align(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	static const int SZ_DATA = 128;
	static const u32 SZ_PACKET = 128;
	u8 buffer[128] = {
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
	};
	u32 offset = 0, option = PKT_OPTION_SR_ALIGN_16;

	panel->panel_data.ddi_props.img_fifo_size = 0;

	KUNIT_EXPECT_EQ(test, panel_dsi_write_img(panel, MIPI_DSI_WR_GRAM_CMD, buffer, offset, SZ_DATA, option), SZ_DATA);
	KUNIT_EXPECT_EQ(test, panel_cmdq_get_size(panel), 1);
	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[0].size, (int)(SZ_PACKET + 1));
	KUNIT_EXPECT_EQ(test, *panel->cmdq.cmd[0].buf, (u8)MIPI_DCS_WRITE_GRAM_START);
	KUNIT_EXPECT_TRUE(test, !memcmp(panel->cmdq.cmd[0].buf + 1, buffer, SZ_DATA));
}

static void panel_test_panel_dsi_write_img_success_with_65byte_fifo_16byte_align(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	static const int DATA_SIZE = 128;
	static const u32 SZ_PACKET_1 = 64;
	static const u32 SZ_DATA_1 = 64;
	static const u32 SZ_PACKET_2 = 64;
	static const u32 SZ_DATA_2 = 64;
	int checked_pos = 0;
	u8 buffer[128] = {
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
	};
	u32 offset = 0, option = PKT_OPTION_SR_ALIGN_16;

	panel->panel_data.ddi_props.img_fifo_size = 65;

	KUNIT_EXPECT_EQ(test, panel_dsi_write_img(panel, MIPI_DSI_WR_GRAM_CMD, buffer, offset, DATA_SIZE, option), DATA_SIZE);
	KUNIT_EXPECT_EQ(test, panel_cmdq_get_size(panel), 2);

	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[0].size, (int)SZ_PACKET_1 + 1);
	KUNIT_EXPECT_EQ(test, *panel->cmdq.cmd[0].buf, (u8)MIPI_DCS_WRITE_GRAM_START);
	KUNIT_EXPECT_TRUE(test, !memcmp(panel->cmdq.cmd[0].buf + 1, buffer, SZ_PACKET_1));
	checked_pos += SZ_PACKET_1;

	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[1].size, (int)SZ_PACKET_2 + 1);
	KUNIT_EXPECT_EQ(test, *panel->cmdq.cmd[1].buf, (u8)MIPI_DCS_WRITE_GRAM_CONTINUE);
	KUNIT_EXPECT_TRUE(test, !memcmp(panel->cmdq.cmd[1].buf + 1, buffer + SZ_DATA_1, SZ_DATA_2));
}

static void panel_test_panel_dsi_write_img_success_with_16byte_align_mismatch_filled_zero(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	static const int SZ_DATA = 33;
	static const u32 SZ_PACKET = 48;
	u8 buffer[33] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x03,
	};
	u8 *remainer_buf;
	u32 offset = 0, option = PKT_OPTION_SR_ALIGN_16;

	panel->panel_data.ddi_props.img_fifo_size = 0;

	KUNIT_EXPECT_EQ(test, panel_dsi_write_img(panel, MIPI_DSI_WR_GRAM_CMD, buffer, offset, SZ_DATA, option), SZ_DATA);
	/* expect one 48byte cmd with 1byte header, 33byte data and zero-filled 15byte data */
	KUNIT_EXPECT_EQ(test, panel_cmdq_get_size(panel), 1);
	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[0].size, (int)SZ_PACKET + 1);
	KUNIT_EXPECT_EQ(test, *panel->cmdq.cmd[0].buf, (u8)MIPI_DCS_WRITE_GRAM_START);
	KUNIT_EXPECT_TRUE(test, !memcmp(panel->cmdq.cmd[0].buf + 1, buffer, SZ_DATA));

	remainer_buf = kunit_kzalloc(test, SZ_PACKET - SZ_DATA, GFP_KERNEL);
	KUNIT_EXPECT_TRUE(test, !memcmp(panel->cmdq.cmd[0].buf + 1 + SZ_DATA, remainer_buf, SZ_PACKET - SZ_DATA));
}

static void panel_test_panel_dsi_write_img_success_with_64byte_fifo_12byte_align_mismatch_filled_zero(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	static const u32 DATA_SIZE = 97;
	static const u32 SZ_PACKET_1 = 60;
	static const u32 SZ_DATA_1 = 60;
	static const u32 SZ_PACKET_2 = 48;
	static const u32 SZ_DATA_2 = 37;
	u32 checked_pos;
	u8 buffer[97] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x09,
	};
	u8 *remainer_buf;
	u32 offset = 0, option = PKT_OPTION_SR_ALIGN_12;

	panel->panel_data.ddi_props.img_fifo_size = 64;

	KUNIT_EXPECT_EQ(test, panel_dsi_write_img(panel, MIPI_DSI_WR_GRAM_CMD, buffer, offset, DATA_SIZE, option), (int)DATA_SIZE);
	KUNIT_EXPECT_EQ(test, panel_cmdq_get_size(panel), 2);
	checked_pos = 0;

	/* compare first packet - 60bytes data */
	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[0].size, (int)SZ_PACKET_1 + 1);
	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[0].cmd_id, (u8)MIPI_DSI_WR_GEN_CMD);
	KUNIT_EXPECT_EQ(test, *panel->cmdq.cmd[0].buf, (u8)MIPI_DCS_WRITE_GRAM_START);
	KUNIT_EXPECT_TRUE(test, !memcmp(panel->cmdq.cmd[0].buf + 1, buffer, SZ_DATA_1));

	/* compare second packet - 37bytes data + 11bytes zero filled */
	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[1].size, (int)SZ_PACKET_2 + 1);
	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[1].cmd_id, (u8)MIPI_DSI_WR_GEN_CMD);
	KUNIT_EXPECT_EQ(test, *panel->cmdq.cmd[1].buf, (u8)MIPI_DCS_WRITE_GRAM_CONTINUE);
	KUNIT_EXPECT_TRUE(test, !memcmp(panel->cmdq.cmd[1].buf + 1, buffer + SZ_DATA_1, SZ_DATA_2));

	remainer_buf = kunit_kzalloc(test, SZ_PACKET_2 - SZ_DATA_2, GFP_KERNEL);
	KUNIT_EXPECT_TRUE(test, !memcmp(panel->cmdq.cmd[1].buf + 1 + SZ_DATA_2, remainer_buf, SZ_PACKET_2 - SZ_DATA_2));
}

static void panel_test_panel_dsi_write_img_success_with_32byte_data_4byte_align_tx_done(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	static const int DATA_SIZE = 32;
	u8 buffer[32] = {
		0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04,
		0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04,
		0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02,
		0x04, 0x03, 0x02, 0x01, 0x04, 0x03, 0x02, 0x01,
	};
	u32 offset = 0, option = PKT_OPTION_SR_ALIGN_4 | PKT_OPTION_CHECK_TX_DONE;
	struct mock_expectation *exp1;
	int i;

	panel->panel_data.ddi_props.img_fifo_size = 0;
	exp1 = AtLeast(1, KUNIT_EXPECT_CALL(panel_cmdq_flush(ptr_eq(test, panel))));
	exp1->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_dsi_write_img(panel, MIPI_DSI_WR_SRAM_CMD, buffer, offset, DATA_SIZE, option), DATA_SIZE);
	KUNIT_EXPECT_EQ(test, panel_cmdq_get_size(panel), 1);
	/* check packets */
	for (i = 0; i < panel_cmdq_get_size(panel); i++) {
		KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[i].size, DATA_SIZE + 1);
		KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[i].cmd_id, (u8)MIPI_DSI_WR_GEN_CMD);
		KUNIT_EXPECT_EQ(test, *panel->cmdq.cmd[i].buf, (u8)(i == 0 ? MIPI_DCS_WRITE_SIDE_RAM_START : MIPI_DCS_WRITE_SIDE_RAM_CONTINUE));
		KUNIT_EXPECT_TRUE(test, !memcmp(panel->cmdq.cmd[i].buf + 1, buffer + (i * 4), 4));
	}
}

static void panel_test_panel_do_dsi_tx_packet_fail_with_invalid_argument(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	u8 test_packet_data[] = { 0x55, 0xFF };
	DEFINE_STATIC_PACKET(test_packet, DSI_PKT_TYPE_WR, test_packet_data, 0);

	KUNIT_EXPECT_EQ(test, panel_do_dsi_tx_packet(panel, NULL, false), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_do_dsi_tx_packet(NULL, &PKTINFO(test_packet), false), -EINVAL);
}

static void panel_test_panel_do_dsi_tx_packet_success_with_static_packet(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	u8 test_packet_data[] = { 0x55, 0xFF };
	DEFINE_STATIC_PACKET(test_packet, DSI_PKT_TYPE_WR, test_packet_data, 0);
	struct mock_expectation *expectation;

	expectation = Never(KUNIT_EXPECT_CALL(panel_cmdq_flush(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	KUNIT_EXPECT_EQ(test, panel_do_dsi_tx_packet(panel, &PKTINFO(test_packet), false), 0);
	KUNIT_ASSERT_EQ(test, panel_cmdq_get_size(panel), 1);
	KUNIT_EXPECT_TRUE(test, !memcmp(panel->cmdq.cmd[0].buf, test_packet_data, sizeof(test_packet_data)));
}

static void panel_test_panel_do_dsi_tx_packet_success_with_variable_packet(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	u8 test_table[2][4][1] = {
		{ { 0x00 }, { 0x01 }, { 0x02 }, { 0x03 } },
		{ { 0x10 }, { 0x11 }, { 0x12 }, { 0x13 } },
	};
	struct maptbl test_maptbl = DEFINE_3D_MAPTBL(test_table,
			&PNOBJ_FUNC(init_common_maptbl),
			&PNOBJ_FUNC(getidx_test_maptbl),
			&PNOBJ_FUNC(copy_common_maptbl));
	u8 test_packet_data[] = { 0x55, 0xFF };
	DECLARE_PKTUI(test_packet1) = {
		{ .offset = 1, .maptbl = &test_maptbl },
	};
	DEFINE_VARIABLE_PACKET(test_packet1, DSI_PKT_TYPE_WR, test_packet_data, 0);
	struct mock_expectation *expectation;
	struct panel_tx_msg tx_msg = {
		.gpara_offset = 0,
		.buf = test_packet_data,
		.len = ARRAY_SIZE(test_packet_data),
	};
	struct pktinfo *test_packet2 = create_tx_packet("test_packet2",
			DSI_PKT_TYPE_WR, &tx_msg, PKTUI(test_packet1), 1, 0);

	test_maptbl.pdata = panel;
	expectation = Never(KUNIT_EXPECT_CALL(panel_cmdq_flush(ptr_eq(test, panel))));
	expectation->action = int_return(test, 0);

	/* test_packet1 is created by macro (DEFINE_VARIABLE_PACKET) */
	test_index = 1;
	KUNIT_EXPECT_EQ(test, panel_do_dsi_tx_packet(panel, &PKTINFO(test_packet1), false), 0);
	KUNIT_ASSERT_EQ(test, panel_cmdq_get_size(panel), 1);
	KUNIT_EXPECT_EQ(test, get_pktinfo_initdata(&PKTINFO(test_packet1))[1], ((u8 *)test_table)[1]);
	KUNIT_EXPECT_EQ(test, get_pktinfo_txbuf(&PKTINFO(test_packet1))[1], ((u8 *)test_table)[1]);
	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[0].buf[0], (u8)0x55);
	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[0].buf[1], ((u8 *)test_table)[1]);

	/* test_packet2 is created by function (create_tx_packet) */
	test_index = 2;
	KUNIT_EXPECT_EQ(test, panel_do_dsi_tx_packet(panel, test_packet2, false), 0);
	KUNIT_ASSERT_EQ(test, panel_cmdq_get_size(panel), 2);
	KUNIT_EXPECT_EQ(test, get_pktinfo_initdata(test_packet2)[1], (u8)0xFF);
	KUNIT_EXPECT_EQ(test, get_pktinfo_txbuf(test_packet2)[1], ((u8 *)test_table)[2]);
	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[1].buf[0], (u8)0x55);
	KUNIT_EXPECT_EQ(test, panel->cmdq.cmd[1].buf[1], ((u8 *)test_table)[2]);
}

int panel_test_dummy_panel_power_ctrl_execute(struct panel_power_ctrl *pctrl)
{
	return 0;
}

struct panel_power_ctrl_funcs panel_test_dummy_pctrl_functions = {
	.execute = panel_test_dummy_panel_power_ctrl_execute,
};

static void panel_test_panel_do_power_ctrl_fail_with_invalid_arg(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct pwrctrl pwrctrl;

	KUNIT_EXPECT_EQ(test, panel_do_power_ctrl(NULL, &pwrctrl), -EINVAL);
	KUNIT_EXPECT_EQ(test, panel_do_power_ctrl(panel, NULL), -EINVAL);
}

static void panel_test_panel_do_power_ctrl_fail_with_invalid_pctrl_data(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct pwrctrl pwrctrl;
	static char *name = "pctrl_packet_name";
	static char *key = "power_ctrl_name";

	pnobj_init(&pwrctrl.base, CMD_TYPE_NONE, name);
	pwrctrl.key = key;
	KUNIT_EXPECT_EQ(test, panel_do_power_ctrl(panel, &pwrctrl), -EINVAL);

	pnobj_init(&pwrctrl.base, MAX_CMD_TYPE, name);
	pwrctrl.key = key;
	KUNIT_EXPECT_EQ(test, panel_do_power_ctrl(panel, &pwrctrl), -EINVAL);

	pnobj_init(&pwrctrl.base, CMD_TYPE_PCTRL, NULL);
	pwrctrl.key = key;
	KUNIT_EXPECT_EQ(test, panel_do_power_ctrl(panel, &pwrctrl), -EINVAL);

	pnobj_init(&pwrctrl.base, CMD_TYPE_PCTRL, name);
	pwrctrl.key = NULL;
	KUNIT_EXPECT_EQ(test, panel_do_power_ctrl(panel, &pwrctrl), -EINVAL);
}

static void panel_test_panel_do_power_ctrl_failed_with_empty_or_not_found_in_power_ctrl_list(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct pwrctrl pwrctrl;
	struct panel_power_ctrl *pctrl;
	static char *name = "pctrl_packet_name";
	static char *key = "power_ctrl_name";

	pnobj_init(&pwrctrl.base, CMD_TYPE_PCTRL, name);
	pwrctrl.key = key;

	KUNIT_EXPECT_EQ(test, panel_do_power_ctrl(panel, &pwrctrl), -ENODATA);

	pctrl = kunit_kzalloc(test, sizeof(*pctrl), GFP_KERNEL);
	pctrl->name = "pctrl_different_name";
	INIT_LIST_HEAD(&pctrl->action_list);
	pctrl->funcs = &panel_test_dummy_pctrl_functions;

	KUNIT_EXPECT_EQ(test, panel_do_power_ctrl(panel, &pwrctrl), -ENODATA);
}

static void panel_test_panel_do_power_ctrl_success(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct pwrctrl pwrctrl;
	struct panel_power_ctrl *pctrl;
	static char *name = "pctrl_packet_name";
	static char *key = "power_ctrl_name";

	pctrl = kunit_kzalloc(test, sizeof(*pctrl), GFP_KERNEL);
	pctrl->name = "power_ctrl_name";
	pctrl->dev_name = panel->of_node_name;
	INIT_LIST_HEAD(&pctrl->action_list);
	pctrl->funcs = &panel_test_dummy_pctrl_functions;
	list_add_tail(&pctrl->head, &panel->power_ctrl_list);

	pnobj_init(&pwrctrl.base, CMD_TYPE_PCTRL, name);
	pwrctrl.key = key;

	KUNIT_EXPECT_EQ(test, panel_do_power_ctrl(panel, &pwrctrl), 0);
}

static void panel_test_find_panel_lut_fail_with_invalid_args(struct kunit *test)
{
	struct panel_dt_lut *found;

	found = find_panel_lut(NULL, 0x804001);
	KUNIT_EXPECT_EQ(test, (int)PTR_ERR(found), -EINVAL);
}

static void panel_test_find_panel_lut_fail_with_empty_lut_list(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_dt_lut *found;

	found = find_panel_lut(panel, 0x804001);
	KUNIT_EXPECT_EQ(test, (int)PTR_ERR(found), -ENODEV);
}

static void panel_test_find_panel_lut_fail_with_empty_id_mask_list(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_dt_lut *found;
	struct panel_dt_lut *lut;

	lut = kunit_kzalloc(test, sizeof(*lut), GFP_KERNEL);
	lut->name = "test_dt_lut";
	INIT_LIST_HEAD(&lut->id_mask_list);

	list_add_tail(&lut->head, &panel->panel_lut_list);

	found = find_panel_lut(panel, 0x804001);
	KUNIT_EXPECT_EQ(test, (int)PTR_ERR(found), -ENODEV);
}

static void panel_test_find_panel_lut_fail_with_id_mask_mismatch(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_dt_lut *found, *lut;
	struct panel_id_mask *id_mask;
	int i;
	u32 id_mask_list[] = {
		/* id, mask */
		0x808182, 0xF0F3F2,
		0xA04001, 0xF0FFF0,
		0x904513, 0xF0F770,
	};

	lut = kunit_kzalloc(test, sizeof(*lut), GFP_KERNEL);
	lut->name = "test_dt_lut";
	INIT_LIST_HEAD(&lut->id_mask_list);

	for (i = 0; i < ARRAY_SIZE(id_mask_list) / 2; i++) {
		id_mask = kunit_kzalloc(test, sizeof(*id_mask), GFP_KERNEL);
		id_mask->id = id_mask_list[i*2];
		id_mask->mask = id_mask_list[i*2+1];
		list_add_tail(&id_mask->head, &lut->id_mask_list);
	}
	list_add_tail(&lut->head, &panel->panel_lut_list);

	found = find_panel_lut(panel, 0x804001);
	KUNIT_EXPECT_EQ(test, (int)PTR_ERR(found), -ENODEV);
}

static void panel_test_find_panel_lut_success_with_default_panel(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_dt_lut *lut_1, *lut_2;
	struct panel_id_mask *id_mask;
	int i;
	u32 id_mask_list_1[] = {
		/* id, mask */
		0x808182, 0xF0F3F2,
		0x000000, 0x000000,
		0xA04001, 0xF0FFF0,
		0x904513, 0xF0F770,
	};
	u32 id_mask_list_2[] = {
		/* id, mask */
		0x999999, 0xF0F3F2,
		0xAAAAAA, 0xF0FFF0,
		0xA0AA0A, 0xF0F770,
	};

	lut_1 = kunit_kzalloc(test, sizeof(*lut_1), GFP_KERNEL);
	lut_1->name = "test_dt_lut";
	INIT_LIST_HEAD(&lut_1->id_mask_list);

	for (i = 0; i < ARRAY_SIZE(id_mask_list_1) / 2; i++) {
		id_mask = kunit_kzalloc(test, sizeof(*id_mask), GFP_KERNEL);
		id_mask->id = id_mask_list_1[i*2];
		id_mask->mask = id_mask_list_1[i*2+1];
		list_add_tail(&id_mask->head, &lut_1->id_mask_list);
	}
	list_add_tail(&lut_1->head, &panel->panel_lut_list);

	lut_2 = kunit_kzalloc(test, sizeof(*lut_2), GFP_KERNEL);
	lut_2->name = "test_dt_lut_2";
	INIT_LIST_HEAD(&lut_2->id_mask_list);

	for (i = 0; i < ARRAY_SIZE(id_mask_list_2) / 2; i++) {
		id_mask = kunit_kzalloc(test, sizeof(*id_mask), GFP_KERNEL);
		id_mask->id = id_mask_list_2[i*2];
		id_mask->mask = id_mask_list_2[i*2+1];
		list_add_tail(&id_mask->head, &lut_2->id_mask_list);
	}
	list_add_tail(&lut_2->head, &panel->panel_lut_list);

	KUNIT_EXPECT_PTR_EQ(test, find_panel_lut(panel, 0x804001), lut_1);
}

static void panel_test_find_panel_lut_success_with_matched_panel(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct panel_dt_lut *lut_1, *lut_2;
	struct panel_id_mask *id_mask;
	int i;
	u32 id_mask_list_1[] = {
		/* id, mask */
		0x808182, 0xF0F3F2,
		0x000000, 0x000000,
		0xA04001, 0xF0FFF0,
		0x904513, 0xF0F770,
	};
	u32 id_mask_list_2[] = {
		/* id, mask */
		0xAAAAAA, 0xF0FFF0,
		0x909190, 0xF0F3F2,
		0xA0AA0A, 0xF0F770,
	};

	lut_1 = kunit_kzalloc(test, sizeof(*lut_1), GFP_KERNEL);
	lut_1->name = "test_dt_lut";
	INIT_LIST_HEAD(&lut_1->id_mask_list);

	for (i = 0; i < ARRAY_SIZE(id_mask_list_1) / 2; i++) {
		id_mask = kunit_kzalloc(test, sizeof(*id_mask), GFP_KERNEL);
		id_mask->id = id_mask_list_1[i*2];
		id_mask->mask = id_mask_list_1[i*2+1];
		list_add_tail(&id_mask->head, &lut_1->id_mask_list);
	}
	list_add_tail(&lut_1->head, &panel->panel_lut_list);

	lut_2 = kunit_kzalloc(test, sizeof(*lut_2), GFP_KERNEL);
	lut_2->name = "test_dt_lut_2";
	INIT_LIST_HEAD(&lut_2->id_mask_list);

	for (i = 0; i < ARRAY_SIZE(id_mask_list_2) / 2; i++) {
		id_mask = kunit_kzalloc(test, sizeof(*id_mask), GFP_KERNEL);
		id_mask->id = id_mask_list_2[i*2];
		id_mask->mask = id_mask_list_2[i*2+1];
		list_add_tail(&id_mask->head, &lut_2->id_mask_list);
	}
	list_add_tail(&lut_2->head, &panel->panel_lut_list);

	KUNIT_EXPECT_PTR_EQ(test, find_panel_lut(panel, 0x808182), lut_1);
	KUNIT_EXPECT_PTR_EQ(test, find_panel_lut(panel, 0x999999), lut_2);
	KUNIT_EXPECT_PTR_EQ(test, find_panel_lut(panel, 0x904513), lut_1);
}

static void panel_test_get_frame_delay(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
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

static void panel_test_get_total_delay(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
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

static void panel_test_get_remaining_delay(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	DEFINE_PANEL_UDELAY(wait_16p7msec, 16700);

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

static void panel_test_strcmp_suffix(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, !strcmp_suffix("hubble1_digital_img1", "digital_img"));
	KUNIT_EXPECT_TRUE(test, !strcmp_suffix("hubble2_digital_img", "digital_img"));
	KUNIT_EXPECT_FALSE(test, !strcmp_suffix("digitAl_img", "digital_img"));
	KUNIT_EXPECT_FALSE(test, !strcmp_suffix("digital_img ", "digital_img"));
}

static void panel_test_find_packet_suffix(struct kunit *test)
{
	DEFINE_PANEL_UDELAY(s6e3hab_aod_self_spsram_sel_delay, 1);
	u8 S6E3HAB_ANALOG_IMG[32];
	u8 S6E3HAB_DIGITAL_IMG[32];
	DEFINE_STATIC_PACKET(s6e3hab_aod_analog_img, DSI_PKT_TYPE_WR_SR, S6E3HAB_ANALOG_IMG, 0);
	DEFINE_STATIC_PACKET(s6e3hab_aod_digital_img_1, DSI_PKT_TYPE_WR_SR, S6E3HAB_DIGITAL_IMG, 0);
	DEFINE_STATIC_PACKET(s6e3hab_aod_digital_img, DSI_PKT_TYPE_WR_SR, S6E3HAB_DIGITAL_IMG, 0);
	void *s6e3hab_aod_analog_img_cmdtbl[] = {
		&DLYINFO(s6e3hab_aod_self_spsram_sel_delay),
		&PKTINFO(s6e3hab_aod_analog_img),
	};
	void *s6e3hab_aod_digital_img_1_cmdtbl[] = {
		&DLYINFO(s6e3hab_aod_self_spsram_sel_delay),
		&PKTINFO(s6e3hab_aod_digital_img_1),
	};
	void *s6e3hab_aod_digital_img_cmdtbl[] = {
		&DLYINFO(s6e3hab_aod_self_spsram_sel_delay),
		&PKTINFO(s6e3hab_aod_digital_img),
	};
	DEFINE_SEQINFO(hubble_analog_img, s6e3hab_aod_analog_img_cmdtbl);
	DEFINE_SEQINFO(hubble_digital_img_1, s6e3hab_aod_digital_img_1_cmdtbl);
	DEFINE_SEQINFO(hubble_digital_img, s6e3hab_aod_digital_img_cmdtbl);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, find_packet_suffix(&SEQINFO(hubble_analog_img), "analog_img"));
	KUNIT_EXPECT_TRUE(test, !find_packet_suffix(&SEQINFO(hubble_digital_img_1), "digital_img"));
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, find_packet_suffix(&SEQINFO(hubble_digital_img), "digital_img"));
}

static void panel_test_find_panel_maptbl_by_substr_success(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct maptbl *m1, *m2;

	KUNIT_ASSERT_TRUE(test, panel_prepare(panel, ctx->cpi) == 0);

	m1 = &ctx->cpi->maptbl[ACL_OPR_MAPTBL];
	m2 = find_panel_maptbl_by_substr(panel, "acl_opr_table");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m2);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m1), maptbl_get_name(m2));

	m2 = find_panel_maptbl_by_substr(panel, "acl_opr");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m2);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m1), maptbl_get_name(m2));

	KUNIT_EXPECT_TRUE(test, !find_panel_maptbl_by_substr(panel, "ACL_OPR"));

	m1 = &ctx->cpi->maptbl[DIA_ONOFF_MAPTBL];
	m2 = find_panel_maptbl_by_substr(panel, "dia_onoff_table");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m2);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m1), maptbl_get_name(m2));

	m2 = find_panel_maptbl_by_substr(panel, "dia_onoff");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, m2);
	KUNIT_EXPECT_STREQ(test, maptbl_get_name(m1), maptbl_get_name(m2));

	KUNIT_EXPECT_TRUE(test, !find_panel_maptbl_by_substr(panel, "DIA_onoff_table"));

	KUNIT_ASSERT_TRUE(test, panel_unprepare(panel) == 0);
}

static void panel_test_find_panel_seq_by_name_success(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct seqinfo *seq1, *seq2;

	KUNIT_ASSERT_TRUE(test, panel_prepare(panel, ctx->cpi) == 0);
	KUNIT_EXPECT_TRUE(test, !find_panel_seq_by_name(panel, NULL));

	seq1 = &ctx->cpi->seqtbl[0];
	seq2 = find_panel_seq_by_name(panel, PANEL_INIT_SEQ);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, seq2);
	KUNIT_EXPECT_PTR_NE(test, seq1, seq2);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(&seq1->base), get_pnobj_name(&seq2->base));

	seq1 = &ctx->cpi->seqtbl[4];
	seq2 = find_panel_seq_by_name(panel, PANEL_EXIT_SEQ);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, seq2);
	KUNIT_EXPECT_PTR_NE(test, seq1, seq2);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(&seq1->base), get_pnobj_name(&seq2->base));

	KUNIT_EXPECT_TRUE(test, !find_panel_seq_by_name(panel, PANEL_DUMMY_SEQ));
	KUNIT_EXPECT_TRUE(test, !find_panel_seq_by_name(panel, "unknown-sequence"));

	KUNIT_ASSERT_TRUE(test, panel_unprepare(panel) == 0);
}

static void panel_test_check_seqtbl_exist_success(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;

	KUNIT_ASSERT_TRUE(test, panel_prepare(panel, ctx->cpi) == 0);

	KUNIT_EXPECT_TRUE(test, check_seqtbl_exist(panel, PANEL_INIT_SEQ));
	KUNIT_EXPECT_TRUE(test, check_seqtbl_exist(panel, PANEL_EXIT_SEQ));
	KUNIT_EXPECT_FALSE(test, check_seqtbl_exist(panel, PANEL_DUMMY_SEQ));
	KUNIT_EXPECT_FALSE(test, check_seqtbl_exist(panel, "unknown-sequence"));

	KUNIT_ASSERT_TRUE(test, panel_unprepare(panel) == 0);
}

static void panel_test_find_panel_rdinfo_success(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct rdinfo *rdi1, *rdi2;

	KUNIT_ASSERT_TRUE(test, panel_prepare(panel, ctx->cpi) == 0);

	rdi1 = &ctx->cpi->rditbl[READ_ID];
	rdi2 = find_panel_rdinfo(panel, "id");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, rdi2);
	KUNIT_EXPECT_PTR_NE(test, rdi1, rdi2);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(&rdi1->base),
			get_pnobj_name(&rdi2->base));

	rdi1 = &ctx->cpi->rditbl[READ_DATE];
	rdi2 = find_panel_rdinfo(panel, "date");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, rdi2);
	KUNIT_EXPECT_PTR_NE(test, rdi1, rdi2);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(&rdi1->base),
			get_pnobj_name(&rdi2->base));

	KUNIT_EXPECT_TRUE(test, !find_panel_rdinfo(panel, "not_exist"));

	KUNIT_ASSERT_TRUE(test, panel_unprepare(panel) == 0);
}

static void panel_test_find_panel_resource_success(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	struct resinfo *res1, *res2;

	KUNIT_ASSERT_TRUE(test, panel_prepare(panel, ctx->cpi) == 0);

	res1 = &ctx->cpi->restbl[RES_ID];
	res2 = find_panel_resource(panel, "id");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, res2);
	KUNIT_EXPECT_PTR_NE(test, res1, res2);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(&res1->base),
			get_pnobj_name(&res2->base));

	res1 = &ctx->cpi->restbl[RES_DATE];
	res2 = find_panel_resource(panel, "date");
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, res2);
	KUNIT_EXPECT_PTR_NE(test, res1, res2);
	KUNIT_EXPECT_STREQ(test, get_pnobj_name(&res1->base),
			get_pnobj_name(&res2->base));

	KUNIT_EXPECT_TRUE(test, !find_panel_resource(panel, "dat"));
	KUNIT_EXPECT_TRUE(test, !find_panel_resource(panel, "not_exist"));

	KUNIT_ASSERT_TRUE(test, panel_unprepare(panel) == 0);
}

static void panel_test_panel_resource_copy_should_be_fail_if_resource_not_exist_in_res_list(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	u8 DATE[7] = {1, 2, 3, 4, 5, 6, 7};
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);
	DEFINE_RESUI(date, &RDINFO(date), 0);
	struct resinfo *res = create_resource("date", DATE, ARRAY_SIZE(DATE), RESUI(date), 1);
	u8 *buf = kunit_kzalloc(test, ARRAY_SIZE(DATE), GFP_KERNEL);

	KUNIT_ASSERT_TRUE(test, res != NULL);

	KUNIT_EXPECT_EQ(test, panel_resource_copy(panel, buf, "date"), -EINVAL);
	destroy_resource(res);
}

static void panel_test_panel_resource_copy_should_be_fail_if_resource_not_initialized(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	u8 DATE[7] = {1, 2, 3, 4, 5, 6, 7};
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);
	DEFINE_RESUI(date, &RDINFO(date), 0);
	struct resinfo *res = create_resource("date", NULL, ARRAY_SIZE(DATE), RESUI(date), 1);
	u8 *buf = kunit_kzalloc(test, ARRAY_SIZE(DATE), GFP_KERNEL);

	KUNIT_ASSERT_TRUE(test, res != NULL);

	list_add_tail(get_pnobj_list(&res->base), &panel->res_list);
	KUNIT_EXPECT_EQ(test, panel_resource_copy(panel, buf, "date"), -EINVAL);
	destroy_resource(res);
}

static void panel_test_panel_resource_copy_success(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	u8 DATE[7] = {1, 2, 3, 4, 5, 6, 7};
	DEFINE_RDINFO(date, DSI_PKT_TYPE_RD, 0xA1, 4, 7);
	DEFINE_RESUI(date, &RDINFO(date), 0);
	struct resinfo *res = create_resource("date", DATE, ARRAY_SIZE(DATE), RESUI(date), 1);
	u8 *buf = kunit_kzalloc(test, ARRAY_SIZE(DATE), GFP_KERNEL);

	KUNIT_ASSERT_TRUE(test, res != NULL);

	list_add_tail(get_pnobj_list(&res->base), &panel->res_list);
	KUNIT_EXPECT_EQ(test, panel_resource_copy(panel, buf, "date"), 0);
	KUNIT_EXPECT_TRUE(test, !memcmp(buf, res->data, sizeof(DATE)));
	destroy_resource(res);
}

/*
 * This is run once before each test case, see the comment on
 * example_test_module for more information.
 */
static int panel_test_init(struct kunit *test)
{
	struct panel_device *panel;
	struct panel_test *ctx = kunit_kzalloc(test, sizeof(struct panel_test), GFP_KERNEL);
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

	panel = panel_device_create();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, panel);

#ifdef CONFIG_USDM_PANEL_COPR
	panel_mutex_init(panel, &panel->copr.lock);
#endif
	panel_mutex_init(panel, &panel->io_lock);
	panel_mutex_init(panel, &panel->op_lock);
	panel_mutex_init(panel, &panel->cmdq.lock);
#ifdef CONFIG_USDM_MDNIE
	panel_mutex_init(panel, &panel->mdnie.lock);
#endif

	panel->cmdbuf = kunit_kzalloc(test, SZ_512, GFP_KERNEL);
	panel->adapter.fifo_size = SZ_512;

	panel->cmdq.top = -1;
	panel->of_node_name = "test_panel";

	ctx->panel = panel;
	ctx->cpi = &test_panel_info;
	test->priv = ctx;

	INIT_LIST_HEAD(&panel->command_initdata_list);

	INIT_LIST_HEAD(&panel->maptbl_list);
	INIT_LIST_HEAD(&panel->seq_list);
	INIT_LIST_HEAD(&panel->rdi_list);
	INIT_LIST_HEAD(&panel->res_list);
	INIT_LIST_HEAD(&panel->key_list);
	INIT_LIST_HEAD(&panel->dump_list);
	INIT_LIST_HEAD(&panel->pkt_list);
	INIT_LIST_HEAD(&panel->dly_list);
	INIT_LIST_HEAD(&panel->cond_list);
	INIT_LIST_HEAD(&panel->pwrctrl_list);
	INIT_LIST_HEAD(&panel->cfg_list);
	INIT_LIST_HEAD(&panel->func_list);

	INIT_LIST_HEAD(&panel->power_ctrl_list);
	INIT_LIST_HEAD(&panel->panel_lut_list);
	INIT_LIST_HEAD(&panel->prop_list);

	KUNIT_ASSERT_TRUE(test,
			!panel_add_property_from_array(panel,
			prop_array, ARRAY_SIZE(prop_array)));

	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(panel_cmdq_flush);

	return 0;
}

/*
 * This is run once after each test case, see the comment on example_test_module
 * for more information.
 */
static void panel_test_exit(struct kunit *test)
{
	struct panel_test *ctx = test->priv;
	struct panel_device *panel = ctx->panel;
	int i;

	for (i = 0; i < panel_cmdq_get_size(panel); i++) {
		kfree(panel->cmdq.cmd[i].buf);
		panel->cmdq.cmd[i].buf = NULL;
	}

	panel_device_destroy(panel);
}

/*
 * Here we make a list of all the test cases we want to add to the test module
 * below.
 */
static struct kunit_case panel_test_cases[] = {
	/*
	 * This is a helper to create a test case object from a test case
	 * function; its exact function is not important to understand how to
	 * use KUnit, just know that this is how you associate test cases with a
	 * test module.
	 */
	/* NOTE: UML TC */
	KUNIT_CASE(panel_test_test),
	KUNIT_CASE(panel_test_dsi_tx_packet_type_to_cmd_id_success),
	KUNIT_CASE(panel_test_panel_dsi_write_img_fail_with_invalid_args),
	KUNIT_CASE(panel_test_panel_dsi_write_img_fail_with_invalid_fifo_size),
	KUNIT_CASE(panel_test_panel_dsi_write_img_success_with_16byte_align),
	KUNIT_CASE(panel_test_panel_dsi_write_img_success_with_65byte_fifo_16byte_align),
	KUNIT_CASE(panel_test_panel_dsi_write_img_success_with_16byte_align_mismatch_filled_zero),
	KUNIT_CASE(panel_test_panel_dsi_write_img_success_with_64byte_fifo_12byte_align_mismatch_filled_zero),
	KUNIT_CASE(panel_test_panel_dsi_write_img_success_with_32byte_data_4byte_align_tx_done),
	KUNIT_CASE(panel_test_panel_do_dsi_tx_packet_fail_with_invalid_argument),
	KUNIT_CASE(panel_test_panel_do_dsi_tx_packet_success_with_static_packet),
	KUNIT_CASE(panel_test_panel_do_dsi_tx_packet_success_with_variable_packet),
	KUNIT_CASE(panel_test_panel_do_power_ctrl_fail_with_invalid_arg),
	KUNIT_CASE(panel_test_panel_do_power_ctrl_fail_with_invalid_pctrl_data),
	KUNIT_CASE(panel_test_panel_do_power_ctrl_failed_with_empty_or_not_found_in_power_ctrl_list),
	KUNIT_CASE(panel_test_panel_do_power_ctrl_success),
	KUNIT_CASE(panel_test_find_panel_lut_fail_with_invalid_args),
	KUNIT_CASE(panel_test_find_panel_lut_fail_with_empty_lut_list),
	KUNIT_CASE(panel_test_find_panel_lut_fail_with_empty_id_mask_list),
	KUNIT_CASE(panel_test_find_panel_lut_fail_with_id_mask_mismatch),
	KUNIT_CASE(panel_test_find_panel_lut_success_with_default_panel),
	KUNIT_CASE(panel_test_find_panel_lut_success_with_matched_panel),
	KUNIT_CASE(panel_test_get_frame_delay),
	KUNIT_CASE(panel_test_get_total_delay),
	KUNIT_CASE(panel_test_get_remaining_delay),
	KUNIT_CASE(panel_test_strcmp_suffix),
	KUNIT_CASE(panel_test_find_packet_suffix),
	KUNIT_CASE(panel_test_find_panel_maptbl_by_substr_success),
	KUNIT_CASE(panel_test_find_panel_seq_by_name_success),
	KUNIT_CASE(panel_test_check_seqtbl_exist_success),
	KUNIT_CASE(panel_test_find_panel_rdinfo_success),
	KUNIT_CASE(panel_test_find_panel_resource_success),
	KUNIT_CASE(panel_test_panel_resource_copy_should_be_fail_if_resource_not_exist_in_res_list),
	KUNIT_CASE(panel_test_panel_resource_copy_should_be_fail_if_resource_not_initialized),
	KUNIT_CASE(panel_test_panel_resource_copy_success),
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
static struct kunit_suite panel_test_module = {
	.name = "panel_test",
	.init = panel_test_init,
	.exit = panel_test_exit,
	.test_cases = panel_test_cases,
};

/*
 * This registers the above test module telling KUnit that this is a suite of
 * tests that need to be run.
 */
kunit_test_suites(&panel_test_module);

MODULE_LICENSE("GPL v2");
