// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <videodev2_exynos_camera.h>

#include "is-video.h"
#include "pablo-debug.h"

static void setup_plane_widths(struct is_frame_cfg *fc, u32 widths[], int cnt)
{
	int p;

	for (p = 0; p < cnt ; p++)
		widths[p] = max(fc->width * fc->format->bitsperpixel[p]
						/ BITS_PER_BYTE,
			       fc->bytesperline[p]);
}

/*
 * V4L2_PIX_FMT_YUV444, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_UYVY,
 * V4L2_PIX_FMT_Y10, V4L2_PIX_FMT_Y12,
 * V4L2_PIX_FMT_JPEG,
 * V4L2_PIX_FMT_Z16,
 */
static void widths0_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 1);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = widths[0] * fc->height;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_Y10BPACK */
static void aligned_widths0_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 1);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = ALIGN(widths[0], 16) * fc->height;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV16, V4L2_PIX_FMT_NV61 */
static void widths01_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = (widths[0] + widths[1]) * fc->height;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_NV21 */
static void widths01_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = widths[0] * fc->height
				+ widths[1] * fc->height / 2;

	sizes[p] = SIZE_OF_META_PLANE;
}

/*
 * V4L2_PIX_FMT_NV16M, V4L2_PIX_FMT_NV61M,
 * V4L2_PIX_FMT_Y8I
 */
static void widths01_2p_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = widths[0] * fc->height;
		sizes[p + 1] = widths[1] * fc->height;
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12M, V4L2_PIX_FMT_NV21M */
static void widths01_2p_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = widths[0] * fc->height;
		sizes[p + 1] = widths[1] * fc->height / 2;
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12M_SBWCL_32_8B, V4L2_PIX_FMT_NV12M_SBWCL_32_10B */
static void sbwcl_align_32b_2p_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p += 2) {
		sizes[p] = SBWCL_32_Y_SIZE(fc->width, fc->height) +
			SBWCL_Y_HEADER_SIZE(fc->width, fc->height);
		sizes[p + 1] = SBWCL_32_CBCR_SIZE(fc->width, fc->height) +
			SBWCL_CBCR_HEADER_SIZE(fc->width, fc->height);
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12M_SBWCL_64_8B, V4L2_PIX_FMT_NV12M_SBWCL_64_10B */
static void sbwcl_align_64b_2p_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p += 2) {
		sizes[p] = SBWCL_64_Y_SIZE(fc->width, fc->height) +
			SBWCL_Y_HEADER_SIZE(fc->width, fc->height);
		sizes[p + 1] = SBWCL_64_CBCR_SIZE(fc->width, fc->height) +
			SBWCL_CBCR_HEADER_SIZE(fc->width, fc->height);
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12M_SBWCL_64_8B_FR, V4L2_PIX_FMT_NV12M_SBWCL_64_10B_FR */
static void sbwcl_align_64b_fr_2p_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p += 2) {
		sizes[p] = SBWCL_64_Y_SIZE_FR(fc->width, fc->height) +
			SBWCL_Y_HEADER_SIZE(fc->width, fc->height);
		sizes[p + 1] = SBWCL_64_CBCR_SIZE_FR(fc->width, fc->height) +
			SBWCL_CBCR_HEADER_SIZE(fc->width, fc->height);
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV16M_P210 */
static void sbwcl_align_64b_2p_422_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = SBWCL_64_Y_SIZE(fc->width, fc->height) +
			SBWCL_Y_HEADER_SIZE(fc->width, fc->height);
		sizes[p + 1] = sizes[p];
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV16M_P210 */
static void aligned_widths01_2p_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;
	u32 sbwc_type = (fc->hw_pixeltype & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;

	dbg("%s, w:%d x h:%d, SBWC(%d)\n", fmt->name, fc->width, fc->height, sbwc_type);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p+=2) {
		if (sbwc_type) {
			sizes[p] = SBWCL_32_Y_SIZE(fc->width, fc->height) +
				SBWCL_Y_HEADER_SIZE(fc->width, fc->height);
			sizes[p + 1] = SBWCL_32_CBCR_SIZE(fc->width, fc->height) +
				SBWCL_CBCR_HEADER_SIZE(fc->width, fc->height);
		} else {
			sizes[p] = ALIGN(widths[0], 16) * fc->height;
			sizes[p + 1] = ALIGN(widths[1], 16) * fc->height;
		}
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12M_P010 */
static void aligned_widths01_2p_420_sps(struct is_frame_cfg *fc,
					unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = ALIGN(widths[0], 16) * fc->height;
		sizes[p + 1] = ALIGN(widths[1], 16) * fc->height / 2;
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV16M_S10B */
static void aligned_2p_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = NV16M_Y_SIZE(fc->width, fc->height)
				+ NV16M_Y_2B_SIZE(fc->width, fc->height);
		sizes[p + 1] = NV16M_CBCR_SIZE(fc->width, fc->height)
				+ NV16M_CBCR_2B_SIZE(fc->width, fc->height);
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_NV12M_S10B */
static void aligned_2p_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = NV12M_Y_SIZE(fc->width, fc->height)
				+ NV12M_Y_2B_SIZE(fc->width, fc->height);
		sizes[p + 1] = NV12M_CBCR_SIZE(fc->width, fc->height)
				+ NV12M_CBCR_2B_SIZE(fc->width, fc->height);
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_YUV422P, V4L2_PIX_FMT_YUV420, V4L2_PIX_FMT_YVU420 */
static void widths012_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 3);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = widths[0] * fc->height
				+ widths[1] * fc->height / 2
				+ widths[2] * fc->height / 2;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_YUV420M, V4L2_PIX_FMT_YVU420M */
static void widths012_3p_420_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 3);
	for (p = 0; p < num_i_planes; p+=3) {
		sizes[p] = widths[0] * fc->height;
		sizes[p + 1] = widths[1] * fc->height / 2;
		sizes[p + 2] = widths[2] * fc->height / 2;
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_YUV32 */
static void widthsp_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, num_i_planes);
	for (p = 0; p < num_i_planes; p++)
		sizes[p] = widths[p] * fc->height;

	sizes[p] = SIZE_OF_META_PLANE;
}

/*
 * V4L2_PIX_FMT_SGRBG8, V4L2_PIX_FMT_SBGGR8,
 * V4L2_PIX_FMT_GREY
 */
static void default_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = fc->width * fc->height;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_RGB24, V4L2_PIX_FMT_BGR24 */
static void default_x3_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = fc->width * fc->height * 3;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_RGB32 */
static void default_x4_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = fc->width * fc->height * 4;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SBGGR10, V4L2_PIX_FMT_SBGGR10P */
#define ALIGN_SBGGR10(w) ALIGN(((w) * 5) >> 2, 16)
static void sbggr10x_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++) {
		sizes[p] = ALIGN_SBGGR10(fc->width) * fc->height;
		if (fc->bytesperline[0]) {
			if (fc->bytesperline[0] >= ALIGN_SBGGR10(fc->width))
				sizes[p] = fc->bytesperline[0] * fc->height;
			else
				err("bytesperline is too small"
					" (%s, width: %d, bytesperline: %d)",
						fmt->name, fc->width,
						fc->bytesperline[0]);
		}
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SBGGR12, V4L2_PIX_FMT_SBGGR12P */
#define ALIGN_SBGGR12(w) ALIGN(((w) * 3) >> 1, 16)
static void sbggr12x_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++) {
		sizes[p] = ALIGN_SBGGR12(fc->width) * fc->height;
		if (fc->bytesperline[0]) {
			if (fc->bytesperline[0] >= ALIGN_SBGGR12(fc->width))
				sizes[p] = fc->bytesperline[0] * fc->height;
			else
				err("bytesperline is too small"
					" (%s, width: %d, bytesperline: %d)",
						fmt->name, fc->width,
						fc->bytesperline[0]);
		}
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SBGGR16 */
static void sbggr16_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++) {
		sizes[p] = fc->width * fc->height * 2;
		if (fc->bytesperline[0]) {
			if (fc->bytesperline[0] >= fc->width * 2)
				sizes[p] = fc->bytesperline[0] * fc->height;
			else
				err("bytesperline is too small"
					" (%s, width: %d, bytesperline: %d)",
						fmt->name, fc->width,
						fc->bytesperline[0]);
		}
	}

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SRGB24_SP */
static void srgb24_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = ALIGN(fc->width * fc->height, 16) * 3;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SRGB36P_SP */
static void srgb36p_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = ALIGN(fc->width * fc->height * 12 / 8, 16) * 3;

	sizes[p] = SIZE_OF_META_PLANE;
}

/* V4L2_PIX_FMT_SRGB36_SP */
static void srgb36_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	for (p = 0; p < num_i_planes; p++)
		sizes[p] = ALIGN(fc->width * fc->height * 2, 16) * 3;

	sizes[p] = SIZE_OF_META_PLANE;
}

static struct is_fmt is_formats[] = {
	{
		.name		= "YUV 4:4:4 packed, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV444,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= 0, /* Not Defined */
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_INOUT_FORMAT_YUV444,
		.hw_order	= DMA_INOUT_ORDER_YCbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_YCbYCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_YCbYCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbYCrY,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths01_sps,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths01_sps,
	}, {
		.name		= "YUV 4:2:2 non-contiguous 2-planar,, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths01_2p_sps,
	}, {
		.name		= "YUV 4:2:2 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV61M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths01_2p_sps,
	}, {
		.name		= "YUV 4:2:2 planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths012_sps,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths012_sps,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths012_sps,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths01_420_sps,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths01_420_sps,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths01_2p_420_sps,
	}, {
		.name		= "YVU 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths01_2p_420_sps,
	}, {
		.name		= "YUV 4:2:0 2 Planes 8bit Compress Lossy 32B align",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_32_8B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= COMP_LOSS,
		.sbwc_align	= 32,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz = sbwcl_align_32b_2p_420_sps,
	}, {
		.name		= "YUV 4:2:0 2 Planes 8bit Compress Lossy 64B align",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_64_8B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= COMP_LOSS,
		.sbwc_align	= 64,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz = sbwcl_align_64b_2p_420_sps,
	}, {
		.name		= "NV12M 8bit SBWC Lossy 64B align footprint reduction",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_64_8B_FR,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= COMP_LOSS,
		.sbwc_align	= 64,
		.sbwc_lossy_fr	= 1,
		.setup_plane_sz = sbwcl_align_64b_fr_2p_420_sps,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.num_planes	= 3 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths012_3p_420_sps,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.num_planes	= 3 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths012_3p_420_sps,
	}, {
		.name		= "BAYER 8 bit(GRBG)",
		.pixelformat	= V4L2_PIX_FMT_SGRBG8,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT, /* memory bitwidth */
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= default_sps,
	}, {
		.name		= "BAYER 8 bit(BA81)",
		.pixelformat	= V4L2_PIX_FMT_SBGGR8,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT, /* memory bitwidth */
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= default_sps,
	}, {
		.name		= "BAYER 10 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT, /* memory bitwidth */
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= sbggr10x_sps,
	}, {
		.name		= "BAYER 12 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT, /* memory bitwidth */
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= sbggr12x_sps,
	}, {
		.name		= "BAYER 16 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT, /* memory bitwidth */
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= sbggr16_sps,
	}, {
		.name		= "BAYER 10 bit packed",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER_PACKED,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= sbggr10x_sps,

	}, {
		.name		= "BAYER 12 bit packed",
		.pixelformat	= V4L2_PIX_FMT_SBGGR12P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_INOUT_FORMAT_BAYER_PACKED,
		.hw_order	= DMA_INOUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_12BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= sbggr12x_sps,
	}, {
		.name		= "ARGB32",
		.pixelformat	= V4L2_PIX_FMT_ARGB32,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_ARGB,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= default_x4_sps,
	}, {
		.name		= "BGRA32",
		.pixelformat	= V4L2_PIX_FMT_BGRA32,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_BGRA,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= default_x4_sps,
	}, {
		.name		= "RGBA32",
		.pixelformat	= V4L2_PIX_FMT_RGBA32,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_RGBA,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= default_x4_sps,
	}, {
		.name		= "ABGR32",
		.pixelformat	= V4L2_PIX_FMT_ABGR32,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_BGRA,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= default_x4_sps,
	}, {
		.name		= "RGB24 planar",
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_BGR,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= default_x3_sps,
	}, {
		.name		= "BGR24 planar",
		.pixelformat	= V4L2_PIX_FMT_BGR24,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_BGR,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= default_x3_sps,
	}, {
		.name		= "BAYER 12bit packed single plane",
		.pixelformat	= V4L2_PIX_FMT_SRGB36P_SP,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_12BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= srgb36p_sps,
	}, {
		.name		= "BAYER 12bit unpacked single plane",
		.pixelformat	= V4L2_PIX_FMT_SRGB36_SP,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= srgb36_sps,
	}, {
		.name		= "BAYER 8bit single plane",
		.pixelformat	= V4L2_PIX_FMT_SRGB24_SP,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_3,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= srgb24_sps,
	}, {
		.name		= "RGB24 interleaved",
		.pixelformat	= V4L2_PIX_FMT_RGB24I_SP,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_INOUT_FORMAT_RGB,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_24BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= srgb24_sps,
	}, {
		.name		= "Y 8bit",
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= default_sps,
	}, {
		.name		= "Y 10bit",
		.pixelformat	= V4L2_PIX_FMT_Y10,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "Y 12bit",
		.pixelformat	= V4L2_PIX_FMT_Y12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "Y 14bit",
		.pixelformat	= V4L2_PIX_FMT_Y14,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 14 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "Y Packed 10bit",
		.pixelformat	= V4L2_PIX_FMT_Y10BPACK,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= aligned_widths0_sps,
	}, {
		.name		= "Y Packed 14bit",
		.pixelformat	= V4L2_PIX_FMT_Y14P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 14 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_14BIT,
		.hw_plane	= DMA_INOUT_PLANE_1,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= aligned_widths0_sps,
	}, {
		.name		= "Y L/R interleaved 8bit",
		.pixelformat	= V4L2_PIX_FMT_Y8I,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths01_2p_sps,
	}, {
		.name		= "YUV32",
		.pixelformat	= V4L2_PIX_FMT_YUV32,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_Y,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widthsp_sps,
	}, {
		.name		= "P210_16B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= aligned_widths01_2p_sps,
	}, {
		.name		= "P210_12B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12, 12 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_12BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= aligned_widths01_2p_sps,
	}, {
		.name		= "P210_10B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10, 10 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= aligned_widths01_2p_sps,
	}, {
		.name		= "P010_16B",
		.pixelformat	= V4L2_PIX_FMT_NV12M_P010,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= aligned_widths01_2p_420_sps,
	}, {
		.name		= "P010_10B",
		.pixelformat	= V4L2_PIX_FMT_NV12M_P010,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10, 10 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= aligned_widths01_2p_420_sps,
	}, {
		.name		= "P010_16B Lossy 10bit 32B align",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_32_10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= COMP_LOSS,
		.sbwc_align	= 32,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz = sbwcl_align_32b_2p_420_sps,
	}, {
		.name		= "P010_16B Lossy 10bit 64B align",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_64_10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= COMP_LOSS,
		.sbwc_align	= 64,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz = sbwcl_align_64b_2p_420_sps,
	}, {
		.name		= "NV12M 10bit SBWC Lossy 64B align footprint reduction",
		.pixelformat	= V4L2_PIX_FMT_NV12M_SBWCL_64_10B_FR,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= COMP_LOSS,
		.sbwc_align	= 64,
		.sbwc_lossy_fr	= 1,
		.setup_plane_sz = sbwcl_align_64b_fr_2p_420_sps,
	}, {
		.name		= "P210_16B Lossy 10bit 64B align",
		.pixelformat	= V4L2_PIX_FMT_NV16M_SBWCL_64_10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_INOUT_PLANE_2,
		.sbwc_type	= COMP_LOSS,
		.sbwc_align	= 64,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz = sbwcl_align_64b_2p_422_sps,
	}, {
		.name		= "YUV422 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV16M_S10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV422,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_4,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= aligned_2p_sps,
	}, {
		.name		= "YUV420 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV12M_S10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_INOUT_FORMAT_YUV420,
		.hw_order	= DMA_INOUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_INOUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_INOUT_PLANE_4,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= aligned_2p_420_sps,
	}, {
		.name		= "JPEG",
		.pixelformat	= V4L2_PIX_FMT_JPEG,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_JPEG_1X8,
		.bitsperpixel	= { 8 },
		.hw_format	= 0,
		.hw_order	= 0,
		.hw_bitwidth	= 0,
		.hw_plane	= 0,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "DEPTH",
		.pixelformat	= V4L2_PIX_FMT_Z16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= 0,
		.hw_order	= DMA_INOUT_ORDER_NO,
		.hw_bitwidth	= 0,
		.hw_plane	= 0,
		.sbwc_type	= NONE,
		.sbwc_align	= 0,
		.sbwc_lossy_fr	= 0,
		.setup_plane_sz	= widths0_sps,
	}
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct is_fmt *pablo_kunit_get_is_formats_struct(ulong index) {
	if (ARRAY_SIZE(is_formats) <= index)
		return NULL;

	return &is_formats[index];
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_is_formats_struct);

ulong pablo_kunit_get_array_size_is_formats(void) {
	return ARRAY_SIZE(is_formats);
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_array_size_is_formats);
#endif

struct is_fmt *is_find_format(u32 pixelformat,
	u32 flags)
{
	ulong i;
	struct is_fmt *result, *fmt;
	u8 pixel_size;
	u32 memory_bitwidth;

	if (!pixelformat) {
		err("pixelformat is null");
		return NULL;
	}

	pixel_size = flags & PIXEL_TYPE_SIZE_MASK;
	if (pixel_size == CAMERA_PIXEL_SIZE_10BIT ||
			pixel_size == CAMERA_PIXEL_SIZE_12BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_16BIT;
	else if (pixel_size == CAMERA_PIXEL_SIZE_PACKED_12BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_12BIT;
	else if (pixel_size == CAMERA_PIXEL_SIZE_PACKED_10BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	else if (pixel_size == CAMERA_PIXEL_SIZE_8_2BIT)
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_10BIT;
	else
		memory_bitwidth = DMA_INOUT_BIT_WIDTH_8BIT;

	result = NULL;
	for (i = 0; i < ARRAY_SIZE(is_formats); ++i) {
		fmt = &is_formats[i];
		if (fmt->pixelformat == pixelformat) {
			if (pixelformat == V4L2_PIX_FMT_NV16M_P210
			    || pixelformat == V4L2_PIX_FMT_NV12M_P010
			    || pixelformat == V4L2_PIX_FMT_YUYV) {
				if (fmt->hw_bitwidth != memory_bitwidth)
					continue;
			}
			result = fmt;
			break;
		}
	}

	return result;
}
EXPORT_SYMBOL_GPL(is_find_format);

MODULE_DESCRIPTION("format module for Exynos Pablo");
MODULE_LICENSE("GPL v2");
