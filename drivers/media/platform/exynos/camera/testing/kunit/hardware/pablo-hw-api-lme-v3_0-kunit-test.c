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
#include "hardware/sfr/is-sfr-lme-v3_0.h"

#define LME_GET_F_DIRECT(base, R, F) \
	is_hw_get_field(base + GET_LME_COREX_OFFSET(COREX_DIRECT), \
			&lme_regs[R], &lme_fields[F])

#define LME_GET_R_DIRECT(base, R) \
	is_hw_get_reg(base + GET_LME_COREX_OFFSET(COREX_DIRECT), &lme_regs[R])

#define LME_GET_F_COREX(base, S, R, F) \
	is_hw_get_field(base + GET_LME_COREX_OFFSET(S), &lme_regs[R], &lme_fields[F])

#define LME_GET_R_COREX(base, S, R) \
	is_hw_get_reg(base + GET_LME_COREX_OFFSET(S), &lme_regs[R])

/* Define the test cases. */
struct pablo_hw_api_lme_test_ctx {
	void *test_addr;
	u32 width, height;
	u32 set_id;
} pablo_hw_api_lme_test_ctx;

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
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	struct lme_param_set param_set;
	u32 enable = 1;
	u32 id = LME_RDMA_CACHE_IN_0;
	u32 expected_val, set_val;
	int ret;

	param_set.dma.output_width = ctx->width;
	param_set.dma.output_height = ctx->height;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	ret = lme_hw_s_rdma_init(ctx->test_addr, &param_set, enable, id, ctx->set_id);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check output*/
	set_val = LME_GET_R_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_CACHE_IN_CLIENT_ENABLE);
	expected_val = enable;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	id = LME_RDMA_MBMV_IN;
	ret = lme_hw_s_rdma_init(ctx->test_addr, &param_set, enable, id, ctx->set_id);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_val = LME_GET_R_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_MBMV_IN_CLIENT_ENABLE);
	expected_val = enable;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = LME_GET_R_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LWIDTH);
	expected_val = 2 * DIV_ROUND_UP(ctx->width, 16);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = LME_GET_R_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_LINE_COUNT);
	expected_val = DIV_ROUND_UP(ctx->height, 16);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_wdma_init_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	struct lme_param_set param_set;
	u32 enable = 1;
	u32 id = LME_WDMA_MV_OUT;
	u32 expected_val, set_val;
	enum lme_sps_out_mode out_mode = LME_OUTPUT_MODE_8_4;
	int ret;

	param_set.dma.cur_input_width = ctx->width;
	param_set.dma.cur_input_height = ctx->height;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	ret = lme_hw_s_wdma_init(ctx->test_addr, &param_set, enable, id, ctx->set_id, out_mode);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check output*/
	set_val = LME_GET_R_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_CLIENT_ENABLE);
	expected_val = enable;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LWIDTH,
					LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LWIDTH);
	expected_val =  4 * DIV_ROUND_UP(ctx->width, 8);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_COUNT,
					LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_COUNT);
	expected_val = DIV_ROUND_UP(ctx->height, 4);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	out_mode = LME_OUTPUT_MODE_2_2;
	ret = lme_hw_s_wdma_init(ctx->test_addr, &param_set, enable, id, ctx->set_id, out_mode);
	KUNIT_EXPECT_EQ(test, ret, 0);


	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LWIDTH,
					LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LWIDTH);
	expected_val =  4 * DIV_ROUND_UP(ctx->width, 2);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_COUNT,
					LME_F_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_LINE_COUNT);
	expected_val = DIV_ROUND_UP(ctx->height, 2);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	struct lme_param_set param_set;
	dma_addr_t dvaddr = 0x87654321f;
	u32 id = LME_RDMA_CACHE_IN_0;
	u32 expected_val, set_val;
	int ret;

	param_set.dma.output_width = ctx->width;
	param_set.dma.output_height = ctx->height;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	ret = lme_hw_s_rdma_addr(ctx->test_addr, &dvaddr, id, ctx->set_id);
	KUNIT_EXPECT_EQ(test, ret, SET_SUCCESS);

	expected_val = 0xf;
	set_val = LME_GET_R_COREX(ctx->test_addr, ctx->set_id, LME_R_CACHE_8BIT_BASE_ADDR_1P_0_LSB);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = 0x87654321;
	set_val = LME_GET_R_COREX(ctx->test_addr, ctx->set_id, LME_R_CACHE_8BIT_BASE_ADDR_1P_0_MSB);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_wdma_addr_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	struct lme_param_set param_set;
	dma_addr_t dvaddr = 0x876543210;
	u32 id = LME_WDMA_MV_OUT;
	u32 expected_val, set_val;
	int ret;

	param_set.dma.output_width = ctx->width;
	param_set.dma.output_height = ctx->height;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	ret = lme_hw_s_wdma_addr(ctx->test_addr, &dvaddr, id, ctx->set_id);
	KUNIT_EXPECT_EQ(test, ret, SET_SUCCESS);

	expected_val = 0;
	set_val = LME_GET_R_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BASE_ADDR_LSB);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = 0x87654321;
	set_val = LME_GET_R_COREX(ctx->test_addr, ctx->set_id,
					LME_R_DMACLIENT_CNTX0_SPS_MV_OUT_GEOM_BASE_ADDR_0);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_corex_update_type_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	u32 set_id = COREX_SET_A;
	u32 type = 0;
	int ret = 0;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret = lme_hw_s_corex_update_type(ctx->test_addr, set_id, type);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_lme_hw_s_cmdq_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	lme_hw_s_cmdq(ctx->test_addr, ctx->set_id, 0, 0);
}

static void pablo_hw_api_lme_hw_s_corex_init_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	bool enable = true;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	lme_hw_s_corex_init(ctx->test_addr, enable);
}

static void pablo_hw_api_lme_hw_s_corex_start_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	bool enable = true;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	lme_hw_s_corex_start(ctx->test_addr, enable);
}

static void pablo_hw_api_lme_hw_g_int_state_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	bool clear = true;
	u32 num_buffers = 1;
	u32 irq_state = 0;
	int ret;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret = lme_hw_g_int_state(ctx->test_addr, clear,
				num_buffers, &irq_state, ctx->set_id);
}

static void pablo_hw_api_lme_hw_s_cache_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	bool enable = true;
	u32 prev_width;
	u32 cur_width;

	prev_width = cur_width = ctx->width;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	lme_hw_s_cache(ctx->test_addr, ctx->set_id, enable, prev_width, cur_width);
}

static void pablo_hw_api_lme_hw_s_cache_size_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	u32 expected_val, set_val;
	u32 prev_width;
	u32 prev_height;
	u32 cur_width;
	u32 cur_height;

	prev_width = cur_width = ctx->width;
	prev_height = cur_height = ctx->height;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	lme_hw_s_cache_size(ctx->test_addr, ctx->set_id, prev_width, prev_height,
				cur_width, cur_height);

	/* check output */
	expected_val = prev_width;
	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id, LME_R_CACHE_8BIT_IMAGE0_CONFIG,
					LME_F_CACHE_8BIT_IMG_WIDTH_X_0);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = prev_height;
	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id, LME_R_CACHE_8BIT_IMAGE0_CONFIG,
					LME_F_CACHE_8BIT_IMG_HEIGHT_Y_0);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = cur_width;
	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id, LME_R_CACHE_8BIT_IMAGE1_CONFIG,
					LME_F_CACHE_8BIT_IMG_WIDTH_X_1);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = cur_height;
	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id, LME_R_CACHE_8BIT_IMAGE1_CONFIG,
					LME_F_CACHE_8BIT_IMG_HEIGHT_Y_1);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_mvct_size_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	u32 width;
	u32 height;
	u32 expected_val, set_val;

	width = ctx->width;
	height = ctx->height;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	lme_hw_s_mvct_size(ctx->test_addr, ctx->set_id, width, height);

	expected_val = width;
	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id, LME_R_MVCT_8BIT_IMAGE_DIMENTIONS,
					LME_F_MVCT_8BIT_IMAGE_WIDTH);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = height;
	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id, LME_R_MVCT_8BIT_IMAGE_DIMENTIONS,
					LME_F_MVCT_8BIT_IMAGE_HEIGHT);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_mvct_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	lme_hw_s_mvct(ctx->test_addr, ctx->set_id);
}

static void pablo_hw_api_lme_hw_s_first_frame_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	bool first_frame = true;
	u32 expected_val, set_val;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

#if (DDK_INTERFACE_VER == 0x1010)
	lme_hw_s_first_frame(ctx->test_addr, ctx->set_id, first_frame);
#endif
	expected_val = first_frame;
	set_val = LME_GET_F_COREX(ctx->test_addr, ctx->set_id,
					LME_R_MVCT_8BIT_LME_CONFIG,
					LME_F_MVCT_8BIT_LME_FIRST_FRAME);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	expected_val = 0;
	set_val = LME_GET_F_DIRECT(ctx->test_addr,
					LME_R_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET,
					LME_F_DMACLIENT_CNTX0_MBMV_IN_GEOM_BASE_ADDR_ROTATION_RESET);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_lme_hw_s_sps_out_mode_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	enum lme_sps_out_mode outmode = LME_OUTPUT_MODE_8_4;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	lme_hw_s_sps_out_mode(ctx->test_addr, ctx->set_id, outmode);
}

static void pablo_hw_api_lme_hw_s_crc_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	u32 seed = 55;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	lme_hw_s_crc(ctx->test_addr, seed);
}

static void pablo_hw_api_lme_hw_get_reg_struct_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	struct is_reg *ret;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret = lme_hw_get_reg_struct();

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, ret);
}

static void pablo_hw_api_lme_hw_get_reg_cnt_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;
	unsigned int ret;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret = lme_hw_get_reg_cnt();

	KUNIT_EXPECT_EQ(test, ret, (unsigned int)LME_REG_CNT);
}

static void pablo_hw_api_lme_hw_s_block_bypass_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	lme_hw_s_block_bypass(ctx->test_addr, ctx->set_id);
}

static void pablo_hw_api_lme_hw_s_clock_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	lme_hw_s_clock(ctx->test_addr, true);

	set_val = LME_GET_F_DIRECT(ctx->test_addr, LME_R_IP_PROCESSING, LME_F_IP_PROCESSING);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	lme_hw_s_clock(ctx->test_addr, false);

	set_val = LME_GET_F_DIRECT(ctx->test_addr, LME_R_IP_PROCESSING, LME_F_IP_PROCESSING);
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

static int pablo_hw_api_lme_hw_kunit_test_init(struct kunit *test)
{
	pablo_hw_api_lme_test_ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pablo_hw_api_lme_test_ctx.test_addr);
	pablo_hw_api_lme_test_ctx.width = 320;
	pablo_hw_api_lme_test_ctx.height = 240;
	pablo_hw_api_lme_test_ctx.set_id = 0;
	test->priv = &pablo_hw_api_lme_test_ctx;

	return 0;
}

static void pablo_hw_api_lme_hw_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_api_lme_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	kunit_kfree(test, ctx->test_addr);
}

struct kunit_suite pablo_hw_api_lme_kunit_test_suite = {
	.name = "pablo-hw-api-lme-kunit-test",
	.init = pablo_hw_api_lme_hw_kunit_test_init,
	.exit = pablo_hw_api_lme_hw_kunit_test_exit,
	.test_cases = pablo_hw_api_lme_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_lme_kunit_test_suite);

MODULE_LICENSE("GPL");
