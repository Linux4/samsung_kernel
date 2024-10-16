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

#include "is-type.h"
#include "pablo-hw-api-common.h"
#include "hardware/api/is-hw-api-rgbp-v2_0.h"
#include "hardware/sfr/is-sfr-rgbp-v1_40.h"

#include "hardware/is-hw-control.h"
#include "include/is-hw.h"

#define HBLANK_CYCLE			0x2D
#define RGBP_LUT_REG_CNT		1650

#define RGBP_SET_F(base, R, F, val)		PMIO_SET_F(base, R, F, val)
#define RGBP_SET_R(base, R, val)		PMIO_SET_R(base, R, val)
#define RGBP_SET_V(base, reg_val, F, val)	PMIO_SET_V(base, reg_val, F, val)
#define RGBP_GET_F(base, R, F)			PMIO_GET_F(base, R, F)
#define RGBP_GET_R(base, R)			PMIO_GET_R(base, R)

#define RGBP_INTR0_MAX	12
#define RGBP_INTR1_MAX	14

#define RGBP_SIZE_TEST_SET	3

static struct rgbp_test_ctx {
	void *test_addr;
	struct pmio_config		pmio_config;
	struct pablo_mmio		*pmio;
	struct pmio_field		*pmio_fields;
	struct pmio_reg_seq		*pmio_reg_seqs;
} test_ctx;

struct is_rectangle rgbp_size_test_config[RGBP_SIZE_TEST_SET] = {
	{128, 28},	/* min size */
	{4880, 12248},	/* max size */
	{4032, 3024},	/* customized size */
};

static void pablo_hw_api_rgbp_hw_s_dtp_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 set_val, expected_val;

	rgbp_hw_s_dtp(test_ctx.pmio, set_id, 0);

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_BYR_DTP_TEST_PATTERN_MODE);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_s_path_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 set_val, expected_val;
	struct is_rgbp_config config;
	struct rgbp_param_set param_set;

	rgbp_hw_s_path(test_ctx.pmio, set_id, &config, &param_set);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CHAIN_MUX_SELECT, RGBP_F_MUX_LUMASHADING_SELECT);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_DMSC_ENABLE);
	expected_val = 3;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_YUVSC_ENABLE);
	expected_val = 3;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CHAIN_SATFLAG_ENABLE, RGBP_F_CHAIN_SATFLAG_ENABLE);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_is_occurred0_kunit_test(struct kunit *test)
{
	u32 ret, status;
	int test_idx;
	u32 err_interrupt_list[RGBP_INTR0_MAX] = {
		INTR0_RGBP_CMDQ_ERROR_INT,
		INTR0_RGBP_C_LOADER_ERROR_INT,
		INTR0_RGBP_COREX_ERROR_INT,
		INTR0_RGBP_CINFIFO_0_OVERFLOW_ERROR_INT,
		INTR0_RGBP_CINFIFO_0_OVERLAP_ERROR_INT,
		INTR0_RGBP_CINFIFO_0_PIXEL_CNT_ERROR_INT,
		INTR0_RGBP_CINFIFO_0_INPUT_PROTOCOL_ERROR_INT,
		INTR0_RGBP_COUTFIFO_0_PIXEL_CNT_ERROR_INT,
		INTR0_RGBP_COUTFIFO_0_INPUT_PROTOCOL_ERROR_INT,
		INTR0_RGBP_COUTFIFO_0_OVERFLOW_ERROR_INT,
		INTR0_RGBP_VOTF_GLOBAL_ERROR_INT,
		INTR0_RGBP_OTF_SEQ_ID_ERROR_INT,
	};

	status = 1 << INTR0_RGBP_FRAME_START_INT;
	ret = rgbp_hw_is_occurred0(status, INTR0_FRAME_START_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_RGBP_FRAME_END_INT;
	ret = rgbp_hw_is_occurred0(status, INTR0_FRAME_END_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_RGBP_CMDQ_HOLD_INT;
	ret = rgbp_hw_is_occurred0(status, INTR0_CMDQ_HOLD_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_RGBP_SETTING_DONE_INT;
	ret = rgbp_hw_is_occurred0(status, INTR0_SETTING_DONE_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_RGBP_C_LOADER_END_INT;
	ret = rgbp_hw_is_occurred0(status, INTR0_C_LOADER_END_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_RGBP_COREX_END_INT_0;
	ret = rgbp_hw_is_occurred0(status, INTR0_COREX_END_INT_0);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_RGBP_COREX_END_INT_1;
	ret = rgbp_hw_is_occurred0(status, INTR0_COREX_END_INT_1);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_RGBP_ROW_COL_INT;
	ret = rgbp_hw_is_occurred0(status, INTR0_ROW_COL_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_RGBP_TRANS_STOP_DONE_INT;
	ret = rgbp_hw_is_occurred0(status, INTR0_TRANS_STOP_DONE_INT);
	KUNIT_EXPECT_EQ(test, ret, status);

	for (test_idx = 0; test_idx < RGBP_INTR0_MAX; test_idx++) {
		status = 1 << err_interrupt_list[test_idx];
		ret = rgbp_hw_is_occurred0(status, INTR0_ERR0);
		KUNIT_EXPECT_EQ(test, ret, status);
	}
}

static void pablo_hw_api_rgbp_hw_is_occurred1_kunit_test(struct kunit *test)
{
	u32 ret, status;
	int test_idx;
	u32 err_interrupt_list[RGBP_INTR1_MAX] = {
		INTR1_RGBP_VOTF0RDMAGLOBALERROR_INT,
		INTR1_RGBP_VOTF1WDMAGLOBALERROR_INT,
		INTR1_RGBP_DTP_DBG_CNT_ERROR_INT,
		INTR1_RGBP_BDNS_DBG_CNT_ERROR_INT,
		INTR1_RGBP_DMSC_DBG_CNT_ERROR_INT,
		INTR1_RGBP_LSC_DBG_CNT_ERROR_INT,
		INTR1_RGBP_RGB_GAMMA_DBG_CNT_ERROR_INT,
		INTR1_RGBP_GTM_DBG_CNT_ERROR_INT,
		INTR1_RGBP_RGB2YUV_DBG_CNT_ERROR_INT,
		INTR1_RGBP_YUV444TO422_DBG_CNT_ERROR_INT,
		INTR1_RGBP_SATFLAG_DBG_CNT_ERROR_INT,
		INTR1_RGBP_CCM_DBG_CNT_ERROR_INT,
		INTR1_RGBP_YUVSC_DBG_CNT_ERROR_INT,
		INTR1_RGBP_SYNC_MERGE_COUTFIFO_DBG_CNT_ERROR_INT,
	};

	for (test_idx = 0; test_idx < RGBP_INTR1_MAX; test_idx++) {
		status = 1 << err_interrupt_list[test_idx];
		ret = rgbp_hw_is_occurred1(status, INTR1_ERR1);
		KUNIT_EXPECT_EQ(test, ret, status);
	}
}

static void pablo_hw_api_rgbp_hw_s_reset_kunit_test(struct kunit *test)
{
	int ret;

	ret = rgbp_hw_s_reset(test_ctx.pmio);

	KUNIT_EXPECT_EQ(test, ret, RGBP_TRY_COUNT + 1); /* timeout */
}

static void pablo_hw_api_rgbp_hw_s_init_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	rgbp_hw_s_init(test_ctx.pmio);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CMDQ_QUE_CMD_L, RGBP_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE);
	expected_val = RGBP_INT_GRP_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CMDQ_QUE_CMD_M, RGBP_F_CMDQ_QUE_CMD_SETTING_MODE);
	expected_val = 3;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_CMDQ_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_wait_idle_kunit_test(struct kunit *test)
{
	int ret;

	ret = rgbp_hw_wait_idle(test_ctx.pmio);

	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_api_rgbp_hw_dump_kunit_test(struct kunit *test)
{
	rgbp_hw_dump(test_ctx.pmio);
}

static void pablo_hw_api_rgbp_hw_s_core_kunit_test(struct kunit *test)
{
	u32 num_buffers = 1;
	u32 set_id = COREX_DIRECT;
	u32 set_val, expected_val;
	u32 pixel_order = 0;
	struct is_rgbp_config config;
	struct rgbp_param_set param_set;

	rgbp_hw_s_core(test_ctx.pmio, num_buffers, set_id, &config, &param_set);

	/* rgbp_hw_s_cin_fifo */
	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_IP_USE_OTF_PATH, RGBP_F_IP_USE_OTF_IN_FOR_PATH_0);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_BYR_CINFIFO_CONFIG,
			RGBP_F_BYR_CINFIFO_STALL_BEFORE_FRAME_START_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_DEBUG_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_AUTO_RECOVERY_EN);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_BYR_CINFIFO_INTERVALS, RGBP_F_BYR_CINFIFO_INTERVAL_HBLANK);
	expected_val = HBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CHAIN_LBCTRL_HBLANK, RGBP_F_CHAIN_LBCTRL_HBLANK);
	expected_val = HBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_BYR_CINFIFO_ENABLE, RGBP_F_BYR_CINFIFO_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* rgbp_hw_s_common */
	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_AUTO_IGNORE_INTERRUPT_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* rgbp_hw_s_int_mask */
	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_INT_REQ_INT0_ENABLE);
	expected_val = RGBP_INT0_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_INT_REQ_INT1_ENABLE);
	expected_val = RGBP_INT1_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* rgbp_hw_s_path */
	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CHAIN_MUX_SELECT, RGBP_F_MUX_LUMASHADING_SELECT);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_DMSC_ENABLE);
	expected_val = 3;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CHAIN_DEMUX_ENABLE, RGBP_F_DEMUX_YUVSC_ENABLE);
	expected_val = 3;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CHAIN_SATFLAG_ENABLE, RGBP_F_CHAIN_SATFLAG_ENABLE);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* rgbp_hw_s_dtp */
	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_BYR_DTP_TEST_PATTERN_MODE);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* rgbp_hw_s_pixel_order */
	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_BYR_DTP_PIXEL_ORDER);
	expected_val =  pixel_order;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_BYR_DMSC_PIXEL_ORDER);
	expected_val =  pixel_order;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_BYR_SATFLAG_PIXEL_ORDER);
	expected_val =  pixel_order;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* rgbp_hw_s_secure_id */
	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_SECU_CTRL_SEQID, RGBP_F_SECU_CTRL_SEQID);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_s_cin_fifo_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	rgbp_hw_s_cin_fifo(test_ctx.pmio, set_id);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_IP_USE_OTF_PATH, RGBP_F_IP_USE_OTF_IN_FOR_PATH_0);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_BYR_CINFIFO_CONFIG,
			RGBP_F_BYR_CINFIFO_STALL_BEFORE_FRAME_START_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_DEBUG_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_BYR_CINFIFO_CONFIG, RGBP_F_BYR_CINFIFO_AUTO_RECOVERY_EN);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_BYR_CINFIFO_INTERVALS, RGBP_F_BYR_CINFIFO_INTERVAL_HBLANK);
	expected_val = HBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_CHAIN_LBCTRL_HBLANK, RGBP_F_CHAIN_LBCTRL_HBLANK);
	expected_val = HBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_BYR_CINFIFO_ENABLE, RGBP_F_BYR_CINFIFO_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_s_cout_fifo_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	u32 set_id = COREX_DIRECT;

	rgbp_hw_s_cout_fifo(test_ctx.pmio, set_id, 1);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_IP_USE_OTF_PATH, RGBP_F_IP_USE_OTF_OUT_FOR_PATH_0);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio,
			RGBP_R_YUV_COUTFIFO_INTERVALS,
			RGBP_F_YUV_COUTFIFO_INTERVAL_HBLANK);
	expected_val = HBLANK_CYCLE;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_YUV_COUTFIFO_CONFIG, RGBP_F_YUV_COUTFIFO_DEBUG_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_YUV_COUTFIFO_CONFIG, RGBP_F_YUV_COUTFIFO_BACK_STALL_EN);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio,
			RGBP_R_YUV_COUTFIFO_ENABLE,
			RGBP_F_YUV_COUTFIFO_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_s_common_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	rgbp_hw_s_common(test_ctx.pmio);

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_AUTO_IGNORE_INTERRUPT_ENABLE);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_s_int_mask_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	rgbp_hw_s_int_mask(test_ctx.pmio);

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_INT_REQ_INT0_ENABLE);
	expected_val = RGBP_INT0_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_INT_REQ_INT1_ENABLE);
	expected_val = RGBP_INT1_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_s_secure_id_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 set_val, expected_val;

	rgbp_hw_s_secure_id(test_ctx.pmio, set_id);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_SECU_CTRL_SEQID, RGBP_F_SECU_CTRL_SEQID);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_dma_dump_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0, };

	rgbp_hw_dma_dump(&dma);
}

static void pablo_hw_api_rgbp_hw_s_rgb_rdma_format_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	int ret;

	/* 12 is unregisted rgbp format */
	ret = pablo_kunit_rgbp_hw_s_rgb_rdma_format(test_ctx.pmio, 12);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	pablo_kunit_rgbp_hw_s_rgb_rdma_format(test_ctx.pmio, DMA_FMT_RGB_RGBA8888);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_RGB1P_FORMAT, RGBP_F_RDMA_RGB1P_FORMAT);
	expected_val = RGBP_DMA_FMT_RGB_RGBA8888;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_g_input_dva_kunit_test(struct kunit *test)
{
	struct rgbp_param_set param_set;
	u32 instance = 0;
	u32 id = 0;
	u32 cmd;

	rgbp_hw_g_input_dva(&param_set, instance, id, &cmd);
}

static void pablo_hw_api_rgbp_hw_g_output_dva_kunit_test(struct kunit *test)
{
	struct rgbp_param_set param_set;
	u32 instance = 0;
	u32 id = 0;
	u32 cmd;

	rgbp_hw_g_output_dva(&param_set, instance, id, &cmd);
}

static void pablo_hw_api_rgbp_hw_s_rdma_corex_id_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0, };
	u32 set_id = COREX_DIRECT;

	rgbp_hw_s_rdma_corex_id(&dma, set_id);
}

static void pablo_hw_api_rgbp_hw_rdma_create_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0, };
	u32 dma_id = 0;
	int test_idx;

	for (test_idx = 0; test_idx < RGBP_RDMA_MAX; test_idx++) {
		dma_id = test_idx;

		ret = rgbp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* DMA id err test */
	dma_id = -1;
	ret = rgbp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
	KUNIT_EXPECT_EQ(test, ret, SET_ERROR);
}

static void pablo_hw_api_rgbp_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0, };
	pdma_addr_t addr = 0;
	u32 plane = 0;
	u32 num_buffers = 0;
	int buf_idx = 0;
	u32 comp_sbwc_en = 0;
	u32 payload_size = 0;

	ret = rgbp_hw_s_rdma_addr(&dma, &addr, plane,
			num_buffers, buf_idx, comp_sbwc_en, payload_size);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_rgbp_hw_s_wdma_corex_id_kunit_test(struct kunit *test)
{
	struct is_common_dma dma = { 0, };
	u32 set_id = COREX_DIRECT;

	rgbp_hw_s_wdma_corex_id(&dma, set_id);
}

static void pablo_hw_api_rgbp_hw_wdma_create_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0, };
	u32 dma_id = 0;
	int test_idx;

	for (test_idx = 0; test_idx < RGBP_WDMA_MAX; test_idx++) {
		dma_id = test_idx;

		ret = rgbp_hw_wdma_create(&dma, test_ctx.pmio, dma_id);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* DMA id err test */
	dma_id = -1;
	ret = rgbp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
	KUNIT_EXPECT_EQ(test, ret, SET_ERROR);
}

static void pablo_hw_api_rgbp_hw_s_wdma_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0, };
	pdma_addr_t addr = 0;
	u32 plane = 0;
	u32 num_buffers = 0;
	int buf_idx = 0;
	u32 comp_sbwc_en = 0;
	u32 payload_size = 0;

	ret = rgbp_hw_s_wdma_addr(&dma, &addr, plane,
			num_buffers, buf_idx, comp_sbwc_en, payload_size);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_rgbp_hw_s_corex_update_type_kunit_test(struct kunit *test)
{
	int ret;
	u32 set_id = COREX_DIRECT;
	u32 type = 0;

	ret = rgbp_hw_s_corex_update_type(test_ctx.pmio, set_id, type);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_rgbp_hw_s_cmdq_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 num_buffers = 1;

	rgbp_hw_s_cmdq(test_ctx.pmio, set_id, num_buffers, 0, 0);
}

static void pablo_hw_api_rgbp_hw_s_corex_init_kunit_test(struct kunit *test)
{
	bool enable = false;

	rgbp_hw_s_corex_init(test_ctx.pmio, enable);
}

static void pablo_hw_api_rgbp_hw_wait_corex_idle_kunit_test(struct kunit *test)
{
	rgbp_hw_wait_corex_idle(test_ctx.pmio);
}

static void pablo_hw_api_rgbp_hw_s_corex_start_kunit_test(struct kunit *test)
{
	bool enable = false;

	rgbp_hw_s_corex_start(test_ctx.pmio, enable);
}

static void pablo_hw_api_rgbp_hw_g_int0_state_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;

	state = rgbp_hw_g_int0_state(test_ctx.pmio, clear, num_buffers, &irq_state);

	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);
}

static void pablo_hw_api_rgbp_hw_g_int0_mask_kunit_test(struct kunit *test)
{
	rgbp_hw_g_int0_mask(test_ctx.pmio);
}

static void pablo_hw_api_rgbp_hw_g_int1_state_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;

	state = rgbp_hw_g_int1_state(test_ctx.pmio, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)0);
}

static void pablo_hw_api_rgbp_hw_g_int1_mask_kunit_test(struct kunit *test)
{
	unsigned int mask;

	mask = rgbp_hw_g_int1_mask(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)0);
}

static void pablo_hw_api_rgbp_hw_s_pixel_order_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	u32 pixel_order = 0;

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_BYR_DTP_PIXEL_ORDER);
	expected_val =  pixel_order;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_BYR_DMSC_PIXEL_ORDER);
	expected_val =  pixel_order;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_R(test_ctx.pmio, RGBP_R_BYR_SATFLAG_PIXEL_ORDER);
	expected_val =  pixel_order;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_s_chain_src_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	int test_idx;

	for (test_idx = 0; test_idx < RGBP_SIZE_TEST_SET; test_idx++) {
		width = rgbp_size_test_config[test_idx].w;
		height = rgbp_size_test_config[test_idx].h;

		rgbp_hw_s_chain_src_size(test_ctx.pmio, set_id, width, height);
	}
}

static void pablo_hw_api_rgbp_hw_s_chain_dst_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	int test_idx;

	for (test_idx = 0; test_idx < RGBP_SIZE_TEST_SET; test_idx++) {
		width = rgbp_size_test_config[test_idx].w;
		height = rgbp_size_test_config[test_idx].h;

		rgbp_hw_s_chain_dst_size(test_ctx.pmio, set_id, width, height);
	}
}

static void pablo_hw_api_rgbp_hw_s_dtp_output_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	int test_idx;

	for (test_idx = 0; test_idx < RGBP_SIZE_TEST_SET; test_idx++) {
		width = rgbp_size_test_config[test_idx].w;
		height = rgbp_size_test_config[test_idx].h;

		rgbp_hw_s_dtp_output_size(test_ctx.pmio, set_id, width, height);
	}
}

static void pablo_hw_api_rgbp_hw_s_decomp_frame_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	int test_idx;

	for (test_idx = 0; test_idx < RGBP_SIZE_TEST_SET; test_idx++) {
		width = rgbp_size_test_config[test_idx].w;
		height = rgbp_size_test_config[test_idx].h;

		rgbp_hw_s_decomp_frame_size(test_ctx.pmio, set_id, width, height);
	}
}

static void pablo_hw_api_rgbp_hw_s_sc_enable_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 enable = 0;
	u32 bypass = 0;
	u32 set_val, expected_val;

	rgbp_hw_s_yuvsc_enable(test_ctx.pmio, set_id, enable, bypass);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_YUV_SC_CTRL0, RGBP_F_YUV_SC_TOP_BYPASS);
	expected_val =  bypass;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_YUV_SC_CTRL0, RGBP_F_YUV_SC_BYPASS);
	expected_val = bypass;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_YUV_SC_CTRL1, RGBP_F_YUV_SC_LR_HR_MERGE_SPLIT_ON);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_rgbp_hw_s_sc_dst_size_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	int test_idx;

	for (test_idx = 0; test_idx < RGBP_SIZE_TEST_SET; test_idx++) {
		width = rgbp_size_test_config[test_idx].w;
		height = rgbp_size_test_config[test_idx].h;

		rgbp_hw_s_sc_dst_size_size(test_ctx.pmio, set_id, width, height);
	}
}

static void pablo_hw_api_rgbp_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;

	rgbp_hw_s_block_bypass(test_ctx.pmio, set_id);
}

static void pablo_hw_api_rgbp_is_scaler_get_yuvsc_dst_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 h_size = 0;
	u32 v_size = 0;

	is_scaler_get_yuvsc_dst_size(test_ctx.pmio, set_id, &h_size, &v_size);
}

static void pablo_hw_api_rgbp_hw_s_yuvsc_coef_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 hratio = 0;
	u32 vratio = 0;

	rgbp_hw_s_yuvsc_coef(test_ctx.pmio, set_id, hratio, vratio);
}

static void pablo_hw_api_rgbp_hw_s_yuvsc_round_mode_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 mode = 0;

	rgbp_hw_s_yuvsc_round_mode(test_ctx.pmio, set_id, mode);
}

static void pablo_hw_api_rgbp_hw_s_upsc_dst_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	int test_idx;

	for (test_idx = 0; test_idx < RGBP_SIZE_TEST_SET; test_idx++) {
		width = rgbp_size_test_config[test_idx].w;
		height = rgbp_size_test_config[test_idx].h;

		rgbp_hw_s_upsc_dst_size(test_ctx.pmio, set_id, width, height);
	}
}

static void pablo_hw_api_rgbp_is_scaler_get_upsc_dst_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 h_size = 0;
	u32 v_size = 0;

	is_scaler_get_upsc_dst_size(test_ctx.pmio, set_id, &h_size, &v_size);
}

static void pablo_hw_api_rgbp_hw_s_upsc_scaling_ratio_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 hratio = 0;
	u32 vratio = 0;

	rgbp_hw_s_upsc_scaling_ratio(test_ctx.pmio, set_id, hratio, vratio);
}

static void pablo_hw_api_rgbp_hw_s_h_init_phase_offset_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 offset = 0;

	rgbp_hw_s_h_init_phase_offset(test_ctx.pmio, set_id, offset);
}

static void pablo_hw_api_rgbp_hw_s_v_init_phase_offset_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 offset = 0;

	rgbp_hw_s_v_init_phase_offset(test_ctx.pmio, set_id, offset);
}

static void pablo_hw_api_rgbp_hw_s_upsc_enable_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;

	rgbp_hw_s_upsc_enable(test_ctx.pmio, set_id, 0, 0);
}

static void pablo_hw_api_rgbp_hw_s_upsc_coef_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 hratio = 0;
	u32 vratio = 0;

	rgbp_hw_s_upsc_coef(test_ctx.pmio, set_id, hratio, vratio);
}

static void pablo_hw_api_rgbp_hw_s_gamma_enable_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 enable = 0;
	u32 bypass = 0;

	rgbp_hw_s_gamma_enable(test_ctx.pmio, set_id, enable, bypass);
}

static void pablo_hw_api_rgbp_hw_s_decomp_enable_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 enable = 0;
	u32 bypass = 0;

	rgbp_hw_s_decomp_enable(test_ctx.pmio, set_id, enable, bypass);
}

static void pablo_hw_api_rgbp_hw_s_decomp_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	int test_idx;

	for (test_idx = 0; test_idx < RGBP_SIZE_TEST_SET; test_idx++) {
		width = rgbp_size_test_config[test_idx].w;
		height = rgbp_size_test_config[test_idx].h;

		rgbp_hw_s_decomp_size(test_ctx.pmio, set_id, width, height);
	}
}

static void pablo_hw_api_rgbp_hw_s_block_crc_kunit_test(struct kunit *test)
{
	u32 seed = 0;

	rgbp_hw_s_block_crc(test_ctx.pmio, seed);
}

static void pablo_hw_api_rgbp_hw_s_grid_cfg_kunit_test(struct kunit *test)
{
	struct rgbp_grid_cfg cfg = { 0, };

	rgbp_hw_s_grid_cfg(test_ctx.pmio, &cfg);
}

static void pablo_hw_api_rgbp_hw_s_votf_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;

	rgbp_hw_s_votf(test_ctx.pmio, set_id, 0, 0);
}

static void pablo_hw_api_rgbp_hw_s_sbwc_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	bool enable = false;

	rgbp_hw_s_sbwc(test_ctx.pmio, set_id, enable);
}

static void pablo_hw_api_rgbp_hw_s_crop_kunit_test(struct kunit *test)
{
	rgbp_hw_s_crop(test_ctx.pmio, 0, 0, 0, 0);
}

static void pablo_hw_api_rgbp_hw_g_reg_cnt_kunit_test(struct kunit *test)
{
	int ret;

	ret = rgbp_hw_g_reg_cnt();

	KUNIT_EXPECT_EQ(test, ret, RGBP_REG_CNT + RGBP_LUT_REG_CNT);
}

static void pablo_hw_api_rgbp_hw_s_strgen_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;

	rgbp_hw_s_strgen(test_ctx.pmio, set_id);
}

static void pablo_hw_api_rgbp_hw_s_clock_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	rgbp_hw_s_clock(test_ctx.pmio, true);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_IP_PROCESSING, RGBP_F_IP_PROCESSING);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	rgbp_hw_s_clock(test_ctx.pmio, false);

	set_val = RGBP_GET_F(test_ctx.pmio, RGBP_R_IP_PROCESSING, RGBP_F_IP_PROCESSING);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static struct kunit_case pablo_hw_api_rgbp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_dtp_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_is_occurred0_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_is_occurred1_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_cin_fifo_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_cout_fifo_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_common_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_int_mask_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_secure_id_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_dma_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_input_dva_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_output_dva_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_rdma_corex_id_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_rdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_wdma_corex_id_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_wdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_wdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_corex_update_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_cmdq_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_corex_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_wait_corex_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_corex_start_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_int0_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_int0_mask_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_int1_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_int1_mask_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_pixel_order_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_chain_src_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_chain_dst_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_dtp_output_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_decomp_frame_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_sc_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_sc_dst_size_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_is_scaler_get_yuvsc_dst_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_yuvsc_coef_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_yuvsc_round_mode_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_upsc_dst_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_is_scaler_get_upsc_dst_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_upsc_scaling_ratio_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_h_init_phase_offset_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_v_init_phase_offset_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_upsc_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_upsc_coef_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_gamma_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_decomp_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_decomp_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_rgb_rdma_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_block_crc_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_grid_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_votf_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_sbwc_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_crop_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_g_reg_cnt_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_strgen_kunit_test),
	KUNIT_CASE(pablo_hw_api_rgbp_hw_s_clock_kunit_test),
	{},
};

static int __pablo_hw_api_rgbp_pmio_init(struct kunit *test)
{
	int ret;

	test_ctx.pmio_config.name = "rgbp";

	test_ctx.pmio_config.mmio_base = test_ctx.test_addr;

	test_ctx.pmio_config.cache_type = PMIO_CACHE_NONE;

	rgbp_hw_init_pmio_config(&test_ctx.pmio_config);

	test_ctx.pmio = pmio_init(NULL, NULL, &test_ctx.pmio_config);
	if (IS_ERR(test_ctx.pmio)) {
		err("failed to init rgbp PMIO: %d", PTR_ERR(test_ctx.pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(test_ctx.pmio, &test_ctx.pmio_fields,
			test_ctx.pmio_config.fields,
			test_ctx.pmio_config.num_fields);
	if (ret) {
		err("failed to alloc rgbp PMIO fields: %d", ret);
		pmio_exit(test_ctx.pmio);
		return ret;
	}

	test_ctx.pmio_reg_seqs = kunit_kzalloc(test, sizeof(struct pmio_reg_seq) * rgbp_hw_g_reg_cnt(), 0);
	if (!test_ctx.pmio_reg_seqs) {
		err("failed to alloc PMIO multiple write buffer");
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		pmio_exit(test_ctx.pmio);
		return -ENOMEM;
	}

	return ret;
}

static void __pablo_hw_api_rgbp_pmio_deinit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.pmio_reg_seqs);
	pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
	pmio_exit(test_ctx.pmio);
}

static int pablo_hw_api_rgbp_kunit_test_init(struct kunit *test)
{
	int ret;

	test_ctx.test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);

	ret = __pablo_hw_api_rgbp_pmio_init(test);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void pablo_hw_api_rgbp_kunit_test_exit(struct kunit *test)
{
	__pablo_hw_api_rgbp_pmio_deinit(test);

	kunit_kfree(test, test_ctx.test_addr);
	memset(&test_ctx, 0, sizeof(struct rgbp_test_ctx));
}

struct kunit_suite pablo_hw_api_rgbp_kunit_test_suite = {
	.name = "pablo-hw-api-rgbp-kunit-test",
	.init = pablo_hw_api_rgbp_kunit_test_init,
	.exit = pablo_hw_api_rgbp_kunit_test_exit,
	.test_cases = pablo_hw_api_rgbp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_rgbp_kunit_test_suite);

MODULE_LICENSE("GPL");
