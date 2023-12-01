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
#include "hardware/api/is-hw-api-lme-v3_0.h"
#include "hardware/sfr/is-sfr-lme-v4_0.h"

#define LME_GET_F(base, R, F)			PMIO_GET_F(base, R, F)
#define LME_GET_R(base, R)			PMIO_GET_R(base, R)

/* Define the test cases. */
static struct lme_test_ctx {
	void *test_addr;
	u32 width, height;
	u32 set_id;
	struct pmio_config		pmio_config;
	struct pablo_mmio		*pmio;
	struct pmio_field		*pmio_fields;
	struct pmio_reg_seq		*pmio_reg_seqs;
} test_ctx;

static void pablo_hw_api_lme_hw_is_occurred(struct kunit *test)
{
	unsigned int occurred;
	unsigned int status = 0;
	enum lme_event_type type = INTR_FRAME_START;

	occurred = lme_hw_is_occurred(status, type);

	KUNIT_EXPECT_EQ(test, occurred, (u32)0);
}

static void pablo_hw_api_lme_hw_s_rdma_init_kunit_test(struct kunit *test)
{
	struct lme_param_set param_set;
	u32 enable = 1;
	u32 id = LME_RDMA_CACHE_IN_0;
	u32 expected_val, set_val;
	int ret;

	param_set.dma.output_width = test_ctx.width;
	param_set.dma.output_height = test_ctx.height;

	ret = lme_hw_s_rdma_init(test_ctx.pmio, &param_set, enable, id, test_ctx.set_id);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check output*/
	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_CACHE_IN_CLIENT_ENABLE);
	expected_val = enable;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	id = LME_RDMA_MBMV_IN;
	ret = lme_hw_s_rdma_init(test_ctx.pmio, &param_set, enable, id, test_ctx.set_id);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_CLIENT_ENABLE);
	expected_val = enable;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LWIDTH);
	expected_val = 2 * DIV_ROUND_UP(test_ctx.width, 16);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LINE_COUNT);
	expected_val = DIV_ROUND_UP(test_ctx.height, 16);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_wdma_init_kunit_test(struct kunit *test)
{
	struct lme_param_set param_set;
	u32 enable = 1;
	u32 id = LME_WDMA_MV_OUT;
	u32 expected_val, set_val;
	enum lme_sps_out_mode out_mode = LME_OUTPUT_MODE_8_4;
	int ret;

	param_set.dma.cur_input_width = test_ctx.width;
	param_set.dma.cur_input_height = test_ctx.height;

	ret = lme_hw_s_wdma_init(test_ctx.pmio, &param_set, enable, id, test_ctx.set_id, out_mode);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check output*/
	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_CLIENT_ENABLE);
	expected_val = enable;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LWIDTH);
	expected_val =  4 * DIV_ROUND_UP(test_ctx.width, 8);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_COUNT);
	expected_val = DIV_ROUND_UP(test_ctx.height, 4);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	out_mode = LME_OUTPUT_MODE_2_2;
	ret = lme_hw_s_wdma_init(test_ctx.pmio, &param_set, enable, id, test_ctx.set_id, out_mode);
	KUNIT_EXPECT_EQ(test, ret, 0);


	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LWIDTH);
	expected_val =  4 * DIV_ROUND_UP(test_ctx.width, 2);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_COUNT);
	expected_val = DIV_ROUND_UP(test_ctx.height, 2);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	struct lme_param_set param_set;
	dma_addr_t dvaddr = 0x87654321f;
	u32 id = LME_RDMA_CACHE_IN_0;
	u32 expected_val, set_val;
	int ret;

	param_set.dma.output_width = test_ctx.width;
	param_set.dma.output_height = test_ctx.height;

	ret = lme_hw_s_rdma_addr(test_ctx.pmio, &dvaddr, id, test_ctx.set_id);
	KUNIT_EXPECT_EQ(test, ret, SET_SUCCESS);

	expected_val = 0xf;
	set_val = LME_GET_R(test_ctx.pmio, LME_R_CACHE_8BIT_BASE_ADDR_1P_0_LSB);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = 0x87654321;
	set_val = LME_GET_R(test_ctx.pmio, LME_R_CACHE_8BIT_BASE_ADDR_1P_0_MSB);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_wdma_addr_kunit_test(struct kunit *test)
{
	struct lme_param_set param_set;
	dma_addr_t dvaddr = 0x876543210;
	u32 id = LME_WDMA_MV_OUT;
	u32 expected_val, set_val;
	int ret;

	param_set.dma.output_width = test_ctx.width;
	param_set.dma.output_height = test_ctx.height;

	ret = lme_hw_s_wdma_addr(test_ctx.pmio, &dvaddr, id, test_ctx.set_id);
	KUNIT_EXPECT_EQ(test, ret, SET_SUCCESS);

	expected_val = 0;
	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BASE_ADDR_LSB);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = 0x87654321;
	set_val = LME_GET_R(test_ctx.pmio, LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BASE_ADDR_0);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_corex_update_type_kunit_test(struct kunit *test)
{
	u32 set_id = COREX_SET_A;
	u32 type = 0;
	int ret = 0;

	ret = lme_hw_s_corex_update_type(test_ctx.pmio, set_id, type);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_lme_hw_s_cmdq_kunit_test(struct kunit *test)
{
	dma_addr_t header_dva = 0;
	u32 num_of_headers = 0, ret;

	/* apb direct mode test */
	lme_hw_s_cmdq(test_ctx.pmio, test_ctx.set_id, header_dva, num_of_headers);
	ret = LME_GET_F(test_ctx.pmio, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE);
	KUNIT_EXPECT_EQ(test, ret, 3);

	/* dma direct mode test */
	header_dva = 0xffffffff;
	num_of_headers = 1;

	lme_hw_s_cmdq(test_ctx.pmio, test_ctx.set_id, header_dva, num_of_headers);
	ret = LME_GET_F(test_ctx.pmio, LME_R_CMDQ_QUE_CMD_M, LME_F_CMDQ_QUE_CMD_SETTING_MODE);
	KUNIT_EXPECT_EQ(test, ret, 1);
}

static void pablo_hw_api_lme_hw_s_corex_init_kunit_test(struct kunit *test)
{
	bool enable = true;

	lme_hw_s_corex_init(test_ctx.pmio, enable);
}

static void pablo_hw_api_lme_hw_s_corex_start_kunit_test(struct kunit *test)
{
	bool enable = true;

	lme_hw_s_corex_start(test_ctx.pmio, enable);
}

static void pablo_hw_api_lme_hw_g_int_state_kunit_test(struct kunit *test)
{

	bool clear = true;
	u32 num_buffers = 1;
	u32 irq_state = 0;
	int ret;

	ret = lme_hw_g_int_state(test_ctx.pmio, clear,
				num_buffers, &irq_state, test_ctx.set_id);
}

static void pablo_hw_api_lme_hw_s_cache_kunit_test(struct kunit *test)
{
	bool enable = true;
	u32 prev_width;
	u32 cur_width;

	prev_width = cur_width = test_ctx.width;
	lme_hw_s_cache(test_ctx.pmio, test_ctx.set_id, enable, prev_width, cur_width);
}

static void pablo_hw_api_lme_hw_s_cache_size_kunit_test(struct kunit *test)
{
	u32 expected_val, set_val;
	u32 prev_width;
	u32 prev_height;
	u32 cur_width;
	u32 cur_height;

	prev_width = cur_width = test_ctx.width;
	prev_height = cur_height = test_ctx.height;

	lme_hw_s_cache_size(test_ctx.pmio, test_ctx.set_id, prev_width, prev_height,
				cur_width, cur_height);

	/* check output */
	expected_val = prev_width;
	set_val = LME_GET_F(test_ctx.pmio,
			LME_R_CACHE_8BIT_IMAGE0_CONFIG,
			LME_F_Y_LME_PRVIMGWIDTH);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = prev_height;
	set_val = LME_GET_F(test_ctx.pmio,
			LME_R_CACHE_8BIT_IMAGE0_CONFIG,
			LME_F_Y_LME_PRVIMGHEIGHT);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = cur_width;
	set_val = LME_GET_F(test_ctx.pmio,
			LME_R_CACHE_8BIT_IMAGE1_CONFIG,
			LME_F_Y_LME_CURIMGWIDTH);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = cur_height;
	set_val = LME_GET_F(test_ctx.pmio,
			LME_R_CACHE_8BIT_IMAGE1_CONFIG,
			LME_F_Y_LME_CURIMGHEIGHT);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_mvct_size_kunit_test(struct kunit *test)
{

	u32 width;
	u32 height;
	u32 expected_val, set_val;

	width = test_ctx.width;
	height = test_ctx.height;

	lme_hw_s_mvct_size(test_ctx.pmio, test_ctx.set_id, width, height);

	expected_val = width;
	set_val = LME_GET_F(test_ctx.pmio,
			LME_R_MVCT_8BIT_IMAGE_DIMENTIONS,
			LME_F_Y_LME_ROISIZEX);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = height;
	set_val = LME_GET_F(test_ctx.pmio,
			LME_R_MVCT_8BIT_IMAGE_DIMENTIONS,
			LME_F_Y_LME_ROISIZEY);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_mvct_kunit_test(struct kunit *test)
{
	lme_hw_s_mvct(test_ctx.pmio, test_ctx.set_id);
}

static void pablo_hw_api_lme_hw_s_first_frame_kunit_test(struct kunit *test)
{
	bool first_frame = true;
	u32 expected_val, set_val;

#if (DDK_INTERFACE_VER == 0x1010)
	lme_hw_s_first_frame(test_ctx.pmio, test_ctx.set_id, first_frame);
#endif
	expected_val = first_frame;
	set_val = LME_GET_F(test_ctx.pmio,
			LME_R_MVCT_8BIT_LME_CONFIG,
			LME_F_Y_LME_FIRSTFRAME);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = 0;
	set_val = LME_GET_F(test_ctx.pmio,
			LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET,
			LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_sps_out_mode_kunit_test(struct kunit *test)
{
	enum lme_sps_out_mode outmode = LME_OUTPUT_MODE_8_4;

	lme_hw_s_sps_out_mode(test_ctx.pmio, test_ctx.set_id, outmode);
}

static void pablo_hw_api_lme_hw_s_crc_kunit_test(struct kunit *test)
{
	u32 seed = 55;

	lme_hw_s_crc(test_ctx.pmio, seed);
}

static void pablo_hw_api_lme_hw_get_reg_struct_kunit_test(struct kunit *test)
{

	struct is_reg *ret;

	ret = lme_hw_get_reg_struct();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, ret);
}

static void pablo_hw_api_lme_hw_get_reg_cnt_kunit_test(struct kunit *test)
{

	unsigned int ret;

	ret = lme_hw_get_reg_cnt();

	KUNIT_EXPECT_EQ(test, ret, (unsigned int)LME_REG_CNT);
}

static void pablo_hw_api_lme_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	lme_hw_s_block_bypass(test_ctx.pmio, test_ctx.set_id);
}

static void pablo_hw_api_lme_hw_s_clock_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;

	lme_hw_s_clock(test_ctx.pmio, true);

	set_val = LME_GET_F(test_ctx.pmio, LME_R_IP_PROCESSING, LME_F_IP_PROCESSING);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	lme_hw_s_clock(test_ctx.pmio, false);

	set_val = LME_GET_F(test_ctx.pmio, LME_R_IP_PROCESSING, LME_F_IP_PROCESSING);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static struct kunit_case pablo_hw_api_lme_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_lme_hw_is_occurred),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_rdma_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_wdma_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_wdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_corex_update_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_cmdq_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_corex_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_corex_start_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_g_int_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_cache_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_cache_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_mvct_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_mvct_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_first_frame_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_sps_out_mode_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_crc_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_get_reg_struct_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_get_reg_cnt_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_block_bypass_kunit_test),
	KUNIT_CASE(pablo_hw_api_lme_hw_s_clock_kunit_test),
	{},
};

static int __pablo_hw_api_lme_pmio_init(struct kunit *test)
{
	int ret;

	test_ctx.pmio_config.name = "lme";

	test_ctx.pmio_config.mmio_base = test_ctx.test_addr;

	test_ctx.pmio_config.cache_type = PMIO_CACHE_NONE;

	lme_hw_init_pmio_config(&test_ctx.pmio_config);

	test_ctx.pmio = pmio_init(NULL, NULL, &test_ctx.pmio_config);
	if (IS_ERR(test_ctx.pmio)) {
		err("failed to init lme PMIO: %ld", PTR_ERR(test_ctx.pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(test_ctx.pmio, &test_ctx.pmio_fields,
			test_ctx.pmio_config.fields,
			test_ctx.pmio_config.num_fields);
	if (ret) {
		err("failed to alloc lme PMIO fields: %d", ret);
		pmio_exit(test_ctx.pmio);
		return ret;

	}

	test_ctx.pmio_reg_seqs = kunit_kzalloc(test, sizeof(struct pmio_reg_seq) * lme_hw_g_reg_cnt(), 0);
	if (!test_ctx.pmio_reg_seqs) {
		err("failed to alloc PMIO multiple write buffer");
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		pmio_exit(test_ctx.pmio);
		return -ENOMEM;
	}

	return ret;
}

static void __pablo_hw_api_lme_pmio_deinit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.pmio_reg_seqs);
	pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
	pmio_exit(test_ctx.pmio);
}

static int pablo_hw_api_lme_hw_kunit_test_init(struct kunit *test)
{
	int ret;

	test_ctx.test_addr = kunit_kzalloc(test, 0x8000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.test_addr);

	test_ctx.width = 320;
	test_ctx.height = 240;
	test_ctx.set_id = COREX_DIRECT;

	if (!lme_hw_use_mmio()) {
		ret = __pablo_hw_api_lme_pmio_init(test);
		KUNIT_ASSERT_EQ(test, ret, 0);
	}

	return 0;
}

static void pablo_hw_api_lme_hw_kunit_test_exit(struct kunit *test)
{
	if (!lme_hw_use_mmio())
		__pablo_hw_api_lme_pmio_deinit(test);

	kunit_kfree(test, test_ctx.test_addr);
	memset(&test_ctx, 0, sizeof(struct lme_test_ctx));
}

struct kunit_suite pablo_hw_api_lme_kunit_test_suite = {
	.name = "pablo-hw-api-lme-v4_0-kunit-test",
	.init = pablo_hw_api_lme_hw_kunit_test_init,
	.exit = pablo_hw_api_lme_hw_kunit_test_exit,
	.test_cases = pablo_hw_api_lme_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_lme_kunit_test_suite);

MODULE_LICENSE("GPL");
