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

#include "is-hw-common-dma.h"
#include "hardware/sfr/is-sfr-common-dma-v2.h"

static const u32 expected_bayer_sbwc_on_table[] = {
	[0 ... 7] = DMA_FMT_BAYER_MAX,
	[8] = DMA_FMT_U8BIT_PACK, /* DMA_INOUT_BIT_WIDTH_8BIT */
	[9] = DMA_FMT_S8BIT_PACK,
	[10] = DMA_FMT_U10BIT_PACK,
	[11] = DMA_FMT_S10BIT_PACK,
	[12] = DMA_FMT_U12BIT_PACK,
	[13] = DMA_FMT_S12BIT_PACK,
	[14] = DMA_FMT_U14BIT_PACK,
	[15] = DMA_FMT_S14BIT_PACK, /* DMA_INOUT_BIT_WIDTH_15BIT */
};

static const u32 expected_bayer_sbwc_off_table[][BAYER_SBWC_OFF] = {
	[0 ... 7] = { DMA_FMT_BAYER_MAX, DMA_FMT_BAYER_MAX, DMA_FMT_BAYER_MAX },
	[8] = { DMA_FMT_U8BIT_PACK,DMA_FMT_U8BIT_UNPACK_LSB_ZERO, DMA_FMT_U8BIT_UNPACK_MSB_ZERO },
	[9] = { DMA_FMT_S8BIT_PACK, DMA_FMT_S8BIT_UNPACK_LSB_ZERO, DMA_FMT_S8BIT_UNPACK_MSB_ZERO },
	[10] = { DMA_FMT_U10BIT_PACK, DMA_FMT_U10BIT_UNPACK_LSB_ZERO, DMA_FMT_U10BIT_UNPACK_MSB_ZERO },
	[11] = { DMA_FMT_S10BIT_PACK, DMA_FMT_S10BIT_UNPACK_LSB_ZERO, DMA_FMT_S10BIT_UNPACK_MSB_ZERO },
	[12] = { DMA_FMT_U12BIT_PACK, DMA_FMT_U12BIT_UNPACK_LSB_ZERO, DMA_FMT_U12BIT_UNPACK_MSB_ZERO },
	[13] = { DMA_FMT_S12BIT_PACK, DMA_FMT_S12BIT_UNPACK_LSB_ZERO, DMA_FMT_S12BIT_UNPACK_MSB_ZERO },
	[14] = { DMA_FMT_U14BIT_PACK, DMA_FMT_U14BIT_UNPACK_LSB_ZERO, DMA_FMT_U14BIT_UNPACK_MSB_ZERO },
	[15] = { DMA_FMT_S14BIT_PACK, DMA_FMT_S14BIT_UNPACK_LSB_ZERO, DMA_FMT_S14BIT_UNPACK_MSB_ZERO },
};

static struct pablo_hw_api_common_dma_v2_kunit_test_ctx {
	struct is_common_dma dma;
	u32 width, height;
	void *test_addr;

	struct pmio_config pmio_config;
	struct pablo_mmio *pmio;
} __ctx;

static void pablo_hw_api_common_dma_v2_get_payload_stride_kunit_test(struct kunit *test)
{
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;
	int ret;
	u32 comp_sbwc_en;
	u32 bit_depth;
	u32 width;
	u32 comp_64b_align;
	u32 block_width;
	u32 block_height;

	width = tctx->width;
	bit_depth = 10;

	/* losselss, 256x1, 32B Aign */
	comp_sbwc_en = COMP;
	block_width = 256;
	block_height = 1;
	comp_64b_align = 0;
	ret = is_hw_dma_get_payload_stride(comp_sbwc_en, bit_depth, width, comp_64b_align,
		0, block_width, block_height);
	KUNIT_EXPECT_EQ(test, 640, ret);

	/* lossy 256x1, 64B Aign */
	comp_sbwc_en = COMP_LOSS;
	block_width = 256;
	block_height = 1;
	comp_64b_align = 1;
	ret = is_hw_dma_get_payload_stride(comp_sbwc_en, bit_depth, width, comp_64b_align,
		0, block_width, block_height);
	KUNIT_EXPECT_EQ(test, 384, ret);

	/* lossless 32x4, 64B Align */
	comp_sbwc_en = COMP;
	block_width = 32;
	block_height = 4;
	comp_64b_align = 1;
	ret = is_hw_dma_get_payload_stride(comp_sbwc_en, bit_depth, width, comp_64b_align,
		0, block_width, block_height);
	KUNIT_EXPECT_EQ(test, 1920, ret);

	/* lossy, 32x4, 64B Align */
	comp_sbwc_en = COMP_LOSS;
	block_width = 32;
	block_height = 4;
	comp_64b_align = 1;
	ret = is_hw_dma_get_payload_stride(comp_sbwc_en, bit_depth, width, comp_64b_align,
		0, block_width, block_height);
	KUNIT_EXPECT_EQ(test, 1280, ret);
}

static void pablo_hw_api_common_dma_v2_comp_sbwc_en_kunit_test(struct kunit *test)
{
	int ret, ret_comp_en, sbwc_type;
	u32 comp_64b_align;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	sbwc_type = DMA_INPUT_SBWC_DISABLE;
	ret_comp_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	KUNIT_EXPECT_EQ(test, ret_comp_en, 0);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_sbwc_en, ret_comp_en);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_32B;
	ret_comp_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	KUNIT_EXPECT_EQ(test, (u32)0, comp_64b_align);
	KUNIT_EXPECT_EQ(test, ret_comp_en, 1);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_sbwc_en, ret_comp_en);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	sbwc_type = DMA_INPUT_SBWC_LOSSYLESS_64B;
	ret_comp_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	KUNIT_EXPECT_EQ(test, (u32)1, comp_64b_align);
	KUNIT_EXPECT_EQ(test, ret_comp_en, 1);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_sbwc_en, ret_comp_en);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	sbwc_type = DMA_INPUT_SBWC_LOSSY_32B;
	ret_comp_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	KUNIT_EXPECT_EQ(test, (u32)0, comp_64b_align);
	KUNIT_EXPECT_EQ(test, ret_comp_en, 2);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_sbwc_en, ret_comp_en);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	sbwc_type = DMA_INPUT_SBWC_LOSSY_64B;
	ret_comp_en = is_hw_dma_get_comp_sbwc_en(sbwc_type, &comp_64b_align);
	KUNIT_EXPECT_EQ(test, (u32)1, comp_64b_align);
	KUNIT_EXPECT_EQ(test, ret_comp_en, 2);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_sbwc_en, ret_comp_en);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void dma_v2_bayer_format_kunit_test(struct kunit *test,
	u32 memory_bitwidth, u32 hw_format, u32 sbwc_en, bool is_msb, u32 pos, u32 *table)
{
	int ret;
	u32 byr_format;
	u32 expect_fmt = DMA_FMT_U8BIT_PACK;
	u32 pixel_size = DMA_INOUT_BIT_WIDTH_32BIT;
	u32 offset = 0;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	if (hw_format == DMA_INOUT_FORMAT_DRCGAIN
		|| hw_format == DMA_INOUT_FORMAT_SVHIST
		|| hw_format == DMA_INOUT_FORMAT_SEGCONF) {
		is_hw_dma_get_bayer_format(memory_bitwidth, pixel_size, hw_format,
				sbwc_en, is_msb, &byr_format);
		KUNIT_EXPECT_EQ(test, byr_format, expect_fmt);

		tctx->dma.available_bayer_format_map = BIT_MASK(byr_format);
		ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, byr_format, DMA_FMT_BAYER);
		KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
		tctx->dma.available_bayer_format_map = 0;
	} else {
		for ( pixel_size = 0; pixel_size <= DMA_INOUT_BIT_WIDTH_15BIT; pixel_size++) {
			if (sbwc_en)
				offset = pixel_size;
			else
				offset = pixel_size*BAYER_SBWC_OFF + pos;
			expect_fmt = *(table + offset);
			is_hw_dma_get_bayer_format(memory_bitwidth, pixel_size, hw_format,
					sbwc_en, is_msb, &byr_format);
			KUNIT_EXPECT_EQ(test, byr_format, expect_fmt);

			if (byr_format != DMA_FMT_BAYER_MAX) {
				tctx->dma.available_bayer_format_map = BIT_MASK(byr_format);
				ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, byr_format, DMA_FMT_BAYER);
				KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
				tctx->dma.available_bayer_format_map = 0;
			}
		}
	}
}

static void pablo_hw_api_common_dma_v2_bayer_format_kunit_test(struct kunit *test)
{
	/* exception */
	dma_v2_bayer_format_kunit_test(test, 0, DMA_INOUT_FORMAT_DRCGAIN,
		false, false, BAYER_PACK, 0);

	/* sbwc_en == true */
	dma_v2_bayer_format_kunit_test(test, 0, DMA_INOUT_FORMAT_BAYER,
		true, false, BAYER_PACK, (u32 *)expected_bayer_sbwc_on_table);

	/* packed, sbwc_en == false */
	dma_v2_bayer_format_kunit_test(test, DMA_INOUT_BIT_WIDTH_16BIT, DMA_INOUT_FORMAT_BAYER_PACKED,
		false, false, BAYER_PACK, (u32 *)expected_bayer_sbwc_off_table);

	/* unpack, lsb, sbwc_en == false */
	dma_v2_bayer_format_kunit_test(test, DMA_INOUT_BIT_WIDTH_16BIT, DMA_INOUT_FORMAT_BAYER,
		false, false, BAYER_UNPACK_LSB, (u32 *)expected_bayer_sbwc_off_table);

	/* unpack, msb, sbwc_en == false */
	dma_v2_bayer_format_kunit_test(test, DMA_INOUT_BIT_WIDTH_16BIT, DMA_INOUT_FORMAT_BAYER,
		false, true, BAYER_UNPACK_MSB, (u32 *)expected_bayer_sbwc_off_table);
}

static void pablo_hw_api_common_dma_v2_rgb_format_kunit_test(struct kunit *test)
{
	int ret, rgb_format;
	u32 bitwidth;
	u32 plane = 0;
	u32 order;
	int expect_fmt;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	/* 32bit bitwidth test */
	bitwidth = DMA_INOUT_BIT_WIDTH_32BIT;
	order = DMA_INOUT_ORDER_ARGB;
	expect_fmt = DMA_FMT_RGB_ARGB8888;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	tctx->dma.available_rgb_format_map = BIT_MASK(rgb_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, rgb_format, DMA_FMT_RGB);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	order = DMA_INOUT_ORDER_BGRA;
	expect_fmt = DMA_FMT_RGB_BGRA8888;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	tctx->dma.available_rgb_format_map = BIT_MASK(rgb_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, rgb_format, DMA_FMT_RGB);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	order = DMA_INOUT_ORDER_RGBA;
	expect_fmt = DMA_FMT_RGB_RGBA8888;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	tctx->dma.available_rgb_format_map = BIT_MASK(rgb_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, rgb_format, DMA_FMT_RGB);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	order = DMA_INOUT_ORDER_ABGR;
	expect_fmt = DMA_FMT_RGB_ABGR8888;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	tctx->dma.available_rgb_format_map = BIT_MASK(rgb_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, rgb_format, DMA_FMT_RGB);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	/* invalid order */
	order = DMA_INOUT_ORDER_GR_BG;
	expect_fmt = -EINVAL;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	/* 10bit bitwidth test*/
	bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	order = DMA_INOUT_ORDER_ARGB;
	expect_fmt = DMA_FMT_RGB_ARGB1010102;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	tctx->dma.available_rgb_format_map = BIT_MASK(rgb_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, rgb_format, DMA_FMT_RGB);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	order = DMA_INOUT_ORDER_BGRA;
	expect_fmt = DMA_FMT_RGB_BGRA1010102;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	tctx->dma.available_rgb_format_map = BIT_MASK(rgb_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, rgb_format, DMA_FMT_RGB);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	order = DMA_INOUT_ORDER_RGBA;
	expect_fmt = DMA_FMT_RGB_RGBA1010102;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	tctx->dma.available_rgb_format_map = BIT_MASK(rgb_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, rgb_format, DMA_FMT_RGB);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	order = DMA_INOUT_ORDER_ABGR;
	expect_fmt = DMA_FMT_RGB_ABGR1010102;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	tctx->dma.available_rgb_format_map = BIT_MASK(rgb_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, rgb_format, DMA_FMT_RGB);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	/* invalid order test */
	order = DMA_INOUT_ORDER_GR_BG;
	expect_fmt = -EINVAL;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);

	/* invalid bitwidth test */
	bitwidth = DMA_INOUT_BIT_WIDTH_11BIT;
	order = DMA_INOUT_ORDER_ARGB;
	expect_fmt = -EINVAL;
	rgb_format = is_hw_dma_get_rgb_format(bitwidth, plane, order);
	KUNIT_EXPECT_EQ(test, rgb_format, expect_fmt);
}

static void pablo_hw_api_common_dma_v2_yuv_format_kunit_test(struct kunit *test)
{
	int ret, yuv_format;
	u32 bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	u32 format = DMA_INOUT_FORMAT_YUV420;
	u32 plane = 2;
	u32 order = DMA_INOUT_ORDER_CbCr;
	int expect_fmt = DMA_FMT_YUV420_2P_UFIRST;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 3;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV420_3P;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV420_2P_VFIRST;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = -EINVAL;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 1;
	order = DMA_INOUT_ORDER_CrYCbY;
	expect_fmt = DMA_FMT_YUV422_1P_VYUY;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 1;
	order = DMA_INOUT_ORDER_CbYCrY;
	expect_fmt = DMA_FMT_YUV422_1P_UYVY;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 1;
	order = DMA_INOUT_ORDER_YCrYCb;
	expect_fmt = DMA_FMT_YUV422_1P_YVYU;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 1;
	order = DMA_INOUT_ORDER_YCbYCr;
	expect_fmt = DMA_FMT_YUV422_1P_YUYV;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV422_2P_UFIRST;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV422_2P_VFIRST;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 3;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV422_3P;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 1;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_1P;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 3;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_3P;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_RGB;
	plane = 3;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_3P;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	/* bitwidth : 10 bit */
	bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV420_2P_UFIRST_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 4;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV420_2P_UFIRST_8P2;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV420_2P_VFIRST_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 4;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV420_2P_VFIRST_8P2;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV422_2P_UFIRST_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 4;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV422_2P_UFIRST_8P2;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV422_2P_VFIRST_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 4;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV422_2P_VFIRST_8P2;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 1;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_1P_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 3;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_3P_PACKED10;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	/* bitwidth : 16 bit */
	bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV420_2P_UFIRST_P010;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV420;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV420_2P_VFIRST_P010;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CbCr;
	expect_fmt = DMA_FMT_YUV422_2P_UFIRST_P210;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV422;
	plane = 2;
	order = DMA_INOUT_ORDER_CrCb;
	expect_fmt = DMA_FMT_YUV422_2P_VFIRST_P210;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 1;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_1P_UNPACKED;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;

	format = DMA_INOUT_FORMAT_YUV444;
	plane = 3;
	order = DMA_INOUT_ORDER_NO;
	expect_fmt = DMA_FMT_YUV444_3P_UNPACKED;
	yuv_format = is_hw_dma_get_yuv_format(bitwidth, format, plane, order);
	KUNIT_EXPECT_EQ(test, yuv_format, expect_fmt);

	tctx->dma.available_yuv_format_map = BIT_MASK(yuv_format);
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_format, yuv_format, DMA_FMT_YUV);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
	tctx->dma.available_rgb_format_map = 0;
}

static void pablo_hw_api_common_dma_v2_set_msb_align_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_msb_align, 0);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_msb_align, 1);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_set_comp_64b_align_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_64b_align, 0);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_64b_align, 1);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_set_comp_error_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_error, COMP_ERROR_MODE_USE_0, 0);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_error, COMP_ERROR_MODE_USE_VALUE, 0xFFFF);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_error, COMP_ERROR_MODE_USE_LAST_PIXEL, 0);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_set_comp_rate_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_rate, 0);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_set_comp_quality_kunit_test(struct kunit *test)
{
	int ret;
	enum lossy_comp_quality_type type;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	/* normal test */
	for (type = LOSSY_COMP_QUALITY_TYPE_DEFAULT; type < LOSSY_COMP_QUALITY_TYPE_MAX; type++) {
		ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_quality, type);
		KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
	}

	/* error test */
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_comp_quality, type);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_ERROR);
}

static void pablo_hw_api_common_dma_v2_set_mono_mode_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_mono_mode, 0);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_mono_mode, 1);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_set_auto_flush_en_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_auto_flush_en, 0);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_auto_flush_en, 1);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_set_size_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_size, 4032, 3024);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_set_img_stride_kunit_test(struct kunit *test)
{
	int ret;
	u32 memory_bitwidth, pixel_size, hw_format, width, align, img_stride;
	bool is_image;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;
	pixel_size = DMA_INOUT_BIT_WIDTH_8BIT;
	hw_format = DMA_INOUT_FORMAT_BAYER;
	width = 4032;
	align = 16;
	is_image = true;

	/* aligned & packed */
	img_stride = is_hw_dma_get_img_stride(memory_bitwidth, pixel_size, hw_format,
		width, align, is_image);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_img_stride, img_stride, img_stride, img_stride);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	/* aligned & unpacked */
	memory_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	pixel_size = DMA_INOUT_BIT_WIDTH_10BIT;
	img_stride = is_hw_dma_get_img_stride(memory_bitwidth, pixel_size, hw_format,
		width, align, is_image);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_img_stride, img_stride, img_stride, img_stride);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	/* not alinged & packed */
	is_image = false;
	img_stride = is_hw_dma_get_img_stride(memory_bitwidth, pixel_size, hw_format,
		width, align, is_image);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_img_stride, img_stride, img_stride, img_stride);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_set_header_stride_kunit_test(struct kunit *test)
{
	int ret;
	u32 width, block_width, align, header_stride;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	width = tctx->width;
	block_width = 256;
	align = 16;
	header_stride = is_hw_dma_get_header_stride(width, block_width, align);
	KUNIT_EXPECT_EQ(test, 16, header_stride);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_header_stride, header_stride, header_stride);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_max_mo_kunit_test(struct kunit *test)
{
	int ret;
	u32 set_mo, get_mo;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	set_mo = 32;
	ret = CALL_DMA_OPS(&tctx->dma, dma_set_max_mo, set_mo);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	get_mo = 0;
	ret = CALL_DMA_OPS(&tctx->dma, dma_get_max_mo, &get_mo);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
	KUNIT_EXPECT_EQ(test, set_mo, get_mo);
}

static void pablo_hw_api_common_dma_v2_set_line_gap_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_line_gap, 45);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_set_bus_info_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_bus_info, 0);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_set_addr_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;
	dma_addr_t addr = 0xBEEEEEEEF;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_img_addr, &addr, 1, 0, 1);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_header_addr, &addr, 1, 0, 1);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_votf_enable_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_votf_enable, 1, 1);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static void pablo_hw_api_common_dma_v2_enable_kunit_test(struct kunit *test)
{
	int ret;
	u32 set_enable, get_enable;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	set_enable = 0;
	ret = CALL_DMA_OPS(&tctx->dma, dma_enable, set_enable);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	get_enable = 1;
	ret = CALL_DMA_OPS(&tctx->dma, dma_get_enable, &get_enable);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
	KUNIT_EXPECT_EQ(test, set_enable, get_enable);

	set_enable = 1;
	ret = CALL_DMA_OPS(&tctx->dma, dma_enable, set_enable);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);

	get_enable = 0;
	ret = CALL_DMA_OPS(&tctx->dma, dma_get_enable, &get_enable);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
	KUNIT_EXPECT_EQ(test, set_enable, get_enable);
}

static void pablo_hw_api_common_dma_v2_get_crc_test(struct kunit *test)
{
	int ret;
	u32 crc[3];
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_get_img_crc, &crc[0], &crc[1], &crc[2]);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
	KUNIT_EXPECT_EQ(test, crc[0], 0);
	KUNIT_EXPECT_EQ(test, crc[1], 0);
	KUNIT_EXPECT_EQ(test, crc[2], 0);

	ret = CALL_DMA_OPS(&tctx->dma, dma_get_header_crc, &crc[0], &crc[1]);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
	KUNIT_EXPECT_EQ(test, crc[0], 0);
	KUNIT_EXPECT_EQ(test, crc[1], 0);
}

static void pablo_hw_api_common_dma_v2_get_mon_test(struct kunit *test)
{
	int ret;
	u32 mon_status = 1;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_get_mon_status, &mon_status);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
	KUNIT_EXPECT_EQ(test, mon_status, 0);
}

static void pablo_hw_api_common_dma_v2_print_info_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_print_info, 0);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);
}

static void pablo_hw_api_common_dma_v2_set_cache_32b_pa_kunit_test(struct kunit *test)
{
	int ret;
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	ret = CALL_DMA_OPS(&tctx->dma, dma_set_cache_32b_pa, 0);
	KUNIT_EXPECT_EQ(test, ret, DMA_OPS_SUCCESS);
}

static struct kunit_case pablo_hw_api_common_pmio_dma_v2_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_common_dma_v2_get_payload_stride_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_comp_sbwc_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_bayer_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_rgb_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_yuv_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_msb_align_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_comp_64b_align_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_comp_error_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_comp_rate_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_comp_quality_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_cache_32b_pa_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_mono_mode_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_auto_flush_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_img_stride_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_header_stride_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_max_mo_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_line_gap_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_bus_info_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_votf_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_get_crc_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_get_mon_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_print_info_kunit_test),
	{},
};

static void kunit_pmio_init(struct kunit *test)
{
	__ctx.pmio_config.name = "kunit_common_dma";

	__ctx.pmio_config.num_corexs = 2;
	__ctx.pmio_config.corex_stride = 0x8000;

	__ctx.pmio_config.max_register = 0x7FFC;
	__ctx.pmio_config.num_reg_defaults_raw = (__ctx.pmio_config.max_register >> 2) + 1;
	__ctx.pmio_config.mmio_base = __ctx.test_addr;

	__ctx.pmio_config.cache_type = PMIO_CACHE_NONE;
	__ctx.pmio_config.phys_base = 0xBEEEEEEF;

	__ctx.pmio = pmio_init(NULL, NULL, &__ctx.pmio_config);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, __ctx.pmio);
}

static int pablo_hw_api_common_pmio_dma_v2_kunit_test_init(struct kunit *test)
{
	int ret;

	memset(&__ctx, 0, sizeof(struct pablo_hw_api_common_dma_v2_kunit_test_ctx));

	__ctx.width = 320;
	__ctx.height = 240;
	__ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __ctx.test_addr);

	kunit_pmio_init(test);

	ret = pmio_dma_create(&__ctx.dma, __ctx.pmio, 0, "kunit_pmio_dma", 0, 0, 0);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);

	ret = pmio_dma_set_ops(&__ctx.dma);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);

	ret = CALL_DMA_OPS(&__ctx.dma, dma_set_corex_id, COREX_DIRECT);
	KUNIT_EXPECT_EQ(test, ret, 0);

	test->priv = &__ctx;

	return 0;
}

static void pablo_hw_api_common_pmio_dma_v2_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tctx);
	pmio_exit(tctx->pmio);
	kunit_kfree(test, tctx->test_addr);
}

struct kunit_suite pablo_hw_api_common_pmio_dma_v2_kunit_test_suite = {
	.name = "pablo-hw-api-common-pmio_dma_v2-kunit-test",
	.init = pablo_hw_api_common_pmio_dma_v2_kunit_test_init,
	.exit = pablo_hw_api_common_pmio_dma_v2_kunit_test_exit,
	.test_cases = pablo_hw_api_common_pmio_dma_v2_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_common_pmio_dma_v2_kunit_test_suite);

static struct kunit_case pablo_hw_api_common_dma_v2_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_api_common_dma_v2_get_payload_stride_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_comp_sbwc_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_bayer_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_rgb_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_yuv_format_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_msb_align_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_comp_64b_align_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_comp_error_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_comp_rate_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_comp_quality_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_cache_32b_pa_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_mono_mode_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_auto_flush_en_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_size_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_img_stride_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_header_stride_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_max_mo_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_line_gap_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_bus_info_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_set_addr_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_votf_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_enable_kunit_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_get_crc_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_get_mon_test),
	KUNIT_CASE(pablo_hw_api_common_dma_v2_print_info_kunit_test),
	{},
};

static int pablo_hw_api_common_dma_v2_kunit_test_init(struct kunit *test)
{
	int ret;

	memset(&__ctx, 0, sizeof(struct pablo_hw_api_common_dma_v2_kunit_test_ctx));

	__ctx.width = 320;
	__ctx.height = 240;
	__ctx.test_addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, __ctx.test_addr);

	ret = is_hw_dma_create(&__ctx.dma, __ctx.test_addr, 0, "kunit_dma", 0, 0, 0);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);

	ret = is_hw_dma_set_ops(&__ctx.dma);
	KUNIT_EXPECT_EQ(test, DMA_OPS_SUCCESS, ret);

	ret = CALL_DMA_OPS(&__ctx.dma, dma_set_corex_id, COREX_DIRECT);
	KUNIT_EXPECT_EQ(test, ret, 0);

	test->priv = &__ctx;

	return 0;
}

static void pablo_hw_api_common_dma_v2_kunit_test_exit(struct kunit *test)
{
	struct pablo_hw_api_common_dma_v2_kunit_test_ctx *tctx = test->priv;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tctx);
	kunit_kfree(test, tctx->test_addr);
}

struct kunit_suite pablo_hw_api_common_dma_v2_kunit_test_suite = {
	.name = "pablo-hw-api-common-dma_v2-kunit-test",
	.init = pablo_hw_api_common_dma_v2_kunit_test_init,
	.exit = pablo_hw_api_common_dma_v2_kunit_test_exit,
	.test_cases = pablo_hw_api_common_dma_v2_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_api_common_dma_v2_kunit_test_suite);

MODULE_LICENSE("GPL");
