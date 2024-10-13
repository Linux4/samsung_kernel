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
#include "hardware/api/is-hw-api-byrp-v2_0.h"
#include "hardware/sfr/is-sfr-byrp-v1_20.h"

#define BYRP_LUT_REG_CNT		1650 /* DRC */

#define BYRP_SET_F(base, R, F, val)		PMIO_SET_F(base, R, F, val)
#define BYRP_SET_R(base, R, val)		PMIO_SET_R(base, R, val)
#define BYRP_SET_V(base, reg_val, F, val)	PMIO_SET_V(base, reg_val, F, val)
#define BYRP_GET_F(base, R, F)			PMIO_GET_F(base, R, F)
#define BYRP_GET_R(base, R)			PMIO_GET_R(base, R)

#define BYRP_SIZE_TEST_SET		3
#define BYRP_INTR0_MAX			16

static struct byrp_test_ctx {
	void *test_addr;
	struct is_byrp_config		config;
	struct pmio_config		pmio_config;
	struct pablo_mmio		*pmio;
	struct pmio_field		*pmio_fields;
	struct pmio_reg_seq		*pmio_reg_seqs;
	struct is_frame 		frame;
	struct byrp_param_set		param_set;
	struct byrp_param		byrp_param;
} test_ctx;

struct is_rectangle byrp_size_test_config[BYRP_SIZE_TEST_SET] = {
	{128, 128},	/* min size */
	{4880, 12248},	/* max size */
	{4032, 3024},	/* customized size */
};

#define BYRP_SUB_BLOCK_CNT		19

struct sub_block_bypass_set {
	u32 reg_name;
	u32 field_name;
	u32 bypass;
};

struct sub_block_bypass_set byrp_bypass_test_list[BYRP_SUB_BLOCK_CNT] = {
	/* Should not be controlled */
	{BYRP_R_BYR_BITMASK_BYPASS, BYRP_F_BYR_BITMASK_BYPASS, 0x0},
	{BYRP_R_BYR_CROPIN_BYPASS, BYRP_F_BYR_CROPIN_BYPASS, 0x0},
	{BYRP_R_BYR_CROPZSL_BYPASS, BYRP_F_BYR_CROPZSL_BYPASS, 0x0},
	{BYRP_R_BYR_CROPOUT_BYPASS, BYRP_F_BYR_CROPOUT_BYPASS, 0x0},
	{BYRP_R_BYR_DISPARITY_BYPASS, BYRP_F_BYR_DISPARITY_BYPASS, 0x1},
	{BYRP_R_BYR_DIR_BYPASS, BYRP_F_BYR_BPCDIRDETECTOR_BYPASS_DD, 0x0},
	{BYRP_R_BYR_OTFHDR_BYPASS, BYRP_F_BYR_OTFHDR_BYPASS, 0x1},

	/* belows are controlled by nonbayer flag */
	{BYRP_R_BYR_BLCNONBYRPED_BYPASS, BYRP_F_BYR_BLCNONBYRPED_BYPASS, 0x1},
	{BYRP_R_BYR_BLCNONBYRRGB_BYPASS, BYRP_F_BYR_BLCNONBYRRGB_BYPASS, 0x1},
	{BYRP_R_BYR_BLCNONBYRZSL_BYPASS, BYRP_F_BYR_BLCNONBYRZSL_BYPASS, 0x1},
	{BYRP_R_BYR_WBGNONBYR_BYPASS, BYRP_F_BYR_WBGNONBYR_BYPASS, 0x1},
	{BYRP_R_BYR_WBGNONBYR_WB_LIMIT_MAIN_EN, BYRP_F_BYR_WBGNONBYR_B2BITREDUCTION, 0x0},

	/* Can be controlled bypass on/off */
	{BYRP_R_BYR_AFIDENTBPC_BYPASS, BYRP_F_BYR_AFIDENTBPC_BYPASS, 0x1},
	{BYRP_R_BYR_GAMMASENSOR_BYPASS, BYRP_F_BYR_GAMMASENSOR_BYPASS, 0x1},
	{BYRP_R_BYR_CGRAS_BYPASS_REG, BYRP_F_BYR_CGRAS_BYPASS, 0x1},
	{BYRP_R_BYR_SUSP_BYPASS, BYRP_F_BYR_BPCSUSPMAP_BYPASS, 0x1},
	{BYRP_R_BYR_GGC_BYPASS, BYRP_F_BYR_BPCGGC_BYPASS, 0x1},
	{BYRP_R_BYR_FLAT_BYPASS, BYRP_F_BYR_BPCFLATDETECTOR_BYPASS, 0x1},
	{BYRP_R_BYR_DIR_BYPASS, BYRP_F_BYR_BPCDIRDETECTOR_BYPASS, 0x1},
};
/* Define the test cases. */
static void pablo_hw_api_byrp_hw_dump_kunit_test(struct kunit *test)
{
	byrp_hw_dump(test_ctx.pmio);
}

static void pablo_hw_api_byrp_hw_g_int_mask_kunit_test(struct kunit *test)
{
	unsigned int mask;

	BYRP_SET_F(test_ctx.pmio, BYRP_R_INT_REQ_INT0_ENABLE,
		BYRP_F_INT_REQ_INT0_ENABLE, BYRP_INT0_EN_MASK);
	BYRP_SET_F(test_ctx.pmio, BYRP_R_INT_REQ_INT1_ENABLE,
		BYRP_F_INT_REQ_INT1_ENABLE, BYRP_INT1_EN_MASK);

	mask = byrp_hw_g_int0_mask(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)BYRP_INT0_EN_MASK);

	mask = byrp_hw_g_int1_mask(test_ctx.pmio);
	KUNIT_EXPECT_EQ(test, mask, (unsigned int)BYRP_INT1_EN_MASK);
}

static void pablo_hw_api_byrp_hw_g_int_state_kunit_test(struct kunit *test)
{
	unsigned int state;
	bool clear = false;
	u32 num_buffers = 0;
	u32 irq_state = 0;
	u32 val_0 = 0x12345678;
	u32 val_1 = 0x87654321;

	BYRP_SET_R(test_ctx.pmio, BYRP_R_INT_REQ_INT0, val_0);

	state = byrp_hw_g_int0_state(test_ctx.pmio, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)val_0);

	BYRP_SET_R(test_ctx.pmio, BYRP_R_INT_REQ_INT1, val_1);

	state = byrp_hw_g_int1_state(test_ctx.pmio, clear, num_buffers, &irq_state);
	KUNIT_EXPECT_EQ(test, state, (unsigned int)val_1);
}

static void pablo_hw_api_byrp_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 ret;
	int test_idx;

	byrp_hw_s_block_bypass(test_ctx.pmio, set_id);

	for (test_idx = 0; test_idx < BYRP_SUB_BLOCK_CNT; test_idx++) {
		ret = BYRP_GET_F(test_ctx.pmio, byrp_bypass_test_list[test_idx].reg_name,
			byrp_bypass_test_list[test_idx].field_name);
		KUNIT_EXPECT_EQ(test, ret, byrp_bypass_test_list[test_idx].bypass);
	}
}

static void pablo_hw_api_byrp_hw_s_corex_init_kunit_test(struct kunit *test)
{
	bool enable;
	int ret;

	enable = false;
	ret = byrp_hw_s_corex_init(test_ctx.pmio, enable);
	KUNIT_EXPECT_EQ(test, ret, 0);

	enable = true;
	ret = byrp_hw_s_corex_init(test_ctx.pmio, enable);
	KUNIT_EXPECT_EQ(test, ret, BYRP_TRY_COUNT + 1);
}

static void pablo_hw_api_byrp_hw_s_corex_update_type_kunit_test(struct kunit *test)
{
	int ret;
	u32 set_id = COREX_DIRECT;
	u32 type = 0;

	ret = byrp_hw_s_corex_update_type(test_ctx.pmio, set_id, type);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_byrp_hw_s_wdma_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	dma_addr_t addr[8] = { 0 };
	u32 plane = 0;
	u32 num_buffers = 0;
	int buf_idx = 0;
	u32 comp_sbwc_en = 0;
	u32 payload_size = 0;
	u32 image_addr_offset = 0;
	u32 header_addr_offset = 0;
	int test_idx;

	for (test_idx = BYRP_WDMA_BYR; test_idx < BYRP_WDMA_MAX; test_idx++) {
		dma.id = test_idx;

		comp_sbwc_en = false;
		ret = byrp_hw_s_wdma_addr(&dma, addr, plane, num_buffers, buf_idx,
				comp_sbwc_en, payload_size, image_addr_offset, header_addr_offset);
		KUNIT_EXPECT_EQ(test, ret, 0);

		comp_sbwc_en = true;
		ret = byrp_hw_s_wdma_addr(&dma, addr, plane, num_buffers, buf_idx,
				comp_sbwc_en, payload_size, image_addr_offset, header_addr_offset);
		KUNIT_EXPECT_EQ(test, ret, 0);

	}

	/* dma id err test */
	dma.id = -1;
	ret = byrp_hw_s_wdma_addr(&dma, addr, plane, num_buffers, buf_idx,
			comp_sbwc_en, payload_size, image_addr_offset, header_addr_offset);
	KUNIT_EXPECT_EQ(test, ret, SET_ERROR);
}

static void pablo_hw_api_byrp_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	dma_addr_t addr[8] = { 0 };
	u32 plane = 0;
	u32 num_buffers = 0;
	int buf_idx = 0;
	u32 comp_sbwc_en = 0;
	u32 payload_size = 0;
	u32 image_addr_offset = 0;
	u32 header_addr_offset = 0;
	int test_idx;

	for (test_idx = BYRP_RDMA_IMG; test_idx < BYRP_RDMA_MAX; test_idx++) {
		dma.id = test_idx;

		comp_sbwc_en = false;
		ret = byrp_hw_s_rdma_addr(&dma, addr, plane, num_buffers, buf_idx,
				comp_sbwc_en, payload_size, image_addr_offset, header_addr_offset);
		KUNIT_EXPECT_EQ(test, ret, 0);

		comp_sbwc_en = true;
		ret = byrp_hw_s_rdma_addr(&dma, addr, plane, num_buffers, buf_idx,
				comp_sbwc_en, payload_size, image_addr_offset, header_addr_offset);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* dma id err test */
	dma.id = -1;
	ret = byrp_hw_s_rdma_addr(&dma, addr, plane, num_buffers, buf_idx,
			comp_sbwc_en, payload_size, image_addr_offset, header_addr_offset);
	KUNIT_EXPECT_EQ(test, ret, SET_ERROR);
}

static void pablo_hw_api_byrp_hw_wdma_create_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 dma_id = 0;
	int test_idx;

	for (test_idx = BYRP_WDMA_BYR; test_idx < BYRP_WDMA_MAX; test_idx++) {
		dma_id = test_idx;

		ret = byrp_hw_wdma_create(&dma, test_ctx.pmio, dma_id);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* dma id err test */
	dma_id = -1;
	ret = byrp_hw_wdma_create(&dma, test_ctx.pmio, dma_id);
	KUNIT_EXPECT_EQ(test, ret, SET_ERROR);
}

static void pablo_hw_api_byrp_hw_rdma_create_kunit_test(struct kunit *test)
{
	int ret;
	struct is_common_dma dma = { 0 };
	u32 dma_id = 0;
	int test_idx;

	for (test_idx = BYRP_RDMA_IMG; test_idx < BYRP_RDMA_MAX; test_idx++) {
		dma_id = test_idx;

		ret = byrp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* dma id err test */
	dma_id = -1;
	ret = byrp_hw_rdma_create(&dma, test_ctx.pmio, dma_id);
	KUNIT_EXPECT_EQ(test, ret, SET_ERROR);
}

static void pablo_hw_api_byrp_hw_s_strgen_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 set_val, expected_val;

	byrp_hw_s_strgen(test_ctx.pmio, set_id);

	/* RDMA input off*/
	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_RDMABYRIN_EN);
	expected_val =  0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* OTF input select */
	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_IP_USE_OTF_PATH);
	expected_val =  1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* DTP path select */
	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_CHAIN_INPUT_0_SELECT);
	expected_val =  1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* STRGEN setting */
	set_val = BYRP_GET_F(test_ctx.pmio, BYRP_R_BYR_CINFIFO_CONFIG, BYRP_F_BYR_CINFIFO_STRGEN_MODE_EN);
	expected_val =  1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* cinfifo enable */
	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_CINFIFO_ENABLE);
	expected_val =  1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_byrp_hw_s_core_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 num_buffers = 1;
	struct byrp_param_set *param_set = &test_ctx.param_set;
	u32 set_val, expected_val;

	byrp_hw_s_core(test_ctx.pmio, num_buffers, set_id, param_set);

	/* byrp_hw_s_cinfifo */
	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_CINFIFO_ENABLE);
	expected_val =  0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* byrp_hw_s_coutfifo */
	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_BYR_COUTFIFO_ENABLE);
	expected_val =  1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_byrp_hw_is_occurred_kunit_test(struct kunit *test)
{
	u32 ret, status;
	int test_idx;
	u32 err_interrupt0_list[BYRP_INTR0_MAX] = {
		INTR0_BYRP_CMDQ_ERROR_INT,
		INTR0_BYRP_C_LOADER_ERROR_INT,
		INTR0_BYRP_COREX_ERROR_INT,
		INTR0_BYRP_CINFIFO_0_OVERFLOW_ERROR_INT,
		INTR0_BYRP_CINFIFO_0_OVERLAP_ERROR_INT,
		INTR0_BYRP_CINFIFO_0_PIXEL_CNT_ERROR_INT,
		INTR0_BYRP_CINFIFO_0_INPUT_PROTOCOL_ERROR_INT,
		INTR0_BYRP_COUTFIFO_0_PIXEL_CNT_ERROR_INT,
		INTR0_BYRP_COUTFIFO_0_INPUT_PROTOCOL_ERROR_INT,
		INTR0_BYRP_COUTFIFO_0_OVERFLOW_ERROR_INT,
		INTR0_BYRP_COUTFIFO_1_PIXEL_CNT_ERROR_INT,
		INTR0_BYRP_COUTFIFO_1_INPUT_PROTOCOL_ERROR_INT,
		INTR0_BYRP_COUTFIFO_1_OVERFLOW_ERROR_INT,
		INTR0_BYRP_VOTF_C2COM_SLOW_RING_INT,
		INTR0_BYRP_VOTF_LOST_CONNECTION_INT,
		INTR0_BYRP_OTF_SEQ_ID_ERROR_INT
	};

	status = 1 << INTR0_BYRP_FRAME_START_INT;
	ret = byrp_hw_is_occurred(status, INTR_FRAME_START);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_BYRP_ROW_COL_INT;
	ret = byrp_hw_is_occurred(status, INTR_FRAME_CINROW);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_BYRP_FRAME_END_INT;
	ret = byrp_hw_is_occurred(status, INTR_FRAME_END);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_BYRP_COREX_END_INT_0;
	ret = byrp_hw_is_occurred(status, INTR_COREX_END_0);
	KUNIT_EXPECT_EQ(test, ret, status);

	status = 1 << INTR0_BYRP_COREX_END_INT_1;
	ret = byrp_hw_is_occurred(status, INTR_COREX_END_1);
	KUNIT_EXPECT_EQ(test, ret, status);

	for (test_idx = 0; test_idx < BYRP_INTR0_MAX; test_idx++) {
		status = 1 << err_interrupt0_list[test_idx];
		ret = byrp_hw_is_occurred(status, INTR_ERR0);
		KUNIT_EXPECT_EQ(test, ret, status);
	}
}

static void pablo_hw_api_byrp_hw_s_reset_kunit_test(struct kunit *test)
{
	int ret;

	ret = byrp_hw_s_reset(test_ctx.pmio);

	KUNIT_EXPECT_EQ(test, ret, BYRP_TRY_COUNT + 1); /* timeout */
}

static void pablo_hw_api_byrp_hw_s_init_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;

	byrp_hw_s_init(test_ctx.pmio, set_id);
}

static void pablo_hw_api_byrp_hw_wait_idle_kunit_test(struct kunit *test)
{
	int ret;

	ret = byrp_hw_wait_idle(test_ctx.pmio);

	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_api_byrp_hw_s_cmdq_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 num_buffers = 1;

	byrp_hw_s_cmdq(test_ctx.pmio, set_id, num_buffers, 0, 0);
}

static void pablo_hw_api_byrp_hw_s_grid_cfg_kunit_test(struct kunit *test)
{
	struct byrp_grid_cfg grid_cfg;

	byrp_hw_s_grid_cfg(test_ctx.pmio, &grid_cfg);
}

static void pablo_hw_api_byrp_hw_s_bcrop_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	int test_idx, test_idx2;

	for (test_idx = 0; test_idx < BYRP_SIZE_TEST_SET; test_idx++) {
		width = byrp_size_test_config[test_idx].w;
		height = byrp_size_test_config[test_idx].h;

		for (test_idx2 = 0; test_idx2 < BYRP_BCROP_MAX; test_idx2++)
			byrp_hw_s_bcrop_size(test_ctx.pmio, set_id, test_idx2, 0, 0, width, height);
	}
}

static void pablo_hw_api_byrp_hw_s_chain_size_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	u32 width, height;
	int test_idx;

	for (test_idx = 0; test_idx < BYRP_SIZE_TEST_SET; test_idx++) {
		width = byrp_size_test_config[test_idx].w;
		height = byrp_size_test_config[test_idx].h;

		byrp_hw_s_chain_size(test_ctx.pmio, set_id, width, height);
	}
}

static void pablo_hw_api_byrp_hw_s_clock_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	byrp_hw_s_clock(test_ctx.pmio, true);

	set_val = BYRP_GET_F(test_ctx.pmio, BYRP_R_IP_PROCESSING, BYRP_F_IP_PROCESSING);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	byrp_hw_s_clock(test_ctx.pmio, false);

	set_val = BYRP_GET_F(test_ctx.pmio, BYRP_R_IP_PROCESSING, BYRP_F_IP_PROCESSING);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_byrp_hw_s_path_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_DIRECT;
	struct byrp_param_set *param_set = &test_ctx.param_set;
	u32 set_val, expected_val;

	byrp_hw_s_path(test_ctx.pmio, set_id, param_set, &test_ctx.config);
	/* test byrp_hw_s_path */
	/* chain input0 select
	 * 0: Disconnect path
	 * 1: CINFIFO (Not used)
	 * 2: RDMA
	 */
	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_CHAIN_INPUT_0_SELECT);
	expected_val =  2;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_CHAIN_OUTPUT_0_SELECT);
	expected_val =  1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = BYRP_GET_R(test_ctx.pmio, BYRP_R_CHAIN_OUTPUT_1_SELECT);
	expected_val =  1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_byrp_hw_g_dma_param_ptr_kunit_test(struct kunit *test)
{
	int ret;
	u32 rdma_param_max_cnt, wdma_param_max_cnt;
	char * name;
	struct param_dma_input *pdi;
	struct param_dma_output *pdo;
	dma_addr_t *dma_frame_dva;
	pdma_addr_t *param_set_dva;
	struct is_frame *dma_frame = &test_ctx.frame;
	struct byrp_param_set *param_set = &test_ctx.param_set;

	name = __getname();

	/* RDMA */
	rdma_param_max_cnt = byrp_hw_g_rdma_cfg_max_cnt();
	KUNIT_EXPECT_EQ(test, rdma_param_max_cnt, BYRP_RDMA_CFG_MAX);

	ret = byrp_hw_g_rdma_param_ptr(BYRP_RDMA_CFG_IMG, dma_frame, param_set,
		name, &dma_frame_dva, &pdi, &param_set_dva);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
	KUNIT_EXPECT_PTR_EQ(test, (dma_addr_t *)dma_frame_dva,
		(dma_addr_t *)dma_frame->dvaddr_buffer);
	KUNIT_EXPECT_PTR_EQ(test, (struct param_dma_input *)pdi,
		(struct param_dma_input *)&param_set->dma_input);
	KUNIT_EXPECT_PTR_EQ(test, (pdma_addr_t *)param_set_dva,
		(pdma_addr_t *)param_set->input_dva);

	ret = byrp_hw_g_rdma_param_ptr(BYRP_RDMA_CFG_HDR, dma_frame, param_set,
		name, &dma_frame_dva, &pdi, &param_set_dva);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
	KUNIT_EXPECT_PTR_EQ(test, (dma_addr_t *)dma_frame_dva,
		(dma_addr_t *)dma_frame->dva_byrp_hdr);
	KUNIT_EXPECT_PTR_EQ(test, (struct param_dma_input *)pdi,
		(struct param_dma_input *)&param_set->dma_input_hdr);
	KUNIT_EXPECT_PTR_EQ(test, (pdma_addr_t *)param_set_dva,
		(pdma_addr_t *)param_set->input_dva_hdr);

	ret = byrp_hw_g_rdma_param_ptr(BYRP_RDMA_CFG_MAX, dma_frame, param_set,
		name, &dma_frame_dva, &pdi, &param_set_dva);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* WDMA */
	wdma_param_max_cnt = byrp_hw_g_wdma_cfg_max_cnt();
	KUNIT_EXPECT_EQ(test, wdma_param_max_cnt, BYRP_WDMA_CFG_MAX);

	ret = byrp_hw_g_wdma_param_ptr(BYRP_WDMA_CFG_BYR, dma_frame, param_set,
		name, &dma_frame_dva, &pdo, &param_set_dva);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
	KUNIT_EXPECT_PTR_EQ(test, (dma_addr_t *)dma_frame_dva,
		(dma_addr_t *)dma_frame->dva_byrp_byr);
	KUNIT_EXPECT_PTR_EQ(test, (struct param_dma_output *)pdo,
		(struct param_dma_output *)&param_set->dma_output_byr);
	KUNIT_EXPECT_PTR_EQ(test, (pdma_addr_t *)param_set_dva,
		(pdma_addr_t *)param_set->output_dva_byr);

	ret = byrp_hw_g_wdma_param_ptr(BYRP_WDMA_CFG_BYR_PROCESSED, dma_frame, param_set,
		name, &dma_frame_dva, &pdo, &param_set_dva);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
	KUNIT_EXPECT_PTR_EQ(test, (dma_addr_t *)dma_frame_dva,
		(dma_addr_t *)dma_frame->dva_byrp_byr_processed);
	KUNIT_EXPECT_PTR_EQ(test, (struct param_dma_output *)pdo,
		(struct param_dma_output *)&param_set->dma_output_byr_processed);
	KUNIT_EXPECT_PTR_EQ(test, (pdma_addr_t *)param_set_dva,
		(pdma_addr_t *)param_set->output_dva_byr_processed);

	ret = byrp_hw_g_wdma_param_ptr(BYRP_WDMA_CFG_MAX, dma_frame, param_set,
		name, &dma_frame_dva, &pdo, &param_set_dva);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	__putname(name);
}

static void pablo_hw_api_byrp_hw_update_param_kunit_test(struct kunit *test)
{
	struct byrp_param_set *byrp_param_set = &test_ctx.param_set;
	struct byrp_param *byrp_param = &test_ctx.byrp_param;

	IS_DECLARE_PMAP(pmap);

	byrp_param->dma_input.cmd = 1;
	byrp_param->hdr.cmd = 1;
	byrp_param->byr.cmd = 1;
	byrp_param->byr_processed.cmd = 1;
	byrp_param->stripe_input.total_count = 3;

	byrp_hw_update_param(byrp_param, pmap, byrp_param_set);
	KUNIT_EXPECT_NE(test, byrp_param_set->dma_input.cmd, byrp_param->dma_input.cmd);
	KUNIT_EXPECT_NE(test, byrp_param_set->dma_input_hdr.cmd, byrp_param->hdr.cmd);
	KUNIT_EXPECT_NE(test, byrp_param_set->dma_output_byr.cmd, byrp_param->byr.cmd);
	KUNIT_EXPECT_NE(test, byrp_param_set->dma_output_byr_processed.cmd, byrp_param->byr_processed.cmd);
	KUNIT_EXPECT_NE(test, byrp_param_set->stripe_input.total_count, byrp_param->stripe_input.total_count);

	set_bit(PARAM_BYRP_DMA_INPUT, pmap);
	set_bit(PARAM_BYRP_HDR, pmap);
	set_bit(PARAM_BYRP_BYR, pmap);
	set_bit(PARAM_BYRP_BYR_PROCESSED, pmap);
	set_bit(PARAM_BYRP_STRIPE_INPUT, pmap);

	byrp_hw_update_param(byrp_param, pmap, byrp_param_set);
	KUNIT_EXPECT_EQ(test, byrp_param_set->dma_input.cmd, byrp_param->dma_input.cmd);
	KUNIT_EXPECT_EQ(test, byrp_param_set->dma_input_hdr.cmd, byrp_param->hdr.cmd);
	KUNIT_EXPECT_EQ(test, byrp_param_set->dma_output_byr.cmd, byrp_param->byr.cmd);
	KUNIT_EXPECT_EQ(test, byrp_param_set->dma_output_byr_processed.cmd, byrp_param->byr_processed.cmd);
	KUNIT_EXPECT_EQ(test, byrp_param_set->stripe_input.total_count, byrp_param->stripe_input.total_count);
}

static struct kunit_case pablo_hw_api_byrp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_byrp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_g_int_mask_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_g_int_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_corex_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_corex_update_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_wdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_wdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_rdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_strgen_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_is_occurred_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_cmdq_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_grid_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_bcrop_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_chain_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_clock_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_s_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_g_dma_param_ptr_kunit_test),
	KUNIT_CASE(pablo_hw_api_byrp_hw_update_param_kunit_test),
	{},
};

static int __pablo_hw_api_byrp_pmio_init(struct kunit *test)
{
	int ret;

	test_ctx.pmio_config.name = "byrp";

	test_ctx.pmio_config.mmio_base = test_ctx.test_addr;

	test_ctx.pmio_config.cache_type = PMIO_CACHE_NONE;

	byrp_hw_init_pmio_config(&test_ctx.pmio_config);

	test_ctx.pmio = pmio_init(NULL, NULL, &test_ctx.pmio_config);
	if (IS_ERR(test_ctx.pmio)) {
		err("failed to init byrp PMIO: %d", PTR_ERR(test_ctx.pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(test_ctx.pmio, &test_ctx.pmio_fields,
			test_ctx.pmio_config.fields,
			test_ctx.pmio_config.num_fields);
	if (ret) {
		err("failed to alloc byrp PMIO fields: %d", ret);
		pmio_exit(test_ctx.pmio);
		return ret;
	}

	test_ctx.pmio_reg_seqs = kunit_kzalloc(test, sizeof(struct pmio_reg_seq) * byrp_hw_g_reg_cnt(), 0);
	if (!test_ctx.pmio_reg_seqs) {
		err("failed to alloc PMIO multiple write buffer");
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		pmio_exit(test_ctx.pmio);
		return -ENOMEM;
	}

	return ret;
}

static void __pablo_hw_api_byrp_pmio_deinit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.pmio_reg_seqs);
	pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
	pmio_exit(test_ctx.pmio);
}

static int pablo_hw_api_byrp_kunit_test_init(struct kunit *test)
{
	int ret;

	test_ctx.test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);

	ret = __pablo_hw_api_byrp_pmio_init(test);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void pablo_hw_api_byrp_kunit_test_exit(struct kunit *test)
{
	__pablo_hw_api_byrp_pmio_deinit(test);

	kunit_kfree(test, test_ctx.test_addr);
	memset(&test_ctx, 0, sizeof(struct byrp_test_ctx));
}

struct kunit_suite pablo_hw_api_byrp_kunit_test_suite = {
	.name = "pablo-hw-api-byrp-v1_20-kunit-test",
	.init = pablo_hw_api_byrp_kunit_test_init,
	.exit = pablo_hw_api_byrp_kunit_test_exit,
	.test_cases = pablo_hw_api_byrp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_byrp_kunit_test_suite);

MODULE_LICENSE("GPL");
