/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>
#include <linux/videodev2_exynos_media.h>
#include <linux/dma-buf.h>
#include <linux/moduleparam.h>

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>

#include "is-time.h"
#include "is-core.h"
#include "is-param.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-debug.h"
#include "pablo-mem.h"
#include "is-video.h"

#define NUM_OF_META_PLANE	1
#if defined(META_ITF_VER_20192002)
#define SIZE_OF_META_PLANE	SZ_32K
#else
#define SIZE_OF_META_PLANE	(SZ_32K + SZ_16K + SZ_2K) /* 50KB */
#endif

/*
 * copy from 'include/media/v4l2-ioctl.h'
 * #define V4L2_DEV_DEBUG_IOCTL		0x01
 * #define V4L2_DEV_DEBUG_IOCTL_ARG	0x02
 * #define V4L2_DEV_DEBUG_FOP		0x04
 * #define V4L2_DEV_DEBUG_STREAMING	0x08
 * #define V4L2_DEV_DEBUG_POLL		0x10
 */
#define V4L2_DEV_DEBUG_DMA	0x100

#define DDD_CTRL_IMAGE		0x01
#define DDD_CTRL_META		0x02
#define DDD_CTRL_TYPE_MASK

#define DDD_TYPE_PERIOD		0 /* all frame count matching period	 */
#define DDD_TYPE_INTERVAL	1 /* from any frame count to greater one */
#define DDD_TYPE_ONESHOT	2 /* specific frame count only		 */

extern unsigned int dbg_draw_digit_ctrl;
unsigned int dbg_dma_dump_ctrl;
unsigned int dbg_dma_dump_type = DDD_TYPE_PERIOD;
unsigned int dbg_dma_dump_arg1 = 30;	/* every frame(s) */
unsigned int dbg_dma_dump_arg2;		/* for interval type */

static int param_get_dbg_dma_dump_ctrl(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "DMA dump control: ");

	if (!dbg_dma_dump_ctrl) {
		ret += sprintf(buffer + ret, "None\n");
		ret += sprintf(buffer + ret, "\t- image(0x1)\n");
		ret += sprintf(buffer + ret, "\t- meta (0x2)\n");
	} else {
		if (dbg_dma_dump_ctrl & DDD_CTRL_IMAGE)
			ret += sprintf(buffer + ret, "dump image(0x1) | ");
		if (dbg_dma_dump_ctrl & DDD_CTRL_META)
			ret += sprintf(buffer + ret, "dump meta (0x2) | ");

		ret -= 3;
		ret += sprintf(buffer + ret, "\n");
	}

	return ret;
}

static const struct kernel_param_ops param_ops_dbg_dma_dump_ctrl = {
	.set = param_set_uint,
	.get = param_get_dbg_dma_dump_ctrl,
};

static int param_get_dbg_dma_dump_type(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "DMA dump type: selected(*)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_PERIOD ?
				"\t- period(0)*\n" : "\t- period(0)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_INTERVAL ?
				"\t- interval(1)*\n" : "\t- interval(1)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_ONESHOT ?
				"\t- oneshot(2)*\n" : "\t- oneshot(2)\n");

	return ret;
}

static const struct kernel_param_ops param_ops_dbg_dma_dump_type = {
	.set = param_set_uint,
	.get = param_get_dbg_dma_dump_type,
};

module_param_cb(dma_dump_ctrl, &param_ops_dbg_dma_dump_ctrl,
			&dbg_dma_dump_ctrl, S_IRUGO | S_IWUSR);
module_param_cb(dma_dump_type, &param_ops_dbg_dma_dump_type,
			&dbg_dma_dump_type, S_IRUGO | S_IWUSR);
module_param_named(dma_dump_arg1, dbg_dma_dump_arg1, uint,
			S_IRUGO | S_IWUSR);
module_param_named(dma_dump_arg2, dbg_dma_dump_arg2, uint,
			S_IRUGO | S_IWUSR);

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

/* V4L2_PIX_FMT_NV16M_P210 */
static void aligned_widths01_2p_sps(struct is_frame_cfg *fc, unsigned int sizes[])
{
	struct is_fmt *fmt = fc->format;
	int num_i_planes = fmt->num_planes - NUM_OF_META_PLANE;
	u32 widths[IS_MAX_PLANES];
	int p;

	dbg("%s, w:%d x h:%d\n", fmt->name, fc->width, fc->height);

	setup_plane_widths(fc, widths, 2);
	for (p = 0; p < num_i_planes; p+=2) {
		sizes[p] = ALIGN(widths[0], 16) * fc->height;
		sizes[p + 1] = ALIGN(widths[1], 16) * fc->height;
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
		.hw_format	= DMA_OUTPUT_FORMAT_YUV444,
		.hw_order	= DMA_OUTPUT_ORDER_YCbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_YCbYCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_YUYV8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_YCbYCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.mbus_code	= MEDIA_BUS_FMT_UYVY8_2X8,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CbYCrY,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= widths01_sps,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= widths01_sps,
	}, {
		.name		= "YUV 4:2:2 non-contiguous 2-planar,, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= widths01_2p_sps,
	}, {
		.name		= "YUV 4:2:2 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV61M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= widths01_2p_sps,
	}, {
		.name		= "YUV 4:2:2 planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_3,
		.setup_plane_sz	= widths012_sps,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_3,
		.setup_plane_sz	= widths012_sps,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_3,
		.setup_plane_sz	= widths012_sps,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= widths01_420_sps,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= widths01_420_sps,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= widths01_2p_420_sps,
	}, {
		.name		= "YVU 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= widths01_2p_420_sps,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.num_planes	= 3 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_3,
		.setup_plane_sz	= widths012_3p_420_sps,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.num_planes	= 3 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_3,
		.setup_plane_sz	= widths012_3p_420_sps,
	}, {
		.name		= "BAYER 8 bit(GRBG)",
		.pixelformat	= V4L2_PIX_FMT_SGRBG8,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= default_sps,
	}, {
		.name		= "BAYER 8 bit(BA81)",
		.pixelformat	= V4L2_PIX_FMT_SBGGR8,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= default_sps,
	}, {
		.name		= "BAYER 10 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= sbggr10x_sps,
	}, {
		.name		= "BAYER 12 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_12BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= sbggr12x_sps,
	}, {
		.name		= "BAYER 16 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= sbggr16_sps,
	}, {
		.name		= "BAYER 10 bit packed",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER_PACKED,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= sbggr10x_sps,
	}, {
		.name		= "BAYER 12 bit packed",
		.pixelformat	= V4L2_PIX_FMT_SBGGR12P,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER_PACKED,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_12BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= sbggr12x_sps,
	}, {
		.name		= "ARGB8888",
		.pixelformat	= V4L2_PIX_FMT_RGB32,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= DMA_OUTPUT_FORMAT_RGB,
		.hw_order	= DMA_OUTPUT_ORDER_ARGB,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= default_x4_sps,
	}, {
		.name		= "RGB888 planar",
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_OUTPUT_FORMAT_RGB,
		.hw_order	= DMA_OUTPUT_ORDER_BGR,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_3,
		.setup_plane_sz	= default_x3_sps,
	}, {
		.name		= "BGR888 planar",
		.pixelformat	= V4L2_PIX_FMT_BGR24,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_OUTPUT_FORMAT_RGB,
		.hw_order	= DMA_OUTPUT_ORDER_BGR,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_3,
		.setup_plane_sz	= default_x3_sps,
	}, {
		.name		= "Y 8bit",
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_Y,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= default_sps,
	}, {
		.name		= "Y 10bit",
		.pixelformat	= V4L2_PIX_FMT_Y10,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_Y,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "Y 12bit",
		.pixelformat	= V4L2_PIX_FMT_Y12,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_Y,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "Y Packed 10bit",
		.pixelformat	= V4L2_PIX_FMT_Y10BPACK,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_OUTPUT_FORMAT_Y,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_1,
		.setup_plane_sz	= aligned_widths0_sps,
	}, {
		.name		= "YUV32",
		.pixelformat	= V4L2_PIX_FMT_YUV32,
		.num_planes = 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_Y,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_32BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= widthsp_sps,
	}, {
		.name		= "P210_16B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= aligned_widths01_2p_sps,
	}, {
		.name		= "P210_10B",
		.pixelformat	= V4L2_PIX_FMT_NV16M_P210,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10, 10 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= aligned_widths01_2p_sps,
	}, {
		.name		= "P010_16B",
		.pixelformat	= V4L2_PIX_FMT_NV12M_P010,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 16, 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_16BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= aligned_widths01_2p_420_sps,
	}, {
		.name		= "P010_10B",
		.pixelformat	= V4L2_PIX_FMT_NV12M_P010,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 10, 10 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_2,
		.setup_plane_sz	= aligned_widths01_2p_420_sps,
	}, {
		.name		= "YUV422 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV16M_S10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_4,
		.setup_plane_sz	= aligned_2p_sps,
	}, {
		.name		= "YUV420 2P 10bit(8+2)",
		.pixelformat	= V4L2_PIX_FMT_NV12M_S10B,
		.num_planes	= 2 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_10BIT,
		.hw_plane	= DMA_OUTPUT_PLANE_4,
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
		.setup_plane_sz	= widths0_sps,
	}, {
		.name		= "DEPTH",
		.pixelformat	= V4L2_PIX_FMT_Z16,
		.num_planes	= 1 + NUM_OF_META_PLANE,
		.bitsperpixel	= { 32 },
		.hw_format	= 0,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= 0,
		.hw_plane	= 0,
		.setup_plane_sz	= widths0_sps,
	}

};

struct is_fmt *is_find_format(u32 pixelformat,
	u32 flags)
{
	ulong i;
	struct is_fmt *result, *fmt;
	u8 pixel_size;
	u32 memory_bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT;

	result = NULL;

	if (!pixelformat) {
		err("pixelformat is null");
		goto p_err;
	}

	pixel_size = flags & PIXEL_TYPE_SIZE_MASK;
	if (pixel_size == CAMERA_PIXEL_SIZE_10BIT)
		memory_bitwidth = DMA_OUTPUT_BIT_WIDTH_16BIT;
	else if (pixel_size == CAMERA_PIXEL_SIZE_PACKED_10BIT)
		memory_bitwidth = DMA_OUTPUT_BIT_WIDTH_10BIT;
	else if (pixel_size == CAMERA_PIXEL_SIZE_8_2BIT)
		memory_bitwidth = DMA_OUTPUT_BIT_WIDTH_10BIT;

	for (i = 0; i < ARRAY_SIZE(is_formats); ++i) {
		fmt = &is_formats[i];
		if (fmt->pixelformat == pixelformat) {
			if (pixelformat == V4L2_PIX_FMT_NV16M_P210
				|| pixelformat == V4L2_PIX_FMT_NV12M_P010
				||  pixelformat == V4L2_PIX_FMT_YUYV) {
				if (fmt->hw_bitwidth != memory_bitwidth)
					continue;
			}

			result = fmt;
			break;
		}
	}

p_err:
	return result;
}

static inline void vref_init(struct is_video *video)
{
	atomic_set(&video->refcount, 0);
}

static inline int vref_get(struct is_video *video)
{
	return atomic_inc_return(&video->refcount) - 1;
}

static inline int vref_put(struct is_video *video,
	void (*release)(struct is_video *video))
{
	int ret = 0;

	ret = atomic_sub_and_test(1, &video->refcount);
	if (ret)
		pr_debug("closed all instacne");

	return atomic_read(&video->refcount);
}

static int queue_init(void *priv, struct vb2_queue *vbq,
	struct vb2_queue *vbq_dst)
{
	int ret = 0;
	struct is_video_ctx *vctx = priv;
	struct is_video *video;
	u32 type;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!vbq);

	video = GET_VIDEO(vctx);

	if (video->type == IS_VIDEO_TYPE_CAPTURE)
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	vbq->type		= type;
	vbq->io_modes		= VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	vbq->drv_priv		= vctx;
	vbq->buf_struct_size	= sizeof(struct is_vb2_buf);
	vbq->ops		= vctx->vb2_ops;
	vbq->mem_ops		= vctx->vb2_mem_ops;
	vbq->timestamp_flags	= V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(vbq);
	if (ret) {
		mverr("vb2_queue_init fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->queue.vbq = vbq;

p_err:
	return ret;
}

int open_vctx(struct file *file,
	struct is_video *video,
	struct is_video_ctx **vctx,
	u32 instance,
	ulong id,
	const char *name)
{
	int ret = 0;

	FIMC_BUG(!file);
	FIMC_BUG(!video);

	if (atomic_read(&video->refcount) > IS_MAX_NODES) {
		err("[V%02d] can't open vctx, refcount is invalid", video->id);
		ret = -EINVAL;
		goto p_err;
	}

	*vctx = vzalloc(sizeof(struct is_video_ctx));
	if (*vctx == NULL) {
		err("[V%02d] vzalloc is fail", video->id);
		ret = -ENOMEM;
		goto p_err;
	}

	(*vctx)->refcount = vref_get(video);
	(*vctx)->instance = instance;
	(*vctx)->queue.id = id;
	snprintf((*vctx)->queue.name, sizeof((*vctx)->queue.name), "%s", name);
	(*vctx)->state = BIT(IS_VIDEO_CLOSE);

	file->private_data = *vctx;

p_err:
	return ret;
}

int close_vctx(struct file *file,
	struct is_video *video,
	struct is_video_ctx *vctx)
{
	int ret = 0;

	vfree(vctx);
	file->private_data = NULL;
	ret = vref_put(video, NULL);

	return ret;
}

/*
 * =============================================================================
 * Queue Opertation
 * =============================================================================
 */

static int is_queue_open(struct is_queue *queue,
	u32 rdycount)
{
	int ret = 0;

	queue->buf_maxcount = 0;
	queue->buf_refcount = 0;
	queue->buf_rdycount = rdycount;
	queue->buf_req = 0;
	queue->buf_pre = 0;
	queue->buf_que = 0;
	queue->buf_com = 0;
	queue->buf_dqe = 0;
	clear_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);
	clear_bit(IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(IS_QUEUE_STREAM_ON, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state);
	memset(&queue->framecfg, 0, sizeof(struct is_frame_cfg));
	frame_manager_probe(&queue->framemgr, queue->id, queue->name);

	return ret;
}

static int is_queue_close(struct is_queue *queue)
{
	int ret = 0;

	queue->buf_maxcount = 0;
	queue->buf_refcount = 0;
	clear_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);
	clear_bit(IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(IS_QUEUE_STREAM_ON, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state);
	clear_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state);
	frame_manager_close(&queue->framemgr);

	return ret;
}

static int is_queue_set_format_mplane(struct is_video_ctx *vctx,
	struct is_queue *queue,
	void *device,
	struct v4l2_format *format)
{
	int ret = 0;
	u32 plane;
	struct v4l2_pix_format_mplane *pix;
	struct is_fmt *fmt;
	struct is_video *video;

	FIMC_BUG(!queue);

	video = GET_VIDEO(vctx);

	pix = &format->fmt.pix_mp;
	fmt = is_find_format(pix->pixelformat, pix->flags);
	if (!fmt) {
		mverr("[%s] pixel format is not found", vctx, video, queue->name);
		ret = -EINVAL;
		goto p_err;
	}

	queue->framecfg.format			= fmt;
	queue->framecfg.colorspace		= pix->colorspace;
	queue->framecfg.quantization	= pix->quantization;
	queue->framecfg.width			= pix->width;
	queue->framecfg.height			= pix->height;

	if (fmt->hw_format == DMA_OUTPUT_FORMAT_BAYER_PACKED)
		queue->framecfg.hw_pixeltype = pix->flags;

	for (plane = 0; plane < fmt->hw_plane; ++plane) {
		if (pix->plane_fmt[plane].bytesperline) {
			queue->framecfg.bytesperline[plane] =
				pix->plane_fmt[plane].bytesperline;
		} else {
			queue->framecfg.bytesperline[plane] = 0;
		}
	}

	ret = CALL_QOPS(queue, s_format, device, queue);
	if (ret) {
		mverr("[%s] s_format is fail(%d)", vctx, video, queue->name, ret);
		goto p_err;
	}

	mvinfo("[%s]pixelformat(%c%c%c%c), bit(%d), size(%dx%d)\n", vctx, video, queue->name,
		(char)((fmt->pixelformat >> 0) & 0xFF),
		(char)((fmt->pixelformat >> 8) & 0xFF),
		(char)((fmt->pixelformat >> 16) & 0xFF),
		(char)((fmt->pixelformat >> 24) & 0xFF),
		queue->framecfg.format->hw_bitwidth,
		queue->framecfg.width,
		queue->framecfg.height);
p_err:
	return ret;
}

int is_queue_setup(struct is_queue *queue,
	void *alloc_ctx, unsigned int *nplanes,
	unsigned int sizes[],	struct device *alloc_devs[])
{
  struct is_video_ctx *ivc = (struct is_video_ctx *)queue->vbq->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = &ivc->queue;
	struct is_frame_cfg *ifc = &iq->framecfg;
	struct is_fmt *fmt = ifc->format;
	int p;
	struct is_mem *mem;

	*nplanes = (unsigned int)fmt->num_planes;

	if (fmt->setup_plane_sz) {
		fmt->setup_plane_sz(ifc, sizes);
	} else {
		err("failed to setup plane sizes for pixelformat(%c%c%c%c)",
			(char)((fmt->pixelformat >> 0) & 0xFF),
			(char)((fmt->pixelformat >> 8) & 0xFF),
			(char)((fmt->pixelformat >> 16) & 0xFF),
			(char)((fmt->pixelformat >> 24) & 0xFF));

			return -EINVAL;
	}

	/* FIXME: 1. share meta plane, 2. configure the size */
	if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &iq->state)) {
		sizes[*nplanes] = SZ_256K + SZ_256K;
		*nplanes += NUM_OF_EXT_PLANE;
	}

	mem = is_hw_get_iommu_mem(iv->id);

	for (p = 0; p < *nplanes; p++) {
		alloc_devs[p] = mem->dev;
		ifc->size[p] = sizes[p];
		mdbgv_vid("queue[%d] size : %d\n", p, sizes[p]);
	}

	return 0;
}

int is_queue_buffer_queue(struct is_queue *queue,
	struct vb2_buffer *vb)
{
	struct is_video_ctx *vctx = container_of(queue, struct is_video_ctx, queue);
	struct is_video *video = GET_VIDEO(vctx);
	struct is_framemgr *framemgr = &queue->framemgr;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	unsigned int index = vb->index;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	unsigned int num_ext_planes = 0, pos_ext_p = 0;
	unsigned int num_buffers, pos_meta_p;
	struct is_frame *frame;
	int i;
	int ret = 0;
	bool need_vmap;

	FIMC_BUG(!video);

	/* take a snapshot whether it is needed or not */
	need_vmap = (dbg_draw_digit_ctrl ||
		((dbg_dma_dump_ctrl & ~DDD_CTRL_META)
		&& (video->vd.dev_debug & V4L2_DEV_DEBUG_DMA))
		);

	/* image planes */
	if (IS_ENABLED(CONFIG_DMA_BUF_CONTAINER) && vbuf->num_merged_dbufs) {
		/* vbuf has been sorted by the order of buffer */
		memcpy(queue->buf_dva[index], vbuf->dva,
			sizeof(dma_addr_t) * vbuf->num_merged_dbufs);

		if (need_vmap)
			memcpy(queue->buf_kva[index], vbuf->kva,
				sizeof(ulong) * vbuf->num_merged_dbufs);
		else
			memset(queue->buf_kva[index], 0x0,
				sizeof(ulong) * vbuf->num_merged_dbufs);

		num_buffers = vbuf->num_merged_dbufs / num_i_planes;
	} else {
		/* if additional plane exist, map for use ext feature */
		if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state))
			num_ext_planes = num_i_planes - queue->framecfg.format->hw_plane;

		num_i_planes -= num_ext_planes;

		for (i = 0; i < num_i_planes; i++) {
			if (test_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state))
				queue->buf_dva[index][i] = vbuf->dva[i];
			else
				queue->buf_dva[index][i] = vbuf->ops->plane_dvaddr(vbuf, i);

			if (test_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state))
				queue->buf_kva[index][i] = vbuf->ops->plane_kmap(vbuf, i, 0);

			else if (need_vmap)
				queue->buf_kva[index][i] = vbuf->ops->plane_kvaddr(vbuf, i);
			else
				queue->buf_kva[index][i] = 0UL;
		}

		num_buffers = 1;
	}

	pos_meta_p = num_buffers * num_i_planes;

	/* meta plane */
	queue->buf_kva[index][pos_meta_p]
			= vbuf->ops->plane_kmap(vbuf, num_i_planes, 0);
	if (!queue->buf_kva[index][pos_meta_p]) {
		mverr("failed to get kva for %s", vctx, video, queue->name);
		ret = -ENOMEM;
		goto err_get_kva_for_meta;
	}

	/* ext plane if exist */
	if(test_bit(IS_QUEUE_NEED_TO_EXTMAP, &queue->state)) {
		if (num_ext_planes)
			pos_ext_p = pos_meta_p + 1;
		for (i = pos_ext_p; i < pos_ext_p + num_ext_planes; i++) {
			queue->buf_kva[index][i]
					= vbuf->ops->plane_kmap(vbuf, i, 0);
			if (!queue->buf_kva[index][i]) {
				mverr("failed to get ext kva for %s", vctx, video, queue->name);
				ret = -ENOMEM;
				goto err_get_kva_for_ext;
			}
		}
	}

	/* setup a frame */
	frame = &framemgr->frames[index];
	frame->num_buffers = num_buffers;
	frame->planes = num_buffers * num_i_planes;
	frame->ext_planes = num_ext_planes;

	if (video->type == IS_VIDEO_TYPE_LEADER) {
		frame->shot_ext
			= (struct camera2_shot_ext *)queue->buf_kva[index][pos_meta_p];
		frame->shot = (struct camera2_shot *)((unsigned long)frame->shot_ext
					+ offsetof(struct camera2_shot_ext, shot));
		frame->shot_size = queue->framecfg.size[pos_meta_p]
					- offsetof(struct camera2_shot_ext, shot);

		for (i = 0; i < num_ext_planes; i++)
			frame->kvaddr_ext[i] = queue->buf_kva[index][pos_ext_p + i];
#ifdef MEASURE_TIME
		frame->tzone = (struct timespec64 *)frame->shot_ext->timeZone;
#endif
	} else {
		frame->stream
			= (struct camera2_stream *)queue->buf_kva[index][pos_meta_p];

		/* TODO : Someday need to change the variable type of struct to ulong */
		frame->stream->address = (u32)queue->buf_kva[index][pos_meta_p];
	}

	/* uninitialized frame need to get info */
	if (!test_bit(FRAME_MEM_INIT, &frame->mem_state))
		goto set_info;

	/* plane address check */
	for (i = 0; i < frame->planes; i++) {
		if (frame->dvaddr_buffer[i] != queue->buf_dva[index][i]) {
			if (video->resourcemgr->hal_version == IS_HAL_VER_3_2) {
				frame->dvaddr_buffer[i] = queue->buf_dva[index][i];
			} else {
				mverr("buffer[%d][%d] is changed(%pad != %pad)",
					vctx, video,
					index, i,
					&frame->dvaddr_buffer[i],
					&queue->buf_dva[index][i]);
				ret = -EINVAL;
				goto err_dva_changed;
			}
		}

		if (frame->kvaddr_buffer[i] != queue->buf_kva[index][i]) {
			if (video->resourcemgr->hal_version == IS_HAL_VER_3_2) {
				frame->kvaddr_buffer[i] = queue->buf_kva[index][i];
			} else {
				mverr("kvaddr buffer[%d][%d] is changed(0x%08lx != 0x%08lx)",
					vctx, video, index, i,
					frame->kvaddr_buffer[i], queue->buf_kva[index][i]);
				ret = -EINVAL;
				goto err_kva_changed;
			}
		}
	}

	return 0;

set_info:
	if (test_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state)) {
		mverr("already prepared but new index(%d) is came", vctx, video, index);
		ret = -EINVAL;
		goto err_queue_prepared_already;
	}

	for (i = 0; i < frame->planes; i++) {
		frame->dvaddr_buffer[i] = queue->buf_dva[index][i];
		frame->kvaddr_buffer[i] = queue->buf_kva[index][i];
		frame->size[i] = queue->framecfg.size[i];
#ifdef PRINT_BUFADDR
		mvinfo("%s %d.%d %pad\n", vctx, video, framemgr->name, index,
					i, &frame->dvaddr_buffer[i]);
#endif
	}

	set_bit(FRAME_MEM_INIT, &frame->mem_state);

	queue->buf_refcount++;

	if (queue->buf_rdycount == queue->buf_refcount)
		set_bit(IS_QUEUE_BUFFER_READY, &queue->state);

	if (queue->buf_maxcount == queue->buf_refcount) {
		if (IS_ENABLED(CONFIG_DMA_BUF_CONTAINER)
				&& vbuf->num_merged_dbufs)
			mvinfo("%s number of merged buffers: %d\n",
				vctx, video, queue->name, num_buffers);
		set_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);
	}

	queue->buf_que++;

err_queue_prepared_already:
err_kva_changed:
err_dva_changed:
err_get_kva_for_ext:
err_get_kva_for_meta:

	return ret;
}

int is_queue_buffer_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;

	vbuf->ops = vctx->is_vb2_buf_ops;

	return 0;
}

void is_queue_buffer_cleanup(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	unsigned int pos_meta_p, pos_ext_p;
	unsigned int num_ext_planes = 0;
	int i;

	if (test_bit(IS_QUEUE_NEED_TO_EXTMAP, &vctx->queue.state))
		num_ext_planes = num_i_planes - vctx->queue.framecfg.format->hw_plane;
	num_i_planes -= num_ext_planes;

	if (test_bit(IS_QUEUE_NEED_TO_KMAP, &vctx->queue.state)) {
		for (i = 0; i < vb->num_planes; i++)
			vbuf->ops->plane_kunmap(vbuf, i, 0);
	} else {
		pos_meta_p = num_i_planes;
		vbuf->ops->plane_kunmap(vbuf, pos_meta_p, 0);

		if (num_ext_planes) {
			pos_ext_p = pos_meta_p + 1;
			for (i = 0; i < num_ext_planes; i++)
				vbuf->ops->plane_kunmap(vbuf, pos_ext_p + i, 0);
		}
	}
}


int is_queue_buffer_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct is_video *video = GET_VIDEO(vctx);
  struct is_mem *mem;
	int ret;

	if (IS_ENABLED(DMABUF_CONTAINER)) {
		mem = is_hw_get_iommu_mem(video->id);
		ret = vbuf->ops->dbufcon_prepare(vbuf, mem->dev);
		if (ret) {
			mverr("failed to prepare dmabuf-container: %d", vctx, video, vb->index);
			return ret;
		}

		if (vbuf->num_merged_dbufs) {
			ret = vbuf->ops->dbufcon_map(vbuf);
			if (ret) {
				mverr("failed to map dmabuf-container: %d", vctx, video, vb->index);
				vbuf->ops->dbufcon_finish(vbuf);
				return ret;
			}
		}
	}

	if (test_bit(IS_QUEUE_NEED_TO_REMAP, &vctx->queue.state)) {
		ret = vbuf->ops->remap_attr(vbuf, 0);
		if (ret) {
			mverr("failed to remap dmabuf: %d", vctx, video, vb->index);
			return ret;
		}
	}

	vctx->queue.buf_pre++;

	return 0;
}

#define V4L2_BUF_FLAG_DISPOSAL	0x10000000
void is_queue_buffer_finish(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct is_video *video = GET_VIDEO(vctx);
	u32 framecount = vctx->queue.framemgr.frames[vb->index].fcount;
	bool ddd_trigger = false;
	struct vb2_queue *q = vb->vb2_queue;
	int plane;
	struct vb2_plane *p;

	if (video->vd.dev_debug & V4L2_DEV_DEBUG_DMA) {
		if (dbg_dma_dump_type == DDD_TYPE_PERIOD)
			ddd_trigger = !(framecount % dbg_dma_dump_arg1);
		else if (dbg_dma_dump_type == DDD_TYPE_INTERVAL)
			ddd_trigger = (framecount >= dbg_dma_dump_arg1)
					&& (framecount <= dbg_dma_dump_arg2);
		else if (dbg_dma_dump_type == DDD_TYPE_ONESHOT)
			ddd_trigger = (framecount == dbg_dma_dump_arg1);

		if (ddd_trigger && (dbg_dma_dump_ctrl & DDD_CTRL_IMAGE))
			is_dbg_dma_dump(&vctx->queue, vctx->instance, vb->index,
					video->id, DBG_DMA_DUMP_IMAGE);

		if (ddd_trigger && (dbg_dma_dump_ctrl & DDD_CTRL_META))
			is_dbg_dma_dump(&vctx->queue, vctx->instance, vb->index,
					video->id, DBG_DMA_DUMP_META);
	}

	if (IS_ENABLED(CONFIG_DMA_BUF_CONTAINER) &&
			(vbuf->num_merged_dbufs)) {
		vbuf->ops->dbufcon_unmap(vbuf);
		vbuf->ops->dbufcon_finish(vbuf);
	}

	if (test_bit(IS_QUEUE_NEED_TO_REMAP, &vctx->queue.state))
		vbuf->ops->unremap_attr(vbuf, 0);

	if ((vb2_v4l2_buf->flags & V4L2_BUF_FLAG_DISPOSAL) &&
			q->memory == VB2_MEMORY_DMABUF) {
		is_queue_buffer_cleanup(vb);

		for (plane = 0; plane < vb->num_planes; plane++) {
			p = &vb->planes[plane];

			if (!p->mem_priv)
				continue;

			if (p->dbuf_mapped)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
				q->mem_ops->unmap_dmabuf(p->mem_priv);
#else
				q->mem_ops->unmap_dmabuf(p->mem_priv, 0);
#endif

			q->mem_ops->detach_dmabuf(p->mem_priv);
			dma_buf_put(p->dbuf);
			p->mem_priv = NULL;
			p->dbuf = NULL;
			p->dbuf_mapped = 0;
		}
	}
}

void is_queue_wait_prepare(struct vb2_queue *vbq)
{
	struct is_video_ctx *vctx;
	struct is_video *video;

	FIMC_BUG_VOID(!vbq);

	vctx = vbq->drv_priv;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	video = vctx->video;
	mutex_unlock(&video->lock);
}

void is_queue_wait_finish(struct vb2_queue *vbq)
{
	struct is_video_ctx *vctx;
	struct is_video *video;

	FIMC_BUG_VOID(!vbq);

	vctx = vbq->drv_priv;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	video = vctx->video;
	mutex_lock(&video->lock);
}

int is_queue_start_streaming(struct is_queue *queue,
	void *qdevice)
{
	int ret = 0;

	FIMC_BUG(!queue);

	if (test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		err("[%s] already stream on(%ld)", queue->name, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	if (queue->buf_rdycount && !test_bit(IS_QUEUE_BUFFER_READY, &queue->state)) {
		err("[%s] buffer state is not ready(%ld)", queue->name, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_QOPS(queue, start_streaming, qdevice, queue);
	if (ret) {
		err("[%s] start_streaming is fail(%d)", queue->name, ret);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(IS_QUEUE_STREAM_ON, &queue->state);

p_err:
	return ret;
}

int is_queue_stop_streaming(struct is_queue *queue,
	void *qdevice)
{
	int ret = 0;

	FIMC_BUG(!queue);

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		err("[%s] already stream off(%ld)", queue->name, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_QOPS(queue, stop_streaming, qdevice, queue);
	if (ret) {
		err("[%s] stop_streaming is fail(%d)", queue->name, ret);
		ret = -EINVAL;
	}

	clear_bit(IS_QUEUE_STREAM_ON, &queue->state);
	clear_bit(IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);

p_err:
	return ret;
}

int is_video_probe(struct is_video *video,
	char *video_name,
	u32 video_number,
	u32 vfl_dir,
	struct is_mem *mem,
	struct v4l2_device *v4l2_dev,
	const struct v4l2_file_operations *fops,
	const struct v4l2_ioctl_ops *ioctl_ops)
{
	int ret = 0;
	u32 video_id;

	vref_init(video);
	mutex_init(&video->lock);
	snprintf(video->vd.name, sizeof(video->vd.name), "%s", video_name);
	video->id		= video_number;
	video->vb2_mem_ops	= mem->vb2_mem_ops;
	video->is_vb2_buf_ops = mem->is_vb2_buf_ops;
	video->alloc_ctx        = mem->priv;
  video->alloc_dev        = mem->dev;
	video->type		= (vfl_dir == VFL_DIR_RX) ? IS_VIDEO_TYPE_CAPTURE : IS_VIDEO_TYPE_LEADER;
	video->vd.vfl_dir	= vfl_dir;
	video->vd.v4l2_dev	= v4l2_dev;
	video->vd.fops		= fops;
	video->vd.ioctl_ops	= ioctl_ops;
	video->vd.minor		= -1;
	video->vd.release	= video_device_release;
	video->vd.lock		= &video->lock;
	video->vd.device_caps	= (vfl_dir == VFL_DIR_RX) ? VIDEO_CAPTURE_DEVICE_CAPS : VIDEO_OUTPUT_DEVICE_CAPS;
	video_set_drvdata(&video->vd, video);

	video_id = EXYNOS_VIDEONODE_FIMC_IS + video_number;
	ret = video_register_device(&video->vd, VFL_TYPE_VIDEO, video_id);
	if (ret) {
		err("[V%02d] Failed to register video device", video->id);
		goto p_err;
	}

p_err:
	info("[VID] %s(%d) is created. minor(%d)\n", video_name, video_id, video->vd.minor);
	return ret;
}

int is_video_open(struct is_video_ctx *vctx,
	void *device,
	u32 buf_rdycount,
	struct is_video *video,
	const struct vb2_ops *vb2_ops,
	const struct is_queue_ops *qops)
{
	int ret = 0;
	struct is_queue *queue;

	FIMC_BUG(!vctx);
	FIMC_BUG(!video);
	FIMC_BUG(!video->vb2_mem_ops);
	FIMC_BUG(!vb2_ops);

	if (!(vctx->state & BIT(IS_VIDEO_CLOSE))) {
		mverr("already open(%lX)", vctx, video, vctx->state);
		return -EEXIST;
	}

	if (atomic_read(&video->refcount) == 1) {
		sema_init(&video->smp_multi_input, 1);
		video->try_smp		= false;
	}

	queue = GET_QUEUE(vctx);
	queue->vbq = NULL;
	queue->qops = qops;

	vctx->device		= device;
	vctx->next_device	= NULL;
	vctx->subdev		= NULL;
	vctx->video		= video;
	vctx->vb2_ops		= vb2_ops;
	vctx->vb2_mem_ops	= video->vb2_mem_ops;
	vctx->is_vb2_buf_ops = video->is_vb2_buf_ops;
	vctx->vops.qbuf		= is_video_qbuf;
	vctx->vops.dqbuf	= is_video_dqbuf;
	vctx->vops.done 	= is_video_buffer_done;

	ret = is_queue_open(queue, buf_rdycount);
	if (ret) {
		mverr("is_queue_open is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	queue->vbq = vzalloc(sizeof(struct vb2_queue));
	if (!queue->vbq) {
		mverr("vzalloc is fail", vctx, video);
		ret = -ENOMEM;
		goto p_err;
	}

	ret = queue_init(vctx, queue->vbq, NULL);
	if (ret) {
		mverr("queue_init fail", vctx, video);
		vfree(queue->vbq);
		goto p_err;
	}

	vctx->state = BIT(IS_VIDEO_OPEN);

p_err:
	return ret;
}

int is_video_close(struct is_video_ctx *vctx)
{
	int ret = 0;
	struct is_video *video;
	struct is_queue *queue;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));

	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	if (vctx->state < BIT(IS_VIDEO_OPEN)) {
		mverr("already close(%lX)", vctx, video, vctx->state);
		return -ENOENT;
	}

	vb2_queue_release(queue->vbq);
	vfree(queue->vbq);
	is_queue_close(queue);

	/*
	 * vb2 release can call stop callback
	 * not if video node is not stream off
	 */
	vctx->device = NULL;
	vctx->state = BIT(IS_VIDEO_CLOSE);

	return ret;
}

int is_video_s_input(struct file *file,
	struct is_video_ctx *vctx)
{
	u32 ret = 0;

	if (!(vctx->state & (BIT(IS_VIDEO_OPEN) | BIT(IS_VIDEO_S_INPUT) | BIT(IS_VIDEO_S_BUFS)))) {
		mverr(" invalid s_input is requested(%lX)", vctx, vctx->video, vctx->state);
		return -EINVAL;
	}

	vctx->state = BIT(IS_VIDEO_S_INPUT);

	return ret;
}

int is_video_poll(struct file *file,
	struct is_video_ctx *vctx,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct is_queue *queue;

	FIMC_BUG(!vctx);

	queue = GET_QUEUE(vctx);
	ret = vb2_poll(queue->vbq, file, wait);

	return ret;
}

int is_video_mmap(struct file *file,
	struct is_video_ctx *vctx,
	struct vm_area_struct *vma)
{
	u32 ret = 0;
	struct is_queue *queue;

	FIMC_BUG(!vctx);

	queue = GET_QUEUE(vctx);

	ret = vb2_mmap(queue->vbq, vma);

	return ret;
}

int is_video_reqbufs(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_requestbuffers *request)
{
	int ret = 0;
	struct is_queue *queue;
	struct is_framemgr *framemgr;
	struct is_video *video;

	FIMC_BUG(!vctx);
	FIMC_BUG(!request);

	video = GET_VIDEO(vctx);
	if (!(vctx->state & (BIT(IS_VIDEO_S_FORMAT) | BIT(IS_VIDEO_STOP) | BIT(IS_VIDEO_S_BUFS)))) {
		mverr("invalid reqbufs is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	queue = GET_QUEUE(vctx);
	if (test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		mverr("video is stream on, not applied", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	/* before call queue ops if request count is zero */
	if (!request->count) {
		ret = CALL_QOPS(queue, request_bufs, GET_DEVICE(vctx), queue, request->count);
		if (ret) {
			mverr("request_bufs is fail(%d)", vctx, video, ret);
			goto p_err;
		}
	}

	ret = vb2_reqbufs(queue->vbq, request);
	if (ret) {
		mverr("vb2_reqbufs is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	framemgr = &queue->framemgr;
	queue->buf_maxcount = request->count;
	if (queue->buf_maxcount == 0) {
		queue->buf_req = 0;
		queue->buf_pre = 0;
		queue->buf_que = 0;
		queue->buf_com = 0;
		queue->buf_dqe = 0;
		queue->buf_refcount = 0;
		clear_bit(IS_QUEUE_BUFFER_READY, &queue->state);
		clear_bit(IS_QUEUE_BUFFER_PREPARED, &queue->state);
		frame_manager_close(framemgr);
	} else {
		if (queue->buf_maxcount < queue->buf_rdycount) {
			mverr("buffer count is not invalid(%d < %d)", vctx, video,
				queue->buf_maxcount, queue->buf_rdycount);
			ret = -EINVAL;
			goto p_err;
		}

		ret = frame_manager_open(framemgr, queue->buf_maxcount);
		if (ret) {
			mverr("is_frame_open is fail(%d)", vctx, video, ret);
			goto p_err;
		}
	}

	/* after call queue ops if request count is not zero */
	if (request->count) {
		ret = CALL_QOPS(queue, request_bufs, GET_DEVICE(vctx), queue, request->count);
		if (ret) {
			mverr("request_bufs is fail(%d)", vctx, video, ret);
			goto p_err;
		}
	}

	vctx->state = BIT(IS_VIDEO_S_BUFS);

p_err:
	return ret;
}

int is_video_querybuf(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_queue *queue;

	FIMC_BUG(!vctx);

	queue = GET_QUEUE(vctx);

	ret = vb2_querybuf(queue->vbq, buf);

	return ret;
}

int is_video_set_format_mplane(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_format *format)
{
	int ret = 0;
	u32 condition;
	void *device;
	struct is_video *video;
	struct is_queue *queue;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_DEVICE(vctx));
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!format);

	device = GET_DEVICE(vctx);
	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	/* capture video node can skip s_input */
	if (video->type  == IS_VIDEO_TYPE_LEADER)
		condition = BIT(IS_VIDEO_S_INPUT) | BIT(IS_VIDEO_S_BUFS);
	else
		condition = BIT(IS_VIDEO_S_INPUT) | BIT(IS_VIDEO_S_BUFS) | BIT(IS_VIDEO_OPEN)
											   | BIT(IS_VIDEO_STOP);

	if (!(vctx->state & condition)) {
		mverr("invalid s_format is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	ret = is_queue_set_format_mplane(vctx, queue, device, format);
	if (ret) {
		mverr("is_queue_set_format_mplane is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->state = BIT(IS_VIDEO_S_FORMAT);

p_err:
	mdbgv_vid("set_format(%d x %d)\n", queue->framecfg.width, queue->framecfg.height);
	return ret;
}

int is_video_qbuf(struct is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct is_queue *queue;
	struct vb2_queue *vbq;
	struct vb2_buffer *vb;
	struct is_video *video;
	int plane;
	struct vb2_plane planes[VB2_MAX_PLANES];

	FIMC_BUG(!vctx);
	FIMC_BUG(!buf);

	TIME_QUEUE(TMQ_QS);

	queue = GET_QUEUE(vctx);
	vbq = queue->vbq;
	video = GET_VIDEO(vctx);

	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_req++;

	ret = is_vb2_qbuf(queue->vbq, buf);
	if (ret) {
		mverr("vb2_qbuf is fail(index : %d, %d)", vctx, video, buf->index, ret);

		if (vbq->fileio) {
			mverr("file io in progress", vctx, video);
			ret = -EBUSY;
			goto p_err;
		}

		if (buf->type != queue->vbq->type) {
			mverr("buf type is invalid(%d != %d)", vctx, video,
				buf->type, queue->vbq->type);
			ret = -EINVAL;
			goto p_err;
		}

		if (buf->index >= vbq->num_buffers) {
			mverr("buffer index%d out of range", vctx, video, buf->index);
			ret = -EINVAL;
			goto p_err;
		}

		if (buf->memory != vbq->memory) {
			mverr("invalid memory type%d", vctx, video, buf->memory);
			ret = -EINVAL;
			goto p_err;
		}

		vb = vbq->bufs[buf->index];
		if (!vb) {
			mverr("vb is NULL", vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		if (V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
			/* Is memory for copying plane information present? */
			if (buf->m.planes == NULL) {
				mverr("multi-planar buffer passed but "
					   "planes array not provided\n", vctx, video);
				ret = -EINVAL;
				goto p_err;
			}

			if (buf->length < vb->num_planes || buf->length > VB2_MAX_PLANES) {
				mverr("incorrect planes array length, "
					   "expected %d, got %d\n", vctx, video,
					   vb->num_planes, buf->length);
				ret = -EINVAL;
				goto p_err;
			}
		}

		/* for detect vb2 framework err, operate some vb2 functions */
		memset(planes, 0, sizeof(planes[0]) * vb->num_planes);
		vb->vb2_queue->buf_ops->is_fill_vb2_buffer(vb, buf, planes);

		for (plane = 0; plane < vb->num_planes; ++plane) {
			struct dma_buf *dbuf;

			dbuf = dma_buf_get(planes[plane].m.fd);
			if (IS_ERR_OR_NULL(dbuf)) {
				mverr("invalid dmabuf fd(%d) for plane %d\n",
					vctx, video, planes[plane].m.fd, plane);
				goto p_err;
			}

			if (planes[plane].length == 0)
				planes[plane].length = (unsigned int)dbuf->size;

			if (planes[plane].length < vb->planes[plane].min_length) {
				mverr("invalid dmabuf length %u for plane %d, "
					"minimum length %u\n",
					vctx, video, planes[plane].length, plane,
					vb->planes[plane].min_length);
				dma_buf_put(dbuf);
				goto p_err;
			}

			dma_buf_put(dbuf);
		}

		goto p_err;
	}

p_err:
	TIME_QUEUE(TMQ_QE);
	return ret;
}

int is_video_dqbuf(struct is_video_ctx *vctx,
	struct v4l2_buffer *buf,
	bool blocking)
{
	int ret = 0;
	struct is_video *video;
	struct is_queue *queue;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!buf);

	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	if (!queue->vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->type != queue->vbq->type) {
		mverr("buf type is invalid(%d != %d)", vctx, video, buf->type, queue->vbq->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		mverr("queue is not streamon(%ld)", vctx,  video, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_dqe++;

	ret = vb2_dqbuf(queue->vbq, buf, blocking);
	if (ret) {
		mverr("vb2_dqbuf is fail(%d)", vctx,  video, ret);
		if (test_bit(IS_HAL_DEBUG_SUDDEN_DEAD_DETECT, &sysfs_debug.hal_debug_mode) &&
				ret == -ERESTARTSYS) {
			msleep(sysfs_debug.hal_debug_delay);
			panic("HAL dead");
		}
		goto p_err;
	}

p_err:
	TIME_QUEUE(TMQ_DQ);
	return ret;
}

int is_video_prepare(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	int index = 0;
	struct is_device_ischain *device;
	struct is_queue *queue;
	struct vb2_queue *vbq;
	struct vb2_buffer *vb;
	struct is_video *video;
#ifdef ENABLE_BUFFER_HIDING
	struct is_pipe *pipe;
#endif

	FIMC_BUG(!file);
	FIMC_BUG(!vctx);
	FIMC_BUG(!buf);

	device = GET_DEVICE_ISCHAIN(vctx);
	queue = GET_QUEUE(vctx);
	vbq = queue->vbq;
	video = GET_VIDEO(vctx);
	index = buf->index;
	if (index >= VB2_MAX_FRAME) {
                mverr("buffer index[%d] range over", vctx, video, buf->index);
                ret = -EINVAL;
                goto p_err;
	}

        vb = queue->vbq->bufs[index];
        if (!vb) {
                mverr("vb is NULL", vctx, video);
                ret = -EINVAL;
                goto p_err;
        }
#ifdef ENABLE_BUFFER_HIDING
	pipe = &device->pipe;

	if ((pipe->dst) && (pipe->dst->leader.vid == video->id)) {
		if (index >=  IS_MAX_PIPE_BUFS) {
			mverr("The leader index(%d) is bigger than array size(%d)",
				vctx, video, index, IS_MAX_PIPE_BUFS);
			ret = -EINVAL;
			goto p_err;
		}

		if (!V4L2_TYPE_IS_MULTIPLANAR(buf->type)) {
			mverr("the type of passed buffer is not multi-planar",
					vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		if (!buf->m.planes) {
			mverr("planes array not provided", vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		if (buf->length > IS_MAX_PLANES) {
			mverr("incorrect planes array length", vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		/* Destination */
		memcpy(&pipe->buf[PIPE_SLOT_DST][index], buf, sizeof(struct v4l2_buffer));
		memcpy(pipe->planes[PIPE_SLOT_DST][index], buf->m.planes, sizeof(struct v4l2_plane) * buf->length);
		pipe->buf[PIPE_SLOT_DST][index].m.planes = (struct v4l2_plane *)pipe->planes[PIPE_SLOT_DST][index];
	} else if ((pipe->dst) && (pipe->vctx[PIPE_SLOT_JUNCTION]) &&
			(pipe->vctx[PIPE_SLOT_JUNCTION]->video->id == video->id)) {
		if (index >=  IS_MAX_PIPE_BUFS) {
			mverr("The junction index(%d) is bigger than array size(%d)",
				vctx, video, index, IS_MAX_PIPE_BUFS);
			ret = -EINVAL;
			goto p_err;
		}

		/* Junction */
		if ((pipe->dst) && test_bit(IS_GROUP_PIPE_INPUT, &pipe->dst->state)) {
			memcpy(&pipe->buf[PIPE_SLOT_JUNCTION][index], buf, sizeof(struct v4l2_buffer));
			memcpy(pipe->planes[PIPE_SLOT_JUNCTION][index], buf->m.planes, sizeof(struct v4l2_plane) * buf->length);
			pipe->buf[PIPE_SLOT_JUNCTION][index].m.planes = (struct v4l2_plane *)pipe->planes[PIPE_SLOT_JUNCTION][index];
		}
	}
#endif

	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	vb = vbq->bufs[buf->index];
	if (!vb) {
		mverr("vb is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_vb2_prepare_buf(vbq, buf);
	if (ret) {
		mverr("vb2_prepare_buf is fail(index : %d, %d)", vctx, video, buf->index, ret);
		goto p_err;
	}

	ret = is_queue_buffer_queue(queue, vb);
	if (ret) {
		mverr("is_queue_buffer_queue is fail(index : %d, %d)", vctx, video, buf->index, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int is_video_streamon(struct file *file,
	struct is_video_ctx *vctx,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct vb2_queue *vbq;
	struct is_video *video;

	FIMC_BUG(!file);
	FIMC_BUG(!vctx);

	video = GET_VIDEO(vctx);
	if (!(vctx->state & (BIT(IS_VIDEO_S_BUFS) | BIT(IS_VIDEO_STOP)))) {
		mverr("invalid streamon is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	vbq = GET_QUEUE(vctx)->vbq;
	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (vbq->type != type) {
		mverr("invalid stream type(%d != %d)", vctx, video, vbq->type, type);
		ret = -EINVAL;
		goto p_err;
	}

	if (vb2_is_streaming(vbq)) {
		mverr("streamon: already streaming", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	ret = vb2_streamon(vbq, type);
	if (ret) {
		mverr("vb2_streamon is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->state = BIT(IS_VIDEO_START);

p_err:
	return ret;
}

int is_video_streamoff(struct file *file,
	struct is_video_ctx *vctx,
	enum v4l2_buf_type type)
{
	int ret = 0;
	u32 rcnt, pcnt, ccnt;
	struct is_queue *queue;
	struct vb2_queue *vbq;
	struct is_framemgr *framemgr;
	struct is_video *video;

	FIMC_BUG(!file);
	FIMC_BUG(!vctx);

	video = GET_VIDEO(vctx);
	if (!(vctx->state & BIT(IS_VIDEO_START))) {
		mverr("invalid streamoff is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	queue = GET_QUEUE(vctx);
	framemgr = &queue->framemgr;
	vbq = queue->vbq;
	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr_e_barrier_irq(framemgr, FMGR_IDX_0);
	rcnt = framemgr->queued_count[FS_REQUEST];
	pcnt = framemgr->queued_count[FS_PROCESS];
	ccnt = framemgr->queued_count[FS_COMPLETE];
	framemgr_x_barrier_irq(framemgr, FMGR_IDX_0);

	if (rcnt + pcnt + ccnt > 0)
		mvwarn("stream off : queued buffer is not empty(R%d, P%d, C%d)", vctx,
			video, rcnt, pcnt, ccnt);

	if (vbq->type != type) {
		mverr("invalid stream type(%d != %d)", vctx, video, vbq->type, type);
		ret = -EINVAL;
		goto p_err;
	}

	if (!vbq->streaming) {
		mverr("streamoff: not streaming", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	ret = vb2_streamoff(vbq, type);
	if (ret) {
		mverr("vb2_streamoff is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	ret = frame_manager_flush(framemgr);
	if (ret) {
		mverr("is_frame_flush is fail(%d)", vctx, video, ret);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->state = BIT(IS_VIDEO_STOP);

p_err:
	return ret;
}

int is_video_s_ctrl(struct file *file,
	struct is_video_ctx *vctx,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video *video;
	struct is_device_ischain *device;
	struct is_resourcemgr *resourcemgr;
	struct is_queue *queue;
	struct is_group *head;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_DEVICE_ISCHAIN(vctx));
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!ctrl);

	device = GET_DEVICE_ISCHAIN(vctx);
	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);
	resourcemgr = device->resourcemgr;

	ret = is_vender_video_s_ctrl(ctrl, device);
	if (ret) {
		mverr("is_vender_video_s_ctrl is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_END_OF_STREAM:
		ret = is_ischain_open_wrap(device, true);
		if (ret) {
			mverr("is_ischain_open_wrap is fail(%d)", vctx, video, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_IS_SET_SETFILE:
	{
		u32 scenario;

		if (test_bit(IS_ISCHAIN_START, &device->state)) {
			mverr("device is already started, setfile applying is fail", vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		device->setfile = ctrl->value;
		scenario = (device->setfile & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT;
		mvinfo(" setfile(%d), scenario(%d) at s_ctrl\n", device, video,
			device->setfile & IS_SETFILE_MASK, scenario);
		break;
	}
	case V4L2_CID_IS_S_DVFS_SCENARIO:
	{
		u32 vendor;

		device->dvfs_scenario = ctrl->value;
		vendor = (device->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT) & IS_DVFS_SCENARIO_VENDOR_MASK;
		mvinfo("[DVFS] value(%x), common(%d), vendor(%d) at s_ctrl\n", device, video,
			device->dvfs_scenario, device->dvfs_scenario & IS_DVFS_SCENARIO_COMMON_MODE_MASK, vendor);
		break;
	}
	case V4L2_CID_IS_HAL_VERSION:
		if (ctrl->value < 0 || ctrl->value >= IS_HAL_VER_MAX) {
			mverr("hal version(%d) is invalid", vctx, video, ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		resourcemgr->hal_version = ctrl->value;
		break;
	case V4L2_CID_IS_DEBUG_DUMP:
		info("Print fimc-is info dump by HAL");
		is_resource_dump();

		if (ctrl->value)
			panic("intentional panic from camera HAL");
		break;
	case V4L2_CID_IS_DVFS_CLUSTER0:
	case V4L2_CID_IS_DVFS_CLUSTER1:
	case V4L2_CID_IS_DVFS_CLUSTER2:
		is_resource_ioctl(resourcemgr, ctrl);
		break;
	case V4L2_CID_IS_DEBUG_SYNC_LOG:
		is_logsync(device->interface, ctrl->value, IS_MSG_TEST_SYNC_LOG);
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->value)
			set_bit(SCALER_FLIP_COMMAND_X_MIRROR, &queue->framecfg.flip);
		else
			clear_bit(SCALER_FLIP_COMMAND_X_MIRROR, &queue->framecfg.flip);
		break;
	case V4L2_CID_VFLIP:
		if (ctrl->value)
			set_bit(SCALER_FLIP_COMMAND_Y_MIRROR, &queue->framecfg.flip);
		else
			clear_bit(SCALER_FLIP_COMMAND_Y_MIRROR, &queue->framecfg.flip);
		break;
	case V4L2_CID_IS_INTENT:
		head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, &device->group_3aa);

		head->intent_ctl.captureIntent = ctrl->value;
		mvinfo(" s_ctrl intent(%d)\n", vctx, video, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_TYPE:
		switch (ctrl->value) {
		case IS_COLD_BOOT:
			/* change value to X when !TWIZ | front */
			is_itf_fwboot_init(device->interface);
			break;
		case IS_WARM_BOOT:
			/* change value to X when TWIZ & back | frist time back camera */
			if (!test_bit(IS_IF_LAUNCH_FIRST, &device->interface->launch_state))
				device->interface->fw_boot_mode = FIRST_LAUNCHING;
			else
				device->interface->fw_boot_mode = WARM_BOOT;
			break;
		default:
			mverr("unsupported ioctl(0x%X)", vctx, video, ctrl->id);
			ret = -EINVAL;
			break;
		}
		break;
	case V4L2_CID_IS_OPENING_HINT:
	{
		struct is_core *core;
		core = (struct is_core *)dev_get_drvdata(is_dev);
		if (core) {
			mvinfo("opening hint(%d)\n", device, video, ctrl->value);
			core->vender.opening_hint = ctrl->value;
		}
		break;
	}
	case V4L2_CID_IS_CLOSING_HINT:
	{
		struct is_core *core;
		core = (struct is_core *)dev_get_drvdata(is_dev);
		if (core) {
			mvinfo("closing hint(%d)\n", device, video, ctrl->value);
			core->vender.closing_hint = ctrl->value;
		}
		break;
	}
	case VENDER_S_CTRL:
		/* This s_ctrl is needed to skip, when the s_ctrl id was found. */
		break;
	default:
		mverr("unsupported ioctl(0x%X)", vctx, video, ctrl->id);
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

int is_video_buffer_done(struct is_video_ctx *vctx,
	u32 index, u32 state)
{
	int ret = 0;
	struct vb2_buffer *vb;
	struct is_queue *queue;
	struct is_video *video;

	FIMC_BUG(!vctx);
	FIMC_BUG(!vctx->video);
	FIMC_BUG(index >= IS_MAX_BUFS);

	queue = GET_QUEUE(vctx);
	video = GET_VIDEO(vctx);

	if (!queue->vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	vb = queue->vbq->bufs[index];
	if (!vb) {
		mverr("vb is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(IS_QUEUE_STREAM_ON, &queue->state)) {
		warn("%d video queue is not stream on", vctx->video->id);
		ret = -EINVAL;
		goto p_err;
	}

	if (vb->state != VB2_BUF_STATE_ACTIVE) {
		mverr("vb buffer[%d] state is not active(%d)", vctx, video, index, vb->state);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_com++;

	vb2_buffer_done(vb, state);

p_err:
	return ret;
}
