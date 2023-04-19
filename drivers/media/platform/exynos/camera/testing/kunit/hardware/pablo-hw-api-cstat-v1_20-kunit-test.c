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
#include "hardware/api/is-hw-api-cstat.h"
#include "hardware/sfr/is-sfr-cstat-v1_20.h"

/* Define the test cases. */
#define CSTAT_GET_F_COREX(base, R, F) \
	is_hw_get_field((base + CSTAT_COREX_SDW_OFFSET), &cstat_regs[R], &cstat_fields[F])

#define CSTAT_GET_F_DIRECT(base, R, F) \
	is_hw_get_field(base, &cstat_regs[R], &cstat_fields[F])

#define CSTAT_TEST_SET		3

static struct pablo_hw_api_cstat_test_ctx {
	void *test_addr;
} pablo_hw_api_cstat_test_ctx;

struct pablo_hw_api_cstat_test_config {
	enum cstat_input_path input_path;
	enum cstat_input_bitwidth input_bitwidth;
	struct is_rectangle input;
	struct is_rectangle cdaf_mw;
	struct is_crop crop_dzoom;
	struct is_rectangle bns;
	struct is_crop cds;
};

const static struct pablo_hw_api_cstat_test_config test_config[CSTAT_TEST_SET] = {
	/* min size */
	{OTF, INPUT_10B, {128, 128},	/* input */
	 {1, 1},			/* cdaf_mw */
	 {0, 0, 128, 128},		/* crop_dzoom */
	 {64, 64},			/* bns */
	 {0, 0, 64, 64}},		/* cds */
	/* max size */
	{OTF, INPUT_10B, {16384, 12288},
	 {19, 15},
	 {0, 0, 16384, 12288},
	 {8192, 6144},
	 {0, 0, 1920, 1080}},
	/* customized size */
	{OTF, INPUT_10B, {4032, 3024},
	 {10, 8},
	 {0, 0, 4032, 3024},
	 {2016, 1512},
	 {0, 0, 960, 720}},
};

static void pablo_hw_api_cstat_hw_corex_resume_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;
	u32 val;

	cstat_hw_corex_resume(ctx->test_addr);

	val = CSTAT_GET_F_DIRECT(ctx->test_addr, CSTAT_R_COREX_UPDATE_TYPE_0,
			CSTAT_F_COREX_UPDATE_TYPE_0);
	KUNIT_EXPECT_EQ(test, val, COREX_COPY);
	val = CSTAT_GET_F_DIRECT(ctx->test_addr, CSTAT_R_COREX_START_0,
			CSTAT_F_COREX_START_0);
	KUNIT_EXPECT_EQ(test, val, 1);
	val = CSTAT_GET_F_DIRECT(ctx->test_addr, CSTAT_R_COREX_UPDATE_MODE_0,
			CSTAT_F_COREX_UPDATE_MODE_0);
	KUNIT_EXPECT_EQ(test, val, HW_TRIGGER);
}

static void pablo_hw_api_cstat_hw_dump_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_dump(ctx->test_addr);
}

static void pablo_hw_api_cstat_hw_g_version_kunit_test(struct kunit *test)
{
	u32 version;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	version =  cstat_hw_g_version(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, version, (u32)0);
}

static void pablo_hw_api_cstat_hw_print_err_kunit_test(struct kunit *test)
{
	char name[16] = "kunit_test";
	u32 instance = 0;
	u32 fcount = 0;
	ulong src1 = 0;
	ulong src2 = 0;

	cstat_hw_print_err(name, instance, fcount, src1, src2);
}

static void pablo_hw_api_cstat_hw_print_stall_status_kunit_test(struct kunit *test)
{
	int ch = 0;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_print_stall_status(ctx->test_addr, ch);
}

static void pablo_hw_api_cstat_hw_s_default_blk_cfg_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_default_blk_cfg(ctx->test_addr);
}

static void pablo_hw_api_cstat_hw_s_freeze_on_col_row_kunit_test(struct kunit *test)
{
	enum cstat_int_on_col_row_src src = CSTAT_CINFIFO;
	u32 col = 0;
	u32 row = 0;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_freeze_on_col_row(ctx->test_addr, src, col, row);
}

static void pablo_hw_api_cstat_hw_s_fro_kunit_test(struct kunit *test)
{
	u32 num_buffer = 0;
	unsigned long grp_mask = 0;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_fro(ctx->test_addr, num_buffer, grp_mask);
}

static void pablo_hw_api_cstat_hw_s_one_shot_enabl_kunit_teste(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* set idleness status */
	is_hw_set_field(ctx->test_addr, &cstat_regs[CSTAT_R_IDLENESS_STATUS],
			&cstat_fields[CSTAT_F_IDLENESS_STATUS], 1);

	ret = cstat_hw_s_one_shot_enable(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_cstat_hw_s_sdc_enable_kunit_test(struct kunit *test)
{
	bool enable = false;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_sdc_enable(ctx->test_addr, enable);
}

static void pablo_hw_api_cstat_hw_s_crc_seed_kunit_test(struct kunit *test)
{
	u32 seed = 0;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_crc_seed(ctx->test_addr, seed);
}

/*
 * CONTINT
 */
static void pablo_hw_api_cstat_hw_s_int_enable_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* run */
	cstat_hw_s_int_enable(ctx->test_addr);

	/* check output */
	set_val = CSTAT_GET_F_DIRECT(ctx->test_addr,
				    CSTAT_R_CONTINT_SIPUIPP1P0P0_LEVEL_PULSE_N_SEL,
				    CSTAT_F_CONTINT_SIPUIPP1P0P0_LEVEL_PULSE_N_SEL);
	expected_val = ((CSTAT_INT_LEVEL << 1) | CSTAT_INT_LEVEL);
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = CSTAT_GET_F_COREX(ctx->test_addr,
				    CSTAT_R_IP_USE_END_INTERRUPT_ENABLE,
				    CSTAT_F_IP_USE_END_INTERRUPT_ENABLE);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = CSTAT_GET_F_COREX(ctx->test_addr,
				    CSTAT_R_IP_END_INTERRUPT_ENABLE,
				    CSTAT_F_IP_END_INTERRUPT_ENABLE);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = CSTAT_GET_F_COREX(ctx->test_addr,
				    CSTAT_R_IP_CORRUPTED_INTERRUPT_ENABLE,
				    CSTAT_F_IP_CORRUPTED_INTERRUPT_ENABLE);
	expected_val = INT_CRPT_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = CSTAT_GET_F_DIRECT(ctx->test_addr,
				    CSTAT_R_CONTINT_SIPUIPP1P0P0_INT1_ENABLE,
				    CSTAT_F_CONTINT_SIPUIPP1P0P0_INT1_ENABLE);
	expected_val = INT1_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = CSTAT_GET_F_DIRECT(ctx->test_addr,
				    CSTAT_R_CONTINT_SIPUIPP1P0P0_INT2_ENABLE,
				    CSTAT_F_CONTINT_SIPUIPP1P0P0_INT2_ENABLE);
	expected_val = INT2_EN_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static u32 pablo_run_cstat_hw_is_occurred(u32 cstat_int, enum cstat_event_type type)
{
	u32 state;

	/* set input */
	state = BIT_MASK(cstat_int);

	/* run */
	return cstat_hw_is_occurred(state, type);
}

static void pablo_hw_api_cstat_hw_is_occurred_kunit_test(struct kunit *test)
{
	u32 ret;

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_FRAME_START, CSTAT_FS);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_FRAME_INT_ON_ROW_COL_INFO, CSTAT_LINE);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_FRAME_END, CSTAT_FE);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_RGBYHIST_OUT_LAST_PIXEL_ON_OUTPUT, CSTAT_RGBY);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_COREX_END_INT_0, CSTAT_COREX_END);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_IRQ_CORRUPTED_INTERRUPT, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_COREX_ERROR_INT, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_SDC_ERROR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_LIC_CONTROLLER_OVERFLOW_ERROR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_LIC_CONTROLLER_ROW_ERROR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_C_LOADER_AND_COREX_OVERLAP, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_CINFIFO_INPUT_PROTOCOL_ERROR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_CINFIFO_PIXEL_CNT_ERROR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_CINFIFO_OVERFLOW_ERROR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR1_CSTAT_CINFIFO_OVERLAP_ERROR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR2_CSTAT_CDAF_DONE, CSTAT_CDAF);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR2_CSTAT_THSTAT_PRE_DONE, CSTAT_THSTAT_PRE);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR2_CSTAT_THSTAT_AWB_DONE, CSTAT_THSTAT_AWB);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(INTR2_CSTAT_THSTAT_AE_DONE, CSTAT_THSTAT_AE);
	KUNIT_EXPECT_TRUE(test, ret);
}

/*
 * LIC
 */
static void pablo_hw_api_cstat_hw_s_simple_lic_cfg_kunit_test(struct kunit *test)
{
	u32 test_idx, set_val, expected_val;
	struct cstat_simple_lic_cfg simple_lic_cfg;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	for (test_idx = 0; test_idx < CSTAT_TEST_SET; test_idx++) {
		/* set input */
		simple_lic_cfg.bypass = false;
		simple_lic_cfg.input_path = test_config[test_idx].input_path;
		simple_lic_cfg.input_bitwidth = test_config[test_idx].input_bitwidth;
		simple_lic_cfg.input_width = test_config[test_idx].input.w;
		simple_lic_cfg.input_height = test_config[test_idx].input.h;

		/* run */
		cstat_hw_s_simple_lic_cfg(ctx->test_addr, &simple_lic_cfg);

		/* check output */
		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_LIC_INPUT_MODE, CSTAT_F_LIC_BYPASS);
		expected_val = (u32)simple_lic_cfg.bypass;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_LIC_INPUT_MODE, CSTAT_F_LIC_OTF_PRIORITY);
		expected_val = 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_LIC_INPUT_SIZE, CSTAT_F_LIC_WIDTH);
		expected_val = simple_lic_cfg.input_width;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_LIC_INPUT_SIZE, CSTAT_F_LIC_HEIGHT);
		expected_val = simple_lic_cfg.input_height;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_LIC_INPUT_CONFIG_0, CSTAT_F_LIC_BIT_MODE);
		expected_val = simple_lic_cfg.input_bitwidth;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_LIC_INPUT_CONFIG_0, CSTAT_F_LIC_VL_NUM_LINE);
		expected_val = 2;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_LIC_INPUT_CONFIG_0, CSTAT_F_LIC_RDMA_EN);
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_LIC_INPUT_CONFIG_0, CSTAT_F_LIC_STALL_MEM_PORTION);
		expected_val = ((simple_lic_cfg.input_width + 16384) / 3) * 100 / 16384;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_LIC_INPUT_BLANK, CSTAT_F_LIC_IN_HBLANK_CYCLE);
		expected_val = 45;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_LIC_INPUT_BLANK, CSTAT_F_LIC_OUT_HBLANK_CYCLE);
		expected_val = 45;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);
	}
}

/*
 * BNS
 */
static void pablo_hw_api_cstat_hw_s_bns_cfg_kunit_test(struct kunit *test)
{
	u32 test_idx, in_width, in_height, out_width, out_height;
	u32 set_val, expected_val;
	enum cstat_bns_scale_ratio scale_ratio;
	struct is_crop crop_bns;
	struct cstat_size_cfg size_cfg;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	for (test_idx = 0; test_idx < CSTAT_TEST_SET; test_idx++) {
		/* set input */
		in_width = test_config->crop_dzoom.w;
		in_height = test_config->crop_dzoom.h;
		scale_ratio = CSTAT_BNS_X2P0;
		out_width = test_config->bns.w;
		out_height = test_config->bns.h;

		crop_bns.x = 0;
		crop_bns.y = 0;
		crop_bns.w = in_width;
		crop_bns.h = in_height;

		/* run */
		cstat_hw_s_bns_cfg(ctx->test_addr, &crop_bns, &size_cfg);

		/* check output */
		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_CONFIG,
					CSTAT_F_BYR_BNS_FACTORX);
		expected_val = 4;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_CONFIG,
					CSTAT_F_BYR_BNS_FACTORY);
		expected_val = 4;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_OUTPUTSIZE,
					CSTAT_F_BYR_BNS_NOUTPUTTOTALWIDTH);
		expected_val = out_width;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_OUTPUTSIZE,
					CSTAT_F_BYR_BNS_NOUTPUTTOTALHEIGHT);
		expected_val = out_height;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_LINEGAP,
					CSTAT_F_BYR_BNS_LINEGAP);
		expected_val = 4;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_BYPASS,
					CSTAT_F_BYR_BNS_BYPASS);
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);
	}

}

static struct kunit_case pablo_hw_api_cstat_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_cstat_hw_corex_resume_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_g_version_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_print_err_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_print_stall_status_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_default_blk_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_freeze_on_col_row_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_fro_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_one_shot_enabl_kunit_teste),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_sdc_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_crc_seed_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_simple_lic_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_bns_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_is_occurred_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_int_enable_kunit_test),
	{},
};

static int pablo_hw_api_cstat_hw_kunit_test_init(struct kunit *test)
{
	pablo_hw_api_cstat_test_ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pablo_hw_api_cstat_test_ctx.test_addr);

	test->priv = &pablo_hw_api_cstat_test_ctx;

	return 0;
}

static void pablo_hw_api_cstat_hw_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	kunit_kfree(test, ctx->test_addr);
}

struct kunit_suite pablo_hw_api_cstat_kunit_test_suite = {
	.name = "pablo-hw-api-cstat-kunit-test",
	.init = pablo_hw_api_cstat_hw_kunit_test_init,
	.exit = pablo_hw_api_cstat_hw_kunit_test_exit,
	.test_cases = pablo_hw_api_cstat_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_cstat_kunit_test_suite);

MODULE_LICENSE("GPL");
