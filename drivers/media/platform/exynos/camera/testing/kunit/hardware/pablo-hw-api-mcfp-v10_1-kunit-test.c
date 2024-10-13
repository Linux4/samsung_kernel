// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "hardware/api/is-hw-api-mcfp-v10_0.h"
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
	mcfp_hw_dump(test_ctx.addr);
}

static void pablo_hw_api_mcfp_hw_s_rdma_init_kunit_test(struct kunit *test)
{
	int ret;

	ret = mcfp_hw_s_rdma_init(&test_ctx.dma, &test_ctx.dma_input, &test_ctx.stripe_input,
		test_ctx.width, test_ctx.height, &test_ctx.sbwc_en, &test_ctx.payload_size,
		&test_ctx.strip_offset, &test_ctx.config);
	KUNIT_EXPECT_EQ(test, 0, ret);

	test_ctx.dma.id = MCFP_RDMA_CUR_IN_UV;
	ret = mcfp_hw_s_rdma_init(&test_ctx.dma, &test_ctx.dma_input, &test_ctx.stripe_input,
		test_ctx.width, test_ctx.height, &test_ctx.sbwc_en, &test_ctx.payload_size,
		&test_ctx.strip_offset, &test_ctx.config);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_hw_api_mcfp_hw_rdma_create_kunit_test(struct kunit *test)
{
	int ret;

	ret = mcfp_hw_rdma_create(&test_ctx.dma, test_ctx.addr, MCFP_RDMA_CUR_IN_Y);
	KUNIT_EXPECT_EQ(test, 0, ret);

	test_ctx.dma.id = MCFP_RDMA_CUR_IN_UV;
	ret = mcfp_hw_rdma_create(&test_ctx.dma, test_ctx.addr, MCFP_RDMA_CUR_IN_UV);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_hw_api_mcfp_hw_s_rdma_addr_kunit_test(struct kunit *test)
{
	int ret;

	ret = mcfp_hw_s_rdma_init(&test_ctx.dma, &test_ctx.dma_input, &test_ctx.stripe_input,
		test_ctx.width, test_ctx.height, &test_ctx.sbwc_en, &test_ctx.payload_size,
		&test_ctx.strip_offset, &test_ctx.config);
	ret = mcfp_hw_s_rdma_addr(&test_ctx.dma, test_ctx.addr, 0, 1, 0,
		test_ctx.sbwc_en, test_ctx.payload_size, test_ctx.strip_offset);
	KUNIT_EXPECT_EQ(test, 0, ret);

	test_ctx.dma.id = MCFP_RDMA_CUR_IN_UV;
	ret = mcfp_hw_s_rdma_init(&test_ctx.dma, &test_ctx.dma_input, &test_ctx.stripe_input,
		test_ctx.width, test_ctx.height, &test_ctx.sbwc_en, &test_ctx.payload_size,
		&test_ctx.strip_offset, &test_ctx.config);
	ret = mcfp_hw_s_rdma_addr(&test_ctx.dma, test_ctx.addr, 0, 1, 0,
		test_ctx.sbwc_en, test_ctx.payload_size, test_ctx.strip_offset);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_hw_api_mcfp_remosaic_tnr_kunit_test(struct kunit *test)
{
	int ret;

	test_ctx.width = 8160, test_ctx.height = 6120;

	test_ctx.stripe_input.total_count = 3;
	test_ctx.stripe_input.index = 0;

	ret = mcfp_hw_s_rdma_init(&test_ctx.dma, &test_ctx.dma_input, &test_ctx.stripe_input,
		test_ctx.width, test_ctx.height, &test_ctx.sbwc_en, &test_ctx.payload_size,
		&test_ctx.strip_offset, &test_ctx.config);
	KUNIT_EXPECT_EQ(test, 0, (int)test_ctx.strip_offset);

	test_ctx.stripe_input.start_pos_x = 2048;
	test_ctx.stripe_input.index = 1;

	ret = mcfp_hw_s_rdma_init(&test_ctx.dma, &test_ctx.dma_input, &test_ctx.stripe_input,
		test_ctx.width, test_ctx.height, &test_ctx.sbwc_en, &test_ctx.payload_size,
		&test_ctx.strip_offset, &test_ctx.config);
	KUNIT_EXPECT_EQ(test, 3072, (int)test_ctx.strip_offset);

	test_ctx.stripe_input.start_pos_x = 4608;
	test_ctx.stripe_input.index = 2;

	ret = mcfp_hw_s_rdma_init(&test_ctx.dma, &test_ctx.dma_input, &test_ctx.stripe_input,
	test_ctx.width, test_ctx.height, &test_ctx.sbwc_en, &test_ctx.payload_size,
	&test_ctx.strip_offset, &test_ctx.config);
	KUNIT_EXPECT_EQ(test, 6912, (int)test_ctx.strip_offset);
}

static int pablo_hw_api_mcfp_kunit_test_init(struct kunit *test)
{
	test_ctx.width = 320, test_ctx.height = 240;
	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);

	test_ctx.dma.id = MCFP_RDMA_CUR_IN_Y;
	test_ctx.dma_input.cmd = DMA_INPUT_COMMAND_ENABLE;
	test_ctx.dma_input.format = DMA_INOUT_FORMAT_YUV422;
	test_ctx.dma_input.bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	test_ctx.dma_input.sbwc_type = DMA_INPUT_SBWC_DISABLE;
	test_ctx.dma_input.msb = test_ctx.dma_input.bitwidth - 1;

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
	KUNIT_CASE(pablo_hw_api_mcfp_remosaic_tnr_kunit_test),
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
