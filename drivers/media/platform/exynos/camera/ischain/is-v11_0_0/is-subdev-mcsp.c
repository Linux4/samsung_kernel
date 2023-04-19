// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kernel-variant.h"
#include "is-device-ischain.h"
#include "is-subdev-ctrl.h"
#include "is-config.h"
#include "is-param.h"
#include "is-video.h"
#include "is-type.h"

#include "is-core.h"
#include "is-dvfs.h"
#include "is-interface-ddk.h"
#include "is-stripe.h"

#define MXP_RATIO_UP		(MCSC_POLY_RATIO_UP)
#define MXP_RATIO_DOWN		(256)
static void is_ischain_mxp_check_align(struct is_device_ischain *device,
	struct is_crop *incrop,
	struct is_crop *otcrop)
{
	if (incrop->x % MCSC_OFFSET_ALIGN) {
		mwarn("Input crop offset x should be multiple of %d, change size(%d -> %d)",
			device, MCSC_OFFSET_ALIGN, incrop->x, ALIGN_DOWN(incrop->x, MCSC_OFFSET_ALIGN));
		incrop->x = ALIGN_DOWN(incrop->x, MCSC_OFFSET_ALIGN);
	}
	if (incrop->w % MCSC_WIDTH_ALIGN) {
		mwarn("Input crop width should be multiple of %d, change size(%d -> %d)",
			device, MCSC_WIDTH_ALIGN, incrop->w, ALIGN_DOWN(incrop->w, MCSC_WIDTH_ALIGN));
		incrop->w = ALIGN_DOWN(incrop->w, MCSC_WIDTH_ALIGN);
	}
	if (incrop->h % MCSC_HEIGHT_ALIGN) {
		mwarn("Input crop height should be multiple of %d, change size(%d -> %d)",
			device, MCSC_HEIGHT_ALIGN, incrop->h, ALIGN_DOWN(incrop->h, MCSC_HEIGHT_ALIGN));
		incrop->h = ALIGN_DOWN(incrop->h, MCSC_HEIGHT_ALIGN);
	}

	if (otcrop->x % MCSC_OFFSET_ALIGN) {
		mwarn("Output crop offset x should be multiple of %d, change size(%d -> %d)",
			device, MCSC_OFFSET_ALIGN, otcrop->x, ALIGN_DOWN(otcrop->x, MCSC_OFFSET_ALIGN));
		otcrop->x = ALIGN_DOWN(otcrop->x, MCSC_OFFSET_ALIGN);
	}
	if (otcrop->w % MCSC_WIDTH_ALIGN) {
		mwarn("Output crop width should be multiple of %d, change size(%d -> %d)",
			device, MCSC_WIDTH_ALIGN, otcrop->w, ALIGN_DOWN(otcrop->w, MCSC_WIDTH_ALIGN));
		otcrop->w = ALIGN_DOWN(otcrop->w, MCSC_WIDTH_ALIGN);
	}
	if (otcrop->h % MCSC_HEIGHT_ALIGN) {
		mwarn("Output crop height should be multiple of %d, change size(%d -> %d)",
			device, MCSC_HEIGHT_ALIGN, otcrop->h, ALIGN_DOWN(otcrop->h, MCSC_HEIGHT_ALIGN));
		otcrop->h = ALIGN_DOWN(otcrop->h, MCSC_HEIGHT_ALIGN);
	}
}

static int is_ischain_mxp_adjust_crop(struct is_device_ischain *device,
	u32 input_crop_w, u32 input_crop_h,
	u32 *output_crop_w, u32 *output_crop_h)
{
	int changed = 0;

	if (*output_crop_w > input_crop_w * MXP_RATIO_UP) {
		mwarn("Cannot be scaled up beyond %d times(%d -> %d)",
			device, MXP_RATIO_UP, input_crop_w, *output_crop_w);
		*output_crop_w = input_crop_w * MXP_RATIO_UP;
		changed |= 0x01;
	}

	if (*output_crop_h > input_crop_h * MXP_RATIO_UP) {
		mwarn("Cannot be scaled up beyond %d times(%d -> %d)",
			device, MXP_RATIO_UP, input_crop_h, *output_crop_h);
		*output_crop_h = input_crop_h * MXP_RATIO_UP;
		changed |= 0x02;
	}

	if (*output_crop_w < (input_crop_w + (MXP_RATIO_DOWN - 1)) / MXP_RATIO_DOWN) {
		mwarn("Cannot be scaled down beyond 1/%d times(%d -> %d)",
			device, MXP_RATIO_DOWN, input_crop_w, *output_crop_w);
		*output_crop_w = ALIGN((input_crop_w + (MXP_RATIO_DOWN - 1)) / MXP_RATIO_DOWN, MCSC_WIDTH_ALIGN);
		changed |= 0x10;
	}

	if (*output_crop_h < (input_crop_h + (MXP_RATIO_DOWN - 1))/ MXP_RATIO_DOWN) {
		mwarn("Cannot be scaled down beyond 1/%d times(%d -> %d)",
			device, MXP_RATIO_DOWN, input_crop_h, *output_crop_h);
		*output_crop_h = ALIGN((input_crop_h + (MXP_RATIO_DOWN - 1))/ MXP_RATIO_DOWN, MCSC_HEIGHT_ALIGN);
		changed |= 0x20;
	}

	return changed;
}

static u32 is_ischain_mxp_get_sbwc_type(struct is_fmt *fmt, u32 flags)
{
	u32 hw_sbwc = (flags & PIXEL_TYPE_EXTRA_MASK) >> PIXEL_TYPE_EXTRA_SHIFT;

	switch (hw_sbwc) {
	case COMP:
		hw_sbwc |= SBWC_STRIDE_64B_ALIGN;
		break;
	case COMP_LOSS:
		if (fmt->sbwc_align == 64)
			hw_sbwc |= SBWC_STRIDE_64B_ALIGN;
		break;
	default:
		hw_sbwc = 0;
		break;
	}

	return hw_sbwc;
}

int __mcsc_dma_out_cfg(struct is_device_ischain *device,
	struct is_frame *ldr_frame,
	struct camera2_node *node,
	u32 pindex,
	IS_DECLARE_PMAP(pmap),
	int index)
{
	int ret = 0;
	struct param_mcs_output *mcs_output;
	struct is_crop *incrop, *otcrop;
	struct is_fmt *fmt;
	u32 flip;
	u32 crange;
	u32 stripe_in_start_pos_x = 0;
	u32 stripe_roi_start_pos_x = 0, stripe_roi_end_pos_x = 0;
	u32 use_out_crop = 0;
	u32 hw_sbwc;

	FIMC_BUG(!device);
	FIMC_BUG(!ldr_frame);
	FIMC_BUG(!node);

	incrop = (struct is_crop *)node->input.cropRegion;
	/* TODO: prevent null crop info */

	otcrop = (struct is_crop *)node->output.cropRegion;
	/* TODO: prevent null crop info */

	/* Check MCSC in/out crop size alignment */
	is_ischain_mxp_check_align(device, incrop, otcrop);

	mcs_output = is_itf_g_param(device, ldr_frame, pindex);

	if (mcs_output->dma_cmd != (node->request & BIT(0))) {
		mlvinfo("[F%d] WDMA enable: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			mcs_output->dma_cmd, node->request);

		/* HF on/off */
		if (pindex == PARAM_MCS_OUTPUT5) {
			struct param_mcs_output *mcs_output_region;

			mcs_output_region = is_itf_g_param(device, NULL, pindex);
			mcs_output_region->dma_cmd = node->request & BIT(0);

			pablo_set_static_dvfs(device, "HF", IS_DVFS_SN_END, IS_DVFS_PATH_M2M);
		}
	}

	mcs_output->otf_cmd = OTF_OUTPUT_COMMAND_DISABLE;
	mcs_output->otf_format = OTF_OUTPUT_FORMAT_YUV422;
	mcs_output->otf_bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT;
	mcs_output->otf_order = OTF_OUTPUT_ORDER_BAYER_GR_BG;

	mcs_output->dma_cmd = DMA_OUTPUT_COMMAND_DISABLE;

	set_bit(pindex, pmap);

	if (!node->request)
		return ret;

	/* if output 5, port for high frequency */
	if (((pindex - PARAM_MCS_OUTPUT0) != MCSC_INPUT_HF) && (otcrop->w % 16)) {
		/*
		* For compatibility with post processing IP,
		* width & stride should be aligned by 16.
		* Just check it because it is not MCSC HW spec
		*/
		mwarn("Output crop width(%d) should be multiple of %d", device, otcrop->w, 16);
	}

	fmt = is_find_format(node->pixelformat, node->flags);
	if (!fmt) {
		mlverr("pixelformat(%c%c%c%c) is not found", device, node->vid,
			(char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF));
		return -EINVAL;
	}
	flip = mcs_output->flip;
	if ((pindex >= PARAM_MCS_OUTPUT0 && pindex < PARAM_MCS_OUTPUT5) &&
		ldr_frame->shot_ext->mcsc_flip[pindex - PARAM_MCS_OUTPUT0] != mcs_output->flip) {
		flip = ldr_frame->shot_ext->mcsc_flip[pindex - PARAM_MCS_OUTPUT0];
		node->flip = flip << 1;
		mlvinfo("[F%d]flip is changed(%d->%d)\n", device,
				node->vid, ldr_frame->fcount,
				mcs_output->flip, flip);
	}

	if (IS_ENABLED(CHAIN_STRIPE_PROCESSING) && ldr_frame->stripe_info.region_num
		&& (pindex - PARAM_MCS_OUTPUT0) != MCSC_INPUT_HF)
		use_out_crop = 1;

	if ((mcs_output->crop_width != incrop->w) ||
		(mcs_output->crop_height != incrop->h) ||
		(mcs_output->width != otcrop->w) ||
		(mcs_output->height != otcrop->h))
		mlvinfo("[F%d]WDMA incrop[%d, %d, %d, %d] otcrop[%d, %d, %d, %d]\n",
			device, node->vid, ldr_frame->fcount,
			incrop->x, incrop->y, incrop->w, incrop->h,
			otcrop->x, otcrop->y, otcrop->w, otcrop->h);

	is_ischain_mxp_adjust_crop(device, incrop->w, incrop->h, &otcrop->w, &otcrop->h);

	if (node->quantization == V4L2_QUANTIZATION_FULL_RANGE)
		crange = SCALER_OUTPUT_YUV_RANGE_FULL;
	else
		crange = SCALER_OUTPUT_YUV_RANGE_NARROW;

	if ((mcs_output->yuv_range != crange) &&
		!IS_VIDEO_HDR_SCENARIO(device->setfile & IS_SETFILE_MASK))
		mlvinfo("[F%d] CRange: %d -> %d\n", device, node->vid,
			ldr_frame->fcount, mcs_output->yuv_range, crange);

	if (mcs_output->dma_format != fmt->hw_format)
		mlvinfo("[F%d]WDMA format: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			mcs_output->dma_format, fmt->hw_format);
	if (mcs_output->dma_bitwidth != fmt->hw_bitwidth)
		mlvinfo("[F%d]WDMA bitwidth: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			mcs_output->dma_bitwidth, fmt->hw_bitwidth);

	/* SBWC Check */
	if (node->vid == MCSC_HF_ID && CHECK_HF_SBWC_DISABLE(exynos_soc_info))
		hw_sbwc = 0;
	else
		hw_sbwc = is_ischain_mxp_get_sbwc_type(fmt, node->flags);

	if (mcs_output->sbwc_type != hw_sbwc)
		mlvinfo("[F%d]WDMA sbwc_type: %d -> %d\n", device,
			node->vid, ldr_frame->fcount,
			mcs_output->sbwc_type, hw_sbwc);

	mcs_output->dma_cmd = DMA_OUTPUT_COMMAND_ENABLE;
	mcs_output->dma_bitwidth = fmt->hw_bitwidth;
	mcs_output->dma_format = fmt->hw_format;
	mcs_output->sbwc_type = hw_sbwc;
	mcs_output->dma_order = fmt->hw_order;
	mcs_output->plane = fmt->hw_plane;
	mcs_output->bitsperpixel = fmt->bitsperpixel[0] | fmt->bitsperpixel[1] << 8 | fmt->bitsperpixel[2] << 16;

	mcs_output->crop_offset_x = incrop->x;
	mcs_output->crop_offset_y = incrop->y;
	mcs_output->crop_width = incrop->w;
	mcs_output->crop_height = incrop->h;

	mcs_output->width = otcrop->w;
	mcs_output->height = otcrop->h;

	mcs_output->full_input_width = incrop->w; /* for stripe */
	mcs_output->full_output_width = otcrop->w; /* for stripe */

	mcs_output->stripe_in_start_pos_x = stripe_in_start_pos_x; /* for stripe */
	mcs_output->stripe_roi_start_pos_x = stripe_roi_start_pos_x; /* for stripe */
	mcs_output->stripe_roi_end_pos_x = stripe_roi_end_pos_x; /* for stripe */
	mcs_output->cmd = (use_out_crop & BIT(MCSC_OUT_CROP)); /* for stripe */
	mcs_output->cmd |= (node->request & BIT(MCSC_CROP_TYPE)); /* 0: ceter crop, 1: freeform crop */

	mcs_output->dma_stride_y = max(node->bytesperline[0], (otcrop->w * fmt->bitsperpixel[0]) / BITS_PER_BYTE);
	mcs_output->dma_stride_c = max(node->bytesperline[1], (otcrop->w * fmt->bitsperpixel[1]) / BITS_PER_BYTE);

	if ((pindex - PARAM_MCS_OUTPUT0) == MCSC_INPUT_HF && !node->bytesperline[0]) {
		mcs_output->dma_stride_y = ALIGN(mcs_output->dma_stride_y, 16);
		mcs_output->dma_stride_c = ALIGN(mcs_output->dma_stride_c, 16);
	} else {
		if (!IS_ALIGNED(mcs_output->dma_stride_y, 16) ||
		!IS_ALIGNED(mcs_output->dma_stride_c, 16))
		mwarn("WDMA stride(y:%#x uv:%#x) should be %d aligned",
			device, mcs_output->dma_stride_y, mcs_output->dma_stride_c, 16);
	}

	mcs_output->yuv_range = crange;
	mcs_output->flip = flip;

#ifdef ENABLE_HWFC
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		mcs_output->hwfc = 1; /* TODO: enum */
	else
		mcs_output->hwfc = 0; /* TODO: enum */
#endif

	return ret;
}

static int is_ischain_mxp_get(struct is_subdev *subdev,
			      struct is_device_ischain *idi,
			      struct is_frame *frame,
			      enum pablo_subdev_get_type type,
			      void *result)
{
	struct camera2_node *node;
	struct is_crop *outcrop;

	msrdbgs(1, "GET type: %d\n", idi, subdev, frame, type);

	switch (type) {
	case PSGT_REGION_NUM:
		node = &frame->shot_ext->node_group.leader;
		outcrop = (struct is_crop *)node->output.cropRegion;
		*(int *)result = is_calc_region_num(outcrop->w, subdev);
		break;
	default:
		break;
	}

	return 0;
}

static const struct is_subdev_ops is_subdev_mcsp_ops = {
	.bypass	= NULL,
	.cfg	= NULL,
	.tag	= NULL,
	.get	= is_ischain_mxp_get,
};

const struct is_subdev_ops *pablo_get_is_subdev_mcsp_ops(void)
{
	return &is_subdev_mcsp_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_get_is_subdev_mcsp_ops);
