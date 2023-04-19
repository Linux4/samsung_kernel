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
#include "hardware/sfr/is-sfr-cstat-v3_0.h"

/* Define the test cases. */
#define CSTAT_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &cstat_regs[R], &cstat_fields[F], val)

#define CSTAT_GET_R_COREX(base, R) \
	is_hw_get_reg((base + CSTAT_COREX_SDW_OFFSET), &cstat_regs[R])

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
	struct is_crop sat;
	struct is_crop cav;
	struct is_crop fdpig;
};

const static struct pablo_hw_api_cstat_test_config test_config[CSTAT_TEST_SET] = {
	/* min size */
	{OTF, INPUT_10B, {128, 128},	/* input */
	 {1, 1},			/* cdaf_mw */
	 {0, 0, 128, 128},		/* crop_dzoom */
	 {84, 84},			/* bns */
	 {0, 0, 64, 64},		/* sat */
	 {0, 0, 64, 64},		/* cav */
	 {0, 0, 64, 64}},		/* fdpig */
	/* max size */
	{OTF, INPUT_10B, {16384, 12288},
	 {19, 15},
	 {0, 0, 16384, 12288},
	 {10922, 8192},
	 {0, 0, 1920, 1440},
	 {0, 0, 640, 480},
	 {0, 0, 640, 480}},
	/* customized size */
	{OTF, INPUT_10B, {4032, 3024},
	 {10, 8},
	 {0, 0, 4032, 3024},
	 {2688, 2016},
	 {0, 0, 960, 720},
	 {0, 0, 320, 240},
	 {0, 0, 320, 240}},
};

static const u32 bns_weights[] = {
	23, 204,  29,  96, 160,   0,   0,   0,   0,   0,   0 /* CSTAT_BNS_X1P5 */
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

static void pablo_hw_api_cstat_hw_s_corex_delay_kunit_test(struct kunit *test)
{
	struct cstat_time_cfg time_cfg = { 0, };
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	time_cfg.req_vvalid = 10000;
	time_cfg.vvalid = 3000;
	time_cfg.clk = 267000000;
	time_cfg.fro_en = true;
	cstat_hw_s_corex_delay(ctx->test_addr, &time_cfg);

	time_cfg.fro_en = false;
	cstat_hw_s_corex_delay(ctx->test_addr, &time_cfg);
}

static void pablo_hw_api_cstat_hw_s_lic_hblank_kunit_test(struct kunit *test)
{
	struct cstat_time_cfg time_cfg = { 0, };
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	time_cfg.clk = 267000000;
	cstat_hw_s_lic_hblank(ctx->test_addr, &time_cfg);
}

static void pablo_hw_api_cstat_hw_is_corex_offset_kunit_test(struct kunit *test)
{
	bool ret;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret = cstat_hw_is_corex_offset(cstat_regs[CSTAT_R_BYR_CGRAS_LUT_START_ADD].sfr_offset);
	KUNIT_EXPECT_EQ(test, ret, false);

	ret = cstat_hw_is_corex_offset(cstat_regs[CSTAT_R_BYR_CGRAS_LUT_ACCESS].sfr_offset);
	KUNIT_EXPECT_EQ(test, ret, false);

	ret = cstat_hw_is_corex_offset(cstat_regs[CSTAT_R_BYR_CGRAS_LUT_ACCESS_SETB].sfr_offset);
	KUNIT_EXPECT_EQ(test, ret, false);

	ret = cstat_hw_is_corex_offset(cstat_regs[CSTAT_R_RGB_LUMASHADING_LUT_START_ADD].sfr_offset);
	KUNIT_EXPECT_EQ(test, ret, false);

	ret = cstat_hw_is_corex_offset(cstat_regs[CSTAT_R_RGB_LUMASHADING_LUT_ACCESS_SETA].sfr_offset);
	KUNIT_EXPECT_EQ(test, ret, false);

	ret = cstat_hw_is_corex_offset(cstat_regs[CSTAT_R_RGB_LUMASHADING_LUT_ACCESS_SETB].sfr_offset);
	KUNIT_EXPECT_EQ(test, ret, false);

	ret = cstat_hw_is_corex_offset(cstat_regs[CSTAT_R_RGB_LUMASHADING_STREAM_CRC].sfr_offset);
	KUNIT_EXPECT_EQ(test, ret, true);
}

static void pablo_hw_api_cstat_hw_s_global_enable_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_global_enable(ctx->test_addr, true, true);
	cstat_hw_s_global_enable(ctx->test_addr, false, true);
}

static void pablo_hw_api_cstat_hw_s_one_shot_enabl_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* set idleness status */
	CSTAT_SET_F_DIRECT(ctx->test_addr, CSTAT_R_IDLENESS_STATUS, CSTAT_F_IDLENESS_STATUS, 1);

	ret = cstat_hw_s_one_shot_enable(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* set idleness status */
	CSTAT_SET_F_DIRECT(ctx->test_addr, CSTAT_R_IDLENESS_STATUS, CSTAT_F_IDLENESS_STATUS, 0);

	ret = cstat_hw_s_one_shot_enable(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, ret, -EBUSY);
}

static void pablo_hw_api_cstat_hw_corex_trigger_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_corex_trigger(ctx->test_addr);
}

static void pablo_hw_api_cstat_hw_s_reset_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	CSTAT_SET_F_DIRECT(ctx->test_addr, CSTAT_R_IDLENESS_STATUS, CSTAT_F_IDLENESS_STATUS, 1);
	cstat_hw_s_reset(ctx->test_addr);
}

static void pablo_hw_api_cstat_hw_s_post_frame_gap_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_post_frame_gap(ctx->test_addr, 0);
}

static void pablo_hw_api_cstat_hw_s_int_on_col_row_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_int_on_col_row(ctx->test_addr, true, CSTAT_BCROP0, 0, 0);
	cstat_hw_s_int_on_col_row(ctx->test_addr, false, CSTAT_BCROP0, 0, 0);
}

static void pablo_hw_api_cstat_hw_s_freeze_on_col_row_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_freeze_on_col_row(ctx->test_addr, CSTAT_BCROP0, 0, 0);
}

static void pablo_hw_api_cstat_hw_core_init_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_core_init(ctx->test_addr);
}

static void pablo_hw_api_cstat_hw_ctx_init_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_ctx_init(ctx->test_addr);
}

static void pablo_hw_api_cstat_hw_g_version_kunit_test(struct kunit *test)
{
	u32 version;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	version =  cstat_hw_g_version(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, version, 0);
}

static void pablo_hw_api_cstat_hw_s_input_kunit_test(struct kunit *test)
{
	u32 ret;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret =  cstat_hw_s_input(ctx->test_addr, OTF);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret =  cstat_hw_s_input(ctx->test_addr, DMA);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret =  cstat_hw_s_input(ctx->test_addr, VOTF);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret =  cstat_hw_s_input(ctx->test_addr, CSTST_INPUT_PATH_NUM);
	KUNIT_EXPECT_EQ(test, ret, -ERANGE);
}

static void pablo_hw_api_cstat_hw_s_format_kunit_test(struct kunit *test)
{
	u32 ret;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret =  cstat_hw_s_format(ctx->test_addr, 4032, 3024, INPUT_10B);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret =  cstat_hw_s_format(ctx->test_addr, 4032, 3024, CSTAT_INPUT_BITWIDTH_NUM);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_hw_api_cstat_hw_s_sdc_enable_kunit_test(struct kunit *test)
{
	bool enable = false;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_sdc_enable(ctx->test_addr, enable);
}

static void pablo_hw_api_cstat_hw_s_fro_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* enable fro */
	cstat_hw_s_fro(ctx->test_addr, 2, 0x1F);
	/* disable fro */
	cstat_hw_s_fro(ctx->test_addr, 1, 0x1F);
}

static void pablo_hw_api_cstat_hw_s_corex_enable_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* enable corex */
	cstat_hw_s_corex_enable(ctx->test_addr, true);
	/* disable corex */
	CSTAT_SET_F_DIRECT(ctx->test_addr, CSTAT_R_COREX_STATUS_0, CSTAT_F_COREX_BUSY_0, 1);
	cstat_hw_s_corex_enable(ctx->test_addr, false);
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

static void pablo_hw_api_cstat_hw_g_int1_status_kunit_test(struct kunit *test)
{
	u32 ret;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* fro int status */
	ret = cstat_hw_g_int1_status(ctx->test_addr, true, true);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* normal int status */
	ret = cstat_hw_g_int1_status(ctx->test_addr, true, false);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_cstat_hw_g_int2_status_kunit_test(struct kunit *test)
{
	u32 ret;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* fro int status */
	ret = cstat_hw_g_int2_status(ctx->test_addr, true, true);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* normal int status */
	ret = cstat_hw_g_int2_status(ctx->test_addr, true, false);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_api_cstat_hw_wait_isr_clear_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	CSTAT_SET_F_DIRECT(ctx->test_addr, CSTAT_R_CONTINT_SIPUIPP1P0P0_INT1,
		CSTAT_F_CONTINT_SIPUIPP1P0P0_INT1, 1);
	CSTAT_SET_F_DIRECT(ctx->test_addr, CSTAT_R_CONTINT_SIPUIPP1P0P0_INT2,
		CSTAT_F_CONTINT_SIPUIPP1P0P0_INT2, 1);
	cstat_hw_wait_isr_clear(ctx->test_addr, false);
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

	ret = pablo_run_cstat_hw_is_occurred(FRAME_START, CSTAT_FS);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(FRAME_INT_ON_ROW_COL_INFO, CSTAT_LINE);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(FRAME_END, CSTAT_FE);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(RGBYHIST_OUT, CSTAT_RGBY);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(COREX_END_INT_0, CSTAT_COREX_END);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(IRQ_CORRUPTED, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(COREX_ERR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(SDC_ERR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(LIC_OVERFLOW, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(LIC_ROW_ERR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(CLOADER_COREX_OVERLAP, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(CINFIFO_PROT_ERR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(CINFIFO_PCNT_ERR, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(CINFIFO_OVERFLOW, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(CINFIFO_OVERLAP, CSTAT_ERR);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(CDAF_SINGLE_DONE, CSTAT_CDAF);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(THSTAT_PRE_DONE, CSTAT_THSTAT_PRE);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(THSTAT_AWB_DONE, CSTAT_THSTAT_AWB);
	KUNIT_EXPECT_TRUE(test, ret);

	ret = pablo_run_cstat_hw_is_occurred(THSTAT_AE_DONE, CSTAT_THSTAT_AE);
	KUNIT_EXPECT_TRUE(test, ret);
}

static void pablo_hw_api_cstat_hw_print_err_kunit_test(struct kunit *test)
{
	char name[16] = "kunit_test";

	cstat_hw_print_err(name, 0, 1, 0xFFFFFFFF, 0);
}

static void pablo_hw_api_cstat_hw_s_seqid_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_seqid(ctx->test_addr, 0);
}

static void pablo_hw_api_cstat_hw_s_crc_seed_kunit_test(struct kunit *test)
{
	u32 seed = 0;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_crc_seed(ctx->test_addr, seed);
}

/*
 * DMA
 */
static struct is_common_dma dma[CSTAT_DMA_NUM];
static struct cstat_param_set param;
static void pablo_hw_api_cstat_hw_s_cdaf_dma_kunit_test(struct kunit *test)
{
	enum cstat_dma_id dma_id;
	enum is_cstat_subdev_id subdev_id;
	dma_addr_t addr;
	int test_idx, ret;
	u32 set_val, expected_val;
	struct is_cstat_config conf;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	for (test_idx = 0; test_idx < CSTAT_TEST_SET; test_idx++) {
		/* set input */
		dma_id = CSTAT_W_CDAFMW;
		subdev_id = IS_CSTAT_CDAF_MW;
		conf.cdaf_mw_bypass = false;
		conf.cdaf_mw_x = test_config[test_idx].cdaf_mw.w;
		conf.cdaf_mw_y = test_config[test_idx].cdaf_mw.h;
		conf.thstatpre_bypass = true;
		conf.thstatae_bypass = true;
		conf.thstatawb_bypass = true;
		conf.rgbyhist_bypass = true;
		conf.lmeds_bypass = true;
		conf.drcclct_bypass = true;
		addr = 0x19860218;

		/* run: cstat_hw_create_dma */
		ret = cstat_hw_create_dma(ctx->test_addr, dma_id, &dma[dma_id]);

		/* check output */
		KUNIT_EXPECT_EQ(test, ret, 0);

		set_val = dma[dma_id].available_bayer_format_map;
		expected_val = BIT_MASK(DMA_FMT_U8BIT_PACK);
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = dma[dma_id].available_yuv_format_map;
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = dma[dma_id].available_rgb_format_map;
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		/* run: cstat_hw_s_dma_cfg */
		cstat_hw_s_dma_cfg(&param, &conf);

		/* check output */
		set_val = param.cdaf_mw.cmd;
		expected_val = !conf.cdaf_mw_bypass;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = param.cdaf_mw.width;
		expected_val = conf.cdaf_mw_x * 48;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = param.cdaf_mw.height;
		expected_val = conf.cdaf_mw_y;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		/* run: cstat_hw_s_stat_cfg */
		cstat_hw_s_stat_cfg(subdev_id, addr, &param);

		/* check output */
		set_val = param.cdaf_mw.format;
		expected_val = DMA_INOUT_FORMAT_BAYER;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = param.cdaf_mw.order;
		expected_val = DMA_INOUT_ORDER_NO;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = param.cdaf_mw.bitwidth;
		expected_val = DMA_INOUT_BIT_WIDTH_8BIT;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = param.cdaf_mw.msb;
		expected_val = DMA_INOUT_BIT_WIDTH_8BIT - 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = param.cdaf_mw.plane;
		expected_val = DMA_INOUT_PLANE_1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		/* run: cstat_hw_s_wdma_cfg */
		ret = cstat_hw_s_wdma_cfg(ctx->test_addr, &dma[dma_id], &param, 1, 0);

		/* check output */
		KUNIT_EXPECT_EQ(test, ret, 0);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_STAT_WDMACDAF_EN,
					CSTAT_F_STAT_WDMACDAF_EN);
		expected_val = param.cdaf_mw.cmd;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_STAT_WDMACDAF_DATA_FORMAT,
					CSTAT_F_STAT_WDMACDAF_DATA_FORMAT_BAYER);
		expected_val = DMA_FMT_U8BIT_PACK;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_STAT_WDMACDAF_MONO_MODE,
					CSTAT_F_STAT_WDMACDAF_MONO_MODE);
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_STAT_WDMACDAF_WIDTH,
					CSTAT_F_STAT_WDMACDAF_WIDTH);
		expected_val = param.cdaf_mw.width;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_STAT_WDMACDAF_HEIGHT,
					CSTAT_F_STAT_WDMACDAF_HEIGHT);
		expected_val = param.cdaf_mw.height;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_STAT_WDMACDAF_IMG_STRIDE_1P,
					CSTAT_F_STAT_WDMACDAF_IMG_STRIDE_1P);
		expected_val = ALIGN(DIV_ROUND_UP(param.cdaf_mw.width * param.cdaf_mw.bitwidth,
						BITS_PER_BYTE), 16);
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		/* MSB */
		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_STAT_WDMACDAF_IMG_BASE_ADDR_1P_FRO0,
					CSTAT_F_STAT_WDMACDAF_IMG_BASE_ADDR_1P_FRO0);
		/* MSB << 4 | LSB */
		set_val = (set_val << 4) | CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_STAT_WDMACDAF_IMG_BASE_ADDR_1P_FRO0_LSB_4B,
					CSTAT_F_STAT_WDMACDAF_IMG_BASE_ADDR_1P_FRO0_LSB_4B);

		expected_val = param.cdaf_mw_dva;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);
	}
}

static void pablo_hw_api_cstat_hw_s_sat_dma_kunit_test(struct kunit *test)
{
	enum cstat_dma_id dma_id;
	struct param_dma_output *dma_out;
	int test_idx, ret;
	u32 set_val, expected_val;
	ulong set_map_val, expected_map_val;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* set input */
	dma_id = CSTAT_W_SAT;

	/* run: cstat_hw_create_dma */
	ret = cstat_hw_create_dma(ctx->test_addr, dma_id, &dma[dma_id]);

	/* check output */
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_val = dma[dma_id].available_bayer_format_map;
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_map_val = dma[dma_id].available_yuv_format_map;
	expected_map_val = (0
			| BIT_MASK(DMA_FMT_YUV422_2P_UFIRST)
			| BIT_MASK(DMA_FMT_YUV420_2P_UFIRST)
			| BIT_MASK(DMA_FMT_YUV420_2P_VFIRST)
			| BIT_MASK(DMA_FMT_YUV444_1P)
		       ) & IS_YUV_FORMAT_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_map_val = dma[dma_id].available_rgb_format_map;
	expected_map_val = (0
			| BIT_MASK(DMA_FMT_RGB_RGBA8888)
			| BIT_MASK(DMA_FMT_RGB_ABGR8888)
			| BIT_MASK(DMA_FMT_RGB_ARGB8888)
			| BIT_MASK(DMA_FMT_RGB_BGRA8888)
		       ) & IS_RGB_FORMAT_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	for (test_idx = 0; test_idx < CSTAT_TEST_SET; test_idx++) {
		/* set input: 8bit Y only */
		dma_out = &param.dma_output_sat;
		dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		dma_out->width = test_config[test_idx].sat.w;
		dma_out->height = test_config[test_idx].sat.h;
		dma_out->format = DMA_INOUT_FORMAT_Y;
		dma_out->order = DMA_INOUT_ORDER_NO;
		dma_out->bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
		dma_out->msb = dma_out->bitwidth - 1;
		dma_out->plane = DMA_INOUT_PLANE_1;
		param.output_dva_sat[0] = 0x19860218;

		ret = cstat_hw_s_wdma_cfg(ctx->test_addr, &dma[dma_id], &param, 1, 0);

		/* check output */
		KUNIT_EXPECT_EQ(test, ret, 0);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_YUV_WDMASAT_EN, CSTAT_F_YUV_WDMASAT_EN);
		expected_val = dma_out->cmd;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMASAT_DATA_FORMAT,
					CSTAT_F_YUV_WDMASAT_DATA_FORMAT_TYPE);
		expected_val = DMA_FMT_YUV;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMASAT_DATA_FORMAT,
					CSTAT_F_YUV_WDMASAT_DATA_FORMAT_YUV);
		expected_val = DMA_FMT_YUV420_2P_UFIRST;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMASAT_MONO_MODE,
					CSTAT_F_YUV_WDMASAT_MONO_MODE);
		expected_val = 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMASAT_WIDTH,
					CSTAT_F_YUV_WDMASAT_WIDTH);
		expected_val = dma_out->width;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMASAT_HEIGHT,
					CSTAT_F_YUV_WDMASAT_HEIGHT);
		expected_val = dma_out->height;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMASAT_IMG_STRIDE_1P,
					CSTAT_F_YUV_WDMASAT_IMG_STRIDE_1P);
		expected_val = ALIGN(DIV_ROUND_UP(dma_out->width * dma_out->bitwidth, BITS_PER_BYTE), 16);
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		/* MSB */
		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMASAT_IMG_BASE_ADDR_1P_FRO0,
					CSTAT_F_YUV_WDMASAT_IMG_BASE_ADDR_1P_FRO0);
		/* MSB << 4 | LSB */
		set_val = (set_val << 4) | CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMASAT_IMG_BASE_ADDR_1P_FRO0_LSB_4B,
					CSTAT_F_YUV_WDMASAT_IMG_BASE_ADDR_1P_FRO0_LSB_4B);

		expected_val = param.output_dva_sat[0];
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_RGB_RGBTOYUVSAT_BYPASS,
					CSTAT_F_RGB_RGBTOYUVSAT_BYPASS);
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_YUV444TO422SAT_BYPASS,
					CSTAT_F_YUV_YUV444TO422SAT_BYPASS);
		expected_val = 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_YUV422TO420SAT_BYPASS,
					CSTAT_F_YUV_YUV422TO420SAT_BYPASS);
		expected_val = 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);
	}
}

static void pablo_hw_api_cstat_hw_s_cav_dma_kunit_test(struct kunit *test)
{
	enum cstat_dma_id dma_id;
	struct param_dma_output *dma_out;
	int test_idx, ret;
	u32 set_val, expected_val;
	ulong set_map_val, expected_map_val;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* set input */
	dma_id = CSTAT_W_CAV;

	/* run: cstat_hw_create_dma */
	ret = cstat_hw_create_dma(ctx->test_addr, dma_id, &dma[dma_id]);

	/* check output */
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_val = dma[dma_id].available_bayer_format_map;
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_map_val = dma[dma_id].available_yuv_format_map;
	expected_map_val = (0
			| BIT_MASK(DMA_FMT_YUV422_2P_UFIRST)
			| BIT_MASK(DMA_FMT_YUV420_2P_UFIRST)
			| BIT_MASK(DMA_FMT_YUV420_2P_VFIRST)
			| BIT_MASK(DMA_FMT_YUV444_1P)
		       ) & IS_YUV_FORMAT_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_map_val = dma[dma_id].available_rgb_format_map;
	expected_map_val = (0
			| BIT_MASK(DMA_FMT_RGB_RGBA8888)
			| BIT_MASK(DMA_FMT_RGB_ABGR8888)
			| BIT_MASK(DMA_FMT_RGB_ARGB8888)
			| BIT_MASK(DMA_FMT_RGB_BGRA8888)
		       ) & IS_RGB_FORMAT_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	for (test_idx = 0; test_idx < CSTAT_TEST_SET; test_idx++) {
		/* set input: RGB888 1P 8bit */
		dma_out = &param.dma_output_cav;
		dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		dma_out->width = test_config[test_idx].cav.w;
		dma_out->height = test_config[test_idx].cav.h;
		dma_out->format = DMA_INOUT_FORMAT_RGB;
		dma_out->order = DMA_INOUT_ORDER_BGR;
		dma_out->bitwidth = DMA_INOUT_BIT_WIDTH_24BIT;
		dma_out->msb = dma_out->bitwidth - 1;
		dma_out->plane = DMA_INOUT_PLANE_1;
		param.output_dva_cav[0] = 0xdeadbeef;

		ret = cstat_hw_s_wdma_cfg(ctx->test_addr, &dma[dma_id], &param, 1, 0);

		/* check output */
		KUNIT_EXPECT_EQ(test, ret, 0);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_YUV_WDMACAV_EN, CSTAT_F_YUV_WDMACAV_EN);
		expected_val = dma_out->cmd;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMACAV_DATA_FORMAT,
					CSTAT_F_YUV_WDMACAV_DATA_FORMAT_TYPE);
		expected_val = DMA_FMT_YUV;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMACAV_DATA_FORMAT,
					CSTAT_F_YUV_WDMACAV_DATA_FORMAT_YUV);
		expected_val = DMA_FMT_YUV444_1P;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMACAV_MONO_MODE,
					CSTAT_F_YUV_WDMACAV_MONO_MODE);
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMACAV_WIDTH,
					CSTAT_F_YUV_WDMACAV_WIDTH);
		expected_val = dma_out->width;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMACAV_HEIGHT,
					CSTAT_F_YUV_WDMACAV_HEIGHT);
		expected_val = dma_out->height;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMACAV_IMG_STRIDE_1P,
					CSTAT_F_YUV_WDMACAV_IMG_STRIDE_1P);
		expected_val = ALIGN(DIV_ROUND_UP(dma_out->width * dma_out->bitwidth, BITS_PER_BYTE), 16);
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		/* MSB */
		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMACAV_IMG_BASE_ADDR_1P_FRO0,
					CSTAT_F_YUV_WDMACAV_IMG_BASE_ADDR_1P_FRO0);
		/* MSB << 4 | LSB */
		set_val = (set_val << 4) | CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMACAV_IMG_BASE_ADDR_1P_FRO0_LSB_4B,
					CSTAT_F_YUV_WDMACAV_IMG_BASE_ADDR_1P_FRO0_LSB_4B);

		expected_val = param.output_dva_cav[0];
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_RGB_RGBTOYUVCAV_BYPASS,
					CSTAT_F_RGB_RGBTOYUVCAV_BYPASS);
		expected_val = 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_YUV444TO422CAV_BYPASS,
					CSTAT_F_YUV_YUV444TO422CAV_BYPASS);
		expected_val = 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_YUV422TO420CAV_BYPASS,
					CSTAT_F_YUV_YUV422TO420CAV_BYPASS);
		expected_val = 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);
	}
}

static void pablo_hw_api_cstat_hw_s_fdpig_wdma_kunit_test(struct kunit *test)
{
	enum cstat_dma_id dma_id;
	struct param_dma_output *dma_out;
	int test_idx, ret;
	u32 set_val, expected_val;
	ulong set_map_val, expected_map_val;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* set input */
	dma_id = CSTAT_W_FDPIG;

	/* run: cstat_hw_create_dma */
	ret = cstat_hw_create_dma(ctx->test_addr, dma_id, &dma[dma_id]);

	/* check output */
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_val = dma[dma_id].available_bayer_format_map;
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_map_val = dma[dma_id].available_yuv_format_map;
	expected_map_val = (0
			    | BIT_MASK(DMA_FMT_YUV444_1P)
			   ) & IS_YUV_FORMAT_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_map_val = dma[dma_id].available_rgb_format_map;
	expected_map_val = (0
			    | BIT_MASK(DMA_FMT_RGB_RGBA8888)
			    | BIT_MASK(DMA_FMT_RGB_ARGB8888)
			    | BIT_MASK(DMA_FMT_RGB_ABGR8888)
			    | BIT_MASK(DMA_FMT_RGB_BGRA8888)
			   ) & IS_RGB_FORMAT_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	for (test_idx = 0; test_idx < CSTAT_TEST_SET; test_idx++) {
		/* set input: 8bit Y only */
		dma_out = &param.dma_output_fdpig;
		dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		dma_out->width = test_config[test_idx].fdpig.w;
		dma_out->height = test_config[test_idx].fdpig.h;
		dma_out->format = DMA_INOUT_FORMAT_RGB;
		dma_out->order = DMA_INOUT_ORDER_RGBA;
		dma_out->bitwidth = DMA_INOUT_BIT_WIDTH_32BIT;
		dma_out->msb = dma_out->bitwidth - 1;
		dma_out->plane = DMA_INOUT_PLANE_1;
		param.output_dva_fdpig[0] = 0x19860218;

		ret = cstat_hw_s_wdma_cfg(ctx->test_addr, &dma[dma_id], &param, 1, 0);

		/* check output */
		KUNIT_EXPECT_EQ(test, ret, 0);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_YUV_WDMAFDPIG_EN, CSTAT_F_YUV_WDMAFDPIG_EN);
		expected_val = dma_out->cmd;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMAFDPIG_DATA_FORMAT,
					CSTAT_F_YUV_WDMAFDPIG_DATA_FORMAT_TYPE);
		expected_val = DMA_FMT_RGB;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMAFDPIG_DATA_FORMAT,
					CSTAT_F_YUV_WDMAFDPIG_DATA_FORMAT_RGB);
		expected_val = DMA_FMT_RGB_RGBA8888;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMAFDPIG_MONO_MODE,
					CSTAT_F_YUV_WDMAFDPIG_MONO_MODE);
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMAFDPIG_WIDTH,
					CSTAT_F_YUV_WDMAFDPIG_WIDTH);
		expected_val = dma_out->width;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMAFDPIG_HEIGHT,
					CSTAT_F_YUV_WDMAFDPIG_HEIGHT);
		expected_val = dma_out->height;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMAFDPIG_IMG_STRIDE_1P,
					CSTAT_F_YUV_WDMAFDPIG_IMG_STRIDE_1P);
		expected_val = ALIGN(DIV_ROUND_UP(dma_out->width * dma_out->bitwidth, BITS_PER_BYTE), 16);
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		/* MSB */
		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMAFDPIG_IMG_BASE_ADDR_1P_FRO0,
					CSTAT_F_YUV_WDMAFDPIG_IMG_BASE_ADDR_1P_FRO0);
		/* MSB << 4 | LSB */
		set_val = (set_val << 4) | CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_WDMAFDPIG_IMG_BASE_ADDR_1P_FRO0_LSB_4B,
					CSTAT_F_YUV_WDMAFDPIG_IMG_BASE_ADDR_1P_FRO0_LSB_4B);

		expected_val = param.output_dva_fdpig[0];
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_RGB_RGBTOYUVFDPIG_BYPASS,
					CSTAT_F_RGB_RGBTOYUVFDPIG_BYPASS);
		expected_val = 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_YUV444TO422FDPIG_BYPASS,
					CSTAT_F_YUV_YUV444TO422FDPIG_BYPASS);
		expected_val = 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_YUV_YUV422TO420FDPIG_BYPASS,
					CSTAT_F_YUV_YUV422TO420FDPIG_BYPASS);
		expected_val = 1;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);
	}
}

static void pablo_hw_api_cstat_hw_s_byr_rdma_cfg_kunit_test(struct kunit *test)
{
	enum cstat_dma_id dma_id;
	struct param_dma_input *dma_input;
	int test_idx, ret;
	u32 set_val, expected_val;
	ulong set_map_val, expected_map_val;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* set input */
	dma_id = CSTAT_R_BYR;

	/* run: cstat_hw_create_dma */
	ret = cstat_hw_create_dma(ctx->test_addr, dma_id, &dma[dma_id]);

	/* check output */
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_val = dma[dma_id].available_bayer_format_map;
	expected_val = (0
			| BIT_MASK(DMA_FMT_U10BIT_PACK)
			| BIT_MASK(DMA_FMT_U10BIT_UNPACK_LSB_ZERO)
			| BIT_MASK(DMA_FMT_U10BIT_UNPACK_MSB_ZERO)
			| BIT_MASK(DMA_FMT_ANDROID10)
			| BIT_MASK(DMA_FMT_U12BIT_PACK)
			| BIT_MASK(DMA_FMT_U12BIT_UNPACK_LSB_ZERO)
			| BIT_MASK(DMA_FMT_U12BIT_UNPACK_MSB_ZERO)
			| BIT_MASK(DMA_FMT_ANDROID12)
			| BIT_MASK(DMA_FMT_U14BIT_PACK)
			| BIT_MASK(DMA_FMT_U14BIT_UNPACK_LSB_ZERO)
			| BIT_MASK(DMA_FMT_U14BIT_UNPACK_MSB_ZERO)
			) & IS_BAYER_FORMAT_MASK;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_map_val = dma[dma_id].available_yuv_format_map;
	expected_map_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_map_val = dma[dma_id].available_rgb_format_map;
	expected_map_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	for (test_idx = 0; test_idx < CSTAT_TEST_SET; test_idx++) {
		/* set input: 8bit Y only */
		dma_input = &param.dma_input;
		dma_input->cmd = DMA_OUTPUT_COMMAND_ENABLE;
		dma_input->width = test_config[test_idx].input.w;
		dma_input->height = test_config[test_idx].input.h;
		dma_input->format = DMA_INOUT_FORMAT_BAYER;
		dma_input->order = DMA_INOUT_ORDER_NO;
		dma_input->bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
		dma_input->msb = dma_input->bitwidth - 1;
		dma_input->plane = DMA_INOUT_PLANE_1;
		dma_input->sbwc_type = DMA_INPUT_SBWC_DISABLE;
		dma_input->v_otf_enable = false;
		param.input_dva[0] = 0x19860218;

		ret = cstat_hw_s_rdma_cfg(&dma[dma_id], &param, 1);

		/* check output */
		KUNIT_EXPECT_EQ(test, ret, 0);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr, CSTAT_R_BYR_RDMABYRIN_EN, CSTAT_F_BYR_RDMABYRIN_EN);
		expected_val = dma_input->cmd;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_RDMABYRIN_DATA_FORMAT,
					CSTAT_F_BYR_RDMABYRIN_DATA_FORMAT_BAYER);
		expected_val = DMA_FMT_U10BIT_PACK;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_RDMABYRIN_COMP_CONTROL,
					CSTAT_F_BYR_RDMABYRIN_COMP_SBWC_EN);
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_RDMABYRIN_WIDTH,
					CSTAT_F_BYR_RDMABYRIN_WIDTH);
		expected_val = dma_input->width;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_RDMABYRIN_HEIGHT,
					CSTAT_F_BYR_RDMABYRIN_HEIGHT);
		expected_val = dma_input->height;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_RDMABYRIN_IMG_STRIDE_1P,
					CSTAT_F_BYR_RDMABYRIN_IMG_STRIDE_1P);
		expected_val = ALIGN(DIV_ROUND_UP(dma_input->width * dma_input->bitwidth, BITS_PER_BYTE), 16);
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		/* MSB */
		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_RDMABYRIN_IMG_BASE_ADDR_1P_FRO0,
					CSTAT_F_BYR_RDMABYRIN_IMG_BASE_ADDR_1P_FRO0);
		/* MSB << 4 | LSB */
		set_val = (set_val << 4) | CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_RDMABYRIN_IMG_BASE_ADDR_1P_FRO0_LSB_4B,
					CSTAT_F_BYR_RDMABYRIN_IMG_BASE_ADDR_1P_FRO0_LSB_4B);

		expected_val = param.input_dva[0];
		KUNIT_EXPECT_EQ(test, set_val, expected_val);
	}
}

static void pablo_hw_api_cstat_hw_s_ds_cfg_kunit_test(struct kunit *test)
{
	int ret;
	u32 test_idx, dma_id;
	struct cstat_size_cfg size_cfg;
	struct param_dma_output *dma_out;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	for (dma_id = 0; dma_id < CSTAT_DMA_NUM; dma_id++) {
		for (test_idx = 0; test_idx < CSTAT_TEST_SET; test_idx++) {
			size_cfg.bns.w = test_config[test_idx].bns.w;
			size_cfg.bns.h = test_config[test_idx].bns.h;

			switch (dma_id) {
			case CSTAT_W_LMEDS0:
				dma_out = &param.dma_output_lme_ds0;
				break;
			case CSTAT_W_LMEDS1:
				dma_out = &param.dma_output_lme_ds1;
				break;
			case CSTAT_W_FDPIG:
				dma_out = &param.dma_output_fdpig;
				dma_out->dma_crop_width = test_config[test_idx].bns.w;
				dma_out->dma_crop_height = test_config[test_idx].bns.h;
				dma_out->width = test_config[test_idx].fdpig.w;
				dma_out->height = test_config[test_idx].fdpig.h;
				break;
			case CSTAT_W_SAT:
				dma_out = &param.dma_output_sat;
				dma_out->dma_crop_width = test_config[test_idx].bns.w;
				dma_out->dma_crop_height = test_config[test_idx].bns.h;
				dma_out->width = test_config[test_idx].sat.w;
				dma_out->height = test_config[test_idx].sat.h;
				break;
			case CSTAT_W_CAV:
				dma_out = &param.dma_output_cav;
				dma_out->dma_crop_width = test_config[test_idx].bns.w;
				dma_out->dma_crop_height = test_config[test_idx].bns.h;
				dma_out->width = test_config[test_idx].cav.w;
				dma_out->height = test_config[test_idx].cav.h;
				break;
			default:
				continue;
			}

			dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
			ret = cstat_hw_s_ds_cfg(ctx->test_addr, dma_id, &size_cfg, &param);
			KUNIT_EXPECT_EQ(test, ret, 0);

			dma_out->cmd = DMA_OUTPUT_COMMAND_DISABLE;
			cstat_hw_s_ds_cfg(ctx->test_addr, dma_id, &size_cfg, &param);
			KUNIT_EXPECT_EQ(test, ret, 0);
		}
	}

	/* incrop error */
	size_cfg.bns.w = test_config[0].bns.w;
	size_cfg.bns.h = test_config[0].bns.h;

	dma_out = &param.dma_output_sat;
	dma_out->dma_crop_width = size_cfg.bns.w + 1;
	dma_out->dma_crop_height = size_cfg.bns.h + 1;

	dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	ret = cstat_hw_s_ds_cfg(ctx->test_addr, CSTAT_W_SAT, &size_cfg, &param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* outcrop error */
	size_cfg.bns.w = test_config[0].bns.w;
	size_cfg.bns.h = test_config[0].bns.h;

	dma_out = &param.dma_output_sat;
	dma_out->dma_crop_width = size_cfg.bns.w;
	dma_out->dma_crop_height = size_cfg.bns.h;
	dma_out->width = size_cfg.bns.w + 1;
	dma_out->height = size_cfg.bns.w + 1;

	dma_out->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	ret = cstat_hw_s_ds_cfg(ctx->test_addr, CSTAT_W_SAT, &size_cfg, &param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

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
		expected_val = 0;
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

static void pablo_hw_api_cstat_hw_s_sram_offset_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_sram_offset(ctx->test_addr);
}

static void pablo_hw_api_cstat_hw_s_crop_kunit_test(struct kunit *test)
{
	u32 crop_id;
	struct is_crop crop = { 0, };
	struct cstat_size_cfg size_cfg = { 0, };
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	for (crop_id = 0; crop_id <= CSTAT_CROP_NUM; crop_id++)
		cstat_hw_s_crop(ctx->test_addr, crop_id, true, &crop, &param, &size_cfg);
}

static void pablo_hw_api_cstat_hw_is_bcrop_kunit_test(struct kunit *test)
{
	bool ret;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret = cstat_hw_is_bcrop(CSTAT_CROP_DZOOM);
	KUNIT_EXPECT_EQ(test, ret, true);
}

static void pablo_hw_api_cstat_hw_s_grid_cfg_kunit_test(struct kunit *test)
{
	u32 grid_id;
	struct cstat_grid_cfg cfg = { 0, };
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	for (grid_id = 0; grid_id <= CSTAT_GRID_LSC; grid_id++)
		cstat_hw_s_grid_cfg(ctx->test_addr, grid_id, &cfg);
}

static void pablo_hw_api_cstat_hw_g_bns_size_kunit_test(struct kunit *test)
{
	u32 test_idx;
	struct is_crop in, out;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	for (test_idx = 0; test_idx < CSTAT_TEST_SET; test_idx++) {
		in.w = test_config[test_idx].crop_dzoom.w;
		in.h = test_config[test_idx].crop_dzoom.h;
		cstat_hw_g_bns_size(&in, &out);
		KUNIT_EXPECT_EQ(test, out.w, test_config[test_idx].bns.w);
		KUNIT_EXPECT_EQ(test, out.h, test_config[test_idx].bns.h);
	}
}

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
		in_width = test_config[test_idx].crop_dzoom.w;
		in_height = test_config[test_idx].crop_dzoom.h;
		scale_ratio = CSTAT_BNS_X2P0;
		out_width = test_config[test_idx].bns.w;
		out_height = test_config[test_idx].bns.h;

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
		expected_val = 3;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_CONFIG,
					CSTAT_F_BYR_BNS_FACTORY);
		expected_val = 3;
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
					CSTAT_R_BYR_BNS_WEIGHT_X_0,
					CSTAT_F_BYR_BNS_WEIGHT_X_0_0);
		expected_val = bns_weights[0];
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_WEIGHT_X_0,
					CSTAT_F_BYR_BNS_WEIGHT_X_0_1);
		expected_val = bns_weights[1];
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_WEIGHT_X_0,
					CSTAT_F_BYR_BNS_WEIGHT_X_0_2);
		expected_val = bns_weights[2];
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_WEIGHT_X_0,
					CSTAT_F_BYR_BNS_WEIGHT_X_0_3);
		expected_val = bns_weights[3];
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_WEIGHT_X_4,
					CSTAT_F_BYR_BNS_WEIGHT_X_0_4);
		expected_val = bns_weights[4];
		KUNIT_EXPECT_EQ(test, set_val, expected_val);

		set_val = CSTAT_GET_F_COREX(ctx->test_addr,
					CSTAT_R_BYR_BNS_BYPASS,
					CSTAT_F_BYR_BNS_BYPASS);
		expected_val = 0;
		KUNIT_EXPECT_EQ(test, set_val, expected_val);
	}

}

static void pablo_hw_api_cstat_hw_g_stat_size_kunit_test(struct kunit *test)
{
	int ret;
	u32 sd_id;
	struct cstat_stat_buf_info info;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* normal test */
	for (sd_id = 0; sd_id < IS_CSTAT_SUBDEV_NUM; sd_id++) {
		ret = cstat_hw_g_stat_size(sd_id, &info);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}
	/* error test */
	ret = cstat_hw_g_stat_size(sd_id, &info);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_hw_api_cstat_hw_g_stat_data_kunit_test(struct kunit *test)
{
	int ret;
	u32 edge_score;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;
	struct cstat_stat_buf_info info;
	struct cstat_cdaf cdaf;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* alloc cdaf buffer */
	ret = cstat_hw_g_stat_size(IS_CSTAT_CDAF, &info);
	KUNIT_EXPECT_EQ(test, ret, 0);
	cdaf.kva = (ulong)kunit_kzalloc(test, (info.w * info.h * info.bpp / 8), 0);

	/* cdaf stat test */
	ret = cstat_hw_g_stat_data(ctx->test_addr, CSTAT_STAT_CDAF, (void *)&cdaf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* free cdaf buffer */
	kunit_kfree(test, (void *)cdaf.kva);

	/* edge score test */
	ret = cstat_hw_g_stat_data(ctx->test_addr, CSTAT_STAT_EDGE_SCORE, (void *)&edge_score);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* error test */
	ret = cstat_hw_g_stat_data(ctx->test_addr, CSTAT_STAT_ID_NUM, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_hw_api_cstat_hw_s_default_blk_cfg_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_default_blk_cfg(ctx->test_addr);

	/* FDPIG */
	set_val = CSTAT_GET_R_COREX(ctx->test_addr, CSTAT_R_RGB_RGBTOYUVFDPIG_COEFF_R1);
	expected_val = 306;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = CSTAT_GET_R_COREX(ctx->test_addr, CSTAT_R_YUV_YUV444TO422FDPIG_MCOEFFS_0);
	expected_val = 0x20402000;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* SAT */
	set_val = CSTAT_GET_R_COREX(ctx->test_addr, CSTAT_R_RGB_RGBTOYUVSAT_COEFF_R1);
	expected_val = 306;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = CSTAT_GET_R_COREX(ctx->test_addr, CSTAT_R_YUV_YUV444TO422SAT_MCOEFFS_0);
	expected_val = 0x20402000;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	/* CAV */
	set_val = CSTAT_GET_R_COREX(ctx->test_addr, CSTAT_R_RGB_RGBTOYUVCAV_COEFF_R1);
	expected_val = 306;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	set_val = CSTAT_GET_R_COREX(ctx->test_addr, CSTAT_R_YUV_YUV444TO422CAV_MCOEFFS_0);
	expected_val = 0x20402000;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static void pablo_hw_api_cstat_hw_s_pixel_order_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_pixel_order(ctx->test_addr, 0);
}

static void pablo_hw_api_cstat_hw_s_mono_mode_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_s_mono_mode(ctx->test_addr, 0);
}

static void pablo_hw_api_cstat_hw_dump_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_dump(ctx->test_addr);
}

static void pablo_hw_api_cstat_hw_print_stall_status_kunit_test(struct kunit *test)
{
	int ch = 0;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cstat_hw_print_stall_status(ctx->test_addr, ch);
}

static void pablo_hw_api_cstat_hw_g_reg_cnt_kunit_test(struct kunit *test)
{
	u32 cnt;
	struct pablo_hw_api_cstat_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	cnt = cstat_hw_g_reg_cnt();
	KUNIT_ASSERT_NE(test, cnt, 0);
}

static struct kunit_case pablo_hw_api_cstat_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_cstat_hw_corex_resume_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_corex_delay_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_lic_hblank_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_is_corex_offset_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_global_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_one_shot_enabl_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_corex_trigger_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_post_frame_gap_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_int_on_col_row_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_freeze_on_col_row_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_core_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_ctx_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_g_version_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_input_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_sdc_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_fro_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_corex_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_int_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_g_int1_status_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_g_int2_status_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_wait_isr_clear_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_is_occurred_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_print_err_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_seqid_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_crc_seed_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_cdaf_dma_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_sat_dma_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_cav_dma_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_fdpig_wdma_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_byr_rdma_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_ds_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_simple_lic_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_sram_offset_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_crop_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_is_bcrop_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_grid_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_g_bns_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_bns_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_g_stat_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_g_stat_data_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_default_blk_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_pixel_order_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_s_mono_mode_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_print_stall_status_kunit_test),
	KUNIT_CASE(pablo_hw_api_cstat_hw_g_reg_cnt_kunit_test),
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
