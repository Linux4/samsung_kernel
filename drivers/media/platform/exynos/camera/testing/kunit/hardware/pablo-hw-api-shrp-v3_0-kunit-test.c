// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "hardware/api/is-hw-api-shrp-v3_0.h"
#include "hardware/sfr/is-sfr-shrp-v3_0.h"

static struct pablo_hw_api_shrp_kunit_test_ctx {
	void *addr;
	struct pmio_config		pmio_config;
	struct pablo_mmio		*pmio;
	struct pmio_field		*pmio_fields;
	struct pmio_reg_seq		*pmio_reg_seqs;
} test_ctx;

struct is_rectangle shrp_size_test_config[] = {
	{128, 32},	/* min size */
	{4880, 12288},	/* max size */
	{4032, 3024},	/* customized size */
};

/* Define the test cases. */
static void pablo_hw_api_shrp_hw_is_occurred0_kunit_test(struct kunit *test)
{
	u32 ret, status;
	int test_idx;
	u32 err_interrupt_list[] = {
		INTR1_SHRP_CMDQ_ERROR_INT,
		INTR1_SHRP_C_LOADER_ERROR_INT,
		INTR1_SHRP_COREX_ERROR_INT,
		INTR1_SHRP_CINFIFO_0_ERROR_INT,
		INTR1_SHRP_COUTFIFO_0_ERROR_INT,
		INTR1_SHRP_VOTF_GLOBAL_ERROR_INT,
		INTR1_SHRP_VOTF_LOST_CONNECTION_INT,
	};

	status = 1 << INTR1_SHRP_FRAME_START_INT;
	ret = shrp_hw_is_occurred0(status, INTR_FRAME_START);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_SHRP_FRAME_END_INT;
	ret = shrp_hw_is_occurred0(status, INTR_FRAME_END);
	KUNIT_EXPECT_EQ(test, ret, status);

	/*
	status = 1 << INTR1_SHRP_CMDQ_HOLD_INT;
	ret = shrp_hw_is_occurred0(status, INTR1_SHRP_CMDQ_HOLD_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_SHRP_SETTING_DONE_INT;
	ret = shrp_hw_is_occurred0(status, INTR1_SHRP_SETTING_DONE_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_SHRP_C_LOADER_END_INT;
	ret = shrp_hw_is_occurred0(status, INTR1_SHRP_C_LOADER_END_INT);
	KUNIT_EXPECT_EQ(test, ret, status);
	*/

	status = 1 << INTR1_SHRP_COREX_END_INT_0;
	ret = shrp_hw_is_occurred0(status, INTR_COREX_END_0);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_SHRP_COREX_END_INT_1;
	ret = shrp_hw_is_occurred0(status, INTR_COREX_END_1);
	KUNIT_EXPECT_EQ(test, ret, status);

	/*
	status = 1 << INTR1_SHRP_ROW_COL_INT;
	ret = shrp_hw_is_occurred0(status, INTR1_SHRP_ROW_COL_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_SHRP_FREEZE_ON_ROW_COL_INT;
	ret = shrp_hw_is_occurred0(status, INTR1_SHRP_FREEZE_ON_ROW_COL_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_SHRP_TRANS_STOP_DONE_INT;
	ret = shrp_hw_is_occurred0(status, INTR1_SHRP_TRANS_STOP_DONE_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_SHRP_VOTF_LOST_CONNECTION_INT;
	ret = shrp_hw_is_occurred0(status, LOST_CONNECTION_INT);
	KUNIT_EXPECT_EQ(test, ret, status);
	*/

	for (test_idx = 0; test_idx < ARRAY_SIZE(err_interrupt_list); test_idx++) {
		status = 1 << err_interrupt_list[test_idx];
		ret = shrp_hw_is_occurred0(status, INTR_ERR);
		KUNIT_EXPECT_EQ(test, ret, status);
	}
}

static void pablo_hw_api_shrp_hw_is_occurred1_kunit_test(struct kunit *test)
{
	u32 ret, status;
	int test_idx;
	u32 err_interrupt_list[] = {
		INTR2_SHRP_SYNCFIFO_SEG_DBG_CNT_ERR,
		INTR2_SHRP_SHARPEN_DBG_CNT_ERR,
		INTR2_SHRP_OETF_GAMMA_DBG_CNT_ERR,
		INTR2_SHRP_YUV444TO422_DBG_CNT_ERR,
		INTR2_SHRP_DITHER_DBG_CNT_ERR,
	};

	/*
	status = 1 << INTR2_SHRP_VOTF_LOST_FLUSH;
	ret = shrp_hw_is_occurred1(status, INTR2_SHRP_VOTF_LOST_FLUSH);
	KUNIT_EXPECT_EQ(test, ret, status);
	*/

	for (test_idx = 0; test_idx < ARRAY_SIZE(err_interrupt_list); test_idx++) {
		status = 1 << err_interrupt_list[test_idx];
		ret = shrp_hw_is_occurred1(status, INTR_ERR);
		KUNIT_EXPECT_EQ(test, ret, status);
	}
}

static void pablo_hw_api_shrp_hw_s_reset_kunit_test(struct kunit *test)
{
	int ret;

	ret = shrp_hw_s_reset(test_ctx.pmio);

	KUNIT_EXPECT_EQ(test, ret, -ENODEV); /* timeout */
}

static void pablo_hw_api_shrp_hw_s_init_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	shrp_hw_s_init(test_ctx.pmio);

	set_val = SHRP_GET_F(test_ctx.pmio, SHRP_R_CMDQ_QUE_CMD_M, SHRP_F_CMDQ_QUE_CMD_SETTING_MODE);
	expected_val = 3;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_R(test_ctx.pmio, SHRP_R_AUTO_IGNORE_PREADY_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_R(test_ctx.pmio, SHRP_R_CMDQ_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_R(test_ctx.pmio, SHRP_R_DEBUG_CLOCK_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_shrp_hw_wait_idle_kunit_test(struct kunit *test)
{
	int ret;

	ret = shrp_hw_wait_idle(test_ctx.pmio);

	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_api_shrp_hw_s_coutfifo_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	shrp_hw_s_coutfifo(test_ctx.pmio, set_id);

	set_val = SHRP_GET_F(test_ctx.pmio,
			SHRP_R_IP_USE_OTF_PATH_01,
			SHRP_F_IP_USE_OTF_OUT_FOR_PATH_0);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_F(test_ctx.pmio,
			SHRP_R_IP_USE_OTF_PATH_01,
			SHRP_F_IP_USE_OTF_OUT_FOR_PATH_1);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_F(test_ctx.pmio,
			SHRP_R_YUV_COUTFIFO0_INTERVALS,
			SHRP_F_YUV_COUTFIFO0_INTERVAL_HBLANK);
	expected_val = HBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_F(test_ctx.pmio,
			SHRP_R_YUV_COUTFIFO0_INTERVAL_VBLANK,
			SHRP_F_YUV_COUTFIFO0_INTERVAL_VBLANK);
	expected_val = VBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_R(test_ctx.pmio, SHRP_R_YUV_COUTFIFO0_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_F(test_ctx.pmio,
			SHRP_R_YUV_COUTFIFO0_CONFIG,
			SHRP_F_YUV_COUTFIFO0_DEBUG_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_F(test_ctx.pmio,
			SHRP_R_YUV_COUTFIFO0_CONFIG,
			SHRP_F_YUV_COUTFIFO0_VVALID_RISE_AT_FIRST_DATA_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_shrp_hw_s_cinfifo_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	shrp_hw_s_cinfifo(test_ctx.pmio, set_id);

	set_val = SHRP_GET_F(test_ctx.pmio,
			SHRP_R_YUV_CINFIFO0_INTERVALS,
			SHRP_F_YUV_CINFIFO0_INTERVAL_HBLANK);
	expected_val = HBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_R(test_ctx.pmio, SHRP_R_YUV_CINFIFO0_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_F(test_ctx.pmio,
			SHRP_R_YUV_CINFIFO0_CONFIG,
			SHRP_F_YUV_CINFIFO0_STALL_BEFORE_FRAME_START_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_F(test_ctx.pmio,
			SHRP_R_YUV_CINFIFO0_CONFIG, SHRP_F_YUV_CINFIFO0_DEBUG_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_shrp_hw_s_core_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	u32 num_buffers = 0;
	u32 set_id = COREX_DIRECT;

	shrp_hw_s_core(test_ctx.pmio, num_buffers, set_id);

	/* _shrp_hw_s_common */
	set_val = SHRP_GET_R(test_ctx.pmio, SHRP_R_AUTO_IGNORE_INTERRUPT_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* _shrp_hw_s_int_mask */
	set_val = SHRP_GET_R(test_ctx.pmio, SHRP_R_INT_REQ_INT0_ENABLE);
	expected_val = INT1_SHRP_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_R(test_ctx.pmio, SHRP_R_INT_REQ_INT1_ENABLE);
	expected_val = INT2_SHRP_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = SHRP_GET_F(test_ctx.pmio, SHRP_R_CMDQ_QUE_CMD_L, SHRP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE);
	expected_val = INT_GRP_SHRP_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* _shrp_hw_s_secure_id */
	set_val = SHRP_GET_R(test_ctx.pmio, SHRP_R_SECU_CTRL_SEQID);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_shrp_hw_rdma_create_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 dma_id = 0;
	int test_idx;

	for (test_idx = SHRP_RDMA_HF; test_idx < SHRP_RDMA_MAX; test_idx++) {
		dma_id = test_idx;

		ret = shrp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* DMA id err test */
	dma_id = -1;
	ret = shrp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
	KUNIT_EXPECT_EQ(test, ret, SET_SUCCESS);
}

static void pablo_hw_api_shrp_hw_s_rdma_corex_id_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };
	u32 set_id = COREX_DIRECT;

	shrp_hw_s_rdma_corex_id(&dma, set_id);
}

static void pablo_hw_api_shrp_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	pdma_addr_t addr = 0;
	u32 plane = 0;
	u32 num_buffers = 0;
	int buf_idx = 0;
	u32 comp_sbwc_en = 0;
	u32 payload_size = 0;

	ret = shrp_hw_s_rdma_addr(&dma, &addr, plane,
			num_buffers, buf_idx, comp_sbwc_en, payload_size);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_shrp_hw_s_corex_update_type_kunit_test(struct kunit *test)
{
	int ret;
	u32 set_id = COREX_DIRECT;

	ret = shrp_hw_s_corex_update_type(test_ctx.pmio, set_id);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_shrp_hw_s_cmdq_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 num_buffers = 1;

	shrp_hw_s_cmdq(test_ctx.pmio, set_id, num_buffers, 0, 0);
}

static void pablo_hw_api_shrp_hw_g_int_state0_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;

	state = shrp_hw_g_int_state0(test_ctx.pmio, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);
}

static void pablo_hw_api_shrp_hw_s_sharpen_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 start_pos = 0;
	u32 frame_width = 0;
	u32 dma_width = 0;
	u32 dma_height = 0;
	u32 stripe_enable = 0;
	struct shrp_radial_cfg cfg;

	shrp_hw_s_sharpen_size(test_ctx.pmio, set_id, start_pos, frame_width, dma_width,
				dma_height, stripe_enable, &cfg);
}

static void pablo_hw_api_shrp_hw_s_strip_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 stripe_enable = 0;
	u32 start_pos = 0;
	int ret;

	ret = shrp_hw_s_strip(test_ctx.pmio, set_id, stripe_enable, start_pos,
		SHRP_STRIP_TYPE_NONE, 0, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_shrp_hw_s_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	u32 stripe_enable = 0;
	int test_idx;

	for (test_idx = 0; test_idx < ARRAY_SIZE(shrp_size_test_config); test_idx++) {
		width = shrp_size_test_config[test_idx].w;
		height = shrp_size_test_config[test_idx].h;

		shrp_hw_s_size(test_ctx.pmio, set_id, width, height, stripe_enable);
	}
}

static void pablo_hw_api_shrp_hw_s_input_path_kunit_test(struct kunit *test)
{
	int path = 0;
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	shrp_hw_s_input_path(test_ctx.pmio, set_id, path);

	set_val = SHRP_GET_F(test_ctx.pmio, SHRP_R_IP_USE_OTF_PATH_01, SHRP_F_IP_USE_OTF_IN_FOR_PATH_0);
	expected_val = path;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_shrp_hw_s_output_path_kunit_test(struct kunit *test)
{
	int path = 0;
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	shrp_hw_s_output_path(test_ctx.pmio, set_id, 0);

	set_val = SHRP_GET_F(test_ctx.pmio, SHRP_R_IP_USE_OTF_PATH_01, SHRP_F_IP_USE_OTF_OUT_FOR_PATH_0);
	expected_val = path;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_shrp_hw_s_mux_dtp_kunit_test(struct kunit *test)
{
	int type = 0;
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	shrp_hw_s_mux_dtp(test_ctx.pmio, set_id, type);

	set_val = SHRP_GET_R(test_ctx.pmio, SHRP_R_CHAIN_MUX_SELECT);
	expected_val = type;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_shrp_hw_dma_dump_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };

	shrp_hw_dma_dump(&dma);
}

static void pablo_hw_api_shrp_hw_dump_kunit_test(struct kunit *test)
{
	shrp_hw_dump(test_ctx.pmio);
}

static void pablo_hw_api_shrp_hw_g_int_mask1_kunit_test(struct kunit *test)
{
	unsigned int mask;

	mask = shrp_hw_g_int_mask1(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);
}

static void pablo_hw_api_shrp_hw_g_int_state1_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;

	state = shrp_hw_g_int_state1(test_ctx.pmio, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);
}

static void pablo_hw_api_shrp_hw_print_chain_debug_cnt_kunit_test(struct kunit *test)
{
	shrp_hw_print_chain_debug_cnt(test_ctx.pmio);
}

static void pablo_hw_api_shrp_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;

	shrp_hw_s_block_bypass(test_ctx.pmio, set_id);
}

static void pablo_hw_api_shrp_hw_s_dtp_pattern_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	enum shrp_dtp_pattern pattern = SHRP_DTP_PATTERN_SOLID;

	shrp_hw_s_dtp_pattern(test_ctx.pmio, set_id, pattern);
}

static void pablo_hw_api_shrp_hw_s_crc_kunit_test(struct kunit *test)
{
	u32 seed = 0;

	shrp_hw_s_crc(test_ctx.pmio, seed);
}

static void pablo_hw_api_shrp_hw_g_int_mask0_kunit_test(struct kunit *test)
{
	unsigned int mask;

	mask = shrp_hw_g_int_mask0(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);
}

static void pablo_hw_api_shrp_hw_s_int_mask0_kunit_test(struct kunit *test)
{
	unsigned int mask = 0;

	shrp_hw_s_int_mask0(test_ctx.pmio, mask);
}

static void pablo_hw_api_shrp_hw_s_strgen_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;

	shrp_hw_s_strgen(test_ctx.pmio, set_id);
}

static void pablo_hw_api_shrp_hw_s_clock_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	shrp_hw_s_clock(test_ctx.pmio, true);

	set_val = SHRP_GET_F(test_ctx.pmio, SHRP_R_IP_PROCESSING, SHRP_F_IP_PROCESSING);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	shrp_hw_s_clock(test_ctx.pmio, false);

	set_val = SHRP_GET_F(test_ctx.pmio, SHRP_R_IP_PROCESSING, SHRP_F_IP_PROCESSING);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static int __pablo_hw_api_shrp_pmio_init(struct kunit *test)
{
	int ret;

	test_ctx.pmio_config.name = "shrp";

	test_ctx.pmio_config.mmio_base = test_ctx.addr;

	test_ctx.pmio_config.cache_type = PMIO_CACHE_NONE;

	shrp_hw_init_pmio_config(&test_ctx.pmio_config);

	test_ctx.pmio = pmio_init(NULL, NULL, &test_ctx.pmio_config);

	if (IS_ERR(test_ctx.pmio)) {
		err("failed to init shrp PMIO: %ld", PTR_ERR(test_ctx.pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(test_ctx.pmio, &test_ctx.pmio_fields,
			test_ctx.pmio_config.fields,
			test_ctx.pmio_config.num_fields);
	if (ret) {
		err("failed to alloc shrp PMIO fields: %d", ret);
		pmio_exit(test_ctx.pmio);
		return ret;

	}

	test_ctx.pmio_reg_seqs = kunit_kzalloc(test, sizeof(struct pmio_reg_seq) * shrp_hw_g_reg_cnt(), 0);
	if (!test_ctx.pmio_reg_seqs) {
		err("failed to alloc PMIO multiple write buffer");
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		pmio_exit(test_ctx.pmio);
		return -ENOMEM;
	}

	return ret;
}

static void __pablo_hw_api_shrp_pmio_deinit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.pmio_reg_seqs);
	pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
	pmio_exit(test_ctx.pmio);
}

static int pablo_hw_api_shrp_kunit_test_init(struct kunit *test)
{
	int ret;

	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);

	ret = __pablo_hw_api_shrp_pmio_init(test);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void pablo_hw_api_shrp_kunit_test_exit(struct kunit *test)
{
	__pablo_hw_api_shrp_pmio_deinit(test);

	kunit_kfree(test, test_ctx.addr);
	memset(&test_ctx, 0, sizeof(struct pablo_hw_api_shrp_kunit_test_ctx));
}

static struct kunit_case pablo_hw_api_shrp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_shrp_hw_is_occurred0_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_is_occurred1_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_coutfifo_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_cinfifo_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_rdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_rdma_corex_id_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_corex_update_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_cmdq_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_g_int_state0_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_sharpen_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_strip_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_input_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_output_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_mux_dtp_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_dma_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_g_int_mask1_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_g_int_state1_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_print_chain_debug_cnt_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_dtp_pattern_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_crc_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_g_int_mask0_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_int_mask0_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_strgen_kunit_test),
	KUNIT_CASE(pablo_hw_api_shrp_hw_s_clock_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_shrp_kunit_test_suite = {
	.name = "pablo-hw-api-shrp-kunit-test",
	.init = pablo_hw_api_shrp_kunit_test_init,
	.exit = pablo_hw_api_shrp_kunit_test_exit,
	.test_cases = pablo_hw_api_shrp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_shrp_kunit_test_suite);

MODULE_LICENSE("GPL");
