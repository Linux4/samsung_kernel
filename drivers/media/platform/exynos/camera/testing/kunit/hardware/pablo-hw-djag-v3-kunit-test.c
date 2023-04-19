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

#include "is-hw-mcscaler-v4.h"
#include "api/is-hw-api-mcscaler-v4.h"
#include "../interface/is-interface-ischain.h"
#include "is-param.h"
#include "is-err.h"
#include "pmio.h"

static struct pablo_hw_djag_v3_kunit_test_ctx {
	struct is_hw_ip *hw_ip;
	u32 instance;
	struct is_core *core;
	struct is_hardware *hardware;
	int hw_slot;
	struct mcs_param param;
	struct param_mcs_input input;
	struct param_stripe_input stripe_input;
	struct is_hw_mcsc_cap cap;
	struct is_framemgr framemgr;
	struct is_region is_region;
	void *addr;
} test_ctx;

static void pablo_hw_mcsc_adjust_size_with_djag_kunit_test(struct kunit *test)
{
	struct pablo_hw_djag_v3_kunit_test_ctx *tctx = test->priv;
	int x, y, width, height;

	tctx->cap.djag = 1;
	tctx->input.djag_out_width = 320;

	tctx->stripe_input.total_count = 1;
	is_hw_mcsc_adjust_size_with_djag(tctx->hw_ip, &tctx->input,
		&tctx->stripe_input, &tctx->cap, 0, &x, &y, &width, &height);
	KUNIT_EXPECT_EQ(test, width, 0);
	KUNIT_EXPECT_EQ(test, height, 0);

	tctx->stripe_input.total_count = 0;
	is_hw_mcsc_adjust_size_with_djag(tctx->hw_ip, &tctx->input,
		&tctx->stripe_input, &tctx->cap, 0, &x, &y, &width, &height);
	KUNIT_EXPECT_EQ(test, width, 0);
	KUNIT_EXPECT_EQ(test, height, 0);
}

static void pablo_hw_mcsc_update_djag_register_kunit_test(struct kunit *test)
{
	struct pablo_hw_djag_v3_kunit_test_ctx *tctx = test->priv;
	int ret;
	struct is_hw_mcsc *hw_mcsc = NULL;

	ret = tctx->hw_ip->ops->open(tctx->hw_ip, tctx->instance);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_mcsc = (struct is_hw_mcsc *)tctx->hw_ip->priv_info;
	hw_mcsc->cur_setfile[0][0] = &hw_mcsc->setfile[0][0];
	tctx->param.input.width = 160;
	tctx->param.input.height = 120;
	tctx->param.output[0].width = 320;
	tctx->param.output[0].height = 240;
	tctx->param.output[0].cmd = 1;
	tctx->param.stripe_input.index = 0;
	tctx->param.stripe_input.total_count = 2;

	hw_mcsc->cur_setfile[0][0]->djag.djag_en = 0;
	ret = is_hw_mcsc_update_djag_register(tctx->hw_ip, &tctx->param, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_mcsc->cur_setfile[0][0]->djag.djag_en = 1;
	ret = is_hw_mcsc_update_djag_register(tctx->hw_ip, &tctx->param, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = tctx->hw_ip->ops->close(tctx->hw_ip, tctx->instance);
}

static int pablo_hw_djag_v3_kunit_test_init(struct kunit *test)
{
	void *addr;

	test_ctx.core = is_get_is_core();
	test_ctx.hardware = &test_ctx.core->hardware;
	test_ctx.hw_slot = CALL_HW_CHAIN_INFO_OPS(test_ctx.hardware, get_hw_slot_id, DEV_HW_MCSC0);
	test_ctx.hw_ip = &test_ctx.hardware->hw_ip[test_ctx.hw_slot];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.hw_ip);
	test_ctx.hw_ip->framemgr = &test_ctx.framemgr;
	test_ctx.hw_ip->region[0] = &test_ctx.is_region;
	test_ctx.hw_ip->hardware = test_ctx.hardware;

	addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr);
	test_ctx.addr = test_ctx.hw_ip->pmio->mmio_base;
	test_ctx.hw_ip->pmio->mmio_base = addr;
	test_ctx.hw_ip->pmio_config.mmio_base = addr;

	test->priv = (struct pablo_hw_djag_v3_kunit_test_ctx *)&test_ctx;

	return 0;
}

static void pablo_hw_djag_v3_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_djag_v3_kunit_test_ctx *test_ctx = test->priv;

	kunit_kfree(test, test_ctx->hw_ip->pmio->mmio_base);
	test_ctx->hw_ip->state = 0;
	test_ctx->hw_ip->pmio->mmio_base = test_ctx->addr;
	test_ctx->hw_ip->pmio_config.mmio_base = test_ctx->addr;

	memset(test_ctx, 0, sizeof(struct pablo_hw_djag_v3_kunit_test_ctx));
}

static struct kunit_case pablo_hw_djag_v3_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_mcsc_adjust_size_with_djag_kunit_test),
	KUNIT_CASE(pablo_hw_mcsc_update_djag_register_kunit_test),
	{},
};

struct kunit_suite pablo_hw_djag_v3_kunit_test_suite = {
	.name = "pablo-hw-djag-v3-kunit-test",
	.init = pablo_hw_djag_v3_kunit_test_init,
	.exit = pablo_hw_djag_v3_kunit_test_exit,
	.test_cases = pablo_hw_djag_v3_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_djag_v3_kunit_test_suite);

MODULE_LICENSE("GPL");
