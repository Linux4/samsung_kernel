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

#include "hardware/api/is-hw-api-yuvp-v2_0.h"
#include "hardware/sfr/is-sfr-yuvp-v3_0.h"

static struct pablo_hw_api_yuvp_kunit_test_ctx {
	void *addr;
	struct pmio_config		pmio_config;
	struct pablo_mmio		*pmio;
	struct pmio_field		*pmio_fields;
	struct pmio_reg_seq		*pmio_reg_seqs;
} test_ctx;

struct is_rectangle yuvp_size_test_config[] = {
	{128, 32},	/* min size */
	{4880, 12288},	/* max size */
	{4032, 3024},	/* customized size */
};

/* Define the test cases. */
static void pablo_hw_api_yuvp_hw_is_occurred0_kunit_test(struct kunit *test)
{
	u32 ret, status;
	int test_idx;
	u32 err_interrupt_list[] = {
		INTR1_YUVP_CMDQ_ERROR_INT,
		INTR1_YUVP_C_LOADER_ERROR_INT,
		INTR1_YUVP_COREX_ERROR_INT,
		INTR1_YUVP_CINFIFO_0_ERROR_INT,
		INTR1_YUVP_COUTFIFO_0_ERROR_INT,
		INTR1_YUVP_COUTFIFO_1_ERROR_INT,
		INTR1_YUVP_VOTF_GLOBAL_ERROR_INT,
		INTR1_YUVP_VOTF_LOST_CONNECTION_INT,
		/* INTR1_YUVP_OTF_SEQ_ID_ERROR_INT, */
	};

	status = 1 << INTR1_YUVP_FRAME_START_INT;
	ret = yuvp_hw_is_occurred0(status, INTR_FRAME_START);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_YUVP_FRAME_END_INT;
	ret = yuvp_hw_is_occurred0(status, INTR_FRAME_END);
	KUNIT_EXPECT_EQ(test, ret, status);

	/*
	status = 1 << INTR1_YUVP_CMDQ_HOLD_INT;
	ret = yuvp_hw_is_occurred0(status, INTR1_YUVP_CMDQ_HOLD_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_YUVP_SETTING_DONE_INT;
	ret = yuvp_hw_is_occurred0(status, INTR1_YUVP_SETTING_DONE_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_YUVP_C_LOADER_END_INT;
	ret = yuvp_hw_is_occurred0(status, INTR1_YUVP_C_LOADER_END_INT);
	KUNIT_EXPECT_EQ(test, ret, status);
	*/

	status = 1 << INTR1_YUVP_COREX_END_INT_0;
	ret = yuvp_hw_is_occurred0(status, INTR_COREX_END_0);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_YUVP_COREX_END_INT_1;
	ret = yuvp_hw_is_occurred0(status, INTR_COREX_END_1);
	KUNIT_EXPECT_EQ(test, ret, status);

	/*
	status = 1 << INTR1_YUVP_ROW_COL_INT;
	ret = yuvp_hw_is_occurred0(status, INTR1_YUVP_ROW_COL_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_YUVP_FREEZE_ON_ROW_COL_INT;
	ret = yuvp_hw_is_occurred0(status, INTR1_YUVP_FREEZE_ON_ROW_COL_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_YUVP_TRANS_STOP_DONE_INT;
	ret = yuvp_hw_is_occurred0(status, INTR1_YUVP_TRANS_STOP_DONE_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR1_YUVP_VOTF_LOST_CONNECTION_INT;
	ret = yuvp_hw_is_occurred0(status, LOST_CONNECTION_INT);
	KUNIT_EXPECT_EQ(test, ret, status);
	*/

	for (test_idx = 0; test_idx < ARRAY_SIZE(err_interrupt_list); test_idx++) {
		status = 1 << err_interrupt_list[test_idx];
		ret = yuvp_hw_is_occurred0(status, INTR_ERR);
		KUNIT_EXPECT_EQ(test, ret, status);
	}
}

static void pablo_hw_api_yuvp_hw_is_occurred1_kunit_test(struct kunit *test)
{
	u32 ret, status;
	int test_idx;
	u32 err_interrupt_list[] = {
		INTR2_YUVP_VOTF_LOST_FLUSH,
		INTR2_YUVP_SYNCFIFO_SEG_DBG_CNT_ERR,
		INTR2_YUVP_DTP_DBG_CNT_ERR,
		INTR2_YUVP_YUVNR_DBG_CNT_ERR,
		INTR2_YUVP_YUV422TO444_DBG_CNT_ERR,
		INTR2_YUVP_YUV2RGB_DBG_CNT_ERR,
		INTR2_YUVP_DEGAMMA_DBG_CNT_ERR,
		INTR2_YUVP_INVCCM_DBG_CNT_ERR,
		INTR2_YUVP_DGF_DBG_CNT_ERR,
		INTR2_YUVP_DRCDSTR_DBG_CNT_ERR,
		INTR2_YUVP_DRCTMC_DBG_CNT_ERR,
		INTR2_YUVP_CCM_DBG_CNT_ERR,
		INTR2_YUVP_RGB_GAMMA_DBG_CNT_ERR,
		INTR2_YUVP_SVHIST_DBG_CNT_ERR,
		INTR2_YUVP_CLAHE_DBG_CNT_ERR,
		INTR2_YUVP_PRC_DBG_CNT_ERR,
		INTR2_YUVP_CLAHE_HIST_ERR,
		INTR2_YUVP_DMA_R2D_CONTENTION_ERR,
	};

	/*
	status = 1 << INTR2_YUVP_VOTF_LOST_FLUSH;
	ret = yuvp_hw_is_occurred1(status, INTR2_YUVP_VOTF_LOST_FLUSH);
	KUNIT_EXPECT_EQ(test, ret, status);
	*/

	for (test_idx = 0; test_idx < ARRAY_SIZE(err_interrupt_list); test_idx++) {
		status = 1 << err_interrupt_list[test_idx];
		ret = yuvp_hw_is_occurred1(status, INTR_ERR);
		KUNIT_EXPECT_EQ(test, ret, status);
	}
}

static void pablo_hw_api_yuvp_hw_s_reset_kunit_test(struct kunit *test)
{
	int ret;

	ret = yuvp_hw_s_reset(test_ctx.pmio);

	KUNIT_EXPECT_EQ(test, ret, -ENODEV); /* timeout */
}

static void pablo_hw_api_yuvp_hw_s_init_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	yuvp_hw_s_init(test_ctx.pmio);

	set_val = YUVP_GET_F(test_ctx.pmio, YUVP_R_CMDQ_QUE_CMD_M, YUVP_F_CMDQ_QUE_CMD_SETTING_MODE);
	expected_val = 3;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_AUTO_IGNORE_PREADY_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_CMDQ_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_DEBUG_CLOCK_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_yuvp_hw_wait_idle_kunit_test(struct kunit *test)
{
	int ret;

	ret = yuvp_hw_wait_idle(test_ctx.pmio);

	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_api_yuvp_hw_s_coutfifo_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_coutfifo(test_ctx.pmio, set_id);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_IP_USE_OTF_PATH_01,
			YUVP_F_IP_USE_OTF_OUT_FOR_PATH_0);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_IP_USE_OTF_PATH_01,
			YUVP_F_IP_USE_OTF_OUT_FOR_PATH_1);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_COUTFIFO0_INTERVALS,
			YUVP_F_YUV_COUTFIFO0_INTERVAL_HBLANK);
	expected_val = HBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_COUTFIFO1_INTERVALS,
			YUVP_F_YUV_COUTFIFO1_INTERVAL_HBLANK);
	expected_val = HBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_COUTFIFO0_INTERVAL_VBLANK,
			YUVP_F_YUV_COUTFIFO0_INTERVAL_VBLANK);
	expected_val = VBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_COUTFIFO1_INTERVAL_VBLANK,
			YUVP_F_YUV_COUTFIFO1_INTERVAL_VBLANK);
	expected_val = VBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_YUV_COUTFIFO0_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_COUTFIFO0_CONFIG,
			YUVP_F_YUV_COUTFIFO0_DEBUG_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_YUV_COUTFIFO1_ENABLE);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_COUTFIFO1_CONFIG,
			YUVP_F_YUV_COUTFIFO1_DEBUG_EN);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_COUTFIFO0_CONFIG,
			YUVP_F_YUV_COUTFIFO0_VVALID_RISE_AT_FIRST_DATA_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);


	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_COUTFIFO1_CONFIG,
			YUVP_F_YUV_COUTFIFO1_VVALID_RISE_AT_FIRST_DATA_EN);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_yuvp_hw_s_cinfifo_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_cinfifo(test_ctx.pmio, set_id);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_CINFIFO0_INTERVALS,
			YUVP_F_YUV_CINFIFO0_INTERVAL_HBLANK);
	expected_val = HBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_YUV_CINFIFO0_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_CINFIFO0_CONFIG,
			YUVP_F_YUV_CINFIFO0_STALL_BEFORE_FRAME_START_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_F(test_ctx.pmio,
			YUVP_R_YUV_CINFIFO0_CONFIG, YUVP_F_YUV_CINFIFO0_DEBUG_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_yuvp_hw_s_core_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	u32 num_buffers = 0;
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_core(test_ctx.pmio, num_buffers, set_id);

	/* _yuvp_hw_s_common */
	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_AUTO_IGNORE_INTERRUPT_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* _yuvp_hw_s_int_mask */
	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_INT_REQ_INT0_ENABLE);
	expected_val = INT1_YUVP_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_INT_REQ_INT1_ENABLE);
	expected_val = INT2_YUVP_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = YUVP_GET_F(test_ctx.pmio, YUVP_R_CMDQ_QUE_CMD_L, YUVP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE);
	expected_val = INT_GRP_YUVP_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* _yuvp_hw_s_secure_id */
	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_SECU_CTRL_SEQID);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_yuvp_hw_rdma_create_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 dma_id = 0;
	int test_idx;

	for (test_idx = YUVP_RDMA_Y; test_idx < YUVP_RDMA_MAX; test_idx++) {
		dma_id = test_idx;

		ret = yuvp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* DMA id err test */
	dma_id = -1;
	ret = yuvp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
	KUNIT_EXPECT_EQ(test, ret, SET_ERROR);
}

static void pablo_hw_api_yuvp_hw_s_rdma_corex_id_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_rdma_corex_id(&dma, set_id);
}

static void pablo_hw_api_yuvp_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	pdma_addr_t addr = 0;
	u32 plane = 0;
	u32 num_buffers = 0;
	int buf_idx = 0;
	u32 comp_sbwc_en = 0;
	u32 payload_size = 0;

	ret = yuvp_hw_s_rdma_addr(&dma, &addr, plane,
			num_buffers, buf_idx, comp_sbwc_en, payload_size);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_yuvp_hw_wdma_create_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 dma_id = 0;
	int test_idx;

	for (test_idx = YUVP_WDMA_SVHIST; test_idx < YUVP_WDMA_MAX; test_idx++) {
		dma_id = test_idx;

		ret = yuvp_hw_wdma_create(&dma, test_ctx.pmio, dma_id);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* DMA id err test */
	dma_id = -1;
	ret = yuvp_hw_wdma_create(&dma, test_ctx.pmio, dma_id);
	KUNIT_EXPECT_EQ(test, ret, SET_ERROR);
}

static void pablo_hw_api_yuvp_hw_s_wdma_corex_id_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_rdma_corex_id(&dma, set_id);
}

static void pablo_hw_api_yuvp_hw_s_wdma_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	pdma_addr_t addr = 0;
	u32 plane = 0;
	u32 num_buffers = 0;
	int buf_idx = 0;
	u32 comp_sbwc_en = 0;
	u32 payload_size = 0;

	ret = yuvp_hw_s_wdma_addr(&dma, &addr, plane,
			num_buffers, buf_idx, comp_sbwc_en, payload_size);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_yuvp_hw_s_corex_update_type_kunit_test(struct kunit *test)
{
	int ret;
	u32 set_id = COREX_DIRECT;

	ret = yuvp_hw_s_corex_update_type(test_ctx.pmio, set_id);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_yuvp_hw_s_cmdq_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 num_buffers = 1;

	yuvp_hw_s_cmdq(test_ctx.pmio, set_id, num_buffers, 0, 0);
}

static void pablo_hw_api_yuvp_hw_g_int_state0_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;

	state = yuvp_hw_g_int_state0(test_ctx.pmio, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);
}

static void pablo_hw_api_yuvp_hw_s_yuvnr_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 start_pos = 0;
	u32 frame_width = 0;
	u32 dma_width = 0;
	u32 dma_height = 0;
	u32 strip_enable = 0;
	struct yuvp_radial_cfg cfg;
	u32 shift_adder = 0;
	int test_idx;

	for (test_idx = 0; test_idx < ARRAY_SIZE(yuvp_size_test_config); test_idx++) {
		dma_width = yuvp_size_test_config[test_idx].w;
		dma_height = yuvp_size_test_config[test_idx].h;

		yuvp_hw_s_yuvnr_size(test_ctx.pmio, set_id, start_pos, frame_width,
					dma_width, dma_height, strip_enable, &cfg, shift_adder);
	}
}

static void pablo_hw_api_yuvp_hw_s_strip_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 stripe_enable = 0;
	u32 start_pos = 0;
	int ret;

	ret = yuvp_hw_s_strip(test_ctx.pmio, set_id, stripe_enable, start_pos,
		YUVP_STRIP_TYPE_NONE, 0, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_yuvp_hw_s_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	u32 stripe_enable = 0;
	int test_idx;

	for (test_idx = 0; test_idx < ARRAY_SIZE(yuvp_size_test_config); test_idx++) {
		width = yuvp_size_test_config[test_idx].w;
		height = yuvp_size_test_config[test_idx].h;

		yuvp_hw_s_size(test_ctx.pmio, set_id, width, height, stripe_enable);
	}
}

static void pablo_hw_api_yuvp_hw_s_input_path_kunit_test(struct kunit *test)
{
	int path = 0;
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_input_path(test_ctx.pmio, set_id, path);

	set_val = YUVP_GET_F(test_ctx.pmio, YUVP_R_IP_USE_OTF_PATH_01, YUVP_F_IP_USE_OTF_IN_FOR_PATH_0);
	expected_val = path;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_yuvp_hw_s_output_path_kunit_test(struct kunit *test)
{
	int path = 0;
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_output_path(test_ctx.pmio, set_id, 0);

	set_val = YUVP_GET_F(test_ctx.pmio, YUVP_R_IP_USE_OTF_PATH_01, YUVP_F_IP_USE_OTF_OUT_FOR_PATH_0);
	expected_val = path;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_yuvp_hw_s_demux_enable_kunit_test(struct kunit *test)
{
	int type = 0;
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_demux_enable(test_ctx.pmio, set_id, type);

	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_CHAIN_DEMUX_ENABLE);
	expected_val = type;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_yuvp_hw_s_mux_dtp_kunit_test(struct kunit *test)
{
	int type = 0;
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_mux_dtp(test_ctx.pmio, set_id, type);

	set_val = YUVP_GET_R(test_ctx.pmio, YUVP_R_CHAIN_MUX_SELECT);
	expected_val = type;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_yuvp_hw_s_mono_mode_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_mono_mode(test_ctx.pmio, set_id, false);
}

static void pablo_hw_api_yuvp_hw_dma_dump_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0 };

	yuvp_hw_dma_dump(&dma);
}

static void pablo_hw_api_yuvp_hw_dump_kunit_test(struct kunit *test)
{
	yuvp_hw_dump(test_ctx.pmio);
}

static void pablo_hw_api_yuvp_hw_g_int_mask1_kunit_test(struct kunit *test)
{
	unsigned int mask;

	mask = yuvp_hw_g_int_mask1(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);
}

static void pablo_hw_api_yuvp_hw_g_int_state1_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;

	state = yuvp_hw_g_int_state1(test_ctx.pmio, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);
}

static void pablo_hw_api_yuvp_hw_print_chain_debug_cnt_kunit_test(struct kunit *test)
{
	yuvp_hw_print_chain_debug_cnt(test_ctx.pmio);
}

static void pablo_hw_api_yuvp_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_block_bypass(test_ctx.pmio, set_id);
}

static void pablo_hw_api_yuvp_hw_s_dtp_pattern_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	enum yuvp_dtp_pattern pattern = YUVP_DTP_PATTERN_SOLID;

	yuvp_hw_s_dtp_pattern(test_ctx.pmio, set_id, pattern);
}

static void pablo_hw_api_yuvp_hw_s_crc_kunit_test(struct kunit *test)
{
	u32 seed = 0;

	yuvp_hw_s_crc(test_ctx.pmio, seed);
}

static void pablo_hw_api_yuvp_hw_g_int_mask0_kunit_test(struct kunit *test)
{
	unsigned int mask;

	mask = yuvp_hw_g_int_mask0(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);
}

static void pablo_hw_api_yuvp_hw_s_int_mask0_kunit_test(struct kunit *test)
{
	unsigned int mask = 0;

	yuvp_hw_s_int_mask0(test_ctx.pmio, mask);
}

static void pablo_hw_api_yuvp_hw_s_strgen_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;

	yuvp_hw_s_strgen(test_ctx.pmio, set_id);
}

static void pablo_hw_api_yuvp_hw_s_clock_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	yuvp_hw_s_clock(test_ctx.pmio, true);

	set_val = YUVP_GET_F(test_ctx.pmio, YUVP_R_IP_PROCESSING, YUVP_F_IP_PROCESSING);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	yuvp_hw_s_clock(test_ctx.pmio, false);

	set_val = YUVP_GET_F(test_ctx.pmio, YUVP_R_IP_PROCESSING, YUVP_F_IP_PROCESSING);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static int __pablo_hw_api_yuvp_pmio_init(struct kunit *test)
{
	int ret;

	test_ctx.pmio_config.name = "yuvp";

	test_ctx.pmio_config.mmio_base = test_ctx.addr;

	test_ctx.pmio_config.cache_type = PMIO_CACHE_NONE;

	yuvp_hw_init_pmio_config(&test_ctx.pmio_config);

	test_ctx.pmio = pmio_init(NULL, NULL, &test_ctx.pmio_config);

	if (IS_ERR(test_ctx.pmio)) {
		err("failed to init yuvp PMIO: %ld", PTR_ERR(test_ctx.pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(test_ctx.pmio, &test_ctx.pmio_fields,
			test_ctx.pmio_config.fields,
			test_ctx.pmio_config.num_fields);
	if (ret) {
		err("failed to alloc yuvp PMIO fields: %d", ret);
		pmio_exit(test_ctx.pmio);
		return ret;

	}

	test_ctx.pmio_reg_seqs = kunit_kzalloc(test, sizeof(struct pmio_reg_seq) * yuvp_hw_g_reg_cnt(), 0);
	if (!test_ctx.pmio_reg_seqs) {
		err("failed to alloc PMIO multiple write buffer");
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		pmio_exit(test_ctx.pmio);
		return -ENOMEM;
	}

	return ret;
}

static void __pablo_hw_api_yuvp_pmio_deinit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.pmio_reg_seqs);
	pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
	pmio_exit(test_ctx.pmio);
}

static int pablo_hw_api_yuvp_kunit_test_init(struct kunit *test)
{
	int ret;

	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);

	ret = __pablo_hw_api_yuvp_pmio_init(test);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void pablo_hw_api_yuvp_kunit_test_exit(struct kunit *test)
{
	__pablo_hw_api_yuvp_pmio_deinit(test);

	kunit_kfree(test, test_ctx.addr);
	memset(&test_ctx, 0, sizeof(struct pablo_hw_api_yuvp_kunit_test_ctx));
}

static struct kunit_case pablo_hw_api_yuvp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_yuvp_hw_is_occurred0_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_is_occurred1_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_coutfifo_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_cinfifo_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_rdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_rdma_corex_id_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_wdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_wdma_corex_id_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_wdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_corex_update_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_cmdq_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_g_int_state0_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_yuvnr_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_strip_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_input_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_output_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_demux_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_mux_dtp_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_mono_mode_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_dma_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_g_int_mask1_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_g_int_state1_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_print_chain_debug_cnt_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_dtp_pattern_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_crc_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_g_int_mask0_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_int_mask0_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_strgen_kunit_test),
	KUNIT_CASE(pablo_hw_api_yuvp_hw_s_clock_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_yuvp_kunit_test_suite = {
	.name = "pablo-hw-api-yuvp-kunit-test",
	.init = pablo_hw_api_yuvp_kunit_test_init,
	.exit = pablo_hw_api_yuvp_kunit_test_exit,
	.test_cases = pablo_hw_api_yuvp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_yuvp_kunit_test_suite);

MODULE_LICENSE("GPL");
