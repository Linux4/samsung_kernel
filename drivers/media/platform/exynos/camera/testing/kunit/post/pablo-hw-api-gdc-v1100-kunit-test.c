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
#include "gdc/pablo-gdc.h"
#include "gdc/pablo-hw-api-gdc.h"
#include "gdc/pablo-hw-reg-gdc-v1100.h"
#include "is-common-enum.h"
#include "is-type.h"

#define MAX_SUPPORT_SBWC_FMT 4
#define GDC_GET_F(base, R, F) PMIO_GET_F((base)->pmio, R, F)
#define GDC_GET_R(base, R) PMIO_GET_R((base)->pmio, R)

static struct cameraepp_hw_api_gdc_kunit_test_ctx {
	void *addr;
	struct gdc_dev *gdc;
	struct gdc_crop_param *crop_param;
} test_ctx;

static const struct gdc_variant gdc_variant = {
	.limit_input = {
		.min_w = 96,
		.min_h = 64,
		.max_w = 16384,
		.max_h = 12288,
	},
	.limit_output = {
		.min_w = 96,
		.min_h = 64,
		.max_w = 16384,
		.max_h = 12288,
	},
	.version = 0x11000000,
};

static u32 support_sbwc_fmt_array[MAX_SUPPORT_SBWC_FMT] = {
	V4L2_PIX_FMT_NV12M_SBWCL_32_8B,
	V4L2_PIX_FMT_NV12M_SBWCL_32_10B,
	V4L2_PIX_FMT_NV12M_SBWCL_64_8B,
	V4L2_PIX_FMT_NV12M_SBWCL_64_10B,
};

static const struct gdc_fmt gdc_formats[] = {
	{
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.cfg_val	= GDC_CFG_FMT_YCBCR420_2P,
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.cfg_val	= GDC_CFG_FMT_YCRCB420_2P,
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.cfg_val	= GDC_CFG_FMT_YCBCR420_2P,
		.bitperpixel	= { 12 },
		.num_planes = 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:0 contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.cfg_val	= GDC_CFG_FMT_YCRCB420_2P,
		.bitperpixel	= { 12 },
		.num_planes = 1,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, YCrYCb",
		.pixelformat	= V4L2_PIX_FMT_YVYU,
		.cfg_val	= GDC_CFG_FMT_YVYU,
		.bitperpixel	= { 16 },
		.num_planes = 1,
		.num_comp	= 1,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.cfg_val	= GDC_CFG_FMT_YUYV,
		.bitperpixel	= { 16 },
		.num_planes = 1,
		.num_comp	= 1,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.cfg_val	= GDC_CFG_FMT_YCBCR422_2P,
		.bitperpixel	= { 16 },
		.num_planes = 1,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.cfg_val	= GDC_CFG_FMT_YCRCB422_2P,
		.bitperpixel	= { 16 },
		.num_planes = 1,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16M,
		.cfg_val	= GDC_CFG_FMT_YCBCR422_2P,
		.bitperpixel	= { 8, 8 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV 4:2:2 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV61M,
		.cfg_val	= GDC_CFG_FMT_YCRCB422_2P,
		.bitperpixel	= { 8, 8 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "P010_16B",
		.pixelformat	= V4L2_PIX_FMT_NV12M_P010,
		.cfg_val	= GDC_CFG_FMT_P010_16B_2P,
		.bitperpixel	= { 16, 16 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "P010_16B",
		.pixelformat	= V4L2_PIX_FMT_NV21M_P010,
		.cfg_val	= GDC_CFG_FMT_P010_16B_2P,
		.bitperpixel	= { 16, 16 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "P210_16B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.cfg_val	= GDC_CFG_FMT_P210_16B_2P,
		.bitperpixel	= { 16, 16 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "P210_16B",
		.pixelformat	= V4L2_PIX_FMT_NV61M_P210,
		.cfg_val	= GDC_CFG_FMT_P210_16B_2P,
		.bitperpixel	= { 16, 16 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV422 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV16M_S10B,
		.cfg_val	= GDC_CFG_FMT_YCRCB422_8P2_2P,
		.bitperpixel	= { 8, 8 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV422 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV61M_S10B,
		.cfg_val	= GDC_CFG_FMT_YCRCB422_8P2_2P,
		.bitperpixel	= { 8, 8 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV420 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV12M_S10B,
		.cfg_val	= GDC_CFG_FMT_YCRCB420_8P2_2P,
		.bitperpixel	= { 8, 8 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "YUV420 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV21M_S10B,
		.cfg_val	= GDC_CFG_FMT_YCRCB420_8P2_2P,
		.bitperpixel	= { 8, 8 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
	}, {
		.name		= "NV12M SBWC LOSSY 8bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_8B,
		.cfg_val	= GDC_CFG_FMT_NV12M_SBWC_LOSSY_8B,
		.bitperpixel	= { 8, 4 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "NV12M SBWC LOSSY 10bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_10B,
		.cfg_val	= GDC_CFG_FMT_NV12M_SBWC_LOSSY_10B,
		.bitperpixel	= { 16, 8 },
		.num_planes	= 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "NV12M SBWC LOSSY 32B Align 8bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_32_8B,
		.cfg_val	= GDC_CFG_FMT_NV12M_SBWC_LOSSY_8B,
		.bitperpixel	= { 8, 4 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "NV12M SBWC LOSSY 32B Align 10bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_32_10B,
		.cfg_val	= GDC_CFG_FMT_NV12M_SBWC_LOSSY_10B,
		.bitperpixel	= { 16, 8 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "NV12M SBWC LOSSY 64B Align 8bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_64_8B,
		.cfg_val	= GDC_CFG_FMT_NV12M_SBWC_LOSSY_8B,
		.bitperpixel	= { 8, 4 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "NV12M SBWC LOSSY 64B Align 10bit",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_64_10B,
		.cfg_val	= GDC_CFG_FMT_NV12M_SBWC_LOSSY_10B,
		.bitperpixel	= { 16, 8 },
		.num_planes = 2,
		.num_comp	= 2,
		.h_shift	= 1,
		.v_shift	= 1,
	},
};

/* Define the test cases. */
static void camerapp_hw_api_gdc_get_size_constraints_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	const struct gdc_variant *constraints;
	u32 val;

	*(u32 *)(tctx->addr + GDC_R_IP_VERSION) = gdc_variant.version;

	constraints = camerapp_hw_gdc_get_size_constraints(gdc->pmio);
	val = camerapp_hw_gdc_get_ver(gdc->pmio);

	KUNIT_EXPECT_EQ(test, constraints->limit_input.min_w, gdc_variant.limit_input.min_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.min_h, gdc_variant.limit_input.min_h);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.max_w, gdc_variant.limit_input.max_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_input.max_h, gdc_variant.limit_input.max_h);
	KUNIT_EXPECT_EQ(test, constraints->limit_output.min_w, gdc_variant.limit_output.min_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_output.min_h, gdc_variant.limit_output.min_h);
	KUNIT_EXPECT_EQ(test, constraints->limit_output.max_w, gdc_variant.limit_output.max_w);
	KUNIT_EXPECT_EQ(test, constraints->limit_output.max_h, gdc_variant.limit_output.max_h);
	KUNIT_EXPECT_EQ(test, constraints->version, gdc_variant.version);
	KUNIT_EXPECT_EQ(test, constraints->version, val);
}

static void camerapp_hw_api_gdc_sfr_dump_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	camerapp_gdc_sfr_dump(gdc->regs_base);
}

static void camerapp_hw_api_gdc_start_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct c_loader_buffer clb;
	u32 val;

	/* APB-DIRECT */
	*(u32 *)(tctx->addr + GDC_R_IP_PROCESSING) = 0x1;

	camerapp_hw_gdc_start(gdc->pmio, &clb);

	val = GDC_GET_R(gdc, GDC_R_CMDQ_ADD_TO_QUEUE_0);
	KUNIT_EXPECT_EQ(test, val, 1);
	val = GDC_GET_R(gdc, GDC_R_IP_PROCESSING);
	KUNIT_EXPECT_EQ(test, val, 0);

	/* C-LOADER */
	*(u32 *)(tctx->addr + GDC_R_IP_PROCESSING) = 0x1;
	*(u32 *)(tctx->addr + GDC_R_CMDQ_ADD_TO_QUEUE_0) = 0x0;

	clb.header_dva = 0xDEADDEAD;
	clb.num_of_headers = 5;
	camerapp_hw_gdc_start(gdc->pmio, &clb);

	val = GDC_GET_R(gdc, GDC_R_CMDQ_QUE_CMD_H);
	KUNIT_EXPECT_EQ(test, val, clb.header_dva >> 4);
	val = GDC_GET_R(gdc, GDC_R_CMDQ_QUE_CMD_M);
	KUNIT_EXPECT_EQ(test, val, (1 << 12) | clb.num_of_headers);
	val = GDC_GET_R(gdc, GDC_R_CMDQ_ADD_TO_QUEUE_0);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_R(gdc, GDC_R_IP_PROCESSING);
	KUNIT_EXPECT_EQ(test, val, (u32)0);
}

static void camerapp_hw_api_gdc_stop_kunit_test(struct kunit *test)
{
}

static void camerapp_hw_api_gdc_sw_reset_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val1, val2;

	val1 = camerapp_hw_gdc_sw_reset(gdc->pmio);
	val2 = GDC_GET_R(gdc, GDC_R_SW_RESET);
	KUNIT_EXPECT_EQ(test, val1, (u32)10001);
	KUNIT_EXPECT_EQ(test, val2, (u32)1);
}

static void camerapp_hw_gdc_get_intr_status_and_clear_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val0, c0, c1;
	u32 int0 = 0xAAAABBBB;
	u32 int1 = 0xCCCCDDDD;

	*(u32 *)(tctx->addr + GDC_R_INT_REQ_INT0) = int0;
	*(u32 *)(tctx->addr + GDC_R_INT_REQ_INT1) = int1;

	val0 = camerapp_hw_gdc_get_intr_status_and_clear(gdc->pmio);

	c0 = GDC_GET_R(gdc, GDC_R_INT_REQ_INT0_CLEAR);
	c1 = GDC_GET_R(gdc, GDC_R_INT_REQ_INT1_CLEAR);

	KUNIT_EXPECT_EQ(test, val0, int0);
	KUNIT_EXPECT_EQ(test, c0, int0);
	KUNIT_EXPECT_EQ(test, c1, int1);
}

static void camerapp_hw_gdc_get_fs_fe_kunit_test(struct kunit *test)
{
	u32 fs = GDC_INT_FRAME_START, fe = GDC_INT_FRAME_END;
	u32 val1, val2;

	val1 = camerapp_hw_gdc_get_int_frame_start();
	val2 = camerapp_hw_gdc_get_int_frame_end();

	KUNIT_EXPECT_EQ(test, fs, val1);
	KUNIT_EXPECT_EQ(test, fe, val2);
}

static int __find_supported_sbwc_fmt(u32 fmt)
{
	int i, ret = -EINVAL;

	for (i = 0 ; i < MAX_SUPPORT_SBWC_FMT; i++) {
		if (support_sbwc_fmt_array[i] == fmt) {
			ret = 0;
			break;
		}
	}

	return ret;
}

static void camerapp_hw_gdc_get_sbwc_const_kunit_test(struct kunit *test)
{
	u32 width, height, pixformat;
	int i, val, result;

	width = 320;
	height = 240;

	for (i = 0; i < ARRAY_SIZE(gdc_formats); ++i) {
		pixformat = gdc_formats[i].pixelformat;
		result = __find_supported_sbwc_fmt(pixformat);
		val = camerapp_hw_get_sbwc_constraint(pixformat, width, height, 0);
		KUNIT_EXPECT_EQ(test, result, val);
	}
}

static void camerapp_hw_gdc_has_comp_header_kunit_test(struct kunit *test)
{
	bool val;

	val = camerapp_hw_gdc_has_comp_header(0);

	KUNIT_EXPECT_EQ(test, val, (bool)true);
}

static void camerapp_hw_get_comp_buf_size_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_frame frame;
	struct gdc_dev *gdc = tctx->gdc;
	u32 width, height, pixfmt, plane;
	int val1, val2;

	width = 320;
	height = 240;
	pixfmt = V4L2_PIX_FMT_NV12M_SBWCL_32_8B;
	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame,
			width, height, pixfmt, plane, GDC_SBWC_SIZE_ALL);
		val2 = plane ?
			(u32)(SBWCL_32_CBCR_SIZE(width, height) + SBWCL_CBCR_HEADER_SIZE(width, height)) :
			(u32)(SBWCL_32_Y_SIZE(width, height) + SBWCL_Y_HEADER_SIZE(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	pixfmt = V4L2_PIX_FMT_NV12M_SBWCL_64_8B;
	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame,
			width, height, pixfmt, plane, GDC_SBWC_SIZE_ALL);
		val2 = plane ?
			(u32)(SBWCL_64_CBCR_SIZE(width, height) + SBWCL_CBCR_HEADER_SIZE(width, height)) :
			(u32)(SBWCL_64_Y_SIZE(width, height) + SBWCL_Y_HEADER_SIZE(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame,
			width, height, pixfmt, plane, GDC_SBWC_SIZE_PAYLOAD);
		val2 = plane ?
			(u32)(SBWCL_64_CBCR_SIZE(width, height)) : (u32)(SBWCL_64_Y_SIZE(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	for (plane = 0; plane < 2; plane++) {
		val1 = camerapp_hw_get_comp_buf_size(gdc, &frame,
			width, height, pixfmt, plane, GDC_SBWC_SIZE_HEADER);
		val2 = plane ?
			(u32)(SBWCL_CBCR_HEADER_SIZE(width, height)) :
			(u32)(SBWCL_Y_HEADER_SIZE(width, height));
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	pixfmt = V4L2_PIX_FMT_NV12M_SBWCL_8B;
	for (plane = 0; plane < 2; plane++) {
	val1 = camerapp_hw_get_comp_buf_size(gdc, &frame,
			width, height, pixfmt, plane, GDC_SBWC_SIZE_ALL);
		KUNIT_EXPECT_EQ(test, val1, (int)-EINVAL);
	}
}

static void camerapp_hw_gdc_set_initialization_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val;

	camerapp_hw_gdc_set_initialization(gdc->pmio);

	val = GDC_GET_R(gdc, GDC_R_IP_PROCESSING);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_R(gdc, GDC_R_C_LOADER_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_R(gdc, GDC_R_STAT_RDMACLOADER_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_F(gdc, GDC_R_CMDQ_QUE_CMD_L, GDC_F_CMDQ_QUE_CMD_INT_GROUP_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)0xFF);
	val = GDC_GET_F(gdc, GDC_R_CMDQ_QUE_CMD_M, GDC_F_CMDQ_QUE_CMD_SETTING_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)3);
	val = GDC_GET_R(gdc, GDC_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_R(gdc, GDC_R_INT_REQ_INT0_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)GDC_INT0_EN);
	val = GDC_GET_R(gdc, GDC_R_INT_REQ_INT1_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)GDC_INT1_EN);
	val = GDC_GET_R(gdc, GDC_R_CMDQ_ENABLE);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
}

static void camerapp_hw_gdc_check_scale_parameters(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_crop_param *crop_param = &ctx->crop_param[ctx->cur_index];
	u32 val1, val2;
	u32 crop_width, crop_height;
	u32 gdc_scale_x, gdc_scale_y, gdc_scale_width, gdc_scale_height;
	u32 scaleShifterX, scaleShifterY, imagewidth, imageheight, gdc_scale_shifter_x, gdc_scale_shifter_y;
	u32 gdc_inv_scale_x, gdc_inv_scale_y, out_scaled_width, out_scaled_height;

	if (crop_param->is_bypass_mode) {
		crop_width = ctx->s_frame.width;
		crop_height = ctx->s_frame.height;
		gdc_scale_width = ctx->d_frame.width;
		gdc_scale_height = ctx->d_frame.height;
		out_scaled_width = 8192;
		out_scaled_height = 8192;
	} else {
		crop_width = crop_param->crop_width;
		crop_height = crop_param->crop_height;
		gdc_scale_width = ctx->s_frame.width;
		gdc_scale_height = ctx->s_frame.height;
		out_scaled_width = 8192 * crop_param->crop_width / ctx->d_frame.width;
		out_scaled_height = 8192 * crop_param->crop_height / ctx->d_frame.height;
	}

	scaleShifterX = DS_SHIFTER_MAX;
	imagewidth = gdc_scale_width << 1;
	while ((imagewidth <= MAX_VIRTUAL_GRID_X) && (scaleShifterX > 0)) {
		imagewidth <<= 1;
		scaleShifterX--;
	}
	gdc_scale_shifter_x = scaleShifterX;

	scaleShifterY = DS_SHIFTER_MAX;
	imageheight = gdc_scale_height << 1;
	while ((imageheight <= MAX_VIRTUAL_GRID_Y) && (scaleShifterY > 0)) {
		imageheight <<= 1;
		scaleShifterY--;
	}
	gdc_scale_shifter_y = scaleShifterY;

	gdc_scale_x = MIN(65535,
		((MAX_VIRTUAL_GRID_X << (DS_FRACTION_BITS + gdc_scale_shifter_x))
		+ gdc_scale_width / 2) / gdc_scale_width);
	gdc_scale_y = MIN(65535,
		((MAX_VIRTUAL_GRID_Y << (DS_FRACTION_BITS + gdc_scale_shifter_y))
		+ gdc_scale_height / 2) / gdc_scale_height);

	gdc_inv_scale_x = ctx->s_frame.width;
	gdc_inv_scale_y = ((ctx->s_frame.height << 3) + 3) / 6;


	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_CONFIG);
	KUNIT_EXPECT_EQ(test, val1, (u32)0);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_ORG_SIZE);
	val2 = (ctx->s_frame.height << 16) |ctx->s_frame.width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_CROP_START);
	val2 = (crop_param->crop_start_y << 16) |crop_param->crop_start_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_CROP_SIZE);
	val2 = (crop_height << 16) |crop_width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_SCALE);
	val2 = (gdc_scale_y << 16) | gdc_scale_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_SCALE_SHIFTER);
	val2 = (gdc_scale_shifter_y << 4) | gdc_scale_shifter_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_INV_SCALE);
	val2 = (gdc_inv_scale_y << 16) | gdc_inv_scale_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_CROP_START);
	val2 = (0x0 << 16) | 0x0;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_CROP_SIZE);
	val2 = (ctx->d_frame.height << 16) | ctx->d_frame.width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_SCALE);
	val2 = (out_scaled_height << 16) | out_scaled_width;
	KUNIT_EXPECT_EQ(test, val1, val2);
}

static void camerapp_hw_gdc_check_grid_parameters(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_crop_param *crop_param = &ctx->crop_param[ctx->cur_index];
	u32 i, j;
	u32 cal_sfr_offset;
	u32 val1, val2;
	u32 sfr_offset = 0x0004;
	u32 sfr_start_x = 0x2000;
	u32 sfr_start_y = 0x3200;

	for (i = 0; i < GRID_Y_SIZE; i++) {
		for (j = 0; j < GRID_X_SIZE; j++) {
			cal_sfr_offset = (sfr_offset * i * GRID_X_SIZE) + (sfr_offset * j);
			val1 = GDC_GET_R(gdc, sfr_start_x + cal_sfr_offset);
			val2 = GDC_GET_R(gdc, sfr_start_y + cal_sfr_offset);

			if (crop_param->use_calculated_grid) {
				KUNIT_EXPECT_EQ(test, val1, crop_param->calculated_grid_x[i][j]);
				KUNIT_EXPECT_EQ(test, val2, crop_param->calculated_grid_y[i][j]);
			} else {
				KUNIT_EXPECT_EQ(test, val1, 0);
				KUNIT_EXPECT_EQ(test, val2, 0);
			}
		}
	}
}

static void camerapp_hw_gdc_check_compressor(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_frame *d_frame, *s_frame;
	u32 val1, val2;
	u32 align_64b = 0;

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	if (s_frame->extra && d_frame->extra) {
		if (s_frame->pixelformat == V4L2_PIX_FMT_NV12M_SBWCL_64_8B &&
			d_frame->pixelformat == V4L2_PIX_FMT_NV12M_SBWCL_64_8B)
			align_64b = 1;

		val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_COMPRESSOR);
		val2 = (d_frame->extra << 8) | s_frame->extra;
		KUNIT_EXPECT_EQ(test, val1, val2);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_COMP_CONTROL);
		val2 = (align_64b << 3) | s_frame->extra;
		KUNIT_EXPECT_EQ(test, val1, val2);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_COMP_CONTROL);
		val2 = (align_64b << 3) | s_frame->extra;
		KUNIT_EXPECT_EQ(test, val1, val2);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_COMP_LOSSY_QUALITY_CONTROL);
		KUNIT_EXPECT_EQ(test, val1, 0);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_COMP_LOSSY_QUALITY_CONTROL);
		KUNIT_EXPECT_EQ(test, val1, 0);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_WDMA_COMP_CONTROL);
		val2 = (align_64b << 3) | d_frame->extra;
		KUNIT_EXPECT_EQ(test, val1, val2);
		val1 = GDC_GET_R(gdc, GDC_R_YUV_WDMA_COMP_LOSSY_QUALITY_CONTROL);
		KUNIT_EXPECT_EQ(test, val1, 0);
	} else {
		val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_COMPRESSOR);
		KUNIT_EXPECT_EQ(test, val1, 0);
	}

}

static void camerapp_hw_gdc_check_format(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val;

	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_YUV_FORMAT);
	KUNIT_EXPECT_EQ(test, val, 0x1);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_DATA_FORMAT);
	KUNIT_EXPECT_EQ(test, val, 0x800);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_DATA_FORMAT);
	KUNIT_EXPECT_EQ(test, val, 0x800);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_DATA_FORMAT);
	KUNIT_EXPECT_EQ(test, val, 0x800);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_MONO_MODE);
	KUNIT_EXPECT_EQ(test, val, 0x0);
}

static void camerapp_hw_gdc_check_dma_addr(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_frame *d_frame, *s_frame;
	u32 val;

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	/* CORE */
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_EN);
	KUNIT_EXPECT_EQ(test, val, 0x1);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_EN);
	KUNIT_EXPECT_EQ(test, val, 0x1);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_EN);
	KUNIT_EXPECT_EQ(test, val, 0x1);

	/* WDMA */
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.y >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.cb >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.y & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.cb & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.y_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.cbcr_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.y_2bit & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, d_frame->addr.cbcr_2bit & 0xF);

	/* RDMA */
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.y_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.cbcr_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.y_2bit & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.cbcr_2bit & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.y_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.cbcr_2bit >> 4);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.y_2bit & 0xF);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_LSB_4B_0_0);
	KUNIT_EXPECT_EQ(test, val, s_frame->addr.cbcr_2bit & 0xF);

	if (d_frame->io_mode == GDC_OUT_OTF) {
		val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_EN);
		KUNIT_EXPECT_EQ(test, val, 0x0);
	}
}

static void camerapp_hw_gdc_check_dma_size(struct kunit *test,
       u32 wdma_width, u32 wdma_height,
       u32 rdma_y_width, u32 rdma_y_height, u32 rdma_uv_width, u32 rdma_uv_height,
       u32 wdma_stride_1p, u32 wdma_stride_2p, u32 wdma_stride_header_1p, u32 wdma_stride_header_2p,
       u32 rdma_stride_1p, u32 rdma_stride_2p, u32 rdma_stride_header_1p, u32 rdma_stride_header_2p)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val;

	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_WIDTH);
	KUNIT_EXPECT_EQ(test, val, wdma_width);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, wdma_height);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_WIDTH);
	KUNIT_EXPECT_EQ(test, val, rdma_y_width);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, rdma_y_height);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_WIDTH);
	KUNIT_EXPECT_EQ(test, val, rdma_uv_width);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, rdma_uv_height);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_1p);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_STRIDE_2P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_2p);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_header_1p);
	val = GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_STRIDE_2P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_header_2p);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_1p);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_2p);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEADER_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_header_1p);
	val = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEADER_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_header_2p);
}

static void camerapp_hw_gdc_check_core_param(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_crop_param *crop_param = &ctx->crop_param[ctx->cur_index];
	u32 val;

	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_INTERPOLATION);
	KUNIT_EXPECT_EQ(test, val, 0x23);
	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_BOUNDARY_OPTION);
	KUNIT_EXPECT_EQ(test, val, 0x1);
	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_GRID_MODE);
	KUNIT_EXPECT_EQ(test, val, crop_param->is_grid_mode);
	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_YUV_FORMAT);
	KUNIT_EXPECT_EQ(test, val, 0x1);
	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_LUMA_MINMAX);
	KUNIT_EXPECT_EQ(test, val, 0xFF0000);
	val = GDC_GET_R(gdc, GDC_R_YUV_GDC_CHROMA_MINMAX);
	KUNIT_EXPECT_EQ(test, val, 0xFF0000);
}

static void camerapp_hw_gdc_check_out_select(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx = gdc->current_ctx;
	struct gdc_crop_param *crop_param = &ctx->crop_param[ctx->cur_index];
	u32 val1, val2;

	if (crop_param->out_mode == GDC_OUT_M2M)
		val2 = 0x1;
	else /* OTF */
		val2 = 0x6;

	val1 = GDC_GET_R(gdc, GDC_R_YUV_GDC_OUTPUT_SELECT);
	KUNIT_EXPECT_EQ(test, val1, val2);
}

static void camerapp_hw_gdc_check_votf_param(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val1, val2;

	val1 = GDC_GET_R(gdc, GDC_R_YUV_WDMA_VOTF_EN);
	val2 = 0x3;
	KUNIT_EXPECT_EQ(test, val1, val2);

	val1 = GDC_GET_R(gdc, GDC_R_YUV_WDMA_BUSINFO);
	val2 = 0x40; /* IS_LLC_CACHE_HINT_VOTF_TYPE */
	KUNIT_EXPECT_EQ(test, val1, val2);
}

static void camerapp_hw_gdc_update_param_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx;
	u32 i, j;
	u64 addr = 0xDEADDEADF;

	ctx = kunit_kzalloc(test, sizeof(struct gdc_ctx), 0);

	ctx->s_frame.width = 320;
	ctx->s_frame.height = 240;
	ctx->s_frame.gdc_fmt = &gdc_formats[1];
	ctx->s_frame.extra = NONE;
	ctx->s_frame.pixelformat = V4L2_PIX_FMT_NV21M;
	ctx->s_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	ctx->s_frame.addr.y = addr;
	ctx->s_frame.addr.cb = addr;
	ctx->s_frame.addr.y_2bit = addr;
	ctx->s_frame.addr.cbcr_2bit = addr;
	ctx->d_frame.width = 320;
	ctx->d_frame.height = 240;
	ctx->d_frame.gdc_fmt = &gdc_formats[1];
	ctx->d_frame.extra = NONE;
	ctx->d_frame.pixelformat = V4L2_PIX_FMT_NV21M;
	ctx->d_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	ctx->d_frame.addr.y = addr;
	ctx->d_frame.addr.cb = addr;
	ctx->d_frame.addr.y_2bit = addr;
	ctx->d_frame.addr.cbcr_2bit = addr;
	ctx->d_frame.io_mode = GDC_OUT_M2M;
	ctx->cur_index = 0;
	ctx->crop_param[ctx->cur_index].is_grid_mode = 0;
	ctx->crop_param[ctx->cur_index].is_bypass_mode = 0;
	ctx->crop_param[ctx->cur_index].use_calculated_grid = 0;
	ctx->crop_param[ctx->cur_index].crop_start_x = 0;
	ctx->crop_param[ctx->cur_index].crop_start_y = 0;
	ctx->crop_param[ctx->cur_index].crop_width = 320;
	ctx->crop_param[ctx->cur_index].crop_height = 240;
	ctx->crop_param[ctx->cur_index].src_bytesperline[0] = 0;
	ctx->crop_param[ctx->cur_index].src_bytesperline[1] = 0;
	ctx->crop_param[ctx->cur_index].src_bytesperline[2] = 0;
	ctx->crop_param[ctx->cur_index].dst_bytesperline[0] = 0;
	ctx->crop_param[ctx->cur_index].dst_bytesperline[1] = 0;
	ctx->crop_param[ctx->cur_index].dst_bytesperline[2] = 0;
	gdc->current_ctx = ctx;

	for (i = 0; i < GRID_Y_SIZE; i++) {
		for (j = 0; j < GRID_X_SIZE; j++) {
			ctx->crop_param[ctx->cur_index].calculated_grid_x[i][j] = 0xAA;
			ctx->crop_param[ctx->cur_index].calculated_grid_y[i][j] = 0xBB;
		}
	}

	/* normal operation */
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_scale_parameters(test);
	camerapp_hw_gdc_check_grid_parameters(test);
	camerapp_hw_gdc_check_compressor(test);
	camerapp_hw_gdc_check_format(test);
	camerapp_hw_gdc_check_dma_addr(test);
	camerapp_hw_gdc_check_dma_size(test, ctx->d_frame.width, ctx->d_frame.height,
		ctx->s_frame.width, ctx->s_frame.height, ctx->s_frame.width, ctx->s_frame.height / 2,
		ALIGN(ctx->s_frame.width, 16), ALIGN(ctx->s_frame.width, 16), 0, 0,
		ALIGN(ctx->crop_param[ctx->cur_index].crop_width, 16),
		ALIGN(ctx->crop_param[ctx->cur_index].crop_width, 16), 0, 0);
	camerapp_hw_gdc_check_core_param(test);
	camerapp_hw_gdc_check_out_select(test);

	/* scale up 2x, use_grid, lossless */
	ctx->s_frame.width = 160;
	ctx->s_frame.height = 120;
	ctx->s_frame.extra = COMP;
	ctx->d_frame.extra = COMP;
	ctx->crop_param[ctx->cur_index].use_calculated_grid = 1;
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_scale_parameters(test);
	camerapp_hw_gdc_check_grid_parameters(test);
	camerapp_hw_gdc_check_compressor(test);

	/* bypass & grid_mode, lossy, votf */
	ctx->crop_param[ctx->cur_index].is_grid_mode = 1;
	ctx->crop_param[ctx->cur_index].is_bypass_mode = 1;
	ctx->crop_param[ctx->cur_index].out_mode = GDC_OUT_VOTF;
	ctx->s_frame.extra = COMP_LOSS;
	ctx->d_frame.extra = COMP_LOSS;
	ctx->s_frame.gdc_fmt = &gdc_formats[22];
	ctx->s_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	ctx->s_frame.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_64_8B;
	ctx->d_frame.gdc_fmt = &gdc_formats[22];
	ctx->d_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	ctx->d_frame.pixelformat = V4L2_PIX_FMT_NV12M_SBWCL_64_8B;
	ctx->d_frame.io_mode = GDC_OUT_VOTF;
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_scale_parameters(test);
	camerapp_hw_gdc_check_compressor(test);
	camerapp_hw_gdc_check_dma_size(test, ctx->d_frame.width, ctx->d_frame.height,
		ctx->s_frame.width, ctx->s_frame.height, ctx->s_frame.width, ctx->s_frame.height / 2,
		SBWCL_64_STRIDE(ctx->crop_param[ctx->cur_index].crop_width),
		SBWCL_64_STRIDE(ctx->crop_param[ctx->cur_index].crop_width),
		SBWCL_HEADER_STRIDE(ctx->crop_param[ctx->cur_index].crop_width),
		SBWCL_HEADER_STRIDE(ctx->crop_param[ctx->cur_index].crop_width),
		SBWCL_64_STRIDE(ctx->s_frame.width),
		SBWCL_64_STRIDE(ctx->s_frame.width),
		SBWCL_HEADER_STRIDE(ctx->s_frame.width),
		SBWCL_HEADER_STRIDE(ctx->s_frame.width));
	camerapp_hw_gdc_check_votf_param(test);

	/* otf */
	ctx->crop_param[ctx->cur_index].out_mode = GDC_OUT_OTF;
	ctx->d_frame.io_mode = GDC_OUT_OTF;
	camerapp_hw_gdc_update_param(gdc->pmio, gdc);
	camerapp_hw_gdc_check_out_select(test);

	kunit_kfree(test, ctx);
}

static void camerapp_hw_gdc_init_pmio_config_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	camerapp_hw_gdc_init_pmio_config(gdc);
}

static void camerapp_hw_gdc_g_reg_cnt_kunit_test(struct kunit *test)
{
	u32 reg_cnt;

	reg_cnt = camerapp_hw_gdc_g_reg_cnt();
	KUNIT_EXPECT_EQ(test, reg_cnt, GDC_REG_CNT);
}

static void __gdc_init_pmio(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	int ret;
	u32 reg_cnt;

	gdc->regs_base = tctx->addr;

	camerapp_hw_gdc_init_pmio_config(gdc);
	gdc->pmio_config.cache_type = PMIO_CACHE_NONE; /* override for kunit test */

	gdc->pmio = pmio_init(NULL, NULL, &gdc->pmio_config);

	ret = pmio_field_bulk_alloc(gdc->pmio, &gdc->pmio_fields,
					gdc->pmio_config.fields,
					gdc->pmio_config.num_fields);
	if (ret)
		return;

	reg_cnt = camerapp_hw_gdc_g_reg_cnt();

	gdc->pmio_reg_seqs = vzalloc(sizeof(struct pmio_reg_seq) * reg_cnt);
	if (!gdc->pmio_reg_seqs) {
		pmio_field_bulk_free(gdc->pmio, gdc->pmio_fields);
		pmio_exit(gdc->pmio);
		return;
	}
}

static void __gdc_exit_pmio(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;

	if (gdc->pmio_reg_seqs)
		vfree(gdc->pmio_reg_seqs);

	if (gdc->pmio) {
		if (gdc->pmio_fields)
			pmio_field_bulk_free(gdc->pmio, gdc->pmio_fields);

		pmio_exit(gdc->pmio);
	}
}

static struct kunit_case camerapp_hw_api_gdc_kunit_test_cases[] = {
	KUNIT_CASE(camerapp_hw_api_gdc_get_size_constraints_kunit_test),
	KUNIT_CASE(camerapp_hw_api_gdc_sfr_dump_kunit_test),
	KUNIT_CASE(camerapp_hw_api_gdc_start_kunit_test),
	KUNIT_CASE(camerapp_hw_api_gdc_stop_kunit_test),
	KUNIT_CASE(camerapp_hw_api_gdc_sw_reset_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_get_intr_status_and_clear_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_get_fs_fe_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_get_sbwc_const_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_has_comp_header_kunit_test),
	KUNIT_CASE(camerapp_hw_get_comp_buf_size_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_set_initialization_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_update_param_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_init_pmio_config_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_g_reg_cnt_kunit_test),
	{},
};

static int camerapp_hw_api_gdc_kunit_test_init(struct kunit *test)
{
	test_ctx.addr = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.addr);

	test_ctx.gdc = kunit_kzalloc(test, sizeof(struct gdc_dev), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.gdc);

	test_ctx.crop_param = kunit_kzalloc(test, sizeof(struct gdc_crop_param), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.crop_param);

	test_ctx.gdc->regs_base = test_ctx.addr;

	test->priv = &test_ctx;

	__gdc_init_pmio(test);

	return 0;
}

static void camerapp_hw_api_gdc_kunit_test_exit(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;

	__gdc_exit_pmio(test);

	kunit_kfree(test, tctx->crop_param);
	kunit_kfree(test, tctx->addr);
	kunit_kfree(test, tctx->gdc);
}

struct kunit_suite camerapp_hw_api_gdc_kunit_test_suite = {
	.name = "pablo-hw-api-gdc-v1100-kunit-test",
	.init = camerapp_hw_api_gdc_kunit_test_init,
	.exit = camerapp_hw_api_gdc_kunit_test_exit,
	.test_cases = camerapp_hw_api_gdc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&camerapp_hw_api_gdc_kunit_test_suite);

MODULE_LICENSE("GPL");
