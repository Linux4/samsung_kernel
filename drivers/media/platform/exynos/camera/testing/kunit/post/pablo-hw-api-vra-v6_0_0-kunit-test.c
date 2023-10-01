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
#include "vra/pablo-vra.h"
#include "vra/pablo-hw-api-vra.h"
#include "vra/pablo-hw-reg-vra-v6_0_0.h"
#include "is-common-enum.h"

static struct cameraepp_hw_api_vra_kunit_test_ctx {
	void *addr;
} test_ctx;

static const struct vra_variant vra_variant[] = {
	{
		.limit_input = {
			.min_w		= 320,
			.min_h		= 240,
			.max_w		= 640,
			.max_h		= 480,
		},
		.version		= IP_VERSION,
	},
};
static const struct vra_fmt vra_formats[] = {
	{
		.name		= "RGB24",
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.bitperpixel	= { 24 },
		.num_planes	= 1,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "GREY",
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.bitperpixel	= { 8 },
		.num_planes	= 1,
		.h_shift	= 1,
		.v_shift	= 1,
	},

};


/* Define the test cases. */
static void camerapp_hw_api_vra_get_size_constraints_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_vra_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_vra_func *func = camerapp_kunit_get_hw_vra_test();
	const struct vra_variant *constraints;
	u32 val;

	constraints = func->camerapp_hw_vra_get_size_constraints(tctx->addr);
	val = func->camerapp_hw_vra_get_ver(tctx->addr);

	KUNIT_EXPECT_EQ(test, constraints->limit_input.min_w, vra_variant->limit_input.min_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.min_h, vra_variant->limit_input.min_h);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.max_w, vra_variant->limit_input.max_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.max_h, vra_variant->limit_input.max_h);
	KUNIT_EXPECT_EQ(test, constraints->version, vra_variant->version);
	KUNIT_EXPECT_EQ(test, constraints->version, val);
}

static void camerapp_hw_api_vra_sfr_dump_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_vra_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_vra_func *func = camerapp_kunit_get_hw_vra_test();

	func->camerapp_vra_sfr_dump(tctx->addr);
}

static void camerapp_hw_api_vra_start_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_vra_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_vra_func *func = camerapp_kunit_get_hw_vra_test();
	u32 val;

	func->camerapp_hw_vra_start(tctx->addr);

	val = *(u32 *)(tctx->addr + vra_regs[VRA_R_GO].sfr_offset);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void camerapp_hw_api_vra_stop_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_vra_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_vra_func *func = camerapp_kunit_get_hw_vra_test();
	u32 val;

	*(u32 *)(tctx->addr + vra_regs[VRA_R_IP_CFG].sfr_offset) = 0x1;

	func->camerapp_hw_vra_stop(tctx->addr);

	val = *(u32 *)(tctx->addr + vra_regs[VRA_R_IP_CFG].sfr_offset);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
}

static void camerapp_hw_api_vra_init_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_vra_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_vra_func *func = camerapp_kunit_get_hw_vra_test();
	u32 val1, val2, val3, val4, val5;

	func->camerapp_hw_vra_sw_init(tctx->addr);

	val1 = *(u32 *)(tctx->addr + vra_regs[VRA_R_IP_CFG].sfr_offset);
	val2 = *(u32 *)(tctx->addr + vra_regs[VRA_R_PREFETCH_CFG].sfr_offset);
	val3 = *(u32 *)(tctx->addr + vra_regs[VRA_R_AXIM_SWAP_CFG].sfr_offset);
	val4 = *(u32 *)(tctx->addr + vra_regs[VRA_R_AXIM_OUTSTANDING_CFG].sfr_offset);
	val5 = *(u32 *)(tctx->addr + vra_regs[VRA_R_IRQ_CFG].sfr_offset);

	KUNIT_EXPECT_EQ(test, val1, (u32)1);
	KUNIT_EXPECT_EQ(test, val2, (u32)0x3f4);
	KUNIT_EXPECT_EQ(test, val3, (u32)0);
	KUNIT_EXPECT_EQ(test, val4, (u32)0x0);
	KUNIT_EXPECT_EQ(test, val5, (u32)0xB);
}

static void camerapp_hw_api_vra_sw_reset_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_vra_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_vra_func *func = camerapp_kunit_get_hw_vra_test();
	u32 val;

	func->camerapp_hw_vra_sw_reset(tctx->addr);

	val = *(u32 *)(tctx->addr + vra_regs[VRA_R_SW_RESET_BIT].sfr_offset);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void camerapp_hw_vra_get_intr_status_and_clear_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_vra_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_vra_func *func = camerapp_kunit_get_hw_vra_test();
	u32 val, clear;
	u32 intr = 0x7;

	*(u32 *)(tctx->addr + vra_regs[VRA_R_IRQ_MASKED].sfr_offset) = intr;

	val = func->camerapp_hw_vra_get_intr_status_and_clear(tctx->addr);

	clear = *(u32 *)(tctx->addr + vra_regs[VRA_R_IRQ_MASKED].sfr_offset);

	KUNIT_EXPECT_EQ(test, val, intr);
	KUNIT_EXPECT_EQ(test, clear, val);
}

static void camerapp_hw_vra_get_fe_kunit_test(struct kunit *test)
{
	struct camerapp_kunit_hw_vra_func *func = camerapp_kunit_get_hw_vra_test();
	u32 fe = 0x1;
	u32 val;

	val = func->camerapp_hw_vra_get_int_frame_end();

	KUNIT_EXPECT_EQ(test, fe, val);
}

static void camerapp_hw_vra_update_param_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_vra_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_vra_func *func = camerapp_kunit_get_hw_vra_test();
	struct vra_dev *vra;
	struct vra_ctx *ctx;

	vra = kzalloc(sizeof(struct vra_dev), GFP_KERNEL);
	ctx = kzalloc(sizeof(struct vra_ctx), GFP_KERNEL);

	ctx->s_frame.width = 320;
	ctx->s_frame.height = 240;
	ctx->s_frame.vra_fmt = &vra_formats[1];
	ctx->s_frame.addr.y = 0x11111111;
	ctx->d_frame.width = 320;
	ctx->d_frame.height = 240;
	ctx->d_frame.addr.y = 0x22222222;
	ctx->d_frame.vra_fmt = &vra_formats[1];
	ctx->i_addr = 0xAAAABBBB;
	ctx->c_addr = 0xCCCCDDDD;
	ctx->t_addr = 0xEEEEFFFF;
	vra->current_ctx = ctx;

	func->camerapp_hw_vra_update_param(tctx->addr, vra);

	kfree(ctx);
	kfree(vra);
}

static void camerapp_hw_vra_interrupt_disable_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_vra_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_vra_func *func = camerapp_kunit_get_hw_vra_test();
	u32 val1;
	u32 val2;
	u32 intr = 0x7;

	*(u32 *)(tctx->addr + vra_regs[VRA_R_IRQ_CFG].sfr_offset) = intr;
	val1 = *(u32 *)(tctx->addr + vra_regs[VRA_R_IRQ_CFG].sfr_offset);
	KUNIT_EXPECT_EQ(test, val1, intr);

	func->camerapp_hw_vra_interrupt_disable(tctx->addr);
	val2 = *(u32 *)(tctx->addr + vra_regs[VRA_R_IRQ_CFG].sfr_offset);
	KUNIT_EXPECT_EQ(test, val2, (u32)0);
}

static struct kunit_case camerapp_hw_api_vra_kunit_test_cases[] = {
	KUNIT_CASE(camerapp_hw_api_vra_get_size_constraints_kunit_test),
	KUNIT_CASE(camerapp_hw_api_vra_sfr_dump_kunit_test),
	KUNIT_CASE(camerapp_hw_api_vra_start_kunit_test),
	KUNIT_CASE(camerapp_hw_api_vra_stop_kunit_test),
	KUNIT_CASE(camerapp_hw_api_vra_init_kunit_test),
	KUNIT_CASE(camerapp_hw_api_vra_sw_reset_kunit_test),
	KUNIT_CASE(camerapp_hw_vra_get_intr_status_and_clear_kunit_test),
	KUNIT_CASE(camerapp_hw_vra_get_fe_kunit_test),
	KUNIT_CASE(camerapp_hw_vra_update_param_kunit_test),
	KUNIT_CASE(camerapp_hw_vra_interrupt_disable_kunit_test),
	{},
};

static int camerapp_hw_api_vra_kunit_test_init(struct kunit *test)
{
	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);
	test->priv = &test_ctx;

	return 0;
}

static void camerapp_hw_api_vra_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.addr);
}

struct kunit_suite camerapp_hw_api_vra_kunit_test_suite = {
	.name = "pablo-hw-api-vra-v6_0_0-kunit-test",
	.init = camerapp_hw_api_vra_kunit_test_init,
	.exit = camerapp_hw_api_vra_kunit_test_exit,
	.test_cases = camerapp_hw_api_vra_kunit_test_cases,
};
define_pablo_kunit_test_suites(&camerapp_hw_api_vra_kunit_test_suite);

MODULE_LICENSE("GPL");
