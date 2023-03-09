// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "hardware/api/is-hw-api-pdp-v1.h"
#if IS_ENABLED(CONFIG_PDP_V4_0)
#include "hardware/sfr/is-sfr-pdp-v4_0.h"
#elif IS_ENABLED(CONFIG_PDP_V4_1) || IS_ENABLED(CONFIG_PDP_V4_4)
#include "hardware/sfr/is-sfr-pdp-v4_1.h"
#else
#include "hardware/sfr/is-sfr-pdp-v5_0.h"
#endif
#include "is-config.h"

#define PDP_SET_F(base, R, F, val) \
	is_hw_set_field(base, &pdp_regs_corex[R], &pdp_fields[F], val)
#define PDP_SET_F_DIRECT(base, R, F, val) \
	is_hw_set_field(base, &pdp_regs[R], &pdp_fields[F], val)
#define PDP_SET_R(base, R, val) \
	is_hw_set_reg(base, &pdp_regs_corex[R], val)
#define PDP_SET_R_DIRECT(base, R, val) \
	is_hw_set_reg(base, &pdp_regs[R], val)
#define PDP_SET_V(reg_val, F, val) \
	is_hw_set_field_value(reg_val, &pdp_fields[F], val)

#define PDP_GET_F(base, R, F) \
	is_hw_get_field(base, &pdp_regs[R], &pdp_fields[F])
#define PDP_GET_R(base, R) \
	is_hw_get_reg(base, &pdp_regs[R])
#define PDP_GET_F_COREX(base, R, F) \
		is_hw_get_field(base, &pdp_regs_corex[R], &pdp_fields[F])
#define PDP_GET_R_COREX(base, R) \
	is_hw_get_reg(base, &pdp_regs_corex[R])
#define PDP_GET_V(reg_val, F) \
	is_hw_get_field_value(reg_val, &pdp_fields[F])

/* Define the test cases. */

struct pablo_hw_api_pdp_test_ctx {
	void *test_addr;
} static pablo_hw_api_pdp_test_ctx;

static void pablo_hw_api_pdp_hw_g_ip_version_kunit_test(struct kunit *test)
{
	u32 version = 0x05000000;
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_IP_VERSION, version);
	val = pdp_hw_g_ip_version(ctx->test_addr);

	KUNIT_EXPECT_EQ(test, val, version);
}

static void pablo_hw_api_pdp_hw_g_idle_state_kunit_test(struct kunit *test)
{
	unsigned int val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	PDP_SET_F_DIRECT(ctx->test_addr, PDP_R_IDLENESS_STATUS,
		PDP_F_IDLENESS_STATUS, 1);

	val = pdp_hw_g_idle_state(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)1);
}

static void pablo_hw_api_pdp_hw_get_line_kunit_test(struct kunit *test)
{
	u32 total_line;
	u32 curr_line;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_get_line(ctx->test_addr, &total_line, &curr_line);

	KUNIT_EXPECT_EQ(test, total_line, (u32)0);
	KUNIT_EXPECT_EQ(test, curr_line, (u32)0);
}

static void pablo_check_corex_enable(struct kunit *test, bool enable)
{
	u32 type_addr, type_num, val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	if (!enable) {
		val = PDP_GET_F(ctx->test_addr, PDP_R_COREX_UPDATE_MODE_0,
			PDP_F_COREX_UPDATE_MODE_0);
		KUNIT_EXPECT_EQ(test, val, (u32)1);
		val = PDP_GET_F(ctx->test_addr, PDP_R_COREX_UPDATE_MODE_1,
			PDP_F_COREX_UPDATE_MODE_1);
		KUNIT_EXPECT_EQ(test, val, (u32)1);
		val = PDP_GET_F(ctx->test_addr, PDP_R_COREX_ENABLE, PDP_F_COREX_ENABLE);
		KUNIT_EXPECT_EQ(test, val, (u32)0);
	} else {
		val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_COREX_ENABLE, PDP_F_COREX_ENABLE);
		KUNIT_EXPECT_EQ(test, val, (u32)1);
		val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_COREX_RESET, PDP_F_COREX_RESET);
		KUNIT_EXPECT_EQ(test, val, (u32)1);
		val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_COREX_TYPE_WRITE_TRIGGER,
			PDP_F_COREX_TYPE_WRITE_TRIGGER);
		KUNIT_EXPECT_EQ(test, val, (u32)1);

		type_num = DIV_ROUND_UP(pdp_regs[(PDP_REG_CNT - 1)].sfr_offset, (4 * 32));
		for (type_addr = 0; type_addr < type_num; type_addr++) {
			val = PDP_GET_F_COREX(ctx->test_addr,
				PDP_R_COREX_TYPE_WRITE, PDP_F_COREX_TYPE_WRITE);
			KUNIT_EXPECT_EQ(test, val, (u32)0);
		}
		val = PDP_GET_F_COREX(ctx->test_addr,
			PDP_R_COREX_UPDATE_TYPE_0, PDP_F_COREX_UPDATE_TYPE_0);
		KUNIT_EXPECT_EQ(test, val, (u32)1);
		val = PDP_GET_F_COREX(ctx->test_addr,
			PDP_R_COREX_UPDATE_TYPE_1, PDP_F_COREX_UPDATE_TYPE_1);
		KUNIT_EXPECT_EQ(test, val, (u32)0);
		val = PDP_GET_F_COREX(ctx->test_addr,
			PDP_R_COREX_UPDATE_MODE_0, PDP_F_COREX_UPDATE_MODE_0);
		KUNIT_EXPECT_EQ(test, val, (u32)0);
		val = PDP_GET_F_COREX(ctx->test_addr,
			PDP_R_COREX_UPDATE_MODE_1, PDP_F_COREX_UPDATE_MODE_1);
		KUNIT_EXPECT_EQ(test, val, (u32)1);
		val = PDP_GET_F_COREX(ctx->test_addr,
			PDP_R_COREX_COPY_FROM_IP_0, PDP_F_COREX_COPY_FROM_IP_0);
		KUNIT_EXPECT_EQ(test, val, (u32)1);
		val = PDP_GET_F_COREX(ctx->test_addr,
			PDP_R_COREX_START_0, PDP_F_COREX_START_0);
		KUNIT_EXPECT_EQ(test, val, (u32)1);
	}
}

static void pablo_hw_api_pdp_hw_s_global_enable_kunit_test(struct kunit *test)
{
	bool enable = true;
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_global_enable(ctx->test_addr, enable);
	pablo_check_corex_enable(test, enable);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_GLOBAL_ENABLE_STOP_CRPT,
			PDP_F_GLOBAL_ENABLE_STOP_CRPT);
	KUNIT_EXPECT_EQ(test, val, (u32)enable);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_FRO_GLOBAL_ENABLE,
			PDP_F_FRO_GLOBAL_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)enable);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_GLOBAL_ENABLE,
			PDP_F_GLOBAL_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)enable);
}

static void pablo_hw_api_pdp_hw_s_global_enable_clear_kunit_test(struct kunit *test)
{
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_global_enable_clear(ctx->test_addr);
	val = PDP_GET_F(ctx->test_addr, PDP_R_GLOBAL_ENABLE, PDP_F_GLOBAL_ENABLE_CLEAR);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void pablo_hw_api_pdp_hw_s_one_shot_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct is_pdp *pdp = kunit_kzalloc(test, sizeof(struct is_pdp), 0);
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;
	u32 val;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdp);

	pdp->base = ctx->test_addr;

	PDP_SET_R_DIRECT(pdp->base, PDP_R_RDMA_AF_EN, 1);

	ret = pdp_hw_s_one_shot_enable(pdp);
	KUNIT_EXPECT_EQ(test, ret, 0);

	val = PDP_GET_F(pdp->base, PDP_R_FRO_GLOBAL_ENABLE, PDP_F_FRO_GLOBAL_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = PDP_GET_F(pdp->base, PDP_R_GLOBAL_ENABLE, PDP_F_GLOBAL_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = PDP_GET_F(pdp->base, PDP_R_FRO_ONE_SHOT_ENABLE, PDP_F_FRO_ONE_SHOT_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = PDP_GET_F(pdp->base, PDP_R_ONE_SHOT_ENABLE, PDP_F_ONE_SHOT_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = PDP_GET_R(pdp->base, PDP_R_RDMA_AF_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	if (pdp)
		kunit_kfree(test, pdp);
}

static void pablo_hw_api_pdp_hw_s_corex_enable_kunit_test(struct kunit *test)
{
	bool enable;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	enable = false;
	pdp_hw_s_corex_enable(ctx->test_addr, enable);
	pablo_check_corex_enable(test, enable);

	enable = true;
	pdp_hw_s_corex_enable(ctx->test_addr, enable);
	pablo_check_corex_enable(test, enable);
}

static void pablo_hw_api_pdp_hw_s_corex_type_kunit_test(struct kunit *test)
{
	u32 type = COREX_IGNORE;
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_corex_type(ctx->test_addr, type);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_COREX_UPDATE_TYPE_0, PDP_F_COREX_UPDATE_TYPE_0);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
}

static void pablo_hw_api_pdp_hw_g_corex_state_kunit_test(struct kunit *test)
{
	u32 corex_enable;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	PDP_SET_F_DIRECT(ctx->test_addr, PDP_R_COREX_ENABLE, PDP_F_COREX_ENABLE, 1);

	pdp_hw_g_corex_state(ctx->test_addr, &corex_enable);

	KUNIT_EXPECT_EQ(test, corex_enable, (u32)1);
}

static void pablo_pdp_check_pd_size(struct kunit *test, u32 width, u32 height,
	u32 hwformat, u32 potf_en)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;
	u32 format;
	u32 byte_per_line;
	u32 unpack_fmt;
	u32 val;

	switch (hwformat) {
	case HW_FORMAT_RAW10:
		format = PDP_DMA_FMT_U10BIT_UNPACK_MSB_ZERO;
		unpack_fmt = PDP_REORDER_10BIT;
		break;
	case HW_FORMAT_RAW12:
		format = PDP_DMA_FMT_U12BIT_UNPACK_MSB_ZERO;
		unpack_fmt = PDP_REORDER_12BIT;
		break;
	case HW_FORMAT_RAW14:
		format = PDP_DMA_FMT_U14BIT_UNPACK_MSB_ZERO;
		unpack_fmt = PDP_REORDER_14BIT;
		break;
	default:
		return;
	}
	byte_per_line = ALIGN(width * 16 / BITS_PER_BYTE, 16);

	if (!potf_en)
		unpack_fmt = PDP_REORDER_BYPASS;

	/* AF RDMA */
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_RDMA_AF_DATA_FORMAT,
			PDP_F_RDMA_AF_DATA_FORMAT_AF);
	KUNIT_EXPECT_EQ(test, val, format);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_RDMA_AF_WIDTH,
			PDP_F_RDMA_AF_WIDTH);
	KUNIT_EXPECT_EQ(test, val, width);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_RDMA_AF_HEIGHT,
			PDP_F_RDMA_AF_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, height);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_RDMA_AF_IMG_STRIDE_1P,
			PDP_F_RDMA_AF_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, byte_per_line);

	/* Y Reorder */
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_Y_REORDER_UNPACK,
			PDP_F_Y_REORDER_UNPACK_FORMAT);
	KUNIT_EXPECT_EQ(test, val, unpack_fmt);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_Y_REORDER_LINEBUF_SIZE,
			PDP_F_Y_REORDER_LINEBUF_SIZE); /* 0 fix */
	KUNIT_EXPECT_EQ(test, val, (u32)0);
}
static void pablo_pdp_check_reorder_cfg(struct kunit *test, bool enable)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;
	u32 val1, val2;
	u32 ppc_val;

	if (CSIS_PIXEL_PER_CLK == CSIS_PPC_4)
		ppc_val = 1;
	else
		ppc_val = 0;

	val1 = PDP_GET_R_COREX(ctx->test_addr, PDP_R_Y_REORDER_MODE);
	val2 = enable | enable << 1 | ppc_val << 2;
	KUNIT_EXPECT_EQ(test, val1, val2);
}

static void pablo_pdp_check_common(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;
	u32 val;

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_INTERRUPT_AUTO_MASK,
			PDP_F_INTERRUPT_AUTO_MASK);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_IP_USE_INPUT_FRAME_START_IN,
			PDP_F_IP_USE_INPUT_FRAME_START_IN);
	KUNIT_EXPECT_EQ(test, val, (u32)START_VVALID_RISE);
}

static void pablo_pdp_check_int_mask(struct kunit *test, u32 path, u32 sensor_type)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;
	u32 val;
	u32 corrupted_int_mask, int2_mask;

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_CONTINT_LEVEL_PULSE_N_SEL,
			PDP_F_CONTINT_LEVEL_PULSE_N_SEL);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_IP_USE_END_INTERRUPT_ENABLE,
			PDP_F_IP_USE_END_INTERRUPT_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_IP_END_INTERRUPT_ENABLE,
			PDP_F_IP_END_INTERRUPT_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	if (path == DMA && sensor_type == SENSOR_TYPE_MOD3) {
		corrupted_int_mask = 0x3F9F;
		int2_mask = INT2_EN_MASK &
			~((1 << CINFIFO_LINES_ERROR) | (1 << CINFIFO_COLUMNS_ERROR));
	} else {
		corrupted_int_mask = 0x3FFF;
		int2_mask = INT2_EN_MASK;
	}

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_IP_CORRUPTED_INTERRUPT_ENABLE,
			PDP_F_IP_CORRUPTED_INTERRUPT_ENABLE);
	KUNIT_EXPECT_EQ(test, val, corrupted_int_mask);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_CONTINT_INT1_ENABLE,
			PDP_F_CONTINT_INT1_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)INT1_EN_MASK);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_CONTINT_INT2_ENABLE,
			PDP_F_CONTINT_INT2_ENABLE);
	KUNIT_EXPECT_EQ(test, val, int2_mask);
}

static void pablo_pdp_check_secure_id(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;
	u32 val;

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_SECU_CTRL_SEQID,
			PDP_F_SECU_CTRL_SEQID);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
}

static void pablo_pdp_check_af_rdma(struct kunit *test,
	u32 path, u32 sensor_type, u32 en_votf)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;
	u32 val, en_afdma;
	u32 rmo = 3;

	if (path == DMA) {
		if (sensor_type == SENSOR_TYPE_MOD3 ||
				sensor_type == SENSOR_TYPE_MSPD_TAIL)
			en_afdma = 1;
		else
			en_afdma = 0;
	} else {
		en_afdma = 0;
	}

	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_RDMA_AF_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)en_afdma);
	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_RDMA_AF_MAX_MO);
	KUNIT_EXPECT_EQ(test, val, rmo);
	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_RDMA_AF_VOTF_EN);
	KUNIT_EXPECT_EQ(test, val, en_votf);
}


static void pablo_hw_api_pdp_hw_s_core_kunit_test(struct kunit *test)
{
	struct is_pdp *pdp = kunit_kzalloc(test, sizeof(struct is_pdp), 0);
	struct is_sensor_cfg *cfg = kunit_kzalloc(test, sizeof(struct is_sensor_cfg), 0);
	struct is_crop img_full_size = { 0 };
	struct is_crop img_crop_size = { 0 };
	struct is_crop img_comp_size = { 0 };
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;
	u32 pd_width, pd_height, pd_hwformat, img_hwformat;
	u32 sensor_type, path, en_votf, num_buffers, potf_en;
	bool enable;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdp);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cfg);

	pdp->base = ctx->test_addr;
	pd_hwformat = img_hwformat = HW_FORMAT_RAW10;
	pd_width = 320;
	pd_height = 240;
	sensor_type = SENSOR_TYPE_MOD3;
	path = DMA;
	en_votf = 1;
	num_buffers = 8;
	enable = true;
	pdp->af_vc = 2;
	cfg->input[pdp->af_vc].extformat = (1 << POTF_EN_BIT);
	potf_en = CHECK_POTF_EN(cfg->input[pdp->af_vc].extformat);

	PDP_SET_R(ctx->test_addr, PDP_R_RDMA_AF_VOTF_EN, 0);
	pdp_hw_s_core(pdp, enable, cfg, img_full_size, img_crop_size, img_comp_size,
		img_hwformat, 0,
		pd_width, pd_height, pd_hwformat, sensor_type, path, 0,
		0, 0, en_votf, num_buffers, 0, 0);
	pablo_pdp_check_pd_size(test, pd_width, pd_height, pd_hwformat, potf_en);
	pablo_pdp_check_reorder_cfg(test, enable);
	pablo_pdp_check_common(test);
	pablo_pdp_check_int_mask(test, path, sensor_type);
	pablo_pdp_check_secure_id(test);
	pablo_pdp_check_af_rdma(test, path, sensor_type, en_votf);

	pd_hwformat = HW_FORMAT_RAW12;
	sensor_type = SENSOR_TYPE_MSPD_TAIL;
	path = OTF;
	en_votf = false;

	PDP_SET_R(ctx->test_addr, PDP_R_RDMA_AF_VOTF_EN, 0);
	pdp_hw_s_core(pdp, enable, cfg, img_full_size, img_crop_size, img_comp_size,
		img_hwformat, 0,
		pd_width, pd_height, pd_hwformat, sensor_type, path, 0,
		0, 0, en_votf, num_buffers, 0, 0);
	pablo_pdp_check_pd_size(test, pd_width, pd_height, pd_hwformat, potf_en);
	pablo_pdp_check_reorder_cfg(test, enable);
	pablo_pdp_check_common(test);
	pablo_pdp_check_int_mask(test, path, sensor_type);
	pablo_pdp_check_secure_id(test);
	pablo_pdp_check_af_rdma(test, path, sensor_type, en_votf);

	pd_hwformat = HW_FORMAT_RAW14;

	pdp_hw_s_core(pdp, enable, cfg, img_full_size, img_crop_size, img_comp_size,
		img_hwformat, 0,
		pd_width, pd_height, pd_hwformat, sensor_type, path, 0,
		0, 0, en_votf, num_buffers, 0, 0);
	pablo_pdp_check_pd_size(test, pd_width, pd_height, pd_hwformat, potf_en);
	pablo_pdp_check_reorder_cfg(test, enable);
	pablo_pdp_check_common(test);
	pablo_pdp_check_int_mask(test, path, sensor_type);
	pablo_pdp_check_secure_id(test);
	pablo_pdp_check_af_rdma(test, path, sensor_type, en_votf);

	cfg->input[pdp->af_vc].extformat = (0 << POTF_EN_BIT);
	potf_en = CHECK_POTF_EN(cfg->input[pdp->af_vc].extformat);
	pdp_hw_s_core(pdp, enable, cfg, img_full_size, img_crop_size, img_comp_size,
		img_hwformat, 0,
		pd_width, pd_height, pd_hwformat, sensor_type, path, 0,
		0, 0, en_votf, num_buffers, 0, 0);
	pablo_pdp_check_pd_size(test, pd_width, pd_height, pd_hwformat, potf_en);
	pablo_pdp_check_reorder_cfg(test, enable);
	pablo_pdp_check_common(test);
	pablo_pdp_check_int_mask(test, path, sensor_type);
	pablo_pdp_check_secure_id(test);
	pablo_pdp_check_af_rdma(test, path, sensor_type, en_votf);

}

static void pablo_hw_api_pdp_hw_s_init_kunit_test(struct kunit *test)
{
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_init(ctx->test_addr);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_AUTO_MASK_PREADY,
			PDP_F_AUTO_MASK_PREADY);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_IP_PROCESSING,
			PDP_F_IP_PROCESSING);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void pablo_hw_api_pdp_hw_s_reset_kunit_test(struct kunit *test)
{
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_reset(ctx->test_addr);

	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_FRO_SW_RESET);
	KUNIT_EXPECT_EQ(test, val, (u32)2);
	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_SW_RESET);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void pablo_hw_api_pdp_hw_s_path_kunit_test(struct kunit *test)
{
	u32 val, path = 1;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_path(ctx->test_addr, path);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_IP_CHAIN_INPUT_SELECT,
			PDP_F_IP_CHAIN_INPUT_SELECT);
	KUNIT_EXPECT_EQ(test, val, path);
}

static void pablo_hw_api_pdp_hw_s_wdma_init_kunit_test(struct kunit *test)
{
	u32 val;
	u32 mroi_no_x, mroi_no_y, mroi_on;
	u32 roi_w, roi_h;
	u32 total_size, height, stride;
	u32 width = PDP_STAT_DMA_WIDTH;
	u32 stat0_size = PDP_STAT_DMA_WIDTH * PDP_STAT0_ROI_NUM;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	mroi_on = 0;
	mroi_no_x = 320;
	mroi_no_y = 240;
	roi_w = 640;
	roi_h = 480;

	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_Y_PDSTAT_ROI_MAIN_MWS_ON, mroi_on);
	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_Y_PDSTAT_ROI_MAIN_MWS_NO_X, mroi_no_x);
	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_Y_PDSTAT_ROI_MAIN_MWS_NO_Y, mroi_no_y);
	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_Y_PDSTAT_IN_SIZE_X, roi_w);
	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_Y_PDSTAT_IN_SIZE_Y, roi_h);

	pdp_hw_s_wdma_init(ctx->test_addr, 0);

	total_size = stat0_size + (mroi_on * (mroi_no_x * mroi_no_y * PDP_STAT_DMA_WIDTH + 4));
	height = (total_size + (width - 1)) / width;
	stride = PDP_STAT_STRIDE(width);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_WDMA_STAT_AUTO_FLUSH_EN,
		PDP_F_WDMA_STAT_AUTO_FLUSH_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_WDMA_STAT_DATA_FORMAT,
		PDP_F_WDMA_STAT_DATA_FORMAT_BAYER);
	KUNIT_EXPECT_EQ(test, val, (u32)PDP_STAT_FORMAT);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_WDMA_STAT_WIDTH,
		PDP_F_WDMA_STAT_WIDTH);
	KUNIT_EXPECT_EQ(test, val, width);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_WDMA_STAT_HEIGHT,
		PDP_F_WDMA_STAT_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, height);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_WDMA_STAT_IMG_STRIDE_1P,
		PDP_F_WDMA_STAT_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, stride);

}

static dma_addr_t pablo_pdp_hw_g_wdma_addr(void __iomem *base)
{
	dma_addr_t ret;

	ret = (dma_addr_t)PDP_GET_R_COREX(base, PDP_R_WDMA_STAT_IMG_BASE_ADDR_1P_FRO0);
#if IS_ENABLED(CONFIG_PDP_V5_0)
	ret = (dma_addr_t)(ret << LSB_BIT) |
		(dma_addr_t)(PDP_GET_R_COREX(base,
			PDP_R_WDMA_STAT_IMG_BASE_ADDR_1P_FRO0_LSB_4B) & LSB_4BITS_MASK);
#endif
	return ret;
}

static void pablo_pdp_s_wdma_addr(void __iomem *base, dma_addr_t address)
{
	PDP_SET_F_DIRECT(base, PDP_R_WDMA_STAT_IMG_BASE_ADDR_1P_FRO0,
		PDP_F_WDMA_STAT_IMG_BASE_ADDR_1P_FRO0,
		(u32)DVA_GET_MSB(address));
#if IS_ENABLED(CONFIG_PDP_V5_0)
	PDP_SET_F_DIRECT(base, PDP_R_WDMA_STAT_IMG_BASE_ADDR_1P_FRO0_LSB_4B,
		PDP_F_WDMA_STAT_IMG_BASE_ADDR_1P_FRO0_LSB_4B,
		(u32)DVA_GET_LSB(address));
#endif
}

static void pablo_hw_api_pdp_hw_wdma_onoff_kunit_test(struct kunit *test)
{
	dma_addr_t addr = 0xDEADDEAD;
	dma_addr_t ret_addr;
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_wdma_enable(ctx->test_addr, addr);
	ret_addr = pablo_pdp_hw_g_wdma_addr(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, ret_addr, addr);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_WDMA_STAT_EN, PDP_F_WDMA_STAT_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	pablo_pdp_s_wdma_addr(ctx->test_addr, addr);
	ret_addr = pdp_hw_g_wdma_addr(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, ret_addr, addr);

	pdp_hw_s_wdma_disable(ctx->test_addr);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_WDMA_STAT_EN, PDP_F_WDMA_STAT_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
}

static void pablo_pdp_hw_g_rdma_addr(void __iomem *base, int idx,
	dma_addr_t *direct, dma_addr_t *corex)
{
	*corex = (dma_addr_t)PDP_GET_R_COREX(base, PDP_R_RDMA_AF_IMG_BASE_ADDR_1P_FRO0 + idx);
	*direct = (dma_addr_t)PDP_GET_R(base, PDP_R_RDMA_AF_IMG_BASE_ADDR_1P_FRO0 + idx);
#if IS_ENABLED(CONFIG_PDP_V5_0)
	*corex = (dma_addr_t)(*corex << LSB_BIT) |
		(dma_addr_t)(PDP_GET_R_COREX(base,
			PDP_R_RDMA_AF_IMG_BASE_ADDR_1P_FRO0_LSB_4B + idx) & LSB_4BITS_MASK);
	*direct = (dma_addr_t)(*direct << LSB_BIT) |
		(dma_addr_t)(PDP_GET_R(base,
			PDP_R_RDMA_AF_IMG_BASE_ADDR_1P_FRO0_LSB_4B + idx) & LSB_4BITS_MASK);
#endif
}

static void pablo_hw_api_pdp_hw_s_af_rdma_addr_kunit_test(struct kunit *test)
{
	dma_addr_t address[8] = {
		0x11111111,
		0x22222222,
		0x33333333,
		0x44444444,
		0x55555555,
		0x66666666,
		0x77777777,
		0x88888888};
	dma_addr_t direct_addr, corex_addr;
	u32 num_buffers = 8;
	u32 direct = 1;
	u32 i;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_af_rdma_addr(ctx->test_addr, address, num_buffers, direct);

	for (i = 0; i < num_buffers; i++) {
		pablo_pdp_hw_g_rdma_addr(ctx->test_addr, i, &direct_addr, &corex_addr);
		KUNIT_EXPECT_EQ(test, direct_addr, address[i]);
		KUNIT_EXPECT_EQ(test, corex_addr, address[i]);
	}
}

static void pablo_hw_api_pdp_hw_s_post_frame_gap_kunit_test(struct kunit *test)
{
	u32 interval = 5;
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_post_frame_gap(ctx->test_addr, interval);

	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_IP_POST_FRAME_GAP);
	KUNIT_EXPECT_EQ(test, val, interval);
}

static void pablo_hw_api_pdp_hw_s_fro_kunit_test(struct kunit *test)
{
	u32 num_buffers = 8;
	u32 center_frame_num;
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	center_frame_num = num_buffers / 2 - 1;

	/* FRO: OFF -> ON */
	pdp_hw_s_fro(ctx->test_addr, num_buffers);

	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_FRO_SW_RESET);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_FRO_MODE_EN, PDP_F_FRO_MODE_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW,
			PDP_F_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW);
	KUNIT_EXPECT_EQ(test, val, (u32)num_buffers - 1);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_FRO_RUN_FRAME_NUMBER_FOR_COL_ROW_INT,
			PDP_F_FRO_RUN_FRAME_NUMBER_FOR_COL_ROW_INT);
	KUNIT_EXPECT_EQ(test, val, center_frame_num);

	/* FRO: ON -> ON */
	PDP_SET_F_DIRECT(ctx->test_addr, PDP_R_FRO_MODE_EN, PDP_F_FRO_MODE_EN, 1);
	PDP_SET_R(ctx->test_addr, PDP_R_FRO_SW_RESET, 0x0);

	pdp_hw_s_fro(ctx->test_addr, num_buffers);

	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_FRO_SW_RESET);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_FRO_MODE_EN, PDP_F_FRO_MODE_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)1);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW,
			PDP_F_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW);
	KUNIT_EXPECT_EQ(test, val, (u32)num_buffers - 1);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_FRO_RUN_FRAME_NUMBER_FOR_COL_ROW_INT,
			PDP_F_FRO_RUN_FRAME_NUMBER_FOR_COL_ROW_INT);
	KUNIT_EXPECT_EQ(test, val, center_frame_num);

	/* FRO: OFF */
	num_buffers = 0;
	pdp_hw_s_fro(ctx->test_addr, num_buffers);

	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW,
			PDP_F_FRO_FRAME_COUNT_TO_RUN_MINUS1_SHADOW);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_FRO_FRAME_COUNT, PDP_F_FRO_FRAME_COUNT);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = PDP_GET_F_COREX(ctx->test_addr, PDP_R_FRO_MODE_EN, PDP_F_FRO_MODE_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
}

static void pablo_hw_api_pdp_hw_wait_idle_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	/* timeout */
	ret = pdp_hw_wait_idle(ctx->test_addr, 0, PDP_TRY_COUNT);
	KUNIT_EXPECT_EQ(test, ret, (int)-ETIME);

	/* normal */
	PDP_SET_F_DIRECT(ctx->test_addr, PDP_R_IDLENESS_STATUS, PDP_F_IDLENESS_STATUS, 1);
	ret = pdp_hw_wait_idle(ctx->test_addr, 0, PDP_TRY_COUNT);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
}

static void pablo_hw_api_pdp_hw_g_pdstat_size_kunit_test(struct kunit *test)
{
	u32 width, height, bits_per_pixel;

	pdp_hw_g_pdstat_size(&width, &height, &bits_per_pixel);

	KUNIT_EXPECT_EQ(test, width, (u32)PDP_STAT_DMA_WIDTH);
	KUNIT_EXPECT_EQ(test, height, (u32)(PDP_STAT_TOTAL_SIZE / PDP_STAT_DMA_WIDTH));
	KUNIT_EXPECT_EQ(test, bits_per_pixel, (u32)8);
}

static void pablo_hw_api_pdp_hw_s_pdstat_path_kunit_test(struct kunit *test)
{
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	PDP_SET_R(ctx->test_addr, PDP_R_RDMA_AF_EN, 1);
	PDP_SET_R(ctx->test_addr, PDP_R_Y_REORDER_ON, 1);

	pdp_hw_s_pdstat_path(ctx->test_addr, 0);

	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_RDMA_AF_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_Y_REORDER_ON);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
}

static void pablo_hw_api_pdp_hw_s_line_row_kunit_test(struct kunit *test)
{
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_line_row(ctx->test_addr, 0, 0, 0);

	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_IP_INT_ON_COL_ROW_CORD);
	if (IS_ENABLED(USE_PDP_LINE_INTR_FOR_PDAF))
		KUNIT_EXPECT_EQ(test, val, (u32)0x10000);
	else
		KUNIT_EXPECT_EQ(test, val, (u32)0x0);
	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_IP_INT_ON_COL_ROW);
	if (IS_ENABLED(USE_PDP_LINE_INTR_FOR_PDAF))
		KUNIT_EXPECT_EQ(test, val, (u32)0xC);
	else
		KUNIT_EXPECT_EQ(test, val, (u32)0x0);
}

static void pablo_hw_api_pdp_hw_s_sensor_type_kunit_test(struct kunit *test)
{
	bool val;
	u32 pd_mode, sensor_type;

	for (pd_mode = 0; pd_mode < 7; pd_mode++) {
		val = pdp_hw_to_sensor_type(pd_mode, &sensor_type);
		if (pd_mode == PD_NONE || pd_mode == PD_MOD2) {
			KUNIT_EXPECT_EQ(test, sensor_type, (u32)SENSOR_TYPE_MOD2);
			KUNIT_EXPECT_EQ(test, val, (bool)false);
		} else if (pd_mode == 6) {
			KUNIT_EXPECT_EQ(test, sensor_type, (u32)SENSOR_TYPE_MOD2);
			KUNIT_EXPECT_EQ(test, val, (bool)false);
		} else {
			KUNIT_EXPECT_EQ(test, sensor_type, pd_mode);
			KUNIT_EXPECT_EQ(test, val, (bool)true);
		}
	}
}

static void pablo_hw_api_pdp_hw_g_input_vc_kunit_test(struct kunit *test)
{
	u32 mux_val = 0x12345, img_vc, af_vc;
	u32 expect_img_vc;

	if (IS_ENABLED(CONFIG_PDP_V4_4))
		expect_img_vc = 0x3;
	else
		expect_img_vc = 0x1;
	pdp_hw_g_input_vc(mux_val, &img_vc, &af_vc);
	KUNIT_EXPECT_EQ(test, img_vc, expect_img_vc);
	KUNIT_EXPECT_EQ(test, af_vc, (u32)0x5);

	mux_val = 0x54321;
	if (IS_ENABLED(CONFIG_PDP_V4_4))
		expect_img_vc = 0x3;
	else
		expect_img_vc = 0x5;
	pdp_hw_g_input_vc(mux_val, &img_vc, &af_vc);
	KUNIT_EXPECT_EQ(test, img_vc, expect_img_vc);
	KUNIT_EXPECT_EQ(test, af_vc, (u32)0x1);
}

static void pablo_hw_api_pdp_hw_g_input_mux_val_kunit_test(struct kunit *test)
{
	u32 mux_val, img_vc = 0, af_vc = 1;

	pdp_hw_g_input_mux_val(&mux_val, img_vc, af_vc);
	KUNIT_EXPECT_EQ(test, mux_val, (u32)0x1);
}

static void pablo_hw_api_pdp_hw_g_int1_state_kunit_test(struct kunit *test)
{
	u32 irq_state;
	unsigned int val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_CONTINT_INT1, INT1_EN_MASK);
	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_FRO_INT0, 0x1);

	val = pdp_hw_g_int1_state(ctx->test_addr, 1, &irq_state);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0x19);
	KUNIT_EXPECT_EQ(test, irq_state, (u32)INT1_EN_MASK);

	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_CONTINT_INT1_CLEAR);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)INT1_EN_MASK);
	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_FRO_INT0_CLEAR);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)0x1);
}

static void pablo_hw_api_pdp_hw_g_int1_mask_kunit_test(struct kunit *test)
{
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_CONTINT_INT1_ENABLE, INT1_EN_MASK);

	val = pdp_hw_g_int1_mask(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, val, (u32)INT1_EN_MASK);
}

static void pablo_hw_api_pdp_hw_g_int2_state_kunit_test(struct kunit *test)
{
	u32 irq_state;
	unsigned int val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_CONTINT_INT2, INT2_EN_MASK);
	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_FRO_INT1, (1 << PDAF_STAT_INT));

	val = pdp_hw_g_int2_state(ctx->test_addr, 1, &irq_state);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)((1 << PDAF_STAT_INT) | INT2_ERR_MASK));
	KUNIT_EXPECT_EQ(test, irq_state, (u32)INT2_EN_MASK);

	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_CONTINT_INT2_CLEAR);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)INT2_EN_MASK);
	val = PDP_GET_R_COREX(ctx->test_addr, PDP_R_FRO_INT1_CLEAR);
	KUNIT_EXPECT_EQ(test, val, (unsigned int)(1 << PDAF_STAT_INT));
}

static void pablo_hw_api_pdp_hw_g_int2_mask_kunit_test(struct kunit *test)
{
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_CONTINT_INT2_ENABLE, INT2_EN_MASK);

	val = pdp_hw_g_int2_mask(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, val, (u32)INT2_EN_MASK);
}

static unsigned int pablo_pdp_check_occurred(enum pdp_event_type type)
{
	u32 i;
	unsigned int ret = 0;
	struct pablo_irq_array {
		u32 type;
		u32 state;
	};
	struct pablo_irq_array occurred_arr[7] = {
		{PE_START, 1 << FRAME_START},
		{PE_END, 1 << FRAME_END_INTERRUPT},
		{PE_PAF_STAT0, 1 << PDAF_STAT_INT},
		{PE_ERR_INT1, INT1_ERR_MASK},
		{PE_ERR_INT2, INT2_ERR_MASK},
		{PE_PAF_OVERFLOW, 1 << CINFIFO_STREAM_OVERFLOW},
		{PE_SPECIAL, 1 << FRAME_START},
	};

	for (i = 0; i < 7; i++) {
		if (occurred_arr[i].type == type) {
			ret = occurred_arr[i].state;
			break;
		}
	}

	return ret;
}

static void pablo_hw_api_pdp_hw_is_occurred_kunit_test(struct kunit *test)
{
	u32 val1, val2, i;
	u32 state = 0xFFFFFFFF;
	for (i = 0; i < PE_SPECIAL + 1; i++) {

		val1 = pdp_hw_is_occurred(state, i);
		val2 = pablo_pdp_check_occurred(i);
		KUNIT_EXPECT_EQ(test, val2, val2);
	}
}

static void pablo_hw_api_pdp_hw_g_int_str_kunit_test(struct kunit *test)
{

	int i, val;
	const char *int1_str[BITS_PER_LONG];
	const char *int2_str[BITS_PER_LONG];

#if IS_ENABLED(CONFIG_PDP_V5_0)
	const char *pdp_int1_str[PDP_INT1_CNT] = {
		[FRAME_START ... PDP_INT1_CNT-1] = "UNKNOWN",
		[FRAME_START] = "FRAME_START",
		[FRAME_END_INTERRUPT] = "FRAME_END_INTERRUPT",
		[FRAME_INT_ON_ROW_COL_INFO] = "FRAME_INT_ON_ROW_COL_INFO",
		[IRQ_CORRUPTED] = "IRQ_CORRUPTED",
		[COREX_ERROR_INT] =  "COREX_ERROR_INT",
		[PRE_FRAME_END_INTERRUPT] = "PRE_FRAME_END_INTERRUPT",
		[INPUT_FRAME_END] = "LIC_INPUT_FRAME_END",
		[REORDER_H_OUTPUT_LAST] = "REORDER_H_OUTPUT_LAST",
		[BPC_H_OUTPUT_LAST] = "BPC_H_OUTPUT_LAST",
		[BPC_V_OUTPUT_LAST] = "BPC_V_OUTPUT_LAST",
		[ALC_H_OUTPUT_LAST] = "ALC_H_OUTPUT_LAST",
		[GAMMA_H_OUTPUT_LAST] = "GAMMA_H_OUTPUT_LAST",
		[GAMMA_V_OUTPUT_LAST] = "GAMMA_V_OUTPUT_LAST",
		[COREX_END_INT_0] = "COREX_END_INT_0",
		[COREX_END_INT_1] = "COREX_END_INT_1",
	};

	const char *pdp_int2_str[PDP_INT2_CNT] = {
		[INPUT_FRAME_END_SRC1 ... PDP_INT2_CNT-1] = "UNKNOWN",
		[INPUT_FRAME_END_SRC1] = "LIC_INPUT_FRAME_END_SRC1",
		[VOTF_LOST_FLUSH_AF] = "VOTF_LOST_FLUSH_AF",
		[C2SER_SLOW_RING] = "C2SER_SLOW_RING",
		[PDAF_STAT_INT] = "PDAF_STAT_INT",
		[SBWC_ERR] = "SBWC_ERR",
		[VOTF_LOST_CON_AF] = "VOTF_LOST_CON_AF",
		[CINFIFO_LINES_ERROR] = "CINFIFO_LINES_ERROR",
		[CINFIFO_COLUMNS_ERROR]  = "CINFIFO_COLUMNS_ERROR",
		[CINFIFO_STREAM_OVERFLOW] = "CINFIFO_STREAM_OVERFLOW",
		[FRAME_START_BEFORE_FRAME_END_CORRUPTED] =
			"FRAME_START_BEFORE_FRAME_END_CORRUPTED",
		[DMACLIENTS_ERROR_IRQ] = "DMACLIENTS_ERROR_IRQ",
	};
#else
	const char *pdp_int1_str[PDP_INT1_CNT] = {
		[FRAME_START ... PDP_INT1_CNT-1] = "UNKNOWN",
		[FRAME_START] = "FRAME_START",
		[FRAME_END_INTERRUPT] = "FRAME_END_INTERRUPT",
		[FRAME_INT_ON_ROW_COL_INFO] = "FRAME_INT_ON_ROW_COL_INFO",
		[IRQ_CORRUPTED]	= "IRQ_CORRUPTED",
		[COREX_ERROR_INT] = "COREX_ERROR_INT",
		[PRE_FRAME_END_INTERRUPT] = "PRE_FRAME_END_INTERRUPT",
		[LIC_INPUT_FRAME_END] = "LIC_INPUT_FRAME_END",
		[LIC_OUTPUT_FRAME_END] = "LIC_OUTPUT_FRAME_END",
		[AFIDENT_DATA_IN_LAST] = "AFIDENT_DATA_IN_LAST",
		[COUTFIFO_DATA_OUT_END_INT] = "COUTFIFO_DATA_OUT_END_INT",
		[COUTFIFO_FRAME_OUT_END_INT] = "COUTFIFO_FRAME_OUT_END_INT",
		[RCB_OUTPUT_LAST] = "RCB_OUTPUT_LAST",
		[MPD_LR_OUTPUT_LAST] = "MPD_LR_OUTPUT_LAST",
		[YEXT_OUTPUT_LAST] = "YEXT_OUTPUT_LAST",
		[BPC_LR_OUTPUT_LAST] = "BPC_LR_OUTPUT_LAST",
		[REORDER_LR_OUTPUT_LAST] = "REORDER_LR_OUTPUT_LAST",
		[GAMMA0_OUTPUT_LAST] = "GAMMA0_OUTPUT_LAST",
		[GAMMA1_LR_OUTPUT_LAST] = "GAMMA1_LR_OUTPUT_LAST",
		[ALC_LR_OUTPUT_LAST] = "ALC_LR_OUTPUT_LAST",
		[COREX_END_INT_0] = "COREX_END_INT_0",
		[COREX_END_INT_1] = "COREX_END_INT_1",
		[COUTFIFO_3AAB_FRAME_DATA_OUT_END]	= "COUTFIFO_3AAB_FRAME_DATA_OUT_END",
		[COUTFIFO_3AAB_FRAME_OUT_END] = "COUTFIFO_3AAB_FRAME_OUT_END",
		[COUTFIFO_3AAY_FRAME_DATA_OUT_END]	= "COUTFIFO_3AAY_FRAME_DATA_OUT_END",
		[COUTFIFO_3AAY_FRAME_OUT_END] = "COUTFIFO_3AAY_FRAME_OUT_END",
	};
	const char *pdp_int2_str[PDP_INT2_CNT] = {
		[LIC_INPUT_FRAME_END_SRC1 ... PDP_INT2_CNT-1] = "UNKNOWN",
		[LIC_INPUT_FRAME_END_SRC1] = "LIC_INPUT_FRAME_END_SRC1",
		[COUTFIFO_FRAME_OUT_START_INT] = "COUTFIFO_FRAME_OUT_START_INT",
		[VOTF_LOST_FLUSH_IMG] = "VOTF_LOST_FLUSH_IMG",
		[VOTF_LOST_FLUSH_AF] = "VOTF_LOST_FLUSH_AF",
		[C2SER_SLOW_RING] = "C2SER_SLOW_RING",
		[PDAF_STAT_INT] = "PDAF_STAT_INT",
		[SBWC_ERR] = "SBWC_ERR",
		[VOTF_LOST_CON_IMG]	= "VOTF_LOST_CON_IMG",
		[VOTF_LOST_CON_AF] = "VOTF_LOST_CON_AF",
		[COUTFIFO_3AA_BAYER_FRAME_OUT_START] = "COUTFIFO_3AA_BAYER_FRAME_OUT_START",
		[COUTFIFO_3AA_Y_FRAME_OUT_START] = "COUTFIFO_3AA_Y_FRAME_OUT_START",
		[COUTFIFO_SIZE_ERROR] = "COUTFIFO_SIZE_ERROR",
		[COUTFIFO_LINE_ERROR] = "COUTFIFO_LINE_ERROR",
		[COUTFIFO_COL_ERROR] = "COUTFIFO_COL_ERROR",
		[COUTFIFO_OVERFLOW_ERROR] = "COUTFIFO_OVERFLOW_ERROR",
		[CINFIFO_TOTAL_SIZE_ERROR] = "CINFIFO_TOTAL_SIZE_ERROR",
		[CINFIFO_LINES_ERROR] = "CINFIFO_LINES_ERROR",
		[CINFIFO_COLUMNS_ERROR] = "CINFIFO_COLUMNS_ERROR",
		[CINFIFO_STREAM_OVERFLOW] = "CINFIFO_STREAM_OVERFLOW",
		[FRAME_START_BEFORE_FRAME_END_CORRUPTED] = "FRAME_START_BEFORE_FRAME_END_CORRUPTED",
		[DMACLIENTS_ERROR_IRQ] = "DMACLIENTS_ERROR_IRQ",
	};
#endif
	for(i = 0; i < PDP_INT1_CNT; i++) {
		pdp_hw_g_int1_str(int1_str);
		val = strncmp(int1_str[i], pdp_int1_str[i], 40);
		KUNIT_EXPECT_EQ(test, val, (int)0);
	}

	for(i = 0; i < PDP_INT2_CNT; i++) {
		pdp_hw_g_int2_str(int2_str);
		val = strncmp(int2_str[i], pdp_int2_str[i], 40);
		KUNIT_EXPECT_EQ(test, val, (int)0);
	}
}

static int pablo_pdp_check_reorder_sram_size(u32 width, int vc_sensor_mode)
{
	int size = 0;

	switch (vc_sensor_mode) {
	case VC_SENSOR_MODE_MSPD_TAIL:
	case VC_SENSOR_MODE_MSPD_GLOBAL_TAIL:
		size = width * 2;
		break;
	case VC_SENSOR_MODE_ULTRA_PD_TAIL:
	case VC_SENSOR_MODE_ULTRA_PD_2_TAIL:
	case VC_SENSOR_MODE_ULTRA_PD_3_TAIL:
	case VC_SENSOR_MODE_SUPER_PD_3_TAIL:
	case VC_SENSOR_MODE_SUPER_PD_5_TAIL:
	case VC_SENSOR_MODE_IMX_2X1OCL_1_TAIL:
		size = width;
		break;
	case VC_SENSOR_MODE_SUPER_PD_TAIL:
	case VC_SENSOR_MODE_SUPER_PD_2_TAIL:
	case VC_SENSOR_MODE_SUPER_PD_4_TAIL:
	case VC_SENSOR_MODE_IMX_2X1OCL_2_TAIL:
		size = width / 2;
		break;
	default:
		break;
	}

	return size;
}

static void pablo_hw_api_pdp_hw_g_reorder_sram_size_kunit_test(struct kunit *test)
{
	u32 width = 320;
	u32 i, init, target;
	int val1, val2;

	/* MSPD */
	init = VC_SENSOR_MODE_MSPD_NORMAL;
	target = VC_SENSOR_MODE_MSPD_GLOBAL_TAIL + 1;
	for (i = init; i < target; i++) {
		val1 = pdp_hw_get_bpc_reorder_sram_size(width, i);
		val2 = pablo_pdp_check_reorder_sram_size(width, i);
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	/* Ultra PD */
	init = VC_SENSOR_MODE_ULTRA_PD_NORMAL;
	target = VC_SENSOR_MODE_ULTRA_PD_3_TAIL + 1;
	for (i = init; i < target; i++) {
		val1 = pdp_hw_get_bpc_reorder_sram_size(width, i);
		val2 = pablo_pdp_check_reorder_sram_size(width, i);
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	/* Super PD */
	init = VC_SENSOR_MODE_SUPER_PD_NORMAL;
	target = VC_SENSOR_MODE_SUPER_PD_5_TAIL + 1;
	for (i = init; i < target; i++) {
		val1 = pdp_hw_get_bpc_reorder_sram_size(width, i);
		val2 = pablo_pdp_check_reorder_sram_size(width, i);
		KUNIT_EXPECT_EQ(test, val1, val2);
	}
}

static void pablo_hw_api_pdp_hw_strgen_enable_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_strgen_enable(ctx->test_addr);
}

static void pablo_hw_api_pdp_hw_strgen_disable_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_strgen_disable(ctx->test_addr);
}

static void pablo_hw_api_pdp_hw_s_config_default_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_config_default(ctx->test_addr);
}

static void pablo_hw_api_pdp_hw_dump_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_dump(ctx->test_addr);
}

static void pablo_hw_api_pdp_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	dma_addr_t address[8] = { 0 };
	u32 num_buffers = 0;
	u32 direct = 0;
	struct is_sensor_cfg cfg = { 0 };
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_rdma_addr(ctx->test_addr, address, num_buffers, direct, &cfg);
}

static void pablo_hw_api_pdp_hw_update_rdma_linegap_kunit_test(struct kunit *test)
{
	struct is_sensor_cfg *cfg = kunit_kzalloc(test, sizeof(struct is_sensor_cfg), 0);
	struct is_pdp *pdp = kunit_kzalloc(test, sizeof(struct is_pdp), 0);
	u32 en_votf = 0;
	u32 min_fps = 0;
	u32 max_fps = 0;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdp);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cfg);

	pdp_hw_update_rdma_linegap(ctx->test_addr, cfg, pdp, en_votf, min_fps, max_fps);

	if (pdp)
		kunit_kfree(test, pdp);
	if (cfg)
		kunit_kfree(test, cfg);
}

static void pablo_hw_api_pdp_hw_g_int2_rdma_state_kunit_test(struct kunit *test)
{
	unsigned int ret;
	bool clear = false;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	ret = pdp_hw_g_int2_rdma_state(ctx->test_addr, clear);
}

static void pablo_hw_api_pdp_hw_s_lic_bit_mode_kunit_test(struct kunit *test)
{
	u32 pixelsize = 0;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	pdp_hw_s_lic_bit_mode(ctx->test_addr, pixelsize);
}

static void pablo_hw_api_pdp_hw_rdma_wait_idle_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	PDP_SET_R_DIRECT(ctx->test_addr, PDP_R_RDMA_AF_MON_STATUS3, 1);

	ret = pdp_hw_rdma_wait_idle(ctx->test_addr);
	KUNIT_EXPECT_EQ(test, ret, -ETIME);
}

static void pablo_hw_api_pdp_hw_s_input_mux_kunit_test(struct kunit *test)
{
	int i;
	u32 val;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;
	struct is_pdp *pdp = kunit_kzalloc(test, sizeof(struct is_pdp), 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdp);

	pdp->img_vc = 0;
	pdp->csi_ch = 5;
	pdp->id = 1;
	pdp->mux_base = ctx->test_addr;
	pdp->mux_elems = 8;
	pdp->mux_val = kunit_kzalloc(test, sizeof(u32) * pdp->mux_elems, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdp->mux_val);

	for(i = 0; i < pdp->mux_elems; i++)
		pdp->mux_val[i] = i;

	pdp_hw_s_input_mux(pdp);

	if (!IS_ENABLED(CONFIG_PDP_V5_0)) {
		val = *(u32 *)(ctx->test_addr);
		KUNIT_EXPECT_EQ(test, val, pdp->mux_val[pdp->csi_ch]);
	}

	kunit_kfree(test, pdp->mux_val);
	kunit_kfree(test, pdp);
}

static void pablo_hw_api_pdp_hw_s_input_enable_kunit_test(struct kunit *test)
{
	u32 val, i;
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;
	struct is_pdp *pdp = kunit_kzalloc(test, sizeof(struct is_pdp), 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdp);

	pdp->csi_ch = 5;
	pdp->id = 1;
	pdp->en_base = ctx->test_addr;
	pdp->en_elems = 7;
	pdp->en_val = kunit_kzalloc(test, sizeof(u32) * pdp->en_elems, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdp->en_val);

	for(i = 0; i < pdp->en_elems; i++)
		pdp->en_val[i] = i;

	pdp_hw_s_input_enable(pdp, true);

	if (pdp->en_base) {
		val = *(u32 *)(ctx->test_addr);
		KUNIT_EXPECT_EQ(test, val, (u32)(1 << pdp->en_val[pdp->csi_ch]));
	}

	pdp_hw_s_input_enable(pdp, false);

	if (pdp->en_base) {
		val = *(u32 *)(ctx->test_addr);
		KUNIT_EXPECT_EQ(test, val, (u32)0);
	}

	kunit_kfree(test, pdp->en_val);
	kunit_kfree(test, pdp);
}

static int pablo_hw_api_pdp_hw_kunit_test_init(struct kunit *test)
{
	pablo_hw_api_pdp_test_ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pablo_hw_api_pdp_test_ctx.test_addr);

	test->priv = &pablo_hw_api_pdp_test_ctx;

	return 0;
}

static void pablo_hw_api_pdp_hw_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_api_pdp_test_ctx *ctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ctx);

	kunit_kfree(test, ctx->test_addr);
}

static struct kunit_case pablo_hw_api_pdp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_ip_version_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_idle_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_get_line_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_global_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_global_enable_clear_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_one_shot_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_corex_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_corex_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_corex_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_core_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_reset_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_wdma_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_wdma_onoff_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_af_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_post_frame_gap_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_fro_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_pdstat_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_pdstat_path_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_line_row_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_sensor_type_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_input_vc_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_input_mux_val_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_int1_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_int1_mask_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_int2_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_int2_mask_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_is_occurred_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_int_str_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_reorder_sram_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_strgen_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_strgen_disable_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_config_default_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_update_rdma_linegap_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_g_int2_rdma_state_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_lic_bit_mode_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_rdma_wait_idle_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_input_mux_kunit_test),
	KUNIT_CASE(pablo_hw_api_pdp_hw_s_input_enable_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_pdp_kunit_test_suite = {
	.name = "pablo-hw-api-pdp-kunit-test",
	.init = pablo_hw_api_pdp_hw_kunit_test_init,
	.exit = pablo_hw_api_pdp_hw_kunit_test_exit,
	.test_cases = pablo_hw_api_pdp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_pdp_kunit_test_suite);

MODULE_LICENSE("GPL");
