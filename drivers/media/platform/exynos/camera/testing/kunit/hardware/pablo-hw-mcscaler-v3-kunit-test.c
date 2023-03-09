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

#include "hardware/include/is-hw-mcscaler-v3.h"
#include "hardware/api/is-hw-api-mcscaler-v3.h"
#include "is-interface-ddk.h"
#include "pmio.h"

static struct pablo_hw_mcsc_kunit_test_ctx {
	struct is_hw_ip *hw_ip;
	u32 instance;
	struct is_core *core;
	struct is_framemgr framemgr;
	struct is_hardware *hardware;
	int hw_slot;
	struct is_region is_region;
	struct is_param_region param_region;
	struct is_frame frame;
	struct camera2_shot shot;
	void *addr;
} test_ctx;

/* Define the test cases. */

static void pablo_hw_mcsc_open_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	int ret;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
}

static void pablo_hw_mcsc_init_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	int ret;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, 0, LIB_FRAME_HDR_SINGLE);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->deinit(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
}

static void pablo_hw_mcsc_enable_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	int ret;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, 0, LIB_FRAME_HDR_SINGLE);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->disable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->deinit(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
}

static void pablo_hw_mcsc_set_param_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	IS_DECLARE_PMAP(pmap);
	int ret;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, 0, LIB_FRAME_HDR_SINGLE);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	IS_INIT_PMAP(pmap);
	ret = tctx->hw_ip->ops->set_param(tctx->hw_ip, &tctx->is_region,
		pmap, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->disable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->deinit(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
}

static void pablo_hw_mcsc_shot_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	IS_DECLARE_PMAP(pmap);
	int ret;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->init(tctx->hw_ip, tctx->instance, 0, LIB_FRAME_HDR_SINGLE);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->enable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	IS_INIT_PMAP(pmap);
	ret = tctx->hw_ip->ops->set_param(tctx->hw_ip, &tctx->is_region,
		pmap, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->shot(tctx->hw_ip, &tctx->frame, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->disable(tctx->hw_ip, tctx->instance, tctx->hardware->hw_map[0]);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->deinit(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
}

#if IS_ENABLED(CONFIG_MC_SCALER_V11_0)
static void pablo_hw_mcsc_dtp_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	int ret, val;

	is_hw_mcsc_s_debug_type(MCSC_DBG_DTP_OTF);
	is_hw_mcsc_s_debug_type(MCSC_DBG_DTP_RDMA);

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	val = MCSC_GET_F(tctx->hw_ip->regs[REG_SETA], MCSC_R_YUV_DTPOTFIN_BYPASS,
		MCSC_F_YUV_DTPOTFIN_BYPASS);
	KUNIT_EXPECT_EQ(test, 0, val);
	val = MCSC_GET_F(tctx->hw_ip->regs[REG_SETA], MCSC_R_YUV_DTPOTFIN_TEST_PATTERN_MODE,
		MCSC_F_YUV_DTPOTFIN_TEST_PATTERN_MODE);
	KUNIT_EXPECT_EQ(test, MCSC_DTP_COLOR_BAR, val);
	val = MCSC_GET_F(tctx->hw_ip->regs[REG_SETA], MCSC_R_YUV_DTPOTFIN_YUV_STANDARD,
		MCSC_F_YUV_DTPOTFIN_YUV_STANDARD);
	KUNIT_EXPECT_EQ(test, MCSC_DTP_COLOR_BAR_BT601, val);

	val = MCSC_GET_F(tctx->hw_ip->regs[REG_SETA], MCSC_R_YUV_DTPRDMAIN_BYPASS,
		MCSC_F_YUV_DTPRDMAIN_BYPASS);
	KUNIT_EXPECT_EQ(test, 0, val);
	val = MCSC_GET_F(tctx->hw_ip->regs[REG_SETA], MCSC_R_YUV_DTPRDMAIN_TEST_PATTERN_MODE,
		MCSC_F_YUV_DTPRDMAIN_TEST_PATTERN_MODE);
	KUNIT_EXPECT_EQ(test, MCSC_DTP_COLOR_BAR, val);
	val = MCSC_GET_F(tctx->hw_ip->regs[REG_SETA], MCSC_R_YUV_DTPRDMAIN_YUV_STANDARD,
		MCSC_F_YUV_DTPRDMAIN_YUV_STANDARD);
	KUNIT_EXPECT_EQ(test, MCSC_DTP_COLOR_BAR_BT601, val);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);

	is_hw_mcsc_c_debug_type(MCSC_DBG_DTP_OTF);
	is_hw_mcsc_c_debug_type(MCSC_DBG_DTP_RDMA);
}

static void pablo_hw_mcsc_strgen_kunit_test(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *tctx = test->priv;
	struct mcs_param *mcs_param;
	struct is_hw_mcsc *hw_mcsc;
	int ret, val;

	is_hw_mcsc_s_debug_type(MCSC_DBG_STRGEN);

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	mcs_param = &tctx->param_region.mcs;
	mcs_param->input.otf_cmd = 1;
	mcs_param->input.width = 320;
	mcs_param->input.height = 240;
	mcs_param->input.otf_format = OTF_INPUT_FORMAT_YUV422;
	mcs_param->input.otf_bitwidth = OTF_INPUT_BIT_WIDTH_10BIT;
	hw_mcsc = (struct is_hw_mcsc *)tctx->hw_ip->priv_info;
	hw_mcsc->cur_setfile[0][0] = &hw_mcsc->setfile[0][0];

	is_hw_mcsc_update_param(tctx->hw_ip, mcs_param, tctx->instance);

	val = MCSC_GET_F(tctx->hw_ip->regs[REG_SETA], MCSC_R_YUV_CINFIFO_CONFIG,
		MCSC_F_YUV_CINFIFO_STRGEN_MODE_EN);
	KUNIT_EXPECT_EQ(test, 1, val);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);

	is_hw_mcsc_c_debug_type(MCSC_DBG_STRGEN);
}
#endif
static int pablo_hw_mcsc_kunit_test_init(struct kunit *test)
{
	void *addr;

	test_ctx.core = is_get_is_core();
	test_ctx.hardware = &test_ctx.core->hardware;
	test_ctx.hw_slot = CALL_HW_CHAIN_INFO_OPS(test_ctx.hardware, get_hw_slot_id, DEV_HW_MCSC0);
	test_ctx.hw_ip = &test_ctx.hardware->hw_ip[test_ctx.hw_slot];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.hw_ip);
	test_ctx.hw_ip->framemgr = &test_ctx.framemgr;
	test_ctx.hw_ip->region[0] = &test_ctx.is_region;
	test_ctx.frame.shot = &test_ctx.shot;
	test_ctx.hw_ip->hardware = test_ctx.hardware;

	addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr);
	test_ctx.addr = test_ctx.hw_ip->pmio->mmio_base;
	test_ctx.hw_ip->pmio->mmio_base = addr;
	test_ctx.hw_ip->pmio_config.mmio_base = addr;

	test->priv = (struct pablo_hw_mcfp_kunit_test_ctx *)&test_ctx;

	return 0;
}

static void pablo_hw_mcsc_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_mcsc_kunit_test_ctx *test_ctx = test->priv;

	kunit_kfree(test, test_ctx->hw_ip->pmio->mmio_base);
	test_ctx->hw_ip->pmio->mmio_base = test_ctx->addr;
	test_ctx->hw_ip->pmio_config.mmio_base = test_ctx->addr;
	test_ctx->hw_ip->state = 0;

	memset(test_ctx, 0, sizeof(struct pablo_hw_mcsc_kunit_test_ctx));
}

static struct kunit_case pablo_hw_mcsc_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_mcsc_open_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_init_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_enable_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_set_param_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_shot_kunit_test),
#if IS_ENABLED(CONFIG_MC_SCALER_V11_0)
	KUNIT_CASE(pablo_hw_mcsc_dtp_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_strgen_kunit_test),
#endif
	{},
};

struct kunit_suite pablo_hw_mcsc_kunit_test_suite = {
	.name = "pablo-hw-mcsc-kunit-test",
	.init = pablo_hw_mcsc_kunit_test_init,
	.exit = pablo_hw_mcsc_kunit_test_exit,
	.test_cases = pablo_hw_mcsc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_mcsc_kunit_test_suite);

MODULE_LICENSE("GPL");
