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

#include "hardware/api/is-hw-api-mcfp.h"
#include "hardware/sfr/is-sfr-mcfp-v10_1.h"

static struct pablo_hw_api_mcfp_kunit_test_ctx {
	struct is_common_dma dma;
	struct param_dma_input dma_input;
	struct param_stripe_input stripe_input;
	u32 width, height;
	u32 sbwc_en, payload_size, strip_offset, header_offset;
	struct is_mcfp_config config;
	void *addr;
} test_ctx;

/* Define the test cases. */

static void pablo_hw_api_mcfp_dump_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_mcfp_kunit_test_ctx *tctx = test->priv;

	mcfp_hw_dump(tctx->addr);
}

static void pablo_hw_api_mcfp_hw_s_rdma_init_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_mcfp_kunit_test_ctx *tctx = test->priv;
	int ret;

	tctx->dma.id = MCFP_RDMA_CUR_IN_Y;
	tctx->dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
	tctx->dma_input.format = DMA_INOUT_FORMAT_YUV422;
	tctx->dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	tctx->dma_input.sbwc_type = DMA_INPUT_SBWC_DISABLE;
	tctx->dma_input.msb = tctx->dma_input.bitwidth - 1;
	ret = mcfp_hw_s_rdma_init(&tctx->dma, &tctx->dma_input, &tctx->stripe_input,
		tctx->width, tctx->height, &tctx->sbwc_en, &tctx->payload_size,
		&tctx->strip_offset, &tctx->header_offset, &tctx->config);
	KUNIT_EXPECT_EQ(test, 0, ret);

	tctx->dma.id = MCFP_RDMA_CUR_IN_WGT;
	ret = mcfp_hw_s_rdma_init(&tctx->dma, &tctx->dma_input, &tctx->stripe_input,
		tctx->width, tctx->height, &tctx->sbwc_en, &tctx->payload_size,
		&tctx->strip_offset, &tctx->header_offset, &tctx->config);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_hw_api_mcfp_hw_rdma_create_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_mcfp_kunit_test_ctx *tctx = test->priv;
	int ret;

	tctx->dma.id = MCFP_RDMA_CUR_IN_Y;
	tctx->dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
	tctx->dma_input.format = DMA_INOUT_FORMAT_YUV422;
	tctx->dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	tctx->dma_input.sbwc_type = DMA_INPUT_SBWC_DISABLE;
	tctx->dma_input.msb = tctx->dma_input.bitwidth - 1;
	ret = mcfp_hw_rdma_create(&tctx->dma, tctx->addr, MCFP_RDMA_CUR_IN_Y);
	KUNIT_EXPECT_EQ(test, 0, ret);

	tctx->dma.id = MCFP_RDMA_CUR_IN_WGT;
	ret = mcfp_hw_rdma_create(&tctx->dma, tctx->addr, MCFP_RDMA_CUR_IN_WGT);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_hw_api_mcfp_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_mcfp_kunit_test_ctx *tctx = test->priv;
	int ret;

	tctx->dma.id = MCFP_RDMA_CUR_IN_Y;
	tctx->dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
	tctx->dma_input.format = DMA_INOUT_FORMAT_YUV422;
	tctx->dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	tctx->dma_input.sbwc_type = DMA_INPUT_SBWC_DISABLE;
	tctx->dma_input.msb = tctx->dma_input.bitwidth - 1;
	ret = mcfp_hw_s_rdma_init(&tctx->dma, &tctx->dma_input, &tctx->stripe_input,
		tctx->width, tctx->height, &tctx->sbwc_en, &tctx->payload_size,
		&tctx->strip_offset, &tctx->header_offset, &tctx->config);
	ret = mcfp_hw_s_rdma_addr(&tctx->dma, tctx->addr, 0, 1, 0,
		tctx->sbwc_en, tctx->payload_size, tctx->strip_offset, tctx->header_offset);
	KUNIT_EXPECT_EQ(test, 0, ret);

	tctx->dma.id = MCFP_RDMA_CUR_IN_WGT;
	ret = mcfp_hw_s_rdma_init(&tctx->dma, &tctx->dma_input, &tctx->stripe_input,
		tctx->width, tctx->height, &tctx->sbwc_en, &tctx->payload_size,
		&tctx->strip_offset, &tctx->header_offset, &tctx->config);
	ret = mcfp_hw_s_rdma_addr(&tctx->dma, tctx->addr, 0, 1, 0,
		tctx->sbwc_en, tctx->payload_size, tctx->strip_offset, tctx->header_offset);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static int pablo_hw_api_mcfp_kunit_test_init(struct kunit *test)
{
	test_ctx.width = 320, test_ctx.height = 240;
	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);
	test->priv = &test_ctx;

	return 0;
}

static void pablo_hw_api_mcfp_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.addr);
}

static struct kunit_case pablo_hw_api_mcfp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_mcfp_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcfp_hw_s_rdma_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcfp_hw_rdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcfp_hw_s_rdma_addr_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_mcfp_kunit_test_suite = {
	.name = "pablo-hw-api-mcfp-v10_1-kunit-test",
	.init = pablo_hw_api_mcfp_kunit_test_init,
	.exit = pablo_hw_api_mcfp_kunit_test_exit,
	.test_cases = pablo_hw_api_mcfp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_mcfp_kunit_test_suite);

MODULE_LICENSE("GPL");
