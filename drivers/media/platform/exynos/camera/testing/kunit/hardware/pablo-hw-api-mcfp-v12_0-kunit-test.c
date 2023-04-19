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

#include "hardware/api/is-hw-api-mcfp-v12.h"
#include "hardware/sfr/is-sfr-mcfp-v12_0.h"

#define MCFP_GET_F(base, R, F)		PMIO_GET_F(base, R, F)

static struct pablo_hw_api_mcfp_kunit_test_ctx {
	struct is_common_dma dma;
	struct param_dma_input dma_input;
	struct param_stripe_input stripe_input;
	u32 width, height;
	u32 sbwc_en, payload_size, strip_offset, header_offset;
	struct is_mcfp_config config;
	void *addr;
	struct pmio_config		pmio_config;
	struct pablo_mmio		*pmio;
	struct pmio_field		*pmio_fields;
	struct pmio_reg_seq		*pmio_reg_seqs;
} test_ctx;

static void pablo_hw_api_mcfp_dump_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_mcfp_kunit_test_ctx *tctx = test->priv;

	mcfp_hw_dump(tctx->pmio);
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

static void pablo_hw_api_mcfp_hw_s_clock_kunit_test(struct kunit *test)
{
	u32 set_val, expected_val;
	struct pablo_hw_api_mcfp_kunit_test_ctx *tctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tctx);

	mcfp_hw_s_clock(tctx->pmio, true);

	set_val = MCFP_GET_F(tctx->pmio, MCFP_R_IP_PROCESSING, MCFP_F_IP_PROCESSING);
	expected_val = 1;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);

	mcfp_hw_s_clock(tctx->pmio, false);

	set_val = MCFP_GET_F(tctx->pmio, MCFP_R_IP_PROCESSING, MCFP_F_IP_PROCESSING);
	expected_val = 0;
	KUNIT_EXPECT_EQ(test, set_val, expected_val);
}

static int __pablo_hw_api_mcfp_pmio_init(struct kunit *test)
{
	int ret;

	test_ctx.pmio_config.name = "mcfp";

	test_ctx.pmio_config.mmio_base = test_ctx.addr;

	test_ctx.pmio_config.cache_type = PMIO_CACHE_NONE;

	mcfp_hw_init_pmio_config(&test_ctx.pmio_config);

	test_ctx.pmio = pmio_init(NULL, NULL, &test_ctx.pmio_config);

	if (IS_ERR(test_ctx.pmio)) {
		err("failed to init mcfp PMIO: %d", PTR_ERR(test_ctx.pmio));
		return -ENOMEM;
	}

	ret = pmio_field_bulk_alloc(test_ctx.pmio, &test_ctx.pmio_fields,
			test_ctx.pmio_config.fields,
			test_ctx.pmio_config.num_fields);
	if (ret) {
		err("failed to alloc mcfp PMIO fields: %d", ret);
		pmio_exit(test_ctx.pmio);
		return ret;

	}

	test_ctx.pmio_reg_seqs = kunit_kzalloc(test, sizeof(struct pmio_reg_seq) * mcfp_hw_g_reg_cnt(), 0);
	if (!test_ctx.pmio_reg_seqs) {
		err("failed to alloc PMIO multiple write buffer");
		pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
		pmio_exit(test_ctx.pmio);
		return -ENOMEM;
	}

	return ret;
}

static void __pablo_hw_api_mcfp_pmio_deinit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.pmio_reg_seqs);
	pmio_field_bulk_free(test_ctx.pmio, test_ctx.pmio_fields);
	pmio_exit(test_ctx.pmio);
}

static int pablo_hw_api_mcfp_kunit_test_init(struct kunit *test)
{
	int ret;

	test_ctx.width = 320, test_ctx.height = 240;
	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);
	test->priv = &test_ctx;

	ret = __pablo_hw_api_mcfp_pmio_init(test);
	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void pablo_hw_api_mcfp_kunit_test_exit(struct kunit *test)
{
	__pablo_hw_api_mcfp_pmio_deinit(test);

	kunit_kfree(test, test_ctx.addr);
	memset(&test_ctx, 0, sizeof(struct pablo_hw_api_mcfp_kunit_test_ctx));
}

static struct kunit_case pablo_hw_api_mcfp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_mcfp_dump_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcfp_hw_s_rdma_init_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcfp_hw_rdma_create_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcfp_hw_s_rdma_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_mcfp_hw_s_clock_kunit_test),
	{},
};

struct kunit_suite pablo_hw_api_mcfp_kunit_test_suite = {
	.name = "pablo-hw-api-mcfp-v11_0-kunit-test",
	.init = pablo_hw_api_mcfp_kunit_test_init,
	.exit = pablo_hw_api_mcfp_kunit_test_exit,
	.test_cases = pablo_hw_api_mcfp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_mcfp_kunit_test_suite);

MODULE_LICENSE("GPL");
