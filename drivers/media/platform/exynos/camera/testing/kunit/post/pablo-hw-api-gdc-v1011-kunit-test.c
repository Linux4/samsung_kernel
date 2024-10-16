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
#include "gdc/pablo-hw-reg-gdc-v1011.h"
#include "is-common-enum.h"

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
	.version = 0x10010001,
};

static u32 support_sbwc_fmt_array[MAX_SUPPORT_SBWC_FMT] = {
	V4L2_PIX_FMT_NV12M_SBWCL_8B,
	V4L2_PIX_FMT_NV12M_SBWCL_10B,
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
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;
	const struct gdc_variant *constraints;
	u32 val;

	*(u32 *)(tctx->addr + GDC_R_IP_VERSION) = gdc_variant.version;

	constraints = func->camerapp_hw_gdc_get_size_constraints(gdc->pmio);
	val = func->camerapp_hw_gdc_get_ver(gdc->pmio);

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
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;

	func->camerapp_gdc_sfr_dump(gdc->regs_base);
}

static void camerapp_hw_api_gdc_start_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;
	struct c_loader_buffer clb;
	u32 val;

	/* APB-DIRECT */
	*(u32 *)(tctx->addr + GDC_R_IP_PROCESSING) = 0x1;

	func->camerapp_hw_gdc_start(gdc->pmio, &clb);

	val = GDC_GET_R(gdc, GDC_R_CMDQ_ADD_TO_QUEUE_0);
	KUNIT_EXPECT_EQ(test, val, (u32)1);
	val = GDC_GET_R(gdc, GDC_R_IP_PROCESSING);
	KUNIT_EXPECT_EQ(test, val, (u32)0);

	/* C-LOADER */
	*(u32 *)(tctx->addr + GDC_R_IP_PROCESSING) = 0x1;
	*(u32 *)(tctx->addr + GDC_R_CMDQ_ADD_TO_QUEUE_0) = 0x0;

	clb.header_dva = 0xDEADDEAD;
	clb.num_of_headers = 5;
	func->camerapp_hw_gdc_start(gdc->pmio, &clb);

	val = GDC_GET_R(gdc, GDC_R_CMDQ_QUE_CMD_H);
	KUNIT_EXPECT_EQ(test, val, (u32)clb.header_dva);
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
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;
	u32 val1, val2;

	val1 = func->camerapp_hw_gdc_sw_reset(gdc->pmio);
	val2 = GDC_GET_R(gdc, GDC_R_SW_RESET);
	KUNIT_EXPECT_EQ(test, val1, (u32)10001);
	KUNIT_EXPECT_EQ(test, val2, (u32)1);
}

static void camerapp_hw_gdc_get_intr_status_and_clear_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;
	u32 val0, c0, c1;
	u32 int0 = 0xAAAABBBB;
	u32 int1 = 0xCCCCDDDD;

	*(u32 *)(tctx->addr + GDC_R_INT_REQ_INT0) = int0;
	*(u32 *)(tctx->addr + GDC_R_INT_REQ_INT1) = int1;

	val0 = func->camerapp_hw_gdc_get_intr_status_and_clear(gdc->pmio);

	c0 = GDC_GET_R(gdc, GDC_R_INT_REQ_INT0_CLEAR);
	c1 = GDC_GET_R(gdc, GDC_R_INT_REQ_INT1_CLEAR);

	KUNIT_EXPECT_EQ(test, val0, int0);
	KUNIT_EXPECT_EQ(test, c0, int0);
	KUNIT_EXPECT_EQ(test, c1, int1);
}

static void camerapp_hw_gdc_get_fs_fe_kunit_test(struct kunit *test)
{
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	u32 fs = GDC_INT_FRAME_START, fe = GDC_INT_FRAME_END;
	u32 val1, val2;

	val1 = func->camerapp_hw_gdc_get_int_frame_start();
	val2 = func->camerapp_hw_gdc_get_int_frame_end();

	KUNIT_EXPECT_EQ(test, fs, val1);
	KUNIT_EXPECT_EQ(test, fe, val2);
}

static void camerapp_hw_gdc_votf_en_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;
	u32 tws = 0, trs = 1;
	u32 val1, val2;

	func->camerapp_hw_gdc_votf_enable(gdc->pmio, tws);

	val1 = GDC_GET_R(gdc, GDC_R_YUV_WDMA_VOTF_EN);
	val2 = GDC_GET_R(gdc, GDC_R_YUV_WDMA_BUSINFO);
	KUNIT_EXPECT_EQ(test, val1, (u32)0x3);
	KUNIT_EXPECT_EQ(test, val2, (u32)0x40);

	func->camerapp_hw_gdc_votf_enable(gdc->pmio, trs);

	val1 = GDC_GET_R(gdc, GDC_R_YUV_RDMAY_VOTF_EN);
	val2 = GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_VOTF_EN);

	KUNIT_EXPECT_EQ(test, val1, (u32)0x1);
	KUNIT_EXPECT_EQ(test, val2, (u32)0x1);
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
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	u32 width, height, pixformat;
	int i, val, result;

	width = 320;
	height = 240;

	for (i = 0; i < ARRAY_SIZE(gdc_formats); ++i) {
		pixformat = gdc_formats[i].pixelformat;
		result = __find_supported_sbwc_fmt(pixformat);
		val = func->camerapp_hw_get_sbwc_constraint(pixformat, width, height, 0);
		KUNIT_EXPECT_EQ(test, result, val);
	}
}

static void camerapp_hw_gdc_has_comp_header_kunit_test(struct kunit *test)
{
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	bool val;

	val = func->camerapp_hw_gdc_has_comp_header(0);

	KUNIT_EXPECT_EQ(test, val, (bool)false);
}

static void camerapp_hw_get_comp_buf_size_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_frame frame;
	struct gdc_dev *gdc = tctx->gdc;
	u32 width, height, pixfmt, plane;
	int val1, val2;

	width = 320;
	height = 240;
	pixfmt = V4L2_PIX_FMT_NV12M_SBWCL_8B;
	for(plane = 0; plane < 2; plane ++) {
		val1 = func->camerapp_hw_get_comp_buf_size(gdc, &frame,
			width, height, pixfmt, plane, GDC_SBWC_SIZE_ALL);
		val2 = plane ? 19264 : 38464;
		KUNIT_EXPECT_EQ(test, val1, val2);
	}

	pixfmt = V4L2_PIX_FMT_NV12M_SBWCL_10B;
	for(plane = 0; plane < 2; plane ++) {
		val1 = func->camerapp_hw_get_comp_buf_size(gdc, &frame,
			width, height, pixfmt, plane, GDC_SBWC_SIZE_ALL);
		val2 = plane ? 19264 : 38464;
		KUNIT_EXPECT_EQ(test, val1, val2);
	}
}

static void camerapp_hw_gdc_set_initialization_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;
	u32 val;

	func->camerapp_hw_gdc_set_initialization(gdc->pmio);

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

static void camerapp_hw_gdc_update_scale_parameters_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_crop_param *crop_param = tctx->crop_param;
	struct gdc_frame s_frame, d_frame;
	u32 val1, val2;

	s_frame.width = 320;
	s_frame.height = 240;
	d_frame.width = 320;
	d_frame.height = 240;
	crop_param->is_grid_mode = 0;
	crop_param->is_bypass_mode = 0;
	crop_param->crop_width = 320;
	crop_param->crop_height = 240;
	crop_param->crop_start_x = 0;
	crop_param->crop_start_y = 0;

	/* normal operation */
	func->camerapp_hw_gdc_update_scale_parameters(gdc->pmio, &s_frame, &d_frame, crop_param);

	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_CONFIG);
	KUNIT_EXPECT_EQ(test, val1, (u32)0);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_ORG_SIZE);
	val2 = (s_frame.height << 16) |s_frame.width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_CROP_START);
	val2 = (crop_param->crop_start_y << 16) |crop_param->crop_start_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_CROP_SIZE);
	val2 = (crop_param->crop_height << 16) |crop_param->crop_width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_SCALE);
	val2 = (0xCCCD << 16) | 0xCCCD;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_SCALE_SHIFTER);
	val2 = (0x2 << 4) | 0x2;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INV_SCALE);
	val2 = (0x140 << 16) | 0x140;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_CROP_START);
	val2 = (0x0 << 16) | 0x0;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_CROP_SIZE);
	val2 = (d_frame.height << 16) | d_frame.width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_SCALE);
	val2 = (0x2000 << 16) | 0x2000;
	KUNIT_EXPECT_EQ(test, val1, val2);

	/* scale up 2x */
	s_frame.width = 160;
	s_frame.height = 120;
	d_frame.width = 320;
	d_frame.height = 240;
	crop_param->crop_width = 160;
	crop_param->crop_height = 120;
	crop_param->crop_start_x = 0;
	crop_param->crop_start_y = 0;

	func->camerapp_hw_gdc_update_scale_parameters(gdc->pmio, &s_frame, &d_frame, crop_param);

	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_CONFIG);
	KUNIT_EXPECT_EQ(test, val1, (u32)0);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_ORG_SIZE);
	val2 = (s_frame.height << 16) |s_frame.width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_CROP_START);
	val2 = (crop_param->crop_start_y << 16) |crop_param->crop_start_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_CROP_SIZE);
	val2 = (crop_param->crop_height << 16) |crop_param->crop_width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_SCALE);
	val2 = (0xCCCD << 16) | 0xCCCD;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_SCALE_SHIFTER);
	val2 = (0x1 << 4) | 0x1;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INV_SCALE);
	val2 = (0xa0 << 16) | 0xa0;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_CROP_START);
	val2 = (0x0 << 16) | 0x0;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_CROP_SIZE);
	val2 = (d_frame.height << 16) | d_frame.width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_SCALE);
	val2 = (0x1000 << 16) | 0x1000;
	KUNIT_EXPECT_EQ(test, val1, val2);

	/* bypass & grid_mode */
	crop_param->is_grid_mode = 1;
	crop_param->is_bypass_mode = 1;

	func->camerapp_hw_gdc_update_scale_parameters(gdc->pmio, &s_frame, &d_frame, crop_param);

	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_CONFIG);
	KUNIT_EXPECT_EQ(test, val1, (u32)0);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_ORG_SIZE);
	val2 = (s_frame.height << 16) |s_frame.width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_CROP_START);
	val2 = (crop_param->crop_start_y << 16) |crop_param->crop_start_x;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INPUT_CROP_SIZE);
	val2 = (crop_param->crop_height << 16) |crop_param->crop_width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_SCALE);
	val2 = (0xCCCD << 16) | 0xCCCD;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_SCALE_SHIFTER);
	val2 = (0x2 << 4) | 0x2;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_INV_SCALE);
	val2 = (0xa0 << 16) | 0xa0;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_CROP_START);
	val2 = (0x0 << 16) | 0x0;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_CROP_SIZE);
	val2 = (d_frame.height << 16) | d_frame.width;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_OUT_SCALE);
	val2 = (0x2000 << 16) | 0x2000;
	KUNIT_EXPECT_EQ(test, val1, val2);
}

static void camerapp_hw_gdc_update_grid_param_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_crop_param *crop_param = tctx->crop_param;
	struct gdc_dev *gdc = tctx->gdc;
	u32 cal_sfr_offset;
	u32 val1, val2;
	u32 i, j;
	u32 sfr_offset = 0x0004;
	u32 sfr_start_x = 0x2000;
	u32 sfr_start_y = 0x3200;

	for (i = 0; i < GRID_Y_SIZE; i++) {
		for (j = 0; j < GRID_X_SIZE; j++) {
			crop_param->calculated_grid_x[i][j] = 0xAA;
			crop_param->calculated_grid_y[i][j] = 0xBB;
		}
	}
	crop_param->use_calculated_grid = 0;

	func->camerapp_hw_gdc_update_grid_param(gdc->pmio, crop_param);

	for (i = 0; i < GRID_Y_SIZE; i++) {
		for (j = 0; j < GRID_X_SIZE; j++) {
			cal_sfr_offset = (sfr_offset * i * GRID_X_SIZE) + (sfr_offset * j);
			val1= GDC_GET_R(gdc, sfr_start_x + cal_sfr_offset);
			val2= GDC_GET_R(gdc, sfr_start_y + cal_sfr_offset);
			KUNIT_EXPECT_EQ(test, val1, (u32)0);
			KUNIT_EXPECT_EQ(test, val2, (u32)0);
		}
	}

	crop_param->use_calculated_grid = 1;

	func->camerapp_hw_gdc_update_grid_param(gdc->pmio, crop_param);

	for (i = 0; i < GRID_Y_SIZE; i++) {
		for (j = 0; j < GRID_X_SIZE; j++) {
			cal_sfr_offset = (sfr_offset * i * GRID_X_SIZE) + (sfr_offset * j);
			val1= GDC_GET_R(gdc, sfr_start_x + cal_sfr_offset);
			val2= GDC_GET_R(gdc, sfr_start_y + cal_sfr_offset);
			KUNIT_EXPECT_EQ(test, val1, (u32)crop_param->calculated_grid_x[i][j]);
			KUNIT_EXPECT_EQ(test, val2, (u32)crop_param->calculated_grid_y[i][j]);
		}
	}
}

static void camerapp_hw_gdc_set_compressor_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_frame s_frame, d_frame;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val1, val2;

	/* no sbwc */
	s_frame.extra = NONE;
	s_frame.pixelformat = V4L2_PIX_FMT_NV21M;
	d_frame.extra = NONE;
	d_frame.pixelformat = V4L2_PIX_FMT_NV21M;

	func->camerapp_hw_gdc_set_compressor(gdc->pmio, &s_frame, &d_frame);

	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_COMPRESSOR);
	KUNIT_EXPECT_EQ(test, val1, (u32) 0);

	/* src : lossless, dst : lossless */
	s_frame.extra = COMP;
	d_frame.extra = COMP;
	d_frame.io_mode = GDC_OUT_M2M;

	func->camerapp_hw_gdc_set_compressor(gdc->pmio, &s_frame, &d_frame);

	val1= GDC_GET_R(gdc, GDC_R_YUV_GDC_COMPRESSOR);
	val2 = (d_frame.extra << 8) | s_frame.extra;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_RDMAY_COMP_CONTROL);
	val2 = s_frame.extra;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_COMP_CONTROL);
	val2 = s_frame.extra;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_RDMAY_COMP_LOSSY_BYTE32NUM);
	val2 = 0;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_COMP_LOSSY_BYTE32NUM);
	val2 = 0;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_WDMA_COMP_CONTROL);
	val2 = d_frame.extra;
	KUNIT_EXPECT_EQ(test, val1, val2);
	val1= GDC_GET_R(gdc, GDC_R_YUV_WDMA_COMP_LOSSY_BYTE32NUM);
	val2 = 0;
	KUNIT_EXPECT_EQ(test, val1, val2);
}

static void camerapp_hw_gdc_set_format_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_frame s_frame, d_frame;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val;

	s_frame.gdc_fmt = &gdc_formats[1];
	d_frame.gdc_fmt = &gdc_formats[1];

	func->camerapp_hw_gdc_set_format(gdc->pmio, &s_frame, &d_frame);

	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_YUV_FORMAT);
	KUNIT_EXPECT_EQ(test, val, (u32)0x1);
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_DATA_FORMAT);
	KUNIT_EXPECT_EQ(test, val, (u32)0x800);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAY_DATA_FORMAT);
	KUNIT_EXPECT_EQ(test, val, (u32)0x800);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_DATA_FORMAT);
	KUNIT_EXPECT_EQ(test, val, (u32)0x800);
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_MONO_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)0x0);
}

static void camerapp_hw_gdc_set_dma_address_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_frame s_frame, d_frame;
	struct gdc_dev *gdc = tctx->gdc;
	u64 addr = 0xDEADDEADF;
	u32 val;

	s_frame.gdc_fmt = &gdc_formats[1];
	d_frame.gdc_fmt = &gdc_formats[1];
	s_frame.addr.y = addr;
	s_frame.addr.cb = addr;
	s_frame.addr.y_2bit = addr;
	s_frame.addr.cbcr_2bit = addr;
	d_frame.addr.y = addr;
	d_frame.addr.cb = addr;
	d_frame.addr.y_2bit = addr;
	d_frame.addr.cbcr_2bit = addr;

	func->camerapp_hw_gdc_set_dma_address(gdc->pmio, &s_frame, &d_frame);

	/* CORE */
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)0x1);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAY_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)0x1);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_EN);
	KUNIT_EXPECT_EQ(test, val, (u32)0x1);

	/* WDMA */
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, (u32)(d_frame.addr.y));
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_BASE_ADDR_2P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, (u32)(d_frame.addr.cb));

	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, (u32)(d_frame.addr.y_2bit));
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_BASE_ADDR_2P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, (u32)(d_frame.addr.cbcr_2bit));


	/* RDMA */
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAY_IMG_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, (u32)(s_frame.addr.y_2bit));
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_IMG_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, (u32)(s_frame.addr.cbcr_2bit));

	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEADER_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, (u32)(s_frame.addr.y_2bit));
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEADER_BASE_ADDR_1P_FRO_0_0);
	KUNIT_EXPECT_EQ(test, val, (u32)(s_frame.addr.cbcr_2bit));

}

static void camerapp_update_dma_size_check_result(struct kunit *test,
	u32 wdma_width, u32 wdma_height,
	u32 rdma_y_width, u32 rdma_y_height, u32 rdma_uv_width, u32 rdma_uv_height,
	u32 wdma_stride_1p, u32 wdma_stride_2p, u32 wdma_stride_header_1p, u32 wdma_stride_header_2p,
	u32 rdma_stride_1p, u32 rdma_stride_2p, u32 rdma_stride_header_1p, u32 rdma_stride_header_2p)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val;
	
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_WIDTH);
	KUNIT_EXPECT_EQ(test, val, wdma_width);
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, wdma_height);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAY_WIDTH);
	KUNIT_EXPECT_EQ(test, val, rdma_y_width);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, rdma_y_height);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_WIDTH);
	KUNIT_EXPECT_EQ(test, val, rdma_uv_width);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEIGHT);
	KUNIT_EXPECT_EQ(test, val, rdma_uv_height);
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_1p);
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_IMG_STRIDE_2P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_2p);
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_header_1p);
	val= GDC_GET_R(gdc, GDC_R_YUV_WDMA_HEADER_STRIDE_2P);
	KUNIT_EXPECT_EQ(test, val, wdma_stride_header_2p);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAY_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_1p);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_IMG_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_2p);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAY_HEADER_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_header_1p);
	val= GDC_GET_R(gdc, GDC_R_YUV_RDMAUV_HEADER_STRIDE_1P);
	KUNIT_EXPECT_EQ(test, val, rdma_stride_header_2p);
}

static void camerapp_hw_gdc_update_dma_size_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_frame s_frame, d_frame;
	struct gdc_crop_param *crop_param = tctx->crop_param;
	struct gdc_dev *gdc = tctx->gdc;
	u32 input_width;
	u32 out_crop;

	/* NV21M */
	s_frame.gdc_fmt = &gdc_formats[1];
	input_width = 320;
	s_frame.width = 320;
	s_frame.height = 240;
	s_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	s_frame.extra = NONE;
	crop_param->src_bytesperline[0] = 0;
	crop_param->src_bytesperline[1] = 0;
	crop_param->src_bytesperline[2] = 0;

	d_frame.gdc_fmt = &gdc_formats[1];
	out_crop = 320;
	d_frame.width = 320;
	d_frame.height = 240;
	d_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	d_frame.extra = NONE;
	crop_param->dst_bytesperline[0] = 0;
	crop_param->dst_bytesperline[1] = 0;
	crop_param->dst_bytesperline[2] = 0;

	*(u32 *)(tctx->addr + GDC_R_YUV_GDC_INPUT_ORG_SIZE) = input_width;
	*(u32 *)(tctx->addr + GDC_R_YUV_GDC_OUT_CROP_SIZE) = out_crop;
	*(u32 *)(tctx->addr + GDC_R_YUV_GDC_YUV_FORMAT) = 0x1; /* YUV420*/

	func->camerapp_hw_gdc_update_dma_size(gdc->pmio, &s_frame, &d_frame, crop_param);

	camerapp_update_dma_size_check_result(test,
		d_frame.width, d_frame.height,
		s_frame.width, s_frame.height, s_frame.width, s_frame.height / 2,
		ALIGN(input_width, 16), ALIGN(input_width, 16), 0, 0,
		ALIGN(out_crop, 16), ALIGN(out_crop, 16), 0, 0);

	/* NV21M, align data received from user space*/
	crop_param->src_bytesperline[0] = 512;
	crop_param->src_bytesperline[1] = 512;
	crop_param->dst_bytesperline[0] = 512;
	crop_param->dst_bytesperline[1] = 512;

	func->camerapp_hw_gdc_update_dma_size(gdc->pmio, &s_frame, &d_frame, crop_param);

	camerapp_update_dma_size_check_result(test,
		d_frame.width, d_frame.height,
		s_frame.width, s_frame.height, s_frame.width, s_frame.height / 2,
		crop_param->dst_bytesperline[0], crop_param->dst_bytesperline[1], 0, 0,
		crop_param->src_bytesperline[0] , crop_param->src_bytesperline[1], 0, 0);

	/* NV21M_P010 */
	s_frame.gdc_fmt = &gdc_formats[11];
	s_frame.pixel_size = CAMERA_PIXEL_SIZE_10BIT;

	d_frame.gdc_fmt = &gdc_formats[11];
	d_frame.pixel_size = CAMERA_PIXEL_SIZE_10BIT;

	func->camerapp_hw_gdc_update_dma_size(gdc->pmio, &s_frame, &d_frame, crop_param);

	camerapp_update_dma_size_check_result(test,
		d_frame.width, d_frame.height,
		s_frame.width, s_frame.height, s_frame.width, s_frame.height / 2,
		ALIGN(input_width * 2, 16), ALIGN(input_width * 2, 16), 0, 0,
		ALIGN(out_crop * 2, 16), ALIGN(out_crop * 2, 16), 0, 0);

	/* NV12M SBWC LOSSY 32B Align 8bit */
	s_frame.gdc_fmt = &gdc_formats[19];
	s_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	s_frame.extra = COMP_LOSS;
	s_frame.comp_rate = GDC_LOSSY_COMP_RATE_50;

	d_frame.gdc_fmt = &gdc_formats[19];
	d_frame.pixel_size = CAMERA_PIXEL_SIZE_8BIT;
	d_frame.extra = COMP_LOSS;
	d_frame.comp_rate = GDC_LOSSY_COMP_RATE_50;

	func->camerapp_hw_gdc_update_dma_size(gdc->pmio, &s_frame, &d_frame, crop_param);

	camerapp_update_dma_size_check_result(test,
		d_frame.width, d_frame.height,
		s_frame.width, s_frame.height, s_frame.width, s_frame.height / 2,
		SBWCL_8B_STRIDE(out_crop, d_frame.comp_rate), SBWCL_8B_STRIDE(out_crop, d_frame.comp_rate),
		SBWCL_HEADER_STRIDE(out_crop), SBWCL_HEADER_STRIDE(out_crop),
		SBWCL_8B_STRIDE(input_width, d_frame.comp_rate), SBWCL_8B_STRIDE(input_width, d_frame.comp_rate),
		SBWCL_HEADER_STRIDE(input_width), SBWCL_HEADER_STRIDE(input_width));
}

static void camerapp_hw_gdc_core_param_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_crop_param *crop_param = tctx->crop_param;
	struct gdc_dev *gdc = tctx->gdc;
	u32 val;

	/* 8bit */
	crop_param->is_grid_mode = 0;
	crop_param->is_bypass_mode = 0;
	*(u32 *)(tctx->addr + GDC_R_YUV_GDC_YUV_FORMAT) = CAMERA_PIXEL_SIZE_8BIT << 1;

	func->camerapp_hw_gdc_set_core_param(gdc->pmio, crop_param);

	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_INTERPOLATION);
	KUNIT_EXPECT_EQ(test, val, (u32)0x23);
	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_BOUNDARY_OPTION);
	KUNIT_EXPECT_EQ(test, val, (u32)0x1);
	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_GRID_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)crop_param->is_grid_mode);
	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_YUV_FORMAT);
	KUNIT_EXPECT_EQ(test, val, (u32)0x0);
	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_LUMA_MINMAX);
	KUNIT_EXPECT_EQ(test, val, (u32)0xFF0000);
	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_CHROMA_MINMAX);
	KUNIT_EXPECT_EQ(test, val, (u32)0xFF0000);

	/* 10bit */
	crop_param->is_grid_mode = 1;
	crop_param->is_bypass_mode = 1;
	*(u32 *)(tctx->addr + GDC_R_YUV_GDC_YUV_FORMAT) = CAMERA_PIXEL_SIZE_10BIT << 1;

	func->camerapp_hw_gdc_set_core_param(gdc->pmio, crop_param);

	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_INTERPOLATION);
	KUNIT_EXPECT_EQ(test, val, (u32)0x23);
	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_BOUNDARY_OPTION);
	KUNIT_EXPECT_EQ(test, val, (u32)0x1);
	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_GRID_MODE);
	KUNIT_EXPECT_EQ(test, val, (u32)crop_param->is_grid_mode);
	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_YUV_FORMAT);
	KUNIT_EXPECT_EQ(test, val, (u32)0x42);
	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_LUMA_MINMAX);
	KUNIT_EXPECT_EQ(test, val, (u32)0x3FF0000);
	val= GDC_GET_R(gdc, GDC_R_YUV_GDC_CHROMA_MINMAX);
	KUNIT_EXPECT_EQ(test, val, (u32)0x3FF0000);
}

static void camerapp_hw_gdc_update_param_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;
	struct gdc_ctx *ctx;

	ctx = kunit_kzalloc(test, sizeof(struct gdc_ctx), 0);

	ctx->s_frame.width = 320;
	ctx->s_frame.height = 240;
	ctx->s_frame.gdc_fmt = &gdc_formats[1];
	ctx->d_frame.width = 320;
	ctx->d_frame.height = 240;
	ctx->d_frame.gdc_fmt = &gdc_formats[1];
	ctx->cur_index = 0;
	ctx->crop_param[ctx->cur_index].is_grid_mode = 0;
	ctx->crop_param[ctx->cur_index].is_bypass_mode = 0;
	ctx->crop_param[ctx->cur_index].crop_start_x = 0;
	ctx->crop_param[ctx->cur_index].crop_start_y = 0;
	ctx->crop_param[ctx->cur_index].crop_width = 320;
	ctx->crop_param[ctx->cur_index].crop_height = 240;
	gdc->current_ctx = ctx;

	func->camerapp_hw_gdc_update_param(gdc->pmio, gdc);

	kunit_kfree(test, ctx);
}

static void camerapp_hw_gdc_init_pmio_config_kunit_test(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;

	func->camerapp_hw_gdc_init_pmio_config(gdc);
}

static void camerapp_hw_gdc_g_reg_cnt_kunit_test(struct kunit *test)
{
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	int reg_cnt;

	reg_cnt = func->camerapp_hw_gdc_g_reg_cnt();
	KUNIT_EXPECT_EQ(test, reg_cnt, GDC_REG_CNT);
}

static void __gdc_init_pmio(struct kunit *test)
{
	struct cameraepp_hw_api_gdc_kunit_test_ctx *tctx = test->priv;
	struct camerapp_kunit_hw_gdc_func *func = camerapp_kunit_get_hw_gdc_test();
	struct gdc_dev *gdc = tctx->gdc;
	int ret;
	u32 reg_cnt;

	gdc->regs_base = tctx->addr;

	func->camerapp_hw_gdc_init_pmio_config(gdc);
	gdc->pmio_config.cache_type = PMIO_CACHE_NONE; /* override for kunit test */

	gdc->pmio = pmio_init(NULL, NULL, &gdc->pmio_config);

	ret = pmio_field_bulk_alloc(gdc->pmio, &gdc->pmio_fields,
					gdc->pmio_config.fields,
					gdc->pmio_config.num_fields);
	if (ret)
		return;

	reg_cnt = func->camerapp_hw_gdc_g_reg_cnt();

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
	KUNIT_CASE(camerapp_hw_gdc_votf_en_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_get_sbwc_const_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_has_comp_header_kunit_test),
	KUNIT_CASE(camerapp_hw_get_comp_buf_size_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_set_initialization_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_update_scale_parameters_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_update_grid_param_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_set_compressor_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_set_format_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_set_dma_address_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_update_dma_size_kunit_test),
	KUNIT_CASE(camerapp_hw_gdc_core_param_kunit_test),
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
	.name = "pablo-hw-api-gdc-v1011-kunit-test",
	.init = camerapp_hw_api_gdc_kunit_test_init,
	.exit = camerapp_hw_api_gdc_kunit_test_exit,
	.test_cases = camerapp_hw_api_gdc_kunit_test_cases,
};
define_pablo_kunit_test_suites(&camerapp_hw_api_gdc_kunit_test_suite);

MODULE_LICENSE("GPL");
